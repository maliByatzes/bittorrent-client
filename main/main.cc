#include "torrent_file.h"
#include "torrent_test.h"
#include "utils.h"
#include <iomanip>
#include <iostream>

void printTorrentInfo(const TorrentMetadata &metadata,
                      const PieceInformation &piece_info) {
  std::cout << "\n" << std::string(60, '=') << "\n";
  std::cout << "TORRENT INFORMATION\n";
  std::cout << std::string(60, '=') << "\n";

  std::cout << "Name: " << metadata.name << "\n";
  std::cout << "Info Hash: " << metadata.info_hash_hex << "\n";
  std::cout << "Total Size: " << metadata.total_size << " bytes (" << std::fixed
            << std::setprecision(2) << (metadata.total_size / 1024.0 / 1024.0)
            << " MB)\n";
  std::cout << "Piece Length: " << metadata.piece_length << " bytes\n";
  std::cout << "Number of Pieces: " << piece_info.totalPieces() << "\n";
  std::cout << "Last Piece Size: " << piece_info.last_piece_size << " bytes\n";
  std::cout << "Type: "
            << (metadata.isSingleFile() ? "Single-file" : "Multi-file") << "\n";

  std::cout << "\nAnnounce URLs (" << metadata.announce_urls.size() << "):\n";
  for (size_t i = 0; i < metadata.announce_urls.size(); i++) {
    std::cout << "  [" << i << "] " << metadata.announce_urls[i] << "\n";
  }

  std::cout << "\nFiles (" << metadata.files.size() << "):\n";
  for (size_t i = 0; i < metadata.files.size(); i++) {
    const auto &file = metadata.files[i];
    std::cout << "  [" << i << "] ";
    for (size_t j = 0; j < file.path.size(); j++) {
      std::cout << file.path[j];
      if (j < file.path.size() - 1)
        std::cout << "/";
    }
    std::cout << " (" << file.length << " bytes, " << std::fixed
              << std::setprecision(2) << (file.length / 1024.0 / 1024.0)
              << " MB)\n";
  }

  if (!metadata.comment.empty()) {
    std::cout << "\nComment: " << metadata.comment << "\n";
  }
  if (!metadata.created_by.empty()) {
    std::cout << "Created by: " << metadata.created_by << "\n";
  }
  if (metadata.creation_date > 0) {
    std::cout << "Creation date: " << metadata.creation_date
              << " (Unix timestamp)\n";
  }

  std::cout << "\nFirst 5 Piece Hashes:\n";
  for (size_t i = 0; i < std::min(size_t(5), piece_info.totalPieces()); i++) {
    std::cout << "  [" << i << "] " << bytesToHex(piece_info.getHash(i))
              << "\n";
  }

  std::cout << std::string(60, '=') << "\n";
}

void printFileMappingSample(const PieceFileMapping &mapping,
                            const TorrentMetadata &metadata) {
  std::cout << "\n" << std::string(60, '=') << "\n";
  std::cout << "FILE MAPPING SAMPLE (First 3 Pieces)\n";
  std::cout << std::string(60, '=') << "\n";

  size_t pieces_to_show = std::min(size_t(3), mapping.piece_to_file_map.size());

  for (size_t piece_idx = 0; piece_idx < pieces_to_show; piece_idx++) {
    std::cout << "\nPiece " << piece_idx << " maps to:\n";

    for (const auto &segment : mapping.piece_to_file_map[piece_idx]) {
      const auto &file = metadata.files[segment.file_index];
      std::cout << "  File [" << segment.file_index << "] ";

      for (size_t j = 0; j < file.path.size(); j++) {
        std::cout << file.path[j];
        if (j < file.path.size() - 1)
          std::cout << "/";
      }

      std::cout << "\n";
      std::cout << "    Offset: " << segment.file_offset << " bytes\n";
      std::cout << "    Length: " << segment.segment_length << " bytes\n";
    }
  }

  std::cout << std::string(60, '=') << "\n";
}

// NOTE: Use the whole executable as test for now, separate this later
int main(int argc, char *argv[]) {
  try {
    std::string file_name = "sample.torrent";
    if (argc > 1) {
      file_name = argv[1];
    }

    std::cout << "🚀 BitTorrent Client - Stage 1 Integration Tests\n";
    std::cout << "Testing file: " << file_name << "\n";
    std::cout << std::string(60, '=') << "\n";

    TorrentFile torrent_file(file_name);
    torrent_file.parse();

    const auto &metadata = torrent_file.getMetadata();
    const auto &piece_info = torrent_file.getPieceInfo();
    const auto &file_mapping = torrent_file.getFileMapping();

    printTorrentInfo(metadata, piece_info);
    printFileMappingSample(file_mapping, metadata);

    std::cout << "\n📋 Running Validation Tests...\n";
    std::cout << std::string(60, '=') << "\n";

    TorrentTestSuite suite;

    TorrentValidator::validateMetadata(metadata, suite);

    TorrentValidator::validateInfoHash(metadata, suite);

    TorrentValidator::validatePieceInfo(piece_info, metadata, suite);

    TorrentValidator::validateTotalSizeConsistency(metadata, piece_info, suite);

    TorrentValidator::validateFileMapping(file_mapping, metadata, piece_info,
                                          suite);

    suite.printSummary();

    return suite.allPassed() ? 0 : 1;

  } catch (const std::exception &e) {
    std::cerr << "\n❌ Fatal Error: " << e.what() << "\n";
    return 1;
  }
}
