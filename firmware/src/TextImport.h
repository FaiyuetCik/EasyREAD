// SPDX-License-Identifier: MIT
#pragma once
#include <Arduino.h>
#include <functional>

namespace fm {
// Builds a disposable, uncompressed EPUB cache. The original TXT is read only.
// progress returns false when the user cancels the import.
bool importText(const String &path, const String &destination, const String &title,
                const std::function<bool(unsigned)> &progress, String &error);
}
