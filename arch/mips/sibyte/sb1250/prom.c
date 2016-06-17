/*
 * Copyright (C) 2000, 2001 Broadcom Corporation
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

extern char arcs_cmdline[];

#ifdef CONFIG_EMBEDDED_RAMDISK
/* These are symbols defined by the ramdisk linker script */
extern unsigned char __rd_start;
extern unsigned char __rd_end;
#endif

#define MAX_RAM_SIZE ((CONFIG_SIBYTE_STANDALONE_RAM_SIZE * 1024 * 1024) - 1)

static __init void prom_meminit(void)
{
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

	initrd_pstart = __pa(initrd_start);
	initrd_pend = __pa(initrd_end);
	if (initrd_start &&
	    ((initrd_pstart > MAX_RAM_SIZE)
	     || (initrd_pend > MAX_RAM_SIZE))) {
		panic("initrd out of addressable memory");
	}

	add_memory_region(0, initrd_pstart,
			  BOOT_MEM_RAM);
	add_memory_region(initrd_pstart, initrd_pend - initrd_pstart,
			  BOOT_MEM_RESERVED);
	add_memory_region(initrd_pend,
			  (CONFIG_SIBYTE_STANDALONE_RAM_SIZE * 1024 * 1024) - initrd_pend,
			  BOOT_MEM_RAM);
#else
	add_memory_region(0, CONFIG_SIBYTE_STANDALONE_RAM_SIZE * 1024 * 1024,
			  BOOT_MEM_RAM);
#endif
}

void prom_cpu0_exit(void *unused)
{
        while (1) ;
}

static void prom_linux_exit(void)
{
#ifdef CONFIG_SMP
	if (smp_processor_id()) {
		smp_call_function(prom_cpu0_exit,NULL,1,1);
	}
#endif
	while(1);
}

/*
 * prom_init is called just after the cpu type is determined, from init_arch()
 */
__init int prom_init(int argc, char **argv, char **envp, int *prom_vec)
{
	_machine_restart   = (void (*)(char *))prom_linux_exit;
	_machine_halt      = prom_linux_exit;
	_machine_power_off = prom_linux_exit;

	strcpy(arcs_cmdline, "root=/dev/ram0 ");

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
	return (addr < (CONFIG_SIBYTE_STANDALONE_RAM_SIZE * 1024 * 1024));
}

void prom_putchar(char c)
{
}
