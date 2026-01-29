/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright © 1998,2003,2004 Doug Rabson
 * All rights reserved.
 * Copyright © 2001 David E. O'Brien
 * Copyright © 2003 Marcel Moolenaar
 * Copyright © 2007 Paolo Pisati
 * Copyright © 2011 Marius Strobl
 * Copyright © 2022 Mitchell Horne
 * Copyright © 2025 Andrew Turner
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

#ifndef	_SYS__INTERRUPT_H_
#define	_SYS__INTERRUPT_H_
#ifdef	_KERNEL

/**
 * @brief Driver interrupt filter return values
 *
 * If a driver provides an interrupt filter routine it must return an
 * integer consisting of oring together zero or more of the following
 * flags:
 *
 *	FILTER_STRAY	- this device did not trigger the interrupt
 *	FILTER_HANDLED	- the interrupt has been fully handled and can be EOId
 *	FILTER_SCHEDULE_THREAD - the threaded interrupt handler should be
 *			  scheduled to execute
 *
 * If the driver does not provide a filter, then the interrupt code will
 * act is if the filter had returned FILTER_SCHEDULE_THREAD.  Note that it
 * is illegal to specify any other flag with FILTER_STRAY and that it is
 * illegal to not specify either of FILTER_HANDLED or FILTER_SCHEDULE_THREAD
 * if FILTER_STRAY is not specified.
 */
#define	FILTER_STRAY		0x01
#define	FILTER_HANDLED		0x02
#define	FILTER_SCHEDULE_THREAD	0x04

/**
 * @brief Driver interrupt service routines
 *
 * The filter routine is run in primary interrupt context and may not
 * block or use regular mutexes.  It may only use spin mutexes for
 * synchronization.  The filter may either completely handle the
 * interrupt or it may perform some of the work and defer more
 * expensive work to the regular interrupt handler.  If a filter
 * routine is not registered by the driver, then the regular interrupt
 * handler is always used to handle interrupts from this device.
 *
 * The regular interrupt handler executes in its own thread context
 * and may use regular mutexes.  However, it is prohibited from
 * sleeping on a sleep queue.
 */
typedef int driver_filter_t(void *);
typedef void driver_intr_t(void *);

/**
 * @brief Interrupt type bits.
 *
 * These flags may be passed by drivers to bus_setup_intr(9) when
 * registering a new interrupt handler. The field is overloaded to
 * specify both the interrupt's type and any special properties.
 *
 * The INTR_TYPE* bits will be passed to intr_priority(9) to determine
 * the scheduling priority of the handler's ithread. Historically, each
 * type was assigned a unique scheduling preference, but now only
 * INTR_TYPE_CLK receives a default priority higher than other
 * interrupts. See sys/priority.h.
 *
 * Buses may choose to modify or augment these flags as appropriate,
 * e.g. nexus may apply INTR_EXCL.
 */
enum intr_type {
	INTR_TYPE_TTY = 1,
	INTR_TYPE_BIO = 2,
	INTR_TYPE_NET = 4,
	INTR_TYPE_CAM = 8,
	INTR_TYPE_MISC = 16,
	INTR_TYPE_CLK = 32,
	INTR_TYPE_AV = 64,
	INTR_EXCL = 256,		/* exclusive interrupt */
	INTR_MPSAFE = 512,		/* this interrupt is SMP safe */
	INTR_ENTROPY = 1024,		/* this interrupt provides entropy */
	INTR_SLEEPABLE = 2048,		/* this interrupt handler can sleep */
	INTR_MD1 = 4096,		/* flag reserved for MD use */
	INTR_MD2 = 8192,		/* flag reserved for MD use */
	INTR_MD3 = 16384,		/* flag reserved for MD use */
	INTR_MD4 = 32768		/* flag reserved for MD use */
};

enum intr_trigger {
	INTR_TRIGGER_INVALID = -1,
	INTR_TRIGGER_CONFORM = 0,
	INTR_TRIGGER_EDGE = 1,
	INTR_TRIGGER_LEVEL = 2
};

enum intr_polarity {
	INTR_POLARITY_CONFORM = 0,
	INTR_POLARITY_HIGH = 1,
	INTR_POLARITY_LOW = 2
};

#endif	/* _KERNEL */
#endif	/* !_SYS__INTERRUPT_H_ */
