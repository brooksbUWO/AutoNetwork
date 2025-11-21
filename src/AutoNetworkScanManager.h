/**
 * @file AutoNetworkScanManager.h
 * @brief WiFi network scanning manager for AutoNetwork library.
 *
 * @details Manages WiFi network scanning operations including scan initiation,
 *          result processing, credential matching, and JSON generation.
 *          Implements the RECENT principle for credential prioritization.
 */

#ifndef AUTONETWORK_SCAN_MANAGER_H
#define AUTONETWORK_SCAN_MANAGER_H

#include <WiFi.h>
#include <Arduino.h>
#include "AutoNetworkCredential.h"
#include "AutoNetworkConstants.h"
#include "AutoNetworkLog.h"

/**
 * @class AutoNetworkScanManager
 * @brief Manages WiFi network scanning and credential matching operations.
 *
 * @details This class encapsulates all WiFi scanning functionality including:
 *          - Starting and monitoring async WiFi scans
 *          - Processing scan results
 *          - Matching scanned networks to saved credentials using RECENT principle
 *          - Generating JSON representations of scan results
 *          - Scan debouncing to prevent scan flooding
 *
 * @par RECENT Principle
 *          The manager implements the RECENT (most RECently used ENTry) principle for
 *          credential selection. When multiple saved credentials are available in scan
 *          results, the credential with the highest `lastUsed` timestamp is selected.
 *          RSSI (signal strength) is used as a tiebreaker when timestamps are equal.
 *
 * @par Usage Pattern
 * @code{.cpp}
 * AutoNetworkScanManager scanMgr;
 * scanMgr.startScan();
 *
 * // Wait for scan completion
 * while (scanMgr.isScanning()) {
 *     delay(100);
 * }
 *
 * if (scanMgr.isScanComplete()) {
 *     int count = scanMgr.getScanResults();
 *     Serial.printf("Found %d networks\n", count);
 *
 *     // Find best credential match using RECENT principle
 *     AutoNetworkCredentialEntry credentials[10];
 *     uint8_t credCount = 10; // Load from storage
 *     int16_t bestIdx = scanMgr.findBestMatchingCredential(credentials, credCount);
 *     if (bestIdx >= 0) {
 *         Serial.printf("Best match: %s\n", credentials[bestIdx].ssid);
 *     }
 * }
 * @endcode
 *
 * @see AutoNetworkCredentialManager
 * @see AutoNetworkConnectionManager
 */
class AutoNetworkScanManager
{
public:
    /**
     * @brief Default constructor.
     */
    AutoNetworkScanManager();

    /**
     * @brief Default destructor.
     */
    ~AutoNetworkScanManager();

    /**
     * @brief Start WiFi network scan.
     *
     * @details Initiates an asynchronous WiFi scan. Includes debouncing to prevent
     *          rapid consecutive scan requests (minimum 100ms between scans).
     *          Automatically configures WiFi mode if needed (STA or AP_STA modes
     *          required for scanning).
     *
     * @param [in] scanStartTime Optional pointer to store scan start timestamp.
     * @param [in] scanActive Optional pointer to store scan active flag.
     *
     * @return bool
     * @retval true Scan started successfully.
     * @retval false Scan failed to start (WiFi mode issue or debounce rejection).
     *
     * @note Scan is asynchronous - use `isScanning()` or `isScanComplete()` to check status.
     *
     * @see isScanning()
     * @see isScanComplete()
     * @see getScanResults()
     */
    bool startScan(unsigned long *scanStartTime = nullptr, bool *scanActive = nullptr);

    /**
     * @brief Get WiFi scan result count.
     *
     * @details Returns the number of networks found in the last scan.
     *          Returns WIFI_SCAN_RUNNING (-1) if scan is in progress.
     *          Returns WIFI_SCAN_FAILED (-2) if scan failed.
     *
     * @par Parameters
     *      None.
     *
     * @return int16_t Number of networks found, or negative scan status code.
     *
     * @see isScanning()
     * @see isScanComplete()
     */
    int16_t getScanResults() const;

    /**
     * @brief Check if WiFi scan is currently in progress.
     *
     * @par Parameters
     *      None.
     *
     * @return bool
     * @retval true Scan is running.
     * @retval false Scan is not running.
     *
     * @see isScanComplete()
     * @see startScan()
     */
    bool isScanning() const;

    /**
     * @brief Check if WiFi scan is complete.
     *
     * @details A scan is complete when it's not running and has results (>= 0).
     *
     * @par Parameters
     *      None.
     *
     * @return bool
     * @retval true Scan completed successfully with results.
     * @retval false Scan not complete (still running or failed).
     *
     * @see isScanning()
     * @see getScanResults()
     */
    bool isScanComplete() const;

    /**
     * @brief Check if a specific SSID is available in scan results.
     *
     * @param [in] ssid Network SSID to search for.
     *
     * @return bool
     * @retval true SSID found in scan results.
     * @retval false SSID not found in scan results.
     *
     * @see getNetworkRSSI()
     * @see findBestMatchingCredential()
     */
    bool isNetworkAvailable(const String &ssid) const;

    /**
     * @brief Get signal strength (RSSI) for a specific SSID.
     *
     * @param [in] ssid Network SSID to get signal strength for.
     *
     * @return int32_t Signal strength in dBm, or AUTONETWORK_WORST_RSSI if not found.
     *
     * @note RSSI values are typically negative (e.g., -50 dBm is stronger than -80 dBm).
     *
     * @see isNetworkAvailable()
     */
    int32_t getNetworkRSSI(const String &ssid) const;

    /**
     * @brief Find best matching credential from scan results using RECENT principle.
     *
     * @details Implements the RECENT (most RECently used ENTry) principle for
     *          credential selection. This algorithm prioritizes recently successful
     *          connections over static priority values or signal strength alone.
     *
     * @par RECENT Algorithm
     *          1. Filter: Only consider credentials with SSID in scan results
     *          2. Filter: Skip credentials with RSSI below minRSSI threshold
     *          3. Filter: Skip credentials with BSSID mismatch (if matchBSSID enabled)
     *          4. Sort: Order by lastUsed timestamp (descending - highest first)
     *          5. Tiebreak: If timestamps equal, use RSSI (higher signal wins)
     *          6. Return: Index of credential with highest lastUsed timestamp
     *
     * @par Rationale
     *          The RECENT principle assumes that recently successful connections are
     *          likely to succeed again. This provides better user experience than
     *          priority-only ordering, especially in environments with multiple
     *          available networks.
     *
     * @param [in] credentials Array of credential entries to match.
     * @param [in] count Number of credentials in array.
     * @param [in] minRSSI Minimum acceptable signal strength (dBm).
     * @param [in] matchBSSID If true, match BSSID in addition to SSID.
     *
     * @return int16_t Index of best matching credential, or -1 if no match found.
     *
     * @par Usage Example:
     * @code{.cpp}
     * // Get credentials sorted by recent usage
     * AutoNetworkCredentialEntry credentials[32];
     * uint8_t count = credentialMgr.getCount();
     * for (uint8_t i = 0; i < count; i++) {
     *     credentialMgr.getByRecent(i, credentials[i]);
     * }
     *
     * // Find best match using RECENT principle with -70 dBm minimum
     * int16_t bestIdx = scanMgr.findBestMatchingCredential(credentials, count, -70);
     * if (bestIdx >= 0) {
     *     Serial.printf("Best credential: %s (last used: %llu)\n",
     *                   credentials[bestIdx].ssid,
     *                   credentials[bestIdx].lastUsed);
     *     connectionMgr.connect(credentials[bestIdx].ssid,
     *                          credentials[bestIdx].password);
     * }
     * @endcode
     *
     * @note Credentials array should be pre-sorted by lastUsed for optimal performance.
     * @note minRSSI defaults to AUTONETWORK_WORST_RSSI (accepts any signal strength).
     *
     * @see AutoNetworkCredentialManager::getByRecent()
     * @see AutoNetworkCredentialManager::updateLastUsed()
     * @see isNetworkAvailable()
     */
    int16_t findBestMatchingCredential(
        const AutoNetworkCredentialEntry *credentials,
        uint8_t count,
        int32_t minRSSI = AUTONETWORK_WORST_RSSI,
        bool matchBSSID = false) const;

    /**
     * @brief Generate JSON representation of scan results.
     *
     * @details Creates a JSON array of scan results in compact format for
     *          transmission to web interfaces or logging.
     *
     * @param [out] str String to store JSON output.
     *
     * @par JSON Format Example:
     * @code{.cpp}
     * [
     *   {"s": "MyWiFi", "b": "AA:BB:CC:DD:EE:FF", "r": -50, "c": 6, "e": 3},
     *   {"s": "Office", "b": "11:22:33:44:55:66", "r": -72, "c": 11, "e": 3}
     * ]
     * @endcode
     *
     * @par JSON Field Descriptions:
     * - s = SSID (string)
     * - b = BSSID (MAC address string, format AA:BB:CC:DD:EE:FF)
     * - r = RSSI (signal strength in dBm, negative integer)
     * - c = channel (integer 1-13 for 2.4GHz)
     * - e = encryption type (AutoNetworkEncryptionType enum value)
     *
     * @par Parameters
     *      str - Cleared and populated with JSON array.
     *
     * @par Returns
     *      Nothing.
     *
     * @note Uses compact field names to minimize JSON size.
     * @note RSSI values are negative (e.g., -50 is stronger than -80).
     *
     * @see getScanResults()
     */
    void generateScanJson(String &str) const;

    /**
     * @brief Delete scan results and free memory.
     *
     * @details Clears cached scan results from WiFi subsystem, freeing the
     *          memory used by scan result storage.
     *
     * @par Parameters
     *      None.
     *
     * @par Returns
     *      Nothing.
     *
     * @see startScan()
     */
    void clearScanResults();

    /**
     * @brief Get timestamp of last scan request (for debouncing).
     *
     * @par Parameters
     *      None.
     *
     * @return unsigned long Timestamp in milliseconds since boot.
     *
     * @note Used internally for debouncing scan requests.
     */
    unsigned long getLastScanRequest() const { return _lastScanRequest; }

private:
    unsigned long _lastScanRequest; ///< Timestamp of last scan request (for debouncing)
};

#endif // AUTONETWORK_SCAN_MANAGER_H
