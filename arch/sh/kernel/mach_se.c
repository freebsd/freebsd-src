/*
 * linux/arch/sh/kernel/mach_se.c
 *
 * Copyright (C) 2000 Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Machine vector for the Hitachi SolutionEngine
 */

#include <linux/config.h>
#include <linux/init.h>

#include <asm/machvec.h>
#include <asm/rtc.h>
#include <asm/machvec_init.h>

#include <asm/io_se.h>

void heartbeat_se(void);
void setup_se(void);
void init_se_IRQ(void);

/*
 * The Machine Vector
 */

struct sh_machine_vector mv_se __initmv = {
	mv_name:		"SolutionEngine",

#if defined(__SH4__)
	mv_nr_irqs:		48,
#elif defined(CONFIG_CPU_SUBTYPE_SH7708)
	mv_nr_irqs:		32,
#elif defined(CONFIG_CPU_SUBTYPE_SH7709)
	mv_nr_irqs:		61,
#endif

	mv_inb:			se_inb,
	mv_inw:			se_inw,
	mv_inl:			se_inl,
	mv_outb:		se_outb,
	mv_outw:		se_outw,
	mv_outl:		se_outl,

	mv_inb_p:		se_inb_p,
	mv_inw_p:		se_inw,
	mv_inl_p:		se_inl,
	mv_outb_p:		se_outb_p,
	mv_outw_p:		se_outw,
	mv_outl_p:		se_outl,

	mv_insb:		se_insb,
	mv_insw:		se_insw,
	mv_insl:		se_insl,
	mv_outsb:		se_outsb,
	mv_outsw:		se_outsw,
	mv_outsl:		se_outsl,

	mv_readb:		se_readb,
	mv_readw:		se_readw,
	mv_readl:		se_readl,
	mv_writeb:		se_writeb,
	mv_writew:		se_writew,
	mv_writel:		se_writel,

	mv_ioremap:		generic_ioremap,
	mv_iounmap:		generic_iounmap,

	mv_isa_port2addr:	se_isa_port2addr,

	mv_init_arch:		setup_se,
	mv_init_irq:		init_se_IRQ,
#ifdef CONFIG_HEARTBEAT
	mv_heartbeat:		heartbeat_se,
#endif

	mv_rtc_gettimeofday:	sh_rtc_gettimeofday,
	mv_rtc_settimeofday:	sh_rtc_settimeofday,

	mv_hw_se:		1,
};
ALIAS_MV(se)
