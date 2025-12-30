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
static const char* TAG = "AutoNetworkCredential";
static const size_t MAX_BLOB_SIZE = 8192; // Limit blob size to prevent memory issues

// Class Implementation
// ****************************************************************************

AutoNetworkCredential::AutoNetworkCredential()
    : _entryCount(0), _containerSize(0), _isDirty(false)
{
    // Layer 3: Environment guard - initialize NVS
    #ifndef UNIT_TEST
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
        // Layer 4: Log successful initialization
        AN_LOGD(TAG, "NVS flash initialized successfully");
    }
    #else
    // Layer 3: In test environment, use mock NVS
    AN_LOGD(TAG, "UNIT_TEST mode - using mock NVS");
    #endif

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
    // Layer 4: Log save operation entry
    AN_LOGD(TAG, "save() entry: ssid=%s", entry.ssid.c_str());
    
    // Layer 1: Entry validation (defense-in-depth with manager layer)
    if (entry.ssid.isEmpty())
    {
        AN_LOGW(TAG, "save() rejected: empty SSID");
        return false;
    }

    // Layer 4: Log NVS access attempt
    AN_LOGV(TAG, "Opening NVS namespace: %s", AUTONETWORK_CREDENTIAL_NAMESPACE);
    
    // Layer 1: Entry validation - verify NVS accessible
    if (!_prefs.begin(AUTONETWORK_CREDENTIAL_NAMESPACE, false))
    {
        AN_LOGE(TAG, "save: Cannot open namespace '%s' - NVS not initialized?",
                AUTONETWORK_CREDENTIAL_NAMESPACE);
        return false;
    }
    
    // Layer 2: Business validation - verify namespace operational
    size_t freeEntries = _prefs.freeEntries();
    AN_LOGD(TAG, "NVS namespace '%s' has %u free entries",
            AUTONETWORK_CREDENTIAL_NAMESPACE, freeEntries);
    
    _prefs.end();  // Close after check, _commit() will reopen

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
    
    // Layer 4: Log result
    if (written > 0) {
        AN_LOGI(TAG, "Credential saved successfully: %s", entry.ssid.c_str());
    } else {
        AN_LOGW(TAG, "Credential save failed: %s", entry.ssid.c_str());
    }
    
    return (written > 0);
}

// Load credential by index
bool AutoNetworkCredential::load(uint8_t index, AutoNetworkCredentialEntry &entry)
{
    return getByIndex(index, entry);
}

// Delete credential by SSID
bool AutoNetworkCredential::del(const char* ssid)
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
bool AutoNetworkCredential::exists(const char* ssid)
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
bool AutoNetworkCredential::getBySSID(const char* ssid, AutoNetworkCredentialEntry &entry)
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
bool AutoNetworkCredential::updateLastUsed(const char* ssid, uint64_t timestamp)
{
    uint8_t index = _findBySSID(ssid);
    if (index >= _credentials.size())
    {
        AN_LOGW(TAG, "Credential not found for timestamp update: %s", ssid);
        return false;
    }

    // Layer 2: Business validation - check monotonic increase
    uint64_t oldTimestamp = _credentials[index].lastUsed;
    if (timestamp < oldTimestamp)
    {
        AN_LOGW(TAG, "Timestamp not increasing for %s: old=%llu, new=%llu (clock went backwards?)",
                ssid, oldTimestamp, timestamp);
        // Still allow the update in case clock was reset
    }
    else if (timestamp == oldTimestamp)
    {
        AN_LOGD(TAG, "Timestamp unchanged for %s: %llu", ssid, timestamp);
    }

    _credentials[index].lastUsed = timestamp;
    _isDirty = true;

    size_t written = _commit();

    // Layer 4: Log old→new timestamp transition
    AN_LOGD(TAG, "Updated lastUsed for %s: %llu (was %llu)", ssid, timestamp, oldTimestamp);
    return (written > 0);
}

// Update priority for a credential
bool AutoNetworkCredential::updatePriority(const char* ssid, uint8_t priority)
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
    // Format: [magic:8][count:1][size:2][entry1][entry2]...[entryN][\0]
    size_t blobSize = 0;

    // Magic identifier (8 bytes)
    const char* magic = AUTONETWORK_CREDENTIAL_IDENTIFIER;
    size_t magicLen = strlen(magic);
    blobSize += magicLen;

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

        // lastUsed (8 bytes)
        blobSize += sizeof(uint64_t);

        // priority (1 byte)
        blobSize += sizeof(uint8_t);
    }

    // Terminator
    if (!_credentials.empty())
    {
        blobSize += sizeof('\0');
    }

    if (blobSize > MAX_BLOB_SIZE)
    {
        AN_LOGE(TAG, "Credential blob too large: %d (max %d)", blobSize, MAX_BLOB_SIZE);
        return 0;
    }

    // Allocate buffer
    uint8_t* blob = static_cast<uint8_t*>(malloc(blobSize));
    if (blob == nullptr)
    {
        AN_LOGE(TAG, "Failed to allocate %d bytes for credential blob", blobSize);
        return 0;
    }

    // Serialize to buffer
    // Use uint16_t for offset since MAX_BLOB_SIZE (8192) < UINT16_MAX (65535)
    uint16_t offset = 0;

    // Magic identifier
    memcpy(&blob[offset], magic, magicLen);
    offset += magicLen;

    // Header
    blob[offset++] = (uint8_t)_credentials.size(); // Entry count
    _containerSize = blobSize - magicLen - 3; // Container size (excluding magic, count and size fields)
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
        memcpy(&blob[offset], &cred.lastUsed, sizeof(uint64_t));
        offset += sizeof(uint64_t);

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
    // Layer 4: Debug instrumentation - log start of import
    AN_LOGD(TAG, "_import: Starting credential blob import");
    
    if (!_prefs.begin(AUTONETWORK_CREDENTIAL_NAMESPACE, true))
    {
        AN_LOGW(TAG, "_import: Failed to open preferences namespace");
        return 0;
    }

    // Get blob size
    size_t blobSize = _prefs.getBytesLength(AUTONETWORK_CREDENTIAL_BLOB_KEY);
    
    // Layer 4: Log blob size for forensics
    AN_LOGD(TAG, "_import: Blob size in NVS: %u bytes", blobSize);
    
    // Layer 1: Entry validation - handle empty blob (first boot)
    if (blobSize == 0)
    {
        AN_LOGD(TAG, "_import: No credential blob found (first boot)");
        _prefs.end();
        return 0;
    }
    
    // Layer 1: Entry validation - minimum blob size check
    // Minimum valid NEW format blob: magic(8) + count(1) + containerSize(2) + terminator(1) = 12 bytes
    static constexpr size_t MIN_BLOB_SIZE = 12;
    if (blobSize < MIN_BLOB_SIZE)
    {
        AN_LOGE(TAG, "_import rejected: blob size %u < minimum %u", blobSize, MIN_BLOB_SIZE);
        _prefs.end();
        return 0;
    }
    
    // Layer 3: Environment guard - prevent memory exhaustion
    if (blobSize > MAX_BLOB_SIZE)
    {
        AN_LOGE(TAG, "_import rejected: blob size %u exceeds MAX_BLOB_SIZE %u", blobSize, MAX_BLOB_SIZE);
        _prefs.end();
        return 0;
    }

    // Allocate buffer
    uint8_t* blob = static_cast<uint8_t*>(malloc(blobSize));
    if (blob == nullptr)
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

    // Layer 2: Business validation - check magic identifier
    // Use uint16_t for offset since MAX_BLOB_SIZE (8192) < UINT16_MAX (65535)
    uint16_t offset = 0;
    const char* magic = AUTONETWORK_CREDENTIAL_IDENTIFIER;
    size_t magicLen = strlen(magic);
    
    // Validate magic length is reasonable (defensive programming)
    if (magicLen == 0 || magicLen > 32)
    {
        AN_LOGE(TAG, "_import rejected: invalid magic identifier length %u", magicLen);
        free(blob);
        return 0;
    }
    
    if (offset + magicLen > blobSize)
    {
        AN_LOGE(TAG, "_import rejected: blob too small for magic check");
        free(blob);
        return 0;
    }
    
    if (memcmp(blob + offset, magic, magicLen) != 0)
    {
        AN_LOGE(TAG, "_import rejected: invalid magic identifier (old format or corrupted blob)");
        free(blob);
        return 0;
    }
    offset += magicLen;

    // Parse header
    // Layer 4: Debug instrumentation - log offset for entry count
    AN_LOGD(TAG, "_import: Reading entry count at offset %u", offset);
    uint8_t count = blob[offset++];
    
    // Layer 4: Debug instrumentation - log offset for container size
    AN_LOGD(TAG, "_import: Reading container size at offset %u", offset);
    _containerSize = blob[offset++]; // Low byte
    _containerSize |= ((uint16_t)blob[offset++] << 8); // High byte

    AN_LOGI(TAG, "_import: Found %u credentials, container size %u", count, _containerSize);

    // Parse entries
    _credentials.clear();
    _credentials.reserve(count);

    for (uint8_t i = 0; i < count && offset < blobSize - 1; i++)
    {
        AutoNetworkCredentialEntry entry;

        // SSID - validate null terminator exists within bounds
        // Layer 4: Debug instrumentation - log SSID offset
        AN_LOGV(TAG, "_import: Reading SSID at offset %u", offset);
        if (offset >= blobSize) {
            AN_LOGE(TAG, "Blob parse error: SSID offset out of bounds");
            free(blob);
            return _credentials.size();
        }
        const char* ssidPtr = (const char*)&blob[offset];
        size_t maxLen = blobSize - offset;
        size_t ssidLen = strnlen(ssidPtr, maxLen);
        if (ssidLen == maxLen) {
            AN_LOGE(TAG, "Blob parse error: unterminated SSID string");
            free(blob);
            return _credentials.size();
        }
        entry.ssid = String(ssidPtr);
        offset += ssidLen + 1;

        // Password - validate null terminator exists within bounds
        // Layer 4: Debug instrumentation - log password offset
        AN_LOGV(TAG, "_import: Reading password at offset %u", offset);
        if (offset >= blobSize) {
            AN_LOGE(TAG, "Blob parse error: password offset out of bounds");
            free(blob);
            return _credentials.size();
        }
        const char* passPtr = (const char*)&blob[offset];
        maxLen = blobSize - offset;
        size_t passLen = strnlen(passPtr, maxLen);
        if (passLen == maxLen) {
            AN_LOGE(TAG, "Blob parse error: unterminated password string");
            free(blob);
            return _credentials.size();
        }
        entry.password = String(passPtr);
        offset += passLen + 1;

        // BSSID - bounds check for 6 bytes
        if (offset + AUTONETWORK_BSSID_LENGTH > blobSize) {
            AN_LOGE(TAG, "Blob parse error: BSSID offset out of bounds");
            free(blob);
            return _credentials.size();
        }
        memcpy(entry.bssid, &blob[offset], AUTONETWORK_BSSID_LENGTH);
        offset += AUTONETWORK_BSSID_LENGTH;

        // Enterprise flag - bounds check for 1 byte
        if (offset >= blobSize) {
            AN_LOGE(TAG, "Blob parse error: enterprise flag offset out of bounds");
            free(blob);
            return _credentials.size();
        }
        entry.enterprise = (blob[offset++] != 0);

        // Enterprise NetID (only if enterprise)
        if (entry.enterprise)
        {
            // Validate null terminator exists within bounds
            if (offset >= blobSize) {
                AN_LOGE(TAG, "Blob parse error: enterpriseNetId offset out of bounds");
                free(blob);
                return _credentials.size();
            }
            const char* netIdPtr = (const char*)&blob[offset];
            size_t netIdMaxLen = blobSize - offset;
            size_t netIdLen = strnlen(netIdPtr, netIdMaxLen);
            if (netIdLen == netIdMaxLen) {
                AN_LOGE(TAG, "Blob parse error: unterminated enterpriseNetId string");
                free(blob);
                return _credentials.size();
            }
            entry.enterpriseNetId = String(netIdPtr);
            offset += netIdLen + 1;
        }

        // DHCP flag - bounds check for 1 byte
        if (offset >= blobSize) {
            AN_LOGE(TAG, "Blob parse error: DHCP flag offset out of bounds");
            free(blob);
            return _credentials.size();
        }
        uint8_t dhcpFlag = blob[offset++];
        entry.dhcp = (dhcpFlag == 0); // 0=DHCP, 1=Static

        // Static IP config (only if not DHCP) - bounds check for 5 x uint32_t = 20 bytes
        if (!entry.dhcp)
        {
            const size_t staticIpSize = 5 * sizeof(uint32_t);
            if (offset + staticIpSize > blobSize) {
                AN_LOGE(TAG, "Blob parse error: static IP config out of bounds");
                free(blob);
                return _credentials.size();
            }
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

        // lastUsed - bounds check for uint64_t (8 bytes)
        // Layer 4: Debug instrumentation - log lastUsed offset (critical for type mismatch detection)
        AN_LOGV(TAG, "_import: Reading lastUsed (uint64_t) at offset %u", offset);
        if (offset + sizeof(uint64_t) > blobSize) {
            AN_LOGE(TAG, "Blob parse error: lastUsed offset out of bounds");
            free(blob);
            return _credentials.size();
        }
        memcpy(&entry.lastUsed, &blob[offset], sizeof(uint64_t));
        offset += sizeof(uint64_t);

        // priority - bounds check for 1 byte
        if (offset >= blobSize) {
            AN_LOGE(TAG, "Blob parse error: priority offset out of bounds");
            free(blob);
            return _credentials.size();
        }
        entry.priority = blob[offset++];

        _credentials.push_back(entry);
        AN_LOGD(TAG, "Imported credential %d: %s", i, entry.ssid.c_str());
    }

    free(blob);

    // Layer 4: Debug instrumentation - log final result
    AN_LOGI(TAG, "_import: Successfully loaded %u credentials from blob", _credentials.size());
    return _credentials.size();
}

// Private helper: Find credential index by SSID
uint8_t AutoNetworkCredential::_findBySSID(const char* ssid)
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
