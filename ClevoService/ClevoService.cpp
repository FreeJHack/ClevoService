#include <libkern/version.h>
#include <IOKit/IOCommandGate.h>
#include <sys/kern_control.h>
#include <ClevoService.hpp>

#include <IOKit/IODeviceTreeSupport.h>
#include <sys/errno.h>

OSDefineMetaClassAndStructors(com_none_ClevoService, IOService)

bool ClevoService::init(OSDictionary *dict)
{
    bool result = super::init(dict);
    device     = NULL;
    gWorkLoop  = NULL;
    gPollTimer = NULL;
    gKBLT      = 0;
    gOldKBLT   = 0;
    return result;
}

IOService* ClevoService::probe(IOService *provider, SInt32 *score)
{
    IOService *result = super::probe(provider, score);
    
    //Check our SSDT methods...
    IOACPIPlatformDevice* pDevice = OSDynamicCast(IOACPIPlatformDevice, provider);
    if (kIOReturnSuccess != pDevice->validateObject("CLVE")) {
        IOLog("CLV0: SSDT method CLVE not found\n");
        return NULL;
    }
    return result;
}

bool ClevoService::start(IOService *provider)
{
    char    name[5];
    
    device = OSDynamicCast(IOACPIPlatformDevice, provider);
    if (device == NULL || !super::start(provider))
        return false;
    
    //Not too early KB on, we just wait for DisplayMangler...
    if (RunningKernel() >= MakeKernelVersion(17,0,0)) {
        waitForService(serviceMatching("IODisplayWrangler"));
    } else {
        IOSleep(2000);
    }

    gWorkLoop = getWorkLoop();
    if (gWorkLoop == NULL)
        return false;
    gWorkLoop->retain();
    
    gPollTimer = IOTimerEventSource::timerEventSource( this, OSMemberFunctionCast(IOTimerEventSource::Action, this, &ClevoService::saveNVRAM) );
    if ((gPollTimer == NULL) || (kIOReturnSuccess != gWorkLoop->addEventSource(gPollTimer)))
        return false;
    
    // KBLT bytes: 0x0ABC  A: 0÷8 keyboard color, B: 0/1 backlight OFF/ON, C: 0÷3 backlight level
    gKBLT = 0x12;  // Default value: color BLUE, keybord ON, backlight level 2
    
    //Trying to read KBLT variable from NVRAM...
    if (IORegistryEntry *nvram = OSDynamicCast(IORegistryEntry, fromPath("/options", gIODTPlane))) {
        
        strlcpy(name, "KBLT", sizeof(name));
        const OSSymbol *tempName = OSSymbol::withCString(name);
        
        bool genericNVRAM = (0 == strncmp(nvram->getName(), "AppleNVRAM", sizeof("AppleNVRAM")));
        if (genericNVRAM) {
            nvram->IORegistryEntry::setProperty(tempName, OSData::withBytes(&gKBLT, sizeof(UInt32)));
        } else {
            if (OSData *myData = OSDynamicCast(OSData, nvram->getProperty(tempName)))
            {
                int length = myData->getLength();
                if (length > 0) {
                    bcopy(myData->getBytesNoCopy(), &gKBLT, length);
                }
            }
        }
        OSSafeReleaseNULL(tempName);
        OSSafeReleaseNULL(nvram);
    }
    
    //The standard _INI method turn OFF dual GPU RX2070 & airplane led (used for Shift-lock indication)
    //Remove from SSDT if not necessary...
    gOldKBLT = gKBLT;
    
    //SetUp KBD backlight
    kbdLightLevel(gKBLT & 3);  // Set backlight level...
    kbdSetColor(); //Set backlight color...
    kbdONOFF(); //Turn backlight on/off...
    
    IOLog("CLV0: START complete KBLT=0x%x", gKBLT);
    
    this->setProperty("KBLT", gKBLT, 8 * sizeof(UInt32));
    this->setName("ClevoKbdFanService");
    this->registerService(0);
    
    return true;
}

void ClevoService::stop(IOService *provider)
{
    saveNVRAM();
    
    if (gPollTimer != NULL)
    {
        gPollTimer->cancelTimeout();
        gWorkLoop->disableAllEventSources();
        gPollTimer->release();
        gPollTimer = NULL;
    }
    if (gWorkLoop != NULL)
    {
        gWorkLoop->release();
        gWorkLoop = NULL;
    }
    super::stop(provider);
}

// Receive Notify message from ACPI, wait 3 secs before writing NVRAM so user may change other KB backlight settings...
IOReturn ClevoService::message(UInt32 type, IOService * provider, void * argument)
{
    UInt8  level = gKBLT & 3;
    UInt32 *myMsg, color = gKBLT & 0xF00;

    if (type == kIOACPIMessageDeviceNotification)
    {
        myMsg = (UInt32 *)argument;
        switch (*myMsg) {
            case msgKbdChangeColor:
                gKBLT &= 0xFF;
                if (color < 0x800) gKBLT |= (color + 0x100);
                kbdSetColor();
                break;
            case msgKbdLightDOWN:
                if (level) kbdLightLevel(--level);
                break;
             case msgKbdLightUP:
                if (level < 3) kbdLightLevel(++level);
                break;
            case msgKbdToggleONOFF:
                if (gKBLT & 0x10) gKBLT &= 0xF0F; else gKBLT |= 0x10;
            case msgKbdWakeUp:
                kbdLightLevel(gKBLT & 3);  // Set backlight level...
                kbdSetColor(); //Set backlight color...
                kbdONOFF(); //Turn backlight on/off...
                break;
        }
        if (gPollTimer != NULL) {
            gPollTimer->cancelTimeout();
            gPollTimer->setTimeoutMS(3000);
        }
        //IOLog("CLV0: Notified 0x%x", *myMsg);
    }
    return super::message(type, provider, argument);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void ClevoService::ACPI_Send(uint32_t arg0, uint32_t arg1, uint32_t arg2)
{
    OSObject *params[3];
    params[0] = OSNumber::withNumber(arg0, 8 * sizeof(uint32_t));
    params[1] = OSNumber::withNumber(arg1, 8 * sizeof(uint32_t));
    params[2] = OSNumber::withNumber(arg2, 8 * sizeof(uint32_t));
    device->evaluateObject("CLVE", nullptr, params, 3); // Call our method...
}

void ClevoService::kbdONOFF(void)
{
    if (gKBLT & 0x10) {
        ACPI_Send(0, SET_KB_LED, 0xE007F001); // ON
        //kbdLightLevel(gKBLT & 3);  // Set Light level...
    } else {
        //ACPI_Send(0, SET_KB_LED, 0xF4000008); // Reducing light...
        //IOSleep(150);
        ACPI_Send(0, SET_KB_LED, 0xE0003001); // OFF
    }
}

void ClevoService::kbdSetColor(void)
{
    UInt32  newColor = 0, color = gKBLT & 0xF00;
    
    ACPI_Send(0, SET_KB_LED, 0x10000000);
    
    switch (color) {
        case 0x0:   newColor = 0x00FF0000; break; // Blue
        case 0x100: newColor = 0x0000FF00; break; // Red
        case 0x200: newColor = 0x00FFFFFF; break; // White
        case 0x300: newColor = 0x00FFFF00; break; // Pink
        case 0x400: newColor = 0x00FF00FF; break; // Light Blue
        case 0x500: newColor = 0x000000FF; break; // Green
        case 0x600: newColor = 0x0000FFFF; break; // Yellow
        case 0x700:                               // RGB
            ACPI_Send(0, SET_KB_LED, 0xF000FF00);
            ACPI_Send(0, SET_KB_LED, 0xF10000FF);
            ACPI_Send(0, SET_KB_LED, 0xF2FF0000);
            break;
        case 0x800:                                // Random...
            ACPI_Send(0, SET_KB_LED, 0x70000000);
            break;
    }
    if (newColor) {
        ACPI_Send(0, SET_KB_LED, 0xF0000000 | newColor); // Set New color
        ACPI_Send(0, SET_KB_LED, 0xF1000000 | newColor);
        ACPI_Send(0, SET_KB_LED, 0xF2000000 | newColor);
    }
}

void ClevoService::kbdLightLevel(UInt8 newLevel)
{
    newLevel &= 3;
    switch (newLevel) {
        case 0: ACPI_Send(0, SET_KB_LED, 0xF4000008); break;
        case 1: ACPI_Send(0, SET_KB_LED, 0xF4000020); break;
        case 2: ACPI_Send(0, SET_KB_LED, 0xF4000060); break;
        case 3: ACPI_Send(0, SET_KB_LED, 0xF40000F0); break;
    }
    gKBLT &= 0xFF0;
    gKBLT |= newLevel;
}

void ClevoService::saveNVRAM(void)
{
    bool    genericNVRAM;
    char    name[5];
    
    gKBLT &= 0xF13;
    if (gOldKBLT != gKBLT) {
        if (IORegistryEntry *nvram = OSDynamicCast(IORegistryEntry, fromPath("/options", gIODTPlane))) {
            strlcpy(name, "KBLT", sizeof(name));
            const OSSymbol *tempName = OSSymbol::withCString(name);
            
            if ((genericNVRAM = (0 == strncmp(nvram->getName(), "AppleNVRAM", sizeof("AppleNVRAM")))))
                IOLog("CLV0: fallback to generic NVRAM methods");
            
            if (genericNVRAM) {
                nvram->IORegistryEntry::setProperty(tempName, OSData::withBytes(&gKBLT, sizeof(UInt32)));
            } else {
                nvram->setProperty(tempName, OSData::withBytes(&gKBLT, sizeof(UInt32)));
                //IOLog("CLV0: genericNVRAM = false");
            }
            
            OSSafeReleaseNULL(tempName);
            OSSafeReleaseNULL(nvram);
            
            gOldKBLT = gKBLT;
            
            //device->setProperty("KBLT", kblt, 8 * sizeof(UInt64));
            this->setProperty("KBLT", gKBLT, 8 * sizeof(UInt32));
            IOLog("CLV0: NVRAM saved KBLT=0x%x",gKBLT);
        }
    }
}
