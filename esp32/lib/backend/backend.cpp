#include "backend.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

// ============================================
// HTTP GET
// ============================================

BackendResponse httpGet(const String& action, const String& params) {
    BackendResponse resp;
    resp.success = false;
    resp.httpCode = -1;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = String(BACKEND_URL) + "?action=" + action;
    if (params.length() > 0) {
        url += "&" + params;
    }

    Serial.println("[NET] GET " + action + "...");
    unsigned long start = millis();

    http.begin(client, url);
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    resp.httpCode = http.GET();

    if (resp.httpCode == 200) {
        String payload = http.getString();
        DeserializationError err = deserializeJson(resp.doc, payload);
        if (err) {
            Serial.println("[NET] GET " + action + " JSON error: " + String(err.c_str()));
        } else {
            resp.success = true;
            Serial.printf("[NET] GET %s -> 200 OK (%lums)\n",
                          action.c_str(), millis() - start);
        }
    } else {
        Serial.printf("[NET] GET %s -> FAILED (HTTP %d, %lums)\n",
                      action.c_str(), resp.httpCode, millis() - start);
    }

    http.end();
    return resp;
}

// ============================================
// HTTP POST
// ============================================

BackendResponse httpPost(JsonDocument& payload) {
    BackendResponse resp;
    resp.success = false;
    resp.httpCode = -1;

    String payloadStr;
    serializeJson(payload, payloadStr);

    String type = payload["type"].as<String>();
    Serial.println("[NET] POST " + type + "...");
    unsigned long start = millis();

    // GAS POST flow (mirrors test-api.sh):
    //   Step 1 — POST to GAS URL without following redirect; capture Location header
    //   Step 2 — GET the redirect URL to retrieve the JSON response
    // Following the redirect as POST causes a 400 from Google's infrastructure.

    // Step 1: POST, capture redirect URL
    String redirectUrl;
    {
        WiFiClientSecure client;
        client.setInsecure();
        HTTPClient http;
        http.begin(client, BACKEND_URL);
        http.setTimeout(HTTP_TIMEOUT_MS);
        http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
        http.addHeader("Content-Type", "application/json");

        const char* locationHeader[] = {"Location"};
        http.collectHeaders(locationHeader, 1);

        int postCode = http.POST(payloadStr);
        redirectUrl = http.header("Location");
        http.end();

        if (redirectUrl.isEmpty()) {
            Serial.printf("[NET] POST %s -> no redirect (HTTP %d, %lums)\n",
                          type.c_str(), postCode, millis() - start);
            resp.httpCode = postCode;
            return resp;
        }
    }

    // Step 2: GET the redirect URL
    {
        WiFiClientSecure client;
        client.setInsecure();
        HTTPClient http;
        http.begin(client, redirectUrl);
        http.setTimeout(HTTP_TIMEOUT_MS);
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

        resp.httpCode = http.GET();

        if (resp.httpCode == 200) {
            String responseStr = http.getString();
            DeserializationError err = deserializeJson(resp.doc, responseStr);
            if (err) {
                Serial.println("[NET] POST " + type + " JSON error: " + String(err.c_str()));
            } else {
                resp.success = true;
                String status = resp.doc["status"].as<String>();
                Serial.printf("[NET] POST %s -> 200 OK (%s, %lums)\n",
                              type.c_str(), status.c_str(), millis() - start);
            }
        } else {
            String body = http.getString();
            Serial.printf("[NET] POST %s -> FAILED (HTTP %d, %lums): %s\n",
                          type.c_str(), resp.httpCode, millis() - start, body.c_str());
        }
        http.end();
    }

    return resp;
}

// ============================================
// Generate unique transaction ID
// Format: esp32-<millis>-<random_hex>
// ============================================

String generateTransactionId() {
    char buf[32];
    snprintf(buf, sizeof(buf), "esp32-%lu-%04lx",
             millis(), (unsigned long)(esp_random() & 0xFFFF));
    return String(buf);
}
