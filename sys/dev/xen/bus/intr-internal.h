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

#endif	/* _XEN_INTR_INTERNAL_H_ */
