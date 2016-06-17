/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Derived from IRIX <sys/SN/kldir.h>, revision 1.21.
 *
 * Copyright (C) 1992-1997,1999,2001-2003 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (C) 1999 by Ralf Baechle
 */
#ifndef _ASM_IA64_SN_KLDIR_H
#define _ASM_IA64_SN_KLDIR_H

#include <asm/sn/sgi.h>

/*
 * The kldir memory area resides at a fixed place in each node's memory and
 * provides pointers to most other IP27 memory areas.  This allows us to
 * resize and/or relocate memory areas at a later time without breaking all
 * firmware and kernels that use them.  Indices in the array are
 * permanently dedicated to areas listed below.  Some memory areas (marked
 * below) reside at a permanently fixed location, but are included in the
 * directory for completeness.
 */

#define KLDIR_MAGIC		0x434d5f53505f5357

/*
 * The upper portion of the memory map applies during boot
 * only and is overwritten by IRIX/SYMMON.
 *
 *                                    MEMORY MAP PER NODE
 *
 * 0x2000000 (32M)         +-----------------------------------------+
 *                         |      IO6 BUFFERS FOR FLASH ENET IOC3    |
 * 0x1F80000 (31.5M)       +-----------------------------------------+
 *                         |      IO6 TEXT/DATA/BSS/stack            |
 * 0x1C00000 (30M)         +-----------------------------------------+
 *                         |      IO6 PROM DEBUG TEXT/DATA/BSS/stack |
 * 0x0800000 (28M)         +-----------------------------------------+
 *                         |      IP27 PROM TEXT/DATA/BSS/stack      |
 * 0x1B00000 (27M)         +-----------------------------------------+
 *                         |      IP27 CFG                           |
 * 0x1A00000 (26M)         +-----------------------------------------+
 *                         |      Graphics PROM                      |
 * 0x1800000 (24M)         +-----------------------------------------+
 *                         |      3rd Party PROM drivers             |
 * 0x1600000 (22M)         +-----------------------------------------+
 *                         |                                         |
 *                         |      Free                               |
 *                         |                                         |
 *                         +-----------------------------------------+
 *                         |      UNIX DEBUG Version                 |
 * 0x190000 (2M--)         +-----------------------------------------+
 *                         |      SYMMON                             |
 *                         |      (For UNIX Debug only)              |
 * 0x34000 (208K)          +-----------------------------------------+
 *                         |      SYMMON STACK [NUM_CPU_PER_NODE]    |
 *                         |      (For UNIX Debug only)              |
 * 0x25000 (148K)          +-----------------------------------------+
 *                         |      KLCONFIG - II (temp)               |
 *                         |                                         |
 *                         |    ----------------------------         |
 *                         |                                         |
 *                         |      UNIX NON-DEBUG Version             |
 * 0x19000 (100K)          +-----------------------------------------+
 *
 *
 * The lower portion of the memory map contains information that is
 * permanent and is used by the IP27PROM, IO6PROM and IRIX.
 *
 * 0x19000 (100K)          +-----------------------------------------+
 *                         |                                         |
 *                         |      PI Error Spools (32K)              |
 *                         |                                         |
 * 0x12000 (72K)           +-----------------------------------------+
 *                         |      Unused                             |
 * 0x11c00 (71K)           +-----------------------------------------+
 *                         |      CPU 1 NMI Eframe area       	     |
 * 0x11a00 (70.5K)         +-----------------------------------------+
 *                         |      CPU 0 NMI Eframe area       	     |
 * 0x11800 (70K)           +-----------------------------------------+
 *                         |      CPU 1 NMI Register save area       |
 * 0x11600 (69.5K)         +-----------------------------------------+
 *                         |      CPU 0 NMI Register save area       |
 * 0x11400 (69K)           +-----------------------------------------+
 *                         |      GDA (1k)                           |
 * 0x11000 (68K)           +-----------------------------------------+
 *                         |      Early cache Exception stack        |
 *                         |             and/or                      |
 *			   |      kernel/io6prom nmi registers	     |
 * 0x10800  (66k)	   +-----------------------------------------+
 *			   |      cache error eframe   	 	     |
 * 0x10400 (65K)           +-----------------------------------------+
 *                         |      Exception Handlers (UALIAS copy)   |
 * 0x10000 (64K)           +-----------------------------------------+
 *                         |                                         |
 *                         |                                         |
 *                         |      KLCONFIG - I (permanent) (48K)     |
 *                         |                                         |
 *                         |                                         |
 *                         |                                         |
 * 0x4000 (16K)            +-----------------------------------------+
 *                         |      NMI Handler (Protected Page)       |
 * 0x3000 (12K)            +-----------------------------------------+
 *                         |      ARCS PVECTORS (master node only)   |
 * 0x2c00 (11K)            +-----------------------------------------+
 *                         |      ARCS TVECTORS (master node only)   |
 * 0x2800 (10K)            +-----------------------------------------+
 *                         |      LAUNCH [NUM_CPU]                   |
 * 0x2400 (9K)             +-----------------------------------------+
 *                         |      Low memory directory (KLDIR)       |
 * 0x2000 (8K)             +-----------------------------------------+
 *                         |      ARCS SPB (1K)                      |
 * 0x1000 (4K)             +-----------------------------------------+
 *                         |      Early cache Exception stack        |
 *                         |             and/or                      |
 *			   |      kernel/io6prom nmi registers	     |
 * 0x800  (2k)	           +-----------------------------------------+
 *			   |      cache error eframe   	 	     |
 * 0x400 (1K)              +-----------------------------------------+
 *                         |      Exception Handlers                 |
 * 0x0   (0K)              +-----------------------------------------+
 */

#ifdef __ASSEMBLY__
#define KLDIR_OFF_MAGIC			0x00
#define KLDIR_OFF_OFFSET		0x08
#define KLDIR_OFF_POINTER		0x10
#define KLDIR_OFF_SIZE			0x18
#define KLDIR_OFF_COUNT			0x20
#define KLDIR_OFF_STRIDE		0x28
#endif /* __ASSEMBLY__ */

#ifndef __ASSEMBLY__
typedef struct kldir_ent_s {
	u64		magic;		/* Indicates validity of entry      */
	off_t		offset;		/* Offset from start of node space  */
	__psunsigned_t	pointer;	/* Pointer to area in some cases    */
	size_t		size;		/* Size in bytes 		    */
	u64		count;		/* Repeat count if array, 1 if not  */
	size_t		stride;		/* Stride if array, 0 if not        */
	char		rsvd[16];	/* Pad entry to 0x40 bytes          */
	/* NOTE: These 16 bytes are used in the Partition KLDIR
	   entry to store partition info. Refer to klpart.h for this. */
} kldir_ent_t;
#endif /* __ASSEMBLY__ */


#define KLDIR_ENT_SIZE			0x40
#define KLDIR_MAX_ENTRIES		(0x400 / 0x40)



/*
 * The upper portion of the memory map applies during boot
 * only and is overwritten by IRIX/SYMMON.  The minimum memory bank
 * size on IP35 is 64M, which provides a limit on the amount of space
 * the PROM can assume it has available.
 *
 * Most of the addresses below are defined as macros in this file, or
 * in SN/addrs.h or SN/SN1/addrs.h.
 *
 *                                    MEMORY MAP PER NODE
 *
 * 0x4000000 (64M)         +-----------------------------------------+
 *                         |                                         |
 *                         |                                         |
 *                         |      IO7 TEXT/DATA/BSS/stack            |
 * 0x3000000 (48M)         +-----------------------------------------+
 *                         |      Free                               |
 * 0x2102000 (>33M)        +-----------------------------------------+
 *                         |      IP35 Topology (PCFG) + misc data   |
 * 0x2000000 (32M)         +-----------------------------------------+
 *                         |      IO7 BUFFERS FOR FLASH ENET IOC3    |
 * 0x1F80000 (31.5M)       +-----------------------------------------+
 *                         |      Free                               |
 * 0x1C00000 (28M)         +-----------------------------------------+
 *                         |      IP35 PROM TEXT/DATA/BSS/stack      |
 * 0x1A00000 (26M)         +-----------------------------------------+
 *                         |      Routing temp. space                |
 * 0x1800000 (24M)         +-----------------------------------------+
 *                         |      Diagnostics temp. space            |
 * 0x1500000 (21M)         +-----------------------------------------+
 *                         |      Free                               |
 * 0x1400000 (20M)         +-----------------------------------------+
 *                         |      IO7 PROM temporary copy            |
 * 0x1300000 (19M)         +-----------------------------------------+
 *                         |                                         |
 *                         |      Free                               |
 *                         |      (UNIX DATA starts above 0x1000000) |
 *                         |                                         |
 *                         +-----------------------------------------+
 *                         |      UNIX DEBUG Version                 |
 * 0x0310000 (3.1M)        +-----------------------------------------+
 *                         |      SYMMON, loaded just below UNIX     |
 *                         |      (For UNIX Debug only)              |
 *                         |                                         |
 *                         |                                         |
 * 0x006C000 (432K)        +-----------------------------------------+
 *                         |      SYMMON STACK [NUM_CPU_PER_NODE]    |
 *                         |      (For UNIX Debug only)              |
 * 0x004C000 (304K)        +-----------------------------------------+
 *                         |                                         |
 *                         |                                         |
 *                         |      UNIX NON-DEBUG Version             |
 * 0x0040000 (256K)        +-----------------------------------------+
 *
 *
 * The lower portion of the memory map contains information that is
 * permanent and is used by the IP35PROM, IO7PROM and IRIX.
 *
 * 0x40000 (256K)          +-----------------------------------------+
 *                         |                                         |
 *                         |      KLCONFIG (64K)                     |
 *                         |                                         |
 * 0x30000 (192K)          +-----------------------------------------+
 *                         |                                         |
 *                         |      PI Error Spools (64K)              |
 *                         |                                         |
 * 0x20000 (128K)          +-----------------------------------------+
 *                         |                                         |
 *                         |      Unused                             |
 *                         |                                         |
 * 0x19000 (100K)          +-----------------------------------------+
 *                         |      Early cache Exception stack (CPU 3)|
 * 0x18800 (98K)           +-----------------------------------------+
 *			   |      cache error eframe (CPU 3)	     |
 * 0x18400 (97K)           +-----------------------------------------+
 *                         |      Exception Handlers (CPU 3)         |
 * 0x18000 (96K)           +-----------------------------------------+
 *                         |                                         |
 *                         |      Unused                             |
 *                         |                                         |
 * 0x13c00 (79K)           +-----------------------------------------+
 *                         |      GPDA (8k)                          |
 * 0x11c00 (71K)           +-----------------------------------------+
 *                         |      Early cache Exception stack (CPU 2)|
 * 0x10800 (66k)	   +-----------------------------------------+
 *			   |      cache error eframe (CPU 2)	     |
 * 0x10400 (65K)           +-----------------------------------------+
 *                         |      Exception Handlers (CPU 2)         |
 * 0x10000 (64K)           +-----------------------------------------+
 *                         |                                         |
 *                         |      Unused                             |
 *                         |                                         |
 * 0x0b400 (45K)           +-----------------------------------------+
 *                         |      GDA (1k)                           |
 * 0x0b000 (44K)           +-----------------------------------------+
 *                         |      NMI Eframe areas (4)       	     |
 * 0x0a000 (40K)           +-----------------------------------------+
 *                         |      NMI Register save areas (4)        |
 * 0x09000 (36K)           +-----------------------------------------+
 *                         |      Early cache Exception stack (CPU 1)|
 * 0x08800 (34K)           +-----------------------------------------+
 *			   |      cache error eframe (CPU 1)	     |
 * 0x08400 (33K)           +-----------------------------------------+
 *                         |      Exception Handlers (CPU 1)         |
 * 0x08000 (32K)           +-----------------------------------------+
 *                         |                                         |
 *                         |                                         |
 *                         |      Unused                             |
 *                         |                                         |
 *                         |                                         |
 * 0x04000 (16K)           +-----------------------------------------+
 *                         |      NMI Handler (Protected Page)       |
 * 0x03000 (12K)           +-----------------------------------------+
 *                         |      ARCS PVECTORS (master node only)   |
 * 0x02c00 (11K)           +-----------------------------------------+
 *                         |      ARCS TVECTORS (master node only)   |
 * 0x02800 (10K)           +-----------------------------------------+
 *                         |      LAUNCH [NUM_CPU]                   |
 * 0x02400 (9K)            +-----------------------------------------+
 *                         |      Low memory directory (KLDIR)       |
 * 0x02000 (8K)            +-----------------------------------------+
 *                         |      ARCS SPB (1K)                      |
 * 0x01000 (4K)            +-----------------------------------------+
 *                         |      Early cache Exception stack (CPU 0)|
 * 0x00800 (2k)	           +-----------------------------------------+
 *			   |      cache error eframe (CPU 0)	     |
 * 0x00400 (1K)            +-----------------------------------------+
 *                         |      Exception Handlers (CPU 0)         |
 * 0x00000 (0K)            +-----------------------------------------+
 */

/*
 * NOTE:  To change the kernel load address, you must update:
 *  - the appropriate elspec files in irix/kern/master.d
 *  - NODEBUGUNIX_ADDR in SN/SN1/addrs.h
 *  - IP27_FREEMEM_OFFSET below
 *  - KERNEL_START_OFFSET below (if supporting cells)
 */


/*
 * This is defined here because IP27_SYMMON_STK_SIZE must be at least what
 * we define here.  Since it's set up in the prom.  We can't redefine it later
 * and expect more space to be allocated.  The way to find out the true size
 * of the symmon stacks is to divide SYMMON_STK_SIZE by SYMMON_STK_STRIDE
 * for a particular node.
 */
#define SYMMON_STACK_SIZE		0x8000

#if defined (PROM) || defined (SABLE)

/*
 * These defines are prom version dependent.  No code other than the IP35
 * prom should attempt to use these values.
 */
#define IP27_LAUNCH_OFFSET		0x2400
#define IP27_LAUNCH_SIZE		0x400
#define IP27_LAUNCH_COUNT		4
#define IP27_LAUNCH_STRIDE		0x100 /* could be as small as 0x80 */

#define IP27_KLCONFIG_OFFSET		0x30000
#define IP27_KLCONFIG_SIZE		0x10000
#define IP27_KLCONFIG_COUNT		1
#define IP27_KLCONFIG_STRIDE		0

#define IP27_NMI_OFFSET			0x3000
#define IP27_NMI_SIZE			0x100
#define IP27_NMI_COUNT			4
#define IP27_NMI_STRIDE			0x40

#define IP27_PI_ERROR_OFFSET		0x20000
#define IP27_PI_ERROR_SIZE		0x10000
#define IP27_PI_ERROR_COUNT		1
#define IP27_PI_ERROR_STRIDE		0

#define IP27_SYMMON_STK_OFFSET		0x4c000
#define IP27_SYMMON_STK_SIZE		0x20000
#define IP27_SYMMON_STK_COUNT		4
/* IP27_SYMMON_STK_STRIDE must be >= SYMMON_STACK_SIZE */
#define IP27_SYMMON_STK_STRIDE		0x8000

#define IP27_FREEMEM_OFFSET		0x40000
#define IP27_FREEMEM_SIZE		(-1)
#define IP27_FREEMEM_COUNT		1
#define IP27_FREEMEM_STRIDE		0

#endif /* PROM || SABLE*/
/*
 * There will be only one of these in a partition so the IO7 must set it up.
 */
#define IO6_GDA_OFFSET			0xb000
#define IO6_GDA_SIZE			0x400
#define IO6_GDA_COUNT			1
#define IO6_GDA_STRIDE			0

/*
 * save area of kernel nmi regs in the prom format
 */
#define IP27_NMI_KREGS_OFFSET		0x9000
#define IP27_NMI_KREGS_CPU_SIZE		0x400
/*
 * save area of kernel nmi regs in eframe format 
 */
#define IP27_NMI_EFRAME_OFFSET		0xa000
#define IP27_NMI_EFRAME_SIZE		0x400

#define GPDA_OFFSET			0x11c00

#endif /* _ASM_IA64_SN_KLDIR_H */
