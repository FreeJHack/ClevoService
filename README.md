# ClevoService

[![Github release](https://img.shields.io/github/v/release/freejhack/ClevoService.svg?color=blue)](https://github.com/freejhack/ClevoService/releases/latest)
[![Github downloads](https://img.shields.io/github/downloads/freejhack/ClevoService/total.svg?color=blue)](https://github.com/freejhack/ClevoService/releases)

An open source kernel extension enabling keyboard backlight, AutoDim & fan control for Clevo Hackintosh.
Fully tested on Clevo P9XXEN_EF_ED, but it should even work on most Clevo's laptops provided with full-colour backlight keyboard.

# How it works
The keyboard backlight is controlled through the numeric keypad and restored during boot and sleep/wake.
Native NVRAM is necessary and supported by Clevo P9XXEN_EF_ED, however, the kext should even work with emulated NVRAM (not tested).

### The following features are supported:
* Backlight directly controlled from numeric keypad (same Windows® keys)
* Backlight colour, on/off and light level saved in NVRAM
* Backlight automatically restored during boot and sleep/wake
* Up to 9 preset keyboard backlight different colours
* Airplane led used to indicate Shift-Lock enabled/disabled
* Fully customizable
* Configurable backlight intensity levels
* AutoDim, an automatic keyboard backlight dimming including: configurable Activation, Dim or OFF and Dimming wait time, restore previous backlight intensity on key stroke, configuration customizable in SSDT-ClevoService.dsl

The below features are requiring `ClevoServiceAgent` running (v. 1.2.0+):
* Keyboard backliighting OSD icon
* Full Airplane compatibility with Wifi/BT power on/off
* Wifi/BT OSD icon

The keyboard backlighting is fully controlled from numeric keypad according to Clevo's standard keys:
* Change colour with `Fn /`
* Turn ON/OFF with `Fn *`
* Decrease backlight with `Fn -`
* Increase backlight with `Fn +`

## How AutoDim works
The AutoDim reduces, or completely turns OFF, the backlight If the keyboard is not used for the preset time.
It is fully controlled from ACPI with three properties: `KbdAutoDimTimerActive, KbdAutoDimActive, KbdAutoDimTime`

It is activated setting the  property `KbdAutoDimTimerActive` to true, or false for deactivate it.
The property `KbdAutoDimActive` is used to control whether the keyboard backlight will be dimmed, setting to true, or turned off, setting to false, after the waiting time is elapsed.
The waiting time is set with `KbdAutoDimTime`. The number must be in the range from 5 to 1800 (seconds).

Therefore, if the AutoDim is activated with `KbdAutoDimTimerActive = true`, the keyboard backlight is automatically reduced at the minimum with `KbdAutoDimActive = true`, or completely turned OFF with `KbdAutoDimActive = false`, after the wait time `KbdAutoDimTime`. Pressing any key, the backlight intensity is restored.

All the above properties can be modified in the `CONF` method of `CLV0` device included into SSDT-ClevoService.dsl file.
The boolean values can be set using the well known method `">y"` for true and `">n"` for false. (Thanks RM)
If the `CONF` method is not found or a property is missing, the default loaded values are the following:

    - KbdAutoDimTimerActive = true
    - KbdAutoDimActive = true
    - KbdAutoDimTime = 180
 
 It is not mandatory to change all properties at once, only the needed ones can be changed.
 You may want to check all the above properties in IORegystryExplorer looking at ClevoService driver attached to CLV0 device.
 
 ## Backlight levels configuration 
 ### For advanced users
 The `CONF` method may also include two properties for controlling the backlight intensity levels, `KbdDimmingLevel` and `KbdLevels`.
 KbdDimmingLevel is the intesity of the backlight when dimmed, if AutoDim is active.
 KbdLevels is an array containing four backlight levels used by ClevoService to change the backlight intesity when `Fn -` or `Fn +` keys are pressed.
 Index zero is the lowest, three the highest. The value is in the range 0÷255.
 Those properties can be added to the `CONF` method otherwise the default values will be used instead.
 
# How to install
In order to simplify the installation process, an SSDT (.dsl) file has been included in the package.
However, some  DSDT's methods need to be patched in order to properly work with provided SSDT.
The file also includes the methods to manage the three fans.

* Open the SSDT-ClevoService.dsl with MaciASL and save it as `.aml` file into Clover/OpenCore EFI ACPI/patched folder.
* Install the ClevoService kext into `/Library/Extensions` folder using your preferred kext installer or using terminal
* Rebuild kext cache and reboot

The following methods must be patched in Clover/OpenCore config.plist ACPI section changing the method name:

Rename _WAK => XWAK 

    * Comment: _WAK to XWAK, Find: 5F57414B, Replace: 5857414B
    
Rename _Q14 => XQ14 (Check your DSDT, Airplane key could be different)
    
    * Comment: _Q14 to XQ14, Find: 5F513134, Replace: 58513134

Rename _Q50 => XQ50

    * Comment: _Q50 to XQ50, Find: 5F513530, Replace: 58513530

## ClevoServiceAgent installation
Starting from version 1.2.0, the ClevoService kext includes in its PlugIns folder the `ClevoServiceAgent` file, which is needed for some OSD features and Wifi/Bluetooth power on/off. This is a low-level agent used by ClevoService that must be launched during boot.
For an easier installation procedure, the launching `com.fjhk.ClevoService.agent.plist` file is already provided. Simply install it in the OSX `/Library/LaunchAgents/` folder with the below terminal commands (without asterisk):

    * sudo cp yourPath/com.fjhk.ClevoService.agent.plist /Library/LaunchAgents
    * sudo chmod 644 /Library/LaunchAgents/com.fjhk.ClevoService.agent.plist
    * sudo chown root:wheel /Library/LaunchAgents/com.fjhk.ClevoService.agent.plist

then reboot.
Note that you have to change `yourPath` with the full directory's path where you saved the `.plist` file.
The ClevoService kext `must` be installed in the `/Library/Extensions` folder (no injecting) otherwise the agent cannot be loaded.

In order to check whether the agent is correctly installed and running, open `Activity Monitor` app and search for `ClevoServiceAgent`: it must be present, otherwise OSD icons and Wifi/BT power on/off won't work.

Below the terminal commands for unload and uninstall the `.plist` agent launching file (without asterisk):

    * launchctl unload /Library/LaunchAgents/com.fjhk.ClevoService.agent.plist
    * sudo rm /Library/LaunchAgents/com.fjhk.ClevoService.agent.plist

## Important Note
ClevoService is using the device DCHU for backlighting and it should be available in your original DSDT. However, if missing, search for method WMBB in the device WMI.
At least one method must be available in your DSDT otherwise the keyboard backlight won't work.
For any issue, you may want to install the debug version checking the system log for more details.
The Kext has been compiled with XCode 10.2.1 and fully tested in Mojave 10.14.6 and Catalina 10.15.5 (Min OSX 10.11).

### Credits
- A big thanks deserve [Datasone](https://github.com/datasone/ClevoControl) for the reverse engineering and for discovering FAN and KBD bios commands, giving me a good starting point and saving me a lot of reversing time.
- Thanks to [RehabMan](https://bitbucket.org/RehabMan/), [SlEePlEs5](https://github.com/SlEePlEs5/logKext) and [hieplpvip](https://github.com/hieplpvip) for some useful pieces of code
- [Apple](https://www.apple.com) for macOS  
