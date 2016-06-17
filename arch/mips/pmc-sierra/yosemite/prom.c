/*
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * Copyright (C) 2003 PMC-Sierra Inc.
 * Author: Manish Lachwani (lachwani@pmc-sierra.com)
 */

#include <linux/sched.h>
#include <linux/mm.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/processor.h>
#include <asm/reboot.h>
#include <asm/system.h>
#include <linux/delay.h>
#include <linux/smp.h>
#include <asm/bootinfo.h>

#include "setup.h"

/* Call Vectors */
struct callvectors {
        int     (*open) (char*, int, int);
        int     (*close) (int);
        int     (*read) (int, void*, int);
        int     (*write) (int, void*, int);
        off_t   (*lseek) (int, off_t, int);
        int     (*printf) (const char*, ...);
        void    (*cacheflush) (void);
        char*   (*gets) (char*);
};

struct callvectors* debug_vectors;
char arcs_cmdline[CL_SIZE];

extern unsigned long yosemite_base;
extern unsigned long cpu_clock;
unsigned char titan_ge_mac_addr_base[6];

const char *get_system_type(void)
{
        return "PMC-Sierra Yosemite";
}

static void prom_cpu0_exit(void)
{
	void	*nvram = YOSEMITE_NVRAM_BASE_ADDR;
	
	/* Ask the NVRAM/RTC/watchdog chip to assert reset in 1/16 second */
        writeb(0x84, nvram + 0xff7);

        /* wait for the watchdog to go off */
        mdelay(100+(1000/16));

        /* if the watchdog fails for some reason, let people know */
        printk(KERN_NOTICE "Watchdog reset failed\n");
}

/*
 * Reset the NVRAM over the local bus
 */
static void prom_exit(void)
{
#ifdef CONFIG_SMP
	if (smp_processor_id())
		/* CPU 1 */
		smp_call_function(prom_cpu0_exit, NULL, 1, 1);
#endif
	prom_cpu0_exit;
}

/*
 * Get the MAC address from the EEPROM using the I2C protocol
 */
void get_mac_address(char dest[6])
{
	/* Use the I2C command code in the i2c-yosemite */
}

/*
 * Halt the system 
 */
static void prom_halt(void)
{
	printk(KERN_NOTICE "\n** You can safely turn off the power\n");
	while (1)
                __asm__(".set\tmips3\n\t"
                        "wait\n\t"
                        ".set\tmips0");
}

/*
 * Init routine which accepts the variables from PMON
 */
__init prom_init(int argc, char **arg, char **env, struct callvectors *cv)
{
	int	i = 0;

	/* Callbacks for halt, restart */
	_machine_restart = (void (*)(char *))prom_exit;	
	_machine_halt = prom_halt;
	_machine_power_off = prom_halt;

#ifdef CONFIG_MIPS64

	/* Do nothing for the 64-bit for now. Just implement for the 32-bit */

#else /* CONFIG_MIPS64 */

	debug_vectors = cv;
	arcs_cmdline[0] = '\0';

	/* Get the boot parameters */
	for (i = 1; i < argc; i++) {
                if (strlen(arcs_cmdline) + strlen(arg[i] + 1) >= sizeof(arcs_cmdline))
                        break;

		strcat(arcs_cmdline, arg[i]);
		strcat(arcs_cmdline, " ");
	}

	while (*env) {
		if (strncmp("ocd_base", *env, strlen("ocd_base")) == 0) 
			yosemite_base = simple_strtol(*env + strlen("ocd_base="),
							NULL, 16);

		if (strncmp("cpuclock", *env, strlen("cpuclock")) == 0) 
			cpu_clock = simple_strtol(*env + strlen("cpuclock="),
							NULL, 10);
		
		env++;
	}
#endif /* CONFIG_MIPS64 */

	mips_machgroup = MACH_GROUP_TITAN;
	mips_machtype = MACH_TITAN_YOSEMITE;

	get_mac_address(titan_ge_mac_addr_base);

	debug_vectors->printf("Booting Linux kernel...\n");
}

void __init prom_free_prom_memory(void)
{
}

void __init prom_fixup_mem_map(unsigned long start, unsigned long end)
{
}

/*
 * SMP support
 */
int prom_setup_smp(void)
{
        int     num_cpus = 2;

        /*
         * We know that the RM9000 on the Jaguar ATX board has 2 cores. Hence, this
         * can be hardcoded for now.
         */
        return num_cpus;
}

int prom_boot_secondary(int cpu, unsigned long sp, unsigned long gp)
{
        /* Clear the semaphore */
        *(volatile u_int32_t *)(0xbb000a68) = 0x80000000;

        return 1;
}

void prom_init_secondary(void)
{
        clear_c0_config(CONF_CM_CMASK);
        set_c0_config(0x2);

        clear_c0_status(ST0_IM);
        set_c0_status(0x1ffff);
}

void prom_smp_finish(void)
{
}
	
