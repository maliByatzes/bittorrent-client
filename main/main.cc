#include "torrent_file.h"
#include "utils.h"
#include <iostream>
#include <string>

int main() {
  try {
    std::string file_name{"sample.torrent"};

    TorrentFile torrent_file(file_name);
    torrent_file.parse();

    const auto &metadata = torrent_file.getMetadata();
    const auto &piece_info = torrent_file.getPieceInfo();

    std::cout << "\n========== TORRENT METADATA ==========\n";
    std::cout << "Name: " << metadata.name << "\n";
    std::cout << "Info Hash (hex): " << metadata.info_hash_hex << "\n";
    std::cout << "Info Hash (URL): " << metadata.info_hash_urlencoded << "\n";
    std::cout << "Total Size: " << metadata.total_size << " bytes ("
              << (metadata.total_size / 1024.0 / 1024.0) << " MB)\n";
    std::cout << "Piece Length: " << metadata.piece_length << " bytes\n";
    std::cout << "Number of Pieces: " << piece_info.totalPieces() << "\n";
    std::cout << "Last Piece Size: " << piece_info.last_piece_size
              << " bytes\n";

    std::cout << "\nAnnounce URLs:\n";
    for (const auto &url : metadata.announce_urls) {
      std::cout << "  - " << url << "\n";
    }

    std::cout << "\nFiles:\n";
    for (size_t i = 0; i < metadata.files.size(); i++) {
      const auto &file = metadata.files[i];
      std::cout << "  [" << i << "] ";
      for (size_t j = 0; j < file.path.size(); j++) {
        std::cout << file.path[j];
        if (j < file.path.size() - 1)
          std::cout << "/";
      }
      std::cout << " (" << file.length << " bytes)\n";
    }

    std::cout << "\nOptional Metadata:\n";
    if (!metadata.comment.empty()) {
      std::cout << "  Comment: " << metadata.comment << "\n";
    }
    if (!metadata.created_by.empty()) {
      std::cout << "  Created by: " << metadata.created_by << "\n";
    }
    if (metadata.creation_date > 0) {
      std::cout << "  Creation date: " << metadata.creation_date << "\n";
    }

    std::cout << "\nFirst 5 piece hashes:\n";
    for (size_t i = 0; i < std::min(size_t(5), piece_info.totalPieces()); i++) {
      std::cout << "  Piece " << i << ": " << bytesToHex(piece_info.getHash(i))
                << "\n";
    }

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
  }
}
