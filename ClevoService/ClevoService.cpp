#include "ClevoService.hpp"

bool ClevoService::init(OSDictionary * dict)
{
    bool result = super::init(dict);
    device          = NULL;
    gWorkLoop       = NULL;
    gNVRAMTimer     = NULL;
    gKBDTimer       = NULL;
    gKBDTime        = KBD_DEF_TIME;
    gKBDInUse       = 0;
    gKBLT           = 0;
    gOldKBLT        = 0;
    notify          = NULL;
    gKBDService     = NULL;
    gKBDIsOFF       = false;
    gDimTimerActive = true;
    gJustDim        = true;

    origCallback        = NULL;
    origSpecialCallback = NULL;
    
    gKBDDimmingLevel = KBD_DIM_LEVEL;
    gKBDLevel[0]     = KBD_L0_LEVEL;
    gKBDLevel[1]     = KBD_L1_LEVEL;
    gKBDLevel[2]     = KBD_L2_LEVEL;
    gKBDLevel[3]     = KBD_L3_LEVEL;

    return result;
}

IOService* ClevoService::probe(IOService * provider, SInt32 * score)
{
    IOService *result = super::probe(provider, score);
    
    //Check our SSDT methods...
    IOACPIPlatformDevice* pDevice = OSDynamicCast(IOACPIPlatformDevice, provider);
    if (kIOReturnSuccess != pDevice->validateObject("CLVE")) {
        IOLog( "%s: SSDT method CLVE not found\n", pDevice->getName() );
        return NULL;
    }
    return result;
}

bool ClevoService::start(IOService * provider)
{
    char    name[5];
    
    DEBUG_LOG( "%s: Starting...\n", getName() );
    this->setName("ClevoService");
    
    device = OSDynamicCast(IOACPIPlatformDevice, provider);
    if (device == NULL || !super::start(provider)) return false;
    
    //KB ON not too early, we just wait for DisplayMangler...
    if (RunningKernel() >= MakeKernelVersion(17,0,0)) {
        waitForService(serviceMatching("IODisplayWrangler"));
    } else {
        IOSleep(2000);
    }

    gWorkLoop = getWorkLoop();
    if (gWorkLoop == NULL) return false;
    gWorkLoop->retain();
    
    gNVRAMTimer = IOTimerEventSource::timerEventSource( this, OSMemberFunctionCast(IOTimerEventSource::Action, this, &ClevoService::saveNVRAM) );
    if ((gNVRAMTimer == NULL) || (kIOReturnSuccess != gWorkLoop->addEventSource(gNVRAMTimer))) {
        DEBUG_LOG( "%s: gNVRAMTimer failed...\n", getName() );
        return false;
    }
    
    gKBDTimer = IOTimerEventSource::timerEventSource( this, OSMemberFunctionCast(IOTimerEventSource::Action, this, &ClevoService::kbdTimerFired) );
    if ((gKBDTimer == NULL) || (kIOReturnSuccess != gWorkLoop->addEventSource(gKBDTimer))) {
        DEBUG_LOG( "%s: gKBDTimer failed...\n", getName() );
        return false;
    }
    
    // KBLT bytes: 0x0ABC  A: 0÷8 keyboard color, B: 0/1 backlight OFF/ON, C: 0÷3 backlight level
    gKBLT = 0x12;  // Default value: color BLUE, keybord ON, backlight level 2
    
    //Trying to read KBLT variable from NVRAM...
    if (IORegistryEntry *nvram = OSDynamicCast(IORegistryEntry, fromPath("/options", gIODTPlane))) {
        
        strlcpy(name, KBD_VAR_NAME, sizeof(name));
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
    kbdSetUpBacklight( 0 );

    gKBDService = this;
    
    //We're going to get user's configuration...
    loadConfiguration();
    
    loggedKeyboards = new OSArray();
    loggedKeyboards->initWithCapacity(1);
    
    DEBUG_LOG( "%s: gIOTerminatedNotification...\n", getName() );
    notifyTerm = addMatchingNotification(gIOTerminatedNotification,
                                         serviceMatching("IOHIKeyboard"),
                                         (IOServiceMatchingNotificationHandler) &ClevoService::kbdTerminatedNotificationHandler,
                                         this, 0);
    
    DEBUG_LOG( "%s: gIOPublishNotification...\n", getName() );
    notify = addMatchingNotification(gIOPublishNotification,
                                     serviceMatching("IOHIKeyboard"),
                                     (IOServiceMatchingNotificationHandler) &ClevoService::kbdNotificationHandler,
                                     this, 0);
    
    setProperty(KBD_VAR_NAME, gKBLT, 8 * sizeof(UInt32));    
    setProperty("KbdAutoDimActive", gJustDim);
    setProperty("KbdAutoDimTimerActive", gDimTimerActive);
    setProperty("KbdAutoDimTime", gKBDTime, 8 * sizeof(UInt32));
    setProperty("KbdDimmingLevel", gKBDDimmingLevel, 8 * sizeof(UInt8));
    
    OSArray * levelsArray = new OSArray();
    levelsArray->initWithCapacity(1);
    if (levelsArray != NULL) {
        for (UInt8 i = 0; i < 4; i++) {
            OSNumber * myNum = OSNumber::withNumber(gKBDLevel[i], 8);
            if (myNum) levelsArray->setObject(i, myNum);
         }
        setProperty("KbdLevels", levelsArray);
        levelsArray->release();
    }
    
    registerService(0);
    
    IOLog( "%s: AutoDim %s\n", getName(), (gDimTimerActive ? "activated" : "not activated") );
    IOLog( "%s: AutoDim %s\n", getName(), (gJustDim ? "dimming activated" : "OFF activated") );
    IOLog( "%s: Backlighting activated: KBLT=0x%x", getName(), gKBLT);
    
    return true;
}

void ClevoService::stop(IOService *provider)
{
    DEBUG_LOG( "%s: Stopping...\n", getName() );
    saveNVRAM();
    
    if (gNVRAMTimer != NULL)
    {
        gNVRAMTimer->cancelTimeout();
        gWorkLoop->disableAllEventSources();
        gNVRAMTimer->release();
        gNVRAMTimer = NULL;
        DEBUG_LOG( "%s: gNVRAMTimer Removed\n", getName() );
    }
  
    if (gKBDTimer != NULL)
    {
        gKBDTimer->cancelTimeout();
        gWorkLoop->disableAllEventSources();
        gKBDTimer->release();
        gKBDTimer = NULL;
        DEBUG_LOG( "%s: gKBDTimer Removed\n", getName() );
    }

    if (gWorkLoop != NULL)
    {
        gWorkLoop->release();
        gWorkLoop = NULL;
        DEBUG_LOG( "%s: WorkLoop Removed\n", getName() );
    }
    
    if (notifyTerm) notifyTerm->remove();
    notifyTerm = NULL;
    DEBUG_LOG( "%s: TerminatedNotification Removed TerminatedNotification\n", getName() );
    
    if (notify) notify->remove();
    notify = NULL;
    DEBUG_LOG( "%s: PublishNotification Removed\n", getName() );
    
    gKBDService = NULL;
    
    kbdClearKeyboards();
    loggedKeyboards->release();
    DEBUG_LOG( "%s: Keyboards Removed...\n", getName() );
   
    IOLog( "%s: Stop Complete\n", getName() );
    
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
                if (!gKBDIsOFF) {
                    gKBLT &= 0xF0F;
                    gKBDIsOFF = true;
                } else {
                    gKBLT |= KBD_ON_MASK;
                    gKBDIsOFF = false;
                }
            case msgKbdWakeUp:
                kbdSetUpBacklight(0);
                break;
        }
        if (gNVRAMTimer != NULL) {
            gNVRAMTimer->cancelTimeout();
            gNVRAMTimer->setTimeoutMS(NVRAM_WAIT_TIME);
        }
    }
    return super::message(type, provider, argument);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

void ClevoService::ACPI_Send(uint32_t arg0, uint32_t arg1, uint32_t arg2)
{
    OSObject *params[3];
    params[0] = OSNumber::withNumber(arg0, 8 * sizeof(uint32_t));
    params[1] = OSNumber::withNumber(arg1, 8 * sizeof(uint32_t));
    params[2] = OSNumber::withNumber(arg2, 8 * sizeof(uint32_t));
    device->evaluateObject("CLVE", NULL, params, 3); // Call our method...
}


void ClevoService::saveNVRAM(void)
{
    bool    genericNVRAM;
    char    name[5];
    
    gKBLT &= 0xF13;
    if (gOldKBLT != gKBLT) {
        if (IORegistryEntry *nvram = OSDynamicCast(IORegistryEntry, fromPath("/options", gIODTPlane))) {
            strlcpy(name, KBD_VAR_NAME, sizeof(name));
            const OSSymbol *tempName = OSSymbol::withCString(name);
            
            if ((genericNVRAM = (0 == strncmp(nvram->getName(), "AppleNVRAM", sizeof("AppleNVRAM")))))
                DEBUG_LOG("%s: fallback to generic NVRAM methods\n", getName());
            
            if (genericNVRAM) {
                nvram->IORegistryEntry::setProperty(tempName, OSData::withBytes(&gKBLT, sizeof(UInt32)));
            } else {
                nvram->setProperty(tempName, OSData::withBytes(&gKBLT, sizeof(UInt32)));
            }
            
            OSSafeReleaseNULL(tempName);
            OSSafeReleaseNULL(nvram);
            
            gOldKBLT = gKBLT;
            
            this->setProperty(KBD_VAR_NAME, gKBLT, 8 * sizeof(UInt32));
            IOLog("%s: NVRAM saved KBLT=0x%x", getName(), gKBLT);
        }
    }
}


//TODO: ---------- Keyboard Backlight Routines ----------

void ClevoService::kbdSetUpBacklight(UInt32 newKBLT)
{
    if (newKBLT) gKBLT = newKBLT;   //We're forcing a new set
    if (gKBLT & KBD_ON_MASK) {      //...no need of level or colour if we're off
        kbdLightLevel(gKBLT & 3);   //Set backlight level...
        kbdSetColor();              //Set backlight color...
    }
    kbdONOFF();                     //Turn backlight on/off...
}


void ClevoService::kbdONOFF()
{
    if (gKBLT & KBD_ON_MASK) {
        ACPI_Send(0, SET_KB_LED, 0xE007F001); // ON
        if (gKBDTimer != NULL) gKBDTimer->setTimeoutMS(gKBDTime);
    } else {
        //ACPI_Send(0, SET_KB_LED, 0xF4000008); // Reducing light...
        //IOSleep(150);
        if (gKBDTimer != NULL) gKBDTimer->cancelTimeout();
        ACPI_Send(0, SET_KB_LED, 0xE0003001); // OFF
    }
    IOSleep(50);
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
    IOSleep(50);
}

void ClevoService::kbdLightLevel( UInt8 newLevel )
{
    newLevel &= 3;
    ACPI_Send(0, SET_KB_LED, 0xF4000000 | gKBDLevel[newLevel]);
    gKBLT &= 0xFF0;
    gKBLT |= newLevel;
    IOSleep(50);
}


bool ClevoService::kbdTerminatedNotificationHandler(void * target, void * refCon,
                                           IOService * newService,
                                           IONotifier * notifier)
{
    ClevoService * self = OSDynamicCast( ClevoService, (OSMetaClassBase*)target );
    if (!self) {
        DEBUG_LOG( "ClevoService: Terminated Notification handler failed...\n");
        return false;
    }
    
    if (!self->loggedKeyboards) {
        DEBUG_LOG( "%s: No Keyboard logged...\n", self->getName() );
        return false;
    }
    
    IOHIKeyboard * keyboard = OSDynamicCast( IOHIKeyboard, newService );
    if (!keyboard) {
         DEBUG_LOG( "%s: Keyboard not found...\n", self->getName() );
        return false;
    }
    
    int index = self->loggedKeyboards->getNextIndexOfObject(keyboard, 0);
    if (index >= 0)
    {
        self->gKBDInUse--;
        self->loggedKeyboards->removeObject(index);
        DEBUG_LOG( "%s: Keyboard Succesfully Removed %lu\n", self->getName(), (long) keyboard );
    }
    return true;
}


bool ClevoService::kbdNotificationHandler(void * target, void * refCon,
                                         IOService * newService,
                                         IONotifier * notifier)
{
    ClevoService * self = OSDynamicCast( ClevoService, (OSMetaClassBase *) target );
    if (!self) {
        DEBUG_LOG( "ClevoService: KbdNotification handler failed...\n");
        return false;
    }
    
    IOHIKeyboard * keyboard = OSDynamicCast( IOHIKeyboard, newService );
    if (!keyboard) {
        DEBUG_LOG( "%s: Keyboard not found...\n", self->getName() );
        return false;
    }
    
    if (!keyboard->_keyboardEventTarget)
    {
        DEBUG_LOG( "%s: No Keyboard event target\n", self->getName());
        return false;
    }
    
    IOService*    targetServ = OSDynamicCast( IOService, keyboard->_keyboardEventTarget );
    if (targetServ)
    {
        DEBUG_LOG( "%s: Keyboard event target is %s\n", self->getName(), targetServ->getName());
    }
    
    
    if (!keyboard->_keyboardEventTarget->metaCast("IOHIDSystem")) {
        DEBUG_LOG( "%s: IOHIDSystem failed. Continue anyway...\n", self->getName());
        //return false;
    }
    
    int index = self->loggedKeyboards->getNextIndexOfObject(keyboard,0);
    if (index < 0)
    {
        self->loggedKeyboards->setObject(keyboard);
        self->gKBDInUse++;
        DEBUG_LOG( "%s::Adding keyboard %lx\n", self->getName(), (long) keyboard );
    }
    
    origCallback = (KeyboardEventCallback)keyboard->_keyboardEventAction;
    keyboard->_keyboardEventAction = (KeyboardEventAction) logAction;
    
    origSpecialCallback = (KeyboardSpecialEventCallback)keyboard->_keyboardSpecialEventAction;
    keyboard->_keyboardSpecialEventAction = (KeyboardSpecialEventAction) specialAction;
    
    DEBUG_LOG( "%s: Keyboard notification complete\n", self->getName());
    return true;
}


void ClevoService::kbdClearKeyboards()
{
    DEBUG_LOG( "%s: Clear Keyboards in use %d!\n", getName(), gKBDInUse );
    
    if (loggedKeyboards)
    {
        int arraySize = loggedKeyboards->getCount();
        for (int i = 0; i < arraySize; i++)
        {
            IOHIKeyboard *curKeyboard = (IOHIKeyboard*)loggedKeyboards->getObject(0);
            
            if (origSpecialCallback)
                curKeyboard->_keyboardSpecialEventAction = (KeyboardSpecialEventAction)origSpecialCallback;
            if (origCallback)
                curKeyboard->_keyboardEventAction = (KeyboardEventAction)origCallback;
            
            loggedKeyboards->removeObject(0);
        }
    }
    origSpecialCallback = NULL;
    origCallback = NULL;
    gKBDInUse = 0;
}


void ClevoService::kbdTimerFired(void)
{
    if (gJustDim) {
        gKBDimmed = true;
        ACPI_Send(0, SET_KB_LED, 0xF4000000 | gKBDDimmingLevel); // Reducing backlight at minimum...
        DEBUG_LOG( "%s: gKBDTimer Fired -> Keyboard Dimmed\n", getName() );
    } else {
        gKBDimmed = false;
        gKBLT &= 0xFFEF;  //OFF
        kbdONOFF();
        DEBUG_LOG( "%s: gKBDTimer Fired -> Keyboard Backlight OFF\n", getName() );
    }
}

//TODO: ---------- User Configuration Routines -------------

OSObject* ClevoService::translateEntry(OSObject* obj)
{
    if (OSArray* array = OSDynamicCast(OSArray, obj))
        return translateArray(array);
    
    if (OSString* string = OSDynamicCast(OSString, obj))
    {
        const char* sz = string->getCStringNoCopy();
        if (sz[0] == '>')
        {
            if (sz[1] == 'y' && !sz[2])
                return OSBoolean::withBoolean(true);
            else if (sz[1] == 'n' && !sz[2])
                return OSBoolean::withBoolean(false);
            // escape case ('>>n' '>>y'), replace with just string '>n' '>y'
            else if (sz[1] == '>' && (sz[2] == 'y' || sz[2] == 'n') && !sz[3])
                return OSString::withCString(&sz[1]);
        }
    }
    return NULL;
}


OSObject* ClevoService::translateArray(OSArray* array)
{
    int count = array->getCount();
    if (!count) return NULL;
    
    OSObject* result = array;
    
    OSArray* test = OSDynamicCast(OSArray, array->getObject(0));
    if (test && test->getCount() == 0)
    {
        array->retain();
        array->removeObject(0);
        --count;
        
        for (int i = 0; i < count; ++i)
        {
            if (OSObject* obj = translateEntry(array->getObject(i)))
            {
                array->replaceObject(i, obj);
                obj->release();
            }
        }
    }
    else
    {
        if (count & 1) return NULL; // array is key/value pairs, so must be even
        
        int size = count >> 1;
        if (!size) size = 1;
        OSDictionary* dict = OSDictionary::withCapacity(size);
        if (!dict) return NULL;
        
        // go through each entry two at a time, building the dictionary
        for (int i = 0; i < count; i += 2)
        {
            OSString* key = OSDynamicCast(OSString, array->getObject(i));
            if (!key)
            {
                dict->release();
                return NULL;
            }
            // get value, use translated value if translated
            OSObject* obj = array->getObject(i+1);
            OSObject* trans = translateEntry(obj);
            if (trans)
                obj = trans;
            dict->setObject(key, obj);
            OSSafeReleaseNULL(trans);
        }
        result = dict;
    }
    
    // Note: result is retained when returned...
    return result;
}


void ClevoService::loadConfiguration()
{
    OSObject * reply = NULL;
    if (kIOReturnSuccess != (device->evaluateObject(ACPI_CONF_NAME, &reply))) {
        DEBUG_LOG( "%s: SSDT Configuration Method not found...\n", getName() );
        return;
    }
    
    OSObject * obj = NULL;
    OSArray * array = OSDynamicCast(OSArray, reply);
    if (array) obj = translateArray(array);
    OSSafeReleaseNULL(reply);
    
    OSDictionary * config = OSDynamicCast(OSDictionary, obj);
    if (!config) {
        DEBUG_LOG( "%s: unable to load Configuration data...\n", getName() );
        OSSafeReleaseNULL(obj);
        return;
    }

    OSBoolean * osBool = OSDynamicCast(OSBoolean, config->getObject("KbdAutoDimTimerActive"));
    if (osBool) gDimTimerActive = osBool->isTrue() ? true : false;
    
    osBool = OSDynamicCast(OSBoolean, config->getObject("KbdAutoDimActive"));
    if (osBool) gJustDim = osBool->isTrue() ? true : false;
    
    OSNumber * osNum = OSDynamicCast(OSNumber, config->getObject("KbdAutoDimTime"));
    if (osNum) {
        gKBDTime = osNum->unsigned32BitValue();
        if (gKBDTime < 5) gKBDTime = 5;             // Minimum 5 secs...
        else if (gKBDTime > 1800) gKBDTime = 1800;  // Maximum 30 minutes...
        gKBDTime *= 1000;                           // Value in SSDT is in seconds, we need mS...
    }
    
    if (OSArray * levelsArray = OSDynamicCast(OSArray, config->getObject("KbdLevels")))
    {
        if (levelsArray->getCount() == 4) { //We need 4 items...
            for (UInt8 i = 0; i < 4; i++) {
                osNum = OSDynamicCast(OSNumber, levelsArray->getObject(i));
                if (osNum && (osNum->unsigned8BitValue() > 0)) gKBDLevel[i] = osNum->unsigned8BitValue() & 0xFF;
            }
         }
    } else
        DEBUG_LOG( "%s: KbdLevels array not found\n", getName() );

    osNum = OSDynamicCast(OSNumber, config->getObject("KbdDimmingLevel"));
    if (osNum) gKBDDimmingLevel = osNum->unsigned8BitValue();
    
    OSSafeReleaseNULL(config);
    DEBUG_LOG( "%s: Configuration Succesfully Loaded\n", getName() );
 }

//TODO: ---------- Keyboard Logger Actions -------------

void specialAction(OSObject * target,
                   /* eventType */        unsigned   eventType,
                   /* flags */            unsigned   flags,
                   /* keyCode */          unsigned   key,
                   /* specialty */        unsigned   flavor,
                   /* source id */        UInt64     guid,
                   /* repeat */           bool       repeat,
                   /* atTime */           AbsoluteTime ts,
                   OSObject * sender,
                   void * refcon __unused)
{
    // only sign of a logout (also thrown when sleeping)
    if ((eventType == NX_SYSDEFINED) && (!flags) && (key == NX_NOSPECIALKEY))
        gKBDService->kbdClearKeyboards();
    
    if (origSpecialCallback)
        (* origSpecialCallback)(target, eventType, flags, key, flavor, guid, repeat, ts, sender, 0);
}


void logAction(OSObject * target,
               /* eventFlags  */      unsigned   eventType,
               /* flags */            unsigned   flags,
               /* keyCode */          unsigned   key,
               /* charCode */         unsigned   charCode,
               /* charSet */          unsigned   charSet,
               /* originalCharCode */ unsigned   origCharCode,
               /* originalCharSet */  unsigned   origCharSet,
               /* keyboardType */     unsigned   keyboardType,
               /* repeat */           bool       repeat,
               /* atTime */           AbsoluteTime ts,
               OSObject * sender,
               void * refcon __unused)
{
    if (gDimTimerActive && !gKBDIsOFF && (eventType == NX_KEYDOWN) && gKBDService) {
        if (gKBDTimer != NULL) {
            gKBDTimer->cancelTimeout();
            gKBDTimer->setTimeoutMS(gKBDTime);
            DEBUG_LOG( "%s: Key Down, Timer Reloaded %d mS\n", gKBDService->getName(), gKBDTime );
            
            if (gJustDim) {
                if (gKBDimmed) gKBDService->kbdLightLevel(gKBLT);
                gKBDimmed = false;
            } else if (!(gKBLT & KBD_ON_MASK)) {
                gKBLT |= KBD_ON_MASK;  //ON
                gKBDService->kbdSetUpBacklight( 0 );
            }
        }
    };
    
    if (origCallback)
        (* origCallback) (target, eventType, flags, key, charCode, charSet, origCharCode, origCharSet, keyboardType, repeat, ts, sender, 0);
}

