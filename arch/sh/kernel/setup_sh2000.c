/*
 * linux/arch/sh/kernel/setup_sh2000.c
 *
 * Copyright (C) 2001  SUGIOKA Tochinobu
 *
 * SH-2000 Support.
 *
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/irq.h>

#include <asm/io.h>
#include <asm/io_generic.h>
#include <asm/machvec.h>
#include <asm/machvec_init.h>
#include <asm/rtc.h>

#define CF_CIS_BASE	0xb4200000

#define PORT_PECR	0xa4000108
#define PORT_PHCR	0xa400010E
#define	PORT_ICR1	0xa4000010
#define	PORT_IRR0	0xa4000004

/*
 * Initialize the board
 */
int __init setup_sh2000(void)
{
	/* XXX: RTC setting comes here */

	/* These should be done by BIOS/IPL ... */
	/* Enable nCE2A, nCE2B output */
	ctrl_outw(ctrl_inw(PORT_PECR) & ~0xf00, PORT_PECR);
	/* Enable the Compact Flash card, and set the level interrupt */
	ctrl_outw(0x0042, CF_CIS_BASE+0x0200);
	/* Enable interrupt */
	ctrl_outw(ctrl_inw(PORT_PHCR) & ~0x03f3, PORT_PHCR);
	ctrl_outw(1, PORT_ICR1);
	ctrl_outw(ctrl_inw(PORT_IRR0) & ~0xff3f, PORT_IRR0);
	printk(KERN_INFO "SH-2000 Setup...done\n");
	return 0;
}
/*
 * The Machine Vector
 */

struct sh_machine_vector mv_sh2000 __initmv = {
	mv_name:		"sh2000",

	mv_nr_irqs:		80,

	mv_inb:			generic_inb,
	mv_inw:			generic_inw,
	mv_inl:			generic_inl,
	mv_outb:		generic_outb,
	mv_outw:		generic_outw,
	mv_outl:		generic_outl,

	mv_inb_p:		generic_inb_p,
	mv_inw_p:		generic_inw_p,
	mv_inl_p:		generic_inl_p,
	mv_outb_p:		generic_outb_p,
	mv_outw_p:		generic_outw_p,
	mv_outl_p:		generic_outl_p,

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

	mv_init_arch:		setup_sh2000,

	mv_isa_port2addr:	sh2000_isa_port2addr,

	mv_ioremap:		generic_ioremap,
	mv_iounmap:		generic_iounmap,

	mv_rtc_gettimeofday:	sh_rtc_gettimeofday,
	mv_rtc_settimeofday:	sh_rtc_settimeofday,

	mv_hw_sh2000:		1,
};
ALIAS_MV(sh2000)
