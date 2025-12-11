#pragma once

#include "torrent_file.h"
#include <functional>
#include <string>
#include <vector>

struct TestResult {
  std::string test_name;
  bool passed;
  std::string message;
  double duration_ms;
};

class TorrentTestSuite {
private:
  std::vector<TestResult> m_results;
  std::string m_current_test_name;

  void recordResult(bool passed, const std::string &message);

public:
  void runTest(const std::string &name, std::function<void()> test_func);

  void assertEqual(const std::string &actual, const std::string &expected,
                   const std::string &context = "");
  void assertEqual(char actual, char expected, const std::string &context = "");
  void assertEqual(uint64_t actual, uint64_t expected,
                   const std::string &context = "");
  void assertEqual(uint32_t actual, uint32_t expected,
                   const std::string &context = "");
  void assertTrue(bool condition, const std::string &message);
  void assertFalse(bool condition, const std::string &message);
  void assertNotEmpty(const std::string &value,
                      const std::string &context = "");
  void assertGreaterThan(uint64_t actual, uint64_t minimum,
                         const std::string &context = "");
  void assertInRange(uint64_t value, uint64_t min, uint64_t max,
                     const std::string &context = "");

  void printSummary() const;
  bool allPassed() const;
  int failedCount() const;
  int passedCount() const;
};

class TorrentValidator {
public:
  static void validateMetadata(const TorrentMetadata &metadata,
                               TorrentTestSuite &suite);
  static void validatePieceInfo(const PieceInformation &piece_info,
                                const TorrentMetadata &metadata,
                                TorrentTestSuite &suite);
  static void validateFileMapping(const PieceFileMapping &mapping,
                                  const TorrentMetadata &metadata,
                                  const PieceInformation &piece_info,
                                  TorrentTestSuite &suite);
  static void validateInfoHash(const TorrentMetadata &metadata,
                               TorrentTestSuite &suite);

  static void validateTotalSizeConsistency(const TorrentMetadata &metadata,
                                           const PieceInformation &piece_info,
                                           TorrentTestSuite &suite);
};
