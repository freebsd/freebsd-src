/*
 * linux/arch/sh/kernel/mach_7751se.c
 *
 * Minor tweak of mach_se.c file to reference 7751se-specific items.
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

#include <asm/io_7751se.h>

void heartbeat_se(void);
void setup_7751se(void);
void init_7751se_IRQ(void);

/*
 * The Machine Vector
 */

struct sh_machine_vector mv_7751se __initmv = {
	mv_name:		"7751 SolutionEngine",

	mv_nr_irqs:		72,

	mv_inb:			sh7751se_inb,
	mv_inw:			sh7751se_inw,
	mv_inl:			sh7751se_inl,
	mv_outb:		sh7751se_outb,
	mv_outw:		sh7751se_outw,
	mv_outl:		sh7751se_outl,

	mv_inb_p:		sh7751se_inb_p,
	mv_inw_p:		sh7751se_inw,
	mv_inl_p:		sh7751se_inl,
	mv_outb_p:		sh7751se_outb_p,
	mv_outw_p:		sh7751se_outw,
	mv_outl_p:		sh7751se_outl,

	mv_insb:		sh7751se_insb,
	mv_insw:		sh7751se_insw,
	mv_insl:		sh7751se_insl,
	mv_outsb:		sh7751se_outsb,
	mv_outsw:		sh7751se_outsw,
	mv_outsl:		sh7751se_outsl,

	mv_readb:		sh7751se_readb,
	mv_readw:		sh7751se_readw,
	mv_readl:		sh7751se_readl,
	mv_writeb:		sh7751se_writeb,
	mv_writew:		sh7751se_writew,
	mv_writel:		sh7751se_writel,

	mv_ioremap:		generic_ioremap,
	mv_iounmap:		generic_iounmap,

	mv_isa_port2addr:	sh7751se_isa_port2addr,

	mv_init_arch:		setup_7751se,
	mv_init_irq:		init_7751se_IRQ,
#ifdef CONFIG_HEARTBEAT
	mv_heartbeat:		heartbeat_se,
#endif

	mv_rtc_gettimeofday:	sh_rtc_gettimeofday,
	mv_rtc_settimeofday:	sh_rtc_settimeofday,

	mv_hw_7751se:		1,
};
ALIAS_MV(7751se)
