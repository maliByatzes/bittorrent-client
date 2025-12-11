#include "torrent_file.h"
#include "bdecoder.h"
#include "utils.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <fstream>
#include <ios>
#include <iostream>
#include <stdexcept>
#include <string>

void TorrentFile::readFile() {
  std::ifstream file(m_file_name, std::ios::binary);

  if (!file.good()) {
    throw std::runtime_error("Cannot open this file: " + m_file_name);
  }

  file.unsetf(std::ios::skipws);
  file.seekg(0, std::ios::end);
  auto file_len = file.tellg();
  file.seekg(0, std::ios::beg);

  m_file_bytes.resize(static_cast<size_t>(file_len));
  file.read(reinterpret_cast<char *>(m_file_bytes.data()), file_len);

  if (file.gcount() != file_len) {
    throw std::runtime_error("Uh oh that did not read the entire file data.");
  }

  file.close();
}

void TorrentFile::extractMetadata(const BNode &root) {
  try {
    if (root.isDictionary() && root.asDict().count("announce")) {
      m_metadata.announce_urls.push_back(root["announce"].asString());
    }
  } catch (const std::exception &e) {
  }

  try {
    if (root.isDictionary() && root.asDict().count("announce-list")) {
      const auto &announce_list = root["announce-list"].asList();
      for (const auto &tier : announce_list) {
        for (const auto &url : tier.asList()) {
          m_metadata.announce_urls.push_back(url.asString());
        }
      }
    }
  } catch (const std::exception &e) {
  }

  const BNode &info = root["info"];

  std::vector<uint8_t> info_bytes = info.encodeToBytes();
  m_metadata.info_hash_bytes = sha1ToBytes(info_bytes);
  m_metadata.info_hash_hex = bytesToHex(m_metadata.info_hash_bytes);
  m_metadata.info_hash_urlencoded =
      bytesToURLEncoded(m_metadata.info_hash_bytes);

  m_metadata.piece_length =
      static_cast<uint32_t>(info["piece length"].asInteger());
  m_metadata.name = info["name"].asString();
  m_metadata.total_size = 0;

  if (info.isDictionary() && info.asDict().count("files")) {
    const auto &files_list = info["files"].asList();

    for (const auto &file_node : files_list) {
      FileInfo file_info;
      file_info.length = static_cast<uint64_t>(file_node["length"].asInteger());

      const auto &path_list = file_node["path"].asList();
      for (const auto &path_component : path_list) {
        file_info.path.push_back(path_component.asString());
      }

      m_metadata.files.push_back(file_info);
      m_metadata.total_size += file_info.length;
    }
  } else {
    FileInfo file_info;
    file_info.length = static_cast<uint64_t>(info["length"].asInteger());
    file_info.path.push_back(m_metadata.name);

    m_metadata.files.push_back(file_info);
    m_metadata.total_size = file_info.length;
  }

  try {
    if (root.isDictionary() && root.asDict().count("comment")) {
      m_metadata.comment = root["comment"].asString();
    }
  } catch (...) {
  }

  try {
    if (root.isDictionary() && root.asDict().count("created by")) {
      m_metadata.created_by = root["created by"].asString();
    }
  } catch (...) {
  }

  try {
    if (root.isDictionary() && root.asDict().count("creation date")) {
      m_metadata.creation_date =
          static_cast<uint64_t>(root["creation date"].asInteger());
    }
  } catch (...) {
  }
}

void TorrentFile::extractPieceInfo(const BNode &info) {
  m_piece_info.piece_length = m_metadata.piece_length;

  const std::string &pieces_string = info["pieces"].asString();
  size_t num_pieces = pieces_string.length() / 20;

  if (pieces_string.length() % 20 != 0) {
    throw std::runtime_error(
        "Invalid pieces string length, not a multiple of 20");
  }

  for (size_t i = 0; i < num_pieces; i++) {
    std::array<uint8_t, 20> hash;
    for (size_t j = 0; j < 20; j++) {
      hash[j] = static_cast<uint8_t>(pieces_string[i * 20 + j]);
    }
    m_piece_info.hashes.push_back(hash);
  }

  uint64_t last_piece_size = m_metadata.total_size % m_metadata.piece_length;
  if (last_piece_size == 0) {
    m_piece_info.last_piece_size = m_metadata.piece_length;
  } else {
    m_piece_info.last_piece_size = static_cast<uint32_t>(last_piece_size);
  }
}

void TorrentFile::buildFileMapping() {
  size_t num_pieces = m_piece_info.hashes.size();
  m_file_mapping.piece_to_file_map.resize(num_pieces);

  uint64_t current_byte = 0;

  for (size_t piece_idx = 0; piece_idx < num_pieces; piece_idx++) {
    uint32_t piece_size;
    if (piece_idx == num_pieces - 1) {
      piece_size = m_piece_info.last_piece_size;
    } else {
      piece_size = m_metadata.piece_length;
    }

    uint64_t piece_start = current_byte;
    uint64_t piece_end = piece_start + piece_size;

    uint64_t file_start_byte = 0;

    for (size_t file_idx = 0; file_idx < m_metadata.files.size(); file_idx++) {
      uint64_t file_end_byte =
          file_start_byte + m_metadata.files[file_idx].length;

      if (file_end_byte > piece_start && file_start_byte < piece_end) {
        PieceFileSegment segment;
        segment.file_index = file_idx;

        uint64_t overlap_start = std::max(piece_start, file_start_byte);
        uint64_t overlap_end = std::min(piece_end, file_end_byte);

        segment.file_offset = overlap_start - file_start_byte;
        segment.segment_length =
            static_cast<uint32_t>(overlap_end - overlap_start);

        m_file_mapping.piece_to_file_map[piece_idx].push_back(segment);
      }

      file_start_byte = file_end_byte;
    }

    current_byte += piece_size;
  }
}

void TorrentFile::parse() {
  std::cout << "Reading torrent file: " << m_file_name << "\n";

  readFile();

  std::string file_contents(m_file_bytes.begin(), m_file_bytes.end());
  BNode root = bdecode(file_contents);

  extractMetadata(root);
  extractPieceInfo(root["info"]);
  buildFileMapping();

  std::cout << "Torrent file parsed successfully.\n";
}
