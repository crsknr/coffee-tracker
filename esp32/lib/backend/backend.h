#ifndef BACKEND_H
#define BACKEND_H

#include <Arduino.h>
#include <ArduinoJson.h>

// ============================================
// Response Structure
// ============================================

struct BackendResponse {
    bool success;
    int httpCode;
    JsonDocument doc;
};

// ============================================
// HTTP Functions
// ============================================

BackendResponse httpGet(const String& action, const String& params = "");
BackendResponse httpPost(JsonDocument& payload);

// ============================================
// Transaction ID
// ============================================

String generateTransactionId();

#endif
