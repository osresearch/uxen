#ifndef _UXENPLATFORM_H_
#define _UXENPLATFORM_H_

#include <IOKit/IOService.h>

#include "uxenplatform_public.h"
#include "balloon.h"

#if DEBUG
#define dprintk(fmt, ...) IOLog("uxenplatform: " fmt, ## __VA_ARGS__)
#else
#define dprintk(fmt, ...) do {} while (0)
#endif

#define kUxenPlatformClassName "uXenPlatform"
class IOPCIDevice;
class IOFilterInterruptEventSource;
class IOInterruptEventSource;

class uXenPlatform : public IOService
{
    OSDeclareDefaultStructors(uXenPlatform);

public:
    /* IOService */
    virtual bool init(OSDictionary *dict = NULL) override;
    virtual void free(void) override;
    virtual IOService *probe(IOService *provider, SInt32 *score) override;

    virtual bool start(IOService *provider) override;
    virtual void stop(IOService *provider) override;

    /* client methods */
    IOReturn get_info(struct uXenPlatformInfo *arg);
    IOReturn get_balloon_stats(struct uXenPlatformBalloonStats *arg);
    IOReturn set_balloon_target(struct uXenPlatformBalloonTarget *arg);
    IODeviceMemory* getPlatformStateBAR(){return bar1;}
private:
    bool filterInterrupt(IOFilterInterruptEventSource *src);
    void handleInterrupt(IOInterruptEventSource *src, int count);

    static bool filter_interrupt(OSObject *owner,
                                 IOFilterInterruptEventSource *src)
    {
        uXenPlatform *platform = (uXenPlatform *)owner;
        return platform->filterInterrupt(src);
    }
    static void handle_interrupt(OSObject *owner,
                                 IOInterruptEventSource *src,
                                 int count)
    {
        uXenPlatform *platform = (uXenPlatform *)owner;
        platform->handleInterrupt(src, count);
    }

    IOPCIDevice *pcidev;
    IODeviceMemory *bar0;
    IODeviceMemory *bar1;
    IODeviceMemory *bar2;

    IOFilterInterruptEventSource *evtsrc;

    uXenBalloon balloon;
    OSArray *nubs;

    void enable_interrupt(int events);
    uint32_t pending_events;

    void enumerate_devices(void);
    void stop_devices(void);
};

#endif /* _UXENPLATFORM_H_ */
