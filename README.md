# NBA Dual 64x64 LED Matrix Scoreboard & Boxscore Display

A high-performance, real-time NBA scoreboard and split-screen active player boxscore display powered by an **ESP32-S3**, dual chained **64x64 HUB75 RGB LED Matrices**, and a **Raspberry Pi** backend server.

To avoid heavy JSON overhead and memory fragmentation on the microcontroller, this project uses a split-tier architecture: the Raspberry Pi polls live NBA data, flattens team payloads, and handles offline logo conversion, while the ESP32-S3 acts as a high-frequency display driver using DMA.

---

## 🏗️ System Architecture

```text
[ NBA / ESPN API Endpoints ]
          │
          ▼  (Python / Flask / Requests)
[ Raspberry Pi Server ]  ──► (Parses game states, team colors, & nested boxscores)
          │
          ▼  (HTTP JSON over Local Wi-Fi)
[ ESP32-S3 + Dual 64x64 Matrices ] (Renders 128x64 UI & 16x16 RGB565 logos via DMA)

```

---

## 🧰 Hardware Components

| Component | Description |
| --- | --- |
| **LED Matrices** | Dual chained 64x64 HUB75 RGB LED Matrix Panels (128x64 total canvas) |
| **Microcontroller** | ESP32-S3 development board (with HUB75 adapter / audio controller board) |
| **Power Supply** | Nekmit 47W Dual-Port USB-C wall block + dual 10ft 100W/5A USB-C cables (independent panel power) |
| **Server** | Raspberry Pi (running Raspberry Pi OS / Linux) |

---

## 🚀 Getting Started From Scratch

### Part 1: Raspberry Pi Server Setup

1. **Clone or create your project directory on the Pi:**

```bash
mkdir -p ~/nba-matrix-server
cd ~/nba-matrix-server

```

2. **Create and activate a Python virtual environment** (to comply with PEP 668 externally managed environment rules):

```bash
python3 -m venv venv
source venv/bin/activate

```

3. **Install dependencies:**

```bash
pip install -r requirements.txt

```

4. **Generate Team Logos (Optional / One-Time):**
The logos should already be in `/controller/logos.h`, but, if needed, you can run the Pillow utility script to fetch team PNGs, composite transparency onto black, and output 16x16 RGB565 arrays with:

```bash
python3 generate_logos.py

```

5. **Configure Auto-Start (`systemd` Service):**
Create a systemd service file to run the Flask backend automatically in the background on boot:

```bash
sudo nano /etc/systemd/system/nba-server.service

```

Paste the following configuration (adjust usernames, paths, and port flags to match your environment):

```ini
[Unit]
Description=NBA Matrix Flask Server
After=network.target

[Service]
Type=idle
User=pi
WorkingDirectory=/home/pi/nba-matrix-server
ExecStart=/home/pi/nba-matrix-server/venv/bin/python /home/pi/nba-matrix-server/app.py
Restart=always

[Install]
WantedBy=multi-user.target

```

Enable and start the service:

```bash
sudo chmod 644 /etc/systemd/system/nba-server.service
sudo systemctl daemon-reload
sudo systemctl enable nba-server.service
sudo systemctl start nba-server.service

```

---

### Part 2: ESP32-S3 Controller Setup

1. **Arduino IDE Libraries:**
Make sure you have the ESP32 board definitions installed, along with the required matrix, graphics, and JSON libraries:

* `ESP32-HUB75-MatrixPanel-I2S-DMA`
* `Adafruit GFX Library`
* `ArduinoJson`

2. **Setup Connection Details:**
Create a `config.h` file in the `/controller` folder (copying from `config.h.example` if applicable) and update it with your Wi-Fi credentials and your Raspberry Pi's local static IP address and port:

```cpp
#define WIFI_SSID "your_wifi_ssid"
#define WIFI_PASSWORD "your_wifi_password"
#define PI_BASE_URL "http://<YOUR_PI_IP>:8080"

```

3. **Upload & Run:**
Connect your ESP32-S3 to your PC, verify your HUB75 pin mappings and panel chain length (`NUM_COLS = 2`, `64x64`), and flash the firmware. The ESP32 will sync time via NTP, fetch live scores and active player boxscores from your Pi, and render the split-screen interface on the 128x64 display.

---

## 🛠️ Server Maintenance & Logs

* **Restart the server** (after updating code or changing ports):
```bash
sudo systemctl restart nba-server.service

```


* **Check service status:**
```bash
sudo systemctl status nba-server.service

```


* **View recent logs:**
```bash
sudo journalctl -u nba-server.service -n 50 --no-pager

```


* **Stream live logs:**
```bash
sudo journalctl -u nba-server.service -f

```



---

*This project is not affiliated with, endorsed by, or connected to the National Basketball Association (NBA) or ESPN. All team names, logos, and trademarks are the property of their respective owners.*

```

```
