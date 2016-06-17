/*
 * linux/arch/sh/kernel/mach_bigsur.c
 *
 * By Dustin McIntire (dustin@sensoria.com) (c)2001
 * Derived from mach_se.h, which bore the message:
 * Copyright (C) 2000 Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Machine vector for the Hitachi Big Sur Evaluation Board
 */

#include <linux/config.h>
#include <linux/init.h>

#include <asm/machvec.h>
#include <asm/rtc.h>
#include <asm/machvec_init.h>
#include <asm/io.h>
#include <asm/io_bigsur.h>
#include <asm/irq.h>

/*
 * The Machine Vector
 */
extern void heartbeat_bigsur(void);
extern void setup_bigsur(void);
extern void init_bigsur_IRQ(void);

struct sh_machine_vector mv_bigsur __initmv = {
	mv_name:		"Big Sur",
	mv_nr_irqs:		NR_IRQS,     // Defined in <asm/irq.h>
	mv_inb:			bigsur_inb,
	mv_inw:			bigsur_inw,
	mv_inl:			bigsur_inl,
	mv_outb:		bigsur_outb,
	mv_outw:		bigsur_outw,
	mv_outl:		bigsur_outl,

	mv_inb_p:		bigsur_inb_p,
	mv_inw_p:		bigsur_inw,
	mv_inl_p:		bigsur_inl,
	mv_outb_p:		bigsur_outb_p,
	mv_outw_p:		bigsur_outw,
	mv_outl_p:		bigsur_outl,

	mv_insb:		bigsur_insb,
	mv_insw:		bigsur_insw,
	mv_insl:		bigsur_insl,
	mv_outsb:		bigsur_outsb,
	mv_outsw:		bigsur_outsw,
	mv_outsl:		bigsur_outsl,

	mv_readb:		generic_readb,
	mv_readw:		generic_readw,
	mv_readl:		generic_readl,
	mv_writeb:		generic_writeb,
	mv_writew:		generic_writew,
	mv_writel:		generic_writel,

	mv_ioremap:		generic_ioremap,
	mv_iounmap:		generic_iounmap,

	mv_isa_port2addr:	bigsur_isa_port2addr,
	mv_irq_demux:       bigsur_irq_demux,

	mv_init_arch:		setup_bigsur,
	mv_init_irq:		init_bigsur_IRQ,
#ifdef CONFIG_HEARTBEAT
	mv_heartbeat:		heartbeat_bigsur,
#endif
	mv_rtc_gettimeofday:	sh_rtc_gettimeofday,
	mv_rtc_settimeofday:	sh_rtc_settimeofday,

};
ALIAS_MV(bigsur)
