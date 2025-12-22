#include "upload_manager.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <ios>
#include <iostream>
#include <vector>

UploadManager::UploadManager(const std::string &download_dir,
                             const TorrentMetadata &metadata,
                             const PieceInformation &piece_info,
                             const PieceFileMapping &file_mapping)
    : m_download_dir(download_dir), m_metadata(metadata),
      m_piece_info(piece_info), m_file_mapping(file_mapping),
      m_uploaded_bytes(0) {}

void UploadManager::addPeer(PeerConnection *peer) {
  if (peer && peer->isConnected()) {
    m_peers.push_back(peer);
  }
}

bool UploadManager::readPieceFromDisk(uint32_t piece_index,
                                      std::vector<uint8_t> &piece_data) {
  if (piece_index >= m_piece_info.totalPieces()) {
    return false;
  }

  uint32_t piece_size;
  if (piece_index == m_piece_info.totalPieces() - 1) {
    piece_size = m_piece_info.last_piece_size;
  } else {
    piece_size = m_piece_info.piece_length;
  }

  piece_data.resize(piece_size);

  if (piece_index >= m_file_mapping.piece_to_file_map.size()) {
    return false;
  }

  const auto &segments = m_file_mapping.piece_to_file_map[piece_index];

  for (const auto &segment : segments) {
    if (segment.file_index >= m_metadata.files.size()) {
      return false;
    }

    const auto &file_info = m_metadata.files[segment.file_index];

    std::string file_path = m_download_dir;
    for (const auto &path_component : file_info.path) {
      file_path += "/" + path_component;
    }

    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
      std::cerr << "Cannot open file for reading: " << file_path << "\n";
      return false;
    }

    uint64_t file_start_in_torrent = 0;
    for (size_t i = 0; i < segment.file_index; i++) {
      file_start_in_torrent += m_metadata.files[i].length;
    }

    uint64_t piece_start_in_torrent =
        piece_index * static_cast<uint64_t>(m_piece_info.piece_length);
    uint64_t segment_start_in_torrent =
        file_start_in_torrent + segment.file_offset;
    uint32_t offset_in_piece = static_cast<uint32_t>(segment_start_in_torrent -
                                                     piece_start_in_torrent);

    file.seekg(segment.file_offset, std::ios::beg);

    file.read(reinterpret_cast<char *>(piece_data.data() + offset_in_piece),
              segment.segment_length);

    if (!file.good() && !file.eof()) {
      std::cerr << "Error reading from files\n";
      file.close();
      return false;
    }

    file.close();
  }

  return true;
}

bool UploadManager::readBlockFromDisk(uint32_t piece_index,
                                      uint32_t block_offset,
                                      uint32_t block_length,
                                      std::vector<uint8_t> &block_data) {
  std::vector<uint8_t> piece_data;

  if (!readPieceFromDisk(piece_index, piece_data)) {
    return false;
  }

  if (block_offset + block_length > piece_data.size()) {
    std::cerr << "Block request out of bounds\n";
    return false;
  }

  block_data.resize(block_length);
  std::memcpy(block_data.data(), piece_data.data() + block_offset,
              block_length);

  return true;
}

void UploadManager::processUploads() {
  for (auto *peer : m_peers) {
    if (!peer->isConnected() || !peer->isHandshakeComplete()) {
      continue;
    }

    handlePeerRequests(peer);
  }
}

void UploadManager::handlePeerRequests(PeerConnection *peer) {
  const auto &state = peer->getState();

  if (state.am_choking) {
    return;
  }

  PeerRequest request(0, 0, 0);
  while (peer->getNextRequest(request)) {
    std::vector<uint8_t> block_data;

    if (!readBlockFromDisk(request.piece_index, request.block_offset,
                           request.block_length, block_data)) {
      std::cerr << "  Failed to read block for upload\n";
      continue;
    }

    if (peer->sendPiece(request.piece_index, request.block_offset,
                        block_data)) {
      m_uploaded_bytes += block_data.size();

      std::cout << "  ↑ Uploaded block: piece " << request.piece_index
                << ", offset " << request.block_offset << ", size "
                << block_data.size() << " bytes"
                << " to " << peer->getIp() << ":" << peer->getPort() << "\n";
    } else {
      std::cerr << "  Failed to send PIECE message\n";
    }
  }
}

// uint64_t getUploadedBytes() const { return m_uploaded_bytes; }
