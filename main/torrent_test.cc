#include "torrent_test.h"
#include "utils.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

void TorrentTestSuite::recordResult(bool passed, const std::string &message) {
  TestResult result;
  result.test_name = m_current_test_name;
  result.passed = passed;
  result.message = message;
  result.duration_ms = 0.0;
  m_results.push_back(result);

  if (!passed) {
    std::cerr << "  ❌ FAILED: " << message << "\n";
  }
}

void TorrentTestSuite::runTest(const std::string &name,
                               std::function<void()> test_func) {
  m_current_test_name = name;
  std::cout << "\n🔍 Running: " << name << "\n";

  auto start = std::chrono::high_resolution_clock::now();

  try {
    test_func();
    auto end = std::chrono::high_resolution_clock::now();
    double duration =
        std::chrono::duration<double, std::milli>(end - start).count();

    TestResult result;
    result.test_name = name;
    result.passed = true;
    result.message = "Test passed";
    result.duration_ms = duration;
    m_results.push_back(result);

    std::cout << "  ✅ PASSED (" << std::fixed << std::setprecision(2)
              << duration << " ms)\n";
  } catch (const std::exception &e) {
    auto end = std::chrono::high_resolution_clock::now();
    double duration =
        std::chrono::duration<double, std::milli>(end - start).count();

    TestResult result;
    result.test_name = name;
    result.passed = false;
    result.message = std::string("Exception: ") + e.what();
    result.duration_ms = duration;
    m_results.push_back(result);

    std::cerr << "  ❌ FAILED: " << e.what() << "\n";
  }
}

void TorrentTestSuite::assertEqual(const std::string &actual,
                                   const std::string &expected,
                                   const std::string &context) {
  if (actual != expected) {
    std::ostringstream oss;
    oss << context << " - Expected: \"" << expected << "\", Got: \"" << actual
        << "\"";
    recordResult(false, oss.str());
    throw std::runtime_error(oss.str());
  }
}

void TorrentTestSuite::assertEqual(char actual, char expected,
                                   const std::string &context) {
  if (actual != expected) {
    std::ostringstream oss;
    oss << context << " - Expected: \"" << expected << "\", Got: \"" << actual
        << "\"";
    recordResult(false, oss.str());
    throw std::runtime_error(oss.str());
  }
}

void TorrentTestSuite::assertEqual(uint64_t actual, uint64_t expected,
                                   const std::string &context) {
  if (actual != expected) {
    std::ostringstream oss;
    oss << context << " - Expected: " << expected << ", Got: " << actual;
    recordResult(false, oss.str());
    throw std::runtime_error(oss.str());
  }
}

void TorrentTestSuite::assertEqual(uint32_t actual, uint32_t expected,
                                   const std::string &context) {
  if (actual != expected) {
    std::ostringstream oss;
    oss << context << " - Expected: " << expected << ", Got: " << actual;
    recordResult(false, oss.str());
    throw std::runtime_error(oss.str());
  }
}

void TorrentTestSuite::assertTrue(bool condition, const std::string &message) {
  if (!condition) {
    recordResult(false, message);
    throw std::runtime_error(message);
  }
}

void TorrentTestSuite::assertFalse(bool condition, const std::string &message) {
  if (condition) {
    recordResult(false, message);
    throw std::runtime_error(message);
  }
}

void TorrentTestSuite::assertNotEmpty(const std::string &value,
                                      const std::string &context) {
  if (value.empty()) {
    std::string msg = context + " - Value should not be empty";
    recordResult(false, msg);
    throw std::runtime_error(msg);
  }
}

void TorrentTestSuite::assertGreaterThan(uint64_t actual, uint64_t minimum,
                                         const std::string &context) {
  if (actual <= minimum) {
    std::ostringstream oss;
    oss << context << " - Expected > " << minimum << ", Got: " << actual;
    recordResult(false, oss.str());
    throw std::runtime_error(oss.str());
  }
}

void TorrentTestSuite::assertInRange(uint64_t value, uint64_t min, uint64_t max,
                                     const std::string &context) {
  if (value < min || value > max) {
    std::ostringstream oss;
    oss << context << " - Expected in range [" << min << ", " << max
        << "], Got: " << value;
    recordResult(false, oss.str());
    throw std::runtime_error(oss.str());
  }
}

void TorrentTestSuite::printSummary() const {
  std::cout << "\n" << std::string(60, '=') << "\n";
  std::cout << "TEST SUMMARY\n";
  std::cout << std::string(60, '=') << "\n";

  int passed = passedCount();
  int failed = failedCount();
  int total = m_results.size();

  std::cout << "Total Tests: " << total << "\n";
  std::cout << "✅ Passed: " << passed << "\n";
  std::cout << "❌ Failed: " << failed << "\n";
  std::cout << "Success Rate: " << std::fixed << std::setprecision(1)
            << (total > 0 ? (100.0 * passed / total) : 0.0) << "%\n";

  if (failed > 0) {
    std::cout << "\nFailed Tests:\n";
    for (const auto &result : m_results) {
      if (!result.passed) {
        std::cout << "  ❌ " << result.test_name << "\n";
        std::cout << "     " << result.message << "\n";
      }
    }
  }

  std::cout << std::string(60, '=') << "\n";
}

bool TorrentTestSuite::allPassed() const {
  for (const auto &result : m_results) {
    if (!result.passed)
      return false;
  }
  return true;
}

int TorrentTestSuite::failedCount() const {
  int count = 0;
  for (const auto &result : m_results) {
    if (!result.passed)
      count++;
  }
  return count;
}

int TorrentTestSuite::passedCount() const {
  int count = 0;
  for (const auto &result : m_results) {
    if (result.passed)
      count++;
  }
  return count;
}

// ========== TorrentValidator Implementation ==========

void TorrentValidator::validateMetadata(const TorrentMetadata &metadata,
                                        TorrentTestSuite &suite) {
  suite.runTest("Metadata: Name is not empty",
                [&]() { suite.assertNotEmpty(metadata.name, "Torrent name"); });

  suite.runTest("Metadata: At least one announce URL", [&]() {
    suite.assertGreaterThan(metadata.announce_urls.size(), 0,
                            "Number of announce URLs");
  });

  suite.runTest("Metadata: Valid piece length", [&]() {
    suite.assertInRange(metadata.piece_length, 16384, 16777216, "Piece length");
  });

  suite.runTest("Metadata: Total size > 0", [&]() {
    suite.assertGreaterThan(metadata.total_size, 0, "Total size");
  });

  suite.runTest("Metadata: At least one file", [&]() {
    suite.assertGreaterThan(metadata.files.size(), 0, "Number of files");
  });

  suite.runTest("Metadata: Files have valid paths", [&]() {
    for (size_t i = 0; i < metadata.files.size(); i++) {
      suite.assertGreaterThan(metadata.files[i].path.size(), 0,
                              "File " + std::to_string(i) + " path");
      suite.assertGreaterThan(metadata.files[i].length, 0,
                              "File " + std::to_string(i) + " length");
    }
  });

  suite.runTest("Metadata: Sum of file sizes equals total size", [&]() {
    uint64_t sum = 0;
    for (const auto &file : metadata.files) {
      sum += file.length;
    }
    suite.assertEqual(sum, metadata.total_size, "Sum of file sizes");
  });

  suite.runTest("Metadata: Single file check consistency", [&]() {
    if (metadata.isSingleFile()) {
      suite.assertEqual(metadata.files.size(), size_t(1),
                        "Single file should have 1 entry");
      suite.assertEqual(metadata.files[0].path.size(), size_t(1),
                        "Single file path should have 1 component");
    }
  });
}

void TorrentValidator::validateInfoHash(const TorrentMetadata &metadata,
                                        TorrentTestSuite &suite) {
  suite.runTest("Info Hash: Hex format is 40 characters", [&]() {
    suite.assertEqual(metadata.info_hash_hex.size(), size_t(40),
                      "Info hash hex length");
  });

  suite.runTest("Info Hash: URL-encoded format is 60 characters", [&]() {
    suite.assertEqual(metadata.info_hash_urlencoded.size(), size_t(60),
                      "Info hash URL-encoded length (20 bytes * 3 chars each)");
  });

  suite.runTest("Info Hash: Hex contains only valid characters", [&]() {
    for (char c : metadata.info_hash_hex) {
      bool valid = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
      suite.assertTrue(valid, "Info hash hex contains invalid character: " +
                                  std::string(1, c));
    }
  });

  suite.runTest("Info Hash: URL-encoded format is correct", [&]() {
    for (size_t i = 0; i < metadata.info_hash_urlencoded.size(); i += 3) {
      suite.assertEqual(metadata.info_hash_urlencoded[i], '%',
                        "URL-encoded format position " + std::to_string(i));
    }
  });

  suite.runTest("Info Hash: Bytes match hex representation", [&]() {
    std::string reconstructed_hex = bytesToHex(metadata.info_hash_bytes);
    suite.assertEqual(reconstructed_hex, metadata.info_hash_hex,
                      "Info hash bytes vs hex");
  });
}

void TorrentValidator::validatePieceInfo(const PieceInformation &piece_info,
                                         const TorrentMetadata &metadata,
                                         TorrentTestSuite &suite) {
  suite.runTest("Piece Info: At least one piece", [&]() {
    suite.assertGreaterThan(piece_info.totalPieces(), size_t(0),
                            "Number of pieces");
  });

  suite.runTest("Piece Info: Piece length matches metadata", [&]() {
    suite.assertEqual(piece_info.piece_length, metadata.piece_length,
                      "Piece length consistency");
  });

  suite.runTest("Piece Info: Expected number of pieces", [&]() {
    size_t expected_pieces = (metadata.total_size + metadata.piece_length - 1) /
                             metadata.piece_length;
    suite.assertEqual(piece_info.totalPieces(), expected_pieces,
                      "Calculated vs actual piece count");
  });

  suite.runTest("Piece Info: Last piece size is valid", [&]() {
    suite.assertGreaterThan(piece_info.last_piece_size, uint32_t(0),
                            "Last piece size");
    suite.assertTrue(piece_info.last_piece_size <= metadata.piece_length,
                     "Last piece size should not exceed piece length");
  });

  suite.runTest("Piece Info: All hashes are 20 bytes", [&]() {
    for (size_t i = 0; i < piece_info.totalPieces(); i++) {
      suite.assertEqual(piece_info.getHash(i).size(), size_t(20),
                        "Piece " + std::to_string(i) + " hash size");
    }
  });

  suite.runTest("Piece Info: Hashes are not all zeros", [&]() {
    if (piece_info.totalPieces() > 0) {
      const auto &first_hash = piece_info.getHash(0);
      bool all_zeros = true;
      for (uint8_t byte : first_hash) {
        if (byte != 0) {
          all_zeros = false;
          break;
        }
      }
      suite.assertFalse(all_zeros, "First piece hash should not be all zeros");
    }
  });
}

void TorrentValidator::validateTotalSizeConsistency(
    const TorrentMetadata &metadata, const PieceInformation &piece_info,
    TorrentTestSuite &suite) {

  suite.runTest("Consistency: Total size from pieces", [&]() {
    uint64_t calculated_size = 0;

    if (piece_info.totalPieces() > 1) {
      calculated_size = (piece_info.totalPieces() - 1) *
                        static_cast<uint64_t>(metadata.piece_length);
      calculated_size += piece_info.last_piece_size;
    } else if (piece_info.totalPieces() == 1) {
      calculated_size = piece_info.last_piece_size;
    }

    suite.assertEqual(calculated_size, metadata.total_size,
                      "Total size from pieces vs metadata");
  });
}

void TorrentValidator::validateFileMapping(const PieceFileMapping &mapping,
                                           const TorrentMetadata &metadata,
                                           const PieceInformation &piece_info,
                                           TorrentTestSuite &suite) {

  suite.runTest("File Mapping: Mapping exists for all pieces", [&]() {
    suite.assertEqual(mapping.piece_to_file_map.size(),
                      piece_info.totalPieces(),
                      "File mapping size vs piece count");
  });

  suite.runTest("File Mapping: Each piece maps to at least one file", [&]() {
    for (size_t i = 0; i < mapping.piece_to_file_map.size(); i++) {
      suite.assertGreaterThan(mapping.piece_to_file_map[i].size(), size_t(0),
                              "Piece " + std::to_string(i) + " mapping");
    }
  });

  suite.runTest("File Mapping: Segment file indices are valid", [&]() {
    for (size_t piece_idx = 0; piece_idx < mapping.piece_to_file_map.size();
         piece_idx++) {
      for (const auto &segment : mapping.piece_to_file_map[piece_idx]) {
        suite.assertTrue(segment.file_index < metadata.files.size(),
                         "Piece " + std::to_string(piece_idx) +
                             " has invalid file index: " +
                             std::to_string(segment.file_index));
      }
    }
  });

  suite.runTest("File Mapping: Segment lengths are valid", [&]() {
    for (size_t piece_idx = 0; piece_idx < mapping.piece_to_file_map.size();
         piece_idx++) {
      uint32_t total_segment_length = 0;

      for (const auto &segment : mapping.piece_to_file_map[piece_idx]) {
        suite.assertGreaterThan(segment.segment_length, uint32_t(0),
                                "Piece " + std::to_string(piece_idx) +
                                    " segment length");
        total_segment_length += segment.segment_length;
      }

      uint32_t expected_piece_size = (piece_idx == piece_info.totalPieces() - 1)
                                         ? piece_info.last_piece_size
                                         : metadata.piece_length;

      suite.assertEqual(total_segment_length, expected_piece_size,
                        "Piece " + std::to_string(piece_idx) +
                            " total segment length");
    }
  });

  suite.runTest("File Mapping: File offsets are within bounds", [&]() {
    for (size_t piece_idx = 0; piece_idx < mapping.piece_to_file_map.size();
         piece_idx++) {
      for (const auto &segment : mapping.piece_to_file_map[piece_idx]) {
        const auto &file = metadata.files[segment.file_index];
        uint64_t segment_end = segment.file_offset + segment.segment_length;

        suite.assertTrue(segment_end <= file.length,
                         "Piece " + std::to_string(piece_idx) +
                             " segment exceeds file " +
                             std::to_string(segment.file_index) + " bounds");
      }
    }
  });

  suite.runTest("File Mapping: Total mapped bytes equals total size", [&]() {
    uint64_t total_mapped = 0;

    for (const auto &piece_segments : mapping.piece_to_file_map) {
      for (const auto &segment : piece_segments) {
        total_mapped += segment.segment_length;
      }
    }

    suite.assertEqual(total_mapped, metadata.total_size, "Total mapped bytes");
  });
}
