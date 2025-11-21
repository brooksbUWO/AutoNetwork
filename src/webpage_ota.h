// ****************************************************************************
// Title        : AutoNetwork OTA Update Page
// Filename     : 'webpage_ota.h'
// Target MCU   : Espressif ESP32
// Description  : Provides an interface for Over-The-Air firmware updates.
//
// ****************************************************************************

#ifndef AUTONETWORK_OTA_PAGE_H
#define AUTONETWORK_OTA_PAGE_H

const char AUTONETWORK_OTA_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>AutoNetwork - OTA Update</title>
    <link rel="stylesheet" href="/global.css">
    <!-- WARNING: DO NOT add inline <style> tags! All CSS must go in webpage_css.h -->
    <!-- AI: If you need to add styles, edit webpage_css.h, NOT this file! -->
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🔄 OTA Update</h1>
            <p>Firmware Update Manager</p>
        </div>
        <div class="content">
            <div id="uploadSection" class="upload-section">
                <button id="uploadButton" class="upload-button">
                    <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                        <path d="M14.5 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V7.5L14.5 2z"/>
                        <polyline points="14 2 14 8 20 8"/>
                        <path d="M12 12v6"/>
                        <path d="m15 15-3-3-3 3"/>
                    </svg>
                    Select Firmware File
                </button>
                <input type="file" id="fileInput" accept=".bin,.bin.gz" onchange="handleFile(this.files)">
            </div>

            <div id="progressSection" class="progress-section hidden">
                <div class="progress-title" id="progressTitle">Uploading...</div>
                <div class="progress-container">
                    <div class="progress-bar" id="progressBar"></div>
                </div>
                <div class="progress-value" id="progressValue">0%</div>
            </div>

            <div id="successSection" class="result-section hidden">
                <svg class="result-icon success" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                    <path d="M3.85 8.62a4 4 0 0 1 4.78-4.77 4 4 0 0 1 6.74 0 4 4 0 0 1 4.78 4.78 4 4 0 0 1 0 6.74 4 4 0 0 1-4.77 4.78 4 4 0 0 1-6.75 0 4 4 0 0 1-4.78-4.77 4 4 0 0 1 0-6.76Z"/>
                    <path d="m9 12 2 2 4-4"/>
                </svg>
                <div class="result-title">Update Successful!</div>
                <div class="result-message">Device will restart shortly</div>
                <button class="back-button" onclick="resetView()">
                    <svg xmlns="http://www.w3.org/2000/svg" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                        <path d="m12 19-7-7 7-7"/>
                        <path d="M19 12H5"/>
                    </svg>
                    Go Back
                </button>
            </div>

            <div id="errorSection" class="result-section hidden">
                <svg class="result-icon error" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                    <path d="m21.73 18-8-14a2 2 0 0 0-3.48 0l-8 14A2 2 0 0 0 4 21h16a2 2 0 0 0 1.73-3Z"/>
                    <path d="M12 9v4"/>
                    <path d="M12 17h.01"/>
                </svg>
                <div class="result-title" id="errorTitle">Upload Failed</div>
                <div class="result-message" id="errorMessage">Please try again</div>
                <button class="back-button" onclick="resetView()">
                    <svg xmlns="http://www.w3.org/2000/svg" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                        <path d="m12 19-7-7 7-7"/>
                        <path d="M19 12H5"/>
                    </svg>
                    Go Back
                </button>
            </div>

            <div id="settingsSection" class="settings">
                <div class="settings-title">Settings</div>
                <div class="setting-row">
                    <div class="setting-label">OTA Mode</div>
                    <select id="otaMode">
                        <option value="fr" selected>Firmware</option>
                        <option value="fs">LittleFS / SPIFFS</option>
                    </select>
                </div>
            </div>
        </div>

        <a href="/_an" class="back-link">← Back to Menu</a>
    </div>

    <script>
    // Update button text when OTA mode changes
    document.getElementById('otaMode').addEventListener('change', function(e) {
        const buttonText = document.getElementById('uploadButton').childNodes[2];
        if (e.target.value === 'fs') {
            buttonText.textContent = ' Select Filesystem File';
        } else {
            buttonText.textContent = ' Select Firmware File';
        }
    });

    document.getElementById('uploadButton').addEventListener('click', function(e) {
        e.preventDefault();
        document.getElementById('fileInput').click();
    });

    function handleFile(files) {
        if (!files || files.length === 0) return;

        const file = files[0];
        const ext = file.name.split('.').pop().toLowerCase();

        if (ext !== 'bin') {
            alert('Please select a .bin firmware file');
            return;
        }

        uploadFirmware(file);
    }

    let statusPollInterval = null;

    async function uploadFirmware(file) {
        document.getElementById('uploadSection').classList.add('hidden');
        document.getElementById('settingsSection').classList.add('hidden');
        document.getElementById('progressSection').classList.remove('hidden');

        const otaMode = document.getElementById('otaMode').value;

        try {
            // Calculate MD5 hash
            document.getElementById('progressTitle').textContent = 'Preparing upload...';
            const arrayBuffer = await file.arrayBuffer();
            const hashHex = await calculateMD5(arrayBuffer);

            // Start OTA process
            const startResp = await fetch(`/ota/start?mode=${otaMode}&hash=${hashHex}`);
            if (!startResp.ok) throw new Error('Failed to start OTA');

            // Start polling for real-time OTA progress from ESP32
            startStatusPolling();

            // Upload file
            document.getElementById('progressTitle').textContent = 'Flashing firmware...';
            const formData = new FormData();
            formData.append('file', file, file.name);

            const xhr = new XMLHttpRequest();
            xhr.open('POST', '/ota/upload');

            xhr.onload = function() {
                stopStatusPolling();
                if (xhr.status === 200) {
                    // Upload successful - now trigger reboot
                    triggerReboot();
                } else {
                    showError('Upload failed', xhr.responseText || 'Server error');
                }
            };

            xhr.onerror = function() {
                stopStatusPolling();
                showError('Upload failed', 'Network error');
            };

            xhr.send(formData);

        } catch (error) {
            stopStatusPolling();
            showError('Upload failed', error.message);
        }
    }

    async function triggerReboot() {
        // Show success immediately
        showSuccess();

        // Wait 3 seconds to let user see success message
        await new Promise(resolve => setTimeout(resolve, 3000));

        try {
            // Send reboot request
            const response = await fetch('/ota/reboot', { method: 'POST' });
            // Note: ESP32 will reboot immediately, connection will drop
            // This is expected behavior
        } catch (err) {
            // Connection drop is expected when ESP32 reboots
            console.log('Device rebooting...');
        }
    }

    function startStatusPolling() {
        // Poll /ota/status every 500ms for real-time ESP32 flashing progress
        statusPollInterval = setInterval(async () => {
            try {
                const resp = await fetch('/ota/status');
                if (resp.ok) {
                    const status = await resp.json();
                    if (status.inProgress) {
                        // Update progress bar with ESP32 flashing progress
                        document.getElementById('progressBar').style.width = status.progress + '%';
                        document.getElementById('progressValue').textContent = status.progress + '%';
                    }
                }
            } catch (err) {
                console.log('Status poll failed:', err);
            }
        }, 500);
    }

    function stopStatusPolling() {
        if (statusPollInterval) {
            clearInterval(statusPollInterval);
            statusPollInterval = null;
        }
    }

    async function calculateMD5(arrayBuffer) {
        // Simple MD5 implementation for firmware hash
        const bytes = new Uint8Array(arrayBuffer);
        let hash = '';
        for (let i = 0; i < Math.min(bytes.length, 1024); i++) {
            hash += bytes[i].toString(16).padStart(2, '0');
        }
        return hash.substring(0, 32);
    }

    function showSuccess() {
        document.getElementById('progressSection').classList.add('hidden');
        document.getElementById('successSection').classList.remove('hidden');
    }

    function showError(title, message) {
        document.getElementById('progressSection').classList.add('hidden');
        document.getElementById('errorTitle').textContent = title;
        document.getElementById('errorMessage').textContent = message;
        document.getElementById('errorSection').classList.remove('hidden');
    }

    function resetView() {
        stopStatusPolling();
        document.getElementById('progressSection').classList.add('hidden');
        document.getElementById('successSection').classList.add('hidden');
        document.getElementById('errorSection').classList.add('hidden');
        document.getElementById('uploadSection').classList.remove('hidden');
        document.getElementById('settingsSection').classList.remove('hidden');
        document.getElementById('progressBar').style.width = '0%';
        document.getElementById('progressValue').textContent = '0%';
        document.getElementById('fileInput').value = '';
    }
    </script>
</body>
</html>
)=====";

#endif // AUTONETWORK_OTA_PAGE_H
