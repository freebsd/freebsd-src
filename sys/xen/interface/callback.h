/******************************************************************************
 * callback.h
 *
 * Register guest OS callbacks with Xen.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Copyright (c) 2006, Ian Campbell
 */

#ifndef __XEN_PUBLIC_CALLBACK_H__
#define __XEN_PUBLIC_CALLBACK_H__

#include "xen.h"

/*
 * Prototype for this hypercall is:
 *   long callback_op(int cmd, void *extra_args)
 * @cmd        == CALLBACKOP_??? (callback operation).
 * @extra_args == Operation-specific extra arguments (NULL if none).
 */

#define CALLBACKTYPE_event                 0
#define CALLBACKTYPE_failsafe              1
#define CALLBACKTYPE_syscall               2 /* x86_64 only */
/*
 * sysenter is only available on x86_32 with the
 * supervisor_mode_kernel option enabled.
 */
#define CALLBACKTYPE_sysenter              3
#define CALLBACKTYPE_nmi                   4

/*
 * Disable event deliver during callback? This flag is ignored for event and
 * NMI callbacks: event delivery is unconditionally disabled.
 */
#define _CALLBACKF_mask_events             0
#define CALLBACKF_mask_events              (1U << _CALLBACKF_mask_events)

/*
 * Register a callback.
 */
#define CALLBACKOP_register                0
struct callback_register {
    uint16_t type;
    uint16_t flags;
    xen_callback_t address;
};
typedef struct callback_register callback_register_t;
DEFINE_XEN_GUEST_HANDLE(callback_register_t);

/*
 * Unregister a callback.
 *
 * Not all callbacks can be unregistered. -EINVAL will be returned if
 * you attempt to unregister such a callback.
 */
#define CALLBACKOP_unregister              1
struct callback_unregister {
    uint16_t type;
    uint16_t _unused;
};
typedef struct callback_unregister callback_unregister_t;
DEFINE_XEN_GUEST_HANDLE(callback_unregister_t);

#endif /* __XEN_PUBLIC_CALLBACK_H__ */

/*
 * Local variables:
 * mode: C
 * c-set-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
