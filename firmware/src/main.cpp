#include "ReaderGeometry.h"
#include "Storage.h"
#include "splash.h"
#include "ui_font.h"
#include <Arduino.h>
#include <BleKeyboardHost.h>
#include <BoardConfig.h>
#include <EInkDisplay.h>
#include <FreeInkBook.h>
#include <FtFont.h>
#include <InputManager.h>
#include <PowerManager.h>
#include <algorithm>
#include <cache/PageCache.h>
#include <epub/ImageProbe.h>
#include <esp_heap_caps.h>
#include <render/ImageRenderer.h>
#include <render/PageRenderer.h>
#include <vector>
using namespace freeink::book;
namespace {
constexpr int W = 480, H = 800, TOP = 100, ROW = 64, VISIBLE = 9;
EInkDisplay display(13, 14, 15, 16, 17, 18);
InputManager input;
Preferences prefs;
uint8_t frame[48000];
FrameTarget target{frame,
                   800,
                   480,
                   100,
                   FrameFormat::Mono1Dithered,
#if FM_SPLASH_ROTATE_CW
                   FrameRotation::Portrait
#else
                   FrameRotation::PortraitInverted
#endif
};
Arena bookArena, layoutArena, parseArena, indexArena, pageArena, imageArena;
Book book;
SdSource source;
SdCache cache;
PageCacheReader reader;
FsFile fontFiles[2];
freeink::font::FtFont faces[4], uiFace;
FontChain fonts, uiFonts;
std::vector<String> books;
enum class View {
  Shelf,
  Reading,
  Menu,
  Contents,
  Settings,
  Bluetooth,
  Image,
  Error
};
View view = View::Shelf, errorReturn = View::Shelf;
String message, currentPath, key, title;
uint32_t identity = 0, chapter = 0, pageNumber = 0, character = 0,
         lastActivity = 0, lastBlePaint = 0;
int selected = 0, offset = 0, imageIndex = 0, zoom = 1, panX = 0, panY = 0;
uint16_t fontSize = 26, lineSpacing = 125, margin = 24, sleepMinutes = 10;
bool useTraditional = false, bleEnabled = true, loaded = false,
     positionReady = false, fontReady = false;
bool dirty = true, forceFull = true, sdReady = false, memoryReady = false;
unsigned refreshes = 0;
struct ImagePlacement {
  String href;
  int x, y, w, h;
};
std::vector<ImagePlacement> images;

void pixel(int x, int y, bool black = true) {
  if (x < 0 || x >= W || y < 0 || y >= H)
    return;
  auto location = fm::address(x, y, FM_SPLASH_ROTATE_CW);
  uint32_t pos = location.byte;
  uint8_t mask = location.mask;
  if (black)
    frame[pos] &= ~mask;
  else
    frame[pos] |= mask;
}
void line(int y) {
  for (int x = 20; x < W - 20; ++x)
    pixel(x, y);
}
void text(const String &s, int x, int baseline, int size = 24,
          bool bold = false) {
  PageTextRun run{s.c_str(),      uint16_t(std::min<size_t>(s.length(), 65535)),
                  int16_t(x),     int16_t(baseline),
                  uint16_t(size), uint8_t(bold ? 1 : 0)};
  Page p{};
  p.runs = &run;
  p.runCount = 1;
  PageRenderer::renderText(p, fontReady ? fonts : uiFonts, target);
}
String fit(const String &s, int width, int size = 24) {
  FontChain &f = fontReady ? fonts : uiFonts;
  size_t i = 0;
  int used = 0;
  while (i < s.length()) {
    size_t start = i;
    uint8_t c = s[i++];
    uint32_t cp = c;
    int extra =
        c < 128 ? 0 : ((c & 0xe0) == 0xc0 ? 1 : ((c & 0xf0) == 0xe0 ? 2 : 3));
    if (extra) {
      cp = c & ((1 << (6 - extra)) - 1);
      while (extra-- && i < s.length())
        cp = (cp << 6) | (s[i++] & 63);
    }
    used += f.advance(cp, size, 0);
    if (used > width - 3 * f.advance('.', size, 0))
      return s.substring(0, start) + "...";
  }
  return s;
}
void header(const String &s) {
  text(fit(s, 440, 30), 20, 48, 30, true);
  line(72);
}
void footer(const String &s) {
  line(750);
  text(fit(s, 440, 18), 20, 780, 18);
}
void row(int i, const String &s, bool highlight = false) {
  int y = TOP + i * ROW;
  text((highlight ? "> " : "  ") + fit(s, 408), 20, y + 36);
  if (highlight)
    for (int x = 12; x < 468; ++x) {
      pixel(x, y);
      pixel(x, y + ROW - 6);
    }
}
bool allocate(Arena &a, size_t n) {
  void *p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p)
    return false;
  a.init(p, n);
  return true;
}
void error(const String &s, View back = View::Shelf) {
  Serial.println(s);
  message = s;
  errorReturn = back;
  view = View::Error;
  dirty = forceFull = true;
}
void enter(View v) {
  view = v;
  selected = offset = 0;
  dirty = forceFull = true;
}
unsigned long fontRead(void *ctx, unsigned long at, unsigned char *dst,
                       unsigned long n) {
  auto *f = static_cast<FsFile *>(ctx);
  if (!f->seekSet(at))
    return 0;
  int got = f->read(dst, n);
  return got < 0 ? 0 : got;
}
bool initFonts() {
  fonts = FontChain{};
  fontReady = false;
  for (auto &f : faces)
    f.deinit();
  for (int i = 0; i < 2; ++i) {
    faces[i].deinit();
    fontFiles[i].close();
    bool tc = (i == 0) ? useTraditional : !useTraditional;
    fontFiles[i] = SdMan.open(tc ? "/Fonts/NotoSansTC-VF.ttf"
                                 : "/Fonts/NotoSansSC-VF.ttf");
    faces[i].setGsubByteBudget(65536);
    faces[i].setGposByteBudget(65536);
    if (fontFiles[i] &&
        faces[i].initStream(fontRead, &fontFiles[i], fontFiles[i].fileSize(),
                            fontSize)) {
      fonts.add(&faces[i]);
      fontReady = true;
    }
  }
  if (fontFiles[0])
    for (int i = 2; i < 4; ++i) {
      faces[i].setGsubByteBudget(65536);
      faces[i].setGposByteBudget(65536);
      if (faces[i].initStream(fontRead, &fontFiles[0], fontFiles[0].fileSize(),
                              fontSize, i == 2 ? 700 : 400, i == 3))
        fonts.add(&faces[i], i == 2 ? 1 : 2);
    }
  fonts.add(&uiFace);
  return fontReady;
}
void saveSettings() {
  prefs.putUShort("size", fontSize);
  prefs.putUShort("spacing", lineSpacing);
  prefs.putUShort("margin", margin);
  prefs.putUShort("sleep", sleepMinutes);
  prefs.putBool("tc", useTraditional);
  prefs.putBool("ble", bleEnabled);
}
void savePosition() {
  if (!loaded || !positionReady || key.isEmpty())
    return;
  ReadingPosition p{1, identity, chapter, character};
  if (prefs.putBytes(key.c_str(), &p, sizeof(p)) != sizeof(p))
    Serial.println("[FM] Position save failed");
  prefs.putString("last", currentPath);
}
void scanDir(const String &path, int depth) {
  if (depth > 12 || books.size() >= 1000)
    return;
  auto dir = SdMan.open(path.c_str());
  if (!dir || !dir.isDirectory())
    return;
  FsFile f;
  char name[512];
  while (f.openNext(&dir, O_RDONLY)) {
    f.getName(name, sizeof(name));
    String n = name, full = path == "/" ? "/" + n : path + "/" + n;
    if (f.isDirectory()) {
      f.close();
      if (!n.startsWith(".") && full != "/Fonts" && full != "/FM")
        scanDir(full, depth + 1);
    } else {
      f.close();
      String lower = n;
      lower.toLowerCase();
      if (lower.endsWith(".epub"))
        books.push_back(full);
    }
    if (books.size() >= 1000)
      break;
    delay(1);
  }
}
void scanBooks() {
  books.clear();
  if (sdReady)
    scanDir("/", 0);
  std::sort(books.begin(), books.end(), [](const String &a, const String &b) {
    return a.compareTo(b) < 0;
  });
  selected = offset = 0;
  dirty = true;
}
LayoutParams params() {
  LayoutParams p;
  p.pageWidth = W;
  p.pageHeight = H;
  p.marginLeft = p.marginRight = margin;
  p.marginTop = 24;
  p.marginBottom = 66;
  p.baseSizePx = fontSize;
  p.font = &fonts;
  p.lineSpacingPct = lineSpacing;
  p.language = book.metadata().language;
  return p;
}
bool loadChapter(uint32_t ch, uint32_t anchor = 0, bool last = false) {
  if (ch >= book.spineCount())
    return false;
  auto *item = book.spineItem(ch);
  auto *entry = book.zip().find(item->href);
  if (!entry) {
    error("Chapter file missing", View::Menu);
    return false;
  }
  LayoutParams p = params();
  uint32_t hash = layoutGenerationHash(
      p, identity ^ (useTraditional ? 0x5443 : 0x5343) ^ 0x10002);
  char name[64];
  pageCacheName(ch, hash, name, sizeof(name));
  indexArena.reset();
  reader = PageCacheReader{};
  auto status = reader.open(cache, name, hash, indexArena);
  if (status != BookStatus::Ok || reader.isPartial()) {
    memset(frame, 255, sizeof(frame));
    header("正在分页 / Preparing");
    text("Please wait...", 20, 150);
    display.setFramebuffer(frame);
    display.displayBuffer(EInkDisplay::FULL_REFRESH);
    layoutArena.reset();
    parseArena.reset();
    indexArena.reset();
    PageCacheWriter writer;
    if (!writer.begin(cache, name, hash, indexArena)) {
      error("SD cache write failed", View::Menu);
      return false;
    }
    uint32_t total = 0;
    status = ChapterLayout::layout(source, book.zip(), *entry, item->href, p,
                                   layoutArena, writer, nullptr, &total,
                                   &parseArena);
    writer.setTotalChars(total);
    if (status != BookStatus::Ok || writer.failed() || !writer.finish()) {
      cache.writing.close();
      SdMan.remove(cache.tempPath.c_str());
      cache.remove(name);
      error(String("EPUB layout: ") + bookStatusName(status), View::Menu);
      return false;
    }
    indexArena.reset();
    reader = PageCacheReader{};
    status = reader.open(cache, name, hash, indexArena);
  }
  if (status != BookStatus::Ok || !reader.pageCount()) {
    error("Empty or invalid chapter", View::Menu);
    return false;
  }
  chapter = ch;
  pageNumber = last ? reader.pageCount() - 1 : reader.pageForChar(anchor);
  character = last ? reader.charStart(pageNumber)
                   : std::min(anchor, reader.totalChars());
  view = View::Reading;
  dirty = forceFull = true;
  positionReady = true;
  lastActivity = millis();
  savePosition();
  return true;
}
void openBook(const String &path) {
  savePosition();
  loaded = false;
  positionReady = false;
  images.clear();
  source.file.close();
  if (!memoryReady) {
    error("PSRAM unavailable. Check board config.");
    return;
  }
  if (!fontReady) {
    error("Copy sd-card/Fonts to microSD first.");
    return;
  }
  if (!source.open(path.c_str())) {
    error("Cannot open EPUB");
    return;
  }
  identity = source.fingerprint();
  if (!identity) {
    error("EPUB read failed");
    return;
  }
  book = Book{};
  bookArena.reset();
  parseArena.reset();
  auto status = book.open(source, bookArena, parseArena);
  if (status != BookStatus::Ok || !book.spineCount()) {
    error(String("EPUB: ") + bookStatusName(status));
    return;
  }
  currentPath = path;
  title = book.metadata().title;
  if (title.isEmpty())
    title = path;
  char id[16];
  snprintf(id, sizeof(id), "b%08lx",
           static_cast<unsigned long>(ZipCatalog::hashPath(path.c_str())));
  key = id;
  char dir[24];
  snprintf(dir, sizeof(dir), "/FM/%08lx", static_cast<unsigned long>(identity));
  cache.root = dir;
  if (!SdMan.ensureDirectoryExists(cache.root.c_str())) {
    error("Cannot create SD cache folder");
    return;
  }
  ReadingPosition pos;
  if (prefs.getBytesLength(key.c_str()) == sizeof(pos))
    prefs.getBytes(key.c_str(), &pos, sizeof(pos));
  if (pos.version != 1 || pos.fingerprint != identity ||
      pos.chapter >= book.spineCount())
    pos = ReadingPosition{};
  loaded = true;
  // Establish the new book's position before any fallible layout. Never
  // persist an old book's chapter under a newly selected book's NVS key.
  chapter = pos.chapter;
  character = pos.character;
  pageNumber = 0;
  loadChapter(pos.chapter, pos.character);
}
void turn(int d) {
  if (!loaded)
    return;
  if (d > 0) {
    if (pageNumber + 1 < reader.pageCount())
      ++pageNumber;
    else if (chapter + 1 < book.spineCount()) {
      loadChapter(chapter + 1);
      return;
    } else
      return;
  } else {
    if (pageNumber)
      --pageNumber;
    else if (chapter) {
      loadChapter(chapter - 1, 0, true);
      return;
    } else
      return;
  }
  character = reader.charStart(pageNumber);
  savePosition();
  dirty = true;
}
void sleepNow() {
  savePosition();
  saveSettings();
  if (!freeink::PowerManager::armPowerButtonWakeup()) {
    error("Wake pin unavailable", view);
    return;
  }
  if (!BleHid.end()) {
    message = "Bluetooth is stopping. Try sleep again.";
    dirty = true;
    return;
  }
  memset(frame, 255, sizeof(frame));
  header("Faiyuet Morgan");
  text("休眠 / Sleeping", 20, 180, 30);
  footer("Press AI / power button to wake");
  display.setFramebuffer(frame);
  display.displayBuffer(EInkDisplay::FULL_REFRESH);
  display.deepSleep();
  source.file.close();
  for (auto &f : faces)
    f.deinit();
  for (auto &f : fontFiles)
    f.close();
  SdMan.shutdown();
  freeink::PowerManager::waitForPowerButtonRelease();
  freeink::PowerManager::powerDownRailsForSleep();
  freeink::PowerManager::deepSleep();
}
String setting(int i) {
  switch (i) {
  case 0:
    return "字号 / Font size: " + String(fontSize);
  case 1:
    return "行距 / Line spacing: " + String(lineSpacing) + "%";
  case 2:
    return "边距 / Margin: " + String(margin);
  case 3:
    return useTraditional ? "字体: 繁體中文" : "字体: 简体中文";
  case 4:
    return "自动休眠 / Sleep: " + String(sleepMinutes) + " min";
  default:
    return "应用并返回 / Apply & Back";
  }
}
int count() {
  switch (view) {
  case View::Shelf:
    return books.size();
  case View::Menu:
    return 7;
  case View::Contents:
    return book.tocCount();
  case View::Settings:
    return 9;
  case View::Bluetooth:
    return 5 + BleHid.deviceCount() + BleHid.pairedCount();
  default:
    return 0;
  }
}
void moveSelection(int d) {
  int n = count();
  if (!n)
    return;
  selected = std::max(0, std::min(n - 1, selected + d));
  if (selected < offset)
    offset = selected;
  if (selected >= offset + VISIBLE)
    offset = selected - VISIBLE + 1;
  dirty = true;
}
void adjust(int i, int d) {
  switch (i) {
  case 0:
    fontSize = std::clamp(int(fontSize) + d * 2, 18, 42);
    break;
  case 1:
    lineSpacing = std::clamp(int(lineSpacing) + d * 10, 100, 180);
    break;
  case 2:
    margin = std::clamp(int(margin) + d * 4, 12, 48);
    break;
  case 3:
    useTraditional = !useTraditional;
    initFonts();
    break;
  case 4:
    sleepMinutes =
        sleepMinutes == 0
            ? 5
            : (sleepMinutes == 5 ? 10 : (sleepMinutes == 10 ? 30 : 0));
    break;
  default:
    if (loaded)
      loadChapter(chapter, character);
    else
      enter(View::Shelf);
    return;
  }
  saveSettings();
  dirty = true;
}
void activate(int i) {
  switch (view) {
  case View::Shelf:
    if (i >= 0 && i < int(books.size()))
      openBook(books[i]);
    break;
  case View::Menu:
    if (i == 0) {
      loadChapter(chapter, character);
    }
    if (i == 1)
      enter(View::Contents);
    if (i == 2)
      enter(View::Settings);
    if (i == 3)
      enter(View::Bluetooth);
    if (i == 4) {
      savePosition();
      enter(View::Shelf);
    }
    if (i == 5)
      sleepNow();
    if (i == 6) {
      bool found = false;
      for (size_t m = 0; m < book.manifestCount(); ++m) {
        auto *im = book.manifestItem(m);
        if (!im->isCoverImage)
          continue;
        imageArena.reset();
        ImageInfo info;
        auto *e = book.zip().find(im->href);
        if (e && probeImage(source, *e, imageArena, &info) == BookStatus::Ok &&
            info.width && info.height) {
          images.clear();
          images.push_back({im->href, 0, 0, info.width, info.height});
          imageIndex = 0;
          zoom = 1;
          panX = panY = 0;
          view = View::Image;
          dirty = true;
          found = true;
        }
        break;
      }
      if (!found)
        error("No supported cover image", View::Menu);
    }
    break;
  case View::Contents: {
    auto *toc = book.tocEntry(i);
    if (!toc)
      break;
    for (size_t ch = 0; ch < book.spineCount(); ++ch)
      if (strcmp(book.spineItem(ch)->href, toc->href) == 0) {
        if (loadChapter(ch)) {
          uint32_t a = 0;
          if (toc->fragment &&
              reader.charForAnchor(ZipCatalog::hashPath(toc->fragment), &a)) {
            pageNumber = reader.pageForChar(a);
            character = reader.charStart(pageNumber);
            savePosition();
          }
        }
        return;
      }
    error("TOC target not in reading order", View::Contents);
    break;
  }
  case View::Settings:
    if (i == 6)
      enter(View::Bluetooth);
    else if (i == 7) {
      scanBooks();
      enter(View::Shelf);
    } else if (i == 8)
      sleepNow();
    else
      adjust(i, 1);
    break;
  case View::Bluetooth:
    if (i == 0) {
      bleEnabled = !bleEnabled;
      saveSettings();
      if (bleEnabled)
        BleHid.begin("Faiyuet Morgan");
      else
        BleHid.end();
    } else if (i == 1) {
      if (!bleEnabled) {
        bleEnabled = true;
        saveSettings();
        BleHid.begin("Faiyuet Morgan");
      }
      BleHid.startScan(8000);
    } else if (i == 2)
      BleHid.disconnect();
    else if (i == 3) {
      if (loaded)
        enter(View::Menu);
      else
        enter(View::Shelf);
    } else if (i == 4) {
      for (int b = BleHid.pairedCount() - 1; b >= 0; --b) {
        String addr = BleHid.paired(b).addr;
        BleHid.forget(addr.c_str());
      }
    } else if (i < 5 + BleHid.deviceCount())
      BleHid.connect(BleHid.device(i - 5).addr);
    else {
      int b = i - 5 - BleHid.deviceCount();
      if (b < BleHid.pairedCount())
        BleHid.connect(BleHid.paired(b).addr);
    }
    dirty = true;
    break;
  case View::Error:
    enter(errorReturn);
    break;
  default:
    break;
  }
}
void back() {
  if (view == View::Reading)
    enter(View::Menu);
  else if (view == View::Image) {
    view = View::Reading;
    dirty = forceFull = true;
  } else if (view == View::Error)
    enter(errorReturn);
  else if (view == View::Settings && loaded)
    loadChapter(chapter, character);
  else if (view == View::Contents || (view == View::Bluetooth && loaded))
    enter(View::Menu);
  else
    enter(View::Shelf);
}
void button(uint8_t b) {
  lastActivity = millis();
  if (b == InputManager::BTN_POWER) {
    sleepNow();
    return;
  }
  if (b == InputManager::BTN_BACK) {
    back();
    return;
  }
  if (view == View::Reading) {
    if (b == InputManager::BTN_LEFT || b == InputManager::BTN_UP)
      turn(-1);
    if (b == InputManager::BTN_RIGHT || b == InputManager::BTN_DOWN)
      turn(1);
    if (b == InputManager::BTN_CONFIRM)
      enter(View::Menu);
    return;
  }
  if (view == View::Image) {
    if (b == InputManager::BTN_CONFIRM) {
      zoom = zoom == 1 ? 2 : (zoom == 2 ? 4 : 1);
      panX = panY = 0;
    }
    if (b == InputManager::BTN_UP)
      panY = std::max(0, panY - 100);
    if (b == InputManager::BTN_DOWN)
      panY += 100;
    if (b == InputManager::BTN_LEFT)
      panX = std::max(0, panX - 100);
    if (b == InputManager::BTN_RIGHT)
      panX += 100;
    dirty = true;
    return;
  }
  if (view == View::Settings && selected < 5 &&
      (b == InputManager::BTN_LEFT || b == InputManager::BTN_RIGHT)) {
    adjust(selected, b == InputManager::BTN_LEFT ? -1 : 1);
    return;
  }
  if (b == InputManager::BTN_UP || b == InputManager::BTN_LEFT)
    moveSelection(-1);
  if (b == InputManager::BTN_DOWN || b == InputManager::BTN_RIGHT)
    moveSelection(1);
  if (b == InputManager::BTN_CONFIRM)
    activate(selected);
}
void portrait(float nx, float ny, int &x, int &y) {
  fm::touchToPortrait(nx, ny, FM_SPLASH_ROTATE_CW, x, y);
}
void tap(int x, int y) {
  lastActivity = millis();
  if (y > 750) {
    back();
    return;
  }
  if (view == View::Reading) {
    for (size_t i = 0; i < images.size(); ++i) {
      const auto &im = images[i];
      if (x >= im.x && x < im.x + im.w && y >= im.y && y < im.y + im.h) {
        imageIndex = i;
        zoom = 1;
        panX = panY = 0;
        view = View::Image;
        dirty = true;
        return;
      }
    }
    if (x < 160)
      turn(-1);
    else if (x > 320)
      turn(1);
    else
      enter(View::Menu);
    return;
  }
  if (view == View::Image) {
    if (y < 100) {
      zoom = zoom == 1 ? 2 : (zoom == 2 ? 4 : 1);
      panX = panY = 0;
    } else {
      if (x < 160)
        panX = std::max(0, panX - 100);
      if (x > 320)
        panX += 100;
      if (y < 400)
        panY = std::max(0, panY - 100);
      else
        panY += 100;
    }
    dirty = true;
    return;
  }
  if (view == View::Error) {
    back();
    return;
  }
  if (view == View::Shelf && y < 72) {
    enter(View::Settings);
    return;
  }
  if (y >= TOP && y < TOP + VISIBLE * ROW) {
    int i = offset + (y - TOP) / ROW;
    if (i < count()) {
      selected = i;
      if (view == View::Settings && i < 5) {
        adjust(i, x < W / 2 ? -1 : 1);
        return;
      }
      activate(i);
    }
  }
}
struct ZoomRows {
  int drawW, drawH, decodeH, x, y;
};
bool zoomRow(void *ctx, uint16_t rowY, const uint8_t *gray, uint16_t width) {
  auto &z = *static_cast<ZoomRows *>(ctx);
  int y0 = int(uint32_t(rowY) * z.drawH / z.decodeH),
      y1 = int(uint32_t(rowY + 1) * z.drawH / z.decodeH);
  static const uint8_t bayer[4][4] = {
      {0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
  for (int y = std::max(100, z.y + y0); y < std::min(750, z.y + y1); ++y)
    for (int x = std::max(0, z.x); x < std::min(W, z.x + z.drawW); ++x) {
      int src =
          std::min(int(width) - 1, int(uint32_t(x - z.x) * width / z.drawW));
      pixel(x, y, gray[src] < (bayer[y & 3][x & 3] * 16 + 8));
    }
  return true;
}
void render() {
  memset(frame, 255, sizeof(frame));
  if (view == View::Reading) {
    pageArena.reset();
    imageArena.reset();
    Page p{};
    auto status = reader.readPage(pageNumber, pageArena, &p);
    if (status != BookStatus::Ok) {
      error(String("Page cache: ") + bookStatusName(status), View::Menu);
      return;
    }
    uint32_t missing = PageRenderer::renderText(p, fonts, target);
    status =
        PageRenderer::renderImages(p, source, book.zip(), imageArena, target);
    images.clear();
    for (int i = 0; i < p.imageCount; ++i) {
      const auto &im = p.images[i];
      images.push_back({im.href, im.x, im.y, im.width, im.height});
    }
    String progress = String("章 ") + String(chapter + 1) + "/" +
                      String(book.spineCount()) + " 页 " +
                      String(pageNumber + 1) + "/" + String(reader.pageCount());
    if (reader.totalChars())
      progress +=
          " " + String(int(100.0 * character / reader.totalChars())) + "%";
    if (missing)
      progress += " [missing glyph]";
    if (status != BookStatus::Ok)
      progress += " [image unsupported]";
    footer(progress);
  } else if (view == View::Image) {
    if (imageIndex >= int(images.size())) {
      view = View::Reading;
      dirty = true;
      return;
    }
    const auto &im = images[imageIndex];
    imageArena.reset();
    ImageInfo info;
    auto *entry = book.zip().find(im.href.c_str());
    if (!entry ||
        probeImage(source, *entry, imageArena, &info) != BookStatus::Ok ||
        !info.width || !info.height) {
      error("Cannot decode image", View::Reading);
      return;
    }
    float scale =
        std::min(float(W - 24) / info.width, float(H - 150) / info.height) *
        zoom;
    int w = std::max(1, int(info.width * scale)),
        h = std::max(1, int(info.height * scale));
    panX = std::clamp(panX, 0, std::max(0, w - (W - 24)));
    panY = std::clamp(panY, 0, std::max(0, h - (H - 150)));
    float decodeScale = std::min(1.0f, scale);
    PageImage img{im.href.c_str(), 0, 0,
                  uint16_t(std::max(1, int(info.width * decodeScale))),
                  uint16_t(std::max(1, int(info.height * decodeScale)))};
    ZoomRows rows{w, h, int(img.height), 12 - panX, 100 - panY};
    auto status = ImageRenderer::render(source, book.zip(), img, imageArena,
                                        zoomRow, &rows);
    for (int y = 0; y < 100; ++y)
      for (int x = 0; x < W; ++x)
        pixel(x, y, false);
    for (int y = 750; y < H; ++y)
      for (int x = 0; x < W; ++x)
        pixel(x, y, false);
    header("图片 / Image " + String(zoom) + "x");
    footer(status == BookStatus::Ok ? "Top: zoom | arrows: pan | bottom: back"
                                    : "Unsupported image | bottom: back");
  } else if (view == View::Error) {
    header("Notice");
    text(fit(message, 440, 20), 20, 160, 20);
    footer("Tap to return");
  } else {
    if (view == View::Shelf) {
      header("Faiyuet Morgan");
      if (books.empty()) {
        text(sdReady ? "No EPUB in microSD" : "microSD mount failed", 20, 150);
        text("Copy Books and Fonts to SD", 20, 200, 20);
      }
      for (int r = 0; r < VISIBLE && offset + r < int(books.size()); ++r) {
        String n = books[offset + r];
        row(r, n.substring(n.lastIndexOf('/') + 1), selected == offset + r);
      }
      footer(String(books.size()) + " books | top: settings | OK: open");
    }
    if (view == View::Menu) {
      header(title);
      const char *labels[] = {"继续阅读 / Continue", "目录 / Contents",
                              "排版 / Settings",     "蓝牙翻页器 / Bluetooth",
                              "书架 / Bookshelf",    "休眠 / Sleep",
                              "封面 / Cover"};
      for (int i = 0; i < 7; ++i)
        row(i, labels[i], selected == i);
      footer("Hold AI: sleep | bottom: bookshelf");
    }
    if (view == View::Contents) {
      header("目录 / Contents");
      for (int r = 0; r < VISIBLE && offset + r < int(book.tocCount()); ++r)
        row(r, book.tocEntry(offset + r)->title, selected == offset + r);
      if (!book.tocCount())
        text("No table of contents", 20, 160);
      footer("Bottom: menu");
    }
    if (view == View::Settings) {
      header("设置 / Settings");
      for (int i = 0; i < 9; ++i)
        row(i,
            i < 6 ? setting(i)
                  : (i == 6 ? "蓝牙 / Bluetooth"
                            : (i == 7 ? "重新扫描 / Rescan" : "休眠 / Sleep")),
            selected == i);
      footer("Row left: - | right: + | bottom: apply");
    }
    if (view == View::Bluetooth) {
      header(BleHid.isConnected()
                 ? String("Connected: ") + BleHid.connectedName()
                 : "蓝牙 / Bluetooth");
      for (int r = 0; r < VISIBLE && offset + r < count(); ++r) {
        int i = offset + r;
        String s;
        if (i == 0)
          s = bleEnabled ? "蓝牙 / Bluetooth: ON" : "蓝牙 / Bluetooth: OFF";
        else if (i == 1)
          s = BleHid.isScanning() ? "正在扫描 / Scanning..."
                                  : "搜索翻页器 / Scan";
        else if (i == 2)
          s = "断开连接 / Disconnect";
        else if (i == 3)
          s = "返回 / Back";
        else if (i == 4)
          s = "清除配对 / Forget pairings";
        else if (i < 5 + BleHid.deviceCount())
          s = String("Pair: ") + BleHid.device(i - 5).name;
        else
          s = String("Saved: ") +
              BleHid.paired(i - 5 - BleHid.deviceCount()).name;
        row(r, s, selected == i);
      }
      footer(message.isEmpty() ? "Left / right: previous / next" : message);
    }
  }
  display.setFramebuffer(frame);
  display.displayBuffer(forceFull || ++refreshes % 8 == 0
                            ? EInkDisplay::FULL_REFRESH
                            : EInkDisplay::FAST_REFRESH);
  dirty = false;
  forceFull = false;
}
} // namespace
void setup() {
  BoardConfig::holdPowerRails();
  Serial.begin(115200);
  display.begin();
  memset(frame, 255, sizeof(frame));
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x)
      if (!(fmSplash[y * 60 + x / 8] & (0x80 >> (x & 7))))
        pixel(x, y);
  display.setFramebuffer(frame);
  display.displayBuffer(EInkDisplay::FULL_REFRESH);
  prefs.begin("faiyuet", false);
  fontSize = std::clamp(int(prefs.getUShort("size", 26)), 18, 42);
  lineSpacing = std::clamp(int(prefs.getUShort("spacing", 125)), 100, 180);
  margin = std::clamp(int(prefs.getUShort("margin", 24)), 12, 48);
  sleepMinutes = prefs.getUShort("sleep", 10);
  useTraditional = prefs.getBool("tc", false);
  bleEnabled = prefs.getBool("ble", true);
  uiFace.init(fmUiFont, sizeof(fmUiFont), 24);
  uiFonts.add(&uiFace);
  memoryReady =
      allocate(bookArena, 768 * 1024) && allocate(layoutArena, 512 * 1024) &&
      allocate(parseArena, 256 * 1024) && allocate(indexArena, 256 * 1024) &&
      allocate(pageArena, 256 * 1024) && allocate(imageArena, 512 * 1024);
  sdReady = SdMan.begin();
  if (sdReady) {
    initFonts();
    scanBooks();
  }
  input.begin();
  input.beginAsync();
  if (bleEnabled)
    BleHid.begin("Faiyuet Morgan");
  if (!memoryReady)
    error("PSRAM unavailable. Check board config.");
  else if (!sdReady)
    error("microSD mount failed. Insert FAT32 card.");
  else if (!fontReady)
    error("Copy sd-card/Fonts to microSD first.");
  else {
    String last = prefs.getString("last", "");
    if (!last.isEmpty() && SdMan.exists(last.c_str()))
      openBook(last.c_str());
  }
  lastActivity = millis();
}
void loop() {
  if (BleHid.isStopping() && BleHid.end(0) && bleEnabled)
    BleHid.begin("Faiyuet Morgan");
  BleHid.poll();
  uint8_t b;
  int handled = 0;
  while (handled++ < 4 && input.popPress(b))
    button(b);
  float nx, ny, sx, sy, ex, ey;
  int x, y;
  while (input.popTouchTap(nx, ny)) {
    portrait(nx, ny, x, y);
    tap(x, y);
  }
  while (input.popSwipe(sx, sy, ex, ey)) {
    int x0, y0, x1, y1;
    portrait(sx, sy, x0, y0);
    portrait(ex, ey, x1, y1);
    lastActivity = millis();
    if (view == View::Image) {
      panX = std::max(0, panX + x0 - x1);
      panY = std::max(0, panY + y0 - y1);
      dirty = true;
    } else if (view == View::Reading)
      turn(x1 < x0 ? 1 : -1);
    else
      moveSelection(y1 < y0 ? VISIBLE : -VISIBLE);
  }
  freeink::KeyEvent ev;
  while (BleHid.popKey(ev)) {
    using K = freeink::SpecialKey;
    if (ev.special == K::Left || ev.special == K::PageUp)
      button(InputManager::BTN_LEFT);
    else if (ev.special == K::Right || ev.special == K::PageDown)
      button(InputManager::BTN_RIGHT);
    else if (ev.special == K::Up)
      button(InputManager::BTN_UP);
    else if (ev.special == K::Down)
      button(InputManager::BTN_DOWN);
    else if (ev.special == K::Enter)
      button(InputManager::BTN_CONFIRM);
    else if (ev.special == K::Escape)
      button(InputManager::BTN_BACK);
  }
  char reason[64];
  if (BleHid.takeConnectFailure(reason, sizeof(reason))) {
    message = reason;
    dirty = true;
  }
  uint32_t passkey;
  if (BleHid.takePairingPasskey(passkey)) {
    message = "Passkey: " + String(passkey);
    dirty = true;
  }
  if (view == View::Bluetooth && millis() - lastBlePaint > 500) {
    lastBlePaint = millis();
    static uint32_t state = 0xffffffff;
    uint32_t next = BleHid.deviceCount() |
                    (uint32_t(BleHid.pairedCount()) << 8) |
                    (uint32_t(BleHid.isScanning()) << 16) |
                    (uint32_t(BleHid.isConnected()) << 17) |
                    (uint32_t(BleHid.isConnecting()) << 18);
    if (next != state) {
      state = next;
      selected = std::min(selected, std::max(0, count() - 1));
      dirty = true;
    }
  }
  if (dirty)
    render();
  if (sleepMinutes &&
      millis() - lastActivity > uint32_t(sleepMinutes) * 60000u) {
    sleepNow();
    lastActivity = millis();
  }
  delay(15);
}
