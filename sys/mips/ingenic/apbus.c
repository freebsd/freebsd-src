/*	$NetBSD: apbus.c,v 1.14 2015/05/04 12:23:15 macallan Exp $ */

/*-
 * Copyright (c) 2014 Michael Lorenz
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
 
/* catch-all for on-chip peripherals */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: apbus.c,v 1.14 2015/05/04 12:23:15 macallan Exp $");

#include "locators.h"
#define	_MIPS_BUS_DMA_PRIVATE

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/device.h>
#include <sys/extent.h>
#include <sys/systm.h>

#include <mips/ingenic/ingenic_var.h>
#include <mips/ingenic/ingenic_regs.h>

#include "opt_ingenic.h"

static int apbus_match(device_t, cfdata_t, void *);
static void apbus_attach(device_t, device_t, void *);
static int apbus_print(void *, const char *);
static void apbus_bus_mem_init(bus_space_tag_t, void *);

CFATTACH_DECL_NEW(apbus, 0, apbus_match, apbus_attach, NULL, NULL);

static struct mips_bus_space	apbus_mbst;
bus_space_tag_t	apbus_memt = NULL;

struct mips_bus_dma_tag	apbus_dmat = {
	._bounce_alloc_hi = 0x10000000,
	._dmamap_ops = _BUS_DMAMAP_OPS_INITIALIZER,
	._dmamem_ops = _BUS_DMAMEM_OPS_INITIALIZER,
	._dmatag_ops = _BUS_DMATAG_OPS_INITIALIZER,
};

typedef struct apbus_dev {
	const char *name;	/* driver name */
	bus_addr_t addr;	/* base address */
	uint32_t irq;		/* interrupt */
	uint32_t clk0;		/* bit(s) in CLKGR0 */
	uint32_t clk1;		/* bit(s) in CLKGR1 */
} apbus_dev_t;

static const apbus_dev_t apbus_devs[] = {
	{ "dwctwo",	JZ_DWC2_BASE,   21, CLK_OTG0 | CLK_UHC, CLK_OTG1},
	{ "ohci",	JZ_OHCI_BASE,    5, CLK_UHC, 0},
	{ "ehci",	JZ_EHCI_BASE,   20, CLK_UHC, 0},
	{ "dme",	JZ_DME_BASE,    -1, 0, 0},
	{ "jzgpio",	JZ_GPIO_A_BASE, 17, 0, 0},
	{ "jzgpio",	JZ_GPIO_B_BASE, 16, 0, 0},
	{ "jzgpio",	JZ_GPIO_C_BASE, 15, 0, 0},
	{ "jzgpio",	JZ_GPIO_D_BASE, 14, 0, 0},
	{ "jzgpio",	JZ_GPIO_E_BASE, 13, 0, 0},
	{ "jzgpio",	JZ_GPIO_F_BASE, 12, 0, 0},
	{ "jziic",	JZ_SMB0_BASE,   60, CLK_SMB0, 0},
	{ "jziic",	JZ_SMB1_BASE,   59, CLK_SMB1, 0},
	{ "jziic",	JZ_SMB2_BASE,   58, CLK_SMB2, 0},
	{ "jziic",	JZ_SMB3_BASE,   57, 0, CLK_SMB3},
	{ "jziic",	JZ_SMB4_BASE,   56, 0, CLK_SMB4},
	{ "jzmmc",	JZ_MSC0_BASE,   37, CLK_MSC0, 0},
	{ "jzmmc",	JZ_MSC1_BASE,   36, CLK_MSC1, 0},
	{ "jzmmc",	JZ_MSC2_BASE,   35, CLK_MSC2, 0},
	{ "jzfb",	JZ_LCDC0_BASE,  31, CLK_LCD, CLK_HDMI},
	{ NULL,		-1,             -1, 0, 0}
};

void
apbus_init(void)
{
	static bool done = false;
	if (done)
		return;
	done = true;

	apbus_bus_mem_init(&apbus_mbst, NULL);
	apbus_memt = &apbus_mbst;
}

int
apbus_match(device_t parent, cfdata_t match, void *aux)
{
	struct mainbusdev {
		const char *md_name;
	} *aa = aux;
	if (strcmp(aa->md_name, "apbus") == 0) return 1;
	return 0;
}

void
apbus_attach(device_t parent, device_t self, void *aux)
{
	uint32_t reg, mpll, m, n, p, mclk, pclk, pdiv, cclk, cdiv;
	aprint_normal("\n");

	/* should have been called early on */
	apbus_init();

#ifdef INGENIC_DEBUG
	printf("core ctrl:   %08x\n", MFC0(12, 2));
	printf("core status: %08x\n", MFC0(12, 3));
	printf("REIM: %08x\n", MFC0(12, 4));
	printf("ID: %08x\n", MFC0(15, 1));
#endif
	/* assuming we're using MPLL */
	mpll = readreg(JZ_CPMPCR);
	m = (mpll & JZ_PLLM_M) >> JZ_PLLM_S;
	n = (mpll & JZ_PLLN_M) >> JZ_PLLN_S;
	p = (mpll & JZ_PLLP_M) >> JZ_PLLP_S;

	/* assuming 48MHz EXTCLK */
	mclk = (48000 * (m + 1) / (n + 1)) / (p + 1);

	reg = readreg(JZ_CPCCR);
	pdiv = ((reg & JZ_PDIV_M) >> JZ_PDIV_S) + 1;
	pclk = mclk / pdiv;
	cdiv = (reg & JZ_CDIV_M) + 1;
	cclk = mclk / cdiv;

	aprint_debug_dev(self, "mclk %d kHz\n", mclk);
	aprint_debug_dev(self, "pclk %d kHz\n", pclk);
	aprint_debug_dev(self, "CPU clock %d kHz\n", cclk);

	/* enable clocks */
	reg = readreg(JZ_CLKGR1);
	reg &= ~CLK_AHB_MON;	/* AHB_MON clock */
	writereg(JZ_CLKGR1, reg);

	/* wake up the USB part */
	reg = readreg(JZ_OPCR);
	reg |= OPCR_SPENDN0 | OPCR_SPENDN1;
	writereg(JZ_OPCR, reg);

	/* wire up GPIOs */
	/* iic0 */
	gpio_as_dev0(3, 30);
	gpio_as_dev0(3, 31);
	/* iic1 */
	gpio_as_dev0(4, 30);
	gpio_as_dev0(4, 31);
	/* iic2 */
	gpio_as_dev2(5, 16);
	gpio_as_dev2(5, 17);
	/* iic3 */
	gpio_as_dev1(3, 10);
	gpio_as_dev1(3, 11);
	/* iic4 */
	/* make sure these aren't SMB4 */
	gpio_as_dev3(4, 3);
	gpio_as_dev3(4, 4);
	/* these are supposed to be connected to the RTC */
	gpio_as_dev1(4, 12);
	gpio_as_dev1(4, 13);
	/* these can be DDC2 or SMB4, set them to DDC2 */
	gpio_as_dev0(5, 24);
	gpio_as_dev0(5, 25);

	/* MSC0 */
	gpio_as_dev1(0, 4);
	gpio_as_dev1(0, 5);
	gpio_as_dev1(0, 6);
	gpio_as_dev1(0, 7);
	gpio_as_dev1(0, 18);
	gpio_as_dev1(0, 19);
	gpio_as_dev1(0, 20);
	gpio_as_dev1(0, 21);
	gpio_as_dev1(0, 22);
	gpio_as_dev1(0, 23);
	gpio_as_dev1(0, 24);
	gpio_as_intr_level_low(5, 20);	/* card detect */

	/* MSC1, for wifi/bt */
	gpio_as_dev0(3, 20);
	gpio_as_dev0(3, 21);
	gpio_as_dev0(3, 22);
	gpio_as_dev0(3, 23);
	gpio_as_dev0(3, 24);
	gpio_as_dev0(3, 25);

	/* MSC2, on expansion header */
	gpio_as_dev0(1, 20);
	gpio_as_dev0(1, 21);
	gpio_as_dev0(1, 28);
	gpio_as_dev0(1, 29);
	gpio_as_dev0(1, 30);
	gpio_as_dev0(1, 31);

#ifdef INGENIC_DEBUG
	printf("JZ_CLKGR0 %08x\n", readreg(JZ_CLKGR0));
	printf("JZ_CLKGR1 %08x\n", readreg(JZ_CLKGR1));
	printf("JZ_SPCR0  %08x\n", readreg(JZ_SPCR0));
	printf("JZ_SPCR1  %08x\n", readreg(JZ_SPCR1));
	printf("JZ_SRBC   %08x\n", readreg(JZ_SRBC));
	printf("JZ_OPCR   %08x\n", readreg(JZ_OPCR));
	printf("JZ_UHCCDR %08x\n", readreg(JZ_UHCCDR));
#endif

	for (const apbus_dev_t *adv = apbus_devs; adv->name != NULL; adv++) {
		struct apbus_attach_args aa;
		aa.aa_name = adv->name;
		aa.aa_addr = adv->addr;
		aa.aa_irq  = adv->irq;
		aa.aa_dmat = &apbus_dmat;
		aa.aa_bst = apbus_memt;
		aa.aa_pclk = pclk;
		aa.aa_mclk = mclk;

		/* enable clocks as needed */
		if (adv->clk0 != 0) {
			reg = readreg(JZ_CLKGR0);
			reg &= ~adv->clk0;
			writereg(JZ_CLKGR0, reg);
		}

		if (adv->clk1 != 0) {
			reg = readreg(JZ_CLKGR1);
			reg &= ~adv->clk1;
			writereg(JZ_CLKGR1, reg);
		}
	
		(void) config_found_ia(self, "apbus", &aa, apbus_print);
	}
}

int
apbus_print(void *aux, const char *pnp)
{
	struct apbus_attach_args *aa = aux;

	if (pnp) {
		aprint_normal("%s at %s", aa->aa_name, pnp);
	}
	if (aa->aa_addr != -1)
		aprint_normal(" addr 0x%" PRIxBUSADDR, aa->aa_addr);
	if ((pnp == NULL) && (aa->aa_irq != -1))
		aprint_normal(" irq %d", aa->aa_irq);
	return (UNCONF);
}

#define CHIP	   		apbus
#define	CHIP_MEM		/* defined */
#define	CHIP_W1_BUS_START(v)	0x10000000UL
#define CHIP_W1_BUS_END(v)	0x20000000UL
#define	CHIP_W1_SYS_START(v)	0x10000000UL
#define	CHIP_W1_SYS_END(v)	0x20000000UL

#include <mips/mips/bus_space_alignstride_chipdep.c>
