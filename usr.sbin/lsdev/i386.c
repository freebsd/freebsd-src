#include "lsdev.h"
#include <stdio.h>
#include <string.h>

static void print_isa(struct devconf *);
static void print_eisa(struct devconf *);
static void print_pci(struct devconf *);
static void print_scsi(struct devconf *);
static void print_disk(struct devconf *);

void
print(struct devconf *dc)
{
	if(vflag)
		printf("%d: ", dc->dc_number);

	switch(dc->dc_devtype) {
	case MDDT_CPU:
		printf("CPU on %s", dc->dc_parent);
		break;
	case MDDT_ISA:
		if(dc->dc_datalen >= ISA_EXTERNALLEN) {
			print_isa(dc);
		} else {
printit:
			printf("%s%d on %s",
			       dc->dc_name, dc->dc_unit, dc->dc_parent);
		}
		break;
	case MDDT_EISA:
		if(dc->dc_datalen >= EISA_EXTERNALLEN) {
			print_eisa(dc);
		} else {
			goto printit;
		}
		break;
	case MDDT_PCI:
		if(dc->dc_datalen >= PCI_EXTERNALLEN) {
			print_pci(dc);
		} else {
			goto printit;
		}
		break;
	case MDDT_SCSI:
		if(dc->dc_datalen >= SCSI_EXTERNALLEN) {
			print_scsi(dc);
		} else {
			goto printit;
		}
		break;
	case MDDT_DISK:
		if(dc->dc_datalen >= DISK_EXTERNALLEN) {
			print_disk(dc);
		} else {
			goto printit;
		}
		break;
		
	default:
		if(dc->dc_devtype >= NDEVTYPES) {
			printf("%s%d (#%d) on %s",
			       dc->dc_name, dc->dc_unit, dc->dc_devtype,
			       dc->dc_parent);
		} else {
			printf("%s%d (%s) on %s",
			       dc->dc_name, dc->dc_unit, 
			       devtypes[dc->dc_devtype], dc->dc_parent);
		}
		break;
	}
	fputc('\n', stdout);
}

static void 
print_isa(struct devconf *dc)
{
	struct isa_device *id = (struct isa_device *)dc->dc_data;

	printf("%s%d on %s", dc->dc_name, dc->dc_unit, dc->dc_parent);

	if(vflag) {
		printf(" (id %d)", id->id_id);
	}

	if(id->id_iobase) {
		if(id->id_iobase < 0) {
			printf(" port ?");
		} else {
			printf(" port 0x%x", id->id_iobase);
		}
	}

	if(id->id_irq) {
		int bit = ffs(id->id_irq) - 1;

		if(id->id_irq & ~(1 << bit)) {
			printf(" irq ?");
		} else {
			printf(" irq %d", bit);
		}
	}

	if(id->id_drq) {
		if(id->id_drq < 0) {
			printf(" drq ?");
		} else {
			printf(" drq %d", id->id_drq);
		}
	}

	if(id->id_maddr) {
		if((unsigned long)id->id_maddr == ~0UL) {
			printf(" iomem ?");
		} else {
			printf(" iomem 0x%lx", (unsigned long)id->id_maddr);
		}
	}

	if(id->id_msize) {
		if(id->id_msize < 0) {
			printf(" iosiz ?", id->id_msize);
		} else {
			printf(" iosiz %d", id->id_msize);
		}
	}

	if(id->id_flags) {
		printf(" flags 0x%x", id->id_flags);
	}
}

static void
print_eisa(struct devconf *dc)
{
	int *slotp = (int *)&dc->dc_data[ISA_EXTERNALLEN];
	print_isa(dc);
	if(vflag) {
		printf(" (slot %d)", *slotp);
	}
}

static void
print_pci(struct devconf *dc)
{
	struct pci_device *pd = (struct pci_device *)dc->dc_data;

	/*
	 * Unfortunately, the `pci_device' struct is completely
	 * useless.  We will have to develop a unique structure
	 * for this task eventually, unless the existing one can
	 * be made to serve.
	 */

	printf("%s%d on %s", dc->dc_name, dc->dc_unit, dc->dc_parent);
}

static void
print_scsi(struct devconf *dc)
{
	struct scsi_link *sl = (struct scsi_link *)dc->dc_data;

	printf("%s%d on SCSI bus %d:%d:%d",
	       dc->dc_name, dc->dc_unit, sl->scsibus, sl->target,
	       sl->lun);
	if(vflag) {
		if(sl->flags & SDEV_MEDIA_LOADED)
			printf(" (ready)");
		if(sl->flags & SDEV_OPEN)
			printf(" (open)");
		if(sl->flags & SDEV_BOUNCE)
			printf(" (bounce)");
	}
}

static void
print_disk(struct devconf *dc)
{
	int *slavep = (int *)dc->dc_data;

	printf("%s%d on %s drive %d",
	       dc->dc_name, dc->dc_unit, dc->dc_parent, *slavep);
}

