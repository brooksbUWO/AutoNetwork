// ****************************************************************************
// Title        : AutoNetwork Credential Storage Implementation
// Filename     : 'AutoNetworkCredential.cpp'
// Target MCU   : Espressif ESP32
// Description  : Multi-credential storage using ESP32 NVS (Preferences)
//                Binary blob format (AutoConnect-style) for robust persistence
//
// Revision History:
// When         Who         Description of change
// -----------  ----------- -----------------------
// 02-OCT-2025  Brooks      Initial implementation
// 23-OCT-2025  Brooks      Refactored to binary blob storage for robustness
//
// ****************************************************************************

// Include Files
// ****************************************************************************
#include "AutoNetworkCredential.h"
#include "AutoNetworkLog.h"
#include "esp_log.h"
#include <algorithm>

// Constants
// ****************************************************************************
static const char *TAG = "AutoNetworkCredential";

// Class Implementation
// ****************************************************************************

AutoNetworkCredential::AutoNetworkCredential()
    : _entryCount(0), _containerSize(0), _isDirty(false)
{
    // Initialize NVS flash storage
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // NVS partition was truncated or version mismatch - erase and retry
        AN_LOGW(TAG, "NVS partition issue detected, erasing and reinitializing");
        nvs_flash_erase();
        err = nvs_flash_init();
    }

    if (err != ESP_OK)
    {
        AN_LOGE(TAG, "Failed to initialize NVS flash: %s", esp_err_to_name(err));
    }
    else
    {
        AN_LOGD(TAG, "NVS flash initialized successfully");
    }

    _load();
}

AutoNetworkCredential::~AutoNetworkCredential()
{
    if (_isDirty)
    {
        _commit();
    }
}

// Load credentials from NVS into memory
void AutoNetworkCredential::_load()
{
    _credentials.clear();
    _entryCount = _import();
    _isDirty = false;
}

// Save or update a credential entry
bool AutoNetworkCredential::save(const AutoNetworkCredentialEntry &entry)
{
    if (entry.ssid.isEmpty())
    {
        AN_LOGW(TAG, "Cannot save credential with empty SSID");
        return false;
    }

    // Check if credential already exists
    uint8_t existingIndex = _findBySSID(entry.ssid.c_str());

    if (existingIndex < _credentials.size())
    {
        // Update existing credential
        AN_LOGI(TAG, "Updating credential: %s", entry.ssid.c_str());
        _credentials[existingIndex] = entry;
    }
    else
    {
        // Add new credential
        if (_credentials.size() >= AUTONETWORK_CREDENTIAL_MAX_ENTRIES)
        {
            AN_LOGW(TAG, "Credential storage full (max %d)", AUTONETWORK_CREDENTIAL_MAX_ENTRIES);
            return false;
        }

        AN_LOGI(TAG, "Adding new credential: %s", entry.ssid.c_str());
        _credentials.push_back(entry);
    }

    _isDirty = true;
    size_t written = _commit();
    return (written > 0);
}

// Load credential by index
bool AutoNetworkCredential::load(uint8_t index, AutoNetworkCredentialEntry &entry)
{
    return getByIndex(index, entry);
}

// Delete credential by SSID
bool AutoNetworkCredential::del(const char *ssid)
{
    uint8_t index = _findBySSID(ssid);
    if (index >= _credentials.size())
    {
        AN_LOGW(TAG, "Credential not found: %s", ssid);
        return false;
    }

    AN_LOGI(TAG, "Deleting credential: %s", ssid);
    _credentials.erase(_credentials.begin() + index);
    _isDirty = true;

    size_t written = _commit();
    return (written > 0);
}

// Delete all credentials
bool AutoNetworkCredential::delAll()
{
    if (!_prefs.begin(AUTONETWORK_CREDENTIAL_NAMESPACE, false))
    {
        AN_LOGE(TAG, "Failed to open preferences");
        return false;
    }

    AN_LOGI(TAG, "Clearing all credentials");
    bool success = _prefs.clear();
    _prefs.end();

    _credentials.clear();
    _entryCount = 0;
    _isDirty = false;

    return success;
}

// Get number of stored credentials
uint8_t AutoNetworkCredential::entries()
{
    return _credentials.size();
}

// Check if credential exists by SSID
bool AutoNetworkCredential::exists(const char *ssid)
{
    return (_findBySSID(ssid) < _credentials.size());
}

// Get credential by index
bool AutoNetworkCredential::getByIndex(uint8_t index, AutoNetworkCredentialEntry &entry)
{
    if (index >= _credentials.size())
    {
        return false;
    }

    entry = _credentials[index];
    return true;
}

// Get credential by recent usage (sorted by lastUsed timestamp)
bool AutoNetworkCredential::getByRecent(uint8_t index, AutoNetworkCredentialEntry &entry)
{
    if (_credentials.empty() || index >= _credentials.size())
    {
        return false;
    }

    // Create sorted indices by timestamp
    std::vector<uint8_t> indices(_credentials.size());
    for (uint8_t i = 0; i < indices.size(); i++)
    {
        indices[i] = i;
    }

    // Sort by timestamp (descending - most recent first)
    std::sort(indices.begin(), indices.end(),
        [this](uint8_t a, uint8_t b) {
            return _credentials[a].lastUsed > _credentials[b].lastUsed;
        });

    entry = _credentials[indices[index]];
    return true;
}

// Get credential by priority (sorted by priority value)
bool AutoNetworkCredential::getByPriority(uint8_t index, AutoNetworkCredentialEntry &entry)
{
    if (_credentials.empty() || index >= _credentials.size())
    {
        return false;
    }

    // Create sorted indices by priority
    std::vector<uint8_t> indices(_credentials.size());
    for (uint8_t i = 0; i < indices.size(); i++)
    {
        indices[i] = i;
    }

    // Sort by priority (descending), then by timestamp as tiebreaker
    std::sort(indices.begin(), indices.end(),
        [this](uint8_t a, uint8_t b) {
            if (_credentials[a].priority != _credentials[b].priority)
            {
                return _credentials[a].priority > _credentials[b].priority;
            }
            return _credentials[a].lastUsed > _credentials[b].lastUsed;
        });

    entry = _credentials[indices[index]];
    return true;
}

// Get credential by SSID
bool AutoNetworkCredential::getBySSID(const char *ssid, AutoNetworkCredentialEntry &entry)
{
    uint8_t index = _findBySSID(ssid);
    if (index >= _credentials.size())
    {
        return false;
    }

    entry = _credentials[index];
    return true;
}

// Update lastUsed timestamp for a credential
bool AutoNetworkCredential::updateLastUsed(const char *ssid, uint32_t timestamp)
{
    uint8_t index = _findBySSID(ssid);
    if (index >= _credentials.size())
    {
        AN_LOGW(TAG, "Credential not found for timestamp update: %s", ssid);
        return false;
    }

    _credentials[index].lastUsed = timestamp;
    _isDirty = true;

    size_t written = _commit();
    AN_LOGD(TAG, "Updated lastUsed for %s: %u", ssid, timestamp);
    return (written > 0);
}

// Update priority for a credential
bool AutoNetworkCredential::updatePriority(const char *ssid, uint8_t priority)
{
    uint8_t index = _findBySSID(ssid);
    if (index >= _credentials.size())
    {
        AN_LOGW(TAG, "Credential not found for priority update: %s", ssid);
        return false;
    }

    _credentials[index].priority = priority;
    _isDirty = true;

    size_t written = _commit();
    AN_LOGD(TAG, "Updated priority for %s: %d", ssid, priority);
    return (written > 0);
}

// Private helper: Serialize credentials to binary blob and write to NVS
size_t AutoNetworkCredential::_commit()
{
    // Calculate serialization size
    // Format: [count:1][size:2][entry1][entry2]...[entryN][\0]
    size_t blobSize = 0;

    // Header: count (1 byte) + container size (2 bytes)
    blobSize += sizeof(uint8_t) + sizeof(uint16_t);

    // Calculate size for each entry
    for (const auto &cred : _credentials)
    {
        // SSID + null terminator
        blobSize += cred.ssid.length() + sizeof('\0');

        // Password + null terminator
        blobSize += cred.password.length() + sizeof('\0');

        // BSSID (6 bytes)
        blobSize += AUTONETWORK_BSSID_LENGTH;

        // Enterprise flag (1 byte)
        blobSize += sizeof(uint8_t);

        // Enterprise NetID + null terminator (if enterprise)
        if (cred.enterprise)
        {
            blobSize += cred.enterpriseNetId.length() + sizeof('\0');
        }

        // DHCP flag (1 byte)
        blobSize += sizeof(uint8_t);

        // Static IP config (20 bytes if not DHCP)
        if (!cred.dhcp)
        {
            blobSize += 5 * sizeof(uint32_t); // ip, gateway, netmask, dns1, dns2
        }

        // lastUsed (4 bytes)
        blobSize += sizeof(uint32_t);

        // priority (1 byte)
        blobSize += sizeof(uint8_t);
    }

    // Terminator
    if (!_credentials.empty())
    {
        blobSize += sizeof('\0');
    }

    // Allocate buffer
    uint8_t *blob = (uint8_t *)malloc(blobSize);
    if (!blob)
    {
        AN_LOGE(TAG, "Failed to allocate %d bytes for credential blob", blobSize);
        return 0;
    }

    // Serialize to buffer
    uint16_t offset = 0;

    // Header
    blob[offset++] = (uint8_t)_credentials.size(); // Entry count
    _containerSize = blobSize - 3; // Container size (excluding count and size fields)
    blob[offset++] = (uint8_t)(_containerSize & 0xFF); // Low byte
    blob[offset++] = (uint8_t)((_containerSize >> 8) & 0xFF); // High byte

    // Serialize each entry
    for (const auto &cred : _credentials)
    {
        // SSID
        size_t ssidLen = cred.ssid.length();
        memcpy(&blob[offset], cred.ssid.c_str(), ssidLen);
        offset += ssidLen;
        blob[offset++] = '\0';

        // Password
        size_t passLen = cred.password.length();
        memcpy(&blob[offset], cred.password.c_str(), passLen);
        offset += passLen;
        blob[offset++] = '\0';

        // BSSID
        memcpy(&blob[offset], cred.bssid, AUTONETWORK_BSSID_LENGTH);
        offset += AUTONETWORK_BSSID_LENGTH;

        // Enterprise flag
        blob[offset++] = cred.enterprise ? 1 : 0;

        // Enterprise NetID (only if enterprise)
        if (cred.enterprise)
        {
            size_t netIdLen = cred.enterpriseNetId.length();
            memcpy(&blob[offset], cred.enterpriseNetId.c_str(), netIdLen);
            offset += netIdLen;
            blob[offset++] = '\0';
        }

        // DHCP flag
        blob[offset++] = cred.dhcp ? 0 : 1; // 0=DHCP, 1=Static

        // Static IP config (only if not DHCP)
        if (!cred.dhcp)
        {
            uint32_t ip = (uint32_t)cred.ip;
            uint32_t gateway = (uint32_t)cred.gateway;
            uint32_t netmask = (uint32_t)cred.netmask;
            uint32_t dns1 = (uint32_t)cred.dns1;
            uint32_t dns2 = (uint32_t)cred.dns2;

            memcpy(&blob[offset], &ip, sizeof(uint32_t));
            offset += sizeof(uint32_t);
            memcpy(&blob[offset], &gateway, sizeof(uint32_t));
            offset += sizeof(uint32_t);
            memcpy(&blob[offset], &netmask, sizeof(uint32_t));
            offset += sizeof(uint32_t);
            memcpy(&blob[offset], &dns1, sizeof(uint32_t));
            offset += sizeof(uint32_t);
            memcpy(&blob[offset], &dns2, sizeof(uint32_t));
            offset += sizeof(uint32_t);
        }

        // lastUsed
        memcpy(&blob[offset], &cred.lastUsed, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        // priority
        blob[offset++] = cred.priority;
    }

    // Terminator
    if (!_credentials.empty())
    {
        blob[offset++] = '\0';
    }

    // Write to NVS
    if (!_prefs.begin(AUTONETWORK_CREDENTIAL_NAMESPACE, false))
    {
        AN_LOGE(TAG, "Failed to open preferences for write");
        free(blob);
        return 0;
    }

    size_t written = _prefs.putBytes(AUTONETWORK_CREDENTIAL_BLOB_KEY, blob, blobSize);
    _prefs.end();

    free(blob);

    if (written > 0)
    {
        AN_LOGI(TAG, "Committed %d credentials (%d bytes)", _credentials.size(), written);
        _isDirty = false;
    }
    else
    {
        AN_LOGE(TAG, "Failed to write credentials to NVS");
    }

    return written;
}

// Private helper: Deserialize credentials from binary blob in NVS
uint8_t AutoNetworkCredential::_import()
{
    if (!_prefs.begin(AUTONETWORK_CREDENTIAL_NAMESPACE, true))
    {
        AN_LOGE(TAG, "Failed to open preferences for read");
        return 0;
    }

    // Get blob size
    size_t blobSize = _prefs.getBytesLength(AUTONETWORK_CREDENTIAL_BLOB_KEY);
    if (blobSize == 0)
    {
        AN_LOGI(TAG, "No saved credentials found");
        _prefs.end();
        return 0;
    }

    // Allocate buffer
    uint8_t *blob = (uint8_t *)malloc(blobSize);
    if (!blob)
    {
        AN_LOGE(TAG, "Failed to allocate %d bytes for reading credentials", blobSize);
        _prefs.end();
        return 0;
    }

    // Read blob
    size_t read = _prefs.getBytes(AUTONETWORK_CREDENTIAL_BLOB_KEY, blob, blobSize);
    _prefs.end();

    if (read != blobSize)
    {
        AN_LOGE(TAG, "Read size mismatch: expected %d, got %d", blobSize, read);
        free(blob);
        return 0;
    }

    // Parse header
    uint16_t offset = 0;
    uint8_t count = blob[offset++];
    _containerSize = blob[offset++]; // Low byte
    _containerSize |= ((uint16_t)blob[offset++] << 8); // High byte

    AN_LOGI(TAG, "Importing %d credentials from blob", count);

    // Parse entries
    _credentials.clear();
    _credentials.reserve(count);

    for (uint8_t i = 0; i < count && offset < blobSize - 1; i++)
    {
        AutoNetworkCredentialEntry entry;

        // SSID
        entry.ssid = String((const char *)&blob[offset]);
        offset += entry.ssid.length() + 1;

        // Password
        entry.password = String((const char *)&blob[offset]);
        offset += entry.password.length() + 1;

        // BSSID
        memcpy(entry.bssid, &blob[offset], AUTONETWORK_BSSID_LENGTH);
        offset += AUTONETWORK_BSSID_LENGTH;

        // Enterprise flag
        entry.enterprise = (blob[offset++] != 0);

        // Enterprise NetID (only if enterprise)
        if (entry.enterprise)
        {
            entry.enterpriseNetId = String((const char *)&blob[offset]);
            offset += entry.enterpriseNetId.length() + 1;
        }

        // DHCP flag
        uint8_t dhcpFlag = blob[offset++];
        entry.dhcp = (dhcpFlag == 0); // 0=DHCP, 1=Static

        // Static IP config (only if not DHCP)
        if (!entry.dhcp)
        {
            uint32_t ip, gateway, netmask, dns1, dns2;
            memcpy(&ip, &blob[offset], sizeof(uint32_t));
            offset += sizeof(uint32_t);
            memcpy(&gateway, &blob[offset], sizeof(uint32_t));
            offset += sizeof(uint32_t);
            memcpy(&netmask, &blob[offset], sizeof(uint32_t));
            offset += sizeof(uint32_t);
            memcpy(&dns1, &blob[offset], sizeof(uint32_t));
            offset += sizeof(uint32_t);
            memcpy(&dns2, &blob[offset], sizeof(uint32_t));
            offset += sizeof(uint32_t);

            entry.ip = IPAddress(ip);
            entry.gateway = IPAddress(gateway);
            entry.netmask = IPAddress(netmask);
            entry.dns1 = IPAddress(dns1);
            entry.dns2 = IPAddress(dns2);
        }

        // lastUsed
        memcpy(&entry.lastUsed, &blob[offset], sizeof(uint32_t));
        offset += sizeof(uint32_t);

        // priority
        entry.priority = blob[offset++];

        _credentials.push_back(entry);
        AN_LOGD(TAG, "Imported credential %d: %s", i, entry.ssid.c_str());
    }

    free(blob);

    AN_LOGI(TAG, "Successfully imported %d credentials", _credentials.size());
    return _credentials.size();
}

// Private helper: Find credential index by SSID
uint8_t AutoNetworkCredential::_findBySSID(const char *ssid)
{
    for (uint8_t i = 0; i < _credentials.size(); i++)
    {
        if (_credentials[i].ssid.equals(ssid))
        {
            return i;
        }
    }

    return AUTONETWORK_CREDENTIAL_MAX_ENTRIES; // Not found
}
