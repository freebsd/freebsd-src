/*
 * linux/arch/sh/kernel/mach_microdev.c
 *
 * Copyright (C) 2003 Sean McGoogan (Sean.McGoogan@superh.com)
 *
 * Machine vector for the SuperH SH4-202 MicroDev board.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 */

#include <linux/config.h>
#include <linux/init.h>

#include <asm/machvec.h>
#include <asm/rtc.h>
#include <asm/machvec_init.h>

#include <asm/io_microdev.h>
#include <asm/irq_microdev.h>

void setup_microdev(void);

/*
 * The Machine Vector
 */

struct sh_machine_vector mv_sh4202_microdev __initmv = {
	mv_name:		"SH4-202 MicroDev",

	mv_nr_irqs:		72,		/* QQQ need to check this - use the MACRO */ 

	mv_inb:			microdev_inb,
	mv_inw:			microdev_inw,
	mv_inl:			microdev_inl,
	mv_outb:		microdev_outb,
	mv_outw:		microdev_outw,
	mv_outl:		microdev_outl,

	mv_inb_p:		microdev_inb_p,
	mv_inw_p:		microdev_inw_p,
	mv_inl_p:		microdev_inl_p,
	mv_outb_p:		microdev_outb_p,
	mv_outw_p:		microdev_outw_p,
	mv_outl_p:		microdev_outl_p,

	mv_insb:		microdev_insb,
	mv_insw:		microdev_insw,
	mv_insl:		microdev_insl,
	mv_outsb:		microdev_outsb,
	mv_outsw:		microdev_outsw,
	mv_outsl:		microdev_outsl,

	mv_readb:		generic_readb,
	mv_readw:		generic_readw,
	mv_readl:		generic_readl,
	mv_writeb:		generic_writeb,
	mv_writew:		generic_writew,
	mv_writel:		generic_writel,

	mv_ioremap:		generic_ioremap,
	mv_iounmap:		generic_iounmap,

	mv_isa_port2addr:	microdev_isa_port2addr,

	mv_init_arch:		setup_microdev,

	mv_init_irq:		init_microdev_irq,

	mv_rtc_gettimeofday:	sh_rtc_gettimeofday,
	mv_rtc_settimeofday:	sh_rtc_settimeofday,

	mv_hw_sh4202_microdev:	1,
};
ALIAS_MV(sh4202_microdev)

