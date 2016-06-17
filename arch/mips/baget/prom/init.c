/*
 * init.c: PROM library initialisation code.
 *
 * Copyright (C) 1998 Gleb Raiko & Vladimir Roganov
 */
#include <linux/init.h>
#include <asm/addrspace.h>
#include <asm/bootinfo.h>

char arcs_cmdline[CL_SIZE];

const char *get_system_type(void)
{
	/* Should probably return one of "BT23-201", "BT23-202" */
	return "Baget";
}

void __init prom_init(unsigned int mem_upper)
{
	mem_upper = PHYSADDR(mem_upper);

	mips_machgroup  = MACH_GROUP_UNKNOWN;
	mips_machtype   = MACH_UNKNOWN;
	arcs_cmdline[0] = 0;

	vac_memory_upper = mem_upper;

	add_memory_region(0, mem_upper, BOOT_MEM_RAM);
}

void prom_free_prom_memory (void)
{
}
