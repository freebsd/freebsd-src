/*
 * BK Id: %F% %I% %G% %U% %#%
 *
 *    Copyright 2001 MontaVista Software Inc.
 *        <mlocke@mvista.com>
 *
 * 	Not much needed for the Embedded Planet 405gp board
 *
 *  	History: 11/09/2001 - armin
 *      added board_init to add in additional instuctions needed during platfrom_init
 *	cleaned up map_irq.
 *	 
 *	1/22/2002 - Armin
 *      converted pci to ocp
 *
 * Please read the COPYING file for all license details.
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <asm/system.h>
#include <asm/pci-bridge.h>
#include <asm/machdep.h>
#include <asm/todc.h>
#include <platforms/ibm_ocp.h>

#undef DEBUG
#ifdef DEBUG
#define DBG(x...) printk(x)
#else
#define DBG(x...)
#endif

u8 *ep405_bcsr;
u8 *ep405_nvram;

static struct {
	u8 cpld_xirq_select;
	int pci_idsel;
	int irq;
} ep405_devtable[] = {
#ifdef CONFIG_EP405PC
	{0x07, 0x0E, 25},		/* EP405PC: USB */
#endif
};
#define EP405_DEVTABLE_SIZE (sizeof(ep405_devtable)/sizeof(ep405_devtable[0]))

int __init
ppc405_map_irq(struct pci_dev *dev, unsigned char idsel, unsigned char pin)
{
	int i;

	/* AFAICT this is only called a few times during PCI setup, so
	   performance is not critical */
	for (i = 0; i < EP405_DEVTABLE_SIZE; i++) {
		if (idsel == ep405_devtable[i].pci_idsel)
			return ep405_devtable[i].irq;
	}
	return -1;
};

void __init
board_setup_arch(void)
{
	bd_t *bip = (bd_t *) __res;

	if (bip->bi_nvramsize == 512*1024) {
		/* FIXME: we should properly handle NVRTCs of different sizes */
		TODC_INIT(TODC_TYPE_DS1557, ep405_nvram, ep405_nvram, ep405_nvram, 8);
	}
}

#ifdef CONFIG_PCI
void __init
bios_fixup(struct pci_controller *hose, struct pcil0_regs *pcip)
{
	unsigned int bar_response, bar;
	/*
	 * Expected PCI mapping:
	 *
	 *  PLB addr             PCI memory addr
	 *  ---------------------       ---------------------
	 *  0000'0000 - 7fff'ffff <---  0000'0000 - 7fff'ffff
	 *  8000'0000 - Bfff'ffff --->  8000'0000 - Bfff'ffff
	 *
	 *  PLB addr             PCI io addr
	 *  ---------------------       ---------------------
	 *  e800'0000 - e800'ffff --->  0000'0000 - 0001'0000
	 *
	 */

	/* Disable region zero first */
	out_le32((void *) &(pcip->pmm[0].ma), 0x00000000);
	/* PLB starting addr, PCI: 0x80000000 */
	out_le32((void *) &(pcip->pmm[0].la), 0x80000000);
	/* PCI start addr, 0x80000000 */
	out_le32((void *) &(pcip->pmm[0].pcila), PPC405_PCI_MEM_BASE);
	/* 512MB range of PLB to PCI */
	out_le32((void *) &(pcip->pmm[0].pciha), 0x00000000);
	/* Enable no pre-fetch, enable region */
	out_le32((void *) &(pcip->pmm[0].ma), ((0xffffffff -
						(PPC405_PCI_UPPER_MEM -
						 PPC405_PCI_MEM_BASE)) | 0x01));

	/* Disable region one */
	out_le32((void *) &(pcip->pmm[1].ma), 0x00000000);
	out_le32((void *) &(pcip->pmm[1].la), 0x00000000);
	out_le32((void *) &(pcip->pmm[1].pcila), 0x00000000);
	out_le32((void *) &(pcip->pmm[1].pciha), 0x00000000);
	out_le32((void *) &(pcip->pmm[1].ma), 0x00000000);
	out_le32((void *) &(pcip->ptm1ms), 0x00000000);

	/* Disable region two */
	out_le32((void *) &(pcip->pmm[2].ma), 0x00000000);
	out_le32((void *) &(pcip->pmm[2].la), 0x00000000);
	out_le32((void *) &(pcip->pmm[2].pcila), 0x00000000);
	out_le32((void *) &(pcip->pmm[2].pciha), 0x00000000);
	out_le32((void *) &(pcip->pmm[2].ma), 0x00000000);
	out_le32((void *) &(pcip->ptm2ms), 0x00000000);

	/* Configure PTM (PCI->PLB) region 1 */
	out_le32((void *) &(pcip->ptm1la), 0x00000000); /* PLB base address */
	/* Disable PTM region 2 */
	out_le32((void *) &(pcip->ptm2ms), 0x00000000);

	/* Zero config bars */
	for (bar = PCI_BASE_ADDRESS_1; bar <= PCI_BASE_ADDRESS_2; bar += 4) {
		early_write_config_dword(hose, hose->first_busno,
					 PCI_FUNC(hose->first_busno), bar,
					 0x00000000);
		early_read_config_dword(hose, hose->first_busno,
					PCI_FUNC(hose->first_busno), bar,
					&bar_response);
		DBG("BUS %d, device %d, Function %d bar 0x%8.8x is 0x%8.8x\n",
		    hose->first_busno, PCI_SLOT(hose->first_busno),
		    PCI_FUNC(hose->first_busno), bar, bar_response);
	}
	/* end work arround */
}
#endif

void __init
board_io_mapping(void)
{
	bd_t *bip = (bd_t *) __res;

	ep405_bcsr = ioremap(EP405_BCSR_PADDR, EP405_BCSR_SIZE);

	if (bip->bi_nvramsize > 0) {
		ep405_nvram = ioremap(EP405_NVRAM_PADDR, bip->bi_nvramsize);
	}
}

void __init
board_setup_irq(void)
{
	int i;

	/* Workaround for a bug in the firmware it incorrectly sets
	   the IRQ polarities for XIRQ0 and XIRQ1 */
	mtdcr(DCRN_UIC_PR(DCRN_UIC0_BASE), 0xffffff80); /* set the polarity */
	mtdcr(DCRN_UIC_SR(DCRN_UIC0_BASE), 0x00000060); /* clear bogus interrupts */

	/* Activate the XIRQs from the CPLD */
	writeb(0xf0, ep405_bcsr+10);

	/* Set up IRQ routing */
	for (i = 0; i < EP405_DEVTABLE_SIZE; i++) {
		if ( (ep405_devtable[i].irq >= 25)
		     && (ep405_devtable[i].irq) <= 31) {
			writeb(ep405_devtable[i].cpld_xirq_select, ep405_bcsr+5);
			writeb(ep405_devtable[i].irq - 25, ep405_bcsr+6);
		}
	}
}

void __init
board_init(void)
{
	bd_t *bip = (bd_t *) __res;

#ifdef CONFIG_PPC_RTC
	/* FIXME: we should be able to access the NVRAM even if PPC_RTC is not configured */
	ppc_md.nvram_read_val = todc_direct_read_val;
	ppc_md.nvram_write_val = todc_direct_write_val;

	if (bip->bi_nvramsize == 512*1024) {
		ppc_md.time_init = todc_time_init;
		ppc_md.set_rtc_time = todc_set_rtc_time;
		ppc_md.get_rtc_time = todc_get_rtc_time;
	} else {
		printk("EP405: NVRTC size is not 512k (not a DS1557).  Not sure what to do with it\n");
	}

#endif
}
