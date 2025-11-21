/**
 * @file AutoNetworkLog.cpp
 * @brief Implementation of logging infrastructure for AutoNetwork library
 * @version 1.1.0
 * @date 2025-11-11
 */

#include "AutoNetworkLog.h"

// ============================================================================
// Global Log Level State Definition
// ============================================================================

namespace AutoNetworkLogging {
    /**
     * @brief Current logging level for the library
     * Default is controlled by AUTONETWORK_DEBUG build flag
     *
     * AUTONETWORK_DEBUG values:
     * - Not defined or 0-2: AN_LOG_WARN (errors and warnings only)
     * - 3: AN_LOG_INFO (informational messages + warnings + errors)
     * - 4: AN_LOG_DEBUG (debug messages and all above)
     * - 5: AN_LOG_VERBOSE (verbose debug including repetitive calls)
     */
#if defined(AUTONETWORK_DEBUG)
    #if AUTONETWORK_DEBUG >= 5
        AutoNetworkLogLevel currentLogLevel = AN_LOG_VERBOSE;
    #elif AUTONETWORK_DEBUG >= 4
        AutoNetworkLogLevel currentLogLevel = AN_LOG_DEBUG;
    #elif AUTONETWORK_DEBUG >= 3
        AutoNetworkLogLevel currentLogLevel = AN_LOG_INFO;
    #else
        AutoNetworkLogLevel currentLogLevel = AN_LOG_WARN;
    #endif
#else
    AutoNetworkLogLevel currentLogLevel = AN_LOG_WARN;
#endif
}
