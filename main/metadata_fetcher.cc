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

bool MetadataFetcher::handleMetadataMessage(PeerConnection *peer, const PeerMessage& msg) {
  if (msg.payload.size() < 2) {
    return false;
  }

  std::string data(msg.payload.begin() + 1, msg.payload.end());

  try
  {
    size_t dict_end = 0;
    int depth = 0;
    for (size_t i = 0; i < data.size(); i++) {
      if (data[i] == 'd') depth++;
      if (data[i] == 'e') {
        depth--;
        if (depth == 0) {
          dict_end = i + 1;
          break;
        }
      }
    }

    if (dict_end == 0) { return false; }

    std::string dict_str = data.substr(0, dict_end);
    BNode response = bdecode(dict_str);

    if (!response.isDictionary()) {
      return false;
    }

    int msg_type = static_cast<int>(response["msg_type"].asInteger());

    if (msg_type == 1) {
      int piece_index = static_cast<int>(response["piece"].asInteger());

      if (response.asDict().count("total_size")) {
        m_total_metadata_size = static_cast<size_t>(
          response["total_size"].asInteger()
        );
      }

      std::vector<uint8_t> piece_data(data.begin() + dict_end, data.end());

      if (piece_index >= 0 && piece_index < (int)m_num_pieces) {
        if (!m_pieces_recieved[piece_index]) {
          m_metadata_pieces[piece_index] = piece_data;
          m_pieces_recieved[piece_index] = true;

          std::cout << "  ✓ Received metadata piece " << piece_index
                    << "/" << m_num_pieces << "\n";

          bool all_received = true;
          for (bool received : m_pieces_recieved) {
            if (!received) {
              all_received = false;
              break;
            }
          }

          if (all_received) {
            std::vector<uint8_t> full_metadata;
            for (const auto& piece : m_metadata_pieces) {
              full_metadata.insert(full_metadata.end(), piece.begin(), piece.end());
            }

            if (verifyMetadata(full_metadata)) {
              m_metadata_complete = true;
            }
          }

          return true;
        }
      }
    } else if (msg_type == 2) {
      std::cerr << "  Peer rejected metadata request\n";
    }
  }
  catch(const std::exception& e)
  {
    std::cerr << "Failed to parse metadata message: " << e.what() << '\n';
  }
  
  return false;
}

bool MetadataFetcher::requestNextPiece() {
  for (size_t i = 0; i < m_pieces_recieved.size(); i++) {
    if (!m_pieces_recieved[i]) {
      for (auto* peer : m_peers) {
        if (peer->supportsExtensions()) {
          peer->requestMetadataPiece(i);
          return true;
        }
      }
    }
  }
  return false;
}

bool MetadataFetcher::verifyMetadata(const std::vector<uint8_t>& full_metadata) {
  std::array<uint8_t, 20> calculated_hash = sha1ToBytes(
    const_cast<std::vector<uint8_t>&>(full_metadata)
  );

  if (calculated_hash != m_info_hash) {
    std::cerr << "Metadata verification failed: hash mismatch\n";
    return false;
  }

  std::cout << "✓ Metadata verified (hash matches)\n";
  return true;
}