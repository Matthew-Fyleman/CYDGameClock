# CYD Game Clock

Animated retro-game clocks (Mario, Space Invaders, Breakout, Pac-Man, plus plain styles) and a Windows PC stats monitor for the 3.5" ESP32 "Cheap Yellow Display" (ESP32-3248S035R, ST7796 480×320 with touch).

This is a port of [SmallOLED-PCMonitor](https://github.com/Keralots/SmallOLED-PCMonitor) by Keralots from a 128×64 OLED to the CYD's colour touch screen. It adds:

* **Colour game clocks** drawn on a 3×-scaled 128×64 canvas
* **Touch icons** (clock style, PC stats, information) next to the date
* **WiFi setup portal** with a QR code and a credential check before saving
* **Encrypted stats link:** AES-256-GCM with a pairing code, plus replay protection
* **Password-protected** settings website (Digest authentication)
* **USB-serial fallback**, with automatic detection of the clock's COM port

## Documents

* [Functional specification](Docs/FSD.md)
* [Setting up the clock securely on your WiFi](Docs/WiFi%20Security%20Setup.md): step-by-step, for non-technical users

## Build and upload the firmware

You need [PlatformIO](https://platformio.org/). Then run:

```
pio run -t upload --upload-port COM3
```

When the clock first starts, it opens the WiFi setup portal; follow the instructions on its screen.

## PC daemon

```
cd daemon
pip install -r requirements.txt
python cyd_game_clock_daemon.py --edit       # choose sensors, enter clock address and pairing code
python cyd_game_clock_daemon.py --minimized  # run in the system tray
python cyd_game_clock_daemon.py --autostart enable
```

For temperatures, fan speeds and similar sensors, run [LibreHardwareMonitor](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor) as administrator with its Remote Web Server enabled.

## Credits

* Clock animations, settings website and the original daemon: SmallOLED-PCMonitor by Keralots (open source, "feel free to modify and share")
* CYD pin-out and touch calibration: the Deskbuddy project
