#include "bdecoder.h"
#include <regex>
#include <stdexcept>
#include <string>
#include <utility>

long long BNode::asInteger() const {
  if (!isInteger())
    throw std::runtime_error("Not an integer node");
  return std::get<long long>(m_value);
}

const std::string &BNode::asString() const {
  if (!isString())
    throw std::runtime_error("Not a string node");
  return std::get<std::string>(m_value);
}

const std::vector<BNode> &BNode::asList() const {
  if (!isList())
    throw std::runtime_error("Not a list node");
  return std::get<std::vector<BNode>>(m_value);
}

const std::map<std::string, BNode> &BNode::asDict() const {
  if (!isDictionary())
    throw std::runtime_error("Not a dictionary node");
  return std::get<std::map<std::string, BNode>>(m_value);
}

const BNode &BNode::operator[](const std::string &key) const {
  const auto &dict = asDict();
  auto it = dict.find(key);
  if (it == dict.end()) {
    throw std::runtime_error("Key not found: " + key);
  }
  return it->second;
}

const BNode &BNode::operator[](size_t index) const {
  const auto &list = asList();
  if (index >= list.size()) {
    throw std::runtime_error("Index out of bounds");
  }
  return list[index];
}

void BNode::print(std::ostream &os, int indent) const {
  std::string indent_str(indent * 2, ' ');

  switch (m_type) {
  case Type::Integer:
    os << asInteger();
    break;
  case Type::String:
    os << "\"" << asString() << "\"";
    break;
  case Type::List:
    os << "[\n";
    for (size_t i = 0; i < asList().size(); ++i) {
      os << indent_str << "  ";
      asList()[i].print(os, indent + 1);
      if (i < asList().size() - 1)
        os << ",";
      os << "\n";
    }
    os << indent_str << "]";
    break;
  case Type::Dictionary:
    os << "{\n";
    size_t count = 0;
    for (const auto &[key, val] : asDict()) {
      os << indent_str << "  \"" << key << "\": ";
      val.print(os, indent + 1);
      if (++count < asDict().size())
        os << ",";
      os << "\n";
    }
    os << indent_str << "}";
    break;
  }
}

void BDecoder::readExpectedChar(char expected_char) {
  int c = m_input.get();
  if (c != expected_char) {
    throw std::runtime_error(std::string("expected '") + expected_char +
                             "' got '" + static_cast<char>(c) + "'");
  }
}

BNode BDecoder::decodeInteger() {
  readExpectedChar('i');
  std::string encoded_integer;
  if (!readUntil(encoded_integer, 'e')) {
    throw std::runtime_error("error during decoding of an integer near '" +
                             encoded_integer + "'");
  }

  std::regex integer_regex("([-+]?(0|[1-9][0-9]*))");
  std::smatch match;
  if (!std::regex_match(encoded_integer, match, integer_regex)) {
    throw std::runtime_error(
        "encountered an encoded integer of invalid format: 'i" +
        encoded_integer + "e'");
  }

  long long value = std::stoll(match[1].str());
  return BNode(value);
}

BNode BDecoder::decodeString() {
  std::string str_len_ascii;
  if (!readUpTo(str_len_ascii, ':')) {
    throw std::runtime_error("error during decoding of a string near '" +
                             str_len_ascii + "'");
  }

  readExpectedChar(':');
  size_t str_len = std::stoull(str_len_ascii);

  std::string str(str_len, char());
  m_input.read(&str[0], str_len);
  size_t chars_read = m_input.gcount();

  if (chars_read != str_len) {
    throw std::runtime_error("expected a string containing " +
                             std::to_string(str_len) +
                             " characters, but read only " +
                             std::to_string(chars_read) + " characters");
  }

  return BNode(str);
}

BNode BDecoder::decodeList() {
  readExpectedChar('l');
  std::vector<BNode> lst;

  while (m_input && m_input.peek() != 'e') {
    lst.push_back(decode());
  }

  readExpectedChar('e');
  return BNode(std::move(lst));
}

BNode BDecoder::decodeDictionary() {
  readExpectedChar('d');
  std::map<std::string, BNode> dict;

  while (m_input && m_input.peek() != 'e') {
    BNode key_node = decode();
    if (!key_node.isString()) {
      throw std::runtime_error("Dictionary key must be a string");
    }
    std::string key = key_node.asString();

    BNode value = decode();

    dict[key] = std::move(value);
  }

  readExpectedChar('e');
  return BNode(std::move(dict));
}

bool BDecoder::readUntil(std::string &read_data, char last) {
  char chr;
  while (m_input.get(chr)) {
    if (chr == last) {
      return true;
    }
    read_data += chr;
  }
  return false;
}

bool BDecoder::readUpTo(std::string &read_data, char sentinel) {
  while (m_input.peek() != std::char_traits<char>::eof() &&
         m_input.peek() != sentinel) {
    read_data += m_input.get();
  }
  return m_input && m_input.peek() == sentinel;
}

BNode BDecoder::decode() {
  int next = m_input.peek();

  switch (next) {
  case 'd':
    return decodeDictionary();
  case 'i':
    return decodeInteger();
  case 'l':
    return decodeList();
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
    return decodeString();
  default:
    throw std::runtime_error(std::string("unexpected character: '") +
                             static_cast<char>(next) + "'");
  }
}

void BDecoder::validate() {
  if (m_input.peek() != std::char_traits<char>::eof()) {
    throw std::runtime_error("input contains undecoded characters");
  }
}

BNode bdecode(const std::string &data) {
  std::istringstream input(data);
  BDecoder decoder(input);
  BNode result = decoder.decode();
  decoder.validate();
  return result;
}

BNode bdecode(std::istream &input) {
  BDecoder decoder(input);
  BNode result = decoder.decode();
  decoder.validate();
  return result;
}
