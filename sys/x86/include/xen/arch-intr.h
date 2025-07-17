/*-
 * SPDX-License-Identifier: MIT OR GPL-2.0-only
 *
 * Copyright © 2015 Julien Grall
 * Copyright © 2021 Elliott Mitchell
 *
 * This file may be distributed separately from the Linux kernel, or
 * incorporated into other software packages, subject to the following license:
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this source file (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef	_MACHINE__XEN_ARCH_INTR_H_
#define	_MACHINE__XEN_ARCH_INTR_H_

#include <x86/intr_machdep.h>
#include <x86/apicvar.h>

typedef struct {
	struct intsrc		intsrc;		/* @TOP -> *xen_arch_isrc */
	u_int			vector;		/* Global isrc vector number */
} xen_arch_isrc_t;

#include <dev/xen/bus/intr-internal.h>

/******************************* ARCH wrappers *******************************/

extern void xen_arch_intr_init(void);

extern struct xenisrc *xen_arch_intr_alloc(void);
extern void     xen_arch_intr_release(struct xenisrc *isrc);

static inline u_int
xen_arch_intr_next_cpu(struct xenisrc *isrc)
{

	return (apic_cpuid(intr_next_cpu(0)));
}

static inline u_long
xen_arch_intr_execute_handlers(struct xenisrc *isrc, struct trapframe *frame)
{

	intr_execute_handlers(&isrc->xi_arch.intsrc, frame);
	return (0);
}

static inline int
xen_arch_intr_add_handler(const char *name, driver_filter_t filter,
    driver_intr_t handler, void *arg, enum intr_type flags,
    struct xenisrc *isrc, void **cookiep)
{

	return (intr_add_handler(&isrc->xi_arch.intsrc, name, filter, handler,
	    arg, flags, cookiep, 0));
}

static inline int
xen_arch_intr_describe(struct xenisrc *isrc, void *cookie, const char *descr)
{

	return (intr_describe(&isrc->xi_arch.intsrc, cookie, descr));
}

static inline int
xen_arch_intr_remove_handler(struct xenisrc *isrc, void *cookie)
{

	return (intr_remove_handler(cookie));
}

static inline int
xen_arch_intr_event_bind(struct xenisrc *isrc, u_int cpu)
{

	return (intr_event_bind(isrc->xi_arch.intsrc.is_event, cpu));
}

#endif	/* _MACHINE__XEN_ARCH_INTR_H_ */
