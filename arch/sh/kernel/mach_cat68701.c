/*
 * linux/arch/sh/kernel/mach_cat68701.c
 *
 * Copyright (C) 2000 Stuart Menefy (stuart.menefy@st.com)
 *               2001 Yutaro Ebihara (ebihara@si-linux.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Machine vector for the A-ONE corp. CAT-68701 SH7708 board
 */

#include <linux/config.h>
#include <linux/init.h>

#include <asm/machvec.h>
#include <asm/rtc.h>
#include <asm/machvec_init.h>
#include <asm/io_cat68701.h>

/*
 * The Machine Vector
 */

struct sh_machine_vector mv_cat68701 __initmv = {
	mv_name:		"CAT-68701",
	mv_nr_irqs:		32,
	mv_inb:			cat68701_inb,
	mv_inw:			cat68701_inw,
	mv_inl:			cat68701_inl,
	mv_outb:		cat68701_outb,
	mv_outw:		cat68701_outw,
	mv_outl:		cat68701_outl,

	mv_inb_p:		cat68701_inb_p,
	mv_inw_p:		cat68701_inw,
	mv_inl_p:		cat68701_inl,
	mv_outb_p:		cat68701_outb_p,
	mv_outw_p:		cat68701_outw,
	mv_outl_p:		cat68701_outl,

	mv_insb:		cat68701_insb,
	mv_insw:		cat68701_insw,
	mv_insl:		cat68701_insl,
	mv_outsb:		cat68701_outsb,
	mv_outsw:		cat68701_outsw,
	mv_outsl:		cat68701_outsl,

	mv_readb:		cat68701_readb,
	mv_readw:		cat68701_readw,
	mv_readl:		cat68701_readl,
	mv_writeb:		cat68701_writeb,
	mv_writew:		cat68701_writew,
	mv_writel:		cat68701_writel,

	mv_ioremap:		cat68701_ioremap,
	mv_iounmap:		cat68701_iounmap,

	mv_isa_port2addr:	cat68701_isa_port2addr,
	mv_irq_demux:           cat68701_irq_demux,

	mv_init_arch:		setup_cat68701,
	mv_init_irq:		init_cat68701_IRQ,
#ifdef CONFIG_HEARTBEAT
	mv_heartbeat:		heartbeat_cat68701,
#endif

	mv_rtc_gettimeofday:	sh_rtc_gettimeofday,
	mv_rtc_settimeofday:	sh_rtc_settimeofday,

};
ALIAS_MV(cat68701)
