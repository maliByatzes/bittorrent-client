#include "utils.h"
#include <cstdint>
#include <iomanip>
#include <ios>
#include <iostream>
#include <sstream>
#include <vector>

uint32_t leftRotate(uint32_t value, uint32_t shift) {
  return (value << shift) | (value >> (32 - shift));
}

std::vector<uint8_t> sha1Preprocess(std::vector<uint8_t> data) {
  uint64_t orig_len = data.size() * 8;
  std::cerr << "original length= " << orig_len << "\n";

  data.push_back(0x80);

  while (data.size() % 64 != 56) {
    data.push_back(0x00);
  }

  for (int i = 7; i >= 0; i--) {
    data.push_back((orig_len >> (i * 8)) & 0xFFU);
  }

  return data;
}

std::string sha1(std::string &data) {
  std::vector<uint8_t> data_bytes(data.begin(), data.end());
  return sha1(data_bytes);
}

std::string sha1(std::vector<uint8_t> &data) {
  uint32_t h0 = 0x67452301;
  uint32_t h1 = 0xEFCDAB89;
  uint32_t h2 = 0x98BADCFE;
  uint32_t h3 = 0x10325476;
  uint32_t h4 = 0xC3D2E1F0;

  auto padded_data = sha1Preprocess(data);

  // Processing
  const size_t chunk_size = 64;
  for (size_t chunk_start = 0; chunk_start < padded_data.size();
       chunk_start += chunk_size) {
    uint32_t w[80];

    for (int i = 0; i < 16; i++) {
      w[i] = (padded_data[chunk_start + i * 4] << 24U) |
             (padded_data[chunk_start + i * 4 + 1] << 16U) |
             (padded_data[chunk_start + i * 4 + 2] << 8U) |
             (padded_data[chunk_start + i * 4 + 3]);
    }

    for (int i = 16; i < 80; i++) {
      w[i] = leftRotate(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    uint32_t a = h0;
    uint32_t b = h1;
    uint32_t c = h2;
    uint32_t d = h3;
    uint32_t e = h4;

    for (int i = 0; i < 80; i++) {
      uint32_t f, k;

      if (i <= 19) {
        f = (b & c) | ((~b) & d);
        k = 0x5A827999;
      } else if (i <= 39) {
        f = b ^ c ^ d;
        k = 0x6ED9EBA1;
      } else if (i <= 59) {
        f = (b & c) | (b & d) | (c & d);
        k = 0x8F1BBCDC;
      } else {
        f = b ^ c ^ d;
        k = 0xCA62C1D6;
      }

      uint32_t temp = leftRotate(a, 5) + f + e + k + w[i];
      e = d;
      d = c;
      c = leftRotate(b, 30);
      b = a;
      a = temp;
    }

    h0 += a;
    h1 += b;
    h2 += c;
    h3 += d;
    h4 += e;
  }

  std::stringstream ss;
  ss << std::hex << std::setfill('0');
  ss << std::setw(8) << h0;
  ss << std::setw(8) << h1;
  ss << std::setw(8) << h2;
  ss << std::setw(8) << h3;
  ss << std::setw(8) << h4;

  return ss.str();
}

std::string bytesToHex(const std::array<uint8_t, 20> &bytes) {
  std::stringstream ss;
  ss << std::hex << std::setfill('0');
  for (uint8_t byte : bytes) {
    ss << std::setw(2) << static_cast<int>(byte);
  }
  return ss.str();
}

std::string bytesToURLEncoded(const std::array<uint8_t, 20> &bytes) {
  std::stringstream ss;
  for (uint8_t byte : bytes) {
    ss << '%' << std::hex << std::setw(2) << std::setfill('0') << std::uppercase
       << static_cast<int>(byte);
  }
  return ss.str();
}

std::array<uint8_t, 20> sha1ToBytes(std::vector<uint8_t> &data) {
  uint32_t h0 = 0x67452301;
  uint32_t h1 = 0xEFCDAB89;
  uint32_t h2 = 0x98BADCFE;
  uint32_t h3 = 0x10325476;
  uint32_t h4 = 0xC3D2E1F0;

  auto padded_data = sha1Preprocess(data);

  const size_t chunk_size = 64;
  for (size_t chunk_start = 0; chunk_start < padded_data.size();
       chunk_start += chunk_size) {
    uint32_t w[80];

    for (int i = 0; i < 16; i++) {
      w[i] = (padded_data[chunk_start + i * 4] << 24U) |
             (padded_data[chunk_start + i * 4 + 1] << 16U) |
             (padded_data[chunk_start + i * 4 + 2] << 8U) |
             (padded_data[chunk_start + i * 4 + 3]);
    }

    for (int i = 16; i < 80; i++) {
      w[i] = leftRotate(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    uint32_t a = h0;
    uint32_t b = h1;
    uint32_t c = h2;
    uint32_t d = h3;
    uint32_t e = h4;

    for (int i = 0; i < 80; i++) {
      uint32_t f, k;

      if (i <= 19) {
        f = (b & c) | ((~b) & d);
        k = 0x5A827999;
      } else if (i <= 39) {
        f = b ^ c ^ d;
        k = 0x6ED9EBA1;
      } else if (i <= 59) {
        f = (b & c) | (b & d) | (c & d);
        k = 0x8F1BBCDC;
      } else {
        f = b ^ c ^ d;
        k = 0xCA62C1D6;
      }

      uint32_t temp = leftRotate(a, 5) + f + e + k + w[i];
      e = d;
      d = c;
      c = leftRotate(b, 30);
      b = a;
      a = temp;
    }

    h0 += a;
    h1 += b;
    h2 += c;
    h3 += d;
    h4 += e;
  }

  std::array<uint8_t, 20> result;
  for (int i = 0; i < 5; i++) {
    uint32_t hash_word;
    if (i == 0)
      hash_word = h0;
    else if (i == 1)
      hash_word = h1;
    else if (i == 2)
      hash_word = h2;
    else if (i == 3)
      hash_word = h3;
    else
      hash_word = h4;

    result[i * 4 + 0] = (hash_word >> 24) & 0xFF;
    result[i * 4 + 1] = (hash_word >> 16) & 0xFF;
    result[i * 4 + 2] = (hash_word >> 8) & 0xFF;
    result[i * 4 + 3] = hash_word & 0xFF;
  }

  return result;
}

void printHex(const std::vector<uint8_t> &data) {
  for (size_t i = 0; i < data.size(); i++) {
    std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)data[i]
              << " ";
    if ((i + 1) % 16 == 0)
      std::cout << "\n";
  }
  if (data.size() % 16 != 0)
    std::cout << "\n";
}
