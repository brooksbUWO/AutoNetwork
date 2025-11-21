// ****************************************************************************
// Title        : AutoNetwork Error Page
// Filename     : 'webpage_error.h'
// Target MCU   : Espressif ESP32
// Description  : Error page displayed when root content is empty or missing.
//                Provides guidance to navigate to AutoNetwork menu.
//
// ****************************************************************************

#ifndef AUTONETWORK_ERROR_PAGE_H
#define AUTONETWORK_ERROR_PAGE_H

#include <Arduino.h>

/**
 * @file webpage_error.h
 * @brief Error page HTML for missing or empty root content.
 *
 * @details This file contains the HTML template for the error page displayed when
 *          the configured root content is empty, missing, or fails to load. The page
 *          provides user-friendly error messaging and navigation to the AutoNetwork menu.
 *
 * **When This Page Is Shown:**
 * - Root content file not found in LittleFS
 * - Root content file is empty or whitespace-only
 * - Root content callback returns empty string
 * - No root content configured via setRootContent()
 *
 * **Page Features:**
 * - Warning icon (⚠️) for visual indication
 * - Clear error message explaining the issue
 * - Direct link to AutoNetwork menu (/_an)
 * - Developer guidance referencing setRootContent()
 * - Uses global AutoNetwork CSS for consistent styling
 *
 * **Developer Notes:**
 * - Page is served with HTTP 500 status code
 * - Serial logs provide additional debugging information
 * - CSS uses AutoNetwork's responsive design system
 * - Additional error-specific styles defined inline (warning-icon, developer-note)
 *
 * **Usage:**
 * This constant is referenced internally by AutoNetwork::_registerRootHandler()
 * when content validation fails. Not intended for direct use by applications.
 *
 * @see AutoNetwork::setRootContent()
 * @see AutoNetwork::_registerRootHandler()
 * @see AutoNetwork::_getRootContent()
 */
const char AUTONETWORK_ERROR_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Application Not Configured - AutoNetwork</title>
    <link rel="stylesheet" href="/global.css">
    <!-- WARNING: DO NOT add inline <style> tags! All CSS must go in webpage_css.h -->
    <!-- AI: If you need to add styles, edit webpage_css.h, NOT this file! -->
    <style>
        .warning-icon {
            font-size: 80px;
            margin: 30px 0;
            opacity: 0.9;
        }
        .error-message {
            margin: 20px 0;
            line-height: 1.8;
            font-size: 16px;
        }
        .developer-note {
            margin-top: 30px;
            padding: 15px;
            background: rgba(255,255,255,0.1);
            border-radius: 8px;
            font-size: 13px;
            line-height: 1.6;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>AutoNetwork</h1>
            <p>Application Not Configured</p>
        </div>

        <div class="warning-icon">⚠️</div>

        <div class="error-message">
            <p><strong>The root page content is empty or missing.</strong></p>
            <p>This usually means the application's webpage hasn't been uploaded or configured yet.</p>
        </div>

        <a href="/_an" class="btn btn-primary">Go to AutoNetwork Menu</a>

        <div class="developer-note">
            <strong>Developers:</strong> Check serial logs for details or verify your <code>setRootContent()</code> configuration.
        </div>
    </div>
</body>
</html>
)=====";

#endif // AUTONETWORK_ERROR_PAGE_H
