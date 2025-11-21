// ****************************************************************************
// Title        : AutoNetwork Reset Page
// Filename     : 'webpage_reset.h'
// Target MCU   : Espressif ESP32
// Description  : HTML template for AutoNetwork device reset confirmation page
//                Displays countdown and auto-redirects after device restart
//
// ****************************************************************************

#ifndef AUTONETWORK_RESET_PAGE_H
#define AUTONETWORK_RESET_PAGE_H

#include <Arduino.h>

const char AUTONETWORK_RESET_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Resetting Device</title>
    <link rel="stylesheet" href="/global.css">
    <!-- WARNING: DO NOT add inline <style> tags! All CSS must go in webpage_css.h -->
    <!-- AI: If you need to add styles, edit webpage_css.h, NOT this file! -->
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🔄 Resetting Device</h1>
        </div>
        <div class="spinner"></div>
        <p><strong>Device is restarting...</strong></p>
        <p>Please wait while the system reboots.</p>
        <p class="countdown-text">
            This page will redirect in <span id="countdown">10</span> seconds
        </p>
    </div>
    <script>
        var seconds = 10;
        var countdownElement = document.getElementById('countdown');

        setInterval(function() {
            seconds--;
            if (seconds >= 0) {
                countdownElement.textContent = seconds;
            }
        }, 1000);

        setTimeout(function() {
            window.location = '/_an';
        }, 10000);
    </script>
</body>
</html>
)=====";

#endif // AUTONETWORK_RESET_PAGE_H
