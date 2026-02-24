#include "state.h"

// ============================================
// Global State Definitions
// ============================================

AppConfig appConfig;
DisplayState displayState = AWAKE;
ConnectionState connectionState = OFFLINE;
Page currentPage = PAGE_BOOT;
String currentUser = "";
int userPageIndex = 0;

String lastTransactionType = "";
String lastTransactionId = "";
float lastTransactionAmount = 0;

unsigned long lastReconnectAttempt = 0;
unsigned long lastSyncTime = 0;
bool sdAvailable = false;

// ============================================
// Parse full getConfig response into appConfig
// ============================================

void parseConfig(JsonDocument& doc) {
    // Parse users
    JsonArray usersArr = doc["users"].as<JsonArray>();
    appConfig.userCount = 0;
    for (JsonObject u : usersArr) {
        if (appConfig.userCount >= MAX_USERS) break;
        appConfig.users[appConfig.userCount].name = u["name"].as<String>();
        appConfig.users[appConfig.userCount].balance = u["balance"].as<float>();
        appConfig.users[appConfig.userCount].isTreasurer = u["isTreasurer"].as<bool>();
        appConfig.userCount++;
    }

    // Parse products
    JsonArray productsArr = doc["products"].as<JsonArray>();
    appConfig.productCount = 0;
    for (JsonObject p : productsArr) {
        if (appConfig.productCount >= MAX_PRODUCTS) break;
        appConfig.products[appConfig.productCount].name = p["name"].as<String>();
        appConfig.products[appConfig.productCount].price = p["price"].as<String>();
        appConfig.productCount++;
    }

    // Parse inventory
    JsonArray invArr = doc["inventory"].as<JsonArray>();
    appConfig.inventoryCount = 0;
    for (JsonObject inv : invArr) {
        if (appConfig.inventoryCount >= MAX_INVENTORY) break;
        appConfig.inventory[appConfig.inventoryCount].item = inv["item"].as<String>();
        appConfig.inventory[appConfig.inventoryCount].price = inv["price"].as<String>();
        appConfig.inventory[appConfig.inventoryCount].stock = inv["stock"].as<String>();
        appConfig.inventory[appConfig.inventoryCount].status = inv["status"].as<String>();
        appConfig.inventoryCount++;
    }

    // Parse config
    JsonObject cfg = doc["config"].as<JsonObject>();
    appConfig.config.treasurer = cfg["treasurer"].as<String>();
    appConfig.config.paypalLink = cfg["paypalLink"].as<String>();
    appConfig.config.balanceThreshold = cfg["balanceThreshold"].as<float>();
    appConfig.config.criticalInventoryItem = cfg["criticalInventoryItem"].as<String>();
}

// ============================================
// Parse getUsers response (lightweight refresh)
// ============================================

void parseUsers(JsonDocument& doc) {
    JsonArray usersArr = doc["users"].as<JsonArray>();
    appConfig.userCount = 0;
    for (JsonObject u : usersArr) {
        if (appConfig.userCount >= MAX_USERS) break;
        appConfig.users[appConfig.userCount].name = u["name"].as<String>();
        appConfig.users[appConfig.userCount].balance = u["balance"].as<float>();
        appConfig.users[appConfig.userCount].isTreasurer = u["isTreasurer"].as<bool>();
        appConfig.userCount++;
    }

    if (!doc["criticalInventoryItem"].isNull()) {
        appConfig.config.criticalInventoryItem = doc["criticalInventoryItem"].as<String>();
    }
}

// ============================================
// Find user by name
// ============================================

User* findUser(const String& name) {
    for (int i = 0; i < appConfig.userCount; i++) {
        if (appConfig.users[i].name == name) {
            return &appConfig.users[i];
        }
    }
    return nullptr;
}

// ============================================
// Format balance for display
// ============================================

String formatBalance(float balance) {
    if (connectionState == OFFLINE) {
        return "offline";
    }
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f\xe2\x82\xac", balance);
    return String(buf);
}

// ============================================
// Parse numeric value from price string
// e.g. "0.30\u20ac" -> 0.30, "30\u20ac / 1kg" -> 30.0
// ============================================

float parsePrice(const String& priceStr) {
    String num = "";
    bool foundDot = false;
    for (unsigned int i = 0; i < priceStr.length(); i++) {
        char c = priceStr.charAt(i);
        if (c >= '0' && c <= '9') {
            num += c;
        } else if (c == '.' && !foundDot) {
            num += c;
            foundDot = true;
        } else if (num.length() > 0) {
            break;
        }
    }
    return num.toFloat();
}
