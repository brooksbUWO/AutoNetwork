// ****************************************************************************
// Title        : AutoNetwork Library
// Filename     : 'AutoNetwork.h'
// Target MCU   : Espressif ESP32 (Doit DevKit Version 1)
// Description  : WiFi connection manager with captive portal functionality
//
// Revision History:
// When         Who         Description of change
// -----------  ----------- -----------------------
// 02-OCT-2025  Brooks      Initial implementation
//
// ****************************************************************************

#pragma once

// Configuration Macros
// ****************************************************************************

// ESP-IDF Logging Configuration
// Set default log level at compile time (can be changed at runtime)
#ifndef AUTONETWORK_LOG_LEVEL
#define AUTONETWORK_LOG_LEVEL ESP_LOG_INFO
#endif

// Constants
// ****************************************************************************

/**
 * @brief Portal timeout duration in milliseconds.
 *
 * @details Defines the default timeout for the captive portal. When set to 0,
 *          the portal remains active indefinitely during the configuration phase.
 *          Portal timeout only applies when no stations are connected (idle state).
 *
 * @note Value: 0 (infinite timeout)
 * @note Can be overridden via `setPortalTimeout()` or `AutoNetworkConfig::timeoutPortalMs`.
 */
#define AUTONETWORK_PORTAL_TIMEOUT 0

/**
 * @brief WiFi connection timeout in milliseconds.
 *
 * @details Maximum time to wait for WiFi connection to complete before considering
 *          the attempt failed and trying the next credential or starting the captive portal.
 *
 * @note Value: 30000ms (30 seconds)
 * @note Can be overridden via `setConnectTimeout()` or `AutoNetworkConfig::timeoutConnectMs`.
 */
#define AUTONETWORK_CONNECT_TIMEOUT 30000

/**
 * @brief Portal exit delay in milliseconds.
 *
 * @details Time to wait after exit is scheduled before actually stopping the portal.
 *          Allows pending HTTP responses to complete before server shutdown.
 *
 * @note Value: 5000ms (5 seconds)
 */
#define AUTONETWORK_EXIT_TIMEOUT 5000

/**
 * @brief Unit time for reconnection interval in seconds.
 *
 * @details Base time unit for calculating background reconnection intervals.
 *          `AutoNetworkConfig::staReconnectInterval` multiplied by this value
 *          determines actual reconnection delay (e.g., interval=2 → 60 seconds).
 *
 * @note Value: 30 seconds
 * @note Reconnection interval = `staReconnectInterval × AUTONETWORK_UNITTIME` seconds
 */
#define AUTONETWORK_UNITTIME 30

#ifndef AUTONETWORK_TICKER_PORT
/**
 * @brief Default GPIO pin for LED status ticker.
 *
 * @details Default pin used for visual WiFi status indication when ticker is enabled.
 *          Uses the built-in LED if available, otherwise must be defined before including library.
 *
 * @note Default: `LED_BUILTIN`
 * @note Can be overridden via `setTickerPort()` or `AutoNetworkConfig::tickerPin`.
 */
#define AUTONETWORK_TICKER_PORT LED_BUILTIN
#endif

// Include Files
// ****************************************************************************
#include "Arduino.h"
#include "stdlib_noniso.h"
#include "vector.h"
#include <functional>
#include "Preferences.h"
#include "ArduinoJson.h"
#include "WiFi.h"
#include "esp_log.h"

// ESP-IDF version-based API selection
// esp_eap_client.h is only available in ESP-IDF v5.0+
// Earlier versions use the deprecated esp_wpa2.h API
#include "esp_idf_version.h"

#if defined(ESP_IDF_VERSION) && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#define AUTONETWORK_ESP_IDF_MODE
#include "esp_eap_client.h" // ESP-IDF v5.0+ modular EAP API
#else
#include "esp_wpa2.h" // ESP-IDF v4.4.x and earlier
#endif

// Embedded web pages (permanent - always included)
#include "webpage_menu.h"
#include "webpage_stats.h"
#include "webpage_ota.h"
#include "webpage_creds.h"
#include "webpage_reset.h"
#include "webpage_disconnect.h"
#include "webpage_wifi.h"
#include "webpage_css.h"
#include "webpage_error.h"

// Debug pages (optional - enable with AUTONETWORK_DEBUG)
#ifdef AUTONETWORK_DEBUG

#endif

// Optional large web pages (dashboard ~355KB, webserial ~148KB)
// Uncomment if you have these files and sufficient flash space
// #include "webpage_dashboard.h"
// #include "webpage_webserial.h"

// Async web server (permanent - sync server code removed)
#include "AsyncTCP.h"
#include "AsyncJson.h"
#include "ESPAsyncWebServer.h"

#include "DNSServer.h"
#include "AutoNetworkConstants.h"
#include "AutoNetworkConfig.h"
#include "AutoNetworkCredential.h"

// Type Definitions and Enumerations
// ****************************************************************************

/**
 * @brief WiFi connection status enumeration.
 *
 * @details Represents the current state of the WiFi connection managed by AutoNetwork.
 *          Status is updated continuously by `loop()` based on WiFi events and connection
 *          state changes. Query with `getConnectionStatus()`.
 */
enum class AutoNetworkConnectionStatus : uint8_t
{
    DISCONNECTED = 0,    /**< Not connected to any WiFi network */
    CONNECTING,          /**< Connection attempt in progress */
    CONNECTED,           /**< Successfully connected to WiFi network */
    CONNECTION_FAILED,   /**< Connection attempt failed (invalid credentials or other error) */
    CONNECTION_LOST,     /**< Previously connected, now disconnected */
    NOT_FOUND,           /**< Target SSID not found in WiFi scan results */
};

// Manager Includes (require enum definitions above)
// ****************************************************************************
#include "AutoNetworkCredentialManager.h"
#include "AutoNetworkConnectionManager.h"
#include "AutoNetworkScanManager.h"

// Forward Declarations
// ****************************************************************************
class AutoNetworkParameter;
class AutoNetworkTicker;
class AutoNetworkPortal;

/**
 * @brief Captive portal state enumeration.
 *
 * @details Represents the current state of the captive portal state machine.
 *          Portal progresses through these states during WiFi configuration and
 *          connection attempts. Query with `getPortalState()`.
 */
enum class AutoNetworkPortalState
{
    IDLE = 0,                  /**< Portal idle, no operation in progress */
    DISCONNECTING,             /**< Disconnecting from current network */
    CONNECTING_WIFI,           /**< Initiating WiFi connection */
    WAITING_FOR_CONNECTION,    /**< Waiting for connection to complete */
    SUCCESS,                   /**< Connection succeeded */
    FAILED,                    /**< Connection failed */
    TIMEOUT                    /**< Connection attempt timed out */
};

/**
 * @brief WiFi encryption type enumeration.
 *
 * @details Represents the security type of WiFi networks detected during scanning.
 *          Used in scan results JSON to indicate network security requirements.
 */
enum class AutoNetworkEncryptionType
{
    OPEN = 0,                  /**< Open network (no encryption) */
    WEP,                       /**< WEP encryption (deprecated, insecure) */
    WPA_PSK,                   /**< WPA-PSK (TKIP) */
    WPA2_PSK,                  /**< WPA2-PSK (AES) - most common */
    WPA_WPA2_PSK,              /**< Mixed WPA/WPA2-PSK mode */
    ENTERPRISE,                /**< Enterprise authentication (802.1X) */
    WPA2_ENTERPRISE,           /**< WPA2 Enterprise (PEAP/MSCHAPv2) */
    WPA3_PSK,                  /**< WPA3-PSK (latest security standard) */
    WPA2_WPA3_PSK,             /**< Mixed WPA2/WPA3-PSK mode */
    WAPI_PSK,                  /**< WAPI-PSK (Chinese standard) */
    OWE,                       /**< Opportunistic Wireless Encryption */
    WPA3_ENT_192,              /**< WPA3 Enterprise 192-bit mode */
    WPA3_EXT_PSK,              /**< WPA3 Extended PSK */
    WPA3_EXT_PSK_MIXED_MODE,   /**< WPA3 Extended PSK mixed mode */
    MAX,                       /**< Maximum enum value */
    UNKNOWN                    /**< Unknown or unsupported encryption type */
};

// Forward declarations
class AutoNetworkParameter;
class AutoNetworkPortal;

// Callback function type definitions
/**
 * @brief Callback function type for WiFi connection status changes.
 *
 * @details Invoked whenever the WiFi connection status changes. Register via
 *          `onConnectionStatus()`. Use to update UI, log events, or trigger
 *          application-specific actions based on connectivity state.
 *
 * @param status New connection status value.
 */
typedef std::function<void(AutoNetworkConnectionStatus)> AutoNetworkOnConnectionStatusCallback;

/**
 * @brief Callback function type for captive portal state changes.
 *
 * @details Invoked whenever the portal state machine transitions to a new state.
 *          Register via `onPortalState()`. Use to track portal lifecycle and
 *          connection progress.
 *
 * @param state New portal state value.
 */
typedef std::function<void(AutoNetworkPortalState)> AutoNetworkOnPortalStateCallback;

/**
 * @brief Callback function type for custom configuration validation.
 *
 * @details Invoked after user submits custom parameter values via portal. Register
 *          via `onConfig()`. Return `true` to accept configuration, `false` to reject
 *          and keep portal open for corrections.
 *
 * @return bool
 * @retval true Configuration accepted, proceed with connection.
 * @retval false Configuration rejected, keep portal open.
 */
typedef std::function<bool()> AutoNetworkOnConfigCallback;

/**
 * @brief Callback function type for first webpage access detection.
 *
 * @details Invoked when a user first accesses any AutoNetwork webpage after connecting
 *          to the portal AP. Register via `onWebpageAccessed()`. Useful for triggering
 *          UI transitions from setup mode to runtime mode.
 *
 * @par Parameters
 *      None.
 *
 * @par Returns
 *      Nothing.
 */
typedef std::function<void()> AutoNetworkOnWebpageAccessedCallback;

// Class Declaration
// ****************************************************************************

/**
 * @brief AutoNetwork WiFi connection manager with captive portal.
 *
 * @details This class provides comprehensive WiFi connection management for ESP32 devices.
 *          It handles automatic connection to saved networks, captive portal configuration,
 *          multi-credential storage with priority ordering, WPA2 Enterprise support, and
 *          visual status feedback via LED ticker.
 *
 *          The library requires periodic loop() calls to process connection events and
 *          portal operations. The captive portal provides a
 *          user-friendly web interface for WiFi configuration accessible from any
 *          smartphone or computer.
 *
 * @par Key Features:
 *      - **Automatic Connection:** Tries saved credentials in priority order on boot
 *      - **Captive Portal:** User-friendly web interface for WiFi configuration
 *      - **Multi-Credential Storage:** Store up to 255 networks with priority ordering
 *      - **WPA2 Enterprise:** PEAP/MSCHAPv2 authentication support
 *      - **Async Web Server:** Non-blocking HTTP server (ESPAsyncWebServer)
 *      - **LED Status Ticker:** Visual connection status via configurable GPIO
 *      - **OTA Updates:** Over-the-air firmware update interface
 *      - **Custom Parameters:** Add custom configuration fields to portal
 *      - **Background Reconnection:** Auto-reconnect on connection loss
 *      - **Comprehensive Callbacks:** Event-driven architecture
 *
 * @par Basic Usage:
 * @code{.cpp}
 * #include <AutoNetwork.h>
 *
 * AsyncWebServer server(80);
 * AutoNetwork autoNetwork(&server);
 *
 * void setup() {
 *     AutoNetworkConfig config;
 *     config.apSSID = "ESP32-Setup";
 *     config.tickerEnable = true;
 *     autoNetwork.config(config);
 *
 *     autoNetwork.onConnectionStatus([](AutoNetworkConnectionStatus status) {
 *         Serial.printf("Status: %d\n", (int)status);
 *     });
 *
 *     autoNetwork.begin();
 * }
 *
 * void loop() {
 *     autoNetwork.loop();
 * }
 * @endcode
 *
 * @note This class requires an existing web server instance to be provided in the constructor.
 * @warning Always call `loop()` in the main Arduino loop for proper operation.
 */
class AutoNetwork
{
public:
    /**
     * @brief Construct a new AutoNetwork object.
     *
     * @details Initializes the AutoNetwork WiFi manager with a reference to an existing
     *          web server instance. The web server is not owned by AutoNetwork and must
     *          remain valid for the lifetime of the AutoNetwork object.
     *
     * @param [in] server Pointer to AsyncWebServer instance.
     *
     * @note The web server should be created before constructing AutoNetwork.
     * @note The server is not started automatically - call `begin()` to start.
     */
    AutoNetwork(AsyncWebServer *server);

    // Authentication Methods
    // ========================================================================

    /**
     * @brief Set HTTP authentication credentials for portal access (String version).
     *
     * @param [in] username HTTP authentication username.
     * @param [in] password HTTP authentication password.
     */
    void setAuthentication(String &username, String &password);

    /**
     * @brief Set HTTP authentication credentials for portal access.
     *
     * @details Enables HTTP Basic/Digest authentication for the captive portal.
     *          Users must provide these credentials to access portal configuration.
     *
     * @param [in] username HTTP authentication username.
     * @param [in] password HTTP authentication password.
     *
     * @note Both username and password must be non-empty to enable authentication.
     */
    void setAuthentication(const char *username, const char *password);

    // Configuration Methods
    // ========================================================================

    /**
     * @brief Apply comprehensive configuration settings.
     *
     * @details Applies all settings from an `AutoNetworkConfig` object including AP settings,
     *          station settings, portal behavior, ticker configuration, and HTTP authentication.
     *
     * @param [in] cfg Reference to `AutoNetworkConfig` object with desired settings.
     *
     * @note Configuration is applied immediately but does not start portal or connection.
     */
    void config(AutoNetworkConfig &cfg);

    /**
     * @brief Get current configuration.
     *
     * @par Parameters
     *      None.
     *
     * @par Returns
     *      Reference to current AutoNetworkConfig.
     */
    const AutoNetworkConfig& getConfig() const;

    /**
     * @brief Set mDNS hostname (C-string version).
     *
     * @param [in] hostname mDNS hostname for the device.
     */
    void setHostname(const char *hostname);

    /**
     * @brief Set mDNS hostname (String version).
     *
     * @param [in] hostname mDNS hostname for the device.
     */
    void setHostname(String &hostname);

    /**
     * @brief Set WiFi connection timeout.
     *
     * @param [in] timeoutMs Timeout in milliseconds (default: 30000ms).
     */
    void setConnectTimeout(uint32_t timeoutMs);

    /**
     * @brief Set captive portal timeout.
     *
     * @param [in] timeoutMs Timeout in milliseconds (0 = infinite).
     */
    void setPortalTimeout(uint32_t timeoutMs);

    // Logging Configuration Methods
    // ========================================================================

    /**
     * @brief Set the logging level for AutoNetwork library diagnostics.
     *
     * @details Controls verbosity of library logging output. Use this to control
     *          how much diagnostic information the library outputs to the serial
     *          console. Lower levels are quieter, higher levels provide more detail.
     *
     * @param [in] level Desired logging level:
     *                   - AN_LOG_NONE: No logging (silent)
     *                   - AN_LOG_ERROR: Only critical errors
     *                   - AN_LOG_WARN: Warnings and errors (default)
     *                   - AN_LOG_INFO: Informational messages + warnings + errors
     *                   - AN_LOG_DEBUG: Debug details + all above
     *                   - AN_LOG_VERBOSE: Everything including repetitive loop() calls
     *
     * @note Default is AN_LOG_WARN (errors and warnings only)
     * @note Can also be set via `AutoNetworkConfig::logLevel`
     *
     * @par Usage Example:
     * @code{.cpp}
     * // Silence library logging
     * autoNetwork.setLogLevel(AN_LOG_NONE);
     *
     * // Enable verbose logging for debugging
     * autoNetwork.setLogLevel(AN_LOG_VERBOSE);
     * @endcode
     */
    static void setLogLevel(AutoNetworkLogLevel level);

    /**
     * @brief Get the current logging level.
     *
     * @return AutoNetworkLogLevel Current logging level.
     */
    static AutoNetworkLogLevel getLogLevel();

    /**
     * @brief Enable verbose logging (AN_LOG_VERBOSE).
     *
     * @details Convenience method to enable maximum verbosity including repetitive
     *          loop() calls and detailed operational messages. Useful for debugging
     *          connection issues or understanding library behavior.
     */
    static void enableVerboseLogging();

    /**
     * @brief Disable all logging (AN_LOG_NONE).
     *
     * @details Convenience method to completely silence library logging output.
     *          Use when you want a clean serial console showing only your
     *          application's logs.
     */
    static void disableLogging();

    /**
     * @brief Set default logging level (AN_LOG_WARN).
     *
     * @details Convenience method to restore the default logging behavior which
     *          shows only warnings and errors. This is the recommended setting
     *          for production use.
     */
    static void setDefaultLogging();

    // Ticker Configuration Methods
    // ========================================================================

    /**
     * @brief Enable or disable LED status ticker.
     *
     * @param [in] enable True to enable ticker, false to disable (default: true).
     */
    void enableTicker(bool enable = true);

    /**
     * @brief Set GPIO pin for LED ticker.
     *
     * @param [in] port GPIO pin number (e.g., `LED_BUILTIN`).
     */
    void setTickerPort(uint8_t port);

    /**
     * @brief Set LED active level (HIGH or LOW).
     *
     * @param [in] activeLevel `HIGH` for active-high, `LOW` for active-low.
     */
    void setTickerOn(uint8_t activeLevel);

    // Credential Methods
    // ========================================================================

    /**
     * @brief Set WiFi credentials programmatically.
     *
     * @details Stores credentials without invoking portal or connection. Use `connect()`
     *          after this to initiate connection.
     *
     * @param [in] ssid WiFi network SSID.
     * @param [in] password WiFi network password.
     */
    void setCredentials(const char *ssid, const char *password);

    // Root Content Configuration Methods
    // ========================================================================

    /**
     * @brief Configure root page content from LittleFS file.
     *
     * @param [in] filePath Path to HTML file in LittleFS.
     */
    void setRootContent(const String& filePath);

    /**
     * @brief Configure root page content from callback function.
     *
     * @param [in] callback Function returning HTML content as String.
     */
    void setRootContent(std::function<String()> callback);

    /**
     * @brief Configure root page content from direct HTML string.
     *
     * @param [in] htmlContent HTML content as C-string.
     */
    void setRootContentHTML(const char* htmlContent);

    /**
     * @brief Set custom menu link replacement for root page.
     *
     * @param [in] replacement HTML for menu link.
     */
    void setRootMenuReplacement(const String& replacement);

    // Connection Methods
    // ========================================================================

    /**
     * @brief Connect to saved credentials or start captive portal.
     *
     * @details Primary method for establishing WiFi connectivity. Attempts to connect
     *          to saved credentials in priority order. If all fail or none exist, starts
     *          captive portal for user configuration.
     *
     * @param [in] ssid Access Point SSID for captive portal.
     * @param [in] password Access Point password (empty for open network).
     *
     * @note This method returns immediately - call `loop()` for connection progress.
     */
    void autoConnect(const char *ssid, const char *password);

    // Callback Registration Methods
    // ========================================================================

    /**
     * @brief Register callback for WiFi connection status changes.
     *
     * @param [in] callback Function invoked when connection status changes.
     */
    void onConnectionStatus(AutoNetworkOnConnectionStatusCallback callback);

    /**
     * @brief Register callback for captive portal state changes.
     *
     * @param [in] callback Function invoked when portal state changes.
     */
    void onPortalState(AutoNetworkOnPortalStateCallback callback);

    /**
     * @brief Register callback for custom configuration validation.
     *
     * @param [in] callback Function invoked after user submits custom parameters.
     */
    void onConfig(AutoNetworkOnConfigCallback callback);

    /**
     * @brief Register callback for first webpage access.
     *
     * @param [in] callback Function invoked when user first accesses portal webpage.
     */
    void onWebpageAccessed(std::function<void()> callback);

    // Status Query Methods
    // ========================================================================

    /**
     * @brief Check if WiFi credentials are configured.
     *
     * @return bool
     * @retval true Credentials configured.
     * @retval false No credentials configured.
     */
    bool isConfigured();

    /**
     * @brief Get current WiFi connection status.
     *
     * @return AutoNetworkConnectionStatus Current status.
     */
    AutoNetworkConnectionStatus getConnectionStatus();

    /**
     * @brief Get current captive portal state.
     *
     * @return AutoNetworkPortalState Current portal state.
     */
    AutoNetworkPortalState getPortalState();

    /**
     * @brief Get connected WiFi network SSID.
     *
     * @return const char* SSID string.
     */
    const char *getSSID();

    /**
     * @brief Get WiFi network password.
     *
     * @return const char* Password string.
     */
    const char *getPassword();

    /**
     * @brief Get WiFi network BSSID (MAC address).
     *
     * @param [out] bssid Buffer to receive 6-byte BSSID.
     */
    void getBSSID(uint8_t *bssid);

    /**
     * @brief Get WiFi channel number.
     *
     * @return uint8_t Channel number (1-13).
     */
    uint8_t getChannel();

    /**
     * @brief Get device IP address.
     *
     * @return IPAddress Local IP address.
     */
    IPAddress localIP();

    /**
     * @brief Get gateway IP address.
     *
     * @return IPAddress Gateway IP.
     */
    IPAddress gatewayIP();

    /**
     * @brief Get subnet mask.
     *
     * @return IPAddress Subnet mask.
     */
    IPAddress subnetMask();

    // Connection Control Methods
    // ========================================================================

    /**
     * @brief Connect using saved credentials.
     *
     * @return bool
     * @retval true Connection succeeded.
     * @retval false Connection failed.
     */
    bool connect();

    /**
     * @brief Connect to specified network.
     *
     * @param [in] ssid Network SSID.
     * @param [in] password Network password.
     *
     * @return bool
     * @retval true Connection succeeded.
     * @retval false Connection failed.
     */
    bool connect(const char *ssid, const char *password);

    /**
     * @brief Erase all saved credentials from NVS.
     *
     * @par Parameters
     *      None.
     *
     * @par Returns
     *      Nothing.
     */
    void erase();

    /**
     * @brief Disconnect from WiFi network.
     *
     * @par Parameters
     *      None.
     *
     * @par Returns
     *      Nothing.
     */
    void disconnect();

    /**
     * @brief Reset device (soft reset).
     *
     * @par Parameters
     *      None.
     *
     * @par Returns
     *      Nothing.
     */
    void reset();

    /**
     * @brief Process AutoNetwork tasks and state machine.
     *
     * @details This method MUST be called repeatedly from the main Arduino `loop()`
     *          function to process AutoNetwork tasks including WiFi monitoring, portal
     *          state machine, DNS requests, background reconnection, and callback invocation.
     *
     * @par Parameters
     *      None.
     *
     * @par Returns
     *      Nothing.
     *
     * @warning Failure to call `loop()` results in non-functional WiFi management.
     */
    void loop();

    // Portal Control Methods
    // ========================================================================

    /**
     * @brief Start captive portal manually.
     *
     * @par Parameters
     *      None.
     *
     * @par Returns
     *      Nothing.
     */
    void startPortal();

    /**
     * @brief Stop captive portal manually.
     *
     * @par Parameters
     *      None.
     *
     * @par Returns
     *      Nothing.
     */
    void stopPortal();

    // Configuration Parameter Methods
    // ========================================================================

    /**
     * @brief Generate next unique parameter ID.
     *
     * @return uint32_t Next available parameter ID.
     */
    uint32_t nextId();

    /**
     * @brief Add custom parameter to portal.
     *
     * @param [in] parameter Pointer to `AutoNetworkParameter` to add.
     */
    void addParameter(AutoNetworkParameter *parameter);

    /**
     * @brief Remove custom parameter from portal.
     *
     * @param [in] parameter Pointer to `AutoNetworkParameter` to remove.
     */
    void removeParameter(AutoNetworkParameter *parameter);

    // Credential Management Methods
    // ========================================================================

    /**
     * @brief Get credential manager instance.
     *
     * @return AutoNetworkCredential* Pointer to credential manager.
     */
    AutoNetworkCredential *credential();

    // Convenience Methods
    // ========================================================================

    /**
     * @brief Start WiFi connection manager.
     *
     * @details Convenience method that calls `autoConnect()` with default AP settings
     *          from configuration. Attempts connection to saved credentials or starts portal.
     *
     * @return bool
     * @retval true Connected successfully.
     * @retval false Connection failed or portal started.
     */
    bool begin();

    /**
     * @brief Stop AutoNetwork and release resources.
     *
     * @par Parameters
     *      None.
     *
     * @par Returns
     *      Nothing.
     */
    void end();


    /**
     * @brief Get web server reference.
     *
     * @return AsyncWebServer* Pointer to web server instance.
     */
    AsyncWebServer *host();

    /**
     * @brief Check if captive portal is currently active.
     *
     * @return bool
     * @retval true Portal is active.
     * @retval false Portal is not active.
     */
    bool isPortalAvailable();

    /**
     * @brief Get current configuration object.
     *
     * @return AutoNetworkConfig& Reference to configuration.
     */
    AutoNetworkConfig &getConfig();

    /**
     * @brief Save current credentials to NVS flash.
     *
     * @par Parameters
     *      None.
     *
     * @par Returns
     *      Nothing.
     */
    void saveCredential();

    /**
     * @brief Restore credentials from NVS flash.
     *
     * @par Parameters
     *      None.
     *
     * @par Returns
     *      Nothing.
     */
    void restoreCredential();

    // Web Server Delegation Methods
    // ========================================================================

    /**
     * @brief Register custom HTTP endpoint (AsyncWebServer version).
     *
     * @param [in] uri Endpoint URI path.
     * @param [in] method HTTP method(s) allowed.
     * @param [in] onRequest Request handler function.
     *
     * @return AsyncCallbackWebHandler& Handler reference.
     */
    AsyncCallbackWebHandler &on(const char *uri, WebRequestMethodComposite method,
                                ArRequestHandlerFunction onRequest);

    /**
     * @brief Destroy the AutoNetwork object.
     *
     * @details Performs cleanup of allocated resources including DNS server, portal,
     *          ticker, and HTTP handlers.
     *
     * @par Parameters
     *      None.
     *
     * @par Returns
     *      Nothing.
     */
    ~AutoNetwork();

private:
    // Private member variables
    AsyncWebServer *_server;
    AutoNetworkCredential _credential;           // Multi-credential storage
    AutoNetworkCredentialManager _credentialMgr; // High-level credential operations
    AutoNetworkConnectionManager *_connectionMgr = nullptr; // WiFi connection manager
    AutoNetworkScanManager _scanManager;         // WiFi scan operations manager
    AutoNetworkTicker *_ticker = nullptr;        // WiFi status LED ticker
    AutoNetworkPortal *_portal = nullptr;        // Portal manager (new refactored architecture)
    AutoNetworkConfig _config;                   // Configuration storage
    AsyncWebHandler *_index_handler = nullptr;
    AsyncWebHandler *_status_handler = nullptr;
    AsyncWebHandler *_schema_handler = nullptr;
    AsyncWebHandler *_scan_handler = nullptr;
    AsyncWebHandler *_save_handler = nullptr;
    AsyncWebHandler *_clear_handler = nullptr;
    AsyncWebHandler *_exit_handler = nullptr;
    bool _serverRunning = false;
    DNSServer *_dns = nullptr;
    bool _dnsRunning = false;
    
    // Hostname management
    String _pendingHostname;  // Hostname to apply when STA mode is ready

    // WiFi disconnection and reconnection tracking
    bool _established = false;        // Track if WiFi was previously connected
    unsigned long _attemptPeriod = 0; // Timestamp for reconnection attempt tracking

    // Pre-allocated buffer for background reconnection (eliminates heap fragmentation)
    AutoNetworkCredentialEntry _reconnectBuffer[AUTONETWORK_MAX_RECONNECT_ENTRIES];

    // Network configuration and state structure
    struct
    {
        AutoNetworkConnectionStatus status = AutoNetworkConnectionStatus::DISCONNECTED;
        AutoNetworkOnConnectionStatusCallback status_cb = nullptr;
        String hostname = "autonetwork";
        bool webpageAccessed = false;
        AutoNetworkOnWebpageAccessedCallback webpageAccessed_cb = nullptr; // New callback member

        // STA (station) parameters
        struct
        {
            bool configured = false;
            unsigned long timeout = AUTONETWORK_CONNECT_TIMEOUT;
            String ssid = "";
            String password = "";

            // WPA2 Enterprise fields
            bool enterprise = false;
            String enterpriseNetId = "";
            // uint8_t bssid[6];
            // uint8_t channel = 0;
        } sta;

        // Ticker configuration
        struct
        {
            bool enabled = false;
            uint8_t port;
            uint8_t activeLevel;
        } ticker;

        // Root content configuration
        String rootContentPath;                          // File path if using file-based content
        std::function<String()> rootContentCallback;     // Callback if using dynamic content
        String rootContentDirect;                        // Direct HTML if using string content
        enum class RootContentType { NONE, FILE, CALLBACK, DIRECT } rootContentType = RootContentType::NONE;
        String rootMenuReplacement;                      // Menu link HTML (defaults to hamburger icon)
    } _an;

    // Private helper functions
    void _connect(const char* ssid, const char* password, bool autoreconnect = false, const AutoNetworkCredentialEntry* credential = nullptr);
    bool _connectEnterprise(const char* ssid, const char* netid, const char* password);
    void _disconnect();
    bool _isIp(String str);
    WiFiMode_t _determineWiFiMode() const;  // Helper for mode switching decision logic

    // HTTP/JSON functions
    void _generateStatusJson(String &str);
    void _generateSchemaJson(String &str);
    void _generateScanJson(String &str);
    bool _parseConfigJson(JsonArray &arr);
    bool _parseCredentialsJson(JsonObject &obj);

    // HTTP server functions
    void _startHTTP();
    void _stopHTTP();

    // Portal functions
    void _startPortal();
    void _stopPortal();

    // Ticker functions
    void _startTicker();
    void _stopTicker();
    void _updateTicker();

    // Timestamp functions
    unsigned long _getMonotonicTimestamp();

    // Loop extraction functions (Task 2.1)
    void _updateConnectionStatus();
    void _processTickerAnimation();
    void _handleWebServerRequests();
    void _monitorWiFiConnection();
    void _processBackgroundReconnection();
    void _restartScan();

    // Root content helper functions
    void _registerRootHandler();
    String _getRootContent();
    bool _isContentEmpty(const String& content);

    // mDNS functions
    /**
     * @brief Start mDNS responder with configured hostname.
     *
     * @par Parameters
     *      None.
     *
     * @par Returns
     *      Nothing.
     */
    void _startMDNS();

    /**
     * @brief Stop mDNS responder.
     *
     * @par Parameters
     *      None.
     *
     * @par Returns
     *      Nothing.
     */
    void _stopMDNS();

    /**
     * @brief Set WiFi hostname with 4-layer defensive validation.
     *
     * @details Ensures WiFi.setHostname() called with proper ordering and validation:
     *          - Layer 1: Check WiFi mode is set (not NULL)
     *          - Layer 2: Validate hostname value and mode
     *          - Layer 3: Handle test environment differences
     *          - Layer 4: Log full state transitions
     *
     * @param [in] hostname Hostname to set (null-terminated string)
     *
     * @par Returns
     *      Nothing.
     */
    void _setHostname(const char* hostname);
    
    /**
     * @brief Apply pending hostname if STA mode is now active.
     * 
     * @details Called internally when WiFi mode changes to STA or AP_STA.
     *          If a hostname was set before WiFi initialization, this method
     *          applies it once the appropriate mode is active.
     *
     * @par Parameters
     *      None.
     *
     * @par Returns
     *      Nothing.
     */
    void _applyPendingHostname();

protected:
    static bool _onAPFilter(AsyncWebServerRequest *request);
};
