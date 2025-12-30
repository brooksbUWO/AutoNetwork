// ****************************************************************************
// Title        : AutoNetwork Library - Connection Manager
// Filename     : 'AutoNetworkConnectionManager.cpp'
// Target MCU   : Espressif ESP32 (Doit DevKit Version 1)
// Description  : WiFi connection management functionality extracted from AutoNetwork
//
// Revision History:
// When         Who         Description of change
// -----------  ----------- -----------------------
// 14-NOV-2025  Brooks      Initial extraction from AutoNetwork
//
// ****************************************************************************

#include "AutoNetworkConnectionManager.h"
#include "AutoNetworkPortal.h"
#include "AutoNetworkCredential.h"
#include "AutoNetwork.h"  // For AutoNetworkConnectionStatus enum values

// ESP-IDF WPA2 Enterprise headers (ESP32 only)
#if defined(ESP32)
    #include "esp_idf_version.h"
    #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        // ESP-IDF 5.0+: Use new EAP client APIs
        #include "esp_eap_client.h"
    #else
        // ESP-IDF 4.x and earlier: Use deprecated WPA2 APIs
        #include "esp_wpa2.h"
    #endif
#endif

static const char* TAG = "AutoNetworkConnMgr";

AutoNetworkConnectionManager::AutoNetworkConnectionManager(
    AutoNetworkPortal *portal,
    AutoNetworkCredential *credential,
    AutoNetworkConnectionStatus &status,
    bool &established)
    : _portal(portal)
    , _credential(credential)
    , _status(status)
    , _established(established)
{
    AN_LOGD(TAG, "ConnectionManager initialized");
}

void AutoNetworkConnectionManager::connect(
    const char *ssid,
    const char *password,
    bool autoReconnect,
    const AutoNetworkCredentialEntry *credential)
{
    // Layer 3: Environment guard - detect test environment
    #ifdef UNIT_TEST
    AN_LOGD(TAG, "connect() in UNIT_TEST mode");
    // In unit tests, WiFi may not be initialized
    if (WiFi.getMode() == WIFI_MODE_NULL)
    {
        AN_LOGW(TAG, "WiFi not initialized (test environment)");
        _status = AutoNetworkConnectionStatus::DISCONNECTED;
        return;
    }
    #endif

    // Set auto reconnect (ESP32)
    if (autoReconnect)
    {
        WiFi.setAutoReconnect(true);
    }

    // Apply static IP configuration if provided and not using DHCP
    if (credential != nullptr && !credential->dhcp)
    {
        // Layer 4: Log static IP configuration attempt
        AN_LOGI(TAG, "Applying static IP configuration:");
        AN_LOGI(TAG, "  IP:      %s", credential->ip.toString().c_str());
        AN_LOGI(TAG, "  Gateway: %s", credential->gateway.toString().c_str());
        AN_LOGI(TAG, "  Netmask: %s", credential->netmask.toString().c_str());
        AN_LOGI(TAG, "  DNS1:    %s", credential->dns1.toString().c_str());
        AN_LOGI(TAG, "  DNS2:    %s", credential->dns2.toString().c_str());
        
        // Layer 1: Entry validation - reject zero addresses
        uint32_t ipAddr = (uint32_t)credential->ip;
        uint32_t gwAddr = (uint32_t)credential->gateway;
        
        if (ipAddr == 0)
        {
            AN_LOGE(TAG, "connect: Static IP is 0.0.0.0 - invalid configuration");
            AN_LOGW(TAG, "Falling back to DHCP");
            WiFi.config(IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0));
        }
        else if (gwAddr == 0)
        {
            AN_LOGE(TAG, "connect: Gateway is 0.0.0.0 - invalid configuration");
            AN_LOGW(TAG, "Falling back to DHCP");
            WiFi.config(IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0));
        }
        else
        {
            // Layer 2: Business validation - subnet consistency
            uint32_t mask = (uint32_t)credential->netmask;
            uint32_t ipNetwork = ipAddr & mask;
            uint32_t gwNetwork = gwAddr & mask;
            
            if (ipNetwork != gwNetwork)
            {
                // Layer 4: Log the unusual configuration for debugging
                AN_LOGW(TAG, "Static IP configuration uses gateway in different subnet:");
                AN_LOGW(TAG, "  IP:      %s (network: %s)",
                        credential->ip.toString().c_str(),
                        IPAddress(ipNetwork).toString().c_str());
                AN_LOGW(TAG, "  Gateway: %s (network: %s)",
                        credential->gateway.toString().c_str(),
                        IPAddress(gwNetwork).toString().c_str());
                AN_LOGW(TAG, "This may indicate:");
                AN_LOGW(TAG, "  - Point-to-point link (/30 or /32 netmask)");
                AN_LOGW(TAG, "  - VPN or route-based configuration");
                AN_LOGW(TAG, "  - Configuration error (if unintentional)");
                AN_LOGW(TAG, "Proceeding with user-specified configuration...");
            }
            
            // Apply configuration WITHOUT falling back to DHCP
            // Trust the user's config, but check if WiFi.config() accepts it
            if (!WiFi.config(credential->ip, credential->gateway, credential->netmask,
                             credential->dns1, credential->dns2))
            {
                // Layer 1: Actual failure - ESP32 WiFi stack rejected it
                AN_LOGE(TAG, "WiFi.config() failed - ESP32 rejected static IP configuration");
                AN_LOGW(TAG, "Falling back to DHCP");
                WiFi.config(IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0));
            }
            else
            {
                // Layer 4: Log success
                AN_LOGI(TAG, "Static IP configuration applied successfully");
            }
        }
    }
    else if (credential != nullptr && credential->dhcp)
    {
        // Layer 4: Log DHCP mode
        AN_LOGD(TAG, "Using DHCP for IP configuration");
        // Reset to DHCP (pass 0.0.0.0 for all parameters)
        if (!WiFi.config(IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0)))
        {
            AN_LOGW(TAG, "Failed to reset to DHCP configuration");
        }
    }

    // Check if this is an enterprise connection for the current attempt
    bool isEnterprise = _portal->isEnterpriseMode();
    String netId = _portal->getEnterpriseNetId();

    if (isEnterprise && netId.length() > 0)
    {
        connectEnterprise(ssid, netId.c_str(), password);
    }
    else
    {
        // Standard PSK connection
        uint8_t channel = (credential == nullptr) ? _portal->getSTAChannel() : 0;
        const uint8_t *bssid = _portal->getSTABSSID();
        bool bssidValid = false;
        if (bssid)
        {
            for (uint8_t i = 0; i < AUTONETWORK_BSSID_LENGTH; i++)
            {
                if (bssid[i] != 0)
                {
                    bssidValid = true;
                    break;
                }
            }
        }

        // Layer 4: Debug entry - log WiFi state before connection attempt
        AN_LOGD(TAG, "connect() entry: ssid=%s, autoReconnect=%d, WiFi.mode=%d, WiFi.status=%d",
                ssid, autoReconnect, WiFi.getMode(), WiFi.status());

        // Layer 1: Pre-flight validation - WiFi mode must support STA
        wifi_mode_t currentMode = WiFi.getMode();
        if (currentMode != WIFI_MODE_STA && currentMode != WIFI_MODE_APSTA)
        {
            AN_LOGE(TAG, "WiFi.begin() requires STA or AP_STA mode (current: %d)", currentMode);
            _status = AutoNetworkConnectionStatus::CONNECTION_FAILED;
            return;
        }

        wl_status_t status;
        if (channel > 0 && bssidValid)
        {
            AN_LOGI(TAG, "Connecting to %s on channel %d with BSSID %02X:%02X:%02X:%02X:%02X:%02X",
                     ssid, channel, bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
            status = WiFi.begin(ssid, password, channel, bssid);
        }
        else if (channel > 0)
        {
            AN_LOGI(TAG, "Connecting to %s on channel %d", ssid, channel);
            status = WiFi.begin(ssid, password, channel);
        }
        else
        {
            AN_LOGI(TAG, "Connecting to %s (no channel specified)", ssid);
            status = WiFi.begin(ssid, password);
        }

        // Layer 4: Log WiFi.begin() return value for debugging
        AN_LOGI(TAG, "WiFi.begin() returned: %d, WiFi.status() now: %d",
                status, WiFi.status());
        
        // Layer 1: Validation - WiFi.begin() return value interpretation
        // ESP32 WiFi.begin() is asynchronous: most return values indicate
        // the START of a connection attempt, not the final result.
        // However, some values (WL_NO_SHIELD, WL_CONNECTED) represent
        // immediate/final states that should be handled directly.
        // monitorConnection() will track async state changes via WiFi.status()
        
        // Layer 2: Map WiFi.begin() return value appropriately
        switch (status)
        {
            case WL_NO_SHIELD:
                // Hardware error - immediate failure
                AN_LOGE(TAG, "WiFi.begin() failed: WL_NO_SHIELD (hardware error)");
                _status = AutoNetworkConnectionStatus::CONNECTION_FAILED;
                return;
            
            case WL_CONNECTED:
                // Already connected (rare - could happen with auto-reconnect)
                AN_LOGI(TAG, "WiFi.begin() returned WL_CONNECTED (already connected)");
                _status = AutoNetworkConnectionStatus::CONNECTED;
                return;
            
            case WL_IDLE_STATUS:
            case WL_DISCONNECTED:
                // Normal - connection attempt initiated
                AN_LOGI(TAG, "WiFi.begin() returned: %d (connection initiated)", status);
                _status = AutoNetworkConnectionStatus::CONNECTING;
                break;
            
            case WL_CONNECT_FAILED:
            case WL_NO_SSID_AVAIL:
                // Immediate failure detected
                AN_LOGW(TAG, "WiFi.begin() returned failure status: %d", status);
                _status = AutoNetworkConnectionStatus::CONNECTION_FAILED;
                return;
            
            case WL_SCAN_COMPLETED:
                // Scan completed - connection not started yet
                AN_LOGD(TAG, "WiFi.begin() returned WL_SCAN_COMPLETED (unexpected)");
                _status = AutoNetworkConnectionStatus::CONNECTING;
                break;
            
            case WL_CONNECTION_LOST:
                // Connection lost - treat as disconnected, will retry
                AN_LOGW(TAG, "WiFi.begin() returned WL_CONNECTION_LOST");
                _status = AutoNetworkConnectionStatus::CONNECTING;
                break;
            
            default:
                // Unexpected return value - log and treat as connecting
                AN_LOGW(TAG, "WiFi.begin() returned unexpected status: %d", status);
                _status = AutoNetworkConnectionStatus::CONNECTING;
                break;
        }
        
        AN_LOGI(TAG, "Connection monitoring active, final status pending...");
    }
}

bool AutoNetworkConnectionManager::connectEnterprise(
    const char* ssid,
    const char* netid,
    const char* password)
{
#if defined(ESP32)
    // Layer 4: Log with PII redaction
    AN_LOGI(TAG, "Attempting WPA2 Enterprise connection");
    AN_LOGD(TAG, "Enterprise - SSID: %s, NetID: ***REDACTED*** (len=%u)", 
            ssid, strlen(netid));

    WiFi.disconnect(true);

    // Only use AP_STA mode if we're currently in AP mode (portal active)
    // Otherwise use STA-only mode to avoid creating unwanted AP
    wifi_mode_t currentMode = WiFi.getMode();
    AN_LOGI(TAG, "Current WiFi mode before Enterprise connection: %d (NULL=0, STA=1, AP=2, APSTA=3)", currentMode);

    if (currentMode == WIFI_MODE_AP || currentMode == WIFI_MODE_APSTA)
    {
        // Keep AP_STA mode to maintain portal connection during setup
        AN_LOGI(TAG, "Setting WIFI_AP_STA mode to maintain portal");
        WiFi.mode(WIFI_AP_STA);
    }
    else
    {
        // Portal not active, use STA-only mode
        AN_LOGI(TAG, "Setting WIFI_STA mode (portal not active)");
        WiFi.mode(WIFI_STA);
    }

    AN_LOGI(TAG, "WiFi mode after setting: %d", WiFi.getMode());

    // Layer 1: Entry validation - check API returns
    esp_err_t err;
    
    AN_LOGD(TAG, "Setting WPA2 Enterprise identity...");
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    err = esp_eap_client_set_identity(
        reinterpret_cast<const uint8_t *>(netid),
        strlen(netid));
#else
    err = esp_wifi_sta_wpa2_ent_set_identity(
        reinterpret_cast<const uint8_t *>(netid),
        strlen(netid));
#endif
    if (err != ESP_OK)
    {
        AN_LOGE(TAG, "Failed to set WPA2 Enterprise identity: 0x%x", err);
        _status = AutoNetworkConnectionStatus::CONNECTION_FAILED;
        return false;
    }
    
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    err = esp_eap_client_set_username(
        reinterpret_cast<const uint8_t *>(netid),
        strlen(netid));
#else
    err = esp_wifi_sta_wpa2_ent_set_username(
        reinterpret_cast<const uint8_t *>(netid),
        strlen(netid));
#endif
    if (err != ESP_OK)
    {
        AN_LOGE(TAG, "Failed to set WPA2 Enterprise username: 0x%x", err);
        _status = AutoNetworkConnectionStatus::CONNECTION_FAILED;
        return false;
    }
    
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    err = esp_eap_client_set_password(
        reinterpret_cast<const uint8_t *>(password),
        strlen(password));
#else
    err = esp_wifi_sta_wpa2_ent_set_password(
        reinterpret_cast<const uint8_t *>(password),
        strlen(password));
#endif
    if (err != ESP_OK)
    {
        AN_LOGE(TAG, "Failed to set WPA2 Enterprise password: 0x%x", err);
        _status = AutoNetworkConnectionStatus::CONNECTION_FAILED;
        return false;
    }

    AN_LOGD(TAG, "Enabling WPA2 Enterprise...");
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    err = esp_wifi_sta_enterprise_enable();
#else
    err = esp_wifi_sta_wpa2_ent_enable();
#endif
    if (err != ESP_OK)
    {
        AN_LOGE(TAG, "Failed to enable WPA2 Enterprise: 0x%x", err);
        _status = AutoNetworkConnectionStatus::CONNECTION_FAILED;
        return false;
    }

    AN_LOGD(TAG, "Starting WiFi connection...");

    // Connect with SSID only (credentials already configured)
    WiFi.begin(ssid);
    _status = AutoNetworkConnectionStatus::CONNECTING;

    // Layer 4: Log success without exposing credentials
    AN_LOGI(TAG, "WPA2 Enterprise connection initiated successfully");
    return true;
#else
    // Layer 4: Log platform limitation
    AN_LOGW(TAG, "WPA2 Enterprise not supported on this platform");
    return false;
#endif
}

void AutoNetworkConnectionManager::disconnect()
{
    // Layer 4: Log disconnect entry
    AN_LOGD(TAG, "disconnect() called, current status=%d", (int)_status);
    
#if defined(ESP32)
    // Layer 3: Environment guard - safe in test environment
    #ifdef UNIT_TEST
    AN_LOGD(TAG, "disconnect() in UNIT_TEST mode (ESP-IDF calls may be mocked)");
    #endif
    
    // Layer 2: Business validation - disable WPA2 Enterprise and clear credentials
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    esp_err_t err = esp_wifi_sta_enterprise_disable();
#else
    esp_err_t err = esp_wifi_sta_wpa2_ent_disable();
#endif
    
    // Layer 1: Check return value
    if (err == ESP_OK)
    {
        // Layer 4: Log cleanup result
        AN_LOGI(TAG, "WPA2 Enterprise credentials cleared successfully");
    }
    else if (err == ESP_ERR_INVALID_STATE)
    {
        // Layer 4: Log expected non-enterprise case
        AN_LOGV(TAG, "WPA2 Enterprise not active (non-enterprise connection)");
    }
    else
    {
        AN_LOGW(TAG, "WPA2 Enterprise disable returned: 0x%x", err);
        
        // Layer 3: In production, this is critical; in tests, may be expected
        #ifndef UNIT_TEST
        AN_LOGE(TAG, "SECURITY: Enterprise credentials may not be cleared!");
        #endif
    }
    
    // Layer 2: Explicitly clear credentials (defense-in-depth)
    // Note: esp_wifi_sta_wpa2_ent_disable() should clear them, but be explicit
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    esp_eap_client_clear_identity();
    esp_eap_client_clear_username();
    esp_eap_client_clear_password();
#else
    esp_wifi_sta_wpa2_ent_clear_identity();
    esp_wifi_sta_wpa2_ent_clear_username();
    esp_wifi_sta_wpa2_ent_clear_password();
#endif
#endif

    // Disconnect STA without erasing AP config (ESP32 only)
    // WiFi.disconnect(wifioff, eraseap) - false, false keeps AP config intact
    WiFi.disconnect(false, false);
    _status = AutoNetworkConnectionStatus::DISCONNECTED;
    
    // Layer 4: Log completion
    AN_LOGD(TAG, "disconnect() complete, status=%d", (int)_status);
}

void AutoNetworkConnectionManager::monitorConnection()
{
    // Layer 4: Log entry with current state
    AN_LOGV(TAG, "monitorConnection: WiFi.status=%d, _status=%d, _established=%d",
            WiFi.status(), (int)_status, _established);

    // Layer 3: Environment guard - check WiFi state in test environment
    #ifdef UNIT_TEST
    // In test environment, verify WiFi state is mocked properly
    if (WiFi.status() == WL_CONNECTED && WiFi.SSID().length() == 0)
    {
        AN_LOGD(TAG, "monitorConnection: WiFi connected but no SSID (mock not configured)");
    }
    #endif

    if (WiFi.status() == WL_CONNECTED)
    {
        _status = AutoNetworkConnectionStatus::CONNECTED;

        if (!_established)
        {
            String currentSSID = WiFi.SSID();

            // Layer 4: Log RECENT principle update
            AN_LOGI(TAG, "First connection established to: %s (updating RECENT timestamp)",
                    currentSSID.c_str());

            if (currentSSID.length() > 0)
            {
                // Layer 1: Entry validation - get monotonic timestamp
                unsigned long timestamp = _getMonotonicTimestamp();

                // Layer 1: Validate timestamp != 0
                if (timestamp == 0)
                {
                    AN_LOGW(TAG, "Monotonic timestamp is 0 (system just started?)");
                }

                // Layer 4: Log timestamp value
                AN_LOGD(TAG, "Monotonic timestamp for RECENT: %lu", timestamp);

                if (_credential != nullptr)
                {
                    // Layer 2: Business validation occurs in updateLastUsed()
                    bool updated = _credential->updateLastUsed(currentSSID.c_str(), timestamp);

                    // Layer 4: Log update result
                    if (updated)
                    {
                        AN_LOGI(TAG, "RECENT principle: Updated lastUsed=%lu for %s",
                                timestamp, currentSSID.c_str());
                    }
                    else
                    {
                        AN_LOGW(TAG, "Failed to update RECENT timestamp for %s", currentSSID.c_str());
                    }
                }
                else
                {
                    AN_LOGW(TAG, "Cannot update credential timestamp - credential manager is null");
                }
            }
        }

        _established = true; // Mark that we had a connection
    }
    else
    {
        // WiFi disconnected - clear established flag and update status
        if (_established)
        {
            _established = false;
            AN_LOGW(TAG, "WiFi connection lost - was connected, now disconnected");
        }

        // Update status based on WiFi state
        if (WiFi.status() == WL_DISCONNECTED)
        {
            _status = AutoNetworkConnectionStatus::DISCONNECTED;
        }
        else if (WiFi.status() == WL_CONNECT_FAILED)
        {
            _status = AutoNetworkConnectionStatus::CONNECTION_FAILED;
        }
        else if (WiFi.status() == WL_CONNECTION_LOST)
        {
            _status = AutoNetworkConnectionStatus::CONNECTION_LOST;
        }
        else if (WiFi.status() == WL_NO_SSID_AVAIL)
        {
            _status = AutoNetworkConnectionStatus::NOT_FOUND;
        }
    }
}

bool AutoNetworkConnectionManager::isConnected() const
{
    return WiFi.status() == WL_CONNECTED;
}

String AutoNetworkConnectionManager::getSSID() const
{
    return WiFi.SSID();
}

IPAddress AutoNetworkConnectionManager::getLocalIP() const
{
    return WiFi.localIP();
}

IPAddress AutoNetworkConnectionManager::getGatewayIP() const
{
    return WiFi.gatewayIP();
}

IPAddress AutoNetworkConnectionManager::getSubnetMask() const
{
    return WiFi.subnetMask();
}

AutoNetworkConnectionStatus AutoNetworkConnectionManager::getStatus() const
{
    return _status;
}

unsigned long AutoNetworkConnectionManager::_getMonotonicTimestamp() const
{
    return millis() / 1000;
}
