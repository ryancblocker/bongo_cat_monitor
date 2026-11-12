// DOM elements
const connectionStatus = document.getElementById('connectionStatus');
const portSelect = document.getElementById('portSelect');
const connectBtn = document.getElementById('connectBtn');
const refreshBtn = document.getElementById('refreshBtn');
const connectionInfo = document.getElementById('connectionInfo');

const settingsBtn = document.getElementById('settingsBtn');
const settingsModal = document.getElementById('settingsModal');
const closeSettings = document.getElementById('closeSettings');
const applySettings = document.getElementById('applySettings');
const saveSettings = document.getElementById('saveSettings');
const resetSettings = document.getElementById('resetSettings');


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

// Setting elements
const showCpu = document.getElementById('showCpu');
const showRam = document.getElementById('showRam');
const showWpm = document.getElementById('showWpm');
const showTime = document.getElementById('showTime');
const timeFormat = document.getElementById('timeFormat');
const sleepTimeout = document.getElementById('sleepTimeout');
const showCpuTemp = document.getElementById('showCpuTemp');
const showGpu = document.getElementById('showGpu');
const showDate = document.getElementById('showDate');
const showWeather = document.getElementById('showWeather');
const weatherLocation = document.getElementById('weatherLocation');
const tempUnit = document.getElementById('tempUnit');
const startAtLogin = document.getElementById('startAtLogin');

// Application state
let isConnected = false;
let currentPort = null;

// Initialize the application
async function initializeApp() {
    try {
        const platform = await window.electronAPI.getPlatform();
        const version = await window.electronAPI.getAppVersion();
        

        
        // Load settings
        await loadSettings();
        
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
    

    
    // Settings modal
    settingsBtn.addEventListener('click', showSettings);
    closeSettings.addEventListener('click', hideSettings);
    applySettings.addEventListener('click', applyAppSettings);
    saveSettings.addEventListener('click', saveAppSettings);
    resetSettings.addEventListener('click', resetAppSettings);
    
    // Close modal when clicking outside
    settingsModal.addEventListener('click', (e) => {
        if (e.target === settingsModal) {
            hideSettings();
        }
    });
    
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
        showSettings();
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

// Settings management
function readSettingsForm() {
    const sleep = parseInt(sleepTimeout.value, 10);
    return {
        showCpu: showCpu.checked,
        showRam: showRam.checked,
        showWpm: showWpm.checked,
        showTime: showTime.checked,
        timeFormat: timeFormat.value,
        sleepTimeout: Number.isFinite(sleep) ? Math.min(60, Math.max(1, sleep)) : 5,
        showCpuTemp: showCpuTemp.checked,
        showGpu: showGpu.checked,
        showDate: showDate.checked,
        showWeather: showWeather.checked,
        weatherLocation: weatherLocation.value.trim(),
        tempUnit: tempUnit.value,
        startAtLogin: startAtLogin.checked
    };
}

function showSettings() {
    settingsModal.classList.add('show');
}

function hideSettings() {
    settingsModal.classList.remove('show');
}

async function loadSettings() {
    try {
        const settings = await window.electronAPI.getSettings();
        
        // Display settings
        showCpu.checked = settings.showCpu !== false; // default true
        showRam.checked = settings.showRam !== false; // default true
        showWpm.checked = settings.showWpm !== false; // default true
        showTime.checked = settings.showTime !== false; // default true
        timeFormat.value = settings.timeFormat || '12'; // default 12-hour
        sleepTimeout.value = settings.sleepTimeout || 5; // default 5 minutes
        showCpuTemp.checked = settings.showCpuTemp !== false;
        showGpu.checked = settings.showGpu !== false;
        showDate.checked = settings.showDate !== false;
        showWeather.checked = settings.showWeather !== false;
        weatherLocation.value = settings.weatherLocation || '';
        tempUnit.value = settings.tempUnit || 'F';
        startAtLogin.checked = settings.startAtLogin !== false;
        
    } catch (error) {
        console.error('Failed to load settings:', error);

    }
}

async function applyAppSettings() {
    try {
        // Validate electronAPI is available
        if (!window.electronAPI) {
            throw new Error('electronAPI not available. Check preload script.');
        }
        
        if (!window.electronAPI.applySettings) {
            throw new Error('applySettings method not available in electronAPI.');
        }
        
        // Show applying state
        const applyBtn = document.getElementById('applySettings');
        const originalText = applyBtn.textContent;
        applyBtn.textContent = 'Applying...';
        applyBtn.disabled = true;
        
        const settings = readSettingsForm();
        
        console.log('Attempting to apply settings:', settings);
        await window.electronAPI.applySettings(settings);
        
        // Show success state briefly
        applyBtn.textContent = 'Applied!';
        setTimeout(() => {
            applyBtn.textContent = originalText;
            applyBtn.disabled = false;
        }, 1000);
        
    } catch (error) {
        console.error('Failed to apply settings:', error);
        console.error('Error details:', {
            message: error.message,
            stack: error.stack,
            name: error.name
        });
        
        // Reset button state on error
        const applyBtn = document.getElementById('applySettings');
        applyBtn.textContent = 'Apply';
        applyBtn.disabled = false;
        alert(`Failed to apply settings. Error: ${error.message}\nCheck console for details.`);
    }
}

async function saveAppSettings() {
    try {
        // Show saving state
        const saveBtn = document.getElementById('saveSettings');
        const originalText = saveBtn.textContent;
        saveBtn.textContent = 'Saving...';
        saveBtn.disabled = true;
        
        const settings = readSettingsForm();
        
        await window.electronAPI.saveSettings(settings);
        
        // Show success state briefly
        saveBtn.textContent = 'Saved!';
        setTimeout(() => {
            saveBtn.textContent = originalText;
            saveBtn.disabled = false;
            hideSettings();
        }, 1000);
        
    } catch (error) {
        console.error('Failed to save settings:', error);
        
        // Reset button state on error
        const saveBtn = document.getElementById('saveSettings');
        saveBtn.textContent = 'Save Settings';
        saveBtn.disabled = false;
        alert('Failed to save settings. Please try again.');
    }
}

async function resetAppSettings() {
    try {
        await window.electronAPI.resetSettings();
        await loadSettings(); // Reload the reset settings

        
    } catch (error) {
        console.error('Failed to reset settings:', error);

    }
}





// Initialize the app when DOM is loaded
document.addEventListener('DOMContentLoaded', initializeApp);