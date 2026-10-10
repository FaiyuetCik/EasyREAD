// SPDX-License-Identifier: MIT
// Host-only adapters; production parser/importer source is compiled unchanged.
#pragma once
#include <string>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <chrono>
#include <cctype>
class String : public std::string {
public:
  using std::string::string;
  using std::string::operator=;
  using std::string::operator+=;
  String() = default;
  String(const std::string &s) : std::string(s) {}
  String(unsigned n) : std::string(std::to_string(n)) {}
  bool isEmpty() const { return empty(); }
  bool startsWith(const char *s) const { return rfind(s, 0) == 0; }
  int indexOf(const char *s) const { auto p = find(s); return p == npos ? -1 : int(p); }
  void toLowerCase() { for (char &c : *this) c = char(std::tolower(static_cast<unsigned char>(c))); }
  void trim() {
    auto first = find_first_not_of(" \r\n\t"), last = find_last_not_of(" \r\n\t");
    *this = first == npos ? std::string() : substr(first, last - first + 1);
  }
  friend String operator+(const String &a, const String &b) { return String(std::string(a) + std::string(b)); }
  friend String operator+(const char *a, const String &b) { return String(std::string(a) + std::string(b)); }
  friend String operator+(const String &a, const char *b) { return String(std::string(a) + b); }
};
inline uint32_t millis() {
  return uint32_t(std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count());
}
inline void delay(unsigned) {}
