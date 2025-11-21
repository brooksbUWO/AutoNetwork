/*!
 * @file AutoNetworkPortalState.h
 *
 * @brief Portal state container and WiFi scan state machine for AutoNetwork library.
 *
 * @details This header defines the internal state management system for the captive portal.
 *          Encapsulates all portal-related state in a single, testable class with controlled
 *          access via getters/setters. Includes WiFi scan state machine for reliable network
 *          discovery.
 *
 * @section revision_history Revision History
 *
 * | Date | Author | Description |
 * |------|--------|-------------|
 * | 2025-10-17 | Brooks | Initial implementation |
 * | 2025-10-24 | Brooks | Added comprehensive Doxygen documentation |
 */

#pragma once

// Include Files
// ****************************************************************************
#include "Arduino.h"
#include "vector.h"
#include <functional>
#include "AutoNetworkConstants.h"

// AutoNetworkPortalState enum and callback typedefs are defined in AutoNetwork.h
// We include AutoNetwork.h to access these shared definitions
// Forward declaration only to avoid circular dependency
class AutoNetwork;

// Forward Declarations
// ****************************************************************************
class AutoNetworkParameter;

// Portal state enum and callback types are defined in AutoNetwork.h
// Include forward declarations here to avoid circular dependency issues
enum class AutoNetworkPortalState;  // Defined in AutoNetwork.h
using AutoNetworkOnPortalStateCallback = std::function<void(AutoNetworkPortalState)>;
using AutoNetworkOnConfigCallback = std::function<bool()>;  // Returns bool, not void

// WiFi Scan State Machine
// ****************************************************************************

/**
 * @brief WiFi scan state machine enumeration.
 *
 * @details Formal state machine for reliable WiFi network scanning with timeout
 *          protection and retry logic. Prevents indefinite hangs and ensures
 *          predictable scan behavior.
 */
enum class ScanState : uint8_t
{
    IDLE,           /**< Not scanning, waiting for scan request. */
    MODE_SWITCHING, /**< WiFi.mode() called, waiting for mode stabilization. */
    STARTING,       /**< Mode ready, attempting to start async scan. */
    RUNNING,        /**< Async WiFi scan in progress. */
    COMPLETE,       /**< Scan completed successfully, results available. */
    RETRY_DELAY     /**< Scan failed or timed out, waiting before retry. */
};

// Class Declaration
// ****************************************************************************

/**
 * @brief Portal state container for internal state management.
 *
 * @details Encapsulates all captive portal-related state in a single, testable class.
 *          Provides controlled access via getters/setters with validation and supports
 *          event-driven communication via callbacks.
 *
 * @par Managed State Categories:
 * - Portal lifecycle (active, blocking, timeouts)
 * - State machine and transitions
 * - Exit and disconnect scheduling
 * - Success delay management
 * - OTA update progress
 * - Access Point and Station configuration
 * - HTTP authentication
 * - WiFi scan state machine
 * - Scan result caching
 * - Custom configuration parameters
 *
 * @note This class is used internally by `AutoNetworkPortal` and should not be
 *       instantiated directly by user code.
 */
class PortalState
{
public:
    // Constructors and Destructor
    // ========================================================================

    /**
     * @brief Construct a new PortalState object with default values.
     *
     * @details Initializes all state variables to safe defaults (inactive, non-blocking,
     *          no timeouts, DHCP enabled, empty credentials).
     *
     * @par Parameters
     *      None.
     */
    PortalState();

    /**
     * @brief Destroy the PortalState object (default).
     *
     * @par Parameters
     *      None.
     */
    ~PortalState() = default;

    // Active State Management
    // ========================================================================

    /**
     * @brief Check if portal is currently active.
     *
     * @par Parameters
     *      None.
     *
     * @return true if portal is running.
     * @return false if portal is stopped.
     */
    bool isActive() const { return _active; }

    /**
     * @brief Set portal active state.
     *
     * @param [in] active True to activate portal, false to deactivate.
     *
     * @par Returns
     *      Nothing.
     */
    void setActive(bool active) { _active = active; }

    /**
     * @brief Check if portal is in blocking mode.
     *
     * @par Parameters
     *      None.
     *
     * @return true if portal blocks in `begin()`.
     * @return false if portal runs non-blocking.
     */
    bool isBlocking() const { return _blocking; }

    /**
     * @brief Set portal blocking mode.
     *
     * @param [in] blocking True for blocking mode, false for non-blocking.
     *
     * @par Returns
     *      Nothing.
     */
    void setBlocking(bool blocking) { _blocking = blocking; }

    // Timeout Management
    // ========================================================================

    /**
     * @brief Get portal timeout value.
     *
     * @par Parameters
     *      None.
     *
     * @return uint32_t Timeout in milliseconds (0 = infinite).
     */
    uint32_t getTimeout() const { return _timeout; }

    /**
     * @brief Set portal timeout value.
     *
     * @param [in] timeout Timeout in milliseconds (0 = infinite).
     *
     * @par Returns
     *      Nothing.
     */
    void setTimeout(uint32_t timeout) { _timeout = timeout; }

    /**
     * @brief Get portal start timestamp.
     *
     * @par Parameters
     *      None.
     *
     * @return uint32_t Timestamp when portal started (millis()).
     */
    uint32_t getTimeStart() const { return _timeStart; }

    /**
     * @brief Set portal start timestamp.
     *
     * @param [in] timeStart Timestamp value from `millis()`.
     *
     * @par Returns
     *      Nothing.
     */
    void setTimeStart(uint32_t timeStart) { _timeStart = timeStart; }

    /**
     * @brief Get connection attempt timestamp.
     *
     * @par Parameters
     *      None.
     *
     * @return uint32_t Timestamp when connection started (millis()).
     */
    uint32_t getTimeConnect() const { return _timeConnect; }

    /**
     * @brief Set connection attempt timestamp.
     *
     * @param [in] timeConnect Timestamp value from `millis()`.
     *
     * @par Returns
     *      Nothing.
     */
    void setTimeConnect(uint32_t timeConnect) { _timeConnect = timeConnect; }

    /**
     * @brief Check if portal has timed out.
     *
     * @details Compares elapsed time since `_timeStart` against `_timeout`.
     *          Returns false if timeout is 0 (infinite).
     *
     * @par Parameters
     *      None.
     *
     * @return true if portal has exceeded timeout duration.
     * @return false if still within timeout or timeout is infinite.
     */
    bool hasTimedOut() const
    {
        if (_timeout == 0) return false;  // 0 = infinite timeout
        return (millis() - _timeStart) > _timeout;
    }

    // State Machine
    // ========================================================================

    /**
     * @brief Get current portal state.
     *
     * @par Parameters
     *      None.
     *
     * @return AutoNetworkPortalState Current state enum value.
     */
    AutoNetworkPortalState getState() const { return _state; }

    /**
     * @brief Set portal state and trigger callback.
     *
     * @param [in] state New state to set.
     *
     * @par Returns
     *      Nothing.
     */
    void setState(AutoNetworkPortalState state);

    /**
     * @brief Set callback for state changes.
     *
     * @param [in] callback Function to call when state changes.
     *
     * @par Returns
     *      Nothing.
     */
    void setStateCallback(AutoNetworkOnPortalStateCallback callback)
    {
        _stateCallback = callback;
    }

    /**
     * @brief Get state change callback function.
     *
     * @par Parameters
     *      None.
     *
     * @return AutoNetworkOnPortalStateCallback Callback function.
     */
    AutoNetworkOnPortalStateCallback getStateCallback() const
    {
        return _stateCallback;
    }

    // Exit Management
    // ========================================================================

    /**
     * @brief Check if portal exit is scheduled.
     *
     * @par Parameters
     *      None.
     *
     * @return true if exit flag is set.
     * @return false otherwise.
     */
    bool shouldExit() const { return _exitFlag; }

    /**
     * @brief Schedule portal exit.
     *
     * @details Sets exit flag and records exit timestamp.
     *
     * @par Parameters
     *      None.
     *
     * @par Returns
     *      Nothing.
     */
    void scheduleExit()
    {
        _exitFlag = true;
        _exitTime = millis();
    }

    /**
     * @brief Clear exit flag.
     *
     * @par Parameters
     *      None.
     *
     * @par Returns
     *      Nothing.
     */
    void clearExit()
    {
        _exitFlag = false;
        _exitTime = 0;
    }

    /**
     * @brief Get exit timestamp.
     *
     * @par Parameters
     *      None.
     *
     * @return uint32_t Timestamp when exit was scheduled.
     */
    uint32_t getExitTime() const { return _exitTime; }

    // Success Delay Management
    // ========================================================================

    /**
     * @brief Check if success delay is active.
     *
     * @par Parameters
     *      None.
     *
     * @return true if delaying success state.
     * @return false otherwise.
     */
    bool isDelayingSuccess() const { return _successDelaying; }

    /**
     * @brief Start success delay timer.
     *
     * @par Parameters
     *      None.
     *
     * @par Returns
     *      Nothing.
     */
    void startSuccessDelay()
    {
        _successDelaying = true;
        _successTime = millis();
    }

    /**
     * @brief Clear success delay state.
     *
     * @par Parameters
     *      None.
     *
     * @par Returns
     *      Nothing.
     */
    void clearSuccessDelay()
    {
        _successDelaying = false;
        _successTime = 0;
    }

    /**
     * @brief Get success delay timestamp.
     *
     * @par Parameters
     *      None.
     *
     * @return uint32_t Timestamp when success delay started.
     */
    uint32_t getSuccessTime() const { return _successTime; }

    // Disconnect Scheduling
    // ========================================================================

    /**
     * @brief Check if disconnect is scheduled.
     *
     * @par Parameters
     *      None.
     *
     * @return true if disconnect is scheduled.
     * @return false otherwise.
     */
    bool isDisconnectScheduled() const { return _disconnectScheduled; }

    /**
     * @brief Schedule disconnect operation.
     *
     * @par Parameters
     *      None.
     *
     * @par Returns
     *      Nothing.
     */
    void scheduleDisconnect()
    {
        _disconnectScheduled = true;
        _disconnectTime = millis();
    }

    /**
     * @brief Clear disconnect schedule.
     *
     * @par Parameters
     *      None.
     *
     * @par Returns
     *      Nothing.
     */
    void clearDisconnect()
    {
        _disconnectScheduled = false;
        _disconnectTime = 0;
    }

    /**
     * @brief Get disconnect schedule timestamp.
     *
     * @par Parameters
     *      None.
     *
     * @return uint32_t Timestamp when disconnect was scheduled.
     */
    uint32_t getDisconnectTime() const { return _disconnectTime; }

    /**
     * @brief Get disconnect start timestamp for non-blocking delay.
     *
     * @par Parameters
     *      None.
     *
     * @return uint32_t Disconnect start timestamp.
     */
    uint32_t getDisconnectStartTime() const { return _disconnectStartTime; }

    /**
     * @brief Set disconnect start timestamp.
     *
     * @param [in] time Timestamp value from `millis()`.
     *
     * @par Returns
     *      Nothing.
     */
    void setDisconnectStartTime(uint32_t time) { _disconnectStartTime = time; }

    // Manual Connection State
    // ========================================================================

    /**
     * @brief Check if connection is manual (user-initiated).
     *
     * @par Parameters
     *      None.
     *
     * @return true if manual connection.
     * @return false if automatic connection.
     */
    bool isManualConnection() const { return _manualConnection; }

    /**
     * @brief Set manual connection flag.
     *
     * @param [in] manual True for manual, false for automatic.
     *
     * @par Returns
     *      Nothing.
     */
    void setManualConnection(bool manual) { _manualConnection = manual; }

    // OTA State Management
    // ========================================================================

    /**
     * @brief Check if OTA update is in progress.
     *
     * @par Parameters
     *      None.
     *
     * @return true if OTA upload/update is active.
     * @return false otherwise.
     */
    bool isOTAInProgress() const { return _otaInProgress; }

    /**
     * @brief Set OTA in progress flag.
     *
     * @param [in] inProgress True if OTA is active.
     *
     * @par Returns
     *      Nothing.
     */
    void setOTAInProgress(bool inProgress) { _otaInProgress = inProgress; }

    /**
     * @brief Check if OTA restart is pending.
     *
     * @par Parameters
     *      None.
     *
     * @return true if ESP32 restart is scheduled after OTA completion.
     * @return false otherwise.
     */
    bool isOTARestartPending() const { return _otaRestartPending; }

    /**
     * @brief Set OTA restart pending flag.
     *
     * @param [in] pending true to schedule restart after OTA completion.
     *
     * @par Returns
     *      Nothing.
     */
    void setOTARestartPending(bool pending) { _otaRestartPending = pending; }

    /**
     * @brief Get OTA mode.
     *
     * @par Parameters
     *      None.
     *
     * @return String OTA mode ("fr" for firmware, "fs" for filesystem).
     */
    String getOTAMode() const { return _otaMode; }

    /**
     * @brief Set OTA mode.
     *
     * @param [in] mode OTA mode string ("fr" or "fs").
     *
     * @par Returns
     *      Nothing.
     */
    void setOTAMode(const String &mode) { _otaMode = mode; }

    /**
     * @brief Get OTA MD5 hash.
     *
     * @par Parameters
     *      None.
     *
     * @return String MD5 hash for verification.
     */
    String getOTAMD5Hash() const { return _otaMD5Hash; }

    /**
     * @brief Set OTA MD5 hash.
     *
     * @param [in] hash MD5 hash string.
     *
     * @par Returns
     *      Nothing.
     */
    void setOTAMD5Hash(const String &hash) { _otaMD5Hash = hash; }

    /**
     * @brief Get OTA total size.
     *
     * @par Parameters
     *      None.
     *
     * @return size_t Total upload size in bytes.
     */
    size_t getOTATotalSize() const { return _otaTotalSize; }

    /**
     * @brief Set OTA total size.
     *
     * @param [in] size Total size in bytes.
     *
     * @par Returns
     *      Nothing.
     */
    void setOTATotalSize(size_t size) { _otaTotalSize = size; }

    /**
     * @brief Get OTA uploaded size.
     *
     * @par Parameters
     *      None.
     *
     * @return size_t Bytes uploaded so far.
     */
    size_t getOTAUploadedSize() const { return _otaUploadedSize; }

    /**
     * @brief Set OTA uploaded size.
     *
     * @param [in] size Uploaded size in bytes.
     *
     * @par Returns
     *      Nothing.
     */
    void setOTAUploadedSize(size_t size) { _otaUploadedSize = size; }

    /**
     * @brief Reset OTA state to defaults.
     *
     * @details Clears OTA progress, sets mode to "fr", and resets sizes to zero.
     *
     * @par Parameters
     *      None.
     *
     * @par Returns
     *      Nothing.
     */
    void resetOTA()
    {
        _otaInProgress = false;
        _otaMode = "fr";
        _otaMD5Hash = "";
        _otaTotalSize = 0;
        _otaUploadedSize = 0;
    }

    // Access Point Configuration
    // ========================================================================

    /**
     * @brief Get Access Point SSID.
     *
     * @par Parameters
     *      None.
     *
     * @return String AP SSID.
     */
    String getAPSSID() const { return _apSSID; }

    /**
     * @brief Set Access Point SSID.
     *
     * @param [in] ssid AP SSID string.
     *
     * @par Returns
     *      Nothing.
     */
    void setAPSSID(const String &ssid) { _apSSID = ssid; }

    /**
     * @brief Get Access Point password.
     *
     * @par Parameters
     *      None.
     *
     * @return String AP password.
     */
    String getAPPassword() const { return _apPassword; }

    /**
     * @brief Set Access Point password.
     *
     * @param [in] password AP password string.
     *
     * @par Returns
     *      Nothing.
     */
    void setAPPassword(const String &password) { _apPassword = password; }

    // Station Configuration (from portal submission)
    // ========================================================================

    /**
     * @brief Get Station SSID from portal submission.
     *
     * @par Parameters
     *      None.
     *
     * @return String STA SSID.
     */
    String getSTASSID() const { return _staSSID; }

    /**
     * @brief Set Station SSID for connection attempt.
     *
     * @param [in] ssid SSID to connect to.
     *
     * @par Returns
     *      Nothing.
     */
    void setSTASSID(const String &ssid) { _staSSID = ssid; }

    /**
     * @brief Get Station password from portal submission.
     *
     * @par Parameters
     *      None.
     *
     * @return String STA password.
     */
    String getSTAPassword() const { return _staPassword; }

    /**
     * @brief Set Station password for connection attempt.
     *
     * @param [in] password Password to use.
     *
     * @par Returns
     *      Nothing.
     */
    void setSTAPassword(const String &password) { _staPassword = password; }

    /**
     * @brief Check if enterprise mode is enabled.
     *
     * @par Parameters
     *      None.
     *
     * @return true if WPA2 Enterprise.
     * @return false if standard WPA2-PSK.
     */
    bool isEnterpriseMode() const { return _staEnterprise; }

    /**
     * @brief Set enterprise mode flag.
     *
     * @param [in] enterprise True for WPA2 Enterprise, false for standard.
     *
     * @par Returns
     *      Nothing.
     */
    void setEnterpriseMode(bool enterprise) { _staEnterprise = enterprise; }

    /**
     * @brief Get enterprise network identity.
     *
     * @par Parameters
     *      None.
     *
     * @return String Network identity (username for PEAP/MSCHAPv2).
     */
    String getEnterpriseNetId() const { return _staEnterpriseNetId; }

    /**
     * @brief Set enterprise network identity.
     *
     * @param [in] netId Network identity string.
     *
     * @par Returns
     *      Nothing.
     */
    void setEnterpriseNetId(const String &netId) { _staEnterpriseNetId = netId; }

    /**
     * @brief Get Station channel.
     *
     * @par Parameters
     *      None.
     *
     * @return uint8_t WiFi channel number (1-13).
     */
    uint8_t getSTAChannel() const { return _staChannel; }

    /**
     * @brief Set Station channel.
     *
     * @param [in] channel WiFi channel number.
     *
     * @par Returns
     *      Nothing.
     */
    void setSTAChannel(uint8_t channel) { _staChannel = channel; }

    /**
     * @brief Get Station BSSID (MAC address).
     *
     * @par Parameters
     *      None.
     *
     * @return const uint8_t* Pointer to 6-byte BSSID array.
     */
    const uint8_t* getSTABSSID() const { return _staBSSID; }

    /**
     * @brief Set Station BSSID for specific AP binding.
     *
     * @param [in] bssid Pointer to 6-byte MAC address (NULL to clear).
     *
     * @par Returns
     *      Nothing.
     */
    void setSTABSSID(const uint8_t* bssid) {
        if (bssid) {
            memcpy(_staBSSID, bssid, AUTONETWORK_BSSID_LENGTH);
        } else {
            memset(_staBSSID, 0, AUTONETWORK_BSSID_LENGTH);
        }
    }

    /**
     * @brief Clear all Station credentials.
     *
     * @details Resets SSID, password, enterprise mode, and network ID to empty.
     *
     * @par Parameters
     *      None.
     *
     * @par Returns
     *      Nothing.
     */
    void clearSTACredentials()
    {
        _staSSID = "";
        _staPassword = "";
        _staEnterprise = false;
        _staEnterpriseNetId = "";
    }

    // HTTP Authentication Configuration
    // ========================================================================

    /**
     * @brief Check if HTTP authentication is enabled.
     *
     * @par Parameters
     *      None.
     *
     * @return true if auth is enabled.
     * @return false otherwise.
     */
    bool isAuthEnabled() const { return _authEnabled; }

    /**
     * @brief Set HTTP authentication enabled flag.
     *
     * @param [in] enabled True to enable authentication.
     *
     * @par Returns
     *      Nothing.
     */
    void setAuthEnabled(bool enabled) { _authEnabled = enabled; }

    /**
     * @brief Get HTTP authentication username.
     *
     * @par Parameters
     *      None.
     *
     * @return String Username.
     */
    String getAuthUsername() const { return _authUsername; }

    /**
     * @brief Set HTTP authentication username.
     *
     * @param [in] username Username string.
     *
     * @par Returns
     *      Nothing.
     */
    void setAuthUsername(const String &username) { _authUsername = username; }

    /**
     * @brief Get HTTP authentication password.
     *
     * @par Parameters
     *      None.
     *
     * @return String Password.
     */
    String getAuthPassword() const { return _authPassword; }

    /**
     * @brief Set HTTP authentication password.
     *
     * @param [in] password Password string.
     *
     * @par Returns
     *      Nothing.
     */
    void setAuthPassword(const String &password) { _authPassword = password; }

    // WiFi Scan State
    // ========================================================================

    /**
     * @brief Check if WiFi scan is active.
     *
     * @par Parameters
     *      None.
     *
     * @return true if scan is running.
     * @return false otherwise.
     */
    bool isScanActive() const { return _scanActive; }

    /**
     * @brief Set scan active flag.
     *
     * @param [in] active True if scan is active.
     *
     * @par Returns
     *      Nothing.
     */
    void setScanActive(bool active) { _scanActive = active; }

    /**
     * @brief Get scan start timestamp.
     *
     * @par Parameters
     *      None.
     *
     * @return uint32_t Timestamp when scan started.
     */
    uint32_t getScanStartTime() const { return _scanStartTime; }

    /**
     * @brief Set scan start timestamp.
     *
     * @param [in] time Timestamp value from `millis()`.
     *
     * @par Returns
     *      Nothing.
     */
    void setScanStartTime(uint32_t time) { _scanStartTime = time; }

    /**
     * @brief Get last scan status code.
     *
     * @par Parameters
     *      None.
     *
     * @return uint16_t Last scan status.
     */
    uint16_t getLastScanStatus() const { return _lastScanStatus; }

    /**
     * @brief Set last scan status code.
     *
     * @param [in] status Scan status code.
     *
     * @par Returns
     *      Nothing.
     */
    void setLastScanStatus(uint16_t status) { _lastScanStatus = status; }

    // WiFi Scan State Machine
    // ========================================================================

    /**
     * @brief Get current scan state machine state.
     *
     * @par Parameters
     *      None.
     *
     * @return ScanState Current state.
     */
    ScanState getScanState() const { return _scanState; }

    /**
     * @brief Set scan state machine state with logging.
     *
     * @param [in] state New state to set.
     *
     * @par Returns
     *      Nothing.
     */
    void setScanState(ScanState state);

    /**
     * @brief Get scan state change timestamp.
     *
     * @par Parameters
     *      None.
     *
     * @return uint32_t Timestamp of last state change.
     */
    uint32_t getScanStateChangeTime() const { return _scanStateChangeTime; }

    /**
     * @brief Set scan state change timestamp.
     *
     * @param [in] time Timestamp value from `millis()`.
     *
     * @par Returns
     *      Nothing.
     */
    void setScanStateChangeTime(uint32_t time) { _scanStateChangeTime = time; }

    /**
     * @brief Request a new WiFi scan.
     *
     * @details Transitions state machine to MODE_SWITCHING or STARTING state.
     *
     * @par Parameters
     *      None.
     *
     * @par Returns
     *      Nothing.
     */
    void requestScan();

    /**
     * @brief Update scan state machine (call from loop).
     *
     * @details Advances state machine through scan lifecycle with timeout protection.
     *
     * @par Parameters
     *      None.
     *
     * @par Returns
     *      Nothing.
     */
    void updateScanStateMachine();

    // Scan Result Caching
    // ========================================================================

    /**
     * @brief Check if scan cache is valid.
     *
     * @param [in] maxAgeMs Maximum cache age in milliseconds (default: 30000).
     *
     * @return true if cache is valid and within age limit.
     * @return false if cache is invalid or too old.
     */
    bool isScanCacheValid(uint32_t maxAgeMs = 30000) const;

    /**
     * @brief Update scan cache with new results.
     *
     * @param [in] count Number of networks found.
     *
     * @par Returns
     *      Nothing.
     */
    void updateScanCache(int16_t count);

    /**
     * @brief Invalidate scan cache.
     *
     * @par Parameters
     *      None.
     *
     * @par Returns
     *      Nothing.
     */
    void invalidateScanCache();

    /**
     * @brief Get cached scan count.
     *
     * @par Parameters
     *      None.
     *
     * @return int16_t Number of networks in cache.
     */
    int16_t getCachedScanCount() const { return _scanCache.count; }

    /**
     * @brief Get scan cache timestamp.
     *
     * @par Parameters
     *      None.
     *
     * @return uint32_t Timestamp when cache was updated.
     */
    uint32_t getScanCacheTimestamp() const { return _scanCache.timestamp; }

    // Custom Configuration Parameters
    // ========================================================================

    /**
     * @brief Get parameter counter value.
     *
     * @par Parameters
     *      None.
     *
     * @return uint32_t Current counter value.
     */
    uint32_t getParameterCounter() const { return _configCounter; }

    /**
     * @brief Increment and return parameter counter.
     *
     * @par Parameters
     *      None.
     *
     * @return uint32_t Next parameter ID.
     */
    uint32_t incrementParameterCounter() { return ++_configCounter; }

    /**
     * @brief Get mutable reference to parameters vector.
     *
     * @par Parameters
     *      None.
     *
     * @return Vector<AutoNetworkParameter*>& Reference to parameters.
     */
    Vector<AutoNetworkParameter *> &getParameters() { return _configParameters; }

    /**
     * @brief Get const reference to parameters vector.
     *
     * @par Parameters
     *      None.
     *
     * @return const Vector<AutoNetworkParameter*>& Const reference to parameters.
     */
    const Vector<AutoNetworkParameter *> &getParameters() const { return _configParameters; }

    /**
     * @brief Add custom parameter to portal.
     *
     * @param [in] param Pointer to parameter (NULL ignored).
     *
     * @par Returns
     *      Nothing.
     */
    void addParameter(AutoNetworkParameter *param)
    {
        if (param != nullptr)
        {
            _configParameters.PushBack(param);
        }
    }

    /**
     * @brief Remove custom parameter from portal.
     *
     * @param [in] param Pointer to parameter to remove.
     *
     * @par Returns
     *      Nothing.
     */
    void removeParameter(AutoNetworkParameter *param)
    {
        for (size_t i = 0; i < _configParameters.Size(); i++)
        {
            if (_configParameters[i] == param)
            {
                _configParameters.Erase(i);  // Erase by index, not iterator
                break;
            }
        }
    }

    /**
     * @brief Clear all custom parameters.
     *
     * @par Parameters
     *      None.
     *
     * @par Returns
     *      Nothing.
     */
    void clearParameters()
    {
        _configParameters.Clear();
    }

    /**
     * @brief Get configuration callback function.
     *
     * @par Parameters
     *      None.
     *
     * @return AutoNetworkOnConfigCallback Callback function.
     */
    AutoNetworkOnConfigCallback getConfigCallback() const { return _configCallback; }

    /**
     * @brief Set configuration callback function.
     *
     * @param [in] callback Function to call on configuration submission.
     *
     * @par Returns
     *      Nothing.
     */
    void setConfigCallback(AutoNetworkOnConfigCallback callback) { _configCallback = callback; }

    // Reset Methods
    // ========================================================================

    /**
     * @brief Reset all state to defaults.
     *
     * @details Clears all portal state including credentials, timeouts, OTA state,
     *          scan state, and parameters.
     *
     * @par Parameters
     *      None.
     *
     * @par Returns
     *      Nothing.
     */
    void reset();

private:
    // Core State
    // ========================================================================
    bool _active;
    bool _blocking;
    uint32_t _timeout;
    uint32_t _timeStart;
    uint32_t _timeConnect;
    AutoNetworkPortalState _state;
    AutoNetworkOnPortalStateCallback _stateCallback;

    // Exit Management
    // ========================================================================
    bool _exitFlag;
    uint32_t _exitTime;

    // Success Delay Management
    // ========================================================================
    bool _successDelaying;
    uint32_t _successTime;

    // Disconnect Scheduling
    // ========================================================================
    bool _disconnectScheduled;
    uint32_t _disconnectTime;
    uint32_t _disconnectStartTime; // For non-blocking delay

    // Manual Connection State
    // ========================================================================
    bool _manualConnection;

    // OTA State
    // ========================================================================
    bool _otaInProgress;
    bool _otaRestartPending;
    String _otaMode;
    String _otaMD5Hash;
    size_t _otaTotalSize;
    size_t _otaUploadedSize;

    // Access Point Configuration
    // ========================================================================
    String _apSSID;
    String _apPassword;

    // Station Configuration
    // ========================================================================
    String _staSSID;
    String _staPassword;
    bool _staEnterprise;
    String _staEnterpriseNetId;
    uint8_t _staChannel;
    uint8_t _staBSSID[AUTONETWORK_BSSID_LENGTH];

    // HTTP Authentication
    // ========================================================================
    bool _authEnabled;
    String _authUsername;
    String _authPassword;

    // WiFi Scan State
    // ========================================================================
    bool _scanActive;
    uint32_t _scanStartTime;
    uint16_t _lastScanStatus;

    // WiFi Scan State Machine
    // ========================================================================
    ScanState _scanState;
    uint32_t _scanStateChangeTime;

    // Scan Result Cache
    // ========================================================================
    struct ScanCache {
        uint32_t timestamp;
        int16_t count;
        bool valid;
    } _scanCache;

    // Custom Configuration
    // ========================================================================
    uint32_t _configCounter;
    Vector<AutoNetworkParameter *> _configParameters;
    AutoNetworkOnConfigCallback _configCallback;
};
