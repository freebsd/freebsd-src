/*
 * linux/arch/sh/kernel/mach_shmse.c
 *
 * Copyright (C) 2003 Takashi Kusuda <kusuda-takashi@hitachi-ul.co.jp> 
 * Based largely on mach_se.c
 *
 * Machine vector for SH-Mobile SolutionEngine
 */

#include <linux/config.h>
#include <linux/init.h>

#include <asm/machvec.h>
#include <asm/rtc.h>
#include <asm/machvec_init.h>

#include <asm/io_shmse.h>

void heartbeat_se(void);
void setup_shmse(void);
void init_shmse_IRQ(void);

/*
 * The Machine Vector
 */

struct sh_machine_vector mv_shmse __initmv = {
	mv_name:		"SH-Mobile SE",

	mv_nr_irqs:		109,

	mv_inb:			shmse_inb,
	mv_inw:			shmse_inw,
	mv_inl:			shmse_inl,
	mv_outb:		shmse_outb,
	mv_outw:		shmse_outw,
	mv_outl:		shmse_outl,

	mv_inb_p:		shmse_inb_p,
	mv_inw_p:		shmse_inw,
	mv_inl_p:		shmse_inl,
	mv_outb_p:		shmse_outb_p,
	mv_outw_p:		shmse_outw,
	mv_outl_p:		shmse_outl,

	mv_insb:		shmse_insb,
	mv_insw:		shmse_insw,
	mv_insl:		shmse_insl,
	mv_outsb:		shmse_outsb,
	mv_outsw:		shmse_outsw,
	mv_outsl:		shmse_outsl,

	mv_readb:		shmse_readb,
	mv_readw:		shmse_readw,
	mv_readl:		shmse_readl,
	mv_writeb:		shmse_writeb,
	mv_writew:		shmse_writew,
	mv_writel:		shmse_writel,

	mv_ioremap:		generic_ioremap,
	mv_iounmap:		generic_iounmap,

	mv_init_arch:		setup_shmse,
	mv_init_irq:		init_shmse_IRQ,
#ifdef CONFIG_HEARTBEAT
	mv_heartbeat:		heartbeat_se,
#endif

	mv_hw_shmse:		1,
};
ALIAS_MV(shmse)
