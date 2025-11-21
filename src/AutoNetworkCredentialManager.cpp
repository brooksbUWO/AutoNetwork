// ****************************************************************************
// Title        : AutoNetwork WiFi Manager
// Filename     : 'AutoNetworkCredentialManager.cpp'
// Target MCU   : Espressif ESP32
// Description  : Credential management implementation for AutoNetwork library
//
// Revision History:
// When         Who         Description of change
// -----------  ----------- -----------------------
// 14-NOV-2025  Claude      Initial implementation - extracted from AutoNetwork
//
// ****************************************************************************

#include "AutoNetworkCredentialManager.h"

// ****************************************************************************
// Constructor
// ****************************************************************************

AutoNetworkCredentialManager::AutoNetworkCredentialManager(AutoNetworkCredential &credential)
    : _credential(credential)
{
}

// ****************************************************************************
// Public Methods - Credential Operations
// ****************************************************************************

bool AutoNetworkCredentialManager::setCredentials(const char *ssid, const char *password)
{
    AutoNetworkCredentialEntry entry;
    entry.ssid = ssid;
    entry.password = password;
    entry.enterprise = false;
    entry.priority = 0;

    return _credential.save(entry);
}

bool AutoNetworkCredentialManager::hasCredentials() const
{
    return _credential.entries() > 0;
}

uint8_t AutoNetworkCredentialManager::getCount() const
{
    return _credential.entries();
}

bool AutoNetworkCredentialManager::getByRecent(uint8_t index, AutoNetworkCredentialEntry &entry) const
{
    return _credential.getByRecent(index, entry);
}

bool AutoNetworkCredentialManager::getByPriority(uint8_t index, AutoNetworkCredentialEntry &entry) const
{
    return _credential.getByPriority(index, entry);
}

bool AutoNetworkCredentialManager::getByIndex(uint8_t index, AutoNetworkCredentialEntry &entry) const
{
    return _credential.getByIndex(index, entry);
}

bool AutoNetworkCredentialManager::save(const AutoNetworkCredentialEntry &entry)
{
    return _credential.save(entry);
}

bool AutoNetworkCredentialManager::remove(const char *ssid)
{
    return _credential.del(ssid);
}

void AutoNetworkCredentialManager::removeAll()
{
    _credential.delAll();
}

bool AutoNetworkCredentialManager::updateLastUsed(const char *ssid, uint64_t timestamp)
{
    return _credential.updateLastUsed(ssid, timestamp);
}
