#pragma once
#include <BookStorage.h>
#include <Preferences.h>
#include <SDCardManager.h>
#include <epub/ZipCatalog.h>

class SdSource : public freeink::book::BookSource {
public:
  FsFile file;
  bool open(const char *path) {
    file.close();
    file = SdMan.open(path);
    return bool(file) && !file.isDirectory();
  }
  int32_t readAt(uint64_t offset, void *dst, uint32_t len) override {
    if (!file || !file.seekSet(offset))
      return -1;
    return file.read(dst, len);
  }
  uint64_t size() const override { return file ? file.fileSize() : 0; }
  uint32_t fingerprint() {
    uint8_t bytes[4096];
    uint32_t hash = 2166136261u;
    for (uint64_t offset = 0; offset < size();) {
      int32_t n = readAt(offset, bytes, sizeof(bytes));
      if (n <= 0)
        return 0;
      for (int32_t i = 0; i < n; ++i)
        hash = (hash ^ bytes[i]) * 16777619u;
      offset += n;
      delay(1);
    }
    return hash;
  }
};

// Only disposable layout caches are written to SD. Reading positions and
// settings use NVS, whose blob writes survive resets without torn records.
class SdCache : public freeink::book::CacheStorage {
public:
  String root;
  FsFile writing;
  String finalPath, tempPath;
  bool healthy = false;
  String path(const char *name) const { return root + "/" + name; }
  bool exists(const char *name) override {
    return SdMan.exists(path(name).c_str());
  }
  bool remove(const char *name) override {
    return !exists(name) || SdMan.remove(path(name).c_str());
  }
  int64_t fileSize(const char *name) override {
    auto f = SdMan.open(path(name).c_str());
    return f ? int64_t(f.fileSize()) : -1;
  }
  int32_t readAt(const char *name, uint32_t offset, void *dst,
                 uint32_t len) override {
    auto f = SdMan.open(path(name).c_str());
    if (!f || !f.seekSet(offset))
      return -1;
    return f.read(dst, len);
  }
  bool beginWrite(const char *name) override {
    writing.close();
    finalPath = path(name);
    tempPath = finalPath + ".tmp";
    writing = SdMan.open(tempPath.c_str(), O_RDWR | O_CREAT | O_TRUNC);
    return healthy = bool(writing);
  }
  bool write(const void *data, uint32_t len) override {
    healthy = healthy && writing.write(data, len) == len;
    return healthy;
  }
  bool endWrite() override {
    bool ok = healthy && writing.sync();
    writing.close();
    if (!ok) {
      SdMan.remove(tempPath.c_str());
      return false;
    }
    // Incomplete files never acquire the final cache name. A power failure
    // during replace leaves a missing cache, which is safely rebuilt.
    return SdMan.replaceFile(tempPath.c_str(), finalPath.c_str());
  }
  int32_t readBackAt(uint32_t offset, void *dst, uint32_t len) override {
    uint64_t cursor = writing.curPosition();
    if (!writing.sync() || !writing.seekSet(offset))
      return -1;
    int32_t n = writing.read(dst, len);
    if (!writing.seekSet(cursor))
      return -1;
    return n;
  }
};

struct ReadingPosition {
  uint32_t version = 1;
  uint32_t fingerprint = 0;
  uint32_t chapter = 0;
  uint32_t character = 0;
};
