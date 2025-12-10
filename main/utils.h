#pragma once

#include <cstdint>
#include <string>
#include <vector>

uint32_t leftRotate(uint32_t value, uint32_t shift);
std::vector<uint8_t> sha1Preprocess(std::vector<uint8_t> data);
std::string sha1(std::string &data);
std::string sha1(std::vector<uint8_t> &data);

void printHex(const std::vector<uint8_t> &data);
