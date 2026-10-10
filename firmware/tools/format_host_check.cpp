// SPDX-License-Identifier: MIT
#include "RasterBook.h"
#include "TextImport.h"
#include <fstream>
#include <iostream>
#include <vector>
class Source : public freeink::book::BookSource {
  std::ifstream file;
  uint64_t length;
public:
  explicit Source(const char *path) : file(path, std::ios::binary) {
    if (!file) throw std::runtime_error("input open failed");
    file.seekg(0, std::ios::end); length = file.tellg();
  }
  uint64_t size() const override { return length; }
  int32_t readAt(uint64_t at, void *dst, uint32_t n) override {
    file.clear(); file.seekg(at); file.read(static_cast<char *>(dst), n); return file.gcount();
  }
};
int main(int argc, char **argv) {
  try {
    if (argc < 4) throw std::runtime_error("arguments");
    String error;
    std::string mode = argv[1];
    if (mode == "import" || mode == "cancel") {
      bool result = fm::importText(argv[2], argv[3], "EasyREAD original test book", [&](unsigned) { return mode != "cancel"; }, error);
      std::cout << (result ? "PASS import" : "REJECT ") << error << '\n';
      return result ? 0 : 2;
    }
    if (argc != 9) throw std::runtime_error("raster arguments");
    Source source(argv[2]); fm::RasterBook book;
    if (!book.open(source, mode == "bmp", error)) { std::cout << "REJECT " << error << '\n'; return 2; }
    std::vector<uint8_t> frame(480 * 800, 255);
    int panX = std::stoi(argv[7]), panY = std::stoi(argv[8]);
    bool result = book.render(std::stoul(argv[4]), std::stoi(argv[5]), std::stoi(argv[6]), panX, panY,
        [&](int x, int y, bool black) {
          if (x < 0 || x >= 480 || y < 0 || y >= 800) throw std::runtime_error("pixel outside screen");
          frame.at(y * 480 + x) = black ? 0 : 255;
        }, error);
    if (!result) { std::cout << "REJECT " << error << '\n'; return 2; }
    std::ofstream output(argv[3], std::ios::binary);
    output << "P5\n480 800\n255\n";
    output.write(reinterpret_cast<const char *>(frame.data()), frame.size());
    std::cout << "PASS pages=" << book.pageCount() << " chapters=" << book.chapters.size()
              << " pan=" << panX << ',' << panY << '\n';
    for (const auto &chapter : book.chapters) std::cout << "TOC " << chapter.page << " " << chapter.name << '\n';
    return 0;
  } catch (const std::exception &e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
}
