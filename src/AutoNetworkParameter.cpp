// ****************************************************************************
// Title        : AutoNetwork Custom Parameter Implementation
// Filename     : 'AutoNetworkParameter.cpp'
// Target MCU   : Espressif ESP32
// Description  : Custom configuration parameter for AutoNetwork portal
//
// Revision History:
// When         Who         Description of change
// -----------  ----------- -----------------------
// 02-OCT-2025  Brooks      Initial implementation
//
// ****************************************************************************

// Include Files
// ****************************************************************************
#include "AutoNetworkParameter.h"

// Class Implementation
// ****************************************************************************

AutoNetworkParameter::AutoNetworkParameter()
    : _id(0),
      _type(AN_INPUT),
      _name(""),
      _placeholder(""),
      _value(""),
      _required(false),
      _length(AUTONETWORK_DEFAULT_PARAM_LENGTH)
{
}

AutoNetworkParameter::AutoNetworkParameter(const char *name)
    : _id(0),
      _type(AN_INPUT),
      _name(name),
      _placeholder(""),
      _value(""),
      _required(false),
      _length(AUTONETWORK_DEFAULT_PARAM_LENGTH)
{
}

AutoNetworkParameter::AutoNetworkParameter(const char *name, const char *placeholder)
    : _id(0),
      _type(AN_INPUT),
      _name(name),
      _placeholder(placeholder),
      _value(""),
      _required(false),
      _length(AUTONETWORK_DEFAULT_PARAM_LENGTH)
{
}

AutoNetworkParameter::AutoNetworkParameter(const char *name, const char *placeholder, const char *value)
    : _id(0),
      _type(AN_INPUT),
      _name(name),
      _placeholder(placeholder),
      _value(value),
      _required(false),
      _length(AUTONETWORK_DEFAULT_PARAM_LENGTH)
{
}

AutoNetworkParameter::AutoNetworkParameter(const char *name, const char *placeholder, const char *value, uint8_t length)
    : _id(0),
      _type(AN_INPUT),
      _name(name),
      _placeholder(placeholder),
      _value(value),
      _required(false),
      _length(length)
{
}

// Configuration Methods
// ****************************************************************************

void AutoNetworkParameter::setType(AutoNetworkParameterType type)
{
    _type = type;
}

void AutoNetworkParameter::setName(const char *name)
{
    _name = name;
}

void AutoNetworkParameter::setPlaceholder(const char *placeholder)
{
    _placeholder = placeholder;
}

void AutoNetworkParameter::setValue(const char *value)
{
    _value = value;
}

void AutoNetworkParameter::setRequired(bool required)
{
    _required = required;
}

void AutoNetworkParameter::setLength(uint8_t length)
{
    _length = length;
}

// Accessor Methods
// ****************************************************************************

AutoNetworkParameterType AutoNetworkParameter::getType() const
{
    return _type;
}

const char *AutoNetworkParameter::getName() const
{
    return _name.c_str();
}

const char *AutoNetworkParameter::getPlaceholder() const
{
    return _placeholder.c_str();
}

const char *AutoNetworkParameter::getValue() const
{
    return _value.c_str();
}

bool AutoNetworkParameter::getRequired() const
{
    return _required;
}

uint8_t AutoNetworkParameter::getLength() const
{
    return _length;
}

uint32_t AutoNetworkParameter::getId() const
{
    return _id;
}
