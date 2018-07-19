/*
 * Copyright (c) 2013, Christian Limpach <Christian.Limpach@gmail.com>
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

#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#include <stdint.h>

int platform_open(void);
int platform_set_time_update_event(HANDLE event);
int platform_service_balloon_update_event(HANDLE event);
struct shared_info;
struct shared_info *platform_map_shared_info(void);
int platform_update_system_time(void);
int platform_service_balloon(void);

#endif  /* _PLATFORM_H_ */
