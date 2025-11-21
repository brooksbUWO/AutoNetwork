/*!
 * @file AutoNetworkConstants.h
 *
 * @brief Shared constants for AutoNetwork library.
 *
 * @details This header defines global constants used throughout the AutoNetwork library.
 *          Constants are separated to avoid circular dependencies and provide centralized
 *          configuration values for network settings, timing intervals, and memory thresholds.
 *
 * @section revision_history Revision History
 *
 * | Date | Author | Description |
 * |------|--------|-------------|
 * | 2025-10-22 | Brooks | Created for quality improvements |
 * | 2025-10-24 | Brooks | Added comprehensive Doxygen documentation |
 */

#pragma once

#include <Arduino.h>

// Network Configuration Constants
// ****************************************************************************

/**
 * @brief MAC address (BSSID) length in bytes.
 *
 * @details Standard WiFi MAC address length used for BSSID storage and comparison.
 *          MAC addresses are 6 bytes (48 bits) per IEEE 802.11 standard.
 *
 * @note Value: 6 bytes
 */
constexpr uint8_t AUTONETWORK_BSSID_LENGTH = 6;

/**
 * @brief Worst priority sentinel value for credential selection.
 *
 * @details Used as initial value when searching for best priority network.
 *          Any actual priority value will be better than this sentinel.
 *
 * @note Value: 999 (worst possible priority)
 */
constexpr int16_t AUTONETWORK_WORST_PRIORITY = 999;

/**
 * @brief Worst RSSI signal strength sentinel value.
 *
 * @details Used as initial value when searching for best signal strength.
 *          Any actual RSSI value will be better than this sentinel.
 *
 * @note Value: -100 dBm (extremely weak signal)
 */
constexpr int16_t AUTONETWORK_WORST_RSSI = -100;

/**
 * @brief Standard HTTP service port number.
 *
 * @details Port number for HTTP service advertised via mDNS.
 *          Standard HTTP port as defined in RFC 2616.
 *
 * @note Value: 80
 */
constexpr uint16_t AUTONETWORK_HTTP_PORT = 80;

/**
 * @brief Maximum credentials for background reconnection buffer.
 *
 * @details Pre-allocated buffer size for background reconnection attempts to avoid
 *          heap fragmentation during `loop()` execution. Limits how many saved
 *          credentials are checked during each reconnection scan.
 *
 * @note Value: 32 entries
 * @note Separate from total credential storage limit (255 max in NVS)
 */
constexpr uint8_t AUTONETWORK_MAX_RECONNECT_ENTRIES = 32;

/**
 * @brief Default maximum length for custom parameter fields.
 *
 * @details Default maximum character length for `AutoNetworkParameter` text input fields.
 *          Can be overridden per-parameter using `setLength()`.
 *
 * @note Value: 64 characters
 */
constexpr uint8_t AUTONETWORK_DEFAULT_PARAM_LENGTH = 64;

// Timing Constants (milliseconds)
// ****************************************************************************

/**
 * @brief Loop status logging interval in milliseconds.
 *
 * @details Interval for periodic status logging in `loop()` when debug logging is enabled.
 *
 * @note Value: 10000ms (10 seconds)
 */
constexpr uint32_t AUTONETWORK_INTERVAL_LOOP_LOG_MS = 10000;

/**
 * @brief WiFi disconnect countdown timeout in milliseconds.
 *
 * @details Maximum time to wait for graceful WiFi disconnection before forcing disconnect.
 *
 * @note Value: 10000ms (10 seconds)
 */
constexpr uint32_t AUTONETWORK_TIMEOUT_DISCONNECT_MS = 10000;

/**
 * @brief Timeout logging interval in milliseconds.
 *
 * @details Interval for periodic timeout status logging when waiting for operations.
 *
 * @note Value: 30000ms (30 seconds)
 */
constexpr uint32_t AUTONETWORK_INTERVAL_TIMEOUT_LOG_MS = 30000;

/**
 * @brief Status print interval in milliseconds.
 *
 * @details Interval for periodic connection status printing to serial console.
 *
 * @note Value: 2000ms (2 seconds)
 */
constexpr uint32_t AUTONETWORK_INTERVAL_STATUS_PRINT_MS = 2000;

/**
 * @brief WiFi scan debounce interval in milliseconds.
 *
 * @details Minimum time between consecutive WiFi scan requests to prevent scan flooding.
 *          Scan requests within this interval are ignored.
 *
 * @note Value: 2000ms (2 seconds)
 */
constexpr uint32_t AUTONETWORK_DEBOUNCE_SCAN_MS = 2000;

/**
 * @brief WiFi scan retry delay in milliseconds.
 *
 * @details Delay before retrying a failed WiFi scan operation. Prevents rapid retry
 *          loops when WiFi scanning fails.
 *
 * @note Value: 1000ms (1 second)
 */
constexpr uint32_t AUTONETWORK_SCAN_RETRY_DELAY_MS = 1000;

/**
 * @brief Device reset delay in milliseconds.
 *
 * @details Delay before executing device reset to allow pending operations to complete.
 *
 * @note Value: 2000ms (2 seconds)
 */
constexpr uint32_t AUTONETWORK_DELAY_RESET_MS = 2000;

// Memory Constants (bytes)
// ****************************************************************************

/**
 * @brief Free heap warning threshold in bytes.
 *
 * @details When free heap falls below this threshold, warning messages are logged.
 *          Indicates potential memory pressure that may affect system stability.
 *
 * @note Value: 20000 bytes (~20KB)
 */
constexpr uint32_t AUTONETWORK_HEAP_WARNING_THRESHOLD = 20000;

/**
 * @brief Free heap good status threshold in bytes.
 *
 * @details When free heap is above this threshold, memory status is considered healthy.
 *          No warnings are generated.
 *
 * @note Value: 50000 bytes (~50KB)
 */
constexpr uint32_t AUTONETWORK_HEAP_GOOD_THRESHOLD = 50000;
