/**
 * @file AutoNetworkRequestValidator.h
 * @brief Request validation parameter object for AutoNetwork endpoints.
 * @author Brooks
 * @date 2025
 */

#ifndef AUTONETWORK_REQUEST_VALIDATOR_H
#define AUTONETWORK_REQUEST_VALIDATOR_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "AutoNetwork.h"
#include "AutoNetworkPortalState.h"

/**
 * @struct AutoNetworkRequestValidation
 * @brief Parameter object for endpoint request validation.
 *
 * @details Consolidates common validation parameters to reduce code duplication
 *          across endpoint handlers. Provides methods to validate authentication
 *          and portal state before processing HTTP requests.
 *
 * @par Usage Example:
 * @code{.cpp}
 * AutoNetworkRequestValidation validation = {
 *     .request = request,
 *     .portalState = &state,
 *     .requireAuth = true,
 *     .checkBusyState = true
 * };
 *
 * if (!validation.validate(authUsername, authPassword)) {
 *     return; // Error response already sent
 * }
 * // Process request...
 * @endcode
 *
 * @see AutoNetworkPortal
 * @see AutoNetworkPortalState
 */
struct AutoNetworkRequestValidation
{
    AsyncWebServerRequest *request;     ///< HTTP request object
    const PortalState *portalState;     ///< Portal state for busy check
    bool requireAuth;                   ///< Enable authentication check
    bool checkBusyState;                ///< Enable portal busy state check

    /**
     * @brief Validate authentication if required.
     *
     * @details Checks HTTP Basic/Digest authentication credentials against
     *          configured username and password if authentication is enabled.
     *
     * @param [in] username HTTP basic auth username.
     * @param [in] password HTTP basic auth password.
     *
     * @return bool
     * @retval true Authenticated or authentication not required.
     * @retval false Authentication failed.
     *
     * @see validate()
     */
    bool validateAuth(const String &username, const String &password) const;

    /**
     * @brief Check if portal is busy processing a request.
     *
     * @details Returns true if portal state indicates an active connection
     *          attempt is in progress (WAITING_FOR_CONNECTION or CONNECTING).
     *
     * @par Parameters
     *      None.
     *
     * @return bool
     * @retval true Portal is busy (connection in progress).
     * @retval false Portal is idle.
     *
     * @see validate()
     */
    bool isBusy() const;

    /**
     * @brief Perform complete validation and send error responses if needed.
     *
     * @details Validates both authentication (if required) and portal busy state
     *          (if checking enabled). Automatically sends appropriate HTTP error
     *          responses (401 Unauthorized or 503 Service Unavailable) if
     *          validation fails.
     *
     * @param [in] username HTTP basic auth username.
     * @param [in] password HTTP basic auth password.
     *
     * @return bool
     * @retval true Validation passed, request may proceed.
     * @retval false Validation failed, error response sent to client.
     *
     * @par Usage Example:
     * @code{.cpp}
     * void handleConfigEndpoint(AsyncWebServerRequest *request) {
     *     AutoNetworkRequestValidation validation = {
     *         request, &portalState, true, true
     *     };
     *     if (!validation.validate(authUser, authPass)) {
     *         return; // Error already sent
     *     }
     *     // Process configuration request...
     * }
     * @endcode
     *
     * @warning This method sends HTTP responses - do not send additional responses
     *          if this method returns false.
     *
     * @see validateAuth()
     * @see isBusy()
     */
    bool validate(const String &username, const String &password);
};

#endif // AUTONETWORK_REQUEST_VALIDATOR_H
