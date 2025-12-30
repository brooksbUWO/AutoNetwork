#include "AutoNetworkRequestValidator.h"
#include "AutoNetworkLog.h"

static const char *TAG = "RequestValidator";

bool AutoNetworkRequestValidation::validateAuth(
    const String &username,
    const String &password) const
{
    // Layer 1: Entry validation - check request valid
    if (request == nullptr)
    {
        AN_LOGE(TAG, "validateAuth: request is nullptr");
        return false;
    }
    
    if (!requireAuth)
    {
        return true;
    }

    // Layer 2: Business validation - check portal state
    if (portalState == nullptr)
    {
        AN_LOGE(TAG, "validateAuth: portalState is nullptr");
        return false;
    }

    if (!portalState->isAuthEnabled())
    {
        return true;
    }

    // Layer 2: Perform authentication
    bool authenticated = request->authenticate(username.c_str(), password.c_str());

    if (!authenticated)
    {
        AN_LOGW(TAG, "Authentication failed for user: %s", 
                username.length() > 0 ? username.c_str() : "(empty)");
    }

    return authenticated;
}

bool AutoNetworkRequestValidation::isBusy() const
{
    if (!checkBusyState)
    {
        return false;
    }

    AutoNetworkPortalState state = portalState->getState();
    return (state == AutoNetworkPortalState::WAITING_FOR_CONNECTION ||
            state == AutoNetworkPortalState::CONNECTING_WIFI);
}

bool AutoNetworkRequestValidation::validate(
    const String &username,
    const String &password)
{
    // Layer 4: Log validation entry
    AN_LOGV(TAG, "validate: requireAuth=%d, checkBusy=%d",
            requireAuth, checkBusyState);
    
    // Layer 1: Entry validation - null pointer check
    if (request == nullptr)
    {
        AN_LOGE(TAG, "validate REJECT: request is nullptr (code: NULL_REQ)");
        return false;
    }
    
    // Layer 2: Auth validation
    if (!validateAuth(username, password))
    {
        AN_LOGW(TAG, "validate REJECT: Authentication failed (code: AUTH_FAIL)");
        request->requestAuthentication();
        return false;
    }

    // Layer 2: Busy check
    if (isBusy())
    {
        AN_LOGW(TAG, "validate REJECT: Portal busy (code: BUSY)");
        request->send(503, "text/plain", "Busy");
        return false;
    }

    // Layer 4: Log success
    AN_LOGV(TAG, "validate ACCEPT: All checks passed");
    return true;
}
