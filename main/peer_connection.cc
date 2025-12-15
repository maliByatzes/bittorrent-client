#include "peer_connection.h"
#include <algorithm>
#include <arpa/inet.h>
#include <array>
#include <asm-generic/socket.h>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <poll.h>
#include <string>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

PeerConnection::PeerConnection(const std::string &ip, uint16_t port,
                               const std::array<uint8_t, 20> &info_hash,
                               const std::string &our_peer_id)
    : m_ip(ip), m_port(port), m_socket(-1), m_info_hash(info_hash),
      m_our_peer_id(our_peer_id), m_connected(false),
      m_handshake_complete(false) {}

PeerConnection::~PeerConnection() { disconnect(); }

bool PeerConnection::connect(int timeout_seconds) {
  if (m_connected) {
    return true;
  }

  m_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (m_socket < 0) {
    std::cerr << "Failed to create socket for " << m_ip << ":" << m_port
              << "\n";
    return false;
  }

  int flags = fcntl(m_socket, F_GETFL, 0);
  fcntl(m_socket, F_SETFL, flags | O_NONBLOCK);

  struct sockaddr_in peer_addr;
  std::memset(&peer_addr, 0, sizeof(peer_addr));
  peer_addr.sin_family = AF_INET;
  peer_addr.sin_port = htons(m_port);

  if (inet_pton(AF_INET, m_ip.c_str(), &peer_addr.sin_addr) <= 0) {
    std::cerr << "Invalid IP address: " << m_ip << "\n";
    close(m_socket);
    m_socket = -1;
    return false;
  }

  int result =
      ::connect(m_socket, (struct sockaddr *)&peer_addr, sizeof(peer_addr));

  if (result < 0 && errno != EINPROGRESS) {
    std::cerr << "Connection failed to " << m_ip << ":" << m_port << "\n";
    close(m_socket);
    m_socket = -1;
    return false;
  }

  struct pollfd pfd;
  pfd.fd = m_socket;
  pfd.events = POLLOUT;

  int poll_result = poll(&pfd, 1, timeout_seconds * 1000);
  if (poll_result <= 0) {
    std::cerr << "Connection timeout to " << m_ip << ":" << m_port << "\n";
    close(m_socket);
    m_socket = -1;
    return false;
  }

  int sock_error = 0;
  socklen_t len = sizeof(sock_error);
  getsockopt(m_socket, SOL_SOCKET, SO_ERROR, &sock_error, &len);

  if (sock_error != 0) {
    std::cerr << "Connection error to " << m_ip << ":" << m_port << "\n";
    close(m_socket);
    m_socket = -1;
    return false;
  }

  m_connected = true;
  std::cout << "✔️ Connected to " << m_ip << ":" << m_port << "\n";
  return true;
}

void PeerConnection::disconnect() {
  if (m_socket >= 0) {
    close(m_socket);
    m_socket = -1;
  }
  m_connected = false;
  m_handshake_complete = false;
}

std::vector<uint8_t> PeerConnection::buildHandshake() const {
  std::vector<uint8_t> handshake;

  // protocol name length (1 byte) = 19
  handshake.push_back(19);

  // protocol name (19 bytes) = "BitTorrent protocol"
  std::string protocol_name{"BitTorrent protocol"};
  handshake.insert(handshake.end(), protocol_name.begin(), protocol_name.end());

  // reserved bytes (8 bytes) = 0s
  for (int i = 0; i < 8; i++) {
    handshake.push_back(0);
  }

  // info hash (20 bytes)
  handshake.insert(handshake.begin(), m_info_hash.begin(), m_info_hash.end());

  // peer id (20 bytes)
  handshake.insert(handshake.begin(), m_our_peer_id.begin(),
                   m_our_peer_id.end());

  return handshake;
}

bool PeerConnection::parseHandshake(const uint8_t *data) {
  if (data[0] != 19) {
    std::cerr << "Invalid handshake: wrong protocol name length\n";
    return false;
  }

  std::string protocol(reinterpret_cast<const char *>(data + 1), 19);
  if (protocol != "BitTorrent protocol") {
    std::cerr << "Invalid handshake: wrong protocol name. Expected 'BitTorrent "
                 "protocol', got '"
              << protocol << "'\n";
    return false;
  }

  std::array<uint8_t, 20> peer_info_hash;
  std::memcpy(peer_info_hash.data(), data + 28, 20);

  if (peer_info_hash != m_info_hash) {
    std::cerr << "Invalid handshake: info hash mismatch.\n";
    return false;
  }

  m_peer_id = std::string(reinterpret_cast<const char *>(data + 48), 20);

  return true;
}

bool PeerConnection::performHandshake() {
  if (!m_connected) {
    std::cerr << "Cannot handshake: not connected.\n";
    return false;
  }

  if (m_handshake_complete) {
    return true;
  }

  std::vector<uint8_t> handshake{buildHandshake()};
  if (!sendData(handshake.data(), handshake.size())) {
    std::cerr << "Failed to send handshake\n";
    return false;
  }

  std::cout << "  → Sent handshake\n";

  uint8_t peer_handshake[68];
  if (!receiveData(peer_handshake, 68, 10)) {
    std::cerr << "Failed to recieve handshake\n";
    return false;
  }

  std::cout << "  ← Received handshake\n";

  if (!parseHandshake(peer_handshake)) {
    return false;
  }

  m_handshake_complete = true;
  std::cout << "  ✓ Handshake complete with peer ID: ";

  for (int i = 0; i < std::min(20, (int)m_peer_id.length()); i++) {
    if (std::isprint(m_peer_id[i])) {
      std::cout << m_peer_id[i];
    } else {
      std::cout << '.';
    }
  }
  std::cout << "\n";

  return true;
}

// bool isHandshakeComplete() const { return m_handshake_complete; }

// bool sendKeepAlive();
// bool sendChoke();
// bool sendUnchoke();
// bool sendInterested();
// bool sendNotInterested();
// bool sendHave(uint32_t piece_index);
// bool sendBitfield(const std::vector<bool> &pieces);
// bool sendRequest(uint32_t piece_index, uint32_t block_offset,
//                   uint32_t block_length);
// bool sendPiece(uint32_t piece_index, uint32_t block_offset,
//                std::vector<uint8_t> &block_data);
// bool sendCancel(uint32_t piece_index, uint32_t block_offset,
//                 uint32_t block_length);

// bool receiveMessage(PeerMessage &message, int timeout_seconds = 30);

// bool sendData(const uint8_t *data, size_t length);
// bool receiveData(uint8_t *buffer, size_t length, int timeout_seconds);

// std::vector<uint8_t> serializeMessage(const PeerMessage &message) const;
