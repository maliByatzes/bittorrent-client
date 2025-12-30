#include "download_manager.h"
#include "magnet_link.h"
#include "metadata_fetcher.h"
#include "peer_connection.h"
#include "torrent_file.h"
#include "tracker.h"
#include "utils.h"
#include "tui/tui_app.h"
#include "tui/tui_state.h"
#include <thread>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <new>
#include <string>
#include <vector>

void printUsage(const char *program_name) {
  std::cout << "Usage: " << program_name << " <torrent_file_or_magnet_link>\n";
  std::cout << "\nExamples:\n";
  std::cout << "  " << program_name << " file.torrent\n";
  std::cout << "  " << program_name << " 'magnet:?xt=urn:btih:...'\n";
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

bool isMagnetLink(const std::string& input) {
  return input.substr(0, 8) == "magnet:?";
}

std::vector<PeerConnection *>
connectToPeers(const TrackerResponse &response,
               const std::array<uint8_t, 20> &info_hash,
               const std::string &peer_id, [[maybe_unused]] int max_peers = 5) {
  std::cout << "\n"
            << std::string(60, '=') << "\n"
            << "CONNECTING TO PEERS\n"
            << std::string(60, '=') << "\n";

  // int attempts = std::max(max_peers, static_cast<int>(response.peers.size()));
  int attempts = static_cast<int>(response.peers.size());
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

  std::string input = argv[1];

  try {
    TorrentMetadata metadata;
    PieceInformation piece_info;
    PieceFileMapping file_mapping;

    if (isMagnetLink(input)) {
      std::cout << "🧲 Processing magnet link...\n\n";

      MagnetLink magnet = MagnetParser::parse(input);

      std::cout << "Info Hash: " << magnet.info_hash_hex << "\n";
      std::cout << "Display Name: " << (magnet.display_name.empty() ? 
                                        "(none)" : magnet.display_name) << "\n";
      std::cout << "Trackers: " << magnet.tracker_urls.size() << "\n";

      if (magnet.tracker_urls.empty()) {
        std::cerr << "❌ Magnet link has no trackers (DHT required)\n";
        return EXIT_FAILURE;
      }

      std::string peer_id = generatePeerId();
      std::cout << "🆔 Generated Peer ID: " << peer_id << "\n\n";

      std::cout << "📡 Contacting tracker: " << magnet.tracker_urls[0] << "\n";

      Tracker tracker(magnet.tracker_urls[0],
                      magnet.info_hash,
                      peer_id,
                      6881,
                      magnet.has_exact_length ? magnet.exact_length : 0);

      TrackerResponse response = tracker.announce("started");

      if (!response.success || response.peers.empty()) {
        std::cerr << "\n❌ No peers found or tracker error\n";
        return EXIT_FAILURE;
      }

      std::cout << "✅ Found " << response.peers.size() << " peer(s)\n\n";

      auto peers = connectToPeers(response, magnet.info_hash, peer_id, 5);

      if (peers.empty()) {
        std::cerr << "\n❌ Could not connect to any peers\n";
        return EXIT_FAILURE;
      }

      MetadataFetcher fetcher(magnet.info_hash);
      for (auto* peer : peers) {
        fetcher.addPeer(peer);
      }

      if (!fetcher.fetchMetadata()) {
        std::cerr << "❌ Failed to fetch metadata\n";
        for (auto* peer : peers) {
          peer->disconnect();
          delete peer;
        }
        return EXIT_FAILURE;
      }

      if (!fetcher.reconstructMetadata(metadata, piece_info, file_mapping)) {
        std::cerr << "❌ Failed to reconstruct metadata\n";
        for (auto* peer : peers) {
          peer->disconnect();
          delete peer;
        }
        return EXIT_FAILURE;
      }

      metadata.announce_urls = magnet.tracker_urls;

      std::cout << "\n✅ Metadata reconstructed successfully!\n";
      printTorrentInfo(metadata);

      DownloadManager download_mgr(metadata, piece_info, file_mapping, "./downloads");
      
      for (auto* peer : peers) {
        download_mgr.addPeer(peer);
      }
      
      std::cout << "\n📥 Starting download...\n";
      bool success = download_mgr.downloadRarestFirst();
      
      for (auto* peer : peers) {
        peer->disconnect();
        delete peer;
      }
      
      if (success) {
        std::cout << "\n✅ Download complete!\n";
        return EXIT_SUCCESS;
      } else {
        std::cerr << "\n❌ Download failed\n";
        return EXIT_SUCCESS;
      }
    } else {
        std::cout << "📁 Parsing torrent file: " << input << "\n";
        TorrentFile torrent(input);
        torrent.parse();

        metadata = torrent.getMetadata();
        piece_info = torrent.getPieceInfo();
        file_mapping = torrent.getFileMapping();

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

        auto tui_state = std::make_shared<TUIState>();

        std::thread tui_thread;
        TUIApp* tui_app = nullptr;

        bool use_tui = true;

        if (use_tui) {
          tui_app = new TUIApp(tui_state);
          tui_thread = std::thread([tui_app]() {
            tui_app->run();
          });

          std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        DownloadManager download_mgr(metadata, piece_info, file_mapping,
                                    "./downloads");

        if (use_tui) {
          download_mgr.setTUIState(tui_state);
        }

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

        if (use_tui) {
          std::this_thread::sleep_for(std::chrono::seconds(2));

          tui_app->stop();
          tui_thread.join();
          delete tui_app;
        }

        if (success) {
          std::cout << "\n✅ Download complete! Check ./downloads directory\n";
          return EXIT_SUCCESS;
        } else {
          std::cerr << "\n❌ Download failed\n";
          return EXIT_FAILURE;
        }
    }

  } catch (const std::exception &e) {
    std::cerr << "\n❌ Error: " << e.what() << "\n";
    return EXIT_FAILURE;
  }
}
