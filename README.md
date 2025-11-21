# AutoNetwork Library

AutoNetwork manages WiFi lifecycle for ESP32 applications:

- Automatic connection to saved networks
- Captive portal for credential configuration
- Visual LED feedback via ticker patterns
- Persistent credential storage in NVS
- AsyncWebServer integration
- Connection monitoring and callbacks
- Automatic root page management with error handling

---

## Installation

### PlatformIO

Add to `platformio.ini`:

```ini
lib_deps =
    https://github.com/brooksbUWO/AutoNetwork.git
    https://github.com/ESP32Async/ESPAsyncWebServer.git
    https://github.com/ESP32Async/AsyncTCP.git
    bblanchon/ArduinoJson@^7.0.0
```

### Arduino IDE

1. Download library ZIP from GitHub
2. Sketch > Include Library > Add .ZIP Library
3. Install dependencies:
   - [ESPAsyncWebServer](https://github.com/ESP32Async/ESPAsyncWebServer)
   - [AsyncTCP](https://github.com/ESP32Async/AsyncTCP)
   - [ArduinoJson](https://github.com/bblanchon/ArduinoJson) v7.x

---

## Quick Start

Minimal working example:

```cpp
#include <AutoNetwork.h>
#include <ESPAsyncWebServer.h>

AsyncWebServer server(80);
AutoNetwork autonetwork(&server);

String rootPage() {
    return R"(
<!DOCTYPE html>
<html>
<head><meta name="viewport" content="width=device-width, initial-scale=1"></head>
<body>
{{AUTONETWORK_MENU}}
<h1>Hello from ESP32</h1>
</body>
</html>
)";
}

void setup() {
    Serial.begin(115200);
    autonetwork.setRootContent(rootPage);
    autonetwork.begin();
}

void loop() {
    autonetwork.loop();
}
```

The `{{AUTONETWORK_MENU}}` placeholder is replaced with a hamburger menu icon linking to `/_an`. This relative link works regardless of the device's IP address, which changes depending on whether the ESP32 is in AP mode (192.168.4.1) or connected to a network (router-assigned IP).

### What happens:

1. **First boot (no saved credentials):** ESP32 creates AP named "AutoNetwork"
2. **Connect to AP:** Join the WiFi network from your phone/laptop
3. **Captive portal opens:** Configure your WiFi credentials
4. **Device connects:** ESP32 connects to your network and serves your webpage
5. **Access settings:** Click the menu icon to return to WiFi settings

## Documentation

- **[Web Interface](docs/web-interface.md)** - Portal documentation


## Configuration

### AutoNetworkConfig Structure

```cpp
AutoNetworkConfig config;

// Device identification
config.apSSID = "MyDevice";
config.apPassword = "";
config.staHostName = "mydevice";

// Ticker LED
config.tickerEnable = true;
config.tickerPin = LED_BUILTIN;
config.tickerActiveLevel = LOW;  // ESP32 DevKit active-LOW

// Timeouts
config.portalTimeout = 300;  // 5 minutes, 0=never

// Credential saving
config.credentialSaveMode = AutoNetworkCredentialSaveMode::AUTO;

// Logging
config.logLevel = AN_LOG_WARN;

autonetwork.config(config);
```

### Configuration Parameters

| Parameter | Type | Description | Default |
|-----------|------|-------------|---------|
| `apSSID` | String | AP mode SSID | "AutoNetwork" |
| `apPassword` | String | AP password (min 8 chars or empty) | "" |
| `staHostName` | String | Station mode hostname | "autonetwork" |
| `tickerEnable` | bool | Enable LED ticker | false |
| `tickerPin` | uint8_t | Ticker GPIO pin | LED_BUILTIN |
| `tickerActiveLevel` | uint8_t | LED active level (HIGH/LOW) | LOW |
| `portalTimeout` | uint32_t | Portal timeout ms (0=never) | 300000 |
| `credentialSaveMode` | enum | ALWAYS, AUTO, NEVER | AUTO |
| `logLevel` | enum | AN_LOG_NONE to AN_LOG_VERBOSE | AN_LOG_WARN |

---

## API Reference

### Constructor

```cpp
AutoNetwork(AsyncWebServer* server);
```

Create AutoNetwork instance with server pointer.

```cpp
AsyncWebServer server(80);
AutoNetwork autonetwork(&server);
```

---

### Configuration Methods

#### `void config(AutoNetworkConfig& cfg)`

Apply configuration settings.

```cpp
AutoNetworkConfig config;
config.apSSID = "Device";
config.tickerEnable = true;
autonetwork.config(config);
```

#### `void setHostname(const char* hostname)`

Set mDNS hostname.

```cpp
autonetwork.setHostname("mydevice");
```

#### `void setConnectTimeout(uint32_t timeoutMs)`

Set WiFi connection timeout.

```cpp
autonetwork.setConnectTimeout(30000);  // 30 seconds
```

#### `void setPortalTimeout(uint32_t timeoutMs)`

Set captive portal timeout (0 = infinite).

```cpp
autonetwork.setPortalTimeout(300000);  // 5 minutes
```

---

### Root Content Methods

#### `void setRootContent(const String& filePath)`

Set root page content from LittleFS file.

```cpp
autonetwork.setRootContent("/index.html");
```

#### `void setRootContent(std::function<String()> callback)`

Set root page content from callback (dynamic).

```cpp
autonetwork.setRootContent([]() {
    return "<html><body>Time: " + String(millis()) + "</body></html>";
});
```

#### `void setRootContentHTML(const char* htmlContent)`

Set root page content from embedded string.

```cpp
autonetwork.setRootContentHTML(R"rawliteral(
<!DOCTYPE html>
<html><body><h1>My App</h1></body></html>
)rawliteral");
```

#### Including the AutoNetwork Menu Link

Use the `{{AUTONETWORK_MENU}}` placeholder in your HTML to include a hamburger menu icon linking to `/_an`. This relative link works regardless of the device's IP address, which changes depending on whether the ESP32 is in AP mode (192.168.4.1) or connected to a network (router-assigned IP):

```cpp
autonetwork.setRootContentHTML(R"rawliteral(
<!DOCTYPE html>
<html>
<head><title>My App</title></head>
<body>
    {{AUTONETWORK_MENU}}
    <h1>My Application</h1>
    <p>Content here...</p>
</body>
</html>
)rawliteral");
```

The placeholder is automatically replaced with a styled hamburger menu button. You can also customize the menu link:

```cpp
autonetwork.setRootMenuReplacement("<a href='/_an'>Settings</a>");
```

---

### Control Methods

#### `bool begin()`

Initialize WiFi and start connection process. Returns true if connected immediately.

```cpp
if (autonetwork.begin()) {
    Serial.println("Connected!");
} else {
    Serial.println("Portal started");
}
```

#### `void loop()`

Process WiFi state machine. **Call first in main loop.**

```cpp
void loop() {
    autonetwork.loop();
}
```

---

### Status Methods

#### `bool isConnected()`

Check if WiFi is connected.

```cpp
if (autonetwork.isConnected()) {
    sendData();
}
```

#### `String getSSID()`

Get connected or configured SSID.

```cpp
String ssid = autonetwork.getSSID();
```

#### `IPAddress getIP()`

Get local IP address.

```cpp
IPAddress ip = autonetwork.getIP();
```

#### `AutoNetworkConnectionStatus getConnectionStatus()`

Get current connection status.

```cpp
auto status = autonetwork.getConnectionStatus();
```

---

### Logging Methods

#### `static void setLogLevel(AutoNetworkLogLevel level)`

Set library logging verbosity.

```cpp
autonetwork.setLogLevel(AN_LOG_DEBUG);
```

Log levels:
- `AN_LOG_NONE` - Silent
- `AN_LOG_ERROR` - Critical errors only
- `AN_LOG_WARN` - Warnings and errors (default)
- `AN_LOG_INFO` - Informational messages
- `AN_LOG_DEBUG` - Debug details
- `AN_LOG_VERBOSE` - Everything

---

## Ticker LED Patterns

Visual feedback for WiFi status:

| Pattern | Timing | Meaning | When Used |
|---------|--------|---------|-----------|
| **SLOW_BLINK** | 500ms on / 500ms off | Portal active | No saved networks |
| **SOLID_ON** | Continuously on | Connected | WiFi connected |
| **FAST_BLINK** | 150ms on / 150ms off | Reconnecting | Lost connection |
| **OFF** | LED off | Uninitialized | Before begin() |

---

## Common Mistakes

### Wrong: No Server Pointer

```cpp
AutoNetwork portal;  // ERROR: No default constructor
```

**Fix:**

```cpp
AsyncWebServer server(80);
AutoNetwork autonetwork(&server);
```

---

### Wrong: Calling WiFi Functions

```cpp
autonetwork.config(config);
WiFi.mode(WIFI_STA);  // Breaks library
autonetwork.begin();
```

**Fix:**

```cpp
autonetwork.config(config);
autonetwork.begin();  // Library controls mode
```

---

### Wrong: Not Calling loop()

```cpp
void loop() {
    doStuff();  // Missing autonetwork.loop()
}
```

**Fix:**

```cpp
void loop() {
    autonetwork.loop();  // Required first
    doStuff();
}
```

---

### Wrong: Starting Services Too Early

```cpp
void setup() {
    autonetwork.begin();
    webSocket.begin();  // WiFi may not be ready
}
```

**Fix:**

```cpp
autonetwork.onConnectionStatus([](AutoNetworkConnectionStatus status) {
    if (status == AutoNetworkConnectionStatus::CONNECTED) {
        webSocket.begin();  // Start when ready
    }
});
```

---

## Complete Example

```cpp
#include <Arduino.h>
#include <LittleFS.h>
#include <AutoNetwork.h>
#include <ESPAsyncWebServer.h>

// Global objects
AsyncWebServer server(80);
AutoNetwork autonetwork(&server);

void setup() {
    Serial.begin(115200);
    LittleFS.begin(true);

    // Get MAC for unique AP name
    WiFi.mode(WIFI_STA);
    String mac = WiFi.macAddress();
    mac.replace(":", "");

    // Configure
    AutoNetworkConfig config;
    config.apSSID = "ESP32_" + mac;
    config.staHostName = config.apSSID;
    config.tickerEnable = true;
    config.tickerPin = LED_BUILTIN;
    config.tickerActiveLevel = LOW;
    config.credentialSaveMode = AutoNetworkCredentialSaveMode::AUTO;
    config.logLevel = AN_LOG_WARN;

    autonetwork.config(config);
    autonetwork.setRootContent("/index.html");

    // Register callbacks
    autonetwork.onConnectionStatus([](AutoNetworkConnectionStatus status) {
        if (status == AutoNetworkConnectionStatus::CONNECTED) {
            Serial.printf("Connected: %s\n", WiFi.SSID().c_str());
            Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
        } else if (status == AutoNetworkConnectionStatus::DISCONNECTED) {
            Serial.println("Disconnected");
        }
    });

    autonetwork.onPortalState([](AutoNetworkPortalState state) {
        if (state == AutoNetworkPortalState::WAITING_FOR_CONNECTION) {
            Serial.printf("Portal at: http://%s\n",
                WiFi.softAPIP().toString().c_str());
        }
    });

    // Start
    if (autonetwork.begin()) {
        Serial.println("WiFi connected!");
    } else {
        Serial.printf("Connect to %s for setup\n", config.apSSID.c_str());
    }
}

void loop() {
    autonetwork.loop();
}
```

---

## Troubleshooting

### Enable Debug Logging

```cpp
config.logLevel = AN_LOG_DEBUG;
autonetwork.config(config);
```

Or after config:

```cpp
autonetwork.setLogLevel(AN_LOG_DEBUG);
```

---

## Summary

**Constructor Pattern:**

```cpp
AsyncWebServer server(80);
AutoNetwork autonetwork(&server);
```

**Setup Pattern:**

```cpp
autonetwork.config(config);
autonetwork.setRootContent("/index.html");
autonetwork.onConnectionStatus(callback);
autonetwork.begin();
```

**Loop Pattern:**

```cpp
void loop() {
    autonetwork.loop();  // Required first
}
```

---

## License

MIT License

