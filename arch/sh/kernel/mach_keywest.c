/*
 * linux/arch/sh/kernel/mach_keywest.c
 *
 * Machine vector for HSA KeyWest Evaluiation board
 *
 * Copyright (C) 2001 Lineo, Japan
 *   
 */

#include <linux/config.h>
#include <linux/init.h>

#include <asm/machvec.h>
#include <asm/rtc.h>
#include <asm/machvec_init.h>
#include <asm/io.h>
#include <asm/io_keywest.h>
#include <asm/irq.h>

/*
 * The Machine Vector
 */
extern void heartbeat_keywest(void);
extern void setup_keywest(void);
extern void init_keywest_IRQ(void);

struct sh_machine_vector mv_keywest __initmv = {
	mv_name:		"Key West",
	mv_nr_irqs:		NR_IRQS,     // Defined in <asm/irq.h>
	mv_inb:			keywest_inb,
	mv_inw:			keywest_inw,
	mv_inl:			keywest_inl,
	mv_outb:		keywest_outb,
	mv_outw:		keywest_outw,
	mv_outl:		keywest_outl,

	mv_inb_p:		keywest_inb_p,
	mv_inw_p:		keywest_inw,
	mv_inl_p:		keywest_inl,
	mv_outb_p:		keywest_outb_p,
	mv_outw_p:		keywest_outw,
	mv_outl_p:		keywest_outl,

	mv_insb:		keywest_insb,
	mv_insw:		keywest_insw,
	mv_insl:		keywest_insl,
	mv_outsb:		keywest_outsb,
	mv_outsw:		keywest_outsw,
	mv_outsl:		keywest_outsl,

	mv_readb:		generic_readb,
	mv_readw:		generic_readw,
	mv_readl:		generic_readl,
	mv_writeb:		generic_writeb,
	mv_writew:		generic_writew,
	mv_writel:		generic_writel,

	mv_ioremap:		generic_ioremap,
	mv_iounmap:		generic_iounmap,

	mv_isa_port2addr:	keywest_isa_port2addr,
	mv_irq_demux:       keywest_irq_demux,

	mv_init_arch:		setup_keywest,
	mv_init_irq:		init_keywest_IRQ,
	mv_rtc_gettimeofday:	sh_rtc_gettimeofday,
	mv_rtc_settimeofday:	sh_rtc_settimeofday,

};
ALIAS_MV(keywest)
