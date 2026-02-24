#ifndef STATE_H
#define STATE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"

// ============================================
// Data Structures
// ============================================

struct User {
    String name;
    float balance;
    bool isTreasurer;
};

struct Product {
    String name;
    String price;       // Pre-formatted "0.30\u20ac"
};

struct InventoryItem {
    String item;
    String price;       // Pre-formatted "30\u20ac / 1kg"
    String stock;       // Pre-formatted "1.350kg"
    String status;      // "red", "yellow", "green"
};

struct Config {
    String treasurer;
    String paypalLink;
    float balanceThreshold;
    String criticalInventoryItem;
};

struct AppConfig {
    User users[MAX_USERS];
    int userCount;
    Product products[MAX_PRODUCTS];
    int productCount;
    InventoryItem inventory[MAX_INVENTORY];
    int inventoryCount;
    Config config;
};

// ============================================
// Enums
// ============================================

enum DisplayState { AWAKE, SLEEPING };
enum ConnectionState { ONLINE, OFFLINE };
enum Page {
    PAGE_BOOT, PAGE_USER, PAGE_CONSUME, PAGE_PURCHASE,
    PAGE_LOGGING, PAGE_LOGGED, PAGE_ACCOUNT, PAGE_BALANCE,
    PAGE_INVENTORY, PAGE_INFO
};

// ============================================
// Global State (extern)
// ============================================

extern AppConfig appConfig;
extern DisplayState displayState;
extern ConnectionState connectionState;
extern Page currentPage;
extern String currentUser;
extern int userPageIndex;

extern String lastTransactionType;
extern String lastTransactionId;
extern float lastTransactionAmount;

extern unsigned long lastReconnectAttempt;
extern unsigned long lastSyncTime;
extern bool sdAvailable;

// ============================================
// Helper Functions
// ============================================

void parseConfig(JsonDocument& doc);
void parseUsers(JsonDocument& doc);
User* findUser(const String& name);
String formatBalance(float balance);
float parsePrice(const String& priceStr);

#endif
