#ifndef ClevoService_hpp
#define ClevoService_hpp

#define ClevoService com_none_ClevoService

#include <IOKit/IOService.h>
#include <IOKit/acpi/IOACPIPlatformDevice.h>
#include <IOKit/IOTimerEventSource.h>

#define RunningKernel() MakeKernelVersion(version_major,version_minor,version_revision)
#define MakeKernelVersion(maj,min,rev) (static_cast<uint32_t>((maj)<<16)|static_cast<uint16_t>((min)<<8)|static_cast<uint8_t>(rev))

#ifndef EXPORT
#define EXPORT __attribute__((visibility("default")))
#endif

#define SET_KB_LED          0x67
#define SET_FAN             0x79

#define msgKbdChangeColor   0x80
#define msgKbdLightDOWN     0x81
#define msgKbdLightUP       0x82
#define msgKbdWakeUp        0x83
#define msgKbdToggleONOFF   0x9F


class EXPORT ClevoService : public IOService
{
    typedef IOService super;
	OSDeclareDefaultStructors(com_none_ClevoService)
    
public:
	virtual bool init(OSDictionary *dictionary = 0) override;
	virtual IOService *probe(IOService *provider, SInt32 *score) override;
	virtual bool start(IOService *provider) override;
	virtual void stop(IOService *provider) override;
    virtual IOReturn message( UInt32 type, IOService *provider, void *argument) override;
	    
protected:
    IOWorkLoop*              gWorkLoop;
    IOTimerEventSource*      gPollTimer;
    UInt32                   gKBLT, gOldKBLT;

    void    ACPI_Send(uint32_t arg0, uint32_t arg1, uint32_t arg2);
    void    saveNVRAM(void);
    void    kbdONOFF(void);
    void    kbdSetColor(void);
    void    kbdLightLevel(UInt8 newLevel);
};

static IOACPIPlatformDevice *device;

#endif
