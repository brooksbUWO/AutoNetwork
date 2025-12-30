/**
 * @file OLEDExample.ino
 * @brief OLED display integration example for AutoNetwork v0.0.1
 *
 * @details
 * Demonstrates complete OLED integration with AutoNetwork library showing:
 * - Six WiFi lifecycle states with visual feedback
 * - QR code generation for AP connection and web access
 * - Serial monitor output mirroring OLED display content
 * - State-based display architecture with callbacks
 * - NTP time synchronization and runtime statistics
 * - Automatic state transitions based on WiFi status
 *
 * Hardware Requirements:
 * - ESP32 (tested: DOIT DevKit v1)
 * - SSD1306 OLED 128x64 I2C display (address 0x3C)
 * - Standard I2C pins (SDA, SCL)
 *
 * Display Layout:
 * - Yellow section (top 16px): Status header
 * - Blue section (bottom 48px): Content area
 * - QR codes: 34x34 pixels (size 2, scale 2)
 *
 * States:
 * 1. Initialization - Boot message
 * 2. WiFi Setup - AP mode with QR code
 * 3. Connected - IP address with QR code
 * 4. Runtime - Live time/date/uptime/IP
 * 5. Connecting - Connection attempt feedback
 * 6. Failed - Connection error message
 *
 * @version 0.0.1
 * @date 2025-11-14
 *
 * Revision History:
 * When         Who         Description of change
 * -----------  ----------- -----------------------
 * 14-NOV-2025  Claude      Initial implementation
 */

// Include Files
// ****************************************************************************
#include <Arduino.h>
#include <WiFi.h>
#include "esp_log.h"
#include <AutoNetwork.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "SSD1306Wire.h"  // OLED display driver
#include "qrcode.h"       // QR code generation library

// Configuration Constants
// ****************************************************************************
// Display Constants
const uint8_t DISPLAY_WIDTH = 128;
const uint8_t DISPLAY_HEIGHT = 64;
const uint8_t DISPLAY_ADDRESS = 0x3C;
const uint8_t YELLOW_SECTION_HEIGHT = 16;
const uint8_t BLUE_SECTION_HEIGHT = 48;

// QR Code Constants
const uint8_t QR_CODE_VERSION = 2;      // QR code version (17x17 modules)
const uint8_t QR_CODE_SCALE = 2;        // Each module = 2x2 pixels
const uint8_t QR_MARGIN_RIGHT = 4;      // Right margin in pixels
const uint8_t QR_TEXT_MAX_CHARS = 11;   // Max characters before text wrapping

// Timing Constants
const uint16_t INTERVAL_DISPLAY_UPDATE = 1000;  // Display update interval (ms)
const uint32_t TIME_VALID_MIN = 1000000000;     // Min valid Unix timestamp
const uint32_t SYNC_RETRY_INTERVAL = 30000;     // NTP retry interval (30s)

// Time Configuration
const char *TZ_INFO = "CST6CDT,M3.2.0,M11.1.0";  // Chicago timezone (adjust for your location)
const char *NTP_SERVER = "pool.ntp.org";
const char *NTP_SERVER2 = "time.nist.gov";

// Logging Tags
// ****************************************************************************
static const char* TAG_MAIN = "OLEDExample";

// Global Objects
// ****************************************************************************
AsyncWebServer server(80);
AutoNetwork autonetwork(&server);
AutoNetworkConfig config;
SSD1306Wire display(DISPLAY_ADDRESS, SDA, SCL, GEOMETRY_128_64);

// State Variables
// ****************************************************************************
uint32_t displayUpdateTime = 0;         // Last display update timestamp
bool webpageAccessed = false;           // Triggers State 4 when true
static uint32_t lastSyncAttempt = 0;    // Last NTP sync attempt

// Function Prototypes
// ****************************************************************************
void displayStatusCard(const char *statusValue, const char *detailLabel,
                      const char *detailValue, const char *qrPayload);
void displayUpdate();
void setupTimeSync();
String getMacAddress();

/**
 * @brief Arduino setup function - runs once at boot
 */
void setup()
{
    // Initialize serial communication
    Serial.begin(115200);
    delay(1000);

    // Configure logging
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("OLEDExample", ESP_LOG_INFO);
    esp_log_level_set("AutoNetwork", ESP_LOG_INFO);

    Serial.println("\n========================================");
    Serial.println("AutoNetwork OLED Display Example");
    Serial.println("========================================\n");

    // Initialize OLED display
    Serial.println("[Display] Initializing OLED...");
    display.init();
    display.flipScreenVertically();
    display.setFont(ArialMT_Plain_10);
    Serial.println("[Display] OLED initialized successfully");

    // STATE 1: Display initialization message
    Serial.println("\n=== OLED State 1: Initialization ===");
    Serial.println("Status: Initializing...");
    Serial.println("Message: Please stand by for connection info");

    display.clear();
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 0, "AutoNetwork Demo");
    display.drawHorizontalLine(0, 12, 128);
    display.drawString(64, 22, "Initializing...");
    display.drawString(64, 38, "Please stand by");
    display.drawString(64, 50, "for connection info");
    display.display();

    // Configure AutoNetwork
    String macAddress = getMacAddress();
    config.apSSID = "ESP32_" + macAddress;
    config.apPassword = "12345678";
    config.staHostName = "esp32-oled-demo";
    config.timeoutPortalMs = 300000;        // 5 minutes
    config.timeoutConnectMs = 30000;        // 30 seconds
    config.staAutoReconnect = true;
    config.credentialSaveMode = AutoNetworkCredentialSaveMode::ALWAYS;

    Serial.println("\n[AutoNetwork] Configuration:");
    Serial.printf("AP SSID: %s\n", config.apSSID.c_str());
    Serial.printf("AP Password: %s\n", config.apPassword.c_str());
    Serial.printf("Hostname: %s\n", config.staHostName.c_str());

    autonetwork.config(config);

    // Register callback for webpage access detection
    autonetwork.onWebpageAccessed([]() {
        if (!webpageAccessed) {
            webpageAccessed = true;
            Serial.println("\n[Callback] Webpage accessed - activating runtime display");
        }
    });

    // Register callback for WiFi connection status changes
    autonetwork.onConnectionStatus([](AutoNetworkConnectionStatus status) {
        switch (status)
        {
        case AutoNetworkConnectionStatus::CONNECTED:
        {
            // STATE 3: WiFi Connected - Display IP with QR code
            Serial.println("\n=== OLED State 3: WiFi Connected ===");
            Serial.printf("SSID: %s\n", WiFi.SSID().c_str());
            Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
            Serial.printf("Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
            Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
            Serial.println("Scan QR code or navigate to IP address");

            String webUrl = "http://" + WiFi.localIP().toString();
            String webpage = WiFi.localIP().toString();
            displayStatusCard("WiFi Connected", "Webpage:", webpage.c_str(), webUrl.c_str());

            // Start NTP time synchronization
            setupTimeSync();
            break;
        }

        case AutoNetworkConnectionStatus::CONNECTING:
        {
            // STATE 5: Connecting - Show connection attempt
            Serial.println("\n=== OLED State 5: Connecting ===");
            Serial.printf("Attempting to connect to: %s\n", autonetwork.getSSID());
            Serial.println("Please wait...");

            display.clear();
            display.setTextAlignment(TEXT_ALIGN_CENTER);
            display.drawString(64, 0, "Connecting...");
            display.drawHorizontalLine(0, 12, 128);
            display.drawString(64, 22, "Attempting to connect to:");
            display.drawString(64, 38, autonetwork.getSSID());
            display.display();
            break;
        }

        case AutoNetworkConnectionStatus::CONNECTION_FAILED:
        {
            // STATE 6: Connection Failed - Show error message
            Serial.println("\n=== OLED State 6: Connection Failed ===");
            Serial.println("Failed to connect to network");
            Serial.println("Please check:");
            Serial.println("- WiFi credentials are correct");
            Serial.println("- Network is available and in range");
            Serial.println("- Router is functioning properly");

            display.clear();
            display.setTextAlignment(TEXT_ALIGN_CENTER);
            display.drawString(64, 0, "Connection Failed");
            display.drawHorizontalLine(0, 12, 128);
            display.drawString(64, 22, "Please check credentials");
            display.drawString(64, 38, "and network availability.");
            display.display();
            break;
        }

        case AutoNetworkConnectionStatus::CONNECTION_LOST:
        case AutoNetworkConnectionStatus::DISCONNECTED:
        {
            // STATE 2: WiFi Setup - Return to AP mode
            Serial.println("\n=== OLED State 2: WiFi Setup (AP Mode) ===");
            Serial.printf("AP SSID: %s\n", config.apSSID.c_str());
            Serial.printf("AP Password: %s\n", config.apPassword.c_str());
            Serial.println("Scan QR code to connect, or connect manually");
            Serial.println("Then navigate to 192.168.4.1 to configure WiFi");

            String wifiPayload = "WIFI:S:" + config.apSSID + ";T:WPA;P:" + config.apPassword + ";;";
            displayStatusCard("WiFi Setup", "AP SSID:", config.apSSID.c_str(), wifiPayload.c_str());
            break;
        }

        default:
            break;
        }
    });

    // Configure root page with setRootContent()
    autonetwork.setRootContent(R"rawliteral(
<!DOCTYPE html>
<html>
<head><title>OLED Demo</title></head>
<body>
    <h1>AutoNetwork OLED Display</h1>
    <p>Check OLED for WiFi status and QR codes.</p>
    <p>{{AUTONETWORK_MENU}}</p>
</body>
</html>
)rawliteral");

    // Start AutoNetwork portal
    Serial.println("\n[AutoNetwork] Starting autonetwork...");
    bool connected = autonetwork.autoConnect();

    if (connected) {
        Serial.println("[AutoNetwork] Successfully connected to WiFi");
    } else {
        Serial.println("[AutoNetwork] Portal started in AP mode");

        // Display AP setup info (STATE 2)
        Serial.println("\n=== OLED State 2: WiFi Setup (AP Mode) ===");
        Serial.printf("AP SSID: %s\n", config.apSSID.c_str());
        Serial.printf("AP Password: %s\n", config.apPassword.c_str());
        Serial.println("Scan QR code to connect, or connect manually");

        String wifiPayload = "WIFI:S:" + config.apSSID + ";T:WPA;P:" + config.apPassword + ";;";
        displayStatusCard("WiFi Setup", "AP SSID:", config.apSSID.c_str(), wifiPayload.c_str());
    }

    Serial.println("\n[Setup] Complete - entering main loop");
}

/**
 * @brief Arduino main loop - runs continuously
 */
void loop()
{
    // Required for AutoNetwork operation
    autonetwork.loop();

    // STATE 4: Runtime Display - Update every second when webpage accessed
    if (webpageAccessed && WiFi.status() == WL_CONNECTED) {
        if (millis() - displayUpdateTime >= INTERVAL_DISPLAY_UPDATE) {
            displayUpdateTime = millis();
            displayUpdate();
        }
    }
}

// Helper Functions
// ****************************************************************************

/**
 * @brief Display status card with QR code for States 2 and 3
 *
 * Layout:
 * - Yellow section: Status text centered + horizontal line
 * - Blue section: Detail info (left) + QR code (right)
 *
 * @param statusValue Status text (e.g., "WiFi Setup", "WiFi Connected")
 * @param detailLabel Label text (e.g., "AP SSID:", "Webpage:")
 * @param detailValue Value text (e.g., SSID or IP address)
 * @param qrPayload QR code data (WiFi config or URL)
 */
void displayStatusCard(const char *statusValue, const char *detailLabel,
                      const char *detailValue, const char *qrPayload)
{
    // Generate QR code
    QRCode qrcode;
    uint8_t qrcodeData[qrcode_getBufferSize(QR_CODE_VERSION)];
    qrcode_initText(&qrcode, qrcodeData, QR_CODE_VERSION, ECC_LOW, qrPayload);

    display.clear();
    display.setFont(ArialMT_Plain_10);

    // Yellow section: Status header
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 0, statusValue);
    display.drawHorizontalLine(0, 12, 128);

    // Blue section: Information and QR code
    display.setTextAlignment(TEXT_ALIGN_LEFT);

    String displayText = String(detailValue);
    display.drawString(2, 18, "Connect to");

    // Text wrapping logic to avoid QR code overlap
    // Available text width: ~85px (14 chars with ArialMT_Plain_10)
    if (displayText.length() <= QR_TEXT_MAX_CHARS) {
        // Short text - single line
        display.drawString(2, 30, displayText);
    }
    else if (displayText.length() <= QR_TEXT_MAX_CHARS * 2) {
        // Medium text - two lines
        String line1 = displayText.substring(0, QR_TEXT_MAX_CHARS);
        display.drawString(2, 30, line1);

        String line2 = displayText.substring(QR_TEXT_MAX_CHARS);
        display.drawString(2, 42, line2);
    }
    else {
        // Long text - three lines
        String line1 = displayText.substring(0, QR_TEXT_MAX_CHARS);
        display.drawString(2, 30, line1);

        String line2 = displayText.substring(QR_TEXT_MAX_CHARS, QR_TEXT_MAX_CHARS * 2);
        display.drawString(2, 42, line2);

        if (displayText.length() > QR_TEXT_MAX_CHARS * 2) {
            String line3 = displayText.substring(QR_TEXT_MAX_CHARS * 2);
            display.drawString(2, 54, line3);
        }
    }

    // Right side: QR code (34x34 pixels)
    int qrCodeSize = qrcode.size * QR_CODE_SCALE;
    int x_offset = DISPLAY_WIDTH - qrCodeSize - QR_MARGIN_RIGHT;
    int y_offset = YELLOW_SECTION_HEIGHT + (BLUE_SECTION_HEIGHT - qrCodeSize) / 2;

    for (uint8_t y = 0; y < qrcode.size; y++) {
        for (uint8_t x = 0; x < qrcode.size; x++) {
            if (qrcode_getModule(&qrcode, x, y)) {
                display.fillRect(x_offset + x * QR_CODE_SCALE,
                               y_offset + y * QR_CODE_SCALE,
                               QR_CODE_SCALE, QR_CODE_SCALE);
            }
        }
    }

    display.display();
}

/**
 * @brief State 4: Runtime display with live time, date, uptime, and IP
 *
 * Called every second from loop() after webpage is accessed
 * Shows NTP-synchronized time and system statistics
 */
void displayUpdate()
{
    bool isConnected = (WiFi.status() == WL_CONNECTED);

    // If disconnected during runtime, revert to State 2 (AP Setup)
    if (!isConnected) {
        Serial.println("\n[DisplayUpdate] WiFi disconnected - reverting to AP mode");
        String wifiPayload = "WIFI:S:" + config.apSSID + ";T:WPA;P:" + config.apPassword + ";;";
        displayStatusCard("WiFi Setup", "AP SSID:", config.apSSID.c_str(), wifiPayload.c_str());
        return;
    }

    // Get current time
    time_t now = time(nullptr);
    tm *timeInfo = localtime(&now);
    bool timeValid = (now > TIME_VALID_MIN);

    // Calculate uptime (days, hours, minutes)
    uint32_t uptimeSec = millis() / 1000;
    uint32_t days = uptimeSec / 86400;
    uint32_t hours = (uptimeSec % 86400) / 3600;
    uint32_t minutes = (uptimeSec % 3600) / 60;

    display.clear();
    display.setFont(ArialMT_Plain_10);

    // Yellow section: Project name
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 0, "AutoNetwork Demo");
    display.drawHorizontalLine(0, 12, 128);

    // Blue section: Runtime information
    display.setTextAlignment(TEXT_ALIGN_LEFT);

    // Line 1: Date and Time
    if (timeValid) {
        char dateTimeStr[32];
        snprintf(dateTimeStr, sizeof(dateTimeStr), "%04d-%02d-%02d   %02d:%02d:%02d",
                 timeInfo->tm_year + 1900,
                 timeInfo->tm_mon + 1,
                 timeInfo->tm_mday,
                 timeInfo->tm_hour,
                 timeInfo->tm_min,
                 timeInfo->tm_sec);
        display.drawString(2, 18, dateTimeStr);
    }
    else {
        // Time not synced yet - retry NTP
        if (millis() - lastSyncAttempt > SYNC_RETRY_INTERVAL) {
            setupTimeSync();
            lastSyncAttempt = millis();
        }
        display.drawString(2, 18, "Syncing time...");
    }

    // Line 2: Runtime/Uptime
    char runtimeStr[24];
    snprintf(runtimeStr, sizeof(runtimeStr), "Runtime: %02lud %02luh %02lum",
             days, hours, minutes);
    display.drawString(2, 30, runtimeStr);

    // Line 3: IP address
    String ipAddress = WiFi.localIP().toString();
    display.drawString(2, 42, ipAddress);

    // Line 4: Available for custom project information
    display.drawString(2, 54, "");

    display.display();

    // Mirror to Serial (only once per minute to reduce spam)
    static uint32_t lastSerialOutput = 0;
    if (millis() - lastSerialOutput >= 60000) {
        lastSerialOutput = millis();
        Serial.println("\n=== OLED State 4: Runtime Display ===");
        if (timeValid) {
            Serial.printf("Date/Time: %04d-%02d-%02d %02d:%02d:%02d\n",
                         timeInfo->tm_year + 1900, timeInfo->tm_mon + 1, timeInfo->tm_mday,
                         timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec);
        }
        Serial.printf("Runtime: %02lud %02luh %02lum\n", days, hours, minutes);
        Serial.printf("IP Address: %s\n", ipAddress.c_str());
    }
}

/**
 * @brief Initialize NTP time synchronization
 *
 * Starts background NTP sync with configured timezone
 * Non-blocking - time becomes available asynchronously
 */
void setupTimeSync()
{
    Serial.println("[Time] Configuring NTP time synchronization...");
    configTime(0, 0, NTP_SERVER, NTP_SERVER2);
    setenv("TZ", TZ_INFO, 1);
    tzset();
    Serial.println("[Time] NTP sync started in background");
}

/**
 * @brief Get MAC address as formatted string
 *
 * @return String MAC address in format AABBCCDDEEFF
 */
String getMacAddress()
{
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char macStr[13];
    snprintf(macStr, sizeof(macStr), "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(macStr);
}
