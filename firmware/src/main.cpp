#include "ReaderGeometry.h"
#include "LayoutMetricsCache.h"
#include "FontReadCache.h"
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
// EPUB parsing, font rasterization and FAT scans can nest on Arduino's task.
SET_LOOP_TASK_STACK_SIZE(16384);
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
fm::IncrementalChapter reader;
FsFile fontFiles[2];
freeink::font::FtFont faces[4], uiFace;
FontChain fonts, uiFonts;
fm::LayoutMetricsCache layoutMetrics;
fm::LayoutMetricsCache::Entry *metricsEntries = nullptr;
fm::FontReadCache fontReadCache;
fm::FontReadCache::Entry *fontReadEntries = nullptr;
uint32_t lastBuildStep = 0;
std::vector<String> books;
enum class View {
  Shelf,
  Reading,
  Menu,
  More,
  Adjustment,
  Contents,
  Settings,
  SystemSettings,
  Bluetooth,
  Image,
  ImageMenu,
  Error
};
View view = View::Shelf, errorReturn = View::Shelf;
View settingsReturn = View::Shelf;
String message, currentPath, key, title;
uint32_t identity = 0, chapter = 0, pageNumber = 0, character = 0,
         lastActivity = 0, lastBlePaint = 0;
int selected = 0, offset = 0, imageIndex = 0, zoom = 1, panX = 0, panY = 0;
int touchSelected = -1;
View touchSelectionView = View::Shelf;
int editingSetting = 0;
uint16_t fontSize = 26, lineSpacing = 125, margin = 24, sleepMinutes = 10;
enum class UiLanguage : uint8_t { Simplified, Traditional, English };
UiLanguage uiLanguage = UiLanguage::Simplified;
bool useTraditional = false, bleEnabled = true, loaded = false,
     positionReady = false, fontReady = false;
bool dirty = true, forceFull = true, sdReady = false, memoryReady = false;
unsigned refreshes = 0;
struct ImagePlacement {
  String href;
  int x, y, w, h;
};
std::vector<ImagePlacement> images;
String ui(const char *zh);

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
void text(const String &s, int x, int baseline, int size = 24,
          bool bold = false) {
  PageTextRun run{s.c_str(),      uint16_t(std::min<size_t>(s.length(), 65535)),
                  int16_t(x),     int16_t(baseline),
                  uint16_t(size), uint8_t(bold ? 1 : 0)};
  Page p{};
  p.runs = &run;
  p.runCount = 1;
  PageRenderer::renderText(p, uiFonts, target);
}
String fit(const String &s, int width, int size = 24) {
  FontChain &f = uiFonts;
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
  text(fit(ui(s.c_str()), 440, 30), 20, 48, 30, true);
}
void footer(const String &s) {
  text(fit(ui(s.c_str()), 440, 18), 20, 780, 18);
}
void invertRect(int x, int y, int w, int h) {
  for (int yy = std::max(0, y); yy < std::min(H, y + h); ++yy)
    for (int xx = std::max(0, x); xx < std::min(W, x + w); ++xx) {
      auto a = fm::address(xx, yy, FM_SPLASH_ROTATE_CW);
      frame[a.byte] ^= a.mask;
    }
}
void row(int i, const String &s, bool highlight = false) {
  int y = TOP + i * ROW;
  text(fit(ui(s.c_str()), 408), 28, y + 36);
  if (highlight) invertRect(16, y + 3, 448, ROW - 10);
}
bool allocate(Arena &a, size_t n) {
  void *p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p)
    return false;
  a.init(p, n);
  return true;
}
void error(const String &s, View back = View::Shelf) {
  BoardConfig::serialTransport().println(s);
  message = s;
  errorReturn = back;
  view = View::Error;
  dirty = forceFull = true;
}
void enter(View v) {
  touchSelected = -1;
  if (v == View::Shelf || v == View::Settings)
    reader.stop();
  view = v;
  selected = offset = 0;
  dirty = forceFull = true;
}
unsigned long fontReadUncached(void *ctx, unsigned long at, unsigned char *dst,
                       unsigned long n) {
  auto *f = static_cast<FsFile *>(ctx);
  if (!f->seekSet(at))
    return 0;
  int got = f->read(dst, n);
  return got < 0 ? 0 : got;
}
unsigned long fontRead(void *ctx, unsigned long at, unsigned char *dst, unsigned long n) {
  return fontReadCache.read(ctx, at, dst, n);
}
bool initFonts() {
  reader.stop();
  layoutMetrics.clear();
  fontReadCache.clear();
  fonts = FontChain{};
  uiFonts = FontChain{};
  uiFonts.add(&uiFace);
  fontReady = false;
  for (auto &f : faces)
    f.deinit();
  for (int i = 0; i < 2; ++i) {
    faces[i].deinit();
    fontFiles[i].close();
    bool tc = (i == 0) ? useTraditional : !useTraditional;
    fontFiles[i] = SdMan.open(tc ? "/Fonts/NotoSansTC-VF.ttf"
                                 : "/Fonts/NotoSansSC-VF.ttf");
    // SdFat remembers verified contiguity, making streamed font seeks O(1).
    // Fragmented files keep the regular FAT-chain path without changing data.
    if (fontFiles[i])
      fontFiles[i].contiguousRange(nullptr, nullptr);
    faces[i].setGsubByteBudget(65536);
    faces[i].setGposByteBudget(65536);
    if (fontFiles[i] &&
        faces[i].initStream(fontRead, &fontFiles[i], fontFiles[i].fileSize(),
                            fontSize)) {
      fonts.add(&faces[i]);
      uiFonts.add(&faces[i]);
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
struct BookSettings {
  uint16_t version = 1;
  uint16_t fontSize = 26;
  uint16_t lineSpacing = 125;
  uint16_t margin = 24;
  uint8_t traditional = 0;
};
String ui(const char *zh) {
  struct Translation { const char *zh; const char *tw; const char *en; };
  static const Translation table[] = {
      {"书架", "書架", "Bookshelf"}, {"设置", "設定", "Settings"},
      {"系统设置", "系統設定", "System settings"}, {"阅读设置", "閱讀設定", "Reading settings"},
      {"阅读菜单", "閱讀選單", "Reader menu"}, {"继续阅读", "繼續閱讀", "Continue reading"},
      {"目录", "目錄", "Contents"}, {"返回书架", "返回書架", "Back to bookshelf"},
      {"更多", "更多", "More"}, {"蓝牙翻页器", "藍牙翻頁器", "Bluetooth page turner"},
      {"查看封面", "查看封面", "View cover"}, {"休眠", "休眠", "Sleep"},
      {"完成并返回", "完成並返回", "Done"}, {"完成", "完成", "Done"},
      {"提示", "提示", "Notice"}, {"返回", "返回", "Back"},
      {"返回阅读", "返回閱讀", "Back to reading"},
      {"返回阅读菜单", "返回閱讀選單", "Back to reader menu"},
      {"返回更多", "返回更多", "Back to more"},
      {"长按 AI 取消等待", "長按 AI 取消等待", "Hold AI to cancel"},
      {"返回系统设置", "返回系統設定", "Back to system settings"},
      {"返回阅读设置", "返回閱讀設定", "Back to reading settings"},
      {"保存并返回阅读", "儲存並返回閱讀", "Save and return to book"},
      {"图片操作", "圖片操作", "Image controls"},
      {"放大", "放大", "Zoom in"}, {"缩小", "縮小", "Zoom out"},
      {"返回图片", "返回圖片", "Back to image"},
      {"点中间或按 AI 打开图片菜单", "點中間或按 AI 開啟圖片選單", "Tap center / AI: image menu"},
      {"按 AI 返回", "按 AI 返回", "Press AI to return"},
      {"语言", "語言", "Language"}, {"简体中文", "簡體中文", "Simplified Chinese"},
      {"繁体中文", "繁體中文", "Traditional Chinese"}, {"English", "English", "English"},
      {"自动休眠", "自動休眠", "Auto sleep"}, {"关闭", "關閉", "Off"},
      {"字号", "字號", "Font size"}, {"行距", "行距", "Line spacing"},
      {"边距", "邊距", "Margins"}, {"字体", "字體", "Font"},
      {"分钟", "分鐘", "min"}, {"调整设置", "調整設定", "Adjust setting"},
      {"5 分钟", "5 分鐘", "5 min"}, {"10 分钟", "10 分鐘", "10 min"},
      {"30 分钟", "30 分鐘", "30 min"},
      {"减小", "減小", "Decrease"}, {"增大", "增大", "Increase"},
      {"上一个时长", "上一個時長", "Previous interval"}, {"下一个时长", "下一個時長", "Next interval"},
      {"按 AI 键唤醒", "按 AI 鍵喚醒", "Press AI to wake"}, {"已休眠", "已休眠", "Sleeping"},
      {"还没有电子书", "還沒有電子書", "No books yet"},
      {"未能读取 SD 卡", "無法讀取 SD 卡", "Cannot read SD card"},
      {"请将书籍放入 SD 卡的 Books 文件夹", "請將書籍放入 SD 卡的 Books 資料夾", "Put books in the SD card Books folder"},
      {"上下键选择 · AI 打开", "上下鍵選擇 · AI 開啟", "Up/down: select · AI: open"},
      {"上下键选择 · AI 确认", "上下鍵選擇 · AI 確認", "Up/down: select · AI: confirm"},
      {"点选 · 再点确认 · AI 确认", "點選 · 再點確認 · AI 確認", "Tap: select / tap again: confirm"},
      {"返回阅读    上下键选择 · AI 确认", "返回閱讀    上下鍵選擇 · AI 確認", "Back to book · Up/down: select · AI: confirm"},
      {"返回菜单    上下键选择 · AI 确认", "返回選單    上下鍵選擇 · AI 確認", "Back to menu · Up/down: select · AI: confirm"},
      {"返回菜单    上下键选择 · AI 打开", "返回選單    上下鍵選擇 · AI 開啟", "Back to menu · Up/down: select · AI: open"},
      {"返回设置    上下键选择 · AI 确认", "返回設定    上下鍵選擇 · AI 確認", "Back to settings · Up/down: select · AI: confirm"},
      {"完成并返回    点击 +/- 调整 · AI 编辑", "完成並返回    點擊 +/- 調整 · AI 編輯", "Done · Tap +/- or press AI to edit"},
      {"点击 +/- 调整 · AI 编辑", "點擊 +/- 調整 · AI 編輯", "Tap +/- or press AI to edit"},
      {"这本书没有目录", "這本書沒有目錄", "No table of contents"},
      {"蓝牙：已开启", "藍牙：已開啟", "Bluetooth: on"}, {"蓝牙：已关闭", "藍牙：已關閉", "Bluetooth: off"},
      {"正在搜索…", "正在搜尋…", "Scanning…"}, {"搜索翻页器", "搜尋翻頁器", "Scan for turner"},
      {"断开连接", "中斷連線", "Disconnect"}, {"清除已配对设备", "清除已配對裝置", "Forget paired devices"},
      {"已连接：", "已連線：", "Connected: "}, {"连接：", "連線：", "Connect: "},
      {"已配对：", "已配對：", "Paired: "}, {"返回    上下键选择 · AI 确认", "返回    上下鍵選擇 · AI 確認", "Back · Up/down: select · AI: confirm"},
      {"返回：点击屏幕，或按 AI", "返回：點擊螢幕，或按 AI", "Tap screen or press AI to return"},
      {"返回    AI 缩放 · 上下键移动", "返回    AI 縮放 · 上下鍵移動", "Back · AI: zoom · Up/down: move"},
      {"返回    暂不支持这张图片", "返回    暫不支援這張圖片", "Back · Image format not supported"},
      {"图片未显示", "圖片未顯示", "Image not shown"},
      {"字符缺失", "字元缺失", "Missing glyphs"},
      {"正在准备当前页", "正在準備目前頁面", "Preparing page"},
      {"取消等待：点这里，或长按 AI", "取消等待：點這裡，或長按 AI", "Cancel: tap here or hold AI"},
      {"开启", "開啟", "On"}, {"已休眠", "已休眠", "Sleeping"},
      {"PSRAM 不可用，请检查板卡设置。", "PSRAM 不可用，請檢查板卡設定。", "PSRAM unavailable. Check board config."},
      {"无法打开 EPUB", "無法開啟 EPUB", "Cannot open EPUB"},
      {"EPUB 读取失败", "EPUB 讀取失敗", "EPUB read failed"},
      {"无法创建 SD 缓存文件夹", "無法建立 SD 快取資料夾", "Cannot create SD cache folder"},
      {"microSD 挂载失败，请插入 FAT32 卡。", "microSD 掛載失敗，請插入 FAT32 記憶卡。", "microSD mount failed. Insert FAT32 card."},
      {"请先将 sd-card/Fonts 复制到 microSD。", "請先將 sd-card/Fonts 複製到 microSD。", "Copy sd-card/Fonts to microSD first."},
      {"没有支持的封面图片", "沒有支援的封面圖片", "No supported cover image"},
      {"无法解码图片", "無法解碼圖片", "Cannot decode image"},
      {"唤醒引脚不可用", "喚醒接腳不可用", "Wake pin unavailable"},
      {"章节为空或格式无效", "章節為空或格式無效", "Empty or invalid chapter"},
      {"章节文件缺失", "找不到章節檔案", "Chapter file missing"},
      {"目录目标不在阅读顺序中", "目錄目標不在閱讀順序中", "TOC target not in reading order"},
  };
  for (const auto &item : table)
    if (strcmp(zh, item.zh) == 0)
      return uiLanguage == UiLanguage::Simplified ? item.zh :
             uiLanguage == UiLanguage::Traditional ? item.tw : item.en;
  for (const auto &item : table)
    if (strcmp(zh, item.en) == 0)
      return uiLanguage == UiLanguage::Simplified ? item.zh :
             uiLanguage == UiLanguage::Traditional ? item.tw : item.en;
  if (strncmp(zh, "已连接：", strlen("已连接：")) == 0)
    return ui("已连接：") + String(zh + strlen("已连接："));
  if (strncmp(zh, "连接：", strlen("连接：")) == 0)
    return ui("连接：") + String(zh + strlen("连接："));
  if (strncmp(zh, "已配对：", strlen("已配对：")) == 0)
    return ui("已配对：") + String(zh + strlen("已配对："));
  if (strncmp(zh, "EPUB layout: ", strlen("EPUB layout: ")) == 0) {
    const char *prefix = uiLanguage == UiLanguage::Simplified ? "EPUB 排版失败：" :
                         uiLanguage == UiLanguage::Traditional ? "EPUB 排版失敗：" : "EPUB layout: ";
    return String(prefix) + String(zh + strlen("EPUB layout: "));
  }
  if (strncmp(zh, "EPUB: ", strlen("EPUB: ")) == 0) {
    const char *prefix = uiLanguage == UiLanguage::Simplified ? "EPUB：" :
                         uiLanguage == UiLanguage::Traditional ? "EPUB：" : "EPUB: ";
    return String(prefix) + String(zh + strlen("EPUB: "));
  }
  if (strncmp(zh, "Passkey: ", strlen("Passkey: ")) == 0) {
    const char *prefix = uiLanguage == UiLanguage::Simplified ? "配对码：" :
                         uiLanguage == UiLanguage::Traditional ? "配對碼：" : "Passkey: ";
    return String(prefix) + String(zh + strlen("Passkey: "));
  }
  return String(zh);
}
void loadBookSettings(uint32_t bookIdentity) {
  fontSize = 26; lineSpacing = 125; margin = 24; useTraditional = false;
  char settingKey[16];
  snprintf(settingKey, sizeof(settingKey), "r%08lx", (unsigned long)bookIdentity);
  BookSettings saved;
  if (prefs.isKey(settingKey) && prefs.getBytesLength(settingKey) == sizeof(saved) &&
      prefs.getBytes(settingKey, &saved, sizeof(saved)) == sizeof(saved) && saved.version == 1) {
    fontSize = std::clamp(int(saved.fontSize), 18, 42);
    lineSpacing = std::clamp(int(saved.lineSpacing), 100, 180);
    margin = std::clamp(int(saved.margin), 12, 48);
    useTraditional = saved.traditional != 0;
  }
}
void saveBookSettings() {
  if (!loaded || !identity) return;
  BookSettings saved{1, fontSize, lineSpacing, margin, uint8_t(useTraditional)};
  char settingKey[16];
  snprintf(settingKey, sizeof(settingKey), "r%08lx", (unsigned long)identity);
  prefs.putBytes(settingKey, &saved, sizeof(saved));
}
void saveSystemSettings() {
  prefs.putUShort("sleep", sleepMinutes);
  prefs.putBool("ble", bleEnabled);
  prefs.putUChar("lang", static_cast<uint8_t>(uiLanguage));
}
void savePosition() {
  if (!loaded || !positionReady || key.isEmpty())
    return;
  ReadingPosition p{1, identity, chapter, character};
  if (prefs.putBytes(key.c_str(), &p, sizeof(p)) != sizeof(p))
    BoardConfig::serialTransport().println("[FM] Position save failed");
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
  p.font = &layoutMetrics;
  p.lineSpacingPct = lineSpacing;
  p.language = book.metadata().language;
  return p;
}
bool waitForPages(uint32_t anchor, bool last = false, uint32_t fragment = 0) {
  auto ready = [&]() {
    if (fragment && !reader.charForAnchor(fragment, &anchor)) return !reader.building();
    return reader.canSeek(anchor, last);
  };
  uint32_t painted = millis(), interval = 1500;
  while (!ready() && reader.building()) {
    auto status = reader.step(1);
    if (status != BookStatus::Ok) {
      BoardConfig::serialTransport().printf("[FM] wait failed at %s\n", reader.failureStage());
      error(String("EPUB layout: ") + bookStatusName(status), View::Menu);
      return false;
    }
    if (!ready() && millis() - painted > interval) {
      memset(frame, 255, sizeof(frame));
      header("正在准备当前页");
      text(uiLanguage == UiLanguage::English
               ? "Processed " + String(reader.progress()) + "%"
               : "已处理 " + String(reader.progress()) + "%", 20, 150);
      text(uiLanguage == UiLanguage::English
               ? "Pages ready: " + String(reader.pageCount())
               : "已生成 " + String(reader.pageCount()) + " 页", 20, 200);
      footer("长按 AI 取消等待");
      display.setFramebuffer(frame);
      display.displayBuffer(EInkDisplay::FAST_REFRESH);
      painted = millis();
      interval = 5000;
    }
    uint8_t b;
    while (input.popPress(b)) {
      if (b == InputManager::BTN_BACK || b == InputManager::BTN_POWER) {
        reader.stop();
        enter(loaded ? View::Menu : View::Shelf);
        return false;
      }
    }
    float nx, ny;
    while (input.popTouchTap(nx, ny)) {
      int x, y;
      fm::touchToPortrait(nx, ny, FM_SPLASH_ROTATE_CW, x, y);
    }
    delay(1);
  }
  if (!reader.pageCount())
    error("Empty or invalid chapter", View::Menu);
  return reader.pageCount() && ready();
}
bool loadChapter(uint32_t ch, uint32_t anchor = 0, bool last = false) {
  if (ch >= book.spineCount()) return false;
  reader.stop();
  auto *item = book.spineItem(ch);
  auto *entry = book.zip().find(item->href);
  if (!entry) { error("Chapter file missing", View::Menu); return false; }
  LayoutParams p = params();
  uint32_t hash = layoutGenerationHash(
      p, identity ^ (useTraditional ? 0x5443 : 0x5343) ^ 0x10002);
  char name[64];
  pageCacheName(ch, hash, name, sizeof(name));
  indexArena.reset();
  auto status = reader.open(cache, name, hash, indexArena);
  const uint32_t started = millis();
  if (status != BookStatus::Ok || reader.isPartial()) {
    layoutArena.reset();
    parseArena.reset();
    status = reader.begin(source, book.zip(), *entry, item->href, p, layoutArena, parseArena);
    if (status != BookStatus::Ok) {
      BoardConfig::serialTransport().printf("[FM] begin chapter=%lu failed at %s\n", (unsigned long)ch, reader.failureStage());
      error(String("EPUB layout: ") + bookStatusName(status), View::Menu);
      return false;
    }
    if (!waitForPages(anchor, last)) return false;
  }
  if (!reader.pageCount()) { error("Empty or invalid chapter", View::Menu); return false; }
  chapter = ch;
  pageNumber = last ? reader.pageCount() - 1 : reader.pageForChar(anchor);
  character = last ? reader.charStart(pageNumber) : anchor;
  view = View::Reading;
  dirty = forceFull = true;
  positionReady = true;
  lastActivity = millis();
  savePosition();
  BoardConfig::serialTransport().printf("[FM] chapter=%lu ready=%lums pages=%lu building=%d metrics=%lu/%lu\n",
      (unsigned long)ch, (unsigned long)(millis() - started), (unsigned long)reader.pageCount(),
      reader.building(), (unsigned long)layoutMetrics.hits, (unsigned long)layoutMetrics.misses);
  return true;
}
void continueLayout() {
  if (!reader.building() || !loaded || view == View::Shelf || view == View::Settings) return;
  // Keep a few pages ahead; resume further work when the reader is idle.
  if (dirty || millis() - lastBuildStep < 100 ||
      (reader.pageCount() > pageNumber + 6 && millis() - lastActivity < 1500)) return;
  lastBuildStep = millis();
  auto status = reader.step(1);
  if (status != BookStatus::Ok) {
    error(String("EPUB layout: ") + bookStatusName(status), View::Menu);
  } else if (!reader.building()) {
    BoardConfig::serialTransport().printf("[FM] chapter complete pages=%lu metrics=%lu/%lu\n",
        (unsigned long)reader.pageCount(), (unsigned long)layoutMetrics.hits,
        (unsigned long)layoutMetrics.misses);
    dirty = view == View::Reading;
  }
}
void removeBenchmarkCaches() {
  // Only our performance probes use these prefixes. Never remove book files,
  // fonts, normal page caches, or NVS reading positions.
  auto dir = SdMan.open(cache.root.c_str());
  FsFile f;
  char name[80];
  while (dir && f.openNext(&dir, O_RDONLY)) {
    f.getName(name, sizeof(name));
    String n = name;
    bool probe = !f.isDirectory() &&
        (n.startsWith("bench-s") || n.startsWith("bench-fc-s")) &&
        (n.endsWith(".fibp") || n.endsWith(".fibp.tmp"));
    auto bytes = f.fileSize();
    f.close();
    if (probe && cache.remove(name))
      BoardConfig::serialTransport().printf("[FM] removed probe cache %s bytes=%lu\n", name, (unsigned long)bytes);
  }
  BoardConfig::serialTransport().printf("[FM] SD used bytes=%llu total=%llu\n",
      (unsigned long long)SdMan.sdUsedBytes(), (unsigned long long)SdMan.sdTotalBytes());
}
void openBook(const String &path) {
  savePosition();
  reader.stop();
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
  loadBookSettings(identity);
  if (!initFonts()) {
    error("Copy sd-card/Fonts to microSD first.");
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
  removeBenchmarkCaches();
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
    while (pageNumber + 1 >= reader.pageCount() && reader.building()) {
      auto status = reader.step(1);
      if (status != BookStatus::Ok) {
        error(String("EPUB layout: ") + bookStatusName(status), View::Menu);
        return;
      }
      delay(1);
    }
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
  reader.stop();
  saveSystemSettings();
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
  header("EasyREAD");
  text(ui("已休眠"), 20, 180, 30);
  footer("按 AI 键唤醒");
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
  case 0: return ui("字号") + ": " + String(fontSize);
  case 1: return ui("行距") + ": " + String(lineSpacing) + "%";
  case 2: return ui("边距") + ": " + String(margin);
  case 3: return ui("字体") + ": " + ui(useTraditional ? "繁体中文" : "简体中文");
  case 4: return ui("自动休眠") + ": " + (sleepMinutes ? String(sleepMinutes) + " " + ui("分钟") : ui("关闭"));
  case 5: return ui("语言") + ": " + (uiLanguage == UiLanguage::Simplified ? ui("简体中文") : uiLanguage == UiLanguage::Traditional ? ui("繁体中文") : ui("English"));
  default: return ui("完成并返回");
  }
}
void settingRow(int i, bool highlight, int displayRow = -1) {
  int y = TOP + (displayRow < 0 ? i : displayRow) * ROW;
  text(ui(setting(i).c_str()), 28, y + 36);
  if (highlight) invertRect(16, y + 3, 448, ROW - 10);
}
int count() {
  switch (view) {
  case View::Shelf:
    return books.size() + 1;
  case View::Menu:
    return 5;
  case View::More:
    return 4;
  case View::ImageMenu:
    return 4;
  case View::Adjustment:
    return settingsReturn == View::SystemSettings && editingSetting == 1 ? 5 :
           settingsReturn == View::SystemSettings && editingSetting == 2 ? 3 :
           settingsReturn == View::SystemSettings ? 4 : 3;
  case View::Contents:
    return book.tocCount() + 1;
  case View::Settings:
    return 5;
  case View::SystemSettings:
    return 4;
  case View::Bluetooth:
    return 5 + BleHid.deviceCount() + BleHid.pairedCount();
  default:
    return 0;
  }
}
void moveSelection(int d) {
  touchSelected = -1;
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
  if (settingsReturn == View::SystemSettings) {
    if (i == 0) {
      uiLanguage = static_cast<UiLanguage>((static_cast<int>(uiLanguage) + (d > 0 ? 1 : 2)) % 3);
      saveSystemSettings();
    } else if (i == 1) {
      const int values[] = {0, 5, 10, 30};
      int at = 0;
      for (int n = 0; n < 4; ++n) if (sleepMinutes == values[n]) at = n;
      sleepMinutes = values[(at + (d > 0 ? 1 : 3)) % 4];
      saveSystemSettings();
    } else if (i == 2) {
      bleEnabled = !bleEnabled;
      saveSystemSettings();
      if (bleEnabled) BleHid.begin("EasyREAD"); else BleHid.end();
    }
    dirty = forceFull = true;
    return;
  }
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
    break;
  case 4: break;
  default:
    initFonts();
    saveBookSettings();
    if (loaded)
      loadChapter(chapter, character);
    else
      enter(View::Shelf);
    return;
  }
  saveBookSettings();
  dirty = true;
}
void activate(int i) {
  touchSelected = -1;
  switch (view) {
  case View::Shelf:
    if (i == int(books.size())) {
      settingsReturn = View::SystemSettings;
      enter(View::SystemSettings);
    } else if (i >= 0 && i < int(books.size()))
      openBook(books[i]);
    break;
  case View::Menu:
    if (i == 0) {
      loadChapter(chapter, character);
    }
    if (i == 1)
      enter(View::Contents);
    if (i == 2) {
      settingsReturn = View::Menu;
      enter(View::Settings);
    }
    if (i == 3) {
      savePosition();
      enter(View::Shelf);
    }
    if (i == 4) enter(View::More);
    break;
  case View::More:
    if (i == 3) { enter(View::Menu); break; }
    if (i == 0) enter(View::Bluetooth);
    if (i == 2) sleepNow();
    if (i == 1) {
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
    if (i == 0) { enter(View::Menu); break; }
    auto *toc = book.tocEntry(i - 1);
    if (!toc)
      break;
    for (size_t ch = 0; ch < book.spineCount(); ++ch)
      if (strcmp(book.spineItem(ch)->href, toc->href) == 0) {
        if (loadChapter(ch)) {
          uint32_t a = 0;
          const uint32_t fragment = toc->fragment ? ZipCatalog::hashPath(toc->fragment) : 0;
          if (fragment && !waitForPages(0, false, fragment)) return;
          if (fragment && reader.charForAnchor(fragment, &a)) {
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
    if (i < 4) {
      editingSetting = i;
      enter(View::Adjustment);
    } else adjust(5, 0);
    break;
  case View::Adjustment:
    if ((settingsReturn == View::SystemSettings && editingSetting == 1 && i == 4) ||
        (settingsReturn == View::SystemSettings && editingSetting == 0 && i == 3) ||
        (settingsReturn == View::SystemSettings && editingSetting == 2 && i == 2) ||
        (settingsReturn != View::SystemSettings && i == 2)) {
      enter(settingsReturn == View::SystemSettings ? View::SystemSettings : View::Settings);
      selected = editingSetting;
    } else if (settingsReturn == View::SystemSettings && editingSetting == 0) {
      uiLanguage = static_cast<UiLanguage>(i);
      saveSystemSettings();
      dirty = forceFull = true;
    } else if (settingsReturn == View::SystemSettings && editingSetting == 1) {
      const int values[] = {0, 5, 10, 30};
      sleepMinutes = values[i];
      saveSystemSettings();
      dirty = true;
    } else if (settingsReturn == View::SystemSettings && editingSetting == 2) {
      bleEnabled = i == 0;
      saveSystemSettings();
      if (bleEnabled) BleHid.begin("EasyREAD"); else BleHid.end();
      dirty = true;
    } else if (editingSetting == 3) {
      useTraditional = i == 1;
      saveBookSettings(); dirty = true;
    } else adjust(editingSetting, i == 0 ? -1 : 1);
    break;
  case View::SystemSettings:
    if (i < 3) { settingsReturn = View::SystemSettings; editingSetting = i; enter(View::Adjustment); }
    else enter(View::Shelf);
    break;
  case View::Bluetooth:
    if (i == 0) {
      bleEnabled = !bleEnabled;
      saveSystemSettings();
      if (bleEnabled)
        BleHid.begin("EasyREAD");
      else
        BleHid.end();
    } else if (i == 1) {
      if (!bleEnabled) {
        bleEnabled = true;
        saveSystemSettings();
        BleHid.begin("EasyREAD");
      }
      BleHid.startScan(8000);
    } else if (i == 2)
      BleHid.disconnect();
    else if (i == 3) {
      if (loaded)
        enter(View::More);
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
  case View::ImageMenu:
    if (i == 3) { loadChapter(chapter, character); break; }
    if (i == 0) zoom = std::min(4, zoom * 2);
    if (i == 1) zoom = std::max(1, zoom / 2);
    if (i < 2) panX = panY = 0;
    view = View::Image;
    dirty = forceFull = true;
    break;
  default:
    break;
  }
}
void back() {
  if (view == View::Reading)
    enter(View::Menu);
  else if (view == View::ImageMenu) {
    view = View::Image;
    dirty = forceFull = true;
  } else if (view == View::Image) {
    view = View::Reading;
    dirty = forceFull = true;
  } else if (view == View::Error)
    enter(errorReturn);
  else if (view == View::Adjustment) {
    enter(settingsReturn == View::SystemSettings ? View::SystemSettings : View::Settings);
    selected = editingSetting;
  } else if (view == View::Settings)
    adjust(5, 0);
  else if (view == View::SystemSettings)
    enter(View::Shelf);
  else if (view == View::Bluetooth && loaded)
    enter(View::More);
  else if (view == View::Contents || view == View::More)
    enter(View::Menu);
  else if (view == View::Menu)
    loadChapter(chapter, character);
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
      enter(View::ImageMenu);
      return;
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
void selectByTouch(int i) {
  if (touchSelectionView == view && touchSelected == i && selected == i) {
    activate(i);
    return;
  }
  selected = i;
  touchSelected = i;
  touchSelectionView = view;
  if (selected < offset) offset = selected;
  if (selected >= offset + VISIBLE) offset = selected - VISIBLE + 1;
  dirty = true;
}
void tap(int x, int y) {
  lastActivity = millis();
  if (y > 750 && view != View::Reading) return;
  if (view == View::Reading) {
    // Edge taps turn pages even when a full-page cover occupies the hit area.
    if (x < 160) {
      turn(-1);
      return;
    }
    if (x > 320) {
      turn(1);
      return;
    }
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
    if (x >= 160 && x <= 320 && y >= 100 && y < 750) {
      enter(View::ImageMenu);
      return;
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
  if (view == View::Shelf && y < 72 && x > 360) {
    selectByTouch(int(books.size()));
    return;
  }
  if (y >= TOP && y < TOP + VISIBLE * ROW) {
    int i = offset + (y - TOP) / ROW;
    if (i < count()) {
      selectByTouch(i);
      return;
    }
  }
  touchSelected = -1;
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
  const uint32_t started = millis();
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
    String progress;
    if (uiLanguage == UiLanguage::English)
      progress = "Ch. " + String(chapter + 1) + "    " + String(pageNumber + 1) +
          " / " + String(reader.pageCount()) + (reader.building() ? "+ pages" : " pages");
    else if (uiLanguage == UiLanguage::Traditional)
      progress = "第 " + String(chapter + 1) + " 章    " + String(pageNumber + 1) +
          " / " + String(reader.pageCount()) + (reader.building() ? "+ 頁" : " 頁");
    else
      progress = "第 " + String(chapter + 1) + " 章    " + String(pageNumber + 1) +
          " / " + String(reader.pageCount()) + (reader.building() ? "+ 页" : " 页");
    if (missing) progress += uiLanguage == UiLanguage::English ? "  Missing glyphs" : "  字符缺失";
    if (status != BookStatus::Ok) progress += uiLanguage == UiLanguage::English ? "  Image error" : "  图片未显示";
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
    header(uiLanguage == UiLanguage::English
               ? "Image  " + String(zoom) + "x"
               : uiLanguage == UiLanguage::Traditional
                     ? "圖片  " + String(zoom) + " 倍"
                     : "图片  " + String(zoom) + " 倍");
    footer("点中间或按 AI 打开图片菜单");
  } else if (view == View::Error) {
    header("提示");
    text(fit(ui(message.c_str()), 440, 20), 20, 160, 20);
    footer("按 AI 返回");
  } else {
    if (view == View::Shelf) {
      header("书架");
      text(ui("设置"), 380, 45, 22);
      if (books.empty()) {
        text(ui(sdReady ? "还没有电子书" : "未能读取 SD 卡"), 20, 150);
        text(ui("请将书籍放入 SD 卡的 Books 文件夹"), 20, 200, 20);
      }
      for (int r = 0; r < VISIBLE && offset + r < count(); ++r) {
        if (offset + r == int(books.size())) {
          row(r, "系统设置", selected == int(books.size()));
          continue;
        }
        String n = books[offset + r];
        n = n.substring(n.lastIndexOf('/') + 1);
        if (n.endsWith(".epub") || n.endsWith(".EPUB")) n.remove(n.length() - 5);
        row(r, n, selected == offset + r);
      }
      footer(uiLanguage == UiLanguage::English
                 ? String(books.size()) + " books    Up/down: select · AI: open"
                 : uiLanguage == UiLanguage::Traditional
                       ? String(books.size()) + " 本書    上下鍵選擇 · AI 開啟"
                       : String(books.size()) + " 本书    上下键选择 · AI 打开");
    }
    if (view == View::Menu) {
      header("阅读菜单");
      text(fit(title, 440, 18), 20, 82, 18);
      const char *labels[] = {"返回阅读", "目录", "阅读设置", "返回书架", "更多"};
      for (int i = 0; i < 5; ++i) row(i, labels[i], selected == i);
      footer("点选 · 再点确认 · AI 确认");
    }
    if (view == View::More) {
      header("更多");
      const char *labels[] = {"蓝牙翻页器", "查看封面", "休眠", "返回阅读菜单"};
      for (int i = 0; i < 4; ++i) row(i, labels[i], selected == i);
      footer("点选 · 再点确认 · AI 确认");
    }
    if (view == View::Adjustment) {
      header("调整设置");
      text(ui(setting(editingSetting).c_str()), 20, 82, 22);
      const char *labels[] = {"减小", "增大", "完成", "完成", "完成"};
      if (settingsReturn == View::SystemSettings && editingSetting == 0) { labels[0] = "简体中文"; labels[1] = "繁体中文"; labels[2] = "English"; }
      if (settingsReturn == View::SystemSettings && editingSetting == 1) { labels[0] = "关闭"; labels[1] = "5 分钟"; labels[2] = "10 分钟"; labels[3] = "30 分钟"; }
      if (settingsReturn == View::SystemSettings && editingSetting == 2) { labels[0] = "开启"; labels[1] = "关闭"; }
      if (settingsReturn != View::SystemSettings && editingSetting == 3) { labels[0] = "简体中文"; labels[1] = "繁体中文"; }
      labels[count() - 1] = settingsReturn == View::SystemSettings ? "返回系统设置" : "返回阅读设置";
      for (int i = 0; i < count(); ++i) row(i, labels[i], selected == i);
      footer("点选 · 再点确认 · AI 确认");
    }
    if (view == View::Contents) {
      header("目录");
      for (int r = 0; r < VISIBLE && offset + r < count(); ++r) {
        int i = offset + r;
        row(r, i == 0 ? ui("返回阅读菜单") : String(book.tocEntry(i - 1)->title), selected == i);
      }
      if (!book.tocCount())
        text(ui("这本书没有目录"), 20, 220);
      footer("点选 · 再点确认 · AI 确认");
    }
    if (view == View::Settings) {
      header("阅读设置");
      for (int i = 0; i < 4; ++i) settingRow(i, selected == i);
      row(4, "保存并返回阅读", selected == 4);
      footer("点选 · 再点确认 · AI 确认");
    }
    if (view == View::SystemSettings) {
      header("系统设置");
      settingRow(5, selected == 0, 0);
      settingRow(4, selected == 1, 1);
      int y = TOP + 2 * ROW;
      text(ui(bleEnabled ? "蓝牙：已开启" : "蓝牙：已关闭"), 28, y + 36);
      if (selected == 2) invertRect(16, y + 3, 448, ROW - 10);
      row(3, "返回书架", selected == 3);
      footer("点选 · 再点确认 · AI 确认");
    }
    if (view == View::Bluetooth) {
      header(BleHid.isConnected()
                 ? String("已连接：") + BleHid.connectedName()
                 : "蓝牙翻页器");
      for (int r = 0; r < VISIBLE && offset + r < count(); ++r) {
        int i = offset + r;
        String s;
        if (i == 0)
          s = bleEnabled ? "蓝牙：已开启" : "蓝牙：已关闭";
        else if (i == 1)
          s = BleHid.isScanning() ? "正在搜索…"
                                  : "搜索翻页器";
        else if (i == 2)
          s = "断开连接";
        else if (i == 3)
          s = loaded ? "返回更多" : "返回书架";
        else if (i == 4)
          s = "清除已配对设备";
        else if (i < 5 + BleHid.deviceCount())
          s = String("连接：") + BleHid.device(i - 5).name;
        else
          s = String("已配对：") +
              BleHid.paired(i - 5 - BleHid.deviceCount()).name;
        row(r, s, selected == i);
      }
      footer(message.isEmpty() ? "点选 · 再点确认 · AI 确认" : message);
    }
    if (view == View::ImageMenu) {
      header("图片操作");
      const char *labels[] = {"放大", "缩小", "返回图片", "返回阅读"};
      for (int i = 0; i < 4; ++i) row(i, labels[i], selected == i);
      footer("点选 · 再点确认 · AI 确认");
    }
  }
  display.setFramebuffer(frame);
  display.displayBuffer(forceFull || ++refreshes % 8 == 0
                            ? EInkDisplay::FULL_REFRESH
                            : EInkDisplay::FAST_REFRESH);
  if (view == View::Reading)
    BoardConfig::serialTransport().printf("[FM] page=%lu rendered=%lums building=%d\n",
        (unsigned long)pageNumber, (unsigned long)(millis() - started), reader.building());
  dirty = false;
  forceFull = false;
}
} // namespace
void setup() {
  BoardConfig::holdPowerRails();
  BoardConfig::serialTransport().begin(115200);
  // Initialize the shared SPI bus with SD's MISO before the display opens it.
  // SPI.begin() does not remap pins once the bus has already been started.
  BoardConfig::ACTIVE.sd.spiHz = 4000000;
  sdReady = SdMan.begin();
  BoardConfig::serialTransport().printf("[FM] SD mount=%d\n", sdReady);
  display.begin();
  memset(frame, 255, sizeof(frame));
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x)
      if (!(fmSplash[y * 60 + x / 8] & (0x80 >> (x & 7))))
        pixel(x, y);
  display.setFramebuffer(frame);
  display.displayBuffer(EInkDisplay::FULL_REFRESH);
  prefs.begin("faiyuet", false);
  // Retire the previous device-wide reading layout values. Every book now
  // starts from defaults until it has its own saved BookSettings record.
  if (prefs.isKey("size")) prefs.remove("size");
  if (prefs.isKey("spacing")) prefs.remove("spacing");
  if (prefs.isKey("margin")) prefs.remove("margin");
  if (prefs.isKey("tc")) prefs.remove("tc");
  sleepMinutes = prefs.getUShort("sleep", 10);
  bleEnabled = prefs.getBool("ble", true);
  uint8_t savedLanguage = prefs.getUChar("lang", 0);
  uiLanguage = savedLanguage <= static_cast<uint8_t>(UiLanguage::English)
                   ? static_cast<UiLanguage>(savedLanguage)
                   : UiLanguage::Simplified;
  uiFace.init(fmUiFont, sizeof(fmUiFont), 24);
  uiFonts.add(&uiFace);
  memoryReady =
      allocate(bookArena, 768 * 1024) && allocate(layoutArena, 512 * 1024) &&
      allocate(parseArena, 256 * 1024) && allocate(indexArena, 256 * 1024) &&
      allocate(pageArena, 256 * 1024) && allocate(imageArena, 512 * 1024);
  metricsEntries = static_cast<fm::LayoutMetricsCache::Entry *>(
      heap_caps_calloc(8192, sizeof(fm::LayoutMetricsCache::Entry), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  layoutMetrics.begin(fonts, metricsEntries, metricsEntries ? 8192 : 0);
  fontReadEntries = static_cast<fm::FontReadCache::Entry *>(
      heap_caps_calloc(256, sizeof(fm::FontReadCache::Entry), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  fontReadCache.begin(fontReadUncached, fontReadEntries, fontReadEntries ? 256 : 0);
  if (sdReady) {
    initFonts();
    scanBooks();
  }
  // ReaderGeometry already rotates the panel frame into portrait. The Sticky
  // profile's extra mirrors reverse portrait Y (upper rows hit below the menu).
  BoardConfig::ACTIVE.touch.flipX = false;
  BoardConfig::ACTIVE.touch.flipY = false;
  input.begin();
  input.beginAsync();
  if (bleEnabled)
    BleHid.begin("EasyREAD");
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
    BleHid.begin("EasyREAD");
  BleHid.poll();
  uint8_t b;
  int handled = 0;
  while (handled++ < 4 && input.popPress(b))
    button(b);
  float nx, ny, sx, sy, ex, ey;
  int x, y;
  while (input.popTouchTap(nx, ny)) {
    portrait(nx, ny, x, y);
    BoardConfig::serialTransport().printf("[FM] tap view=%d native=%.3f,%.3f portrait=%d,%d row=%d offset=%d count=%d\n",
        int(view), double(nx), double(ny), x, y,
        y >= TOP && y < TOP + VISIBLE * ROW ? (y - TOP) / ROW : -1, offset, count());
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
  continueLayout();
  if (sleepMinutes &&
      millis() - lastActivity > uint32_t(sleepMinutes) * 60000u) {
    sleepNow();
    lastActivity = millis();
  }
  delay(15);
}
