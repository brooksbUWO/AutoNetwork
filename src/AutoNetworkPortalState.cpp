// ****************************************************************************
// Title        : AutoNetwork Library - Portal State Management
// Filename     : 'AutoNetworkPortalState.cpp'
// Target MCU   : Espressif ESP32 (Doit DevKit Version 1)
// Description  : Portal state encapsulation implementation
//
// Revision History:
// When         Who         Description of change
// -----------  ----------- -----------------------
// 17-OCT-2025  Brooks      Created for portal refactoring
//
// ****************************************************************************

// Include Files
// ****************************************************************************
#include "AutoNetworkPortalState.h"
#include "AutoNetwork.h"  // For full AutoNetworkPortalState enum definition
#include "AutoNetworkParameter.h"
#include "AutoNetworkLog.h"

static const char* TAG = "AutoNetworkPortalState";

// Constants
// ****************************************************************************

// Helper Functions
// ****************************************************************************

#ifdef AUTONETWORK_DEBUG
static const char* scanStateToString(ScanState state)
{
    switch (state)
    {
        case ScanState::IDLE: return "IDLE";
        case ScanState::MODE_SWITCHING: return "MODE_SWITCHING";
        case ScanState::STARTING: return "STARTING";
        case ScanState::RUNNING: return "RUNNING";
        case ScanState::COMPLETE: return "COMPLETE";
        case ScanState::RETRY_DELAY: return "RETRY_DELAY";
        default: return "UNKNOWN";
    }
}

static const char* portalStateToString(AutoNetworkPortalState state)
{
    switch (state)
    {
        case AutoNetworkPortalState::IDLE: return "IDLE";
        case AutoNetworkPortalState::DISCONNECTING: return "DISCONNECTING";
        case AutoNetworkPortalState::CONNECTING_WIFI: return "CONNECTING_WIFI";
        case AutoNetworkPortalState::WAITING_FOR_CONNECTION: return "WAITING_FOR_CONNECTION";
        case AutoNetworkPortalState::SUCCESS: return "SUCCESS";
        case AutoNetworkPortalState::FAILED: return "FAILED";
        case AutoNetworkPortalState::TIMEOUT: return "TIMEOUT";
        default: return "UNKNOWN";
    }
}
#endif

// Constructors
// ****************************************************************************

PortalState::PortalState()
    : _active(false),
      _timeout(AUTONETWORK_PORTAL_TIMEOUT),
      _timeStart(0),
      _timeConnect(0),
      _state(AutoNetworkPortalState::IDLE),
      _stateCallback(nullptr),
      _exitFlag(false),
      _exitTime(0),
      _successDelaying(false),
      _successTime(0),
      _disconnectScheduled(false),
      _disconnectTime(0),
      _otaInProgress(false),
      _otaRestartPending(false),
      _otaMode("fr"),
      _otaMD5Hash(""),
      _otaTotalSize(0),
      _otaUploadedSize(0),
      _apSSID(""),
      _apPassword(""),
      _staSSID(""),
      _staPassword(""),
      _staEnterprise(false),
      _staEnterpriseNetId(""),
      _authEnabled(false),
      _authUsername(""),
      _authPassword(""),
      _scanActive(false),
      _scanStartTime(0),
      _lastScanStatus(0),
      _scanState(ScanState::IDLE),
      _scanStateChangeTime(0),
      _configCounter(0),
      _configCallback(nullptr)
{
    _scanCache.timestamp = 0;
    _scanCache.count = 0;
    _scanCache.valid = false;
}

// Public Methods
// ****************************************************************************

void PortalState::setState(AutoNetworkPortalState state)
{
    if (_state != state)
    {
        #ifdef AUTONETWORK_DEBUG
        AutoNetworkPortalState oldState = _state;
        AN_LOGI(TAG, "Portal State: %s -> %s",
                 portalStateToString(oldState), portalStateToString(state));
        #endif

        _state = state;

        if (_stateCallback)
        {
            _stateCallback(state);
        }
    }
}

void PortalState::setScanState(ScanState state)
{
    if (_scanState != state)
    {
        #ifdef AUTONETWORK_DEBUG
        ScanState oldState = _scanState;
        AN_LOGI(TAG, "Scan State: %s -> %s",
                 scanStateToString(oldState), scanStateToString(state));
        #endif

        _scanState = state;
        _scanStateChangeTime = millis();
    }
}

void PortalState::reset()
{
    _active = false;
    setState(AutoNetworkPortalState::IDLE);
    clearExit();
    clearSuccessDelay();
    clearDisconnect();
    resetOTA();
    clearSTACredentials();
    _scanActive = false;
    setScanState(ScanState::IDLE);
    invalidateScanCache();
}

// WiFi Scan State Machine Implementation
// ****************************************************************************

// Scan Cache Implementation
// ****************************************************************************

bool PortalState::isScanCacheValid(uint32_t maxAgeMs) const
{
    return _scanCache.valid && (millis() - _scanCache.timestamp < maxAgeMs);
}

void PortalState::updateScanCache(int16_t count)
{
    _scanCache.count = count;
    _scanCache.timestamp = millis();
    _scanCache.valid = true;
}

void PortalState::invalidateScanCache()
{
    _scanCache.valid = false;
}

// Scan State Machine
// ****************************************************************************

void PortalState::requestScan()
{
    if (_scanState == ScanState::IDLE)
    {
        wifi_mode_t currentMode = WiFi.getMode();
        AN_LOGI(TAG, "[AutoNetworkPortalState] requestScan() - currentMode: %d", currentMode);

        if (currentMode == WIFI_MODE_NULL || currentMode == WIFI_MODE_AP)
        {
            WiFi.mode(WIFI_AP_STA);
            setScanState(ScanState::MODE_SWITCHING);
        }
        else
        {
            setScanState(ScanState::STARTING);
        }
    }
}

void PortalState::updateScanStateMachine()
{
    uint32_t currentTime = millis();
    uint32_t elapsed = currentTime - _scanStateChangeTime;

    switch (_scanState)
    {
        case ScanState::IDLE:
            break;

        case ScanState::MODE_SWITCHING:
            AN_LOGI(TAG, "[AutoNetworkPortalState] MODE_SWITCHING - WiFi.getMode(): %d", WiFi.getMode());
            if (WiFi.getMode() & WIFI_MODE_STA)
            {
                setScanState(ScanState::STARTING);
            }
            else if (elapsed > 1000)
            {
                setScanState(ScanState::IDLE);
                _lastScanStatus = static_cast<uint16_t>(-1);
            }
            break;

        case ScanState::STARTING:
            {
                WiFi.scanDelete();
                WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);

                int16_t result = WiFi.scanNetworks(true);
                AN_LOGI(TAG, "[AutoNetworkPortalState] STARTING - WiFi.scanNetworks(true) result: %d", result);

                if (result == WIFI_SCAN_RUNNING)
                {
                    setScanState(ScanState::RUNNING);
                    _scanActive = true;
                }
                else if (result == WIFI_SCAN_FAILED)
                {
                    setScanState(ScanState::RETRY_DELAY);
                    _lastScanStatus = static_cast<uint16_t>(WIFI_SCAN_FAILED);
                    _scanActive = false;
                }
                else
                {
                    setScanState(ScanState::RETRY_DELAY);
                    _lastScanStatus = static_cast<uint16_t>(WIFI_SCAN_FAILED);
                    _scanActive = false;
                }
            }
            break;

        case ScanState::RUNNING:
            {
                int16_t result = WiFi.scanComplete();

                if (result >= 0)
                {
                    setScanState(ScanState::COMPLETE);
                    _lastScanStatus = static_cast<uint16_t>(result);
                    _scanActive = false;
                    updateScanCache(result);
                }
                else if (result == WIFI_SCAN_FAILED)
                {
                    setScanState(ScanState::RETRY_DELAY);
                    _lastScanStatus = static_cast<uint16_t>(WIFI_SCAN_FAILED);
                    _scanActive = false;
                }
                else if (elapsed > 10000)
                {
                    WiFi.scanDelete();
                    setScanState(ScanState::RETRY_DELAY);
                    _lastScanStatus = static_cast<uint16_t>(WIFI_SCAN_FAILED);
                    _scanActive = false;
                }
            }
            break;

        case ScanState::COMPLETE:
            setScanState(ScanState::IDLE);
            break;

        case ScanState::RETRY_DELAY:
            if (elapsed > AUTONETWORK_SCAN_RETRY_DELAY_MS)
            {
                setScanState(ScanState::STARTING);
            }
            break;
    }
}
