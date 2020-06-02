/*
 * main.m
 *
 * ClevoServiceDaemon
 *
*/

#import <Cocoa/Cocoa.h>
#import <CoreWLAN/CoreWLAN.h>
#import <sys/ioctl.h>
#import <sys/socket.h>
#import <sys/kern_event.h>
#import "OSD.h"
#include <dlfcn.h>

#define ClevoVendorSTR      "com.fjhk"  //Server Vendor Code
#define ClevoEventCode      0x1962      //Server Event Code
enum {evtKeyboardBacklight = 1, evtAirplaneMode };  //Clevo Server Event Type

struct ClevoServiceMessage {
    int type;
    int param1;
    int param2;
};

typedef enum {
    BSGraphicBacklightMeter                         = 0xfffffff7,
    BSGraphicBacklightFailure                       = 0xfffffff6,
    BSGraphicBacklightFailureMessage                = 0xfffffff3,
    BSGraphicBacklightDoubleFailureMessage          = 0xfffffff2,
    BSGraphicKeyboardBacklightMeter                 = 0xfffffff1,
    BSGraphicKeyboardBacklightDisabledMeter         = 0xfffffff0,
    BSGraphicKeyboardBacklightNotConnected          = 0xffffffef,
    BSGraphicKeyboardBacklightDisabledNotConnected  = 0xffffffee,
    BSGraphicMacProOpen                             = 0xffffffe9,
    BSGraphicSpeakerMuted                           = 0xffffffe8,
    BSGraphicSpeaker                                = 0xffffffe7,
    BSGraphicRemoteBattery                          = 0xffffffe6,
    BSGraphicHotspot                                = 0xffffffe5,
    BSGraphicSleep                                  = 0xffffffe3,
    BSGraphicSpeakerDisabledMuted                   = 0xffffffe2,
    BSGraphicSpeakerDisabled                        = 0xffffffe1,
    BSGraphicSpeakerMeter                           = 0xffffffe0,
    BSGraphicNewRemoteBattery                       = 0xffffffcb,
} BSGraphic;

#pragma mark -------------- External Functions --------------

extern void *BSDoGraphicWithMessage(long arg0, BSGraphic arg1, int arg2, const char *arg3, int length);
extern void *BSDoGraphicWithMeterAndTimeout(long arg0, BSGraphic arg1, int arg2, float v, int timeout);

// requires IOBluetooth.framework
extern void IOBluetoothPreferenceSetControllerPowerState(int);
extern int IOBluetoothPreferenceGetControllerPowerState(void);

static void *(*_BSDoGraphicWithMeterAndTimeout)(CGDirectDisplayID arg0, BSGraphic arg1, int arg2, float v, int timeout) = NULL;

#pragma mark -------------- Functions --------------

bool _loadBezelServices() {
    void *handle = dlopen("/System/Library/PrivateFrameworks/BezelServices.framework/Versions/A/BezelServices", RTLD_GLOBAL);
    if (!handle) {
#if DEBUG
        NSLog(@"Error opening framework");
#endif
        return NO;
    } else {
        _BSDoGraphicWithMeterAndTimeout = dlsym(handle, "BSDoGraphicWithMeterAndTimeout");
        return _BSDoGraphicWithMeterAndTimeout != NULL;
    }
}

bool _loadOSDFramework() {
    return [[NSBundle bundleWithPath:@"/System/Library/PrivateFrameworks/OSD.framework"] load];
}

void showBezelServices(BSGraphic image, float filled) {
    CGDirectDisplayID currentDisplayId = [NSScreen.mainScreen.deviceDescription [@"NSScreenNumber"] unsignedIntValue];
    _BSDoGraphicWithMeterAndTimeout(currentDisplayId, image, 0x0, filled, 1);
}

void showOSD(OSDGraphic image, int filled, int total) {
    CGDirectDisplayID currentDisplayId = [NSScreen.mainScreen.deviceDescription [@"NSScreenNumber"] unsignedIntValue];
    [[NSClassFromString(@"OSDManager") sharedManager] showImage:image onDisplayID:currentDisplayId priority:OSDPriorityDefault msecUntilFade:1000 filledChiclets:filled totalChiclets:total locked:NO];
}

void showKBoardBLightStatus(int level, int max) {
    if (_BSDoGraphicWithMeterAndTimeout != NULL) {
        // El Capitan and probably older systems
        if (level)
            showBezelServices(BSGraphicKeyboardBacklightMeter, (float)level/max);
        else
            showBezelServices(BSGraphicKeyboardBacklightDisabledMeter, 0);
    } else {
        // Sierra+
        if (level)
            showOSD(OSDGraphicKeyboardBacklightMeter, level, max);
        else
            showOSD(OSDGraphicKeyboardBacklightDisabledMeter, level, max);
    }
}

void showWifiStatus(bool enabled) {
    if (_BSDoGraphicWithMeterAndTimeout != NULL) {
        if (!enabled) showBezelServices(BSGraphicHotspot, 0);
    } else {
        if (!enabled) showOSD(OSDGraphicNoWiFi, 0, 1);
    }
}

BOOL airplaneModeEnabled = NO, lastWifiState;
int lastBluetoothState;
void toggleAirplaneMode() {
    airplaneModeEnabled = !airplaneModeEnabled;

    CWInterface *currentInterface = [CWWiFiClient.sharedWiFiClient interface];
    NSError *err = nil;
    
    if (airplaneModeEnabled) {
        lastWifiState = currentInterface.powerOn;
        lastBluetoothState = IOBluetoothPreferenceGetControllerPowerState();
        [currentInterface setPower:NO error:&err];
        IOBluetoothPreferenceSetControllerPowerState(0);
        showWifiStatus( NO );
    } else {
        [currentInterface setPower:lastWifiState error:&err];
        IOBluetoothPreferenceSetControllerPowerState(lastBluetoothState);
        showWifiStatus( lastWifiState );
    }
}

#pragma mark -------------- Main --------------

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        //printf("ClevoServiceDaemon started...\n");

        if (!_loadBezelServices()) _loadOSDFramework();

        //system socket
        int systemSocket = -1;

        //create system socket to receive kernel event data
        systemSocket = socket(PF_SYSTEM, SOCK_RAW, SYSPROTO_EVENT);

        //struct for vendor code
        // ->set via call to ioctl/SIOCGKEVVENDOR
        struct kev_vendor_code vendorCode = {0};

        //set vendor name string
        strncpy(vendorCode.vendor_string, ClevoVendorSTR, KEV_VENDOR_CODE_MAX_STR_LEN);

        //get vendor name -> vendor code mapping
        // ->vendor id, saved in 'vendorCode' variable
        ioctl(systemSocket, SIOCGKEVVENDOR, &vendorCode);

        //struct for kernel request
        // ->set filtering options
        struct kev_request kevRequest = {0};

        //init filtering options
        // ->only interested in objective-see's events kevRequest.vendor_code = vendorCode.vendor_code;

        //...any class
        kevRequest.kev_class = KEV_ANY_CLASS;

        //...any subclass
        kevRequest.kev_subclass = KEV_ANY_SUBCLASS;

        //tell kernel what we want to filter on
        ioctl(systemSocket, SIOCSKEVFILT, &kevRequest);

        //bytes received from system socket
        ssize_t bytesReceived = -1;

        //message from kext
        // ->size is cumulation of header, struct, and max length of a proc path
        char kextMsg[KEV_MSG_HEADER_SIZE + sizeof(struct ClevoServiceMessage)] = {0};

        struct ClevoServiceMessage *message = NULL;

        while (YES) {
            //printf("listening...\n");

            bytesReceived = recv(systemSocket, kextMsg, sizeof(kextMsg), 0);
            if (bytesReceived != sizeof(kextMsg)) continue;

            //struct for broadcast data from the kext
            struct kern_event_msg *kernEventMsg = {0};

            //type cast
            // ->to access kev_event_msg header
            kernEventMsg = (struct kern_event_msg*)kextMsg;

            //only care about 'process began' events
            if (ClevoEventCode != kernEventMsg->event_code) continue;

            //printf("new message\n");

            //typecast custom data
            // ->begins right after header
            message = (struct ClevoServiceMessage*)&kernEventMsg->event_data[0];

#if DEBUG
            printf("type:%d x:%d y:%d\n", message->type, message->param1, message->param2);
#endif
            switch (message->type) {
                case evtKeyboardBacklight:
                    showKBoardBLightStatus(message->param1, message->param2);
                    break;
                case evtAirplaneMode:
                    toggleAirplaneMode();
                    break;
#if DEBUG
                default:
                    printf("unknown type %d, param1 %d\n", message->type, message->param1);
                    //Just for testing OSD images...
                    int a, b, c;
                    a = (message->param1 & 0xFF00) >> 8;
                    b = (message->param1 & 0xF0) >> 4;
                    c = message->param1 & 0xF;
                    showOSD(a, b, c);
#endif
            }
        }
    }

    return 0;
}
