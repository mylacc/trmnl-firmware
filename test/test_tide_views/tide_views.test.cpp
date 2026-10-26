#include <string.h>
#include <tide_views.h>
#include <unity.h>

static const uint32_t NOW = 1793050000; // 2026-10-26

// A header like the Worker's 800x480 1-bit BMP; the pixels don't matter here.
static uint8_t image[TIDE_BMP_SIZE];
static void makeImage() {
  memset(image, 0, sizeof(image));
  image[0] = 'B';
  image[1] = 'M';
  uint32_t size = TIDE_BMP_SIZE, w = 800, h = 480;
  memcpy(image + 2, &size, 4); // little-endian, as on the ESP32 and the test host
  memcpy(image + 18, &w, 4);
  memcpy(image + 22, &h, 4);
  image[28] = 1;
}

void test_parse_full_object(void) {
  TideInfo t = parseTideInfo("{\"view\":1,\"expires\":1793060000,\"rates\":[900,914,924],"
                             "\"cache\":{\"id\":\"20261026-14\",\"url\":\"https://x/api/views/a.bin\","
                             "\"views\":[0,2],\"expires\":1793061000}}");
  TEST_ASSERT_TRUE(t.present);
  TEST_ASSERT_EQUAL(1, t.view);
  TEST_ASSERT_EQUAL_UINT32(1793060000, t.expires);
  TEST_ASSERT_EQUAL_UINT32(900, t.rates[0]);
  TEST_ASSERT_EQUAL_UINT32(914, t.rates[1]);
  TEST_ASSERT_EQUAL_UINT32(924, t.rates[2]);
  TEST_ASSERT_EQUAL_STRING("20261026-14", t.cacheId.c_str());
  TEST_ASSERT_EQUAL_STRING("https://x/api/views/a.bin", t.cacheUrl.c_str());
  TEST_ASSERT_EQUAL(2, t.cacheCount);
  TEST_ASSERT_EQUAL(0, t.cacheViews[0]);
  TEST_ASSERT_EQUAL(2, t.cacheViews[1]);
  TEST_ASSERT_EQUAL_UINT32(1793061000, t.cacheExpires);
}

void test_parse_absent_or_bad(void) {
  TEST_ASSERT_FALSE(parseTideInfo("").present);
  TEST_ASSERT_FALSE(parseTideInfo("not json").present);
  TEST_ASSERT_FALSE(parseTideInfo("{\"expires\":5}").present); // no view
  TEST_ASSERT_FALSE(parseTideInfo("{\"view\":3}").present);    // out of range
  TideInfo noCache = parseTideInfo("{\"view\":0}");
  TEST_ASSERT_TRUE(noCache.present);
  TEST_ASSERT_EQUAL(0, noCache.cacheCount);
  TEST_ASSERT_EQUAL_UINT32(0, noCache.rates[2]);
}

void test_parse_rejects_bad_cache_lists(void) {
  TEST_ASSERT_EQUAL(0, parseTideInfo("{\"view\":0,\"cache\":{\"id\":\"a\",\"url\":\"u\",\"views\":[1,7]}}").cacheCount);
  TEST_ASSERT_EQUAL(0, parseTideInfo("{\"view\":0,\"cache\":{\"id\":\"a\",\"url\":\"u\",\"views\":[0,1,2,1]}}").cacheCount);
  TEST_ASSERT_EQUAL(0, parseTideInfo("{\"view\":0,\"cache\":{\"id\":\"\",\"url\":\"u\",\"views\":[1]}}").cacheCount);
  TEST_ASSERT_EQUAL(0, parseTideInfo("{\"view\":0,\"cache\":{\"id\":\"a\",\"views\":[1]}}").cacheCount);
  TEST_ASSERT_EQUAL(1, parseTideInfo("{\"view\":0,\"cache\":{\"id\":\"a\",\"url\":\"u\",\"views\":[1]}}").cacheCount);
}

void test_view_for_button(void) {
  TEST_ASSERT_EQUAL(0, tideViewForButton("D1"));
  TEST_ASSERT_EQUAL(1, tideViewForButton("D2"));
  TEST_ASSERT_EQUAL(2, tideViewForButton("D4"));
  TEST_ASSERT_EQUAL(-1, tideViewForButton(""));
  TEST_ASSERT_EQUAL(-1, tideViewForButton("D3"));
}

void test_image_usable(void) {
  makeImage();
  TEST_ASSERT_TRUE(tideImageUsable(image, TIDE_BMP_SIZE, NOW, NOW + 60));
  TEST_ASSERT_FALSE(tideImageUsable(image, TIDE_BMP_SIZE, NOW, NOW));         // expired
  TEST_ASSERT_FALSE(tideImageUsable(image, TIDE_BMP_SIZE, 1000, 2000));       // clock never set
  TEST_ASSERT_FALSE(tideImageUsable(nullptr, TIDE_BMP_SIZE, NOW, NOW + 60));
  TEST_ASSERT_FALSE(tideImageUsable(image, TIDE_BMP_SIZE - 1, NOW, NOW + 60)); // cut short
  image[0] = 'P';
  TEST_ASSERT_FALSE(tideImageUsable(image, TIDE_BMP_SIZE, NOW, NOW + 60));
  makeImage();
  image[28] = 8;
  TEST_ASSERT_FALSE(tideImageUsable(image, TIDE_BMP_SIZE, NOW, NOW + 60));     // not 1 bit
}

void test_slot_paths(void) {
  TEST_ASSERT_EQUAL_STRING("/tide0.bmp", tideSlotPath(0));
  TEST_ASSERT_EQUAL_STRING("/tide2.bmp", tideSlotPath(2));
  TEST_ASSERT_NULL(tideSlotPath(-1));
  TEST_ASSERT_NULL(tideSlotPath(3));
}

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_parse_full_object);
  RUN_TEST(test_parse_absent_or_bad);
  RUN_TEST(test_parse_rejects_bad_cache_lists);
  RUN_TEST(test_view_for_button);
  RUN_TEST(test_image_usable);
  RUN_TEST(test_slot_paths);
  return UNITY_END();
}
