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

// DownloadManager::DownloadManager(const TorrentMetadata &metadata,
//                                  const PieceInformation &piece_info,
//                                  const PieceFileMapping &file_mapping,
//                                  const std::string &download_dir) {}

// ~DownloadManager();

// void addPeer(PeerConnection *peer);
// bool downloadSequential();
// bool downloadPiece(uint32_t piece_index);
// bool verifyPiece(uint32_t piece_index);
// bool writePieceToDisk(uint32_t piece_index);

// double getProgress() const;

// bool requestBlocksForPiece(PeerConnection *peer, uint32_t piece_index);
// bool receivePieceData(PeerConnection *peer, uint32_t piece_index);
// PeerConnection *findAvailablePeer(uint32_t piece_index);
// void createDirectoryStructure();
