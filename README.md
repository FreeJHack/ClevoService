# ClevoService
An open source kernel extension enabling keyboard backlight, AutoDim & fan control for Clevo Hackintosh.
Fully tested on Clevo P9XXEN_EF_ED, but it should even work on most Clevo's laptops provided with full-colour backlight keyboard.

# How it works
The keyboard backlight is controlled through the numeric keypad and restored during boot/reboot and sleep/wake.
Native NVRAM is necessary and supported by Clevo P9XXEN_EF_ED, however, the kext should even work with emulated NVRAM (not tested).

The following features are supported:
* Backlight directly controlled from numeric keypad (same Windows® keys)
* Backlight colour, on/off and light level saved in NVRAM
* Backlight automatically set during boot/reboot and sleep/wake
* Up to 9 preset keyboard backlight different colours
* Airplane led used to indicate Shift-Lock enabled/disabled
* Fully customizable
* Configurable backlight intensity levels
* New AutoDim feature, an automatic keyboard backlight dimming including: configurable Activation, Dim or OFF and Dimming wait time, restore previous backlight intensity on key stroke, configuration customizable in SSDT-ClevoService.dsl 

The keyboard backlight is fully controlled from numeric keypad according to Clevo's standard keys:
* Change colour with `Fn /`
* Turn ON/OFF with `Fn *`
* Decrease backlight with `Fn -`
* Increase backlight with `Fn +`

## How AutoDim works
The AutoDim reduces, or completely turns OFF, the backlight If the keyboard is not used for the preset time.
It is fully controlled from ACPI with three properties: `KbdAutoDimTimerActive, KbdAutoDimActive, KbdAutoDimTime`

It is activated setting the  property `KbdAutoDimTimerActive` to true, or false for deactivate it.
The property `KbdAutoDimActive` is used to control whether the keyboard backlight will be dimmed, setting to true, or tuned off, setting to false, after the waiting time is elapsed.
The waiting time is set with `KbdAutoDimTime`. The number must be in the range from 5 to 1800 (seconds).

Therefore, if the AutoDim is activated with `KbdAutoDimTimerActive = true`, the keyboard backlight is automatically reduced at the minimum with `KbdAutoDimActive = true`, or completely turned OFF with `KbdAutoDimActive = false`, after the wait time `KbdAutoDimTime`. Pressing any key (Ctrl, Command & Option keys excluded) the backlight intensity is restored.

All the above properties can be modified in the `CONF` method of `CLV0` device included into SSDT-ClevoService.dsl file.
The boolean values can be set using the well known method `">y"` for true and `">n"` for false. (Thanks RM)
If the `CONF` method is not found or a property is missing, the loaded default values are the following:
 `KbdAutoDimTimerActive = true
 KbdAutoDimActive = true
 KbdAutoDimTime = 180`
 It is not necessay to change all properties at once, only the needed ones can be changed.
 
 ## Backlight levels configuration 
 ### For advanced users
 The `CONF` method may also include two properties for controlling the backlight intensity levels, `KbdDimmingLevel` and `KbdLevels`.
 KbdDimmingLevel is the intesity of the backlight when dimmed, if AutoDim is active.
 KbdLevels is an array containing four backlight levels used by ClevoService to chamge the backlight intesity when `Fn -` or `Fn +` keys are pressed.
 Index zero is the lowest, three the highest. The value is in the range 0÷255.
 Those properties can be added to the `CONF` method otherwise the default values will be used instead.
 
# How to install
In order to simplify the installation process, an SSDT (.dsl) file has been included in the package.
However, some  DSDT's methods need to be patched in order to properly work with provided SSDT.
The file also includes the methods to manage the three fans.

* Open the SSDT-CloverService.dsl with MaciASL and save it as `.aml` file into Clover EFI ACPI/patched folder.
* Install the ClevoService kext into `/Library/Extensions` folder using your preferred kext installer or using terminal
* Rebuild kext cache and reboot

The following methods must be patched in clover config.plist ACPI section changing the method name:

Rename _WAK => XWAK 

    * Comment: _WAK to XWAK, Find: 5F57414B, Replace: 5857414B
    
Rename _Q50 => XQ50

    * Comment: _Q50 to XQ50, Find: 5F513530, Replace: 58513530

You may want to check all the above properties in IORegystryExplorer looking at ClevoService driver attached to CLV0 device.

### Important Note
ClevoService is using the method DCHU for backlighting and this method should be available in your original DSDT. However, if missing, search for method WMBB in the device WMI.
At least one method must be available in your DSDT otherwise the keyboard backlight won't work.
For any issue, you may want to install the debug version checking the system log for more details.
The Kext has been compiled with XCode 10.2.1 and fully tested in Mojave 10.14.6.

### Credits
- A big thanks deserve [Datasone](https://github.com/datasone/ClevoControl) for the reverse engineering and for discovering FAN and KBD bios commands, giving me a good starting point and saving me a lot of reversing time.
- Thanks to [RehabMan](https://bitbucket.org/RehabMan/) and [SlEePlEs5](https://github.com/SlEePlEs5/logKext) for some useful piece of code
- [Apple](https://www.apple.com) for macOS  
