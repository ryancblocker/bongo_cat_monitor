# 🐱 Bongo Cat Monitor: Ross edition

Fork of [vostoklabs/bongo_cat_monitor](https://github.com/vostoklabs/bongo_cat_monitor), rebuilt for a 2.8" **ESP32-32E (USB-C only) Cheap Yellow Display** standing in a portrait stand.

- **The original bongo cat in HD pixel art**: the classic sprites upscaled (Scale2x) and redrawn at twice the detail, with shading, eye shine, blush, inner ears and a wooden desk. **8 coats**: white, black, grey, orange tabby, calico, tuxedo, Siamese, cream. Or the **original 8-bit** cat.
- **Livelier**: it breathes, blinks, twitches an ear, glances around, bounces while you type, yawns before a nap, and shows hearts when you tap it.
- **Game Boy Color style UI**: pixel font (Press Start 2P), Pokémon-style text boxes with a blinking ▼, HP-bar PC stats, and a name plate with **Tamagotchi mood hearts** that fill while you type.
- **Pixel weather in the background**: dithered sky bands, sun, moon and stars, block clouds, rain, snow, fog and lightning, from the real weather (Open-Meteo, no API key).
- **Header** with temperature, time and a ⚙ gear. Below it a **card that cycles** between weather and PC stats (CPU, GPU, RAM with temps, typing speed). Tap the card to switch it.
- **Focus timer + levels**: start a 15/25/45/60-minute focus session from the phone page or the ⚙ OPTIONS screen. A timer card with an EXP bar counts down, and finishing it levels the cat up with a full-screen anime **LEVEL UP!** (speed lines, golden aura, shockwave, slamming title, screen shake), then a "MOCHI GREW TO LEVEL 4!" box.
- **Demo**: Phone page → SCREEN → **PLAY DEMO** runs a 1-minute tour of every screen and animation (all weather, typing, sleeping, focus, level up, every coat, OPTIONS). It doesn't change settings or the level; tap the cat's screen to stop it. The level lives on the board and shows on the name plate. Mood hearts fill while you type and drain slowly when you don't.
- **⚙ OPTIONS screen**: a QR code for the phone settings page, the focus timer (tap the length to change it), and brightness blocks (tap ◀ ▶ or drag).
- **Phone settings page** (same Game Boy style), served by the desktop app: name, coat, style, brightness, cards, clock, nap time, weather location and units. Changes show up instantly.

```
firmware/                  PlatformIO project (LovyanGFX), the screen
  src/cat_assets.cpp       generated cat art (tools/pixelcat.py --export)
bongo-cat-electron/        desktop app (Windows + Mac): stats, typing, weather, phone page
  renderer/config.html     the phone settings page
  renderer/cats/           cat previews for the page
case/RossBongoStand/       Fusion script for the portrait stand (STLs in case/stl/)
tools/
  pixelcat.py              HD pixel cat + coats from the original sprites (--export for firmware + app)
  bongo.py                 test the board without the app: sample data, settings, screenshots
```

## 1. Flash the board

Needs [PlatformIO](https://platformio.org/) (`brew install platformio`). With the board plugged in (and the desktop app closed):

```bash
cd firmware && pio run -t upload
```

On first start it asks you to **tap four arrows** to calibrate touch. Hold a finger on the screen while plugging it in to calibrate again.

### Testing without the app

Set up the Python tools once:

```bash
python3 -m venv .venv && .venv/bin/pip install pillow freetype-py pyserial
```

```bash
.venv/bin/python tools/bongo.py demo --code 61 --night --typing 3 --shot rain.png
```

That sends sample weather (WMO code 61 = rain, at night) and PC stats, makes the cat type for 3 seconds, and saves a screenshot of the real screen. `send "SET:coat=calico|name=Mochi"` changes settings, and `dbg` prints the cat's state and the frame time.

### Changing the cats

`tools/pixelcat.py` builds the HD cat from the original sprites in `animations/`: Scale2x, a 1 px outline, shading, then the coat colours and patterns (all in the `COAT` table at the top). Previews land in `tools/out/`. Run it with `--export` to regenerate `firmware/src/cat_assets.cpp` and the phone page's coat previews, then reflash.

## 2. Ross's PC (Windows)

1. Run `Bongo Cat Setup 2.0.0.exe` (see step 4). It's unsigned, so SmartScreen warns: **More info → Run anyway**.
2. **Windows Firewall** asks whether Bongo Cat may use the network: tick **Private networks** and **Allow**. That's what lets the phone reach the settings page.
3. Plug in the display. The app finds it within a few seconds. If Windows doesn't recognise the board, install the **CH340** USB driver (WCH's CH341SER) and replug.
4. Closing the window keeps the app running in the tray. It starts with Windows.
5. **Settings**: tap the **⚙** on the cat's screen and scan the QR code with a phone on the same Wi-Fi. Or click **Open settings on this PC** in the app.
6. **CPU temperature** needs [LibreHardwareMonitor](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/releases), because Windows doesn't give CPU temps to normal apps. Run it **as administrator**, then turn on Options → **Run On Windows Startup**, **Start Minimized**, **Minimize To Tray**, and **Remote Web Server → Run** (keep its default port, 8085). NVIDIA GPUs work without it.

## 3. Test on a Mac

```bash
cd bongo-cat-electron && npm ci && node node_modules/electron/install.js
```

```bash
BONGO_FAKE_HW=1 npm start
```

`BONGO_FAKE_HW=1` makes up CPU/GPU temps (Macs don't expose them). macOS asks for **Input Monitoring / Accessibility** permission so the cat can see typing. `npm test` runs the sensor tests.

If Electron says it "failed to install correctly" (npm 11 skips its download, and its unzip breaks on Node 26):

```bash
cd node_modules/electron && rm -rf dist && mkdir dist && unzip -q ~/Library/Caches/electron/*/electron-v28.3.3-darwin-arm64.zip -d dist && printf 'Electron.app/Contents/MacOS/Electron' > path.txt
```

## 4. Build the Windows installer (on the Mac)

```bash
cd bongo-cat-electron && npx electron-builder --win
```

Output: `bongo-cat-electron/dist/Bongo Cat Setup 2.0.0.exe`.

## 5. Print the portrait stand

`case/RossBongoStand/RossBongoStand.py` is the sports-ticker stand rotated to portrait, with the USB-C port at the bottom. Run it in Fusion (Utilities → Add-Ins → Scripts and Add-Ins → **+**, add `case/RossBongoStand`, **Run**). It checks for collisions (0), penny-bore clearance and tip-over (about 25° of lean before it tips), and writes STLs to `case/stl/`.

- Board pins, window, screw bosses and plug bay are the ticker's measurements rotated 90°.
- The cavity has an extra **8 mm below the board** for the 90° USB-C cable to turn into the cable hole, low on the back behind the plug.
- The rails sit higher (the bottom shell screws are where they used to be), so the base is **82 mm** tall.

Print (PLA/PETG, 0.2 mm) like the ticker: **Pod Front** face down, **Pod Back** open side down, **Base** upright, **Penny Lid** flat with the snap arm up. Hardware: 6× M3 heat-set inserts (4.0 OD × 4), 6× M3×8, a low-profile 90° USB-C cable, 40 pennies. Outside size **66 W × 66 D × 152 H mm**.

## Serial protocol (PC → board, 115200 baud, one line each)

| Line | Example |
|---|---|
| `TIME:` | `TIME:21:41:05` (the board keeps time from here) |
| `DATE:` | `DATE:Thu, Nov 12` |
| `WX2:` | `WX2:code=3\|day=1\|t=54\|hi=60\|lo=41\|wind=8\|wu=mph\|hum=40\|u=F\|place=Highlands Ranch` (`WX2:` alone = no weather) |
| `PC:` | `PC:cpu=23\|ram=41\|wpm=82\|cput=136\|gpu=63\|gput=160\|u=F` (`-1` = unknown) |
| `SET:` | `SET:style=hd\|coat=calico\|name=Mochi\|bright=200\|cycle=8\|cards=3\|h24=0\|sleep=5` (style `hd`/`8bit`; coats `white black grey orange calico tuxedo siamese cream`) |
| `URL:` | phone page address for the QR code |
| `FOCUS:` | `FOCUS:25` starts a 25-minute session, `FOCUS:0` stops it |
| `DEMO` | tour of every screen (`DEMO:0` stops it); `LEVELUP` previews the level-up animation |
| typing | `SPEED:<ms>`, `STOP`, `IDLE_START`, `STREAK_ON`, `STREAK_OFF` |
| testing | `PING`, `DBG`, `SHOT` (raw screenshot), `UI:settings` |

The board sends `HELLO:bongo2` at boot, `EVT:level=<n>` (at boot and after each `SET:`), `EVT:bright=<n>` when its brightness blocks change, and `EVT:focus=start|min=25|left=1500`, `EVT:focus=stop`, `EVT:focus=done|level=<n>` for focus sessions, and `EVT:demo=start` / `EVT:demo=end`.

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


