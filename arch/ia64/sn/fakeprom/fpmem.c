/* 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2000-2003 Silicon Graphics, Inc.  All rights reserved.
 */



/*
 * FPROM EFI memory descriptor build routines
 *
 * 	- Routines to build the EFI memory descriptor map
 * 	- Should also be usable by the SGI prom to convert
 * 	  klconfig to efi_memmap
 */


/*
 * Sometimes it is useful to build a fake prom with a memmap that matches another
 * platform. There are restrictions on how successful you will be, but here
 * are the instructions for what I have used in the past & had good luck:
 *
 * 	- compile a kernel for the target platform (like zx1) with the following
 * 	  set: 
 * 	  	#define EFI_DEBUG 1		# in arch/ia64/kernel/efi.c
 * 	  	#define SRAT_DEBUG		# in drivers/acpi/numa.c
 *
 *	- this causes the kernel to dump the memmap & SRAT during boot.
 *
 *	- copy the console output to a file.
 *
 *	- run the following script with the file as input:
 *		
 *			#!/bin/sh
 *			
 *			awk '
 *			/^mem/ {
 *			        printf "\t{%s, %sUL, %sUL, %sUL},\n", $4, $7, $11, $12
 *			} 
 *			/SRAT Memory/ {
 *			        if (srat != 1) print "SRAT"
 *			        srat = 1
 *			        for (i=1; i<=0;i++)
 *			                printf "%4d |%s|\n", i, $i
 *			        printf "\t{%s, %sUL, %sUL, %sUL},\n", $4, $6, $8, $13
 *			}
 *			BEGIN {
 *			        FS="[ \[\)\(\t:=,-]"
 *			} '
 *			
 * 	- this converts the memmap & SRAT info into C code array initilization statements.
 *
 * 	- copy & paste the initialization statements into the arrays below. Look for
 * 	  comments "PASTE xxx HERE". In general, on the MEMMAP is present on most other
 * 	  systems. If you have an SRAT, you may need to hack the number of nodes in fw-emu.
 * 	  Good luck...
 *
 * 	- set "#define FAKE_MEMMAP 1
 *
 * 	- When running medusa, make sure you set the node1_memory_config to cover the
 * 	  amount of memory that you are going to use. Also note that you can run the
 * 	  kernel in "alias" space (starts at phy adr 0). This is kinda tricky, though
 */


#include <linux/config.h>
#include <linux/efi.h>
#include <linux/acpi.h>
#include "fpmem.h"

/*
 * args points to a layout in memory like this
 *
 *		32 bit		32 bit
 *
 * 		numnodes	numcpus
 *
 *		16 bit   16 bit		   32 bit
 *		nasid0	cpuconf		membankdesc0
 *		nasid1	cpuconf		membankdesc1
 *			   .
 *			   .
 *			   .
 *			   .
 *			   .
 */

sn_memmap_t	*sn_memmap ;
sn_config_t	*sn_config ;

/*
 * There is a hole in the node 0 address space. Dont put it
 * in the memory map
 */
#define NODE0_HOLE_SIZE         (20*MB)
#define NODE0_HOLE_END          (4UL*GB)

#define	MB			(1024*1024)
#define GB			(1024*MB)
#define KERNEL_SIZE		(4*MB)
#define PROMRESERVED_SIZE	(1*MB)

#ifdef SGI_SN2
#define PHYS_ADDRESS(_n, _x)		(((long)_n<<38) | (long)_x | 0x3000000000UL)
#define MD_BANK_SHFT 34
#endif

/*
 * For SN, this may not take an arg and gets the numnodes from 
 * the prom variable or by traversing klcfg or promcfg
 */
int
GetNumNodes(void)
{
	return sn_config->nodes;
}

int
GetNumCpus(void)
{
	return sn_config->cpus;
}

/* For SN, get the index th nasid */

int
GetNasid(int cnode)
{
	return sn_memmap[cnode].nasid ;
}

node_memmap_t
GetMemBankInfo(int index)
{
	return sn_memmap[index].node_memmap ;
}

int
IsCpuPresent(int cnode, int cpu)
{
	return  sn_memmap[cnode].cpuconfig & (1UL<<cpu);
}

void
GetLogicalCpu(int bsp, int *nasidp, int *cpup)
{
	int cnode, cpu;

	for (cnode=0; cnode<GetNumNodes(); cnode++) {
		for (cpu=0; cpu<MAX_CPUS_NODE; cpu++)
			if (IsCpuPresent(cnode,cpu)) {
				*nasidp = GetNasid(cnode);
				*cpup = cpu;
				if (bsp-- == 0)
					return;
			}
	}
}

/*
 * Made this into an explicit case statement so that
 * we can assign specific properties to banks like bank0
 * actually disabled etc.
 */

#ifdef SGI_SN2
long
GetBankSize(int index, node_memmap_t nmemmap)
{
	int bsize, bdou, hack;
	/*
	 * Add 2 because there are 4 dimms per bank.
	 */
        switch (index) {
                case 0:
			bsize = nmemmap.b0size;
			bdou = nmemmap.b0dou;
			hack = nmemmap.hack0;
			break;
                case 1:
			bsize = nmemmap.b1size;
			bdou = nmemmap.b1dou;
			hack = nmemmap.hack1;
			break;
                case 2:
			bsize = nmemmap.b2size;
			bdou = nmemmap.b2dou;
			hack = nmemmap.hack2;
			break;
                case 3:
			bsize = nmemmap.b3size;
			bdou = nmemmap.b3dou;
			hack = nmemmap.hack3;
			break;
                default:return -1 ;
        }

	if (bsize < 6 && hack == 0)
		return (1UL<<((2+bsize+bdou)+SN2_BANK_SIZE_SHIFT))*31/32;
	else 
		return (16*MB)*hack;
}

#endif

void
build_mem_desc(efi_memory_desc_t *md, int type, long paddr, long numbytes, long attr)
{
        md->type = type;
        md->phys_addr = paddr;
        md->virt_addr = 0;
        md->num_pages = numbytes >> 12;
        md->attribute = attr;
}


//#define FAKE_MEMMAP 1
#ifdef FAKE_MEMMAP

#define OFF 0x3000000000UL
struct {
	unsigned long	type;
	unsigned long	attr;
	unsigned long	start;
	unsigned long	end;
} mdx[] = {
	/* PASTE SRAT HERE */
};

struct srat {
	unsigned long	start;
	unsigned long	len;
	unsigned long	type;
	unsigned long	pxm;
} srat[] = {

	/* PASTE SRAT HERE */

};



void *
build_memory_srat(struct acpi_table_memory_affinity *ptr)
{
	int i;
	int n = sizeof(srat)/sizeof(struct srat);

        for (i=0; i<n; i++)
		srat[i].start += OFF;
        for (i=0; i<n; i++) {
                ptr->header.type = ACPI_SRAT_MEMORY_AFFINITY;
                ptr->header.length = sizeof(struct acpi_table_memory_affinity);
                ptr->proximity_domain = srat[i].pxm;
                ptr->base_addr_lo = srat[i].start & 0xffffffff;
                ptr->length_lo = srat[i].len & 0xffffffff;
                ptr->base_addr_hi = srat[i].start >> 32;
                ptr->length_hi = srat[i].len >> 32;
                ptr->memory_type = ACPI_ADDRESS_RANGE_MEMORY;
                ptr->flags.enabled = 1;
		ptr++;
        }
	return ptr;
}

int
build_efi_memmap(void *md, int mdsize)
{
	int		i;

	for (i=0; i<sizeof(mdx)/32; i++) {
		mdx[i].start += OFF;
		mdx[i].end += OFF;
	}
	for (i=0; i<sizeof(mdx)/32; i++) {
			build_mem_desc(md, mdx[i].type, mdx[i].start, mdx[i].end-mdx[i].start, mdx[i].attr);
			md += mdsize;
	}
	return i;

}

#else /* ! FAKE_MEMMAP */

void *
build_memory_srat(struct acpi_table_memory_affinity *ptr)
{
	int	cnode, nasid;

	for (cnode=0; cnode<GetNumNodes(); cnode++) {
		nasid = GetNasid(cnode);
		ptr->header.type = ACPI_SRAT_MEMORY_AFFINITY;
		ptr->header.length = sizeof(struct acpi_table_memory_affinity);
		ptr->proximity_domain = PROXIMITY_DOMAIN(nasid);
		ptr->base_addr_lo = 0;
		ptr->length_lo = 0;
#if defined(SGI_SN2)
		ptr->base_addr_hi = (nasid<<6) | (3<<4);
		ptr->length_hi = (MD_BANKSIZE*MD_BANKS_PER_NODE)>>32;
#endif
		ptr->memory_type = ACPI_ADDRESS_RANGE_MEMORY;
		ptr->flags.enabled = 1;
		ptr++;
	}
	return ptr;
}


int
build_efi_memmap(void *md, int mdsize)
{
	int		numnodes = GetNumNodes() ;
	int		cnode,bank ;
	int		nasid ;
	node_memmap_t	membank_info ;
	int		count = 0 ;
	long		paddr, hole, numbytes;


	for (cnode=0;cnode<numnodes;cnode++) {
		nasid = GetNasid(cnode) ;
		membank_info = GetMemBankInfo(cnode) ;
		for (bank=0;bank<MD_BANKS_PER_NODE;bank++) {
			numbytes = GetBankSize(bank, membank_info);
			if (numbytes) {
                                paddr = PHYS_ADDRESS(nasid, (long)bank<<MD_BANK_SHFT);
				/*
				 * Only emulate the memory prom grabs
				 * if we have lots of memory, to allow
				 * us to simulate smaller memory configs than
				 * we can actually run on h/w.  Otherwise,
				 * linux throws away a whole "granule".
				 */
				if (cnode == 0 && bank == 0 &&
				    numbytes > 128*1024*1024) {
					numbytes -= 1000;
				}

                                /*
                                 * Check for the node 0 hole. Since banks cant
                                 * span the hole, we only need to check if the end of
                                 * the range is the end of the hole.
                                 */
                                if (paddr+numbytes == NODE0_HOLE_END)
                                        numbytes -= NODE0_HOLE_SIZE;
                                /*
                                 * UGLY hack - we must skip overr the kernel and
                                 * PROM runtime services but we dont exactly where it is.
                                 * So lets just reserve:
				 *	node 0
				 *		0-1MB for PAL
				 *		1-4MB for SAL
				 *	node 1-N
				 *		0-1 for SAL
                                 */
                                if (bank == 0) {
					if (cnode == 0) {
						hole = 2*1024*1024;
						build_mem_desc(md, EFI_PAL_CODE, paddr, hole, EFI_MEMORY_WB|EFI_MEMORY_WB);
						numbytes -= hole;
                                        	paddr += hole;
			        		count++ ;
                                        	md += mdsize;
						hole = 1*1024*1024;
						build_mem_desc(md, EFI_CONVENTIONAL_MEMORY, paddr, hole, EFI_MEMORY_UC);
						numbytes -= hole;
                                        	paddr += hole;
			        		count++ ;
                                        	md += mdsize;
						hole = 1*1024*1024;
						build_mem_desc(md, EFI_RUNTIME_SERVICES_DATA, paddr, hole, EFI_MEMORY_WB|EFI_MEMORY_WB);
						numbytes -= hole;
                                        	paddr += hole;
			        		count++ ;
                                        	md += mdsize;
					} else {
						hole = 2*1024*1024;
                                        	build_mem_desc(md, EFI_RUNTIME_SERVICES_DATA, paddr, hole, EFI_MEMORY_WB|EFI_MEMORY_WB);
						numbytes -= hole;
                                        	paddr += hole;
			        		count++ ;
                                        	md += mdsize;
						hole = 2*1024*1024;
                                        	build_mem_desc(md, EFI_RUNTIME_SERVICES_DATA, paddr, hole, EFI_MEMORY_UC);
						numbytes -= hole;
                                        	paddr += hole;
			        		count++ ;
                                        	md += mdsize;
					}
                                }
                                build_mem_desc(md, EFI_CONVENTIONAL_MEMORY, paddr, numbytes, EFI_MEMORY_WB|EFI_MEMORY_WB);

			        md += mdsize ;
			        count++ ;
			}
		}
	}
	return count ;
}
#endif /* FAKE_MEMMAP */

void
build_init(unsigned long args)
{
	sn_config = (sn_config_t *) (args);	
	sn_memmap = (sn_memmap_t *)(args + 8) ; /* SN equiv for this is */
						/* init to klconfig start */
}
