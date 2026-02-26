#ifndef CONFIG_H
#define CONFIG_H

// ============================================
// Network
// ============================================
#define WIFI_SSID           "YOUR_WIFI_SSID"
#define WIFI_PASSWORD       "YOUR_WIFI_PASSWORD"
#define BACKEND_URL         "https://script.google.com/macros/s/YOUR_DEPLOYMENT_ID/exec"

// ============================================
// Pin Assignments
// ============================================
#define NEXTION_RX_PIN      44
#define NEXTION_TX_PIN      43
#define SD_CS_PIN           10
#define SD_MOSI_PIN         11
#define SD_MISO_PIN         13
#define SD_SCK_PIN          12
#define I2S_WS_PIN          4
#define I2S_SCK_PIN         5
#define I2S_SD_PIN          6

// ============================================
// Timing
// ============================================
#define WIFI_CONNECT_TIMEOUT_MS     5000
#define HTTP_TIMEOUT_MS             10000
#define RECONNECT_INTERVAL_MS       60000
#define BOOT_READY_DELAY_MS         500
#define LOGGED_FLOW_TIMER_MS        5000
#define BALANCE_FLOW_TIMER_MS       180000
#define NEXTION_BUFFER_SIZE         256
#define BUFFER_TIMEOUT_MS           2000

// ============================================
// Sleep Timeouts (thsp in ms)
// ============================================
#define THSP_USER           45000
#define THSP_CONSUME        120000
#define THSP_PURCHASE       120000
#define THSP_LOGGING        0
#define THSP_LOGGED         30000
#define THSP_ACCOUNT        120000
#define THSP_BALANCE        180000
#define THSP_INVENTORY      60000
#define THSP_INFO           60000

// ============================================
// Audio
// ============================================
#define ENABLE_AUDIO_WAKE           false
#define I2S_SAMPLE_RATE             16000
#define XCORR_FRAME_SIZE            64        // samples per xcorr frame (4 ms @ 16 kHz)
#define XCORR_THRESHOLD             0.4f      // detection threshold (0–1)
#define XCORR_SMOOTH_WINDOW         64        // EMA window in frames
#define GRINDER_SUSTAINED_MS        1500      // ms above threshold before triggering
#define RECORD_DURATION_MS          3000      // on-device reference recording length
#define GRINDER_REF_PATH            "/grinder_ref.bin"

// ============================================
// SD Paths
// ============================================
#define CONFIG_CACHE_PATH           "/config.json"
#define OFFLINE_LOG_PATH            "/offline.json"
#define TRANSACTION_LOG_PATH        "/transactions.log"

// ============================================
// Data Limits
// ============================================
#define MAX_USERS           16
#define MAX_PRODUCTS        8
#define MAX_INVENTORY       8

// ============================================
// Picture IDs (from Nextion HMI)
// ============================================
#define PIC_ICON_COFFEE     19
#define PIC_ICON_PURCHASE   20
#define PIC_ICON_BALANCE    21
#define PIC_ICON_UNDO       22

#define PIC_BEANS_GREEN     26
#define PIC_BEANS_YELLOW    25
#define PIC_BEANS_RED       24
#define PIC_WATER_GREEN     29
#define PIC_WATER_YELLOW    28
#define PIC_WATER_RED       27
#define PIC_COW_GREEN       32
#define PIC_COW_YELLOW      31
#define PIC_COW_RED         30
#define PIC_OAT_GREEN       35
#define PIC_OAT_YELLOW      34
#define PIC_OAT_RED         33

// ============================================
// Inventory Colors (Nextion 65536-format .bco)
// ============================================
#define COLOR_GREEN         50800
#define COLOR_YELLOW        48332
#define COLOR_RED           43690

// ============================================
// Firmware
// ============================================
#define FIRMWARE_VERSION    "v1.0.0"

#endif
