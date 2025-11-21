// ****************************************************************************
// Title        : AutoNetwork LED Ticker Implementation
// Filename     : 'AutoNetworkTicker.cpp'
// Target MCU   : Espressif ESP32
// Description  : Visual WiFi status indicator using LED blink patterns
//
// Revision History:
// When         Who         Description of change
// -----------  ----------- -----------------------
// 02-OCT-2025  Brooks      Initial implementation
//
// ****************************************************************************

// Include Files
// ****************************************************************************
#include "AutoNetworkTicker.h"
#include "AutoNetworkLog.h"
#include "esp_log.h"

// Constants
// ****************************************************************************
static const char *TAG = "AutoNetworkTicker";

// Class Implementation
// ****************************************************************************

AutoNetworkTicker::AutoNetworkTicker(uint8_t pin, uint8_t activeLevel)
    : _pin(pin),
      _activeLevel(activeLevel),
      _pattern(AutoNetworkTickerPattern::OFF),
      _running(false),
      _state(false),
      _onTimeMs(0),
      _offTimeMs(0),
      _lastToggleMs(0)
{
    // Configure GPIO pin
    pinMode(_pin, OUTPUT);
    _setLED(false); // Start with LED off

    AN_LOGD(TAG, "Ticker initialized on GPIO %d (active %s)", _pin,
             _activeLevel == LOW ? "LOW" : "HIGH");
}

AutoNetworkTicker::~AutoNetworkTicker()
{
    stop();
}

// Start ticker with predefined pattern
void AutoNetworkTicker::start(AutoNetworkTickerPattern pattern)
{
    AN_LOGD(TAG, "start() called with pattern=%d", static_cast<int>(pattern));
    _pattern = pattern;

    switch (pattern)
    {
    case AutoNetworkTickerPattern::OFF:
        AN_LOGD(TAG, "Pattern is OFF, calling stop()");
        stop();
        break;

    case AutoNetworkTickerPattern::SLOW_BLINK:
        _onTimeMs = TICKER_BLINK_SLOW_ON_MS;
        _offTimeMs = TICKER_BLINK_SLOW_OFF_MS;
        _running = true;
        _state = false;
        _lastToggleMs = millis();
        _setLED(true); // Start with LED on
        _state = true;
        AN_LOGI(TAG, "Started SLOW_BLINK pattern (%ums on, %ums off)",
                 _onTimeMs, _offTimeMs);
        break;

    case AutoNetworkTickerPattern::FAST_BLINK:
        _onTimeMs = TICKER_BLINK_FAST_ON_MS;
        _offTimeMs = TICKER_BLINK_FAST_OFF_MS;
        _running = true;
        _state = false;
        _lastToggleMs = millis();
        _setLED(true); // Start with LED on
        _state = true;
        AN_LOGI(TAG, "Started FAST_BLINK pattern (%ums on, %ums off)",
                 _onTimeMs, _offTimeMs);
        break;

    case AutoNetworkTickerPattern::SOLID_ON:
        _running = false; // No updates needed for solid on
        _state = true;    // LED state is ON
        AN_LOGD(TAG, "SOLID_ON: calling _setLED(false)");
        _setLED(false);   // Turn LED on immediately (active-LOW logic)
        AN_LOGI(TAG, "Started SOLID_ON pattern (LED solid on)");
        break;

    case AutoNetworkTickerPattern::CUSTOM:
        // Custom pattern already configured via setCustomPattern()
        if (_onTimeMs > 0 || _offTimeMs > 0)
        {
            _running = true;
            _state = false;
            _lastToggleMs = millis();
            _setLED(true); // Start with LED on
            _state = true;
            AN_LOGD(TAG, "Started CUSTOM pattern (%ums on, %ums off)",
                     _onTimeMs, _offTimeMs);
        }
        else
        {
            AN_LOGW(TAG, "Cannot start CUSTOM pattern without timing configuration");
        }
        break;

    default:
        AN_LOGW(TAG, "Unknown pattern: %d", static_cast<int>(pattern));
        break;
    }
}

// Start ticker with custom timing
void AutoNetworkTicker::start(uint32_t onTimeMs, uint32_t offTimeMs)
{
    _pattern = AutoNetworkTickerPattern::CUSTOM;
    _onTimeMs = onTimeMs;
    _offTimeMs = offTimeMs;
    _running = true;
    _state = false;
    _lastToggleMs = millis();
    _setLED(true); // Start with LED on
    _state = true;

    AN_LOGD(TAG, "Started custom pattern (%ums on, %ums off)", onTimeMs, offTimeMs);
}

// Stop ticker and turn off LED
void AutoNetworkTicker::stop()
{
    _running = false;
    _setLED(false);
    _state = false;

    AN_LOGD(TAG, "Ticker stopped");
}

// Update ticker state (call from main loop)
void AutoNetworkTicker::update()
{
    if (!_running)
    {
        return;
    }

    // SOLID_ON pattern: LED is already set in start(), nothing to update
    if (_pattern == AutoNetworkTickerPattern::SOLID_ON)
    {
        return;
    }

    unsigned long currentMs = millis();
    unsigned long elapsedMs = currentMs - _lastToggleMs;

    // Check if it's time to toggle LED state
    uint32_t targetTimeMs = _state ? _onTimeMs : _offTimeMs;

    if (elapsedMs >= targetTimeMs)
    {
        _state = !_state;
        _setLED(_state);
        _lastToggleMs = currentMs;
    }
}

// Configuration Methods
// ****************************************************************************

void AutoNetworkTicker::setPin(uint8_t pin)
{
    bool wasRunning = _running;
    if (wasRunning)
    {
        stop();
    }

    _pin = pin;
    pinMode(_pin, OUTPUT);
    _setLED(false);

    if (wasRunning)
    {
        start(_pattern);
    }

    AN_LOGD(TAG, "Pin changed to GPIO %d", pin);
}

void AutoNetworkTicker::setActiveLevel(uint8_t activeLevel)
{
    _activeLevel = activeLevel;

    // Update current LED state with new active level
    if (_running)
    {
        _setLED(_state);
    }

    AN_LOGD(TAG, "Active level changed to %s",
             _activeLevel == LOW ? "LOW" : "HIGH");
}

void AutoNetworkTicker::setPattern(AutoNetworkTickerPattern pattern)
{
    if (_running)
    {
        start(pattern);
    }
    else
    {
        _pattern = pattern;
    }
}

void AutoNetworkTicker::setCustomPattern(uint32_t onTimeMs, uint32_t offTimeMs)
{
    _pattern = AutoNetworkTickerPattern::CUSTOM;
    _onTimeMs = onTimeMs;
    _offTimeMs = offTimeMs;

    AN_LOGD(TAG, "Custom pattern configured (%ums on, %ums off)",
             onTimeMs, offTimeMs);
}

// Status Query Methods
// ****************************************************************************

bool AutoNetworkTicker::isRunning() const
{
    return _running;
}

AutoNetworkTickerPattern AutoNetworkTicker::getPattern() const
{
    return _pattern;
}

// Private Helper Methods
// ****************************************************************************

void AutoNetworkTicker::_setLED(bool on)
{
    // Apply active level logic
    // If on=true: output the active level (LOW for active-LOW LED = LED ON)
    // If on=false: output the opposite level (HIGH for active-LOW LED = LED OFF)
    uint8_t outputLevel = on ? _activeLevel : (_activeLevel == LOW ? HIGH : LOW);

    AN_LOGD(TAG, "_setLED(%s): _activeLevel=%d, outputLevel=%d, pin=%d",
            on ? "true" : "false", _activeLevel, outputLevel, _pin);

    digitalWrite(_pin, outputLevel);
}

void AutoNetworkTicker::_updatePattern()
{
    // Update timing based on current pattern
    switch (_pattern)
    {
    case AutoNetworkTickerPattern::SLOW_BLINK:
        _onTimeMs = TICKER_BLINK_SLOW_ON_MS;
        _offTimeMs = TICKER_BLINK_SLOW_OFF_MS;
        break;

    case AutoNetworkTickerPattern::FAST_BLINK:
        _onTimeMs = TICKER_BLINK_FAST_ON_MS;
        _offTimeMs = TICKER_BLINK_FAST_OFF_MS;
        break;

    case AutoNetworkTickerPattern::OFF:
    case AutoNetworkTickerPattern::CUSTOM:
    default:
        // No change for OFF or CUSTOM patterns
        break;
    }
}
