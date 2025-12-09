#include "torrent_file.h"
#include "utils.h"
#include <iostream>
#include <string>

int main() {
  std::string file_name{"sample.torrent"};

  TorrentFile torrent_file(file_name);
  torrent_file.parse();

  std::vector<std::pair<std::string, std::string>> test_cases = {
      {"", "da39a3ee5e6b4b0d3255bfef95601890afd80709"},
      {"abc", "a9993e364706816aba3e25717850c26c9cd0d89d"},
      {"The quick brown fox jumps over the lazy dog",
       "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12"},
      {"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
       "84983e441c3bd26ebaae4aa1f95129e5e54670f1"}};

  std::cout << "SHA-1 Implementation Test Results:\n";
  std::cout << std::string(70, '=') << "\n\n";

  for (const auto &test : test_cases) {
    std::string message = test.first;
    std::string expected = test.second;
    std::string result = sha1(message);

    std::cout << "Message: \"" << (message.empty() ? "(empty string)" : message)
              << "\"\n";
    std::cout << "Expected: " << expected << "\n";
    std::cout << "Got:      " << result << "\n";
    std::cout << "Status:   " << (result == expected ? "✅ PASS" : "❌ FAIL")
              << "\n\n";
  }
}
