/*-
 * Copyright (c) 2016, Anish Gupta (anish@freebsd.org)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_acpi.h"
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>

#include <machine/vmparam.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <dev/acpica/acpivar.h>

#include "io/iommu.h"
#include "amdvi_priv.h"

device_t *ivhd_devs;			/* IVHD or AMD-Vi device list. */
int	ivhd_count;			/* Number of IVHD or AMD-Vi devices. */

extern int amdvi_ptp_level;		/* Page table levels. */

typedef int (*ivhd_iter_t)(ACPI_IVRS_HEADER * ptr, void *arg);

/*
 * Iterate IVRS table for IVHD and IVMD device type.
 */
static void
ivrs_hdr_iterate_tbl(ivhd_iter_t iter, void *arg)
{
	ACPI_TABLE_IVRS *ivrs;
	ACPI_IVRS_HEADER *ivrs_hdr, *end;
	ACPI_STATUS status;

	status = AcpiGetTable(ACPI_SIG_IVRS, 1, (ACPI_TABLE_HEADER **)&ivrs);
	if (ACPI_FAILURE(status))
		return;

	if (ivrs->Header.Length == 0) {
		return;
	}

	ivrs_hdr = (ACPI_IVRS_HEADER *)(ivrs + 1);
	end = (ACPI_IVRS_HEADER *)((char *)ivrs + ivrs->Header.Length);

	while (ivrs_hdr < end) {
		switch (ivrs_hdr->Type) {
		case ACPI_IVRS_TYPE_HARDWARE:	/* Legacy */
		case 0x11:
		case 0x40: 			/* ACPI HID */
			if (!iter(ivrs_hdr, arg))
				return;
			break;
		
		case ACPI_IVRS_TYPE_MEMORY1:
		case ACPI_IVRS_TYPE_MEMORY2:
		case ACPI_IVRS_TYPE_MEMORY3:
			if (!iter(ivrs_hdr, arg))
				return;

			break;
		
		default:
			printf("AMD-Vi:Not IVHD/IVMD type(%d)", ivrs_hdr->Type);

		}

		ivrs_hdr = (ACPI_IVRS_HEADER *)((uint8_t *)ivrs_hdr +
			ivrs_hdr->Length);
		if (ivrs_hdr->Length < 0) {
			printf("AMD-Vi:IVHD/IVMD is corrupted, length : %d\n", ivrs_hdr->Length);
			break;
		}
	}
}

static  int
ivrs_is_ivhd(UINT8 type)
{

	if ((type == ACPI_IVRS_TYPE_HARDWARE) || (type == 0x11)	|| (type == 0x40))
		return (1);

	return (0);
}

/* Count the number of AMD-Vi devices in the system. */
static int
ivhd_count_iter(ACPI_IVRS_HEADER * ivrs_he, void *arg)
{

	if (ivrs_is_ivhd(ivrs_he->Type))
		ivhd_count++;

	return (1);
}

struct find_ivrs_hdr_args {
	int	i;
	ACPI_IVRS_HEADER *ptr;
};

static int
ivrs_hdr_find_iter(ACPI_IVRS_HEADER * ivrs_hdr, void *args)
{
	struct find_ivrs_hdr_args *fi;

	fi = (struct find_ivrs_hdr_args *)args;
	if (ivrs_is_ivhd(ivrs_hdr->Type)) {
		if (fi->i == 0) {
			fi->ptr = ivrs_hdr;
			return (0);
		}
		fi->i--;
	}

	return (1);
}

static ACPI_IVRS_HARDWARE *
ivhd_find_by_index(int idx)
{
	struct find_ivrs_hdr_args fi;

	fi.i = idx;
	fi.ptr = NULL;

	ivrs_hdr_iterate_tbl(ivrs_hdr_find_iter, &fi);

	return ((ACPI_IVRS_HARDWARE *)fi.ptr);
}

static void
ivhd_dev_add_entry(struct amdvi_softc *softc, uint32_t start_id,
    uint32_t end_id, uint8_t cfg, bool ats)
{
	struct ivhd_dev_cfg *dev_cfg;

	/* If device doesn't have special data, don't add it. */
	if (!cfg)
		return;

	dev_cfg = &softc->dev_cfg[softc->dev_cfg_cnt++];
	dev_cfg->start_id = start_id;
	dev_cfg->end_id = end_id;
	dev_cfg->data = cfg;
	dev_cfg->enable_ats = ats;
}

/*
 * Record device attributes as suggested by BIOS.
 */
static int
ivhd_dev_parse(ACPI_IVRS_HARDWARE * ivhd, struct amdvi_softc *softc)
{
	ACPI_IVRS_DE_HEADER *de, *end;
	int range_start_id = 0, range_end_id = 0;
	uint32_t *extended;
	uint8_t all_data = 0, range_data = 0;
	bool range_enable_ats = false, enable_ats;

	softc->start_dev_rid = ~0;
	softc->end_dev_rid = 0;

	de = (ACPI_IVRS_DE_HEADER *) ((uint8_t *)ivhd +
	    sizeof(ACPI_IVRS_HARDWARE));
	end = (ACPI_IVRS_DE_HEADER *) ((uint8_t *)ivhd +
	    ivhd->Header.Length);

	while (de < (ACPI_IVRS_DE_HEADER *) end) {
		softc->start_dev_rid = MIN(softc->start_dev_rid, de->Id);
		softc->end_dev_rid = MAX(softc->end_dev_rid, de->Id);
		switch (de->Type) {
		case ACPI_IVRS_TYPE_ALL:
			all_data = de->DataSetting;
			break;

		case ACPI_IVRS_TYPE_SELECT:
		case ACPI_IVRS_TYPE_ALIAS_SELECT:
		case ACPI_IVRS_TYPE_EXT_SELECT:
			enable_ats = false;
			if (de->Type == ACPI_IVRS_TYPE_EXT_SELECT) {
				extended = (uint32_t *)(de + 1);
				enable_ats =
				    (*extended & IVHD_DEV_EXT_ATS_DISABLE) ?
					false : true;
			}
			ivhd_dev_add_entry(softc, de->Id, de->Id,
			    de->DataSetting | all_data, enable_ats);
			break;

		case ACPI_IVRS_TYPE_START:
		case ACPI_IVRS_TYPE_ALIAS_START:
		case ACPI_IVRS_TYPE_EXT_START:
			range_start_id = de->Id;
			range_data = de->DataSetting;
			if (de->Type == ACPI_IVRS_TYPE_EXT_START) {
				extended = (uint32_t *)(de + 1);
				range_enable_ats =
				    (*extended & IVHD_DEV_EXT_ATS_DISABLE) ?
					false : true;
			}
			break;

		case ACPI_IVRS_TYPE_END:
			range_end_id = de->Id;
			ivhd_dev_add_entry(softc, range_start_id, range_end_id,
				range_data | all_data, range_enable_ats);
			range_start_id = range_end_id = 0;
			range_data = 0;
			all_data = 0;
			break;

		case ACPI_IVRS_TYPE_PAD4:
			break;

		case ACPI_IVRS_TYPE_SPECIAL:
			/* HPET or IOAPIC */
			break;
		default:
			if ((de->Type < 5) ||
			    (de->Type >= ACPI_IVRS_TYPE_PAD8))
				device_printf(softc->dev,
				    "Unknown dev entry:0x%x\n", de->Type);
		}

		if (softc->dev_cfg_cnt >
			(sizeof(softc->dev_cfg) / sizeof(softc->dev_cfg[0]))) {
			device_printf(softc->dev,
			    "WARN Too many device entries.\n");
			return (EINVAL);
		}
		de++;
	}

	KASSERT((softc->end_dev_rid >= softc->start_dev_rid),
	    ("Device end[0x%x] < start[0x%x.\n",
	    softc->end_dev_rid, softc->start_dev_rid));

	return (0);
}

static void
ivhd_identify(driver_t *driver, device_t parent)
{
	ACPI_TABLE_IVRS *ivrs;
	ACPI_IVRS_HARDWARE *ivhd;
	ACPI_STATUS status;
	uint32_t info;
	int i, count = 0;

	if (acpi_disabled("ivhd"))
		return;

	status = AcpiGetTable(ACPI_SIG_IVRS, 1, (ACPI_TABLE_HEADER **)&ivrs);
	if (ACPI_FAILURE(status))
		return;

	if (ivrs->Header.Length == 0) {
		return;
	}

	info = ivrs->Info;
	printf("AMD-Vi IVRS VAsize = %d PAsize = %d GVAsize = %d flags:%b\n",
		REG_BITS(info, 21, 15), REG_BITS(info, 14, 8), 
		REG_BITS(info, 7, 5), REG_BITS(info, 22, 22),
		"\020\001HtAtsResv");

	ivrs_hdr_iterate_tbl(ivhd_count_iter, NULL);
	if (!ivhd_count)
		return;

	ivhd_devs = malloc(sizeof(device_t) * ivhd_count, M_DEVBUF,
		M_WAITOK | M_ZERO);
	for (i = 0; i < ivhd_count; i++) {
		ivhd = ivhd_find_by_index(i);
		if (ivhd == NULL) {
			printf("Can't find IVHD entry%d\n", i);
			continue;
		}

		ivhd_devs[i] = BUS_ADD_CHILD(parent, 1, "ivhd", i);
		/*
		 * XXX: In case device was not destroyed before, add will fail.
		 * locate the old device instance.
		 */
		if (ivhd_devs[i] == NULL) {
			ivhd_devs[i] = device_find_child(parent, "ivhd", i);
			if (ivhd_devs[i] == NULL) {
				printf("AMD-Vi: cant find AMD-Vi dev%d\n", i);
				break;
			}
		}
		count++;
	}

	/*
	 * Update device count in case failed to attach.
	 */
	ivhd_count = count;
}

static int
ivhd_probe(device_t dev)
{

	if (acpi_get_handle(dev) != NULL)
		return (ENXIO);
	device_set_desc(dev, "AMD-Vi/IOMMU or ivhd");

	return (BUS_PROBE_NOWILDCARD);
}

static int
ivhd_print_cap(struct amdvi_softc *softc, ACPI_IVRS_HARDWARE * ivhd)
{
	device_t dev;
	int max_ptp_level;

	dev = softc->dev;
	device_printf(dev, "Flag:%b\n", softc->ivhd_flag,
	    "\020\001HtTunEn\002PassPW\003ResPassPW\004Isoc\005IotlbSup"
	    "\006Coherent\007PreFSup\008PPRSup");
	/*
	 * If no extended feature[EFR], its rev1 with maximum paging level as 7.
	 */
	max_ptp_level = 7;
	if (softc->ivhd_efr) {
		device_printf(dev, "EFR HATS = %d GATS = %d GLXSup = %d "
		    "MsiNumPr = %d PNBanks= %d PNCounters= %d\n"
		    "max PASID = %d EFR: %b \n",
		    REG_BITS(softc->ivhd_efr, 31, 30),
		    REG_BITS(softc->ivhd_efr, 29, 28),
		    REG_BITS(softc->ivhd_efr, 4, 3),
		    REG_BITS(softc->ivhd_efr, 27, 23),
		    REG_BITS(softc->ivhd_efr, 22, 17),
		    REG_BITS(softc->ivhd_efr, 16, 13),
		    REG_BITS(softc->ivhd_efr, 12, 8),
		    softc->ivhd_efr, "\020\001XTSup\002NXSup\003GTSup\005IASup"
		    "\006GASup\007HESup\008PPRSup");

		max_ptp_level = REG_BITS(softc->ivhd_efr, 31, 30) + 4;
	}

	/* Make sure device support minimum page level as requested by user. */
	if (max_ptp_level < amdvi_ptp_level) {
		device_printf(dev, "Insufficient PTP level:%d\n",
		    max_ptp_level);
		return (EINVAL);
	}

	device_printf(softc->dev, "max supported paging level:%d restricting to: %d\n",
	    max_ptp_level, amdvi_ptp_level);
	device_printf(softc->dev, "device supported range "
	    "[0x%x - 0x%x]\n", softc->start_dev_rid, softc->end_dev_rid);

	return (0);
}

static int
ivhd_attach(device_t dev)
{
	ACPI_IVRS_HARDWARE *ivhd;
	struct amdvi_softc *softc;
	int status, unit;

	unit = device_get_unit(dev);
	/* Make sure its same device for which attach is called. */
	if (ivhd_devs[unit] != dev)
		panic("Not same device old %p new %p", ivhd_devs[unit], dev);

	softc = device_get_softc(dev);
	softc->dev = dev;
	ivhd = ivhd_find_by_index(unit);
	if (ivhd == NULL)
		return (EINVAL);

	softc->pci_seg = ivhd->PciSegmentGroup;
	softc->pci_rid = ivhd->Header.DeviceId;
	softc->ivhd_flag = ivhd->Header.Flags;
	softc->ivhd_efr = ivhd->Reserved;
	/* 
	 * PCI capability has more capabilities that are not part of IVRS.
	 */
	softc->cap_off = ivhd->CapabilityOffset;

#ifdef notyet
	/* IVHD Info bit[4:0] is event MSI/X number. */
	softc->event_msix = ivhd->Info & 0x1F;
#endif
	softc->ctrl = (struct amdvi_ctrl *) PHYS_TO_DMAP(ivhd->BaseAddress);
	status = ivhd_dev_parse(ivhd, softc);
	if (status != 0) {
		device_printf(dev,
		    "endpoint device parsing error=%d\n", status);
	}

	status = ivhd_print_cap(softc, ivhd);
	if (status != 0) {
		return (status);
	}

	status = amdvi_setup_hw(softc);
	if (status != 0) {
		device_printf(dev, "couldn't be initialised, error=%d\n", 
		    status);
		return (status);
	}

	return (0);
}

static int
ivhd_detach(device_t dev)
{
	struct amdvi_softc *softc;

	softc = device_get_softc(dev);

	amdvi_teardown_hw(softc);

	/*
	 * XXX: delete the device.
	 * don't allow detach, return EBUSY.
	 */
	return (0);
}

static int
ivhd_suspend(device_t dev)
{

	return (0);
}

static int
ivhd_resume(device_t dev)
{

	return (0);
}

static device_method_t ivhd_methods[] = {
	DEVMETHOD(device_identify, ivhd_identify),
	DEVMETHOD(device_probe, ivhd_probe),
	DEVMETHOD(device_attach, ivhd_attach),
	DEVMETHOD(device_detach, ivhd_detach),
	DEVMETHOD(device_suspend, ivhd_suspend),
	DEVMETHOD(device_resume, ivhd_resume),
	DEVMETHOD_END
};

static driver_t ivhd_driver = {
	"ivhd",
	ivhd_methods,
	sizeof(struct amdvi_softc),
};

static devclass_t ivhd_devclass;

/*
 * Load this module at the end after PCI re-probing to configure interrupt.
 */
DRIVER_MODULE_ORDERED(ivhd, acpi, ivhd_driver, ivhd_devclass, 0, 0,
		      SI_ORDER_ANY);
MODULE_DEPEND(ivhd, acpi, 1, 1, 1);
MODULE_DEPEND(ivhd, pci, 1, 1, 1);
