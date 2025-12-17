#include "download_manager.h"
#include "utils.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>

PieceDownload::PieceDownload(uint32_t idx, uint32_t piece_size,
                             uint32_t block_size)
    : piece_index(idx), state(PieceState::NOT_STARTED) {
  uint32_t num_blocks = (piece_size + block_size - 1) / block_size;

  for (uint32_t i = 0; i < num_blocks; i++) {
    uint32_t offset = i * block_size;
    uint32_t length = block_size;

    if (offset + length > piece_size) {
      length = piece_size - offset;
    }

    blocks.emplace_back(offset, length);
  }

  piece_data.resize(piece_size);
}

bool PieceDownload::isComplete() const {
  for (const auto &block : blocks) {
    if (!block.received) {
      return false;
    }
  }
  return true;
}

int PieceDownload::blocksReceived() const {
  int count = 0;
  for (const auto &block : blocks) {
    if (block.received)
      count++;
  }
  return count;
}

int PieceDownload::totalBlocks() const { return blocks.size(); }

DownloadManager::DownloadManager(const TorrentMetadata &metadata,
                                 const PieceInformation &piece_info,
                                 const PieceFileMapping &file_mapping,
                                 const std::string &download_dir)
    : m_metadata(metadata), m_piece_info(piece_info),
      m_file_mapping(file_mapping), m_download_dir(download_dir),
      m_downloaded_bytes(0), m_uploaded_bytes(0) {
  size_t num_pieces = piece_info.totalPieces();

  for (size_t i = 0; i < num_pieces; i++) {
    uint32_t piece_size;

    if (i == num_pieces - 1) {
      piece_size = piece_info.last_piece_size;
    } else {
      piece_size = piece_info.piece_length;
    }

    m_pieces.emplace_back(i, piece_size, BLOCK_SIZE);
  }

  std::cout << "DownloadManager initialized:\n"
            << "  Total pieces: " << num_pieces << "\n"
            << "  Piece size: " << piece_info.piece_length << " bytes\n"
            << "  Block size: " << BLOCK_SIZE << " bytes\n"
            << "  Total size: " << metadata.total_size << " bytes\n";
}

DownloadManager::~DownloadManager() {
  // Do not delete peers, they are managed externally
}

void DownloadManager::addPeer(PeerConnection *peer) {
  if (peer && peer->isConnected() && peer->isHandshakeComplete()) {
    m_peers.push_back(peer);
    std::cout << "Addd peer: " << peer->getIp() << ":" << peer->getPort()
              << "\n";
  }
}

double DownloadManager::getProgress() const {
  if (m_metadata.total_size == 0)
    return 0.0;
  return (100.0 * m_downloaded_bytes) / m_metadata.total_size;
}

PeerConnection *DownloadManager::findAvailablePeer(uint32_t piece_index) {
  // Use of a greedy method for sequential, return the first available suitable
  // peer

  for (auto *peer : m_peers) {
    if (!peer->isConnected() || !peer->isHandshakeComplete()) {
      continue;
    }

    const auto &state = peer->getState();
    if (state.peer_choking) {
      continue;
    }

    const auto &peer_pieces = peer->getPeerPieces();
    if (piece_index < peer_pieces.size() && peer_pieces[piece_index]) {
      return peer;
    }
  }

  return nullptr;
}

// bool downloadSequential();
// bool downloadPiece(uint32_t piece_index);
// bool verifyPiece(uint32_t piece_index);
// bool writePieceToDisk(uint32_t piece_index);

// bool requestBlocksForPiece(PeerConnection *peer, uint32_t piece_index);
// bool receivePieceData(PeerConnection *peer, uint32_t piece_index);
// void createDirectoryStructure();
