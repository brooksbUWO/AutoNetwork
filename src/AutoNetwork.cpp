/*!
 * @file AutoNetwork.cpp
 *
 * @mainpage AutoNetwork Library Documentation
 *
 * @section intro_sec Introduction
 *
 * This is the documentation for the AutoNetwork library, an ESP32 library
 * for PlatformIO using the Arduino framework. It simplifies network
 * connections (WiFi, Ethernet, Cellular) by providing a unified interface
 * and automatic network selection based on availability and priority.
 *
 * @section features_sec Key Features
 *
 * - **Automatic WiFi Connection:** Connects to saved networks automatically on boot
 * - **Captive Portal:** User-friendly web interface for WiFi configuration
 * - **Multi-Credential Storage:** Store up to 255 WiFi networks with priority ordering
 * - **WPA2 Enterprise Support:** PEAP/MSCHAPv2 authentication for enterprise networks
 * - **Async Web Server:** Non-blocking web server using ESPAsyncWebServer
 * - **LED Status Indicator:** Visual feedback via configurable GPIO LED ticker
 * - **OTA Updates:** Over-the-air firmware update support
 * - **Comprehensive Callbacks:** Event-driven architecture for connection status changes
 * - **Background Reconnection:** Automatic reconnection on WiFi loss
 * - **Custom Parameters:** Add custom configuration fields to portal
 *
 * @section dependencies Dependencies
 *
 * This library requires the following ESP32 libraries:
 * - **WiFi:** ESP32 core WiFi library
 * - **ESPAsyncWebServer:** Async HTTP server (recommended)
 * - **AsyncTCP:** Async TCP library for ESP32
 * - **ArduinoJson 7.x:** JSON parsing and generation
 * - **DNSServer:** Captive portal DNS redirection
 * - **Preferences:** ESP32 NVS (non-volatile storage) for credential persistence
 *
 * @section usage_sec Basic Usage
 *
 * @code{.cpp}
 * #include <AutoNetwork.h>
 *
 * // Create async web server on port 80
 * AsyncWebServer server(80);
 * AutoNetwork autoNetwork(&server);
 *
 * void setup() {
 *     Serial.begin(115200);
 *
 *     // Configure AutoNetwork
 *     AutoNetworkConfig config;
 *     config.apSSID = "ESP32-Setup";
 *     config.apPassword = "12345678";
 *     config.tickerEnable = true;
 *     config.tickerPin = LED_BUILTIN;
 *     config.portalRetain = false;
 *     autoNetwork.config(config);
 *
 *     // Register callbacks
 *     autoNetwork.onConnectionStatus([](AutoNetworkConnectionStatus status) {
 *         Serial.printf("WiFi Status: %d\n", (int)status);
 *     });
 *
 *     // Start connection manager
 *     autoNetwork.begin();
 * }
 *
 * void loop() {
 *     autoNetwork.loop(); // Must be called repeatedly
 * }
 * @endcode
 *
 * @section advanced_sec Advanced Features
 *
 * **Multi-Credential Storage:**
 * @code{.cpp}
 * // Credentials are automatically saved via captive portal
 * // Or save programmatically:
 * autoNetwork.setCredentials("HomeNetwork", "password123");
 * autoNetwork.setCredentials("WorkNetwork", "work_pass");
 * @endcode
 *
 * **WPA2 Enterprise:**
 * @code{.cpp}
 * // Enterprise credentials saved via portal or programmatically
 * // Portal provides enterprise network ID and password fields
 * @endcode
 *
 * **Custom Parameters:**
 * @code{.cpp}
 * AutoNetworkParameter deviceName("deviceName", "Device Name", "ESP32", 32);
 * autoNetwork.addParameter(&deviceName);
 * // Parameter appears in portal and value retrieved after config
 * @endcode
 *
 * @section architecture_sec Architecture
 *
 * AutoNetwork uses a modular architecture with the following components:
 * - **AutoNetwork:** Main WiFi manager class
 * - **AutoNetworkPortal:** Captive portal manager
 * - **AutoNetworkCredential:** Multi-credential storage using ESP32 NVS
 * - **AutoNetworkTicker:** LED status indicator
 * - **AutoNetworkParameter:** Custom configuration parameters
 * - **AutoNetworkConfig:** Configuration container
 * - **PortalState:** Portal state machine management
 *
 * @section author Author
 *
 * Written by Brooks.
 *
 * @section license License
 *
 * MIT License
 *
 * @section revision_history Revision History
 *
 * | Date | Author | Description |
 * |------|--------|-------------|
 * | 2025-10-02 | Brooks | Initial implementation |
 * | 2025-10-17 | Brooks | Refactored portal into separate class |
 * | 2025-10-22 | Brooks | Added quality improvements and scan caching |
 * | 2025-10-24 | Brooks | Added comprehensive Doxygen documentation |
 */

// Include Files
// ****************************************************************************
#include "AutoNetwork.h"
#include "AutoNetworkLog.h"
#include "AutoNetworkParameter.h"
#include "AutoNetworkCredential.h"
#include "AutoNetworkPortal.h"
#include "AutoNetworkTicker.h"
#include <LittleFS.h>
#include <Update.h>
#define FILESYSTEM LittleFS
#include "esp_idf_version.h"
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    // ESP-IDF 5.0+: Use new EAP client APIs
    #include "esp_eap_client.h"
#else
    // ESP-IDF 4.x and earlier: Use deprecated WPA2 APIs
    #include "esp_wpa2.h"
#endif
#include <ESPmDNS.h>

// ESP-IDF Logging Tag
// ****************************************************************************
static const char *TAG = "AutoNetwork";

// Constants
// ****************************************************************************
#define AUTONETWORK_STATUS_JSON_SIZE 1024
#define AUTONETWORK_SCAN_JSON_SIZE 1024
#define AUTONETWORK_CONFIG_JSON_SIZE 1024
#define AUTONETWORK_CONNECT_JSON_SIZE 512

// Timing and Memory constants now defined in AutoNetwork.h with AUTONETWORK_ prefix

// Globals
// ****************************************************************************
struct AutoNetworkParameterTypeNames paramTypes[] =
    {
        {AN_HEADER, "h"},
        {AN_DIVIDER, "d"},
        {AN_SPACER, "s"},
        {AN_INPUT, "i"},
};

// Class Implementations
// ****************************************************************************

AutoNetwork::AutoNetwork(AsyncWebServer *server)
    : _credentialMgr(_credential)
{
    _server = server;
    _an.sta.configured = false;
    _an.rootMenuReplacement = AUTONETWORK_LINK();  // Set default hamburger menu icon

    // Portal manager initialization
    // Note: Counter is initialized in PortalState constructor
    _portal = new AutoNetworkPortal(_server, this);

    // Connection manager initialization
    _connectionMgr = new AutoNetworkConnectionManager(_portal, &_credential, _an.status, _established);

    // Event callbacks replace friend class access pattern
    _portal->setConnectCallback([this](const char *ssid, const char *pass, bool autoReconnect, const AutoNetworkCredentialEntry *entry)
                                { _connect(ssid, pass, autoReconnect, entry); });

    _portal->setConnectEnterpriseCallback([this](const char *ssid, const char *netId, const char *pass)
                                          { _connectEnterprise(ssid, netId, pass); });

    _portal->setDisconnectCallback([this]()
                                   { _disconnect(); });

    _portal->setGetConfigCallback([this]() -> const AutoNetworkConfig &
                                  { return _config; });

    _portal->setGetHostnameCallback([this]() -> String
                                    { return _an.hostname; });

    _portal->setGetSTASSIDCallback([this]() -> String
                                   { return _an.sta.ssid; });

    _portal->setGetSTAPasswordCallback([this]() -> String
                                       { return _an.sta.password; });

    _portal->setGetSTAEnterpriseCallback([this]() -> bool
                                         { return _an.sta.enterprise; });

    _portal->setGetSTAEnterpriseNetIdCallback([this]() -> String
                                              { return _an.sta.enterpriseNetId; });

    _portal->setGetSTAConfiguredCallback([this]() -> bool
                                         { return _an.sta.configured; });

    _portal->setGetStatusCallback([this]() -> uint8_t
                                  { return (uint8_t)_an.status; });

    _portal->setSetSTACredentialsCallback([this](const String &ssid, const String &pass, bool enterprise, const String &netId, bool configured)
                                          {
        _an.sta.ssid = ssid;
        _an.sta.password = pass;
        _an.sta.enterprise = enterprise;
        _an.sta.enterpriseNetId = netId;
        _an.sta.configured = configured; });

    _portal->setSetPortalRetainCallback([this](bool retain)
                                        { _config.portalRetain = retain; });

    _portal->setGetCredentialEntriesCallback([this]() -> uint8_t
                                             { return _credential.entries(); });

    _portal->setGetCredentialByPriorityCallback([this](uint8_t index, AutoNetworkCredentialEntry &entry) -> bool
                                                { return _credential.getByPriority(index, entry); });

    _portal->setGetCredentialByRecentCallback([this](uint8_t index, AutoNetworkCredentialEntry &entry) -> bool
                                              { return _credential.getByRecent(index, entry); });

    _portal->setGetCredentialByIndexCallback([this](uint8_t index, AutoNetworkCredentialEntry &entry) -> bool
                                             { return _credential.getByIndex(index, entry); });

    _portal->setSaveCredentialCallback([this](const AutoNetworkCredentialEntry &entry) -> bool
                                       { return _credential.save(entry); });

    _portal->setDeleteCredentialCallback([this](const char *ssid) -> bool
                                         { return _credential.del(ssid); });

    _portal->setRequestScanCallback([this]()
                                    { _restartScan(); });

    _portal->setUpdateTickerCallback([this]()
                                     { _updateTicker(); });

    _portal->setGetMonotonicTimestampCallback([this]() -> uint64_t
                                              { return _getMonotonicTimestamp(); });

    _portal->setIsConfiguredCallback([this]() -> bool
                                     { return isConfigured(); });

    _portal->setSetWebpageAccessedCallback([this]()
                                           {
        if (!_an.webpageAccessed) {
            _an.webpageAccessed = true;
            AN_LOGI(TAG, "First webpage access detected - activating runtime display");
            if (_an.webpageAccessed_cb) {
                _an.webpageAccessed_cb(); // Trigger the main.cpp callback
            }
        } });
}

void AutoNetwork::setAuthentication(String &username, String &password)
{
    this->setAuthentication(username.c_str(), password.c_str());
}

void AutoNetwork::setAuthentication(
    const char *username,
    const char *password)
{
    if (strlen(username) > 0 && strlen(password) > 0)
    {
        _portal->setAuthentication(username, password);
    }
}

void AutoNetwork::setHostname(String &hostname)
{
    this->setHostname(hostname.c_str());
}

void AutoNetwork::setHostname(const char *hostname)
{
    _an.hostname = hostname;
}

// Logging Configuration Methods
// ==============================================================================

void AutoNetwork::setLogLevel(AutoNetworkLogLevel level)
{
    AutoNetworkLogging::setLogLevel(level);
}

AutoNetworkLogLevel AutoNetwork::getLogLevel()
{
    return AutoNetworkLogging::getLogLevel();
}

void AutoNetwork::enableVerboseLogging()
{
    AutoNetworkLogging::setLogLevel(AN_LOG_VERBOSE);
}

void AutoNetwork::disableLogging()
{
    AutoNetworkLogging::setLogLevel(AN_LOG_NONE);
}

void AutoNetwork::setDefaultLogging()
{
    AutoNetworkLogging::setLogLevel(AN_LOG_WARN);
}

void AutoNetwork::config(AutoNetworkConfig &cfg)
{
    // Apply logging level first (before other logging occurs)
    AutoNetworkLogging::setLogLevel(cfg.logLevel);

    // Apply configuration settings to AutoNetwork instance
    AN_LOGD(TAG, "[AutoNetwork] Applying AutoNetworkConfig...");

    // Check for AP name change and restart portal if needed
    if (cfg.apSSID != _config.apSSID || cfg.apPassword != _config.apPassword)
    {
        AN_LOGI(TAG, "AP credentials changed from '%s' to '%s'", _config.apSSID.c_str(), cfg.apSSID.c_str());
        if (_portal != nullptr)
        {
            if (_portal->isActive())
            {
                // Portal is active, restart it with new credentials
                AN_LOGI(TAG, "Restarting portal with new AP credentials");
                _portal->stop();
                _portal->setAPCredentials(cfg.apSSID.c_str(), cfg.apPassword.c_str());
                _portal->start();
            }
            else
            {
                // Portal not active, just update credentials for next start
                _portal->setAPCredentials(cfg.apSSID.c_str(), cfg.apPassword.c_str());
            }
        }
    }

    // Check for hostname change and restart mDNS if needed
    if (cfg.staHostName != _config.staHostName)
    {
        AN_LOGI(TAG, "Hostname changed from '%s' to '%s'", _config.staHostName.c_str(), cfg.staHostName.c_str());
        _an.hostname = cfg.staHostName;

        // Restart mDNS if WiFi is connected to apply new hostname
        if (WiFi.status() == WL_CONNECTED)
        {
            AN_LOGI(TAG, "Restarting mDNS with new hostname");
            _stopMDNS();
            _startMDNS();
        }
    }
    else
    {
        // Station configuration (no change)
        _an.hostname = cfg.staHostName;
    }

    // Portal configuration
    _portal->setTimeout(cfg.timeoutPortalMs);
    _an.sta.timeout = cfg.timeoutConnectMs;

    // Ticker configuration
    _an.ticker.enabled = cfg.tickerEnable;
    _an.ticker.port = cfg.tickerPin;
    _an.ticker.activeLevel = cfg.tickerActiveLevel;

    // Authentication configuration
    if (cfg.authType.length() > 0 && cfg.authUsername.length() > 0 && cfg.authPassword.length() > 0)
    {
        _portal->setAuthentication(cfg.authUsername.c_str(), cfg.authPassword.c_str());
    }

    // Store the new configuration
    _config = cfg;

    AN_LOGI(TAG, "[AutoNetwork] Configuration applied: APID=%s, Hostname=%s, Ticker=%s",
             cfg.apSSID.c_str(),
             cfg.staHostName.c_str(),
             cfg.tickerEnable ? "enabled" : "disabled");
}

void AutoNetwork::enableTicker(bool enable)
{
    _an.ticker.enabled = enable;
    AN_LOGI(TAG, "Ticker %s", enable ? "enabled" : "disabled");
}

void AutoNetwork::setTickerPort(uint8_t port)
{
    _an.ticker.port = port;
    AN_LOGD(TAG, "Ticker port set to GPIO %d", port);
}

void AutoNetwork::setTickerOn(uint8_t activeLevel)
{
    _an.ticker.activeLevel = activeLevel;
    AN_LOGD(TAG, "Ticker active level set to %s",
             activeLevel == HIGH ? "HIGH" : "LOW");
}

void AutoNetwork::setConnectTimeout(uint32_t timeoutMs)
{
    _an.sta.timeout = timeoutMs;
}

void AutoNetwork::setPortalTimeout(uint32_t timeoutMs)
{
    _portal->setTimeout(timeoutMs);
}

void AutoNetwork::setCredentials(const char *ssid, const char *password)
{
    AutoNetworkCredentialEntry entry;
    entry.ssid = ssid;
    entry.password = password;
    entry.enterprise = false;
    entry.priority = 0;

    if (_credential.save(entry))
    {
        _an.sta.ssid = ssid;
        _an.sta.password = password;
        _an.sta.enterprise = false;
        _an.sta.configured = true;
        AN_LOGI(TAG, "Credentials set for SSID: %s", ssid);
    }
    else
    {
        AN_LOGE(TAG, "Failed to save credentials for SSID: %s", ssid);
    }
}

void AutoNetwork::autoConnect(const char *ssid, const char *password)
{

    // Disconnect if connected, but DON'T turn WiFi OFF
    // Turning WiFi OFF causes ESP-IDF subsystem initialization failures
    if (WiFi.status() == WL_CONNECTED)
    {
        AN_LOGD(TAG, "Disconnecting from current network");
        _disconnect();
        // REMOVED: delay(100) - WiFi.disconnect() is synchronous, no delay needed
    }

    _portal->setAPCredentials(ssid, password);
    _startTicker();

    // Initialize WiFi mode before starting HTTP server
    // AsyncWebServer requires TCP/IP stack to be initialized
    // If WiFi mode is NULL, initialize it to prevent crash in _startHTTP()
    if (WiFi.getMode() == WIFI_MODE_NULL)
    {
        AN_LOGI(TAG, "WiFi not initialized - setting mode to AP for HTTP server");
        WiFi.mode(WIFI_AP);
    }

    // Web server must run regardless of connection success (unlike portal which only runs on failure)
    // This allows the device to be accessible via web interface after connecting to WiFi
    if (!_serverRunning)
    {
        AN_LOGI(TAG, "Starting HTTP server for web interface");
        _startHTTP();
    }

    // Try to connect to saved credentials (multi-credential support)
    uint8_t credentialCount = _credential.entries();
    AN_LOGI(TAG, "Found %d saved credential(s)", credentialCount);

    bool flagStartCaptivePortal = true;

    // autoReconnect = try saved credentials first
    // autoRise = start portal if connection fails
    // Both can be enabled: try credentials first, portal as fallback
    bool shouldTryCredentials = _config.staAutoReconnect;

    // Pre-scan WiFi networks to optimize connection attempts
    int16_t scanResults = -1;
    if (credentialCount > 0 && shouldTryCredentials)
    {
        AN_LOGI(TAG, "Pre-scanning WiFi networks to optimize connection attempts...");
        scanResults = WiFi.scanNetworks();

        if (scanResults > 0)
        {
            AN_LOGI(TAG, "Found %d network(s) in range", scanResults);
        }
        else if (scanResults == 0)
        {
            AN_LOGW(TAG, "WiFi scan found no networks");
        }
        else
        {
            AN_LOGW(TAG, "WiFi scan failed with error: %d", scanResults);
        }
    }

    // Try saved credentials if they exist AND conditions allow
    if (credentialCount > 0 && shouldTryCredentials)
    {
        // Try each saved credential (most recently used first)
        for (uint8_t i = 0; i < credentialCount && flagStartCaptivePortal; i++)
        {
            AutoNetworkCredentialEntry entry;

            // Try most recently used credential first
            bool loaded = _credential.getByRecent(i, entry);

            if (!loaded)
            {
                continue;
            }

            // Skip if scan was successful and this SSID is not available
            if (scanResults > 0)
            {
                bool isAvailable = false;
                for (int16_t j = 0; j < scanResults; j++)
                {
                    if (WiFi.SSID(j) == entry.ssid)
                    {
                        isAvailable = true;
                        AN_LOGI(TAG, "Network %s found in scan (RSSI: %d)", entry.ssid.c_str(), WiFi.RSSI(j));
                        break;
                    }
                }

                if (!isAvailable)
                {
                    AN_LOGI(TAG, "Skipping credential %d/%d: %s (not in range)",
                             i + 1, credentialCount, entry.ssid.c_str());
                    continue;
                }
            }

            AN_LOGI(TAG, "Trying credential %d/%d: %s (using RECENT principle)",
                     i + 1, credentialCount, entry.ssid.c_str());

            // Measure connection time for performance analysis
            unsigned long connectionStartTime = millis();

            // Load this credential into active configuration
            _an.sta.ssid = entry.ssid;
            _an.sta.password = entry.password;
            _an.sta.enterprise = entry.enterprise;
            _an.sta.enterpriseNetId = entry.enterpriseNetId;
            _an.sta.configured = true;

            // Disable WiFi persistence before mode change
            WiFi.persistent(false);

            // Determine and set WiFi mode using centralized decision logic
            WiFiMode_t targetMode = _determineWiFiMode();

            if (targetMode == WIFI_AP_STA && _config.staPreserveAPMode)
            {
                // Special case: Maintain AP mode while enabling STA (ESP-MESH/ESP-NOW)
                bool cs = WiFi.enableSTA(true);
                AN_LOGI(TAG, "WiFi mode %d maintained, STA %s", WiFi.getMode(), cs ? "enabled" : "unavailable");
            }
            else
            {
                // Standard case: Set WiFi mode directly
                WiFi.mode(targetMode);
                if (targetMode == WIFI_AP_STA)
                {
                    AN_LOGI(TAG, "Using AP+STA mode (portal active with retainPortal=true)");
                }
                else
                {
                    AN_LOGI(TAG, "Using STA-only mode");
                }
            }

            // REMOVED: delay(100) after mode change
            // Modern ESP32 WiFi stack handles mode changes synchronously
            // No delay needed - removes 100ms from connection time

            // Set hostname AFTER WiFi mode is initialized
            // CRITICAL: WiFi.setHostname() MUST be called AFTER WiFi.mode() on ESP32
            // Calling setHostname() before mode initialization causes ESP32 to revert
            // to default hostname format: esp32-XXXXXX
            if (_an.hostname != "")
            {
                _setHostname(_an.hostname.c_str());
            }

            // Attempt connection
            if (entry.enterprise && entry.enterpriseNetId.length() > 0)
            {
                _connectEnterprise(entry.ssid.c_str(), entry.enterpriseNetId.c_str(), entry.password.c_str());
            }
            else
            {
                _connect(entry.ssid.c_str(), entry.password.c_str(), true, &entry);
            }

            // Wait for connection with timeout
            unsigned long timeStart = millis();
            while ((unsigned long)(millis() - timeStart) < _an.sta.timeout)
            {
                yield();
                this->loop();

                if (_an.status == AutoNetworkConnectionStatus::CONNECTED)
                {
                    unsigned long connectionTime = millis() - connectionStartTime;
                    AN_LOGI(TAG, "Connected to %s in %lums", entry.ssid.c_str(), connectionTime);
                    flagStartCaptivePortal = false;
                    break;
                }
            }

            if (_an.status != AutoNetworkConnectionStatus::CONNECTED)
            {
                AN_LOGW(TAG, "Failed to connect to %s", entry.ssid.c_str());
                _disconnect();
            }
        } // End of for loop

        // Clean up scan results to free memory - use ScanManager
        if (scanResults >= 0)
        {
            _scanManager.clearScanResults();
        }
    }
    else if (credentialCount > 0 && !shouldTryCredentials)
    {
        // Credentials exist but autoReconnect is disabled
        AN_LOGI(TAG, "Skipping %d saved credential(s) - autoReconnect disabled", credentialCount);
    }

    // If no saved credentials exist, mark as not configured
    if (credentialCount == 0)
    {
        _an.sta.configured = false;
    }

    if (flagStartCaptivePortal)
    {
        _stopPortal();

        AN_LOGI(TAG, "About to start captive portal...");
        _startPortal();
        AN_LOGI(TAG, "Portal start completed");
    }

    // Register root handler
    _registerRootHandler();
}

void AutoNetwork::onConnectionStatus(
    AutoNetworkOnConnectionStatusCallback callback)
{
    _an.status_cb = callback;
}

void AutoNetwork::onPortalState(
    AutoNetworkOnPortalStateCallback callback)
{
    _portal->onPortalState(callback);
}

void AutoNetwork::onConfig(AutoNetworkOnConfigCallback callback)
{
    _portal->onConfig(callback);
}

void AutoNetwork::onWebpageAccessed(std::function<void()> callback)
{
    _an.webpageAccessed_cb = callback;
}

bool AutoNetwork::isConfigured()
{
    return _an.sta.configured;
}

AutoNetworkConnectionStatus AutoNetwork::getConnectionStatus()
{
    return _an.status;
}

AutoNetworkPortalState AutoNetwork::getPortalState()
{
    return _portal->getState();
}

const char *AutoNetwork::getSSID()
{
    return _an.sta.ssid.c_str();
}

const char *AutoNetwork::getPassword()
{
    return _an.sta.password.c_str();
}

void AutoNetwork::getBSSID(uint8_t *bssid)
{
    // Return current WiFi BSSID
    uint8_t *mac = WiFi.BSSID();
    if (mac)
    {
        memcpy(bssid, mac, AUTONETWORK_BSSID_LENGTH);
    }
}

uint8_t AutoNetwork::getChannel()
{
    return WiFi.channel();
}

IPAddress AutoNetwork::localIP()
{
    return _connectionMgr->getLocalIP();
}

IPAddress AutoNetwork::gatewayIP()
{
    return _connectionMgr->getGatewayIP();
}

IPAddress AutoNetwork::subnetMask()
{
    return _connectionMgr->getSubnetMask();
}

bool AutoNetwork::connect()
{
    if (_an.sta.configured)
    {
        _connect(_an.sta.ssid.c_str(), _an.sta.password.c_str(), true);
        return true;
    }
    return false;
}

bool AutoNetwork::connect(const char *ssid, const char *password)
{
    // Save credentials using new multi-credential system
    AutoNetworkCredentialEntry entry;
    entry.ssid = ssid;
    entry.password = password;
    entry.enterprise = false;
    entry.priority = 0;

    if (_credential.save(entry))
    {
        _an.sta.configured = true;
        _an.sta.ssid = ssid;
        _an.sta.password = password;

        // Connect
        _connect(_an.sta.ssid.c_str(), _an.sta.password.c_str(), true);
        return true;
    }
    return false;
}

void AutoNetwork::erase()
{
    _credential.delAll();
    _an.sta.configured = false;
    _an.sta.ssid = "";
    _an.sta.password = "";
    _an.sta.enterprise = false;
    _an.sta.enterpriseNetId = "";
    AN_LOGI(TAG, "All credentials erased");
}

void AutoNetwork::disconnect()
{
    _disconnect();
}

void AutoNetwork::reset()
{
    // Erase all credentials
    _credential.delAll();

    _an.sta.configured = false;
    _an.sta.ssid = "";
    _an.sta.password = "";
    _an.sta.enterprise = false;
    _an.sta.enterpriseNetId = "";

    // Disconnect from wifi
    _disconnect();
    _an.status = AutoNetworkConnectionStatus::DISCONNECTED;

    // Stop captive portal
    if (_portal->isActive())
    {
        // Set exit flag
        _portal->scheduleExit();
    }

    AN_LOGI(TAG, "AutoNetwork reset complete");
}

// Loop Extraction Methods (Task 2.1)
// ****************************************************************************

void AutoNetwork::_updateConnectionStatus()
{
    static AutoNetworkConnectionStatus lastStatus = AutoNetworkConnectionStatus::DISCONNECTED;
    if (_an.status != lastStatus)
    {
        _updateTicker();
        lastStatus = _an.status;
    }
}

void AutoNetwork::_processTickerAnimation()
{
    if (_ticker != nullptr)
    {
        _ticker->update();
    }
}

void AutoNetwork::_handleWebServerRequests()
{
    // AsyncWebServer handles requests automatically - no manual polling needed
}

void AutoNetwork::_monitorWiFiConnection()
{
    // Store previous established state to detect transitions
    bool wasEstablished = _established;

    // Delegate core connection monitoring to connection manager
    _connectionMgr->monitorConnection();

    // Update lastUsed timestamp when connection is first established (RECENT principle)
    // This ensures the most recently successful credential is tried first on next boot
    if (!wasEstablished && _established)
    {
        String currentSSID = WiFi.SSID();
        if (currentSSID.length() > 0)
        {
            unsigned long timestamp = _getMonotonicTimestamp();
            if (_credential.updateLastUsed(currentSSID.c_str(), timestamp))
            {
                AN_LOGD(TAG, "Updated lastUsed timestamp for %s: %lu", currentSSID.c_str(), timestamp);
            }
        }
    }

    // Handle portal reactivation logic (AutoNetwork-specific behavior)
    // Only if connection was lost (transition from established to not established)
    if (wasEstablished && !_established)
    {
        // Reactivate captive portal if autoRise is enabled
        if (_config.staAutoRise && !_portal->isManualConnection())
        {
            AN_LOGI(TAG, "Reactivating captive portal due to WiFi disconnection");
            AN_LOGI(TAG, "  retainPortal=%s, autoRise=%s",
                     _config.portalRetain ? "true" : "false",
                     _config.staAutoRise ? "true" : "false");

            // Disable WiFi auto-reconnect to ensure portal detection works
            if (WiFi.getAutoReconnect())
            {
                WiFi.setAutoReconnect(false);
            }

            // Restart SoftAP if not already running
            // Portal start will automatically initialize DNS server
            if (!(WiFi.getMode() & WIFI_AP))
            {
                AN_LOGD(TAG, "Starting SoftAP for portal reactivation");
                _startPortal();
            }
        }
    }
}

void AutoNetwork::_processBackgroundReconnection()
{
    // Background reconnection attempts (if enabled and disconnected)
    // autoReconnect takes priority over portal
    // When both autoReconnect and autoRise are enabled:
    //   - Try reconnection first (background scanning)
    //   - Portal serves as fallback UI for manual configuration
    // Only skip reconnection if portal is in ACTIVE USE (not just running in background)

    // Stop background reconnection when portal is in active use
    // Background WiFi scanning/reconnection causes instability when clients are configuring
    // Check if portal has connected clients - if so, skip reconnection attempts
    bool portalInActiveUse = _portal->isActive() && (WiFi.softAPgetStationNum() > 0);

    // Debug logging for troubleshooting
    static bool lastPortalInActiveUse = false;
    if (portalInActiveUse != lastPortalInActiveUse)
    {
        AN_LOGI(TAG, "Portal in active use changed: %d (active=%d, stations=%d)",
                 portalInActiveUse, _portal->isActive(), WiFi.softAPgetStationNum());
        lastPortalInActiveUse = portalInActiveUse;
    }

    if (_config.staAutoReconnect && _config.staReconnectInterval > 0 &&
        WiFi.status() != WL_CONNECTED &&
        !portalInActiveUse && !_portal->isManualConnection()) // Skip reconnection when portal is actively being used or manual connection is in progress
    {
        int8_t scanComplete = WiFi.scanComplete();

        // Scan has not been triggered - start async scan at configured intervals
        if (scanComplete == WIFI_SCAN_FAILED)
        {
            unsigned long interval = (unsigned long)_config.staReconnectInterval * AUTONETWORK_UNITTIME * 1000;
            if (millis() - _attemptPeriod > interval)
            {
                AN_LOGD(TAG, "Starting background WiFi scan for reconnection (interval=%lums)", interval);
                _disconnect();                                     // Clean disconnect before scanning
                int8_t scanResult = WiFi.scanNetworks(true, true); // Async scan with show_hidden=true
                if (scanResult == WIFI_SCAN_RUNNING)
                {
                    AN_LOGD(TAG, "Background scan started successfully");
                }
                else
                {
                    AN_LOGW(TAG, "Background scan failed to start: %d", scanResult);
                }
                _attemptPeriod = millis();
            }
        }
        // Scan is complete - attempt to reconnect to a known network
        else if (scanComplete != WIFI_SCAN_RUNNING && scanComplete > 0)
        {
            AN_LOGD(TAG, "Background scan found %d networks, checking credentials", scanComplete);

            // Use pre-allocated buffer to avoid heap fragmentation in loop
            // Buffer is a class member, preventing repeated allocation/deallocation
            uint8_t maxEntries = AUTONETWORK_MAX_RECONNECT_ENTRIES;
            AutoNetworkCredentialEntry *entries = _reconnectBuffer; // Use pre-allocated buffer

            uint8_t count = _credential.entries();
            if (count > maxEntries)
                count = maxEntries;

            // Load credentials using RECENT principle (most recently used first)
            for (uint8_t i = 0; i < count; i++)
            {
                bool loadSuccess = _credential.getByRecent(i, entries[i]);

                if (!loadSuccess)
                {
                    AN_LOGW(TAG, "Failed to load credential at index %d", i);
                    count = i; // Reduce count to only successfully loaded entries
                    break;
                }
            }

            if (count > 0)
            {
                AN_LOGD(TAG, "Checking %d saved credentials against scan results", count);

                // Use ScanManager to find best matching credential (eliminates duplicate RECENT principle code)
                int16_t bestCredentialIndex = _scanManager.findBestMatchingCredential(
                    entries, count, _config.staMinRSSI, _config.staMatchBSSID);

                // Attempt connection to best match
                if (bestCredentialIndex >= 0)
                {
                    AutoNetworkCredentialEntry &cred = entries[bestCredentialIndex];
                    AN_LOGI(TAG, "Background reconnection attempt to: %s (using RECENT principle)",
                             cred.ssid.c_str());

                    if (cred.enterprise)
                    {
                        _connectEnterprise(cred.ssid.c_str(), cred.enterpriseNetId.c_str(), cred.password.c_str());
                    }
                    else
                    {
                        _connect(cred.ssid.c_str(), cred.password.c_str(), false, &cred);
                    }
                }
                else
                {
                    AN_LOGD(TAG, "No matching saved credentials found in scan results");
                }
            }
            else
            {
                AN_LOGD(TAG, "No saved credentials available for reconnection");
            }

            // Note: No need to delete entries - using pre-allocated buffer

            // Clean up scan results
            WiFi.scanDelete();
        }
    }
}

void AutoNetwork::loop()
{
    // DEBUG: Log that loop() is being called (verbose only - repetitive heartbeat)
    static unsigned long lastLoopLog = 0;
    if (millis() - lastLoopLog > AUTONETWORK_INTERVAL_LOOP_LOG_MS) // Every 10 seconds
    {
        AN_LOGV(TAG, "[AutoNetwork] loop() called - portal.active=%d, portal.state=%d, timeout=%lu",
                 _portal->isActive(), (int)_portal->getState(), _portal->getTimeout());
        lastLoopLog = millis();
    }

    _updateConnectionStatus();
    _processTickerAnimation();
    _handleWebServerRequests();

    // DNS processing moved to AutoNetworkPortal::loop() in Phase 4 refactoring

    _monitorWiFiConnection();

    // Background reconnection (extracted method call replaces inline code below)
    _processBackgroundReconnection();

    // Check scheduled disconnect (from disconnect countdown page)
    if (_portal->isDisconnectScheduled())
    {
        if (millis() - _portal->getDisconnectTime() > AUTONETWORK_TIMEOUT_DISCONNECT_MS)
        {
            AN_LOGI(TAG, "Disconnect countdown complete - disconnecting from WiFi");
            _disconnect();
            _portal->clearDisconnect();
        }
    }

    // Check portal timeout
    // Portal stays active throughout captive portal phase
    // Timeout only applies when NO stations are connected (idle waiting)
    // When stations connect, timer resets to allow ongoing configuration
    if (_portal->isActive())
    {
        // Reset timeout timer if stations are connected to AP
        // This keeps portal active during entire user configuration session
        uint8_t stationNum = WiFi.softAPgetStationNum();
        if (stationNum > 0)
        {
            _portal->setTimeStart(millis());
        }

        // DEBUG: Log timeout configuration every 30 seconds
        static unsigned long lastTimeoutLog = 0;
        if (millis() - lastTimeoutLog > AUTONETWORK_INTERVAL_TIMEOUT_LOG_MS)
        {
            AN_LOGV(TAG, "[AutoNetwork] Portal timeout config: timeout=%lu, timeStart=%lu, elapsed=%lu, stations=%d",
                     _portal->getTimeout(), _portal->getTimeStart(), millis() - _portal->getTimeStart(), stationNum);
            lastTimeoutLog = millis();
        }

        // Check if portal has timed out (only if timeout is configured AND no stations connected)
        // Portal timeout only applies when idle (no users configuring)
        // During active configuration (stations connected), portal stays open indefinitely
        bool timedOut = false;
        if (_portal->getTimeout() > 0 && stationNum == 0)
        {
            timedOut = (millis() - _portal->getTimeStart()) > _portal->getTimeout();
        }

        if (timedOut)
        {
            // Portal timed out with no active connections
            AN_LOGW(TAG, "Portal timeout reached (timeout=%lums, elapsed=%lums)",
                     _portal->getTimeout(), millis() - _portal->getTimeStart());
            AN_LOGW(TAG, "  No stations connected to AP - portal was idle");

            // Handle timeout based on retainPortal setting
            if (!_config.portalRetain)
            {
                AN_LOGI(TAG, "Stopping portal (retainPortal=false)");
                _stopPortal();
                _portal->setState(AutoNetworkPortalState::IDLE);
            }
            else
            {
                AN_LOGI(TAG, "Portal timeout but retained (retainPortal=true)");
            }
        }
    }

    // Portal state machine processing delegated to AutoNetworkPortal::loop()
    // State machine moved to AutoNetworkPortal::_processStateMachine() in Phase 4 refactoring
    _portal->loop();

    // If exit flag is set, stop portal after timeout
    if (_portal->shouldExit() &&
        ((unsigned long)(millis() - _portal->getExitTime()) >
         AUTONETWORK_EXIT_TIMEOUT))
    {
        _stopPortal();
        _portal->clearExit();
        // Handled by clearExit()
    }

    // Connection Status Listener
    static AutoNetworkConnectionStatus statusLast;
    if (_an.status_cb != nullptr && _an.status != statusLast)
    {
        statusLast = _an.status;
        _an.status_cb(_an.status);
    }

    // Portal State Listener
    static AutoNetworkPortalState stateLast;
    if (_portal->getStateCallback() != nullptr && _portal->getState() != stateLast)
    {
        stateLast = _portal->getState();
        _portal->getStateCallback()(_portal->getState());
    }
}

void AutoNetwork::startPortal()
{
    return _startPortal();
}

void AutoNetwork::stopPortal()
{
    return _stopPortal();
}

uint32_t AutoNetwork::nextId()
{
    return _portal->nextParameterId();
}

void AutoNetwork::addParameter(AutoNetworkParameter *parameter)
{
    _portal->getParameters().PushBack(parameter);
}

void AutoNetwork::removeParameter(AutoNetworkParameter *parameter)
{
    for (size_t i = 0; i < _portal->getParameters().Size(); i++)
    {
        AutoNetworkParameter *p = _portal->getParameters()[i];
        if (p->_id == parameter->_id)
        {
            _portal->getParameters().Erase(i);
            return;
        }
    }
}

bool AutoNetwork::_connectEnterprise(
    const char *ssid,
    const char *netid,
    const char *password)
{
    // Delegate to connection manager
    return _connectionMgr->connectEnterprise(ssid, netid, password);
}

void AutoNetwork::_connect(
    const char *ssid,
    const char *password,
    bool flagAutoReconnect,
    const AutoNetworkCredentialEntry *credential)
{
    // Apply pending hostname if WiFi mode is now suitable (STA or AP_STA)
    _applyPendingHostname();
    
    // Delegate to connection manager
    _connectionMgr->connect(ssid, password, flagAutoReconnect, credential);
}

void AutoNetwork::_disconnect()
{
    // Delegate to connection manager
    _connectionMgr->disconnect();
}

// Provide access to credential storage
AutoNetworkCredential *AutoNetwork::credential()
{
    return &_credential;
}

bool AutoNetwork::_isIp(String str)
{
    for (size_t i = 0; i < str.length(); i++)
    {
        char c = str.charAt(i);
        if (c != '.' && (c < '0' || c > '9'))
        {
            return false;
        }
    }
    return true;
}

// Helper method to determine WiFi mode based on configuration flags
// Simplifies complex boolean logic in multiple locations
WiFiMode_t AutoNetwork::_determineWiFiMode() const
{
    // Priority 1: preserveAPMode explicitly set (e.g., for ESP-MESH/ESP-NOW)
    // Maintains AP mode while adding STA functionality
    if (_config.staPreserveAPMode && !_config.staAutoRise)
    {
        return WIFI_AP_STA;
    }

    // Priority 2: Portal is active and should be retained after connection
    // Keeps portal accessible for reconfiguration while WiFi is connected
    if (_portal->isActive() && _config.portalRetain)
    {
        return WIFI_AP_STA;
    }

    // Default: Standard STA-only mode for typical WiFi client usage
    return WIFI_STA;
}

void AutoNetwork::_generateStatusJson(String &str)
{
    JsonDocument json;

    json["conn"]["status"] = (uint8_t)_an.status;
    json["conn"]["wifiConnected"] = (WiFi.status() == WL_CONNECTED);
    json["conn"]["ssid"] = _an.sta.ssid.c_str();
    json["conn"]["mac"] = WiFi.macAddress();
    json["conn"]["ip"] = WiFi.localIP().toString();
    json["portal"]["state"] = (uint8_t)_portal->getState();
    json["portal"]["active"] = _portal->isActive();

    // Serialize JSON to string
    serializeJson(json, str);

    // Clear JSON document
    json.clear();
}

void AutoNetwork::_generateSchemaJson(String &str)
{
    JsonDocument json;

    JsonArray arr = json.to<JsonArray>();

    // Create nested objects
    for (size_t i = 0; i < _portal->getParameters().Size(); i++)
    {
        AutoNetworkParameter *p = _portal->getParameters()[i];
        JsonObject obj = arr.add<JsonObject>();
        obj["id"] = p->_id;
        obj["t"] = paramTypes[p->_type].type;
        obj["n"] = p->_name;
        obj["v"] = p->_value;
        obj["p"] = p->_placeholder;
        obj["r"] = p->_required;
    }

    // Serialize JSON to string
    serializeJson(json, str);

    // Clear JSON document
    json.clear();
}

void AutoNetwork::_generateScanJson(String &str)
{
    // Delegate to ScanManager for JSON generation
    _scanManager.generateScanJson(str);
}

bool AutoNetwork::_parseConfigJson(JsonArray &arr)
{
    // Validate parameters
    for (uint8_t i = 0; i < arr.size(); i++)
    {
        JsonObject obj = arr[i];

        // Check if id and value are present
        if (!obj["id"].is<JsonVariant>() || !obj["v"].is<JsonVariant>())
        {
            return false;
        }

        // Value must be a string
        if (!obj["v"].is<const char *>())
        {
            return false;
        }
    }

    // Parse parameters
    for (uint8_t i = 0; i < arr.size(); i++)
    {
        JsonObject obj = arr[i];
        for (size_t j = 0; j < _portal->getParameters().Size(); j++)
        {
            AutoNetworkParameter *p = _portal->getParameters()[j];
            if (p->_id == obj["id"].as<uint32_t>())
            {
                p->_value = obj["v"].as<const char *>();
                break;
            }
        }
    }

    if (_portal->getConfigCallback() != nullptr)
    {
        return _portal->getConfigCallback()();
    }
    else
    {
        return true;
    }
}

bool AutoNetwork::_parseCredentialsJson(JsonObject &obj)
{
    if (obj["ssid"].is<const char *>() && obj["password"].is<const char *>())
    {
        _portal->setSTASSID(obj["ssid"].as<const char *>());
        _portal->setSTAPassword(obj["password"].as<const char *>());

        // Parse enterprise fields if present
        if (obj["enterprise"].is<bool>() && obj["enterprise"].as<bool>())
        {
            _portal->setEnterpriseMode(true);
            if (obj["netid"].is<const char *>())
            {
                _portal->setEnterpriseNetId(obj["netid"].as<const char *>());
                AN_LOGI(TAG, "Enterprise credentials parsed");
                AN_LOGD(TAG, "Enterprise - NetID: %s",
                         _portal->getEnterpriseNetId().c_str());
            }
        }
        else
        {
            _portal->setEnterpriseMode(false);
            _portal->setEnterpriseNetId("");
        }

        // FIX #2: Set portal state AND activate portal for state machine processing
        // This ensures the state machine executes even when portal.active was previously false
        _portal->setState(AutoNetworkPortalState::CONNECTING_WIFI);
        _portal->setActive(true);          // Ensure state machine executes
        _portal->setTimeConnect(millis()); // Track connection start time for timeout handling
        AN_LOGI(TAG, "Starting WiFi connection to SSID: %s (Ent: %s)",
                 _portal->getSTASSID().c_str(),
                 _portal->isEnterpriseMode() ? "Yes" : "No");

        return true;
    }
    else
    {
        return false;
    }
}

void AutoNetwork::_restartScan()
{
    AN_LOGI(TAG, "[AutoNetwork] _restartScan() called - delegating to ScanManager");
    
    // Delegate scan management to ScanManager
    // ScanManager handles:
    // - WiFi mode validation and switching (NULL/AP -> AP_STA)
    // - Scan configuration (WIFI_ALL_CHANNEL_SCAN, WIFI_CONNECT_AP_BY_SIGNAL)
    // - Async scan initiation with error handling
    // - Scan state tracking via portal updates
    int16_t result = _scanManager.startScan();
    
    // Update portal state based on scan result
    if (result == WIFI_SCAN_FAILED)
    {
        AN_LOGW(TAG, "[AutoNetwork] WiFi scan failed to start - WiFi may not be ready yet");
        _portal->setScanActive(false);
        AN_LOGI(TAG, "[AutoNetwork] Scan state: active=false (scan failed to start)");
    }
    else
    {
        // Scan is either running, will start, or completed instantly
        // Trust the WiFi subsystem - WiFi.scanComplete() handles actual state checking
        AN_LOGI(TAG, "[AutoNetwork] WiFi scan initiated (result=%d)", result);
        _portal->setScanStartTime(millis());
        _portal->setScanActive(true);
        AN_LOGI(TAG, "[AutoNetwork] Scan state updated - active=true, timeStart=%lu", _portal->getScanStartTime());
    }
}

void AutoNetwork::_startHTTP()
{
    // HTTP server startup is now handled by AutoNetworkPortal
    // Portal registers all endpoints in _registerEndpoints()
    if (!_serverRunning)
    {
        AN_LOGI(TAG, "Starting HTTP server (delegated to portal)");
        _portal->startHTTP();
        _serverRunning = true;
    }
}

void AutoNetwork::_stopHTTP()
{
    // HTTP server shutdown is now handled by AutoNetworkPortal
    if (_serverRunning)
    {
        AN_LOGI(TAG, "Stopping HTTP server (delegated to portal)");
        _portal->stopHTTP();
        _serverRunning = false;
    }
}

// REMOVED: All HTTP endpoint registration moved to AutoNetworkPortal::_registerEndpoints()
// The following 950+ lines of duplicate endpoint handlers have been removed.
// See AutoNetworkPortal.cpp lines 663-1514 for the canonical endpoint registration.

void AutoNetwork::_startPortal()
{
    // Delegate portal start to AutoNetworkPortal
    // Portal manages its own DNS server, HTTP endpoints, and SoftAP lifecycle
    // Note: Portal will call _onUpdateTicker callback to update ticker pattern
    _portal->start();
}

void AutoNetwork::_stopPortal()
{
    // Delegate portal stop to AutoNetworkPortal
    // Portal manages its own DNS server, HTTP endpoints, and SoftAP lifecycle
    // Note: Portal will call _onUpdateTicker callback to update ticker pattern
    _portal->stop();
}

bool AutoNetwork::_onAPFilter(AsyncWebServerRequest *request)
{
    // Allow requests when portal is active (stations connected to AP)
    // The filter should allow any client connected to our SoftAP
    return WiFi.softAPgetStationNum() > 0;
}

// Convenience Methods
// ****************************************************************************

/**
 * @brief Starts WiFi connection
 */
bool AutoNetwork::begin()
{
    // CRITICAL: Initialize WiFi subsystem to prepare TCP/IP stack
    // This must be done BEFORE reading MAC address
    if (WiFi.getMode() == WIFI_MODE_NULL)
    {
        // Disable WiFi persistence FIRST to prevent ESP-IDF auto-reconnect
        // This prevents the framework from creating unwanted APs from cached config
        WiFi.persistent(false);
        WiFi.mode(WIFI_STA);
        AN_LOGI(TAG, "WiFi subsystem initialized (persistence disabled)");
        
        // Apply any hostname that was set before WiFi initialization
        _applyPendingHostname();
    }

    // Register WiFi event callback for mDNS auto-configuration
    WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
        switch (event) {
            case ARDUINO_EVENT_WIFI_STA_GOT_IP:
                AN_LOGI(TAG, "WiFi connected, starting mDNS");
                _startMDNS();
                break;
            case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
                AN_LOGI(TAG, "WiFi disconnected, stopping mDNS");
                _stopMDNS();
                break;
            default:
                break;
        }
    });

    autoConnect(_portal->getAPSSID().c_str(), _portal->getAPPassword().c_str());

    // Disable WiFi sleep mode and automatic reconnection
    // setSleep(false) prevents WiFi power management interference
    // setAutoReconnect(false) prevents automatic reconnects that could conflict with manual connection logic
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(false);

    return WiFi.status() == WL_CONNECTED;
}

/**
 * @brief Stop AutoNetwork and release resources
 * Disconnects WiFi, stops portal, and cleans up
 */
void AutoNetwork::end()
{
    AN_LOGI(TAG, "Shutting down AutoNetwork");

    // Stop portal if active
    if (_portal->isActive())
    {
        stopPortal();
    }

    // Disconnect WiFi
    disconnect();

    // Stop ticker
    _stopTicker();

    // Stop HTTP server
    _stopHTTP();
}


/**
 * @brief Get reference to the web server
 * @return Pointer to AsyncWebServer instance
 */
AsyncWebServer *AutoNetwork::host()
{
    return _server;
}

/**
 * @brief Check if captive portal is currently active
 * @return true if portal is running, false otherwise
 */
bool AutoNetwork::isPortalAvailable()
{
    return _portal->isActive();
}

/**
 * @brief Get current configuration
 * @return Reference to AutoNetworkConfig object
 */
AutoNetworkConfig &AutoNetwork::getConfig()
{
    return _config;
}

/**
 * @brief Get current configuration (const version)
 * @return Const reference to AutoNetworkConfig object
 */
const AutoNetworkConfig& AutoNetwork::getConfig() const
{
    return _config;
}

/**
 * @brief Save current WiFi credentials to flash memory
 * Stores the currently connected SSID and password
 */
void AutoNetwork::saveCredential()
{
    if (_an.sta.ssid.length() == 0)
    {
        AN_LOGW(TAG, "No credentials to save (SSID empty)");
        return;
    }

    AN_LOGI(TAG, "Saving credential: %s", _an.sta.ssid.c_str());

    // Create entry structure
    AutoNetworkCredentialEntry entry;
    entry.ssid = _an.sta.ssid;
    entry.password = _an.sta.password;
    entry.enterprise = _an.sta.enterprise;
    entry.enterpriseNetId = _an.sta.enterpriseNetId;

    // Use the credential manager to save
    _credential.save(entry);
}

/**
 * @brief Restore WiFi credentials from flash memory
 * Loads saved credentials into memory (does not connect)
 */
void AutoNetwork::restoreCredential()
{
    AN_LOGI(TAG, "Restoring credentials from flash");

    // Check if any credentials exist
    uint8_t count = _credential.entries();
    if (count > 0)
    {
        AN_LOGI(TAG, "Found %d saved credential(s)", count);

        // Load first credential
        AutoNetworkCredentialEntry entry;
        if (_credential.load(0, entry))
        {
            _an.sta.ssid = entry.ssid;
            _an.sta.password = entry.password;
            _an.sta.enterprise = entry.enterprise;
            _an.sta.enterpriseNetId = entry.enterpriseNetId;
            _an.sta.configured = true;
        }
    }
    else
    {
        AN_LOGW(TAG, "No saved credentials found");
        _an.sta.configured = false;
    }
}

// Web Server Delegation Methods
// ****************************************************************************

/**
 * @brief Register a handler for web server requests (delegates to underlying web server)
 */
AsyncCallbackWebHandler &AutoNetwork::on(const char *uri, WebRequestMethodComposite method,
                                         ArRequestHandlerFunction onRequest)
{
    return _server->on(uri, method, onRequest);
}

// Destructor
// ****************************************************************************

AutoNetwork::~AutoNetwork()
{
    // Clear all parameters BEFORE deleting portal (prevents use-after-free)
    if (_portal != nullptr)
    {
        // Clean up parameters while portal is still valid
        for (size_t i = 0; i < _portal->getParameters().Size(); i++)
        {
            AutoNetworkParameter *p = _portal->getParameters()[i];
            delete p;
        }

        // Now safe to delete portal
        delete _portal;
        _portal = nullptr;
    }

    // Stop and delete connection manager
    if (_connectionMgr != nullptr)
    {
        delete _connectionMgr;
        _connectionMgr = nullptr;
    }

    // Stop and delete ticker
    if (_ticker != nullptr)
    {
        delete _ticker;
        _ticker = nullptr;
    }
}

// Start ticker based on WiFi status
// ****************************************************************************
void AutoNetwork::_startTicker()
{
    if (!_an.ticker.enabled)
    {
        AN_LOGD(TAG, "_startTicker: ticker not enabled");
        return;
    }

    // Create ticker if it doesn't exist
    if (_ticker == nullptr)
    {
        AN_LOGD(TAG, "_startTicker: creating ticker on GPIO %d with activeLevel=%d",
                _an.ticker.port, _an.ticker.activeLevel);
        _ticker = new AutoNetworkTicker(
            _an.ticker.port,
            _an.ticker.activeLevel);
        AN_LOGI(TAG, "Created ticker on GPIO %d", _an.ticker.port);
    }

    // Update ticker pattern based on current WiFi status
    // This ensures the correct pattern is set regardless of connection state
    _updateTicker();
}

// Stop ticker
// ****************************************************************************
void AutoNetwork::_stopTicker()
{
    if (_ticker != nullptr)
    {
        _ticker->stop();
        AN_LOGV(TAG, "Ticker stopped");
    }
}

// Update ticker based on current WiFi state
// ****************************************************************************
void AutoNetwork::_updateTicker()
{
    if (!_an.ticker.enabled || _ticker == nullptr)
    {
        AN_LOGD(TAG, "_updateTicker: ticker disabled or null (enabled=%d, _ticker=%p)",
                _an.ticker.enabled, _ticker);
        return;
    }

    // Determine blink pattern based on state
    // Priority: WiFi connected > Portal active > Disconnected
    // WiFi connection status takes precedence over portal state

    AN_LOGD(TAG, "_updateTicker: WiFi.status=%d, portal.isActive=%d",
            WiFi.status(), _portal->isActive());

    if (WiFi.status() == WL_CONNECTED)
    {
        // Connected: solid on (highest priority)
        AN_LOGD(TAG, "_updateTicker: WiFi connected, setting SOLID_ON");
        _ticker->start(AutoNetworkTickerPattern::SOLID_ON);
        AN_LOGI(TAG, "Ticker: connected (solid on)");
    }
    else if (_portal->isActive())
    {
        // Portal active (AP mode): slow blink pattern (500ms on, 500ms off)
        AN_LOGD(TAG, "_updateTicker: Portal active, setting SLOW_BLINK");
        _ticker->start(AutoNetworkTickerPattern::SLOW_BLINK);
        AN_LOGI(TAG, "Ticker: portal pattern");
    }
    else
    {
        // Disconnected: fast blink pattern (150ms on, 150ms off)
        AN_LOGD(TAG, "_updateTicker: Disconnected, setting FAST_BLINK");
        _ticker->start(AutoNetworkTickerPattern::FAST_BLINK);
        AN_LOGI(TAG, "Ticker: disconnected pattern");
    }
}

// _getMonotonicTimestamp - Generate monotonic timestamp that persists across reboots
// ****************************************************************************
unsigned long AutoNetwork::_getMonotonicTimestamp()
{
    // Use Preferences to maintain a connection counter
    // Simply increment on each connection - no complex math needed
    Preferences prefs;
    prefs.begin("AutoNetwork", false); // Read-write mode

    // Get and increment connection counter
    unsigned long timestamp = prefs.getULong("connCnt", 0);
    timestamp++;
    prefs.putULong("connCnt", timestamp);

    prefs.end();

    // Return the counter as timestamp
    // Each connection gets next sequential number: 1, 2, 3, 4...
    // Higher number = more recent connection
    AN_LOGD(TAG, "Generated monotonic timestamp: %lu", timestamp);

    return timestamp;
}

// _startMDNS - Start mDNS service with hostname
// ****************************************************************************
void AutoNetwork::_startMDNS()
{
    // Stop mDNS if already running to ensure clean state
    MDNS.end();

    // Start mDNS with the configured hostname (without .local suffix)
    if (MDNS.begin(_config.staHostName.c_str()))
    {
        AN_LOGI(TAG, "mDNS started: %s.local", _config.staHostName.c_str());

        // Add HTTP service for service discovery
        MDNS.addService("http", "tcp", AUTONETWORK_HTTP_PORT);
        AN_LOGI(TAG, "mDNS HTTP service added on port %d", AUTONETWORK_HTTP_PORT);
    }
    else
    {
        AN_LOGW(TAG, "Failed to start mDNS for hostname: %s", _config.staHostName.c_str());
    }
}

// _stopMDNS - Stop mDNS service
// ****************************************************************************
void AutoNetwork::_stopMDNS()
{
    MDNS.end();
    AN_LOGI(TAG, "mDNS stopped");
}

// _setHostname - Set WiFi hostname with defensive validation
// ****************************************************************************
void AutoNetwork::_setHostname(const char* hostname)
{
    // Layer 1: Entry validation - hostname requirements
    if (hostname == nullptr || hostname[0] == '\0')
    {
        AN_LOGW(TAG, "_setHostname rejected: null or empty");
        return;
    }
    
    wifi_mode_t currentMode = WiFi.getMode();
    AN_LOGD(TAG, "_setHostname: hostname=%s, current_mode=%d", 
            hostname, currentMode);
    
    // Layer 2: Business validation - hostname only meaningful in STA/AP_STA mode
    if (currentMode == WIFI_MODE_NULL || currentMode == WIFI_MODE_AP)
    {
        AN_LOGW(TAG, "_setHostname: WiFi mode %d does not support hostname (STA required)", 
                currentMode);
        AN_LOGW(TAG, "Hostname '%s' will be applied when STA mode is activated", hostname);
        
        // Store for later application (DO NOT force mode change)
        _pendingHostname = String(hostname);
        return;
    }
    
    // Layer 3: Apply hostname in valid mode (STA or AP_STA)
    #ifdef UNIT_TEST
    AN_LOGD(TAG, "_setHostname: UNIT_TEST mode");
    #endif
    
    bool result = WiFi.setHostname(hostname);
    
    // Layer 4: Log result with verification
    if (result)
    {
        String actualHostname = WiFi.getHostname();
        if (actualHostname == hostname)
        {
            AN_LOGI(TAG, "_setHostname: Success - hostname=%s (mode=%d)", 
                    hostname, currentMode);
        }
        else
        {
            AN_LOGW(TAG, "_setHostname: Set returned true but hostname mismatch: wanted=%s, got=%s",
                    hostname, actualHostname.c_str());
        }
    }
    else
    {
        #ifdef UNIT_TEST
        AN_LOGD(TAG, "_setHostname: WiFi.setHostname() returned false (expected in test)");
        #else
        AN_LOGW(TAG, "_setHostname: WiFi.setHostname() returned false");
        #endif
    }
}

// _applyPendingHostname - Apply hostname deferred from pre-initialization
// ****************************************************************************
void AutoNetwork::_applyPendingHostname()
{
    if (_pendingHostname.length() > 0)
    {
        wifi_mode_t currentMode = WiFi.getMode();
        if (currentMode == WIFI_MODE_STA || currentMode == WIFI_MODE_APSTA)
        {
            AN_LOGI(TAG, "Applying pending hostname: %s", _pendingHostname.c_str());
            _setHostname(_pendingHostname.c_str());
            _pendingHostname = "";  // Clear after application
        }
    }
}

// Root Content Configuration Methods
// ****************************************************************************

void AutoNetwork::setRootContent(const String& filePath)
{
    _an.rootContentPath = filePath;
    _an.rootContentType = _an.RootContentType::FILE;
    AN_LOGI(TAG, "Root content set to file: %s", filePath.c_str());
}

void AutoNetwork::setRootContent(std::function<String()> callback)
{
    _an.rootContentCallback = callback;
    _an.rootContentType = _an.RootContentType::CALLBACK;
    AN_LOGI(TAG, "Root content set to callback function");
}

void AutoNetwork::setRootContentHTML(const char* htmlContent)
{
    _an.rootContentDirect = String(htmlContent);
    _an.rootContentType = _an.RootContentType::DIRECT;
    AN_LOGI(TAG, "Root content set to direct HTML (%d bytes)", _an.rootContentDirect.length());
}

void AutoNetwork::setRootMenuReplacement(const String& replacement)
{
    _an.rootMenuReplacement = replacement;
}

// Root Content Retrieval Methods
// ****************************************************************************

String AutoNetwork::_getRootContent()
{
    String content;

    switch (_an.rootContentType)
    {
        case _an.RootContentType::FILE:
            if (LittleFS.exists(_an.rootContentPath))
            {
                File file = LittleFS.open(_an.rootContentPath, "r");
                if (file)
                {
                    content = file.readString();
                    file.close();
                    AN_LOGD(TAG, "Read %d bytes from %s", content.length(), _an.rootContentPath.c_str());
                }
                else
                {
                    AN_LOGW(TAG, "Failed to open file: %s", _an.rootContentPath.c_str());
                }
            }
            else
            {
                AN_LOGW(TAG, "File not found: %s", _an.rootContentPath.c_str());
            }
            break;

        case _an.RootContentType::CALLBACK:
            content = _an.rootContentCallback();
            AN_LOGD(TAG, "Generated content from callback (%d bytes)", content.length());
            break;

        case _an.RootContentType::DIRECT:
            content = _an.rootContentDirect;
            AN_LOGD(TAG, "Using direct HTML content (%d bytes)", content.length());
            break;

        case _an.RootContentType::NONE:
        default:
            AN_LOGW(TAG, "No root content configured");
            break;
    }

    return content;
}

bool AutoNetwork::_isContentEmpty(const String& content)
{
    if (content.length() == 0)
        return true;

    // Check if only whitespace
    for (size_t i = 0; i < content.length(); i++)
    {
        if (!isspace(content[i]))
            return false;
    }

    return true;
}

// Root Handler Registration
// ****************************************************************************

void AutoNetwork::_registerRootHandler()
{
    _server->on("/", HTTP_GET, [this](AsyncWebServerRequest *request)
    {
        // Trigger webpage accessed callback
        if (_an.webpageAccessed_cb)
        {
            _an.webpageAccessed_cb();
        }

        // In AP mode, redirect to AutoNetwork menu
        if (WiFi.status() != WL_CONNECTED)
        {
            AN_LOGI(TAG, "AP mode - redirecting root to /_an");
            request->redirect("/_an");
            return;
        }

        // Get root content
        String content = _getRootContent();

        // Check if content is empty
        if (_isContentEmpty(content))
        {
            AN_LOGW(TAG, "Root content is empty!");

            if (_an.rootContentType == _an.RootContentType::FILE)
            {
                AN_LOGW(TAG, "File: %s (not found or empty)", _an.rootContentPath.c_str());
            }

            AN_LOGW(TAG, "Serving error page. Configure with setRootContent()");

            // Serve error page
            String errorPage = AUTONETWORK_ERROR_HTML;
            errorPage.replace("%CSS%", WEBPAGE_CSS);
            request->send(500, "text/html", errorPage);
            return;
        }

        // Replace menu placeholder
        content.replace("{{AUTONETWORK_MENU}}", _an.rootMenuReplacement);

        // Serve content
        request->send(200, "text/html", content);
    });

    AN_LOGI(TAG, "Root handler registered");
}
