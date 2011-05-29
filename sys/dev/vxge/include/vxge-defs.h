/*-
 * Copyright(c) 2002-2011 Exar Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification are permitted provided the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *    3. Neither the name of the Exar Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*$FreeBSD$*/

#ifndef	VXGE_DEFS_H
#define	VXGE_DEFS_H

#define	VXGE_PCI_VENDOR_ID			0x17D5
#define	VXGE_PCI_DEVICE_ID_TITAN_1		0x5833
#define	VXGE_PCI_REVISION_TITAN_1		1
#define	VXGE_PCI_DEVICE_ID_TITAN_1A		0x5833
#define	VXGE_PCI_REVISION_TITAN_1A		2
#define	VXGE_PCI_DEVICE_ID_TITAN_2		0x5834
#define	VXGE_PCI_REVISION_TITAN_2		1

#define	VXGE_MIN_FW_MAJOR_VERSION		1
#define	VXGE_MIN_FW_MINOR_VERSION		8
#define	VXGE_MIN_FW_BUILD_NUMBER		1

#define	VXGE_DRIVER_VENDOR			"Exar Corp."
#define	VXGE_CHIP_FAMILY			"X3100"
#define	VXGE_SUPPORTED_MEDIA_0			"Fiber"

#define	VXGE_DRIVER_NAME			\
	"Neterion X3100 10GbE PCIe Server Adapter Driver"
/*
 * mBIT(loc) - set bit at offset
 */
#define	mBIT(loc)		(0x8000000000000000ULL >> (loc))

/*
 * vBIT(val, loc, sz) - set bits at offset
 */
#define	vBIT(val, loc, sz)	(((u64)(val)) << (64-(loc)-(sz)))
#define	vBIT32(val, loc, sz)	(((u32)(val)) << (32-(loc)-(sz)))

/*
 * bVALx(bits, loc) - Get the value of x bits at location
 */
#define	bVAL1(bits, loc)  ((((u64)bits) >> (64-(loc+1))) & 0x1)
#define	bVAL2(bits, loc)  ((((u64)bits) >> (64-(loc+2))) & 0x3)
#define	bVAL3(bits, loc)  ((((u64)bits) >> (64-(loc+3))) & 0x7)
#define	bVAL4(bits, loc)  ((((u64)bits) >> (64-(loc+4))) & 0xF)
#define	bVAL5(bits, loc)  ((((u64)bits) >> (64-(loc+5))) & 0x1F)
#define	bVAL6(bits, loc)  ((((u64)bits) >> (64-(loc+6))) & 0x3F)
#define	bVAL7(bits, loc)  ((((u64)bits) >> (64-(loc+7))) & 0x7F)
#define	bVAL8(bits, loc)  ((((u64)bits) >> (64-(loc+8))) & 0xFF)
#define	bVAL9(bits, loc)  ((((u64)bits) >> (64-(loc+9))) & 0x1FF)
#define	bVAL11(bits, loc) ((((u64)bits) >> (64-(loc+11))) & 0x7FF)
#define	bVAL12(bits, loc) ((((u64)bits) >> (64-(loc+12))) & 0xFFF)
#define	bVAL14(bits, loc) ((((u64)bits) >> (64-(loc+14))) & 0x3FFF)
#define	bVAL15(bits, loc) ((((u64)bits) >> (64-(loc+15))) & 0x7FFF)
#define	bVAL16(bits, loc) ((((u64)bits) >> (64-(loc+16))) & 0xFFFF)
#define	bVAL17(bits, loc) ((((u64)bits) >> (64-(loc+17))) & 0x1FFFF)
#define	bVAL18(bits, loc) ((((u64)bits) >> (64-(loc+18))) & 0x3FFFF)
#define	bVAL20(bits, loc) ((((u64)bits) >> (64-(loc+20))) & 0xFFFFF)
#define	bVAL22(bits, loc) ((((u64)bits) >> (64-(loc+22))) & 0x3FFFFF)
#define	bVAL24(bits, loc) ((((u64)bits) >> (64-(loc+24))) & 0xFFFFFF)
#define	bVAL28(bits, loc) ((((u64)bits) >> (64-(loc+28))) & 0xFFFFFFF)
#define	bVAL32(bits, loc) ((((u64)bits) >> (64-(loc+32))) & 0xFFFFFFFF)
#define	bVAL36(bits, loc) ((((u64)bits) >> (64-(loc+36))) & 0xFFFFFFFFFULL)
#define	bVAL40(bits, loc) ((((u64)bits) >> (64-(loc+40))) & 0xFFFFFFFFFFULL)
#define	bVAL44(bits, loc) ((((u64)bits) >> (64-(loc+44))) & 0xFFFFFFFFFFFULL)
#define	bVAL48(bits, loc) ((((u64)bits) >> (64-(loc+48))) & 0xFFFFFFFFFFFFULL)
#define	bVAL52(bits, loc) ((((u64)bits) >> (64-(loc+52))) & 0xFFFFFFFFFFFFFULL)
#define	bVAL56(bits, loc) ((((u64)bits) >> (64-(loc+56))) & 0xFFFFFFFFFFFFFFULL)
#define	bVAL60(bits, loc)   \
		((((u64)bits) >> (64-(loc+60))) & 0xFFFFFFFFFFFFFFFULL)
#define	bVAL61(bits, loc)   \
		((((u64)bits) >> (64-(loc+61))) & 0x1FFFFFFFFFFFFFFFULL)

#define	VXGE_HAL_VPATH_BMAP_START	47
#define	VXGE_HAL_VPATH_BMAP_END		63

#define	VXGE_HAL_ALL_FOXES		0xFFFFFFFFFFFFFFFFULL

#define	VXGE_HAL_INTR_MASK_ALL		0xFFFFFFFFFFFFFFFFULL

#define	VXGE_HAL_MAX_VIRTUAL_PATHS	17

#define	VXGE_HAL_MAX_FUNCTIONS		8

#define	VXGE_HAL_MAX_ITABLE_ENTRIES	256

#define	VXGE_HAL_MAX_RSS_KEY_SIZE	40

#define	VXGE_HAL_MAC_MAX_WIRE_PORTS	2

#define	VXGE_HAL_MAC_SWITCH_PORT	2

#define	VXGE_HAL_MAC_MAX_AGGR_PORTS	2

#define	VXGE_HAL_MAC_MAX_PORTS		3

#define	VXGE_HAL_INTR_ALARM		(1<<4)

#define	VXGE_HAL_INTR_TX		(1<<(3-VXGE_HAL_VPATH_INTR_TX))

#define	VXGE_HAL_INTR_RX		(1<<(3-VXGE_HAL_VPATH_INTR_RX))

#define	VXGE_HAL_INTR_EINTA		(1<<(3-VXGE_HAL_VPATH_INTR_EINTA))

#define	VXGE_HAL_INTR_BMAP		(1<<(3-VXGE_HAL_VPATH_INTR_BMAP))

#define	VXGE_HAL_PCI_CONFIG_SPACE_SIZE	VXGE_OS_PCI_CONFIG_SIZE

#define	VXGE_HAL_DEFAULT_32		0xffffffff

#define	VXGE_HAL_DEFAULT_64		0xffffffffffffffff

#define	VXGE_HAL_DUMP_BUF_SIZE		0x10000

#define	VXGE_HAL_VPD_BUFFER_SIZE	128

#define	VXGE_HAL_VPD_LENGTH		80

/* Check whether an address is multicast. */
#define	VXGE_HAL_IS_NULL(Address)	(Address == 0x0000000000000000ULL)

/* Check whether an address is multicast. */
#define	VXGE_HAL_IS_MULTICAST(Address)	(Address & 0x0000010000000000ULL)

/* Check whether an address is broadcast. */
#define	VXGE_HAL_IS_BROADCAST(Address)	\
	((Address & 0x0000FFFF00000000ULL) == 0x0000FFFF00000000ULL)

#define	VXGE_HAL_IS_UNICAST(Address)		\
	(!(VXGE_HAL_IS_NULL(Address) ||		\
	VXGE_HAL_IS_MULTICAST(Address) ||	\
	VXGE_HAL_IS_BROADCAST(Address)))

/* frames sizes */
#define	VXGE_HAL_HEADER_ETHERNET_II_802_3_SIZE		14
#define	VXGE_HAL_HEADER_802_2_SIZE			3
#define	VXGE_HAL_HEADER_SNAP_SIZE			5
#define	VXGE_HAL_HEADER_VLAN_SIZE			4
#define	VXGE_HAL_MAC_HEADER_MAX_SIZE \
			(VXGE_HAL_HEADER_ETHERNET_II_802_3_SIZE + \
			VXGE_HAL_HEADER_802_2_SIZE + \
			VXGE_HAL_HEADER_SNAP_SIZE)

#define	VXGE_HAL_TCPIP_HEADER_MAX_SIZE			(64 + 64)

/* 32bit alignments */

/* A receive data corruption can occur resulting in either a single-bit or
double-bit ECC error being flagged in the ASIC if starting offset of a
buffer in single buffer mode is 0x2 to 0xa. Single bit ECC error will not
lock up the card but can hide the data corruption while the double-bit ECC
error will lock up the card. Limiting the starting offset of the buffers to
0x0, 0x1 or to a value greater than 0xF will workaround this issue.
VXGE_HAL_HEADER_ETHERNET_II_802_3_ALIGN of 2 causes the starting offset of
buffer to be 0x2, 0x12 and so on, to have the start of the ip header dword
aligned. The start of buffer of 0x2 will cause this problem to occur.
To avoid this problem in all cases, add 0x10 to 0x2, to ensure that the start
of buffer is outside of the problem causing offsets.
*/

#define	VXGE_HAL_HEADER_ETHERNET_II_802_3_ALIGN		0x12
#define	VXGE_HAL_HEADER_802_2_SNAP_ALIGN		2
#define	VXGE_HAL_HEADER_802_2_ALIGN			3
#define	VXGE_HAL_HEADER_SNAP_ALIGN			1

#define	VXGE_HAL_MIN_MTU				46
#define	VXGE_HAL_MAX_MTU				9600
#define	VXGE_HAL_DEFAULT_MTU				1500

#define	VXGE_HAL_SEGEMENT_OFFLOAD_MAX_SIZE		81920

#if defined(__EXTERN_BEGIN_DECLS)
#undef __EXTERN_BEGIN_DECLS
#endif

#if defined(__EXTERN_END_DECLS)
#undef __EXTERN_END_DECLS
#endif

#if defined(__cplusplus)
#define	__EXTERN_BEGIN_DECLS		extern "C" {
#define	__EXTERN_END_DECLS			}
#else
#define	__EXTERN_BEGIN_DECLS
#define	__EXTERN_END_DECLS
#endif

__EXTERN_BEGIN_DECLS

/* --------------------------- common stuffs ------------------------------ */
/*
 * VXGE_OS_DMA_REQUIRES_SYNC  - should be defined or
 * NOT defined in the Makefile
 */
#define	VXGE_OS_DMA_CACHELINE_ALIGNED		0x1

/*
 * Either STREAMING or CONSISTENT should be used.
 * The combination of both or none is invalid
 */
#define	VXGE_OS_DMA_STREAMING			0x2
#define	VXGE_OS_DMA_CONSISTENT			0x4
#define	VXGE_OS_SPRINTF_STRLEN			64

/* --------------------------- common stuffs ------------------------------ */
#ifndef	VXGE_OS_LLXFMT
#define	VXGE_OS_LLXFMT				"%llx"
#endif

#ifndef	VXGE_OS_LLDFMT
#define	VXGE_OS_LLDFMT				"%lld"
#endif

#ifndef	VXGE_OS_STXFMT
#define	VXGE_OS_STXFMT				"%zx"
#endif

#ifndef	VXGE_OS_STDFMT
#define	VXGE_OS_STDFMT				"%zd"
#endif


__EXTERN_END_DECLS

#endif	/* VXGE_DEFS_H */
