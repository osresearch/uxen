/*
 * Copyright (c) 2012, 2013, Christian Limpach <Christian.Limpach@gmail.com>
 * 
 * Permission to use, copy, modify, and/or distribute this software
 * for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice appear
 * in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _UXEN_PLATFORM_H_
#define _UXEN_PLATFORM_H_

#include <dm/dev.h>

void uxen_platform_time_update(void);
int uxen_platform_set_balloon_size(int min_mb, int max_mb);
int uxen_platform_get_balloon_size(int *current, int *min, int *max);

typedef struct UXenPlatformDevice {
    DeviceState qdev;
    uint8_t *config;
} UXenPlatformDevice;

UXenPlatformDevice *uxenplatform_device_create(const char *name);
UXenPlatformDevice *uxenplatform_create_simple(const char *model);
UXenPlatformDevice *uxenplatform_nic_init(NICInfo *nd, const char *model);

typedef struct UXenPlatformDeviceInfo {
    DeviceInfo qdev;
    int (*init)(UXenPlatformDevice *dev);
    int (*exit)(UXenPlatformDevice *dev);
    int (*unplug)(UXenPlatformDevice *dev);
    uint8_t devtype;
} UXenPlatformDeviceInfo;

void uxenplatform_qdev_register(UXenPlatformDeviceInfo *info);

int uxenplatform_device_add_property(UXenPlatformDevice *dev,
                                     uint8_t property_type,
                                     void *property, size_t property_len);

int uxenplatform_device_get_instance_id(UXenPlatformDevice *dev);

#endif  /* _UXEN_PLATFORM_H_ */
