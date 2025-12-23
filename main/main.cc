#include "download_manager.h"
#include "peer_connection.h"
#include "torrent_file.h"
#include "tracker.h"
#include "utils.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <new>
#include <string>
#include <vector>

void printUsage(const char *program_name) {
  std::cout << "Usage: " << program_name << " <torrent_file>\n";
  std::cout << "\nBitTorrent Client.\n";
}

void printTorrentInfo(const TorrentMetadata &metadata) {
  std::cout << "\n"
            << std::string(60, '=') << "\n"
            << "TORRENT INFORMATION\n"
            << std::string(60, '=') << "\n"
            << "Name: " << metadata.name << "\n"
            << "Size: " << (metadata.total_size / 1024.0 / 1024.0) << " MB\n"
            << "Files: " << metadata.files.size() << "\n"
            << "Info Hash: " << metadata.info_hash_hex << "\n"
            << std::string(60, '=') << "\n\n";
}

void printTrackerResponse(const TrackerResponse &response) {
  std::cout << "\n"
            << std::string(60, '=') << "\n"
            << "TRACKER RESPONSE\n"
            << std::string(60, '=') << "\n";

  if (!response.success) {
    std::cout << "❌ Failed: " << response.failure_reason << "\n";
    return;
  }

  std::cout << "✅ Success!\n"
            << "Interval: " << response.interval << " seconds\n"
            << "Seeders: " << response.complete << "\n"
            << "Leechers: " << response.incomplete << "\n"
            << "Peers found: " << response.peers.size() << "\n\n";

  if (response.peers.size() > 0) {
    std::cout << "First 10 peers:\n";
    for (size_t i = 0; i < std::min(size_t(10), response.peers.size()); i++) {
      std::cout << "  [" << i << "] " << response.peers[i].ip << ":"
                << response.peers[i].port << "\n";
    }
  }

  std::cout << std::string(60, '=') << "\n";
}

std::vector<PeerConnection *>
connectToPeers(const TrackerResponse &response,
               const std::array<uint8_t, 20> &info_hash,
               const std::string &peer_id, int max_peers = 5) {
  std::cout << "\n"
            << std::string(60, '=') << "\n"
            << "CONNECTING TO PEERS\n"
            << std::string(60, '=') << "\n";

  int attempts = std::min(max_peers, static_cast<int>(response.peers.size()));
  std::vector<PeerConnection *> successful_peers;

  for (int i = 0; i < attempts; i++) {
    const auto &peer = response.peers[i];

    std::cout << "\n[" << (i + 1) << "/" << attempts << "] Peer: " << peer.ip
              << ":" << peer.port << "\n";

    PeerConnection *conn =
        new PeerConnection(peer.ip, peer.port, info_hash, peer_id);

    if (!conn->connect(10)) {
      std::cout << "  ❌ Connection failed\n";
      delete conn;
      continue;
    }

    if (!conn->performHandshake()) {
      std::cout << "  ❌ Handshake failed\n";
      conn->disconnect();
      delete conn;
      continue;
    }

    std::cout << "  ✔️ Connection and handshake successful!\n";

    std::vector<bool> our_pieces;

    PeerMessage msg(MessageType::KEEP_ALIVE);
    if (conn->receiveMessage(msg, 5)) {
      if (msg.type == MessageType::BIT_FIELD) {
        const auto &pieces = conn->getPeerPieces();
        int piece_count = 0;
        for (bool has : pieces) {
          if (has)
            piece_count++;
        }

        std::cout << "  Peer has " << piece_count << "/" << pieces.size()
                  << " pieces\n";
      }
    }

    std::cout << "  → Sending INTERESTED\n";
    if (conn->sendInterested()) {
      if (conn->receiveMessage(msg, 10)) {
        if (msg.type == MessageType::UNCHOKE) {
          std::cout << "  ✔️ Peer UNCHOKED us!\n";
          successful_peers.push_back(conn);
        } else {
          std::cout << "  Peer did not unchoke (will try anyway)\n";
          successful_peers.push_back(conn);
        }
      } else {
        std::cout << "  No unchoke response (will try anyway)\n";
        successful_peers.push_back(conn);
      }
    }
  }

  std::cout << "\n"
            << std::string(60, '=') << "\n"
            << "Connected to " << successful_peers.size() << " peer(s)\n"
            << std::string(60, '=') << "\n";

  return successful_peers;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printUsage(argv[0]);
    return EXIT_FAILURE;
  }

  std::string torrent_file = argv[1];

  try {
    std::cout << "📁 Parsing torrent file: " << torrent_file << "\n";
    TorrentFile torrent(torrent_file);
    torrent.parse();

    const auto &metadata = torrent.getMetadata();
    const auto &piece_info = torrent.getPieceInfo();
    const auto &file_mapping = torrent.getFileMapping();

    printTorrentInfo(metadata);

    std::string peer_id = generatePeerId();
    std::cout << "🆔 Generated Peer ID: " << peer_id << "\n\n";

    if (metadata.announce_urls.empty()) {
      std::cerr << "❌ No announce URLs found in torrent.\n";
      return EXIT_FAILURE;
    }

    std::cout << "📡 Contacting tracker: " << metadata.announce_urls[0] << "\n";

    Tracker tracker(metadata.announce_urls[0], metadata.info_hash_bytes,
                    peer_id, 6881, metadata.total_size);

    TrackerResponse response = tracker.announce("started");
    printTrackerResponse(response);

    if (!response.success || response.peers.empty()) {
      std::cerr << "\n❌ No peers found or tracker error\n";
      return EXIT_FAILURE;
    }

    auto peers = connectToPeers(response, metadata.info_hash_bytes, peer_id, 5);

    if (peers.empty()) {
      std::cerr << "\n❌ Could not connect to any peers\n";
      return EXIT_FAILURE;
    }

    DownloadManager download_mgr(metadata, piece_info, file_mapping,
                                 "./downloads");

    for (auto *peer : peers) {
      download_mgr.addPeer(peer);
    }

    // bool success = download_mgr.downloadSequential();
    // bool success = download_mgr.downloadParallel();
    bool success = download_mgr.downloadRarestFirst();

    for (auto *peer : peers) {
      peer->disconnect();
      delete peer;
    }

    if (success) {
      std::cout << "\n✅ Download complete! Check ./downloads directory\n";
      return EXIT_SUCCESS;
    } else {
      std::cerr << "\n❌ Download failed\n";
      return EXIT_FAILURE;
    }
  } catch (const std::exception &e) {
    std::cerr << "\n❌ Error: " << e.what() << "\n";
    return EXIT_FAILURE;
  }
}
