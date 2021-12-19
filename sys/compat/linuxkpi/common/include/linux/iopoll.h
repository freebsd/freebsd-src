/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020-2021 Bjoern A. Zeeb
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

#ifndef	_LINUXKPI_LINUX_IOPOLL_H
#define	_LINUXKPI_LINUX_IOPOLL_H

#include <sys/types.h>
#include <sys/time.h>
#include <linux/delay.h>

#define	read_poll_timeout(_pollfp, _var, _cond, _us, _to, _early_sleep, ...)	\
({										\
	struct timeval __now, __end;						\
	if (_to) {								\
		__end.tv_sec = (_to) / USEC_PER_SEC;				\
		__end.tv_usec = (_to) % USEC_PER_SEC;				\
		microtime(&__now);						\
		timevaladd(&__end, &__now);					\
	}									\
										\
	if ((_early_sleep) && (_us) > 0)					\
		usleep_range(_us, _us);						\
	do {									\
		(_var) = _pollfp(__VA_ARGS__);					\
		if (_cond)							\
			break;							\
		if (_to) {							\
			microtime(&__now);					\
			if (timevalcmp(&__now, &__end, >))			\
				break;						\
		}								\
		if ((_us) != 0)							\
			usleep_range(_us, _us);					\
	} while (1);								\
	(_cond) ? 0 : (-ETIMEDOUT);						\
})

#define readx_poll_timeout(_pollfp, _addr, _var, _cond, _us, _to)		\
	read_poll_timeout(_pollfp, _var, _cond, _us, _to, false, _addr)

#define	read_poll_timeout_atomic(_pollfp, _var, _cond, _us, _to, _early_sleep, ...)	\
({										\
	struct timeval __now, __end;						\
	if (_to) {								\
		__end.tv_sec = (_to) / USEC_PER_SEC;				\
		__end.tv_usec = (_to) % USEC_PER_SEC;				\
		microtime(&__now);						\
		timevaladd(&__end, &__now);					\
	}									\
										\
	if ((_early_sleep) && (_us) > 0)					\
		DELAY(_us);							\
	do {									\
		(_var) = _pollfp(__VA_ARGS__);					\
		if (_cond)							\
			break;							\
		if (_to) {							\
			microtime(&__now);					\
			if (timevalcmp(&__now, &__end, >))			\
				break;						\
		}								\
		if ((_us) != 0)							\
			DELAY(_us);						\
	} while (1);								\
	(_cond) ? 0 : (-ETIMEDOUT);						\
})

#endif	/* _LINUXKPI_LINUX_IOPOLL_H */
