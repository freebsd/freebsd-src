/*
 * Copyright 2001 MontaVista Software Inc.
 * Author: jsun@mvista.com or jsun@junsun.net
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/bootmem.h>

#include <asm/addrspace.h>
#include <asm/bootinfo.h>

struct callvectors {
	int	(*open) (char*, int, int);
	int	(*close) (int);
	int	(*read) (int, void*, int);
	int	(*write) (int, void*, int);
	off_t	(*lseek) (int, off_t, int);
	int	(*printf) (const char*, ...);
	void	(*cacheflush) (void);
	char*	(*gets) (char*);
};

struct callvectors* debug_vectors;
char arcs_cmdline[CL_SIZE];

extern unsigned long gt64120_base;

const char *get_system_type(void)
{
	return "Momentum Ocelot";
}

/* [jsun@junsun.net] PMON passes arguments in C main() style */
void __init prom_init(int argc, char **arg, char** env, struct callvectors *cv)
{
	int i;
	uint32_t tmp;

	/* save the PROM vectors for debugging use */
	debug_vectors = cv;

	/* arg[0] is "g", the rest is boot parameters */
	arcs_cmdline[0] = '\0';
	for (i = 1; i < argc; i++) {
		if (strlen(arcs_cmdline) + strlen(arg[i] + 1)
		    >= sizeof(arcs_cmdline))
			break;
		strcat(arcs_cmdline, arg[i]);
		strcat(arcs_cmdline, " ");
	}

	mips_machgroup = MACH_GROUP_MOMENCO;
	mips_machtype = MACH_MOMENCO_OCELOT;

	while (*env) {
		if (strncmp("gtbase", *env, 6) == 0) {
			gt64120_base = simple_strtol(*env + strlen("gtbase="),
							NULL, 16);
			break;
		}
		*env++;
	}

	debug_vectors->printf("Booting Linux kernel...\n");

	/* All the boards have at least 64MiB. If there's more, we
	   detect and register it later */
	add_memory_region(0, 64 << 20, BOOT_MEM_RAM);
}

void __init prom_free_prom_memory(void)
{
}

void __init prom_fixup_mem_map(unsigned long start, unsigned long end)
{
}
