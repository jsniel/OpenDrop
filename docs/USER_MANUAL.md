# OpenDrop User Manual

OpenDrop is a low-power autonomous irrigation controller.

Project by Jean-Sébastien Niel.  
GitHub: https://github.com/jsniel

---

## 1. Powering the device

1. Insert a charged 18650 battery.
2. Verify wiring.
3. Power on the system.

---

## 2. Buttons

OpenDrop uses two physical buttons.

---

### Wi-Fi configuration button

Hold the Wi-Fi button while the device wakes up.

The controller periodically wakes from deep sleep to check the buttons.

When the button is detected:

- the Wi-Fi access point starts;
- the configuration web interface becomes available;
- the LED blinks slowly.

Connect to:

```txt
SSID: arrosageESP
Password: secretESP32
```

Then open:

```txt
http://192.168.4.1
```

The Wi-Fi configuration mode automatically stops after 5 minutes.

---

### Automation button

Hold the automation button while the device wakes up.

This toggles automatic watering ON or OFF.

The setting is stored in EEPROM and remains active after reboot or deep sleep.

If automation is disabled while watering is active, the valve immediately closes.

---

## 3. Web interface

The web interface allows:

- enabling or disabling automation;
- setting watering start time;
- setting watering end time;
- setting watering duration;
- setting the number of watering cycles;
- setting the RTC time.

---

## 4. LED feedback

The status LED provides information about the current system state.

| LED behavior | Meaning |
|---|---|
| Solid ON | Valve open |
| Two short blinks | Automatic watering enabled |
| One long blink | Automatic watering disabled |
| Slow blinking | Wi-Fi configuration portal active |
| OFF | Deep sleep mode |

The LED is configured as active-LOW.

Before entering deep sleep, the firmware disables the LED pin to minimize current leakage and battery consumption.

---

## 5. Maintenance

Regularly check:

- battery charge;
- valve operation;
- waterproofing;
- cable condition;
- corrosion on connectors;
- enclosure sealing.

---

## 6. Safety

OpenDrop is a DIY prototype.

Do not expose unprotected electronics to rain, irrigation spray or condensation.

Use waterproof connectors and proper insulation for outdoor installations.
