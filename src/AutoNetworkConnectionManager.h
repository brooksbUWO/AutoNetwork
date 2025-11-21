// ****************************************************************************
// Title        : AutoNetwork Library - Connection Manager
// Filename     : 'AutoNetworkConnectionManager.h'
// Target MCU   : Espressif ESP32 (Doit DevKit Version 1)
// Description  : WiFi connection management functionality extracted from AutoNetwork
//
// Revision History:
// When         Who         Description of change
// -----------  ----------- -----------------------
// 14-NOV-2025  Brooks      Initial extraction from AutoNetwork
//
// ****************************************************************************

#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <IPAddress.h>
#include "AutoNetworkConstants.h"
#include "AutoNetworkLog.h"

// Forward declarations
class AutoNetworkPortal;
class AutoNetworkCredential;
struct AutoNetworkCredentialEntry;
enum class AutoNetworkConnectionStatus : uint8_t;  // Defined in AutoNetwork.h

/**
 * @brief Manages WiFi connection operations for AutoNetwork.
 *
 * @details This class encapsulates all WiFi connection management functionality
 *          including standard WPA/WPA2 connections, WPA2 Enterprise connections,
 *          disconnection, and connection monitoring. It maintains connection state
 *          and provides status information.
 *
 * @par Key Responsibilities
 * - Execute WiFi connection attempts (standard and enterprise)
 * - Apply static IP configuration when specified
 * - Monitor WiFi connection state changes
 * - Manage WiFi disconnection
 * - Track connection establishment state
 *
 * @par Usage Pattern
 * @code{.cpp}
 * AutoNetworkConnectionManager connectionMgr(portal, credential, status, established);
 *
 * // Connect to standard WiFi network
 * connectionMgr.connect("MySSID", "MyPassword");
 *
 * // Monitor connection in loop
 * void loop() {
 *     connectionMgr.monitorConnection();
 *     if (connectionMgr.isConnected()) {
 *         IPAddress ip = connectionMgr.getLocalIP();
 *         Serial.printf("Connected: %s\n", ip.toString().c_str());
 *     }
 * }
 * @endcode
 *
 * @note This class does not own the portal or credential objects - it only
 *       maintains references to them for querying connection parameters.
 *
 * @see AutoNetworkScanManager
 * @see AutoNetworkCredentialManager
 */
class AutoNetworkConnectionManager
{
public:
    /**
     * @brief Construct a new AutoNetworkConnectionManager.
     *
     * @details Creates a connection manager that operates on the provided portal
     *          and credential instances. The status and established parameters are
     *          references that will be updated as connection state changes.
     *
     * @param [in] portal Portal instance providing enterprise mode, channel, BSSID info.
     * @param [in] credential Credential storage for updating lastUsed timestamps.
     * @param [in,out] status Reference to status enum that will be updated by the manager.
     * @param [in,out] established Reference to flag tracking connection establishment.
     *
     * @note All pointer and reference parameters must remain valid for the lifetime
     *       of the ConnectionManager object.
     */
    AutoNetworkConnectionManager(
        AutoNetworkPortal *portal,
        AutoNetworkCredential *credential,
        AutoNetworkConnectionStatus &status,
        bool &established);

    /**
     * @brief Default destructor.
     */
    ~AutoNetworkConnectionManager() = default;

    /**
     * @brief Connect to WiFi network using SSID and password.
     *
     * @details Initiates standard WPA/WPA2 connection. Checks portal for enterprise
     *          mode and delegates to enterprise connection if required. Supports
     *          channel and BSSID optimization when provided by portal configuration.
     *
     * @param [in] ssid C-string containing network SSID (must not be null).
     * @param [in] password C-string containing network password (must not be null).
     * @param [in] autoReconnect Enable WiFi auto-reconnect feature.
     * @param [in] credential Optional pointer to credential with static IP configuration.
     *
     * @par Returns
     *      Nothing.
     *
     * @par Side Effects
     * - Updates connection status to CONNECTING
     * - Calls WiFi.begin() with appropriate parameters
     * - May apply static IP configuration if credential provided
     * - May delegate to `connectEnterprise()` if portal is in enterprise mode
     *
     * @par Usage Example:
     * @code{.cpp}
     * // Basic connection
     * connectionMgr.connect("MyWiFi", "password123");
     *
     * // With auto-reconnect enabled
     * connectionMgr.connect("MyWiFi", "password123", true);
     *
     * // With static IP configuration
     * AutoNetworkCredentialEntry entry;
     * entry.useStaticIP = true;
     * entry.staticIP.fromString("192.168.1.100");
     * entry.staticGateway.fromString("192.168.1.1");
     * entry.staticSubnet.fromString("255.255.255.0");
     * connectionMgr.connect("MyWiFi", "password123", false, &entry);
     * @endcode
     *
     * @see connectEnterprise()
     * @see disconnect()
     * @see monitorConnection()
     */
    void connect(
        const char *ssid,
        const char *password,
        bool autoReconnect = false,
        const AutoNetworkCredentialEntry *credential = nullptr);

    /**
     * @brief Connect to WPA2 Enterprise network.
     *
     * @details Initiates WPA2 Enterprise connection using PEAP/MSCHAPv2 with
     *          provided NetID (username) and password. Only supported on ESP32
     *          platforms with ESP-IDF WPA2 Enterprise support.
     *
     * @param [in] ssid C-string containing enterprise network SSID.
     * @param [in] netid C-string containing network ID/username.
     * @param [in] password C-string containing authentication password.
     *
     * @return bool
     * @retval true Connection initiated successfully.
     * @retval false WPA2 Enterprise not supported on this platform.
     *
     * @par Side Effects
     * - Disconnects from current WiFi
     * - Sets WiFi mode to AP_STA
     * - Configures WPA2 Enterprise credentials
     * - Updates connection status to CONNECTING
     *
     * @par Usage Example:
     * @code{.cpp}
     * // Connect to university or corporate WPA2 Enterprise network
     * if (connectionMgr.connectEnterprise("eduroam", "john.doe", "password123")) {
     *     Serial.println("Enterprise connection initiated");
     * } else {
     *     Serial.println("WPA2 Enterprise not supported");
     * }
     * @endcode
     *
     * @note Only available on ESP32 platform.
     * @note Requires ESP-IDF WPA2 Enterprise functions.
     *
     * @see connect()
     */
    bool connectEnterprise(
        const char *ssid,
        const char *netid,
        const char *password);

    /**
     * @brief Disconnect from WiFi network.
     *
     * @details Disconnects STA interface while preserving AP configuration.
     *          Does not erase saved credentials or AP settings. The WiFi mode
     *          remains unchanged to preserve any active Access Point.
     *
     * @par Parameters
     *      None.
     *
     * @par Returns
     *      Nothing.
     *
     * @par Side Effects
     * - Calls WiFi.disconnect(false, false) to preserve AP config
     * - Updates connection status to DISCONNECTED
     *
     * @see connect()
     * @see isConnected()
     */
    void disconnect();

    /**
     * @brief Monitor WiFi connection state and update status.
     *
     * @details Checks current WiFi status and updates connection status enum.
     *          Updates credential lastUsed timestamp when connection is first
     *          established. Detects connection loss and updates the established flag.
     *
     *          This method should be called repeatedly from the main loop to ensure
     *          timely status updates and credential tracking.
     *
     * @par Parameters
     *      None.
     *
     * @par Returns
     *      Nothing.
     *
     * @par Side Effects
     * - Updates connection status based on WiFi.status()
     * - Sets established flag on first successful connection
     * - Clears established flag on disconnection
     * - Updates credential lastUsed timestamp on initial connection
     *
     * @par Usage Example:
     * @code{.cpp}
     * void loop() {
     *     connectionMgr.monitorConnection();
     *
     *     AutoNetworkConnectionStatus status = connectionMgr.getStatus();
     *     if (status == AutoNetworkConnectionStatus::CONNECTED) {
     *         // Handle connected state
     *     } else if (status == AutoNetworkConnectionStatus::CONNECTION_LOST) {
     *         // Handle disconnection
     *     }
     * }
     * @endcode
     *
     * @note Should be called regularly from main loop for proper operation.
     * @note Does NOT trigger portal reactivation (handled by AutoNetwork).
     *
     * @see getStatus()
     * @see isConnected()
     * @see AutoNetworkCredentialManager::updateLastUsed()
     */
    void monitorConnection();

    /**
     * @brief Check if WiFi is currently connected.
     *
     * @par Parameters
     *      None.
     *
     * @return bool
     * @retval true WiFi is connected.
     * @retval false WiFi is not connected.
     *
     * @see getStatus()
     * @see monitorConnection()
     */
    bool isConnected() const;

    /**
     * @brief Get current WiFi SSID.
     *
     * @par Parameters
     *      None.
     *
     * @return String containing current SSID, or empty string if not connected.
     *
     * @see isConnected()
     */
    String getSSID() const;

    /**
     * @brief Get local IP address.
     *
     * @par Parameters
     *      None.
     *
     * @return IPAddress object containing local IP, or 0.0.0.0 if not connected.
     *
     * @see getGatewayIP()
     * @see getSubnetMask()
     */
    IPAddress getLocalIP() const;

    /**
     * @brief Get gateway IP address.
     *
     * @par Parameters
     *      None.
     *
     * @return IPAddress object containing gateway IP, or 0.0.0.0 if not connected.
     *
     * @see getLocalIP()
     * @see getSubnetMask()
     */
    IPAddress getGatewayIP() const;

    /**
     * @brief Get subnet mask.
     *
     * @par Parameters
     *      None.
     *
     * @return IPAddress object containing subnet mask, or 0.0.0.0 if not connected.
     *
     * @see getLocalIP()
     * @see getGatewayIP()
     */
    IPAddress getSubnetMask() const;

    /**
     * @brief Get current connection status.
     *
     * @par Parameters
     *      None.
     *
     * @return AutoNetworkConnectionStatus Current connection status enum value.
     *
     * @see isConnected()
     * @see monitorConnection()
     */
    AutoNetworkConnectionStatus getStatus() const;

private:
    AutoNetworkPortal *_portal;           ///< Portal reference for connection parameters
    AutoNetworkCredential *_credential;   ///< Credential storage reference
    AutoNetworkConnectionStatus &_status; ///< Reference to status variable
    bool &_established;                   ///< Reference to established flag

    /**
     * @brief Get monotonic timestamp for credential tracking.
     *
     * @par Parameters
     *      None.
     *
     * @par Returns
     *      Unsigned long timestamp in seconds since boot.
     */
    unsigned long _getMonotonicTimestamp() const;

    static constexpr const char *TAG = "AutoNetworkConnMgr"; ///< Log tag
};
