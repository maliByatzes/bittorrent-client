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

void connectToPeers(const TrackerResponse &response,
                    const std::array<uint8_t, 20> &info_hash,
                    const std::string &peer_id, int max_peers = 3) {
  std::cout << "\n"
            << std::string(60, '=') << "\n"
            << "CONNECTING TO PEERS\n"
            << std::string(60, '=') << "\n";

  int attempts = std::min(max_peers, static_cast<int>(response.peers.size()));
  int successful = 0;

  std::vector<PeerConnection *> connections;

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
    successful++;
    connections.push_back(conn);

    std::cout << "  Waiting for initial message...\n";

    PeerMessage msg(MessageType::KEEP_ALIVE);
    if (conn->receiveMessage(msg, 5)) {
      std::cout << "  ← Received message type: " << static_cast<int>(msg.type)
                << "\n";

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
      std::cout << "  ✔️ INTERESTED sent\n";

      std::cout << "  Waiting for UNCHOKE...\n";
      if (conn->receiveMessage(msg, 10)) {
        if (msg.type == MessageType::UNCHOKE) {
          std::cout << "  ✔️ Peer UNCHOKED us! Ready to download.\n";
        } else {
          std::cout << "  Received message type: " << static_cast<int>(msg.type)
                    << "\n";
        }
      } else {
        std::cout << "  No response (peer might keep us choked)\n";
      }
    }
  }

  std::cout << std::string(60, '=') << "\n"
            << "SUMMARY\n"
            << std::string(60, '=') << "\n"
            << "Attempted: " << attempts << " peers\n"
            << "Successful: " << successful << " connections\n"
            << std::string(60, '=') << "\n";

  for (auto *conn : connections) {
    conn->disconnect();
    delete conn;
  }
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

    if (response.success && response.peers.size() > 0) {
      std::cout << "\n✅ Successfully found peers!\n";
      connectToPeers(response, metadata.info_hash_bytes, peer_id, 3);
      std::cout << "\n✅ Ready for downloading!\n";
      return EXIT_SUCCESS;
    } else {
      std::cout << "\n⚠️ No peers were found or tracker error.\n";
      return EXIT_FAILURE;
    }
  } catch (const std::exception &e) {
    std::cerr << "\n❌ Error: " << e.what() << "\n";
    return EXIT_FAILURE;
  }
}
