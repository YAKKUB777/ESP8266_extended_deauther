// Global state
let selectedAP = null;
let networks = [];
let statusInterval = null;
let uptimeInterval = null;

// API base URL
const API_BASE = 'http://192.168.4.1/api';

// DOM Elements
const elements = {
    connectionStatus: document.getElementById('connectionStatus'),
    clientCount: document.getElementById('clientCount'),
    networkList: document.getElementById('networkList'),
    scanProgress: document.getElementById('scanProgress'),
    selectedTarget: document.getElementById('selectedTarget'),
    
    // Buttons
    deauthStartBtn: document.getElementById('deauthStartBtn'),
    deauthStopBtn: document.getElementById('deauthStopBtn'),
    beaconStartBtn: document.getElementById('beaconStartBtn'),
    beaconStopBtn: document.getElementById('beaconStopBtn'),
    probeStartBtn: document.getElementById('probeStartBtn'),
    probeStopBtn: document.getElementById('probeStopBtn'),
    irStartBtn: document.getElementById('irStartBtn'),
    irStopBtn: document.getElementById('irStopBtn'),
    
    // Stats
    deauthStats: document.getElementById('deauthStats'),
    deauthPacketCount: document.getElementById('deauthPacketCount'),
    deauthChannel: document.getElementById('deauthChannel'),
    beaconStats: document.getElementById('beaconStats'),
    beaconPacketCount: document.getElementById('beaconPacketCount'),
    beaconCount: document.getElementById('beaconCount'),
    probeStats: document.getElementById('probeStats'),
    probePacketCount: document.getElementById('probePacketCount'),
    
    // System
    uptime: document.getElementById('uptime')
};

// API calls
async function api(cmd, params = {}) {
    let url = `${API_BASE}?cmd=${cmd}`;
    for (let key in params) {
        url += `&${key}=${encodeURIComponent(params[key])}`;
    }
    
    try {
        const res = await fetch(url);
        return await res.json();
    } catch (e) {
        console.error('API Error:', e);
        return { error: e.message };
    }
}

// WiFi Scanner
async function scanWiFi() {
    elements.networkList.innerHTML = '<div class="placeholder">Scanning...</div>';
    elements.scanProgress.style.display = 'block';
    
    await api('scan');
    
    setTimeout(async () => {
        const data = await api('results');
        elements.scanProgress.style.display = 'none';
        
        if (data.networks && data.networks.length > 0) {
            networks = data.networks.sort((a, b) => b.rssi - a.rssi);
            renderNetworkList();
        } else {
            elements.networkList.innerHTML = '<div class="placeholder">No networks found</div>';
        }
    }, 3000);
}

function renderNetworkList() {
    let html = '';
    networks.forEach((net, index) => {
        const signalStrength = Math.min(4, Math.floor((net.rssi + 100) / 10));
        const signalBars = [];
        for (let i = 0; i < 4; i++) {
            signalBars.push(`<div class="signal-bar ${i < signalStrength ? 'active' : ''}"></div>`);
        }
        
        html += `
            <div class="network-item ${selectedAP && selectedAP.index === index ? 'selected' : ''}" onclick="selectAP(${index})">
                <div class="network-info">
                    <div class="network-signal">${signalBars.join('')}</div>
                    <span class="network-ssid">${escapeHtml(net.ssid) || '[Hidden]'}</span>
                </div>
                <div class="network-details">
                    <span class="network-channel">CH ${net.ch}</span>
                    <span class="network-rssi">${net.rssi} dBm</span>
                </div>
            </div>
        `;
    });
    
    elements.networkList.innerHTML = html;
}

function selectAP(index) {
    selectedAP = { ...networks[index], index };
    renderNetworkList();
    
    elements.selectedTarget.innerHTML = `
        <div class="target-info">
            <span class="target-ssid">${escapeHtml(selectedAP.ssid) || '[Hidden]'}</span>
            <div class="target-details">
                <span>Channel: ${selectedAP.ch}</span>
                <span>${selectedAP.rssi} dBm</span>
                <span>${selectedAP.bssid}</span>
            </div>
        </div>
    `;
}

// Deauth
async function startDeauth() {
    if (!selectedAP) {
        alert('Please select a target network first!');
        return;
    }
    
    const data = await api('deauth', {
        start: true,
        ssid: selectedAP.ssid,
        ch: selectedAP.ch,
        bssid: selectedAP.bssid
    });
    
    if (!data.error) {
        elements.deauthStartBtn.disabled = true;
        elements.deauthStopBtn.disabled = false;
        elements.deauthStats.style.display = 'grid';
        elements.deauthChannel.textContent = selectedAP.ch;
    }
}

async function stopDeauth() {
    await api('deauth', { start: false });
    elements.deauthStartBtn.disabled = false;
    elements.deauthStopBtn.disabled = true;
}

// Beacon
async function startBeacon() {
    const list = document.getElementById('beaconList').value;
    const data = await api('beacon', { start: true, list });
    
    if (!data.error) {
        elements.beaconStartBtn.disabled = true;
        elements.beaconStopBtn.disabled = false;
        elements.beaconStats.style.display = 'grid';
        elements.beaconCount.textContent = list.split(',').length;
    }
}

async function stopBeacon() {
    await api('beacon', { start: false });
    elements.beaconStartBtn.disabled = false;
    elements.beaconStopBtn.disabled = true;
}

// Probe
async function startProbe() {
    const data = await api('probe', { start: true });
    
    if (!data.error) {
        elements.probeStartBtn.disabled = true;
        elements.probeStopBtn.disabled = false;
        elements.probeStats.style.display = 'grid';
    }
}

async function stopProbe() {
    await api('probe', { start: false });
    elements.probeStartBtn.disabled = false;
    elements.probeStopBtn.disabled = true;
}

// IR
async function startIR() {
    const data = await api('ir', { start: true });
    
    if (!data.error) {
        elements.irStartBtn.disabled = true;
        elements.irStopBtn.disabled = false;
    }
}

async function stopIR() {
    await api('ir', { start: false });
    elements.irStartBtn.disabled = false;
    elements.irStopBtn.disabled = true;
}

// Stop all
async function stopAll() {
    await api('stopall');
    
    // Reset UI
    elements.deauthStartBtn.disabled = false;
    elements.deauthStopBtn.disabled = true;
    elements.beaconStartBtn.disabled = false;
    elements.beaconStopBtn.disabled = true;
    elements.probeStartBtn.disabled = false;
    elements.probeStopBtn.disabled = true;
    elements.irStartBtn.disabled = false;
    elements.irStopBtn.disabled = true;
}

// Status update
async function updateStatus() {
    const data = await api('status');
    
    if (data.error) {
        elements.connectionStatus.className = 'status-dot danger';
        return;
    }
    
    // Connection status
    elements.connectionStatus.className = 'status-dot';
    elements.clientCount.textContent = data.clients || 0;
    
    // Attack states
    if (data.deauth) {
        elements.deauthStartBtn.disabled = true;
        elements.deauthStopBtn.disabled = false;
        elements.deauthStats.style.display = 'grid';
        elements.deauthPacketCount.textContent = data.packets || 0;
        if (data.target) {
            elements.selectedTarget.innerHTML = `
                <div class="target-info">
                    <span class="target-ssid">${escapeHtml(data.target)}</span>
                    <div class="target-details">
                        <span>Channel: ${data.channel || '-'}</span>
                    </div>
                </div>
            `;
        }
    } else {
        elements.deauthStartBtn.disabled = !selectedAP;
        elements.deauthStopBtn.disabled = true;
    }
    
    if (data.beacon) {
        elements.beaconStartBtn.disabled = true;
        elements.beaconStopBtn.disabled = false;
        elements.beaconStats.style.display = 'grid';
        elements.beaconPacketCount.textContent = data.packets || 0;
        elements.beaconCount.textContent = data.beaconCount || 0;
    }
    
    if (data.probe) {
        elements.probeStartBtn.disabled = true;
        elements.probeStopBtn.disabled = false;
        elements.probeStats.style.display = 'grid';
        elements.probePacketCount.textContent = data.packets || 0;
    }
    
    if (data.ir) {
        elements.irStartBtn.disabled = true;
        elements.irStopBtn.disabled = false;
    }
    
    // Uptime
    if (data.uptime) {
        const uptime = parseInt(data.uptime);
        const hours = Math.floor(uptime / 3600);
        const minutes = Math.floor((uptime % 3600) / 60);
        const seconds = uptime % 60;
        elements.uptime.textContent = `${hours}h ${minutes}m ${seconds}s`;
    }
}

// Utility
function escapeHtml(text) {
    if (!text) return '';
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

// Initialize
function init() {
    // Start status updates
    updateStatus();
    statusInterval = setInterval(updateStatus, 2000);
    
    // Enable deauth button only when target selected
    elements.deauthStartBtn.disabled = true;
}

// Cleanup on page unload
window.addEventListener('beforeunload', () => {
    if (statusInterval) clearInterval(statusInterval);
    if (uptimeInterval) clearInterval(uptimeInterval);
});

// Start
init();