#pragma once
#include <Font.h>
#include <cstring>

namespace fm {
// Cache measurements only. Values, styles and rasterization remain unchanged.
class LayoutMetricsCache : public freeink::font::Font {
public:
  struct Entry {
    uint32_t left, right;
    uint16_t size;
    uint8_t style, operation;
    int32_t value;
  };
  void begin(Font &font, Entry *entries, uint32_t count) {
    font_ = &font;
    entries_ = entries;
    count_ = entries ? count : 0;
    clear();
  }
  void clear() {
    if (entries_) std::memset(entries_, 0, sizeof(Entry) * count_);
    hits = misses = 0;
  }
  int16_t advance(uint32_t cp, uint16_t size, uint8_t style) override {
    return value(1, cp, 0, size, style);
  }
  int16_t lineHeight(uint16_t size) override { return value(2, 0, 0, size, 0); }
  int16_t ascent(uint16_t size) override { return value(3, 0, 0, size, 0); }
  int16_t kerning(uint32_t a, uint32_t b, uint16_t size, uint8_t style) override {
    return value(4, a, b, size, style);
  }
  uint32_t ligature(uint32_t a, uint32_t b, uint8_t style) override {
    return uint32_t(value(5, a, b, 0, style));
  }
  bool covers(uint32_t cp) override { return value(6, cp, 0, 0, 0) != 0; }
  uint32_t hits = 0, misses = 0;
private:
  int32_t value(uint8_t op, uint32_t a, uint32_t b, uint16_t size, uint8_t style) {
    uint32_t hash = a * 0x9e3779b1u ^ b * 0x85ebca6bu ^
                    uint32_t(size) * 0xc2b2ae35u ^ uint32_t(style) * 97u ^ op;
    hash ^= hash >> 16;
    Entry *e = count_ ? &entries_[hash % count_] : nullptr;
    if (e && e->operation == op && e->left == a && e->right == b &&
        e->size == size && e->style == style) {
      ++hits;
      return e->value;
    }
    ++misses;
    int32_t result = 0;
    switch (op) {
    case 1: result = font_->advance(a, size, style); break;
    case 2: result = font_->lineHeight(size); break;
    case 3: result = font_->ascent(size); break;
    case 4: result = font_->kerning(a, b, size, style); break;
    case 5: result = int32_t(font_->ligature(a, b, style)); break;
    case 6: result = font_->covers(a); break;
    }
    if (e) *e = {a, b, size, style, op, result};
    return result;
  }
  Font *font_ = nullptr;
  Entry *entries_ = nullptr;
  uint32_t count_ = 0;
};
} // namespace fm
