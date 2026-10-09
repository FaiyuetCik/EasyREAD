#pragma once
#include <cache/PageCache.h>
#include <algorithm>
#include <cstring>

namespace fm {
using namespace freeink::book;
class IncrementalCacheStorage : public CacheStorage {
public:
  virtual void discardWrite() = 0;
};
// Layout, cache writes and rendering alternate on the application thread.
// Their arenas and the writer's filename stay alive for the entire session.
class IncrementalChapter {
public:
  BookStatus open(IncrementalCacheStorage &storage, const char *name, uint32_t hash, Arena &index) {
    if (building_) return BookStatus::Unsupported;
    if (std::strlen(name) >= sizeof(name_)) return BookStatus::Unsupported;
    std::strcpy(name_, name);
    storage_ = &storage; index_ = &index; hash_ = hash;
    cached_ = reader_.open(storage, name_, hash, index) == BookStatus::Ok;
    return cached_ ? BookStatus::Ok : BookStatus::Stale;
  }
  BookStatus begin(BookSource &source, const ZipCatalog &zip, const ZipEntry &entry,
                   const char *href, const LayoutParams &params, Arena &layout, Arena &parse) {
    writer_ = PageCacheWriter{};
    failure_ = "cache begin";
    if (!writer_.begin(*storage_, name_, hash_, *index_)) return BookStatus::IoError;
    building_ = true;
    failure_ = "chapter stream open";
    // The SDK LayoutEngine keeps a reference. The caller's stack parameters
    // may disappear before background steps, so retain our own stable copy.
    params_ = params;
    auto status = session_.begin(source, &zip, source, entry, href, params_, layout, writer_, &parse);
    if (status != BookStatus::Ok) stop(false);
    return status;
  }
  BookStatus step(uint32_t pages = 1) {
    if (!building_) return BookStatus::Ok;
    failure_ = "chapter stream step";
    auto status = session_.step(pages);
    if (status != BookStatus::Ok || writer_.failed()) {
      if (writer_.failed()) failure_ = "page cache write";
      stop(false);
      return status == BookStatus::Ok ? BookStatus::IoError : status;
    }
    if (session_.done()) {
      writer_.setTotalChars(session_.totalChars());
      failure_ = "cache finish";
      if (!writer_.finish()) { stop(false); return BookStatus::IoError; }
      session_.abort(); building_ = false;
      index_->reset();
      cached_ = reader_.open(*storage_, name_, hash_, *index_) == BookStatus::Ok;
      failure_ = "cache reopen";
      return cached_ ? BookStatus::Ok : BookStatus::IoError;
    }
    return BookStatus::Ok;
  }
  bool stop(bool preserve = true) {
    if (!building_) return true;
    bool ok = true;
    if (preserve && writer_.pageCount() && !writer_.failed()) {
      writer_.setTotalChars(session_.totalChars());
      ok = writer_.suspend(uint32_t(session_.bytesConsumed()), uint32_t(session_.bytesTotal()));
    } else {
      // Remove only the uncommitted temporary file; an older partial stays usable.
      storage_->discardWrite();
    }
    session_.abort(); building_ = false;
    return ok;
  }
  bool building() const { return building_; }
  const char *failureStage() const { return failure_; }
  bool isPartial() const { return building_ || (cached_ && reader_.isPartial()); }
  uint32_t pageCount() const {
    return std::max(cached_ ? reader_.pageCount() : 0u, building_ ? writer_.pageCount() : 0u);
  }
  uint32_t charStart(uint32_t page) const {
    return building_ && page < writer_.pageCount() ? writer_.charStart(page) : reader_.charStart(page);
  }
  uint32_t pageForChar(uint32_t anchor) const {
    return building_ && writer_.pageCount() >= (cached_ ? reader_.pageCount() : 0u)
             ? writer_.pageForChar(anchor) : reader_.pageForChar(anchor);
  }
  bool canSeek(uint32_t anchor, bool last = false) const {
    if (!pageCount()) return false;
    if (!building_) return true;
    if (last) return false;
    if (!anchor || (cached_ && anchor < reader_.totalChars())) return true;
    return writer_.pageCount() && writer_.charStart(writer_.pageCount() - 1) > anchor;
  }
  uint32_t totalChars() const { return building_ ? 0 : reader_.totalChars(); }
  uint32_t progress() const {
    return building_ && session_.bytesTotal()
             ? uint32_t(std::min<uint64_t>(99, session_.bytesConsumed() * 100 / session_.bytesTotal())) : 100;
  }
  bool charForAnchor(uint32_t id, uint32_t *out) const {
    return (building_ && writer_.charForAnchor(id, out)) || (cached_ && reader_.charForAnchor(id, out));
  }
  BookStatus readPage(uint32_t page, Arena &scratch, Page *out) {
    if (building_ && page < writer_.pageCount()) return writer_.readPage(page, scratch, out);
    return cached_ ? reader_.readPage(page, scratch, out) : BookStatus::IoError;
  }
private:
  ChapterLayoutSession session_;
  PageCacheWriter writer_;
  PageCacheReader reader_;
  LayoutParams params_{};
  IncrementalCacheStorage *storage_ = nullptr;
  Arena *index_ = nullptr;
  char name_[64] = {};
  uint32_t hash_ = 0;
  bool cached_ = false, building_ = false;
  const char *failure_ = "none";
};
} // namespace fm
