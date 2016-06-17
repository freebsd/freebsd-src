/*
 * linux/arch/sh/kernel/mach_adx.c
 *
 * Copyright (C) 2001 A&D Co., Ltd.
 *
 * This file may be copied or modified under the terms of the GNU
 * General Public License.  See linux/COPYING for more information.
 *
 * Machine vector for the A&D ADX Board
 */

#include <linux/config.h>
#include <linux/init.h>

#include <asm/machvec.h>
#include <asm/rtc.h>
#include <asm/machvec_init.h>
#include <asm/io_adx.h>

extern void setup_adx(void);
extern void init_adx_IRQ(void);

/*
 * The Machine Vector
 */

struct sh_machine_vector mv_adx __initmv = {
	mv_name:		"A&D_ADX",

	mv_nr_irqs:		48,

	mv_inb:			adx_inb,
	mv_inw:			adx_inw,
	mv_inl:			adx_inl,
	mv_outb:		adx_outb,
	mv_outw:		adx_outw,
	mv_outl:		adx_outl,

	mv_inb_p:		adx_inb_p,
	mv_inw_p:		adx_inw,
	mv_inl_p:		adx_inl,
	mv_outb_p:		adx_outb_p,
	mv_outw_p:		adx_outw,
	mv_outl_p:		adx_outl,

	mv_insb:		adx_insb,
	mv_insw:		adx_insw,
	mv_insl:		adx_insl,
	mv_outsb:		adx_outsb,
	mv_outsw:		adx_outsw,
	mv_outsl:		adx_outsl,

	mv_readb:		adx_readb,
	mv_readw:		adx_readw,
	mv_readl:		adx_readl,
	mv_writeb:		adx_writeb,
	mv_writew:		adx_writew,
	mv_writel:		adx_writel,

	mv_ioremap:		adx_ioremap,
	mv_iounmap:		adx_iounmap,

	mv_isa_port2addr:	adx_isa_port2addr,

	mv_init_arch:		setup_adx,
	mv_init_irq:		init_adx_IRQ,

	mv_rtc_gettimeofday:	sh_rtc_gettimeofday,
	mv_rtc_settimeofday:	sh_rtc_settimeofday,

	mv_hw_adx:		1,
};
ALIAS_MV(adx)
