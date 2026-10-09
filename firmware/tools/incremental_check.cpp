#include "ReaderGeometry.h"
#include "IncrementalChapter.h"
#include "LayoutMetricsCache.h"
#include "FontReadCache.h"
#include <chrono>
#include <FreeInkBook.h>
#include <FtFont.h>
#include <cache/PageCache.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <render/ImageRenderer.h>
#include <render/PageRenderer.h>
#include <stdexcept>
#include <vector>
using namespace freeink::book;
namespace fs = std::filesystem;
void check(bool ok, const char *why) {
  if (!ok)
    throw std::runtime_error(why);
}
class Source : public BookSource {
public:
  mutable std::ifstream f;
  uint64_t length;
  explicit Source(const char *p) : f(p, std::ios::binary) {
    check(bool(f), "source open");
    f.seekg(0, std::ios::end);
    length = f.tellg();
  }
  int32_t readAt(uint64_t at, void *dst, uint32_t n) override {
    f.clear();
    f.seekg(at);
    f.read(static_cast<char *>(dst), n);
    return f.gcount();
  }
  uint64_t size() const override { return length; }
};
unsigned long rawRead(void *p, unsigned long at, unsigned char *out,
                       unsigned long n) {
  return static_cast<Source *>(p)->readAt(at, out, n);
}
fm::FontReadCache blockCache;
bool cachedReads = false;
unsigned long fontRead(void *p, unsigned long at, unsigned char *out, unsigned long n) {
  return cachedReads ? blockCache.read(p, at, out, n) : rawRead(p, at, out, n);
}
class Cache : public fm::IncrementalCacheStorage {
public:
  fs::path root;
  std::ofstream out;
  fs::path dest, tmp;
  explicit Cache(const char *p) : root(p) { fs::create_directories(root); }
  bool exists(const char *n) override { return fs::exists(root / n); }
  bool remove(const char *n) override {
    return !exists(n) || fs::remove(root / n);
  }
  int64_t fileSize(const char *n) override {
    return exists(n) ? int64_t(fs::file_size(root / n)) : -1;
  }
  int32_t readAt(const char *n, uint32_t at, void *dst, uint32_t len) override {
    std::ifstream f(root / n, std::ios::binary);
    if (!f)
      return -1;
    f.seekg(at);
    f.read(static_cast<char *>(dst), len);
    return f.gcount();
  }
  bool beginWrite(const char *n) override {
    dest = root / n;
    tmp = dest.string() + ".tmp";
    out.open(tmp, std::ios::binary | std::ios::trunc);
    return bool(out);
  }
  bool write(const void *p, uint32_t n) override {
    out.write(static_cast<const char *>(p), n);
    return bool(out);
  }
  void discardWrite() override { out.close(); if (fs::exists(tmp)) fs::remove(tmp); }
  int32_t readBackAt(uint32_t at, void *dst, uint32_t len) override {
    out.flush(); std::ifstream f(tmp, std::ios::binary); f.seekg(at); f.read(static_cast<char *>(dst),len); return f.gcount();
  }
  bool endWrite() override {
    out.close();
    if (fs::exists(dest))
      fs::remove(dest);
    fs::rename(tmp, dest);
    return true;
  }
};
struct Memory {
  std::vector<uint8_t> bytes;
  Arena a;
  explicit Memory(size_t n) : bytes(n), a(bytes.data(), bytes.size()) {};
};
struct ZoomCheck {
  unsigned rows = 0;
};
bool imageRow(void *p, uint16_t, const uint8_t *, uint16_t width) {
  check(width > 0, "image width");
  ++static_cast<ZoomCheck *>(p)->rows;
  return true;
}

std::string bytes(const fs::path &p) { std::ifstream f(p,std::ios::binary); return {std::istreambuf_iterator<char>(f),{}}; }
int main(int argc,char **argv) {
 try {
  check(argc==5,"args"); Source source(argv[1]),sc(argv[2]),tc(argv[3]); Cache cache(argv[4]);
  std::vector<fm::FontReadCache::Entry> blocks(256);
  blockCache.begin(rawRead, blocks.data(), blocks.size());
  unsigned char expected[2048], actual[2048];
  for(auto *file : {&sc,&tc}) for(unsigned long at : {0UL, 1UL, 511UL, 512UL, 12345UL, (unsigned long)file->size()-37, (unsigned long)file->size()}) {
    for(unsigned long len : {0UL,1UL,19UL,511UL,512UL,2048UL}) {
      auto n=rawRead(file,at,expected,len), m=blockCache.read(file,at,actual,len);
      check(n==m && std::memcmp(expected,actual,n)==0,"font stream boundary/EOF parity");
    }
  }
  blockCache.begin(rawRead,nullptr,256);
  check(blockCache.read(&sc,0,actual,19)==rawRead(&sc,0,expected,19) && std::memcmp(actual,expected,19)==0,"allocation fallback");
  blockCache.begin(rawRead,blocks.data(),blocks.size());
  freeink::font::FtFont faces[4]; FontChain fonts;
  Source *files[]={&sc,&tc,&sc,&sc};
  for(int i=0;i<4;++i) {
   faces[i].setGsubByteBudget(65536); faces[i].setGposByteBudget(65536);
   check(faces[i].initStream(fontRead,files[i],files[i]->size(),26,i==2?700:400,i==3),"font init");
   fonts.add(&faces[i],i==2?1:i==3?2:0);
  }
  std::vector<fm::LayoutMetricsCache::Entry> entries(8192);
  fm::LayoutMetricsCache memo; memo.begin(fonts,entries.data(),entries.size());
  Memory bm(768*1024),layout(512*1024),parse(256*1024),index(256*1024),page(256*1024);
  Book book; check(book.open(source,bm.a,parse.a)==BookStatus::Ok,"book");
  uint32_t total=0; size_t longest=0; uint32_t longestBytes=0;
  for(size_t c=0;c<book.spineCount();++c) {
   auto *entry=book.zip().find(book.spineItem(c)->href); check(entry,"entry");
   if(entry->uncompressedSize>longestBytes){longest=c;longestBytes=entry->uncompressedSize;}
   for(int size:{26,34}) {
    LayoutParams p; p.pageWidth=480;p.pageHeight=800;p.marginLeft=p.marginRight=24;p.marginTop=24;p.marginBottom=66;p.baseSizePx=size;p.lineSpacingPct=125;p.language=book.metadata().language;
    cachedReads=false; p.font=&fonts; layout.a.reset();parse.a.reset();index.a.reset();
    PageCacheWriter baseline; check(baseline.begin(cache,"baseline",123,index.a),"baseline start");
    uint32_t chars=0; check(ChapterLayout::layout(source,book.zip(),*entry,book.spineItem(c)->href,p,layout.a,baseline,nullptr,&chars,&parse.a)==BookStatus::Ok,"baseline layout");
    baseline.setTotalChars(chars);check(baseline.finish(),"baseline finish");
    layout.a.reset();parse.a.reset();index.a.reset();cache.remove("incremental");
    fm::IncrementalChapter inc; check(inc.open(cache,"incremental",123,index.a)!=BookStatus::Ok,"cold cache");
    cachedReads=true; p.font=&memo; check(inc.begin(source,book.zip(),*entry,book.spineItem(c)->href,p,layout.a,parse.a)==BookStatus::Ok,"incremental start");
    // Mimic loadChapter returning and its stack frame being overwritten.
    p.font=nullptr; p.baseSizePx=99; p.lineSpacingPct=250;
    bool live=false;
    while(inc.building()) {
     check(inc.step()==BookStatus::Ok,"step");
     if(inc.pageCount()) {page.a.reset(); Page pg{};check(inc.readPage(inc.pageCount()-1,page.a,&pg)==BookStatus::Ok,"live page");live=true;}
    }
    check(live,"first page");check(bytes(cache.root/"baseline")==bytes(cache.root/"incremental"),"exact cache parity");total+=inc.pageCount();
    std::cout<<"PARITY spine="<<c<<" size="<<size<<" pages="<<inc.pageCount()<<"\n";
   }
  }
  auto *entry=book.zip().find(book.spineItem(longest)->href); LayoutParams p;p.pageWidth=480;p.pageHeight=800;p.baseSizePx=26;p.font=&memo;
  layout.a.reset();parse.a.reset();index.a.reset();cache.remove("resume");fm::IncrementalChapter inc;
  inc.open(cache,"resume",789,index.a);check(inc.begin(source,book.zip(),*entry,book.spineItem(longest)->href,p,layout.a,parse.a)==BookStatus::Ok,"resume begin");
  check(inc.step()==BookStatus::Ok && inc.building() && inc.pageCount()>0,"early pages before chapter end");
  uint32_t prefix=inc.pageCount();page.a.reset();Page pg{};check(inc.readPage(prefix-1,page.a,&pg)==BookStatus::Ok,"prefix read");uint32_t anchor=pg.charStart;
  check(inc.stop(),"suspend");layout.a.reset();parse.a.reset();index.a.reset();
  check(inc.open(cache,"resume",789,index.a)==BookStatus::Ok && inc.isPartial() && inc.pageCount()==prefix,"partial reopen");
  check(inc.begin(source,book.zip(),*entry,book.spineItem(longest)->href,p,layout.a,parse.a)==BookStatus::Ok,"partial rebuild");
  check(inc.canSeek(anchor),"resume immediately from cached prefix");page.a.reset();check(inc.readPage(prefix-1,page.a,&pg)==BookStatus::Ok && pg.charStart==anchor,"partial served while writer empty");
  check(inc.stop(),"abort zero-page rebuild");check(cache.exists("resume") && !fs::exists(cache.tmp),"zero-page abort preserves prefix");
  layout.a.reset();parse.a.reset();index.a.reset();inc.open(cache,"resume",789,index.a);inc.begin(source,book.zip(),*entry,book.spineItem(longest)->href,p,layout.a,parse.a);
  while(inc.building())check(inc.step()==BookStatus::Ok,"resume complete");check(!inc.isPartial() && inc.pageCount()>prefix,"final cache");
  check(inc.pageForChar(anchor)==prefix-1,"resume anchor stable");
  std::cout<<"PASS exact parity pages="<<total<<" partial resume/cancel/anchor PASS metrics hits="<<memo.hits<<" misses="<<memo.misses<<"\n";
 } catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
}
