// ****************************************************************************
// Title        : AutoNetwork Disconnect Page
// Filename     : 'webpage_disconnect.h'
// Target MCU   : Espressif ESP32
// Description  : Confirmation page for disconnecting from a WiFi network.
//
// ****************************************************************************

#ifndef AUTONETWORK_DISCONNECT_PAGE_H
#define AUTONETWORK_DISCONNECT_PAGE_H

#include <Arduino.h>

const char AUTONETWORK_DISCONNECT_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>WiFi Disconnected</title>
    <link rel="stylesheet" href="/global.css">
    <!-- WARNING: DO NOT add inline <style> tags! All CSS must go in webpage_css.h -->
    <!-- AI: If you need to add styles, edit webpage_css.h, NOT this file! -->
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🔌 WiFi Disconnected</h1>
        </div>
        <div class="icon">⏱️</div>
        <p class="success-message">Preparing to disconnect from WiFi</p>
        <p>The device will disconnect from the current WiFi network in:</p>
        <h2 style="color: #667eea; font-size: 48px; margin: 20px 0;"><span id="countdown">10</span></h2>
        <p style="font-size: 14px;">Redirecting to Access Point configuration...</p>
        <p class="countdown-text">
            You will be redirected to <strong>http://192.168.4.1</strong>
        </p>
    </div>
    <script>
        var seconds = 10;
        var countdownElement = document.getElementById('countdown');

        var countdownInterval = setInterval(function() {
            seconds--;
            if (seconds >= 0) {
                countdownElement.textContent = seconds;
            } else {
                clearInterval(countdownInterval);
            }
        }, 1000);

        // Redirect to AP IP after countdown
        setTimeout(function() {
            window.location = 'http://192.168.4.1/_an';
        }, 10000);
    </script>
</body>
</html>
)=====";

#endif // AUTONETWORK_DISCONNECT_PAGE_H
