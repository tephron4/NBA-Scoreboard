# ESP32-S3 Dual-Panel NBA Scoreboard & Boxscore Display

An IoT-driven, real-time NBA scoreboard and interactive boxscore display built for dual chained 64x64 HUB75 LED matrices powered by an ESP32-S3. The system features dynamic team logos, color-matched themes, NTP time synchronization, and a split-screen active player boxscore viewer.

---

## 🚀 Features

* **Dual-Panel Rendering:** Drives dual 64x64 HUB75 LED matrices (128x64 total canvas) for high-density sports telemetry.
* **Dynamic Game States:** Automatically switches between pregame start times, live quarter clocks/scores, and final summaries.
* **Team Branding:** Parses team colors, alternate colors, and renders pre-compiled 16x16 transparent RGB565 team logos.
* **Split-Screen Boxscore View:** Displays active player rosters side-by-side with column headers (`#`, `PLAYER`, `PTS`, `REB`, `AST`), showing jersey numbers, last name snippets, and live stat lines.
* **Flask Backend Integration:** A lightweight Python server that fetches, processes, and structures data cleanly for low-memory microcontrollers.

---

## 🛠️ Hardware Components

* **Microcontroller:** ESP32-S3 development board.
* **Display:** Dual WatangTech 64x64 RGB LED Matrix Panels (HUB75 interface).
* **Power Supply & Cabling:** Powered independently via a Nekmit 47W Dual-Port USB-C wall block paired with 10ft Cable Matters 100W/5A cables.
* **Server Host:** Raspberry Pi running the Python Flask backend.

---

## 📁 Project Structure

```text
nba-scoreboard/
├── backend/
│   ├── app.py              # Flask server and ESPN data parser
│   ├── utils.py            # Pillow-based logo converter (PNG to RGB565)
│   └── requirements.txt    # Python dependencies
└── controller/
    ├── scoreboard.ino      # Main Arduino/ESP32-S3 firmware
    ├── config.h            # Wi-Fi and server endpoint configuration
    └── logos.h             # Pre-compiled 16x16 RGB565 team logo arrays

```

---

## 📦 Backend Setup & Maintenance (Raspberry Pi)

### 1. Installation

Navigate to your backend directory and install the required Python dependencies (including Pillow for transparent logo conversion):

```bash
cd backend
pip install -r requirements.txt

```

### 2. Running the Server Manually

To test or run the Flask application interactively (e.g., on an alternative port like `8080`):

```bash
python3 app.py

```

### 3. Managing the Systemd Background Service

If configured as a systemd service (`nba-server.service`) on your Raspberry Pi, use standard `systemctl` commands to control it:

* **Check Service Status:**
```bash
sudo systemctl status nba-server.service

```


* **Start the Service:**
```bash
sudo systemctl start nba-server.service

```


* **Restart the Service** (run this after pulling code updates or changing ports):
```bash
sudo systemctl restart nba-server.service

```


* **Enable Auto-Start on Boot:**
```bash
sudo systemctl enable nba-server.service

```



### 4. Viewing Logs

To stream live server output or troubleshoot incoming HTTP requests from your ESP32:

```bash
sudo journalctl -u nba-server.service -f

```

To view the last 50 lines without opening a scrolling pager:

```bash
sudo journalctl -u nba-server.service -n 50 --no-pager

```

---

## 🎛️ ESP32-S3 Controller Setup

1. **Configure Wi-Fi & Base URL:**
Open `config.h` in your Arduino IDE or PlatformIO workspace and update your network credentials and server endpoint:
```cpp
#define WIFI_SSID "your_wifi_ssid"
#define WIFI_PASSWORD "your_wifi_password"
#define PI_BASE_URL "http://<YOUR_PI_IP>:8080"

```


2. **Compile and Flash:**
Ensure you have the `ESP32-HUB75-MatrixPanel-I2S-DMA`, `ArduinoJson`, and Adafruit GFX libraries installed, then flash the sketch to your ESP32-S3.
