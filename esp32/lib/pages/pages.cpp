#include "pages.h"
#include "config.h"
#include "state.h"
#include "nextion.h"
#include "backend.h"
#include <WiFi.h>
#include <math.h>

// ============================================
// Status Bar (common to all pages except boot)
// Call after setPage() — vis only works on current page
// ============================================

void updateStatusBar(const String &page)
{
    if (connectionState == OFFLINE)
    {
        setText(page, "status", "Offline \xe2\x80\x93 logging locally");
    }
    else
    {
        setText(page, "status", "");
    }
    setVis("wifi_icon", WiFi.status() == WL_CONNECTED);
    setVis("backend_icon", false);
}

// ============================================
// Boot Page
// ============================================

void showBootPage()
{
    currentPage = PAGE_BOOT;
    setPage("boot");
    setText("boot", "version", FIRMWARE_VERSION);
    Serial.println("[PAGE] -> boot");
}

void setBootStatus(const String &status)
{
    setText("boot", "status", status);
    Serial.println("[BOOT] " + status);
}

// ============================================
// User Page
// ============================================

void showUserPage()
{
    int totalPages = (appConfig.userCount + 7) / 8;
    if (totalPages == 0)
        totalPages = 1;

    // Clamp page index
    if (userPageIndex >= totalPages)
        userPageIndex = totalPages - 1;
    if (userPageIndex < 0)
        userPageIndex = 0;

    int startIdx = userPageIndex * 8;

    // Switch page first
    currentPage = PAGE_USER;
    setPage("user");
    setThsp(THSP_USER);

    // Set text for all user slots
    for (int i = 0; i < 8; i++)
    {
        int userIdx = startIdx + i;
        String slot = "username" + String(i);
        setText("user", slot, userIdx < appConfig.userCount ? appConfig.users[userIdx].name : "");
    }

    // Page indicator
    setText("user", "page", String(userPageIndex + 1) + "/" + String(totalPages));

    // Inventory warning
    setText("user", "inventory",
            connectionState == ONLINE ? appConfig.config.criticalInventoryItem : "");

    // Status bar text
    updateStatusBar("user");

    // Visibility (all after setPage)
    for (int i = 0; i < 8; i++)
    {
        int userIdx = startIdx + i;
        setVis("username" + String(i), userIdx < appConfig.userCount);
    }
    setVis("nav_prev", userPageIndex > 0);
    setVis("nav_next", userPageIndex < totalPages - 1);
    setVis("page", totalPages > 1);

    Serial.println("[PAGE] -> user (page " + String(userPageIndex + 1) +
                   "/" + String(totalPages) + ")");
}

// ============================================
// Consume Page
// ============================================

void showConsumePage(const String &username)
{
    User *user = findUser(username);

    // Switch page first
    currentPage = PAGE_CONSUME;
    setPage("consume");
    setThsp(THSP_CONSUME);

    // Text content
    setText("consume", "username", username);
    setText("consume", "userbalance",
            (connectionState == ONLINE && user) ? formatBalance(user->balance) : "offline");

    // Product prices (reformatted from "0.30\u20ac" to "\u20ac0.30")
    const char *priceComponents[] = {
        "single_price", "double_price", "cow_price", "oat_price"};
    for (int i = 0; i < 4 && i < appConfig.productCount; i++)
    {
        float price = parsePrice(appConfig.products[i].price);
        char buf[16];
        snprintf(buf, sizeof(buf), "\xe2\x82\xac%.2f", price);
        setText("consume", priceComponents[i], String(buf));
    }

    // Status bar text
    updateStatusBar("consume");

    // Visibility (all after setPage)
    setVis("purchase", user && user->isTreasurer);

    Serial.println("[PAGE] -> consume (" + username + ")");
}

// ============================================
// Purchase Page
// ============================================

void showPurchasePage(const String &username)
{
    User *user = findUser(username);

    // Switch page first
    currentPage = PAGE_PURCHASE;
    setPage("purchase");
    setThsp(THSP_PURCHASE);

    // Text content
    setText("purchase", "username", username);
    setText("purchase", "userbalance",
            (connectionState == ONLINE && user) ? formatBalance(user->balance) : "offline");

    // Inventory prices (displayed as-is from backend)
    const char *priceComponents[] = {
        "beans_price", "water_price", "cow_price", "oat_price"};
    for (int i = 0; i < 4 && i < appConfig.inventoryCount; i++)
    {
        setText("purchase", priceComponents[i], appConfig.inventory[i].price);
    }

    // Status bar text + visibility
    updateStatusBar("purchase");

    Serial.println("[PAGE] -> purchase (" + username + ")");
}

// ============================================
// Logging Page (unified loading screen)
// ============================================

void showLoggingPage(const String &type, const String &username, float amount)
{
    User *user = findUser(username);

    // Switch page first
    currentPage = PAGE_LOGGING;
    setPage("logging");
    setThsp(THSP_LOGGING);

    // Text and picture content
    setText("logging", "username", username);
    setText("logging", "userbalance",
            (connectionState == ONLINE && user) ? formatBalance(user->balance) : "offline");

    char amountBuf[16];
    snprintf(amountBuf, sizeof(amountBuf), "\xe2\x82\xac%.2f", amount);
    setText("logging", "amount", String(amountBuf));

    if (type == "consume")
    {
        setText("logging", "title", "Logging...");
        setPic("logging", "icon", PIC_ICON_COFFEE);
        setText("logging", "subtitle", "Logging coffee");
    }
    else if (type == "purchase")
    {
        setText("logging", "title", "Logging...");
        setPic("logging", "icon", PIC_ICON_PURCHASE);
        setText("logging", "subtitle", "Logging purchase");
    }
    else if (type == "settle")
    {
        setText("logging", "title", "Recording payment");
        setPic("logging", "icon", PIC_ICON_BALANCE);
        setText("logging", "subtitle", "Clearing balance");
    }
    else if (type == "undo")
    {
        setText("logging", "title", "Undo");
        setPic("logging", "icon", PIC_ICON_UNDO);
        setText("logging", "subtitle", "Undo last action");
    }

    // Status bar text + visibility, then backend_icon on top
    updateStatusBar("logging");
    setVis("backend_icon", true);

    Serial.println("[PAGE] -> logging (" + type + ")");
}

// ============================================
// Logged Page (unified done screen)
// ============================================

void showLoggedPage(const String &type, const String &username,
                    float newBalance, float amount)
{
    // Switch page first
    currentPage = PAGE_LOGGED;
    setPage("logged");
    setThsp(THSP_LOGGED);

    // Text content
    setText("logged", "username", username);
    setText("logged", "userbalance",
            connectionState == ONLINE ? formatBalance(newBalance) : "offline");

    char amountBuf[16];
    snprintf(amountBuf, sizeof(amountBuf), "\xe2\x82\xac%.2f", amount);
    setText("logged", "amount", String(amountBuf));

    if (type == "consume")
    {
        setText("logged", "subtitle", "Logged coffee");
    }
    else if (type == "purchase")
    {
        setText("logged", "subtitle", "Logged purchase");
    }
    else if (type == "settle")
    {
        setText("logged", "subtitle", "Balance settled");
    }
    else if (type == "undo")
    {
        setText("logged", "subtitle", "Action undone");
    }

    // Flow timer
    setTimer("logged", "tm0", LOGGED_FLOW_TIMER_MS, 1);

    // Status bar text + visibility
    updateStatusBar("logged");

    // Undo button visibility (after setPage)
    setVis("undo_btn", type != "undo");

    Serial.printf("[PAGE] -> logged (%s, balance: %.2f)\n",
                  type.c_str(), newBalance);
}

// ============================================
// Account Page
// ============================================

bool showAccountPage(const String &username)
{
    // Fetch transaction history before switching page
    BackendResponse resp = httpGet("getLastTransactions",
                                   "user=" + username + "&limit=10");
    if (!resp.success)
    {
        Serial.println("[PAGE] Account page: failed to fetch transactions");
        return false;
    }

    User *user = findUser(username);

    // Switch page
    currentPage = PAGE_ACCOUNT;
    setPage("account");
    setThsp(THSP_ACCOUNT);

    // Text content
    setText("account", "username", username);
    if (user)
    {
        setText("account", "userbalance", formatBalance(user->balance));
    }

    // Fill transaction rows
    JsonArray transactions = resp.doc["transactions"].as<JsonArray>();
    for (int i = 0; i < 10; i++)
    {
        String idx = String(i);
        if (i < (int)transactions.size())
        {
            JsonObject row = transactions[i].as<JsonObject>();
            setText("account", "row" + idx + "_date", row["timestamp"].as<String>());
            setText("account", "row" + idx + "_action", row["action"].as<String>());
            setText("account", "row" + idx + "_item", row["item"].as<String>());
            setText("account", "row" + idx + "_qty", row["qty"].as<String>());
        }
        else
        {
            setText("account", "row" + idx + "_date", "");
            setText("account", "row" + idx + "_action", "");
            setText("account", "row" + idx + "_item", "");
            setText("account", "row" + idx + "_qty", "");
        }
    }

    // Status bar text + visibility
    updateStatusBar("account");

    Serial.println("[PAGE] -> account (" + username + ")");
    return true;
}

// ============================================
// Balance Page
// ============================================

void showBalancePage(const String &username)
{
    User *user = findUser(username);
    float balance = user ? user->balance : 0;

    // Switch page first
    currentPage = PAGE_BALANCE;
    setPage("balance");
    setThsp(THSP_BALANCE);

    // Text content
    setText("balance", "username", username);
    setText("balance", "userbalance", formatBalance(balance));

    // Construct PayPal URL
    float absBalance = fabs(balance);
    char amountStr[16];
    snprintf(amountStr, sizeof(amountStr), "%.2f", absBalance);

    String paypalUrl = appConfig.config.paypalLink + "/" + String(amountStr);
    setText("balance", "qr", "https://" + paypalUrl);
    setText("balance", "paypal_url", paypalUrl);

    // Flow timer
    setTimer("balance", "tm0", BALANCE_FLOW_TIMER_MS, 1);

    // Status bar text + visibility
    updateStatusBar("balance");

    Serial.println("[PAGE] -> balance (" + username + ")");
}

// ============================================
// Inventory Page
// ============================================

bool showInventoryPage()
{
    // Fetch fresh inventory data before switching page
    BackendResponse resp = httpGet("getInventory");
    if (resp.success)
    {
        JsonArray invArr = resp.doc["inventory"].as<JsonArray>();
        appConfig.inventoryCount = 0;
        for (JsonObject inv : invArr)
        {
            if (appConfig.inventoryCount >= MAX_INVENTORY)
                break;
            appConfig.inventory[appConfig.inventoryCount].item = inv["item"].as<String>();
            appConfig.inventory[appConfig.inventoryCount].price = inv["price"].as<String>();
            appConfig.inventory[appConfig.inventoryCount].stock = inv["stock"].as<String>();
            appConfig.inventory[appConfig.inventoryCount].status = inv["status"].as<String>();
            appConfig.inventoryCount++;
        }
    }

    // Switch page
    currentPage = PAGE_INVENTORY;
    setPage("inventory");
    setThsp(THSP_INVENTORY);

    // Item keys matching Nextion component names
    const char *itemKeys[] = {"beans", "water", "cow", "oat"};

    // Pic IDs per item per status
    const int greenPics[] = {PIC_BEANS_GREEN, PIC_WATER_GREEN, PIC_COW_GREEN, PIC_OAT_GREEN};
    const int yellowPics[] = {PIC_BEANS_YELLOW, PIC_WATER_YELLOW, PIC_COW_YELLOW, PIC_OAT_YELLOW};
    const int redPics[] = {PIC_BEANS_RED, PIC_WATER_RED, PIC_COW_RED, PIC_OAT_RED};

    for (int i = 0; i < 4 && i < appConfig.inventoryCount; i++)
    {
        String key = String(itemKeys[i]);

        int picId;
        int bcoColor;
        if (appConfig.inventory[i].status == "green")
        {
            picId = greenPics[i];
            bcoColor = COLOR_GREEN;
        }
        else if (appConfig.inventory[i].status == "yellow")
        {
            picId = yellowPics[i];
            bcoColor = COLOR_YELLOW;
        }
        else
        {
            picId = redPics[i];
            bcoColor = COLOR_RED;
        }

        setText("inventory", key + "_qty", appConfig.inventory[i].stock);
        setBco("inventory", key + "_qty", bcoColor);
        setBco("inventory", key, bcoColor);
        setPic("inventory", key + "_bg", picId);
    }

    // Status bar text + visibility
    updateStatusBar("inventory");

    Serial.println("[PAGE] -> inventory");
    return true;
}

// ============================================
// Info Page
// ============================================

void showInfoPage()
{
    // Switch page first
    currentPage = PAGE_INFO;
    setPage("info");
    setThsp(THSP_INFO);

    // Text content
    if (WiFi.status() == WL_CONNECTED)
    {
        setText("info", "wifi_status", "Connected (" + WiFi.SSID() + ")");
    }
    else
    {
        setText("info", "wifi_status", "Disconnected");
    }

    setText("info", "backend_status",
            connectionState == ONLINE ? "Online" : "Offline");

    if (lastSyncTime == 0)
    {
        setText("info", "last_sync", "Never");
    }
    else
    {
        unsigned long elapsed = (millis() - lastSyncTime) / 1000;
        if (elapsed < 60)
        {
            setText("info", "last_sync", "Just now");
        }
        else if (elapsed < 3600)
        {
            setText("info", "last_sync", String(elapsed / 60) + "m ago");
        }
        else
        {
            setText("info", "last_sync", String(elapsed / 3600) + "h ago");
        }
    }

    setText("info", "sd_status", sdAvailable ? "OK" : "Error");
    setText("info", "firmware", FIRMWARE_VERSION);

    // Status bar text + visibility
    updateStatusBar("info");

    Serial.println("[PAGE] -> info");
}
