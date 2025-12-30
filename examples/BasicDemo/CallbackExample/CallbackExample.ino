/**
 * @file CallbackExample.ino
 * @brief Demonstrates AutoNetwork callback registration and usage
 *
 * @details
 * Demonstrates:
 * - Connection status callbacks with comprehensive state handling
 * - Portal state callbacks with user guidance
 * - ESP-IDF logging integration (INFO, WARN, ERROR levels)
 * - Application state tracking using callback events
 * - Custom web endpoints (/status API)
 * - Best practices for callback implementation
 * - Callback safety guidelines and common use cases
 *
 * This example provides detailed documentation on:
 * - Keeping callbacks short and non-blocking
 * - Avoiding network operations in callbacks
 * - Thread safety considerations
 * - Error handling patterns
 * - Display updates, LED indicators, and service management
 *
 * Hardware:
 * - ESP32 board
 *
 * @version 0.0.1
 * @date 2025-11-14
 */

// ****************************************************************************
// Title        : AutoNetwork Callback Example
// Filename     : 'CallbackExample.ino'
// Target MCU   : ESP32
// Description  : Demonstrates AutoNetwork callback registration and usage
//                Shows how to handle WiFi connection and portal state events
//                Includes logging patterns and best practices
//
// Revision History:
// When         Who         Description of change
// -----------  ----------- -----------------------
// 14-NOV-2025  Brooks      Updated to v0.0.1 with Doxygen headers
// 09-OCT-2025  Brooks      Initial implementation
//
// ****************************************************************************

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AutoNetwork.h>
#include "esp_log.h"

// Logging Tags
// ****************************************************************************
static const char* TAG_MAIN = "MainApp";

// Globals
// ****************************************************************************
AsyncWebServer server(80);
AutoNetwork autonetwork(&server);
AutoNetworkConfig config;

// Connection state tracking (optional - for user application logic)
bool wifiConnected = false;
bool portalActive = false;
String currentSSID = "";
IPAddress currentIP;

// Setup Code
// ****************************************************************************
void setup()
{
    Serial.begin(115200);
    delay(1000);

    // Configure logging
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("MainApp", ESP_LOG_INFO);
    esp_log_level_set("AutoNetwork", ESP_LOG_INFO);

    Serial.println();
    Serial.println("========================================");
    Serial.println("AutoNetwork Callback Example");
    Serial.println("========================================");
    Serial.println();

    // Configure AutoNetwork
    config.apSSID = "AutoNetwork_Callbacks";
    config.apPassword = "12345678";
    config.staHostName = "callbacks-demo";
    config.timeoutPortalMs = 300000;  // 5 minutes
    config.timeoutConnectMs = 30000;  // 30 seconds
    config.staAutoReconnect = true;
    config.portalRetain = false;    // Close portal after successful connection

    autonetwork.config(config);

    Serial.println("Registering callbacks...");
    Serial.println();

    // ========================================================================
    // CALLBACK 1: WiFi Connection Status Changes
    // ========================================================================
    // This callback is triggered whenever the WiFi connection status changes
    // Use this to update UI, control hardware, or trigger application logic

    autonetwork.onConnectionStatus([](AutoNetworkConnectionStatus status)
    {
        // Handle different connection states
        switch (status)
        {
        case AutoNetworkConnectionStatus::CONNECTED:
            // Successfully connected to WiFi network
            ESP_LOGI(TAG_MAIN, "╔══════════════════════════════════════╗");
            ESP_LOGI(TAG_MAIN, "║     WiFi Connected Successfully!     ║");
            ESP_LOGI(TAG_MAIN, "╚══════════════════════════════════════╝");
            ESP_LOGI(TAG_MAIN, "SSID:       %s", WiFi.SSID().c_str());
            ESP_LOGI(TAG_MAIN, "IP Address: %s", WiFi.localIP().toString().c_str());
            ESP_LOGI(TAG_MAIN, "Gateway:    %s", WiFi.gatewayIP().toString().c_str());
            ESP_LOGI(TAG_MAIN, "RSSI:       %d dBm", WiFi.RSSI());
            Serial.println();

            // Update application state
            wifiConnected = true;
            currentSSID = WiFi.SSID();
            currentIP = WiFi.localIP();
            break;

        case AutoNetworkConnectionStatus::CONNECTING:
            // Connection attempt in progress
            ESP_LOGI(TAG_MAIN, "⏳ Connecting to WiFi...");
            break;

        case AutoNetworkConnectionStatus::CONNECTION_FAILED:
            // Connection attempt failed
            ESP_LOGW(TAG_MAIN, "❌ WiFi Connection Failed");
            ESP_LOGW(TAG_MAIN, "   Check credentials and network availability");
            Serial.println();

            // Update application state
            wifiConnected = false;
            break;

        case AutoNetworkConnectionStatus::CONNECTION_LOST:
            // Previously connected but lost connection
            ESP_LOGW(TAG_MAIN, "⚠️  WiFi Connection Lost");
            ESP_LOGW(TAG_MAIN, "   AutoNetwork will attempt reconnection...");
            Serial.println();

            // Update application state
            wifiConnected = false;
            break;

        case AutoNetworkConnectionStatus::DISCONNECTED:
            // WiFi explicitly disconnected
            ESP_LOGI(TAG_MAIN, "🔌 WiFi Disconnected");

            // Update application state
            wifiConnected = false;
            currentSSID = "";
            break;

        default:
            // Handle any other status (future-proofing)
            break;
        }
    });

    // ========================================================================
    // CALLBACK 2: Captive Portal State Changes
    // ========================================================================
    // This callback is triggered when the captive portal state changes
    // Use this to track portal lifecycle and user configuration progress

    autonetwork.onPortalState([](AutoNetworkPortalState state)
    {
        // Handle different portal states
        switch (state)
        {
        case AutoNetworkPortalState::IDLE:
            // Portal is not active
            ESP_LOGI(TAG_MAIN, "Portal: Idle (not running)");
            portalActive = false;
            break;

        case AutoNetworkPortalState::WAITING_FOR_CONNECTION:
            // Portal is active and waiting for user to configure WiFi
            ESP_LOGI(TAG_MAIN, "╔══════════════════════════════════════╗");
            ESP_LOGI(TAG_MAIN, "║      Captive Portal Active!          ║");
            ESP_LOGI(TAG_MAIN, "╚══════════════════════════════════════╝");
            ESP_LOGI(TAG_MAIN, "1. Connect to WiFi network: %s", config.apSSID.c_str());
            ESP_LOGI(TAG_MAIN, "2. Open browser (should auto-redirect)");
            ESP_LOGI(TAG_MAIN, "3. Or navigate to: http://%s", WiFi.softAPIP().toString().c_str());
            ESP_LOGI(TAG_MAIN, "4. Configure your WiFi credentials");
            Serial.println();

            // Update application state
            portalActive = true;
            break;

        case AutoNetworkPortalState::CONNECTING:
            // User submitted credentials, attempting connection
            ESP_LOGI(TAG_MAIN, "Portal: Testing credentials...");
            break;

        case AutoNetworkPortalState::SUCCESS:
            // Portal configuration completed successfully
            ESP_LOGI(TAG_MAIN, "✅ Portal: Configuration successful!");

            if (!config.portalRetain)
            {
                ESP_LOGI(TAG_MAIN, "   Portal will close automatically");
                ESP_LOGI(TAG_MAIN, "   Device will operate in Station mode");
            }
            else
            {
                ESP_LOGI(TAG_MAIN, "   Portal retained (AP+STA mode)");
                ESP_LOGI(TAG_MAIN, "   You can reconfigure at any time");
            }
            Serial.println();
            break;

        case AutoNetworkPortalState::FAILED:
            // Portal configuration failed (wrong credentials)
            ESP_LOGW(TAG_MAIN, "❌ Portal: Configuration failed");
            ESP_LOGW(TAG_MAIN, "   Portal remains open for retry");
            Serial.println();
            break;

        case AutoNetworkPortalState::TIMEOUT:
            // Portal timed out waiting for configuration
            ESP_LOGW(TAG_MAIN, "⏱️  Portal: Timeout");

            if (!config.portalRetain)
            {
                ESP_LOGW(TAG_MAIN, "   Portal will close");
            }
            else
            {
                ESP_LOGW(TAG_MAIN, "   Portal retained");
            }
            Serial.println();
            break;

        default:
            // Handle any other portal state (future-proofing)
            break;
        }
    });

    Serial.println("✅ Callbacks registered successfully");
    Serial.println();

    // ========================================================================
    // Configure Root Page Content
    // ========================================================================

    // Configure root page with dynamic content
    autonetwork.setRootContent([]() {
        String html = "<!DOCTYPE html><html><head><title>Callback Demo</title></head><body>";
        html += "<h1>AutoNetwork Callback Example</h1>";
        html += "<p>Current millis: " + String(millis()) + "</p>";
        html += "<p>{{AUTONETWORK_MENU}}</p>";
        html += "</body></html>";
        return html;
    });

    // ========================================================================
    // Optional: Add custom web endpoints
    // ========================================================================

    // Example: Status endpoint showing current connection state
    autonetwork.on("/status", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        String json = "{";
        json += "\"connected\":" + String(wifiConnected ? "true" : "false") + ",";
        json += "\"ssid\":\"" + currentSSID + "\",";
        json += "\"ip\":\"" + currentIP.toString() + "\",";
        json += "\"portal\":" + String(portalActive ? "true" : "false");
        json += "}";

        request->send(200, "application/json", json);
    });

    // ========================================================================
    // Start AutoNetwork
    // ========================================================================

    Serial.println("Starting AutoNetwork...");
    Serial.println();

    if (autonetwork.begin())
    {
        // Connected to WiFi on startup (using saved credentials)
        Serial.println("Connected on startup!");
        Serial.println("Access AutoNetwork menu: http://" + WiFi.localIP().toString() + "/_an");
    }
    else
    {
        // No saved credentials or connection failed
        // Captive portal is now active (check callback output above)
        Serial.println("Portal started - waiting for configuration");
    }

    Serial.println();
    Serial.println("========================================");
    Serial.println("Setup Complete - Monitor Callbacks Above");
    Serial.println("========================================");
    Serial.println();
}

// Main Program Loop
// ****************************************************************************
void loop()
{
    // REQUIRED: Call autonetwork.loop() to process AutoNetwork tasks
    autonetwork.loop();

    delay(10);
}

// ****************************************************************************
// CALLBACK BEST PRACTICES
// ****************************************************************************
//
// 1. KEEP CALLBACKS SHORT AND NON-BLOCKING
//    - Avoid long delays, blocking operations, or heavy processing
//    - Set flags and handle complex logic in loop() if needed
//    - Callbacks run in the context of AutoNetwork's state machine
//
// 2. AVOID NETWORK OPERATIONS IN CALLBACKS
//    - Don't call WiFi.disconnect(), WiFi.begin(), etc.
//    - Don't start heavy network operations (large HTTP requests, etc.)
//    - Let AutoNetwork manage WiFi state
//
// 3. THREAD SAFETY (if using RTOS tasks)
//    - Use proper synchronization (mutexes, semaphores) if accessing
//      shared data from both callbacks and RTOS tasks
//    - Consider using FreeRTOS task notifications for inter-task signaling
//
// 4. ERROR HANDLING
//    - Check for NULL pointers when accessing WiFi objects
//    - Validate data before using (WiFi.SSID() might be empty)
//    - Handle all enum cases for future compatibility
//
// 5. LOGGING LEVELS
//    - Use ESP_LOGI for normal informational messages
//    - Use ESP_LOGW for warnings (failures, timeouts)
//    - Use ESP_LOGE for errors (critical failures)
//    - Use ESP_LOGD for debugging (verbose details)
//
// 6. STATE TRACKING (optional)
//    - Maintain application state flags if needed
//    - Use state to control application behavior
//    - Don't rely solely on WiFi.status() - track events
//
// 7. USER FEEDBACK
//    - Update displays (OLED, LCD, LED) to show status
//    - Provide clear feedback for each state change
//    - Help users understand what's happening
//
// ****************************************************************************
// COMMON USE CASES
// ****************************************************************************
//
// DISPLAY UPDATES:
//   - Show WiFi connection status on OLED/LCD
//   - Display IP address when connected
//   - Show portal QR code when in AP mode
//   - Animate connection progress
//
// LED INDICATORS:
//   - Solid: Connected
//   - Blinking fast: Portal active
//   - Blinking slow: Connecting/Disconnected
//   - See TickerExample.cpp for automated LED handling
//
// SERVICE MANAGEMENT:
//   - Start NTP sync when connected
//   - Initialize MQTT client after connection
//   - Pause cloud uploads when disconnected
//   - Buffer data during connection loss
//
// APPLICATION LOGIC:
//   - Enable features requiring WiFi
//   - Disable network-dependent operations
//   - Trigger data sync when connected
//   - Handle offline modes gracefully
//
// ****************************************************************************
