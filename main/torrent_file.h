#pragma once

#include "bdecoder.h"
#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

struct FileInfo {
  std::vector<std::string> path;
  uint64_t length;
};

struct TorrentMetadata {
  std::vector<std::string> announce_urls;

  std::array<uint8_t, 20> info_hash_bytes;
  std::string info_hash_hex;
  std::string info_hash_urlencoded;

  uint32_t piece_length;
  uint64_t total_size;

  std::string name;
  std::vector<FileInfo> files;

  std::string comment;
  std::string created_by;
  uint64_t creation_date;

  bool isSingleFile() const {
    return files.size() == 1 && files[0].path.size() == 1;
  }
};

struct PieceInformation {
  std::vector<std::array<uint8_t, 20>> hashes;

  uint32_t piece_length;
  uint32_t last_piece_size;

  size_t totalPieces() const { return hashes.size(); }

  const std::array<uint8_t, 20> &getHash(size_t piece_index) const {
    return hashes.at(piece_index);
  }
};

struct PieceFileSegment {
  size_t file_index;
  uint64_t file_offset;
  uint32_t segment_length;
};

struct PieceFileMapping {
  std::vector<std::vector<PieceFileSegment>> piece_to_file_map;
};

class TorrentFile {
private:
  std::string m_file_name;
  std::vector<uint8_t> m_file_bytes;

  TorrentMetadata m_metadata;
  PieceInformation m_piece_info;
  PieceFileMapping m_file_mapping;

  void readFile();
  void extractMetadata(const BNode &root);
  void extractPieceInfo(const BNode &info);
  void buildFileMapping();

public:
  TorrentFile(const std::string file_name)
      : m_file_name(std::move(file_name)) {}

  void parse();

  const TorrentMetadata &getMetadata() const { return m_metadata; }
  const PieceInformation &getPieceInfo() const { return m_piece_info; }
  const PieceFileMapping &getFileMapping() const { return m_file_mapping; }
};
