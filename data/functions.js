// Shared functions between pages
const API_BASE = 'http://192.168.4.1/api';

async function apiRequest(cmd, params = {}) {
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

function formatBytes(bytes) {
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1048576) return (bytes / 1024).toFixed(2) + ' KB';
    return (bytes / 1048576).toFixed(2) + ' MB';
}

function formatUptime(seconds) {
    const h = Math.floor(seconds / 3600);
    const m = Math.floor((seconds % 3600) / 60);
    const s = seconds % 60;
    return `${h}h ${m}m ${s}s`;
}