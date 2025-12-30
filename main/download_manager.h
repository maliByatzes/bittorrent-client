#pragma once

#include "peer_connection.h"
#include "resume_state.h"
#include "torrent_file.h"
#include "upload_manager.h"
#include "tui/tui_state.h"
#include <cstdint>
#include <map>
#include <memory>
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

struct DownloadTask {
  uint32_t piece_index;
  PeerConnection *peer;
  bool blocks_requested;
  bool complete;

  DownloadTask(uint32_t idx, PeerConnection *p)
      : piece_index(idx), peer(p), blocks_requested(false), complete(false) {}
};

class DownloadManager {
private:
  static const uint32_t BLOCK_SIZE;
  static const int MAX_CONCURRENT_PIECES;
  static const int RANDOM_FIRST_COUNT;

  TorrentMetadata m_metadata;
  PieceInformation m_piece_info;
  PieceFileMapping m_file_mapping;

  std::string m_download_dir;
  std::vector<PieceDownload> m_pieces;
  std::vector<PeerConnection *> m_peers;

  uint64_t m_downloaded_bytes;
  uint64_t m_uploaded_bytes;

  std::map<uint32_t, PeerConnection *> m_piece_assignments;
  std::vector<DownloadTask> m_active_tasks;
  std::vector<int> m_piece_availability;
  std::vector<uint32_t> m_random_first_pieces;

  ResumeState *m_resume_state;
  bool m_use_resume;

  UploadManager *m_upload_manager;

  std::shared_ptr<TUIState> m_tui_state;

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

  bool downloadParallel();
  int getNextPieceToDownload();
  bool isComplete() const;
  std::vector<uint32_t> getAvailablePiecesForPeer(PeerConnection *peer);

  bool downloadRarestFirst();

  void setResumeEnabled(bool enabled) { m_use_resume = enabled; }
  bool loadResumeState();
  bool saveResumeState();

  void setTUIState(std::shared_ptr<TUIState> state) {
    m_tui_state = state;
  }

private:
  bool requestBlocksForPiece(PeerConnection *peer, uint32_t piece_index);
  bool receivePieceData(PeerConnection *peer, uint32_t piece_index);
  PeerConnection *findAvailablePeer(uint32_t piece_index);
  void createDirectoryStructure();

  void processActiveTasks();
  bool handleTaskMessage(DownloadTask &task);
  bool startPieceDownload(uint32_t piece_index, PeerConnection *peer);

  void updatePieceAvailability();
  int getNextRarestPiece();
};
