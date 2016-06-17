/*
 * linux/arch/sh/kernel/mach_hs7729pci.c
 *
 * Copyright (C) 2000 Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Machine vector for the Hitachi Semiconductor ans Devices HS7729PCI
 */

#include <linux/config.h>
#include <linux/init.h>

#include <asm/machvec.h>
#include <asm/machvec_init.h>
#include <asm/rtc.h>

#include <asm/io_hs7729pci.h>

void setup_hs7729pci(void);
void init_hs7729pci_IRQ(void);


/*
 * The Machine Vector
 */

struct sh_machine_vector mv_hs7729pci __initmv = {
	mv_name:		"HS7729PCI",

#if defined(__SH4__)
	mv_nr_irqs:		48,
#elif defined(CONFIG_CPU_SUBTYPE_SH7708)
	mv_nr_irqs:		32,
#elif defined(CONFIG_CPU_SUBTYPE_SH7709)
	mv_nr_irqs:		61,
#endif

	mv_inb:			hs7729pci_inb,
	mv_inw:			hs7729pci_inw,
	mv_inl:			hs7729pci_inl,
	mv_outb:		hs7729pci_outb,
	mv_outw:		hs7729pci_outw,
	mv_outl:		hs7729pci_outl,

	mv_inb_p:		hs7729pci_inb_p,
	mv_inw_p:		hs7729pci_inw,
	mv_inl_p:		hs7729pci_inl,
	mv_outb_p:		hs7729pci_outb_p,
	mv_outw_p:		hs7729pci_outw,
	mv_outl_p:		hs7729pci_outl,

	mv_insb:		hs7729pci_insb,
	mv_insw:		hs7729pci_insw,
	mv_insl:		hs7729pci_insl,
	mv_outsb:		hs7729pci_outsb,
	mv_outsw:		hs7729pci_outsw,
	mv_outsl:		hs7729pci_outsl,

	mv_readb:		generic_readb,
	mv_readw:		generic_readw,
	mv_readl:		generic_readl,
	mv_writeb:		generic_writeb,
	mv_writew:		generic_writew,
	mv_writel:		generic_writel,

	mv_ioremap:		hs7729pci_ioremap,
	mv_iounmap:		hs7729pci_iounmap,

	mv_isa_port2addr:	hs7729pci_isa_port2addr,

	mv_init_arch:		setup_hs7729pci,
	mv_init_irq:		init_hs7729pci_IRQ,

	mv_rtc_gettimeofday:	sh_rtc_gettimeofday,
	mv_rtc_settimeofday:	sh_rtc_settimeofday,

	mv_hw_hs7729pci:	1,
};
ALIAS_MV(hs7729pci)
