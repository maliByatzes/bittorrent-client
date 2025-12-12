#include "torrent_file.h"
#include "tracker.h"
#include "utils.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>

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
