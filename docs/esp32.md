# Coffee Tracker - ESP32 Implementation Specification

---

## 1. Hardware

### 1.1 ESP32-S3 DevKit

Any ESP32-S3 board with sufficient GPIO. No special peripherals beyond what's listed below.

### 1.2 Pin Assignments

| Peripheral         | Function | GPIO   | Notes                          |
|--------------------|----------|--------|--------------------------------|
| Nextion TX (ESP RX)| Serial2 RX | TBD | 115200 baud, 8N1              |
| Nextion RX (ESP TX)| Serial2 TX | TBD | 115200 baud, 8N1              |
| SD Card MOSI       | SPI      | TBD    | SPI bus for SD card            |
| SD Card MISO       | SPI      | TBD    | SPI bus for SD card            |
| SD Card SCK        | SPI      | TBD    | SPI bus for SD card            |
| SD Card CS         | SPI      | TBD    | Chip select                    |
| INMP441 WS         | I2S      | TBD    | I2S word select                |
| INMP441 SCK        | I2S      | TBD    | I2S clock                      |
| INMP441 SD         | I2S      | TBD    | I2S data in                    |

Pin assignments are defined as constants in a config header. The implementer should pick available GPIOs based on the specific devkit and wire accordingly.

### 1.3 Nextion Display

- Communication: UART at 115200 baud
- Termination: every command from ESP ends with three `0xFF` bytes
- Commands from Nextion are ASCII strings terminated by newline (sent via `prints` command in Nextion HMI)
- System events `0x86` (sleep) and `0x87` (wake) are single-byte codes followed by `0xFF 0xFF 0xFF`

### 1.4 SD Card

- Connected via SPI
- FAT32 formatted
- Used for: config cache, offline transaction log, transaction audit log

### 1.5 INMP441 Microphone

- I2S MEMS microphone
- 16kHz sample rate, 16-bit mono
- Only active when display is sleeping

---

## 2. Project Structure

```
coffee-tracker-esp32/
├── platformio.ini
├── src/
│   ├── main.cpp              // setup(), loop(), top-level state machine
│   ├── config.h              // Pin definitions, constants, compile-time config
│   ├── nextion.h / .cpp      // Nextion serial protocol (send commands, read commands)
│   ├── backend.h / .cpp      // HTTP client (GET/POST to Google Apps Script)
│   ├── storage.h / .cpp      // SD card operations (config cache, offline log, audit log)
│   ├── state.h / .cpp        // Application state (cached config, current user, etc.)
│   ├── pages.h / .cpp        // Page display functions (showUserPage, showConsumePage, etc.)
│   ├── commands.h / .cpp     // Command handler (maps Nextion commands to actions)
│   └── audio.h / .cpp        // I2S microphone and grinder detection
```

### 2.1 platformio.ini

```ini
[env:esp32s3]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200
lib_deps =
    bblanchon/ArduinoJson@^7
```

No external libraries needed beyond ArduinoJson. WiFi, HTTPClient, SD, I2S are all built into the Arduino ESP32 core.

---

## 3. Configuration (`config.h`)

Compile-time constants. No runtime configuration beyond what comes from the backend Config sheet.

```
// Pin assignments (TBD by implementer)
NEXTION_RX_PIN
NEXTION_TX_PIN
SD_CS_PIN
SD_MOSI_PIN, SD_MISO_PIN, SD_SCK_PIN
I2S_WS_PIN, I2S_SCK_PIN, I2S_SD_PIN

// Network
WIFI_SSID
WIFI_PASSWORD
BACKEND_URL                    // Google Apps Script web app URL

// Timing
WIFI_CONNECT_TIMEOUT_MS        5000
HTTP_TIMEOUT_MS                10000
RECONNECT_INTERVAL_MS          60000
BOOT_READY_DELAY_MS            500
LOGGED_FLOW_TIMER_MS           5000
BALANCE_FLOW_TIMER_MS          180000

// Sleep timeouts (thsp values in ms for Nextion)
THSP_USER                      45000
THSP_CONSUME                   120000
THSP_PURCHASE                  120000
THSP_LOGGING                   0
THSP_LOGGED                    30000
THSP_ACCOUNT                   120000
THSP_BALANCE                   180000
THSP_INVENTORY                 60000
THSP_INFO                      60000

// Audio
I2S_SAMPLE_RATE                16000
I2S_BUFFER_SIZE                1024
GRINDER_DETECTION_THRESHOLD    TBD
GRINDER_SUSTAINED_MS           TBD

// SD file paths
CONFIG_CACHE_PATH              "/config.json"
OFFLINE_LOG_PATH               "/offline.json"
TRANSACTION_LOG_PATH           "/transactions.log"
```

---

## 4. Application State (`state.h`)

Global state held in RAM. Loaded from backend at boot, refreshed on sync, cached to SD card.

### 4.1 Cached Config

Entire `getConfig` response stored in a struct:

```
struct User {
    String name;
    float balance;
    bool isTreasurer;
};

struct Product {
    String name;
    String price;          // pre-formatted "0.30€"
};

struct InventoryItem {
    String item;
    String price;          // pre-formatted "30€ / 1kg"
    String stock;          // pre-formatted "1.350kg"
    String status;         // "red", "yellow", "green"
};

struct Config {
    String treasurer;
    String paypalLink;
    float balanceThreshold;
    String criticalInventoryItem;
};

struct AppConfig {
    User users[MAX_USERS];       // MAX_USERS = 16 (2 pages of 8)
    int userCount;
    Product products[MAX_PRODUCTS]; // MAX_PRODUCTS = 8
    int productCount;
    InventoryItem inventory[MAX_INVENTORY]; // MAX_INVENTORY = 8
    int inventoryCount;
    Config config;
};
```

### 4.2 Runtime State

```
enum DisplayState { AWAKE, SLEEPING };
enum ConnectionState { ONLINE, OFFLINE };
enum Page { BOOT, USER, CONSUME, PURCHASE, LOGGING, LOGGED, ACCOUNT, BALANCE, INVENTORY, INFO };

DisplayState displayState;
ConnectionState connectionState;
Page currentPage;
String currentUser;
int userPageIndex;                    // 0-based, for pagination (8 users per page)

// Last transaction context (for logged page routing and undo)
String lastTransactionType;           // "consume", "purchase", "settle"
String lastTransactionId;
float lastTransactionAmount;          // for display on logged page
```

---

## 5. Nextion Communication (`nextion.h`)

### 5.1 Sending Commands

Function `sendNextion(String cmd)` writes the command string followed by three `0xFF` bytes to Serial2.

Example calls:

```
sendNextion("user.username0.txt=\"Christoph\"");
sendNextion("vis user.username0,1");
sendNextion("thsp=45000");
sendNextion("page user");
```

Helper functions for common patterns:

```
setText(page, component, value)     →  sendNextion("<page>.<component>.txt=\"<value>\"")
setVis(page, component, visible)   →  sendNextion("vis <page>.<component>,<0|1>")
setPic(page, component, picId)     →  sendNextion("<page>.<component>.pic=<picId>")
setPage(pageName)                  →  sendNextion("page <pageName>")
setThsp(ms)                        →  sendNextion("thsp=<ms>")
setTimer(page, timer, ms, enable)  →  sendNextion("<page>.<timer>.tim=<ms>")
                                      sendNextion("<page>.<timer>.en=<enable>")
```

### 5.2 Receiving Commands

Serial2 is read in the main loop. Incoming bytes are accumulated into a buffer until a complete message is detected.

**Nextion `prints` commands** send ASCII strings terminated by a newline character. The serial reader accumulates bytes until `\n` is received, then returns the complete string.

**System events** (`0x86` sleep, `0x87` wake) are detected by checking if the first byte of a received sequence is `0x86` or `0x87`. These are followed by `0xFF 0xFF 0xFF`.

The reader function returns one of:

- A command string (e.g. `"PAGE:USER:SELECT:Christoph"`)
- A system event identifier (`NEXTION_SLEEP`, `NEXTION_WAKE`)
- Empty/null if no complete message available

### 5.3 Command Format

All commands from the Nextion follow the pattern: `PAGE:<PAGE>:<ACTION>` with optional parameters after additional colons or commas.

Full command list (from UI spec §5.1):

```
PAGE:USER:SELECT:<username>
PAGE:USER:INVENTORY
PAGE:USER:INFO
PAGE:USER:NAV:<next|prev>

PAGE:CONSUME:CONFIRM:<username>,<single>,<double>,<cow>,<oat>
PAGE:CONSUME:BACK
PAGE:CONSUME:ACCOUNT:<username>
PAGE:CONSUME:PURCHASE:<username>

PAGE:PURCHASE:CONFIRM:<username>,<beans>,<water>,<cow>,<oat>
PAGE:PURCHASE:BACK

PAGE:LOGGED:UNDO:<username>
PAGE:LOGGED:BACK
PAGE:LOGGED:TIMEOUT

PAGE:BALANCE:PAYPAL:<username>
PAGE:BALANCE:BACK
PAGE:BALANCE:TIMEOUT

PAGE:ACCOUNT:BALANCE:<username>
PAGE:ACCOUNT:BACK

PAGE:INVENTORY:BACK

PAGE:INFO:BACK
PAGE:INFO:SYNC
```

The `logging` page sends no commands.

---

## 6. Backend Communication (`backend.h`)

### 6.1 HTTP Client Setup

Uses Arduino `HTTPClient` over HTTPS. Google Apps Script URLs redirect (302) to `script.googleusercontent.com`, so `setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS)` is required.

For TLS: use `WiFiClientSecure` with `setInsecure()` (skip certificate validation). Acceptable for a trusted office network. Alternative: use `esp_crt_bundle_attach` for proper CA validation.

### 6.2 GET Requests

```
BackendResponse httpGet(String action, String params = "")
```

Constructs URL: `BACKEND_URL?action=<action>&<params>`

Returns parsed JSON or error state.

**Endpoints used:**

| Function call | URL params |
|--------------|------------|
| `httpGet("getConfig")` | Boot, sync |
| `httpGet("getUsers")` | User page refresh, reconnect check |
| `httpGet("getInventory")` | Inventory page |
| `httpGet("getLastTransactions", "user=X&limit=10")` | Account page |

### 6.3 POST Requests

```
BackendResponse httpPost(JsonObject payload)
```

Sends JSON payload to `BACKEND_URL`.

The response always contains `status`, `balance`, and `criticalInventoryItem` (for success/duplicate) or `status` and `error` (for errors).

### 6.4 Transaction ID Generation

```
String generateTransactionId()
```

Format: `esp32-<millis>-<random_hex>`

Uses `millis()` for timestamp component and `esp_random()` for the random component (4 hex digits). Generated at the moment the user confirms an action, before the HTTP call.

### 6.5 Error Handling

On any HTTP failure (timeout, connection refused, non-200 status):

1. Set `connectionState = OFFLINE`
2. If the request was a POST: write the payload to the offline log (the transaction still "succeeds" from the user's perspective)
3. On GET failures during page navigation: show the page in offline mode or fall back to cached data

Never block the UI on a network error. The user should not notice backend failures except through the status bar.

---

## 7. SD Card Storage (`storage.h`)

### 7.1 Config Cache (`/config.json`)

The complete `getConfig` JSON response serialized to file.

**Written:** after every successful `getConfig` call (boot, sync, reconnect).

**Read:** during offline boot when backend is unreachable. Parsed into the `AppConfig` struct.

If the file doesn't exist and the backend is unreachable, the ESP stays on the boot page with `"No data - connect to WiFi"`.

### 7.2 Offline Log (`/offline.json`)

A JSON array of POST request payloads awaiting sync.

**Appended to:** when a POST request fails due to offline state, or when the ESP is already in offline mode.

**Format:**

```json
[
  {"user":"Christoph","type":"consume","items":["Double"],"quantities":[1],"transactionId":"esp32-xxx"},
  {"user":"Laura","type":"consume","items":["Single","Cow Milk Portion"],"quantities":[1,1],"transactionId":"esp32-yyy"}
]
```

**Synced:** when connection is restored. Process entries in order:

1. POST each entry to the backend
2. On `success` or `duplicate` response: remove from array
3. On `error`: log the error, skip entry (don't retry malformed requests)
4. After all entries processed: delete the file

**Note on settle:** settle transactions are never written to the offline log. The settle flow is blocked in offline mode because the QR code requires an accurate balance.

### 7.3 Transaction Audit Log (`/transactions.log`)

Append-only, one JSON object per line (JSONL format).

**Appended to:** before every POST request, regardless of online/offline state. This is the safety net.

**Format:**

```
{"ts":"2025-01-10T16:34:00","user":"Christoph","type":"consume","items":["Double"],"quantities":[1],"tid":"esp32-xxx"}
{"ts":"2025-01-10T16:35:00","user":"Laura","type":"consume","items":["Single"],"quantities":[1],"tid":"esp32-yyy"}
```

Never read by the ESP. Exists only for disaster recovery (manual replay if backend data is lost).

---

## 8. Page Display Functions (`pages.h`)

Each function sets all Nextion objects for a page, then sends `thsp` and `page` commands. These map directly to the "ESP-Set Objects" sections in the UI spec.

### 8.1 Status Bar (common to all pages except boot)

```
updateStatusBar(Page page)
```

Called before every page switch:

```
<page>.status.txt = connectionState == OFFLINE ? "Offline - logging locally" : ""
vis <page>.wifi_icon = WiFi.status() == WL_CONNECTED ? 1 : 0
vis <page>.backend_icon = 0     // set to 1 only during active HTTP requests
```

### 8.2 showBootPage()

Sets `boot.status.txt` and `boot.version.txt`. Called during the boot sequence only. No `thsp` (boot manages its own flow).

### 8.3 showUserPage()

Reads from `appConfig.users[]` and `userPageIndex`.

- Calculate start index: `userPageIndex * 8`
- Set `user.usernameN.txt` for up to 8 slots, empty string for unused slots
- Set `vis user.usernameN` for each slot
- Set `user.page_indicator.txt` (e.g. `"1/2"`)
- Set `vis user.nav_left` (hidden if `userPageIndex == 0`)
- Set `vis user.nav_right` (hidden if on last page)
- Set `user.inventory_warning.txt`:
  - Online: `appConfig.config.criticalInventoryItem`
  - Offline: `""` (blank)
- Update status bar
- `thsp = THSP_USER`
- `page user`

### 8.4 showConsumePage(String username)

Looks up user balance from `appConfig.users[]`.

- Set `consume.username.txt`
- Set `consume.userbalance.txt`:
  - Online: formatted balance (e.g. `"-15.40€"`)
  - Offline: `"offline"`
- Set price texts from `appConfig.products[]`:
  - `consume.single_price.txt` = product[0].price
  - `consume.double_price.txt` = product[1].price
  - `consume.cow_price.txt` = product[2].price
  - `consume.oat_price.txt` = product[3].price
- Set `vis consume.purchase` = 1 if user `isTreasurer`, else 0
- Update status bar
- `thsp = THSP_CONSUME`
- `page consume`

### 8.5 showPurchasePage(String username)

- Set `purchase.username.txt`
- Set `purchase.userbalance.txt` (same online/offline logic as consume)
- Set price texts from `appConfig.inventory[]`:
  - `purchase.beans_price.txt` = inventory[0].price
  - `purchase.water_price.txt` = inventory[1].price
  - `purchase.cow_price.txt` = inventory[2].price
  - `purchase.oat_price.txt` = inventory[3].price
- Update status bar
- `thsp = THSP_PURCHASE`
- `page purchase`

### 8.6 showLoggingPage(String type, String username, float amount)

- Set `logging.username.txt`
- Set `logging.userbalance.txt` (current balance before transaction)
- Set `logging.amount.txt` = formatted amount (e.g. `"€1.60"`)
- Set `logging.title.txt`, `logging.icon.pic`, `logging.subtitle.txt` based on type:

| type | title.txt | icon.pic | subtitle.txt |
|------|-----------|----------|--------------|
| `"consume"` | `"Logging..."` | coffee cup pic ID | `"Logging coffee"` |
| `"purchase"` | `"Logging..."` | box pic ID | `"Logging purchase"` |
| `"settle"` | `"Recording payment"` | hand pic ID | `"Clearing balance"` |
| `"undo"` | `"Undo"` | undo arrow pic ID | `"Undo last action"` |

- Update status bar (with `backend_icon` visible)
- `thsp = THSP_LOGGING` (0 — disabled)
- `page logging`

### 8.7 showLoggedPage(String type, String username, float newBalance, float amount)

- Set `logged.username.txt`
- Set `logged.userbalance.txt` = formatted new balance
- Set `logged.amount.txt` = formatted amount
- Set `logged.subtitle.txt` based on type:

| type | subtitle.txt | undo_btn visible |
|------|-------------|-----------------|
| `"consume"` | `"Logged coffee"` | yes |
| `"purchase"` | `"Logged purchase"` | yes |
| `"settle"` | `"Balance settled"` | yes |
| `"undo"` | `"Action undone"` | no |

- Set `vis logged.undo_btn`
- Set flow timer: `logged.tm0.tim = 5000`, `logged.tm0.en = 1`
- Update status bar
- `thsp = THSP_LOGGED`
- `page logged`

### 8.8 showAccountPage(String username)

Makes a `getLastTransactions` call to the backend. Only available online.

- Set `account.username.txt`
- Set `account.userbalance.txt`
- For each of 10 rows (0-9):
  - If transaction data exists at index: set `rowN_date`, `rowN_action`, `rowN_item`, `rowN_qty`
  - Else: set all four fields to `""`
- Update status bar
- `thsp = THSP_ACCOUNT`
- `page account`

### 8.9 showBalancePage(String username)

- Look up user balance from cached config
- Set `balance.username.txt`
- Set `balance.userbalance.txt`
- Construct PayPal URL: `"https://<paypalLink>/<absBalance>"`
- Set `balance.qr.txt` = full PayPal URL (for QR code rendering)
- Set `balance.paypal_url.txt` = display-friendly URL
- Set flow timer: `balance.tm0.tim = 180000`, `balance.tm0.en = 1`
- Update status bar
- `thsp = THSP_BALANCE`
- `page balance`

### 8.10 showInventoryPage()

Reads from `appConfig.inventory[]`. Only meaningful online (stale data offline).

- For each inventory item (beans, water, cow milk, oat milk):
  - Set `inventory.<item>_qty.txt` = `inventory[i].stock`
  - Set `inventory.<item>_bg.pic` = picture ID based on `inventory[i].status`:
    - `"green"` → green background pic ID
    - `"yellow"` → yellow background pic ID
    - `"red"` → red background pic ID
- Update status bar
- `thsp = THSP_INVENTORY`
- `page inventory`

### 8.11 showInfoPage()

- Set `info.wifi_status.txt`:
  - Connected: `"Connected (<SSID>)"`
  - Disconnected: `"Disconnected"`
- Set `info.backend_status.txt`:
  - Online: `"Online"`
  - Offline: `"Offline"`
- Set `info.last_sync.txt`: timestamp of last successful sync or `"Never"`
- Set `info.sd_status.txt`:
  - SD mounted: `"OK"`
  - SD error: `"Error"`
- Set `info.firmware.txt` = firmware version string from `config.h`
- Update status bar
- `thsp = THSP_INFO`
- `page info`

---

## 9. Command Handler (`commands.h`)

Parses incoming Nextion command strings and dispatches to appropriate actions. This is the core business logic.

### 9.1 Command Parsing

Split the command string by `:` to get parts:

- `parts[0]` = `"PAGE"`
- `parts[1]` = page name
- `parts[2]` = action
- `parts[3+]` = parameters (may contain commas for further splitting)

### 9.2 Command Handlers

#### USER page

**`PAGE:USER:SELECT:<username>`**

1. Set `currentUser = <username>`
2. Call `showConsumePage(currentUser)`

**`PAGE:USER:INVENTORY`**

1. If offline: ignore (inventory page not available offline)
2. Call `showInventoryPage()`

**`PAGE:USER:INFO`**

1. Call `showInfoPage()`

**`PAGE:USER:NAV:<direction>`**

1. If `next`: `userPageIndex++`
2. If `prev`: `userPageIndex--`
3. Clamp to valid range
4. Call `showUserPage()`

#### CONSUME page

**`PAGE:CONSUME:CONFIRM:<username>,<single>,<double>,<cow>,<oat>`**

1. Parse quantities from comma-separated values
2. Build items array and quantities array from non-zero values:
   - If `single > 0`: add `"Single"` with quantity
   - If `double > 0`: add `"Double"` with quantity
   - If `cow > 0`: add `"Cow Milk Portion"` with quantity
   - If `oat > 0`: add `"Oat Milk Portion"` with quantity
3. Calculate display amount from cached prices (sum of qty × price for each item)
4. Generate `transactionId`
5. Store: `lastTransactionType = "consume"`, `lastTransactionId`, `lastTransactionAmount`
6. Call `showLoggingPage("consume", username, amount)`
7. Write to audit log (SD)
8. Attempt POST:

   ```json
   {"user": "<username>", "type": "consume", "items": [...], "quantities": [...], "transactionId": "..."}
   ```

9. On success/duplicate:
   - Update cached user balance from response
   - Update `criticalInventoryItem` from response
   - Call `showLoggedPage("consume", username, newBalance, amount)`
10. On failure:
    - Write to offline log (SD)
    - Set `connectionState = OFFLINE`
    - Call `showLoggedPage("consume", username, 0, amount)` — balance displayed as `"offline"`

**`PAGE:CONSUME:BACK`**

1. Call `showUserPage()`

**`PAGE:CONSUME:ACCOUNT:<username>`**

1. If offline: ignore (account page not available offline)
2. Call `showAccountPage(username)`

**`PAGE:CONSUME:PURCHASE:<username>`**

1. Call `showPurchasePage(username)`

#### PURCHASE page

**`PAGE:PURCHASE:CONFIRM:<username>,<beans>,<water>,<cow>,<oat>`**

1. Parse quantities
2. Build items array from non-zero values:
   - If `beans > 0`: add `"Beans"` with quantity
   - If `water > 0`: add `"Water"` with quantity
   - If `cow > 0`: add `"Cow Milk Carton"` with quantity
   - If `oat > 0`: add `"Oat Milk Carton"` with quantity
3. Calculate display amount from cached inventory costs
4. Generate `transactionId`
5. Store last transaction context
6. Call `showLoggingPage("purchase", username, amount)`
7. Write to audit log
8. Attempt POST:

   ```json
   {"user": "<username>", "type": "purchase", "items": [...], "quantities": [...], "transactionId": "..."}
   ```

9. Handle success/failure same as consume

**`PAGE:PURCHASE:BACK`**

1. Call `showConsumePage(currentUser)`

#### LOGGED page

**`PAGE:LOGGED:UNDO:<username>`**

1. Generate new `transactionId`
2. Call `showLoggingPage("undo", username, lastTransactionAmount)`
3. Write undo to audit log
4. Attempt POST:

   ```json
   {"user": "<username>", "type": "undo", "transactionId": "..."}
   ```

5. On success:
   - Update cached balance
   - Set `lastTransactionType = "undo"`
   - Call `showLoggedPage("undo", username, newBalance, amount)`
6. On failure:
   - Write to offline log
   - Call `showLoggedPage("undo", username, 0, amount)`

**`PAGE:LOGGED:BACK`**

1. Call `showUserPage()`

**`PAGE:LOGGED:TIMEOUT`**

1. If `lastTransactionType == "consume"`:
   - Look up user balance from cached config
   - If `balance < appConfig.config.balanceThreshold`: call `showBalancePage(currentUser)`
   - Else: call `showUserPage()`
2. For all other types: call `showUserPage()`

#### BALANCE page

**`PAGE:BALANCE:PAYPAL:<username>`**

1. If offline: ignore (settle not available offline)
2. Get user balance from cache (the amount the user saw on the QR code)
3. `amount = abs(balance)`
4. Generate `transactionId`
5. Store: `lastTransactionType = "settle"`, etc.
6. Call `showLoggingPage("settle", username, amount)`
7. Write to audit log
8. Attempt POST:

   ```json
   {"user": "<username>", "type": "settle", "amount": <amount>, "transactionId": "..."}
   ```

9. On success: call `showLoggedPage("settle", username, newBalance, amount)`
10. On failure: this is a problem — settle should only be allowed online. If the POST fails unexpectedly, show an error state and return to user page. Do NOT write settle to offline log.

**`PAGE:BALANCE:BACK`**

1. Call `showUserPage()`

**`PAGE:BALANCE:TIMEOUT`**

1. Call `showUserPage()`

#### ACCOUNT page

**`PAGE:ACCOUNT:BALANCE:<username>`**

1. Call `showBalancePage(username)`

**`PAGE:ACCOUNT:BACK`**

1. Call `showConsumePage(currentUser)`

#### INVENTORY page

**`PAGE:INVENTORY:BACK`**

1. Call `showUserPage()`

#### INFO page

**`PAGE:INFO:BACK`**

1. Call `showUserPage()`

**`PAGE:INFO:SYNC`**

1. Show backend_icon on status bar
2. Call `getConfig` from backend
3. If success:
   - Update `appConfig` in RAM
   - Write to SD config cache
   - Sync RTC: parse server time, set Nextion RTC via `rtc0`-`rtc5` commands
   - Set `connectionState = ONLINE`
   - If offline log exists: replay all entries, clear file
   - Set `info.last_sync.txt = "Just now"`
4. If failure:
   - Set `info.last_sync.txt = "Sync failed"`
5. Hide backend_icon

---

## 10. Main Loop (`main.cpp`)

### 10.1 Setup

```
setup():
    Serial.begin(115200)                    // Debug output
    Serial2.begin(115200, RX, TX)           // Nextion
    SD.begin(CS_PIN)                        // SD card
    
    showBootPage()
    
    // Step 1: WiFi
    setBootStatus("Connecting to WiFi...")
    WiFi.begin(SSID, PASS)
    wait up to WIFI_CONNECT_TIMEOUT_MS
    
    if (WiFi connected):
        connectionState = ONLINE
        
        // Step 2: Clock sync
        setBootStatus("Syncing clock...")
        syncNTP()                           // get time, set Nextion RTC
        
        // Step 3: Fetch data
        setBootStatus("Fetching data...")
        response = httpGet("getConfig")
        parseConfig(response) → appConfig
        writeConfigCache(appConfig)         // SD card
        
        // Step 4: Ready
        setBootStatus("Ready")
        delay(BOOT_READY_DELAY_MS)
        
    else:
        connectionState = OFFLINE
        if (configCacheExists()):
            setBootStatus("Offline - using cached data")
            loadConfigCache() → appConfig
            delay(BOOT_READY_DELAY_MS)
        else:
            setBootStatus("No data - connect to WiFi")
            // Stay on boot page, retry WiFi periodically
            // Once connected, restart boot sequence from step 2
            return
    
    // Step 5: Show user page
    userPageIndex = 0
    displayState = AWAKE
    showUserPage()
```

### 10.2 Loop

```
loop():
    switch (displayState):
        
        case AWAKE:
            // Check for Nextion commands
            command = readNextionCommand()
            if (command == NEXTION_SLEEP):
                displayState = SLEEPING
                startAudio()
                return
            if (command != empty):
                handleCommand(command)
            
            // Periodic reconnect check (if offline)
            if (connectionState == OFFLINE):
                if (millis() - lastReconnectAttempt > RECONNECT_INTERVAL_MS):
                    lastReconnectAttempt = millis()
                    tryReconnect()
        
        case SLEEPING:
            // Process audio for grinder detection
            if (processAudioFrame()):          // returns true if grinder detected
                stopAudio()
                wakeDisplay()
                return
            
            // Also check for touch wake
            event = readNextionEvent()         // non-blocking, checks for 0x87
            if (event == NEXTION_WAKE):
                stopAudio()
                handleWake()
                return
```

### 10.3 Wake Handlers

**`handleWake()`** — touch-triggered wake:

1. `displayState = AWAKE`
2. Refresh user data if online (lightweight `getUsers` call)
3. `showUserPage()`

**`wakeDisplay()`** — grinder-triggered wake:

1. Send `sleep=0` to Nextion (force wake)
2. `displayState = AWAKE`
3. Refresh user data if online
4. `showUserPage()`
5. Trigger edge-pulse animation (TBD — may involve sending a series of Nextion commands)

### 10.4 Reconnect

**`tryReconnect()`**:

1. If WiFi not connected: attempt reconnect
2. If WiFi connected: try `httpGet("getUsers")`
3. If success:
   - `connectionState = ONLINE`
   - Call `httpGet("getConfig")`, update `appConfig`, write to SD
   - Replay offline log if it exists
   - Update status bar on current page (clear offline message)
4. If failure: remain offline, try again next interval

---

## 11. Audio Detection (`audio.h`)

### 11.1 Lifecycle

- **`startAudio()`**: initialize I2S peripheral, begin DMA-based sampling at 16kHz mono 16-bit
- **`stopAudio()`**: deinitialize I2S, free buffers
- **`processAudioFrame()`**: read one DMA buffer of samples, run detection algorithm, return `true` if grinder detected

Audio is only active between `startAudio()` and `stopAudio()` calls. This happens exclusively during display sleep.

### 11.2 Detection Algorithm (Starting Point)

The grinder produces sustained broadband noise in the 1-8kHz range. The algorithm:

1. Read I2S buffer (1024 samples at 16kHz = 64ms window)
2. Compute RMS energy of the frame
3. If energy exceeds `GRINDER_DETECTION_THRESHOLD`:
   - Increment a sustained counter
   - If sustained counter × frame duration exceeds `GRINDER_SUSTAINED_MS` (e.g. 1500ms): return `true`
4. If energy is below threshold: reset sustained counter

This is a starting point. Refinements might include:

- Simple FFT to check spectral flatness (grinder is broadband, speech has peaks)
- Adaptive threshold based on ambient noise floor
- Debounce after detection (ignore for N seconds after triggering)

The detection parameters (`GRINDER_DETECTION_THRESHOLD`, `GRINDER_SUSTAINED_MS`) need to be tuned empirically with the actual grinder and microphone placement.

### 11.3 Non-blocking Serial Check During Audio

While processing audio frames, the loop also checks for Nextion wake events (`0x87`). Since each audio frame takes ~64ms to process, and the serial buffer holds 256+ bytes, there's ample time to check `Serial2.available()` between frames without missing data from either source.

---

## 12. Offline Mode Behavior Summary

| Feature | Online | Offline |
|---------|--------|---------|
| Consume | Full flow, backend POST | Full flow, logged to SD, balance shows `"offline"` |
| Purchase | Full flow, backend POST | Full flow, logged to SD, balance shows `"offline"` |
| Settle | Full flow, Telegram notification | **Blocked** — balance page not accessible |
| Undo | Full flow, backend reversal | Logged to SD, references local `lastTransactionId` |
| User balance display | From backend response | `"offline"` |
| Inventory warning | `criticalInventoryItem` from backend | `""` (blank) |
| Inventory page | Live data from `getInventory` | **Not accessible** |
| Account page | Live data from `getLastTransactions` | **Not accessible** |
| Status bar | `""` (empty) | `"Offline - logging locally"` |
| User page | Cached users from config | Cached users from config |
| Consume/purchase prices | Cached from config | Cached from config |

---

## 13. Balance Formatting

The ESP formats balances for display on the Nextion. The backend returns raw numbers.

**Rules:**

- Always show 2 decimal places
- Append `€` suffix
- Negative: `"-15.40€"` (Nextion detects leading `-` and sets red color)
- Zero: `"0.00€"`
- Positive: `"59.90€"`

**Offline mode:** display `"offline"` instead of any balance value.

---

## 14. RTC Synchronization

The Nextion has an internal RTC. The ESP sets it:

- At boot (after NTP sync)
- On manual sync (`PAGE:INFO:SYNC`)

**Nextion RTC commands:**

```
rtc0=2025    // year
rtc1=1       // month
rtc2=10      // day
rtc3=16      // hour
rtc4=34      // minute
rtc5=0       // second
```

The ESP uses NTP to get the current time. Timezone should be set to CET/CEST (UTC+1/+2) since the tracker is in a German office.

Each Nextion page has a timer component that reads the RTC and updates the time display every second. This is entirely Nextion-side — the ESP doesn't participate in ongoing time display.

---

## 15. Nextion Picture IDs

The Nextion HMI project contains pre-exported images. The ESP references them by picture ID. These IDs are determined during Nextion HMI development and must be defined as constants in `config.h`.

```
// Logging page icons
PIC_ICON_COFFEE          // coffee cup with steam
PIC_ICON_PURCHASE        // box with up arrow
PIC_ICON_BALANCE         // hand with coins
PIC_ICON_UNDO            // undo arrow

// Inventory backgrounds (per item × 3 colors)
PIC_BEANS_GREEN
PIC_BEANS_YELLOW
PIC_BEANS_RED
PIC_WATER_GREEN
PIC_WATER_YELLOW
PIC_WATER_RED
PIC_COW_GREEN
PIC_COW_YELLOW
PIC_COW_RED
PIC_OAT_GREEN
PIC_OAT_YELLOW
PIC_OAT_RED
```

---

## 16. Error States

### 16.1 Network Error During POST (Consume/Purchase/Undo)

1. The transaction is already written to the audit log
2. Write to offline log
3. Set `connectionState = OFFLINE`
4. Show `logged` page with balance as `"offline"`
5. User experience is seamless — they don't know the backend is down

### 16.2 Network Error During POST (Settle)

1. Settle is NOT written to offline log (amount may be stale)
2. Show error indication on logging page (TBD — maybe a brief error text)
3. Return to user page
4. The user can retry once back online

### 16.3 Network Error During GET (Account/Inventory)

1. These pages require live data
2. If the GET fails: stay on the current page, show a brief error, or ignore the tap
3. Do not navigate to a page with stale or empty data

### 16.4 SD Card Error

1. If SD is not mounted at boot: proceed without caching (all transactions are in-memory only until backend confirms)
2. `info.sd_status.txt = "Error"`
3. Offline mode is effectively disabled (no offline log, no config cache)
4. Log a warning to Serial debug output

---

## 17. Debug Logging

Use `Serial.println()` (USB serial) for debug output. Prefix with category:

```
[BOOT] Connecting to WiFi...
[BOOT] WiFi connected: 192.168.1.100
[NET]  GET getConfig → 200 OK (342ms)
[NET]  POST consume → 200 OK (1204ms)
[NET]  POST consume → FAILED (timeout)
[SD]   Config cache written (1.2KB)
[SD]   Offline log: 3 entries pending
[CMD]  PAGE:USER:SELECT:Christoph
[CMD]  PAGE:CONSUME:CONFIRM:Christoph,0,1,0,2
[PAGE] → consume (Christoph)
[PAGE] → logging (consume)
[PAGE] → logged (consume, balance: -16.40)
[AUDIO] Grinder detected, waking display
[SLEEP] Display entered sleep
[WAKE]  Touch wake
```

---

## 18. Implementation Priority

Suggested order of implementation:

1. **Nextion serial communication** — send/receive, verify with test commands
2. **Boot sequence** — WiFi, NTP, show boot page statuses
3. **Backend GET** — `getConfig`, parse JSON, populate `appConfig`
4. **User page** — display users, handle SELECT
5. **Consume page** — display, handle CONFIRM
6. **Logging + Logged pages** — display, flow timer
7. **Backend POST** — consume transaction, handle response
8. **Purchase flow** — purchase page, POST
9. **Undo flow** — logged undo button, POST
10. **Account page** — `getLastTransactions`, display table
11. **Balance page** — QR code, PayPal URL, settle POST
12. **SD card** — config cache, offline log, audit log
13. **Offline mode** — detection, fallback behavior, reconnect, replay
14. **Inventory page** — `getInventory`, traffic lights
15. **Info page** — system info, sync button
16. **Audio detection** — I2S setup, grinder algorithm, wake integration
17. **Sleep/wake** — `thsp`, `0x86`/`0x87` handling, full lifecycle
