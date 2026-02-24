#include "storage.h"
#include "config.h"
#include "state.h"
#include <SD.h>
#include <SPI.h>
#include <time.h>

// ============================================
// Initialization
// ============================================

bool storageInit() {
    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    sdAvailable = SD.begin(SD_CS_PIN);
    if (!sdAvailable) {
        Serial.println("[SD] Card mount failed");
    } else {
        Serial.println("[SD] Card mounted OK");
    }
    return sdAvailable;
}

// ============================================
// Config Cache
// ============================================

bool writeConfigCache(const String& jsonStr) {
    if (!sdAvailable) return false;

    File f = SD.open(CONFIG_CACHE_PATH, FILE_WRITE);
    if (!f) {
        Serial.println("[SD] Failed to open config cache for writing");
        return false;
    }
    f.print(jsonStr);
    f.close();
    Serial.printf("[SD] Config cache written (%d bytes)\n", jsonStr.length());
    return true;
}

String readConfigCache() {
    if (!sdAvailable) return "";

    File f = SD.open(CONFIG_CACHE_PATH, FILE_READ);
    if (!f) {
        Serial.println("[SD] No config cache found");
        return "";
    }
    String content = f.readString();
    f.close();
    Serial.printf("[SD] Config cache read (%d bytes)\n", content.length());
    return content;
}

// ============================================
// Offline Log
// ============================================

bool appendOfflineLog(JsonDocument& payload) {
    if (!sdAvailable) return false;

    // Read existing array or create new one
    JsonDocument arrDoc;
    JsonArray arr;

    if (SD.exists(OFFLINE_LOG_PATH)) {
        File f = SD.open(OFFLINE_LOG_PATH, FILE_READ);
        if (f) {
            DeserializationError err = deserializeJson(arrDoc, f);
            f.close();
            if (!err && arrDoc.is<JsonArray>()) {
                arr = arrDoc.as<JsonArray>();
            } else {
                arrDoc.clear();
                arr = arrDoc.to<JsonArray>();
            }
        } else {
            arr = arrDoc.to<JsonArray>();
        }
    } else {
        arr = arrDoc.to<JsonArray>();
    }

    // Add the new payload to the array
    JsonObject entry = arr.add<JsonObject>();
    for (JsonPair kv : payload.as<JsonObject>()) {
        entry[kv.key()] = kv.value();
    }

    // Write back
    File f = SD.open(OFFLINE_LOG_PATH, FILE_WRITE);
    if (!f) {
        Serial.println("[SD] Failed to open offline log for writing");
        return false;
    }
    serializeJson(arrDoc, f);
    f.close();

    Serial.printf("[SD] Offline log: %d entries pending\n", arr.size());
    return true;
}

bool readOfflineLog(JsonDocument& doc) {
    if (!sdAvailable) return false;

    File f = SD.open(OFFLINE_LOG_PATH, FILE_READ);
    if (!f) return false;

    DeserializationError err = deserializeJson(doc, f);
    f.close();
    return !err;
}

bool clearOfflineLog() {
    if (!sdAvailable) return false;
    if (SD.exists(OFFLINE_LOG_PATH)) {
        SD.remove(OFFLINE_LOG_PATH);
        Serial.println("[SD] Offline log cleared");
    }
    return true;
}

bool hasOfflineLog() {
    if (!sdAvailable) return false;
    return SD.exists(OFFLINE_LOG_PATH);
}

// ============================================
// Audit Log (JSONL append-only)
// ============================================

bool appendAuditLog(const String& user, const String& type,
                    const String& items, const String& quantities,
                    const String& transactionId) {
    if (!sdAvailable) return false;

    File f = SD.open(TRANSACTION_LOG_PATH, FILE_APPEND);
    if (!f) {
        Serial.println("[SD] Failed to open audit log");
        return false;
    }

    // Get timestamp
    String ts = "";
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 0)) {
        char buf[24];
        strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &timeinfo);
        ts = String(buf);
    } else {
        ts = "ms-" + String(millis());
    }

    // Build JSONL line
    JsonDocument lineDoc;
    lineDoc["ts"] = ts;
    lineDoc["user"] = user;
    lineDoc["type"] = type;
    lineDoc["items"] = items;
    lineDoc["quantities"] = quantities;
    lineDoc["tid"] = transactionId;

    serializeJson(lineDoc, f);
    f.println();
    f.close();
    return true;
}
