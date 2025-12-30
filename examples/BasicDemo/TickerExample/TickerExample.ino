/**
 * @file TickerExample.ino
 * @brief Demonstrates AutoNetwork ticker feature for visual WiFi status
 *
 * @details
 * Demonstrates:
 * - Enabling ticker with default settings
 * - Custom ticker GPIO configuration
 * - Active HIGH/LOW LED polarity configuration
 * - LED blink patterns for different WiFi states
 * - Runtime ticker enable/disable
 * - External LED usage
 * - Adaptive ticker based on power mode
 *
 * LED Blink Patterns:
 * - Fast blink (~1 sec cycle, 37% duty): Portal/AP mode active
 * - Slow blink (~2 sec cycle, 6% duty): Disconnected/connecting
 * - No blink: Connected to WiFi
 *
 * Hardware:
 * - ESP32 board
 * - Built-in LED or external LED on configurable GPIO
 *
 * @version 0.0.1
 * @date 2025-11-14
 */

// ****************************************************************************
// Title        : WiFi Ticker Status Indicator Example
// Filename     : 'TickerExample.ino'
// Target MCU   : ESP32
// Description  : Demonstrates AutoNetwork ticker feature for visual WiFi status
//                Shows different LED blink patterns for different states
//
// Revision History:
// When         Who         Description of change
// -----------  ----------- -----------------------
// 14-NOV-2025  Brooks      Updated to v0.0.1 with Doxygen headers
// 03-OCT-2025  Brooks      Initial implementation
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

    ESP_LOGI(TAG_MAIN, "WiFi Ticker Example");
    ESP_LOGI(TAG_MAIN, "===================");

    // Example 1: Enable ticker with default settings
    // ===============================================
    ESP_LOGI(TAG_MAIN, "Enabling ticker with default settings...");
    autonetwork.enableTicker(true);

    // The ticker will automatically show:
    // - Fast blink (1s cycle, ~37% duty): Portal/AP mode
    // - Slow blink (2s cycle, ~6% duty):  Disconnected
    // - No blink:                         Connected

    // Example 2: Custom ticker configuration
    // =======================================
    // Use GPIO 2 instead of LED_BUILTIN
    // autonetwork.setTickerPort(2);

    // Set active HIGH for common anode LEDs
    // autonetwork.setTickerOn(HIGH);

    // Example 3: Multiple board configurations
    // =========================================
    #ifdef LED_BUILTIN
        ESP_LOGI(TAG_MAIN, "Using LED_BUILTIN on GPIO %d", LED_BUILTIN);
        autonetwork.setTickerPort(LED_BUILTIN);
    #else
        ESP_LOGI(TAG_MAIN, "LED_BUILTIN not defined, using GPIO 2");
        autonetwork.setTickerPort(2);
    #endif

    // Most ESP32 boards use active LOW
    autonetwork.setTickerOn(LOW);

    // Configure root page
    autonetwork.setRootContent(R"rawliteral(
<!DOCTYPE html>
<html>
<head><title>Ticker Demo</title></head>
<body>
    <h1>AutoNetwork Ticker Example</h1>
    <p>LED ticker indicates WiFi status.</p>
    <p>{{AUTONETWORK_MENU}}</p>
</body>
</html>
)rawliteral");

    // Configure AutoNetwork

    // Connection status callback
    autonetwork.onConnectionStatus([](AutoNetworkConnectionStatus status)
    {
        switch (status)
        {
        case AutoNetworkConnectionStatus::CONNECTED:
            ESP_LOGI(TAG_MAIN, "WiFi Connected! (LED stops blinking)");
            ESP_LOGI(TAG_MAIN, "IP: %s", WiFi.localIP().toString().c_str());
            break;

        case AutoNetworkConnectionStatus::CONNECTING:
            ESP_LOGI(TAG_MAIN, "Connecting... (LED blinking slowly)");
            break;

        case AutoNetworkConnectionStatus::CONNECTION_FAILED:
            ESP_LOGW(TAG_MAIN, "Connection Failed (LED blinking slowly)");
            break;

        case AutoNetworkConnectionStatus::DISCONNECTED:
            ESP_LOGW(TAG_MAIN, "Disconnected (LED blinking slowly)");
            break;

        default:
            break;
        }
    });

    // Portal state callback
    autonetwork.onPortalState([](AutoNetworkPortalState state)
    {
        if (state == AutoNetworkPortalState::WAITING_FOR_CONNECTION)
        {
            ESP_LOGI(TAG_MAIN, "Portal Active (LED blinking fast)");
            ESP_LOGI(TAG_MAIN, "Connect to AP: AutoNetworkAP");
        }
    });

    // Start AutoNetwork
    autonetwork.autoConnect("AutoNetworkAP", "");

    server.begin();
    ESP_LOGI(TAG_MAIN, "HTTP server started");

    ESP_LOGI(TAG_MAIN, "");
    ESP_LOGI(TAG_MAIN, "LED Blink Patterns:");
    ESP_LOGI(TAG_MAIN, "- Fast blink (~1 sec): Portal/AP mode");
    ESP_LOGI(TAG_MAIN, "- Slow blink (~2 sec): Disconnected");
    ESP_LOGI(TAG_MAIN, "- No blink: Connected");
}

// Main Program
// ****************************************************************************
void loop()
{
    autonetwork.loop();
}

// ****************************************************************************
// Additional Examples
// ****************************************************************************

// Example: Disable ticker at runtime
void disableTicker()
{
    autonetwork.enableTicker(false);
    ESP_LOGI(TAG_MAIN, "Ticker disabled");
}

// Example: Custom LED on external pin
void useExternalLED()
{
    const uint8_t EXTERNAL_LED_PIN = 25;

    // Configure external LED
    pinMode(EXTERNAL_LED_PIN, OUTPUT);
    digitalWrite(EXTERNAL_LED_PIN, LOW);

    // Use external LED for ticker
    autonetwork.setTickerPort(EXTERNAL_LED_PIN);
    autonetwork.setTickerOn(HIGH);  // Active HIGH
    autonetwork.enableTicker(true);

    ESP_LOGI(TAG_MAIN, "Using external LED on GPIO %d", EXTERNAL_LED_PIN);
}

// Example: Change ticker during runtime based on application state
void adaptiveTickerExample()
{
    static bool highPowerMode = true;

    if (highPowerMode)
    {
        // Normal operation: use ticker
        autonetwork.enableTicker(true);
        ESP_LOGI(TAG_MAIN, "High power mode: ticker enabled");
    }
    else
    {
        // Low power mode: disable ticker to save energy
        autonetwork.enableTicker(false);
        ESP_LOGI(TAG_MAIN, "Low power mode: ticker disabled");
    }
}
