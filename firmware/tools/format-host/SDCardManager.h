// SPDX-License-Identifier: MIT
#pragma once
#include "Arduino.h"
#include <fstream>
#include <filesystem>
#include <memory>
constexpr int O_RDONLY = 0, O_RDWR = 1, O_CREAT = 2, O_TRUNC = 4;
class FsFile {
  std::shared_ptr<std::fstream> file;
  std::string path;
public:
  FsFile() = default;
  FsFile(const char *p, int flags) : path(p) {
    auto mode = std::ios::binary | std::ios::in;
    if (flags & O_RDWR) mode |= std::ios::out;
    if (flags & O_TRUNC) mode |= std::ios::trunc;
    file = std::make_shared<std::fstream>(p, mode);
  }
  explicit operator bool() const { return file && file->is_open(); }
  uint64_t fileSize() { return *this ? std::filesystem::file_size(path) : 0; }
  bool seekSet(uint64_t at) { file->clear(); file->seekg(at); file->seekp(at); return bool(*file); }
  int read(void *dst, size_t n) { file->read(static_cast<char *>(dst), n); return file->gcount(); }
  size_t write(const void *src, size_t n) { file->write(static_cast<const char *>(src), n); return *file ? n : 0; }
  uint64_t curPosition() { return uint64_t(file->tellp()); }
  bool sync() { file->flush(); return bool(*file); }
  void close() { if (file) file->close(); }
};
struct HostSD {
  FsFile open(const char *p, int flags = O_RDONLY) { return FsFile(p, flags); }
  bool exists(const char *p) { return std::filesystem::exists(p); }
  bool remove(const char *p) { return !exists(p) || std::filesystem::remove(p); }
  bool replaceFile(const char *a, const char *b) {
    std::error_code error; std::filesystem::rename(a, b, error); return !error;
  }
};
inline HostSD SdMan;
