// ****************************************************************************
// Title        : AutoNetwork Basic Example
// Filename     : 'BasicDemo.ino'
// Target MCU   : Espressif ESP32 (Doit DevKit Version 1)
// Description  : Minimal AutoNetwork example demonstrating WiFi management
//                with a custom webpage and non-blocking LED indicator.
//
// This example demonstrates:
//   1. Basic AutoNetwork setup with AsyncWebServer
//   2. Custom webpage with AutoNetwork menu integration
//   3. Non-blocking LED blink pattern for background processing
//
// Revision History:
// When         Who         Description of change
// -----------  ----------- -----------------------
// 21-NOV-2025  Brooks      Initial basic example
//
// ****************************************************************************

// Include Files
// ****************************************************************************
#include <Arduino.h>
#include <WiFi.h>			   // ESP32 WiFi library
#include <AutoNetwork.h>	   // AutoNetwork library for WiFi management
#include <AsyncTCP.h>		   // github.com/ESP32Async/AsyncTCP
#include <ESPAsyncWebServer.h> // github.com/ESP32Async/ESPAsyncWebServer

// Globals
// ****************************************************************************
const uint8_t PIN_LED = 15;				  // GPIO pin for external LED
const uint16_t INTERVAL_LED_BLINK = 1000; // LED blink interval (ms)
uint32_t ledBlinkTime = 0;				  // Timestamp for non-blocking timer

// Simple LED abstraction for cleaner state management
struct Led
{
	uint8_t pin;
	bool state;

	void update()
	{
		digitalWrite(pin, state ? HIGH : LOW);
	}
};

bool defaultState = false;				   // LED off by default (active HIGH)
Led ledExternal = {PIN_LED, defaultState}; // External LED instance

// WiFi Management
const uint8_t HTTP_PORT = 80;	  // HTTP server port
AsyncWebServer server(HTTP_PORT); // Create the web server instance
AutoNetwork autonetwork(&server); // Create the AutoNetwork instance

// Your Webpage
// ****************************************************************************
// Returns the root webpage HTML content. The {{AUTONETWORK_MENU}} placeholder
// is replaced with a hamburger menu icon linking to /_an. This relative link
// works regardless of the device's IP address, which changes depending on
// whether the ESP32 is in AP mode (192.168.4.1) or connected to a network
// (router-assigned IP).
String rootPage()
{
	return R"(
<!DOCTYPE html>
<html>
<head><meta name="viewport" content="width=device-width, initial-scale=1"></head>
<body>
{{AUTONETWORK_MENU}}
<h1>Hello from ESP32</h1>
</body>
</html>
)";
}

// Setup Code
// ****************************************************************************
void setup()
{
	Serial.begin(115200);
	Serial.println();
	pinMode(ledExternal.pin, OUTPUT);

	autonetwork.setRootContent(rootPage); // Set custom root webpage
	autonetwork.begin();				  // Start AutoNetwork WiFi management
}

// Main Program Loop
// ****************************************************************************
void loop()
{
	// Must be called first to process AutoNetwork WiFi management tasks
	autonetwork.loop();

	// Non-blocking LED blink demonstrates that your application code runs
	// continuously alongside AutoNetwork's WiFi management. Using millis()
	// instead of delay() allows both the LED and WiFi to operate smoothly.
	if (millis() - ledBlinkTime >= INTERVAL_LED_BLINK)
	{
		ledBlinkTime = millis();
		ledExternal.state = !ledExternal.state;
		ledExternal.update();
	}
}
