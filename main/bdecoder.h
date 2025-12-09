#pragma once

#include <istream>
#include <map>
#include <ostream>
#include <string>
#include <variant>
#include <vector>

// TODO: Handle edge cases for bdecoding

class BNode;

using BNodeValue = std::variant<long long, std::string, std::vector<BNode>,
                                std::map<std::string, BNode>>;

class BNode {
public:
  enum class Type { Integer, String, List, Dictionary };

private:
  Type m_type;
  BNodeValue m_value;

public:
  BNode() : m_type(Type::String), m_value(std::string()) {}

  explicit BNode(long long val) : m_type(Type::Integer), m_value(val) {}

  explicit BNode(const std::string &val) : m_type(Type::String), m_value(val) {}

  explicit BNode(std::vector<BNode> val)
      : m_type(Type::List), m_value(std::move(val)) {}

  explicit BNode(std::map<std::string, BNode> val)
      : m_type(Type::Dictionary), m_value(std::move(val)) {}

  Type type() const { return m_type; }
  bool isInteger() const { return m_type == Type::Integer; }
  bool isString() const { return m_type == Type::String; }
  bool isList() const { return m_type == Type::List; }
  bool isDictionary() const { return m_type == Type::Dictionary; }

  long long asInteger() const;
  const std::string &asString() const;
  const std::vector<BNode> &asList() const;
  const std::map<std::string, BNode> &asDict() const;

  const BNode &operator[](const std::string &key) const;
  const BNode &operator[](size_t index) const;

  void print(std::ostream &os, int indent = 0) const;
};

class BDecoder {
private:
  std::istream &m_input;

  void readExpectedChar(char expected_char);
  BNode decodeInteger();
  BNode decodeString();
  BNode decodeList();
  BNode decodeDictionary();

  bool readUntil(std::string &read_data, char last);
  bool readUpTo(std::string &read_data, char sentinel);

public:
  explicit BDecoder(std::istream &input) : m_input(input) {}

  BNode decode();
  void validate();
};

BNode bdecode(const std::string &data);
BNode bdecode(std::istream &input);
