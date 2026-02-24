#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#include <ArduinoJson.h>

// ============================================
// Initialization
// ============================================

bool storageInit();

// ============================================
// Config Cache (/config.json)
// ============================================

bool writeConfigCache(const String& jsonStr);
String readConfigCache();

// ============================================
// Offline Log (/offline.json)
// ============================================

bool appendOfflineLog(JsonDocument& payload);
bool readOfflineLog(JsonDocument& doc);
bool clearOfflineLog();
bool hasOfflineLog();

// ============================================
// Audit Log (/transactions.log)
// ============================================

bool appendAuditLog(const String& user, const String& type,
                    const String& items, const String& quantities,
                    const String& transactionId);

#endif
