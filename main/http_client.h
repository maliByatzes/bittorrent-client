#pragma once

#include <cstdint>
#include <map>
#include <string>

struct HttpResponse {
  int status_code;
  std::string status_message;
  std::map<std::string, std::string> headers;
  std::string body;

  bool isSuccess() const { return status_code >= 200 && status_code < 300; }
};

class HttpClient {
public:
  static HttpResponse get(const std::string &url, int timeout_seconds = 10);

private:
  static bool parseUrl(const std::string &url, std::string &scheme,
                       std::string &host, uint16_t &port, std::string &path);

  static std::string buildGetRequest(const std::string &host,
                                     const std::string &path);

  static HttpResponse parseResponse(const std::string &response_data);
};
