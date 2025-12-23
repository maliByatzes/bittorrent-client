#pragma once

#include "peer_connection.h"
#include "torrent_file.h"
#include "magnet_link.h"
#include "bdecoder.h"
#include <vector>
#include <array>
#include <cstdint>

class MetadataFetcher {
private:
  std::array<uint8_t, 20> m_info_hash;
  std::vector<PeerConnection*> m_peers;
  
  std::vector<std::vector<uint8_t>> m_metadata_pieces;
  std::vector<bool> m_pieces_recieved;

  size_t m_total_metadata_size;
  size_t m_metadata_piece_size;
  size_t m_num_pieces;

  bool m_metadata_complete;

public:
  MetadataFetcher(const std::array<uint8_t, 20>& info_hash);

  void addPeer(PeerConnection *peer);
  
  bool fetchMetadata();
  bool isComplete() const { return m_metadata_complete; }
  bool reconstructMetadata(TorrentMetadata& metadata,
                           PieceInformation& piece_info,
                           PieceFileMapping& file_mapping);

private:
  bool requestNextPiece();
  bool handleMetadataMessage(PeerConnection *peer, const PeerMessage& msg);
  bool verifyMetadata(const std::vector<uint8_t>& full_metadata);
};