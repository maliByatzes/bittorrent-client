#include "http_client.h"
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>

// HttpResponse get(const std::string &url, int timeout_seconds = 10);

bool HttpClient::parseUrl(const std::string &url, std::string &scheme,
                          std::string &host, uint16_t &port,
                          std::string &path) {
  std::regex url_regex(R"(^(https?)://([^/:]+)(?::(\d+))?(/.*)?$)");
  std::smatch matches;

  if (!std::regex_match(url, matches, url_regex)) {
    return false;
  }

  scheme = matches[1].str();
  host = matches[2].str();

  if (matches[3].matched) {
    port = static_cast<uint16_t>(std::stoi(matches[3].str()));
  } else {
    port = (scheme == "https") ? 443 : 80;
  }

  path = matches[4].matched ? matches[4].str() : "/";

  return true;
}

std::string HttpClient::buildGetRequest(const std::string &host,
                                        const std::string &path) {
  std::ostringstream request;

  request << "GET " << path << " HTTP/1.1\r\n";
  request << "Host: " << host << "\r\n";
  request << "Connection: closer\r\n";
  request << "User-Agent: BitTorrent Client/1.0\r\n";
  request << "\r\n";

  return request.str();
}

HttpResponse HttpClient::parseResponse(const std::string &response_data) {
  HttpResponse response;

  size_t header_end = response_data.find("\r\n\r\n");
  if (header_end == std::string::npos) {
    throw std::runtime_error("Invalid HTTP response: no header/body separator");
  }

  std::string headers_section = response_data.substr(0, header_end);
  response.body = response_data.substr(header_end + 4);

  std::istringstream stream(headers_section);
  std::string http_version;
  stream >> http_version >> response.status_code;
  std::getline(stream, response.status_message);

  if (!response.status_message.empty() && response.status_message[0] == ' ') {
    response.status_message = response.status_message.substr(1);
  }

  if (!response.status_message.empty() &&
      response.status_message.back() == '\r') {
    response.status_message.pop_back();
  }

  std::string line;
  while (std::getline(stream, line) && !line.empty()) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }

    size_t colon_pos = line.find(':');
    if (colon_pos != std::string::npos) {
      std::string header_name = line.substr(0, colon_pos);
      std::string header_value = line.substr(colon_pos + 1);

      size_t value_start = header_value.find_first_of(" \t");
      if (value_start != std::string::npos) {
        header_value = header_value.substr(value_start);
      }

      response.headers[header_name] = header_value;
    }
  }

  return response;
}
