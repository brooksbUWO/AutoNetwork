/*!
 * @file AutoNetworkPortal.h
 *
 * @brief Captive portal manager for AutoNetwork library.
 *
 * @details This header defines the internal portal management system including HTTP server
 *          endpoints, DNS redirection, Access Point lifecycle, state machine, custom
 *          parameters, and OTA update interface.
 *
 * @section revision_history Revision History
 *
 * | Date | Author | Description |
 * |------|--------|-------------|
 * | 2025-10-17 | Brooks | Extracted from AutoNetwork.cpp |
 * | 2025-10-24 | Brooks | Added comprehensive Doxygen documentation |
 */

#pragma once

// Include Files
// ****************************************************************************
#include "Arduino.h"
#include "vector.h"
#include <functional>
#include "ArduinoJson.h"
#include "WiFi.h"
#include "esp_log.h"

// Async web server (permanent - sync server code removed)
#include "AsyncTCP.h"
#include "AsyncJson.h"
#include "ESPAsyncWebServer.h"

#include "DNSServer.h"
#include "AutoNetworkParameter.h"
#include "AutoNetworkPortalState.h"
#include <set> // Include for std::set to track active users.

// Forward declarations
class AutoNetwork;
class AutoNetworkParameter;
class AutoNetworkConfig;
class AutoNetworkCredential;
class AutoNetworkCredentialEntry;

// Type Definitions
// ****************************************************************************

// AutoNetworkPortalState enum is defined in AutoNetwork.h
// No need to redefine it here

// Forward declaration for callback types (if not already defined in AutoNetwork.h)
// These are typically defined in AutoNetwork.h already

// Portal Event Callback Types
// ****************************************************************************

/**
 * @brief Callback function types for portal-to-AutoNetwork communication.
 *
 * @details These callbacks enable `AutoNetworkPortal` to coordinate with `AutoNetwork`
 *          without using friend access, maintaining clean encapsulation.
 */

// WiFi Connection Callbacks
// ========================================================================

/** @brief Callback for WiFi connection request (standard WPA2-PSK).
 *  @param [in] ssid SSID to connect to.
 *  @param [in] password WiFi password.
 *  @param [in] autoReconnect Enable WiFi auto-reconnect on connection loss.
 *  @param [in] entry Pointer to credential entry (optional).
 */
using PortalConnectCallback = std::function<void(const char*, const char*, bool, const AutoNetworkCredentialEntry*)>;

/** @brief Callback for WPA2 Enterprise connection request.
 *  @param [in] ssid SSID to connect to.
 *  @param [in] netId Enterprise network identity (username).
 *  @param [in] password Enterprise password.
 */
using PortalConnectEnterpriseCallback = std::function<void(const char*, const char*, const char*)>;

/** @brief Callback for WiFi disconnection request. */
using PortalDisconnectCallback = std::function<void()>;

/** @brief Callback to request WiFi network scan. */
using PortalRequestScanCallback = std::function<void()>;

// Configuration Access Callbacks
// ========================================================================

/** @brief Callback to get current configuration.
 *  @return AutoNetworkConfig reference.
 */
using PortalGetConfigCallback = std::function<const AutoNetworkConfig&()>;

/** @brief Callback to get device hostname.
 *  @return String hostname.
 */
using PortalGetHostnameCallback = std::function<String()>;

/** @brief Callback to get configured Station SSID.
 *  @return String SSID.
 */
using PortalGetSTASSIDCallback = std::function<String()>;

/** @brief Callback to get configured Station password.
 *  @return String password.
 */
using PortalGetSTAPasswordCallback = std::function<String()>;

/** @brief Callback to check if enterprise mode is enabled.
 *  @return true if WPA2 Enterprise.
 */
using PortalGetSTAEnterpriseCallback = std::function<bool()>;

/** @brief Callback to get enterprise network identity.
 *  @return String network ID.
 */
using PortalGetSTAEnterpriseNetIdCallback = std::function<String()>;

/** @brief Callback to check if Station is configured.
 *  @return true if configured.
 */
using PortalGetSTAConfiguredCallback = std::function<bool()>;

/** @brief Callback to get WiFi connection status.
 *  @return uint8_t status code.
 */
using PortalGetStatusCallback = std::function<uint8_t()>;

// Configuration Modification Callbacks
// ========================================================================

/** @brief Callback to set Station credentials.
 *  @param [in] ssid SSID.
 *  @param [in] password Password.
 *  @param [in] enterprise Enterprise mode flag.
 *  @param [in] netId Enterprise network ID.
 *  @param [in] save Save to NVS flag.
 */
using PortalSetSTACredentialsCallback = std::function<void(const String&, const String&, bool, const String&, bool)>;

/** @brief Callback to set portal retention flag.
 *  @param [in] retain True to retain portal after connection.
 */
using PortalSetPortalRetainCallback = std::function<void(bool)>;

// Credential Manager Callbacks
// ========================================================================

/** @brief Callback to get total credential count.
 *  @return uint8_t number of entries.
 */
using PortalGetCredentialEntriesCallback = std::function<uint8_t()>;

/** @brief Callback to get credential by priority.
 *  @param [in] index Priority index.
 *  @param [out] entry Credential entry.
 *  @return true if found.
 */
using PortalGetCredentialByPriorityCallback = std::function<bool(uint8_t, AutoNetworkCredentialEntry&)>;

/** @brief Callback to get credential by recency.
 *  @param [in] index Recency index.
 *  @param [out] entry Credential entry.
 *  @return true if found.
 */
using PortalGetCredentialByRecentCallback = std::function<bool(uint8_t, AutoNetworkCredentialEntry&)>;

/** @brief Callback to get credential by storage index.
 *  @param [in] index Storage index.
 *  @param [out] entry Credential entry.
 *  @return true if found.
 */
using PortalGetCredentialByIndexCallback = std::function<bool(uint8_t, AutoNetworkCredentialEntry&)>;

/** @brief Callback to save credential.
 *  @param [in] entry Credential entry to save.
 *  @return true if save succeeded.
 */
using PortalSaveCredentialCallback = std::function<bool(const AutoNetworkCredentialEntry&)>;

/** @brief Callback to delete credential by SSID.
 *  @param [in] ssid SSID of credential to delete.
 *  @return true if delete succeeded.
 */
using PortalDeleteCredentialCallback = std::function<bool(const char*)>;

// Utility Callbacks
// ========================================================================

/** @brief Callback to update LED ticker. */
using PortalUpdateTickerCallback = std::function<void()>;

/** @brief Callback to get monotonic timestamp.
 *  @return uint64_t monotonic time value.
 */
using PortalGetMonotonicTimestampCallback = std::function<uint64_t()>;

/** @brief Callback to check if WiFi is configured.
 *  @return true if configured.
 */
using PortalIsConfiguredCallback = std::function<bool()>;

/** @brief Callback to signal webpage has been accessed. */
using PortalSetWebpageAccessedCallback = std::function<void()>;

// Class Declaration
// ****************************************************************************

/**
 * @brief AutoNetworkPortal - Captive Portal Manager
 *
 * Manages the captive portal functionality including:
 * - HTTP web server endpoints for WiFi configuration
 * - DNS server for captive portal redirection
 * - Access Point (SoftAP) lifecycle
 * - Portal state machine and timeout management
 * - Custom parameter management
 * - OTA update interface
 *
 * This class is used internally by AutoNetwork and should not be
 * instantiated directly by user code.
 */
class AutoNetworkPortal
{
public:
    /**
     * @brief Construct a new AutoNetworkPortal object
     *
     * @param server Pointer to web server instance (owned by AutoNetwork)
     * @param parent Pointer to parent AutoNetwork instance for coordination
     */
    AutoNetworkPortal(AsyncWebServer *server, AutoNetwork *parent);

    /**
     * @brief Destroy the AutoNetworkPortal object
     *
     * Performs cleanup of DNS server, handlers, and allocated resources
     */
    ~AutoNetworkPortal();

    // Portal Lifecycle Methods
    // ========================================================================

    /**
     * @brief Start the captive portal
     *
     * Initializes and starts:
     * - SoftAP (Access Point)
     * - DNS server for captive portal redirection
     * - HTTP server endpoints
     * - Portal state machine
     */
    void start();

    /**
     * @brief Stop the captive portal
     *
     * Gracefully shuts down:
     * - DNS server
     * - HTTP endpoints (unregisters handlers)
     * - SoftAP (optionally, based on configuration)
     */
    void stop();

    /**
     * @brief Process portal tasks (call from main loop)
     *
     * Handles:
     * - DNS request processing
     * - Portal timeout checking
     * - Exit flag handling
     * - Success delay management
     * - Scheduled operations
     */
    void loop();

    // Configuration Methods
    // ========================================================================

    /**
     * @brief Set Access Point credentials
     *
     * @param ssid SSID for the captive portal AP
     * @param password Password for the captive portal AP
     */
    void setAPCredentials(const char *ssid, const char *password);

    /**
     * @brief Set HTTP authentication credentials
     *
     * @param username HTTP Basic/Digest auth username
     * @param password HTTP Basic/Digest auth password
     */
    void setAuthentication(const char *username, const char *password);

    /**
     * @brief Set portal timeout duration
     *
     * @param timeout Timeout in milliseconds (0 = infinite)
     */
    void setTimeout(unsigned long timeout);

    /**
     * @brief Set whether to retain portal after successful connection
     *
     * @param retain If true, portal stays active after WiFi connects
     */
    void setRetainPortal(bool retain);

    // State Management Methods
    // ========================================================================

    /**
     * @brief Check if portal is currently active
     *
     * @return true if portal is running
     * @return false if portal is stopped
     */
    bool isActive() const { return _state.isActive(); }

    /**
     * @brief Get current portal state
     *
     * @return AutoNetworkPortalState Current state
     */
    AutoNetworkPortalState getState() const { return _state.getState(); }

    /**
     * @brief Set portal state
     *
     * @param state New state to set
     */
    void setState(AutoNetworkPortalState state);

    /**
     * @brief Schedule portal exit
     *
     * Sets exit flag and schedules portal shutdown after delay
     */
    void scheduleExit();

    /**
     * @brief Check if portal exit is scheduled
     *
     * @return true if exit flag is set
     * @return false otherwise
     */
    bool shouldExit() const { return _state.shouldExit(); }

    // Custom Parameter Methods
    // ========================================================================

    /**
     * @brief Generate next unique parameter ID
     *
     * @return uint32_t Next available parameter ID
     */
    uint32_t nextParameterId() { return _state.incrementParameterCounter(); }

    /**
     * @brief Add custom parameter to portal
     *
     * @param parameter Pointer to AutoNetworkParameter to add
     */
    void addParameter(AutoNetworkParameter *parameter);

    /**
     * @brief Remove custom parameter from portal
     *
     * @param parameter Pointer to AutoNetworkParameter to remove
     */
    void removeParameter(AutoNetworkParameter *parameter);

    // Callback Registration Methods
    // ========================================================================

    /**
     * @brief Register callback for portal state changes
     *
     * @param callback Function to call when state changes
     */
    void onPortalState(AutoNetworkOnPortalStateCallback callback)
    {
        _state.setStateCallback(callback);
    }

    /**
     * @brief Register callback for configuration submission
     *
     * @param callback Function to call when config is submitted
     */
    void onConfig(AutoNetworkOnConfigCallback callback)
    {
        _state.setConfigCallback(callback);
    }

    // Portal Event Callback Registration (for AutoNetwork coordination)
    // ========================================================================

    void setConnectCallback(PortalConnectCallback cb) { _onConnect = cb; }
    void setConnectEnterpriseCallback(PortalConnectEnterpriseCallback cb) { _onConnectEnterprise = cb; }
    void setDisconnectCallback(PortalDisconnectCallback cb) { _onDisconnect = cb; }
    void setRequestScanCallback(PortalRequestScanCallback cb) { _onRequestScan = cb; }
    void setGetConfigCallback(PortalGetConfigCallback cb) { _onGetConfig = cb; }
    void setGetHostnameCallback(PortalGetHostnameCallback cb) { _onGetHostname = cb; }
    void setGetSTASSIDCallback(PortalGetSTASSIDCallback cb) { _onGetSTASSID = cb; }
    void setGetSTAPasswordCallback(PortalGetSTAPasswordCallback cb) { _onGetSTAPassword = cb; }
    void setGetSTAEnterpriseCallback(PortalGetSTAEnterpriseCallback cb) { _onGetSTAEnterprise = cb; }
    void setGetSTAEnterpriseNetIdCallback(PortalGetSTAEnterpriseNetIdCallback cb) { _onGetSTAEnterpriseNetId = cb; }
    void setGetSTAConfiguredCallback(PortalGetSTAConfiguredCallback cb) { _onGetSTAConfigured = cb; }
    void setGetStatusCallback(PortalGetStatusCallback cb) { _onGetStatus = cb; }
    void setSetSTACredentialsCallback(PortalSetSTACredentialsCallback cb) { _onSetSTACredentials = cb; }
    void setSetPortalRetainCallback(PortalSetPortalRetainCallback cb) { _onSetPortalRetain = cb; }
    void setGetCredentialEntriesCallback(PortalGetCredentialEntriesCallback cb) { _onGetCredentialEntries = cb; }
    void setGetCredentialByPriorityCallback(PortalGetCredentialByPriorityCallback cb) { _onGetCredentialByPriority = cb; }
    void setGetCredentialByRecentCallback(PortalGetCredentialByRecentCallback cb) { _onGetCredentialByRecent = cb; }
    void setGetCredentialByIndexCallback(PortalGetCredentialByIndexCallback cb) { _onGetCredentialByIndex = cb; }
    void setSaveCredentialCallback(PortalSaveCredentialCallback cb) { _onSaveCredential = cb; }
    void setDeleteCredentialCallback(PortalDeleteCredentialCallback cb) { _onDeleteCredential = cb; }
    void setUpdateTickerCallback(PortalUpdateTickerCallback cb) { _onUpdateTicker = cb; }
    void setGetMonotonicTimestampCallback(PortalGetMonotonicTimestampCallback cb) { _onGetMonotonicTimestamp = cb; }
    void setIsConfiguredCallback(PortalIsConfiguredCallback cb) { _onIsConfigured = cb; }
    void setSetWebpageAccessedCallback(PortalSetWebpageAccessedCallback cb) { _onSetWebpageAccessed = cb; }

    // HTTP Server Control Methods (for AutoNetwork coordination)
    // ========================================================================

    /**
     * @brief Start HTTP server and register endpoints
     *
     * Called by AutoNetwork to delegate HTTP server startup to portal
     */
    void startHTTP() { _startHTTP(); }

    /**
     * @brief Stop HTTP server and unregister endpoints
     *
     * Called by AutoNetwork to delegate HTTP server shutdown to portal
     */
    void stopHTTP() { _stopHTTP(); }

    // OTA Support Methods
    // ========================================================================

    /**
     * @brief Check if OTA update is in progress
     *
     * @return true if OTA upload/update is active
     * @return false otherwise
     */
    bool isOTAInProgress() const { return _state.isOTAInProgress(); }

    // Additional State Access Methods (for AutoNetwork coordination)
    // ========================================================================

    /**
     * @brief Set portal active state
     *
     * @param active True to activate portal, false to deactivate
     */
    void setActive(bool active) { _state.setActive(active); }

    /**
     * @brief Get portal timeout value
     *
     * @return unsigned long Timeout in milliseconds (0 = infinite)
     */
    unsigned long getTimeout() const { return _state.getTimeout(); }

    /**
     * @brief Get portal start timestamp
     *
     * @return unsigned long Timestamp when portal started (millis())
     */
    unsigned long getTimeStart() const { return _state.getTimeStart(); }

    /**
     * @brief Set portal start timestamp
     *
     * @param timeStart Timestamp value from millis()
     */
    void setTimeStart(unsigned long timeStart) { _state.setTimeStart(timeStart); }

    /**
     * @brief Get connection attempt timestamp
     *
     * @return unsigned long Timestamp when connection started (millis())
     */
    unsigned long getTimeConnect() const { return _state.getTimeConnect(); }

    /**
     * @brief Set connection attempt timestamp
     *
     * @param timeConnect Timestamp value from millis()
     */
    void setTimeConnect(unsigned long timeConnect) { _state.setTimeConnect(timeConnect); }

    /**
     * @brief Get exit flag timestamp
     *
     * @return unsigned long Timestamp when exit was scheduled
     */
    unsigned long getExitTime() const { return _state.getExitTime(); }

    /**
     * @brief Clear exit flag
     */
    void clearExit() { _state.clearExit(); }

    /**
     * @brief Check if success delay is active
     *
     * @return true if delaying success state
     * @return false otherwise
     */
    bool isDelayingSuccess() const { return _state.isDelayingSuccess(); }

    /**
     * @brief Start success delay timer
     */
    void startSuccessDelay() { _state.startSuccessDelay(); }

    /**
     * @brief Get success delay timestamp
     *
     * @return unsigned long Timestamp when success delay started
     */
    unsigned long getSuccessTime() const { return _state.getSuccessTime(); }

    /**
     * @brief Clear success delay state
     */
    void clearSuccessDelay() { _state.clearSuccessDelay(); }

    /**
     * @brief Check if disconnect is scheduled
     *
     * @return true if disconnect is scheduled
     * @return false otherwise
     */
    bool isDisconnectScheduled() const { return _state.isDisconnectScheduled(); }

    /**
     * @brief Schedule disconnect operation
     */
    void scheduleDisconnect() { _state.scheduleDisconnect(); }

    /**
     * @brief Get disconnect schedule timestamp
     *
     * @return unsigned long Timestamp when disconnect was scheduled
     */
    unsigned long getDisconnectTime() const { return _state.getDisconnectTime(); }

    /**
     * @brief Clear disconnect schedule
     */
    void clearDisconnect() { _state.clearDisconnect(); }

    /**
     * @brief Get STA SSID from portal submission
     *
     * @return String SSID string
     */
    String getSTASSID() const { return _state.getSTASSID(); }

    /**
     * @brief Set STA SSID for connection attempt
     *
     * @param ssid SSID to connect to
     */
    void setSTASSID(const String &ssid) { _state.setSTASSID(ssid); }

    /**
     * @brief Get STA password from portal submission
     *
     * @return String Password string
     */
    String getSTAPassword() const { return _state.getSTAPassword(); }

    /**
     * @brief Set STA password for connection attempt
     *
     * @param password Password to use
     */
    void setSTAPassword(const String &password) { _state.setSTAPassword(password); }

    /**
     * @brief Check if enterprise mode is enabled
     *
     * @return true if WPA2 Enterprise
     * @return false if standard WPA2-PSK
     */
    bool isEnterpriseMode() const { return _state.isEnterpriseMode(); }

    /**
     * @brief Set enterprise mode
     *
     * @param enterprise True for WPA2 Enterprise, false for standard
     */
    void setEnterpriseMode(bool enterprise) { _state.setEnterpriseMode(enterprise); }

    /**
     * @brief Get enterprise network identity
     *
     * @return String Network identity (username for PEAP/MSCHAPv2)
     */
    String getEnterpriseNetId() const { return _state.getEnterpriseNetId(); }

    uint8_t getSTAChannel() const { return _state.getSTAChannel(); }

    const uint8_t* getSTABSSID() const { return _state.getSTABSSID(); }

    bool isManualConnection() const { return _state.isManualConnection(); }

    /**
     * @brief Set enterprise network identity
     *
     * @param netId Network identity string
     */
    void setEnterpriseNetId(const String &netId) { _state.setEnterpriseNetId(netId); }

    /**
     * @brief Clear STA credentials
     */
    void clearSTACredentials() { _state.clearSTACredentials(); }

    /**
     * @brief Get AP SSID
     *
     * @return String AP SSID
     */
    String getAPSSID() const { return _state.getAPSSID(); }

    /**
     * @brief Get AP password
     *
     * @return String AP password
     */
    String getAPPassword() const { return _state.getAPPassword(); }

    /**
     * @brief Get configuration parameters vector
     *
     * @return Vector<AutoNetworkParameter*>& Reference to parameters
     */
    Vector<AutoNetworkParameter *> &getParameters() { return _state.getParameters(); }

    /**
     * @brief Get configuration callback
     *
     * @return AutoNetworkOnConfigCallback Configuration callback function
     */
    AutoNetworkOnConfigCallback getConfigCallback() const { return _state.getConfigCallback(); }

    /**
     * @brief Get state change callback
     *
     * @return AutoNetworkOnPortalStateCallback State callback function
     */
    AutoNetworkOnPortalStateCallback getStateCallback() const { return _state.getStateCallback(); }

    /**
     * @brief Check if scan is active
     *
     * @return true if WiFi scan is running
     * @return false otherwise
     */
    bool isScanActive() const { return _state.isScanActive(); }

    /**
     * @brief Set scan active state
     *
     * @param active True if scan is active
     */
    void setScanActive(bool active) { _state.setScanActive(active); }

    /**
     * @brief Get scan start timestamp
     *
     * @return unsigned long Timestamp when scan started
     */
    unsigned long getScanStartTime() const { return _state.getScanStartTime(); }

    /**
     * @brief Set scan start timestamp
     *
     * @param time Timestamp value from millis()
     */
    void setScanStartTime(unsigned long time) { _state.setScanStartTime(time); }

    /**
     * @brief Get last scan status
     *
     * @return uint16_t Last scan status code
     */
    uint16_t getLastScanStatus() const { return _state.getLastScanStatus(); }

    /**
     * @brief Set last scan status
     *
     * @param status Scan status code
     */
    void setLastScanStatus(uint16_t status) { _state.setLastScanStatus(status); }

    /**
     * @brief Get parameter counter value
     *
     * @return uint32_t Current counter value
     */
    uint32_t getParameterCounter() const { return _state.getParameterCounter(); }

    /**
     * @brief Set OTA mode
     *
     * @param mode OTA mode string ("fr" for firmware, "fs" for filesystem)
     */
    void setOTAMode(const String &mode) { _state.setOTAMode(mode); }

    /**
     * @brief Get OTA mode
     *
     * @return String OTA mode
     */
    String getOTAMode() const { return _state.getOTAMode(); }

    /**
     * @brief Set OTA MD5 hash
     *
     * @param hash MD5 hash string
     */
    void setOTAMD5Hash(const String &hash) { _state.setOTAMD5Hash(hash); }

    /**
     * @brief Get OTA MD5 hash
     *
     * @return String MD5 hash
     */
    String getOTAMD5Hash() const { return _state.getOTAMD5Hash(); }

    /**
     * @brief Set OTA total size
     *
     * @param size Total size in bytes
     */
    void setOTATotalSize(size_t size) { _state.setOTATotalSize(size); }

    /**
     * @brief Get OTA total size
     *
     * @return size_t Total size in bytes
     */
    size_t getOTATotalSize() const { return _state.getOTATotalSize(); }

    /**
     * @brief Set OTA uploaded size
     *
     * @param size Uploaded size in bytes
     */
    void setOTAUploadedSize(size_t size) { _state.setOTAUploadedSize(size); }

    /**
     * @brief Get OTA uploaded size
     *
     * @return size_t Uploaded size in bytes
     */
    size_t getOTAUploadedSize() const { return _state.getOTAUploadedSize(); }

    /**
     * @brief Set OTA in progress flag
     *
     * @param inProgress True if OTA is active
     */
    void setOTAInProgress(bool inProgress) { _state.setOTAInProgress(inProgress); }

    /**
     * @brief Check if HTTP authentication is enabled
     *
     * @return true if auth is enabled
     * @return false otherwise
     */
    bool isAuthEnabled() const { return _state.isAuthEnabled(); }

    /**
     * @brief Get authentication username
     *
     * @return String Username
     */
    String getAuthUsername() const { return _state.getAuthUsername(); }

    /**
     * @brief Get authentication password
     *
     * @return String Password
     */
    String getAuthPassword() const { return _state.getAuthPassword(); }

private:
    // Server References
    // ========================================================================
    AsyncWebServer *_server;  // Web server (owned by AutoNetwork)
    AutoNetwork *_parent;            // Parent for resource access
    DNSServer *_dns;                 // DNS server (owned by portal)

    // HTTP Server State
    // ========================================================================
    bool _serverRunning;                                 // HTTP server active flag
    bool _dnsRunning;                                    // DNS server active flag
    Vector<AsyncWebHandler *> _handlers;         // HTTP handlers for cleanup
    AsyncWebHandler *_index_handler = nullptr;   // Main page handler
    AsyncWebHandler *_status_handler = nullptr;  // Status endpoint
    AsyncWebHandler *_schema_handler = nullptr;  // Schema endpoint
    AsyncWebHandler *_scan_handler = nullptr;    // Scan endpoint
    AsyncWebHandler *_save_handler = nullptr;    // Connect endpoint
    AsyncWebHandler *_clear_handler = nullptr;   // Clear endpoint
    AsyncWebHandler *_exit_handler = nullptr;    // Exit endpoint

    // Request Debouncing
    // ========================================================================
    unsigned long _lastScanRequest;  // Timestamp of last scan request

    // Portal State (Encapsulated)
    // ========================================================================
    PortalState _state;  // Owns all portal state
    std::set<IPAddress> _activeUsers; // Track client IPs that have demonstrated genuine interaction, to distinguish from captive portal probes.

    // Portal Event Callbacks (Replace friend class access)
    // ========================================================================
    PortalConnectCallback _onConnect = nullptr;
    PortalConnectEnterpriseCallback _onConnectEnterprise = nullptr;
    PortalDisconnectCallback _onDisconnect = nullptr;
    PortalRequestScanCallback _onRequestScan = nullptr;
    PortalGetConfigCallback _onGetConfig = nullptr;
    PortalGetHostnameCallback _onGetHostname = nullptr;
    PortalGetSTASSIDCallback _onGetSTASSID = nullptr;
    PortalGetSTAPasswordCallback _onGetSTAPassword = nullptr;
    PortalGetSTAEnterpriseCallback _onGetSTAEnterprise = nullptr;
    PortalGetSTAEnterpriseNetIdCallback _onGetSTAEnterpriseNetId = nullptr;
    PortalGetSTAConfiguredCallback _onGetSTAConfigured = nullptr;
    PortalGetStatusCallback _onGetStatus = nullptr;
    PortalSetSTACredentialsCallback _onSetSTACredentials = nullptr;
    PortalSetPortalRetainCallback _onSetPortalRetain = nullptr;
    PortalGetCredentialEntriesCallback _onGetCredentialEntries = nullptr;
    PortalGetCredentialByPriorityCallback _onGetCredentialByPriority = nullptr;
    PortalGetCredentialByRecentCallback _onGetCredentialByRecent = nullptr;
    PortalGetCredentialByIndexCallback _onGetCredentialByIndex = nullptr;
    PortalSaveCredentialCallback _onSaveCredential = nullptr;
    PortalDeleteCredentialCallback _onDeleteCredential = nullptr;
    PortalUpdateTickerCallback _onUpdateTicker = nullptr;
    PortalGetMonotonicTimestampCallback _onGetMonotonicTimestamp = nullptr;
    PortalIsConfiguredCallback _onIsConfigured = nullptr;
    PortalSetWebpageAccessedCallback _onSetWebpageAccessed = nullptr;

    // OTA State
    // ========================================================================
    bool _resetScheduled;
    TaskHandle_t _resetTaskHandle;
    int32_t _lastOTAProgress;

    // Private Methods - HTTP Server Management
    // ========================================================================

    /**
     * @brief Start HTTP server and register endpoints
     */
    void _startHTTP();

    /**
     * @brief Stop HTTP server and unregister endpoints
     */
    void _stopHTTP();

    /**
     * @brief Register all HTTP endpoint handlers
     */
    void _registerEndpoints();

    // Endpoint registration helper methods (Task 2.2)
    // Root endpoint removed - handled by AutoNetwork::_registerRootHandler()
    void _registerStaticResourceEndpoints();
    void _registerPortalPageEndpoint();
    void _registerInfoEndpoint();
    void _registerOTAEndpoints();
    void _registerScanEndpoint();
    void _registerWifiConnectEndpoint();
    void _registerSavedNetworksEndpoint();
    void _registerResetEndpoint();
    void _registerUserActiveEndpoint();
    void _registerCaptiveEndpoints();
    void _registerMenuEndpoint();

    /**
     * @brief Unregister all HTTP endpoint handlers
     */
    void _unregisterEndpoints();

    // Private Methods - DNS Server Management
    // ========================================================================

    /**
     * @brief Start DNS server for captive portal redirection
     */
    void _startDNS();

    /**
     * @brief Stop DNS server
     */
    void _stopDNS();

    // Private Methods - Access Point Management
    // ========================================================================

    /**
     * @brief Start SoftAP (Access Point)
     */
    void _startAP();

    /**
     * @brief Stop SoftAP
     */
    void _stopAP();

    // Private Methods - JSON Generation
    // ========================================================================

    /**
     * @brief Generate status JSON response
     *
     * @param str Output string to populate with JSON
     */
    void _generateStatusJson(String &str);

    /**
     * @brief Generate parameter schema JSON response
     *
     * @param str Output string to populate with JSON
     */
    void _generateSchemaJson(String &str);

    /**
     * @brief Generate WiFi scan results JSON response
     *
     * @param str Output string to populate with JSON
     */
    void _generateScanJson(String &str);

    // Private Methods - JSON Parsing
    // ========================================================================

    /**
     * @brief Parse custom configuration parameters from JSON
     *
     * @param arr JSON array containing parameter values
     * @return true if parsing succeeded
     * @return false if parsing failed
     */
    bool _parseConfigJson(JsonArray &arr);

    /**
     * @brief Parse WiFi credentials from JSON
     *
     * @param obj JSON object containing credentials
     * @return true if parsing succeeded
     * @return false if parsing failed
     */
    bool _parseCredentialsJson(JsonObject &obj);

    // Private Methods - Credential Management
    // ========================================================================

    /**
     * @brief Save credential entry to NVS storage with limit checking
     *
     * Handles:
     * - Credential limit enforcement
     * - Oldest credential deletion when limit reached
     * - Credential save via callback
     *
     * @param entry Credential entry to save
     * @return true if credential was saved successfully
     * @return false if save failed
     */
    bool _saveCredentialEntry(const AutoNetworkCredentialEntry &entry);

    // Private Methods - State Machine
    // ========================================================================

    /**
     * @brief Process portal state machine
     *
     * Core state machine that handles:
     * - IDLE: No operation pending
     * - CONNECTING_WIFI: Initiating WiFi connection
     * - WAITING_FOR_CONNECTION: Monitoring connection progress
     * - SUCCESS: Connection succeeded, handle portal closure
     * - FAILED: Connection failed, keep portal open
     * - TIMEOUT: Connection timed out, handle per configuration
     *
     * State transitions trigger callbacks and credential storage.
     */
    void _processStateMachine();

    // Private Methods - WiFi Scanning
    // ========================================================================

    /**
     * @brief Restart WiFi network scan (with debouncing)
     */
    void _restartScan();

    // Private Methods - Request Filtering
    // ========================================================================

    /**
     * @brief Filter function for captive portal detection
     *
     * @param request HTTP request to filter
     * @return true if request should be handled
     * @return false if request should be ignored
     */
    static bool _onAPFilter(AsyncWebServerRequest *request);
};
