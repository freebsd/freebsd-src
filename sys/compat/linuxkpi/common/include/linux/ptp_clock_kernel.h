/*-
 * Copyright (c) 2023 The FreeBSD Foundation
 *
 * This software was developed by Björn Zeeb under sponsorship from
 * the FreeBSD Foundation.
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
 *
 * $FreeBSD$
 */

#ifndef	_LINUXKPI_LINUX_PTP_CLOCK_KERNEL_H
#define	_LINUXKPI_LINUX_PTP_CLOCK_KERNEL_H

#include <linux/types.h>
#include <linux/device.h>
#include <linux/kernel.h>	/* pr_debug */
#include <linux/ktime.h>	/* system_device_crosststamp */

/* This very likely belongs elsewhere. */
struct system_device_crosststamp {
	ktime_t	device;
	ktime_t	sys_realtime;
	ktime_t	sys_monotonic_raw;	/* name guessed based on comment */
};

struct ptp_clock_info {
	char		name[32];
	int		max_adj;
	void		*owner;			/* THIS_MODULE */
	int (*adjfine)(struct ptp_clock_info *, long);
	int (*adjtime)(struct ptp_clock_info *, s64);
	int (*getcrosststamp)(struct ptp_clock_info *, struct system_device_crosststamp *);
	int (*gettime64)(struct ptp_clock_info *, struct timespec *);
};

static inline struct ptp_clock *
ptp_clock_register(struct ptp_clock_info *ptpci, struct device *dev)
{

	pr_debug("%s: TODO\n", __func__);
	return (NULL);
}

static inline void
ptp_clock_unregister(struct ptp_clock *ptpc)
{
	pr_debug("%s: TODO\n", __func__);
}

static inline int
ptp_clock_index(struct ptp_clock *ptpc)
{
	pr_debug("%s: TODO\n", __func__);
	return (0);
}

#endif	/* _LINUXKPI_LINUX_PTP_CLOCK_KERNEL_H */
