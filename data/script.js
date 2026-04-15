let selectedAP = null;

async function api(cmd, params = {}) {
    let url = '/api?cmd=' + cmd;
    for (let key in params) {
        url += '&' + key + '=' + encodeURIComponent(params[key]);
    }
    const res = await fetch(url);
    return await res.json();
}

async function scanWiFi() {
    document.getElementById('networkList').innerHTML = 'Scanning...';
    await api('scan');
    setTimeout(async () => {
        const data = await api('results');
        updateNetworks(data.networks || []);
    }, 3000);
}

function updateNetworks(networks) {
    let html = '';
    networks.forEach((net, i) => {
        html += `<div class="network-item" onclick="selectAP(${i}, '${net.ssid}', ${net.ch}, '${net.bssid}')">
            ${net.ssid} [CH:${net.ch}] (${net.rssi}dBm)
        </div>`;
    });
    document.getElementById('networkList').innerHTML = html || 'No networks found';
}

function selectAP(index, ssid, ch, bssid) {
    selectedAP = { ssid, ch, bssid };
    document.querySelectorAll('.network-item').forEach((el, i) => {
        el.classList.toggle('selected', i === index);
    });
}

async function startDeauth() {
    if (!selectedAP) { alert('Select a network first!'); return; }
    await api('deauth', { 
        start: true, 
        ssid: selectedAP.ssid, 
        ch: selectedAP.ch,
        bssid: selectedAP.bssid
    });
    updateStatus();
}

async function stopDeauth() {
    await api('deauth', { start: false });
    updateStatus();
}

async function startBeacon() {
    const list = document.getElementById('beaconList').value;
    await api('beacon', { start: true, list: list });
    updateStatus();
}

async function stopBeacon() {
    await api('beacon', { start: false });
    updateStatus();
}

async function startProbe() {
    await api('probe', { start: true });
    updateStatus();
}

async function stopProbe() {
    await api('probe', { start: false });
    updateStatus();
}

async function startIR() {
    await api('ir', { start: true });
    updateStatus();
}

async function stopIR() {
    await api('ir', { start: false });
    updateStatus();
}

async function stopAll() {
    await api('stopall');
    updateStatus();
}

async function updateStatus() {
    const data = await api('status');
    let statusText = [];
    if (data.deauth) statusText.push('💀 DEAUTH');
    if (data.beacon) statusText.push('📡 BEACON');
    if (data.probe) statusText.push('📨 PROBE');
    if (data.ir) statusText.push('💡 IR');
    
    let status = statusText.length ? statusText.join(' + ') : 'Ready';
    let info = `Packets: ${data.packets} | Clients: ${data.clients}`;
    
    if (data.target) info += ` | Target: ${data.target}`;
    
    document.getElementById('status').innerHTML = `${status}<br>${info}`;
}

setInterval(updateStatus, 2000);
updateStatus();