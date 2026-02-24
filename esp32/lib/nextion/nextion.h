#ifndef NEXTION_H
#define NEXTION_H

#include <Arduino.h>

// System event identifiers returned by readNextionCommand()
#define NEXTION_SLEEP_EVENT "NEXTION_SLEEP"
#define NEXTION_WAKE_EVENT  "NEXTION_WAKE"

// ============================================
// Initialization
// ============================================

void nextionInit();

// ============================================
// Send Commands
// ============================================

void sendNextion(const String& cmd);
void setText(const String& page, const String& component, const String& value);
void setVis(const String& component, bool visible);  // current page only
void setPic(const String& page, const String& component, int picId);
void setPage(const String& pageName);
void setThsp(int ms);
void setTimer(const String& page, const String& timer, int ms, int enable);
void setBco(const String& page, const String& component, int color);

// ============================================
// Receive Commands
// ============================================

// Returns command string, NEXTION_SLEEP_EVENT, NEXTION_WAKE_EVENT, or ""
String readNextionCommand();

#endif
