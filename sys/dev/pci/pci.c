/**************************************************************************
**
**  $Id: pci.c,v 2.0.0.8 94/08/21 19:57:39 wolf Exp $
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
**
**  $Log:	pci.c,v $
**  Revision 2.0.0.8  94/08/21  19:57:39  wolf
**  Unneeded declarations removed (FreeBSD2.0)
**  
**  Revision 2.0.0.7  94/08/21  19:25:54  wolf
**  pci_intr simplified.
**  new not_supported() function.
**  Vendor and device ids moved into tables.
**  
**  Revision 2.0.0.6  94/08/18  22:58:23  wolf
**  Symbolic names for pci configuration space registers.
**  last_device: from configuration mode
**  last_bus: from pcibios.
**  PCI_MAX_DPI: changed to 4 (settable by config)
**  interrupt configuration by line or pin
**  
**  Revision 2.0.0.5  94/08/11  19:04:10  wolf
**  display of interrupt line configuration register.
**  
**  Revision 2.0.0.4  94/08/01  20:36:28  wolf
**  Tiny clean up.
**  
**  Revision 2.0.0.3  94/08/01  18:52:33  wolf
**  New vendor entry:  S3.
**  Scan pci busses #0..#255 as default.
**  Number of scanned busses and devices settable as option.
**  Show these numbers before starting the scan.
**  
**  Revision 2.0.0.2  94/07/27  09:27:19  wolf
**  New option PCI_QUIET: suppress log messages.
**  
**  Revision 2.0.0.1  94/07/19  19:06:44  wolf
**  New vendor entry:  MATROX
**  
**  Revision 2.0  94/07/10  15:53:29  wolf
**  FreeBSD release.
**  
**  Revision 1.0  94/06/07  20:02:19  wolf
**  Beta release.
**  
***************************************************************************
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

#include <types.h>
#include <cdefs.h>
#include <errno.h>
#include <param.h>

#include <vm/vm.h>
#include <vm/vm_param.h>

#include <i386/isa/icu.h>
#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>

#include <i386/pci/pci.h>
#include <i386/pci/pci_device.h>
#include <i386/pci/pcibios.h>


char ident_pci_c[] =
	"\n$Id: pci.c,v 2.0.0.8 94/08/21 19:57:39 wolf Exp $\n"
	"Copyright (c) 1994, Wolfgang Stanglmeier\n";

/*
**	Function prototypes missing in system headers
*/

extern int printf();
extern int ffs();
#if ! (__FreeBSD__ >= 2)
extern pmap_t pmap_kernel(void);
#endif


/*========================================================
**
**	Autoconfiguration (of isa bus)
**
**========================================================
*/

/*
**      per device (interrupt) data structure.
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
		(void)(*pcidata[unit].vector[i].proc)(pcidata[unit].vector[i].unit);
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
	printf ("PCI configuration mode %d.\n", pci_mode);
	printf ("Scanning device 0..%d on pci bus 0..%d "
		"($Revision: 2.0.0.8 $)\n",
		last_device, last_bus);
#endif

	for (bus=0;bus<=last_bus; bus++)
	    for (device=0; device<=last_device; device ++) {
		tag = pcitag (bus, device, 0);
		type = pci_conf_read (tag, PCI_ID_REG);

		if ((!type) || (type==0xfffffffful)) continue;

		/*
		**	lookup device in ioconfiguration:
		*/

		for (dvp = pci_devtab; drp=dvp->pd_driver; dvp++) {
			if (drp->device_id == type) break;
		};

#ifdef PCI_QUIET
		if (!drp) continue;
#endif
		printf ("on pci%d:%d ", bus, device);
#ifndef PCI_QUIET
		if (!drp) {
			not_supported (tag, type);
			continue;
		};
#endif

		/*
		**	found it.
		**	probe returns the device unit.
		*/

		printf ("<%s>", drp -> vendor);

		unit = (*drp->probe) (tag);

		if (unit<0) {
			printf (" probe failed.\n");
			continue;
		};

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
				printf (" il=%d", isanum);
				for (idx = 0; idx < NPCI; idx++) {
					if (pcidata[idx].isanum == isanum)
						break;
				};
			};

			/*
			**	Or believe to the interrupt pin register.
			*/

			if (idx >= NPCI) idx = intpin-1;

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
				printf (" not installed");
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

		printf (" as %s%d\n", drp->name,unit);
		(void) (*drp->attach) (tag);

	};

	printf ("pci uses physical addresses from %x to %x\n",
			PCI_PMEM_START, pci_paddr);
}

/*-----------------------------------------------------------------------
**
**	Map device into port space.
**
**	PCI-Specification:  6.2.5.1: address maps
**
**-----------------------------------------------------------------------
*/

extern vm_map_t kernel_map;

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
	u_long data, result;
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

	printf ("          memory size=0x%x", vsize);

	if (!vsize) return (EINVAL);

	/*
	**	try to map device to virtual space
	*/

	vaddr  = vm_map_min (kernel_map);

	result = vm_map_find (kernel_map, (void*)0, (vm_offset_t) 0,
				&vaddr, vsize, TRUE);

	if (result != KERN_SUCCESS) {
		printf (" vm_map_find failed(%d)\n", result);
		return (ENOMEM);
	};

	/*
	**	align physical address to virtual size
	*/

	if (data = pci_paddr % vsize)
		pci_paddr += vsize - data;

	/*
	**	display values.
	*/

	printf (" virtual=0x%x physical=0x%x\n", vaddr, pci_paddr);

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
	**	map physical
	*/

	while (vsize >= NBPG) {
		pmap_enter (pmap_kernel(), vaddr, pci_paddr,
				VM_PROT_READ|VM_PROT_WRITE, TRUE);
		vaddr     += NBPG;
		pci_paddr += NBPG;
		vsize     -= NBPG;
	};

	return (0);
}

struct vt {
	u_short	ident;
	char*	name;
};

struct dt {
	u_long	ident;
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

static struct dt DeviceTable[] = {
	{0x04848086, " 82378IB pci-isa bridge"},
	{0x04838086, " 82424ZX cache dram controller"},
	{0x04828086, " 82375EB pci-eisa bridge"},
	{0x04A38086, " 82434LX pci cache memory controller"},
	{0,0}
};

void not_supported (pcici_t tag, u_long type)
{
	u_char	reg;
	u_long	data;
	struct vt * vp;
	struct dt * dp;

	/*
	**	lookup the names.
	*/

	for (vp=VendorTable; vp->ident; vp++)
		if (vp->ident == (type & 0xffff))
			break;

	for (dp=DeviceTable; dp->ident; dp++)
		if (dp->ident == type)
			break;

	/*
	**	and display them.
	*/

	if (vp->ident) printf (vp->name);
		else   printf ("vendor=%x", type & 0xffff);

	if (dp->ident) printf (dp->name);
		else   printf (", device=%x", type >> 16);

	printf (" [not supported]\n");

	for (reg=PCI_MAP_REG_START; reg<PCI_MAP_REG_END; reg+=4) {
		data = pci_conf_read (tag, reg);
		if (!data) continue;
		switch (data&7) {

		case 1:
		case 5:
			printf ("	map(%x): io(%x)\n", reg, data & ~3);
			break;
		case 0:
			printf ("	map(%x): mem32(%x)\n", reg, data & ~7);
			break;
		case 2:
			printf ("	map(%x): mem20(%x)\n", reg, data & ~7);
			break;
		case 4:
			printf ("	map(%x): mem64(%x)\n", reg, data & ~7);
			break;
		};
	};
}
#endif
