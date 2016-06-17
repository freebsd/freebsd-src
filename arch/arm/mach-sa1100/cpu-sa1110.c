/*
 *  linux/arch/arm/mach-sa1100/cpu-sa1110.c
 *
 *  Copyright (C) 2001 Russell King
 *
 *  $Id: cpu-sa1110.c,v 1.6 2001/10/22 11:53:47 rmk Exp $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Note: there are two erratas that apply to the SA1110 here:
 *  7 - SDRAM auto-power-up failure (rev A0)
 * 13 - Corruption of internal register reads/writes following
 *      SDRAM reads (rev A0, B0, B1)
 *
 * We ignore rev. A0 and B0 devices; I don't think they're worth supporting.
 */
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/init.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/system.h>

#undef DEBUG

extern unsigned int sa11x0_freq_to_ppcr(unsigned int khz);
extern unsigned int sa11x0_validatespeed(unsigned int khz);

struct sdram_params {
	u_char  rows;		/* bits				 */
	u_char  cas_latency;	/* cycles			 */
	u_char  tck;		/* clock cycle time (ns)	 */
	u_char  trcd;		/* activate to r/w (ns)		 */
	u_char  trp;		/* precharge to activate (ns)	 */
	u_char  twr;		/* write recovery time (ns)	 */
	u_short refresh;	/* refresh time for array (us)	 */
};

struct sdram_info {
	u_int	mdcnfg;
	u_int	mdrefr;
	u_int	mdcas[3];
};

static struct sdram_params tc59sm716_cl2_params __initdata = {
	.rows		=    12,
	.tck		=    10,
	.trcd		=    20,
	.trp		=    20,
	.twr		=    10,
	.refresh	= 64000,
	.cas_latency	=     2,
};

static struct sdram_params tc59sm716_cl3_params __initdata = {
	.rows		=    12,
	.tck		=     8,
	.trcd		=    20,
	.trp		=    20,
	.twr		=     8,
	.refresh	= 64000,
	.cas_latency	=     3,
};

static struct sdram_params samsung_k4s641632d_tc75 __initdata = {
	.rows		=    14,
	.tck		=     9,
	.trcd		=    27,
	.trp		=    20,
	.twr		=     9,
	.refresh	= 64000,
	.cas_latency	=     3,
};

static struct sdram_params sdram_params;

/*
 * Given a period in ns and frequency in khz, calculate the number of
 * cycles of frequency in period.  Note that we round up to the next
 * cycle, even if we are only slightly over.
 */
static inline u_int ns_to_cycles(u_int ns, u_int khz)
{
	return (ns * khz + 999999) / 1000000;
}

/*
 * Create the MDCAS register bit pattern.
 */
static inline void set_mdcas(u_int *mdcas, int delayed, u_int rcd)
{
	u_int shift;

	rcd = 2 * rcd - 1;
	shift = delayed + 1 + rcd;

	mdcas[0]  = (1 << rcd) - 1;
	mdcas[0] |= 0x55555555 << shift;
	mdcas[1]  = mdcas[2] = 0x55555555 << (shift & 1);
}

static void
sdram_calculate_timing(struct sdram_info *sd, u_int cpu_khz,
		       struct sdram_params *sdram)
{
	u_int mem_khz, sd_khz, trp, twr;

	mem_khz = cpu_khz / 2;
	sd_khz = mem_khz;

	/*
	 * If SDCLK would invalidate the SDRAM timings,
	 * run SDCLK at half speed.
	 *
	 * CPU steppings prior to B2 must either run the memory at
	 * half speed or use delayed read latching (errata 13).
	 */
	if ((ns_to_cycles(sdram->tck, sd_khz) > 1) ||
	    (CPU_REVISION < CPU_SA1110_B2 && sd_khz < 62000))
		sd_khz /= 2;

	sd->mdcnfg = MDCNFG & 0x007f007f;

	twr = ns_to_cycles(sdram->twr, mem_khz);

	/* trp should always be >1 */
	trp = ns_to_cycles(sdram->trp, mem_khz) - 1;
	if (trp < 1)
		trp = 1;

	sd->mdcnfg |= trp << 8;
	sd->mdcnfg |= trp << 24;
	sd->mdcnfg |= sdram->cas_latency << 12;
	sd->mdcnfg |= sdram->cas_latency << 28;
	sd->mdcnfg |= twr << 14;
	sd->mdcnfg |= twr << 30;

	sd->mdrefr = MDREFR & 0xffbffff0;
	sd->mdrefr |= 7;

	if (sd_khz != mem_khz)
		sd->mdrefr |= MDREFR_K1DB2;

	/* initial number of '1's in MDCAS + 1 */
	set_mdcas(sd->mdcas, sd_khz >= 62000, ns_to_cycles(sdram->trcd, mem_khz));

#ifdef DEBUG
	printk("MDCNFG: %08x MDREFR: %08x MDCAS0: %08x MDCAS1: %08x MDCAS2: %08x\n",
		sd->mdcnfg, sd->mdrefr, sd->mdcas[0], sd->mdcas[1], sd->mdcas[2]);
#endif
}

/*
 * Set the SDRAM refresh rate.
 */
static inline void sdram_set_refresh(u_int dri)
{
	MDREFR = (MDREFR & 0xffff000f) | (dri << 4);
	(void) MDREFR;
}

/*
 * Update the refresh period.  We do this such that we always refresh
 * the SDRAMs within their permissible period.  The refresh period is
 * always a multiple of the memory clock (fixed at cpu_clock / 2).
 *
 * FIXME: we don't currently take account of burst accesses here,
 * but neither do Intels DM nor Angel.
 */
static void
sdram_update_refresh(u_int cpu_khz, struct sdram_params *sdram)
{
	u_int ns_row = (sdram->refresh * 1000) >> sdram->rows;
	u_int dri = ns_to_cycles(ns_row, cpu_khz / 2) / 32;

#ifdef DEBUG
	mdelay(250);
	printk("new dri value = %d\n", dri);
#endif

	sdram_set_refresh(dri);
}

/*
 * Ok, set the CPU frequency.  Since we've done the validation
 * above, we can match for an exact frequency.  If we don't find
 * an exact match, we will to set the lowest frequency to be safe.
 */
static void sa1110_setspeed(unsigned int khz)
{
	struct sdram_params *sdram = &sdram_params;
	struct sdram_info sd;
	unsigned long flags;
	unsigned int ppcr, unused;

	ppcr = sa11x0_freq_to_ppcr(khz);
	sdram_calculate_timing(&sd, khz, sdram);

#if 0
	/*
	 * These values are wrong according to the SA1110 documentation
	 * and errata, but they seem to work.  Need to get a storage
	 * scope on to the SDRAM signals to work out why.
	 */
	if (khz < 147500) {
		sd.mdrefr |= MDREFR_K1DB2;
		sd.mdcas[0] = 0xaaaaaa7f;
	} else {
		sd.mdrefr &= ~MDREFR_K1DB2;
		sd.mdcas[0] = 0xaaaaaa9f;
	}
	sd.mdcas[1] = 0xaaaaaaaa;
	sd.mdcas[2] = 0xaaaaaaaa;
#endif
	/*
	 * The clock could be going away for some time.  Set the SDRAMs
	 * to refresh rapidly (every 64 memory clock cycles).  To get
	 * through the whole array, we need to wait 262144 mclk cycles.
	 * We wait 20ms to be safe.
	 */
	sdram_set_refresh(2);
	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_timeout(20 * HZ / 1000);

	/*
	 * Reprogram the DRAM timings with interrupts disabled, and
	 * ensure that we are doing this within a complete cache line.
	 * This means that we won't access SDRAM for the duration of
	 * the programming.
	 */
	local_irq_save(flags);
	asm("mcr p15, 0, %0, c7, c10, 4" : : "r" (0));
	udelay(10);
	__asm__ __volatile__("					\n\
		b	2f					\n\
		.align	5					\n\
1:		str	%3, [%1, #0]		@ MDCNFG	\n\
		str	%4, [%1, #28]		@ MDREFR	\n\
		str	%5, [%1, #4]		@ MDCAS0	\n\
		str	%6, [%1, #8]		@ MDCAS1	\n\
		str	%7, [%1, #12]		@ MDCAS2	\n\
		str	%8, [%2, #0]		@ PPCR		\n\
		ldr	%0, [%1, #0]				\n\
		b	3f					\n\
2:		b	1b					\n\
3:		nop						\n\
		nop"
		: "=&r" (unused)
		: "r" (&MDCNFG), "r" (&PPCR), "0" (sd.mdcnfg),
		  "r" (sd.mdrefr), "r" (sd.mdcas[0]),
		  "r" (sd.mdcas[1]), "r" (sd.mdcas[2]), "r" (ppcr));
	local_irq_restore(flags);

	/*
	 * Now, return the SDRAM refresh back to normal.
	 */
	sdram_update_refresh(khz, sdram);
}

static int __init sa1110_clk_init(void)
{
	struct sdram_params *sdram = NULL;

	if (machine_is_assabet())
		sdram = &tc59sm716_cl3_params;

	if (machine_is_pt_system3())
		sdram = &samsung_k4s641632d_tc75;

	if (sdram) {
		printk(KERN_DEBUG "SDRAM: tck: %d trcd: %d trp: %d"
			" twr: %d refresh: %d cas_latency: %d\n",
			sdram->tck, sdram->trcd, sdram->trp,
			sdram->twr, sdram->refresh, sdram->cas_latency);

		memcpy(&sdram_params, sdram, sizeof(sdram_params));

		sa1110_setspeed(cpufreq_get(0));
		cpufreq_setfunctions(sa11x0_validatespeed, sa1110_setspeed);
	}

	return 0;
}

__initcall(sa1110_clk_init);
