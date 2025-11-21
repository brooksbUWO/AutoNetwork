// ****************************************************************************
// Title        : AutoNetwork Statistics Page
// Filename     : 'AutoNetworkStatsPage.h'
// Target MCU   : Espressif ESP32
// Description  : HTML template for AutoNetwork statistics/info page
//                Displays comprehensive WiFi and hardware information
//
// Revision History:
// When         Who         Description of change
// -----------  ----------- -----------------------
// 03-OCT-2025  Brooks      Initial implementation
//
// ****************************************************************************

#ifndef AUTONETWORK_STATS_PAGE_H
#define AUTONETWORK_STATS_PAGE_H

const char AUTONETWORK_STATS_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>AutoNetwork Statistics</title>
    <link rel="stylesheet" href="/global.css">
    <!-- WARNING: DO NOT add inline <style> tags! All CSS must go in webpage_css.h -->
    <!-- AI: If you need to add styles, edit webpage_css.h, NOT this file! -->
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>📊 System Statistics</h1>
            <p>WiFi & Hardware Information</p>
        </div>

        <div class="stats-section">
            <div class="section-title">Network Information</div>

            <div class="stat-row">
                <span class="stat-label">Established Connection:</span>
                <span class="stat-value %ESTAB_CLASS%">%ESTAB_SSID%</span>
            </div>
            <div class="stat-row">
                <span class="stat-label">WiFi Mode:</span>
                <span class="stat-value">%WIFI_MODE% (%WIFI_STATUS%)</span>
            </div>
            <div class="stat-row">
                <span class="stat-label">IP Address:</span>
                <span class="stat-value">%LOCAL_IP%</span>
            </div>
            <div class="stat-row">
                <span class="stat-label">Gateway:</span>
                <span class="stat-value">%GATEWAY%</span>
            </div>
            <div class="stat-row">
                <span class="stat-label">Subnet Mask:</span>
                <span class="stat-value">%NETMASK%</span>
            </div>
            <div class="stat-row">
                <span class="stat-label">SoftAP IP:</span>
                <span class="stat-value">%SOFTAP_IP%</span>
            </div>
            <div class="stat-row">
                <span class="stat-label">AP MAC:</span>
                <span class="stat-value">%AP_MAC%</span>
            </div>
            <div class="stat-row">
                <span class="stat-label">STA MAC:</span>
                <span class="stat-value">%STA_MAC%</span>
            </div>
            <div class="stat-row">
                <span class="stat-label">Channel:</span>
                <span class="stat-value">%CHANNEL%</span>
            </div>
            <div class="stat-row">
                <span class="stat-label">Signal Strength:</span>
                <span class="stat-value %DBM_CLASS%">%DBM% dBm</span>
            </div>

            <div class="section-title">Hardware Information</div>

            <div class="stat-row">
                <span class="stat-label">Chip ID:</span>
                <span class="stat-value">%CHIP_ID%</span>
            </div>
            <div class="stat-row">
                <span class="stat-label">CPU Frequency:</span>
                <span class="stat-value">%CPU_FREQ% MHz</span>
            </div>
            <div class="stat-row">
                <span class="stat-label">Flash Size:</span>
                <span class="stat-value">%FLASH_SIZE%</span>
            </div>
            <div class="stat-row">
                <span class="stat-label">Free Memory:</span>
                <span class="stat-value %HEAP_CLASS%">%FREE_HEAP%</span>
            </div>
            <div class="stat-row">
                <span class="stat-label">System Uptime:</span>
                <span class="stat-value">%SYSTEM_UPTIME%</span>
            </div>
        </div>

        <a href="/_an" class="back-link">← Back to Menu</a>
    </div>
</body>
</html>
)=====";

#endif // AUTONETWORK_STATS_PAGE_H
