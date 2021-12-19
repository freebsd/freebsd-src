/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020-2021 The FreeBSD Foundation
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

#ifndef _LINUXKPI_LINUX_AVERAGE_H
#define _LINUXKPI_LINUX_AVERAGE_H

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <linux/log2.h>

/* EWMA stands for Exponentially Weighted Moving Average. */
/*
 * Z_t = d X_t + (1 - d) * Z_(t-1); 0 < d <= 1, t >= 1; Roberts (1959).
 * t  : observation number in time.
 * d  : weight for current observation.
 * Xt : observations over time.
 * Zt : EWMA value after observation t.
 *
 * wmba_*_read seems to return up-to [u]long values; have to deal with 32/64bit.
 * According to the ath5k.h change log this seems to be a fix-(_p)recision impl.
 * assert 2/4 bits for frac.
 * Also all (_d) values seem to be pow2 which simplifies maths (shift by
 * d = ilog2(_d) instead of doing division (d = 1/_d)).  Keep it this way until
 * we hit the CTASSERT.
 */

#define	DECLARE_EWMA(_name, _p, _d)						\
										\
	CTASSERT((sizeof(unsigned long) <= 4) ? (_p < 30) : (_p < 60));		\
	CTASSERT(_d > 0 && powerof2(_d));					\
										\
	struct ewma_ ## _name {							\
		unsigned long zt;						\
	};									\
										\
	static __inline void							\
	ewma_ ## _name ## _init(struct ewma_ ## _name *ewma)			\
	{									\
		/* No target (no historical data). */				\
		ewma->zt = 0;							\
	}									\
										\
	static __inline void							\
	ewma_ ## _name ## _add(struct ewma_ ## _name *ewma, unsigned long x)	\
	{									\
		unsigned long ztm1 = ewma->zt;	/* Z_(t-1). */			\
		int d = ilog2(_d);						\
										\
		if (ewma->zt == 0)						\
			ewma->zt = x << (_p);					\
		else								\
			ewma->zt = ((x << (_p)) >> d) +				\
			    (((ztm1 << d) - ztm1) >> d);			\
	}									\
										\
	static __inline unsigned long						\
	ewma_ ## _name ## _read(struct ewma_ ## _name *ewma)			\
	{									\
		return (ewma->zt >> (_p));					\
	}									\

#endif /* _LINUXKPI_LINUX_AVERAGE_H */
