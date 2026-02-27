#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "config.h"
#include "state.h"
#include "nextion.h"
#include "backend.h"
#include "storage.h"
#include "pages.h"
#include "commands.h"
#include "audio.h"

// ============================================
// Forward declarations
// ============================================

static void syncNextionRTC();
static void handleTouchWake();
static void wakeDisplay();
static void tryReconnect();
static void replayOfflineLog();

// ============================================
// Setup
// ============================================

void setup() {
    Serial.begin(115200);
    // Give USB CDC time to enumerate (skipped if no host connected)
    unsigned long cdc_t = millis();
    while (!Serial && millis() - cdc_t < 3000) delay(10);
    Serial.println("\n=== Coffee Tracker " FIRMWARE_VERSION " ===");

    // Initialize Nextion display
    nextionInit();
    delay(100);

    // Enable auto-wake on touch
    sendNextion("thup=1");

    // Initialize SD card
    storageInit();

    // Show boot page
    showBootPage();

    // Step 1: WiFi
    setBootStatus("Connecting to WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long wifiStart = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - wifiStart < WIFI_CONNECT_TIMEOUT_MS) {
        delay(100);
    }

    if (WiFi.status() == WL_CONNECTED) {
        connectionState = ONLINE;
        Serial.println("[BOOT] WiFi connected: " + WiFi.localIP().toString());

        // Step 2: Clock sync
        setBootStatus("Syncing clock...");
        configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org");
        syncNextionRTC();

        // Step 3: Fetch data
        setBootStatus("Fetching data...");
        BackendResponse resp = httpGet("getConfig");

        if (resp.success) {
            parseConfig(resp.doc);

            // Cache to SD
            String cacheStr;
            serializeJson(resp.doc, cacheStr);
            writeConfigCache(cacheStr);

            lastSyncTime = millis();
        } else {
            // Backend unreachable but WiFi connected: try SD cache
            Serial.println("[BOOT] Backend failed, trying SD cache...");
            String cached = readConfigCache();
            if (cached.length() > 0) {
                JsonDocument cacheDoc;
                if (!deserializeJson(cacheDoc, cached)) {
                    parseConfig(cacheDoc);
                    Serial.println("[BOOT] Loaded config from SD cache");
                }
            } else {
                setBootStatus("No data \xe2\x80\x93 connect to WiFi");
                // Stay on boot page, retry loop
                while (true) {
                    delay(RECONNECT_INTERVAL_MS);
                    WiFi.reconnect();
                    unsigned long start = millis();
                    while (WiFi.status() != WL_CONNECTED &&
                           millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
                        delay(100);
                    }
                    if (WiFi.status() == WL_CONNECTED) {
                        BackendResponse r = httpGet("getConfig");
                        if (r.success) {
                            parseConfig(r.doc);
                            String cs;
                            serializeJson(r.doc, cs);
                            writeConfigCache(cs);
                            lastSyncTime = millis();
                            connectionState = ONLINE;
                            break;
                        }
                    }
                }
            }
        }

        // Step 4: Ready
        setBootStatus("Ready");
        delay(BOOT_READY_DELAY_MS);

    } else {
        // WiFi failed
        connectionState = OFFLINE;
        Serial.println("[BOOT] WiFi failed");

        String cached = readConfigCache();
        if (cached.length() > 0) {
            setBootStatus("Offline \xe2\x80\x93 using cached data");
            JsonDocument cacheDoc;
            if (!deserializeJson(cacheDoc, cached)) {
                parseConfig(cacheDoc);
                Serial.println("[BOOT] Loaded config from SD cache");
            }
            delay(BOOT_READY_DELAY_MS);
        } else {
            setBootStatus("No data \xe2\x80\x93 connect to WiFi");
            // Retry loop until WiFi + backend succeeds
            while (true) {
                delay(RECONNECT_INTERVAL_MS);
                WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
                unsigned long start = millis();
                while (WiFi.status() != WL_CONNECTED &&
                       millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
                    delay(100);
                }
                if (WiFi.status() == WL_CONNECTED) {
                    connectionState = ONLINE;
                    setBootStatus("Syncing clock...");
                    configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org");
                    syncNextionRTC();

                    setBootStatus("Fetching data...");
                    BackendResponse r = httpGet("getConfig");
                    if (r.success) {
                        parseConfig(r.doc);
                        String cs;
                        serializeJson(r.doc, cs);
                        writeConfigCache(cs);
                        lastSyncTime = millis();
                        setBootStatus("Ready");
                        delay(BOOT_READY_DELAY_MS);
                        break;
                    }
                }
            }
        }
    }

    // Step 5: Show user page
    userPageIndex = 0;
    displayState = AWAKE;
    showUserPage();
}

// ============================================
// Main Loop
// ============================================

void loop() {
    switch (displayState) {

    case AWAKE: {
        String cmd = readNextionCommand();

        if (cmd == NEXTION_SLEEP_EVENT) {
            displayState = SLEEPING;
            Serial.println("[SLEEP] Display entered sleep");
            if (ENABLE_AUDIO_WAKE) startAudio();
            return;
        }

        if (cmd.length() > 0) {
            handleCommand(cmd);
        }

        // Serial commands (for calibration / debugging)
        if (Serial.available()) {
            String serialCmd = Serial.readStringUntil('\n');
            serialCmd.trim();
            if (serialCmd == "RECORD_REF") {
                recordReference();
            }
        }

        // Audio diagnostics page: push live xcorr and threshold to waveform graph
        if (currentPage == PAGE_AUDIO && ENABLE_AUDIO_WAKE) {
            processAudioFrame();  // updates smoothedEnergy; detection return ignored
            uint8_t threshVal = (uint8_t)(getXcorrThreshold() * 255.0f);
            uint8_t energyVal = (uint8_t)(min(1.0f, getSmoothedEnergy()) * 255.0f);
            addWaveformPoint(13, 0, threshVal);  // channel 0 = threshold line
            addWaveformPoint(13, 1, energyVal);  // channel 1 = live xcorr energy
        }

        // Periodic reconnect when offline
        if (connectionState == OFFLINE &&
            millis() - lastReconnectAttempt > RECONNECT_INTERVAL_MS) {
            lastReconnectAttempt = millis();
            tryReconnect();
        }
        break;
    }

    case SLEEPING: {
        // Check for grinder detection
        if (ENABLE_AUDIO_WAKE && processAudioFrame()) {
            Serial.println("[AUDIO] Grinder detected, waking display");
            stopAudio();
            wakeDisplay();
            return;
        }

        // Check for touch wake
        String event = readNextionCommand();
        if (event == NEXTION_WAKE_EVENT) {
            Serial.println("[WAKE] Touch wake");
            if (ENABLE_AUDIO_WAKE) stopAudio();
            handleTouchWake();
            return;
        }
        break;
    }
    }

    delay(5);
}

// ============================================
// Sync Nextion RTC from NTP
// ============================================

static void syncNextionRTC() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 5000)) {
        Serial.println("[BOOT] NTP sync failed");
        return;
    }
    sendNextion("rtc0=" + String(timeinfo.tm_year + 1900));
    sendNextion("rtc1=" + String(timeinfo.tm_mon + 1));
    sendNextion("rtc2=" + String(timeinfo.tm_mday));
    sendNextion("rtc3=" + String(timeinfo.tm_hour));
    sendNextion("rtc4=" + String(timeinfo.tm_min));
    sendNextion("rtc5=" + String(timeinfo.tm_sec));
    Serial.printf("[BOOT] RTC synced: %04d-%02d-%02d %02d:%02d:%02d\n",
                  timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
}

// ============================================
// Touch wake handler
// ============================================

static void handleTouchWake() {
    displayState = AWAKE;

    // Refresh user data if online
    if (connectionState == ONLINE) {
        BackendResponse resp = httpGet("getUsers");
        if (resp.success) {
            parseUsers(resp.doc);
        }
    }

    showUserPage();
}

// ============================================
// Grinder wake handler
// ============================================

static void wakeDisplay() {
    sendNextion("sleep=0");
    displayState = AWAKE;

    // Refresh user data if online
    if (connectionState == ONLINE) {
        BackendResponse resp = httpGet("getUsers");
        if (resp.success) {
            parseUsers(resp.doc);
        }
    }

    showUserPage();
}

// ============================================
// Reconnect attempt (called periodically when offline)
// ============================================

static void tryReconnect() {
    Serial.println("[NET] Reconnect attempt...");

    if (WiFi.status() != WL_CONNECTED) {
        WiFi.reconnect();
        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED &&
               millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
            delay(100);
        }
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[NET] WiFi reconnect failed");
            return;
        }
        Serial.println("[NET] WiFi reconnected");
    }

    // Try a lightweight request to verify backend
    BackendResponse resp = httpGet("getUsers");
    if (!resp.success) {
        Serial.println("[NET] Backend still unreachable");
        return;
    }

    // Success: go online
    connectionState = ONLINE;
    parseUsers(resp.doc);
    Serial.println("[NET] Back online!");

    // Full sync
    BackendResponse configResp = httpGet("getConfig");
    if (configResp.success) {
        parseConfig(configResp.doc);
        String cacheStr;
        serializeJson(configResp.doc, cacheStr);
        writeConfigCache(cacheStr);
        lastSyncTime = millis();
    }

    // Replay offline log
    replayOfflineLog();

    // Update status bar on current page if on user page
    if (currentPage == PAGE_USER) {
        showUserPage();
    }
}

// ============================================
// Replay offline log entries
// ============================================

static void replayOfflineLog() {
    if (!hasOfflineLog()) return;

    Serial.println("[NET] Replaying offline log...");
    JsonDocument offlineDoc;
    if (!readOfflineLog(offlineDoc)) return;

    JsonArray entries = offlineDoc.as<JsonArray>();
    int replayed = 0;

    for (JsonObject entry : entries) {
        JsonDocument entryDoc;
        for (JsonPair kv : entry) {
            entryDoc[kv.key()] = kv.value();
        }
        BackendResponse r = httpPost(entryDoc);
        if (r.success) {
            String s = r.doc["status"].as<String>();
            if (s == "success" || s == "duplicate") {
                replayed++;
            }
        }
        // On error: skip entry (don't retry malformed requests)
    }

    clearOfflineLog();
    Serial.printf("[NET] Replayed %d/%d offline entries\n",
                  replayed, (int)entries.size());
}
