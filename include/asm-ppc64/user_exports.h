#ifndef _USER_EXPORTS_H
#define _USER_EXPORTS_H

/* 
 * Dave Engebretsen and Mike Corrigan {engebret|mikejc}@us.ibm.com
 *   Copyright (C) 2002 Dave Engebretsen & Mike Corrigan
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
#ifdef __powerpc64__
typedef unsigned long u64;
#else
typedef unsigned long long u64;
#endif

struct user_exports {
	/*==================================================================
	 * Cache line 1: 0x0000 - 0x007F
	 * Kernel only data - undefined for user space
	 *==================================================================
	 */
	u64 undefined[16]; 

	/*==================================================================
	 * Cache line 2: 0x0080 - 0x00FF
	 * Kernel / User data
	 *==================================================================
	 */
	u8  eye_catcher[6];      /* Eyecatcher: PPC64         0x00 */
	u16 version;             /* Version number            0x06 */
	u16 platform;	         /* Platform type             0x08 */
	u16 processor;		 /* Processor type            0x0A */
	u32 processorCount;	 /* # of physical processors  0x0C */
	u64 physicalMemorySize;	 /* Size of real memory(B)    0x10 */

	u16 dCacheL1Size;	 /* L1 d-cache size           0x18 */
	u16 dCacheL1LineSize;	 /* L1 d-cache line size      0x1A */
	u16 dCacheL1LogLineSize; /* L1 d-cache line size Log2 0x1C */
	u16 dCacheL1LinesPerPage;/* L1 d-cache lines / page   0x1E */
	u16 dCacheL1Assoc;       /* L1 d-cache associativity  0x20 */

	u16 iCacheL1Size;	 /* L1 i-cache size           0x22 */
	u16 iCacheL1LineSize;	 /* L1 i-cache line size      0x24 */
	u16 iCacheL1LogLineSize; /* L1 i-cache line size Log2 0x26 */
	u16 iCacheL1LinesPerPage;/* L1 i-cache lines / page   0x28 */
	u16 iCacheL1Assoc;       /* L1 i-cache associativity  0x2A */

	u16 cacheL2Size;	 /* L2 cache size             0x2C */
	u16 cacheL2Assoc;	 /* L2 cache associativity    0x2E */

	u64 tb_orig_stamp;       /* Timebase at boot          0x30 */
	u64 tb_ticks_per_sec;    /* Timebase tics / sec       0x38 */
	u64 tb_to_xs;            /* Inverse of TB to 2^20     0x40 */
	u64 stamp_xsec;          /*                           0x48 */
	volatile u64 tb_update_count; /* Timebase atomicity   0x50 */
	u32 tz_minuteswest;      /* Minutes west of Greenwich 0x58 */
	u32 tz_dsttime;          /* Type of dst correction    0x5C */

	u64 resv1[4];            /* Reserverd          0x60 - 0x7F */
};

/* Platform types */
#define PLATFORM_PSERIES      0x0100
#define PLATFORM_PSERIES_LPAR 0x0101
#define PLATFORM_ISERIES_LPAR 0x0201

/* Processor types */
#define PV_NORTHSTAR    0x0033
#define PV_PULSAR       0x0034
#define PV_POWER4       0x0035
#define PV_ICESTAR      0x0036
#define PV_SSTAR        0x0037
#define PV_POWER4p      0x0038
#define PV_POWER4ul     0x0039
#define PV_630          0x0040
#define PV_630p         0x0041

#endif /* USER_EXPORTS_H */
