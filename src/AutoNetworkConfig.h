/*!
 * @file AutoNetworkConfig.h
 *
 * @brief Configuration container for AutoNetwork WiFi manager.
 *
 * @details This header defines the configuration class and credential save mode enumeration
 *          for AutoNetwork. All settings are exposed as public member variables for easy
 *          configuration. Settings take effect when applied via `AutoNetwork::config()`.
 *
 * @section revision_history Revision History
 *
 * | Date | Author | Description |
 * |------|--------|-------------|
 * | 2025-10-03 | Brooks | Initial implementation |
 * | 2025-10-24 | Brooks | Added comprehensive Doxygen documentation |
 */

#pragma once

#include "Arduino.h"
#include "AutoNetworkLog.h"
#include "esp_mac.h"

// Credential Save Mode Enumeration
// ****************************************************************************

/**
 * @brief Credential save mode enumeration.
 *
 * @details Determines when WiFi credentials submitted via the captive portal
 *          are saved to ESP32 NVS (non-volatile storage). Provides control over
 *          when credentials become persistent across device reboots.
 */
enum class AutoNetworkCredentialSaveMode : uint8_t
{
    AUTO = 0,   /**< Save only after successful WiFi connection (default, safest - prevents invalid credentials) */
    ALWAYS = 1, /**< Save immediately upon portal submission, regardless of connection success */
    NEVER = 2   /**< Never save automatically (requires manual save via `saveCredential()`) */
};

// Configuration Class
// ****************************************************************************

/**
 * @brief Configuration container for AutoNetwork settings.
 *
 * @details This class holds all configuration settings as public member variables.
 *          Create an instance, set desired values, then apply via `AutoNetwork::config()`.
 *          Settings only take effect when the configuration is applied - they do not
 *          automatically trigger connection or portal startup.
 *
 * @par Configuration Categories:
 *      - **Access Point:** Portal AP SSID, password, channel, visibility
 *      - **Station:** Hostname, reconnection behavior, signal thresholds
 *      - **Portal:** Timeout, retention after connection
 *      - **Ticker:** LED status indicator settings
 *      - **Authentication:** HTTP portal security credentials
 *      - **DNS:** Captive portal DNS server control
 *      - **Credentials:** Storage limits and save behavior
 *
 * @par Usage Example:
 * @code{.cpp}
 * AutoNetworkConfig cfg;
 * cfg.apSSID = "ESP32-Setup";
 * cfg.apPassword = "12345678";
 * cfg.staHostName = "my-device";
 * cfg.tickerEnable = true;
 * cfg.portalRetain = false;
 * cfg.timeoutPortalMs = 300000; // 5 minutes
 * autoNetwork.config(cfg);
 * @endcode
 *
 * @note All member variables have sensible defaults in the constructor.
 * @note Configuration is applied immediately but does not trigger connection.
 */
class AutoNetworkConfig
{
public:
    // Access Point Configuration
    // ========================================================================

    /**
     * @brief Access Point SSID for captive portal.
     *
     * @details SSID broadcasted when captive portal is active. Users connect to this
     *          network to access the WiFi configuration interface.
     *
     * @note Default: "ESP32_{MAC_ADDRESS}" (generated from device MAC address in constructor)
     * @note Fallback: "ESP32_AP_ERROR" if MAC read fails (hardware fault)
     */
    String apSSID = "AutoNetwork";  // Overwritten in constructor

    /**
     * @brief Access Point password for captive portal.
     *
     * @details Password required to connect to portal AP. Empty string creates an open
     *          network (no password required). Minimum 8 characters if set.
     *
     * @note Default: "" (open network)
     */
    String apPassword = "";

    /**
     * @brief Access Point WiFi channel.
     *
     * @details WiFi channel for portal AP (1-13 for 2.4GHz). Channel 1, 6, and 11
     *          are recommended to minimize interference.
     *
     * @note Default: 1
     */
    uint8_t apChannel = 1;

    /**
     * @brief Access Point visibility.
     *
     * @details Controls whether AP SSID is broadcasted. 0 = visible, 1 = hidden.
     *
     * @note Default: 0 (visible)
     */
    uint8_t apHidden = 0;

    // Station (STA) Configuration
    // ========================================================================

    /** @brief mDNS hostname for the device. Default: "autonetwork" */
    String staHostName = "autonetwork";

    /** @brief Enable automatic reconnection on connection loss. Default: true */
    bool staAutoReconnect = true;

    /**
     * @brief Reconnection interval multiplier.
     *
     * @details Interval in UNITTIME (30s) units. 0 = disable, 1 = 30s, 2 = 60s, etc.
     *          Actual interval = `staReconnectInterval × 30 seconds`.
     *
     * @note Default: 1 (30 seconds)
     */
    uint8_t staReconnectInterval = 1;

    /** @brief Auto-launch portal on WiFi disconnection. Default: true */
    bool staAutoRise = true;

    /**
     * @brief Maintain AP mode during STA connection.
     *
     * @details Keeps Access Point active while connected to WiFi station.
     *          Useful for ESP-MESH or ESP-NOW applications requiring simultaneous AP/STA mode.
     *
     * @note Default: false
     */
    bool staPreserveAPMode = false;

    // Connection Behavior Configuration
    // ========================================================================

    /**
     * @brief Match credentials by BSSID or SSID only.
     *
     * @details If true, credentials match specific AP by MAC address (BSSID).
     *          If false (default), credentials match any AP with same SSID.
     *
     * @note Default: false (match by SSID only)
     */
    bool staMatchBSSID = false;

    /**
     * @brief Minimum RSSI signal strength threshold.
     *
     * @details Networks with signal strength below this threshold are ignored.
     *          -120 dBm = no limit (accept any signal strength).
     *
     * @note Default: -120 (no limit)
     */
    int16_t staMinRSSI = -120;

    // Portal Configuration
    // ========================================================================

    /** @brief Keep portal running after successful connection. Default: false */
    bool portalRetain = false;

    /**
     * @brief Portal timeout in milliseconds.
     *
     * @details 0 = infinite (portal stays active indefinitely).
     *          Non-zero = portal closes after timeout if no activity.
     *
     * @note Default: 0 (infinite)
     */
    uint32_t timeoutPortalMs = 0;

    /** @brief WiFi connection timeout in milliseconds. Default: 30000 (30 seconds) */
    uint32_t timeoutConnectMs = 30000;

    // Ticker Configuration
    // ========================================================================

    /** @brief Enable WiFi status LED ticker. Default: false */
    bool tickerEnable = false;

    /** @brief GPIO pin for ticker LED. Default: `LED_BUILTIN` */
    uint8_t tickerPin = LED_BUILTIN;

    /** @brief LED active level (LOW or HIGH). Default: LOW */
    uint8_t tickerActiveLevel = LOW;

    // Authentication Configuration
    // ========================================================================

    /** @brief HTTP auth type ("BASIC", "DIGEST", or empty). Default: "" (disabled) */
    String authType = "";

    /** @brief HTTP authentication username. Default: "" */
    String authUsername = "";

    /** @brief HTTP authentication password. Default: "" */
    String authPassword = "";

    // DNS Configuration
    // ========================================================================

    /** @brief Start DNS server with captive portal. Default: true */
    bool dnsEnable = true;

    // Logging Configuration
    // ========================================================================

    /**
     * @brief Logging level for AutoNetwork library diagnostics.
     *
     * @details Controls verbosity of library logging output:
     *          - AN_LOG_NONE: No logging (silent)
     *          - AN_LOG_ERROR: Only critical errors
     *          - AN_LOG_WARN: Warnings and errors (default)
     *          - AN_LOG_INFO: Informational messages + warnings + errors
     *          - AN_LOG_DEBUG: Debug details + all above
     *          - AN_LOG_VERBOSE: Everything including repetitive loop() calls
     *
     * @note Default: AN_LOG_WARN
     * @note Can also be set at runtime via `AutoNetwork::setLogLevel()`
     */
    AutoNetworkLogLevel logLevel = AN_LOG_WARN;

    // Web Server Configuration
    // ========================================================================

    /** @brief Enable async web server (ESPAsyncWebServer). Default: true */
    bool serverAsync = true;

    // Credential Management Configuration
    // ========================================================================

    /**
     * @brief Maximum number of stored credentials.
     *
     * @details Limits credential storage in ESP32 NVS. Range: 1-255.
     *
     * @note Default: 10
     */
    uint8_t credentialsMax = 10;

    /**
     * @brief When to save credentials to NVS.
     *
     * @details Controls credential persistence behavior:
     *          - AUTO: Save only after successful connection (safest)
     *          - ALWAYS: Save immediately upon submission
     *          - NEVER: Never save automatically
     *
     * @note Default: AUTO
     */
    AutoNetworkCredentialSaveMode credentialSaveMode = AutoNetworkCredentialSaveMode::AUTO;

    /**
     * @brief Construct a new AutoNetworkConfig object with default values.
     *
     * @details All member variables are initialized with sensible defaults.
     *          Modify as needed before applying via `AutoNetwork::config()`.
     *
     * @par Parameters
     *      None.
     *
     * @par Returns
     *      Nothing (constructor).
     */
    AutoNetworkConfig()
    {
        // Generate unique MAC-based default SSID
        uint8_t mac[6];
        esp_err_t err = esp_efuse_mac_get_default(mac);
        
        if (err == ESP_OK)
        {
            // Convert MAC bytes to hex string (12 characters, uppercase, no colons)
            // Format: ESP32_AABBCCDDEEFF
            char macStr[13];  // 12 hex chars + null terminator
            snprintf(macStr, sizeof(macStr), "%02X%02X%02X%02X%02X%02X",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            apSSID = "ESP32_" + String(macStr);
        }
        else
        {
            // eFuse MAC read failed - use error SSID for debugging
            apSSID = "ESP32_AP_ERROR";
        }
        
        // All other defaults already set in member initializers above
    }
};
