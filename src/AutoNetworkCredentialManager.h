// ****************************************************************************
// Title        : AutoNetwork WiFi Manager
// Filename     : 'AutoNetworkCredentialManager.h'
// Target MCU   : Espressif ESP32
// Description  : Credential management abstraction for AutoNetwork library
//
// Revision History:
// When         Who         Description of change
// -----------  ----------- -----------------------
// 14-NOV-2025  Brooks      Initial implementation - extracted from AutoNetwork
//
// ****************************************************************************

#ifndef AUTONETWORK_CREDENTIAL_MANAGER_H
#define AUTONETWORK_CREDENTIAL_MANAGER_H

#include "AutoNetworkCredential.h"

/**
 * @brief High-level credential management interface for AutoNetwork.
 *
 * @details This class provides a higher-level abstraction over AutoNetworkCredential,
 *          offering simplified credential management operations. It acts as a facade
 *          to credential operations while maintaining backward compatibility with
 *          direct AutoNetworkCredential access.
 *
 *          The manager handles:
 *          - Saving and retrieving credentials
 *          - Checking credential availability
 *          - Managing credential retrieval by different criteria (recent, priority, index)
 *          - Updating credential metadata (last used timestamp)
 *          - Bulk operations (delete all)
 *
 * @par Usage Example:
 * @code{.cpp}
 * AutoNetworkCredential credential;
 * AutoNetworkCredentialManager manager(credential);
 *
 * // Save a simple credential
 * manager.setCredentials("MyNetwork", "password123");
 *
 * // Check if credentials exist
 * if (manager.hasCredentials()) {
 *     Serial.printf("Stored %d credentials\n", manager.getCount());
 * }
 *
 * // Retrieve most recently used credential
 * AutoNetworkCredentialEntry entry;
 * if (manager.getByRecent(0, entry)) {
 *     Serial.printf("Most recent: %s\n", entry.ssid);
 * }
 * @endcode
 *
 * @see AutoNetworkCredential
 * @see AutoNetworkCredentialEntry
 */
class AutoNetworkCredentialManager
{
public:
    /**
     * @brief Construct credential manager with reference to credential storage.
     *
     * @details Creates a credential manager facade that operates on the provided
     *          AutoNetworkCredential instance. The credential object must remain
     *          valid for the lifetime of the manager.
     *
     * @param [in] credential Reference to the AutoNetworkCredential instance to manage.
     *
     * @note The credential storage is not owned by the manager and must not be destroyed
     *       while the manager is in use.
     */
    explicit AutoNetworkCredentialManager(AutoNetworkCredential &credential);

    /**
     * @brief Save new credentials (convenience method).
     *
     * @details Creates a basic credential entry with enterprise flag set to false
     *          and priority set to 0. For more control, use `save()` with a full
     *          AutoNetworkCredentialEntry structure. If a credential with the same
     *          SSID already exists, it will be updated.
     *
     * @param [in] ssid Network SSID (must not be null or empty).
     * @param [in] password Network password (may be empty for open networks).
     *
     * @return bool
     * @retval true Credentials saved successfully to persistent storage.
     * @retval false Save operation failed (invalid SSID or storage error).
     *
     * @par Usage Example:
     * @code{.cpp}
     * AutoNetworkCredentialManager manager(credential);
     * if (manager.setCredentials("MyWiFi", "password123")) {
     *     Serial.println("Credentials saved successfully");
     * }
     * @endcode
     *
     * @see save()
     * @see AutoNetworkCredentialEntry
     */
    bool setCredentials(const char *ssid, const char *password);

    /**
     * @brief Check if any credentials are stored.
     *
     * @par Parameters
     *      None.
     *
     * @return bool
     * @retval true At least one credential exists in storage.
     * @retval false No credentials are stored.
     *
     * @see getCount()
     */
    bool hasCredentials() const;

    /**
     * @brief Get total number of saved credentials.
     *
     * @par Parameters
     *      None.
     *
     * @return uint8_t Number of credential entries (0-255).
     *
     * @see hasCredentials()
     */
    uint8_t getCount() const;

    /**
     * @brief Get credential by recent usage order.
     *
     * @details Credentials are sorted by lastUsed timestamp in descending order.
     *          Index 0 returns the most recently used credential. This is useful
     *          for implementing connection strategies that prefer recently successful
     *          networks.
     *
     * @param [in] index Index in recent-usage sorted list (0 = most recent).
     * @param [out] entry Output parameter to receive credential data.
     *
     * @return bool
     * @retval true Credential loaded successfully into entry parameter.
     * @retval false Invalid index or no credentials available.
     *
     * @par Usage Example:
     * @code{.cpp}
     * AutoNetworkCredentialEntry entry;
     * if (manager.getByRecent(0, entry)) {
     *     Serial.printf("Most recent network: %s\n", entry.ssid);
     *     Serial.printf("Last used: %llu\n", entry.lastUsed);
     * }
     * @endcode
     *
     * @see getByPriority()
     * @see getByIndex()
     * @see updateLastUsed()
     */
    bool getByRecent(uint8_t index, AutoNetworkCredentialEntry &entry) const;

    /**
     * @brief Get credential by priority order.
     *
     * @details Credentials are sorted by priority field in ascending order.
     *          Lower priority values are considered higher priority. Index 0
     *          returns the credential with the lowest priority value (highest
     *          importance).
     *
     * @param [in] index Index in priority sorted list (0 = highest priority).
     * @param [out] entry Output parameter to receive credential data.
     *
     * @return bool
     * @retval true Credential loaded successfully into entry parameter.
     * @retval false Invalid index or no credentials available.
     *
     * @note Priority value 0 is highest priority, 255 is lowest priority.
     *
     * @see getByRecent()
     * @see getByIndex()
     */
    bool getByPriority(uint8_t index, AutoNetworkCredentialEntry &entry) const;

    /**
     * @brief Get credential by storage index.
     *
     * @details Retrieves credential in the order they are stored internally.
     *          This is the fastest lookup method but order is not guaranteed
     *          to be consistent across reboots or modifications.
     *
     * @param [in] index Storage index (0-based).
     * @param [out] entry Output parameter to receive credential data.
     *
     * @return bool
     * @retval true Credential loaded successfully into entry parameter.
     * @retval false Invalid index or no credentials available.
     *
     * @note Use `getByRecent()` or `getByPriority()` for sorted access.
     *
     * @see getByRecent()
     * @see getByPriority()
     */
    bool getByIndex(uint8_t index, AutoNetworkCredentialEntry &entry) const;

    /**
     * @brief Save a complete credential entry.
     *
     * @details If a credential with the same SSID already exists, it will be updated
     *          with the new values. Otherwise a new credential entry is created. The
     *          credential is immediately persisted to ESP32 NVS flash storage.
     *
     * @param [in] entry Credential entry to save (contains SSID, password, metadata).
     *
     * @return bool
     * @retval true Credential saved successfully to persistent storage.
     * @retval false Save operation failed (invalid entry or storage error).
     *
     * @par Usage Example:
     * @code{.cpp}
     * AutoNetworkCredentialEntry entry;
     * strncpy(entry.ssid, "OfficeWiFi", sizeof(entry.ssid));
     * strncpy(entry.password, "securePass", sizeof(entry.password));
     * entry.priority = 1;
     * entry.enterprise = false;
     * if (manager.save(entry)) {
     *     Serial.println("Full credential entry saved");
     * }
     * @endcode
     *
     * @see setCredentials()
     */
    bool save(const AutoNetworkCredentialEntry &entry);

    /**
     * @brief Remove specific credential by SSID.
     *
     * @param [in] ssid SSID of the credential to remove.
     *
     * @return bool
     * @retval true Credential removed successfully from persistent storage.
     * @retval false Credential not found or removal failed.
     *
     * @see removeAll()
     */
    bool remove(const char *ssid);

    /**
     * @brief Remove all stored credentials.
     *
     * @details This operation clears all credential entries from persistent storage
     *          (ESP32 NVS). The operation is immediate and cannot be undone. Use with
     *          caution in production environments.
     *
     * @par Parameters
     *      None.
     *
     * @par Returns
     *      Nothing.
     *
     * @warning This operation is permanent and cannot be undone.
     *
     * @see remove()
     */
    void removeAll();

    /**
     * @brief Update last-used timestamp for a credential.
     *
     * @details Updates the lastUsed field which affects the order returned by `getByRecent()`.
     *          This should be called when a successful connection is made to a network to
     *          maintain accurate usage tracking for recent connection ordering.
     *
     * @param [in] ssid SSID of the credential to update.
     * @param [in] timestamp Monotonic timestamp (typically from `_getMonotonicTimestamp()`).
     *
     * @return bool
     * @retval true Credential found and timestamp updated successfully.
     * @retval false Credential not found or update failed.
     *
     * @note The timestamp should be monotonic (always increasing) for correct RECENT ordering.
     *
     * @see getByRecent()
     * @see AutoNetworkScanManager::findBestMatchingCredential()
     */
    bool updateLastUsed(const char *ssid, uint64_t timestamp);

private:
    AutoNetworkCredential &_credential; ///< Reference to underlying credential storage
};

#endif // AUTONETWORK_CREDENTIAL_MANAGER_H
