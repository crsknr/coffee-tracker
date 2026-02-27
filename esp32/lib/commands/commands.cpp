#include "commands.h"
#include "config.h"
#include "state.h"
#include "nextion.h"
#include "backend.h"
#include "storage.h"
#include "pages.h"
#include "audio.h"
#include <ArduinoJson.h>
#include <math.h>

// ============================================
// Utility: split string by delimiter
// ============================================

// Nextion may send quantities as raw bytes (e.g. from chr(n.val)) or ASCII digits.
// This handles both: if the string is a single control byte, use its value directly.
static int parseQty(const String &s)
{
    if (s.length() == 1 && (uint8_t)s.charAt(0) < 0x20)
    {
        return (uint8_t)s.charAt(0);
    }
    return s.toInt();
}

static int splitString(const String &str, char delimiter, String *parts, int maxParts)
{
    int count = 0;
    int start = 0;
    for (int i = 0; i <= (int)str.length() && count < maxParts; i++)
    {
        if (i == (int)str.length() || str.charAt(i) == delimiter)
        {
            parts[count++] = str.substring(start, i);
            start = i + 1;
        }
    }
    return count;
}

// ============================================
// Handle consume confirm
// ============================================

static void handleConsumeConfirm(const String &data)
{
    // Parse: username,single,double,cow,oat
    // Nextion sends \x00 for zero quantities; String truncates at null, so we
    // may receive fewer than 5 fields — treat missing fields as 0.
    String params[5];
    splitString(data, ',', params, 5);
    if (params[0].length() == 0)
    {
        Serial.println("[CMD] Invalid CONSUME:CONFIRM: missing username");
        return;
    }

    String username = params[0];
    int quantities[] = {
        parseQty(params[1]), // single
        parseQty(params[2]), // double
        parseQty(params[3]), // cow
        parseQty(params[4])  // oat
    };

    const char *itemNames[] = {"Single", "Double", "Cow Milk Portion", "Oat Milk Portion"};

    // Calculate display amount from cached prices
    float amount = 0;
    for (int i = 0; i < 4 && i < appConfig.productCount; i++)
    {
        if (quantities[i] > 0)
        {
            amount += quantities[i] * parsePrice(appConfig.products[i].price);
        }
    }

    // Generate transaction ID
    String txId = generateTransactionId();
    lastTransactionType = "consume";
    lastTransactionId = txId;
    lastTransactionAmount = amount;

    // Show logging page
    showLoggingPage("consume", username, amount);

    // Build items and quantities arrays for POST
    JsonDocument payload;
    payload["user"] = username;
    payload["type"] = "consume";
    JsonArray itemsArr = payload["items"].to<JsonArray>();
    JsonArray qtyArr = payload["quantities"].to<JsonArray>();

    String itemsStr = "";
    String qtyStr = "";

    for (int i = 0; i < 4; i++)
    {
        if (quantities[i] > 0)
        {
            itemsArr.add(itemNames[i]);
            qtyArr.add(quantities[i]);
            if (itemsStr.length() > 0)
            {
                itemsStr += ",";
                qtyStr += ",";
            }
            itemsStr += itemNames[i];
            qtyStr += String(quantities[i]);
        }
    }
    payload["transactionId"] = txId;

    // Write to audit log
    appendAuditLog(username, "consume", itemsStr, qtyStr, txId);

    // Attempt POST
    BackendResponse resp = httpPost(payload);

    if (resp.success)
    {
        String status = resp.doc["status"].as<String>();
        if (status == "success" || status == "duplicate")
        {
            float newBalance = resp.doc["balance"].as<float>();
            // Update cached user balance
            User *user = findUser(username);
            if (user)
                user->balance = newBalance;
            // Update critical inventory item
            if (!resp.doc["criticalInventoryItem"].isNull())
            {
                appConfig.config.criticalInventoryItem =
                    resp.doc["criticalInventoryItem"].as<String>();
            }
            showLoggedPage("consume", username, newBalance, amount);
            return;
        }
    }

    // POST failed: write to offline log
    appendOfflineLog(payload);
    connectionState = OFFLINE;
    showLoggedPage("consume", username, 0, amount);
}

// ============================================
// Handle purchase confirm
// ============================================

static void handlePurchaseConfirm(const String &data)
{
    // Parse: username,beans,water,cow,oat
    // Treat missing fields as 0 (same null-byte truncation as consume).
    String params[5];
    splitString(data, ',', params, 5);
    if (params[0].length() == 0)
    {
        Serial.println("[CMD] Invalid PURCHASE:CONFIRM: missing username");
        return;
    }

    String username = params[0];
    int quantities[] = {
        parseQty(params[1]), // beans
        parseQty(params[2]), // water
        parseQty(params[3]), // cow
        parseQty(params[4])  // oat
    };

    const char *itemNames[] = {"Beans", "Water", "Cow Milk Carton", "Oat Milk Carton"};

    // Calculate display amount from cached inventory prices
    float amount = 0;
    for (int i = 0; i < 4 && i < appConfig.inventoryCount; i++)
    {
        if (quantities[i] > 0)
        {
            amount += quantities[i] * parsePrice(appConfig.inventory[i].price);
        }
    }

    // Generate transaction ID
    String txId = generateTransactionId();
    lastTransactionType = "purchase";
    lastTransactionId = txId;
    lastTransactionAmount = amount;

    // Show logging page
    showLoggingPage("purchase", username, amount);

    // Build POST payload
    JsonDocument payload;
    payload["user"] = username;
    payload["type"] = "purchase";
    JsonArray itemsArr = payload["items"].to<JsonArray>();
    JsonArray qtyArr = payload["quantities"].to<JsonArray>();

    String itemsStr = "";
    String qtyStr = "";

    for (int i = 0; i < 4; i++)
    {
        if (quantities[i] > 0)
        {
            itemsArr.add(itemNames[i]);
            qtyArr.add(quantities[i]);
            if (itemsStr.length() > 0)
            {
                itemsStr += ",";
                qtyStr += ",";
            }
            itemsStr += itemNames[i];
            qtyStr += String(quantities[i]);
        }
    }
    payload["transactionId"] = txId;

    // Write to audit log
    appendAuditLog(username, "purchase", itemsStr, qtyStr, txId);

    // Attempt POST
    BackendResponse resp = httpPost(payload);

    if (resp.success)
    {
        String status = resp.doc["status"].as<String>();
        if (status == "success" || status == "duplicate")
        {
            float newBalance = resp.doc["balance"].as<float>();
            User *user = findUser(username);
            if (user)
                user->balance = newBalance;
            if (!resp.doc["criticalInventoryItem"].isNull())
            {
                appConfig.config.criticalInventoryItem =
                    resp.doc["criticalInventoryItem"].as<String>();
            }
            showLoggedPage("purchase", username, newBalance, amount);
            return;
        }
    }

    // POST failed
    appendOfflineLog(payload);
    connectionState = OFFLINE;
    showLoggedPage("purchase", username, 0, amount);
}

// ============================================
// Handle undo
// ============================================

static void handleUndo(const String &username)
{
    String txId = generateTransactionId();
    float amount = lastTransactionAmount;

    // Show logging page
    showLoggingPage("undo", username, amount);

    // Write to audit log
    appendAuditLog(username, "undo", lastTransactionType, "", txId);

    // Build POST payload
    JsonDocument payload;
    payload["user"] = username;
    payload["type"] = "undo";
    payload["transactionId"] = txId;

    // Attempt POST
    BackendResponse resp = httpPost(payload);

    if (resp.success)
    {
        String status = resp.doc["status"].as<String>();
        if (status == "success" || status == "duplicate")
        {
            float newBalance = resp.doc["balance"].as<float>();
            User *user = findUser(username);
            if (user)
                user->balance = newBalance;
            if (!resp.doc["criticalInventoryItem"].isNull())
            {
                appConfig.config.criticalInventoryItem =
                    resp.doc["criticalInventoryItem"].as<String>();
            }
            lastTransactionType = "undo";
            lastTransactionId = txId;
            showLoggedPage("undo", username, newBalance, amount);
            return;
        }
    }

    // POST failed
    appendOfflineLog(payload);
    connectionState = OFFLINE;
    showLoggedPage("undo", username, 0, amount);
}

// ============================================
// Handle settle (PayPal)
// ============================================

static void handleSettle(const String &username)
{
    // Settle is blocked offline
    if (connectionState == OFFLINE)
    {
        Serial.println("[CMD] Settle blocked: offline");
        return;
    }

    User *user = findUser(username);
    if (!user)
        return;

    float amount = fabs(user->balance);
    String txId = generateTransactionId();

    lastTransactionType = "settle";
    lastTransactionId = txId;
    lastTransactionAmount = amount;

    // Show logging page
    showLoggingPage("settle", username, amount);

    // Write to audit log
    appendAuditLog(username, "settle", "PayPal", String(amount, 2), txId);

    // Build POST payload
    JsonDocument payload;
    payload["user"] = username;
    payload["type"] = "settle";
    payload["amount"] = amount;
    payload["transactionId"] = txId;

    // Attempt POST
    BackendResponse resp = httpPost(payload);

    if (resp.success)
    {
        String status = resp.doc["status"].as<String>();
        if (status == "success" || status == "duplicate")
        {
            float newBalance = resp.doc["balance"].as<float>();
            User *u = findUser(username);
            if (u)
                u->balance = newBalance;
            showLoggedPage("settle", username, newBalance, amount);
            return;
        }
    }

    // Settle failed: do NOT write to offline log, return to user page
    Serial.println("[CMD] Settle POST failed, returning to user page");
    showUserPage();
}

// ============================================
// Handle sync (INFO:SYNC)
// ============================================

static void handleSync()
{
    // Show backend activity via timer
    startBackendAnim();

    BackendResponse resp = httpGet("getConfig");

    if (resp.success)
    {
        // Update config
        parseConfig(resp.doc);

        // Write config cache to SD
        String cacheStr;
        serializeJson(resp.doc, cacheStr);
        writeConfigCache(cacheStr);

        // Sync Nextion RTC
        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 5000))
        {
            sendNextion("rtc0=" + String(timeinfo.tm_year + 1900));
            sendNextion("rtc1=" + String(timeinfo.tm_mon + 1));
            sendNextion("rtc2=" + String(timeinfo.tm_mday));
            sendNextion("rtc3=" + String(timeinfo.tm_hour));
            sendNextion("rtc4=" + String(timeinfo.tm_min));
            sendNextion("rtc5=" + String(timeinfo.tm_sec));
        }

        connectionState = ONLINE;
        lastSyncTime = millis();

        // Replay offline log if exists
        if (hasOfflineLog())
        {
            Serial.println("[CMD] Replaying offline log...");
            JsonDocument offlineDoc;
            if (readOfflineLog(offlineDoc))
            {
                JsonArray entries = offlineDoc.as<JsonArray>();
                int replayed = 0;
                for (JsonObject entry : entries)
                {
                    JsonDocument entryDoc;
                    for (JsonPair kv : entry)
                    {
                        entryDoc[kv.key()] = kv.value();
                    }
                    BackendResponse r = httpPost(entryDoc);
                    if (r.success)
                    {
                        String s = r.doc["status"].as<String>();
                        if (s == "success" || s == "duplicate")
                        {
                            replayed++;
                        }
                    }
                }
                Serial.printf("[CMD] Replayed %d offline entries\n", replayed);
            }
            clearOfflineLog();
        }

        setText("info", "last_sync", "Just now");
    }
    else
    {
        setText("info", "last_sync", "Sync failed");
    }

    stopBackendAnim();
}

// ============================================
// Main command dispatcher
// ============================================

void handleCommand(const String &command)
{
    Serial.println("[CMD] " + command);

    // Split by ':'
    String parts[6];
    int partCount = splitString(command, ':', parts, 6);

    if (partCount < 3 || parts[0] != "PAGE")
    {
        Serial.println("[CMD] Invalid command format");
        return;
    }

    String page = parts[1];
    String action = parts[2];
    String data = (partCount > 3) ? parts[3] : "";

    // --- USER page ---
    if (page == "USER")
    {
        if (action == "SELECT" && data.length() > 0)
        {
            currentUser = data;
            showConsumePage(currentUser);
        }
        else if (action == "INVENTORY")
        {
            if (connectionState == OFFLINE)
            {
                Serial.println("[CMD] Inventory blocked: offline");
                return;
            }
            showInventoryPage();
        }
        else if (action == "INFO")
        {
            showInfoPage();
        }
        else if (action == "NAV")
        {
            if (data == "next")
            {
                userPageIndex++;
            }
            else if (data == "prev")
            {
                userPageIndex--;
            }
            showUserPage();
        }
    }

    // --- CONSUME page ---
    else if (page == "CONSUME")
    {
        if (action == "CONFIRM" && data.length() > 0)
        {
            handleConsumeConfirm(data);
        }
        else if (action == "BACK")
        {
            showUserPage();
        }
        else if (action == "ACCOUNT" && data.length() > 0)
        {
            if (connectionState == OFFLINE)
            {
                Serial.println("[CMD] Account blocked: offline");
                return;
            }
            showAccountPage(data);
        }
        else if (action == "PURCHASE")
        {
            showPurchasePage(data.length() > 0 ? data : currentUser);
        }
    }

    // --- PURCHASE page ---
    else if (page == "PURCHASE")
    {
        if (action == "CONFIRM" && data.length() > 0)
        {
            handlePurchaseConfirm(data);
        }
        else if (action == "BACK")
        {
            showConsumePage(currentUser);
        }
    }

    // --- LOGGED page ---
    else if (page == "LOGGED")
    {
        if (action == "UNDO")
        {
            handleUndo(currentUser);
        }
        else if (action == "BACK")
        {
            showUserPage();
        }
        else if (action == "TIMEOUT")
        {
            if (lastTransactionType == "consume")
            {
                User *user = findUser(currentUser);
                if (user && user->balance < appConfig.config.balanceThreshold)
                {
                    showBalancePage(currentUser);
                    return;
                }
            }
            showUserPage();
        }
    }

    // --- BALANCE page ---
    else if (page == "BALANCE")
    {
        if (action == "PAYPAL" && data.length() > 0)
        {
            handleSettle(data);
        }
        else if (action == "BACK" || action == "TIMEOUT")
        {
            showUserPage();
        }
    }

    // --- ACCOUNT page ---
    else if (page == "ACCOUNT")
    {
        if (action == "BALANCE" && data.length() > 0)
        {
            showBalancePage(data);
        }
        else if (action == "BACK")
        {
            showConsumePage(currentUser);
        }
    }

    // --- INVENTORY page ---
    else if (page == "INVENTORY")
    {
        if (action == "BACK")
        {
            showUserPage();
        }
    }

    // --- INFO page ---
    else if (page == "INFO")
    {
        if (action == "BACK")
        {
            showUserPage();
        }
        else if (action == "SYNC")
        {
            handleSync();
        }
        else if (action == "AUDIO")
        {
            showAudioPage();
        }
    }

    // --- AUDIO page ---
    else if (page == "AUDIO")
    {
        if (action == "BACK")
        {
            if (ENABLE_AUDIO_WAKE) stopAudio();
            showInfoPage();
        }
        else if (action == "RECORD")
        {
            recordReference();
        }
        else if (action == "XCORR+")
        {
            adjustXcorrThreshold(+0.05f);
            char buf[8];
            snprintf(buf, sizeof(buf), "%.2f", getXcorrThreshold());
            setText("audio", "threshold_val", String(buf));
        }
        else if (action == "XCORR-")
        {
            adjustXcorrThreshold(-0.05f);
            char buf[8];
            snprintf(buf, sizeof(buf), "%.2f", getXcorrThreshold());
            setText("audio", "threshold_val", String(buf));
        }
    }
}
