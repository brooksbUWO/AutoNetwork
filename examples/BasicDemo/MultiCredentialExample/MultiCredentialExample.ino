/**
 * @file MultiCredentialExample.ino
 * @brief Demonstrates AutoNetwork multi-credential storage system
 *
 * @details
 * Demonstrates:
 * - Manually adding multiple WiFi credentials with priority ordering
 * - WPA2 Enterprise credential storage (PEAP/MSCHAPv2)
 * - Listing all stored credentials
 * - Checking if specific SSID exists
 * - Deleting specific credentials
 * - Automatic priority-based connection attempts
 * - Runtime credential API access
 * - Saving credentials from user input
 * - Updating credential priority
 * - Factory reset (clear all credentials)
 *
 * Priority System:
 * - Priority 0 = highest (tried first)
 * - Priority 1, 2, 3... = lower priority
 * - AutoConnect tries credentials in priority order
 *
 * Hardware:
 * - ESP32 board
 *
 * @version 0.0.1
 * @date 2025-11-14
 */

// ****************************************************************************
// Title        : Multi-Credential WiFi Example
// Filename     : 'MultiCredentialExample.ino'
// Target MCU   : ESP32
// Description  : Demonstrates AutoNetwork multi-credential storage system
//                Shows how to save and manage multiple WiFi networks
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

    ESP_LOGI(TAG_MAIN, "Multi-Credential WiFi Example");

    // Example 1: Manually add multiple credentials
    // =============================================
    ESP_LOGI(TAG_MAIN, "Adding multiple WiFi credentials...");

    // Add home WiFi (priority 0 = highest)
    AutoNetworkCredentialEntry homeWiFi;
    homeWiFi.ssid = "HomeNetwork";
    homeWiFi.password = "homepassword123";
    homeWiFi.enterprise = false;
    homeWiFi.priority = 0;  // Try this first
    autonetwork.credential()->save(homeWiFi);

    // Add office WiFi (priority 1)
    AutoNetworkCredentialEntry officeWiFi;
    officeWiFi.ssid = "OfficeNetwork";
    officeWiFi.password = "officepassword456";
    officeWiFi.enterprise = false;
    officeWiFi.priority = 1;  // Try this second
    autonetwork.credential()->save(officeWiFi);

    // Add WPA2 Enterprise WiFi (priority 2)
    AutoNetworkCredentialEntry enterpriseWiFi;
    enterpriseWiFi.ssid = "UniversityWiFi";
    enterpriseWiFi.password = "mypassword";
    enterpriseWiFi.enterprise = true;
    enterpriseWiFi.enterpriseNetId = "student123";
    enterpriseWiFi.priority = 2;  // Try this third
    autonetwork.credential()->save(enterpriseWiFi);

    // Example 2: List all stored credentials
    // =======================================
    uint8_t count = autonetwork.credential()->entries();
    ESP_LOGI(TAG_MAIN, "Total stored credentials: %d", count);

    for (uint8_t i = 0; i < count; i++)
    {
        AutoNetworkCredentialEntry entry;
        if (autonetwork.credential()->getByIndex(i, entry))
        {
            ESP_LOGI(TAG_MAIN, "  [%d] SSID: %s, Enterprise: %s, Priority: %d",
                i,
                entry.ssid.c_str(),
                entry.enterprise ? "Yes" : "No",
                entry.priority);
        }
    }

    // Example 3: Check if specific SSID exists
    // =========================================
    if (autonetwork.credential()->exists("HomeNetwork"))
    {
        ESP_LOGI(TAG_MAIN, "HomeNetwork credentials found!");
    }

    // Example 4: Delete specific credential
    // ======================================
    // Uncomment to delete a specific network:
    // autonetwork.credential()->del("OfficeNetwork");
    // ESP_LOGI(TAG_MAIN, "Deleted OfficeNetwork credentials");

    // Configure root page
    autonetwork.setRootContent(R"rawliteral(
<!DOCTYPE html>
<html>
<head><title>Multi-Credential Demo</title></head>
<body>
    <h1>Multiple WiFi Credentials</h1>
    <p>This example manages multiple saved WiFi networks.</p>
    <p>{{AUTONETWORK_MENU}}</p>
</body>
</html>
)rawliteral");

    // Example 5: AutoConnect will try credentials in priority order
    // ==============================================================

    // This will automatically try:
    // 1. HomeNetwork (priority 0)
    // 2. OfficeNetwork (priority 1)
    // 3. UniversityWiFi (priority 2)
    // If none connect, it will start the captive portal
    autonetwork.autoConnect("AutoNetworkAP", "");

    // Connection status callback
    autonetwork.onConnectionStatus([](AutoNetworkConnectionStatus status)
    {
        if (status == AutoNetworkConnectionStatus::CONNECTED)
        {
            ESP_LOGI(TAG_MAIN, "WiFi Connected!");
            ESP_LOGI(TAG_MAIN, "IP: %s", WiFi.localIP().toString().c_str());
            ESP_LOGI(TAG_MAIN, "Connected to: %s", WiFi.SSID().c_str());
        }
    });

    server.begin();
    ESP_LOGI(TAG_MAIN, "HTTP server started");
}

// Main Program
// ****************************************************************************
void loop()
{
    autonetwork.loop();

    // Example: Access credential API at runtime
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > 30000)  // Every 30 seconds
    {
        lastCheck = millis();

        uint8_t count = autonetwork.credential()->entries();
        ESP_LOGI(TAG_MAIN, "Current credential count: %d", count);
    }
}

// ****************************************************************************
// Additional Examples
// ****************************************************************************

// Example: Save credential from user input
void saveCredentialFromInput(const char* ssid, const char* password, bool isEnterprise = false)
{
    AutoNetworkCredentialEntry entry;
    entry.ssid = ssid;
    entry.password = password;
    entry.enterprise = isEnterprise;
    entry.priority = 0;  // New credentials get highest priority

    if (autonetwork.credential()->save(entry))
    {
        ESP_LOGI(TAG_MAIN, "Saved credential: %s", ssid);
    }
    else
    {
        ESP_LOGE(TAG_MAIN, "Failed to save credential: %s", ssid);
    }
}

// Example: Find and update priority
void updatePriority(const char* ssid, int8_t newPriority)
{
    int8_t index = autonetwork.credential()->find(ssid);
    if (index >= 0)
    {
        AutoNetworkCredentialEntry entry;
        if (autonetwork.credential()->load(index, entry))
        {
            entry.priority = newPriority;
            autonetwork.credential()->save(entry);
            ESP_LOGI(TAG_MAIN, "Updated %s priority to %d", ssid, newPriority);
        }
    }
}

// Example: Clear all credentials and reset
void factoryReset()
{
    autonetwork.credential()->delAll();
    ESP_LOGI(TAG_MAIN, "All credentials cleared - factory reset complete");
}
