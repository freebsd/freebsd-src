/*-
 * Copyright (c) 2002-2006 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 *
 * $FreeBSD$
 */
#ifndef _ATH_AH_OSDEP_H_
#define _ATH_AH_OSDEP_H_
/*
 * Atheros Hardware Access Layer (HAL) OS Dependent Definitions.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/endian.h>

#include <machine/bus.h>

/*
 * Delay n microseconds.
 */
extern	void ath_hal_delay(int);
#define	OS_DELAY(_n)	ath_hal_delay(_n)

#define	OS_INLINE	__inline
#define	OS_MEMZERO(_a, _n)	ath_hal_memzero((_a), (_n))
extern void ath_hal_memzero(void *, size_t);
#define	OS_MEMCPY(_d, _s, _n)	ath_hal_memcpy(_d,_s,_n)
extern void *ath_hal_memcpy(void *, const void *, size_t);

#define	abs(_a)		__builtin_abs(_a)

struct ath_hal;
extern	u_int32_t ath_hal_getuptime(struct ath_hal *);
#define	OS_GETUPTIME(_ah)	ath_hal_getuptime(_ah)

/*
 * Register read/write operations are either handled through
 * platform-dependent routines (or when debugging is enabled
 * with AH_DEBUG); or they are inline expanded using the macros
 * defined below.  For public builds we inline expand only for
 * platforms where it is certain what the requirements are to
 * read/write registers--typically they are memory-mapped and
 * no explicit synchronization or memory invalidation operations
 * are required (e.g. i386).
 */
#if defined(AH_DEBUG) || defined(AH_REGOPS_FUNC) || defined(AH_DEBUG_ALQ)
#define	OS_REG_WRITE(_ah, _reg, _val)	ath_hal_reg_write(_ah, _reg, _val)
#define	OS_REG_READ(_ah, _reg)		ath_hal_reg_read(_ah, _reg)

extern	void ath_hal_reg_write(struct ath_hal *ah, u_int reg, u_int32_t val);
extern	u_int32_t ath_hal_reg_read(struct ath_hal *ah, u_int reg);
#else
/*
 * The hardware registers are native little-endian byte order.
 * Big-endian hosts are handled by enabling hardware byte-swap
 * of register reads and writes at reset.  But the PCI clock
 * domain registers are not byte swapped!  Thus, on big-endian
 * platforms we have to explicitly byte-swap those registers.
 * Most of this code is collapsed at compile time because the
 * register values are constants.
 */
#define	AH_LITTLE_ENDIAN	1234
#define	AH_BIG_ENDIAN		4321

#if _BYTE_ORDER == _BIG_ENDIAN
#define OS_REG_WRITE(_ah, _reg, _val) do {				\
	if ( (_reg) >= 0x4000 && (_reg) < 0x5000)			\
		bus_space_write_4((bus_space_tag_t)(_ah)->ah_st,	\
		    (bus_space_handle_t)(_ah)->ah_sh, (_reg), (_val));	\
	else								\
		bus_space_write_stream_4((bus_space_tag_t)(_ah)->ah_st,	\
		    (bus_space_handle_t)(_ah)->ah_sh, (_reg), (_val));	\
} while (0)
#define OS_REG_READ(_ah, _reg)						\
	(((_reg) >= 0x4000 && (_reg) < 0x5000) ?			\
		bus_space_read_4((bus_space_tag_t)(_ah)->ah_st,		\
		    (bus_space_handle_t)(_ah)->ah_sh, (_reg)) :		\
		bus_space_read_stream_4((bus_space_tag_t)(_ah)->ah_st,	\
		    (bus_space_handle_t)(_ah)->ah_sh, (_reg)))
#else /* _BYTE_ORDER == _LITTLE_ENDIAN */
#define	OS_REG_WRITE(_ah, _reg, _val)					\
	bus_space_write_4((bus_space_tag_t)(_ah)->ah_st,		\
	    (bus_space_handle_t)(_ah)->ah_sh, (_reg), (_val))
#define	OS_REG_READ(_ah, _reg)						\
	bus_space_read_4((bus_space_tag_t)(_ah)->ah_st,			\
	    (bus_space_handle_t)(_ah)->ah_sh, (_reg))
#endif /* _BYTE_ORDER */
#endif /* AH_DEBUG || AH_REGFUNC || AH_DEBUG_ALQ */

#ifdef AH_DEBUG_ALQ
extern	void OS_MARK(struct ath_hal *, u_int id, u_int32_t value);
#else
#define	OS_MARK(_ah, _id, _v)
#endif

#endif /* _ATH_AH_OSDEP_H_ */
