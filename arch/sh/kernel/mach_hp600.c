/*
 * linux/arch/sh/kernel/mach_hp600.c
 *
 * Copyright (C) 2000 Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Machine vector for the HP600
 */

#include <linux/init.h>

#include <asm/machvec.h>
#include <asm/rtc.h>
#include <asm/machvec_init.h>

#include <asm/io.h>
#include <asm/hd64461.h>
#include <asm/irq.h>

/*
 * The Machine Vector
 */

struct sh_machine_vector mv_hp620 __initmv = {
	mv_name:		"hp620",

	mv_nr_irqs:		HD64461_IRQBASE+HD64461_IRQ_NUM,

	mv_inb:			hd64461_inb,
	mv_inw:			hd64461_inw,
	mv_inl:			hd64461_inl,
	mv_outb:		hd64461_outb,
	mv_outw:		hd64461_outw,
	mv_outl:		hd64461_outl,

	mv_inb_p:		hd64461_inb_p,
	mv_inw_p:		hd64461_inw,
	mv_inl_p:		hd64461_inl,
	mv_outb_p:		hd64461_outb_p,
	mv_outw_p:		hd64461_outw,
	mv_outl_p:		hd64461_outl,

	mv_insb:		hd64461_insb,
	mv_insw:		hd64461_insw,
	mv_insl:		hd64461_insl,
	mv_outsb:		hd64461_outsb,
	mv_outsw:		hd64461_outsw,
	mv_outsl:		hd64461_outsl,

	mv_readb:		generic_readb,
	mv_readw:		generic_readw,
	mv_readl:		generic_readl,
	mv_writeb:		generic_writeb,
	mv_writew:		generic_writew,
	mv_writel:		generic_writel,

	mv_irq_demux:		hd64461_irq_demux,

	mv_rtc_gettimeofday:	sh_rtc_gettimeofday,
	mv_rtc_settimeofday:	sh_rtc_settimeofday,

	mv_hw_hp600:		1,
	mv_hw_hp620:		1,
	mv_hw_hd64461:		1,
};
ALIAS_MV(hp620)


struct sh_machine_vector mv_hp680 __initmv = {
	mv_name:		"hp680",

	mv_nr_irqs:		HD64461_IRQBASE+HD64461_IRQ_NUM,

	mv_inb:			hd64461_inb,
	mv_inw:			hd64461_inw,
	mv_inl:			hd64461_inl,
	mv_outb:		hd64461_outb,
	mv_outw:		hd64461_outw,
	mv_outl:		hd64461_outl,

	mv_inb_p:		hd64461_inb_p,
	mv_inw_p:		hd64461_inw,
	mv_inl_p:		hd64461_inl,
	mv_outb_p:		hd64461_outb_p,
	mv_outw_p:		hd64461_outw,
	mv_outl_p:		hd64461_outl,

	mv_insb:		hd64461_insb,
	mv_insw:		hd64461_insw,
	mv_insl:		hd64461_insl,
	mv_outsb:		hd64461_outsb,
	mv_outsw:		hd64461_outsw,
	mv_outsl:		hd64461_outsl,

	mv_readb:		generic_readb,
	mv_readw:		generic_readw,
	mv_readl:		generic_readl,
	mv_writeb:		generic_writeb,
	mv_writew:		generic_writew,
	mv_writel:		generic_writel,

	mv_irq_demux:		hd64461_irq_demux,

	mv_rtc_gettimeofday:	sh_rtc_gettimeofday,
	mv_rtc_settimeofday:	sh_rtc_settimeofday,

	mv_hw_hp600:		1,
	mv_hw_hp680:		1,
	mv_hw_hd64461:		1,
};
ALIAS_MV(hp680)


struct sh_machine_vector mv_hp690 __initmv = {
	mv_name:		"hp690",

	mv_nr_irqs:		HD64461_IRQBASE+HD64461_IRQ_NUM,

	mv_inb:			hd64461_inb,
	mv_inw:			hd64461_inw,
	mv_inl:			hd64461_inl,
	mv_outb:		hd64461_outb,
	mv_outw:		hd64461_outw,
	mv_outl:		hd64461_outl,

	mv_inb_p:		hd64461_inb_p,
	mv_inw_p:		hd64461_inw,
	mv_inl_p:		hd64461_inl,
	mv_outb_p:		hd64461_outb_p,
	mv_outw_p:		hd64461_outw,
	mv_outl_p:		hd64461_outl,

	mv_insb:		hd64461_insb,
	mv_insw:		hd64461_insw,
	mv_insl:		hd64461_insl,
	mv_outsb:		hd64461_outsb,
	mv_outsw:		hd64461_outsw,
	mv_outsl:		hd64461_outsl,

	mv_readb:		generic_readb,
	mv_readw:		generic_readw,
	mv_readl:		generic_readl,
	mv_writeb:		generic_writeb,
	mv_writew:		generic_writew,
	mv_writel:		generic_writel,

	mv_irq_demux:		hd64461_irq_demux,

	mv_rtc_gettimeofday:	sh_rtc_gettimeofday,
	mv_rtc_settimeofday:	sh_rtc_settimeofday,

	mv_hw_hp600:		1,
	mv_hw_hp690:		1,
	mv_hw_hd64461:		1,
};
ALIAS_MV(hp690)
