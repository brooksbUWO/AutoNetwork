// ****************************************************************************
// Title        : AutoNetwork Library - Portal Manager
// Filename     : 'AutoNetworkPortal.cpp'
// Target MCU   : Espressif ESP32 (Doit DevKit Version 1)
// Description  : Captive portal management implementation
//
// Revision History:
// When         Who         Description of change
// -----------  ----------- -----------------------
// 17-OCT-2025  Brooks      Extracted from AutoNetwork.cpp
//
// ****************************************************************************

#include "AutoNetworkPortal.h"
#include "AutoNetwork.h"
#include "AutoNetworkLog.h"
#include "AutoNetworkJsonBuilder.h"
#include "AutoNetworkRequestValidator.h"

// Include embedded web pages (permanent - always included)
#include "webpage_menu.h"
#include "webpage_stats.h"
#include "webpage_ota.h"
#include "webpage_creds.h"
#include "webpage_reset.h"
#include "webpage_disconnect.h"
#include "webpage_css.h"

#include <LittleFS.h>
#include <Update.h>
#define FILESYSTEM LittleFS

// ESP-IDF Logging Tag
static const char *TAG = "AutoNetworkPortal";

// Constants
#define AUTONETWORK_STATUS_JSON_SIZE 1024
#define AUTONETWORK_SCAN_JSON_SIZE 1024
#define AUTONETWORK_CONFIG_JSON_SIZE 1024

// Timing Constants
const uint32_t TIMEOUT_AP_MODE_MS = 2000;      // AP mode initialization timeout
const uint32_t TIMEOUT_SOFTAP_IP_MS = 2000;    // SoftAP IP assignment timeout
const uint32_t DELAY_PORTAL_SUCCESS_MS = 3000; // Portal success display delay
const uint32_t TIMEOUT_DISCONNECT_MS = 10000;  // WiFi disconnect countdown
const uint32_t DEBOUNCE_SCAN_MS = 2000;        // WiFi scan debounce interval
const uint32_t DELAY_RESET_MS = 2000;          // Reset delay

// Memory Constants
const uint32_t HEAP_WARNING_THRESHOLD = 20000; // Free heap warning threshold (bytes)
const uint32_t HEAP_GOOD_THRESHOLD = 50000;    // Free heap good status threshold (bytes)

// Constructor/Destructor
// ****************************************************************************

AutoNetworkPortal::AutoNetworkPortal(AUTONETWORK_WEBSERVER *server, AutoNetwork *parent)
    : _server(server),
      _parent(parent),
      _dns(nullptr),
      _serverRunning(false),
      _dnsRunning(false),
      _lastScanRequest(0),
      _index_handler(nullptr),
      _status_handler(nullptr),
      _schema_handler(nullptr),
      _scan_handler(nullptr),
      _save_handler(nullptr),
      _clear_handler(nullptr),
      _exit_handler(nullptr),
      _state() // PortalState initialized with default constructor
{
    AN_LOGI(TAG, "AutoNetworkPortal initialized");
}

AutoNetworkPortal::~AutoNetworkPortal()
{
    AN_LOGI(TAG, "AutoNetworkPortal destructor called");

    // Stop portal services
    stop();

    // Clean up DNS server
    if (_dns != nullptr)
    {
        delete _dns;
        _dns = nullptr;
    }

    // Clear parameter list
    _state.getParameters().Clear();

    AN_LOGI(TAG, "AutoNetworkPortal destroyed");
}

// Portal Lifecycle Methods
// ****************************************************************************

void AutoNetworkPortal::start()
{
    AN_LOGI(TAG, "Starting captive portal...");

    // Disable WiFi persistence before mode change
    WiFi.persistent(false);
    WiFi.mode(WIFI_AP_STA);

    // Set hostname AFTER WiFi mode is initialized
    // CRITICAL: WiFi.setHostname() MUST be called AFTER WiFi.mode() on ESP32
    // Calling setHostname() before mode initialization causes ESP32 to revert
    // to default hostname format: esp32-XXXXXX
    if (_onGetHostname)
    {
        String hostname = _onGetHostname();
        if (hostname != "")
        {
            WiFi.setHostname(hostname.c_str());
            AN_LOGD(TAG, "Hostname set to: %s", hostname.c_str());
        }
    }

    // Non-blocking wait for AP mode - using millis() timing
    AN_LOGD(TAG, "Waiting for WiFi AP mode...");
    unsigned long apModeStart = millis();
    while (!(WiFi.getMode() & WIFI_AP))
    {
        yield(); // Allow background tasks

        // Timeout after 2 seconds to prevent infinite loop
        if ((unsigned long)(millis() - apModeStart) > TIMEOUT_AP_MODE_MS)
        {
            AN_LOGW(TAG, "Timeout waiting for AP mode");
            break;
        }
    }
    AN_LOGD(TAG, "WiFi AP mode ready");

    AN_LOGI(TAG, "Starting SoftAP with SSID: %s", _state.getAPSSID().c_str());

    // Check if parent has configured WiFi credentials (via callback)
    if (_onIsConfigured && _onIsConfigured())
    {
        // Stop WiFi connection attempts when starting portal
        AN_LOGI(TAG, "Starting portal - disconnecting from WiFi to prevent interference");
        if (_onDisconnect)
        {
            _onDisconnect();
        }

        // Start in AP+STA mode but WITHOUT active connection attempt
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP(_state.getAPSSID().c_str(), _state.getAPPassword().c_str());
    }
    else
    {
        WiFi.softAP(_state.getAPSSID().c_str(), _state.getAPPassword().c_str());
    }

    AN_LOGI(TAG, "SoftAP started with SSID: %s", WiFi.softAPSSID().c_str());

    // Non-blocking wait for SoftAP IP - using millis() timing
    AN_LOGD(TAG, "Waiting for SoftAP IP...");
    unsigned long softAPIPStart = millis();
    while (!WiFi.softAPIP())
    {
        yield(); // Allow background tasks

        // Timeout after 2 seconds to prevent infinite loop
        if ((unsigned long)(millis() - softAPIPStart) > TIMEOUT_SOFTAP_IP_MS)
        {
            AN_LOGW(TAG, "Timeout waiting for SoftAP IP");
            break;
        }
    }
    AN_LOGD(TAG, "SoftAP ready with IP: %s", WiFi.softAPIP().toString().c_str());

    // Start DNS server
    _startDNS();

    // Start HTTP server and register endpoints
    _startHTTP();
    AN_LOGI(TAG, "HTTP server started");

    // Mark scan as inactive - it will be started on first request
    // Immediate scan after SoftAP start often fails due to WiFi subsystem not ready
    _state.setScanActive(false);
    _state.setScanStartTime(millis());
    AN_LOGD(TAG, "Initial WiFi scan deferred - will start on first /_an/scan request");

    // Store time of Portal start
    _state.setActive(true);
    _state.setBlocking(true);
    _state.setTimeStart(millis());

    // Access config via callback
    if (_onGetConfig)
    {
        const AutoNetworkConfig &config = _onGetConfig();
        AN_LOGI(TAG, "SoftAP %s/%s Ch(%d) IP:%s %s",
                 _state.getAPSSID().c_str(),
                 _state.getAPPassword().length() > 0 ? "****" : "open",
                 config.apChannel,
                 WiFi.softAPIP().toString().c_str(),
                 config.apHidden ? "hidden" : "visible");
    }

    // Update ticker via callback
    if (_onUpdateTicker)
    {
        _onUpdateTicker();
    }

    AN_LOGI(TAG, "Captive portal started successfully");
}

void AutoNetworkPortal::stop()
{
    if (!_state.isActive())
    {
        AN_LOGD(TAG, "Portal already stopped");
        return;
    }

    AN_LOGI(TAG, "Stopping captive portal...");

    // HTTP server is NOT stopped here - it continues running
    // This allows web interface access at the new WiFi IP address
    AN_LOGI(TAG, "Portal closing - HTTP server will remain active on STA IP");

    // Stop DNS server (portal-specific)
    _stopDNS();

    // Stop Portal (respecting retainPortal and preserveAPMode settings via callback)
    if (_onGetConfig)
    {
        const AutoNetworkConfig &config = _onGetConfig();

        if (!config.portalRetain)
        {
            // Standard behavior: disconnect SoftAP
            WiFi.softAPdisconnect(true);

            // Only disable AP if not preserving AP mode
            if (!config.staPreserveAPMode)
            {
                WiFi.enableAP(false);
            }
            else
            {
                AN_LOGI(TAG, "Maintain SoftAP (preserveAPMode=true)");
            }
        }
        else
        {
            AN_LOGI(TAG, "Maintain SoftAP (retainPortal=true)");
        }

        // Handle reconnection based on configuration (via callback)
        if (_onIsConfigured && _onIsConfigured())
        {
            AN_LOGI(TAG, "Connecting to configured connection");

            // Handle WiFi mode based on preserveAPMode setting
            WiFi.persistent(false);
            if (config.staPreserveAPMode && !config.staAutoRise)
            {
                // Maintain AP mode while enabling STA
                bool cs = WiFi.enableSTA(true);
                AN_LOGI(TAG, "WiFi mode %d maintained, STA %s", WiFi.getMode(), cs ? "enabled" : "unavailable");
            }
            else if (!config.portalRetain)
            {
                // Standard mode: Switch to STA only
                WiFi.mode(WIFI_STA);
            }

            // Set hostname AFTER WiFi mode is initialized
            // CRITICAL: WiFi.setHostname() MUST be called AFTER WiFi.mode() on ESP32
            // Calling setHostname() before mode initialization causes ESP32 to revert
            // to default hostname format: esp32-XXXXXX
            if (_onGetHostname)
            {
                String hostname = _onGetHostname();
                if (hostname != "")
                {
                    WiFi.setHostname(hostname.c_str());
                    AN_LOGD(TAG, "Hostname set to: %s", hostname.c_str());
                }
            }

            // Connect using stored credentials (via callback)
            if (_onConnect && _onGetSTASSID && _onGetSTAPassword)
            {
                String configuredSSID = _onGetSTASSID();
                if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == configuredSSID)
                {
                    AN_LOGI(TAG, "Already connected to %s, skipping reconnection in stop()", configuredSSID.c_str());
                }
                else
                {
                    AN_LOGI(TAG, "Connecting to configured connection: %s", configuredSSID.c_str());
                    _onConnect(configuredSSID.c_str(), _onGetSTAPassword().c_str(), true, nullptr);
                }
            }
        }
    }
    else
    {
        AN_LOGI(TAG, "Switching off wifi (not configured)");
        if (_onGetConfig)
        {
            const AutoNetworkConfig &config = _onGetConfig();
            if (!config.portalRetain && !config.staPreserveAPMode)
            {
                WiFi.mode(WIFI_OFF);
            }
        }
    }

    // Reset portal state
    _state.setActive(false);
    _state.setState(AutoNetworkPortalState::IDLE);
    _state.setTimeStart(0);

    // Update ticker based on new state (via callback)
    if (_onUpdateTicker)
    {
        _onUpdateTicker();
    }

    AN_LOGI(TAG, "Captive portal stopped");
}

void AutoNetworkPortal::loop()
{
    if (!_state.isActive() && _state.getState() == AutoNetworkPortalState::IDLE)
    {
        return;
    }

    // Process DNS requests
    if (_dns != nullptr && _dnsRunning)
    {
        _dns->processNextRequest();
    }

    // Check portal timeout
    if (_state.getTimeout() > 0)
    {
        unsigned long elapsed = millis() - _state.getTimeStart();
        if (elapsed >= _state.getTimeout())
        {
            AN_LOGI(TAG, "Portal timeout reached (%lu ms)", _state.getTimeout());
            _state.setState(AutoNetworkPortalState::TIMEOUT);

            // Trigger state callback if registered
            if (_state.getStateCallback() != nullptr)
            {
                _state.getStateCallback()(_state.getState());
            }

            // Stop portal on timeout
            stop();
            return;
        }
    }

    // Check exit flag
    if (_state.shouldExit())
    {
        unsigned long elapsed = millis() - _state.getExitTime();
        if (elapsed >= 1000) // 1 second delay before exit
        {
            AN_LOGI(TAG, "Exit flag detected, stopping portal");
            _state.clearExit();
            stop();
            return;
        }
    }

    // Check success delay
    if (_state.isDelayingSuccess())
    {
        unsigned long elapsed = millis() - _state.getSuccessTime();
        if (elapsed >= DELAY_PORTAL_SUCCESS_MS) // 3 second delay after success
        {
            AN_LOGI(TAG, "Success delay complete, stopping portal");
            _state.clearSuccessDelay();

            // Check retainPortal setting via callback
            if (_onGetConfig)
            {
                const AutoNetworkConfig &config = _onGetConfig();
                if (!config.portalRetain)
                {
                    stop();
                }
            }
            return;
        }
    }

    // Check scheduled disconnect
    if (_state.isDisconnectScheduled())
    {
        unsigned long elapsed = millis() - _state.getDisconnectTime();
        if (elapsed >= TIMEOUT_DISCONNECT_MS) // 10 second countdown
        {
            AN_LOGI(TAG, "Scheduled disconnect executing");
            _state.clearDisconnect();

            // Disconnect from WiFi (via callback)
            if (_onDisconnect)
            {
                _onDisconnect();
            }

            AN_LOGI(TAG, "WiFi disconnected, device accessible at AP IP: %s",
                     WiFi.softAPIP().toString().c_str());
        }
    }

    // Update WiFi Scan State Machine
    _state.updateScanStateMachine();

    // Portal State Machine - processes connection workflow
    // This is the core state machine moved from AutoNetwork.cpp (Phase 4 refactoring)
    _processStateMachine();
}

// Private Methods - State Machine
// ****************************************************************************

void AutoNetworkPortal::_processStateMachine()
{
    // FIX #1: State machine should ALWAYS execute when state != IDLE
    // This allows web-based credential submission to work even when portal.active=false
    // Separates portal timeout handling (in loop()) from connection state processing
    if (_state.getState() == AutoNetworkPortalState::IDLE)
    {
        return;
    }

    // Go through portal state transitions
    switch (_state.getState())
    {
    case AutoNetworkPortalState::IDLE:
        // Nothing to do in IDLE state
        break;

    case AutoNetworkPortalState::CONNECTING_WIFI:
    {
        // Check if currently connected and disconnect first to prevent WiFi driver state corruption
        // This prevents the "NO_AP_FOUND" error when switching networks while connected
        if (WiFi.status() == WL_CONNECTED)
        {
            AN_LOGI(TAG, "Currently connected - initiating disconnect before new connection");

            // Call disconnect callback (executes WiFi.disconnect())
            if (_onDisconnect)
            {
                _onDisconnect();
            }
        }

        // Start non-blocking delay for WiFi driver stabilization
        _state.setDisconnectStartTime(millis());
        _state.setState(AutoNetworkPortalState::DISCONNECTING);
        break;
    }

    case AutoNetworkPortalState::DISCONNECTING:
    {
        // ESP32 WiFi driver needs sufficient time to:
        //   - Send deauth frames to AP
        //   - Clean up TCP connections
        //   - Release radio hardware
        //   - Reset internal state machines
        const uint32_t WIFI_STABILIZATION_DELAY = 500;

        // Non-blocking delay check
        if (millis() - _state.getDisconnectStartTime() > WIFI_STABILIZATION_DELAY)
        {
            AN_LOGI(TAG, "Connecting to temporary credentials");
            AN_LOGD(TAG, "SSID: %s", _state.getSTASSID().c_str());
            AN_LOGV(TAG, "Password: ********");

            // Connect to temporary credentials
            WiFi.persistent(false);

            // Check if this is an enterprise connection from portal
            if (_state.isEnterpriseMode() && _state.getEnterpriseNetId().length() > 0)
            {
                if (_onConnectEnterprise)
                {
                    _onConnectEnterprise(_state.getSTASSID().c_str(),
                                         _state.getEnterpriseNetId().c_str(),
                                         _state.getSTAPassword().c_str());
                }
            }
            else
            {
                if (_onConnect)
                {
                    _onConnect(_state.getSTASSID().c_str(),
                               _state.getSTAPassword().c_str(), false, nullptr);
                }
            }

            _state.setTimeConnect(millis());
            _state.setState(AutoNetworkPortalState::WAITING_FOR_CONNECTION);
        }
        break;
    }

    case AutoNetworkPortalState::WAITING_FOR_CONNECTION:
    {
        wl_status_t wifiStatus = WiFi.status();

        if (wifiStatus == WL_CONNECTED)
        {
            // WiFi.status() can be WL_CONNECTED but IP is still 0.0.0.0
            // Non-blocking check - uses millis() timing instead of delay()
            IPAddress localIP = WiFi.localIP();
            unsigned long ipWaitStart = millis();
            while ((uint32_t)localIP == 0UL)
            {
                // Non-blocking check with yield - no delay()
                yield();
                localIP = WiFi.localIP();

                // Timeout after 5 seconds to prevent infinite loop
                if ((unsigned long)(millis() - ipWaitStart) > 5000)
                {
                    AN_LOGW(TAG, "Timeout waiting for IP address");
                    break;
                }
            }

            AN_LOGI(TAG, "Connected to temporary credentials");
            AN_LOGI(TAG, "========================================");
            AN_LOGI(TAG, "WiFi Connected Successfully!");
            AN_LOGI(TAG, "SSID: %s", WiFi.SSID().c_str());
            AN_LOGI(TAG, "IP Address: %s", localIP.toString().c_str());
            AN_LOGI(TAG, "Gateway: %s", WiFi.gatewayIP().toString().c_str());
            AN_LOGI(TAG, "DNS: %s", WiFi.dnsIP().toString().c_str());
            AN_LOGI(TAG, "RSSI: %d dBm", WiFi.RSSI());

            if (_state.isEnterpriseMode())
            {
                AN_LOGI(TAG, "Enterprise Mode: YES (NetID: %s)",
                         _state.getEnterpriseNetId().c_str());
            }
            AN_LOGI(TAG, "========================================");

            // Copy temporary credentials to STA credentials (via callback)
            if (_onSetSTACredentials)
            {
                _onSetSTACredentials(_state.getSTASSID(),
                                     _state.getSTAPassword(),
                                     _state.isEnterpriseMode(),
                                     _state.getEnterpriseNetId(),
                                     true); // configured = true
            }

            // Save credentials based on save mode configuration
            if (_onGetConfig)
            {
                const AutoNetworkConfig &config = _onGetConfig();

                // Check save mode: AUTO saves on success, NEVER never saves
                if (config.credentialSaveMode != AutoNetworkCredentialSaveMode::NEVER)
                {
                    AutoNetworkCredentialEntry entry;
                    entry.ssid = _state.getSTASSID();
                    entry.password = _state.getSTAPassword();
                    entry.enterprise = _state.isEnterpriseMode();
                    entry.enterpriseNetId = _state.getEnterpriseNetId();
                    entry.priority = 0; // Highest priority for newly added networks

                    if (_onGetMonotonicTimestamp)
                    {
                        entry.lastUsed = _onGetMonotonicTimestamp(); // Set current timestamp
                    }

                    // Capture BSSID from connected AP for specific AP binding
                    const uint8_t *bssid = WiFi.BSSID();
                    if (bssid != nullptr)
                    {
                        memcpy(entry.bssid, bssid, AUTONETWORK_BSSID_LENGTH);
                        AN_LOGD(TAG, "Captured BSSID: %02X:%02X:%02X:%02X:%02X:%02X",
                                 bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
                    }
                    else
                    {
                        AN_LOGW(TAG, "WiFi.BSSID() returned nullptr - BSSID not captured");
                    }

                    _saveCredentialEntry(entry);
                }
                else
                {
                    AN_LOGI(TAG, "Credential save mode is NEVER - skipping save");
                }
            }

            // Set success state
            _state.setManualConnection(false);
            _state.setState(AutoNetworkPortalState::SUCCESS);
        }
        else
        {
            // Debug WiFi status periodically
            static unsigned long timeLastStatusPrint = 0;
            if (millis() - timeLastStatusPrint > 2000)
            {
                AN_LOGD(TAG, "WiFi Status: %d (WL_CONNECTED=%d)",
                         wifiStatus, WL_CONNECTED);
                timeLastStatusPrint = millis();
            }

            // Check for connection timeout
            if ((unsigned long)(millis() - _state.getTimeConnect()) > AUTONETWORK_CONNECT_TIMEOUT)
            {
                AN_LOGE(TAG, "Connection timeout!");
                AN_LOGD(TAG, "Final WiFi status: %d", wifiStatus);

                // Check if we should save credentials despite connection failure
                // (ALWAYS mode saves regardless of connection success)
                if (_onGetConfig)
                {
                    const AutoNetworkConfig &config = _onGetConfig();

                    if (config.credentialSaveMode == AutoNetworkCredentialSaveMode::ALWAYS)
                    {
                        AN_LOGI(TAG, "Credential save mode is ALWAYS - saving despite connection failure");

                        AutoNetworkCredentialEntry entry;
                        entry.ssid = _state.getSTASSID();
                        entry.password = _state.getSTAPassword();
                        entry.enterprise = _state.isEnterpriseMode();
                        entry.enterpriseNetId = _state.getEnterpriseNetId();
                        entry.priority = 0; // Highest priority for newly added networks

                        if (_onGetMonotonicTimestamp)
                        {
                            entry.lastUsed = _onGetMonotonicTimestamp(); // Set current timestamp
                        }

                        // Note: BSSID not captured here because connection failed
                        // BSSID will remain zeros (initialized in AutoNetworkCredentialEntry constructor)

                        _saveCredentialEntry(entry);
                    }
                }

                _state.clearSTACredentials(); // Clears all STA fields

                // Disconnect from WiFi
                if (_onDisconnect)
                {
                    _onDisconnect();
                }
                _state.setManualConnection(false);
                _state.setState(AutoNetworkPortalState::TIMEOUT);
            }
        }
        break;
    }

    case AutoNetworkPortalState::SUCCESS:
    {
        // Connection successful - handle portal closure per configuration
        bool portalRetain = false;
        if (_onGetConfig)
        {
            portalRetain = _onGetConfig().portalRetain;
        }

        if (!portalRetain)
        {
            // Non-blocking delay before portal closure
            // Give client time to receive success status and display success page
            if (!_state.isDelayingSuccess())
            {
                _state.startSuccessDelay();
                AN_LOGI(TAG, "Connection successful, will close portal in 5 seconds");
                AN_LOGI(TAG, "  Waiting for client to receive success page...");
            }

            // Non-blocking delay: wait 5 seconds before closing portal
            // This allows the client's JavaScript to:
            // 1. Poll /_an/status and detect SUCCESS state
            // 2. Display success page with new IP address
            // 3. User sees confirmation before portal closes
            if (millis() - _state.getSuccessTime() > 10000) // Increased delay for browser stability
            {
                AN_LOGI(TAG, "  Portal closing now - device will operate in STA mode");
                stop(); // Stop portal
                _state.setState(AutoNetworkPortalState::IDLE);
                _state.setBlocking(false);
                _state.clearSuccessDelay(); // Reset for next connection
            }
        }
        else
        {
            AN_LOGI(TAG, "Connection successful, portal retained (retainPortal=true)");
            AN_LOGI(TAG, "  Portal remains open in AP+STA mode for reconfiguration");
            // Keep portal running but mark as non-blocking
            _state.setBlocking(false);
            // Transition to IDLE but keep portal active
            _state.setState(AutoNetworkPortalState::IDLE);
        }
        break;
    }

    case AutoNetworkPortalState::FAILED:
    {
        // Connection failed - keep portal open for retry
        AN_LOGW(TAG, "Connection failed, portal remains open for retry");
        _state.setBlocking(false);
        break;
    }

    case AutoNetworkPortalState::TIMEOUT:
    {
        // Connection timeout - handle per configuration
        bool portalRetain = false;
        if (_onGetConfig)
        {
            portalRetain = _onGetConfig().portalRetain;
        }

        if (!portalRetain)
        {
            AN_LOGW(TAG, "Connection timeout, closing portal");
            stop(); // Stop portal
            _state.setState(AutoNetworkPortalState::IDLE);
            _state.setBlocking(false);
        }
        else
        {
            AN_LOGW(TAG, "Connection timeout, portal retained (retainPortal=true)");
            _state.setBlocking(false);
        }
        break;
    }
    }
}

// Configuration Methods
// ****************************************************************************

void AutoNetworkPortal::setAPCredentials(const char *ssid, const char *password)
{
    _state.setAPSSID(ssid);
    _state.setAPPassword(password);
    AN_LOGI(TAG, "AP credentials set: SSID=%s", ssid);
}

void AutoNetworkPortal::setAuthentication(const char *username, const char *password)
{
    if (strlen(username) > 0 && strlen(password) > 0)
    {
        _state.setAuthEnabled(true);
        _state.setAuthUsername(username);
        _state.setAuthPassword(password);
        AN_LOGI(TAG, "HTTP authentication enabled for user: %s", username);
    }
    else
    {
        _state.setAuthEnabled(false);
        _state.setAuthUsername("");
        _state.setAuthPassword("");
        AN_LOGI(TAG, "HTTP authentication disabled");
    }
}

void AutoNetworkPortal::setTimeout(unsigned long timeout)
{
    _state.setTimeout(timeout);
    AN_LOGI(TAG, "Portal timeout set to: %lu ms", timeout);
}

void AutoNetworkPortal::setRetainPortal(bool retain)
{
    // Update portal retain setting via callback
    if (_onSetPortalRetain)
    {
        _onSetPortalRetain(retain);
    }
    AN_LOGI(TAG, "Retain portal set to: %s", retain ? "true" : "false");
}

// State Management Methods
// ****************************************************************************

void AutoNetworkPortal::setState(AutoNetworkPortalState state)
{
    AutoNetworkPortalState oldState = _state.getState();
    _state.setState(state);

    AN_LOGI(TAG, "Portal state changed: %d -> %d", (int)oldState, (int)state);

    // Trigger state callback if registered and state actually changed
    if (_state.getStateCallback() != nullptr && oldState != state)
    {
        _state.getStateCallback()(_state.getState());
    }
}

void AutoNetworkPortal::scheduleExit()
{
    if (!_state.shouldExit())
    {
        _state.scheduleExit();
        AN_LOGI(TAG, "Portal exit scheduled");
    }
}

// Custom Parameter Methods
// ****************************************************************************

void AutoNetworkPortal::addParameter(AutoNetworkParameter *parameter)
{
    if (parameter == nullptr)
    {
        AN_LOGW(TAG, "Cannot add null parameter");
        return;
    }

    // Assign parameter ID if not already set
    if (parameter->_id == 0)
    {
        parameter->_id = nextParameterId();
    }

    // Add to parameter list
    _state.getParameters().PushBack(parameter);

    AN_LOGD(TAG, "Parameter added: %s", parameter->_name);
}

void AutoNetworkPortal::removeParameter(AutoNetworkParameter *parameter)
{
    if (parameter == nullptr)
    {
        AN_LOGW(TAG, "Cannot remove null parameter");
        return;
    }

    // Find and remove parameter
    for (int i = 0; i < _state.getParameters().Size(); i++)
    {
        if (_state.getParameters()[i] == parameter)
        {
            _state.getParameters().Erase(i);
            AN_LOGD(TAG, "Parameter removed: %s", parameter->_name);
            return;
        }
    }

    AN_LOGW(TAG, "Parameter not found in list");
}

// Private Methods - DNS Server Management
// ****************************************************************************

void AutoNetworkPortal::_startDNS()
{
    // Create DNS server if not exists
    if (_dns == nullptr)
    {
        _dns = new DNSServer();
        _dns->setErrorReplyCode(DNSReplyCode::NoError);
        AN_LOGD(TAG, "Initialized DNS Server");
    }

    // Start DNS server immediately
    if (!_dnsRunning)
    {
        AN_LOGD(TAG, "AP IP: %s", WiFi.softAPIP().toString().c_str());
        _dns->start(53, "*", WiFi.softAPIP());
        _dnsRunning = true;
        AN_LOGI(TAG, "Started DNS Server for captive portal on port 53");
    }
}

void AutoNetworkPortal::_stopDNS()
{
    if (_dns != nullptr)
    {
        _dns->stop();
        _dnsRunning = false;
        delete _dns;
        _dns = nullptr;
        AN_LOGI(TAG, "Stopped DNS Server");
    }
}

// Private Methods - HTTP Server Management
// ****************************************************************************

void AutoNetworkPortal::_startHTTP()
{
    AN_LOGI(TAG, "Starting HTTP server...");
    AN_LOGI(TAG, "Using embedded web content (no filesystem needed)");

    // Register all HTTP endpoints
    _registerEndpoints();

    // Start server
    AN_LOGI(TAG, "Calling _server->begin()...");
    _server->begin();
    _serverRunning = true;
    AN_LOGI(TAG, "HTTP server started successfully on port 80");
}

void AutoNetworkPortal::_stopHTTP()
{
    if (!_serverRunning)
    {
        AN_LOGD(TAG, "HTTP server not running");
        return;
    }

    AN_LOGI(TAG, "Stopping HTTP server endpoints...");

    // Unregister all handlers
    _unregisterEndpoints();

    // Clear not found handler
    _server->onNotFound(nullptr);

    _serverRunning = false;
    AN_LOGI(TAG, "HTTP server endpoints stopped");
}

// Endpoint Registration Helper Methods (Task 2.2)
// ****************************************************************************

// Root handler removed - now handled by AutoNetwork::_registerRootHandler()
// which serves configured content or error page based on setRootContent()

void AutoNetworkPortal::_registerStaticResourceEndpoints()
{
    // Serve css from webpage_css.h
    _server->on("/global.css", HTTP_GET, [&](AsyncWebServerRequest *request)
                {
        AN_LOGD(TAG, "Serving global.css");
        request->send(200, "text/css", WEBPAGE_CSS);
    });
}

void AutoNetworkPortal::_registerPortalPageEndpoint()
{
    // Configuration Page (/_an/config)
    _server->on("/_an/config", HTTP_GET, [&](AsyncWebServerRequest *request)
                {
        // The webpageAccessed flag is now set via the /user_active endpoint,
        // which is triggered by JavaScript in the initial HTML page.
        // This ensures webpageAccessed is only set on genuine user interaction.
        AN_LOGD(TAG, "Serving embedded captive portal HTML");
                    String html = String(AUTONETWORK_WIFI_HTML);
                    html.replace("%AUTONETWORK_MENU%", "");
                    request->send(200, "text/html", html);
                });
}

void AutoNetworkPortal::_registerInfoEndpoint()
{
    // Statistics Page (/_an/stats)
    _server->on("/_an/stats", HTTP_GET, [&](AsyncWebServerRequest *request)
                {
        AN_LOGD(TAG, "Statistics page requested");
        String html = AUTONETWORK_STATS_HTML;

        // Network Information
        if (WiFi.status() == WL_CONNECTED)
        {
            html.replace("%ESTAB_SSID%", WiFi.SSID());
            html.replace("%ESTAB_CLASS%", "good");
        }
        else
        {
            html.replace("%ESTAB_SSID%", "N/A");
            html.replace("%ESTAB_CLASS%", "bad");
        }

        // WiFi Mode
        String wifiMode;
        switch (WiFi.getMode())
        {
            case WIFI_MODE_STA: wifiMode = "STA"; break;
            case WIFI_MODE_AP: wifiMode = "AP"; break;
            case WIFI_MODE_APSTA: wifiMode = "AP+STA"; break;
            default: wifiMode = "OFF"; break;
        }
        html.replace("%WIFI_MODE%", wifiMode);

        // WiFi Status
        String wifiStatus;
        switch (WiFi.status())
        {
            case WL_CONNECTED: wifiStatus = "Connected"; break;
            case WL_DISCONNECTED: wifiStatus = "Disconnected"; break;
            case WL_CONNECT_FAILED: wifiStatus = "Failed"; break;
            case WL_CONNECTION_LOST: wifiStatus = "Lost"; break;
            case WL_NO_SSID_AVAIL: wifiStatus = "No SSID"; break;
            default: wifiStatus = "Idle"; break;
        }
        html.replace("%WIFI_STATUS%", wifiStatus);

        // IP addresses
        html.replace("%LOCAL_IP%", WiFi.localIP().toString());
        html.replace("%GATEWAY%", WiFi.gatewayIP().toString());
        html.replace("%NETMASK%", WiFi.subnetMask().toString());
        html.replace("%SOFTAP_IP%", WiFi.softAPIP().toString());

        // MAC addresses
        html.replace("%AP_MAC%", WiFi.softAPmacAddress());
        html.replace("%STA_MAC%", WiFi.macAddress());

        // Channel
        html.replace("%CHANNEL%", String(WiFi.channel()));

        // Signal strength
        int32_t rssi = WiFi.RSSI();
        if (rssi == 0 || WiFi.status() != WL_CONNECTED)
        {
            html.replace("%DBM%", "N/A");
            html.replace("%DBM_CLASS%", "");
        }
        else
        {
            html.replace("%DBM%", String(rssi));
            if (rssi > -50) html.replace("%DBM_CLASS%", "good");
            else if (rssi > -70) html.replace("%DBM_CLASS%", "warning");
            else html.replace("%DBM_CLASS%", "bad");
        }

        // Hardware Information
        uint64_t chipId = ESP.getEfuseMac();
        html.replace("%CHIP_ID%", String((uint32_t)(chipId >> 32), HEX) + String((uint32_t)chipId, HEX));
        html.replace("%CPU_FREQ%", String(ESP.getCpuFreqMHz()));
        html.replace("%FLASH_SIZE%", String(ESP.getFlashChipSize() / 1024 / 1024) + " MB");

        uint32_t freeHeap = ESP.getFreeHeap();
        html.replace("%FREE_HEAP%", String(freeHeap / 1024) + " KB");
        if (freeHeap > HEAP_GOOD_THRESHOLD) html.replace("%HEAP_CLASS%", "good");
        else if (freeHeap > HEAP_WARNING_THRESHOLD) html.replace("%HEAP_CLASS%", "warning");
        else html.replace("%HEAP_CLASS%", "bad");

        // System uptime
        uint32_t uptimeSeconds = millis() / 1000;
        uint32_t days = uptimeSeconds / 86400;
        uint32_t hours = (uptimeSeconds % 86400) / 3600;
        uint32_t minutes = (uptimeSeconds % 3600) / 60;
        uint32_t seconds = uptimeSeconds % 60;

        String uptime = "";
        if (days > 0) uptime += String(days) + "d ";
        if (hours > 0 || days > 0) uptime += String(hours) + "h ";
        if (minutes > 0 || hours > 0 || days > 0) uptime += String(minutes) + "m ";
        uptime += String(seconds) + "s";

        html.replace("%SYSTEM_UPTIME%", uptime);
        html.replace("%AUTONETWORK_MENU%", "/_an");

        request->send(200, "text/html", html); });

    // Statistics Page alias
    _server->on("/stats", HTTP_GET, [&](AsyncWebServerRequest *request)
                { request->redirect("/_an/stats"); });

    // Status API endpoint (/_an/status)
    _status_handler = &_server->on("/_an/status", HTTP_GET,
                                   [&](AsyncWebServerRequest *request)
                                   {
                                       if (_state.isAuthEnabled() &&
                                           !request->authenticate(_state.getAuthUsername().c_str(),
                                                                  _state.getAuthPassword().c_str()))
                                       {
                                           return request->requestAuthentication();
                                       }
                                       String output;
                                       _generateStatusJson(output);
                                       AsyncWebServerResponse *response = request->beginResponse(200,
                                                                                                 "application/json", output);
                                       request->send(response);
                                   });

    // Schema API endpoint (/_an/schema)
    _schema_handler = &_server->on("/_an/schema", HTTP_GET,
                                   [&](AsyncWebServerRequest *request)
                                   {
                                       if (_state.isAuthEnabled() &&
                                           !request->authenticate(_state.getAuthUsername().c_str(),
                                                                  _state.getAuthPassword().c_str()))
                                       {
                                           return request->requestAuthentication();
                                       }
                                       String output;
                                       _generateSchemaJson(output);
                                       AsyncWebServerResponse *response = request->beginResponse(200,
                                                                                                 "application/json", output);
                                       request->send(response);
                                   });
}

void AutoNetworkPortal::_registerOTAEndpoints()
{
    AN_LOGI(TAG, "Registering OTA endpoints");

    // OTA Page (/_an/ota)
    _server->on("/_an/ota", HTTP_GET, [&](AsyncWebServerRequest *request)
                {
        AN_LOGD(TAG, "OTA page requested");
        String html = String(AUTONETWORK_OTA_HTML);
        html.replace("%AUTONETWORK_MENU%", "");
        request->send(200, "text/html", html); });

    // OTA Start endpoint
    _server->on("/ota/start", HTTP_GET, [&](AsyncWebServerRequest *request)
                {
        AN_LOGI(TAG, "OTA start requested");

        if (_state.isOTAInProgress())
        {
            AN_LOGW(TAG, "OTA already in progress");
            request->send(400, "text/plain", "OTA already in progress");
            return;
        }

        String mode = request->hasParam("mode") ? request->getParam("mode")->value() : "fr";
        String hash = request->hasParam("hash") ? request->getParam("hash")->value() : "";

        AN_LOGI(TAG, "OTA mode: %s, hash: %s", mode.c_str(), hash.c_str());

        if (mode == "fr")
        {
            uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
            if (!Update.begin(maxSketchSpace, U_FLASH))
            {
                AN_LOGE(TAG, "OTA begin failed: %s", Update.errorString());
                request->send(500, "text/plain", Update.errorString());
                return;
            }
            AN_LOGI(TAG, "OTA firmware update started, max space: %u", maxSketchSpace);
        }
        else
        {
            size_t fsSize = LittleFS.totalBytes();
            if (!Update.begin(fsSize, U_SPIFFS))
            {
                AN_LOGE(TAG, "OTA filesystem begin failed: %s", Update.errorString());
                request->send(500, "text/plain", Update.errorString());
                return;
            }
            AN_LOGI(TAG, "OTA filesystem update started, size: %u", fsSize);
        }

        _state.setOTAInProgress(true);
        _state.setOTAMode(mode);
        _state.setOTAMD5Hash(hash);
        _state.setOTATotalSize(0);
        _state.setOTAUploadedSize(0);

        request->send(200, "text/plain", "OTA started"); });

    // OTA Status endpoint
    _server->on("/ota/status", HTTP_GET, [&](AsyncWebServerRequest *request)
                {
        String json = "{";
        json += "\"inProgress\":" + String(_state.isOTAInProgress() ? "true" : "false") + ",";
        json += "\"totalSize\":" + String(_state.getOTATotalSize()) + ",";
        json += "\"uploadedSize\":" + String(_state.getOTAUploadedSize()) + ",";

        int progress = 0;
        if (_state.getOTATotalSize() > 0)
        {
            progress = (_state.getOTAUploadedSize() * 100) / _state.getOTATotalSize();
        }
        json += "\"progress\":" + String(progress);
        json += "}";

        request->send(200, "application/json", json); });

    // OTA Upload endpoint
    _server->on("/ota/upload", HTTP_POST, [&](AsyncWebServerRequest *request)
                {
            AN_LOGI(TAG, "OTA upload completed - finalizing update");

            if (!_state.isOTAInProgress())
            {
                request->send(400, "text/plain", "OTA not started");
                return;
            }

            // Call Update.end(true) to finalize and validate the upload
            bool success = Update.end(true);

            if (success)
            {
                AN_LOGI(TAG, "OTA update successful, size: %u bytes", Update.size());
                _state.setOTAInProgress(false);
                request->send(200, "text/plain", "Update successful");
                // Note: Restart will be triggered by separate /ota/reboot request
            }
            else
            {
                AN_LOGE(TAG, "OTA update failed: %s", Update.errorString());
                _state.setOTAInProgress(false);
                request->send(500, "text/plain", Update.errorString());
            } }, [&](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
                {
            if (!_state.isOTAInProgress())
            {
                AN_LOGW(TAG, "OTA upload received but not started");
                return;
            }

            if (index == 0)
            {
                AN_LOGI(TAG, "OTA upload started: %s, size: %u", filename.c_str(), request->contentLength());
                _state.setOTATotalSize(request->contentLength());
            }

            if (Update.write(data, len) != len)
            {
                AN_LOGE(TAG, "OTA write failed at index %u", index);
                _state.setOTAInProgress(false);
                return;
            }

            _state.setOTAUploadedSize(_state.getOTAUploadedSize() + len);

            if (_state.getOTATotalSize() > 0)
            {
                int progress = (_state.getOTAUploadedSize() * 100) / _state.getOTATotalSize();
                static int lastProgress = -1;
                if (progress >= lastProgress + 10)
                {
                    AN_LOGI(TAG, "OTA progress: %d%%", progress);
                    lastProgress = progress;
                }
            }

            if (final)
            {
                AN_LOGI(TAG, "OTA upload finished: %u bytes written", index + len);
            } });

    // OTA Reboot endpoint - called by client after successful upload
    _server->on("/ota/reboot", HTTP_POST, [&](AsyncWebServerRequest *request)
                {
        AN_LOGI(TAG, "Reboot request received, restarting ESP32 now");
        request->send(200, "text/plain", "Rebooting");

        // Restart immediately - response will be sent before restart
        ESP.restart(); });

#ifdef AUTONETWORK_DEBUG
    _server->on("/_an/debug/scan", HTTP_GET,
                [&](AsyncWebServerRequest *request)
                {
                    if (_state.isAuthEnabled() &&
                        !request->authenticate(_state.getAuthUsername().c_str(),
                                               _state.getAuthPassword().c_str()))
                    {
                        return request->requestAuthentication();
                    }

                    AsyncJsonResponse* response = new AsyncJsonResponse();
                    JsonObject root = response->getRoot();

                    root["scanState"] = static_cast<uint8_t>(_state.getScanState());
                    root["scanActive"] = _state.isScanActive();
                    root["scanStartTime"] = _state.getScanStartTime();
                    root["scanStateChangeTime"] = _state.getScanStateChangeTime();
                    root["lastScanStatus"] = _state.getLastScanStatus();
                    root["cacheValid"] = _state.isScanCacheValid();
                    root["cacheTimestamp"] = _state.getScanCacheTimestamp();
                    root["cachedCount"] = _state.getCachedScanCount();
                    root["currentTime"] = millis();

                    response->setLength();
                    request->send(response);
                });
#endif
}

void AutoNetworkPortal::_registerScanEndpoint()
{
    // Scan API endpoint (/_an/scan)
    _scan_handler = &_server->on("/_an/scan", HTTP_GET,
                                 [&](AsyncWebServerRequest *request)
                                 {
                                     AN_LOGV(TAG, "/_an/scan endpoint called");

                                     AutoNetworkRequestValidation validation{
                                         request,
                                         &_state,
                                         true,  // requireAuth
                                         true   // checkBusyState
                                     };

                                     if (!validation.validate(
                                             _state.getAuthUsername(),
                                             _state.getAuthPassword()))
                                     {
                                         return;
                                     }

                                     int16_t n = WiFi.scanComplete();
                                     AN_LOGI(TAG, "[AutoNetworkPortal] /_an/scan endpoint - WiFi.scanComplete() = %d", n);

                                     if ((n == WIFI_SCAN_FAILED || n == -2) && !_state.isScanActive())
                                     {
                                         unsigned long currentTime = millis();

                                         if (currentTime - _lastScanRequest < DEBOUNCE_SCAN_MS)
                                         {
                                             AN_LOGD(TAG, "[AutoNetworkPortal] Scan debounced - too soon after last request");
                                             return request->send(202, "text/plain", "");
                                         }

                                         AN_LOGI(TAG, "[AutoNetworkPortal] First scan request - starting WiFi scan");
                                         _lastScanRequest = currentTime;
                                         _restartScan();
                                         return request->send(202, "text/plain", "");
                                     }

                                     if (n == WIFI_SCAN_RUNNING)
                                     {
                                         AN_LOGI(TAG, "[AutoNetworkPortal] Scan still running, returning 202");
                                         return request->send(202, "text/plain", "");
                                     }
                                     else if (n == WIFI_SCAN_FAILED)
                                     {
                                         AN_LOGI(TAG, "[AutoNetworkPortal] Scan failed, restarting");
                                         _restartScan();
                                         return request->send(202, "text/plain", "");
                                     }
                                     else
                                     {
                                         AN_LOGI(TAG, "[AutoNetworkPortal] Scan complete with %d networks", n);
                                         String output;
                                         output.reserve(1024);
                                         _generateScanJson(output);
                                         AN_LOGI(TAG, "[AutoNetworkPortal] JSON size: %d bytes, restarting scan", output.length());
                                         _restartScan();
                                         return request->send(200, "application/json", output);
                                     }
                                 });
}

void AutoNetworkPortal::_registerWifiConnectEndpoint()
{
    // Connect API endpoint (/_an/connect)
    _save_handler = new AsyncCallbackJsonWebHandler("/_an/connect",
                                                    [&](AsyncWebServerRequest *request, JsonVariant &json)
                                                    {
                                                        AN_LOGD(TAG, "/_an/connect endpoint called");

                                                        AutoNetworkRequestValidation validation{
                                                            request,
                                                            &_state,
                                                            true,  // requireAuth
                                                            true   // checkBusyState
                                                        };

                                                        if (!validation.validate(
                                                                _state.getAuthUsername(),
                                                                _state.getAuthPassword()))
                                                        {
                                                            return;
                                                        }

                                                        if (!json.is<JsonObject>() || json.size() == 0)
                                                        {
                                                            AN_LOGW(TAG, "Invalid JSON received");
                                                            return request->send(400, "text/plain", "Invalid request data");
                                                        }

                                                        AN_LOGD(TAG, "Processing save request...");
                                                        String jsonString;
                                                        serializeJson(json, jsonString);
                                                        AN_LOGD(TAG, "Received JSON: %s", jsonString.c_str());

                                                        JsonObject obj = json.as<JsonObject>();

                                                        if (obj["params"].is<JsonArray>())
                                                        {
                                                            JsonArray params = obj["params"];
                                                            if (!_parseConfigJson(params))
                                                            {
                                                                return request->send(400, "text/plain", "Invalid data");
                                                            }
                                                            else
                                                            {
                                                                if (!obj["credentials"].is<JsonObject>())
                                                                {
                                                                    _state.setState(AutoNetworkPortalState::SUCCESS);
                                                                }
                                                            }
                                                        }

                                                        if (obj["credentials"].is<JsonObject>())
                                                        {
                                                            JsonObject credentials = obj["credentials"];
                                                            if (!_parseCredentialsJson(credentials))
                                                            {
                                                                return request->send(400, "text/plain", "Invalid data");
                                                            }
                                                        }

                                                        // Redirect to root after successful credential submission
                                                        AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "Redirecting...");
                                                        response->addHeader("Location", "/");
                                                        return request->send(response);
                                                    });
    _server->addHandler(_save_handler);

    // Delete API endpoint (/_an/delete)
    _clear_handler = &_server->on("/_an/delete", HTTP_POST,
                                  [&](AsyncWebServerRequest *request)
                                  {
                                      if (_state.isAuthEnabled() &&
                                          !request->authenticate(_state.getAuthUsername().c_str(),
                                                                 _state.getAuthPassword().c_str()))
                                      {
                                          return request->requestAuthentication();
                                      }

                                      if (_state.getState() == AutoNetworkPortalState::IDLE ||
                                          _state.getState() == AutoNetworkPortalState::SUCCESS ||
                                          _state.getState() == AutoNetworkPortalState::FAILED ||
                                          _state.getState() == AutoNetworkPortalState::TIMEOUT)
                                      {
                                          _state.setState(AutoNetworkPortalState::IDLE);
                                          request->send(200, "text/plain", "OK");
                                      }
                                      else
                                      {
                                          request->send(503, "text/plain", "Busy");
                                      }
                                  });

    // Exit API endpoint (/_an/exit)
    _exit_handler = &_server->on("/_an/exit", HTTP_POST,
                                 [&](AsyncWebServerRequest *request)
                                 {
                                     if (_state.isAuthEnabled() &&
                                         !request->authenticate(_state.getAuthUsername().c_str(),
                                                                _state.getAuthPassword().c_str()))
                                     {
                                         return request->requestAuthentication();
                                     }

                                     if (_state.getState() == AutoNetworkPortalState::WAITING_FOR_CONNECTION ||
                                         _state.getState() == AutoNetworkPortalState::CONNECTING_WIFI)
                                     {
                                         return request->send(503, "text/plain", "Busy");
                                     }

                                     if (!_state.shouldExit())
                                     {
                                         _state.scheduleExit();
                                     }
                                     return request->send(200, "text/plain", "OK");
                                 });
}

void AutoNetworkPortal::_registerSavedNetworksEndpoint()
{
    // Saved Credentials Page (/_an/open)
    _server->on("/_an/open", HTTP_GET, [&](AsyncWebServerRequest *request)
                {
        // The webpageAccessed flag is now set via the /user_active endpoint,
        // which is triggered by JavaScript in the initial HTML page.
        // This ensures webpageAccessed is only set on genuine user interaction.
        AN_LOGD(TAG, "Credentials page requested");

        String html = AUTONETWORK_CREDENTIALS_HTML;
        String credList = "";

        // Access credential manager via callback
        uint8_t count = 0;
        if (_onGetCredentialEntries)
        {
            count = _onGetCredentialEntries();
        }

        if (count == 0)
        {
            credList = "<div class=\"credential-item\">No saved credentials</div>";
        }
        else
        {
            for (uint8_t i = 0; i < count; i++)
            {
                AutoNetworkCredentialEntry entry;
                if (_onGetCredentialByPriority && _onGetCredentialByPriority(i, entry))
                {
                    credList += "<div class=\"credential-item\">";
                    credList += "<input type=\"checkbox\" class=\"credential-checkbox\" value=\"" + entry.ssid + "\" onchange=\"updateActionButtons()\">";
                    credList += "<div class=\"credential-content\">";
                    credList += "<div class=\"credential-ssid\">" + entry.ssid;
                    if (entry.enterprise)
                    {
                        credList += "<span class=\"badge badge-enterprise\">Enterprise</span>";
                    }
                    credList += "</div>";
                    credList += "</div>";
                    credList += "</div>";
                }
            }
        }

        html.replace("%CREDENTIALS%", credList);
        request->send(200, "text/html", html); });

    // Delete specific credentials endpoint (/_an/delete_creds)
    _server->on("/_an/delete_creds", HTTP_POST, [&](AsyncWebServerRequest *request) {}, NULL, [&](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
                {
            AN_LOGD(TAG, "Delete credentials request received");

            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, data, len);

            if (error)
            {
                AN_LOGW(TAG, "JSON parsing failed: %s", error.c_str());
                request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
                return;
            }

            JsonArray ssids = doc["ssids"];
            if (!ssids)
            {
                request->send(400, "application/json", "{\"success\":false,\"message\":\"No SSIDs provided\"}");
                return;
            }

            // Delete credentials via callback
            int deleted = 0;
            for (JsonVariant ssid : ssids)
            {
                const char* ssidStr = ssid.as<const char*>();
                if (ssidStr && _onDeleteCredential && _onDeleteCredential(ssidStr))
                {
                    AN_LOGI(TAG, "Deleted credential: %s", ssidStr);
                    deleted++;
                }
                else
                {
                    AN_LOGW(TAG, "Failed to delete credential: %s", ssidStr ? ssidStr : "(null)");
                }
            }

            String response = "{\"success\":true,\"deleted\":" + String(deleted) + "}";
            request->send(200, "application/json", response); });

    // Connect to saved credential endpoint (/_an/connect_saved)
    _server->on("/_an/connect_saved", HTTP_POST, [&](AsyncWebServerRequest *request) {}, NULL, [&](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
                {
            AN_LOGD(TAG, "Connect to saved credential request received");

            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, data, len);

            if (error)
            {
                AN_LOGW(TAG, "JSON parsing failed: %s", error.c_str());
                request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
                return;
            }

            const char* ssid = doc["ssid"];
            if (!ssid || strlen(ssid) == 0)
            {
                request->send(400, "application/json", "{\"success\":false,\"message\":\"No SSID provided\"}");
                return;
            }

            // Access credential manager via callback
            AutoNetworkCredentialEntry entry;
            bool found = false;
            uint8_t count = 0;

            if (_onGetCredentialEntries)
            {
                count = _onGetCredentialEntries();
            }

            for (uint8_t i = 0; i < count; i++)
            {
                if (_onGetCredentialByIndex && _onGetCredentialByIndex(i, entry))
                {
                    if (entry.ssid == String(ssid))
                    {
                        found = true;
                        break;
                    }
                }
            }

            if (!found)
            {
                AN_LOGW(TAG, "Credential not found: %s", ssid);
                request->send(404, "application/json", "{\"success\":false,\"message\":\"Credential not found\"}");
                return;
            }

            AN_LOGI(TAG, "Initiating connection to saved credential: %s", ssid);

            // Disconnect and connect (via callback)
            if (_onDisconnect)
            {
                _onDisconnect();
            }

            if (entry.enterprise)
            {
                if (_onConnectEnterprise)
                {
                    _onConnectEnterprise(entry.ssid.c_str(), entry.enterpriseNetId.c_str(), entry.password.c_str());
                }
            }
            else
            {
                if (_onConnect)
                {
                    _onConnect(entry.ssid.c_str(), entry.password.c_str(), false, &entry);
                }
            }

            String response = "{\"success\":true,\"message\":\"Connection initiated to " + entry.ssid + "\"}";
            request->send(200, "application/json", response); });

    // Disconnect endpoint (/_an/disc)
    _server->on("/_an/disc", HTTP_GET, [&](AsyncWebServerRequest *request)
                {
        // The webpageAccessed flag is now set via the /user_active endpoint,
        // which is triggered by JavaScript in the initial HTML page.
        // This ensures webpageAccessed is only set on genuine user interaction.
        AN_LOGI(TAG, "Manual disconnect requested. Starting portal.");

        // 1. Disconnect from WiFi
        if (_onDisconnect) {
            _onDisconnect();
        }

        // 2. Immediately start the portal (which starts the AP)
        start();

        // 3. Redirect to the portal's IP address
        AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "Redirecting...");
        response->addHeader("Location", "http://" + WiFi.softAPIP().toString() + "/_an");
        request->send(response);
    });
}

void AutoNetworkPortal::_registerResetEndpoint()
{
    // Reset endpoint (/_an/reset)
    _server->on("/_an/reset", HTTP_GET, [&](AsyncWebServerRequest *request)
                {
        AN_LOGI(TAG, "Reset requested");

        AutoNetworkRequestValidation validation{
            request,
            &_state,
            true,   // requireAuth
            false   // checkBusyState - reset allowed when busy
        };

        if (!validation.validate(_state.getAuthUsername(), _state.getAuthPassword()))
        {
            return;
        }

        AN_LOGI(TAG, "Reset authorized - redirecting and restarting");

        // Send a server-side redirect to the root page
        AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "Redirecting...");
        response->addHeader("Location", "/");
        request->send(response);

        // Restart the ESP32 after a short delay to allow the response to be sent
        static bool resetScheduled = false;
        if (!resetScheduled) {
            resetScheduled = true;
            xTaskCreate([](void* param) {
                vTaskDelay(pdMS_TO_TICKS(100)); // Short delay
                ESP.restart();
            }, "reset_task", 2048, NULL, 1, NULL);
        }
    });
}

void AutoNetworkPortal::_registerUserActiveEndpoint()
{
    // Dedicated endpoint for JavaScript signal of genuine user interaction
    _server->on("/user_active", HTTP_GET, [&](AsyncWebServerRequest *request)
                {
        // Mark the client's IP as active to indicate genuine user interaction.
        // This prevents automated captive portal probes from triggering webpageAccessed.
        _activeUsers.insert(request->client()->remoteIP()); // Access remoteIP via pointer
        AN_LOGD(TAG, "Client %s marked as active via /user_active endpoint", request->client()->remoteIP().toString().c_str()); // Access remoteIP via pointer

        // Signal AutoNetwork that a user-facing page has been accessed.
        // This triggers the OLED display transition to State 4.
        if (_onSetWebpageAccessed) {
            _onSetWebpageAccessed();
        }
        request->send(200, "text/plain", "OK"); });
}

void AutoNetworkPortal::_registerCaptiveEndpoints()
{
    // Android/Google connectivity probes
    _server->on("/generate_204", HTTP_GET, [&](AsyncWebServerRequest *request)
                {
        AN_LOGV(TAG, "Android/Google connectivity probe detected");
        request->redirect("/_an"); })
        .setFilter(this->_onAPFilter);

    _server->on("/gen_204", HTTP_GET, [&](AsyncWebServerRequest *request)
                {
        AN_LOGV(TAG, "Android/Google connectivity probe detected");
        request->redirect("/_an"); })
        .setFilter(this->_onAPFilter);

    // Apple (iOS/macOS) connectivity probes
    _server->on("/hotspot-detect.html", HTTP_GET, [&](AsyncWebServerRequest *request)
                {
        AN_LOGV(TAG, "Apple iOS/macOS connectivity probe detected");
        request->redirect("/_an"); })
        .setFilter(this->_onAPFilter);

    _server->on("/library/test/success.html", HTTP_GET, [&](AsyncWebServerRequest *request)
                {
        AN_LOGV(TAG, "Apple connectivity probe detected");
        request->redirect("/_an"); })
        .setFilter(this->_onAPFilter);

    // Microsoft Windows connectivity probes
    _server->on("/ncsi.txt", HTTP_GET, [&](AsyncWebServerRequest *request)
                {
        AN_LOGV(TAG, "Windows NCSI connectivity probe detected");
        request->redirect("/_an"); })
        .setFilter(this->_onAPFilter);

    _server->on("/connecttest.txt", HTTP_GET, [&](AsyncWebServerRequest *request)
                {
        AN_LOGV(TAG, "Windows connectivity probe detected");
        request->redirect("/_an"); })
        .setFilter(this->_onAPFilter);

    _server->on("/fwlink", HTTP_GET, [&](AsyncWebServerRequest *request)
                {
        // Respond with 204 No Content to satisfy Windows captive portal probes without triggering webpageAccessed.
        AN_LOGV(TAG, "Windows fwlink check - responding with 204 No Content");
        request->send(204); })
        .setFilter(this->_onAPFilter);

}

void AutoNetworkPortal::_registerMenuEndpoint()
{
    // AutoNetwork Menu Page (/_an)
    // IMPORTANT: This MUST be registered LAST after all /_an/* endpoints
    // to avoid catching requests meant for more specific routes
    _server->on("/_an", HTTP_GET, [&](AsyncWebServerRequest *request)
                {
        AN_LOGD(TAG, "Menu page requested");

        String html = AUTONETWORK_MENU_HTML;

        if (WiFi.status() == WL_CONNECTED)
        {
            html.replace("%STATUS%", "Connected");
            html.replace("%STATUS_CLASS%", "status-connected");
            html.replace("%SSID%", WiFi.SSID());
            html.replace("%IP%", WiFi.localIP().toString());
        }
        else
        {
            html.replace("%STATUS%", "Disconnected");
            html.replace("%STATUS_CLASS%", "status-disconnected");
            html.replace("%SSID%", "None");
            html.replace("%IP%", WiFi.softAPIP().toString());
        }

        // Get credential count via callback
        uint8_t credCount = 0;
        if (_onGetCredentialEntries)
        {
            credCount = _onGetCredentialEntries();
        }
        html.replace("%COUNT%", String(credCount));

        request->send(200, "text/html", html);
    });
}

void AutoNetworkPortal::_registerEndpoints()
{
    AN_LOGI(TAG, "Registering HTTP endpoints...");

    // Root endpoint is now handled by AutoNetwork::_registerRootHandler()
    _registerStaticResourceEndpoints();
    _registerPortalPageEndpoint();
    _registerInfoEndpoint();
    _registerOTAEndpoints();
    _registerScanEndpoint();
    _registerWifiConnectEndpoint();
    _registerSavedNetworksEndpoint();
    _registerResetEndpoint();
    _registerUserActiveEndpoint();
    _registerCaptiveEndpoints();
    // CRITICAL: Menu endpoint MUST be registered LAST to avoid catching /_an/* routes
    _registerMenuEndpoint();

    AN_LOGI(TAG, "HTTP endpoints registered successfully");
}

void AutoNetworkPortal::_unregisterEndpoints()
{
    AN_LOGI(TAG, "Unregistering HTTP endpoints...");

    if (_index_handler != nullptr)
    {
        _server->removeHandler(_index_handler);
        _index_handler = nullptr;
    }

    if (_status_handler != nullptr)
    {
        _server->removeHandler(_status_handler);
        _status_handler = nullptr;
    }

    if (_schema_handler != nullptr)
    {
        _server->removeHandler(_schema_handler);
        _schema_handler = nullptr;
    }

    if (_scan_handler != nullptr)
    {
        _server->removeHandler(_scan_handler);
        _scan_handler = nullptr;
    }

    if (_save_handler != nullptr)
    {
        _server->removeHandler(_save_handler);
        _save_handler = nullptr;
    }

    if (_clear_handler != nullptr)
    {
        _server->removeHandler(_clear_handler);
        _clear_handler = nullptr;
    }

    if (_exit_handler != nullptr)
    {
        _server->removeHandler(_exit_handler);
        _exit_handler = nullptr;
    }

    AN_LOGI(TAG, "HTTP endpoints unregistered");
}

// Private Methods - JSON Generation
// ****************************************************************************

void AutoNetworkPortal::_generateStatusJson(String &str)
{
    uint8_t status = 0;
    String staSSID = "";

    if (_onGetStatus)
    {
        status = _onGetStatus();
    }

    if (_onGetSTASSID)
    {
        staSSID = _onGetSTASSID();
    }

    AutoNetworkJsonBuilder::buildStatusJson(
        str,
        status,
        (WiFi.status() == WL_CONNECTED),
        staSSID,
        WiFi.macAddress(),
        WiFi.localIP().toString(),
        (uint8_t)_state.getState(),
        _state.isActive());
}

void AutoNetworkPortal::_generateSchemaJson(String &str)
{
    extern struct AutoNetworkParameterTypeNames paramTypes[];

    AutoNetworkJsonBuilder::buildSchemaJson(
        str,
        _state.getParameters().Data(),
        _state.getParameters().Size(),
        paramTypes);
}

void AutoNetworkPortal::_generateScanJson(String &str)
{
    JsonDocument json;

    JsonArray arr = json.to<JsonArray>();

    for (uint16_t i = 0; i < WiFi.scanComplete(); i++)
    {
        JsonObject obj = arr.add<JsonObject>();
        obj["s"] = WiFi.SSID(i);
        obj["b"] = WiFi.BSSIDstr(i);
        obj["r"] = WiFi.RSSI(i);
        obj["c"] = WiFi.channel(i);

        AutoNetworkEncryptionType enc = AutoNetworkEncryptionType::OPEN;
        switch (WiFi.encryptionType(i))
        {
        case WIFI_AUTH_OPEN:
            enc = AutoNetworkEncryptionType::OPEN;
            break;
        case WIFI_AUTH_WEP:
            enc = AutoNetworkEncryptionType::WEP;
            break;
        case WIFI_AUTH_WPA_PSK:
            enc = AutoNetworkEncryptionType::WPA_PSK;
            break;
        case WIFI_AUTH_WPA2_PSK:
            enc = AutoNetworkEncryptionType::WPA2_PSK;
            break;
        case WIFI_AUTH_WPA_WPA2_PSK:
            enc = AutoNetworkEncryptionType::WPA_WPA2_PSK;
            break;
        case WIFI_AUTH_WPA2_ENTERPRISE:
            enc = AutoNetworkEncryptionType::WPA2_ENTERPRISE;
            break;
        case WIFI_AUTH_WPA3_PSK:
            enc = AutoNetworkEncryptionType::WPA3_PSK;
            break;
        case WIFI_AUTH_WPA2_WPA3_PSK:
            enc = AutoNetworkEncryptionType::WPA2_WPA3_PSK;
            break;
        case WIFI_AUTH_WAPI_PSK:
            enc = AutoNetworkEncryptionType::WAPI_PSK;
            break;
        case WIFI_AUTH_WPA3_ENT_192:
            enc = AutoNetworkEncryptionType::WPA3_ENT_192;
            break;
        case WIFI_AUTH_MAX:
            enc = AutoNetworkEncryptionType::MAX;
            break;
        default:
            enc = AutoNetworkEncryptionType::UNKNOWN;
            break;
        }
        obj["e"] = (uint8_t)enc;
    }
    serializeJson(json, str);
    json.clear();
}

// Private Methods - JSON Parsing
// ****************************************************************************

bool AutoNetworkPortal::_parseConfigJson(JsonArray &arr)
{
    // Validate parameters
    for (uint8_t i = 0; i < arr.size(); i++)
    {
        JsonObject obj = arr[i];

        if (!obj["id"].is<JsonVariant>() || !obj["v"].is<JsonVariant>())
        {
            return false;
        }

        if (!obj["v"].is<const char *>())
        {
            return false;
        }
    }

    // Parse parameters
    for (uint8_t i = 0; i < arr.size(); i++)
    {
        JsonObject obj = arr[i];
        for (int j = 0; j < _state.getParameters().Size(); j++)
        {
            AutoNetworkParameter *p = _state.getParameters()[j];
            if (p->_id == obj["id"].as<uint32_t>())
            {
                p->_value = obj["v"].as<const char *>();
                break;
            }
        }
    }

    if (_state.getConfigCallback() != nullptr)
    {
        return _state.getConfigCallback()();
    }
    else
    {
        return true;
    }
}

bool AutoNetworkPortal::_parseCredentialsJson(JsonObject &obj)
{
        if (obj["ssid"].is<const char *>() && obj["password"].is<const char *>()) {
            _state.setSTASSID(obj["ssid"].as<const char *>());
            _state.setSTAPassword(obj["password"].as<const char *>());
    
                    _state.setSTAChannel(0);
            
                    if (obj["bssid"].is<const char*>()) {
                        const char* bssidStr = obj["bssid"].as<const char*>();
                        uint8_t bssid[6];
                        if (sscanf(bssidStr, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &bssid[0], &bssid[1], &bssid[2], &bssid[3], &bssid[4], &bssid[5]) == 6) {
                            _state.setSTABSSID(bssid);
                        } else {
                            _state.setSTABSSID(nullptr);
                        }
                    } else {
                        _state.setSTABSSID(nullptr);
                    }
            
                    // Set manual connection flag
            _state.setManualConnection(true);

        // Parse enterprise fields if present
        if (obj["enterprise"].is<bool>() && obj["enterprise"].as<bool>())
        {
            _state.setEnterpriseMode(true);
            if (obj["netid"].is<const char *>())
            {
                _state.setEnterpriseNetId(obj["netid"].as<const char *>());
                AN_LOGI(TAG, "Enterprise credentials parsed");
                AN_LOGD(TAG, "Enterprise - NetID: %s",
                         _state.getEnterpriseNetId().c_str());
            }
        }
        else
        {
            _state.setEnterpriseMode(false);
            _state.setEnterpriseNetId("");
        }

        // Set portal state
        _state.setState(AutoNetworkPortalState::CONNECTING_WIFI);
        AN_LOGI(TAG, "Starting WiFi connection to SSID: %s (Ent: %s)",
                 _state.getSTASSID().c_str(),
                 _state.isEnterpriseMode() ? "Yes" : "No");

        return true;
    }
    else
    {
        return false;
    }
}

// Private Methods - Credential Management
// ****************************************************************************

bool AutoNetworkPortal::_saveCredentialEntry(const AutoNetworkCredentialEntry &entry)
{
    // Check credential limit before saving (via callback)
    if (_onGetCredentialEntries && _onGetConfig)
    {
        uint8_t currentCount = _onGetCredentialEntries();
        const AutoNetworkConfig &config = _onGetConfig();

        if (currentCount >= config.credentialsMax)
        {
            AN_LOGW(TAG, "Credential limit reached (%d/%d) - oldest credential will be replaced",
                     currentCount, config.credentialsMax);

            // Find and delete oldest credential (lowest lastUsed timestamp)
            AutoNetworkCredentialEntry oldestEntry;
            if (_onGetCredentialByRecent && _onGetCredentialByRecent(config.credentialsMax - 1, oldestEntry))
            {
                if (_onDeleteCredential)
                {
                    _onDeleteCredential(oldestEntry.ssid.c_str());
                    AN_LOGI(TAG, "Deleted oldest credential: %s (lastUsed=%lu)",
                             oldestEntry.ssid.c_str(), oldestEntry.lastUsed);
                }
            }
        }
    }

    if (_onSaveCredential && _onSaveCredential(entry))
    {
        AN_LOGI(TAG, "Saved new credential: %s (lastUsed=%lu)", entry.ssid.c_str(), entry.lastUsed);
        return true;
    }
    else
    {
        AN_LOGE(TAG, "Failed to save credential: %s", entry.ssid.c_str());
        return false;
    }
}

// Private Methods - WiFi Scanning
// ****************************************************************************

void AutoNetworkPortal::_restartScan()
{
    AN_LOGI(TAG, "[AutoNetworkPortal] _restartScan() called");
    if (_onRequestScan)
    {
        AN_LOGI(TAG, "[AutoNetworkPortal] Delegating scan request to parent");
        _onRequestScan();
    }
    else
    {
        AN_LOGE(TAG, "[AutoNetworkPortal] Scan request failed: _onRequestScan callback is not set!");
    }
}

// Private Methods - Request Filtering
// ****************************************************************************

bool AutoNetworkPortal::_onAPFilter(AsyncWebServerRequest *request)
{
    // Allow requests when portal is active (stations connected to AP)
    return WiFi.softAPgetStationNum() > 0;
}
