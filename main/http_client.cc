#include "http_client.h"
#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

HttpResponse HttpClient::get(const std::string &url, int timeout_seconds) {
  std::string scheme, host, path;
  uint16_t port;

  if (!parseUrl(url, scheme, host, port, path)) {
    throw std::runtime_error("Invalid URL format: " + url);
  }

  if (scheme != "http") {
    throw std::runtime_error("Only HTTP is supported.");
  }

  struct hostent *he =
      gethostbyname(host.c_str()); // `gethostbyname` is deprecated
  if (he == nullptr) {
    throw std::runtime_error("Failed to resolve hostname: " + host);
  }

  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    throw std::runtime_error("Failed to create socket");
  }

  int flags = fcntl(sock, F_GETFL, 0);
  fcntl(sock, F_SETFL, flags | O_NONBLOCK);

  struct sockaddr_in server_addr;
  std::memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  std::memcpy(&server_addr.sin_addr.s_addr, he->h_addr, he->h_length);

  int connect_result =
      connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));

  if (connect_result < 0 && errno != EINPROGRESS) {
    close(sock);
    throw std::runtime_error("Failed to connect to " + host);
  }

  struct pollfd pfd;
  pfd.fd = sock;
  pfd.events = POLLOUT;

  int poll_result = poll(&pfd, 1, timeout_seconds * 1000);
  if (poll_result <= 0) {
    close(sock);
    throw std::runtime_error("Connection timeout");
  }

  int sock_error = 0;
  socklen_t len = sizeof(sock_error);
  getsockopt(sock, SOL_SOCKET, SO_ERROR, &sock_error, &len);
  if (sock_error != 0) {
    close(sock);
    throw std::runtime_error("Connection failed");
  }

  std::string request = buildGetRequest(host, path);
  ssize_t sent = send(sock, request.c_str(), request.length(), 0);
  if (sent < 0) {
    close(sock);
    throw std::runtime_error("Failed to send request");
  }

  std::string response_data;
  char buffer[4096];

  while (true) {
    pfd.events = POLLIN;
    poll_result = poll(&pfd, 1, timeout_seconds * 1000);

    if (poll_result <= 0) {
      break;
    }

    ssize_t received = recv(sock, buffer, sizeof(buffer), 0);
    if (received <= 0) {
      break;
    }

    response_data.append(buffer, received);
  }

  close(sock);

  if (response_data.empty()) {
    throw std::runtime_error("No response received from tracker");
  }

  return parseResponse(response_data);
}

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
