/*-
 * Copyright (c) 2002, 2003 Sam Leffler, Errno Consulting, Atheros
 * Communications, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the following conditions are met:
 * 1. The materials contained herein are unmodified and are used
 *    unmodified.
 * 2. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following NO
 *    ''WARRANTY'' disclaimer below (''Disclaimer''), without
 *    modification.
 * 3. Redistributions in binary form must reproduce at minimum a
 *    disclaimer similar to the Disclaimer below and any redistribution
 *    must be conditioned upon including a substantially similar
 *    Disclaimer requirement for further binary redistribution.
 * 4. Neither the names of the above-listed copyright holders nor the
 *    names of any contributors may be used to endorse or promote
 *    product derived from this software without specific prior written
 *    permission.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ''AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT,
 * MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
 * FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES.
 *
 * $Id: ah_osdep.h,v 1.9 2003/07/26 14:55:11 sam Exp $
 */
#ifndef _ATH_AH_OSDEP_H_
#define _ATH_AH_OSDEP_H_
/*
 * Atheros Hardware Access Layer (HAL) OS Dependent Definitions.
 */
#include <sys/param.h>
#include <sys/systm.h>

#include <machine/bus.h>

typedef void* HAL_SOFTC;
typedef bus_space_tag_t HAL_BUS_TAG;
typedef bus_space_handle_t HAL_BUS_HANDLE;
typedef bus_addr_t HAL_BUS_ADDR;

#define	OS_DELAY(_n)	DELAY(_n)
#define	OS_INLINE	__inline
#define	OS_MEMZERO(_a, _size)		bzero((_a), (_size))
#define	OS_MEMCPY(_dst, _src, _size)	bcopy((_src), (_dst), (_size))
#define	OS_MACEQU(_a, _b) \
	(bcmp((_a), (_b), IEEE80211_ADDR_LEN) == 0)

struct ath_hal;
extern 	u_int32_t OS_GETUPTIME(struct ath_hal *);

#ifdef AH_DEBUG_ALQ
extern	void OS_REG_WRITE(struct ath_hal *, u_int32_t, u_int32_t);
extern	u_int32_t OS_REG_READ(struct ath_hal *, u_int32_t);
extern	void OS_MARK(struct ath_hal *, u_int id, u_int32_t value);
#else
#define	OS_REG_WRITE(_ah, _reg, _val)					   \
	bus_space_write_4((_ah)->ah_st, (_ah)->ah_sh, (_reg), (_val))
#define	OS_REG_READ(_ah, _reg)						   \
	((u_int32_t) bus_space_read_4((_ah)->ah_st, (_ah)->ah_sh, (_reg)))
#define	OS_MARK(_ah, _id, _v)
#endif

#endif /* _ATH_AH_OSDEP_H_ */
