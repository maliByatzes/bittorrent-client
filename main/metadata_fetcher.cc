#include "metadata_fetcher.h"
#include "utils.h"
#include <iostream>
#include <cstring>

MetadataFetcher::MetadataFetcher(const std::array<uint8_t, 20>& info_hash)
  : m_info_hash(info_hash),
    m_total_metadata_size(0),
    m_metadata_piece_size(16384),
    m_num_pieces(0),
    m_metadata_complete(false) {}

void MetadataFetcher::addPeer(PeerConnection *peer) {
  if (peer && peer->isConnected() && peer->supportsExtensions()) {
    m_peers.push_back(peer);
  }
}

bool MetadataFetcher::fetchMetadata() {
  if (m_peers.empty()) {
      std::cerr << "No peers with extension support available\n";
      return false;
  }

  std::cout << "\n🔍 Fetching matadata from peers...\n";
  std::cout << "Using " << m_peers.size() << " peer(s) with extension support\n\n";

  for (auto* peer : m_peers) {
      if (!peer->sendExtensionHandshake()) {
          std::cerr << "Failed to send extensions hanshake to "
                    << peer->getIp() << "\n";
      }
  }

  std::cout << "Waiting for extension handshakes...\n";
  for (auto *peer : m_peers) {
    PeerMessage msg(MessageType::KEEP_ALIVE);
    if (peer->receiveMessage(msg, 5)) {
      if (msg.type == MessageType::EXTENDED) {
        peer->handleExtensionMessage(msg);
      }
    }
  }

  PeerConnection *metadata_peer = nullptr;
  for (auto* peer : m_peers) {
    if (peer->supportsExtensions()) {
      metadata_peer = peer;
      break;
    }
  }

  if (!metadata_peer) {
    std::cout << "No peers support ut_metadata extensions\n";
    return false;
  }

  std::cout << "Requesting metadata info...\n";
  if (!metadata_peer->requestMetadataPiece(0)) {
    std::cerr << "Failed to request metadata piece 0\n";
    return false;
  }

  bool first_piece = true;
  int timeout_count = 0;
  const int MAX_TIMEOUTS = 10;

  while (!m_metadata_complete && timeout_count < MAX_TIMEOUTS) {
    bool progress = false;

    for (auto* peer : m_peers) {
      PeerMessage msg(MessageType::KEEP_ALIVE);
      if (peer->receiveMessage(msg, 1)) {
        if (msg.type == MessageType::EXTENDED) {
          if (handleMetadataMessage(peer, msg)) {
            progress = true;

            if (first_piece && m_total_metadata_size > 0) {
              m_num_pieces = (m_total_metadata_size + m_metadata_piece_size - 1)
                           / m_metadata_piece_size;

              m_metadata_pieces.resize(m_num_pieces);
              m_pieces_recieved.resize(m_num_pieces, false);

              std::cout << "Metadata size: " << m_total_metadata_size
                        << " bytes (" << m_num_pieces << " pieces)\n";

              first_piece = false;

              for (size_t i = 1; i < m_num_pieces; i++) {
                peer->requestMetadataPiece(i);
              }
            }
          }
        }
      }
    }

    if (!progress) {
      timeout_count++;
    } else {
      timeout_count = 0;
    }

    if (!first_piece) {
      requestNextPiece();
    }
  }

  if (!m_metadata_complete) {
    std::cerr << "Failed to fetch complete metadata (timeout)\n";
    return false;
  }

  std::cout << "✓ Metadata fetched successfully!\n\n";
  return true;
}