#include <ArduinoJson.h>
#include <tide_views.h>

TideInfo parseTideInfo(const String &json) {
  TideInfo t;
  if (json.length() == 0) return t;
  JsonDocument doc;
  if (deserializeJson(doc, json)) return t;

  int view = doc["view"] | -1;
  if (view < 0 || view >= TIDE_VIEWS) return t;
  t.present = true;
  t.view = view;
  t.expires = doc["expires"] | 0u;
  JsonArrayConst rates = doc["rates"];
  for (int i = 0; i < TIDE_VIEWS && i < (int)rates.size(); i++) t.rates[i] = rates[i] | 0u;

  JsonObjectConst cache = doc["cache"];
  if (cache.isNull()) return t;
  t.cacheId = cache["id"] | "";
  t.cacheUrl = cache["url"] | "";
  t.cacheExpires = cache["expires"] | 0u;
  for (JsonVariantConst v : cache["views"].as<JsonArrayConst>()) {
    int n = v | -1;
    if (n < 0 || n >= TIDE_VIEWS || t.cacheCount == TIDE_VIEWS) {
      t.cacheCount = 0; // malformed list: fetch nothing rather than misfile a screen
      return t;
    }
    t.cacheViews[t.cacheCount++] = n;
  }
  if (t.cacheId.length() == 0 || t.cacheUrl.length() == 0) t.cacheCount = 0;
  return t;
}

int tideViewForButton(const String &button) {
  if (button == "D1") return 0;
  if (button == "D2") return 1;
  if (button == "D4") return 2;
  return -1;
}

static uint32_t le32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

bool tideImageUsable(const uint8_t *data, size_t size, uint32_t now, uint32_t expires) {
  if (!data || size != TIDE_BMP_SIZE) return false;
  if (data[0] != 'B' || data[1] != 'M' || le32(data + 2) != TIDE_BMP_SIZE) return false;
  if (le32(data + 18) != 800 || le32(data + 22) != 480 || data[28] != 1) return false; // 800x480, 1 bit
  return now >= TIDE_CLOCK_VALID_AFTER && now < expires;
}

const char *tideSlotPath(int view) {
  static const char *paths[TIDE_VIEWS] = {"/tide0.bmp", "/tide1.bmp", "/tide2.bmp"};
  return view >= 0 && view < TIDE_VIEWS ? paths[view] : nullptr;
}
