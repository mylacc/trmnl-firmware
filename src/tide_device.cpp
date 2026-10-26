#ifdef TIDE_BUTTONS
#include <Arduino.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <display.h>
#include <filesystem.h>
#include <globals.h>
#include <http_client.h>
#include <misc/clock.h>
#include <refresh_interval.h>
#include <tide_device.h>
#include <tide_views.h>
#include <trmnl_log.h>

#define CURRENT_FILE "/current.bmp"

// NVS keys (Preferences "data" namespace; at most 15 characters)
static const char *KEY_CUR = "tide_cur";         // screen held by /current.bmp
static const char *KEY_CUR_EXP = "tide_cur_exp"; // until when /current.bmp may be shown offline
static const char *KEY_EXP[TIDE_VIEWS] = {"tide_exp0", "tide_exp1", "tide_exp2"};
static const char *KEY_RATE[TIDE_VIEWS] = {"tide_rate0", "tide_rate1", "tide_rate2"};
static const char *KEY_CACHE_ID = "tide_cache_id";

// NVS entries wear the flash on every write, so only write what changed.
static void putUInt(Preferences &prefs, const char *key, uint32_t value) {
  if (!prefs.isKey(key) || prefs.getUInt(key, 0) != value) prefs.putUInt(key, value);
}
static void putInt(Preferences &prefs, const char *key, int32_t value) {
  if (!prefs.isKey(key) || prefs.getInt(key, -1) != value) prefs.putInt(key, value);
}

bool tideShowStored(Preferences &prefs, int view) {
  if (view < 0 || view >= TIDE_VIEWS) return false;
  uint32_t rate = prefs.getUInt(KEY_RATE[view], 0);
  if (rate == 0) return false; // no check-in has told us the rates yet
  uint32_t now = systemClock().getTime();
  int cur = prefs.getInt(KEY_CUR, -1);

  if (view == cur) {
    // That screen is already up: nothing to draw, just keep it up for its full time again.
    uint32_t expires = prefs.getUInt(KEY_CUR_EXP, 0);
    if (now < TIDE_CLOCK_VALID_AFTER || now >= expires) return false;
    refreshInterval.applyServerRate(rate);
    Log_info("tide: screen %d is already up; refresh rate %u", view, rate);
    return true;
  }

  uint8_t *image = nullptr;
  size_t size = filesystem_read_and_allocate(tideSlotPath(view), &image);
  if (!tideImageUsable(image, size, now, prefs.getUInt(KEY_EXP[view], 0))) {
    Log_info("tide: no usable stored screen %d (size %u), going online", view, (unsigned)size);
    free(image);
    return false;
  }

  // The screen being replaced becomes its file again, so its key keeps working without Wi-Fi.
  const char *curPath = tideSlotPath(cur);
  if (curPath && LittleFS.exists(CURRENT_FILE) && LittleFS.rename(CURRENT_FILE, curPath))
    putUInt(prefs, KEY_EXP[cur], prefs.getUInt(KEY_CUR_EXP, 0));
  // Write before drawing: display_show_image() flips the bitmap in place.
  writeImageToFile(CURRENT_FILE, image, size);
  display_show_image(image, size, true);
  free(image);

  putInt(prefs, KEY_CUR, view);
  putUInt(prefs, KEY_CUR_EXP, prefs.getUInt(KEY_EXP[view], 0));
  refreshInterval.applyServerRate(rate);
  Log_info("tide: showed stored screen %d without Wi-Fi; refresh rate %u", view, rate);
  return true;
}

// Stream one screen into its flash file. The old file goes first, so a full partition still has room.
static bool writeScreen(WiFiClient *stream, int view) {
  const char *path = tideSlotPath(view);
  LittleFS.remove(path);
  if (LittleFS.totalBytes() - LittleFS.usedBytes() < TIDE_BMP_SIZE + 2 * 4096) {
    Log_error("tide: no room in flash for %s", path);
    return false;
  }
  File file = LittleFS.open(path, FILE_WRITE);
  if (!file) return false;
  uint8_t chunk[1024];
  size_t left = TIDE_BMP_SIZE;
  uint32_t start = millis();
  while (left > 0 && millis() - start < 20000) {
    size_t ready = stream->available();
    if (ready == 0) {
      if (!stream->connected()) break;
      delay(2);
      continue;
    }
    size_t n = stream->readBytes(chunk, min(left, min(ready, sizeof(chunk))));
    if (n == 0 || file.write(chunk, n) != n) break;
    left -= n;
  }
  file.close();
  if (left > 0) {
    LittleFS.remove(path);
    Log_error("tide: %s cut short, %u bytes missing", path, (unsigned)left);
    return false;
  }
  return true;
}

void tideFetchScreens(Preferences &prefs, const String &tideJson) {
  TideInfo t = parseTideInfo(tideJson);
  if (!t.present || t.cacheCount == 0) return;
  if (prefs.getString(KEY_CACHE_ID, "") == t.cacheId) return; // already have this hour's screens

  uint32_t start = millis();
  bool ok = withHttp(t.cacheUrl, [&](HTTPClient *http, HttpError error) -> bool {
    if (error != HTTPCLIENT_SUCCESS || !http) return false;
    http->setTimeout(15000);
    int code = http->GET();
    int length = http->getSize();
    if (code != HTTP_CODE_OK || length != (int)(TIDE_BMP_SIZE * t.cacheCount)) {
      Log_error("tide: screens download answered %d with %d bytes", code, length);
      return false;
    }
    WiFiClient *stream = http->getStreamPtr();
    for (int i = 0; i < t.cacheCount; i++) {
      int view = t.cacheViews[i];
      if (!writeScreen(stream, view)) return false;
      putUInt(prefs, KEY_EXP[view], t.cacheExpires);
    }
    return true;
  });
  if (ok) prefs.putString(KEY_CACHE_ID, t.cacheId);
  Log_info("tide: screens %s %s in %lu ms", t.cacheId.c_str(), ok ? "stored" : "not stored",
           (unsigned long)(millis() - start));
}

void tideNewCurrent(Preferences &prefs, const String &tideJson) {
  TideInfo t = parseTideInfo(tideJson);
  if (!t.present) return;
  int cur = prefs.getInt(KEY_CUR, -1);
  const char *curPath = tideSlotPath(cur);
  // Keep the screen being replaced (it's fresher than its file) unless the same screen is redrawn.
  if (cur != t.view && curPath && LittleFS.exists(CURRENT_FILE) && LittleFS.rename(CURRENT_FILE, curPath))
    putUInt(prefs, KEY_EXP[cur], prefs.getUInt(KEY_CUR_EXP, 0));
  putInt(prefs, KEY_CUR, t.view);
  putUInt(prefs, KEY_CUR_EXP, t.expires);
  for (int i = 0; i < TIDE_VIEWS; i++)
    if (t.rates[i]) putUInt(prefs, KEY_RATE[i], t.rates[i]);
}
#endif // TIDE_BUTTONS
