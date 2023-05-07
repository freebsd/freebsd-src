/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright © 2015 Julien Grall
 * Copyright © 2021 Elliott Mitchell
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef	_MACHINE__XEN_ARCH_INTR_H_
#define	_MACHINE__XEN_ARCH_INTR_H_

#include <sys/intr.h>

typedef struct intr_irqsrc xen_arch_isrc_t;

#include <dev/xen/bus/intr-internal.h>

/****************************** ARCH wrappers ********************************/

static inline void
xen_arch_intr_init(void)
{

	/* Nothing to do */
}

extern struct xenisrc *_Nullable xen_arch_intr_alloc(void);
extern void     xen_arch_intr_release(struct xenisrc *_Nonnull isrc);

static inline u_int
xen_arch_intr_next_cpu(struct xenisrc *_Nonnull isrc)
{
	static u_int current = 0;

	return (current = intr_irq_next_cpu(current, &all_cpus));
}

u_long	xen_arch_intr_execute_handlers(struct xenisrc *_Nonnull isrc,
	    struct trapframe *_Nullable frame);
int	xen_arch_intr_add_handler(const char *_Nonnull name,
	    driver_filter_t filter, driver_intr_t handler, void *_Nullable arg,
	    enum intr_type flags, struct xenisrc *_Nonnull isrc,
	    void *_Nullable *_Nonnull cookiep);

static inline int
xen_arch_intr_describe(struct xenisrc *_Nonnull isrc, void *_Nonnull cookie,
    const char *_Nonnull descr)
{

	return (intr_describe(&isrc->xi_arch, cookie, descr));
}

static inline int
xen_arch_intr_remove_handler(struct xenisrc *_Nonnull isrc,
    void *_Nonnull cookie)
{
	int rc = intr_event_remove_handler(cookie);

	if (rc == 0)
		--isrc->xi_arch.isrc_handlers;
	return (rc);
}

static inline int
xen_arch_intr_event_bind(struct xenisrc *_Nonnull isrc, u_int cpu)
{

	MPASS(isrc->xi_arch.isrc_event != NULL);
	return (intr_event_bind(isrc->xi_arch.isrc_event, cpu));
}

#endif	/* _MACHINE__XEN_ARCH_INTR_H_ */
