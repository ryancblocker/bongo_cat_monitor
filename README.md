# 🐱 Bongo Cat Monitor: Ross edition

Fork of [vostoklabs/bongo_cat_monitor](https://github.com/vostoklabs/bongo_cat_monitor) for a 2.8" **ESP32-32E (USB-C only) Cheap Yellow Display**, in a portrait stand. On top of the original typing cat, CPU, RAM and WPM, it adds:

- **CPU temperature** and **GPU usage + temperature**
- **Date** and **weather** (Open-Meteo, no API key; default ZIP 80130)
- 12-hour clock, °F everywhere (changeable in the app)
- A desktop app that **finds the board by itself**, reconnects after an unplug, lives in the tray and **starts with Windows**

```
 9:41PM        54F      <- clock / outside temp
 THU 12     CLOUDY      <- date / condition
 CPU: 23% 136F
 GPU: 63% 160F          <- hidden if the PC has no GPU reading
 RAM: 41%
 WPM: 82
      (cat)
```

## 1. Flash the board (from a Mac or PC)

Needs [PlatformIO](https://platformio.org/) (`brew install platformio`). From the repo root, with the board plugged in:

```bash
pio run -t upload && pio device monitor
```

`User_Setup.h` is set for this board: ILI9341 panel, not inverted, backlight on **GPIO 21** (the original repo's GPIO 27 leaves this board's screen dark).

Check the board without the desktop app (fills every row with sample data):

```bash
python3 tools/send_test.py /dev/cu.usbserial-10
```

Preview the layout without the board (uses the real sprites and LVGL font, after one `pio run`): `python3 tools/simulate_screen.py out.png`

## 2. Ross's PC (Windows)

1. Run `Bongo Cat Setup 1.1.0.exe` (build it with step 4 below). It's unsigned, so Windows SmartScreen will warn you: click **More info → Run anyway**. It installs for the current user and opens.
2. Plug in the display. The app connects on its own within a few seconds. If Windows doesn't recognise the board, install the **CH340** USB driver (WCH's CH341SER), then replug.
3. Close the window when you're done. The app keeps running in the tray (bottom-right) and starts with Windows. Right-click the tray icon → **Settings** to change the weather location, units or rows. **Quit** stops it.
4. **CPU temperature** needs [LibreHardwareMonitor](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/releases), because Windows doesn't give CPU temps to normal apps:
   - Unzip and run `LibreHardwareMonitor.exe` **as administrator**.
   - Options → **Run On Windows Startup**, Options → **Start Minimized**, Options → **Minimize To Tray**.
   - Options → **Remote Web Server → Run** (port 8085). Bongo Cat reads `http://localhost:8085/data.json`.
   - Without it, the CPU row shows just the percentage.
5. **GPU**: NVIDIA cards work with no setup (`nvidia-smi` ships with the driver). AMD/Intel GPUs come from LibreHardwareMonitor (step 4).

The app window shows exactly what it sends to the screen ("Sent to screen") and where the sensor readings come from, which helps when something is blank.

## 3. Test on a Mac

```bash
cd bongo-cat-electron && npm ci && node node_modules/electron/install.js
```

```bash
BONGO_FAKE_HW=1 npm start
```

`BONGO_FAKE_HW=1` makes up CPU/GPU temps (Macs don't expose them) so every row can be checked. macOS will ask for **Input Monitoring / Accessibility** permission so the cat can see typing. `npm test` runs the sensor-parsing tests (LibreHardwareMonitor and nvidia-smi formats).

If Electron says it "failed to install correctly" (npm 11 skips its download, and its unzip breaks on Node 26), unzip the cached download yourself:

```bash
cd node_modules/electron && rm -rf dist && mkdir dist && unzip -q ~/Library/Caches/electron/*/electron-v28.3.3-darwin-arm64.zip -d dist && printf 'Electron.app/Contents/MacOS/Electron' > path.txt
```

## 4. Build the Windows installer (on the Mac)

```bash
cd bongo-cat-electron && npx electron-builder --win
```

Output: `bongo-cat-electron/dist/Bongo Cat Setup 1.1.0.exe` (electron-builder downloads its own Wine/NSIS the first time).

## 5. Print the portrait stand

`case/RossBongoStand/RossBongoStand.py` is the sports-ticker stand rotated to portrait, with the USB-C port at the bottom. Run it in Fusion (Utilities → Add-Ins → Scripts and Add-Ins → **+**, add `case/RossBongoStand`, **Run**). It builds the design with a keep-out model of the board, checks for collisions (0), penny-bore clearance and tip-over (about 25° of lean before it tips), and writes STLs to `case/stl/`.

What's different from the ticker stand:
- Board pins, window, screw bosses and plug bay are the exact ticker measurements rotated 90°, so the board fits the same way.
- The cavity has an extra **8 mm below the board**, so the 90° USB-C cable can turn into the cable hole, which sits low on the back directly behind the plug.
- The rails sit higher (the bottom shell screws are where they used to be), so the base is **82 mm** tall instead of 62.

Print (PLA/PETG, 0.2 mm) and lay each STL flat like the ticker: **Pod Front** face down, **Pod Back** open side down (rails up), **Base** upright (tree supports from the build plate only if the penny-bore roofs sag), **Penny Lid** flat with the snap arm up.

Hardware is the same as the ticker: 6× M3 heat-set inserts (4.0 OD × 4 long), 6× M3×8, a low-profile 90° USB-C cable, 40 pennies.

Assembly: plug the 90° cable into the board and point the cable at the back → set the board on the pins in the pod front, USB-C at the bottom → feed the cable out through the hole at the bottom of the pod back and screw the back on → feed the cable through the base tunnel from the front → slide the rails down into the slots until the nub clicks, then bolt from the back → 10 pennies per bore, hook the lid's tabs in and swing it shut.

Outside size: **66 W × 66 D × 152 H mm**, screen tilted 12° back.

## Serial protocol additions

| Command | Example | Notes |
|---|---|---|
| `HW:` | `HW:CPUT:136,GPU:63,GPUT:160,U:F` | `-1` = unknown (GPU row hides, CPU temp is left off) |
| `DATE:` | `DATE:THU 12` | empty hides it |
| `WX:` | `WX:54F\|CLOUDY` | temperature, then condition; empty hides it |

Every row holds 15 characters (unscii 16 is 16 px wide); the app keeps each field within that.

---

*Original README below.*

# 🐱 Bongo Cat ESP32 - Your Cute Digital Typing Companion

![Bongo Cat](web%20resources/cat1.png)

Bongo Cat is a cute digital pet that lives on your desk and types along with you! This ESP32-based project displays a charming cat that responds to your typing speed, shows system statistics (CPU, RAM, WPM), and features adorable animations that bring joy to your workspace.

## ✨ Features

- **Real-time Typing Detection** - Cat types faster when you type faster
- **System Monitoring** - Displays CPU usage, RAM usage, and current time
- **WPM Tracking** - Shows your words per minute in real-time
- **Multiple Animations** - Various cute expressions and movements
- **Sleep Mode** - Cat goes to sleep when you're not active
- **Customizable Settings** - Adjust display preferences and behavior
- **Cross-Platform** - **🆕 Now available on Mac!** Windows and macOS support
- **Open Source** - Complete source code available
- **Easy Assembly** - No soldering required, around $10 to build
- **3D printed case** - files available on [MakerWorld](https://makerworld.com/en/models/1654522)

## 🎉 **NEW: Mac Support Available!** 

🍎 **Mac users can now enjoy Bongo Cat!** We've added full macOS support with:
- **Native Mac app** - Download and install like any other Mac application
- **Universal binary** - Works on both Intel and Apple Silicon Macs
- **Easy installation** - Simple drag-and-drop setup with step-by-step guide
- **Proper permissions** - Seamless integration with macOS security

📥 **[Download for Mac](https://github.com/vostoklabs/bongo_cat_monitor/releases)** | 📖 **[Mac Installation Guide](bongo-cat-electron/MAC_USER_GUIDE.md)**

## 🛒 Hardware Requirements

You'll need an **ESP32 board with 2.4 inch TFT display**. We used this board:

**[ESP32-2432S028R 2.4" TFT Display Board](https://www.aliexpress.com/item/1005008176009397.html?spm=a2g0o.order_list.order_list_main.4.5abc1802ZwAPQo)**

This board includes:
- ESP32-WROOM-32 module
- 2.4" ILI9341 TFT LCD (240x320 resolution)
- Touch screen capability
- USB-C connector
- All necessary components integrated

## 🚀 Quick Start

### Option 1: Web Flasher (Recommended)
1. **Flash the ESP32**: Visit our [Web Flasher](https://vostoklabs.github.io/Bongo_cat_webflasher/)
2. **Connect your ESP32** board with 2.4 inch TFT display to your computer's USB port
3. **Click "Connect"** and select your ESP32 device
4. **Click "Install"** and wait for the process to complete

### Option 2: Arduino IDE
1. Open `bongo_cat.ino` in Arduino IDE
2. Install required libraries (see Configuration section)
3. Select your ESP32 board and upload

### Desktop Application Setup

#### 🍎 For Mac Users
1. **Download** the Mac DMG file from [GitHub Releases](https://github.com/vostoklabs/bongo_cat_monitor/releases)
2. **Follow** our detailed [Mac Installation Guide](bongo-cat-electron/MAC_USER_GUIDE.md)
3. **Enjoy** your Bongo Cat with native Mac integration!

#### 🪟 For Windows Users  
1. **Download** the PC application archive: `BongoCat_v1.0.0_Windows.zip`
2. **Extract** the ZIP file to a temporary folder
3. **Run** `BongoCat_Setup.exe` from the extracted files
4. **Install** and run the application
5. The app will **automatically find and connect** to your ESP32 board
6. **Check system tray** - the app runs in the background
7. **Right-click tray icon** to access settings and configure display options

## 📁 Project Structure

```
├── 📁 animations/              # Animation sprite data
│   ├── 📁 body/               # Body movement animations
│   ├── 📁 faces/              # Facial expressions
│   ├── 📁 paws/               # Paw movement animations
│   ├── 📁 effects/            # Special effects (clicks, sleep)
│   ├── 📁 table/              # Table/background elements
│   └── 📄 Animation guidelines.md
├── 📁 bongo_cat_app/          # Desktop companion application (Windows)
│   ├── 📄 main.py             # Application entry point
│   ├── 📄 engine.py           # Core logic and serial communication
│   ├── 📄 gui.py              # Settings GUI
│   ├── 📄 tray.py             # System tray functionality
│   ├── 📄 config.py           # Configuration management
│   └── 📄 requirements_app.txt
├── 📁 bongo-cat-electron/     # 🆕 Cross-platform Electron app (Mac & Windows)
│   ├── 📄 main.js             # Main Electron process
│   ├── 📄 MAC_USER_GUIDE.md   # Step-by-step Mac installation guide
│   ├── 📁 renderer/           # UI components
│   ├── 📁 src/                # Core functionality modules
│   └── 📄 package.json        # Dependencies and build configuration
├── 📁 BongoCat_Release/       # Windows installer package
│   ├── 📄 BongoCat_Setup.exe  # Ready-to-install executable
│   ├── 📄 README.md           # Installation instructions
│   └── 📄 LICENSE.txt
├── 📁 web resources/          # Web flasher assets
│   ├── 🖼️ cat1.png, cat2.png   # High-quality cat images
│   ├── 🎞️ typing.gif          # Typing animation examples
│   ├── 🎞️ typing fast.gif     # Fast typing animation
│   ├── 🎞️ sleeping.gif        # Sleep mode animation
│   └── 📸 settings.png        # Screenshots for documentation
├── 📁 3d_printing/            # 3D printable case files (coming soon)
│   └── 📄 case.3mf            # 3MF format case file
├── 📄 bongo_cat.ino           # Main ESP32 firmware
├── 📄 animations_sprites.h    # Sprite data header
├── 📄 lv_conf.h              # LVGL configuration
├── 📄 User_Setup.h           # TFT_eSPI library configuration
├── 📄 Free_Fonts.h           # Font definitions
├── 📄 manifest.json          # Web flasher configuration
└── 📄 LICENSE.txt            # MIT License
```

## 🎨 Animation System

The Bongo Cat features a sophisticated animation system with multiple states:

### Animation Categories
- **Idle States**: Default cat expressions and subtle movements
- **Typing States**: Paw movements that match your typing speed
- **Sleep Mode**: Peaceful sleeping animations when inactive
- **Special Effects**: Click effects and transitions

### Animation Examples
| State | Speed | Description |
|-------|-------|-------------|
| ![Typing](web%20resources/typing.gif) | Normal | Regular typing animation |
| ![Fast Typing](web%20resources/typing%20fast.gif) | Fast | High-speed typing animation |
| ![Sleeping](web%20resources/sleeping.gif) | Idle | Sleep mode when inactive |

### Adding Custom Animations
1. Create your sprite data in the `animations/` folder
2. Update `animations_sprites.h` with new sprite definitions
3. Modify the animation logic in `bongo_cat.ino`

## ⚙️ Configuration

### TFT_eSPI Library Setup
The `User_Setup.h` file contains the display configuration for your specific ESP32 board:

```cpp
#define ESP32_2432S028R     // Enable this for the recommended board
#define TFT_MOSI 23
#define TFT_MISO 19
#define TFT_SCLK 18
#define TFT_CS   15
#define TFT_DC   2
#define TFT_RST  4
```

### LVGL Configuration
The `lv_conf.h` file configures the LVGL graphics library:
- Display resolution: 240x320 pixels
- Color depth: 16-bit
- Animation settings
- Memory allocation

### Required Arduino Libraries
- **TFT_eSPI** (configured with User_Setup.h)
- **LVGL** (version 8.x)
- **WiFi** (for time synchronization)

## 🔧 Development

### Building from Source
1. **Clone the repository**
   ```bash
   git clone https://github.com/your-username/bongo-cat-esp32.git
   ```

2. **ESP32 Firmware**
   - Open `bongo_cat.ino` in Arduino IDE
   - Ensure libraries are installed
   - Upload to your ESP32

3. **Desktop Application**
   ```bash
   cd bongo_cat_app
   pip install -r requirements_app.txt
   python main.py
   ```

### Creating Installers
- **Windows**: Use `installer.nsi` with NSIS
- **Executable**: Use `bongo_cat.spec` with PyInstaller

## 🎯 PC Application Features

### System Tray Integration
![Tray Screenshot](web%20resources/tray%20screenshot.png)

The application runs quietly in your system tray and provides:
- Automatic ESP32 detection and connection
- Real-time data transmission
- System resource monitoring

### Settings Configuration
![Settings](web%20resources/settings.png)

Customize your Bongo Cat experience:
- **Display Options**: Choose what information to show
- **Sleep Timer**: Set inactivity timeout
- **Animation Speed**: Adjust responsiveness
- **Connection Settings**: COM port configuration

## 🛠️ 3D Printing 

You can download and 3D print the standing case for this project. It contains .step, .stl and .3mf files available on [MakerWorld](https://makerworld.com/en/models/1654522)


## 🤝 Contributing

We welcome contributions! Here's how to help:

1. **Fork** the repository
2. **Create** a feature branch
3. **Make** your changes
4. **Test** thoroughly
5. **Submit** a pull request

### Areas for Contribution
- New animations and sprites
- Additional display layouts
- Case designs and 3D models
- Code optimizations
- Documentation improvements

## 📝 License

This project is licensed under the MIT License - see the [LICENSE.txt](LICENSE.txt) file for details.

## 🙏 Acknowledgments

- Original Bongo Cat meme creators

## 📞 Support


- **Web Flasher**: [https://vostoklabs.github.io/Bongo_cat_webflasher/](https://vostoklabs.github.io/Bongo_cat_webflasher/)

---


