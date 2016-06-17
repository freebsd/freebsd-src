/*
 * arch/ppc/simple/misc-44x.c
 *
 * Misc. bootloader code for IBM PPC 44x reference boards (Ebony, Ocotea)
 *
 * Matt Porter <mporter@mvista.com>
 * Eugene Surovegin <ebs@ebshome.net>
 * 
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#include <linux/config.h> 
#include <linux/types.h>
#include <asm/io.h>
#include <asm/processor.h>
#include <asm/ibm4xx.h>

extern struct bi_record *decompress_kernel(unsigned long load_addr,
	int num_words, unsigned long cksum);
extern unsigned long timebase_period_ns;

struct bi_record *
load_kernel(unsigned long load_addr, int num_words, unsigned long cksum)
{
	timebase_period_ns = 3;
	mtdcr(DCRN_MALCR(DCRN_MAL_BASE), MALCR_MMSR);		  /* reset MAL */
	while (mfdcr(DCRN_MALCR(DCRN_MAL_BASE)) & MALCR_MMSR) {}; /* wait for reset */
	*(volatile unsigned long *)PPC44x_EMAC0_MR0 = 0x20000000; /* reset EMAC */
	eieio();

	return decompress_kernel(load_addr, num_words, cksum);
}
