#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

class ResumeState {
private:
  std::string m_info_hash_hex;
  std::string m_torrent_path;
  std::vector<bool> m_completed_pieces;
  uint64_t m_downloaded_bytes;
  uint64_t m_uploaded_bytes;

  std::string m_resume_file_path;

public:
  ResumeState(const std::string &info_hash_hex, const std::string &torrent_path,
              size_t total_pieces);

  bool load(const std::string &resume_dir = "./.resume");
  bool save(const std::string &resume_dir = "./.resume");

  void markPieceComplete(uint32_t piece_index);
  bool isPieceComplete(uint32_t piece_index) const;
  std::vector<uint32_t> getCompletedPieces() const;

  void setDownloadedBytes(uint64_t bytes) { m_downloaded_bytes = bytes; }
  void setUploadedBytes(uint64_t bytes) { m_uploaded_bytes = bytes; }

  uint64_t getDownloadedBytes() const { return m_downloaded_bytes; }
  uint64_t getUploadedBytes() const { return m_uploaded_bytes; }

  double getProgress() const;
  size_t getCompletedPieceCount() const;
};
