// ****************************************************************************
// Title        : AutoNetwork CSS Stylesheet
// Filename     : 'webpage_css.h'
// Target MCU   : Espressif ESP32
// Description  : Global CSS styles for all AutoNetwork web pages.
//
// ****************************************************************************
#ifndef WEBPAGE_CSS_H
#define WEBPAGE_CSS_H

#include <Arduino.h>

// WEBPAGE Embedded CSS content
const char WEBPAGE_CSS[] PROGMEM = R"=====(
:root {
    /* Default Theme: Blue */
    --primary-color: #4299e1;
    --primary-hover: #3182ce;
    --secondary-color: #4a5568;
    --text-color: #333;
    --light-text-color: #f8f9fa;
    --container-bg: white;
    --danger-color: #dc3545;
    --success-color: #28a745;
    --font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
}

/* Optional Theme: Purple
   To use purple theme, uncomment this block and comment out the blue theme above
:root {
    --primary-color: #667eea;
    --primary-hover: #5568d3;
}
*/

* {
    margin: 0;
    padding: 0;
    box-sizing: border-box;
}

body {
    font-family: var(--font-family);
    background: linear-gradient(135deg, #6b7280 0%, #1e3a8a 100%);
    min-height: 100vh;
    display: flex;
    justify-content: center;
    align-items: center;
    padding: 20px;
    color: var(--text-color);
}

.container {
    background: var(--container-bg);
    border-radius: 16px;
    box-shadow: 0 20px 60px rgba(0, 0, 0, 0.3);
    max-width: 700px;
    width: 100%;
    overflow: hidden;
}

.header {
    background: var(--secondary-color);
    color: var(--light-text-color);
    padding: 30px 20px;
    text-align: center;
}

.header h1 {
    font-size: 28px;
    margin-bottom: 8px;
    font-weight: 600;
    color: var(--light-text-color);
}

.header p {
    font-size: 14px;
    opacity: 0.9;
    margin: 0;
}

.btn {
    padding: 12px 24px;
    border-radius: 8px;
    border: none;
    font-size: 16px;
    font-weight: 500;
    cursor: pointer;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    transition: all 0.3s ease;
    text-decoration: none;
}

.btn-primary {
    background: var(--primary-color);
    color: white;
}

.btn-primary:hover {
    background: var(--primary-hover);
}

.btn-danger {
    background: var(--danger-color);
    color: white;
}

.btn-danger:hover {
    background: #c82333;
}

.form-group {
    margin-bottom: 20px;
}

.form-group label {
    display: block;
    font-weight: 500;
    margin-bottom: 8px;
    color: var(--secondary-color);
}

.form-group input {
    width: 100%;
    padding: 12px 16px;
    border: 2px solid #e2e8f0;
    border-radius: 8px;
    font-size: 16px;
    transition: border-color 0.3s ease;
}

.form-group input:focus {
    outline: none;
    border-color: var(--primary-color);
}

.back-link {
    display: block;
    text-align: center;
    padding: 20px;
    color: var(--primary-color);
    text-decoration: none;
    font-weight: 500;
    transition: color 0.3s;
    border-top: 1px solid #e9ecef;
}

.back-link:hover {
    color: var(--primary-hover);
}

.spinner {
    display: inline-block;
    width: 20px;
    height: 20px;
    border: 2px solid transparent;
    border-top: 2px solid currentColor;
    border-radius: 50%;
    animation: spin 1s linear infinite;
}

@keyframes spin {
    to {
        transform: rotate(360deg);
    }
}

.menu {
    display: flex;
    flex-direction: column;
    gap: 10px;
    padding: 20px;
}

.menu-item {
    display: flex;
    align-items: center;
    padding: 15px 20px;
    background: #f7fafc;
    border-radius: 8px;
    text-decoration: none;
    color: var(--text-color);
    font-weight: 500;
    transition: background 0.3s ease, transform 0.2s ease;
    box-shadow: 0 2px 4px rgba(0, 0, 0, 0.05);
}

.menu-item:hover {
    background: #edf2f7;
    transform: translateY(-2px);
}

.menu-icon {
    font-size: 24px;
    margin-right: 15px;
    color: var(--primary-color);
}

.menu-item.danger {
    color: var(--danger-color);
}

.menu-item.danger .menu-icon {
    color: var(--danger-color);
}

.menu-item.danger:hover {
    background: #ffebeb;
}

.footer {
    text-align: center;
    padding: 20px;
    font-size: 12px;
    color: #a0aec0;
    border-top: 1px solid #e2e8f0;
}


/* CSS from webpage_creds.h */
* {
    margin: 0;
    padding: 0;
    box-sizing: border-box;
}

body {
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
    background: linear-gradient(135deg, #6b7280 0%, #1e3a8a 100%);
    min-height: 100vh;
    display: flex;
    justify-content: center;
    align-items: center;
    padding: 20px;
}

.container {
    background: white;
    border-radius: 16px;
    box-shadow: 0 20px 60px rgba(0, 0, 0, 0.3);
    max-width: 600px;
    width: 100%;
    overflow: hidden;
}

.header {
    background: #4a5568;
    color: white;
    padding: 30px 20px;
    text-align: center;
    position: relative;
}

.header h1 {
    font-size: 28px;
    margin-bottom: 8px;
    color: white;
}

.header p {
    opacity: 0.9;
    font-size: 14px;
}

.credential-list {
    padding: 20px;
    min-height: 200px;
}

.credential-item {
    background: #f8f9fa;
    border-radius: 10px;
    padding: 16px;
    margin-bottom: 12px;
    border-left: 4px solid #667eea;
    display: flex;
    align-items: center;
    gap: 12px;
}

.credential-item.no-creds {
    border-left: 4px solid #6c757d;
    color: #6c757d;
    text-align: center;
    padding: 30px 16px;
    display: block;
}

.credential-checkbox {
    width: 20px;
    height: 20px;
    cursor: pointer;
    flex-shrink: 0;
}

.credential-content {
    flex: 1;
}

.credential-ssid {
    font-weight: 600;
    font-size: 16px;
    margin-bottom: 8px;
    color: #212529;
}

.credential-info {
    font-size: 12px;
    color: #6c757d;
}

.badge {
    display: inline-block;
    padding: 2px 8px;
    border-radius: 4px;
    font-size: 11px;
    margin-left: 8px;
    font-weight: 600;
}

.badge-enterprise {
    background: #28a745;
    color: white;
}

.actions-bar {
    padding: 20px;
    background: #f8f9fa;
    display: flex;
    gap: 12px;
    justify-content: space-between;
    align-items: center;
}

.btn {
    padding: 12px 24px;
    border: none;
    border-radius: 8px;
    font-size: 14px;
    font-weight: 600;
    cursor: pointer;
    transition: all 0.3s;
    text-decoration: none;
    display: inline-block;
}

.btn-primary {
    background: #667eea;
    color: white;
}

.btn-primary:hover {
    background: #5568d3;
}

.btn-primary:disabled {
    background: #6c757d;
    cursor: not-allowed;
    opacity: 0.5;
}

.btn-danger {
    background: #dc3545;
    color: white;
}

.btn-danger:hover {
    background: #c82333;
}

.btn-danger:disabled {
    background: #6c757d;
    cursor: not-allowed;
    opacity: 0.5;
}

.select-info {
    font-size: 13px;
    color: #6c757d;
}

.back-link {
    display: block;
    text-align: center;
    padding: 20px;
    color: #667eea;
    text-decoration: none;
    font-weight: 500;
    transition: color 0.3s;
}

.back-link:hover {
    color: #5568d3;
}


/* CSS from webpage_disconnect.h */
* {
    margin: 0;
    padding: 0;
    box-sizing: border-box;
}

body {
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
    background: linear-gradient(135deg, #6b7280 0%, #1e3a8a 100%);
    min-height: 100vh;
    display: flex;
    align-items: center;
    justify-content: center;
    padding: 20px;
}

.container {
    background: white;
    border-radius: 16px;
    box-shadow: 0 20px 60px rgba(0, 0, 0, 0.3);
    padding: 50px 40px;
    text-align: center;
    max-width: 450px;
    width: 100%;
}

.header {
    background: #4a5568;
    color: white;
    padding: 20px;
    border-radius: 12px;
    margin: -50px -40px 30px -40px;
    text-align: center;
}

.header h1 {
    font-size: 28px;
    margin: 0;
    color: white;
}

.icon {
    font-size: 64px;
    margin: 20px 0;
}

p {
    color: #6c757d;
    font-size: 16px;
    margin: 15px 0;
    line-height: 1.6;
}

.success-message {
    color: #28a745;
    font-weight: 600;
    font-size: 18px;
    margin: 20px 0;
}

.countdown-text {
    margin-top: 30px;
    font-size: 14px;
    color: #6c757d;
}

#countdown {
    font-weight: 600;
    color: #667eea;
    font-size: 16px;
}

.button {
    display: inline-block;
    background: #667eea;
    color: white;
    padding: 12px 30px;
    border-radius: 8px;
    text-decoration: none;
    margin-top: 20px;
    transition: background 0.3s;
    font-weight: 500;
}

.button:hover {
    background: #5568d3;
}


/* CSS from webpage_menu.h */
* {
    margin: 0;
    padding: 0;
    box-sizing: border-box;
}

body {
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
    background: linear-gradient(135deg, #6b7280 0%, #1e3a8a 100%);
    min-height: 100vh;
    display: flex;
    justify-content: center;
    align-items: center;
    padding: 20px;
}

.container {
    background: white;
    border-radius: 16px;
    box-shadow: 0 20px 60px rgba(0, 0, 0, 0.3);
    max-width: 500px;
    width: 100%;
    overflow: hidden;
}

.header {
    background: #4a5568;
    color: white;
    padding: 30px 20px;
    text-align: center;
}

.header h1 {
    font-size: 28px;
    margin-bottom: 8px;
}

.header p {
    opacity: 0.9;
    font-size: 14px;
}

.status {
    padding: 20px;
    background: #f8f9fa;
    border-bottom: 1px solid #e9ecef;
}

.status-item {
    display: flex;
    justify-content: space-between;
    padding: 8px 0;
    font-size: 14px;
}

.status-label {
    font-weight: 600;
    color: #6c757d;
}

.status-value {
    color: #212529;
}

.status-connected {
    color: #28a745;
    font-weight: 600;
}

.status-disconnected {
    color: #dc3545;
    font-weight: 600;
}

.menu {
    padding: 10px;
}

.menu-item {
    display: block;
    padding: 16px 20px;
    margin: 8px 0;
    background: white;
    border: 2px solid #e9ecef;
    border-radius: 10px;
    color: #495057;
    text-decoration: none;
    transition: all 0.3s ease;
    font-weight: 500;
}

.menu-item:hover {
    border-color: #667eea;
    background: #f8f9ff;
    transform: translateX(4px);
}

.menu-item.danger:hover {
    border-color: #dc3545;
    background: #fff5f5;
}

.menu-icon {
    display: inline-block;
    width: 24px;
    text-align: center;
    margin-right: 12px;
}

.footer {
    padding: 20px;
    text-align: center;
    color: #6c757d;
    font-size: 12px;
    border-top: 1px solid #e9ecef;
}


/* CSS from webpage_ota.h */
* {
    margin: 0;
    padding: 0;
    box-sizing: border-box;
}

body {
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
    background: linear-gradient(135deg, #6b7280 0%, #1e3a8a 100%);
    min-height: 100vh;
    padding: 20px;
}

.container {
    background: white;
    border-radius: 16px;
    box-shadow: 0 20px 60px rgba(0, 0, 0, 0.3);
    max-width: 700px;
    margin: 0 auto;
    overflow: hidden;
}

.header {
    background: #4a5568;
    color: white;
    padding: 30px 20px;
    text-align: center;
    position: relative;
}

.header h1 {
    font-size: 28px;
    margin-bottom: 8px;
}

.header p {
    opacity: 0.9;
    font-size: 14px;
}

.content {
    padding: 30px 20px;
}

.upload-section {
    text-align: center;
    margin: 40px 0;
}

.upload-button {
    background: #3b82f6;
    color: white;
    border: none;
    padding: 15px 30px;
    font-size: 16px;
    border-radius: 8px;
    cursor: pointer;
    display: inline-flex;
    align-items: center;
    gap: 10px;
    transition: background 0.3s;
}

.upload-button:hover {
    background: #2563eb;
}

.upload-button svg {
    width: 20px;
    height: 20px;
}

input[type="file"] {
    display: none;
}

.progress-section {
    margin: 40px 0;
}

.progress-section.hidden {
    display: none;
}

.progress-title {
    text-align: center;
    margin-bottom: 15px;
    color: #4a5568;
    font-size: 14px;
}

.progress-container {
    background: #e5e7eb;
    border-radius: 10px;
    height: 10px;
    overflow: hidden;
    margin-bottom: 10px;
}

.progress-bar {
    background: #3b82f6;
    height: 100%;
    width: 0%;
    transition: width 0.3s;
}

.progress-value {
    text-align: center;
    font-size: 14px;
    color: #4a5568;
}

.result-section {
    margin: 40px 0;
}

.result-section.hidden {
    display: none;
}

.result-icon {
    width: 60px;
    height: 60px;
    margin: 0 auto 20px;
}

.result-icon.success {
    color: #10b981;
}

.result-icon.error {
    color: #ef4444;
}

.result-title {
    font-size: 20px;
    margin-bottom: 10px;
    color: #1f2937;
}

.result-message {
    color: #6b7280;
    font-size: 14px;
    margin-bottom: 20px;
}

.back-button {
    background: #6b7280;
    color: white;
    border: none;
    padding: 10px 20px;
    font-size: 14px;
    border-radius: 6px;
    cursor: pointer;
    display: inline-flex;
    align-items: center;
    gap: 8px;
}

.back-button:hover {
    background: #4b5563;
}

.settings {
    margin-top: 40px;
    padding-top: 30px;
    border-top: 1px solid #e5e7eb;
}

.settings-title {
    text-align: center;
    color: #6b7280;
    font-size: 12px;
    text-transform: uppercase;
    letter-spacing: 1px;
    margin-bottom: 20px;
}

.setting-row {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 15px 0;
}

.setting-label {
    color: #1f2937;
    font-size: 15px;
}

select {
    padding: 8px 12px;
    border: 1px solid #d1d5db;
    border-radius: 6px;
    font-size: 14px;
    color: #1f2937;
    background: white;
}

.hidden {
    display: none;
}

.back-link {
    display: block;
    text-align: center;
    padding: 20px;
    color: #667eea;
    text-decoration: none;
    font-weight: 500;
}

.back-link:hover {
    color: #764ba2;
}


/* CSS from webpage_redirect.h */
* {
    margin: 0;
    padding: 0;
    box-sizing: border-box;
}

body {
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
    background: linear-gradient(135deg, #4a5568 0%, #2d3748 100%);
    min-height: 100vh;
    display: flex;
    align-items: center;
    justify-content: center;
    padding: 20px;
}

.container {
    background: white;
    border-radius: 16px;
    box-shadow: 0 20px 60px rgba(0, 0, 0, 0.3);
    padding: 40px;
    text-align: center;
    max-width: 400px;
}

h1 {
    color: #2d3748;
    font-size: 24px;
    margin-bottom: 15px;
}

p {
    color: #718096;
    font-size: 16px;
    line-height: 1.6;
}

.spinner {
    border: 4px solid #e2e8f0;
    border-top: 4px solid #4a5568;
    border-radius: 50%;
    width: 40px;
    height: 40px;
    animation: spin 1s linear infinite;
    margin: 20px auto 0;
}

@keyframes spin {
    0% {
        transform: rotate(0deg);
    }

    100% {
        transform: rotate(360deg);
    }
}

a {
    color: #4a5568;
    text-decoration: none;
    font-weight: 600;
}

a:hover {
    text-decoration: underline;
}


/* CSS from webpage_reset.h */
* {
    margin: 0;
    padding: 0;
    box-sizing: border-box;
}

body {
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
    background: linear-gradient(135deg, #6b7280 0%, #1e3a8a 100%);
    min-height: 100vh;
    display: flex;
    align-items: center;
    justify-content: center;
    padding: 20px;
}

.container {
    background: white;
    border-radius: 16px;
    box-shadow: 0 20px 60px rgba(0, 0, 0, 0.3);
    padding: 50px 40px;
    text-align: center;
    max-width: 450px;
    width: 100%;
}

.header {
    background: #4a5568;
    color: white;
    padding: 20px;
    border-radius: 12px;
    margin: -50px -40px 30px -40px;
    text-align: center;
}

.header h1 {
    font-size: 28px;
    margin: 0;
    color: white;
}

.spinner {
    border: 4px solid #f3f3f3;
    border-top: 4px solid #667eea;
    border-radius: 50%;
    width: 50px;
    height: 50px;
    animation: spin 1s linear infinite;
    margin: 30px auto;
}

@keyframes spin {
    0% {
        transform: rotate(0deg);
    }

    100% {
        transform: rotate(360deg);
    }
}

p {
    color: #6c757d;
    font-size: 16px;
    margin: 15px 0;
    line-height: 1.6;
}

.countdown-text {
    margin-top: 30px;
    font-size: 14px;
    color: #6c757d;
}

#countdown {
    font-weight: 600;
    color: #667eea;
    font-size: 16px;
}

.icon {
    font-size: 48px;
    margin-bottom: 10px;
}


/* CSS from webpage_stats.h */
* {
    margin: 0;
    padding: 0;
    box-sizing: border-box;
}

body {
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
    background: linear-gradient(135deg, #6b7280 0%, #1e3a8a 100%);
    min-height: 100vh;
    padding: 20px;
}

.container {
    background: white;
    border-radius: 16px;
    box-shadow: 0 20px 60px rgba(0, 0, 0, 0.3);
    max-width: 700px;
    margin: 0 auto;
    overflow: hidden;
}

.header {
    background: #4a5568;
    color: white;
    padding: 30px 20px;
    text-align: center;
    position: relative;
}

.header h1 {
    font-size: 28px;
    margin-bottom: 8px;
}

.header p {
    opacity: 0.9;
    font-size: 14px;
}

.stats-section {
    padding: 20px;
}

.section-title {
    font-size: 18px;
    font-weight: 600;
    color: #667eea;
    margin: 20px 0 12px 0;
    padding-bottom: 8px;
    border-bottom: 2px solid #e9ecef;
}

.stats-section:first-child .section-title:first-child {
    margin-top: 0;
}

.stat-row {
    display: flex;
    justify-content: space-between;
    padding: 12px 0;
    border-bottom: 1px solid #f1f3f5;
}

.stat-row:last-child {
    border-bottom: none;
}

.stat-label {
    font-weight: 600;
    color: #6c757d;
    font-size: 14px;
}

.stat-value {
    color: #212529;
    font-size: 14px;
    text-align: right;
    font-family: 'Courier New', monospace;
}

.stat-value.good {
    color: #28a745;
    font-weight: 600;
}

.stat-value.warning {
    color: #ffc107;
    font-weight: 600;
}

.stat-value.bad {
    color: #dc3545;
    font-weight: 600;
}

.back-link {
    display: block;
    text-align: center;
    padding: 20px;
    color: #667eea;
    text-decoration: none;
    font-weight: 500;
}

.back-link:hover {
    color: #764ba2;
}


/* CSS from webpage_test.h */
* {
    margin: 0;
    padding: 0;
    box-sizing: border-box;
}

.container {
    background: white;
    border-radius: 16px;
    box-shadow: 0 20px 60px rgba(0, 0, 0, 0.3);
    padding: 30px;
    max-width: 600px;
    margin: 40px auto;
}

h1 {
    color: #2d3748;
    margin-bottom: 20px;
    font-size: 24px;
}

button {
    background: #667eea;
    color: white;
    border: none;
    padding: 12px 30px;
    border-radius: 8px;
    font-size: 16px;
    font-weight: 600;
    cursor: pointer;
    transition: all 0.3s ease;
    margin: 10px 0;
}

button:hover {
    background: #5a67d8;
    transform: translateY(-2px);
    box-shadow: 0 4px 12px rgba(102, 126, 234, 0.4);
}

button:active {
    transform: translateY(0);
}

#output {
    background: #f7fafc;
    border: 1px solid #e2e8f0;
    border-radius: 8px;
    padding: 20px;
    margin-top: 20px;
    min-height: 100px;
    font-family: 'Courier New', monospace;
    font-size: 14px;
    line-height: 1.6;
    color: #2d3748;
    white-space: pre-wrap;
    word-wrap: break-word;
}

.success {
    color: #38a169;
    font-weight: 600;
}

.error {
    color: #e53e3e;
    font-weight: 600;
}

.info {
    color: #3182ce;
})=====";

#endif // WEBPAGE_CSS_H