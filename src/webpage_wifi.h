/**
 * @file webpage_wifi.h
 * @brief Embedded WiFi configuration web page for AutoNetwork captive portal
 * @version 1.1.0
 * @date 2025-11-11
 *
 * Contains HTML, CSS, and JavaScript for the WiFi setup interface.
 * Provides network scanning, credential entry, and connection management.
 *
 * @par Endpoints Used
 *      - `/_an/scan` - WiFi network scanning
 *      - `/_an/connect` - WiFi connection submission
 *      - `/_an/status` - Connection status updates
 *
 * @par Features
 *      - Automatic network discovery with signal strength indicators
 *      - WPA2-PSK and WPA2 Enterprise support
 *      - Real-time connection status feedback
 *      - Responsive design for mobile devices
 */

#ifndef AUTONETWORK_WIFI_PAGE
#define AUTONETWORK_WIFI_PAGE

#include <Arduino.h>

/**
 * @brief Embedded HTML content for WiFi setup page
 *
 * @details Stored in PROGMEM to minimize RAM usage. Contains complete
 *          HTML document with inline CSS and JavaScript.
 */
const char AUTONETWORK_WIFI_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>AutoNetwork WiFi Setup</title>
  <link rel="stylesheet" href="/global.css">

  <style>
/* ========================================================================== */
/* WARNING: DO NOT ADD DUPLICATE STYLES FROM global.css                      */
/*                                                                            */
/* This page uses global.css for all base styles (body, container, header,   */
/* form-group, btn, etc.). Only page-specific styles should be added here.   */
/*                                                                            */
/* Before adding ANY CSS here:                                               */
/*   1. Check if it already exists in webpage_css.h                          */
/*   2. If it's reusable, add it to webpage_css.h instead                    */
/*   3. Only add WiFi-specific styles (step wizard, network list, etc.)      */
/*                                                                            */
/* AI Assistants: READ THIS BEFORE MODIFYING CSS!                            */
/* ========================================================================== */

.step-indicator {
  display: flex;
  justify-content: center;
  padding: 20px;
  background: #f7fafc;
  border-bottom: 1px solid #e2e8f0;
}

.step {
  display: flex;
  flex-direction: column;
  align-items: center;
  margin: 0 20px;
  position: relative;
}

.step:not(:last-child)::after {
  content: '';
  position: absolute;
  top: 20px;
  left: 100%;
  width: 40px;
  height: 2px;
  background: #e2e8f0;
  z-index: -1;
}

.step.active:not(:last-child)::after {
  background: #4299e1;
}

.step-number {
  width: 40px;
  height: 40px;
  border-radius: 50%;
  background: #e2e8f0;
  color: #718096;
  display: flex;
  align-items: center;
  justify-content: center;
  font-weight: 600;
  margin-bottom: 8px;
  transition: all 0.3s ease;
}

.step.active .step-number {
  background: #4299e1;
  color: white;
}

.step-text {
  font-size: 14px;
  color: #718096;
  font-weight: 500;
}

.step.active .step-text {
  color: #4299e1;
}

.step-content {
  display: none;
  padding: 30px;
}

.step-content.active {
  display: block;
}

.section {
  margin-bottom: 30px;
}

.section h2 {
  font-size: 24px;
  font-weight: 600;
  margin-bottom: 20px;
  color: #2d3748;
}

.form-group input:readonly {
  background: #f7fafc;
  color: #718096;
}

.password-toggle {
  margin-top: 8px;
}

.password-toggle input[type="checkbox"] {
  width: auto;
  margin-right: 8px;
}

.password-toggle label {
  margin-bottom: 0;
  font-weight: normal;
  cursor: pointer;
}

.btn {
  position: relative; /* Needed for loading spinner */
}

.btn-secondary {
  background: #e2e8f0;
  color: #4a5568;
}

.btn-secondary:hover {
  background: #cbd5e0;
}

.btn-text {
  transition: opacity 0.3s ease;
}

.btn.loading .btn-text {
  opacity: 0;
}

.btn .spinner {
  display: none;
  position: absolute;
}

.btn.loading .spinner {
  display: block;
}

/* @keyframes spin is in global.css */

.form-actions {
  display: flex;
  gap: 12px;
  justify-content: flex-end;
  margin-top: 30px;
}

.networks-list {
  border: 2px solid #e2e8f0;
  border-radius: 8px;
  overflow: hidden;
}

.network-item {
  padding: 16px;
  border-bottom: 1px solid #e2e8f0;
  cursor: pointer;
  transition: background-color 0.3s ease;
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.network-item:last-child {
  border-bottom: none;
}

.network-item:hover {
  background: #f7fafc;
}

.network-item.selected {
  background: #ebf8ff;
  border-color: #4299e1;
}

.network-name {
  font-weight: 500;
  color: #2d3748;
}

.network-security {
  font-size: 14px;
  color: #718096;
}

.loading {
  text-align: center;
  padding: 40px;
  color: #718096;
}

.status-section {
  background: #f7fafc;
  padding: 20px 30px;
  border-top: 1px solid #e2e8f0;
}

.status-item {
  display: flex;
  justify-content: space-between;
  margin-bottom: 8px;
}

.status-item:last-child {
  margin-bottom: 0;
}

.status-label {
  font-weight: 500;
  color: #4a5568;
}

.status-value {
  color: #2d3748;
}

.alert {
  padding: 16px;
  border-radius: 8px;
  margin: 20px 30px;
  font-weight: 500;
}

.alert.success {
  background: #c6f6d5;
  color: #22543d;
  border: 1px solid #9ae6b4;
}

.alert.error {
  background: #fed7d7;
  color: #742a2a;
  border: 1px solid #fc8181;
}

.alert.warning {
  background: #fefcbf;
  color: #744210;
  border: 1px solid #f6e05e;
}

.alert.info {
  background: #bee3f8;
  color: #2a4365;
  border: 1px solid #90cdf4;
}

@media (max-width: 768px) {
  body {
    padding: 10px;
  }
  .container {
    margin: 0;
    border-radius: 0;
  }
  .header {
    padding: 20px;
  }
  .header h1 {
    font-size: 24px;
  }
  .step-indicator {
    padding: 15px 10px;
  }
  .step {
    margin: 0 10px;
  }
  .step-text {
    font-size: 12px;
  }
  .step-content {
    padding: 20px;
  }
  .form-actions {
    flex-direction: column;
  }
  .btn {
    width: 100%;
    margin-bottom: 10px;
  }
}

.enterprise-fields {
  margin-top: 16px;
  padding-top: 16px;
  border-top: 1px solid #e2e8f0;
}

.help-text {
  display: block;
  margin-top: 6px;
  font-size: 13px;
  color: #718096;
}

.enterprise-badge {
  display: inline-block;
  padding: 2px 8px;
  background: #ebf8ff;
  color: #2c5282;
  border-radius: 4px;
  font-size: 12px;
  font-weight: 500;
  margin-left: 8px;
}

.network-security.enterprise {
  color: #2c5282;
  font-weight: 500;
}

.networks-container {
  max-height: 300px;
  overflow-y: auto;
}

.network-info {
  flex: 1;
}

.network-details {
  display: flex;
  gap: 12px;
  margin-top: 4px;
  font-size: 14px;
}

.network-arrow {
  color: #718096;
  font-size: 18px;
  font-weight: bold;
}

.security.open {
  color: #38a169;
}

.security.secured {
  color: #e53e3e;
}

.signal.excellent {
  color: #38a169;
}

.signal.good {
  color: #68d391;
}

.signal.fair {
  color: #f6e05e;
}

.signal.poor {
  color: #fc8181;
}

.no-networks {
  text-align: center;
  padding: 40px;
  color: #718096;
}

.error {
  text-align: center;
  padding: 20px;
  color: #e53e3e;
}

.error button {
  margin-top: 10px;
  padding: 8px 16px;
  background: #e53e3e;
  color: white;
  border: none;
  border-radius: 4px;
  cursor: pointer;
}

/* .back-link styles are in global.css */
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <h1>AutoNetwork WiFi Setup</h1>
      <p>Configure your WiFi connection</p>
    </div>

    <div class="step-indicator">
      <div class="step active" id="step-1-indicator">
        <span class="step-number">1</span>
        <span class="step-text">Scan Networks</span>
      </div>
      <div class="step" id="step-2-indicator">
        <span class="step-number">2</span>
        <span class="step-text">Connect</span>
      </div>
      <div class="step" id="step-3-indicator">
        <span class="step-number">3</span>
        <span class="step-text">Configure</span>
      </div>
    </div>

    <div class="step-content active" id="step-1">
      <div class="section">
        <h2>Available Networks</h2>
        <button id="refresh-btn" class="btn btn-secondary">
          <span class="btn-text">Refresh Networks</span>
          <span class="spinner" id="refresh-spinner"></span>
        </button>
        <div id="networks-list" class="networks-list">
          <div class="loading">Scanning for networks...</div>
        </div>
      </div>
    </div>

    <div class="step-content" id="step-2">
      <div class="section">
        <h2>WiFi Credentials</h2>
        <form id="wifi-form">
          <div class="form-group">
            <label for="ssid">Network Name (SSID)</label>
            <input type="text" id="ssid" name="ssid" required readonly>
          </div>

          <div class="form-group">
            <label for="password">Password</label>
            <input type="password" id="password" name="password" placeholder="Enter WiFi password">
            <div class="password-toggle">
              <input type="checkbox" id="show-password">
              <label for="show-password">Show password</label>
            </div>
          </div>

          <div class="form-group enterprise-fields" id="enterprise-fields" style="display: none;">
            <label for="netid">ID (Username/Identity)</label>
            <input type="text" id="netid" name="netid" placeholder="Enter username or identity">
            <small class="help-text">For enterprise networks, enter your network username or identity</small>
          </div>

          <div class="form-actions">
            <button type="button" id="back-btn" class="btn btn-secondary">Back</button>
            <button type="submit" id="connect-btn" class="btn btn-primary">
              <span class="btn-text">Connect</span>
              <span class="spinner"></span>
            </button>
          </div>
        </form>
      </div>
    </div>

    <div class="step-content" id="step-3">
      <div class="section">
        <h2>Connection Status</h2>
        <div id="connection-result">
          <p>✅ Successfully connected to WiFi network!</p>
          <div style="margin-top: 20px; padding: 15px; background: #f7fafc; border-radius: 8px; border-left: 4px solid #667eea;">
            <div id="portal-retained-message" style="display: none;">
              <p style="margin: 0 0 10px 0; color: #48bb78;">Portal is running in AP+STA mode.</p>
              <a href="#" id="reload-link" style="display: inline-block; padding: 10px 20px; background: #667eea; color: white; text-decoration: none; border-radius: 4px; font-weight: 500;">Access Device on New Network</a>
            </div>
            <div id="portal-closing-message" style="display: none;">
              <p style="margin: 0; color: #718096;">Portal is closing. Reconnect to the WiFi network to access the device.</p>
            </div>
          </div>
        </div>
      </div>
    </div>

    <div class="status-section">
      <div class="status-item">
        <span class="status-label">Connection Status:</span>
        <span class="status-value" id="connection-status">Disconnected</span>
      </div>
      <div class="status-item">
        <span class="status-label">Portal State:</span>
        <span class="status-value" id="portal-state">Idle</span>
      </div>
    </div>

    <div id="alert" class="alert" style="display: none;">
      <span id="alert-message"></span>
    </div>

    <a href="/_an" class="back-link">← Back to Menu</a>
  </div>

  <script>
let networks = [];
let selectedNetwork = null;
let currentStep = 1;

document.addEventListener('DOMContentLoaded', function() {
    console.log('AutoNetwork: Page loaded');
    setupEventListeners();
    refreshNetworks();

    setInterval(updateStatus, 2000);
    updateStatus();
});

function setupEventListeners() {
    const refreshBtn = document.getElementById('refresh-btn');
    if (refreshBtn) {
        refreshBtn.addEventListener('click', refreshNetworks);
    }

    const backBtn = document.getElementById('back-btn');
    if (backBtn) {
        backBtn.addEventListener('click', () => showStep(1));
    }

    const wifiForm = document.getElementById('wifi-form');
    if (wifiForm) {
        wifiForm.addEventListener('submit', handleWifiSubmit);
    }

    const showPassword = document.getElementById('show-password');
    const passwordInput = document.getElementById('password');
    if (showPassword && passwordInput) {
        showPassword.addEventListener('change', function() {
            passwordInput.type = this.checked ? 'text' : 'password';
        });
    }
}

async function refreshNetworks() {
    console.log('AutoNetwork: Refreshing networks...');

    const refreshBtn = document.getElementById('refresh-btn');
    const networksList = document.getElementById('networks-list');

    if (refreshBtn) {
        refreshBtn.disabled = true;
        refreshBtn.innerHTML = '<span class="spinner"></span> Scanning...';
    }

    // Only show "Scanning..." message if the list is empty or has error message
    // This prevents flickering when re-scanning with existing results
    if (networksList && (networks.length === 0 || networksList.querySelector('.error'))) {
        networksList.innerHTML = '<div class="loading">Scanning for networks...</div>';
    }

    try {
        const response = await fetch('/_an/scan');

        if (response.status === 202) {
            setTimeout(refreshNetworks, 2000); // Poll every 2s while scan in progress
            return;
        }

        if (response.ok) {
            networks = await response.json();
            console.log('AutoNetwork: Found', networks.length, 'networks');
            displayNetworks();
        } else {
            throw new Error('Scan failed: ' + response.status);
        }

    } catch (error) {
        console.error('AutoNetwork: Scan error:', error);
        if (networksList) {
            networksList.innerHTML = '<div class="error">Failed to scan networks. <button onclick="refreshNetworks()">Try again</button></div>';
        }
    } finally {
        if (refreshBtn) {
            refreshBtn.disabled = false;
            refreshBtn.innerHTML = '<span class="btn-text">Refresh Networks</span>';
        }
    }
}

function displayNetworks() {
    const networksList = document.getElementById('networks-list');
    if (!networksList) return;

    if (networks.length === 0) {
        networksList.innerHTML = '<div class="no-networks">No networks found</div>';
        return;
    }

    let html = '<div class="networks-container">';

    networks.forEach((network, index) => {
        const isOpen = network.e === 0;
        const isEnterprise = network.e === 6 || network.e === 11;
        const signalStrength = getSignalStrength(network.r);

        let securityLabel = 'Secured';
        let securityClass = 'security secured';

        if (isOpen) {
            securityLabel = 'Open';
            securityClass = 'security open';
        } else if (isEnterprise) {
            securityLabel = 'WPA2 Enterprise';
            securityClass = 'security enterprise';
        }

        html += `
            <div class="network-item" onclick="selectNetwork(${index})">
                <div class="network-info">
                    <div class="network-name">${escapeHtml(network.s)}</div>
                    <div class="network-details">
                        <span class="signal ${signalStrength.class}">${signalStrength.text}</span>
                        <span class="${securityClass}">${securityLabel}</span>
                        ${isEnterprise ? '<span class="enterprise-badge">Enterprise</span>' : ''}
                    </div>
                </div>
                <div class="network-arrow">→</div>
            </div>
        `;
    });

    html += '</div>';
    networksList.innerHTML = html;
}

function selectNetwork(index) {
    selectedNetwork = networks[index];
    // The channel and bssid are already in the networks[index] object.
    // No need to re-read them from the DOM.
    console.log('AutoNetwork: Selected network:', selectedNetwork.s, 'on channel', selectedNetwork.c, 'with BSSID', selectedNetwork.b);

    const ssidInput = document.getElementById('ssid');
    if (ssidInput) {
        ssidInput.value = selectedNetwork.s;
    }

    const passwordInput = document.getElementById('password');
    if (passwordInput) {
        passwordInput.value = '';
        passwordInput.required = selectedNetwork.e !== 0;
    }

    const isEnterprise = selectedNetwork.e === 6 || selectedNetwork.e === 11;

    const enterpriseFields = document.getElementById('enterprise-fields');
    const netidInput = document.getElementById('netid');

    if (enterpriseFields && netidInput) {
        if (isEnterprise) {
            enterpriseFields.style.display = 'block';
            netidInput.required = true;
            netidInput.value = '';
            console.log('AutoNetwork: Enterprise network detected');
        } else {
            enterpriseFields.style.display = 'none';
            netidInput.required = false;
            netidInput.value = '';
        }
    }

    showStep(2);
}

function showStep(step) {
    currentStep = step;

    for (let i = 1; i <= 4; i++) {
        const stepContent = document.getElementById(`step-${i}`);
        const stepIndicator = document.getElementById(`step-${i}-indicator`);

        if (stepContent) {
            stepContent.classList.remove('active');
        }
        if (stepIndicator) {
            stepIndicator.classList.remove('active');
        }
    }

    const currentStepContent = document.getElementById(`step-${step}`);
    const currentStepIndicator = document.getElementById(`step-${step}-indicator`);

    if (currentStepContent) {
        currentStepContent.classList.add('active');
    }
    if (currentStepIndicator) {
        currentStepIndicator.classList.add('active');
    }
}

async function handleWifiSubmit(event) {
    event.preventDefault();

    const ssid = document.getElementById('ssid').value;
    const password = document.getElementById('password').value;
    const netidInput = document.getElementById('netid');
    const netid = netidInput ? netidInput.value : '';

    if (!ssid) {
        showAlert('Please select a network first', 'error');
        return;
    }

    if (selectedNetwork && selectedNetwork.e !== 0 && !password) {
        showAlert('Password required for secured networks', 'error');
        return;
    }

    const isEnterprise = selectedNetwork && (selectedNetwork.e === 6 || selectedNetwork.e === 11);
    if (isEnterprise && !netid) {
        showAlert('Username/Identity required for enterprise networks', 'error');
        return;
    }

    console.log('AutoNetwork: Connecting to', ssid, isEnterprise ? '(Enterprise)' : '', 'on channel', selectedNetwork.c);

    try {
        const connectBtn = document.getElementById('connect-btn');
        if (connectBtn) {
            connectBtn.disabled = true;
            connectBtn.innerHTML = '<span class="spinner"></span> Connecting...';
        }

        const payload = {
            credentials: {
                ssid: ssid,
                password: password,
                channel: selectedNetwork.c ? parseInt(selectedNetwork.c) : 0,
                bssid: selectedNetwork.b ? selectedNetwork.b : ""
            }
        };

        if (isEnterprise) {
            payload.credentials.enterprise = true;
            payload.credentials.netid = netid;
            console.log('AutoNetwork: Sending enterprise credentials');
        } else {
            payload.credentials.enterprise = false;
        }

        const response = await fetch('/_an/connect', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(payload)
        });

        if (response.ok) {
            showAlert('Connection successful!', 'success');
            showStep(3);
        } else {
            throw new Error('Connection failed: ' + response.status);
        }

    } catch (error) {
        console.error('AutoNetwork: Connection error:', error);
        showAlert('Failed to connect. Please try again.', 'error');
    } finally {
        const connectBtn = document.getElementById('connect-btn');
        if (connectBtn) {
            connectBtn.disabled = false;
            connectBtn.innerHTML = '<span class="btn-text">Connect</span>';
        }
    }
}

function getSignalStrength(rssi) {
    if (rssi >= -50) return { class: 'excellent', text: 'Excellent' };
    if (rssi >= -60) return { class: 'good', text: 'Good' };
    if (rssi >= -70) return { class: 'fair', text: 'Fair' };
    return { class: 'poor', text: 'Poor' };
}

function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

function showAlert(message, type = 'info') {
    const alert = document.getElementById('alert');
    const alertMessage = document.getElementById('alert-message');

    if (alert && alertMessage) {
        alertMessage.textContent = message;
        alert.className = `alert ${type}`;
        alert.style.display = 'block';

        setTimeout(() => {
            alert.style.display = 'none';
        }, 5000);
    }
}

async function updateStatus() {
    try {
        const response = await fetch('/_an/status');
        if (response.ok) {
            const status = await response.json();

            const connectionStatus = document.getElementById('connection-status');
            const portalState = document.getElementById('portal-state');

            if (connectionStatus) {
                let statusText = 'Disconnected';
                if (status.status === 3) {
                    statusText = 'Connected';
                } else if (status.status === 2) {
                    statusText = 'Connecting...';
                } else if (status.status === 4) {
                    statusText = 'Connection Failed';
                } else if (status.status === 5) {
                    statusText = 'Not Found';
                }
                connectionStatus.textContent = statusText;
            }

            if (portalState) {
                const stateNames = ['Idle', 'Connecting', 'Waiting', 'Success', 'Failed', 'Timeout'];
                portalState.textContent = stateNames[status.portal.state] || 'Unknown';
            }

            // Success handling
            if (status.status === 3 && status.localIP && status.localIP !== '0.0.0.0') {
                if (currentStep === 2) {
                    // Show success step
                    showStep(3);

                    // Show appropriate message based on portal retention
                    const portalRetainedMsg = document.getElementById('portal-retained-message');
                    const portalClosingMsg = document.getElementById('portal-closing-message');
                    const reloadLink = document.getElementById('reload-link');

                    if (status.portal.active) {
                        // Portal retained - device accessible on new IP via AP+STA mode
                        if (portalRetainedMsg) portalRetainedMsg.style.display = 'block';
                        if (portalClosingMsg) portalClosingMsg.style.display = 'none';

                        // Set up reload link to access device on new network
                        if (reloadLink) {
                            reloadLink.href = `http://${status.localIP}/`;
                            reloadLink.onclick = (e) => {
                                e.preventDefault();
                                // Open in new tab so portal remains accessible
                                window.open(`http://${status.localIP}/`, '_blank');
                            };
                        }
                    } else {
                        // Portal closing - user must manually switch networks
                        if (portalRetainedMsg) portalRetainedMsg.style.display = 'none';
                        if (portalClosingMsg) portalClosingMsg.style.display = 'block';
                    }
                }
            }
        }
    } catch (error) {
        console.error('Status update error:', error);
    }
}

window.refreshNetworks = refreshNetworks;
window.selectNetwork = selectNetwork;
  </script>
</body>
</html>
)=====";

#endif // AUTONETWORK_WIFI_PAGE
