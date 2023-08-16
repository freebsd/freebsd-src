/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2021, Intel Corporation
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of the Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file iavf_osdep.h
 * @brief OS compatibility layer definitions
 *
 * Contains macros and definitions used to implement an OS compatibility layer
 * used by some of the hardware files.
 */
#ifndef _IAVF_OSDEP_H_
#define _IAVF_OSDEP_H_

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/endian.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/clock.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include "iavf_status.h"
#include "iavf_debug.h"

#define iavf_usec_delay(x) DELAY(x)
#define iavf_msec_delay(x) DELAY(1000 * (x))

#define DBG 0
#define DEBUGFUNC(F)        DEBUGOUT(F);
#if DBG
	#define DEBUGOUT(S)         printf(S "\n")
	#define DEBUGOUT1(S,A)      printf(S "\n",A)
	#define DEBUGOUT2(S,A,B)    printf(S "\n",A,B)
	#define DEBUGOUT3(S,A,B,C)  printf(S "\n",A,B,C)
	#define DEBUGOUT7(S,A,B,C,D,E,F,G)  printf(S "\n",A,B,C,D,E,F,G)
#else
	#define DEBUGOUT(S)
	#define DEBUGOUT1(S,A)
	#define DEBUGOUT2(S,A,B)
	#define DEBUGOUT3(S,A,B,C)
	#define DEBUGOUT6(S,A,B,C,D,E,F)
	#define DEBUGOUT7(S,A,B,C,D,E,F,G)
#endif

#define UNREFERENCED_PARAMETER(_p) _p = _p
#define UNREFERENCED_1PARAMETER(_p) do {			\
	UNREFERENCED_PARAMETER(_p);				\
} while (0)
#define UNREFERENCED_2PARAMETER(_p, _q) do {			\
	UNREFERENCED_PARAMETER(_p);				\
	UNREFERENCED_PARAMETER(_q);				\
} while (0)
#define UNREFERENCED_3PARAMETER(_p, _q, _r) do {		\
	UNREFERENCED_PARAMETER(_p);				\
	UNREFERENCED_PARAMETER(_q);				\
	UNREFERENCED_PARAMETER(_r);				\
} while (0)
#define UNREFERENCED_4PARAMETER(_p, _q, _r, _s) do {		\
	UNREFERENCED_PARAMETER(_p);				\
	UNREFERENCED_PARAMETER(_q);				\
	UNREFERENCED_PARAMETER(_r);				\
	UNREFERENCED_PARAMETER(_s);				\
} while (0)
#define UNREFERENCED_5PARAMETER(_p, _q, _r, _s, _t) do {	\
	UNREFERENCED_PARAMETER(_p);				\
	UNREFERENCED_PARAMETER(_q);				\
	UNREFERENCED_PARAMETER(_r);				\
	UNREFERENCED_PARAMETER(_s);				\
	UNREFERENCED_PARAMETER(_t);				\
} while (0)

#define STATIC	static
#define INLINE  inline

#define iavf_memset(a, b, c, d)  memset((a), (b), (c))
#define iavf_memcpy(a, b, c, d)  memcpy((a), (b), (c))

#define CPU_TO_LE16(o)	htole16(o)
#define CPU_TO_LE32(s)	htole32(s)
#define CPU_TO_LE64(h)	htole64(h)
#define LE16_TO_CPU(a)	le16toh(a)
#define LE32_TO_CPU(c)	le32toh(c)
#define LE64_TO_CPU(k)	le64toh(k)

/**
 * @typedef u8
 * @brief compatibility typedef for uint8_t
 */
typedef uint8_t		u8;

/**
 * @typedef s8
 * @brief compatibility typedef for int8_t
 */
typedef int8_t		s8;

/**
 * @typedef u16
 * @brief compatibility typedef for uint16_t
 */
typedef uint16_t	u16;

/**
 * @typedef s16
 * @brief compatibility typedef for int16_t
 */
typedef int16_t		s16;

/**
 * @typedef u32
 * @brief compatibility typedef for uint32_t
 */
typedef uint32_t	u32;

/**
 * @typedef s32
 * @brief compatibility typedef for int32_t
 */
typedef int32_t		s32;

/**
 * @typedef u64
 * @brief compatibility typedef for uint64_t
 */
typedef uint64_t	u64;

#define __le16  u16
#define __le32  u32
#define __le64  u64
#define __be16  u16
#define __be32  u32
#define __be64  u64

/**
 * @struct iavf_spinlock
 * @brief OS wrapper for a non-sleeping lock
 *
 * Wrapper used to provide an implementation of a non-sleeping lock.
 */
struct iavf_spinlock {
        struct mtx mutex;
};

/**
 * @struct iavf_osdep
 * @brief Storage for data used by the osdep interface
 *
 * Contains data used by the osdep layer. Accessed via the hw->back pointer.
 */
struct iavf_osdep {
	bus_space_tag_t		mem_bus_space_tag;
	bus_space_handle_t	mem_bus_space_handle;
	bus_size_t		mem_bus_space_size;
	uint32_t		flush_reg;
	int			i2c_intfc_num;
	device_t		dev;
};

/**
 * @struct iavf_dma_mem
 * @brief DMA memory map
 *
 * Structure representing a DMA memory mapping.
 */
struct iavf_dma_mem {
	void			*va;
	u64			pa;
	bus_dma_tag_t		tag;
	bus_dmamap_t		map;
	bus_dma_segment_t	seg;
	bus_size_t              size;
	int			nseg;
	int                     flags;
};

/**
 * @struct iavf_virt_mem
 * @brief Virtual memory
 *
 * Structure representing some virtual memory.
 */
struct iavf_virt_mem {
	void *va;
	u32 size;
};

struct iavf_hw; /* forward decl */
u16	iavf_read_pci_cfg(struct iavf_hw *, u32);
void	iavf_write_pci_cfg(struct iavf_hw *, u32, u16);

/*
** iavf_debug - OS dependent version of shared code debug printing
*/
#define iavf_debug(h, m, s, ...)  iavf_debug_shared(h, m, s, ##__VA_ARGS__)
void iavf_debug_shared(struct iavf_hw *hw, uint64_t mask,
    char *fmt_str, ...) __printflike(3, 4);

/*
** This hardware supports either 16 or 32 byte rx descriptors;
** the driver only uses the 32 byte kind.
*/
#define iavf_rx_desc iavf_32byte_rx_desc

uint32_t iavf_rd32(struct iavf_hw *hw, uint32_t reg);
void iavf_wr32(struct iavf_hw *hw, uint32_t reg, uint32_t val);
void iavf_flush(struct iavf_hw *hw);
#define rd32(hw, reg)		iavf_rd32(hw, reg)
#define wr32(hw, reg, val)	iavf_wr32(hw, reg, val)

#endif /* _IAVF_OSDEP_H_ */
