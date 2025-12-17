#pragma once

#include "peer_connection.h"
#include "torrent_file.h"
#include <cstdint>
#include <vector>

enum class PieceState { NOT_STARTED, IN_PROGRESS, COMPLETE, VERIFIED };

struct Block {
  uint32_t offset;
  uint32_t length;
  bool requested;
  bool received;
  std::vector<uint8_t> data;

  Block(uint32_t off, uint32_t len)
      : offset(off), length(len), requested(false), received(false) {}
};

struct PieceDownload {
  uint32_t piece_index;
  PieceState state;
  std::vector<Block> blocks;
  std::vector<uint8_t> piece_data;

  PieceDownload(uint32_t idx, uint32_t piece_size, uint32_t block_size = 16384);

  bool isComplete() const;
  int blocksReceived() const;
  int totalBlocks() const;
};

class DownloadManager {
private:
  TorrentMetadata m_metadata;
  PieceInformation m_piece_info;
  PieceFileMapping m_file_mapping;

  std::string m_download_dir;
  std::vector<PieceDownload> m_pieces;
  std::vector<PeerConnection *> m_peers;

  uint64_t m_downloaded_bytes;
  uint64_t m_uploaded_bytes;

  static const uint32_t BLOCK_SIZE = 16384;

public:
  DownloadManager(const TorrentMetadata &metadata,
                  const PieceInformation &piece_info,
                  const PieceFileMapping &file_mapping,
                  const std::string &download_dir = ".");

  ~DownloadManager();

  void addPeer(PeerConnection *peer);
  bool downloadSequential();
  bool downloadPiece(uint32_t piece_index);
  bool verifyPiece(uint32_t piece_index);
  bool writePieceToDisk(uint32_t piece_index);

  double getProgress() const;
  uint64_t getDownloadedBytes() const { return m_downloaded_bytes; }
  uint64_t getUploadedBytes() const { return m_uploaded_bytes; }

private:
  bool requestBlocksForPiece(PeerConnection *peer, uint32_t piece_index);
  bool receivePieceData(PeerConnection *peer, uint32_t piece_index);
  PeerConnection *findAvailablePeer(uint32_t piece_index);
  void createDirectoryStructure();
};
