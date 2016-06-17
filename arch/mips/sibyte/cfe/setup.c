/*
 * Copyright (C) 2000, 2001, 2002, 2003 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/blk.h>
#include <linux/bootmem.h>
#include <linux/smp.h>

#include <asm/bootinfo.h>
#include <asm/reboot.h>
#include <asm/sibyte/board.h>

#include "cfe_api.h"
#include "cfe_error.h"

/* Max ram addressable in 32-bit segments */
#ifdef CONFIG_MIPS64
#define MAX_RAM_SIZE (~0ULL)
#else
#ifdef CONFIG_HIGHMEM
#ifdef CONFIG_64BIT_PHYS_ADDR
#define MAX_RAM_SIZE (~0ULL)
#else
#define MAX_RAM_SIZE (0xffffffffULL)
#endif
#else
#define MAX_RAM_SIZE (0x1fffffffULL)
#endif
#endif

#define SIBYTE_MAX_MEM_REGIONS 8
phys_t board_mem_region_addrs[SIBYTE_MAX_MEM_REGIONS];
phys_t board_mem_region_sizes[SIBYTE_MAX_MEM_REGIONS];
unsigned int board_mem_region_count;

/* This is the kernel command line.  Actually, it's
   copied, eventually, to command_line, and looks to be
   quite redundant.  But not something to fix just now */
extern char arcs_cmdline[];

int cfe_cons_handle;

#ifdef CONFIG_EMBEDDED_RAMDISK
/* These are symbols defined by the ramdisk linker script */
extern unsigned char __rd_start;
extern unsigned char __rd_end;
#endif

#ifdef CONFIG_SMP
static int reboot_smp = 0;
#endif

#ifdef CONFIG_KGDB
extern int kgdb_port;
#endif

static void cfe_linux_exit(void)
{
#ifdef CONFIG_SMP
	if (smp_processor_id()) {
		if (reboot_smp) {
			/* Don't repeat the process from another CPU */
			for (;;);
		} else {
			/* Get CPU 0 to do the cfe_exit */
			reboot_smp = 1;
			smp_call_function((void *)_machine_restart, NULL, 1, 0);
			for (;;);
		}
	}
#endif
	printk("passing control back to CFE\n");
	cfe_exit(1, 0);
	printk("cfe_exit returned??\n");
	while(1);
}

static __init void prom_meminit(void)
{
	u64 addr, size, type; /* regardless of 64BIT_PHYS_ADDR */
	int mem_flags = 0;
	unsigned int idx;
	int rd_flag;
#ifdef CONFIG_BLK_DEV_INITRD
	unsigned long initrd_pstart;
	unsigned long initrd_pend;

#ifdef CONFIG_EMBEDDED_RAMDISK
	/* If we're using an embedded ramdisk, then __rd_start and __rd_end
	   are defined by the linker to be on either side of the ramdisk
	   area.  Otherwise, initrd_start should be defined by kernel command
	   line arguments */
	if (initrd_start == 0) {
		initrd_start = (unsigned long)&__rd_start;
		initrd_end = (unsigned long)&__rd_end;
	}
#endif

	initrd_pstart = CPHYSADDR(initrd_start);
	initrd_pend = CPHYSADDR(initrd_end);
	if (initrd_start &&
	    ((initrd_pstart > MAX_RAM_SIZE)
	     || (initrd_pend > MAX_RAM_SIZE))) {
		panic("initrd out of addressable memory");
	}

#endif /* INITRD */

	for (idx = 0; cfe_enummem(idx, mem_flags, &addr, &size, &type) != CFE_ERR_NOMORE;
	     idx++) {
		rd_flag = 0;
		if (type == CFE_MI_AVAILABLE) {
			/*
			 * See if this block contains (any portion of) the
			 * ramdisk
			 */
#ifdef CONFIG_BLK_DEV_INITRD
			if (initrd_start) {
				if ((initrd_pstart > addr) &&
				    (initrd_pstart < (addr + size))) {
					add_memory_region(addr,
					                  initrd_pstart - addr,
					                  BOOT_MEM_RAM);
					rd_flag = 1;
				}
				if ((initrd_pend > addr) &&
				    (initrd_pend < (addr + size))) {
					add_memory_region(initrd_pend,
						(addr + size) - initrd_pend,
						 BOOT_MEM_RAM);
					rd_flag = 1;
				}
			}
#endif
			if (!rd_flag) {
				if (addr > MAX_RAM_SIZE)
					continue;
				if (addr+size > MAX_RAM_SIZE)
					size = MAX_RAM_SIZE - (addr+size) + 1;
				/*
				 * memcpy/__copy_user prefetch, which
				 * will cause a bus error for
				 * KSEG/KUSEG addrs not backed by RAM.
				 * Hence, reserve some padding for the
				 * prefetch distance.
				 */
				if (size > 512)
					size -= 512;
				add_memory_region(addr, size, BOOT_MEM_RAM);
			}
			board_mem_region_addrs[board_mem_region_count] = addr;
			board_mem_region_sizes[board_mem_region_count] = size;
			board_mem_region_count++;
			if (board_mem_region_count ==
			    SIBYTE_MAX_MEM_REGIONS) {
				/*
				 * Too many regions.  Need to configure more
				 */
				while(1);
			}
		}
	}
#ifdef CONFIG_BLK_DEV_INITRD
	if (initrd_start) {
		add_memory_region(initrd_pstart, initrd_pend - initrd_pstart,
				  BOOT_MEM_RESERVED);
	}
#endif
}

#ifdef CONFIG_BLK_DEV_INITRD
static int __init initrd_setup(char *str)
{
	char rdarg[64];
	int idx;

	/* Make a copy of the initrd argument so we can smash it up here */
	for (idx = 0; idx < sizeof(rdarg)-1; idx++) {
		if (!str[idx] || (str[idx] == ' ')) break;
		rdarg[idx] = str[idx];
	}

	rdarg[idx] = 0;
	str = rdarg;

	/*
	 *Initrd location comes in the form "<hex size of ramdisk in bytes>@<location in memory>"
	 *  e.g. initrd=3abfd@80010000.  This is set up by the loader.
	 */
	char *tmp, *endptr;
	unsigned long initrd_size;
	for (tmp = str; *tmp != '@'; tmp++) {
		if (!*tmp) {
			goto fail;
		}
	}
	*tmp = 0;
	tmp++;
	if (!*tmp) {
		goto fail;
	}
	initrd_size = simple_strtoul(str, &endptr, 16);
	if (*endptr) {
		*(tmp-1) = '@';
		goto fail;
	}
	*(tmp-1) = '@';
	initrd_start = simple_strtoul(tmp, &endptr, 16);
	if (*endptr) {
		goto fail;
	}
	initrd_end = initrd_start + initrd_size;
	prom_printf("Found initrd of %lx@%lx\n", initrd_size, initrd_start);
	return 1;
 fail:
	prom_printf("Bad initrd argument.  Disabling initrd\n");
	initrd_start = 0;
	initrd_end = 0;
	return 1;
}

#endif

/*
 * prom_init is called just after the cpu type is determined, from init_arch()
 */
__init int prom_init(int argc, char **argv, char **envp, int *prom_vec)
{
	uint64_t cfe_ept, cfe_handle;
	unsigned int cfe_eptseal;
#ifdef CONFIG_KGDB
	char *arg;
#endif

	_machine_restart   = (void (*)(char *))cfe_linux_exit;
	_machine_halt      = cfe_linux_exit;
	_machine_power_off = cfe_linux_exit;

	/*
	 * Check if a loader was used; if NOT, the 4 arguments are
	 * what CFE gives us (handle, 0, EPT and EPTSEAL)
	 */
	if (argc < 0) {
		cfe_handle = (uint64_t)(long)argc;
		cfe_ept = (long)envp;
		cfe_eptseal = (uint32_t)(unsigned long)prom_vec;
	} else {
		if ((int32_t)(long)prom_vec < 0) {
			/*
			 * Old loader; all it gives us is the handle,
			 * so use the "known" entrypoint and assume
			 * the seal.
			 */
			cfe_handle = (uint64_t)(long)prom_vec;
			cfe_ept = (uint64_t)((int32_t)0x9fc00500);
			cfe_eptseal = CFE_EPTSEAL;
		} else {
			/*
			 * Newer loaders bundle the handle/ept/eptseal
			 * Note: prom_vec is in the loader's useg
			 * which is still alive in the TLB.
			 */
			cfe_handle = (uint64_t)((int32_t *)prom_vec)[0];
			cfe_ept = (uint64_t)((int32_t *)prom_vec)[2];
			cfe_eptseal = (unsigned int)((uint32_t *)prom_vec)[3];
		}
	}
	if (cfe_eptseal != CFE_EPTSEAL) {
		/* too early for panic to do any good */
		prom_printf("CFE's entrypoint seal doesn't match. Spinning.");
		while (1) ;
	}
	cfe_init(cfe_handle, cfe_ept);
	/* 
	 * Get the handle for (at least) prom_putchar, possibly for
	 * boot console
	 */
	cfe_cons_handle = cfe_getstdhandle(CFE_STDHANDLE_CONSOLE);
	if (cfe_getenv("LINUX_CMDLINE", arcs_cmdline, CL_SIZE) < 0) {
		if (argc < 0) {
			/*
			 * It's OK for direct boot to not provide a
			 *  command line
			 */
			strcpy(arcs_cmdline, "root=/dev/ram0 ");
		} else {
			/* The loader should have set the command line */
			/* too early for panic to do any good */
			prom_printf("LINUX_CMDLINE not defined in cfe.");
			while (1) ;
		}
	}

#ifdef CONFIG_KGDB
	if ((arg = strstr(arcs_cmdline,"kgdb=duart")) != NULL)
		kgdb_port = (arg[10] == '0') ? 0 : 1;
	else
		kgdb_port = 1;
#endif

#ifdef SIBYTE_DEFAULT_CONSOLE
	/* Force default console from board header (allowing override) */
	if (!strstr(arcs_cmdline,"console=")) {
		strcat(arcs_cmdline, "console=" SIBYTE_DEFAULT_CONSOLE);
	}
#endif

#ifdef CONFIG_BLK_DEV_INITRD
	{
		char *ptr;
		/* Need to find out early whether we've got an initrd.  So scan
		   the list looking now */
		for (ptr = arcs_cmdline; *ptr; ptr++) {
			while (*ptr == ' ') {
				ptr++;
			}
			if (!strncmp(ptr, "initrd=", 7)) {
				initrd_setup(ptr+7);
				break;
			} else {
				while (*ptr && (*ptr != ' ')) {
					ptr++;
				}
			}
		}
	}
#endif /* CONFIG_BLK_DEV_INITRD */

	/* Not sure this is needed, but it's the safe way. */
	arcs_cmdline[CL_SIZE-1] = 0;

	mips_machgroup = MACH_GROUP_SIBYTE;
	prom_meminit();

	return 0;
}

void prom_free_prom_memory(void)
{
	/* Not sure what I'm supposed to do here.  Nothing, I think */
}

int page_is_ram(unsigned long pagenr)
{
	phys_t addr = pagenr << PAGE_SHIFT;
	int i;
	for (i = 0; i < board_mem_region_count; i++) {
		if ((addr >= board_mem_region_addrs[i])
		    && (addr < (board_mem_region_addrs[i] + board_mem_region_sizes[i]))) {
			return 1;
		}
	}
	return 0;
}

void prom_putchar(char c)
{
	int ret;

	while ((ret = cfe_write(cfe_cons_handle, &c, 1)) == 0)
		;
}
