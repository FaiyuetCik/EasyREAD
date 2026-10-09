#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>

namespace fm {
// FreeType revisits small font tables between glyph reads. Retain SD sectors
// in PSRAM without changing the stream callback's byte or EOF semantics.
class FontReadCache {
public:
  using Read = unsigned long (*)(void *, unsigned long, unsigned char *, unsigned long);
  struct Entry {
    void *context;
    unsigned long offset;
    uint16_t bytes;
    bool valid;
    unsigned char data[512];
  };
  void begin(Read read, Entry *entries, size_t count) {
    read_ = read; entries_ = entries; count_ = entries ? count : 0; clear();
  }
  void clear() {
    for (size_t i = 0; i < count_; ++i) entries_[i].valid = false;
    hits = misses = 0;
  }
  unsigned long read(void *ctx, unsigned long at, unsigned char *dst, unsigned long n) {
    if (!n) return 0;
    if (!count_) return read_(ctx, at, dst, n);
    unsigned long done = 0;
    while (done < n) {
      unsigned long pos = at + done, base = pos & ~511UL;
      if (pos < at) break;
      size_t slot = ((base / 512) * 2654435761UL ^ (uintptr_t(ctx) >> 4)) % count_;
      Entry &e = entries_[slot];
      if (!e.valid || e.context != ctx || e.offset != base) {
        ++misses;
        e.valid = false;
        e.bytes = uint16_t(std::min<unsigned long>(512, read_(ctx, base, e.data, 512)));
        e.context = ctx; e.offset = base; e.valid = true;
      } else ++hits;
      unsigned long offset = pos - base;
      if (offset >= e.bytes) break;
      unsigned long chunk = std::min<unsigned long>(n - done, e.bytes - offset);
      std::memcpy(dst + done, e.data + offset, chunk);
      done += chunk;
      if (e.bytes < 512) break;
    }
    return done;
  }
  uint32_t hits = 0, misses = 0;
private:
  Read read_ = nullptr;
  Entry *entries_ = nullptr;
  size_t count_ = 0;
};
} // namespace fm
