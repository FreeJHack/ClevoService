
#ifndef ClevoService_hpp
#define ClevoService_hpp

#define ClevoService ClevoService

#define EXPORT __attribute__((visibility("default")))

//#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#include <IOKit/acpi/IOACPIPlatformDevice.h>
//#pragma clang diagnostic pop
#include <IOKit/IODeviceTreeSupport.h>
#include <libkern/version.h>

#define private public
#define protected public
#include <IOKit/hidsystem/IOHIKeyboard.h>
#undef private
#undef protected

//Kernel Server
extern "C" {
#include <sys/kern_event.h>
}

#define RunningKernel() MakeKernelVersion(version_major,version_minor,version_revision)
#define MakeKernelVersion(maj,min,rev) (static_cast<uint32_t>((maj)<<16)|static_cast<uint16_t>((min)<<8)|static_cast<uint8_t>(rev))

#ifdef DEBUG
#define DEBUG_LOG(args...)  do { IOLog(args); } while (0)
#else
#define DEBUG_LOG(args...)  do { } while (0)
#endif

#define SET_KB_LED          0x67
#define SET_FAN             0x79

#define msgKbdChangeColor   0x80
#define msgKbdLightDOWN     0x81
#define msgKbdLightUP       0x82
#define msgKbdWakeUp        0x83
#define msgAirplane         0x84
#define msgKbdToggleONOFF   0x9F

#define NVRAM_WAIT_TIME     3000        //3 secs. Time is in mS
#define ACPI_METD_NAME      "CLVE"      //Our ACPI method name
#define ACPI_CONF_NAME      "CONF"      //Configuration package name
#define KBD_VAR_NAME        "KBLT"      //ACPI backlight var name
#define ClevoVendorSTR      "com.fjhk"  //Server Vendor String
#define ClevoEventCode      0x1962      //Server Event Code
#define KBD_ON_MASK         0x10        //Kbd On/Off Mask
#define KBD_DEF_TIME        180000      //Default 3 minutes. Time is in mS
#define KBD_DIM_LEVEL       5           //Default backlight value when auto dimmed
#define KBD_L0_LEVEL        10          //Default backlight index 0 value
#define KBD_L1_LEVEL        32          //Default backlight index 1 value
#define KBD_L2_LEVEL        96          //Default backlight index 2 value
#define KBD_L3_LEVEL        248         //Default backlight index 3 value

enum {evtKeyboardBacklight = 1, evtAirplaneMode };  //Clevo Server Event Type

// Our ClevoService Class...
class EXPORT ClevoService : public IOService
{
    OSDeclareDefaultStructors(ClevoService)
    
public:
	virtual bool        init(OSDictionary *dictionary = 0) override;
	virtual IOService * probe(IOService *provider, SInt32 *score) override;
	virtual bool        start(IOService *provider) override;
	virtual void        stop(IOService *provider) override;
    virtual IOReturn    message( UInt32 type, IOService *provider, void *argument) override;
    virtual void        kbdClearKeyboards();
    virtual void        kbdSetUpBacklight();
    virtual void        kbdLightLevel(UInt8 newLevel);

protected:
    IOWorkLoop          * gWorkLoop;
    IOTimerEventSource  * gNVRAMTimer;
    OSArray             * loggedKeyboards;
    UInt32              gOldKBLT, gKBDInUse;
    UInt8               gKBDLevel[4], gKBDDimmingLevel;
    u_int32_t           vendorID;

    bool    setVendorID(const char *vendorCode);
    bool    sendUserLandMessage(int type, int param1, int param2);
    void    ACPI_Send(uint32_t arg0, uint32_t arg1, uint32_t arg2);
    void    saveNVRAM();
    void    kbdTimerFired();
    void    kbdONOFF();
    void    kbdSetColor();
    void    loadConfiguration();
    OSObject  * translateEntry(OSObject* obj);
    OSObject  * translateArray(OSArray* array);
    
    static bool kbdNotificationHandler(void * target, void * refCon, IOService * newService, IONotifier * notifier);
    static bool kbdTerminatedNotificationHandler(void * target, void * refCon, IOService * newService, IONotifier * notifier);
    
};

static  IOACPIPlatformDevice          * device;
        IONotifier                    * notify, *notifyTerm;
        ClevoService                  * gKBDService;
        IOTimerEventSource            * gKBDTimer;
        KeyboardEventCallback           origCallback;
        KeyboardSpecialEventCallback    origSpecialCallback;
        UInt32                          gKBLT, gKBDTime;
        Boolean                         gKBDIsOFF, gKBDimmed, gDimTimerActive, gKBDJustDim;


void logAction(OSObject *,unsigned,unsigned,unsigned,unsigned,
               unsigned,unsigned,unsigned,unsigned,bool,AbsoluteTime,OSObject *,void *);

void specialAction(OSObject *,unsigned,unsigned,
                   unsigned,unsigned,UInt64,bool,AbsoluteTime,OSObject *,void *);

#endif
