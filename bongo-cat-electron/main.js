const { app, BrowserWindow, ipcMain, Menu, Tray, nativeImage, shell } = require('electron');
const path = require('path');
const { EventEmitter } = require('events');

// Import our custom modules
const SettingsManager = require('./src/settings');
const ESP32SerialManager = require('./src/serial');
const SystemMonitor = require('./src/system-monitor');
const KeyboardMonitor = require('./src/keyboard-monitor');
const ExtrasMonitor = require('./src/extras-monitor');
const ConfigServer = require('./src/config-server');
const boardSettings = require('./src/board-settings');

const isDev = process.argv.includes('--dev');
// Launched at Windows login: stay in the tray instead of opening the window
const startHidden = process.argv.includes('--hidden');

// Keep a global reference of the window object
let mainWindow;
let tray;
let isQuitting = false;

// Initialize global instances
let eventEmitter;
let settingsManager;
let esp32SerialManager;
let systemMonitor;
let keyboardMonitor;
let extrasMonitor;
let configServer;
let lastSystemStats = { cpu: 0, memory: 0 };
let lastTypingStats = { wpm: 0, isActive: false };
let focusState = { active: false, endsAt: 0, min: 0 };  // mirrored from the board's EVT:focus lines
let demoRunning = false;  // board's EVT:demo=start / end
let onboardState = null;  // first boot on the board: 'waiting' (egg + QR), 'hatching', 'tour', 'done'

// Auto-connect state
let connecting = false;
let userDisconnected = false; // pressing Disconnect pauses auto-connect until Connect is pressed

// Monitoring state
let monitoringActive = false;

// Development mode indicator
if (isDev) {
  console.log('Running in development mode');
}

function createWindow() {
  // Create the browser window
  mainWindow = new BrowserWindow({
    width: 400,
    height: 600,
    minWidth: 350,
    minHeight: 500,
    webPreferences: {
      nodeIntegration: false, // Security: disable node integration
      contextIsolation: true, // Security: enable context isolation
      preload: path.join(__dirname, 'preload.js'), // Use preload script
      enableRemoteModule: false // Security: disable remote module
    },
    icon: path.join(__dirname, 'assets', 'icons', 'icon.png'),
    show: false, // Don't show until ready
    titleBarStyle: process.platform === 'darwin' ? 'hiddenInset' : 'default'
  });

  // Load the app
  mainWindow.loadFile('renderer/index.html');

  // Show window when ready to prevent visual flash
  mainWindow.once('ready-to-show', () => {
    if (!startHidden) mainWindow.show();
    
    // Open DevTools in development
    if (isDev) {
      mainWindow.webContents.openDevTools();
    }
  });

  // Handle window closed
  mainWindow.on('closed', () => {
    mainWindow = null;
  });

  // Handle window close (keep running in the tray instead of quitting, on every OS)
  mainWindow.on('close', (event) => {
    if (!isQuitting) {
      event.preventDefault();
      mainWindow.hide();
    }
  });

  // Handle window minimize (hide on Mac)
  mainWindow.on('minimize', (event) => {
    if (process.platform === 'darwin') {
      event.preventDefault();
      mainWindow.hide();
    }
  });
}

function createTray() {
  // Create tray icon
  const iconPath = path.join(__dirname, 'assets', 'icons', 'tray.png');
  const trayIcon = nativeImage.createFromPath(iconPath);
  tray = new Tray(trayIcon.resize({ width: 16, height: 16 }));

  // Create context menu
  const contextMenu = Menu.buildFromTemplate([
    {
      label: 'Show Bongo Cat',
      click: () => {
        if (mainWindow) {
          mainWindow.show();
          mainWindow.focus();
        }
      }
    },
    {
      label: 'Settings',
      click: () => {
        if (mainWindow) {
          mainWindow.show();
          mainWindow.focus();
          mainWindow.webContents.send('show-settings');
        }
      }
    },
    { type: 'separator' },
    {
      label: 'Quit Bongo Cat',
      click: () => {
        isQuitting = true;
        app.quit();
      }
    }
  ]);

  tray.setContextMenu(contextMenu);
  tray.setToolTip('Bongo Cat - ESP32 Desktop Companion');

  // Handle tray click
  tray.on('click', () => {
    if (mainWindow) {
      mainWindow.isVisible() ? mainWindow.hide() : mainWindow.show();
    }
  });
}

// Initialize application modules
function initializeModules() {
  try {
    // Create event emitter for inter-module communication
    eventEmitter = new EventEmitter();
    
    // Initialize managers
    settingsManager = new SettingsManager();
    esp32SerialManager = new ESP32SerialManager(eventEmitter);
    systemMonitor = new SystemMonitor(eventEmitter);
    keyboardMonitor = new KeyboardMonitor(eventEmitter);
    extrasMonitor = new ExtrasMonitor(eventEmitter);
    
    // Set up event forwarding to renderer
    setupEventForwarding();
    
    console.log('All modules initialized successfully');
  } catch (error) {
    console.error('Failed to initialize modules:', error);
  }
}

// Setup event forwarding from modules to renderer
function setupEventForwarding() {
  eventEmitter.on('connection-change', (data) => {
    if (mainWindow && !mainWindow.isDestroyed()) {
      mainWindow.webContents.send('connection-change', data);
    }
    if (data.connected) {
      syncBoard();
    }
  });

  eventEmitter.on('extras-stats', (status) => {
    if (mainWindow && !mainWindow.isDestroyed()) {
      mainWindow.webContents.send('extras-stats', status);
    }
  });

  // clock, date and weather every 15 s (PC stats go every second, below)
  setInterval(() => sendExtras(), 15000);
  
  eventEmitter.on('system-stats', (stats) => {
    if (mainWindow && !mainWindow.isDestroyed()) {
      mainWindow.webContents.send('system-stats', stats);
    }
    
    // Cache system stats for combined sending
    lastSystemStats = stats;
  });
  
  eventEmitter.on('typing-stats', (stats) => {
    // Only log when there's actual WPM activity to reduce spam

    
    if (mainWindow && !mainWindow.isDestroyed()) {
      mainWindow.webContents.send('typing-stats', stats);
    }
    
    // Cache typing stats for combined sending
    lastTypingStats = stats;
  });
  
  // Handle keyboard fallback notifications
  eventEmitter.on('keyboard-fallback', (data) => {
    console.log('⚠️ Keyboard monitoring fallback mode enabled');
    if (mainWindow && !mainWindow.isDestroyed()) {
      mainWindow.webContents.send('keyboard-fallback', data);
    }
  });
  
  // Every second: PC stats card + typing animation
  setInterval(async () => {
    if (!esp32SerialManager || !esp32SerialManager.getConnectionStatus().isConnected) return;
    try {
      await esp32SerialManager.sendCommand(extrasMonitor.pcCommand(lastSystemStats, lastTypingStats));
      await esp32SerialManager.sendAnimationCommands(Math.round(lastTypingStats.wpm || 0), lastTypingStats.isActive || false);
    } catch (error) {
      console.error('Failed to send stats:', error.message);
    }
  }, 1000);
  
  // Auto-start monitoring when app is ready
  setTimeout(async () => {
    try {
      console.log('🚀 Auto-starting system monitoring...');
      if (systemMonitor) {
        await systemMonitor.startMonitoring(2000); // 2-second intervals
        console.log('✅ System monitoring started automatically');
      }
      
      console.log('🚀 Auto-starting keyboard monitoring...');
      if (keyboardMonitor) {
        await keyboardMonitor.startMonitoring();
        console.log('✅ Keyboard monitoring started automatically');
        console.log('🧪 Type something now to test if keyboard monitoring works...');
        console.log('💡 If no keys are detected, check permissions or run as administrator');
      }
      
      // Auto-start as monitoring state
      monitoringActive = true;

      console.log('🚀 Starting extras (temps, GPU, date, weather) and auto-connect...');
      extrasMonitor.start(settingsManager.getAllSettings());
      await startConfigServer();
      autoConnectTick();
      setInterval(autoConnectTick, 3000);
      
    } catch (error) {
      console.error('Failed to auto-start monitoring:', error);
    }
  }, 1000); // Start after 1 second delay
  
  eventEmitter.on('serial-data', (data) => {
    if (mainWindow && !mainWindow.isDestroyed()) {
      mainWindow.webContents.send('serial-data', data);
    }
    // brightness slider on the board's settings screen
    const m = /^EVT:bright=(\d+)/.exec(data);
    if (m) settingsManager.set('brightness', Math.min(255, Math.max(10, parseInt(m[1], 10))));
    // level + focus sessions (the board keeps the timer and the level)
    const lv = /^EVT:(?:level=|focus=done\|level=)(\d+)/.exec(data);
    if (lv) settingsManager.set('catLevel', Math.max(1, parseInt(lv[1], 10)));
    const fs = /^EVT:focus=start\|min=(\d+)\|left=(\d+)/.exec(data);
    if (fs) focusState = { active: true, endsAt: Date.now() + parseInt(fs[2], 10) * 1000, min: parseInt(fs[1], 10) };
    if (/^EVT:focus=(stop|done)/.test(data)) focusState = { active: false, endsAt: 0, min: 0 };
    if (/^EVT:demo=start/.test(data)) demoRunning = true;
    if (/^EVT:demo=end/.test(data) || /^HELLO:/.test(data)) demoRunning = false;
    const ob = /^EVT:onboard=(\w+)/.exec(data);
    if (ob) onboardState = ob[1];
    if (/^HELLO:/.test(data)) onboardState = null;  // rebooted: it says again if it's still new
  });
}

// Send the board everything it needs after (re)connecting
async function syncBoard() {
  try {
    await esp32SerialManager.sendCommand(boardSettings.setLine(settingsManager.getAllSettings()));
    if (configServer && configServer.port) await esp32SerialManager.sendCommand('URL:' + configServer.url());
    await sendExtras();
    await esp32SerialManager.sendCommand(extrasMonitor.pcCommand(lastSystemStats, lastTypingStats));
  } catch (error) {
    console.error('Board sync failed:', error);
  }
}

// clock, date, weather
async function sendExtras() {
  if (!extrasMonitor || !esp32SerialManager || !esp32SerialManager.getConnectionStatus().isConnected) return;
  try {
    await esp32SerialManager.sendTimeUpdate();
    await esp32SerialManager.sendCommand(extrasMonitor.dateLine());
    await esp32SerialManager.sendCommand(extrasMonitor.wx2Command());
  } catch (error) {
    console.error('Failed to send extras:', error.message);
  }
}

// ----------------------------------------------------------------- phone config page
function configState() {
  const s = settingsManager.getAllSettings();
  const pick = {};
  for (const k of ['style', 'coat', 'catName', 'focusMin', 'wear', 'brightness', 'cycleSec', 'showWeatherCard', 'showPcCard', 'showFocusCard',
    'timeFormat', 'sleepTimeout', 'weatherLocation', 'placeName', 'tempUnit', 'showCpuTemp', 'showGpu', 'startAtLogin']) pick[k] = s[k];
  const ex = extrasMonitor ? extrasMonitor.getStatus() : {};
  return {
    settings: pick,
    status: {
      connected: !!(esp32SerialManager && esp32SerialManager.getConnectionStatus().isConnected),
      place: ex.place || '',
      weatherError: ex.weatherError || '',
      sensors: ex.hw ? ex.hw.source : 'none',
      platform: process.platform,
      level: settingsManager.get('catLevel', 1),
      demo: demoRunning,
      onboard: onboardState,
      focus: { active: focusState.active, leftSec: focusState.active ? Math.max(0, Math.round((focusState.endsAt - Date.now()) / 1000)) : 0, min: focusState.min },
    },
  };
}

async function applyChanges(changes) {
  // demo: {demo: true} plays the tour of every screen, {demo: false} stops it
  if (changes && 'demo' in changes && esp32SerialManager && esp32SerialManager.getConnectionStatus().isConnected) {
    await esp32SerialManager.sendCommand(changes.demo ? 'DEMO' : 'DEMO:0');
    await new Promise(r => setTimeout(r, 150));
  }
  // hatch: {hatch: true} the welcome page picked the cat: the egg on the board hatches
  if (changes && changes.hatch === true && esp32SerialManager && esp32SerialManager.getConnectionStatus().isConnected) {
    await esp32SerialManager.sendCommand('HATCH');
    await new Promise(r => setTimeout(r, 200));  // the board answers with EVT:onboard=hatching
  }
  // onboard: {onboard: true} plays the first-boot welcome again (egg, QR, hatch, tour)
  if (changes && changes.onboard === true && esp32SerialManager && esp32SerialManager.getConnectionStatus().isConnected) {
    await esp32SerialManager.sendCommand('ONBOARD');
    await new Promise(r => setTimeout(r, 200));
  }
  // resetProgress: {resetProgress: true} puts the cat back to Lv 1 (the page asks first)
  if (changes && changes.resetProgress === true && esp32SerialManager && esp32SerialManager.getConnectionStatus().isConnected) {
    await esp32SerialManager.sendCommand('RESET_PROGRESS');
    await new Promise(r => setTimeout(r, 200));  // the board answers with EVT:level=1
  }
  // focus: {focus: minutes} starts a session on the board, {focus: 0} stops it
  if (changes && 'focus' in changes && esp32SerialManager && esp32SerialManager.getConnectionStatus().isConnected) {
    const min = Math.max(0, Math.min(180, Math.round(Number(changes.focus) || 0)));
    await esp32SerialManager.sendCommand(`FOCUS:${min}`);
    await new Promise(r => setTimeout(r, 150));  // let the board answer with EVT:focus=...
  }
  const clean = boardSettings.validate(changes);
  if (!Object.keys(clean).length) return configState();
  settingsManager.updateSettings(clean);
  const all = settingsManager.getAllSettings();
  extrasMonitor.updateSettings(all);
  if ('startAtLogin' in clean) applyLoginItem();
  if (esp32SerialManager && esp32SerialManager.getConnectionStatus().isConnected) {
    await esp32SerialManager.sendCommand(boardSettings.setLine(all));
    await esp32SerialManager.sendCommand(extrasMonitor.wx2Command());
    await esp32SerialManager.sendCommand(extrasMonitor.pcCommand(lastSystemStats, lastTypingStats));
  }
  if (mainWindow && !mainWindow.isDestroyed()) mainWindow.webContents.send('settings-changed', configState());
  return configState();
}

async function startConfigServer() {
  if (!settingsManager.get('configKey', '')) settingsManager.set('configKey', ConfigServer.newKey());
  configServer = new ConfigServer({
    getKey: () => settingsManager.get('configKey', ''),
    getState: configState,
    onChange: applyChanges,
  });
  const ok = await configServer.start();
  if (ok && esp32SerialManager.getConnectionStatus().isConnected) {
    await esp32SerialManager.sendCommand('URL:' + configServer.url());
  }
}

// Find the board and connect without anyone touching the app: tries the last
// port first, then any USB-serial chip an ESP32 board uses (CH340, CP210x...).
async function autoConnectTick() {
  if (connecting || userDisconnected || !esp32SerialManager) return;
  if (esp32SerialManager.getConnectionStatus().isConnected) return;
  if (!settingsManager.get('autoConnect', true)) return;
  connecting = true;
  try {
    const ports = await esp32SerialManager.getAvailablePorts();
    const last = settingsManager.getLastUsedPort();
    const candidates = ports.filter(p => p.isESP32Likely).map(p => p.path);
    candidates.sort((a, b) => (b === last) - (a === last));
    for (const port of candidates) {
      try {
        console.log(`🔌 Auto-connect: trying ${port}`);
        await esp32SerialManager.connectToDevice(port, settingsManager.getESP32Settings());
        settingsManager.saveLastUsedPort(port);
        break;
      } catch (error) {
        console.log(`Auto-connect: ${port} failed (${error.message})`);
      }
    }
  } finally {
    connecting = false;
  }
}

// Start with Windows (installed app only), hidden in the tray
function applyLoginItem() {
  if (!app.isPackaged || process.platform === 'linux') return;
  const enabled = settingsManager.get('startAtLogin', true);
  app.setLoginItemSettings({ openAtLogin: enabled, args: ['--hidden'] });
}

// App event handlers
app.whenReady().then(() => {
  // Initialize modules first
  initializeModules();
  
  // Create UI
  createWindow();
  createTray();
  applyLoginItem();

  // Handle app activation (macOS)
  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow();
    } else if (mainWindow) {
      mainWindow.show();
    }
  });
});

// Quit when all windows are closed
app.on('window-all-closed', () => {
  // On macOS, keep app running even when all windows are closed
  if (process.platform !== 'darwin') {
    app.quit();
  }
});

// Handle app quit
app.on('before-quit', () => {
  isQuitting = true;
});

// Security: Prevent new window creation
app.on('web-contents-created', (event, contents) => {
  contents.on('new-window', (event, navigationUrl) => {
    event.preventDefault();
  });
});

// =============================================================================
// IPC HANDLERS - Bridge between renderer and backend modules
// =============================================================================

// App Info Handlers
ipcMain.handle('get-app-version', () => {
  return app.getVersion();
});

ipcMain.handle('get-platform', () => {
  return process.platform;
});

// Serial Communication Handlers
ipcMain.handle('get-serial-ports', async () => {
  try {
    if (!esp32SerialManager) {
      throw new Error('Serial manager not initialized');
    }
    return await esp32SerialManager.getAvailablePorts();
  } catch (error) {
    console.error('Get serial ports error:', error);
    throw error;
  }
});

ipcMain.handle('connect-to-device', async (event, port) => {
  try {
    if (!esp32SerialManager) {
      throw new Error('Serial manager not initialized');
    }
    
    userDisconnected = false;
    const esp32Settings = settingsManager.getESP32Settings();
    const result = await esp32SerialManager.connectToDevice(port, esp32Settings);
    
    // Save successful port for auto-connect
    if (result.success) {
      settingsManager.saveLastUsedPort(port);
    }
    
    return result;
  } catch (error) {
    console.error('Connect to device error:', error);
    throw error;
  }
});

ipcMain.handle('disconnect-device', async () => {
  try {
    if (!esp32SerialManager) {
      throw new Error('Serial manager not initialized');
    }
    userDisconnected = true;
    return await esp32SerialManager.disconnect();
  } catch (error) {
    console.error('Disconnect device error:', error);
    throw error;
  }
});

ipcMain.handle('send-serial-data', async (event, data) => {
  try {
    if (!esp32SerialManager) {
      throw new Error('Serial manager not initialized');
    }
    await esp32SerialManager.sendCommand(data);
    return { success: true };
  } catch (error) {
    console.error('Send serial data error:', error);
    throw error;
  }
});

// System Monitoring Handlers
ipcMain.handle('get-system-stats', async () => {
  try {
    if (!systemMonitor) {
      throw new Error('System monitor not initialized');
    }
    return await systemMonitor.getCurrentStats();
  } catch (error) {
    console.error('Get system stats error:', error);
    throw error;
  }
});

ipcMain.handle('start-monitoring', async () => {
  try {
    if (!systemMonitor) {
      throw new Error('System monitor not initialized');
    }
    
    const updateInterval = settingsManager.getUpdateInterval();
    const result = await systemMonitor.startMonitoring(updateInterval);
    
    if (result.success) {
      monitoringActive = true;
      
      // Send time updates every minute
      setInterval(async () => {
        if (esp32SerialManager && esp32SerialManager.getConnectionStatus().isConnected) {
          await esp32SerialManager.sendTimeUpdate();
        }
      }, 60000);
    }
    
    return result;
  } catch (error) {
    console.error('Start monitoring error:', error);
    throw error;
  }
});

ipcMain.handle('stop-monitoring', async () => {
  try {
    if (!systemMonitor) {
      throw new Error('System monitor not initialized');
    }
    
    const result = systemMonitor.stopMonitoring();
    monitoringActive = false;
    
    return result;
  } catch (error) {
    console.error('Stop monitoring error:', error);
    throw error;
  }
});

// Keyboard Monitoring Handlers
ipcMain.handle('start-keyboard-monitoring', async () => {
  try {
    if (!keyboardMonitor) {
      throw new Error('Keyboard monitor not initialized');
    }
    return await keyboardMonitor.startMonitoring();
  } catch (error) {
    console.error('Start keyboard monitoring error:', error);
    throw error;
  }
});

ipcMain.handle('stop-keyboard-monitoring', async () => {
  try {
    if (!keyboardMonitor) {
      throw new Error('Keyboard monitor not initialized');
    }
    return await keyboardMonitor.stopMonitoring();
  } catch (error) {
    console.error('Stop keyboard monitoring error:', error);
    throw error;
  }
});

ipcMain.handle('get-typing-stats', async () => {
  try {
    if (!keyboardMonitor) {
      throw new Error('Keyboard monitor not initialized');
    }
    return keyboardMonitor.getCurrentStats();
  } catch (error) {
    console.error('Get typing stats error:', error);
    throw error;
  }
});

// Settings Management Handlers
ipcMain.handle('get-settings', async () => {
  try {
    if (!settingsManager) {
      throw new Error('Settings manager not initialized');
    }
    return settingsManager.getAllSettings();
  } catch (error) {
    console.error('Get settings error:', error);
    throw error;
  }
});

ipcMain.handle('apply-settings', async (event, settings) => {
  try {
    console.log('Applying settings temporarily (not saving to disk):', settings);
    
    // Validate settings object
    if (!settings || typeof settings !== 'object') {
      throw new Error('Invalid settings object provided');
    }
    
    // Check if core modules are initialized
    console.log('Module status:', {
      systemMonitor: !!systemMonitor,
      esp32SerialManager: !!esp32SerialManager,
      settingsManager: !!settingsManager
    });
    
    // Apply settings that affect monitoring without saving to disk
    if (settings.updateInterval && systemMonitor && systemMonitor.isActive()) {
      try {
        systemMonitor.updateInterval(settings.updateInterval);
      } catch (error) {
        console.error('Failed to update monitoring interval:', error);
        // Don't throw - this shouldn't prevent other settings from applying
      }
    }
    
    if (extrasMonitor) extrasMonitor.updateSettings(settings);

    // Send settings to ESP32 device if connected
    if (esp32SerialManager && esp32SerialManager.isConnected) {
      try {
        await esp32SerialManager.sendCommand(boardSettings.setLine({ ...settingsManager.getAllSettings(), ...boardSettings.validate(settings) }));
        await sendExtras();
        console.log('Settings sent to ESP32 device successfully');
      } catch (error) {
        console.error('Failed to send settings to ESP32:', error);
        // Don't throw here, as the settings were applied locally successfully
      }
    }
    
    console.log('Settings applied successfully');
    return { success: true };
  } catch (error) {
    console.error('Apply settings error:', error);
    console.error('Error stack:', error.stack);
    throw new Error(`Failed to apply settings: ${error.message}`);
  }
});

ipcMain.handle('save-settings', async (event, settings) => {
  try {
    if (!settingsManager) {
      throw new Error('Settings manager not initialized');
    }
    
    const success = settingsManager.updateSettings(settings);
    if (extrasMonitor) extrasMonitor.updateSettings(settingsManager.getAllSettings());
    applyLoginItem();
    
    // Apply settings that affect monitoring
    if (settings.updateInterval && systemMonitor && systemMonitor.isActive()) {
      try {
        systemMonitor.updateInterval(settings.updateInterval);
      } catch (error) {
        console.error('Failed to update monitoring interval:', error);
        // Don't throw - this shouldn't prevent other settings from applying
      }
    }
    
    // Send settings to ESP32 device if connected
    if (esp32SerialManager && esp32SerialManager.isConnected) {
      try {
        await esp32SerialManager.sendCommand(boardSettings.setLine(settingsManager.getAllSettings()));
        await sendExtras();
        console.log('Settings saved and sent to ESP32 device successfully');
      } catch (error) {
        console.error('Failed to send settings to ESP32:', error);
        // Don't throw here, as the settings were saved locally successfully
      }
    }
    
    return { success };
  } catch (error) {
    console.error('Save settings error:', error);
    throw error;
  }
});

ipcMain.handle('reset-settings', async () => {
  try {
    if (!settingsManager) {
      throw new Error('Settings manager not initialized');
    }
    
    const success = settingsManager.resetToDefaults();
    if (extrasMonitor) extrasMonitor.updateSettings(settingsManager.getAllSettings());
    applyLoginItem();
    if (esp32SerialManager && esp32SerialManager.isConnected) await syncBoard();
    return { success };
  } catch (error) {
    console.error('Reset settings error:', error);
    throw error;
  }
});



ipcMain.handle('get-config-url', () => {
  return configServer && configServer.port ? configServer.url() : '';
});

ipcMain.handle('open-config-page', () => {
  if (configServer && configServer.port) shell.openExternal(configServer.url('localhost'));
});

ipcMain.handle('get-extras-status', () => {
  return extrasMonitor ? extrasMonitor.getStatus() : null;
});

// Cleanup on app quit
app.on('before-quit', async () => {
  isQuitting = true;
  
  try {
    // Stop all monitoring
    if (systemMonitor && systemMonitor.isActive()) {
      systemMonitor.stopMonitoring();
    }
    
    if (extrasMonitor) extrasMonitor.stop();
    if (configServer) configServer.stop();

    if (keyboardMonitor && keyboardMonitor.isActive()) {
      await keyboardMonitor.stopMonitoring();
    }
    
    // Disconnect ESP32
    if (esp32SerialManager && esp32SerialManager.getConnectionStatus().isConnected) {
      await esp32SerialManager.disconnect();
    }
    
    console.log('App cleanup completed');
  } catch (error) {
    console.error('Error during app cleanup:', error);
  }
});