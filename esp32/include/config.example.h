#ifndef CONFIG_H
#define CONFIG_H

// ============================================
// Network
// ============================================
#define WIFI_HOSTNAME       "Coffee Tracker"
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
// Sleep Timeouts (thsp in s)
// ============================================
#define THSP_USER           120
#define THSP_CONSUME        120
#define THSP_PURCHASE       120
#define THSP_LOGGING        0
#define THSP_LOGGED         30
#define THSP_ACCOUNT        120
#define THSP_BALANCE        180
#define THSP_INVENTORY      60
#define THSP_INFO           60

// ============================================
// Audio
// ============================================
#define ENABLE_AUDIO_WAKE           false
#define I2S_SAMPLE_RATE             16000
#define XCORR_FRAME_SIZE            64        // samples per xcorr frame (4 ms @ 16 kHz)
#define XCORR_THRESHOLD             0.3f      // detection threshold (0–1)
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
// Firmware
// ============================================
#define FIRMWARE_VERSION    "v1.0.1"

#endif
