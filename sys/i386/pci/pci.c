/**************************************************************************
**
**  $Id: pci.c,v 2.0.0.1 94/07/19 19:06:44 wolf Exp $
**
**  General subroutines for the PCI bus on 80*86 systems.
**  pci_configure ()
**
**-------------------------------------------------------------------------
**
**  Copyright (c) 1994	Wolfgang Stanglmeier, Koeln, Germany
**			<wolf@dentaro.GUN.de>
**
**  This is a beta version - use with care.
**
**-------------------------------------------------------------------------
**
**  $Log:	pci.c,v $
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

#define PCI_MAX_DPI	1

/*
**	there may be up to 255 busses on pci.
**	don't probe them all :-)
*/

#define	FIRST_BUS	0
#define	LAST_BUS	0

/*
**	there may be up to 32 devices per bus.
**	do probe them all ;-)
*/

#define	FIRST_DEVICE	0
#define	LAST_DEVICE	31

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


static char ident[] =
	"\n$Id: pci.c,v 2.0.0.1 94/07/19 19:06:44 wolf Exp $\n"
	"Copyright (c) 1994, Wolfgang Stanglmeier\n";

/*
**	Function prototypes missing in system headers
*/

extern int printf();
extern int ffs();
extern pmap_t pmap_kernel(void);

/*
**	function prototypes
*/

int	pci_map_mem   
		(pcici_t tag, u_long reg, u_long *va, vm_offset_t *pa);
int	pci_map_port  (pcici_t tag, u_long reg, u_short* pa);
void	pci_configure (void);

/*========================================================
**
**	Autoconfiguration (of isa bus)	(Free/386)
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

#ifndef __NetBSD__

/*
**	check device ready
*/
static int pciprobe (struct isa_device *dev)
{
	if (dev->id_unit >= NPCI)
		return (0);

	if (!pci_conf_mode())
		return (0);

	return (1);
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

#if defined (GENERICAH) || defined (GENERICBT) || defined (GENERICNCR) \
	|| defined (GENERICAHA) || defined (GENERICISA)
	for (unit=0; unit < NPCI; unit++)
#endif

	for (i=0; i<pcidata[unit].number; i++) {
		(void)(*pcidata[unit].vector[i].proc)(pcidata[unit].vector[i].unit);
	};
}

#endif /* __NetBSD__ */

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

void pci_configure()
{
	u_char	bus, device, reg;
	pcici_t	tag;
	pcidi_t type;
	u_long	data;
	int	unit;
	int	intpin;
	int	pci_mode;

	struct pci_driver *drp;
	struct pci_device *dvp;

	/*
	**	check pci bus present
	*/

	pci_mode = pci_conf_mode ();
	if (!pci_mode) return;

	/*
	**	hello world ..
	*/

	printf ("PCI configuration mode %d.\n", pci_mode);

	for (bus=FIRST_BUS;bus<=LAST_BUS; bus++)
	    for (device=FIRST_DEVICE; device<=LAST_DEVICE; device ++) {
		tag = pcitag (bus, device, 0);
		type = pci_conf_read (tag, 0);

		if ((!type) || (type==0xfffffffful)) continue;
		printf ("on pci%d:%d ", bus, device);

		/*
		**	lookup device in ioconfiguration:
		*/

		for (dvp = pci_devtab; drp=dvp->pd_driver; dvp++) {
			if (drp->device_id == type) break;
		};

		if (!drp) {

			/*
			**	not found
			*/

			switch (type & 0xffff) {

			case 0x1002:
				printf ("ATI TECHNOLOGIES INC");
				break;
			case 0x101A:
				printf ("NCR");
				break;
			case 0x102B:
				printf ("MATROX");
				break;
			case 0x1045:
				printf ("OPTI");
				break;
			case 0x8086:
				printf ("INTEL CORPORATION");
				break;

			default:
				printf ("vendor=%x", type & 0xffff);
			};

			switch (type) {

			case 0x04848086:
				printf (" 82378IB pci-isa bridge");
				break;
			case 0x04838086:
				printf (" 82424ZX cache dram controller");
				break;
			case 0x04828086:
				printf (" 82375EB pci-eisa bridge");
				break;
			case 0x04A38086:
				printf (" 82434LX pci cache memory controller");
				break;
			default:
				printf (", device=%x", type >> 16);
			};
			printf (" [not supported]\n");

			for (reg=0x10; reg<=0x20; reg+=4) {
				data = pci_conf_read (tag, reg);
				if (!data) continue;
				switch (data&7) {

				case 1:
				case 5:
					printf ("	map(%x): io(%x)\n",
						reg, data & ~3);
					break;
				case 0:
					printf ("	map(%x): mem32(%x)\n",
						reg, data & ~7);
					break;
				case 2:
					printf ("	map(%x): mem20(%x)\n",
						reg, data & ~7);
					break;
				case 4:
					printf ("	map(%x): mem64(%x)\n",
						reg, data & ~7);
					break;
				};
			};
			continue;
		};

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

		intpin = (pci_conf_read (tag, 0x3c) >> 8) & 0xff;
		if (intpin) {
			printf (" irq %c", 0x60+intpin);
			intpin--;
			if (intpin < NPCI) {
				u_short entry = pcidata[intpin].number;
				if (entry < PCI_MAX_DPI) {
					pcidata[intpin].vector[entry].proc = drp->intr;
					pcidata[intpin].vector[entry].unit = unit;
					entry++;
				};
				printf (" isa=%d [%d]",pcidata[intpin].isanum, entry);
				pcidata[intpin].number=entry;
			} else printf (" not installed");
		};

		/*
		**	enable memory access
		*/
		data = pci_conf_read (tag, 0x04) & 0xffff | 0x0002;
		pci_conf_write (tag, (u_char) 0x04, data);

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
**	Map device into virtual and physical space
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


int pci_map_mem (pcici_t tag, u_long reg, vm_offset_t* va, vm_offset_t* pa)
{
	u_long data, result;
	vm_size_t vsize;
	vm_offset_t vaddr;

	/*
	**	sanity check
	*/

	if (reg <= 0x10 || reg >= 0x20 || (reg & 3))
		return (EINVAL);

	/*
	**	get size and type of memory
	*/

	pci_conf_write (tag, reg, 0xfffffffful);
	data = pci_conf_read (tag, reg);

	switch (data & 0x0f) {

	case 0x0:	/* 32 bit non cachable */
		break;

	default:	/* unknown */
		return (EINVAL);
	};

	vsize = round_page (-(data & 0xfffffff0));

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
#endif
