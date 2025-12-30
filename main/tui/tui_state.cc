#include "tui_state.h"

TUIState::TUIState() 
  : m_total_size(0),
    m_progress(0.0),
    m_downloaded_bytes(0),
    m_uploaded_bytes(0),
    m_download_speed(0.0),
    m_upload_speed(0.0),
    m_peer_count(0),
    m_status("Initializing"),
    m_total_pieces(0),
    m_completed_pieces(0),
    m_eta_seconds(0)
{
}

void TUIState::setFilename(const std::string& name) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_filename = name;
}

void TUIState::setTotalSize(uint64_t size) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_total_size = size;
}

void TUIState::setProgress(double progress) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_progress = progress;
}

void TUIState::setDownloadedBytes(uint64_t bytes) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_downloaded_bytes = bytes;
}

void TUIState::setUploadedBytes(uint64_t bytes) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_uploaded_bytes = bytes;
}

void TUIState::setDownloadSpeed(double speed) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_download_speed = speed;
}

void TUIState::setUploadSpeed(double speed) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_upload_speed = speed;
}

void TUIState::setPeerCount(int count) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_peer_count = count;
}

void TUIState::setStatus(const std::string& status) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_status = status;
}

void TUIState::setPieceInfo(int total, int completed) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_total_pieces = total;
  m_completed_pieces = completed;
}

void TUIState::setETA(int seconds) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_eta_seconds = seconds;
}

std::string TUIState::getFilename() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_filename;
}

uint64_t TUIState::getTotalSize() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_total_size;
}

double TUIState::getProgress() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_progress;
}

uint64_t TUIState::getDownloadedBytes() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_downloaded_bytes;
}

uint64_t TUIState::getUploadedBytes() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_uploaded_bytes;
}

double TUIState::getDownloadSpeed() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_download_speed;
}

double TUIState::getUploadSpeed() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_upload_speed;
}

int TUIState::getPeerCount() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_peer_count;
}

std::string TUIState::getStatus() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_status;
}

int TUIState::getTotalPieces() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_total_pieces;
}

int TUIState::getCompletedPieces() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_completed_pieces;
}

int TUIState::getETA() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_eta_seconds;
}