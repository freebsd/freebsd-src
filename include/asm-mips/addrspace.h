/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1996 by Ralf Baechle
 * Copyright (C) 2000, 2002  Maciej W. Rozycki
 *
 * Definitions for the address spaces of the MIPS CPUs.
 */
#ifndef __ASM_MIPS_ADDRSPACE_H
#define __ASM_MIPS_ADDRSPACE_H

/*
 *  Configure language
 */
#ifdef __ASSEMBLY__
#define _ATYPE_
#define _ATYPE32_
#define _ATYPE64_
#else
#define _ATYPE_		__PTRDIFF_TYPE__
#define _ATYPE32_	int
#define _ATYPE64_	long long
#endif

/*
 *  32-bit MIPS address spaces
 */
#ifdef __ASSEMBLY__
#define _ACAST32_
#define _ACAST64_
#else
#define _ACAST32_		(_ATYPE_)(_ATYPE32_)	/* widen if necessary */
#define _ACAST64_		(_ATYPE64_)		/* do _not_ narrow */
#endif

/*
 * Memory segments (32bit kernel mode addresses)
 */
#define KUSEG			0x00000000
#define KSEG0			0x80000000
#define KSEG1			0xa0000000
#define KSEG2			0xc0000000
#define KSEG3			0xe0000000

#define K0BASE			KSEG0

/*
 * Returns the kernel segment base of a given address
 */
#define KSEGX(a)		((_ACAST32_ (a)) & 0xe0000000)

/*
 * Returns the physical address of a KSEG0/KSEG1 address
 */
#define CPHYSADDR(a)		((_ACAST32_ (a)) & 0x1fffffff)

#ifndef __ASSEMBLY__
#define PHYSADDR(a)		CPHYSADDR(a)
#endif

/*
 * Map an address to a certain kernel segment
 */
#define KSEG0ADDR(a)		(CPHYSADDR(a) | KSEG0)
#define KSEG1ADDR(a)		(CPHYSADDR(a) | KSEG1)
#define KSEG2ADDR(a)		(CPHYSADDR(a) | KSEG2)
#define KSEG3ADDR(a)		(CPHYSADDR(a) | KSEG3)

/*
 * Memory segments (64bit kernel mode addresses)
 */
#define XKUSEG			0x0000000000000000
#define XKSSEG			0x4000000000000000
#define XKPHYS			0x8000000000000000
#define XKSEG			0xc000000000000000
#define CKSEG0			0xffffffff80000000
#define CKSEG1			0xffffffffa0000000
#define CKSSEG			0xffffffffc0000000
#define CKSEG3			0xffffffffe0000000

/*
 * Cache modes for XKPHYS address conversion macros
 */
#define K_CALG_COH_EXCL1_NOL2	0
#define K_CALG_COH_SHRL1_NOL2	1
#define K_CALG_UNCACHED		2
#define K_CALG_NONCOHERENT	3
#define K_CALG_COH_EXCL		4
#define K_CALG_COH_SHAREABLE	5
#define K_CALG_NOTUSED		6
#define K_CALG_UNCACHED_ACCEL	7

#define TO_PHYS_MASK			0xfffffffffULL		/* 36 bit */

/*
 * 64-bit address conversions
 */
#define PHYS_TO_XKSEG_UNCACHED(p)	PHYS_TO_XKPHYS(K_CALG_UNCACHED,(p))
#define PHYS_TO_XKSEG_CACHED(p)		PHYS_TO_XKPHYS(K_CALG_COH_SHAREABLE,(p))
#define XKPHYS_TO_PHYS(p)		((p) & TO_PHYS_MASK)
#define PHYS_TO_XKPHYS(cm,a)		(0x8000000000000000 | ((cm)<<59) | (a))

#endif /* __ASM_MIPS_ADDRSPACE_H */
