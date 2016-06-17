/*
 * linux/arch/sh/kernel/pci-bigsur.c
 *
 * By Dustin McIntire (dustin@sensoria.com) (c)2001

 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * PCI initialization for the Hitachi Big Sur Evaluation Board
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pci.h>

#include <asm/io.h>
#include <asm/pci-sh7751.h>
#include <asm/bigsur.h>

#define PCI_REG(reg)        (SH7751_PCIREG_BASE+reg)

/*
 * Initialize the Big Sur PCI interface 
 * Setup hardware to be Central Funtion
 * Copy the BSR regs to the PCI interface
 * Setup PCI windows into local RAM
 */
int __init pcibios_init_platform(void) {
	u32 reg;
	u32 word;

	PCIDBG(1,"PCI: bigsur_pci_init called\n");
	/* Set the BCR's to enable PCI access */
	reg = inl(SH7751_BCR1);
	reg |= 0x80000;
	outl(reg, SH7751_BCR1);
	
	/* Setup the host hardware */
	if(inl(PCI_REG(SH7751_PCICONF0)) !=
	   (u32)((SH7751_DEVICE_ID <<16) | (SH7751_VENDOR_ID))) {
	   printk("PCI: Unkown PCI host bridge.\n");
	   return 0;
	}  
	printk("PCI: SH7751 PCI host bridge found.\n");
	
	/* Turn the clocks back on (not done in reset)*/
	outl(0, PCI_REG(SH7751_PCICLKR));
	/* Clear Powerdown IRQ's (not done in reset) */
	word = SH7751_PCIPINT_D3 | SH7751_PCIPINT_D0;
	outl(word, PCI_REG(SH7751_PCICLKR));

	/* toggle PCI reset pin */
	word = SH7751_PCICR_PREFIX | SH7751_PCICR_PRST;
	outl(word,PCI_REG(SH7751_PCICR));    
	/* Wait for a long time... not 1 sec. but long enough */
	mdelay(100);
	word = SH7751_PCICR_PREFIX;
	outl(word,PCI_REG(SH7751_PCICR)); 
	
    /* set the command/status bits to:
     * Wait Cycle Control + Parity Enable + Bus Master +
     * Mem space enable
     */
    word = SH7751_PCICONF1_WCC | SH7751_PCICONF1_PER | 
           SH7751_PCICONF1_BUM | SH7751_PCICONF1_MES;
	outl(word, PCI_REG(SH7751_PCICONF1));

	/* define this host as the host bridge */
	word = SH7751_PCI_HOST_BRIDGE << 24;
	outl(word, PCI_REG(SH7751_PCICONF2));

	/* Set IO and Mem windows to local address 
	 * Make PCI and local address the same for easy 1 to 1 mapping 
	 * Window0 = BIGSUR_LSR0_SIZE @ non-cached CS3 base = SDRAM
	 * Window1 = BIGSUR_LSR1_SIZE @ cached CS3 base = SDRAM 
	 */
	word = BIGSUR_LSR0_SIZE - 1;
	outl(word, PCI_REG(SH7751_PCILSR0));
	word = BIGSUR_LSR1_SIZE - 1;
	outl(word, PCI_REG(SH7751_PCILSR1));
	/* Set the values on window 0 PCI config registers */
	word = P2SEGADDR(SH7751_CS3_BASE_ADDR);
	outl(word, PCI_REG(SH7751_PCILAR0));
	outl(word, PCI_REG(SH7751_PCICONF5));
	/* Set the values on window 1 PCI config registers */
	word =  PHYSADDR(SH7751_CS3_BASE_ADDR);
	outl(word, PCI_REG(SH7751_PCILAR1));
	outl(word, PCI_REG(SH7751_PCICONF6));

	/* Set the local 16MB PCI memory space window to 
	 * the lowest PCI mapped address
	 */
	word = PCIBIOS_MIN_MEM & SH7751_PCIMBR_MASK;
	PCIDBG(2,"PCI: Setting upper bits of Memory window to 0x%x\n", word);
	outl(word , PCI_REG(SH7751_PCIMBR));

	/* Map IO space into PCI IO window
	 * The IO window is 64K-PCIBIOS_MIN_IO in size
	 * IO addresses will be translated to the 
	 * PCI IO window base address
	 */
	PCIDBG(3,"PCI: Mapping IO address 0x%x - 0x%x to base 0x%x\n", PCIBIOS_MIN_IO,
	    (64*1024), SH7751_PCI_IO_BASE+PCIBIOS_MIN_IO);
	bigsur_port_map(PCIBIOS_MIN_IO, (64*1024), SH7751_PCI_IO_BASE+PCIBIOS_MIN_IO,0);
	    
	/* Make sure the MSB's of IO window are set to access PCI space correctly */
	word = PCIBIOS_MIN_IO & SH7751_PCIIOBR_MASK;
	PCIDBG(2,"PCI: Setting upper bits of IO window to 0x%x\n", word);
	outl(word, PCI_REG(SH7751_PCIIOBR));
	
	/* Set PCI WCRx, BCRx's, copy from BSC locations */
	word = inl(SH7751_BCR1);
	/* check BCR for SDRAM in area 3 */
	if(((word >> 3) & 1) == 0) {
		printk("PCI: Area 3 is not configured for SDRAM. BCR1=0x%x\n", word);
		return 0;
	}
	outl(word, PCI_REG(SH7751_PCIBCR1));
	word = (u16)inw(SH7751_BCR2);
	/* check BCR2 for 32bit SDRAM interface*/
	if(((word >> 6) & 0x3) != 0x3) {
		printk("PCI: Area 3 is not 32 bit SDRAM. BCR2=0x%x\n", word);
		return 0;
	}
	outl(word, PCI_REG(SH7751_PCIBCR2));
	/* configure the wait control registers */
	word = inl(SH7751_WCR1);
	outl(word, PCI_REG(SH7751_PCIWCR1));
	word = inl(SH7751_WCR2);
	outl(word, PCI_REG(SH7751_PCIWCR2));
	word = inl(SH7751_WCR3);
	outl(word, PCI_REG(SH7751_PCIWCR3));
	word = inl(SH7751_MCR);
	outl(word, PCI_REG(SH7751_PCIMCR));

	/* NOTE: I'm ignoring the PCI error IRQs for now..
	 * TODO: add support for the internal error interrupts and
	 * DMA interrupts...
	 */
	 
	/* SH7751 init done, set central function init complete */
	word = SH7751_PCICR_PREFIX | SH7751_PCICR_CFIN;
	outl(word,PCI_REG(SH7751_PCICR)); 
	PCIDBG(2,"PCI: bigsur_pci_init finished\n");

	return 1;
}

int pcibios_map_platform_irq(u8 slot, u8 pin)
{
    /* The Big Sur can be used in a CPCI chassis, but the SH7751 PCI interface is on the
     * wrong end of the board so that it can also support a V320 CPI interface chip...
     * Therefor the IRQ mapping is somewhat use dependent... I'l assume a linear map for
     * now, i.e. INTA=slot0,pin0... INTD=slot3,pin0...
     */ 
    int irq = (slot + pin-1)%4 + BIGSUR_SH7751_PCI_IRQ_BASE;
    PCIDBG(2,"PCI: Mapping Big Sur IRQ for slot %d, pin %c to irq %d\n", slot, pin-1+'A', irq);
    return irq;
     
}
