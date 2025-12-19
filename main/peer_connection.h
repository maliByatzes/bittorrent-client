#pragma once

#include "torrent_file.h"
#include <array>
#include <cstdint>
#include <queue>
#include <string>
#include <utility>
#include <vector>

struct PeerState {
  bool am_choking;
  bool am_interested;
  bool peer_choking;
  bool peer_interested;

  PeerState()
      : am_choking(true), am_interested(false), peer_choking(true),
        peer_interested(false) {}
};

enum class MessageType : uint8_t {
  CHOKE = 0,
  UNCHOKE = 1,
  INTERESTED = 2,
  NOT_INTERESTED = 3,
  HAVE = 4,
  BIT_FIELD = 5,
  REQUEST = 6,
  PIECE = 7,
  CANCEL = 8,
  KEEP_ALIVE = 255
};

struct PeerMessage {
  MessageType type;
  std::vector<uint8_t> payload;

  PeerMessage(MessageType t) : type(t) {}
  PeerMessage(MessageType t, std::vector<uint8_t> p)
      : type(t), payload(std::move(p)) {}
};

struct PeerRequest {
  uint32_t piece_index;
  uint32_t block_offset;
  uint32_t block_length;

  PeerRequest(uint32_t idx, uint32_t off, uint32_t len)
      : piece_index(idx), block_offset(off), block_length(len) {}
};

class PeerConnection {
private:
  std::string m_ip;
  uint16_t m_port;
  int m_socket;

  std::array<uint8_t, 20> m_info_hash;
  std::string m_our_peer_id;
  std::string m_peer_id;

  PeerState m_state;
  std::vector<bool> m_peer_pieces;

  bool m_connected;
  bool m_handshake_complete;

  std::queue<PeerRequest> m_peer_requests;

public:
  PeerConnection(const std::string &ip, uint16_t port,
                 const std::array<uint8_t, 20> &info_hash,
                 const std::string &our_peer_id);

  ~PeerConnection();

  bool connect(int timeout_seconds = 10);
  void disconnect();
  bool isConnected() const { return m_connected; }

  bool performHandshake();
  bool isHandshakeComplete() const { return m_handshake_complete; }

  bool sendKeepAlive();
  bool sendChoke();
  bool sendUnchoke();
  bool sendInterested();
  bool sendNotInterested();
  bool sendHave(uint32_t piece_index);
  bool sendBitfield(const std::vector<bool> &pieces);
  bool sendRequest(uint32_t piece_index, uint32_t block_offset,
                   uint32_t block_length);
  bool sendPiece(uint32_t piece_index, uint32_t block_offset,
                 std::vector<uint8_t> &block_data);
  bool sendCancel(uint32_t piece_index, uint32_t block_offset,
                  uint32_t block_length);

  bool receiveMessage(PeerMessage &message, int timeout_seconds = 30);

  const PeerState &getState() const { return m_state; }
  const std::vector<bool> &getPeerPieces() const { return m_peer_pieces; }
  const std::string &getPeerId() const { return m_peer_id; }
  std::string getIp() const { return m_ip; }
  uint16_t getPort() const { return m_port; }

  size_t getPendingRequestCount() const { return m_peer_requests.size(); }
  bool getNextRequest(PeerRequest &request);
  void addPeerRequest(uint32_t piece_index, uint32_t block_offset,
                      uint32_t block_length);

private:
  bool sendData(const uint8_t *data, size_t length);
  bool receiveData(uint8_t *buffer, size_t length, int timeout_seconds);

  std::vector<uint8_t> serializeMessage(const PeerMessage &message) const;

  std::vector<uint8_t> buildHandshake() const;
  bool parseHandshake(const uint8_t *data);
};
