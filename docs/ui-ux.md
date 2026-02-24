# Coffee Tracker - UI/UX Specification

---

## 1. System Architecture

### 1.1 Display-ESP Interaction Model

The Nextion display acts as a dumb terminal. It captures user input and renders what the ESP32 tells it. All business logic, navigation decisions, and data fetching happen on the ESP32.

**On every page transition, the ESP32:**

1. Sets all text fields and properties on the target page
2. Sets component visibility where needed
3. Sets flow timer if applicable
4. Sets sleep-on-no-touch timeout (`thsp`) appropriate for the target page
5. Sends page switch command

**Nextion-managed logic (no ESP involvement):**

- Badge counter cycling on `consume` and `purchase` (tap to increment, wrap at limit, reset to 0 on page load)
- Confirm button enable/disable based on badge values
- Balance text color (checks first character for `-`, sets red if negative, green otherwise)

### 1.2 Pages

| #   | Page ID     | Purpose                                                   |
| --- | ----------- | --------------------------------------------------------- |
| 0   | `boot`      | Startup/initialization screen                             |
| 1   | `user`      | User selection grid                                       |
| 2   | `consume`   | Product item selection                                    |
| 3   | `purchase`  | Inventory item selection                                  |
| 4   | `logging`   | Unified loading screen (consume, purchase, balance, undo) |
| 5   | `logged`    | Unified done screen (consume, purchase, balance, undo)    |
| 6   | `account`   | Transaction history                                       |
| 7   | `balance`   | QR code / PayPal settlement                               |
| 8   | `inventory` | Inventory overview with traffic lights                    |
| 9   | `info`      | System info and manual sync                               |

### 1.3 Status Bar Pattern

The status bar is present on all pages except `boot`. The ESP updates these objects before every page switch:

```nextion
<page>.status.txt           // "Offline - logging locally" or "" (empty when online)
vis <page>.wifi_icon        // 1 if WiFi connected, 0 if disconnected
vis <page>.backend_icon     // 1 when doing a backend request, 0 if idle
```

The time display (HH:MM, top left) is driven by the Nextion's internal RTC via a per-page timer component that refreshes every second. The ESP sets the RTC at boot and on manual sync only.

---

## 2. Boot Sequence

### 2.1 Boot Page

**Page ID:** `boot`  

**When shown:** ESP32 power-on or restart only

#### ESP-Set Objects

```nextion
boot.status.txt             // "Connecting to WiFi...", "Syncing clock...",
                            // "Fetching data...", "Ready",
                            // "Offline - using cached data",
                            // "No data - connect to WiFi"
boot.version.txt            // "v1.0.0"
```

Title ("Coffee List") and coffee icon are static.

#### Boot Steps

| Step | `boot.status.txt`       | Action                                                      |
| ---- | ----------------------- | ----------------------------------------------------------- |
| 1    | `Connecting to WiFi...` | ESP attempts WiFi connection                                |
| 2    | `Syncing clock...`      | ESP sets Nextion RTC via `rtc0`-`rtc5` commands             |
| 3    | `Fetching data...`      | ESP fetches users, products, inventory, config from backend |
| 4    | `Ready`                 | Brief 500ms pause                                           |
| 5    | —                       | ESP initializes `user` page normally and switches to it     |

#### Offline Boot

If WiFi fails at step 1:

1. `boot.status.txt="Offline - using cached data"`
2. ESP skips clock sync (Nextion RTC retains last-set time)
3. ESP loads users, products, inventory, config from SD card cache
4. If cached data exists: proceed to step 5, offline banner active on all subsequent pages
5. If no cached data: `boot.status.txt="No data - connect to WiFi"`, ESP retries periodically

---

## 3. Sleep & Wake

### 3.1 Mechanism

- **`thsp`** (Sleep on No Touch): set by ESP after every page transition
- **`thup=1`** (Auto Wake on Touch): set once during boot initialization
- **`0x86`** serial event: Nextion notifies ESP when it enters sleep
- **`0x87`** serial event: Nextion notifies ESP when it wakes from touch

### 3.2 Sleep Timeout Values

| Page        | `thsp`                | On sleep                           |
| ----------- | --------------------- | ---------------------------------- |
| `user`      | 45s                   | Display sleeps                     |
| `consume`   | 120s                  | Display sleeps                     |
| `purchase`  | 120s                  | Display sleeps                     |
| `logging`   | 0 (disabled)          | ESP manages this page entirely     |
| `logged`    | 30s (safety fallback) | Display sleeps if flow timer fails |
| `account`   | 120s                  | Display sleeps                     |
| `balance`   | 180s                  | Display sleeps                     |
| `inventory` | 60s                   | Display sleeps                     |
| `info`      | 60s                   | Display sleeps                     |

### 3.3 Flow Timers

| Page      | Timer value | On timeout command     | ESP action                            |
| --------- | ----------- | ---------------------- | ------------------------------------- |
| `logged`  | 5000ms      | `PAGE:LOGGED:TIMEOUT`  | Check context and navigate (see §5.5) |
| `balance` | 180000ms    | `PAGE:BALANCE:TIMEOUT` | Navigate to `user`, balance unchanged |

Flow timers reset on any touch. The `thsp` acts as a safety net behind the flow timer.

### 3.4 Wake from Touch

1. Nextion wakes and sends `0x87`
2. ESP receives `0x87`, marks display as awake
3. ESP populates `user` page and switches to it with `thsp=45`

### 3.5 Wake from Grinder Detection

Detection only triggers when display is in sleep mode (tracked via `0x86`/`0x87`). Ignored if display is already awake.

1. ESP sends `sleep=0` to force-wake Nextion
2. ESP populates `user` page and switches to it
3. ESP triggers edge-pulse animation (slow warm pulse, 2-3 cycles at ~1.5s each) (TBD)

### 3.6 RTC

Set by ESP at boot (§2.1 step 2) and on `PAGE:INFO:SYNC`. Each page has a Nextion timer component that updates the time text from the internal RTC every second.

---

## 4. Screen Specifications

---

### 4.1 User

**Page ID:** `user`  

**Entry points:** Boot completion, wake from sleep/grinder, return from any completed flow  

**`thsp`:** 45s

#### ESP-Set Objects

```nextion
user.username0.txt          // user name or "" if slot unused
user.username1.txt
user.username2.txt
user.username3.txt
user.username4.txt
user.username5.txt
user.username6.txt
user.username7.txt
vis user.username0          // 0 if slot unused, 1 if active
vis user.username1
vis user.username2
vis user.username3
vis user.username4
vis user.username5
vis user.username6
vis user.username7
user.page.txt          // "1/2"
vis user.nav_prev           // 0 if on first page
vis user.nav_next           // 0 if on last page
user.inventory.txt         // "Low on Beans!" or ""
--- status bar ---
```

#### Interactions

| Element             | Command to ESP                | ESP action                                   |
| ------------------- | ----------------------------- | -------------------------------------------- |
| User name button    | `PAGE:USER:SELECT:<username>` | Populate `consume`, switch to it             |
| "Coffee List" title | `PAGE:USER:INVENTORY`         | Populate `inventory`, switch to it           |
| Top bar             | `PAGE:USER:INFO`              | Populate `info`, switch to it                |
| Left/right icons    | `PAGE:USER:NAV:<next/prev>`   | Populate `user` with next/prev page of users |

---

### 4.2 Consume

**Page ID:** `consume`  

**Entry point:** `PAGE:USER:SELECT:<username>`  

**`thsp`:** 120s

#### ESP-Set Objects

```nextion
consume.username.txt      // "Christoph"
consume.userbalance.txt   // "-15.40€" (Nextion sets color based on leading "-")
consume.single_price.txt  // "€0.40"
consume.double_price.txt  // "€0.60"
consume.cow_price.txt     // "€0.20"
consume.oat_price.txt     // "€0.30"
vis consume.purchase      // 1 if user is treasurer
--- status bar ---
```

Badges and confirm button state are Nextion-managed.

#### Interactions

| Element        | Command to ESP                                                  | ESP action                                                    |
| -------------- | --------------------------------------------------------------- | ------------------------------------------------------------- |
| Back icon      | `PAGE:CONSUME:BACK`                                             | Populate `user`, switch to it                                 |
| Confirm button | `PAGE:CONSUME:CONFIRM:<username>,<single>,<double>,<cow>,<oat>` | Populate `logging` for consume, switch, initiate backend call |
| User name text | `PAGE:CONSUME:ACCOUNT:<username>`                               | Populate `account`, switch to it                              |
| Purchase icon  | `PAGE:CONSUME:PURCHASE:<username>`                              | Populate `purchase`, switch to it                             |

#### Badge Logic (Nextion-managed)

| Item     | Max | Cycle                 |
| -------- | --- | --------------------- |
| Single   | 1   | 0 → 1 → 0             |
| Double   | 2   | 0 → 1 → 2 → 0         |
| Cow Milk | 4   | 0 → 1 → 2 → 3 → 4 → 0 |
| Oat Milk | 4   | 0 → 1 → 2 → 3 → 4 → 0 |

---

### 4.3 Purchase

**Page ID:** `purchase`  

**Entry points:** purchase icon on `consume`  

**`thsp`:** 120s

#### ESP-Set Objects

```
purchase.username.txt      // "Christoph"
purchase.userbalance.txt   // "-15.40€"
purchase.beans_price.txt   // "30€ / 1kg"
purchase.water_price.txt   // "0.85€ / 2L"
purchase.cow_price.txt     // "1.69€ / 1L"
purchase.oat_price.txt     // "2.55€ / 1L"
--- status bar ---
```

Badges and confirm button state are Nextion-managed.

#### Interactions

| Element        | Command to ESP                                                 | ESP action                                                     |
| -------------- | -------------------------------------------------------------- | -------------------------------------------------------------- |
| Back icon      | `PAGE:PURCHASE:BACK`                                           | Populate `consume`, switch to it                               |
| Confirm button | `PAGE:PURCHASE:CONFIRM:<username>,<beans>,<water>,<cow>,<oat>` | Populate `logging` for purchase, switch, initiate backend call |

#### Badge Logic (Nextion-managed)

| Item     | Max | Cycle           |
| -------- | --- | --------------- |
| Beans    | 9   | 0 → ... → 9 → 0 |
| Water    | 9   | 0 → ... → 9 → 0 |
| Cow Milk | 9   | 0 → ... → 9 → 0 |
| Oat Milk | 9   | 0 → ... → 9 → 0 |

---

### 4.4 Logging (Unified Loading Screen)

**Page ID:** `logging`  

**Entry points:** Confirm on `consume`, Confirm on `purchase`, "I've PayPal'ed" on `balance`, Undo on `logged`  

**`thsp`:** 0 (disabled)

#### ESP-Set Objects

```
logging.username.txt         // "Christoph"
logging.userbalance.txt      // "-15.40€" (pre-transaction)
logging.title.txt            // "Logging..." or "Recording payment" or "Undo"
logging.icon.pic             // picture ID (coffee cup / box / hand / undo arrow)
logging.amount.txt           // "€1.60"
logging.subtitle.txt         // "Logging coffee" / "Logging purchase" /
                             // "Clearing balance" / "Undo last action"
--- status bar ---
```

No interactive elements. The ESP switches away when the backend responds.

#### Configurations

| Context  | title.txt           | icon.pic          | subtitle.txt       |
| -------- | ------------------- | ----------------- | ------------------ |
| Consume  | `Logging...`        | coffee cup        | `Logging coffee`   |
| Purchase | `Logging...`        | box with arrow    | `Logging purchase` |
| Balance  | `Recording payment` | hand with coins   | `Clearing balance` |
| Undo     | `Undo`              | undo arrow (dark) | `Undo last action` |

---

### 4.5 Logged (Unified Done Screen)

**Page ID:** `logged`  

**Entry points:** Completion of any action on `logging`  

**`thsp`:** 30s (safety fallback)  

**Flow timer:** 5000ms

#### ESP-Set Objects

```
logged.username.txt          // "Christoph"
logged.userbalance.txt       // "-17.00€" (updated balance)
logged.amount.txt            // "€1.60"
logged.subtitle.txt          // "Logged coffee" / "Logged purchase" /
                             // "Balance settled" / "Action undone"
vis logged.undo_btn          // 1 to show, 0 to hide
logged.tm0.tim               // 5000
logged.tm0.en                // 1
--- status bar ---
```

#### Configurations

| Context            | subtitle.txt      | undo_btn |
| ------------------ | ----------------- | -------- |
| Coffee order       | `Logged coffee`   | visible  |
| Inventory addition | `Logged purchase` | visible  |
| Balance settlement | `Balance settled` | visible  |
| Undo completed     | `Action undone`   | hidden   |

#### Flow Timer Behavior (5 seconds)

When `PAGE:LOGGED:TIMEOUT` fires:

| Original action    | ESP checks                 | Result                               |
| ------------------ | -------------------------- | ------------------------------------ |
| Coffee order       | User balance vs. threshold | Below → `balance`. At/above → `user` |
| Inventory addition | —                          | → `user`                             |
| Balance settlement | —                          | → `user`                             |
| Undo completed     | —                          | → `user`                             |

#### Interactions

| Element      | Command to ESP                | ESP action                                             |
| ------------ | ----------------------------- | ------------------------------------------------------ |
| Back icon    | `PAGE:LOGGED:BACK`            | Switch to `user`                                       |
| Undo button  | `PAGE:LOGGED:UNDO:<username>` | Populate `logging` for undo, switch, initiate reversal |
| Flow timeout | `PAGE:LOGGED:TIMEOUT`         | Check context and navigate (see table above)           |

---

### 4.6 Account

**Page ID:** `account`  

**Entry point:** Tap user name text on `consume`  

**`thsp`:** 120s

#### ESP-Set Objects

```nextion
account.username.txt           // "Christoph"
account.userbalance.txt        // "-15.40€"
account.row0_date.txt          // "11.01 | 08:34" or ""
account.row0_action.txt        // "Purchase" or ""
account.row0_item.txt          // "Beans" or ""
account.row0_qty.txt           // "5" or ""
account.row1_date.txt
account.row1_action.txt
account.row1_item.txt
account.row1_qty.txt
account.row2_date.txt
account.row2_action.txt
account.row2_item.txt
account.row2_qty.txt
account.row3_date.txt
account.row3_action.txt
account.row3_item.txt
account.row3_qty.txt
account.row4_date.txt
account.row4_action.txt
account.row4_item.txt
account.row4_qty.txt
account.row5_date.txt
account.row5_action.txt
account.row5_item.txt
account.row5_qty.txt
account.row6_date.txt
account.row6_action.txt
account.row6_item.txt
account.row6_qty.txt
account.row7_date.txt
account.row7_action.txt
account.row7_item.txt
account.row7_qty.txt
account.row8_date.txt
account.row8_action.txt
account.row8_item.txt
account.row8_qty.txt
account.row9_date.txt
account.row9_action.txt
account.row9_item.txt
account.row9_qty.txt
--- status bar ---
```

Unused rows are set to empty strings.

#### Table Format

- **Date:** `DD.MM | HH:MM`
- **Action values:** `Consume`, `Purchase`, `Settling`, `Undo`
- **Item values:** `Single`, `Double`, `Cow Milk`, `Oat Milk`, `Beans`, `Water`, `PayPal`
- **Qty:** Integer quantity or euro amount for settlements (e.g. `17.00`)

#### Interactions

| Element        | Command to ESP                    | ESP action                       |
| -------------- | --------------------------------- | -------------------------------- |
| Back icon      | `PAGE:ACCOUNT:BACK`               | Switch to `consume`              |
| Balance button | `PAGE:ACCOUNT:BALANCE:<username>` | Populate `balance`, switch to it |

---

### 4.7 Balance

**Page ID:** `balance`  

**Entry points:** Auto-navigate from `logged` (balance below threshold), "Balance" on `account`  

**`thsp`:** 180s  

**Flow timer:** 180000ms

#### ESP-Set Objects

```
balance.username.txt        // "Christoph"
balance.userbalance.txt     // "-17.00€"
balance.qr.txt              // "https://paypal.me/crsknr/17.00"
balance.paypal_url.txt      // "paypal.me/crsknr/17.00"
balance.tm0.tim             // 180000
balance.tm0.en              // 1
--- status bar ---
```

The Nextion QR component renders the QR code from `qr.txt`.

#### Interactions

| Element        | Command to ESP                   | ESP action                                                    |
| -------------- | -------------------------------- | ------------------------------------------------------------- |
| Back icon      | `PAGE:BALANCE:BACK`              | Switch to `user`, balance unchanged                           |
| Confirm button | `PAGE:BALANCE:PAYPAL:<username>` | Populate `logging` for balance, switch, initiate backend call |
| Flow timeout   | `PAGE:BALANCE:TIMEOUT`           | Switch to `user`, balance unchanged                           |

---

### 4.8 Inventory

**Page ID:** `inventory`  

**Entry point:** Tap "Coffee List" title on `user`  

**`thsp`:** 60s

#### ESP-Set Objects

```
inventory.beans_qty.txt         // "1350g"
inventory.beans_qty.bco         // text background color (see 7.1)
inventory.beans.bco
inventory.beans_bg.pic          // picture ID for green/yellow/red variant
inventory.water_qty.txt         // "4670ml"
inventory.water_qty.bco
inventory.water.bco
inventory.water_bg.pic
inventory.cow_qty.txt           // "4170ml"
inventory.cow_qty.bco
inventory.cow.bco
inventory.cow_bg.pic
inventory.oat_qty.txt           // "6670ml"
inventory.oat_qty.bco
inventory.oat.bco
inventory.oat_bg.pic
--- status bar ---
```

Each `_bg.pic` is set to one of three pre-exported picture IDs (green, yellow, red) based on backend-defined inventory thresholds.

Each `.bco` is set to one of the three colors (green, yellow, red)

Read-only screen.

#### Interactions

| Element   | Command to ESP        | ESP action       |
| --------- | --------------------- | ---------------- |
| Back icon | `PAGE:INVENTORY:BACK` | Switch to `user` |

---

### 4.9 Info

**Page ID:** `info`  

**Entry point:** Tap top bar on `user`  

**`thsp`:** 60s

#### ESP-Set Objects

```
info.wifi_status.txt        // "Connected (NetworkName)" or "Disconnected"
info.backend_status.txt     // "Online" or "Offline"
info.last_sync.txt          // "Today 04:00" or "Just now" or "Sync failed"
info.sd_status.txt          // "OK" or "Error"
info.firmware.txt           // "v1.0.0"
--- status bar ---
```

#### Sync Now Behavior

1. Display sends `PAGE:INFO:SYNC`
2. ESP fetches latest users, products, inventory, config from backend and syncs RTC
3. ESP updates: `info.last_sync.txt="Just now"`
4. Page remains on `info`
5. If offline: `info.last_sync.txt="Sync failed"`

#### Interactions

| Element     | Command to ESP   | ESP action                                      |
| ----------- | ---------------- | ----------------------------------------------- |
| Back icon   | `PAGE:INFO:BACK` | Switch to `user`                                |
| Sync button | `PAGE:INFO:SYNC` | Fetch latest data, sync RTC, update info fields |

---

## 5. Command Protocol

### 5.1 Display → ESP

```serial
# User Select
PAGE:USER:SELECT:<username>
PAGE:USER:INVENTORY
PAGE:USER:INFO
PAGE:USER:NAV:<next/prev>

# Menu Item
PAGE:CONSUME:CONFIRM:<username>,<single>,<double>,<cow>,<oat>
PAGE:CONSUME:BACK
PAGE:CONSUME:ACCOUNT:<username>
PAGE:CONSUME:PURCHASE:<username>

# Menu Inventory
PAGE:PURCHASE:CONFIRM:<username>,<beans>,<water>,<cow>,<oat>
PAGE:PURCHASE:BACK

# Logged (Done screen)
PAGE:LOGGED:UNDO:<username>
PAGE:LOGGED:BACK
PAGE:LOGGED:TIMEOUT

# Account Balance (QR)
PAGE:BALANCE:PAYPAL:<username>
PAGE:BALANCE:BACK
PAGE:BALANCE:TIMEOUT

# Account Info
PAGE:ACCOUNT:BALANCE:<username>
PAGE:ACCOUNT:BACK

# Inventory
PAGE:INVENTORY:BACK

# Info
PAGE:INFO:BACK
PAGE:INFO:SYNC

# System Events (Nextion built-in)
0x86    (Nextion entered sleep)
0x87    (Nextion woke from touch)
```

The `logging` page sends no commands — it has no interactive elements.

### 5.2 ESP → Display

The ESP uses Nextion's native instruction set. Example sequences:

---

## 6. Flow Diagrams

### 6.1 Boot Flow

```flow
[Power On]
    │
    ▼
┌────────┐
│  boot  │  "Connecting to WiFi..."
└───┬────┘  "Syncing clock..."
    │       "Fetching data..."
    │       "Ready" (500ms pause)
    │
    │  WiFi failed?
    │  ├─ cached data exists → proceed with "Offline - using cached data"
    │  └─ no cached data → stay on boot, "No data - connect to WiFi", retry
    │
    ▼
┌────────┐
│  user  │  (normal thsp=45s, goes to sleep on inactivity)
└────────┘
```

### 6.2 Primary Flow — Logging Coffee

```flow
[Sleep]
  │
  ├─ grinder detected ──┐
  ├─ touch (0x87) ───────┤
  │                      ▼
  │               ┌──────────┐
  │               │   user   │◄──────────────────────────────┐
  │               └────┬─────┘                               │
  │                    │ SELECT username                      │
  │                    ▼                                      │
  │               ┌──────────┐                               │
  │               │ consume  │                               │
  │               └────┬─────┘                               │
  │                    │ CONFIRM                              │
  │                    ▼                                      │
  │               ┌──────────┐                               │
  │               │ logging  │  "Logging..." / coffee icon   │
  │               └────┬─────┘                               │
  │                    │ backend responds                     │
  │                    ▼                                      │
  │               ┌──────────┐                               │
  │               │  logged  │  "Done" / "Logged coffee"     │
  │               │          │──── Undo → logging → logged   │
  │               └────┬─────┘                               │
  │                    │ 5s TIMEOUT                           │
  │          ┌─────────┴──────────┐                          │
  │          │                    │                           │
  │  balance < threshold   balance ≥ threshold               │
  │          │                    │                           │
  │          ▼                    └───────────────────────────┘
  │   ┌──────────────┐
  │   │   balance    │
  │   └──────┬───────┘
  │          │
  │   ┌──────┴───────┐
  │   │              │
  │  ← BACK      PAYPAL
  │ (pay later)      │
  │   │              ▼
  │   │     ┌──────────┐
  │   │     │ logging  │  "Recording payment"
  │   │     └────┬─────┘
  │   │          │
  │   │          ▼
  │   │     ┌──────────┐
  │   │     │  logged  │  "Balance settled"
  │   │     │          │──── Undo → logging → logged
  │   │     └────┬─────┘
  │   │          │ 5s TIMEOUT
  │   └──────────┴───────────────────────────────────────────┘
```

### 6.3 Inventory Flow

```flow
  consume
      │
      │ ADD_INVENTORY
      ▼
┌──────────┐
│ purchase │
└────┬─────┘
     │ CONFIRM
     ▼
┌──────────┐
│ logging  │  "Logging..." / box icon
└────┬─────┘
     │
     ▼
┌──────────┐
│  logged  │  "Done" / "Logged inventory"
│          │──── Undo → logging → logged
└────┬─────┘
     │ 5s TIMEOUT
     ▼
   user
```

### 6.4 Account Info Flow

```flow
  consume
      │
      │ ACCOUNT username
      ▼
┌──────────────┐
│ account │
└──────┬───────┘
       │
       │          
    SETTLE    
       │          
       ▼            
    balance    
   (see §6.2)
```

### 6.5 Undo Flow

```flow
  logged (any variant)
      │
      │ UNDO username
      ▼
┌──────────┐
│ logging  │  "Undo" / undo arrow / "Undo last action"
└────┬─────┘
     │ backend responds
     ▼
┌──────────┐
│  logged  │  "Undo" / "Action undone"
│          │  (Undo button hidden)
└────┬─────┘
     │ 5s TIMEOUT
     ▼
    user
```

---

## 7. Visual Design

### 7.1 Color Palette

| Element    | hex       | 65536   |
| ---------- | --------- | ------- |
| Background | `#fffbf9` | `65503` |
| Button     | `#e6e0dd` | `59163` |
| Text       | `#372a29` | `12613` |
| Green      | `#c0cf82` | `50800` |
| Red        | `#ab5651` | `43690` |
| Yellow     | `#ba9a67` | `48332` |

### 7.2 Typography

| Usage  | Style    |
| ------ | -------- |
| Body   | 15       |
| Button | 20, blod |
| Title  | 28, bold |

### 7.3 Icons

Icons selected by the ESP via picture ID.

| Icon                   | Picture ID | Usage            |
| ---------------------- | ---------- | ---------------- |
| Coffee cup with steam  | 19         | Consume logging  |
| Box with up arrow      | 20         | Purchase logging |
| Hand with coins        | 21         | Balance  logging |
| Undo arrow             | 22         | Undo logging     |
| Inventory beans green  | 26         | Beans inventory  |
| Inventory beans yellow | 25         | Beans inventory  |
| Inventory beans red    | 25         | Beans inventory  |
| Inventory water green  | 29         | Water inventory  |
| Inventory water yellow | 28         | Water inventory  |
| Inventory water red    | 27         | Water inventory  |
| Inventory cow green    | 32         | Cow inventory    |
| Inventory cow yellow   | 31         | Cow inventory    |
| Inventory cow red      | 30         | Cow inventory    |
| Inventory oat green    | 35         | Oat inventory    |
| Inventory oat yellow   | 34         | Oat inventory    |
| Inventory oat red      | 33         | Oat inventory    |

### 7.4 Animations (TBD)

| Animation    | Details                                               |
| ------------ | ----------------------------------------------------- |
| Grinder wake | Slow edge pulse, warm color, 2-3 cycles at ~1.5s each |

---
