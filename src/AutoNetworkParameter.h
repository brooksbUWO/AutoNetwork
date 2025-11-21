/*!
 * @file AutoNetworkParameter.h
 *
 * @brief Custom configuration parameter for AutoNetwork portal.
 *
 * @details This header defines the custom parameter class for adding user-defined
 *          configuration fields to the AutoNetwork captive portal. Supports various
 *          input types including text input, headers, dividers, and spacers.
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
#include "AutoNetworkConstants.h"

// Parameter Type Enumeration
// ****************************************************************************

/**
 * @brief Custom parameter type enumeration.
 *
 * @details Defines the type of form field to display in the captive portal.
 *          Each type renders differently in the web interface.
 */
enum AutoNetworkParameterType
{
    AN_INPUT = 0,   /**< Text input field - allows user text entry */
    AN_HEADER,      /**< Header text - displays non-editable title text */
    AN_DIVIDER,     /**< Visual divider line - horizontal separator */
    AN_SPACER       /**< Vertical spacing - adds vertical whitespace */
};

// Type Name Structure (for JSON serialization)
// ****************************************************************************

/**
 * @brief Parameter type name mapping structure.
 *
 * @details Maps parameter type enum values to string names for JSON serialization
 *          in portal API responses.
 */
struct AutoNetworkParameterTypeNames
{
    AutoNetworkParameterType value;  /**< Enum value */
    const char *type;                 /**< String type name */
};

// Class Declaration
// ****************************************************************************

/**
 * @brief Custom configuration parameter for captive portal.
 *
 * @details Represents a user-defined form field in the captive portal web interface.
 *          Add custom parameters to collect additional configuration beyond WiFi credentials.
 *
 * @par Usage Example:
 * @code{.cpp}
 * AutoNetworkParameter deviceName("deviceName", "Device Name", "ESP32", 32);
 * deviceName.setRequired(true);
 * autoNetwork.addParameter(&deviceName);
 *
 * // After configuration, retrieve value:
 * String name = deviceName.getValue();
 * @endcode
 *
 * @note Parameters must remain in scope for the lifetime of AutoNetwork.
 * @note Use `addParameter()` to register and `removeParameter()` to unregister.
 */
class AutoNetworkParameter
{
public:
    // Constructors
    // ========================================================================

    /**
     * @brief Construct a new empty AutoNetworkParameter object.
     *
     * @par Parameters
     *      None.
     */
    AutoNetworkParameter();

    /**
     * @brief Construct parameter with name only.
     *
     * @param [in] name Parameter name (used as HTML name attribute).
     */
    AutoNetworkParameter(const char *name);

    /**
     * @brief Construct parameter with name and placeholder.
     *
     * @param [in] name Parameter name.
     * @param [in] placeholder Placeholder text displayed in empty field.
     */
    AutoNetworkParameter(const char *name, const char *placeholder);

    /**
     * @brief Construct parameter with name, placeholder, and default value.
     *
     * @param [in] name Parameter name.
     * @param [in] placeholder Placeholder text.
     * @param [in] value Default value.
     */
    AutoNetworkParameter(const char *name, const char *placeholder, const char *value);

    /**
     * @brief Construct parameter with name, placeholder, value, and max length.
     *
     * @param [in] name Parameter name.
     * @param [in] placeholder Placeholder text.
     * @param [in] value Default value.
     * @param [in] length Maximum input length in characters.
     */
    AutoNetworkParameter(const char *name, const char *placeholder, const char *value, uint8_t length);

    // Configuration Methods
    // ========================================================================

    /**
     * @brief Set parameter type.
     *
     * @param [in] type Parameter type (`AN_INPUT`, `AN_HEADER`, `AN_DIVIDER`, `AN_SPACER`).
     */
    void setType(AutoNetworkParameterType type);

    /**
     * @brief Set parameter name.
     *
     * @param [in] name Parameter name (HTML name attribute).
     */
    void setName(const char *name);

    /**
     * @brief Set placeholder text.
     *
     * @param [in] placeholder Placeholder text for input field.
     */
    void setPlaceholder(const char *placeholder);

    /**
     * @brief Set parameter value.
     *
     * @param [in] value Parameter value (default or user-submitted).
     */
    void setValue(const char *value);

    /**
     * @brief Set required flag.
     *
     * @param [in] required True if field is required, false if optional.
     */
    void setRequired(bool required);

    /**
     * @brief Set maximum input length.
     *
     * @param [in] length Maximum characters allowed in input field.
     */
    void setLength(uint8_t length);

    // Accessor Methods
    // ========================================================================

    /**
     * @brief Get parameter type.
     *
     * @return AutoNetworkParameterType Current parameter type.
     */
    AutoNetworkParameterType getType() const;

    /**
     * @brief Get parameter name.
     *
     * @return const char* Parameter name string.
     */
    const char *getName() const;

    /**
     * @brief Get placeholder text.
     *
     * @return const char* Placeholder text string.
     */
    const char *getPlaceholder() const;

    /**
     * @brief Get parameter value.
     *
     * @return const char* Current parameter value.
     */
    const char *getValue() const;

    /**
     * @brief Get required flag.
     *
     * @return bool
     * @retval true Field is required.
     * @retval false Field is optional.
     */
    bool getRequired() const;

    /**
     * @brief Get maximum input length.
     *
     * @return uint8_t Maximum characters allowed.
     */
    uint8_t getLength() const;

    /**
     * @brief Get unique parameter ID.
     *
     * @return uint32_t Unique ID assigned by AutoNetwork.
     */
    uint32_t getId() const;

    // Public Member Variables (for AutoNetwork internal access)
    // ========================================================================

    uint32_t _id;                     /**< Unique parameter ID */
    AutoNetworkParameterType _type;   /**< Parameter type */
    String _name;                     /**< Parameter name */
    String _placeholder;              /**< Placeholder text */
    String _value;                    /**< Parameter value */
    bool _required;                   /**< Required flag */
    uint8_t _length;                  /**< Maximum input length */

private:
    // No private members - all accessible for portal integration
};
