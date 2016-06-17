/*
 * linux/arch/sh/kernel/mach_snapgear.c
 *
 * Minor tweak of mach_se.c file to reference SnapGear items.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Machine vector for the Hitachi 7751 SolutionEngine
 */

#include <linux/config.h>
#include <linux/init.h>

#include <asm/machvec.h>
#include <asm/rtc.h>
#include <asm/machvec_init.h>

#include <asm/io_snapgear.h>

extern void setup_snapgear(void);
extern void init_snapgear_IRQ(void);

extern void snapgear_rtc_gettimeofday(struct timeval *tv);
extern int snapgear_rtc_settimeofday(const struct timeval *tv);

/*
 * The Machine Vector
 */

struct sh_machine_vector mv_snapgear __initmv = {
	mv_name:		"SnapGear",

	mv_nr_irqs:		72,

	mv_inb:			snapgear_inb,
	mv_inw:			snapgear_inw,
	mv_inl:			snapgear_inl,
	mv_outb:		snapgear_outb,
	mv_outw:		snapgear_outw,
	mv_outl:		snapgear_outl,

	mv_inb_p:		snapgear_inb_p,
	mv_inw_p:		snapgear_inw,
	mv_inl_p:		snapgear_inl,
	mv_outb_p:		snapgear_outb_p,
	mv_outw_p:		snapgear_outw,
	mv_outl_p:		snapgear_outl,

	mv_insb:		snapgear_insb,
	mv_insw:		snapgear_insw,
	mv_insl:		snapgear_insl,
	mv_outsb:		snapgear_outsb,
	mv_outsw:		snapgear_outsw,
	mv_outsl:		snapgear_outsl,

	mv_readb:		snapgear_readb,
	mv_readw:		snapgear_readw,
	mv_readl:		snapgear_readl,
	mv_writeb:		snapgear_writeb,
	mv_writew:		snapgear_writew,
	mv_writel:		snapgear_writel,

	mv_ioremap:		generic_ioremap,
	mv_iounmap:		generic_iounmap,

	mv_isa_port2addr:	snapgear_isa_port2addr,

	mv_init_arch:		setup_snapgear,
	mv_init_irq:		init_snapgear_IRQ,

#ifndef CONFIG_RTC
	mv_rtc_gettimeofday:	sh_rtc_gettimeofday,
	mv_rtc_settimeofday:	sh_rtc_settimeofday,
#else
	mv_rtc_gettimeofday:	snapgear_rtc_gettimeofday,
	mv_rtc_settimeofday:	snapgear_rtc_settimeofday,
#endif

	mv_hw_snapgear:		1,
};
ALIAS_MV(snapgear)
