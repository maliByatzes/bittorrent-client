#include "download_manager.h"
#include "peer_connection.h"
#include "utils.h"
#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <ios>
#include <iostream>
#include <random>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

const uint32_t DownloadManager::BLOCK_SIZE = 16384;
const int DownloadManager::MAX_CONCURRENT_PIECES = 3;
const int DownloadManager::RANDOM_FIRST_COUNT = 4;

PieceDownload::PieceDownload(uint32_t idx, uint32_t piece_size,
                             uint32_t block_size)
    : piece_index(idx), state(PieceState::NOT_STARTED) {
  uint32_t num_blocks = (piece_size + block_size - 1) / block_size;

  for (uint32_t i = 0; i < num_blocks; i++) {
    uint32_t offset = i * block_size;
    uint32_t length = block_size;

    if (offset + length > piece_size) {
      length = piece_size - offset;
    }

    blocks.emplace_back(offset, length);
  }

  piece_data.resize(piece_size);
}

bool PieceDownload::isComplete() const {
  for (const auto &block : blocks) {
    if (!block.received) {
      return false;
    }
  }
  return true;
}

int PieceDownload::blocksReceived() const {
  int count = 0;
  for (const auto &block : blocks) {
    if (block.received)
      count++;
  }
  return count;
}

int PieceDownload::totalBlocks() const { return blocks.size(); }

DownloadManager::DownloadManager(const TorrentMetadata &metadata,
                                 const PieceInformation &piece_info,
                                 const PieceFileMapping &file_mapping,
                                 const std::string &download_dir)
    : m_metadata(metadata), m_piece_info(piece_info),
      m_file_mapping(file_mapping), m_download_dir(download_dir),
      m_downloaded_bytes(0), m_uploaded_bytes(0), m_resume_state(nullptr),
      m_use_resume(true), m_upload_manager(nullptr), m_tui_state(nullptr) {
  size_t num_pieces = piece_info.totalPieces();

  for (size_t i = 0; i < num_pieces; i++) {
    uint32_t piece_size;

    if (i == num_pieces - 1) {
      piece_size = piece_info.last_piece_size;
    } else {
      piece_size = piece_info.piece_length;
    }

    m_pieces.emplace_back(i, piece_size, BLOCK_SIZE);
  }

  std::cout << "DownloadManager initialized:\n"
            << "  Total pieces: " << num_pieces << "\n"
            << "  Piece size: " << piece_info.piece_length << " bytes\n"
            << "  Block size: " << BLOCK_SIZE << " bytes\n"
            << "  Total size: " << metadata.total_size << " bytes\n";

  m_resume_state = new ResumeState(metadata.info_hash_hex, "torrent_file",
                                   piece_info.totalPieces());

  m_upload_manager =
      new UploadManager(download_dir, metadata, piece_info, file_mapping);
}

DownloadManager::~DownloadManager() {
  // Do not delete peers, they are managed externally
  if (m_resume_state) {
    delete m_resume_state;
  }
  if (m_upload_manager) {
    delete m_upload_manager;
  }
}

void DownloadManager::addPeer(PeerConnection *peer) {
  if (peer && peer->isConnected() && peer->isHandshakeComplete()) {
    m_peers.push_back(peer);

    if (m_upload_manager) {
      m_upload_manager->addPeer(peer);
    }

    std::cout << "Addd peer: " << peer->getIp() << ":" << peer->getPort()
              << "\n";
  }
}

double DownloadManager::getProgress() const {
  if (m_metadata.total_size == 0)
    return 0.0;
  return (100.0 * m_downloaded_bytes) / m_metadata.total_size;
}

PeerConnection *DownloadManager::findAvailablePeer(uint32_t piece_index) {
  // Use of a greedy method for sequential, return the first available suitable
  // peer

  for (auto *peer : m_peers) {
    if (!peer->isConnected() || !peer->isHandshakeComplete()) {
      continue;
    }

    const auto &state = peer->getState();
    if (state.peer_choking) {
      continue;
    }

    const auto &peer_pieces = peer->getPeerPieces();
    if (piece_index < peer_pieces.size() && peer_pieces[piece_index]) {
      return peer;
    }
  }

  return nullptr;
}

bool DownloadManager::requestBlocksForPiece(PeerConnection *peer,
                                            uint32_t piece_index) {
  if (piece_index >= m_pieces.size()) {
    return false;
  }

  PieceDownload &piece = m_pieces[piece_index];

  std::cout << "  Requesting blocks for piece " << piece_index << " ("
            << piece.blocks.size() << " blocks)\n";

  for (auto &block : piece.blocks) {
    if (!block.requested) {
      if (!peer->sendRequest(piece_index, block.offset, block.length)) {
        std::cerr << "    Failed to send request for block at offset "
                  << block.offset << "\n";
        return false;
      }

      block.requested = true;
    }
  }

  std::cout << "  ✓ All block requests sent\n";
  return true;
}

bool DownloadManager::receivePieceData(PeerConnection *peer,
                                       uint32_t piece_index) {
  if (piece_index >= m_pieces.size()) {
    return false;
  }

  PieceDownload &piece = m_pieces[piece_index];

  std::cout << "  Receiving piece data...\n";

  while (!piece.isComplete()) {
    PeerMessage msg(MessageType::KEEP_ALIVE);

    if (!peer->receiveMessage(msg, 30)) {
      std::cerr << "    Failed to receive message (timeout or error)\n";
      return false;
    }

    switch (msg.type) {
    case MessageType::PIECE: {
      if (msg.payload.size() < 8) {
        std::cerr << "   Invalid PIECE message (too short)\n";
        continue;
      }

      uint32_t recv_piece_index =
          (static_cast<uint32_t>(msg.payload[0]) << 24U) |
          (static_cast<uint32_t>(msg.payload[1]) << 16U) |
          (static_cast<uint32_t>(msg.payload[2]) << 8U) |
          static_cast<uint32_t>(msg.payload[3]);

      uint32_t block_offset = (static_cast<uint32_t>(msg.payload[4]) << 24U) |
                              (static_cast<uint32_t>(msg.payload[5]) << 16U) |
                              (static_cast<uint32_t>(msg.payload[6]) << 8U) |
                              static_cast<uint32_t>(msg.payload[7]);

      if (recv_piece_index != piece_index) {
        std::cerr << "    Received wrong piece index: " << recv_piece_index
                  << " (expected " << piece_index << ")\n";
        continue;
      }

      Block *target_block = nullptr;
      for (auto &block : piece.blocks) {
        if (block.offset == block_offset) {
          target_block = &block;
          break;
        }
      }

      if (!target_block) {
        std::cerr << "    Received block with unknown offset: " << block_offset
                  << "\n";
        continue;
      }

      size_t data_length = msg.payload.size() - 8;
      target_block->data.resize(data_length);
      std::memcpy(target_block->data.data(), msg.payload.data() + 8,
                  data_length);

      target_block->received = true;
      m_downloaded_bytes += data_length;

      std::memcpy(piece.piece_data.data() + block_offset,
                  target_block->data.data(), data_length);

      std::cout << "    ✓ Block at offset " << block_offset << " ("
                << data_length << " bytes) - " << piece.blocksReceived() << "/"
                << piece.totalBlocks() << " blocks\n";

      break;
    }

    case MessageType::CHOKE:
      std::cerr << "   Peer choked us!\n";
      return false;

    case MessageType::KEEP_ALIVE:
      break;

    default:
      std::cout << "    Received message type: " << static_cast<int>(msg.type)
                << "\n";
      break;
    }
  }

  std::cout << "  ✓ Piece " << piece_index
            << " complete (all blocks received)\n";
  piece.state = PieceState::COMPLETE;
  return true;
}

bool DownloadManager::verifyPiece(uint32_t piece_index) {
  if (piece_index >= m_pieces.size()) {
    return false;
  }

  PieceDownload &piece = m_pieces[piece_index];

  if (piece.state != PieceState::COMPLETE) {
    std::cerr << "  Cannot verify piece " << piece_index << " - not complete\n";
    return false;
  }

  std::cout << "  Verifying piece " << piece_index << "...\n";

  const auto &expected_hash = m_piece_info.getHash(piece_index);

  std::vector<uint8_t> piece_data_vec(piece.piece_data.begin(),
                                      piece.piece_data.end());
  std::array<uint8_t, 20> calculated_hash = sha1ToBytes(piece_data_vec);

  if (calculated_hash != expected_hash) {
    std::cerr << "  ✗ Hash mismatch for piece " << piece_index << "!\n"
              << "    Expected: " << bytesToHex(expected_hash) << "\n"
              << "    Got: " << bytesToHex(calculated_hash) << "\n";

    piece.state = PieceState::NOT_STARTED;
    for (auto &block : piece.blocks) {
      block.requested = false;
      block.received = false;
      block.data.clear();
    }

    return false;
  }

  std::cout << "  ✓ Piece " << piece_index << " verified successfully\n";
  piece.state = PieceState::VERIFIED;
  return true;
}

bool DownloadManager::writePieceToDisk(uint32_t piece_index) {
  if (piece_index >= m_pieces.size()) {
    return false;
  }

  PieceDownload &piece = m_pieces[piece_index];

  if (piece.state != PieceState::VERIFIED) {
    std::cerr << "  Cannot write piece " << piece_index << " - not verified\n";
    return false;
  }

  std::cout << "  Writing piece " << piece_index << " to disk...\n";

  if (piece_index >= m_file_mapping.piece_to_file_map.size()) {
    std::cerr << "  No file mapping for piece " << piece_index << "\n";
    return false;
  }

  const auto &segments = m_file_mapping.piece_to_file_map[piece_index];

  for (const auto &segment : segments) {
    if (segment.file_index >= m_metadata.files.size()) {
      std::cerr << "  Invalid file index in segment\n";
      return false;
    }

    const auto &file_info = m_metadata.files[segment.file_index];

    std::string file_path = m_download_dir;
    for (const auto &path_component : file_info.path) {
      file_path += "/" + path_component;
    }

    std::cout << "    Writing " << segment.segment_length << " bytes to "
              << file_path << " at offset " << segment.file_offset << "\n";

    std::fstream file(file_path,
                      std::ios::in | std::ios::out | std::ios::binary);

    if (!file.is_open()) {
      file.open(file_path, std::ios::out | std::ios::binary);
      if (!file.is_open()) {
        std::cerr << "    Failed to create file: " << file_path << "\n";
        return false;
      }
      file.close();

      file.open(file_path, std::ios::in | std::ios::out | std::ios::binary);
    }

    if (!file.is_open()) {
      std::cerr << "    Failed to open file: " << file_path << "\n";
      return false;
    }

    file.seekp(segment.file_offset, std::ios::beg);

    if (!file.good()) {
      std::cerr << "    Failed to seek in file\n";
      file.close();
      return false;
    }

    uint64_t file_start_in_torrent = 0;
    for (size_t i = 0; i < segment.file_index; i++) {
      file_start_in_torrent += m_metadata.files[i].length;
    }

    uint64_t piece_start_in_torrent =
        piece_index * static_cast<uint64_t>(m_piece_info.piece_length);
    uint64_t segment_start_in_torrent =
        file_start_in_torrent + segment.file_offset;

    uint32_t offset_in_piece = static_cast<uint32_t>(segment_start_in_torrent -
                                                     piece_start_in_torrent);

    file.write(reinterpret_cast<const char *>(piece.piece_data.data() +
                                              offset_in_piece),
               segment.segment_length);

    if (!file.good()) {
      std::cerr << "    Failed to write data to file\n";
      file.close();
      return false;
    }

    file.close();
  }

  std::cout << "  ✓ Piece " << piece_index << " written to disk\n";
  return true;
}

void DownloadManager::createDirectoryStructure() {
  if (m_metadata.isSingleFile()) {
    return;
  }

  std::string base_path = m_download_dir + "/" + m_metadata.name;

  mkdir(base_path.c_str(), 0755);

  for (const auto &file : m_metadata.files) {
    std::string dir_path = base_path;

    for (size_t i = 0; i < file.path.size() - 1; i++) {
      dir_path += "/" + file.path[i];
      mkdir(dir_path.c_str(), 0755);
    }
  }
}

bool DownloadManager::downloadPiece(uint32_t piece_index) {
  if (piece_index >= m_pieces.size()) {
    std::cerr << "Invalid piece index: " << piece_index << "\n";
    return false;
  }

  PieceDownload &piece = m_pieces[piece_index];

  if (piece.state == PieceState::VERIFIED) {
    std::cout << "Piece " << piece_index << " already downloaded\n";
    return true;
  }

  std::cout << "\n[Piece " << piece_index << "/" << m_pieces.size() << "]\n";

  PeerConnection *peer = findAvailablePeer(piece_index);
  if (!peer) {
    std::cerr << "  No available peer has this piece " << piece_index << "\n";
    return false;
  }

  std::cout << "  Using peer: " << peer->getIp() << ":" << peer->getPort()
            << "\n";

  piece.state = PieceState::IN_PROGRESS;

  if (!requestBlocksForPiece(peer, piece_index)) {
    std::cerr << "  Failed to request blocks\n";
    piece.state = PieceState::NOT_STARTED;
    return false;
  }

  if (!receivePieceData(peer, piece_index)) {
    std::cerr << "  Failed to receive piece data\n";
    piece.state = PieceState::NOT_STARTED;
    return false;
  }

  if (!verifyPiece(piece_index)) {
    std::cerr << "  Piece verification failed\n";
    return false;
  }

  if (!writePieceToDisk(piece_index)) {
    std::cerr << "  Failed to write piece to disk\n";
    return false;
  }

  std::cout << "  ✓ Piece " << piece_index << " complete!\n";
  return true;
}

bool DownloadManager::downloadSequential() {
  std::cout << "\n"
            << std::string(60, '=') << "\n"
            << "STARTING SEQUENTIAL DOWNLOAD\n"
            << std::string(60, '=') << "\n";

  if (m_peers.empty()) {
    std::cerr << "No peers available for download\n";
    return false;
  }

  std::cout << "Using " << m_peers.size() << " peer(s)\n";
  std::cout << "Total pieces to download: " << m_pieces.size() << "\n\n";

  createDirectoryStructure();

  for (size_t piece_index = 0; piece_index < m_pieces.size(); piece_index++) {
    if (!downloadPiece(piece_index)) {
      std::cerr << "\nFailed to download piece " << piece_index << "\n";
      std::cerr << "Download incomplete!\n";
      return false;
    }

    std::cout << "\nProgress: " << std::fixed << std::setprecision(2)
              << getProgress() << "% (" << (piece_index + 1) << "/"
              << m_pieces.size() << " pieces)\n";
  }

  std::cout << "\n"
            << std::string(60, '=') << "\n"
            << "DOWNLOAD COMPLETE!\n"
            << std::string(60, '=') << "\n"
            << "Downloaded: " << m_downloaded_bytes << " bytes\n"
            << "Files saved to: " << m_download_dir << "\n";

  return true;
}

int DownloadManager::getNextPieceToDownload() {
  for (size_t i = 0; i < m_pieces.size(); i++) {
    PieceDownload &piece = m_pieces[i];

    if (piece.state == PieceState::VERIFIED) {
      continue;
    }

    if (m_piece_assignments.find(i) != m_piece_assignments.end()) {
      continue;
    }

    return i;
  }

  return -1;
}

bool DownloadManager::isComplete() const {
  for (const auto &piece : m_pieces) {
    if (piece.state != PieceState::VERIFIED) {
      return false;
    }
  }
  return true;
}

std::vector<uint32_t>
DownloadManager::getAvailablePiecesForPeer(PeerConnection *peer) {
  std::vector<uint32_t> available_pieces;

  if (!peer->isConnected() || !peer->isHandshakeComplete()) {
    return available_pieces;
  }

  const auto &state = peer->getState();
  if (state.peer_choking) {
    return available_pieces;
  }

  const auto &peer_pieces = peer->getPeerPieces();

  for (size_t i = 0; i < m_pieces.size(); i++) {
    if (m_pieces[i].state == PieceState::VERIFIED) {
      continue;
    }

    if (m_piece_assignments.find(i) != m_piece_assignments.end()) {
      continue;
    }

    if (i < peer_pieces.size() && peer_pieces[i]) {
      available_pieces.push_back(i);
    }
  }

  return available_pieces;
}

bool DownloadManager::downloadParallel() {
  std::cout << "\n"
            << std::string(60, '=') << "\n"
            << "STARTING PARALLEL DOWNLOAD\n"
            << std::string(60, '=') << "\n";

  if (m_peers.empty()) {
    std::cerr << "No peers available\n";
    return false;
  }

  std::cout << "Using " << m_peers.size() << " peer(s)\n";
  std::cout << "Total pieces: " << m_pieces.size() << "\n\n";

  createDirectoryStructure();

  while (!isComplete()) {
    for (auto *peer : m_peers) {
      bool peer_busy = false;
      for (const auto &task : m_active_tasks) {
        if (task.peer == peer && !task.complete) {
          peer_busy = true;
          break;
        }
      }

      if (peer_busy) {
        continue;
      }

      auto available_pieces = getAvailablePiecesForPeer(peer);
      if (available_pieces.empty()) {
        continue;
      }

      uint32_t piece_index = available_pieces[0];
      startPieceDownload(piece_index, peer);
    }

    processActiveTasks();

    auto it = m_active_tasks.begin();
    while (it != m_active_tasks.end()) {
      if (it->complete) {
        uint32_t piece_index = it->piece_index;
        PieceDownload &piece = m_pieces[piece_index];

        if (piece.isComplete()) {
          piece.state = PieceState::COMPLETE;

          if (verifyPiece(piece_index)) {
            if (writePieceToDisk(piece_index)) {
              std::cout << "  ✓ Piece " << piece_index
                        << " verified and saved\n";
            } else {
              std::cerr << "  ✗ Failed to write piece " << piece_index << "\n";
            }
          } else {
            std::cerr << "  ✗ Piece " << piece_index
                      << " verification failed\n";
            piece.state = PieceState::NOT_STARTED;
            for (auto &block : piece.blocks) {
              block.requested = false;
              block.received = false;
              block.data.clear();
            }
          }
        }

        m_piece_assignments.erase(piece_index);

        it = m_active_tasks.erase(it);

        int completed_pieces = 0;
        for (const auto &p : m_pieces) {
          if (p.state == PieceState::VERIFIED)
            completed_pieces++;
        }

        std::cout << "\nProgress: " << std::fixed << std::setprecision(2)
                  << getProgress() << "% (" << completed_pieces << "/"
                  << m_pieces.size() << " pieces)\n";
      } else {
        ++it;
      }
    }

    usleep(10000);
  }

  std::cout << "\n"
            << std::string(60, '=') << "\n"
            << "PARALLEL DOWNLOAD COMPLETE!\n"
            << std::string(60, '=') << "\n"
            << "Downloaded: " << m_downloaded_bytes << " bytes\n"
            << "Files saved to: " << m_download_dir << "\n";

  return true;
}

void DownloadManager::processActiveTasks() {
  for (auto &task : m_active_tasks) {
    handleTaskMessage(task);
  }
}

bool DownloadManager::handleTaskMessage(DownloadTask &task) {
  PeerConnection *peer = task.peer;
  uint32_t piece_index = task.piece_index;
  PieceDownload &piece = m_pieces[piece_index];

  PeerMessage msg(MessageType::KEEP_ALIVE);

  if (!peer->receiveMessage(msg, 1)) {
    return false;
  }

  switch (msg.type) {
  case MessageType::PIECE: {
    if (msg.payload.size() < 8) {
      return false;
    }

    uint32_t recv_piece_index = (static_cast<uint32_t>(msg.payload[0]) << 24) |
                                (static_cast<uint32_t>(msg.payload[1]) << 16) |
                                (static_cast<uint32_t>(msg.payload[2]) << 8) |
                                static_cast<uint32_t>(msg.payload[3]);

    uint32_t block_offset = (static_cast<uint32_t>(msg.payload[4]) << 24) |
                            (static_cast<uint32_t>(msg.payload[5]) << 16) |
                            (static_cast<uint32_t>(msg.payload[6]) << 8) |
                            static_cast<uint32_t>(msg.payload[7]);

    if (recv_piece_index != piece_index) {
      return false;
    }

    Block *target_block = nullptr;
    for (auto &block : piece.blocks) {
      if (block.offset == block_offset) {
        target_block = &block;
        break;
      }
    }

    if (!target_block) {
      return false;
    }

    size_t data_length = msg.payload.size() - 8;
    target_block->data.resize(data_length);
    std::memcpy(target_block->data.data(), msg.payload.data() + 8, data_length);

    target_block->received = true;
    m_downloaded_bytes += data_length;

    std::memcpy(piece.piece_data.data() + block_offset,
                target_block->data.data(), data_length);

    if (piece.isComplete()) {
      std::cout << "  [Peer " << peer->getIp() << ":" << peer->getPort()
                << "] Piece " << piece_index << " complete ("
                << piece.blocksReceived() << "/" << piece.totalBlocks()
                << ")\n";
      task.complete = true;
    }

    return true;
  }

  case MessageType::CHOKE:
    std::cerr << "  [Peer " << peer->getIp() << ":" << peer->getPort()
              << "] Choked us during piece " << piece_index << "\n";
    task.complete = true;
    return false;

  case MessageType::KEEP_ALIVE:
    return false;

  default:
    return false;
  }

  return false;
}

bool DownloadManager::startPieceDownload(uint32_t piece_index,
                                         PeerConnection *peer) {
  if (piece_index >= m_pieces.size()) {
    return false;
  }

  PieceDownload &piece = m_pieces[piece_index];

  std::cout << "\n[Peer " << peer->getIp() << ":" << peer->getPort()
            << "] Starting piece " << piece_index << "\n";

  piece.state = PieceState::IN_PROGRESS;
  m_piece_assignments[piece_index] = peer;

  if (!requestBlocksForPiece(peer, piece_index)) {
    std::cerr << "  Failed to send block requests\n";
    piece.state = PieceState::NOT_STARTED;
    m_piece_assignments.erase(piece_index);
    return false;
  }

  DownloadTask task(piece_index, peer);
  task.blocks_requested = true;
  m_active_tasks.push_back(task);

  return true;
}

void DownloadManager::updatePieceAvailability() {
  m_piece_availability.clear();
  m_piece_availability.resize(m_pieces.size(), 0);

  for (auto *peer : m_peers) {
    if (!peer->isConnected() || !peer->isHandshakeComplete()) {
      continue;
    }

    const auto &peer_pieces = peer->getPeerPieces();
    for (size_t i = 0;
         i < peer_pieces.size() && i < m_piece_availability.size(); i++) {
      if (peer_pieces[i]) {
        m_piece_availability[i]++;
      }
    }
  }

  std::cout << "\nPiece availability:\n";
  for (size_t i = 0; i < m_piece_availability.size(); i++) {
    std::cout << "  Piece " << i << ": " << m_piece_availability[i]
              << " peer(s)\n";
  }
  std::cout << "\n";
}

int DownloadManager::getNextRarestPiece() {
  int completed_pieces = 0;
  for (const auto &piece : m_pieces) {
    if (piece.state == PieceState::VERIFIED) {
      completed_pieces++;
    }
  }

  if (completed_pieces < RANDOM_FIRST_COUNT && !m_random_first_pieces.empty()) {
    if (m_random_first_pieces.empty()) {
      std::vector<uint32_t> available;
      for (size_t i = 0; i < m_pieces.size(); i++) {
        if (m_pieces[i].state == PieceState::NOT_STARTED &&
            m_piece_assignments.find(i) == m_piece_assignments.end() &&
            m_piece_availability[i] > 0) {
          available.push_back(i);
        }
      }

      std::random_device rd;
      std::mt19937 g(rd());
      std::shuffle(available.begin(), available.end(), g);

      int count = std::min(RANDOM_FIRST_COUNT, (int)available.size());
      m_random_first_pieces.assign(available.begin(),
                                   available.begin() + count);
    }

    if (!m_random_first_pieces.empty()) {
      uint32_t piece_idx = m_random_first_pieces.back();
      m_random_first_pieces.pop_back();

      if (m_pieces[piece_idx].state == PieceState::NOT_STARTED &&
          m_piece_assignments.find(piece_idx) == m_piece_assignments.end()) {
        std::cout << "  [Random first] Selecting piece " << piece_idx << "\n";
        return piece_idx;
      }
    }
  }

  int rarest_piece = -1;
  int min_availability = INT_MAX;

  for (size_t i = 0; i < m_pieces.size(); i++) {
    if (m_pieces[i].state == PieceState::IN_PROGRESS) {
      continue;
    }

    if (m_piece_assignments.find(i) != m_piece_assignments.end()) {
      continue;
    }

    if (m_piece_availability[i] == 0) {
      continue;
    }

    if (m_piece_availability[i] < min_availability) {
      min_availability = m_piece_availability[i];
      rarest_piece = i;
    }
  }

  if (rarest_piece >= 0) {
    std::cout << "  [Rarest first] Selecting piece " << rarest_piece
              << " (availability: " << min_availability << ")\n";
  }

  return rarest_piece;
}

bool DownloadManager::downloadRarestFirst() {
  std::cout << "\n"
            << std::string(60, '=') << "\n"
            << "STARTING RAREST-FIRST DOWNLOAD\n"
            << std::string(60, '=') << "\n";

  bool resumed = loadResumeState();

  if (m_tui_state) {
    m_tui_state->setFilename(m_metadata.name);
    m_tui_state->setTotalSize(m_metadata.total_size);
    m_tui_state->setStatus("Downloading");
    m_tui_state->setPieceInfo(m_pieces.size(), 0);
  }

  if (m_peers.empty()) {
    std::cerr << "No peer available\n";
    return false;
  }

  std::cout << "Using " << m_peers.size() << " peers(s)\n"
            << "Total pieces: " << m_pieces.size() << "\n"
            << "Strategy: Random first (" << RANDOM_FIRST_COUNT
            << " pieces), then rarest-first\n\n";

  createDirectoryStructure();
  updatePieceAvailability();

  /*
  std::cout << "\nUnchoking interested peers...\n";
  for (auto *peer : m_peers) {
    const auto &state = peer->getState();
    if (state.peer_interested) {
      peer->sendUnchoke();
      std::cout << "  Unchoked: " << peer->getIp() << ":" << peer->getPort()
                << "\n";
    }
  }
  std::cout << "\n";*/

  std::cout << "\nReady to download. Peer states:\n";
  for (auto* peer : m_peers) {
    const auto& state = peer->getState();
    std::cout << "  " << peer->getIp() << ":" << peer->getPort();
    
    if (!state.peer_choking) {
      std::cout << " - ✓ Unchoked (ready)";
    } else {
      std::cout << " - ⏳ Choked (waiting)";
    }
    
    if (state.am_interested) {
      std::cout << " | We're interested";
    }
    std::cout << "\n";
  }
  std::cout << "\n";

  while (!isComplete()) {
    for (auto *peer : m_peers) {
      bool peer_busy = false;
      for (const auto &task : m_active_tasks) {
        if (task.peer == peer && !task.complete) {
          peer_busy = true;
          break;
        }
      }

      if (peer_busy) {
        continue;
      }

      auto available_pieces = getAvailablePiecesForPeer(peer);
      if (available_pieces.empty()) {
        continue;
      }

      int best_piece = -1;
      int min_availability = INT_MAX;

      for (uint32_t piece_idx : available_pieces) {
        if (m_piece_availability[piece_idx] < min_availability) {
          min_availability = m_piece_availability[piece_idx];
          best_piece = piece_idx;
        }
      }

      if (best_piece < 0) {
        best_piece = getNextRarestPiece();
      }

      if (best_piece >= 0) {
        const auto &peer_pieces = peer->getPeerPieces();
        if (best_piece < (int)peer_pieces.size() && peer_pieces[best_piece]) {
          startPieceDownload(best_piece, peer);
        }
      }
    }

    processActiveTasks();

    if (m_upload_manager) {
      m_upload_manager->processUploads();
      m_uploaded_bytes = m_upload_manager->getUploadedBytes();
    }

    auto it = m_active_tasks.begin();
    while (it != m_active_tasks.end()) {
      if (it->complete) {
        uint32_t piece_index = it->piece_index;
        PieceDownload &piece = m_pieces[piece_index];

        if (piece.isComplete()) {
          piece.state = PieceState::COMPLETE;

          if (verifyPiece(piece_index)) {
            if (writePieceToDisk(piece_index)) {
              std::cout << "  ✓ Piece " << piece_index
                        << " verified and saved\n";

              if (m_resume_state) {
                m_resume_state->markPieceComplete(piece_index);
                saveResumeState();
              }

              updatePieceAvailability();
            }
          } else {
            std::cerr << "  ✗ Piece " << piece_index
                      << " verification failed\n";
            piece.state = PieceState::NOT_STARTED;
            for (auto &block : piece.blocks) {
              block.requested = false;
              block.received = false;
              block.data.clear();
            }
          }
        }

        m_piece_assignments.erase(piece_index);
        it = m_active_tasks.erase(it);

        int completed = 0;
        for (const auto &p : m_pieces) {
          if (p.state == PieceState::VERIFIED)
            completed++;
        }

        std::cout << "\nProgress: " << std::fixed << std::setprecision(2)
                  << getProgress() << "% (" << completed << "/"
                  << m_pieces.size() << " pieces)\n";

        std::cout << "Downloaded: " << (m_downloaded_bytes / 1024.0) << " KB, "
                  << "Uploaded: " << (m_uploaded_bytes / 1024.0) << " KB\n";

        if (m_tui_state) {
          m_tui_state->setProgress(getProgress());
          m_tui_state->setDownloadedBytes(m_downloaded_bytes);
          m_tui_state->setUploadedBytes(m_uploaded_bytes);
          m_tui_state->setPeerCount(m_peers.size());
          m_tui_state->setPieceInfo(m_pieces.size(), completed);
        }
      } else {
        ++it;
      }
    }

    usleep(10000);
  }

  std::cout << "\n"
            << std::string(60, '=') << "\n"
            << "RAREST-FIRST DOWNLOAD COMPLETE!\n"
            << std::string(60, '=') << "\n"
            << "Downloaded: " << m_downloaded_bytes << " bytes\n"
            << "Files save to: " << m_download_dir << "\n";
  
  if (m_tui_state) {
    m_tui_state->setStatus("Complete");
    m_tui_state->setProgress(100.0);
  }

  return true;
}

bool DownloadManager::loadResumeState() {
  if (!m_use_resume || !m_resume_state) {
    return false;
  }

  if (!m_resume_state->load()) {
    return false;
  }

  for (uint32_t piece_idx : m_resume_state->getCompletedPieces()) {
    if (piece_idx < m_pieces.size()) {
      m_pieces[piece_idx].state = PieceState::VERIFIED;
    }
  }

  m_downloaded_bytes = m_resume_state->getDownloadedBytes();

  std::cout << "Resumed: " << m_resume_state->getCompletedPieceCount()
            << " pieces already complete\n\n";

  return true;
}

bool DownloadManager::saveResumeState() {
  if (!m_use_resume || !m_resume_state) {
    return false;
  }

  m_resume_state->setDownloadedBytes(m_downloaded_bytes);
  m_resume_state->setUploadedBytes(m_uploaded_bytes);

  return m_resume_state->save();
}
