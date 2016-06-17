/* 
 * Copyright (C) 2001 David J. Mckay (david.mckay@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.                            
 *
 * Support functions for the ST40 PCI hardware.
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <asm/pci.h>
#include <linux/irq.h>

#include "pci_st40.h"

static struct resource pci_io_space, pci_mem_space;

/* This is in P2 of course */
#define ST40PCI_BASE_ADDRESS     (0xb0000000)
#define ST40PCI_MEM_ADDRESS      (ST40PCI_BASE_ADDRESS+0x0)
#define ST40PCI_IO_ADDRESS       (ST40PCI_BASE_ADDRESS+0x06000000)
#define ST40PCI_REG_ADDRESS      (ST40PCI_BASE_ADDRESS+0x07000000)

#define ST40PCI_REG(x) (ST40PCI_REG_ADDRESS+(ST40PCI_##x))
#define ST40PCI_REG_INDEXED(reg, index) 				\
	(ST40PCI_REG(reg##0) +					\
	  ((ST40PCI_REG(reg##1) - ST40PCI_REG(reg##0))*index))

#define ST40PCI_WRITE(reg,val) writel((val),ST40PCI_REG(reg));udelay(2);
#define ST40PCI_WRITE_SHORT(reg,val) writew((val),ST40PCI_REG(reg));udelay(2);
#define ST40PCI_WRITE_BYTE(reg,val) writeb((val),ST40PCI_REG(reg));udelay(2);
#define ST40PCI_WRITE_INDEXED(reg, index, val)				\
	 writel((val), ST40PCI_REG_INDEXED(reg, index)); udelay(2);

#define ST40PCI_READ(reg) readl(ST40PCI_REG(reg))
#define ST40PCI_READ_SHORT(reg) readw(ST40PCI_REG(reg))
#define ST40PCI_READ_BYTE(reg) readb(ST40PCI_REG(reg))
#define ST40PCI_READ_INDEXED(reg, index) readl(ST40PCI_REG_INDEXED(reg, index))

#define ST40PCI_SERR_IRQ        64
#define ST40PCI_ERR_IRQ        65
#define ST40PCI_AD_INT			66
#define ST40PCI_PWR_DWN_INT		67

#define PLLPCICR (0xbb040000+0x10)	// CLKGENA.PLL2CR

/* From ST's include/asm-sh/st40_clock.h */
/* Macros to extract PLL params */
#define PLL_MDIV(reg)  ( ((unsigned)reg) & 0xff )
#define PLL_NDIV(reg) ( (((unsigned)reg)>>8) & 0xff )
#define PLL_PDIV(reg) ( (((unsigned)reg)>>16) & 0x7 )
#define PLL_SETUP(reg) ( (((unsigned)reg)>>19) & 0x1ff )

/*
 * The pcibios_map_platform_irq function is defined in the appropraite
 * board specific code and referenced here
 */
extern int __init pcibios_map_platform_irq(struct pci_dev *dev, u8 slot, u8 pin);

static void __init pcibios_assign_resources(void);

static __init void SetPCIPLL(void)
{
	{
		/* Lets play with the PLL values */
		unsigned long pll1cr1;
		unsigned long mdiv, ndiv, pdiv;
		unsigned long muxcr;
		unsigned int muxcr_ratios[4] = { 8, 16, 21, 1 };
		unsigned int freq;

#define CLKGENA            0xbb040000
#define CLKGENA_PLL2_MUXCR CLKGENA + 0x48
		pll1cr1 = ctrl_inl(PLLPCICR);
		printk("PLL1CR1 %08x\n", pll1cr1);
		mdiv = PLL_MDIV(pll1cr1);
		ndiv = PLL_NDIV(pll1cr1);
		pdiv = PLL_PDIV(pll1cr1);
		printk("mdiv %02x ndiv %02x pdiv %02x\n", mdiv, ndiv, pdiv);
		freq = ((2*27*ndiv)/mdiv) / (1 << pdiv);
		printk("PLL freq %dMHz\n", freq);
		muxcr = ctrl_inl(CLKGENA_PLL2_MUXCR);
		printk("PCI freq %dMhz\n", freq / muxcr_ratios[muxcr & 3]);
	}
}


struct pci_err {
  unsigned mask;
  const char *error_string;
};


static struct pci_err int_error[]={
  { INT_MNLTDIM,"MNLTDIM: Master non-lock transfer"},
  { INT_TTADI,  "TTADI: Illegal byte enable in I/O transfer"},  
  { INT_TMTO,   "TMTO: Target memory read/write timeout"},  
  { INT_MDEI,   "MDEI: Master function disable error"},
  { INT_APEDI,  "APEDI: Address parity error"},
  { INT_SDI,    "SDI: SERR detected"},
  { INT_DPEITW, "DPEITW: Data parity error target write"},  
  { INT_PEDITR, "PEDITR: PERR detected"},
  { INT_TADIM,  "TADIM: Target abort detected"},
  { INT_MADIM,  "MADIM: Master abort detected"},
  { INT_MWPDI,  "MWPDI: PERR from target at data write"},
  { INT_MRDPEI, "MRDPEI: Master read data parity error"}
};
#define NUM_PCI_INT_ERRS (sizeof(int_error)/sizeof(struct pci_err))

static struct pci_err aint_error[]={
  { AINT_MBI,   "MBI: Master broken"},
  { AINT_TBTOI, "TBTOI: Target bus timeout"},
  { AINT_MBTOI, "MBTOI: Master bus timeout"},
  { AINT_TAI,   "TAI: Target abort"},
  { AINT_MAI,   "MAI: Master abort"},
  { AINT_RDPEI, "RDPEI: Read data parity"},
  { AINT_WDPE,  "WDPE: Write data parity"}
};


#define NUM_PCI_AINT_ERRS (sizeof(aint_error)/sizeof(struct pci_err))

static void print_pci_errors(unsigned reg,struct pci_err *error,int num_errors)
{
  int i;

  for(i=0;i<num_errors;i++) {
    if(reg & error[i].mask) {
      printk("%s\n",error[i].error_string);
    }
  }

}


static char * pci_commands[16]={
	"Int Ack",
	"Special Cycle",
	"I/O Read",
	"I/O Write",
	"Reserved",
	"Reserved",
	"Memory Read",
	"Memory Write",
	"Reserved",
	"Reserved",
	"Configuration Read",
	"Configuration Write",
	"Memory Read Multiple",
	"Dual Address Cycle",
	"Memory Read Line",
	"Memory Write-and-Invalidate"
};


static void st40_pci_irq(int irq, void *dev_instance, struct pt_regs *regs)
{

	unsigned pci_int, pci_air, pci_cir, pci_aint;
	static int count=0;


	pci_int = ST40PCI_READ(INT);pci_aint = ST40PCI_READ(AINT);
	pci_cir = ST40PCI_READ(CIR);pci_air = ST40PCI_READ(AIR);

	/* Reset state to stop multiple interrupts */
        ST40PCI_WRITE(INT, ~0); ST40PCI_WRITE(AINT, ~0); 


	if(++count>1) return;

	printk("** PCI ERROR **\n");

        if(pci_int) {
		printk("** INT register status\n");
		print_pci_errors(pci_int,int_error,NUM_PCI_INT_ERRS);
	}

        if(pci_aint) {
		printk("** AINT register status\n");
		print_pci_errors(pci_aint,aint_error,NUM_PCI_AINT_ERRS);
	}   
	
	printk("** Address and command info\n");

	printk("** Command  %s : Address 0x%x\n",
	       pci_commands[pci_cir&0xf],pci_air);

	if(pci_cir&CIR_PIOTEM) {
		printk("CIR_PIOTEM:PIO transfer error for master\n");
	}
        if(pci_cir&CIR_RWTET) {
		printk("CIR_RWTET:Read/Write transfer error for target\n");
	}
}


/* Rounds a number UP to the nearest power of two. Used for
 * sizing the PCI window.
 */
static u32 r2p2(u32 num)
{
	int i = 31;
	u32 tmp = num;

	if (num == 0)
		return 0;

	do {
		if (tmp & (1 << 31))
			break;
		i--;
		tmp <<= 1;
	} while (i >= 0);

	tmp = 1 << i;
	/* If the original number isn't a power of 2, round it up */
	if (tmp != num)
		tmp <<= 1;

	return tmp;
}

static void __init pci_fixup_ide_bases(struct pci_dev *d)
{
	int i;

	/*
	 * PCI IDE controllers use non-standard I/O port decoding, respect it.
	 */
	if ((d->class >> 8) != PCI_CLASS_STORAGE_IDE)
		return;
	printk("PCI: IDE base address fixup for %s\n", d->slot_name);
	for(i=0; i<4; i++) {
		struct resource *r = &d->resource[i];
		if ((r->start & ~0x80) == 0x374) {
			r->start |= 2;
			r->end = r->start;
		}
	}
}


static void __init pci_fixup_cache_line(struct pci_dev *d)
{
	pci_write_config_byte(d,PCI_CACHE_LINE_SIZE,0);
}


/* Add future fixups here... */
struct pci_fixup pcibios_fixups[] = {
	{ PCI_FIXUP_HEADER,	PCI_ANY_ID,	PCI_ANY_ID,	pci_fixup_ide_bases },
	{ PCI_FIXUP_FINAL,	PCI_ANY_ID,	PCI_ANY_ID,	pci_fixup_cache_line },
	{ 0 }
};

char * __init pcibios_setup(char *str)
{
	return str;
}

static void __init st40pci_init_resources(void)
{
	pci_io_space.start = PCIBIOS_MIN_IO;
	pci_io_space.end   = 64*1024 - PCIBIOS_MIN_IO - 1;
	pci_io_space.name = "ST40 PCI";
	pci_io_space.flags = IORESOURCE_IO;

	request_resource(&ioport_resource, &pci_io_space);

	pci_mem_space.start = PCIBIOS_MIN_MEM;
	pci_mem_space.end   = PCIBIOS_MIN_MEM + (96*1024*1024) -1;
	pci_mem_space.name = "ST40 PCI";
	pci_mem_space.flags = IORESOURCE_MEM;

	request_resource(&iomem_resource, &pci_mem_space);
}

int __init st40pci_init(unsigned memStart, unsigned memSize)
{
	u32 lsr0;

 	printk("PCI version register reads 0x%x\n",ST40PCI_READ(VCR_VERSION));

	SetPCIPLL();
	st40pci_init_resources();

	/* Initialises the ST40 pci subsystem, performing a reset, then programming
	 * up the address space decoders appropriately
	 */

	/* Should reset core here as well methink */

	ST40PCI_WRITE(CR, CR_LOCK_MASK | CR_SOFT_RESET);

	/* Loop while core resets */
	while (ST40PCI_READ(CR) & CR_SOFT_RESET);

	/* Switch off interrupts */
	ST40PCI_WRITE(INTM, 0);
	ST40PCI_WRITE(AINT, 0);

	/* Now, lets reset all the cards on the bus with extreme prejudice */
	ST40PCI_WRITE(CR, CR_LOCK_MASK | CR_RSTCTL);
	udelay(250);

	/* Set bus active, take it out of reset */
	ST40PCI_WRITE(CR, CR_LOCK_MASK | CR_BMAM | CR_CFINT | CR_PFCS | CR_PFE);

	/* The PCI spec says that no access must be made to the bus until 1 second
	 * after reset. This seem ludicrously long, but some delay is needed here
	 */
	mdelay(1000);


	/* Allow it to be a master */

	ST40PCI_WRITE_SHORT(CSR_CMD,
			    PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER |
			    PCI_COMMAND_IO);

	/* Accesse to the 0xb0000000 -> 0xb6000000 area will go through to 0x10000000 -> 0x16000000
	 * on the PCI bus. This allows a nice 1-1 bus to phys mapping.
	 */


	ST40PCI_WRITE(MBR, 0x10000000);
	/* Always set the max size 128M (actually, it is only 96MB wide) */
	ST40PCI_WRITE(MBMR, 0x07ff0000);

	/* I/O addresses are mapped at 0xb6000000 -> 0xb7000000. These are changed to 0, to 
	 * allow cards that have legacy io such as vga to function correctly. This gives a 
	 * maximum of 64K of io/space as only the bottom 16 bits of the address are copied 
	 * over to the bus  when the transaction is made. 64K of io space is more than enough
	 */
	ST40PCI_WRITE(IOBR, 0x0);
	/* Set up the 64K window */
	ST40PCI_WRITE(IOBMR, 0x0);

	/* Now we set up the mbars so the PCI bus can see the local memory */
	/* Expose a 256M window starting at PCI address 0... */
	ST40PCI_WRITE(CSR_MBAR0, 0);
	ST40PCI_WRITE(LSR0, 0x0fff0001);

	/* ... and set up the initial incomming window to expose all of RAM */
	pci_set_rbar_region(7, memStart, memStart, memSize);

	/* Maximise timeout values */
	ST40PCI_WRITE_BYTE(CSR_TRDY, 0xff);
	ST40PCI_WRITE_BYTE(CSR_RETRY, 0xff);
	ST40PCI_WRITE_BYTE(CSR_MIT, 0xff);

	ST40PCI_WRITE_BYTE(PERF,PERF_MASTER_WRITE_POSTING);

	return 1;
}

#define SET_CONFIG_BITS(bus,devfn,where)\
  (((bus) << 16) | ((devfn) << 8) | ((where) & ~3) | (bus!=0))

#define CONFIG_CMD(dev, where) SET_CONFIG_BITS((dev)->bus->number,(dev)->devfn,where)


static int CheckForMasterAbort(void)
{
	if (ST40PCI_READ(INT) & INT_MADIM) {
		/* Should we clear config space version as well ??? */
		ST40PCI_WRITE(INT, INT_MADIM);
		ST40PCI_WRITE_SHORT(CSR_STATUS, PCI_RMA);
		return 1;
	}

	return 0;
}

/* Write to config register */
static int st40pci_read_config_byte(struct pci_dev *dev, int where,
				    u8 * val)
{
	CheckForMasterAbort();

	ST40PCI_WRITE(PAR, CONFIG_CMD(dev, where));

	*val = ST40PCI_READ_BYTE(PDR + (where & 3));

	if (CheckForMasterAbort())
		*val = 0xff;


	return PCIBIOS_SUCCESSFUL;
}

static int st40pci_read_config_word(struct pci_dev *dev, int where,
				    u16 * val)
{
	CheckForMasterAbort();

	ST40PCI_WRITE(PAR, CONFIG_CMD(dev, where));

	*val = ST40PCI_READ_SHORT(PDR + (where & 2));

	if (CheckForMasterAbort())
		*val = 0xffff;

	return PCIBIOS_SUCCESSFUL;
}


static int st40pci_read_config_dword(struct pci_dev *dev, int where,
				     u32 * val)
{
	CheckForMasterAbort();

	ST40PCI_WRITE(PAR, CONFIG_CMD(dev, where));

	*val = ST40PCI_READ(PDR);

	if (CheckForMasterAbort()) {
		*val = 0xffffffff;
	}

	return PCIBIOS_SUCCESSFUL;
}

static int st40pci_write_config_byte(struct pci_dev *dev, int where,
				     u8 val)
{
	ST40PCI_WRITE(PAR, CONFIG_CMD(dev, where));

	ST40PCI_WRITE_BYTE(PDR + (where & 3), val);

	CheckForMasterAbort();

	return PCIBIOS_SUCCESSFUL;
}


static int st40pci_write_config_word(struct pci_dev *dev, int where,
				     u16 val)
{
	ST40PCI_WRITE(PAR, CONFIG_CMD(dev, where));

	ST40PCI_WRITE_SHORT(PDR + (where & 2), val);

	CheckForMasterAbort();

	return PCIBIOS_SUCCESSFUL;
}

static int st40pci_write_config_dword(struct pci_dev *dev, int where,
				      u32 val)
{
	ST40PCI_WRITE(PAR, CONFIG_CMD(dev, where));

	ST40PCI_WRITE(PDR, val);

	CheckForMasterAbort();

	return PCIBIOS_SUCCESSFUL;
}

static struct pci_ops pci_config_ops = {
	st40pci_read_config_byte,
	st40pci_read_config_word,
	st40pci_read_config_dword,
	st40pci_write_config_byte,
	st40pci_write_config_word,
	st40pci_write_config_dword
};


/* Everything hangs off this */
static struct pci_bus *pci_root_bus;


static u8 __init no_swizzle(struct pci_dev *dev, u8 * pin)
{
	printk("swizzle for dev %d on bus %d slot %d pin is %d\n",
	       dev->devfn,dev->bus->number, PCI_SLOT(dev->devfn),*pin);
	return PCI_SLOT(dev->devfn);
}

static inline u8 bridge_swizzle(u8 pin, u8 slot) 
{
	return (((pin-1) + slot) % 4) + 1;
}

u8 __init common_swizzle(struct pci_dev *dev, u8 *pinp)
{
	if (dev->bus->number != 0) {
		u8 pin = *pinp;
		do {
			pin = bridge_swizzle(pin, PCI_SLOT(dev->devfn));
			/* Move up the chain of bridges. */
			dev = dev->bus->self;
		} while (dev->bus->self);
		*pinp = pin;

		/* The slot is the slot of the last bridge. */
	}

	return PCI_SLOT(dev->devfn);
}

void __init
pcibios_fixup_pbus_ranges(struct pci_bus *bus,
			  struct pbus_set_ranges_data *ranges)
{
}

void __init pcibios_init(void)
{
	extern unsigned long memory_start, memory_end;

	if (sh_mv.mv_init_pci != NULL) {
		sh_mv.mv_init_pci();
	}

	/* The pci subsytem needs to know where memory is and how much 
	 * of it there is. I've simply made these globals. A better mechanism
	 * is probably needed.
	 */
	st40pci_init(PHYSADDR(memory_start),
		     PHYSADDR(memory_end) - PHYSADDR(memory_start));

	if (request_irq(ST40PCI_SERR_IRQ, st40_pci_irq, 
                        SA_INTERRUPT, "st40pci", NULL)) {
		printk(KERN_ERR "st40pci: Cannot hook interrupt\n");
		return;
	}

	if (request_irq(ST40PCI_ERR_IRQ, st40_pci_irq, 
                        SA_INTERRUPT, "st40pci", NULL)) {
		printk(KERN_ERR "st40pci: Cannot hook interrupt\n");
		return;
	}

	/* Reset state just in case any outstanding (usually SERR) */
        ST40PCI_WRITE(INT, ~0); ST40PCI_WRITE(AINT, ~0); 
	/* Enable the PCI interrupts on the device */
	ST40PCI_WRITE(INTM, ~0);
	ST40PCI_WRITE(AINT, ~0);

	/* Map the io address apprioately */
#ifdef CONFIG_HD64465
	hd64465_port_map(PCIBIOS_MIN_IO, (64 * 1024) - PCIBIOS_MIN_IO + 1,
			 ST40_IO_ADDR + PCIBIOS_MIN_IO, 0);
#endif

	/* ok, do the scan man */
	pci_root_bus = pci_scan_bus(0, &pci_config_ops, NULL);
	pci_assign_unassigned_resources();
	pci_fixup_irqs(no_swizzle, pcibios_map_platform_irq);

}

void __init
pcibios_fixup_resource(struct resource *res, struct resource *root)
{
        res->start += root->start;
        res->end += root->start;
}

void __init
pcibios_fixup_device_resources(struct pci_dev *dev, struct pci_bus *bus)
{
        /* Update device resources.  */
        int i;

        for (i = 0; i < PCI_NUM_RESOURCES; i++) {
                if (!dev->resource[i].start)
                        continue;
                if (dev->resource[i].flags & IORESOURCE_IO)
                        pcibios_fixup_resource(&dev->resource[i],
                                               &pci_io_space);
                else if (dev->resource[i].flags & IORESOURCE_MEM)
                        pcibios_fixup_resource(&dev->resource[i],
                                               &pci_mem_space);
        }
}

void __init pcibios_fixup_bus(struct pci_bus *bus)
{
	/* Propogate hose info into the subordinate devices.  */
	struct list_head *ln;
	struct pci_dev *dev = bus->self;

	if (!dev) {
		bus->resource[0] = &pci_io_space;
		bus->resource[1] = &pci_mem_space;
	}

	for (ln = bus->devices.next; ln != &bus->devices; ln = ln->next) {
		struct pci_dev *dev = pci_dev_b(ln);
		if ((dev->class >> 8) != PCI_CLASS_BRIDGE_PCI)
			pcibios_fixup_device_resources(dev, bus);
	}
}

static void __init pcibios_assign_resources(void)
{
	struct pci_dev *dev;
	int idx;
	struct resource *r;
	pci_for_each_dev(dev) {
		int class = dev->class >> 8;

		/* Don't touch classless devices and host bridges */
		if (!class || class == PCI_CLASS_BRIDGE_HOST)
			continue;

		for(idx=0; idx<6; idx++) {
			r = &dev->resource[idx];

			/*
			 *  Don't touch IDE controllers and I/O ports of video cards!
			 */
			if ((class == PCI_CLASS_STORAGE_IDE && idx < 4) ||
			    (class == PCI_CLASS_DISPLAY_VGA && (r->flags & IORESOURCE_IO)))
				continue;

			/*
			 *  We shall assign a new address to this resource, either because
			 *  the BIOS forgot to do so or because we have decided the old
			 *  address was unusable for some reason.
			 */
			if (!r->start && r->end)
                        {
                          	pci_assign_resource(dev, idx);
                         }
		}
#if 0 /* don't assign ROMs */
		if (pci_probe & PCI_ASSIGN_ROMS) {
			r = &dev->resource[PCI_ROM_RESOURCE];
			r->end -= r->start;
			r->start = 0;
			if (r->end)
				pci_assign_resource(dev, PCI_ROM_RESOURCE);
		}
#endif
        }
}

/*
 * Publish a region of local address space over the PCI bus
 * to other devices.
 */
void pci_set_rbar_region(unsigned int region,     unsigned long localAddr,
			 unsigned long pciOffset, unsigned long regionSize)
{
	unsigned long mask;

	if (region > 7)
		return;

	if (regionSize > (512 * 1024 * 1024))
		return;

	mask = r2p2(regionSize) - 0x10000;

	/* Diable the region (in case currently in use, should never happen) */
	ST40PCI_WRITE_INDEXED(RSR, region, 0);

	/* Start of local address space to publish */
	ST40PCI_WRITE_INDEXED(RLAR, region, PHYSADDR(localAddr) );

	/* Start of region in PCI address space as an offset from MBAR0 */
	ST40PCI_WRITE_INDEXED(RBAR, region, pciOffset);

	/* Size of region */
	ST40PCI_WRITE_INDEXED(RSR, region, mask | 1);
} 

/*
 * Make a previously published region of local address space
 * inaccessible to other PCI devices.
 */ 
void pci_clear_rbar_region(unsigned int region)
{
	if (region > 7)
		return;

	ST40PCI_WRITE_INDEXED(RSR, region, 0);
	ST40PCI_WRITE_INDEXED(RBAR, region, 0);
	ST40PCI_WRITE_INDEXED(RLAR, region, 0);
}
