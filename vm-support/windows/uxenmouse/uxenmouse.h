/*
 * Copyright (c) 2012, Christian Limpach <Christian.Limpach@gmail.com>
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

#ifndef _UXENMOUSE_H_
#define _UXENMOUSE_H_

#include <ntddk.h>
#include <kbdmou.h>
#include <ntddmou.h>
#include <wdf.h>

typedef struct _DEVICE_EXTENSION {
    CONNECT_DATA upper_connect_data;
    struct mouse_shared_page {
	volatile unsigned int x;
	volatile unsigned int y;
    } *mouse_shared_page;
    unsigned int mouse_shared_mfn[1];
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_EXTENSION, FilterGetData)

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD uxenmouse_add;
EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL uxenmouse_ioctl;

#endif	/* _UXENMOUSE_H_ */
