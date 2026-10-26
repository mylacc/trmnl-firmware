#pragma once

// Tide-charts screens kept on the device (OG DIY Kit, TIDE_BUTTONS builds). These are the pure
// parts, shared with the native tests: parsing the Worker's "tide" object and deciding whether a
// stored screen may be shown. The flash and display side lives in src/tide_device.cpp.

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

constexpr int TIDE_VIEWS = 3;                         // 0 today, 1 tomorrow, 2 week
constexpr size_t TIDE_BMP_SIZE = 48062;               // 800x480 1-bit BMP, as the Worker draws it
constexpr uint32_t TIDE_CLOCK_VALID_AFTER = 1767225600; // 2026-01-01: before this the clock was never set

// The /api/display "tide" object.
struct TideInfo {
  bool present = false;                    // a usable object was there
  int view = -1;                           // screen in image_url
  uint32_t expires = 0;                    // unix time after which that screen is too old to show offline
  uint32_t rates[TIDE_VIEWS] = {0, 0, 0};  // refresh_rate that keeps each screen up
  String cacheId;                          // changes about hourly; fetch cacheUrl when it differs from ours
  String cacheUrl;                         // the screens in cacheViews, concatenated
  int cacheViews[TIDE_VIEWS] = {-1, -1, -1};
  int cacheCount = 0;                      // 0 = nothing to fetch
  uint32_t cacheExpires = 0;
};

TideInfo parseTideInfo(const String &json);

// "D1" -> 0 (today), "D2" -> 1 (tomorrow), "D4" -> 2 (week); anything else -1.
int tideViewForButton(const String &button);

// A stored screen can be drawn: the exact BMP the Worker sends, the clock has been set, not expired.
bool tideImageUsable(const uint8_t *data, size_t size, uint32_t now, uint32_t expires);

// Flash file for a screen that isn't the one up ("/tide0.bmp".."/tide2.bmp"); nullptr if out of range.
const char *tideSlotPath(int view);
