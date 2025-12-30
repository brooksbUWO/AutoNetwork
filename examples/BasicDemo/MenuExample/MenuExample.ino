/**
 * @file MenuExample.ino
 * @brief Demonstrates AUTONETWORK_LINK macro for menu integration
 *
 * @details
 * Demonstrates:
 * - Using AUTONETWORK_LINK macro to add menu icons to custom pages
 * - Multiple icon styles: BAR_32, BAR_24, COG_24
 * - Inline menu links using AUTONETWORK_LINK_INLINE
 * - Integration with custom HTML pages (home, settings, about)
 * - API endpoint with menu URL in JSON response
 * - Dynamic menu button based on connection state
 * - Custom styled menu links
 *
 * Available Menu Icons:
 * - BAR_32: Hamburger menu icon (32x32 px)
 * - BAR_24: Hamburger menu icon (24x24 px)
 * - COG_24: Settings cog icon (24x24 px)
 *
 * Hardware:
 * - ESP32 board
 *
 * @version 0.0.1
 * @date 2025-11-14
 */

// ****************************************************************************
// Title        : AutoNetwork Menu Example
// Filename     : 'MenuExample.ino'
// Target MCU   : ESP32
// Description  : Demonstrates how to use AUTONETWORK_LINK macro to add
//                menu links to custom web pages
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

    ESP_LOGI(TAG_MAIN, "AutoNetwork Menu Example");

    // Configure AutoNetwork

    // Configure root page with setRootContent()
    // This demonstrates using the {{AUTONETWORK_MENU}} placeholder for menu integration
    autonetwork.setRootContent(R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>My Application</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            max-width: 800px;
            margin: 50px auto;
            padding: 20px;
        }
        .menu-link {
            position: fixed;
            top: 20px;
            right: 20px;
        }
    </style>
</head>
<body>
    <div class="menu-link">
        {{AUTONETWORK_MENU}}
    </div>

    <h1>Welcome to My Application</h1>
    <p>This is a custom home page with an AutoNetwork menu link.</p>
    <p>Click the menu icon (☰) in the top-right corner to access WiFi settings.</p>

    <h2>Features:</h2>
    <ul>
        <li>Configure WiFi networks</li>
        <li>View saved credentials</li>
        <li>Disconnect from current network</li>
        <li>Reset device</li>
    </ul>
</body>
</html>
)rawliteral");

    autonetwork.autoConnect("AutoNetworkAP", "");

    // Example 2: Custom page with COG icon
    server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        String html = R"=====(
<!DOCTYPE html>
<html>
<head>
    <title>Application Settings</title>
</head>
<body>
    <h1>Application Settings</h1>
    {{WIFI_MENU}}
    <p>WiFi Settings Link (Cog Icon)</p>

    <h2>Other Settings:</h2>
    <form>
        <label><input type="checkbox"> Enable feature A</label><br>
        <label><input type="checkbox"> Enable feature B</label><br>
        <button type="submit">Save</button>
    </form>
</body>
</html>
        )=====";

        html.replace("{{WIFI_MENU}}", AUTONETWORK_LINK(COG_24));
        request->send(200, "text/html", html);
    });

    // Example 3: Inline menu link in text
    server.on("/about", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        String html = R"=====(
<!DOCTYPE html>
<html>
<head>
    <title>About</title>
</head>
<body>
    <h1>About This Device</h1>
    <p>Device Name: ESP32 AutoNetwork Demo</p>
    <p>Firmware Version: 1.0.0</p>

    <p>To configure WiFi settings, click here: {{MENU_INLINE}} Menu</p>

    <p>The AutoNetwork menu provides access to:</p>
    <ul>
        <li>WiFi configuration</li>
        <li>Saved network credentials</li>
        <li>Device management</li>
    </ul>
</body>
</html>
        )=====";

        html.replace("{{MENU_INLINE}}", AUTONETWORK_LINK_INLINE(BAR_24));
        request->send(200, "text/html", html);
    });

    // Example 4: API endpoint with menu link in JSON response
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        String json = "{";
        json += "\"wifi_status\":\"" + String(WiFi.status() == WL_CONNECTED ? "connected" : "disconnected") + "\",";
        json += "\"ssid\":\"" + WiFi.SSID() + "\",";
        json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
        json += "\"menu_url\":\"/_an\"";
        json += "}";

        request->send(200, "application/json", json);
    });

    server.begin();
    ESP_LOGI(TAG_MAIN, "HTTP server started");
    ESP_LOGI(TAG_MAIN, "");
    ESP_LOGI(TAG_MAIN, "Access points:");
    ESP_LOGI(TAG_MAIN, "  Home:     http://%s/", WiFi.localIP().toString().c_str());
    ESP_LOGI(TAG_MAIN, "  Settings: http://%s/settings", WiFi.localIP().toString().c_str());
    ESP_LOGI(TAG_MAIN, "  About:    http://%s/about", WiFi.localIP().toString().c_str());
    ESP_LOGI(TAG_MAIN, "  Menu:     http://%s/_an", WiFi.localIP().toString().c_str());
}

// Main Program
// ****************************************************************************
void loop()
{
    autonetwork.loop();
}

// ****************************************************************************
// Additional Menu Integration Examples
// ****************************************************************************

// Example: Dynamic menu button based on connection state
String getMenuButton()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        return AUTONETWORK_LINK(BAR_32);  // Show menu when connected
    }
    else
    {
        return "<a href=\"/configure\">Configure WiFi</a>";  // Direct link when disconnected
    }
}

// Example: Custom styled menu link
String getStyledMenuLink()
{
    return R"=====(
        <style>
            .custom-menu-button {
                position: fixed;
                top: 10px;
                right: 10px;
                background: #667eea;
                padding: 10px;
                border-radius: 8px;
                box-shadow: 0 4px 6px rgba(0,0,0,0.1);
            }
            .custom-menu-button img {
                filter: brightness(0) invert(1);
            }
        </style>
        <div class="custom-menu-button">
        )=====" + String(AUTONETWORK_LINK(BAR_24)) + R"=====(
        </div>
    )=====";
}
