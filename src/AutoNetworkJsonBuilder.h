/**
 * @file AutoNetworkJsonBuilder.h
 * @brief JSON response builder utility for AutoNetwork library.
 * @author Brooks
 * @date 2025
 *
 * @details Provides helper methods for building JSON responses with
 *          automatic memory management and ArduinoJson v7 compatibility.
 */

#ifndef AUTONETWORK_JSON_BUILDER_H
#define AUTONETWORK_JSON_BUILDER_H

#include <Arduino.h>
#include <ArduinoJson.h>

// Forward declarations
class AutoNetworkParameter;
struct AutoNetworkParameterTypeNames;

/**
 * @class AutoNetworkJsonBuilder
 * @brief Helper class for building JSON responses.
 *
 * @details Simplifies JSON document creation with:
 *          - ArduinoJson v7 support
 *          - String pre-allocation for efficiency
 *          - Automatic serialization and cleanup
 *          - Pre-built methods for common response types
 *
 * @par Usage Example:
 * @code{.cpp}
 * // Create and serialize a status JSON response
 * String output;
 * AutoNetworkJsonBuilder::buildStatusJson(
 *     output, 1, true, "MyWiFi", "AA:BB:CC:DD:EE:FF",
 *     "192.168.1.100", 2, true
 * );
 * server.send(200, "application/json", output);
 * @endcode
 *
 * @see AutoNetworkParameter
 * @see AutoNetworkScanManager::generateScanJson()
 */
class AutoNetworkJsonBuilder
{
public:
    /**
     * @brief Create JSON document with estimated size.
     *
     * @details Creates a JsonDocument with pre-allocated memory for efficient
     *          JSON building. Works with both ArduinoJson v6 and v7.
     *
     * @param [in] estimatedSize Estimated JSON size in bytes.
     *
     * @return JsonDocument Configured JSON document ready for population.
     *
     * @note Default size of 512 bytes is suitable for most AutoNetwork responses.
     *
     * @see serialize()
     */
    static JsonDocument createDocument(size_t estimatedSize = 512);

    /**
     * @brief Serialize JSON document to String with pre-allocation.
     *
     * @details Serializes the JSON document to a String with pre-allocated
     *          capacity for optimal memory usage and performance.
     *
     * @param [in] doc JSON document to serialize.
     * @param [out] output String to store serialized JSON (cleared first).
     * @param [in] estimatedSize Estimated serialized size in bytes.
     *
     * @par Returns
     *      Nothing.
     *
     * @note Output string is cleared and reserved before serialization.
     *
     * @see createDocument()
     */
    static void serialize(JsonDocument &doc, String &output, size_t estimatedSize = 512);

    /**
     * @brief Build status JSON response.
     *
     * @details Creates a JSON response with connection and portal status information
     *          for the web interface status endpoint.
     *
     * @param [out] output String to store JSON response.
     * @param [in] status Connection status code (AutoNetworkConnectionStatus).
     * @param [in] wifiConnected WiFi connection state.
     * @param [in] staSSID Current WiFi SSID.
     * @param [in] macAddress Device MAC address.
     * @param [in] ipAddress Device IP address.
     * @param [in] portalState Portal state code (PortalStateValue).
     * @param [in] portalActive Portal active flag.
     *
     * @par Returns
     *      Nothing.
     *
     * @par JSON Format Example:
     * @code{.cpp}
     * {
     *   "status": 1,
     *   "connected": true,
     *   "ssid": "MyWiFi",
     *   "mac": "AA:BB:CC:DD:EE:FF",
     *   "ip": "192.168.1.100",
     *   "portalState": 2,
     *   "portalActive": true
     * }
     * @endcode
     *
     * @see AutoNetworkConnectionStatus
     * @see PortalStateValue
     */
    static void buildStatusJson(
        String &output,
        uint8_t status,
        bool wifiConnected,
        const String &staSSID,
        const String &macAddress,
        const String &ipAddress,
        uint8_t portalState,
        bool portalActive);

    /**
     * @brief Build parameter schema JSON response.
     *
     * @details Creates a JSON schema describing custom portal parameters for
     *          dynamic form generation in the web interface.
     *
     * @param [out] output String to store JSON response.
     * @param [in] parameters Array of parameter pointers.
     * @param [in] paramCount Number of parameters in array.
     * @param [in] paramTypes Array of parameter type name mappings.
     *
     * @par Returns
     *      Nothing.
     *
     * @par JSON Format Example:
     * @code{.cpp}
     * [
     *   {
     *     "id": "api_key",
     *     "label": "API Key",
     *     "type": "password",
     *     "placeholder": "Enter your API key",
     *     "default": ""
     *   }
     * ]
     * @endcode
     *
     * @see AutoNetworkParameter
     * @see AutoNetworkParameterTypeNames
     */
    static void buildSchemaJson(
        String &output,
        AutoNetworkParameter **parameters,
        uint8_t paramCount,
        const AutoNetworkParameterTypeNames *paramTypes);
};

#endif // AUTONETWORK_JSON_BUILDER_H
