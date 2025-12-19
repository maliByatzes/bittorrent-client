#pragma once

#include "peer_connection.h"
#include "torrent_file.h"
#include <cstdint>
#include <string>
#include <vector>

class UploadManager {
private:
  std::string m_download_dir;
  TorrentMetadata m_metadata;
  PieceInformation m_piece_info;
  PieceFileMapping m_file_mapping;

  std::vector<PeerConnection *> m_peers;

  uint64_t m_uploaded_bytes;

  bool readPieceFromDisk(uint32_t piece_index,
                         std::vector<uint8_t> &piece_data);
  bool readBlockFromDisk(uint32_t piece_index, uint32_t block_offset,
                         uint32_t block_length,
                         std::vector<uint8_t> &block_data);

public:
  UploadManager(const std::string &download_dir,
                const TorrentMetadata &metadata,
                const PieceInformation &piece_info,
                const PieceFileMapping &file_mapping);

  void addPeer(PeerConnection *peer);
  void processUploads();
  void handlePeerRequests(PeerConnection *peer);
  uint64_t getUploadedBytes() const { return m_uploaded_bytes; }
};
