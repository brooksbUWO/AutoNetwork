/*!
 * @file AutoNetworkTicker.h
 *
 * @brief Visual WiFi status indicator using LED blink patterns.
 *
 * @details This header defines the LED ticker class for AutoNetwork, providing
 *          non-blocking LED control with configurable blink patterns to indicate
 *          WiFi connection status. Supports predefined patterns (OFF, SLOW_BLINK,
 *          FAST_BLINK) and custom user-defined patterns.
 *
 * @section revision_history Revision History
 *
 * | Date | Author | Description |
 * |------|--------|-------------|
 * | 2025-10-02 | Brooks | Initial implementation |
 * | 2025-10-24 | Brooks | Added comprehensive Doxygen documentation |
 */

#pragma once

#include "Arduino.h"

// Ticker Timing Constants (milliseconds)
// ****************************************************************************

constexpr uint16_t TICKER_BLINK_FAST_ON_MS = 150;   // Fast blink ON duration
constexpr uint16_t TICKER_BLINK_FAST_OFF_MS = 150;  // Fast blink OFF duration
constexpr uint16_t TICKER_BLINK_SLOW_ON_MS = 500;   // Slow blink ON duration
constexpr uint16_t TICKER_BLINK_SLOW_OFF_MS = 500;  // Slow blink OFF duration

// Constants
// ****************************************************************************

#ifndef AUTONETWORK_TICKER_PORT
/**
 * @brief Default GPIO pin for LED ticker.
 *
 * @details Uses built-in LED if available. Can be overridden by defining before including library.
 *
 * @note Default: `LED_BUILTIN`
 */
#define AUTONETWORK_TICKER_PORT LED_BUILTIN
#endif

// Ticker Pattern Enumeration
// ****************************************************************************

/**
 * @brief LED blink pattern enumeration.
 *
 * @details Defines predefined blink patterns for visual WiFi status indication.
 *          Each pattern corresponds to a specific connection state.
 */
enum class AutoNetworkTickerPattern
{
    OFF = 0,           /**< LED off - Unused/disabled state */
    SLOW_BLINK,        /**< Slow blink (500ms on, 500ms off) - Captive portal active */
    FAST_BLINK,        /**< Fast blink (150ms on, 150ms off) - Disconnected, not in portal mode */
    SOLID_ON,          /**< LED solid on - WiFi connected */
    CUSTOM             /**< User-defined pattern with custom timing */
};

// Class Declaration
// ****************************************************************************

/**
 * @brief Visual WiFi status indicator using LED blink patterns.
 *
 * @details Provides non-blocking LED control to visually indicate WiFi connection status.
 *          Supports predefined patterns for common states and custom user-defined patterns.
 *
 * @par Predefined Patterns:
 *      - **OFF:** LED off (unused/disabled)
 *      - **SLOW_BLINK:** 500ms on, 500ms off (portal active)
 *      - **FAST_BLINK:** 150ms on, 150ms off (disconnected)
 *      - **SOLID_ON:** LED solid on (WiFi connected)
 *      - **CUSTOM:** User-defined on/off timing
 *
 * @par Usage Example:
 * @code{.cpp}
 * AutoNetworkTicker ticker(LED_BUILTIN, LOW); // Active-low LED
 * ticker.start(AutoNetworkTickerPattern::FAST_BLINK);
 *
 * void loop() {
 *     ticker.update(); // Must be called repeatedly for animation
 * }
 * @endcode
 *
 * @note Call `update()` in main loop for proper LED animation.
 * @note Ticker operates in non-blocking mode - does not use delay().
 */
class AutoNetworkTicker
{
public:
    // Constructor and Destructor
    // ========================================================================

    /**
     * @brief Construct a new AutoNetworkTicker object.
     *
     * @param [in] pin GPIO pin number for LED.
     * @param [in] activeLevel Active level (LOW for active-low, HIGH for active-high).
     *
     * @note Default active level is LOW (suitable for most built-in LEDs).
     */
    AutoNetworkTicker(uint8_t pin, uint8_t activeLevel = LOW);

    /**
     * @brief Destroy the AutoNetworkTicker object.
     *
     * @details Stops ticker and performs cleanup.
     *
     * @par Parameters
     *      None.
     *
     * @par Returns
     *      Nothing (destructor).
     */
    ~AutoNetworkTicker();

    // Control Methods
    // ========================================================================

    /**
     * @brief Start ticker with predefined pattern.
     *
     * @param [in] pattern Pattern to use (`OFF`, `SLOW_BLINK`, `FAST_BLINK`, `CUSTOM`).
     */
    void start(AutoNetworkTickerPattern pattern);

    /**
     * @brief Start ticker with custom timing.
     *
     * @param [in] onTimeMs LED on duration in milliseconds.
     * @param [in] offTimeMs LED off duration in milliseconds.
     */
    void start(uint32_t onTimeMs, uint32_t offTimeMs);

    /**
     * @brief Stop ticker and turn LED off.
     *
     * @par Parameters
     *      None.
     *
     * @par Returns
     *      Nothing.
     */
    void stop();

    /**
     * @brief Update ticker state (call from loop).
     *
     * @details Non-blocking method that updates LED state based on timing.
     *          Must be called repeatedly from main loop for proper animation.
     *
     * @par Parameters
     *      None.
     *
     * @par Returns
     *      Nothing.
     *
     * @warning Failure to call `update()` results in non-functional ticker.
     */
    void update();

    // Configuration Methods
    // ========================================================================

    /**
     * @brief Set GPIO pin for LED.
     *
     * @param [in] pin GPIO pin number.
     */
    void setPin(uint8_t pin);

    /**
     * @brief Set LED active level.
     *
     * @param [in] activeLevel `LOW` for active-low, `HIGH` for active-high.
     */
    void setActiveLevel(uint8_t activeLevel);

    /**
     * @brief Set ticker pattern.
     *
     * @param [in] pattern Pattern to use.
     */
    void setPattern(AutoNetworkTickerPattern pattern);

    /**
     * @brief Set custom pattern timing.
     *
     * @param [in] onTimeMs LED on duration in milliseconds.
     * @param [in] offTimeMs LED off duration in milliseconds.
     */
    void setCustomPattern(uint32_t onTimeMs, uint32_t offTimeMs);

    // Status Query Methods
    // ========================================================================

    /**
     * @brief Check if ticker is running.
     *
     * @return bool
     * @retval true Ticker is active.
     * @retval false Ticker is stopped.
     */
    bool isRunning() const;

    /**
     * @brief Get current ticker pattern.
     *
     * @return AutoNetworkTickerPattern Current pattern.
     */
    AutoNetworkTickerPattern getPattern() const;

private:
    // Configuration
    uint8_t _pin;
    uint8_t _activeLevel;
    AutoNetworkTickerPattern _pattern;

    // Timing state
    bool _running;
    bool _state;
    uint32_t _onTimeMs;
    uint32_t _offTimeMs;
    unsigned long _lastToggleMs;

    // Helper methods
    void _setLED(bool on);
    void _updatePattern();
};
