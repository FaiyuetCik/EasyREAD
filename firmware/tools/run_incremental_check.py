"""Run under WSL/Linux; optional argument is a locally owned DRM-free EPUB.

Default input is the project-authored fixture. Real-book test inputs are never
copied into the repository. Results and objects stay in ignored test-results.
"""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

base = Path(__file__).resolve().parents[1]
subprocess.run([sys.executable, str(base / "tools/run_host_check.py")], check=True)
sdk = base / "freeink-sdk/libs"
book = sdk / "book/FreeInkBook"
font = sdk / "font/FreeInkFont"
includes = [base / "src", book / "include", font / "include", font / "third_party/stb",
            font / "third_party/freetype/include"]
includes += [book / "third_party" / x for x in ["expat", "miniz", "libunibreak", "pngle", "tjpgd", "stb"]]
objects = [p for p in (base / "test-results/host-build").glob("*.o")
           if p.name != "tools_host_check.cpp.o"]
exe = base / "test-results/incremental-check"
subprocess.run(["g++", "-std=c++17", "-O1", *[f"-I{p}" for p in includes],
                str(base / "tools/incremental_check.cpp"), *map(str, objects), "-o", str(exe)], check=True)
input_book = Path(sys.argv[1]) if len(sys.argv) > 1 else base.parent / "sd-card/Books/Faiyuet-Morgan-test.epub"
with tempfile.TemporaryDirectory(prefix="fm-incremental-") as temp:
    temp = Path(temp)
    shutil.copy2(input_book, temp / "book.epub")
    for name in ["NotoSansSC-VF.ttf", "NotoSansTC-VF.ttf"]:
        shutil.copy2(base.parent / "sd-card/Fonts" / name, temp / name)
    result = subprocess.run([str(exe), str(temp / "book.epub"), str(temp / "NotoSansSC-VF.ttf"),
                             str(temp / "NotoSansTC-VF.ttf"), str(temp / "cache")],
                            capture_output=True, text=True)
    (base / "test-results/incremental-check.txt").write_text(result.stdout + result.stderr, encoding="utf-8")
    print(result.stdout + result.stderr)
    result.check_returncode()
