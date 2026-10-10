# SPDX-License-Identifier: MIT
# Copyright (c) 2026 FaiyuetCik
"""Build the sharing package against its editable Arduino source directory.

Relative PlatformIO symlink package URIs may reuse another project's cached
pointer. Override framework lookup for this build rather than editing that cache.
"""
Import("env")
from pathlib import Path
import json

bundled = Path(env.subst("$PROJECT_DIR")) / "vendor" / "Arduino-ESP32"
metadata = bundled / "package.json"
if not metadata.is_file():
    raise RuntimeError("The sharing package must include vendor/Arduino-ESP32")
if json.loads(metadata.read_text(encoding="utf-8"))["version"] != "3.3.7":
    raise RuntimeError("Expected the pinned Arduino-ESP32 3.3.7 source")

platform = env.PioPlatform()
original_lookup = platform.get_package_dir

def package_dir(name):
    if name == "framework-arduinoespressif32":
        return str(bundled.resolve())
    return original_lookup(name)

platform.get_package_dir = package_dir
print("Using bundled Arduino source:", bundled.resolve())
