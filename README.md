# ClevoService
An open source kernel extension enabling keyboard backlight control for Clevo Hackintosh.

Fully tested on Clevo P9XXEN_EF_ED, but it should even work on most Clevo's laptops provided with full-colour backlight keyboard.

# How it works
The keyboard backlight is controlled through the numeric keypad and restored during boot/reboot and sleep/wake.
Native NVRAM is necessary and supported by Clevo P9XXEN_EF_ED, however, the kext should even work with emulated NVRAM (not tested).

The following features are supported:
* Backlight direct controlled from numeric keypad
* Backlight colour, on/off and light level saved in NVRAM
* Backlight automatically set during boot/reboot and sleep/wake
* Up to 9 preset keyboard backlight colours
* Airplane led used to indicate Shift-Lock enabled/disabled

The keyboard backlight is fully controlled from numeric keypad according to Clovo's standard keys:
* Change colour with `Fn + "/"`
* Turn ON/OFF with `Fn + "*"`
* Decrease backlight with `Fn + "-"`
* Increase backlight with `Fn + "+"`

# How to install
In order to simplify the installation process, an SSDT file has been included in the package.
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

### Coming Soon...
Keyboard & Fan control through preference panel (time permitting...)

### Credits
- A big thanks to [Datasone](https://github.com/datasone/ClevoControl) for the reverse engineering and for discovering FAN and KBD bios commands, giving me a good starting point and saving me a lot of reversing time.
- [Apple](https://www.apple.com) for macOS  
