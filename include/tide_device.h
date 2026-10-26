#pragma once

// Tide-charts screens kept in flash on the OG DIY Kit (TIDE_BUTTONS builds only). The screen that is
// up lives in /current.bmp as usual; the other two live in /tide<N>.bmp. A key press shows its
// screen from flash without Wi-Fi; check-ins keep the screens fresh. See tide_views.h for the
// "tide" object the Worker sends with /api/display.

#ifdef TIDE_BUTTONS
#include <Arduino.h>
#include <Preferences.h>

// Key press: draw the stored screen for `view` and set the refresh rate that keeps it up. False if
// it isn't stored, is malformed or too old (the caller then fetches it online instead).
bool tideShowStored(Preferences &prefs, int view);

// Check-in, before Wi-Fi goes off: once an hour (when the Worker's cache id changes) download the
// other screens in one request into their flash files.
void tideFetchScreens(Preferences &prefs, const String &tideJson);

// Check-in, just before the new image overwrites /current.bmp: keep the old one as its screen's
// file if the screen changes, and remember which screen /current.bmp now holds.
void tideNewCurrent(Preferences &prefs, const String &tideJson);
#endif
