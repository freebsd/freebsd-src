/* 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2000-2003 Silicon Graphics, Inc.  All rights reserved.
 */

#include <linux/config.h>

/*
 * Structure of the mem config of the node as a SN MI reg
 * Medusa supports this reg config.
 *
 * BankSize nibble to bank size mapping
 *
 *      1 - 64 MB
 *      2 - 128 MB
 *      3 - 256 MB
 *      4 - 512 MB
 *      5 - 1024 MB (1GB)
 */

#define MBSHIFT				20

#ifdef SGI_SN2
typedef struct node_memmap_s
{
        unsigned int    b0size  :3,     /* 0-2   bank 0 size */
                        b0dou   :1,     /* 3     bank 0 is 2-sided */
                        hack0   :4,     /* 4-7   bank 0 hack */
        		b1size  :3,     /* 8-10  bank 1 size */
                        b1dou   :1,     /* 11    bank 1 is 2-sided */
                        hack1   :4,     /* 12-15 bank 1 hack */
        		b2size  :3,     /* 16-18 bank 2 size */
                        b2dou   :1,     /* 19    bank 1 is 2-sided */
                        hack2   :4,     /* 20-23 bank 2 hack */
        		b3size  :3,     /* 24-26 bank 3 size */
                        b3dou   :1,     /* 27    bank 3 is 2-sided */
                        hack3   :4;     /* 28-31 bank 3 hack */
} node_memmap_t ;

#define PROXIMITY_DOMAIN(nasid) (((nasid)>>1) & 255)
#define SN2_BANK_SIZE_SHIFT		(MBSHIFT+6)     /* 64 MB */
#define MD_BANKS_PER_NODE 4
#define MD_BANKSIZE			(1UL << 34)

#define MAX_SN_NODES            256
#define MAX_LSAPICS             512
#define MAX_CPUS_NODE           4
#define MAX_CPUS                (MAX_CPUS_NODE*MAX_SN_NODES)
#define CPUS_PER_FSB            2
#define CPUS_PER_FSB_MASK       (CPUS_PER_FSB-1)
#define MAX_NASID               2048

#endif

#define FPROM_BUG()             do {while (1);} while (0)

typedef struct sn_memmap_s
{
	short		nasid ;
	short		cpuconfig;
	node_memmap_t 	node_memmap ;
} sn_memmap_t ;

typedef struct sn_config_s
{
	int		cpus;
	int		nodes;
	sn_memmap_t	memmap[1];		/* start of array */
} sn_config_t;

struct acpi_table_memory_affinity;

extern long base_nasid;

extern void build_init(unsigned long);
extern int build_efi_memmap(void *, int);
extern int GetNumNodes(void);
extern int GetNumCpus(void);
extern int IsCpuPresent(int, int);
extern int GetNasid(int);
extern void* build_memory_srat(struct acpi_table_memory_affinity *);
extern void GetLogicalCpu(int, int *, int *);
