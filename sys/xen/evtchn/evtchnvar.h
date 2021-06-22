/******************************************************************************
 * evtchn.h
 * 
 * Data structures and definitions private to the FreeBSD implementation
 * of the Xen event channel API.
 * 
 * Copyright (c) 2004, K A Fraser
 * Copyright (c) 2012, Spectra Logic Corporation
 * Copyright © 2022, Elliott Mitchell
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

#ifndef __XEN_EVTCHN_EVTCHNVAR_H__
#define __XEN_EVTCHN_EVTCHNVAR_H__

#include <xen/hypervisor.h>
#include <contrib/xen/event_channel.h>

/* Macros for accessing event channel values */
#define	EVTCHN_PTR(type, port) \
	(HYPERVISOR_shared_info->evtchn_##type + ((port) / __LONG_BIT))
#define	EVTCHN_BIT(port)	((port) & (__LONG_BIT - 1))
#define	EVTCHN_MASK(port)	(1UL << EVTCHN_BIT(port))

/**
 * Disable signal delivery for an event channel port, returning its
 * previous mask state.
 *
 * \param port  The event channel port to query and mask.
 *
 * \returns  1 if event delivery was previously disabled.  Otherwise 0.
 */
static inline int
evtchn_test_and_set_mask(evtchn_port_t port)
{

	return (atomic_testandset_long(EVTCHN_PTR(mask, port),
	    EVTCHN_BIT(port)));
}

/**
 * Clear any pending event for the given event channel port.
 *
 * \param port  The event channel port to clear.
 */
static inline void 
evtchn_clear_port(evtchn_port_t port)
{

	atomic_clear_long(EVTCHN_PTR(pending, port), EVTCHN_MASK(port));
}

/**
 * Disable signal delivery for an event channel port.
 *
 * \param port  The event channel port to mask.
 */
static inline void
evtchn_mask_port(evtchn_port_t port)
{

	atomic_set_long(EVTCHN_PTR(mask, port), EVTCHN_MASK(port));
}

/**
 * Enable signal delivery for an event channel port.
 *
 * \param port  The event channel port to enable.
 */
static inline void
evtchn_unmask_port(evtchn_port_t port)
{
	evtchn_unmask_t op = { .port = port };

	HYPERVISOR_event_channel_op(EVTCHNOP_unmask, &op);
}

#endif /* __XEN_EVTCHN_EVTCHNVAR_H__ */
