// DOM elements
const connectionStatus = document.getElementById('connectionStatus');
const portSelect = document.getElementById('portSelect');
const connectBtn = document.getElementById('connectBtn');
const refreshBtn = document.getElementById('refreshBtn');
const connectionInfo = document.getElementById('connectionInfo');

const openConfigBtn = document.getElementById('openConfigBtn');
const configUrl = document.getElementById('configUrl');


// Stat display elements
const wpmValue = document.getElementById('wpmValue');
const cpuValue = document.getElementById('cpuValue');
const ramValue = document.getElementById('ramValue');
const timeValue = document.getElementById('timeValue');
const cpuTempValue = document.getElementById('cpuTempValue');
const gpuValue = document.getElementById('gpuValue');
const weatherValue = document.getElementById('weatherValue');
const linesValue = document.getElementById('linesValue');
const sensorSource = document.getElementById('sensorSource');


// Application state
let isConnected = false;
let currentPort = null;

// Initialize the application
async function initializeApp() {
    try {
        const platform = await window.electronAPI.getPlatform();
        const version = await window.electronAPI.getAppVersion();
        

        
        // Phone settings link
        refreshConfigUrl();
        setInterval(refreshConfigUrl, 5000);
        
        // Refresh serial ports
        await refreshSerialPorts();
        
        // Set up event listeners
        setupEventListeners();
        
        // Update time display
        updateTimeDisplay();
        setInterval(updateTimeDisplay, 1000);
        

        
    } catch (error) {
        console.error('Failed to initialize app:', error);

    }
}

// Set up event listeners
function setupEventListeners() {
    // Connection controls
    connectBtn.addEventListener('click', toggleConnection);
    refreshBtn.addEventListener('click', refreshSerialPorts);
    

    
    openConfigBtn.addEventListener('click', () => window.electronAPI.openConfigPage());
    
    // Electron API event listeners
    window.electronAPI.onConnectionChange((event, data) => {
        updateConnectionStatus(data.connected, data.port);
    });
    
    window.electronAPI.onSystemStats((event, stats) => {
        updateSystemStats(stats);
    });
    
    window.electronAPI.onTypingStats((event, stats) => {

        updateTypingStats(stats);
    });
    
    window.electronAPI.onKeyboardFallback((event, data) => {
        console.log('⚠️ Keyboard fallback mode activated');

    });
    
    window.electronAPI.onSerialData((event, data) => {

    });
    
    window.electronAPI.onShowSettings(() => {
        window.electronAPI.openConfigPage();
    });

    window.electronAPI.onExtrasStats((event, status) => {
        updateExtras(status);
    });
    window.electronAPI.getExtrasStatus().then(s => s && updateExtras(s));
}

// Refresh available serial ports
async function refreshSerialPorts() {
    try {

        const ports = await window.electronAPI.getSerialPorts();
        
        // Clear existing options
        portSelect.innerHTML = '<option value="">Select ESP32 Port...</option>';
        
        // Add available ports
        ports.forEach(port => {
            const option = document.createElement('option');
            option.value = port.path;
            option.textContent = `${port.path} - ${port.manufacturer || 'Unknown'}`;
            portSelect.appendChild(option);
        });
        

        
    } catch (error) {
        console.error('Failed to refresh ports:', error);

    }
}

// Toggle device connection
async function toggleConnection() {
    try {
        if (isConnected) {
            // Disconnect
            await window.electronAPI.disconnectDevice();

        } else {
            // Connect
            const selectedPort = portSelect.value;
            if (!selectedPort) {

                return;
            }
            

            const result = await window.electronAPI.connectToDevice(selectedPort);
            
            if (result.success) {
                currentPort = selectedPort;

            } else {

            }
        }
    } catch (error) {
        console.error('Connection error:', error);

    }
}

// Update connection status UI
function updateConnectionStatus(connected, port) {
    isConnected = connected;
    currentPort = port;
    
    const statusDot = connectionStatus.querySelector('.status-dot');
    const statusText = connectionStatus.querySelector('.status-text');
    
    if (connected) {
        statusDot.className = 'status-dot connected';
        statusText.textContent = `Connected to ${port}`;
        connectBtn.textContent = 'Disconnect';
        connectBtn.className = 'btn btn-danger';
        connectionInfo.innerHTML = `<p>Connected to ESP32 on port ${port}</p>`;
    } else {
        statusDot.className = 'status-dot disconnected';
        statusText.textContent = 'Disconnected';
        connectBtn.textContent = 'Connect';
        connectBtn.className = 'btn btn-primary';
        connectionInfo.innerHTML = '<p>Select a port and click Connect to start monitoring</p>';
    }
}



// Update system stats display
function updateSystemStats(stats) {
    if (stats.cpu !== undefined) {
        cpuValue.textContent = `${Math.round(stats.cpu)}%`;
    }
    if (stats.memory !== undefined) {
        ramValue.textContent = `${Math.round(stats.memory)}%`;
    }
}

// Update extras display (temps, GPU, weather, exact lines sent to the board)
function updateExtras(status) {
    const u = status.unit;
    const conv = c => (c === null || c === undefined) ? null : Math.round(u === 'F' ? c * 9 / 5 + 32 : c);
    const cpuT = conv(status.hw.cpuTempC);
    cpuTempValue.textContent = cpuT === null ? 'n/a' : `${cpuT}°${u}`;
    const gLoad = status.hw.gpuLoad, gT = conv(status.hw.gpuTempC);
    gpuValue.textContent = (gLoad === null && gT === null) ? 'n/a'
        : `${gLoad === null ? '--' : Math.round(gLoad) + '%'}${gT === null ? '' : ' ' + gT + '°' + u}`;
    if (status.weather) {
        const t = u === 'F' ? status.weather.tempF : (status.weather.tempF - 32) * 5 / 9;
        weatherValue.textContent = `${Math.round(t)}°${u}${status.place ? ' · ' + status.place : ''}`;
    } else {
        weatherValue.textContent = status.weatherError ? `Error: ${status.weatherError}` : 'n/a';
    }
    linesValue.textContent = status.lines.join('  ');
    sensorSource.textContent = `Sensors: ${status.hw.source}`;
}

// Update typing stats display
function updateTypingStats(stats) {

    if (stats.wpm !== undefined) {
        const wpmText = `${Math.round(stats.wpm)} WPM`;

        wpmValue.textContent = wpmText;
    } else {

    }
}

// Update time display
function updateTimeDisplay() {
    const now = new Date();
    const timeString = now.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
    timeValue.textContent = timeString;
}

// Phone settings link
async function refreshConfigUrl() {
    const url = await window.electronAPI.getConfigUrl();
    configUrl.textContent = url || 'Starting…';
}

// Initialize the app when DOM is loaded
document.addEventListener('DOMContentLoaded', initializeApp);