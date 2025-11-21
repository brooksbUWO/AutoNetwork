/**
 * @file webpage_creds.h
 * @brief Embedded credentials management web page for AutoNetwork
 * @version 1.1.0
 * @date 2025-11-11
 *
 * Displays saved WiFi credentials and provides management interface.
 * Allows users to view, delete, and reprioritize stored networks.
 *
 * @par Endpoints Used
 *      - `/_an/saved` - Retrieve saved credentials list
 *      - `/_an/connect` - Connect to saved network
 *
 * @par Features
 *      - List all saved WiFi credentials
 *      - Delete individual credentials
 *      - View connection priority
 *      - Distinguish WPA2-PSK from WPA2 Enterprise networks
 */

#ifndef AUTONETWORK_CREDS_PAGE_H
#define AUTONETWORK_CREDS_PAGE_H

#include <Arduino.h>

/**
 * @brief Embedded HTML content for credentials management page
 *
 * @details Stored in PROGMEM to minimize RAM usage. Displays saved
 *          networks with management controls.
 */
const char AUTONETWORK_CREDENTIALS_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Saved WiFi Credentials</title>
    <link rel="stylesheet" href="/global.css">
    <!-- WARNING: DO NOT add inline <style> tags! All CSS must go in webpage_css.h -->
    <!-- AI: If you need to add styles, edit webpage_css.h, NOT this file! -->
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>📋 Saved WiFi Credentials</h1>
            <p>Stored Network Configurations</p>
        </div>
        <div class="credential-list">
            %CREDENTIALS%
        </div>
        <div class="actions-bar" id="actionsBar" style="display:none;">
            <span class="select-info" id="selectInfo">0 networks selected</span>
            <div style="display: flex; gap: 12px;">
                <button class="btn btn-primary" id="connectBtn" onclick="connectSelected()" disabled>
                    📡 Connect
                </button>
                <button class="btn btn-danger" id="deleteBtn" onclick="deleteSelected()" disabled>
                    🗑️ Delete Selected
                </button>
            </div>
        </div>
        <a href="/_an" class="back-link">← Back to Menu</a>
    </div>
    <script>
        function updateActionButtons() {
            const checkboxes = document.querySelectorAll('.credential-checkbox:checked');
            const connectBtn = document.getElementById('connectBtn');
            const deleteBtn = document.getElementById('deleteBtn');
            const selectInfo = document.getElementById('selectInfo');
            const count = checkboxes.length;

            selectInfo.textContent = count + ' network' + (count !== 1 ? 's' : '') + ' selected';

            // Connect button: only enabled when exactly 1 network selected
            connectBtn.disabled = count !== 1;

            // Delete button: enabled when 1 or more networks selected
            deleteBtn.disabled = count === 0;
        }

        function connectSelected() {
            const checkboxes = document.querySelectorAll('.credential-checkbox:checked');
            if (checkboxes.length !== 1) {
                alert('Please select exactly one network to connect.');
                return;
            }

            const ssid = checkboxes[0].value;

            if (!confirm('Connect to network: ' + ssid + '. The device will disconnect from the current network and connect to this saved network.')) {
                return;
            }

            // Show connecting message
            alert('Connecting to ' + ssid + '... Please wait while the device connects to the network.');

            // Send connect request
            fetch('/_an/connect_saved', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ ssid: ssid })
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    alert('Connection initiated to ' + ssid + '. Please check the device status.');
                    window.location.href = '/_an';
                } else {
                    alert('Error: ' + data.message);
                }
            })
            .catch(error => {
                alert('Failed to initiate connection: ' + error);
            });
        }

        function deleteSelected() {
            const checkboxes = document.querySelectorAll('.credential-checkbox:checked');
            if (checkboxes.length === 0) return;

            const ssids = Array.from(checkboxes).map(cb => cb.value);
            const count = ssids.length;

            if (!confirm('Delete ' + count + ' network' + (count !== 1 ? 's' : '') + '? This action cannot be undone.')) {
                return;
            }

            // Send delete request
            fetch('/_an/delete_creds', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ ssids: ssids })
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    alert(data.deleted + ' network' + (data.deleted !== 1 ? 's' : '') + ' deleted successfully!');
                    window.location.reload();
                } else {
                    alert('Error: ' + data.message);
                }
            })
            .catch(error => {
                alert('Failed to delete networks: ' + error);
            });
        }

        // Show actions bar if there are any credentials
        document.addEventListener('DOMContentLoaded', function() {
            const hasCredentials = document.querySelectorAll('.credential-checkbox').length > 0;
            if (hasCredentials) {
                document.getElementById('actionsBar').style.display = 'flex';
            }
        });
    </script>
</body>
</html>
)=====";

#endif // AUTONETWORK_CREDS_PAGE_H
