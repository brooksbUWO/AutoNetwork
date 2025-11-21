#include "AutoNetworkRequestValidator.h"
#include "AutoNetworkLog.h"

static const char *TAG = "RequestValidator";

bool AutoNetworkRequestValidation::validateAuth(
    const String &username,
    const String &password) const
{
    if (!requireAuth)
    {
        return true;
    }

    if (!portalState->isAuthEnabled())
    {
        return true;
    }

    bool authenticated = request->authenticate(username.c_str(), password.c_str());

    if (!authenticated)
    {
        AN_LOGW(TAG, "Authentication failed");
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
    if (!validateAuth(username, password))
    {
        request->requestAuthentication();
        return false;
    }

    if (isBusy())
    {
        AN_LOGW(TAG, "Portal busy");
        request->send(503, "text/plain", "Busy");
        return false;
    }

    return true;
}
