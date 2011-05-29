/*-
 * Copyright (c) 2007-2011 Broadcom Corporation. All rights reserved.
 *
 *    Gary Zambrano <zambrano@broadcom.com>
 *    David Christensen <davidch@broadcom.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Broadcom Corporation nor the name of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written consent.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

 /*$FreeBSD$*/

#ifndef _BXE_INCLUDE_H
#define _BXE_INCLUDE_H

#include <sys/param.h>
#include <sys/types.h>
#include <sys/endian.h>

#include <machine/bus.h>

/*
 * Convert FreeBSD byte order to match generated code usage.
 */
#if BYTE_ORDER == BIG_ENDIAN
#define __BIG_ENDIAN 1
#undef  __LITTLE_ENDIAN
#else
#undef  __BIG_ENDIAN
#define __LITTLE_ENDIAN 1
#endif

#include "bxe_debug.h"
#include "bxe_reg.h"
#include "bxe_fw_defs.h"
#include "bxe_hsi.h"
#include "bxe_link.h"

/*
 * Convenience definitions used in multiple files.
 */
#define BXE_PRINTF(fmt, args...)					\
	do {								\
		device_printf(sc->dev, fmt, ##args);			\
	}while(0)


#ifdef BXE_DEBUG

#define REG_WR(sc, offset, val)			\
	bxe_reg_write32(sc, offset, val)
#define REG_WR8(sc, offset, val)		\
	bxe_reg_write8(sc, offset, val)
#define REG_WR16(sc, offset, val)		\
	bxe_reg_write16(sc, offset, val)
#define REG_WR32(sc, offset, val)		\
	bxe_reg_write32(sc, offset, val)

#define REG_RD(sc, offset)			\
	bxe_reg_read32(sc, offset)
#define REG_RD8(sc, offset)			\
	bxe_reg_read8(sc, offset)
#define REG_RD16(sc, offset)			\
	bxe_reg_read16(sc, offset)
#define REG_RD32(sc, offset)			\
	bxe_reg_read32(sc, offset)

#define REG_RD_IND(sc, offset)			\
	bxe_reg_rd_ind(sc, offset)
#define REG_WR_IND(sc, offset, val)		\
	bxe_reg_wr_ind(sc, offset, val)

#else

#define REG_WR(sc, offset, val)						\
	bus_space_write_4(sc->bxe_btag, sc->bxe_bhandle, offset, val)
#define REG_WR8(sc, offset, val)					\
	bus_space_write_1(sc->bxe_btag, sc->bxe_bhandle, offset, val)
#define REG_WR16(sc, offset, val)					\
	bus_space_write_2(sc->bxe_btag, sc->bxe_bhandle, offset, val)
#define REG_WR32(sc, offset, val)					\
	bus_space_write_4(sc->bxe_btag, sc->bxe_bhandle, offset, val)

#define REG_RD(sc, offset)						\
	bus_space_read_4(sc->bxe_btag, sc->bxe_bhandle, offset)
#define REG_RD8(sc, offset)						\
	bus_space_read_1(sc->bxe_btag, sc->bxe_bhandle, offset)
#define REG_RD16(sc, offset)						\
	bus_space_read_2(sc->bxe_btag, sc->bxe_bhandle, offset)
#define REG_RD32(sc, offset)						\
	bus_space_read_4(sc->bxe_btag, sc->bxe_bhandle, offset)

#define REG_RD_IND(sc, offset)						\
	bxe_reg_rd_ind(sc, offset)
#define REG_WR_IND(sc, offset, val)					\
	bxe_reg_wr_ind(sc, offset, val)

#endif /* BXE_DEBUG */


#define REG_RD_DMAE(sc, offset, val, len32)				\
	do {								\
		bxe_read_dmae(sc, offset, len32);			\
		memcpy(val, BXE_SP(sc, wb_data[0]), len32 * 4); 	\
	} while (0)


#define REG_WR_DMAE(sc, offset, val, len32)				\
	do { 								\
		memcpy(BXE_SP(sc, wb_data[0]), val, len32 * 4); 	\
		bxe_write_dmae(sc, BXE_SP_MAPPING(sc, wb_data), 	\
			offset, len32);					\
	} while (0)


#define SHMEM_ADDR(sc, field)	(sc->common.shmem_base + 		\
	offsetof(struct shmem_region, field))

#define SHMEM_RD(sc, field) 						\
	REG_RD(sc, SHMEM_ADDR(sc, field))
#define SHMEM_RD16(sc, field) 						\
	REG_RD16(sc, SHMEM_ADDR(sc, field))

#define SHMEM_WR(sc, field, val) 					\
	REG_WR(sc, SHMEM_ADDR(sc, field), val)

#define SHMEM2_ADDR(sc, field)		(sc->common.shmem2_base + 	\
	offsetof(struct shmem2_region, field))
#define SHMEM2_RD(sc, field)		REG_RD(sc, SHMEM2_ADDR(sc, field))
#define SHMEM2_WR(sc, field, val)	REG_WR(sc, SHMEM2_ADDR(sc, field), val)


#define EMAC_RD(sc, reg) 						\
	REG_RD(sc, emac_base + (uint32_t) reg)
#define EMAC_WR(sc, reg, val) 						\
	REG_WR(sc, emac_base + (uint32_t) reg, val)

#define BMAC_WR(sc, reg, val) 						\
	REG_WR(sc, GRCBASE_NIG + bmac_addr + reg, val)

#endif /* _BXE_INCLUDE_H */
