/**
 * @file webpage_menu.h
 * @brief Embedded menu navigation page for AutoNetwork portal
 * @version 1.1.0
 * @date 2025-11-11
 *
 * Provides main navigation menu for all AutoNetwork features.
 * Includes hamburger menu icon macro for consistent UI.
 *
 * @par Features
 *      - WiFi configuration
 *      - Saved credentials management
 *      - System statistics
 *      - OTA firmware updates
 *      - Factory reset
 */

#ifndef AUTONETWORK_MENU_PAGE_H
#define AUTONETWORK_MENU_PAGE_H

#include <Arduino.h>

/**
 * @brief Macro for AutoNetwork hamburger menu link icon
 *
 * @details Generates HTML for a styled hamburger menu button that links
 *          back to the main menu page.
 *
 * @return String HTML markup for menu icon
 */
#define AUTONETWORK_LINK()                \
    (String(                              \
        "<a href=\"/_an\" style=\""       \
        "display: inline-flex; "          \
        "flex-direction: column; "        \
        "justify-content: space-around; " \
        "width: 28px; "                   \
        "height: 24px; "                  \
        "padding: 4px; "                  \
        "text-decoration: none; "         \
        "cursor: pointer; "               \
        "margin-right: 8px; "             \
        "margin-top: -8px; "              \
        "vertical-align: top;"            \
        "\" title=\"AutoNetwork Menu\">"  \
        "<span style=\""                  \
        "display: block; "                \
        "width: 100%; "                   \
        "height: 3px; "                   \
        "background: white; "             \
        "border-radius: 2px; "            \
        "transition: all 0.3s;"           \
        "\"></span>"                      \
        "<span style=\""                  \
        "display: block; "                \
        "width: 100%; "                   \
        "height: 3px; "                   \
        "background: white; "             \
        "border-radius: 2px; "            \
        "transition: all 0.3s;"           \
        "\"></span>"                      \
        "<span style=\""                  \
        "display: block; "                \
        "width: 100%; "                   \
        "height: 3px; "                   \
        "background: white; "             \
        "border-radius: 2px; "            \
        "transition: all 0.3s;"           \
        "\"></span>"                      \
        "</a>"))

// Backward compatibility - ignore icon parameter
#define AUTONETWORK_LINK_INLINE() AUTONETWORK_LINK()

const char AUTONETWORK_MENU_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>AutoNetwork Menu</title>
    <link rel="stylesheet" href="/global.css">
    <!-- WARNING: DO NOT add inline <style> tags! All CSS must go in webpage_css.h -->
    <!-- AI: If you need to add styles, edit webpage_css.h, NOT this file! -->
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>⚙️ AutoNetwork</h1>
            <p>WiFi Configuration & Management</p>
        </div>

        <div class="menu">
            <a href="/" class="menu-item">
                <span class="menu-icon">🏠</span> Home
            </a>
            <a href="/_an/stats" class="menu-item">
                <span class="menu-icon">📊</span> Statistics
            </a>
            <a href="/_an/config" class="menu-item">
                <span class="menu-icon">🔧</span> Configure WiFi
            </a>
            <a href="/_an/open" class="menu-item">
                <span class="menu-icon">📋</span> Saved Credentials
            </a>
            <a href="/_an/ota" class="menu-item">
                <span class="menu-icon">🔄</span> OTA Update
            </a>
            <a href="/_an/disc" class="menu-item">
                <span class="menu-icon">🔌</span> Disconnect
            </a>
            <a href="/_an/reset" class="menu-item danger">
                <span class="menu-icon">⚠️</span> Reset Device
            </a>
        </div>

        <div class="footer">
            AutoNetwork v1.0 | ESP32
        </div>
    </div>
</body>
</html>
)=====";

#endif // AUTONETWORK_MENU_PAGE_H
