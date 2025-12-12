#pragma once

#include "torrent_file.h"
#include <array>
#include <cstdint>
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

public:
  PeerConnection(const std::string &ip, uint16_t port,
                 const std::array<uint8_t, 20> &info_hash,
                 const std::string &our_peer_id);

  ~PeerConnection();
};
