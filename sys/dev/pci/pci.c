/**************************************************************************
**
**  $Id: pci.c,v 2.1 94/09/16 08:01:20 wolf Rel $
**
**  General subroutines for the PCI bus on 80*86 systems.
**  pci_configure ()
**
**  386bsd / FreeBSD
**
**-------------------------------------------------------------------------
**
** Copyright (c) 1994 Wolfgang Stanglmeier.  All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
**-------------------------------------------------------------------------
*/

#include <pci.h>
#if NPCI > 0

/*========================================================
**
**	Configuration
**
**========================================================
*/

/*
**	maximum number of devices which share one interrupt line
*/

#ifndef PCI_MAX_DPI
#define PCI_MAX_DPI	(4)
#endif  /*PCI_MAX_DPI*/


/*========================================================
**
**	#includes  and  declarations
**
**========================================================
*/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>

#include <vm/vm.h>
#include <vm/vm_param.h>

#include <i386/isa/icu.h>
#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>

#include <i386/pci/pci.h>
#include <i386/pci/pci_device.h>
#include <i386/pci/pcibios.h>

/*
**	Function prototypes missing in system headers
*/

#if ! (__FreeBSD__ >= 2)
extern	pmap_t pmap_kernel(void);
static	vm_offset_t pmap_mapdev (vm_offset_t paddr, vm_size_t vsize);
#endif


/*========================================================
**
**	Autoconfiguration (of isa bus)
**
**========================================================
*/

/*
**      per slot data structure for passing interupts..
*/

static struct {
	u_short number;
	u_short isanum;
	struct {
		int (*proc)(int dev);
		dev_t unit;
	} vector[PCI_MAX_DPI];
} pcidata [NPCI];

/*
**	check device ready
*/
static int pciprobe (struct isa_device *dev)
{
	if (dev->id_unit >= NPCI)
		return (0);

	if (!pci_conf_mode())
		return (0);

	return (-1);
}

/*
**	initialize the driver structure
*/
static int pciattach (struct isa_device *isdp)
{
	pcidata[isdp->id_unit].number = 0;
	pcidata[isdp->id_unit].isanum = ffs(isdp->id_irq)-1;
	return (1);
}

/*
**      ISA driver structure
*/

struct isa_driver pcidriver = {
	pciprobe,
	pciattach,
	"pci"
};

/*========================================================
**
**	Interrupt forward from isa to pci devices.
**
**========================================================
*/


void pciintr (int unit)
{
	u_short i;
	if (unit >= NPCI) return;
	for (i=0; i<pcidata[unit].number; i++) {
		(void)(*pcidata[unit].vector[i].proc)
			(pcidata[unit].vector[i].unit);
	};
}


/*========================================================
**
**	Autoconfiguration of pci devices.
**
**	This is reverse to the isa configuration.
**	(1) find a pci device.
**	(2) look for a driver.
**
**========================================================
*/

/*--------------------------------------------------------
**
**	The pci devices can be mapped to any address.
**	As default we start at the last gigabyte.
**
**--------------------------------------------------------
*/

#ifndef PCI_PMEM_START
#define PCI_PMEM_START 0xc0000000
#endif

static vm_offset_t pci_paddr = PCI_PMEM_START;

/*---------------------------------------------------------
**
**	pci_configure ()
**
**---------------------------------------------------------
*/

static void not_supported (pcici_t tag, u_long type);

void pci_configure()
{
	u_char	device,last_device;
	u_short	bus,last_bus;
	pcici_t	tag;
	pcidi_t type;
	u_long	data;
	int	unit;
	int	intpin;
	int	isanum;
	int	pci_mode;

	struct pci_driver *drp;
	struct pci_device *dvp;

	/*
	**	check pci bus present
	*/

	pci_mode = pci_conf_mode ();
	if (!pci_mode) return;
	last_bus = pci_last_bus ();
	last_device = pci_mode==1 ? 31 : 15;

	/*
	**	hello world ..
	*/

#ifndef PCI_QUIET
	printf ("pci*: mode=%d, scanning bus 0..%d, device 0..%d.\n",
		pci_mode, last_bus, last_device);
#endif

	for (bus=0;bus<=last_bus; bus++)
	    for (device=0; device<=last_device; device ++) {
		tag = pcitag (bus, device, 0);
		type = pci_conf_read (tag, PCI_ID_REG);

		if ((!type) || (type==0xfffffffful)) continue;

		/*
		**	lookup device in ioconfiguration:
		*/

		for (dvp = pci_devtab; dvp->pd_device_id; dvp++) {
			if (dvp->pd_device_id == type) break;
		};
		drp = dvp->pd_driver;

		if (!dvp->pd_device_id) {

			/*
			**	not found
			**	try to dig out some information.
			**
			**	By Garrett Wollman
			**	<wollman@halloran-eldar.lcs.mit.edu>
			*/

			int data = pci_conf_read(tag, PCI_CLASS_REG);
			vm_offset_t va;
			vm_offset_t pa;
			int reg;

			switch (data & PCI_CLASS_MASK) {

			case PCI_CLASS_PREHISTORIC:
				if ((data & PCI_SUBCLASS_MASK)
					!= PCI_SUBCLASS_PREHISTORIC_VGA)
					break;

			case PCI_CLASS_DISPLAY:
				for (reg = PCI_MAP_REG_START;
				     reg < PCI_MAP_REG_END;
				     reg += 4) {
					data = pci_map_mem(tag, reg, &va, &pa);
					if (data == 0)
						printf (
		"pci%d:%d: mapped VGA-like device at physaddr 0x%lx\n",
						bus, device, (u_long)pa);

				}
				continue;
			};
#ifndef PCI_QUIET
			printf("pci%d:%d: ", bus, device);
			not_supported (tag, type);
#endif
		};

		if (!drp) {
			if(dvp->pd_flags & PDF_LOADABLE) {
				printf("%s: loadable device on pci%d:%d\n",
				       dvp->pd_name, bus, device);
			}
			continue;
		}

		/*
		**	found it.
		**	probe returns the device unit.
		*/

		unit = (*drp->probe) (tag);

		if (unit<0) {
			printf ("%s <%s>: probe failed on pci%d:%d\n",
				dvp->pd_name, drp->name, bus, device);
			continue;
		};

		printf ("%s%d <%s>", dvp->pd_name, unit, drp->name);

		/*
		**	install interrupts
		*/

		data = pci_conf_read (tag, PCI_INTERRUPT_REG);
		intpin = PCI_INTERRUPT_PIN_EXTRACT(data);
		if (intpin) {
			int idx=NPCI;

			/*
			**  Usage of int line register (if set by bios)
			**  Matt Thomas	<thomas@lkg.dec.com>
			*/

			isanum = PCI_INTERRUPT_LINE_EXTRACT(data);
			if (isanum) {

				/*
				**	@INT@	FIXME!!!
				**
				**	Should try to use "register_interupt"
				**	at this point.
				*/

				printf (" line=%d", isanum);
				for (idx = 0; idx < NPCI; idx++) {
					if (pcidata[idx].isanum == isanum)
						break;
				};
			};

			/*
			**	Or take the device number as index ...
			*/

			if (idx >= NPCI) idx = device;

			/*
			**	And install the interrupt.
			*/

			if (idx<NPCI) {
				u_short entry = pcidata[idx].number;
				printf (" irq %c", 0x60+intpin);
				if (entry < PCI_MAX_DPI) {
					pcidata[idx].vector[entry].proc = drp->intr;
					pcidata[idx].vector[entry].unit = unit;
					entry++;
				};
				printf (" isa=%d [%d]",pcidata[idx].isanum, entry);
				pcidata[idx].number=entry;
			} else {
				printf (" no int");
			};
		};

		/*
		**	enable memory access
		*/

		data = pci_conf_read (tag, PCI_COMMAND_STATUS_REG)
			& 0xffff | PCI_COMMAND_MEM_ENABLE;

		pci_conf_write (tag, (u_char) PCI_COMMAND_STATUS_REG, data);

		/*
		**	attach device
		**	may produce additional log messages,
		**	i.e. when installing subdevices.
		*/

		printf (" on pci%d:%d\n", bus, device);

		(void) (*drp->attach) (tag);
	}

#ifndef PCI_QUIET
	printf ("pci uses physical addresses from 0x%lx to 0x%lx\n",
			(u_long)PCI_PMEM_START, (u_long)pci_paddr);
#endif
}

/*-----------------------------------------------------------------------
**
**	Map device into port space.
**
**	PCI-Specification:  6.2.5.1: address maps
**
**-----------------------------------------------------------------------
*/

int pci_map_port (pcici_t tag, u_long reg, u_short* pa)
{
	/*
	**	@MAPIO@ not yet implemented.
	*/
	return (ENOSYS);
}

/*-----------------------------------------------------------------------
**
**	Map device into virtual and physical space
**
**	PCI-Specification:  6.2.5.1: address maps
**
**-----------------------------------------------------------------------
*/

int pci_map_mem (pcici_t tag, u_long reg, vm_offset_t* va, vm_offset_t* pa)
{
	u_long data;
	vm_size_t vsize;
	vm_offset_t vaddr;

	/*
	**	sanity check
	*/

	if (reg <= PCI_MAP_REG_START || reg >= PCI_MAP_REG_END || (reg & 3))
		return (EINVAL);

	/*
	**	get size and type of memory
	**
	**	type is in the lowest four bits.
	**	If device requires 2^n bytes, the next
	**	n-4 bits are read as 0.
	*/

	pci_conf_write (tag, reg, 0xfffffffful);
	data = pci_conf_read (tag, reg);

	switch (data & 0x0f) {

	case PCI_MAP_MEMORY_TYPE_32BIT:	/* 32 bit non cachable */
		break;

	default:	/* unknown */
		return (EINVAL);
	};

	/*
	**	mask out the type,
	**	and round up to a page size
	*/

	vsize = round_page (-(data &  PCI_MAP_MEMORY_ADDRESS_MASK));

	if (!vsize) return (EINVAL);

	/*
	**	align physical address to virtual size
	*/

	if (data = pci_paddr % vsize)
		pci_paddr += vsize - data;

	vaddr = pmap_mapdev (pci_paddr, vsize);

	if (!vaddr) return (EINVAL);

#ifndef PCI_QUIET
	/*
	**	display values.
	*/

	printf ("\treg%d: virtual=0x%lx physical=0x%lx\n",
		reg, (u_long)vaddr, (u_long)pci_paddr);
#endif

	/*
	**	return them to the driver
	*/

	*va = vaddr;
	*pa = pci_paddr;

	/*
	**	set device address
	*/

	pci_conf_write (tag, reg, pci_paddr);

	/*
	**	and don't forget to increment pci_paddr
	*/

	pci_paddr += vsize;

	return (0);
}

/*-----------------------------------------------------------
**
**	Mapping of physical to virtual memory
**
**-----------------------------------------------------------
*/

#if ! (__FreeBSD__ >= 2)

extern vm_map_t kernel_map;

static	vm_offset_t pmap_mapdev (vm_offset_t paddr, vm_size_t vsize)
{
	vm_offset_t	vaddr,value;
	u_long		result;

	vaddr  = vm_map_min (kernel_map);

	result = vm_map_find (kernel_map, (void*)0, (vm_offset_t) 0,
				&vaddr, vsize, TRUE);

	if (result != KERN_SUCCESS) {
		printf (" vm_map_find failed(%d)\n", result);
		return (0);
	};

	/*
	**	map physical
	*/

	value = vaddr;
	while (vsize >= NBPG) {
		pmap_enter (pmap_kernel(), vaddr, paddr,
				VM_PROT_READ|VM_PROT_WRITE, TRUE);
		vaddr += NBPG;
		paddr += NBPG;
		vsize -= NBPG;
	};
	return (value);
}
#endif

/*-----------------------------------------------------------
**
**	Display of unknown devices.
**
**-----------------------------------------------------------
*/
struct vt {
	u_short	ident;
	char*	name;
};

static struct vt VendorTable[] = {
	{0x1002, "ATI TECHNOLOGIES INC"},
	{0x1011, "DIGITAL EQUIPMENT COMPANY"},
	{0x101A, "NCR"},
	{0x102B, "MATROX"},
	{0x1045, "OPTI"},
	{0x5333, "S3 INC."},
	{0x8086, "INTEL CORPORATION"},
	{0,0}
};

static const char *const majclasses[] = {
	"old", "storage", "network", "display",
	"multimedia", "memory", "bridge" 
};

void not_supported (pcici_t tag, u_long type)
{
	u_char	reg;
	u_long	data;
	struct vt * vp;

	/*
	**	lookup the names.
	*/

	for (vp=VendorTable; vp->ident; vp++)
		if (vp->ident == (type & 0xffff))
			break;

	/*
	**	and display them.
	*/

	if (vp->ident) printf (vp->name);
		else   printf ("vendor=0x%lx", type & 0xffff);

	printf (", device=0x%lx", type >> 16);

	data = (pci_conf_read(tag, PCI_CLASS_REG) >> 24) & 0xff;
	if (data < sizeof(majclasses) / sizeof(majclasses[0]))
		printf(", class=%s", majclasses[data]);

	printf (" [not supported]\n");

	for (reg=PCI_MAP_REG_START; reg<PCI_MAP_REG_END; reg+=4) {
		data = pci_conf_read (tag, reg);
		if (!data) continue;
		switch (data&7) {

		case 1:
		case 5:
			printf ("	map(%lx): io(%lx)\n", reg, data & ~3);
			break;
		case 0:
			printf ("	map(%lx): mem32(%lx)\n", reg, data & ~7);
			break;
		case 2:
			printf ("	map(%lx): mem20(%lx)\n", reg, data & ~7);
			break;
		case 4:
			printf ("	map(%lx): mem64(%lx)\n", reg, data & ~7);
			break;
		}
	}
}
#endif
