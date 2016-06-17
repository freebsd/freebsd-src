/* 
 * Copyright (C) 2001 David J. Mckay (david.mckay@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.                            
 *
 * Support functions for the SH5 PCI hardware.
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

#include <asm/io.h>
#include <asm/hardware.h>
#include "pci_sh5.h"

#undef DEBUG

#ifdef DEBUG
#  define dprintk(x...)  printk(KERN_DEBUG x)
#else
#  define dprintk(x...)  do { } while (0)
#endif /* DEBUG */

static unsigned long pcicr_virt;
unsigned long pciio_virt;

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

/* Add future fixups here... */
struct pci_fixup pcibios_fixups[] = {
	{ PCI_FIXUP_HEADER,	PCI_ANY_ID,	PCI_ANY_ID,	pci_fixup_ide_bases },
	{ 0 }
};

char * __init pcibios_setup(char *str)
{
	return str;
}

/* Rounds a number UP to the nearest power of two. Used for
 * sizing the PCI window.
 */
static u32 __init r2p2(u32 num)
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

extern unsigned long long memory_start, memory_end;

int __init sh5pci_init(unsigned memStart, unsigned memSize)
{
	u32 lsr0;
	u32 uval;

	pcicr_virt = onchip_remap(SH5PCI_ICR_BASE, 1024, "PCICR");
	if (!pcicr_virt) {
		panic("Unable to remap PCICR\n");
	}

	pciio_virt = onchip_remap(SH5PCI_IO_BASE, 0x10000, "PCIIO");
	if (!pciio_virt) {
		panic("Unable to remap PCIIO\n");
	}

	dprintk("Register base addres is 0x%08lx\n", pcicr_virt);

	/* Clear snoop registers */
        SH5PCI_WRITE(CSCR0, 0);
        SH5PCI_WRITE(CSCR1, 0);

	dprintk("Wrote to reg\n");

        /* Switch off interrupts */
        SH5PCI_WRITE(INTM,  0);
        SH5PCI_WRITE(AINTM, 0);
        SH5PCI_WRITE(PINTM, 0);

        /* Set bus active, take it out of reset */
        uval = SH5PCI_READ(CR);

	/* Set command Register */
        SH5PCI_WRITE(CR, uval | CR_LOCK_MASK | CR_CFINT| CR_FTO | CR_PFE | CR_PFCS | CR_BMAM );

	uval=SH5PCI_READ(CR);
        dprintk("CR is actually 0x%08x\n",uval);

        /* Allow it to be a master */
	/* NB - WE DISABLE I/O ACCESS to stop overlap */
        /* set WAIT bit to enable stepping, an attempt to improve stability */
	SH5PCI_WRITE_SHORT(CSR_CMD,
			    PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER | PCI_COMMAND_WAIT);

        /* 
        ** Set translation mapping memory in order to convert the address
        ** used for the main bus, to the PCI internal address.
        */
        SH5PCI_WRITE(MBR,0x40000000);

        /* Always set the max size 512M */
        SH5PCI_WRITE(MBMR, PCISH5_MEM_SIZCONV(512*1024*1024));

        /* 
        ** I/O addresses are mapped at internal PCI specific address
        ** as is described into the configuration bridge table.
        ** These are changed to 0, to allow cards that have legacy 
        ** io such as vga to function correctly. We set the SH5 IOBAR to
        ** 256K, which is a bit big as we can only have 64K of address space
        */

        SH5PCI_WRITE(IOBR,0x0);
      	
	dprintk("PCI:Writing 0x%08x to IOBR\n",0);

        /* Set up a 256K window. Totally pointless waste  of address space */
        SH5PCI_WRITE(IOBMR,0);
	dprintk("PCI:Writing 0x%08x to IOBMR\n",0);

	/* The SH5 has a HUGE 256K I/O region, which breaks the PCI spec. Ideally, 
         * we would want to map the I/O region somewhere, but it is so big this is not
         * that easy!
         */
	SH5PCI_WRITE(CSR_IBAR0,~0);
	/* Set memory size value */
        memSize = memory_end - memory_start;

        /* Now we set up the mbars so the PCI bus can see the memory of the machine */
        if (memSize < (1024 * 1024)) {
                printk(KERN_ERR "PCISH5: Ridiculous memory size of 0x%x?\n", memSize);
                return(-1);
        }

        /* Set LSR 0 */
        lsr0 = (memSize > (512 * 1024 * 1024)) ? 0x1ff00001 : ((r2p2(memSize) - 0x100000) | 0x1);
        SH5PCI_WRITE(LSR0, lsr0);

	dprintk("PCI:Writing 0x%08x to LSR0\n",lsr0);

        /* Set MBAR 0 */
        SH5PCI_WRITE(CSR_MBAR0, memory_start);
        SH5PCI_WRITE(LAR0, memory_start);


        SH5PCI_WRITE(CSR_MBAR1,0);
        SH5PCI_WRITE(LAR1,0);
        SH5PCI_WRITE(LSR1,0);

	dprintk("PCI:Writing 0x%08llx to CSR_MBAR0\n",memory_start);         
	dprintk("PCI:Writing 0x%08llx to LAR0\n",memory_start);         

        /* Enable the PCI interrupts on the device */

#if 1
        SH5PCI_WRITE(INTM,  ~0);
        SH5PCI_WRITE(AINTM, ~0);
        SH5PCI_WRITE(PINTM, ~0);
#endif

	dprintk("Switching on all error interrupts\n");

        return(0);
}



/* Write to config register */
static int sh5pci_read_config_byte(struct pci_dev *dev, int where,
				    u8 * val)
{
	SH5PCI_WRITE(PAR, CONFIG_CMD(dev, where));

	*val = SH5PCI_READ_BYTE(PDR + (where & 3));

	return PCIBIOS_SUCCESSFUL;
}

static int sh5pci_read_config_word(struct pci_dev *dev, int where,
				    u16 * val)
{
	SH5PCI_WRITE(PAR, CONFIG_CMD(dev, where));

	*val = SH5PCI_READ_SHORT(PDR + (where & 2));


	return PCIBIOS_SUCCESSFUL;
}

static int sh5pci_read_config_dword(struct pci_dev *dev, int where,
				     u32 * val)
{
	SH5PCI_WRITE(PAR, CONFIG_CMD(dev, where));

	*val = SH5PCI_READ(PDR);

	return PCIBIOS_SUCCESSFUL;
}

static int sh5pci_write_config_byte(struct pci_dev *dev, int where,
				     u8 val)
{
	SH5PCI_WRITE(PAR, CONFIG_CMD(dev, where));

	SH5PCI_WRITE_BYTE(PDR + (where & 3), val);


	return PCIBIOS_SUCCESSFUL;
}


static int sh5pci_write_config_word(struct pci_dev *dev, int where,
				     u16 val)
{
	SH5PCI_WRITE(PAR, CONFIG_CMD(dev, where));

	SH5PCI_WRITE_SHORT(PDR + (where & 2), val);

	return PCIBIOS_SUCCESSFUL;
}

static int sh5pci_write_config_dword(struct pci_dev *dev, int where,
				      u32 val)
{
	SH5PCI_WRITE(PAR, CONFIG_CMD(dev, where));

	SH5PCI_WRITE(PDR, val);

	return PCIBIOS_SUCCESSFUL;
}


static struct pci_ops pci_config_ops = {
	sh5pci_read_config_byte,
	sh5pci_read_config_word,
	sh5pci_read_config_dword,
	sh5pci_write_config_byte,
	sh5pci_write_config_word,
	sh5pci_write_config_dword
};

/* Everything hangs off this */
static struct pci_bus *pci_root_bus;


static u8 __init no_swizzle(struct pci_dev *dev, u8 * pin)
{
	dprintk("swizzle for dev %d on bus %d slot %d pin is %d\n",
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

/* This needs to be shunted out of here into the board specific bit */

static int __init map_cayman_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
	int result = -1;

	if (dev->bus->number == 0) {
	        switch ((slot + (pin-1)) & 3) {
	        case 0:
	          result = IRQ_INTA;
		  break;
                case 1:
	          result = IRQ_INTB;
		  break;
                case 2:
	          result = IRQ_INTC;
		  break;
		case 3:
		  result = IRQ_INTD;
		  break;
                }
        }

	if (dev->bus->number == 2) {
	        switch((slot + (pin-1)) & 3) {
	        case 0:
	          result = IRQ_P2INTA;
		  break;
                case 1:
	          result = IRQ_P2INTB;
		  break;
                case 2:
	          result = IRQ_P2INTC;
		  break;
                case 3:
	          result = IRQ_P2INTD;
		  break;
                }
        }

	dprintk("map_cayman_irq for dev %d on bus %d slot %d, pin is %d : irq=%d\n",
	       dev->devfn,dev->bus->number,slot,pin,result);

	return result;
}

#ifdef DEBUG
void print_resource(struct resource *r) 
{
	if (r == NULL)
		return;

	//printk("Resource %s\n",r->name);
	printk("Start 0x%lx end 0x%lx\n",r->start,r->end);
}

void print_set_ranges_data(struct pbus_set_ranges_data *ranges)
{

	if (!ranges) {
		printk("NO RANGES\n");
		return;
	}

	printk("io_start is 0x%lx\n", ranges->io_start);
	printk("io_end is 0x%lx\n", ranges->io_end);
	printk("mem_start is 0x%lx\n", ranges->mem_start);
	printk("mem_end is 0x%lx\n", ranges->mem_end);
}
#endif /* DEBUG */

void __init
  pcibios_fixup_pbus_ranges(struct pci_bus *bus,
	        struct pbus_set_ranges_data *ranges)
{
#ifdef DEBUG
	int i;

	dprintk("%s, bus number %d\n",__FUNCTION__,bus->number);

	print_set_ranges_data(ranges);

	for (i = 0; i < 2; i++)
		print_resource(bus->resource[i]);
#endif
}

void  pcish5_err_irq(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned pci_int, pci_air, pci_cir, pci_aint;

	pci_int = SH5PCI_READ(INT);
	pci_cir = SH5PCI_READ(CIR);
	pci_air = SH5PCI_READ(AIR);

	if (pci_int) {
		printk("PCI INTERRUPT (at %08llx)!\n", regs->pc);
		printk("PCI INT -> 0x%x\n", pci_int & 0xffff);
		printk("PCI AIR -> 0x%x\n", pci_air);
		printk("PCI CIR -> 0x%x\n", pci_cir);
		SH5PCI_WRITE(INT, ~0);
	}

	pci_aint = SH5PCI_READ(AINT);
	if (pci_aint) {
		printk("PCI ARB INTERRUPT!\n");
		printk("PCI AINT -> 0x%x\n", pci_aint);
		printk("PCI AIR -> 0x%x\n", pci_air);
		printk("PCI CIR -> 0x%x\n", pci_cir);
		SH5PCI_WRITE(AINT, ~0);
	}

}


void pcish5_serr_irq(int irq, void *dev_id, struct pt_regs *regs)
{
  printk("SERR IRQ\n");

}

#define ROUND_UP(x, a)		(((x) + (a) - 1) & ~((a) - 1))

static void __init
pcibios_size_bridge(struct pci_bus *bus, struct pbus_set_ranges_data *outer)
{
	struct pbus_set_ranges_data inner;
	struct pci_dev *dev;
	struct pci_dev *bridge = bus->self;
	struct list_head *ln;

	if (!bridge)
		return;	/* host bridge, nothing to do */

	/* set reasonable default locations for pcibios_align_resource */
	inner.io_start = PCIBIOS_MIN_IO;
	inner.mem_start = PCIBIOS_MIN_MEM;
	inner.io_end = inner.io_start;
	inner.mem_end = inner.mem_start;

	/* Collect information about how our direct children are layed out. */
	for (ln=bus->devices.next; ln != &bus->devices; ln=ln->next) {
		int i;
		dev = pci_dev_b(ln);

		/* Skip bridges for now */
		if (dev->class >> 8 == PCI_CLASS_BRIDGE_PCI)
			continue;

		for (i = 0; i < PCI_NUM_RESOURCES; i++) {
			struct resource res;
			unsigned long size;

			memcpy(&res, &dev->resource[i], sizeof(res));
			size = res.end - res.start + 1;

			if (res.flags & IORESOURCE_IO) {
				res.start = inner.io_end;
				pcibios_align_resource(dev, &res, size, 0);
				inner.io_end = res.start + size;
			} else if (res.flags & IORESOURCE_MEM) {
				res.start = inner.mem_end;
				pcibios_align_resource(dev, &res, size, 0);
				inner.mem_end = res.start + size;
			}
		}
	}

	/* And for all of the subordinate busses. */
	for (ln=bus->children.next; ln != &bus->children; ln=ln->next)
		pcibios_size_bridge(pci_bus_b(ln), &inner);

	/* turn the ending locations into sizes (subtract start) */
	inner.io_end -= inner.io_start;
	inner.mem_end -= inner.mem_start;

	/* Align the sizes up by bridge rules */
	inner.io_end = ROUND_UP(inner.io_end, 4*1024) - 1;
	inner.mem_end = ROUND_UP(inner.mem_end, 1*1024*1024) - 1;

	/* Adjust the bridge's allocation requirements */
	bridge->resource[0].end = bridge->resource[0].start + inner.io_end;
	bridge->resource[1].end = bridge->resource[1].start + inner.mem_end;

	bridge->resource[PCI_BRIDGE_RESOURCES].end =
	    bridge->resource[PCI_BRIDGE_RESOURCES].start + inner.io_end;
	bridge->resource[PCI_BRIDGE_RESOURCES+1].end =
	    bridge->resource[PCI_BRIDGE_RESOURCES+1].start + inner.mem_end;

	/* adjust parent's resource requirements */
	if (outer) {
		outer->io_end = ROUND_UP(outer->io_end, 4*1024);
		outer->io_end += inner.io_end;

		outer->mem_end = ROUND_UP(outer->mem_end, 1*1024*1024);
		outer->mem_end += inner.mem_end;
	}
}

#undef ROUND_UP

static void __init 
pcibios_size_bridges(void)
{
	struct pbus_set_ranges_data outer;

	memset(&outer,0,sizeof(outer));
	pcibios_size_bridge(pci_root_bus,&outer);
}

void __init
common_init_pci(void)
{
	pci_root_bus = pci_scan_bus(0, &pci_config_ops, NULL);
	pcibios_size_bridges();
	pci_assign_unassigned_resources();
	pci_fixup_irqs(no_swizzle, map_cayman_irq);
	//	pci_set_bus_ranges();
}

void __init pcibios_init(void)
{

        if (request_irq(IRQ_ERR, pcish5_err_irq,
                        SA_INTERRUPT, "PCI Error",NULL) < 0) {
                printk(KERN_ERR "PCISH5: Cannot hook PCI_PERR interrupt\n");
		return;
        }


        if (request_irq(IRQ_SERR, pcish5_serr_irq,
                        SA_INTERRUPT, "PCI SERR interrupt", NULL) < 0) {
                printk(KERN_ERR "PCISH5: Cannot hook PCI_SERR interrupt\n");
		return;
        }


	/* The pci subsytem needs to know where memory is and how much 
	 * of it there is. I've simply made these globals. A better mechanism
	 * is probably needed. 
	 */
	sh5pci_init(__pa(memory_start),
		     __pa(memory_end) - __pa(memory_start));


	common_init_pci();

#if 0
	pci_root_bus = pci_scan_bus(0, &pci_config_ops, NULL);

	pci_assign_unassigned_resources();

	pci_fixup_irqs(no_swizzle, map_cayman_irq);

	pci_set_bus_ranges();
#endif

}

void __init pcibios_fixup_bus(struct pci_bus *bus)
{
	struct pci_dev *dev = bus->self;
	int i;

#if 1
	if(dev) {
		for(i=0; i<3; i++) {
			bus->resource[i] =
				&dev->resource[PCI_BRIDGE_RESOURCES+i];
			bus->resource[i]->name = bus->name;
		}
		bus->resource[0]->flags |= IORESOURCE_IO;
		bus->resource[1]->flags |= IORESOURCE_MEM;

		/* For now, propogate host limits to the bus;
		 * we'll adjust them later. */

#if 1
		bus->resource[0]->end = 64*1024 - 1 ;
		bus->resource[1]->end = PCIBIOS_MIN_MEM+(256*1024*1024)-1;
		bus->resource[0]->start = PCIBIOS_MIN_IO;
		bus->resource[1]->start = PCIBIOS_MIN_MEM;
#else
		bus->resource[0]->end = 0
		bus->resource[1]->end = 0
		bus->resource[0]->start =0
		  bus->resource[1]->start = 0;
#endif

#ifdef DEBUG
		dprintk("in fixup bus resource 0 is:\n");
		print_resource(bus->resource[0]);
		dprintk("in fixup bus resource 1 is:\n");
		print_resource(bus->resource[1]);
#endif /* DEBUG */
		
		/* Turn off downstream PF memory address range by default */
		bus->resource[2]->start = 1024*1024;
		bus->resource[2]->end = bus->resource[2]->start - 1;
	}
#endif 

}

