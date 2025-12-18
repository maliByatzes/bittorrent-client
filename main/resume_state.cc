#include "resume_state.h"
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

ResumeState::ResumeState(const std::string &info_hash_hex,
                         const std::string &torrent_path, size_t total_pieces)
    : m_info_hash_hex(info_hash_hex), m_torrent_path(torrent_path),
      m_downloaded_bytes(0), m_uploaded_bytes(0) {
  m_completed_pieces.resize(total_pieces, false);
}

bool ResumeState::load(const std::string &resume_dir) {
  m_resume_file_path = resume_dir + "/" + m_info_hash_hex + ".resume";

  std::ifstream file(m_resume_file_path);
  if (!file.is_open()) {
    std::cout << "No resume found (starting a fresh download)\n";
    return false;
  }

  std::cout << "Loading resume state from: " << m_resume_file_path << "\n";

  std::string line;
  std::string loaded_hash;
  size_t total_pieces = 0;
  std::vector<uint32_t> completed_list;

  while (std::getline(file, line)) {
    if (line.empty() || line[0] == '#')
      continue;

    size_t eq_pos = line.find('=');
    if (eq_pos == std::string::npos)
      continue;

    std::string key = line.substr(0, eq_pos);
    std::string value = line.substr(eq_pos + 1);

    if (key == "info_hash") {
      loaded_hash = value;
    } else if (key == "torrent_path") {

    } else if (key == "total_pieces") {
      total_pieces = std::stoull(value);
    } else if (key == "downloaded_bytes") {
      m_downloaded_bytes = std::stoull(value);
    } else if (key == "uploaded_bytes") {
      m_uploaded_bytes = std::stoull(value);
    } else if (key == "completed_pieces") {
      std::istringstream ss(value);
      std::string piece_str;
      while (std::getline(ss, piece_str, ',')) {
        if (!piece_str.empty()) {
          completed_list.push_back(std::stoul(piece_str));
        }
      }
    }
  }

  file.close();

  if (loaded_hash != m_info_hash_hex) {
    std::cerr << "Resume file info hash mismatch!\n";
    return false;
  }

  if (total_pieces != m_completed_pieces.size()) {
    std::cerr << "Resume file piece count mismatch!\n";
    return false;
  }

  for (uint32_t piece_idx : completed_list) {
    if (piece_idx < m_completed_pieces.size()) {
      m_completed_pieces[piece_idx] = true;
    }
  }

  std::cout << "Resume state loaded: " << getCompletedPieceCount() << "/"
            << m_completed_pieces.size() << " pieces complete\n";

  return true;
}

// bool save(const std::string &resume_dir = "./.resume");

// void markPieceComplete(uint32_t piece_index);
// bool isPieceComplete(uint32_t piece_index) const;
// std::vector<uint32_t> getCompletedPieces() const;

// void setDownloadedBytes(uint64_t bytes) { m_downloaded_bytes = bytes; }
// void setUploadedBytes(uint64_t bytes) { m_uploaded_bytes = bytes; }

// uint64_t getDownloadedBytes() const { return m_downloaded_bytes; }
// uint64_t getUploadedBytes() const { return m_uploaded_bytes; }

// double getProgress() const;
// size_t getCompletedPieceCount() const;
