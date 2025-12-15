#include "peer_connection.h"
#include <algorithm>
#include <arpa/inet.h>
#include <array>
#include <asm-generic/socket.h>
#include <cctype>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <poll.h>
#include <string>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
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

bool PeerConnection::sendData(const uint8_t *data, size_t length) {
  if (!m_connected || m_socket < 0) {
    return false;
  }

  size_t total_sent = 0;
  while (total_sent < length) {
    ssize_t sent = send(m_socket, data + total_sent, length - total_sent, 0);

    if (sent < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        struct pollfd pfd;
        pfd.fd = m_socket;
        pfd.events = POLLOUT;
        poll(&pfd, 1, 1000);
        continue;
      }
      std::cerr << "Send error: " << strerror(errno) << "\n";
      return false;
    }

    if (sent == 0) {
      std::cerr << "Connection closed by peer\n";
      return false;
    }

    total_sent += sent;
  }

  return true;
}

bool PeerConnection::receiveData(uint8_t *buffer, size_t length,
                                 int timeout_seconds) {
  if (!m_connected || m_socket < 0) {
    return false;
  }

  size_t total_received = 0;

  while (total_received < length) {
    struct pollfd pfd;
    pfd.fd = m_socket;
    pfd.events = POLLIN;

    int poll_result = poll(&pfd, 1, timeout_seconds * 1000);

    if (poll_result == 0) {
      std::cerr << "Recieve timeout\n";
      return false;
    }

    if (poll_result < 0) {
      std::cerr << "Poll error: " << strerror(errno) << "\n";
      return false;
    }

    ssize_t received =
        recv(m_socket, buffer + total_received, length - total_received, 0);

    if (received < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        continue;
      }
      std::cerr << "Receive error: " << strerror(errno) << "\n";
      return false;
    }

    if (received == 0) {
      std::cerr << "Connection closed by peer\n";
      return false;
    }

    total_received += received;
  }

  return true;
}

std::vector<uint8_t>
PeerConnection::serializeMessage(const PeerMessage &message) const {
  std::vector<uint8_t> data;

  if (message.type == MessageType::KEEP_ALIVE) {
    data.push_back(0);
    data.push_back(0);
    data.push_back(0);
    data.push_back(0);
    return data;
  }

  uint32_t message_length = 1 + message.payload.size();

  data.push_back((message_length >> 24U) & 0xFFU);
  data.push_back((message_length >> 16U) & 0xFFU);
  data.push_back((message_length >> 8U) & 0xFFU);
  data.push_back(message_length & 0xFFU);

  data.push_back(static_cast<uint8_t>(message.type));

  data.insert(data.end(), message.payload.begin(), message.payload.end());

  return data;
}

bool PeerConnection::sendKeepAlive() {
  PeerMessage msg(MessageType::KEEP_ALIVE);
  std::vector<uint8_t> data{serializeMessage(msg)};
  return sendData(data.data(), data.size());
}

bool PeerConnection::sendChoke() {
  PeerMessage msg(MessageType::CHOKE);
  std::vector<uint8_t> data{serializeMessage(msg)};

  if (sendData(data.data(), data.size())) {
    m_state.am_choking = true;
    return true;
  }

  return false;
}

bool PeerConnection::sendUnchoke() {
  PeerMessage msg(MessageType::UNCHOKE);
  std::vector<uint8_t> data{serializeMessage(msg)};

  if (sendData(data.data(), data.size())) {
    m_state.am_choking = false;
    return true;
  }

  return false;
}

bool PeerConnection::sendInterested() {
  PeerMessage msg(MessageType::INTERESTED);
  std::vector<uint8_t> data{serializeMessage(msg)};

  if (sendData(data.data(), data.size())) {
    m_state.am_interested = true;
    return true;
  }

  return false;
}

bool PeerConnection::sendNotInterested() {
  PeerMessage msg(MessageType::NOT_INTERESTED);
  auto data{serializeMessage(msg)};

  if (sendData(data.data(), data.size())) {
    m_state.am_interested = false;
    return true;
  }

  return false;
}

bool PeerConnection::sendHave(uint32_t piece_index) {
  std::vector<uint8_t> payload(4);
  payload[0] = (piece_index >> 24U) & 0xFFU;
  payload[1] = (piece_index >> 16U) & 0xFFU;
  payload[2] = (piece_index >> 8U) & 0xFFU;
  payload[3] = piece_index & 0xFFU;

  PeerMessage msg(MessageType::HAVE, std::move(payload));
  auto data{serializeMessage(msg)};
  return sendData(data.data(), data.size());
}

bool PeerConnection::sendBitfield(const std::vector<bool> &pieces) {
  size_t num_bytes = (pieces.size() + 7) / 8;
  std::vector<uint8_t> payload(num_bytes, 0);

  for (size_t i = 0; i < pieces.size(); i++) {
    if (pieces[i]) {
      size_t byte_index = i / 8;
      size_t bit_index = 7 - (i % 8);
      payload[byte_index] |= (1U << bit_index);
    }
  }

  PeerMessage msg(MessageType::BIT_FIELD, std::move(payload));
  auto data{serializeMessage(msg)};
  return sendData(data.data(), data.size());
}

bool PeerConnection::sendRequest(uint32_t piece_index, uint32_t block_offset,
                                 uint32_t block_length) {
  std::vector<uint8_t> payload(12);

  payload[0] = (piece_index >> 24U) & 0xFFU;
  payload[1] = (piece_index >> 16U) & 0xFFU;
  payload[2] = (piece_index >> 8U) & 0xFFU;
  payload[3] = piece_index & 0xFFU;

  payload[4] = (block_offset >> 24U) & 0xFFU;
  payload[5] = (block_offset >> 16U) & 0xFFU;
  payload[6] = (block_offset >> 8U) & 0xFFU;
  payload[7] = block_offset & 0xFFU;

  payload[8] = (block_length >> 24U) & 0xFFU;
  payload[9] = (block_length >> 16U) & 0xFFU;
  payload[10] = (block_length >> 8U) & 0xFFU;
  payload[11] = block_length & 0xFFU;

  PeerMessage msg(MessageType::REQUEST, std::move(payload));
  auto data{serializeMessage(msg)};
  return sendData(data.data(), data.size());
}

bool PeerConnection::sendPiece(uint32_t piece_index, uint32_t block_offset,
                               std::vector<uint8_t> &block_data) {
  std::vector<uint8_t> payload(8 + block_data.size());

  payload[0] = (piece_index >> 24U) & 0xFFU;
  payload[1] = (piece_index >> 16U) & 0xFFU;
  payload[2] = (piece_index >> 8U) & 0xFFU;
  payload[3] = piece_index & 0xFFU;

  payload[4] = (block_offset >> 24U) & 0xFFU;
  payload[5] = (block_offset >> 16U) & 0xFFU;
  payload[6] = (block_offset >> 8U) & 0xFFU;
  payload[7] = block_offset & 0xFFU;

  std::memcpy(payload.data() + 8, block_data.data(), block_data.size());

  PeerMessage msg(MessageType::PIECE, std::move(payload));
  auto data{serializeMessage(msg)};
  return sendData(data.data(), data.size());
}

bool PeerConnection::sendCancel(uint32_t piece_index, uint32_t block_offset,
                                uint32_t block_length) {
  std::vector<uint8_t> payload(12);

  payload[0] = (piece_index >> 24U) & 0xFFU;
  payload[1] = (piece_index >> 16U) & 0xFFU;
  payload[2] = (piece_index >> 8U) & 0xFFU;
  payload[3] = piece_index & 0xFFU;

  payload[4] = (block_offset >> 24U) & 0xFFU;
  payload[5] = (block_offset >> 16U) & 0xFFU;
  payload[6] = (block_offset >> 8U) & 0xFFU;
  payload[7] = block_offset & 0xFFU;

  payload[8] = (block_length >> 24U) & 0xFFU;
  payload[9] = (block_length >> 16U) & 0xFFU;
  payload[10] = (block_length >> 8U) & 0xFFU;
  payload[11] = block_length & 0xFFU;

  PeerMessage msg(MessageType::CANCEL, std::move(payload));
  auto data{serializeMessage(msg)};
  return sendData(data.data(), data.size());
}

// bool receiveMessage(PeerMessage &message, int timeout_seconds = 30);
