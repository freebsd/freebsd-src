/*
 * linux/arch/sh/kernel/mach_ec3104.c
 *  EC3104 companion chip support
 *
 * Copyright (C) 2000 Philipp Rumpf <prumpf@tux.org>
 *
 */
/* EC3104 note:
 * This code was written without any documentation about the EC3104 chip.  While
 * I hope I got most of the basic functionality right, the register names I use
 * are most likely completely different from those in the chip documentation.
 *
 * If you have any further information about the EC3104, please tell me
 * (prumpf@tux.org).
 */

#include <linux/init.h>

#include <asm/machvec.h>
#include <asm/rtc.h>
#include <asm/machvec_init.h>

#include <asm/io.h>
#include <asm/irq.h>

/*
 * The Machine Vector
 */

struct sh_machine_vector mv_ec3104 __initmv = {
	mv_name:		"EC3104",

	mv_nr_irqs:		96,

	mv_inb:			ec3104_inb,
	mv_inw:			ec3104_inw,
	mv_inl:			ec3104_inl,
	mv_outb:		ec3104_outb,
	mv_outw:		ec3104_outw,
	mv_outl:		ec3104_outl,

	mv_inb_p:		generic_inb_p,
	mv_inw_p:		generic_inw,
	mv_inl_p:		generic_inl,
	mv_outb_p:		generic_outb_p,
	mv_outw_p:		generic_outw,
	mv_outl_p:		generic_outl,

	mv_insb:		generic_insb,
	mv_insw:		generic_insw,
	mv_insl:		generic_insl,
	mv_outsb:		generic_outsb,
	mv_outsw:		generic_outsw,
	mv_outsl:		generic_outsl,

	mv_readb:		generic_readb,
	mv_readw:		generic_readw,
	mv_readl:		generic_readl,
	mv_writeb:		generic_writeb,
	mv_writew:		generic_writew,
	mv_writel:		generic_writel,

	mv_irq_demux:		ec3104_irq_demux,

	mv_rtc_gettimeofday:	sh_rtc_gettimeofday,
	mv_rtc_settimeofday:	sh_rtc_settimeofday,
};

ALIAS_MV(ec3104)
