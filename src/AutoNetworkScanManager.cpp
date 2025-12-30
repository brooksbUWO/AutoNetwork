/**
 * @file AutoNetworkScanManager.cpp
 * @brief WiFi network scanning manager implementation for AutoNetwork library.
 * @author AutoNetwork Team
 * @date 2025
 */

#include "AutoNetworkScanManager.h"
#include "AutoNetworkConstants.h"
#include "AutoNetwork.h"  // For AutoNetworkEncryptionType

#include <ArduinoJson.h>

// JSON buffer size for scan results
#define AUTONETWORK_SCAN_JSON_SIZE 1024

static const char *TAG = "AutoNetworkScanManager";

// Constructor
AutoNetworkScanManager::AutoNetworkScanManager()
    : _lastScanRequest(0 - AUTONETWORK_DEBOUNCE_SCAN_MS - 1000)
{
    // CRITICAL FIX: Initialize _lastScanRequest to allow first scan at boot
    //
    // Problem: If initialized to 0, when autoConnect() calls startScan() at ~1300ms after boot,
    // the debounce check (now - _lastScanRequest < 2000ms) evaluates to (1300 - 0 = 1300ms < 2000ms),
    // causing the first scan to be REJECTED. This breaks the RECENT principle and pre-scan optimization,
    // forcing a 10-second timeout and falling back to ESP-IDF auto-reconnect.
    //
    // Solution: Use unsigned integer underflow to create a timestamp far in the past.
    // 0 - 3000 = 4,294,964,296 (very old timestamp)
    // Now the debounce check: (1300 - 4,294,964,296) wraps to large positive number > 2000ms
    // Result: First scan is ALLOWED, RECENT principle works, fast connection restored!
}

// Destructor
AutoNetworkScanManager::~AutoNetworkScanManager()
{
    clearScanResults();
}

bool AutoNetworkScanManager::startScan(unsigned long *scanStartTime, bool *scanActive)
{
    AN_LOGI(TAG, "[ScanManager] startScan() called");

    // Debounce scan requests to prevent flooding
    unsigned long now = millis();
    if (now - _lastScanRequest < AUTONETWORK_DEBOUNCE_SCAN_MS)
    {
        AN_LOGW(TAG, "[ScanManager] Scan request debounced (too soon: %lu ms since last request)",
                 now - _lastScanRequest);
        return false;
    }
    _lastScanRequest = now;

    // Ensure WiFi is in a mode that supports scanning (STA or AP_STA)
    wifi_mode_t currentMode = WiFi.getMode();
    AN_LOGI(TAG, "[ScanManager] Current WiFi mode: %d", currentMode);

    // WiFi scanning requires STA capability
    // Only switch modes if NOT already capable of scanning
    // STA and AP_STA modes already support scanning without mode change
    // Changing mode while connected disrupts the connection and causes scan failures
    if (currentMode == WIFI_MODE_NULL || currentMode == WIFI_MODE_AP)
    {
        // Need STA capability for scanning
        AN_LOGI(TAG, "[ScanManager] Switching to AP_STA mode for scanning (was mode %d)", currentMode);
        WiFi.mode(WIFI_AP_STA);
        // WiFi.mode() is synchronous, no delay needed
    }
    else
    {
        AN_LOGI(TAG, "[ScanManager] Current mode %d already supports scanning, no mode change needed", currentMode);
    }

    // Clear previous scan results
    WiFi.scanDelete();
    WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
    WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);

    // Layer 3: Environment guard - check heap before scan
    uint32_t freeHeap = ESP.getFreeHeap();
    // Measured WiFi scan memory requirements:
    // - Scan data: 50 bytes × 30 networks = 1.5KB
    // - WiFi driver overhead: ~1.5KB
    // - Safety margin: 1KB
    // Total: 4KB conservative estimate
    static constexpr uint32_t MIN_HEAP_FOR_SCAN = 4096; // 4KB minimum (measured + margin)
    
    if (freeHeap < MIN_HEAP_FOR_SCAN)
    {
        AN_LOGE(TAG, "[ScanManager] Insufficient heap for scan: %u < %u bytes",
                freeHeap, MIN_HEAP_FOR_SCAN);
        if (scanActive != nullptr)
            *scanActive = false;
        return false;
    }
    
    // Layer 4: Log heap before scan
    AN_LOGD(TAG, "[ScanManager] Free heap before scan: %u bytes", freeHeap);

    // Layer 4: Log scan start
    AN_LOGD(TAG, "[ScanManager] Starting async scan (results will be limited in getScanResults to %d)",
            AUTONETWORK_MAX_SCAN_RESULTS);
    
    // Start async scan with error handling
    // NOTE: ESP32 Arduino WiFi.scanNetworks() does NOT have max_results parameter
    // Layer 1 limiting happens in getScanResults() by clamping the returned value
    int16_t result = WiFi.scanNetworks(true);
    
    // Layer 4: Log result with heap
    AN_LOGI(TAG, "[ScanManager] scanNetworks returned: %d, heap now: %u",
            result, ESP.getFreeHeap());

    // Trust the scan started unless explicitly failed
    // WiFi.scanComplete() will return the actual state when polled
    if (result == WIFI_SCAN_FAILED)
    {
        AN_LOGW(TAG, "[ScanManager] WiFi scan failed to start - WiFi may not be ready yet");
        if (scanActive)
            *scanActive = false;
        AN_LOGI(TAG, "[ScanManager] Scan state: active=false (scan failed to start)");
        return false;
    }
    else
    {
        // Scan is either running, will start, or completed instantly
        // Trust the WiFi subsystem - WiFi.scanComplete() handles actual state checking
        AN_LOGI(TAG, "[ScanManager] WiFi scan initiated (result=%d)", result);
        if (scanStartTime)
            *scanStartTime = millis();
        if (scanActive)
            *scanActive = true;
        AN_LOGI(TAG, "[ScanManager] Scan state updated - active=true");
        return true;
    }
}

int16_t AutoNetworkScanManager::getScanResults() const
{
    int16_t result = WiFi.scanComplete();
    
    // Layer 2: Business validation - check for error codes
    if (result == WIFI_SCAN_FAILED)
    {
        AN_LOGW(TAG, "getScanResults: Scan failed");
        return WIFI_SCAN_FAILED;
    }
    
    if (result == WIFI_SCAN_RUNNING)
    {
        AN_LOGV(TAG, "getScanResults: Scan still running");
        return WIFI_SCAN_RUNNING;
    }
    
    // Layer 2: Validate result is non-negative
    if (result < 0)
    {
        AN_LOGW(TAG, "getScanResults: Unexpected negative result: %d", result);
        return result;
    }
    
    // Layer 1: CRITICAL - Limit result count to prevent memory exhaustion
    // This is the core defense mechanism since WiFi.scanNetworks() doesn't support max_results
    if (result > AUTONETWORK_MAX_SCAN_RESULTS)
    {
        AN_LOGW(TAG, "getScanResults: Found %d networks, LIMITING to %d (defense-in-depth)",
                result, AUTONETWORK_MAX_SCAN_RESULTS);
        result = AUTONETWORK_MAX_SCAN_RESULTS;
    }
    
    // Layer 4: Log result with heap
    if (result >= 0)
    {
        AN_LOGI(TAG, "getScanResults: Returning %d networks, heap=%u",
                result, ESP.getFreeHeap());
    }
    
    return result;
}

bool AutoNetworkScanManager::isScanning() const
{
    return WiFi.scanComplete() == WIFI_SCAN_RUNNING;
}

bool AutoNetworkScanManager::isScanComplete() const
{
    int16_t result = WiFi.scanComplete();
    return (result != WIFI_SCAN_RUNNING && result >= 0);
}

bool AutoNetworkScanManager::isNetworkAvailable(const String &ssid) const
{
    int16_t scanComplete = WiFi.scanComplete();
    if (scanComplete <= 0)
        return false;

    for (int16_t i = 0; i < scanComplete; i++)
    {
        if (WiFi.SSID(i) == ssid)
            return true;
    }

    return false;
}

int32_t AutoNetworkScanManager::getNetworkRSSI(const String &ssid) const
{
    int16_t scanComplete = WiFi.scanComplete();
    if (scanComplete <= 0)
        return AUTONETWORK_WORST_RSSI;

    for (int16_t i = 0; i < scanComplete; i++)
    {
        if (WiFi.SSID(i) == ssid)
            return WiFi.RSSI(i);
    }

    return AUTONETWORK_WORST_RSSI;
}

int16_t AutoNetworkScanManager::findBestMatchingCredential(
    const AutoNetworkCredentialEntry *credentials,
    uint8_t count,
    int32_t minRSSI,
    bool matchBSSID) const
{
    int16_t scanComplete = WiFi.scanComplete();
    if (scanComplete <= 0 || count == 0 || credentials == nullptr)
    {
        AN_LOGD(TAG, "[ScanManager] No scan results or credentials available for matching");
        return -1;
    }

    AN_LOGD(TAG, "[ScanManager] Checking %d saved credentials against %d scan results", count, scanComplete);

    // Find highest priority network that's available
    int16_t bestCredentialIndex = -1;
    int16_t bestPriority = AUTONETWORK_WORST_PRIORITY;
    int16_t bestSignalStrength = AUTONETWORK_WORST_RSSI;

    for (int16_t scanIndex = 0; scanIndex < scanComplete; scanIndex++)
    {
        String scannedSSID = WiFi.SSID(scanIndex);
        int32_t scannedRSSI = WiFi.RSSI(scanIndex);
        uint8_t *scannedBSSID = WiFi.BSSID(scanIndex);

        // Apply minRSSI filter
        if (scannedRSSI < minRSSI)
        {
            AN_LOGV(TAG, "[ScanManager] Skipping %s: RSSI %d < minRSSI %d", scannedSSID.c_str(), scannedRSSI, minRSSI);
            continue;
        }

        // Check if this network matches any saved credential
        for (size_t credIndex = 0; credIndex < count; credIndex++)
        {
            bool ssidMatches = (scannedSSID == credentials[credIndex].ssid);

            // Check BSSID matching if enabled
            bool bssidMatches = true;
            if (matchBSSID)
            {
                // Check if credential has non-zero BSSID
                bool hasStoredBSSID = false;
                for (uint8_t i = 0; i < AUTONETWORK_BSSID_LENGTH; i++)
                {
                    if (credentials[credIndex].bssid[i] != 0)
                    {
                        hasStoredBSSID = true;
                        break;
                    }
                }

                if (hasStoredBSSID)
                {
                    bssidMatches = memcmp(scannedBSSID, credentials[credIndex].bssid, AUTONETWORK_BSSID_LENGTH) == 0;
                }
            }

            if (ssidMatches && bssidMatches)
            {
                // Found a match - check if it's better than current best
                bool isBetter = false;

                // RECENT principle: Most recently used connection wins
                // Credentials loaded via getByRecent() are already sorted newest-first

                if (bestCredentialIndex == -1)
                {
                    // First match - always better
                    isBetter = true;
                }
                else if (credentials[credIndex].lastUsed > credentials[bestCredentialIndex].lastUsed)
                {
                    // More recently used
                    isBetter = true;
                }
                else if (credentials[credIndex].lastUsed == credentials[bestCredentialIndex].lastUsed &&
                         scannedRSSI > bestSignalStrength)
                {
                    // Same timestamp, better signal (tiebreaker)
                    isBetter = true;
                }

                if (isBetter)
                {
                    bestCredentialIndex = credIndex;
                    bestPriority = credentials[credIndex].priority;
                    bestSignalStrength = scannedRSSI;
                    AN_LOGD(TAG, "[ScanManager] Found candidate: %s (priority=%d, RSSI=%d, using RECENT principle)",
                             credentials[credIndex].ssid.c_str(), bestPriority, bestSignalStrength);
                }
            }
        }
    }

    if (bestCredentialIndex >= 0)
    {
        AN_LOGI(TAG, "[ScanManager] Best match found: %s (priority=%d, RSSI=%d)",
                 credentials[bestCredentialIndex].ssid.c_str(), bestPriority, bestSignalStrength);
    }
    else
    {
        AN_LOGD(TAG, "[ScanManager] No matching saved credentials found in scan results");
    }

    return bestCredentialIndex;
}

void AutoNetworkScanManager::generateScanJson(String &str) const
{
    int16_t n = WiFi.scanComplete();
    if (n <= 0)
    {
        str = "[]";
        return;
    }

    // Estimate: ~100 bytes per network entry
    constexpr size_t BYTES_PER_NETWORK = 100;
    size_t estimatedSize = BYTES_PER_NETWORK * n + 50;
    str.reserve(estimatedSize);

    JsonDocument json;

    JsonArray arr = json.to<JsonArray>();

    int16_t scanComplete = n;

    // Add scan result to JSON array
    for (uint16_t i = 0; i < scanComplete; i++)
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

void AutoNetworkScanManager::clearScanResults()
{
    WiFi.scanDelete();
}
