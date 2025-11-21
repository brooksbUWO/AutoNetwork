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
#include "esp_wpa2.h"
#endif

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
    // Set auto reconnect (ESP32)
    if (autoReconnect)
    {
        WiFi.setAutoReconnect(true);
    }

    // Apply static IP configuration if provided and not using DHCP
    if (credential != nullptr && !credential->dhcp)
    {
        AN_LOGI(TAG, "Applying static IP configuration: IP=%s, Gateway=%s, Netmask=%s",
                 credential->ip.toString().c_str(),
                 credential->gateway.toString().c_str(),
                 credential->netmask.toString().c_str());

        if (!WiFi.config(credential->ip, credential->gateway, credential->netmask,
                         credential->dns1, credential->dns2))
        {
            AN_LOGW(TAG, "Failed to apply static IP configuration, falling back to DHCP");
        }
    }
    else if (credential != nullptr && credential->dhcp)
    {
        AN_LOGD(TAG, "Using DHCP for IP configuration");
        // Reset to DHCP (pass 0.0.0.0 for all parameters)
        WiFi.config(IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0));
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

        if (channel > 0 && bssidValid)
        {
            AN_LOGI(TAG, "Connecting to %s on channel %d with BSSID %02X:%02X:%02X:%02X:%02X:%02X",
                     ssid, channel, bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
            WiFi.begin(ssid, password, channel, bssid);
        }
        else if (channel > 0)
        {
            AN_LOGI(TAG, "Connecting to %s on channel %d", ssid, channel);
            WiFi.begin(ssid, password, channel);
        }
        else
        {
            AN_LOGI(TAG, "Connecting to %s (no channel specified)", ssid);
            WiFi.begin(ssid, password);
        }
        _status = AutoNetworkConnectionStatus::CONNECTING;
    }
}

bool AutoNetworkConnectionManager::connectEnterprise(
    const char *ssid,
    const char *netid,
    const char *password)
{
#if defined(ESP32)
    AN_LOGI(TAG, "Attempting WPA2 Enterprise connection");
    AN_LOGD(TAG, "Enterprise connection - SSID: %s, NetID: %s",
             ssid, netid);

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

    AN_LOGD(TAG, "Setting WPA2 Enterprise identity...");

    // Configure identity and username (both use netid)
    esp_wifi_sta_wpa2_ent_set_identity(
        reinterpret_cast<const uint8_t *>(netid),
        strlen(netid));
    esp_wifi_sta_wpa2_ent_set_username(
        reinterpret_cast<const uint8_t *>(netid),
        strlen(netid));
    esp_wifi_sta_wpa2_ent_set_password(
        reinterpret_cast<const uint8_t *>(password),
        strlen(password));

    AN_LOGD(TAG, "Enabling WPA2 Enterprise...");

    // Enable WPA2 Enterprise
    esp_wifi_sta_wpa2_ent_enable();

    AN_LOGD(TAG, "Starting WiFi connection...");

    // Connect with SSID only (credentials already configured)
    WiFi.begin(ssid);
    _status = AutoNetworkConnectionStatus::CONNECTING;

    AN_LOGI(TAG, "WPA2 Enterprise connection initiated");
    return true;
#else
    AN_LOGW(TAG, "WPA2 Enterprise not supported");
    return false;
#endif
}

void AutoNetworkConnectionManager::disconnect()
{
    // Disconnect STA without erasing AP config (ESP32 only)
    // WiFi.disconnect(wifioff, eraseap) - false, false keeps AP config intact
    WiFi.disconnect(false, false);
    _status = AutoNetworkConnectionStatus::DISCONNECTED;
}

void AutoNetworkConnectionManager::monitorConnection()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        _status = AutoNetworkConnectionStatus::CONNECTED;

        if (!_established)
        {
            String currentSSID = WiFi.SSID();
            if (currentSSID.length() > 0)
            {
                unsigned long timestamp = _getMonotonicTimestamp();
                if (_credential->updateLastUsed(currentSSID.c_str(), timestamp))
                {
                    AN_LOGD(TAG, "Updated lastUsed timestamp for %s: %lu", currentSSID.c_str(), timestamp);
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
