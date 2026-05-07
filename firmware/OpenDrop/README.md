# OpenDrop firmware

ESP32-C3 firmware for the OpenDrop low-power wireless irrigation controller.

Firmware by Jean-Sébastien Niel.  
GitHub: https://github.com/jsniel

License: GPL-3.0-or-later

---

## Arduino IDE setup

### 1. Add ESP32 board support

Open Arduino IDE settings or preferences.

Add this URL to **Additional Boards Manager URLs**:

```txt
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

---

### 2. Install ESP32 board package

Open:

```txt
Tools → Board → Boards Manager
```

Search for:

```txt
esp32
```

Install:

```txt
esp32 by Espressif Systems
```

Tested version:

```txt
3.3.5
```

---

### 3. Select board

Select:

```txt
ESP32C3 Dev Module
```

---

## Required libraries

Install from the Arduino Library Manager:

| Library | Tested version | Notes |
|---|---:|---|
| RTClib by Adafruit | 2.1.4 | DS3231 RTC support |
| Adafruit BusIO | 1.17.4 | RTClib dependency |

See also:

```txt
libraries.txt
```

---

## Default access point

```txt
SSID: arrosageESP
Password: secretESP32
IP address: 192.168.4.1
```

Open:

```txt
http://192.168.4.1
```

---

## Buttons

### Wi-Fi button

GPIO: `GPIO10`

Holding the button during wake-up starts the Wi-Fi configuration portal for 5 minutes.

### Automation button

GPIO: `GPIO21`

Holding the button during wake-up toggles automatic watering ON or OFF.

The setting is saved in EEPROM.

If automation is disabled while watering is active, the valve is immediately closed.

---

## LED behavior

| LED behavior | Meaning |
|---|---|
| Solid ON | Valve open |
| Two short blinks | Automation enabled |
| One long blink | Automation disabled |
| Slow blinking | Wi-Fi portal active |
| OFF | Deep sleep |

The LED is configured as active-LOW.

The firmware disables the LED pin before deep sleep to minimize current consumption.

---

## Pin mapping

| Function | GPIO |
|---|---:|
| Wi-Fi mode button | 10 |
| Automation toggle button | 21 |
| Relay open | 4 |
| Relay close | 1 |
| LED | 2 |
| SDA | 8 |
| SCL | 9 |

---

## Deep sleep behavior

Default values in the firmware:

```txt
Button polling interval: 10 seconds
Web configuration window: 5 minutes
Relay pulse duration: 200 ms
```

---

## Web interface routes

| Route | Description |
|---|---|
| `/` | Main configuration page |
| `/save` | Save watering settings |
| `/setrtc` | Set RTC time |
| `/sleep` | Stop Wi-Fi and return to sleep |
