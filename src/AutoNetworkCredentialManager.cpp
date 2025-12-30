// ****************************************************************************
// Title        : AutoNetwork WiFi Manager
// Filename     : 'AutoNetworkCredentialManager.cpp'
// Target MCU   : Espressif ESP32
// Description  : Credential management implementation for AutoNetwork library
//
// Revision History:
// When         Who         Description of change
// -----------  ----------- -----------------------
// 14-NOV-2025  Claude      Initial implementation - extracted from AutoNetwork
//
// ****************************************************************************

#include "AutoNetworkCredentialManager.h"
#include "AutoNetworkLog.h"

static const char* TAG = "AutoNetworkCredMgr";

// IEEE 802.11 SSID max length
static constexpr uint8_t MAX_SSID_LENGTH = 32;
// WPA2-PSK password max length
static constexpr uint8_t MAX_PASSWORD_LENGTH = 63;

// ****************************************************************************
// Constructor
// ****************************************************************************

AutoNetworkCredentialManager::AutoNetworkCredentialManager(AutoNetworkCredential &credential)
    : _credential(credential)
{
}

// ****************************************************************************
// Public Methods - Credential Operations
// ****************************************************************************

bool AutoNetworkCredentialManager::setCredentials(const char* ssid, const char* password)
{
    // Layer 4: Debug instrumentation - log entry with sanitized data
    AN_LOGD(TAG, "setCredentials() called with ssid=%s (ssid_len=%u, password_len=%u)", 
            ssid ? ssid : "(null)",
            ssid ? strlen(ssid) : 0,
            password ? strlen(password) : 0);

    // Layer 1: Entry point validation - reject null/empty at API boundary
    if (ssid == nullptr) {
        AN_LOGE(TAG, "setCredentials rejected: ssid is null");
        return false;
    }
    if (ssid[0] == '\0') {
        AN_LOGE(TAG, "setCredentials rejected: ssid is empty");
        return false;
    }
    // Layer 1: Entry validation - password can be null OR empty for open WiFi
    // Convert null to empty string (defensive normalization, not rejection)
    const char* normalizedPassword = (password == nullptr) ? "" : password;

    // Layer 2: Business logic validation - length constraints
    size_t ssidLen = strlen(ssid);
    if (ssidLen > MAX_SSID_LENGTH) {
        AN_LOGE(TAG, "setCredentials rejected: ssid length %u > max %u", 
                ssidLen, MAX_SSID_LENGTH);
        return false;
    }

    size_t passwordLen = strlen(normalizedPassword);
    
    // Layer 2: Business validation - WPA2-PSK password constraints
    // WPA2-PSK: password must be 8-63 characters if non-empty
    // Open WiFi: password is empty string (length 0)
    if (passwordLen > 0 && passwordLen < 8) {
        AN_LOGE(TAG, "setCredentials rejected: WPA2 password too short (%u < 8)", passwordLen);
        return false;
    }
    
    if (passwordLen > MAX_PASSWORD_LENGTH) {
        AN_LOGE(TAG, "setCredentials rejected: password length %u > max %u",
                passwordLen, MAX_PASSWORD_LENGTH);
        return false;
    }

    AutoNetworkCredentialEntry entry;
    entry.ssid = ssid;
    entry.password = normalizedPassword;
    entry.enterprise = false;
    entry.priority = 0;

    // Layer 4: Log what we're accepting
    AN_LOGI(TAG, "setCredentials: SSID=%s, password_len=%u (open=%d)",
            ssid, passwordLen, passwordLen == 0);

    bool result = _credential.save(entry);

    // Layer 4: Debug instrumentation - log result
    if (result) {
        AN_LOGI(TAG, "Credential saved successfully: %s", ssid);
    } else {
        AN_LOGW(TAG, "Credential save failed: %s", ssid);
    }

    return result;
}

bool AutoNetworkCredentialManager::hasCredentials() const
{
    return _credential.entries() > 0;
}

uint8_t AutoNetworkCredentialManager::getCount() const
{
    return _credential.entries();
}

bool AutoNetworkCredentialManager::getByRecent(uint8_t index, AutoNetworkCredentialEntry &entry) const
{
    return _credential.getByRecent(index, entry);
}

bool AutoNetworkCredentialManager::getByPriority(uint8_t index, AutoNetworkCredentialEntry &entry) const
{
    return _credential.getByPriority(index, entry);
}

bool AutoNetworkCredentialManager::getByIndex(uint8_t index, AutoNetworkCredentialEntry &entry) const
{
    return _credential.getByIndex(index, entry);
}

bool AutoNetworkCredentialManager::save(const AutoNetworkCredentialEntry &entry)
{
    return _credential.save(entry);
}

bool AutoNetworkCredentialManager::remove(const char* ssid)
{
    return _credential.del(ssid);
}

void AutoNetworkCredentialManager::removeAll()
{
    _credential.delAll();
}

bool AutoNetworkCredentialManager::updateLastUsed(const char* ssid, uint64_t timestamp)
{
    return _credential.updateLastUsed(ssid, timestamp);
}
