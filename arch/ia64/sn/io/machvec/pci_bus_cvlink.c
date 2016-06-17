/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997, 2000-2003 Silicon Graphics, Inc. All rights reserved.
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/pci_ids.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <asm/sn/types.h>
#include <asm/sn/sgi.h>
#include <asm/sn/io.h>
#include <asm/sn/driver.h>
#include <asm/sn/iograph.h>
#include <asm/param.h>
#include <asm/sn/pio.h>
#include <asm/sn/xtalk/xwidget.h>
#include <asm/sn/sn_private.h>
#include <asm/sn/addrs.h>
#include <asm/sn/invent.h>
#include <asm/sn/hcl.h>
#include <asm/sn/hcl_util.h>
#include <asm/sn/intr.h>
#include <asm/sn/xtalk/xtalkaddrs.h>
#include <asm/sn/klconfig.h>
#include <asm/sn/nodepda.h>
#include <asm/sn/pci/pciio.h>
#include <asm/sn/pci/pcibr.h>
#include <asm/sn/pci/pcibr_private.h>
#include <asm/sn/pci/pci_bus_cvlink.h>
#include <asm/sn/simulator.h>
#include <asm/sn/sn_cpuid.h>
#include <asm/sn/arch.h>

extern int bridge_rev_b_data_check_disable;

vertex_hdl_t busnum_to_pcibr_vhdl[MAX_PCI_XWIDGET];
nasid_t busnum_to_nid[MAX_PCI_XWIDGET];
void * busnum_to_atedmamaps[MAX_PCI_XWIDGET];
unsigned char num_bridges;
static int done_probing;
extern irqpda_t *irqpdaindr;

static int pci_bus_map_create(vertex_hdl_t xtalk, int brick_type, char * io_moduleid);
vertex_hdl_t devfn_to_vertex(unsigned char busnum, unsigned int devfn);

extern void register_pcibr_intr(int irq, pcibr_intr_t intr);

void sn_dma_flush_init(unsigned long start, unsigned long end, int idx, int pin, int slot);

/*
 * For the given device, initialize whether it is a PIC device.
 */
static void
set_isPIC(struct sn_device_sysdata *device_sysdata)
{
	pciio_info_t pciio_info = pciio_info_get(device_sysdata->vhdl);
	pcibr_soft_t pcibr_soft = (pcibr_soft_t) pciio_info_mfast_get(pciio_info);

	device_sysdata->isPIC = IS_PIC_SOFT(pcibr_soft);;
}

/*
 * pci_bus_cvlink_init() - To be called once during initialization before 
 *	SGI IO Infrastructure init is called.
 */
void
pci_bus_cvlink_init(void)
{

	extern void ioconfig_bus_init(void);

	memset(busnum_to_pcibr_vhdl, 0x0, sizeof(vertex_hdl_t) * MAX_PCI_XWIDGET);
	memset(busnum_to_nid, 0x0, sizeof(nasid_t) * MAX_PCI_XWIDGET);

	memset(busnum_to_atedmamaps, 0x0, sizeof(void *) * MAX_PCI_XWIDGET);

	num_bridges = 0;

	ioconfig_bus_init();
}

/*
 * pci_bus_to_vertex() - Given a logical Linux Bus Number returns the associated 
 *	pci bus vertex from the SGI IO Infrastructure.
 */
vertex_hdl_t
pci_bus_to_vertex(unsigned char busnum)
{

	vertex_hdl_t	pci_bus = NULL;


	/*
	 * First get the xwidget vertex.
	 */
	pci_bus = busnum_to_pcibr_vhdl[busnum];
	return(pci_bus);
}

/*
 * devfn_to_vertex() - returns the vertex of the device given the bus, slot, 
 *	and function numbers.
 */
vertex_hdl_t
devfn_to_vertex(unsigned char busnum, unsigned int devfn)
{

	int slot = 0;
	int func = 0;
	char	name[16];
	vertex_hdl_t  pci_bus = NULL;
	vertex_hdl_t	device_vertex = (vertex_hdl_t)NULL;

	/*
	 * Go get the pci bus vertex.
	 */
	pci_bus = pci_bus_to_vertex(busnum);
	if (!pci_bus) {
		/*
		 * During probing, the Linux pci code invents non-existent
		 * bus numbers and pci_dev structures and tries to access
		 * them to determine existence. Don't crib during probing.
		 */
		if (done_probing)
			printk("devfn_to_vertex: Invalid bus number %d given.\n", busnum);
		return(NULL);
	}


	/*
	 * Go get the slot&function vertex.
	 * Should call pciio_slot_func_to_name() when ready.
	 */
	slot = PCI_SLOT(devfn);
	func = PCI_FUNC(devfn);

	/*
	 * For a NON Multi-function card the name of the device looks like:
	 * ../pci/1, ../pci/2 ..
	 */
	if (func == 0) {
        	sprintf(name, "%d", slot);
		if (hwgraph_traverse(pci_bus, name, &device_vertex) == GRAPH_SUCCESS) {
			if (device_vertex) {
				return(device_vertex);
			}
		}
	}
			
	/*
	 * This maybe a multifunction card.  It's names look like:
	 * ../pci/1a, ../pci/1b, etc.
	 */
	sprintf(name, "%d%c", slot, 'a'+func);
	if (hwgraph_traverse(pci_bus, name, &device_vertex) != GRAPH_SUCCESS) {
		if (!device_vertex) {
			return(NULL);
		}
	}

	return(device_vertex);
}

struct sn_flush_nasid_entry flush_nasid_list[MAX_NASIDS];

// Initialize the data structures for flushing write buffers after a PIO read.
// The theory is: 
// Take an unused int. pin and associate it with a pin that is in use.
// After a PIO read, force an interrupt on the unused pin, forcing a write buffer flush
// on the in use pin.  This will prevent the race condition between PIO read responses and 
// DMA writes.
void
sn_dma_flush_init(unsigned long start, unsigned long end, int idx, int pin, int slot) {
	nasid_t nasid; 
	unsigned long dnasid;
	int wid_num;
	int bus;
	struct sn_flush_device_list *p;
	bridge_t *b;
	bridgereg_t dev_sel;
	extern int isIO9(int);
	int bwin;
	int i;

	nasid = NASID_GET(start);
	wid_num = SWIN_WIDGETNUM(start);
	bus = (start >> 23) & 0x1;
	bwin = BWIN_WINDOWNUM(start);

	if (flush_nasid_list[nasid].widget_p == NULL) {
		flush_nasid_list[nasid].widget_p = (struct sn_flush_device_list **)kmalloc((HUB_WIDGET_ID_MAX+1) *
			sizeof(struct sn_flush_device_list *), GFP_KERNEL);
		if (flush_nasid_list[nasid].widget_p <= 0)
			BUG(); /* Cannot afford to run out of memory. */

		memset(flush_nasid_list[nasid].widget_p, 0, (HUB_WIDGET_ID_MAX+1) * sizeof(struct sn_flush_device_list *));
	}
	if (bwin > 0) {
		bwin--;
		switch (bwin) {
			case 0:
				flush_nasid_list[nasid].iio_itte1 = HUB_L(IIO_ITTE_GET(nasid, 0));
				wid_num = ((flush_nasid_list[nasid].iio_itte1) >> 8) & 0xf;
				bus = flush_nasid_list[nasid].iio_itte1 & 0xf;
				if (bus == 0x4 || bus == 0x8)
					bus = 0;
				else
					bus = 1;
				break;
			case 1:
				flush_nasid_list[nasid].iio_itte2 = HUB_L(IIO_ITTE_GET(nasid, 1));
				wid_num = ((flush_nasid_list[nasid].iio_itte2) >> 8) & 0xf;
				bus = flush_nasid_list[nasid].iio_itte2 & 0xf;
				if (bus == 0x4 || bus == 0x8)
					bus = 0;
				else
					bus = 1;
				break;
			case 2:
				flush_nasid_list[nasid].iio_itte3 = HUB_L(IIO_ITTE_GET(nasid, 2));
				wid_num = ((flush_nasid_list[nasid].iio_itte3) >> 8) & 0xf;
				bus = flush_nasid_list[nasid].iio_itte3 & 0xf;
				if (bus == 0x4 || bus == 0x8)
					bus = 0;
				else
					bus = 1;
				break;
			case 3:
				flush_nasid_list[nasid].iio_itte4 = HUB_L(IIO_ITTE_GET(nasid, 3));
				wid_num = ((flush_nasid_list[nasid].iio_itte4) >> 8) & 0xf;
				bus = flush_nasid_list[nasid].iio_itte4 & 0xf;
				if (bus == 0x4 || bus == 0x8)
					bus = 0;
				else
					bus = 1;
				break;
			case 4:
				flush_nasid_list[nasid].iio_itte5 = HUB_L(IIO_ITTE_GET(nasid, 4));
				wid_num = ((flush_nasid_list[nasid].iio_itte5) >> 8) & 0xf;
				bus = flush_nasid_list[nasid].iio_itte5 & 0xf;
				if (bus == 0x4 || bus == 0x8)
					bus = 0;
				else
					bus = 1;
				break;
			case 5:
				flush_nasid_list[nasid].iio_itte6 = HUB_L(IIO_ITTE_GET(nasid, 5));
				wid_num = ((flush_nasid_list[nasid].iio_itte6) >> 8) & 0xf;
				bus = flush_nasid_list[nasid].iio_itte6 & 0xf;
				if (bus == 0x4 || bus == 0x8)
					bus = 0;
				else
					bus = 1;
				break;
			case 6:
				flush_nasid_list[nasid].iio_itte7 = HUB_L(IIO_ITTE_GET(nasid, 6));
				wid_num = ((flush_nasid_list[nasid].iio_itte7) >> 8) & 0xf;
				bus = flush_nasid_list[nasid].iio_itte7 & 0xf;
				if (bus == 0x4 || bus == 0x8)
					bus = 0;
				else
					bus = 1;
				break;
		}
	}

	// if it's IO9, bus 1, we don't care about slots 1, 3, and 4.  This is
	// because these are the IOC4 slots and we don't flush them.
	if (isIO9(nasid) && bus == 0 && (slot == 1 || slot == 4)) {
		return;
	}
	if (flush_nasid_list[nasid].widget_p[wid_num] == NULL) {
		flush_nasid_list[nasid].widget_p[wid_num] = (struct sn_flush_device_list *)kmalloc(
			DEV_PER_WIDGET * sizeof (struct sn_flush_device_list), GFP_KERNEL);
		if (flush_nasid_list[nasid].widget_p[wid_num] <= 0)
			BUG(); /* Cannot afford to run out of memory. */

		memset(flush_nasid_list[nasid].widget_p[wid_num], 0, 
			DEV_PER_WIDGET * sizeof (struct sn_flush_device_list));
		p = &flush_nasid_list[nasid].widget_p[wid_num][0];
		for (i=0; i<DEV_PER_WIDGET;i++) {
			p->bus = -1;
			p->pin = -1;
			p++;
		}
	}

	p = &flush_nasid_list[nasid].widget_p[wid_num][0];
	for (i=0;i<DEV_PER_WIDGET; i++) {
		if (p->pin == pin && p->bus == bus) break;
		if (p->pin < 0) {
			p->pin = pin;
			p->bus = bus;
			break;
		}
		p++;
	}

	for (i=0; i<PCI_ROM_RESOURCE; i++) {
		if (p->bar_list[i].start == 0) {
			p->bar_list[i].start = start;
			p->bar_list[i].end = end;
			break;
		}
	}
	b = (bridge_t *)(NODE_SWIN_BASE(nasid, wid_num) | (bus << 23) );

	// If it's IO9, then slot 2 maps to slot 7 and slot 6 maps to slot 8.
	// To see this is non-trivial.  By drawing pictures and reading manuals and talking
	// to HW guys, we can see that on IO9 bus 1, slots 7 and 8 are always unused.
	// Further, since we short-circuit slots  1, 3, and 4 above, we only have to worry
	// about the case when there is a card in slot 2.  A multifunction card will appear
	// to be in slot 6 (from an interrupt point of view) also.  That's the  most we'll
	// have to worry about.  A four function card will overload the interrupt lines in
	// slot 2 and 6.  
	// We also need to special case the 12160 device in slot 3.  Fortunately, we have
	// a spare intr. line for pin 4, so we'll use that for the 12160.
	// All other buses have slot 3 and 4 and slots 7 and 8 unused.  Since we can only
	// see slots 1 and 2 and slots 5 and 6 coming through here for those buses (this
	// is true only on Pxbricks with 2 physical slots per bus), we just need to add
	// 2 to the slot number to find an unused slot.
	// We have convinced ourselves that we will never see a case where two different cards
	// in two different slots will ever share an interrupt line, so there is no need to
	// special case this.

	if (isIO9(nasid) && wid_num == 0xc && bus == 0) {
		if (slot == 2) {
			p->force_int_addr = (unsigned long)&b->b_force_always[6].intr;
			dev_sel = b->b_int_device;
			dev_sel |= (1<<18);
			b->b_int_device = dev_sel;
			dnasid = NASID_GET(virt_to_phys(&p->flush_addr));
			b->p_int_addr_64[6] = (virt_to_phys(&p->flush_addr) & 0xfffffffff) | 
				(dnasid << 36) | (0xfUL << 48);
		} else  if (slot == 3) { /* 12160 SCSI device in IO9 */
			p->force_int_addr = (unsigned long)&b->b_force_always[4].intr;
			dev_sel = b->b_int_device;
			dev_sel |= (2<<12);
			b->b_int_device = dev_sel;
			dnasid = NASID_GET(virt_to_phys(&p->flush_addr));
			b->p_int_addr_64[4] = (virt_to_phys(&p->flush_addr) & 0xfffffffff) | 
				(dnasid << 36) | (0xfUL << 48);
		} else { /* slot == 6 */
			p->force_int_addr = (unsigned long)&b->b_force_always[7].intr;
			dev_sel = b->b_int_device;
			dev_sel |= (5<<21);
			b->b_int_device = dev_sel;
			dnasid = NASID_GET(virt_to_phys(&p->flush_addr));
			b->p_int_addr_64[7] = (virt_to_phys(&p->flush_addr) & 0xfffffffff) | 
				(dnasid << 36) | (0xfUL << 48);
		}
	} else {
		p->force_int_addr = (unsigned long)&b->b_force_always[pin + 2].intr;
		dev_sel = b->b_int_device;
		dev_sel |= ((slot - 1) << ( pin * 3) );
		b->b_int_device = dev_sel;
		dnasid = NASID_GET(virt_to_phys(&p->flush_addr));
		b->p_int_addr_64[pin + 2] = (virt_to_phys(&p->flush_addr) & 0xfffffffff) | 
			(dnasid << 36) | (0xfUL << 48);
	}
}

/*
 * Most drivers currently do not properly tell the arch specific pci dma
 * interfaces whether they can handle A64. Here is where we privately
 * keep track of this.
 */
static void __init
set_sn_pci64(struct pci_dev *dev)
{
	unsigned short vendor = dev->vendor;
	unsigned short device = dev->device;

	if (vendor == PCI_VENDOR_ID_QLOGIC) {
		if ((device == PCI_DEVICE_ID_QLOGIC_ISP2100) ||
				(device == PCI_DEVICE_ID_QLOGIC_ISP2200)) {
			SET_PCIA64(dev);
			return;
		}
	}

}

/*
 * sn_pci_fixup() - This routine is called when platform_pci_fixup() is 
 *	invoked at the end of pcibios_init() to link the Linux pci 
 *	infrastructure to SGI IO Infrasturcture - ia64/kernel/pci.c
 *
 *	Other platform specific fixup can also be done here.
 */
void
sn_pci_fixup(int arg)
{
	struct list_head *ln;
	struct pci_bus *pci_bus = NULL;
	struct pci_dev *device_dev = NULL;
	struct sn_widget_sysdata *widget_sysdata;
	struct sn_device_sysdata *device_sysdata;
	pciio_intr_t intr_handle;
	int cpuid;
	vertex_hdl_t device_vertex;
	pciio_intr_line_t lines;
	extern void sn_pci_find_bios(void);
	extern int numnodes;
	int cnode;

	if (arg == 0) {
#ifdef CONFIG_PROC_FS
		extern void register_sn_procfs(void);
#endif

		sn_pci_find_bios();
		for (cnode = 0; cnode < numnodes; cnode++) {
			extern void intr_init_vecblk(nodepda_t *npda, cnodeid_t, int);
			intr_init_vecblk(NODEPDA(cnode), cnode, 0);
		} 
#ifdef CONFIG_PROC_FS
		register_sn_procfs();
#endif
		return;
	}


	done_probing = 1;

	/*
	 * Initialize the pci bus vertex in the pci_bus struct.
	 */
	for( ln = pci_root_buses.next; ln != &pci_root_buses; ln = ln->next) {
		pci_bus = pci_bus_b(ln);
		widget_sysdata = kmalloc(sizeof(struct sn_widget_sysdata), 
					GFP_KERNEL);
		if (widget_sysdata <= 0)
			BUG(); /* Cannot afford to run out of memory */

		widget_sysdata->vhdl = pci_bus_to_vertex(pci_bus->number);
		pci_bus->sysdata = (void *)widget_sysdata;
	}

	/*
 	 * set the root start and end so that drivers calling check_region()
	 * won't see a conflict
	 */
	ioport_resource.start  = 0xc000000000000000;
	ioport_resource.end =    0xcfffffffffffffff;

	/*
	 * Set the root start and end for Mem Resource.
	 */
	iomem_resource.start = 0;
	iomem_resource.end = 0xffffffffffffffff;

	/*
	 * Initialize the device vertex in the pci_dev struct.
	 */
	pci_for_each_dev(device_dev) {
		unsigned int irq;
		int idx;
		u16 cmd;
		vertex_hdl_t vhdl;
		unsigned long size;
		extern int bit_pos_to_irq(int);

		/* Set the device vertex */

		device_sysdata = kmalloc(sizeof(struct sn_device_sysdata),
					GFP_KERNEL);
		if (device_sysdata <= 0)
			BUG(); /* Cannot afford to run out of memory */

		device_sysdata->vhdl = devfn_to_vertex(device_dev->bus->number, device_dev->devfn);
		device_sysdata->isa64 = 0;
		device_dev->sysdata = (void *) device_sysdata;
		set_sn_pci64(device_dev);
		set_isPIC(device_sysdata);

		pci_read_config_word(device_dev, PCI_COMMAND, &cmd);

		/*
		 * Set the resources address correctly.  The assumption here 
		 * is that the addresses in the resource structure has been
		 * read from the card and it was set in the card by our
		 * Infrastructure ..
		 */
		vhdl = device_sysdata->vhdl;
		for (idx = 0; idx < PCI_ROM_RESOURCE; idx++) {
			size = 0;
			size = device_dev->resource[idx].end -
				device_dev->resource[idx].start;
			if (size) {
				device_dev->resource[idx].start = (unsigned long)pciio_pio_addr(vhdl, 0, PCIIO_SPACE_WIN(idx), 0, size, 0, 0);
				device_dev->resource[idx].start |= __IA64_UNCACHED_OFFSET;
			}
			else
				continue;

			device_dev->resource[idx].end = 
				device_dev->resource[idx].start + size;

			if (device_dev->resource[idx].flags & IORESOURCE_IO)
				cmd |= PCI_COMMAND_IO;

			if (device_dev->resource[idx].flags & IORESOURCE_MEM)
				cmd |= PCI_COMMAND_MEMORY;
		}
#if 0
	/*
	 * Software WAR for a Software BUG.
	 * This is only temporary.
	 * See PV 872791
	 */

		/*
		 * Now handle the ROM resource ..
		 */
		size = device_dev->resource[PCI_ROM_RESOURCE].end -
			device_dev->resource[PCI_ROM_RESOURCE].start;

		if (size) {
			device_dev->resource[PCI_ROM_RESOURCE].start =
			(unsigned long) pciio_pio_addr(vhdl, 0, PCIIO_SPACE_ROM, 0, 
				size, 0, 0);
			device_dev->resource[PCI_ROM_RESOURCE].start |= __IA64_UNCACHED_OFFSET;
			device_dev->resource[PCI_ROM_RESOURCE].end =
			device_dev->resource[PCI_ROM_RESOURCE].start + size;
		}
#endif

		/*
		 * Update the Command Word on the Card.
		 */
		cmd |= PCI_COMMAND_MASTER; /* If the device doesn't support */
					   /* bit gets dropped .. no harm */
		pci_write_config_word(device_dev, PCI_COMMAND, cmd);

		pci_read_config_byte(device_dev, PCI_INTERRUPT_PIN, (unsigned char *)&lines);
		device_sysdata = (struct sn_device_sysdata *)device_dev->sysdata;
		device_vertex = device_sysdata->vhdl;
 
		irqpdaindr->current = device_dev;
		intr_handle = pciio_intr_alloc(device_vertex, NULL, lines, device_vertex);

		irq = intr_handle->pi_irq;
		irqpdaindr->device_dev[irq] = device_dev;
		cpuid = intr_handle->pi_cpu;
		pciio_intr_connect(intr_handle, (intr_func_t)0, (intr_arg_t)0);
		device_dev->irq = irq;
		register_pcibr_intr(irq, (pcibr_intr_t)intr_handle);

		for (idx = 0; idx < PCI_ROM_RESOURCE; idx++) {
			int ibits = ((pcibr_intr_t)intr_handle)->bi_ibits;
			int i;

			size = device_dev->resource[idx].end -
				device_dev->resource[idx].start;
			if (size == 0) continue;

			for (i=0; i<8; i++) {
				if (ibits & (1 << i) ) {
					sn_dma_flush_init(device_dev->resource[idx].start, 
							device_dev->resource[idx].end,
							idx,
							i,
							PCI_SLOT(device_dev->devfn));
				}
			}
		}

	}
#ifdef ajmtestintr
		{
			int slot = PCI_SLOT(device_dev->devfn);
			static int timer_set = 0;
			pcibr_intr_t	pcibr_intr = (pcibr_intr_t)intr_handle;
			pcibr_soft_t	pcibr_soft = pcibr_intr->bi_soft;
			extern void intr_test_handle_intr(int, void*, struct pt_regs *);

			if (!timer_set) {
				intr_test_set_timer();
				timer_set = 1;
			}
			intr_test_register_irq(irq, pcibr_soft, slot);
			request_irq(irq, intr_test_handle_intr,0,NULL, NULL);
		}
#endif
}

/*
 * linux_bus_cvlink() Creates a link between the Linux PCI Bus number 
 *	to the actual hardware component that it represents:
 *	/dev/hw/linux/busnum/0 -> ../../../hw/module/001c01/slab/0/Ibrick/xtalk/15/pci
 *
 *	The bus vertex, when called to devfs_generate_path() returns:
 *		hw/module/001c01/slab/0/Ibrick/xtalk/15/pci
 *		hw/module/001c01/slab/1/Pbrick/xtalk/12/pci-x/0
 *		hw/module/001c01/slab/1/Pbrick/xtalk/12/pci-x/1
 */
void
linux_bus_cvlink(void)
{
	char name[8];
	int index;
	
	for (index=0; index < MAX_PCI_XWIDGET; index++) {
		if (!busnum_to_pcibr_vhdl[index])
			continue;

		sprintf(name, "%x", index);
		(void) hwgraph_edge_add(linux_busnum, busnum_to_pcibr_vhdl[index], 
				name);
	}
}

/*
 * pci_bus_map_create() - Called by pci_bus_to_hcl_cvlink() to finish the job.
 *
 *	Linux PCI Bus numbers are assigned from lowest module_id numbers
 *	(rack/slot etc.) starting from HUB_WIDGET_ID_MAX down to 
 *	HUB_WIDGET_ID_MIN:
 *		widgetnum 15 gets lower Bus Number than widgetnum 14 etc.
 *
 *	Given 2 modules 001c01 and 001c02 we get the following mappings:
 *		001c01, widgetnum 15 = Bus number 0
 *		001c01, widgetnum 14 = Bus number 1
 *		001c02, widgetnum 15 = Bus number 3
 *		001c02, widgetnum 14 = Bus number 4
 *		etc.
 *
 * The rational for starting Bus Number 0 with Widget number 15 is because 
 * the system boot disks are always connected via Widget 15 Slot 0 of the 
 * I-brick.  Linux creates /dev/sd* devices(naming) strating from Bus Number 0 
 * Therefore, /dev/sda1 will be the first disk, on Widget 15 of the lowest 
 * module id(Master Cnode) of the system.
 *	
 */
static int 
pci_bus_map_create(vertex_hdl_t xtalk, int brick_type, char * io_moduleid)
{

	vertex_hdl_t xwidget = NULL;
	vertex_hdl_t pci_bus = NULL;
	xwidgetnum_t widgetnum;
	char pathname[128];
	graph_error_t rv;
	int bus;
	int basebus_num;
	extern void ioconfig_get_busnum(char *, int *);

	int bus_number;

	/*
	 * PCIX devices
	 * We number busses differently for PCI-X devices.
	 * We start from Lowest Widget on up ..
	 */

        (void) ioconfig_get_busnum((char *)io_moduleid, &basebus_num);

	for (widgetnum = HUB_WIDGET_ID_MIN; widgetnum <= HUB_WIDGET_ID_MAX; widgetnum++) {

		/* Do both buses */
		for ( bus = 0; bus < 2; bus++ ) {
			sprintf(pathname, "%d", widgetnum);
			xwidget = NULL;
			
			/*
			 * Example - /hw/module/001c16/Pbrick/xtalk/8 is the xwidget
			 *	     /hw/module/001c16/Pbrick/xtalk/8/pci-x/0 is the bus
			 *	     /hw/module/001c16/Pbrick/xtalk/8/pci-x/0/1 is device
			 */
			rv = hwgraph_traverse(xtalk, pathname, &xwidget);
			if ( (rv != GRAPH_SUCCESS) ) {
				if (!xwidget) {
					continue;
				}
			}
	
			if ( bus == 0 )
				sprintf(pathname, "%d/"EDGE_LBL_PCIX_0, widgetnum);
			else
				sprintf(pathname, "%d/"EDGE_LBL_PCIX_1, widgetnum);
			pci_bus = NULL;
			if (hwgraph_traverse(xtalk, pathname, &pci_bus) != GRAPH_SUCCESS)
				if (!pci_bus) {
					continue;
				}
	
			/*
			 * Assign the correct bus number and also the nasid of this 
			 * pci Xwidget.
			 * 
			 * Should not be any race here ...
			 */
			bus_number = basebus_num + bus + io_brick_map_widget(brick_type, widgetnum);
#ifdef DEBUG
			printk("bus_number %d basebus_num %d bus %d io %d pci_bus 0x%x brick_type %d \n", 
				bus_number, basebus_num, bus, 
				io_brick_map_widget(brick_type, widgetnum), pci_bus, brick_type);
#endif
			busnum_to_pcibr_vhdl[bus_number] = pci_bus;
	
			/*
			 * Pre assign DMA maps needed for 32 Bits Page Map DMA.
			 */
			busnum_to_atedmamaps[bus_number] = (void *) kmalloc(
				sizeof(struct sn_dma_maps_s) * MAX_ATE_MAPS, GFP_KERNEL);
			if (busnum_to_atedmamaps[bus_number] <= 0)
				BUG(); /* Cannot afford to run out of memory. */
	
			memset(busnum_to_atedmamaps[bus_number], 0x0, 
				sizeof(struct sn_dma_maps_s) * MAX_ATE_MAPS);
		}
	}

	/* AGP/CGbrick */

	for (widgetnum = HUB_WIDGET_ID_MIN; widgetnum <= HUB_WIDGET_ID_MAX; widgetnum++) {

		/* Do both buses */
		for ( bus = 0; bus < 2; bus++ ) {
			sprintf(pathname, "%d", widgetnum);
			xwidget = NULL;
			
			/*
			 * Example - /hw/module/001c16/slab/0/CGbrick/xtalk/15 is the xwidget
			 *	     /hw/module/001c16/slab/0/CGbrick/xtalk/15/agp/0 is the bus
			 *	     /hw/module/001c16/slab/0/CGbrick/xtalk/15/agp/0/1a is device
			 */
			rv = hwgraph_traverse(xtalk, pathname, &xwidget);
			if ( (rv != GRAPH_SUCCESS) ) {
				if (!xwidget) {
					continue;
				}
			}
	
			if ( bus == 0 )
				sprintf(pathname, "%d/"EDGE_LBL_AGP_0, widgetnum);
			else
				sprintf(pathname, "%d/"EDGE_LBL_AGP_1, widgetnum);
			pci_bus = NULL;
			if (hwgraph_traverse(xtalk, pathname, &pci_bus) != GRAPH_SUCCESS)
				if (!pci_bus) {
					continue;
				}
	
			/*
			 * Assign the correct bus number and also the nasid of this 
			 * pci Xwidget.
			 * 
			 * Should not be any race here ...
			 */
			bus_number = basebus_num + bus + io_brick_map_widget(brick_type, widgetnum);
#ifdef DEBUG
			printk("bus_number %d basebus_num %d bus %d io %d pci_bus 0x%x\n", 
				bus_number, basebus_num, bus, 
				io_brick_map_widget(brick_type, widgetnum), pci_bus);
#endif
			busnum_to_pcibr_vhdl[bus_number] = pci_bus;

			/*
			 * Pre assign DMA maps needed for 32 Bits Page Map DMA.
			 */
			busnum_to_atedmamaps[bus_number] = (void *) kmalloc(
				sizeof(struct sn_dma_maps_s) * MAX_ATE_MAPS, GFP_KERNEL);
			if (busnum_to_atedmamaps[bus_number] <= 0)
				BUG(); /* Cannot afford to run out of memory */
	
			memset(busnum_to_atedmamaps[bus_number], 0x0, 
				sizeof(struct sn_dma_maps_s) * MAX_ATE_MAPS);
		}
	}
        return(0);
}

/*
 * pci_bus_to_hcl_cvlink() - This routine is called after SGI IO Infrastructure   
 *      initialization has completed to set up the mappings between Xbridge
 *      and logical pci bus numbers.  We also set up the NASID for each of these
 *      xbridges.
 *
 *      Must be called before pci_init() is invoked.
 */
int
pci_bus_to_hcl_cvlink(void) 
{

	vertex_hdl_t devfs_hdl = NULL;
	vertex_hdl_t xtalk = NULL;
	int rv = 0;
	char *name;
	char *tmp_name;
	int i, ii, j;
	char *brick_name;
	extern void ioconfig_bus_new_entries(void);
	extern int iobrick_type_get_nasid(nasid_t);

	name = kmalloc(256, GFP_KERNEL);
	if (!name)
		BUG();

	tmp_name = kmalloc(256, GFP_KERNEL);
	if (!name)
		 BUG();

	/*
	 * Figure out which IO Brick is connected to the Compute Bricks.
	 */
	for (i = 0; i < nummodules; i++) {
		extern int iomoduleid_get(nasid_t);
		moduleid_t iobrick_id;
		nasid_t nasid = -1;
		int n = 0;

		for ( n = 0; n <= MAX_SLABS; n++ ) {
			if (modules[i]->nodes[n] == -1)
				continue; /* node is not alive in module */

			nasid = cnodeid_to_nasid(modules[i]->nodes[n]);
			iobrick_id = iomoduleid_get(nasid);
			if ((int)iobrick_id > 0) { /* Valid module id */
				char name[12];
				memset(name, 0, 12);
				format_module_id((char *)&(modules[i]->io[n].moduleid), iobrick_id, MODULE_FORMAT_BRIEF);
				modules[i]->io[n].iobrick_type = (uint64_t)iobrick_type_get_nasid(nasid);
			}
		}
	}
				
	devfs_hdl = hwgraph_path_to_vertex("hw/module");
	for (i = 0; i < nummodules ; i++) {
	    for ( j = 0; j < 4; j++ ) {
		if ( j == 0 )
			brick_name = EDGE_LBL_IXBRICK;
		else if ( j == 1 )
			brick_name = EDGE_LBL_PXBRICK;
		else if ( j == 2 )
			brick_name = EDGE_LBL_OPUSBRICK;
		else	/* 3 */
			brick_name = EDGE_LBL_CGBRICK;

		for ( ii = 0; ii <= MAX_SLABS ; ii++ ) {
			if (modules[i]->nodes[ii] == -1)
				continue; /* Missing slab */

			memset(name, 0, 256);
			memset(tmp_name, 0, 256);
			format_module_id(name, modules[i]->id, MODULE_FORMAT_BRIEF);
			sprintf(tmp_name, "/slab/%d/%s/xtalk", geo_slab(modules[i]->geoid[ii]), brick_name);
			strcat(name, tmp_name);
			xtalk = NULL;
			rv = hwgraph_edge_get(devfs_hdl, name, &xtalk);
			if ( rv == 0 ) 
				pci_bus_map_create(xtalk, (int)modules[i]->io[ii].iobrick_type, (char *)&(modules[i]->io[ii].moduleid));
		}
	    }
	}

	kfree(name);
	kfree(tmp_name);

	/*
	 * Create the Linux PCI bus number vertex link.
	 */
	(void)linux_bus_cvlink();
	(void)ioconfig_bus_new_entries();

	return(0);
}
