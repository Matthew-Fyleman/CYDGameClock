# Setting up the CYD Game Clock securely on your WiFi

This guide takes you through connecting the clock to your home WiFi and linking it to your PC so that only your PC can send it information. It takes about 15 minutes. You don't need any technical knowledge; just follow the steps in order.

## What the security protects you from

Your PC sends its statistics (temperatures, how busy it is) to the clock over your home WiFi. Several safeguards keep this private and safe:

* **A pairing code.** The clock makes up a secret code of 16 letters and numbers. Your PC is given this code once, by you. Everything sent from the PC to the clock is scrambled (encrypted) with it, so other devices on your network cannot read the information or pretend to be your PC.
* **Replay protection.** Every message is numbered and time-stamped, so nobody can record a message and play it back later.
* **A password on the settings page.** Only someone who knows the password can change the clock's settings or install new software on it.
* **A temporary password for setup.** When the clock is being set up, the temporary WiFi network it creates gets a new random password each time.

You do not need to change anything on your router for any of this to work.

## Before you start

You will need:

* The clock, plugged into power with its USB cable
* A phone or tablet with WiFi and a camera
* The name and password of your home WiFi network
* Your Windows 11 PC, with the CYD Game Clock files copied onto it

> **Tip:** the clock only works with **2.4 GHz** WiFi. Most home routers provide both 2.4 GHz and 5 GHz under the same name, and that is fine. If your router shows two network names (often one ending in "5G"), choose the one **without** "5G".

---

## Part 1: Connect the clock to your WiFi

1. **Plug in the clock.** The first time it starts, it shows **WiFi setup**, a network name such as `CYDGameClock-9B48`, a **password**, and a square barcode (QR code).

2. **Connect your phone to the clock.**
   * **Easiest:** open your phone's camera, point it at the QR code and tap the message that appears ("Join network").
   * **Or:** open your phone's WiFi settings, choose the network shown on the clock (e.g. `CYDGameClock-9B48`) and type the password shown under it.

   Your phone may warn that this network has "no internet". That is expected; choose to **stay connected**.

3. **The setup page opens by itself** after a few seconds. If it doesn't, open your web browser and type `192.168.4.1` into the address bar.

4. **Choose your home network** from the list by tapping it. If it's not listed, tap **Search again**, or type its name exactly in **Network name**.

5. **Type your home WiFi password** in **Password**. Tick **Show password** to check you've typed it correctly; capital letters matter.

6. **Tap Connect.** The clock tries your network, which takes up to 20 seconds.
   * If it says **Could not connect**, check the password and try again.
   * When it says **Connected!**, the clock saves the details and restarts. You can now reconnect your phone to your normal home WiFi.

7. After restarting, the clock shows **Connected!**, its address on your network, and the **pairing code**, which looks like `ABCD-EFGH-JKLM-NPQR`. **Write the pairing code down**; you'll need it in Part 3.
   You can see this information again at any time by tapping the **(i)** icon at the top-right of the clock.

---

## Part 2: Secure the clock's settings page

The clock has a settings page that you open in a web browser. It is protected by a password.

1. On your PC, which must be connected to the same home network as the clock, open your web browser (for example Edge or Chrome).

2. Type `http://cydgameclock.local` into the address bar and press Enter.
   If that doesn't open, type the clock's address instead, shown on the **(i)** screen, for example `http://192.168.1.57`.

3. A login box appears:
   * **User name:** `admin`
   * **Password:** the **pairing code**, including the dashes, e.g. `ABCD-EFGH-JKLM-NPQR`

4. **Recommended: choose your own password for this page.**
   1. Scroll down and tap **🔒 Security & PC Link** to open that section.
   2. Under **Password for this settings page**, type a new password of at least 8 characters. Use something you don't use anywhere else.
   3. Click **Change password**. The browser asks you to log in again; use `admin` and your new password.

   If you ever forget it: tap **(i)** on the clock and hold **Forget WiFi** for 3 seconds. The settings-page password goes back to the pairing code. You will need to repeat Part 1 to reconnect the clock to your WiFi.

---

## Part 3: Link your PC to the clock

### 3a. Install the helper program (one time only)

1. Install **Python** from the Microsoft Store (search for "Python 3.12" or newer) if it is not already installed.
2. Install **LibreHardwareMonitor** (free) if you want temperatures and fan speeds:
   * Download it from its GitHub page, unzip it and run `LibreHardwareMonitor.exe` **as administrator**. Right-click it and choose **Run as administrator**.
   * In its menu, choose **Options → Remote Web Server → Run**, so the helper program can read the sensors. Also tick **Options → Run On Windows Startup** if you want it to start automatically.
3. Open the CYD Game Clock folder on your PC, then the `daemon` folder.
4. Click the address bar of that File Explorer window, type `cmd` and press Enter. A black command window opens in that folder.
5. Type this line and press Enter, then wait for it to finish:
   ```
   pip install -r requirements.txt
   ```

### 3b. Enter the pairing code

1. In the same command window, type:
   ```
   python cyd_game_clock_daemon.py --edit
   ```
2. A configuration window opens. At the top:
   * **Clock address:** leave it as `cydgameclock.local`, or type the clock's address from its (i) screen (e.g. `192.168.1.57`). The number is more reliable on some networks.
   * **Pairing code:** type the code from the clock, e.g. `ABCD-EFGH-JKLM-NPQR`. Capitals and dashes are optional.
   * **Connection:** leave it on **auto**. The PC then uses WiFi, and switches to the USB cable automatically if WiFi stops working.
3. Tick the statistics you want (for example CPU usage, CPU temperature, GPU temperature), then click **Save and start**.
4. Within a few seconds the clock switches to the PC stats screen. Tap the **(i)** icon: **PC stats link** should say **WiFi, encrypted (AES-256)**.

To choose where each statistic appears on the clock, open the settings page (Part 2) and use **Visible Metrics**.

### 3c. Start automatically with Windows (optional)

In the configuration window, click **Enable** next to **Windows Autostart**. From the next time you sign in, the helper runs quietly in the background as a small icon near the clock in the taskbar. Right-click that icon to change the settings or quit.

---

## Keeping it secure

| Do | Why |
|---|---|
| Use WPA2 or WPA3 security on your router, with a strong WiFi password | The clock's own protections work best on a network strangers can't join |
| Keep the pairing code private: don't post photos of the clock's (i) screen | Anyone with the code could send the clock fake statistics |
| Make a **new pairing code** if you think someone else has seen it | Settings page → Security & PC Link → **Make a new pairing code**, then enter the new code in the PC helper (`--edit`) |
| Set your own settings-page password | Stops other people on your network changing the clock |
| If you give the clock away, hold **Forget WiFi** on the (i) screen for 3 seconds first | This deletes your WiFi password from the clock |

### Checking that nothing is trying to get in

The **(i)** screen and the settings page show **Rejected packets**. A small number after you change the pairing code is normal (the PC was still using the old code). If the number keeps growing while your PC has the correct code, something else on your network is sending messages to the clock. Change your WiFi password and make a new pairing code.

---

## Troubleshooting

| Problem | What to do |
|---|---|
| The setup page doesn't open on the phone | Make sure the phone is still connected to the `CYDGameClock-…` network, then type `192.168.4.1` in the browser |
| "Could not connect" during setup | Check the password (tick **Show password**). Make sure you chose the 2.4 GHz network. Move the clock closer to the router. |
| `cydgameclock.local` doesn't open | Use the number address from the clock's (i) screen instead |
| The PC stats never appear, and **Rejected packets** goes up | The pairing code in the PC helper is wrong: run `--edit` and type it again |
| The PC stats never appear, and **Rejected packets** stays at 0 | The PC can't reach the clock. Check that both are on the same network and that the clock address is right. As a fallback, connect the clock to the PC with its USB cable: with **Connection: auto** the stats then come over the cable. |
| **PC stats link** says "USB serial cable" | WiFi isn't getting through, but the cable is working. Check the clock address and the pairing code. |
| Windows Firewall asks about Python | Choose **Private networks** and click **Allow**, so the clock's replies can reach the PC |
| You moved house or changed your router | On the clock, tap (i) and hold **Forget WiFi** for 3 seconds, then repeat Part 1 |
