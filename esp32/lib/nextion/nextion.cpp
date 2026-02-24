#include "nextion.h"
#include "config.h"
#include <HardwareSerial.h>

// ============================================
// Static state
// ============================================

static char rxBuffer[NEXTION_BUFFER_SIZE];
static int bufferIndex = 0;
static unsigned long lastByteTime = 0;

// ============================================
// Initialization
// ============================================

void nextionInit() {
    Serial2.begin(115200, SERIAL_8N1, NEXTION_RX_PIN, NEXTION_TX_PIN);
    delay(100);
    Serial.println("[NXT] Initialized on RX=" + String(NEXTION_RX_PIN) +
                   " TX=" + String(NEXTION_TX_PIN));
}

// ============================================
// Send raw command with 0xFF 0xFF 0xFF terminator
// ============================================

void sendNextion(const String& cmd) {
    Serial2.print(cmd);
    Serial2.write(0xFF);
    Serial2.write(0xFF);
    Serial2.write(0xFF);
    Serial.println("[NXT] TX: " + cmd);
}

// ============================================
// Helper: set text field
// page.component.txt="value"
// ============================================

void setText(const String& page, const String& component, const String& value) {
    sendNextion(page + "." + component + ".txt=\"" + value + "\"");
}

// ============================================
// Helper: set visibility (current page only)
// vis component,0|1
// ============================================

void setVis(const String& component, bool visible) {
    sendNextion("vis " + component + "," + String(visible ? 1 : 0));
}

// ============================================
// Helper: set picture ID
// page.component.pic=picId
// ============================================

void setPic(const String& page, const String& component, int picId) {
    sendNextion(page + "." + component + ".pic=" + String(picId));
}

// ============================================
// Helper: switch to page
// page pageName
// ============================================

void setPage(const String& pageName) {
    sendNextion("page " + pageName);
}

// ============================================
// Helper: set sleep-on-no-touch timeout
// thsp=ms
// ============================================

void setThsp(int ms) {
    sendNextion("thsp=" + String(ms));
}

// ============================================
// Helper: set timer
// page.timer.tim=ms, page.timer.en=enable
// ============================================

void setTimer(const String& page, const String& timer, int ms, int enable) {
    sendNextion(page + "." + timer + ".tim=" + String(ms));
    sendNextion(page + "." + timer + ".en=" + String(enable));
}

// ============================================
// Helper: set background color (.bco)
// page.component.bco=color
// ============================================

void setBco(const String& page, const String& component, int color) {
    sendNextion(page + "." + component + ".bco=" + String(color));
}

// ============================================
// Consume trailing 0xFF bytes after system event
// ============================================

static void consumeTrailingFF() {
    unsigned long start = millis();
    int consumed = 0;
    while (consumed < 3 && millis() - start < 50) {
        if (Serial2.available()) {
            uint8_t b = Serial2.read();
            if (b == 0xFF) consumed++;
            else break;
        }
    }
}

// ============================================
// Read incoming Nextion command (non-blocking)
// ============================================

String readNextionCommand() {
    // Buffer timeout: clear stale incomplete data
    if (bufferIndex > 0 && millis() - lastByteTime > BUFFER_TIMEOUT_MS) {
        bufferIndex = 0;
        rxBuffer[0] = '\0';
    }

    while (Serial2.available()) {
        uint8_t b = Serial2.read();
        lastByteTime = millis();

        // Check for system sleep event (0x86)
        if (bufferIndex == 0 && b == 0x86) {
            consumeTrailingFF();
            return String(NEXTION_SLEEP_EVENT);
        }

        // Check for system wake event (0x87)
        if (bufferIndex == 0 && b == 0x87) {
            consumeTrailingFF();
            return String(NEXTION_WAKE_EVENT);
        }

        // Newline = end of ASCII command
        if (b == '\n') {
            if (bufferIndex > 0) {
                rxBuffer[bufferIndex] = '\0';
                String cmd = String(rxBuffer);
                cmd.trim();
                bufferIndex = 0;
                if (cmd.length() > 0) {
                    return cmd;
                }
            }
            continue;
        }

        // Skip carriage return
        if (b == '\r') continue;

        // Accumulate into buffer
        if (bufferIndex < NEXTION_BUFFER_SIZE - 1) {
            rxBuffer[bufferIndex++] = (char)b;
        } else {
            // Buffer overflow, discard
            Serial.println("[NXT] RX buffer overflow, clearing");
            bufferIndex = 0;
        }
    }

    return "";
}
