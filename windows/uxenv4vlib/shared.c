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

#include "uxenv4vlib_private.h"

#pragma data_seg(".shared")

uxen_v4vlib_hypercall_func_t *hypercall_func;
uxen_v4vlib_page_notify_func_t *page_notify_func;

struct uxp_state_bar **state_bar_ptr;
xenv4v_extension_t *uxen_v4v_pde;
KSPIN_LOCK uxen_v4v_pde_lock;
int uxen_v4v_am_dom0;
KDPC *uxen_v4vlib_resume_dpcs[UXEN_V4VLIB_MAX_RESUME_DPCS];
void *uxen_v4vlib_resume_dpcs_arg1[UXEN_V4VLIB_MAX_RESUME_DPCS];
uxen_v4v_logger_t uxen_v4v_logger;

#pragma data_seg()

void uxen_v4v_init_shared(void)
{
    unsigned i;

    //KeInitializeSpinLock(&uxen_v4v_pde_lock);

    for (i = 0; i < UXEN_V4VLIB_MAX_RESUME_DPCS; ++i) {
        uxen_v4vlib_resume_dpcs[i] = NULL;
    }

    uxen_v4v_logger = NULL;
}



