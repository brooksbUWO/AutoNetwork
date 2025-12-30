/*!
 * @file AutoNetworkCredential.h
 *
 * @brief Multi-credential storage class for AutoNetwork library.
 *
 * @details This header defines the credential storage system using ESP32 NVS (Preferences).
 *          Provides persistent storage for up to 255 WiFi credentials with priority-based
 *          ordering and WPA2 Enterprise authentication support.
 *
 * @section revision_history Revision History
 *
 * | Date | Author | Description |
 * |------|--------|-------------|
 * | 2025-10-02 | Brooks | Initial implementation |
 * | 2025-10-24 | Brooks | Added comprehensive Doxygen documentation |
 */

#pragma once

#include "Arduino.h"
#include "Preferences.h"
#include "AutoNetworkConstants.h"
#include <vector>
#include "nvs_flash.h"

// Constants
// ****************************************************************************

/** @brief NVS namespace for credential storage. */
#define AUTONETWORK_CREDENTIAL_NAMESPACE "an_creds"

/** @brief Maximum number of credential entries (0-255). */
#define AUTONETWORK_CREDENTIAL_MAX_ENTRIES 255

/** @brief NVS key for credential blob storage. */
#define AUTONETWORK_CREDENTIAL_BLOB_KEY "AN_CREDT"

/** @brief Magic identifier for credential blob validation. */
#define AUTONETWORK_CREDENTIAL_IDENTIFIER "AN_CREDT"

// Credential Entry Structure
// ****************************************************************************

/**
 * @brief WiFi credential entry structure.
 *
 * @details Represents a single stored WiFi credential with authentication details,
 *          priority ordering, and optional static IP configuration.
 *
 * @note Default constructor initializes all fields to safe defaults (DHCP enabled,
 *       no enterprise authentication, zero timestamps).
 */
struct AutoNetworkCredentialEntry
{
    String ssid;                /**< Network SSID (Service Set Identifier). */
    String password;            /**< Network password (PSK or Enterprise password). */
    bool enterprise;            /**< True if WPA2 Enterprise, false for WPA2-PSK. */
    String enterpriseNetId;     /**< Enterprise network identity (username for PEAP/MSCHAPv2). */
    uint64_t lastUsed;          /**< Last connection timestamp (monotonic time). */
    uint8_t priority;           /**< Connection priority (0 = highest priority). */

    // Network configuration (for static IP support)
    uint8_t bssid[AUTONETWORK_BSSID_LENGTH];  /**< MAC address for specific AP binding (optional). */
    bool dhcp;                                 /**< Use DHCP if true, static IP if false. */
    IPAddress ip;                              /**< Static IP address. */
    IPAddress gateway;                         /**< Gateway address. */
    IPAddress netmask;                         /**< Subnet mask. */
    IPAddress dns1;                            /**< Primary DNS server. */
    IPAddress dns2;                            /**< Secondary DNS server. */

    /**
     * @brief Construct a new credential entry with default values.
     *
     * @details Initializes entry with empty strings, DHCP enabled, zero priority,
     *          and default network mask (255.255.255.0).
     *
     * @par Parameters
     *      None.
     */
    AutoNetworkCredentialEntry()
        : ssid(""),
          password(""),
          enterprise(false),
          enterpriseNetId(""),
          lastUsed(0),
          priority(0),
          dhcp(true),  // Default to DHCP
          ip(0, 0, 0, 0),
          gateway(0, 0, 0, 0),
          netmask(255, 255, 255, 0),
          dns1(0, 0, 0, 0),
          dns2(0, 0, 0, 0)
    {
        memset(bssid, 0, sizeof(bssid));
    }
};

// Class Declaration
// ****************************************************************************

/**
 * @brief Multi-credential storage manager for WiFi credentials.
 *
 * @details Provides persistent storage for WiFi credentials using ESP32 NVS (Preferences).
 *          Supports up to 255 credential entries with priority-based ordering, enterprise
 *          authentication, and static IP configuration.
 *
 * @par Usage Example:
 * @code{.cpp}
 * AutoNetworkCredential creds;
 *
 * // Save a credential
 * AutoNetworkCredentialEntry entry;
 * entry.ssid = "MyNetwork";
 * entry.password = "MyPassword";
 * entry.priority = 1;
 * creds.save(entry);
 *
 * // Retrieve by priority
 * AutoNetworkCredentialEntry retrieved;
 * if (creds.getByPriority(0, retrieved)) {
 *     Serial.println("Highest priority network: " + retrieved.ssid);
 * }
 *
 * // Check credential count
 * Serial.printf("Total credentials: %d\n", creds.entries());
 * @endcode
 *
 * @note All credential operations are persisted to NVS flash storage.
 * @note Maximum storage: 255 entries (limited by uint8_t index).
 */
class AutoNetworkCredential
{
public:
    // Constructors and Destructor
    // ========================================================================

    /**
     * @brief Construct a new AutoNetworkCredential object.
     *
     * @details Initializes NVS storage and loads existing credentials from flash.
     *
     * @par Parameters
     *      None.
     */
    AutoNetworkCredential();

    /**
     * @brief Destroy the AutoNetworkCredential object.
     *
     * @details Commits any pending changes to NVS and closes Preferences handle.
     *
     * @par Parameters
     *      None.
     */
    ~AutoNetworkCredential();

    // Credential Storage Operations
    // ========================================================================

    /**
     * @brief Save credential entry to NVS storage.
     *
     * @details If entry with same SSID exists, it is updated. Otherwise, a new entry
     *          is created. Credentials are persisted to flash immediately.
     *
     * @param [in] entry Credential entry to save.
     *
     * @return true if save succeeded.
     * @return false if save failed (NVS error or storage full).
     */
    bool save(const AutoNetworkCredentialEntry &entry);

    /**
     * @brief Load credential entry by storage index.
     *
     * @param [in] index Zero-based storage index (0 to `entries()-1`).
     * @param [out] entry Credential entry to populate.
     *
     * @return true if load succeeded.
     * @return false if index out of range.
     */
    bool load(uint8_t index, AutoNetworkCredentialEntry &entry);

    /**
     * @brief Delete credential by SSID.
     *
     * @param [in] ssid SSID of credential to delete.
     *
     * @return true if deletion succeeded.
     * @return false if SSID not found or delete failed.
     */
    bool del(const char *ssid);

    /**
     * @brief Delete all stored credentials.
     *
     * @details Clears all credentials from NVS storage and memory.
     *
     * @par Parameters
     *      None.
     *
     * @return true if deletion succeeded.
     * @return false if delete failed.
     */
    bool delAll();

    // Query Operations
    // ========================================================================

    /**
     * @brief Get total number of stored credentials.
     *
     * @par Parameters
     *      None.
     *
     * @return uint8_t Number of credentials (0-255).
     */
    uint8_t entries();

    /**
     * @brief Check if credential exists for given SSID.
     *
     * @param [in] ssid SSID to search for.
     *
     * @return true if credential exists.
     * @return false if not found.
     */
    bool exists(const char *ssid);

    // Retrieval Operations (By Different Criteria)
    // ========================================================================

    /**
     * @brief Get credential by storage index.
     *
     * @param [in] index Zero-based storage index (0 to `entries()-1`).
     * @param [out] entry Credential entry to populate.
     *
     * @return true if retrieval succeeded.
     * @return false if index out of range.
     */
    bool getByIndex(uint8_t index, AutoNetworkCredentialEntry &entry);

    /**
     * @brief Get credential by recency (most recently used first).
     *
     * @details Credentials are sorted by `lastUsed` timestamp in descending order.
     *
     * @param [in] index Index in recency-sorted list (0 = most recent).
     * @param [out] entry Credential entry to populate.
     *
     * @return true if retrieval succeeded.
     * @return false if index out of range.
     */
    bool getByRecent(uint8_t index, AutoNetworkCredentialEntry &entry);

    /**
     * @brief Get credential by priority (highest priority first).
     *
     * @details Credentials are sorted by `priority` field in ascending order
     *          (0 = highest priority).
     *
     * @param [in] index Index in priority-sorted list (0 = highest priority).
     * @param [out] entry Credential entry to populate.
     *
     * @return true if retrieval succeeded.
     * @return false if index out of range.
     */
    bool getByPriority(uint8_t index, AutoNetworkCredentialEntry &entry);

    /**
     * @brief Get credential by SSID.
     *
     * @param [in] ssid SSID to search for.
     * @param [out] entry Credential entry to populate.
     *
     * @return true if credential found.
     * @return false if SSID not found.
     */
    bool getBySSID(const char *ssid, AutoNetworkCredentialEntry &entry);

    // Update Operations
    // ========================================================================

    /**
     * @brief Update last used timestamp for credential.
     *
     * @param [in] ssid SSID of credential to update.
     * @param [in] timestamp New last used timestamp (monotonic time).
     *
     * @return true if update succeeded.
     * @return false if SSID not found or update failed.
     */
    bool updateLastUsed(const char *ssid, uint64_t timestamp);

    /**
     * @brief Update priority for credential.
     *
     * @param [in] ssid SSID of credential to update.
     * @param [in] priority New priority value (0 = highest).
     *
     * @return true if update succeeded.
     * @return false if SSID not found or update failed.
     */
    bool updatePriority(const char *ssid, uint8_t priority);

private:
    Preferences _prefs;

    // Internal storage using std::vector for in-memory credential management
    std::vector<AutoNetworkCredentialEntry> _credentials;
    uint8_t _entryCount;
    uint16_t _containerSize;

    // Binary blob serialization methods (AutoConnect-style)
    size_t _commit();
    uint8_t _import();

    // Helper methods
    uint8_t _findBySSID(const char *ssid);
    void _load();
    bool _isDirty;
};
