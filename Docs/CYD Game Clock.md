# CYD Game Clock

## Role & Persona: 

You are an esp32 programmer creating migrating a ESP32/Oled display clock project to a ESP32 based CYD. It also provides Windows 11 PC monitoring stats.

## Context: 

The main component is:

An ESP-32 based 3.5 inch CYD currently attached to COM6 and utilized by the Deskbuddy project described in the repository found in the C:\Users\matth\OneDrive\Documents\PlatformIO\Projects\Deskbuddy folder of this computer. There is no need to preserve the Deskbuddy firmware currently on the device when the firmware for this project is uploaded.

## Main Tasks: 

1. Analyse the code in the Deskbuddy repository to get the necessary information about the CYD attached to COM6, such as display pixel dimensions, ESP32 pinout and connections to the display.
2. Analyse the code in the SmallOLED-PCMonitor repository found in the C:\Users\matth\OneDrive\Documents\PlatformIO\Projects\SmallOLED-PCMonitor folder of this computer. Port the firmware to work on the CYD attached to COM6, and create a PlatformIO project for the new firmware in the current folder and commit it to the git repository in this folder.
3. Copy the python code for the supporting daemon in the  SmallOLED-PCMonitor folder to this folder, modify it  to work with the new firmware and commit it to the git repository in this folder. The daemon should have a negligeable impact on the Windows 11 computer's performance.
4. Create a functional specification describing the new firmware in .MD format and store it in the Docs folder in the current folder.

## Detailed specification:

### Firmware

Create firmware for the ESP32 built-in to the CYD that will do the following:

1. On boot after reset, if it finds no stored WiFi credentials, will create a WAP and provide a rudimentary web site for entering the local WiFi credentials through a user-friendly interface. Once the WiFi credentials have been entered and verified, they will be securely stored in NVM on the microcontroller, and the microcontroller will reset.
2. On boot after reset, if it finds WiFi credentials in the NVM it will attach to the WiFi,.
3. Have all the functionality of the project in the SmallOLED-PCMonitor folder, including the configuration website once the WiFi credentials have been captured.
4. Change the colours of the following elements of the various game versions of the clock UI:
   1. PacMan in the PacMan clock should be yellow and the clock dots should be cyan.
   2. Mario in the Mario Clock should look like and be coloured like a simplified version of Mario in the Nintendo SuperMario games (black for the cap, boots and face features, white for the face, red for the body suit). The clock dots should be yellow.
   3. The Laser in the Space Invaders clock should be cyan, the laser bolts should be yellow, and the clock dots should be green.
   4. The paddle in the breakout clock should be red, and each row of the clock dots should be a different randomised colour chosen from (Red, Blue, Green, Yellow, White, Cyan).
5. There will be three small icons right justified, with 5 pixels spacing to the right of the date displayed at the top of the screen. The left icon will be a clock face and tapping on it will cycle through the available clock UIs. The middle icon will be a simplified computer and will switch the display to the PC stats. The right icon will be an information symbol, and will display information about the clock including its IP address. 
6. If it is possible, use the WiFi connection to read the stats for the WIndows 11, but ensure that the WiFi connection is as secure as possible. If the WiFi option is used, provide detailed instructions on how to set up the WiFi security options in .MD format in language that an educated, non-technical user can follow. Store these instructions in the Docs folder of this folder. 

### Daemon

Create a daemon in Python to run on the Windows 11 computer that will do the following:

1. Monitor the host PCs statistics configured in the configuration website and transmit them to the CYD over the secured WiFi network it is attached to if possible, or a COM port if not.
2. If the daemon is not connected to the WiFi network, it will automatically detect the COM port the CYD is connected to, even if there are other CYD devices attached to other COM ports. 

## Output: 

Create an FSD for the project in .MD format in the docs folder located in the current folder, then generate the firmware code for the project as a Platform IO project in the current folder. Ensure the firmware compiles correctly and upload to the CYD on COM6. Generate the Python code for the daemon  store it in an appropriate folder in the current folder and ensure it compiles and runs correctly. Place all code and documentation under git control in the repository in the current folder. Commit and push. 