/*-
 * SPDX-License-Identifier: MIT OR GPL-2.0-only
 *
 * Copyright © 2002-2005 K A Fraser
 * Copyright © 2005 Intel Corporation <xiaofeng.ling@intel.com>
 * Copyright © 2005-2006 Kip Macy
 * Copyright © 2013 Spectra Logic Corporation
 * Copyright © 2015 Julien Grall
 * Copyright © 2021,2022 Elliott Mitchell
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

#ifndef	_XEN_INTR_INTERNAL_H_
#define	_XEN_INTR_INTERNAL_H_

#ifndef	_MACHINE__XEN_ARCH_INTR_H_
#error	"do not #include intr-internal.h, #include machine/arch-intr.h instead"
#endif

/* Current implementation only supports 2L event channels. */
#define NR_EVENT_CHANNELS EVTCHN_2L_NR_CHANNELS

enum evtchn_type {
	EVTCHN_TYPE_UNBOUND,
	EVTCHN_TYPE_VIRQ,
	EVTCHN_TYPE_IPI,
	EVTCHN_TYPE_PORT,
	EVTCHN_TYPE_COUNT
};

struct xenisrc {
	xen_arch_isrc_t		xi_arch;	/* @TOP -> *xi_arch=*xenisrc */
	enum evtchn_type	xi_type;
	u_int			xi_cpu;		/* VCPU for delivery */
	evtchn_port_t		xi_port;
	u_int			xi_virq;
	void			*xi_cookie;
	bool			xi_close:1;	/* close on unbind? */
	bool			xi_masked:1;
	volatile u_int		xi_refcount;
};

/***************** Functions called by the architecture code *****************/

extern void	xen_intr_resume(void);
extern void	xen_intr_enable_source(struct xenisrc *isrc);
extern void	xen_intr_disable_source(struct xenisrc *isrc);
extern void	xen_intr_enable_intr(struct xenisrc *isrc);
extern void	xen_intr_disable_intr(struct xenisrc *isrc);
extern int	xen_intr_assign_cpu(struct xenisrc *isrc, u_int to_cpu);

/******************* Functions implemented by each architecture **************/

#if 0
/*
 * These are sample prototypes, the architecture should include its own in
 * <machine/xen/arch-intr.h>.  The architecture may implement these as inline.
 */
void	xen_arch_intr_init(void);
struct xenisrc *xen_arch_intr_alloc(void);
void	xen_arch_intr_release(struct xenisrc *isrc);
u_int	xen_arch_intr_next_cpu(struct xenisrc *isrc);
u_long	xen_arch_intr_execute_handlers(struct xenisrc *isrc,
	    struct trapframe *frame);
int	xen_arch_intr_add_handler(const char *name,
	    driver_filter_t filter, driver_intr_t handler, void *arg,
	    enum intr_type flags, struct xenisrc *isrc,
	    void **cookiep);
int	xen_arch_intr_describe(struct xenisrc *isrc, void *cookie,
	    const char *descr);
int	xen_arch_intr_remove_handler(struct xenisrc *isrc,
	    void *cookie);
int	xen_arch_intr_event_bind(struct xenisrc *isrc, u_int cpu);
#endif

#endif	/* _XEN_INTR_INTERNAL_H_ */
