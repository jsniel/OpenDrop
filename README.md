# OpenDrop

**Low-power wireless irrigation controller**

OpenDrop is an open-source autonomous irrigation controller based on an ESP32-C3 Mini.

Created and maintained by Jean-Sébastien Niel.

GitHub:  
https://github.com/jsniel

---

## Features

- ESP32-C3 based controller
- Deep sleep low-power operation
- Local Wi-Fi configuration portal
- RTC scheduling with DS3231
- Relay controlled irrigation valve
- Battery-powered operation
- 3D-printable enclosure
- Open-source hardware and firmware

---

## Buttons and LED behavior

OpenDrop uses two physical buttons and one status LED.

### Wi-Fi button

GPIO: `GPIO10`

Hold the Wi-Fi button while the controller wakes up to start the configuration access point.

When active:

- the ESP32 creates a Wi-Fi access point;
- the web interface becomes available;
- the LED blinks slowly.

Default configuration:

```txt
SSID: arrosageESP
Password: secretESP32
Address: http://192.168.4.1
```

The Wi-Fi configuration mode automatically stops after 5 minutes.

---

### Automation button

GPIO: `GPIO21`

Hold the automation button while the controller wakes up to enable or disable automatic watering.

The setting is saved in EEPROM.

If watering is active while automation is disabled, the valve is immediately closed.

---

### LED behavior

GPIO: `GPIO2`

The LED is configured as active-LOW.

| LED behavior | Meaning |
|---|---|
| Solid ON | Valve open |
| Two short blinks | Automation enabled |
| One long blink | Automation disabled |
| Slow blinking | Wi-Fi configuration mode |
| OFF during sleep | Deep sleep active |

The firmware completely disables the LED pin before deep sleep to minimize power consumption.

---

## Repository structure

```txt
OpenDrop/
├── firmware/
├── hardware/
├── enclosure/
├── docs/
└── LICENSES/
```

---

## Photos

Prototype photos are available in:

```txt
docs/photos/
```

Wiring schematic:

```txt
docs/wiring/openDrop-complete-schematic.jpg
```

---

## Firmware build environment

The firmware is designed for the Arduino IDE.

Tested with:

- ESP32 Arduino Core by Espressif Systems: 3.3.5
- RTClib by Adafruit: 2.1.4
- Adafruit BusIO: 1.17.4

See:

```txt
firmware/OpenDrop/README.md
firmware/OpenDrop/libraries.txt
```

---

## Licenses

Hardware:  
CERN-OHL-S-2.0

Firmware:  
GPL-3.0-or-later
