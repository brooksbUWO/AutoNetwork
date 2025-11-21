/**
 * @file AutoNetworkLog.h
 * @brief Logging infrastructure for AutoNetwork library
 * @version 1.1.0
 * @date 2025-11-11
 *
 * Provides configurable logging levels and macros to control library verbosity.
 * Users can control how much diagnostic information the library outputs without
 * affecting their own application logging.
 */

#pragma once

#include <Arduino.h>

// ============================================================================
// Logging Level Definitions
// ============================================================================

/**
 * @brief Logging levels for AutoNetwork library
 *
 * Controls the verbosity of library diagnostic output:
 * - AN_LOG_NONE: Completely silent (no output)
 * - AN_LOG_ERROR: Only critical errors
 * - AN_LOG_WARN: Warnings and errors
 * - AN_LOG_INFO: Important state changes + warnings + errors
 * - AN_LOG_DEBUG: Detailed operations + all above
 * - AN_LOG_VERBOSE: Everything including repetitive loop calls
 */
enum AutoNetworkLogLevel {
    AN_LOG_NONE    = 0,  ///< No logging output
    AN_LOG_ERROR   = 1,  ///< Only critical errors
    AN_LOG_WARN    = 2,  ///< Warnings and errors
    AN_LOG_INFO    = 3,  ///< Informational messages, warnings, and errors
    AN_LOG_DEBUG   = 4,  ///< Debug messages and all above
    AN_LOG_VERBOSE = 5   ///< Verbose debug including repetitive calls
};

// ============================================================================
// Global Log Level State
// ============================================================================

namespace AutoNetworkLogging {
    /**
     * @brief Current logging level for the library
     * Default is AN_LOG_WARN (errors and warnings only)
     */
    extern AutoNetworkLogLevel currentLogLevel;

    /**
     * @brief Set the logging level
     * @param level New logging level
     */
    inline void setLogLevel(AutoNetworkLogLevel level) {
        currentLogLevel = level;
    }

    /**
     * @brief Get the current logging level
     * @return Current logging level
     */
    inline AutoNetworkLogLevel getLogLevel() {
        return currentLogLevel;
    }
}

// ============================================================================
// Internal Logging Macros
// ============================================================================

/**
 * @brief Internal logging function
 *
 * Wraps ESP_LOG macros with level checking. Only outputs if the current
 * log level is high enough for the requested severity.
 *
 * @param level Minimum level required to output this message
 * @param tag Tag string for the component
 * @param format Printf-style format string
 * @param ... Variable arguments for format string
 */
#define AN_LOG(level, tag, format, ...) \
    do { \
        if (AutoNetworkLogging::currentLogLevel >= (level)) { \
            if ((level) == AN_LOG_ERROR) { \
                ESP_LOGE(tag, format, ##__VA_ARGS__); \
            } else if ((level) == AN_LOG_WARN) { \
                ESP_LOGW(tag, format, ##__VA_ARGS__); \
            } else if ((level) == AN_LOG_INFO) { \
                ESP_LOGI(tag, format, ##__VA_ARGS__); \
            } else if ((level) == AN_LOG_DEBUG) { \
                ESP_LOGD(tag, format, ##__VA_ARGS__); \
            } else if ((level) == AN_LOG_VERBOSE) { \
                ESP_LOGV(tag, format, ##__VA_ARGS__); \
            } \
        } \
    } while(0)

/**
 * @brief Log an error message
 * Always outputs regardless of log level setting
 */
#define AN_LOGE(tag, format, ...) AN_LOG(AN_LOG_ERROR, tag, format, ##__VA_ARGS__)

/**
 * @brief Log a warning message
 * Outputs if log level is WARN or higher
 */
#define AN_LOGW(tag, format, ...) AN_LOG(AN_LOG_WARN, tag, format, ##__VA_ARGS__)

/**
 * @brief Log an informational message
 * Outputs if log level is INFO or higher
 */
#define AN_LOGI(tag, format, ...) AN_LOG(AN_LOG_INFO, tag, format, ##__VA_ARGS__)

/**
 * @brief Log a debug message
 * Outputs if log level is DEBUG or higher
 */
#define AN_LOGD(tag, format, ...) AN_LOG(AN_LOG_DEBUG, tag, format, ##__VA_ARGS__)

/**
 * @brief Log a verbose message
 * Outputs only if log level is VERBOSE
 */
#define AN_LOGV(tag, format, ...) AN_LOG(AN_LOG_VERBOSE, tag, format, ##__VA_ARGS__)

// ============================================================================
// Log Level String Conversion
// ============================================================================

/**
 * @brief Convert log level enum to human-readable string
 * @param level Log level to convert
 * @return String representation of log level
 */
inline const char* autoNetworkLogLevelToString(AutoNetworkLogLevel level) {
    switch (level) {
        case AN_LOG_NONE:    return "NONE";
        case AN_LOG_ERROR:   return "ERROR";
        case AN_LOG_WARN:    return "WARN";
        case AN_LOG_INFO:    return "INFO";
        case AN_LOG_DEBUG:   return "DEBUG";
        case AN_LOG_VERBOSE: return "VERBOSE";
        default:             return "UNKNOWN";
    }
}
