#ifndef _SYSTEMCFG_H
#define _SYSTEMCFG_H

/* 
 * Copyright (C) 2002 Peter Bergner <bergner@vnet.ibm.com>, IBM
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

/* Change Activity:
 * 2002/09/30 : bergner  : Created
 * End Change Activity 
 */


#ifndef __KERNEL__
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/types.h>
#endif

/*
 * If the major version changes we are incompatible.
 * Minor version changes are a hint.
 */
#define SYSTEMCFG_MAJOR 1
#define SYSTEMCFG_MINOR 0

struct systemcfg {
	__u8  eye_catcher[16];		/* Eyecatcher: SYSTEMCFG:PPC64	0x00 */
	struct {			/* Systemcfg version numbers	     */
		__u32 major;		/* Major number			0x10 */
		__u32 minor;		/* Minor number			0x14 */
	} version;

	__u32 platform;			/* Platform flags		0x18 */
	__u32 processor;		/* Processor type		0x1C */
	__u64 processorCount;		/* # of physical processors	0x20 */
	__u64 physicalMemorySize;	/* Size of real memory(B)	0x28 */
	__u64 tb_orig_stamp;		/* Timebase at boot		0x30 */
	__u64 tb_ticks_per_sec;		/* Timebase tics / sec		0x38 */
	__u64 tb_to_xs;			/* Inverse of TB to 2^20	0x40 */
	__u64 stamp_xsec;		/*				0x48 */
	__u64 tb_update_count;		/* Timebase atomicity ctr	0x50 */
	__u32 tz_minuteswest;		/* Minutes west of Greenwich	0x58 */
	__u32 tz_dsttime;		/* Type of dst correction	0x5C */
	__u32 dCacheL1Size;		/* L1 d-cache size		0x60 */
	__u32 dCacheL1LineSize;		/* L1 d-cache line size		0x64 */
	__u32 iCacheL1Size;		/* L1 i-cache size		0x68 */
	__u32 iCacheL1LineSize;		/* L1 i-cache line size		0x6C */
	__u8  reserved0[3984];		/* Reserve rest of page		0x70 */
};

#ifdef __KERNEL__
extern struct systemcfg *systemcfg;
#else

/* Processor Version Register (PVR) field extraction */
#define PVR_VER(pvr)  (((pvr) >>  16) & 0xFFFF) /* Version field */
#define PVR_REV(pvr)  (((pvr) >>   0) & 0xFFFF) /* Revison field */

/* Processor Version Numbers */
#define PV_NORTHSTAR    0x0033
#define PV_PULSAR       0x0034
#define PV_POWER4       0x0035
#define PV_ICESTAR      0x0036
#define PV_SSTAR        0x0037
#define PV_POWER4p      0x0038
#define PV_POWER4ul	0x0039
#define PV_630          0x0040
#define PV_630p         0x0041

/* Platforms supported by PPC64 */
#define PLATFORM_PSERIES      0x0100
#define PLATFORM_PSERIES_LPAR 0x0101
#define PLATFORM_ISERIES_LPAR 0x0201


static inline volatile struct systemcfg *systemcfg_init(void)
{
	int fd = open("/proc/ppc64/systemcfg", O_RDONLY);
	volatile struct systemcfg *ret;

	if (fd == -1)
		return 0;
	ret = mmap(0, sizeof(struct systemcfg), PROT_READ, MAP_SHARED, fd, 0);
	close(fd);
	if (!ret)
		return 0;
	if (ret->version.major != SYSTEMCFG_MAJOR || ret->version.minor < SYSTEMCFG_MINOR) {
		munmap((void *)ret, sizeof(struct systemcfg));
		return 0;
	}
	return ret;
}
#endif /* __KERNEL__ */

#endif /* _SYSTEMCFG_H */
