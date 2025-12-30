#pragma once

#include <mutex>
#include <string>
#include <cstdint>

class TUIState {
private:
  mutable std::mutex m_mutex;

  std::string m_filename;
  uint64_t m_total_size;

  double m_progress;
  uint64_t m_downloaded_bytes;
  uint64_t m_uploaded_bytes;

  double m_download_speed;
  double m_upload_speed;

  int m_peer_count;

  std::string m_status;

  int m_total_pieces;
  int m_completed_pieces;

  int m_eta_seconds;

public:
  TUIState();

  void setFilename(const std::string& name);
  void setTotalSize(uint64_t size);
  void setProgress(double progress);
  void setDownloadedBytes(uint64_t bytes);
  void setUploadedBytes(uint64_t bytes);
  void setDownloadSpeed(double speed);
  void setUploadSpeed(double speed);
  void setPeerCount(int count);
  void setStatus(const std::string& status);
  void setPieceInfo(int total, int completed);
  void setETA(int seconds);

  std::string getFilename() const;
  uint64_t getTotalSize() const;
  double getProgress() const;
  uint64_t getDownloadedBytes() const;
  uint64_t getUploadedBytes() const;
  double getDownloadSpeed() const;
  double getUploadSpeed() const;
  int getPeerCount() const;
  std::string getStatus() const;
  int getTotalPieces() const;
  int getCompletedPieces() const;
  int getETA() const;
};