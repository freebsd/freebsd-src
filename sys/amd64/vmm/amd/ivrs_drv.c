/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2016, Anish Gupta (anish@freebsd.org)
 * Copyright (c) 2021 The FreeBSD Foundation
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
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include "io/iommu.h"
#include "amdvi_priv.h"

device_t *ivhd_devs;			/* IVHD or AMD-Vi device list. */
int	ivhd_count;			/* Number of IVHD header. */
/* 
 * Cached IVHD header list.
 * Single entry for each IVHD, filtered the legacy one.
 */
ACPI_IVRS_HARDWARE1 **ivhd_hdrs;

extern int amdvi_ptp_level;		/* Page table levels. */

typedef int (*ivhd_iter_t)(ACPI_IVRS_HEADER *ptr, void *arg);
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
		if ((uint8_t *)ivrs_hdr + ivrs_hdr->Length > (uint8_t *)end) {
			printf("AMD-Vi:IVHD/IVMD is corrupted, length : %d\n",
			    ivrs_hdr->Length);
			break;
		}

		switch (ivrs_hdr->Type) {
		case IVRS_TYPE_HARDWARE_LEGACY:	/* Legacy */
		case IVRS_TYPE_HARDWARE_EFR:
		case IVRS_TYPE_HARDWARE_MIXED:
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
	}
}

static bool
ivrs_is_ivhd(UINT8 type)
{

	switch(type) {
	case IVRS_TYPE_HARDWARE_LEGACY:
	case IVRS_TYPE_HARDWARE_EFR:
	case IVRS_TYPE_HARDWARE_MIXED:
		return (true);

	default:
		return (false);
	}
}

/* Count the number of AMD-Vi devices in the system. */
static int
ivhd_count_iter(ACPI_IVRS_HEADER * ivrs_he, void *arg)
{
	int *count;

	count = (int *)arg;
	if (ivrs_is_ivhd(ivrs_he->Type))
		(*count)++;

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

static ACPI_IVRS_HARDWARE1 *
ivhd_find_by_index(int idx)
{
	struct find_ivrs_hdr_args fi;

	fi.i = idx;
	fi.ptr = NULL;

	ivrs_hdr_iterate_tbl(ivrs_hdr_find_iter, &fi);

	return ((ACPI_IVRS_HARDWARE1 *)fi.ptr);
}

static void
ivhd_dev_add_entry(struct amdvi_softc *softc, uint32_t start_id,
    uint32_t end_id, uint8_t cfg, bool ats)
{
	struct ivhd_dev_cfg *dev_cfg;

	KASSERT(softc->dev_cfg_cap >= softc->dev_cfg_cnt,
	    ("Impossible case: number of dev_cfg exceeding capacity"));
	if (softc->dev_cfg_cap == softc->dev_cfg_cnt) {
		if (softc->dev_cfg_cap == 0)
			softc->dev_cfg_cap = 1;
		else
			softc->dev_cfg_cap <<= 2;
		softc->dev_cfg = realloc(softc->dev_cfg,
		    sizeof(*softc->dev_cfg) * softc->dev_cfg_cap, M_DEVBUF,
		    M_WAITOK);
	}

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
ivhd_dev_parse(ACPI_IVRS_HARDWARE1 *ivhd, struct amdvi_softc *softc)
{
	ACPI_IVRS_DE_HEADER *de;
	uint8_t *p, *end;
	int range_start_id = -1, range_end_id = -1, i;
	uint32_t *extended;
	uint8_t all_data = 0, range_data = 0;
	bool range_enable_ats = false, enable_ats;

	switch (ivhd->Header.Type) {
		case IVRS_TYPE_HARDWARE_LEGACY:
			p = (uint8_t *)ivhd + sizeof(ACPI_IVRS_HARDWARE1);
			break;

		case IVRS_TYPE_HARDWARE_EFR:
		case IVRS_TYPE_HARDWARE_MIXED:
			p = (uint8_t *)ivhd + sizeof(ACPI_IVRS_HARDWARE2);
			break;

		default:
			device_printf(softc->dev, 
				"unknown type: 0x%x\n", ivhd->Header.Type);
			return (-1);
	}

	end = (uint8_t *)ivhd + ivhd->Header.Length;

	while (p < end) {
		de = (ACPI_IVRS_DE_HEADER *)p;
		switch (de->Type) {
		case ACPI_IVRS_TYPE_ALL:
			all_data = de->DataSetting;
			for (i = 0; i < softc->dev_cfg_cnt; i++)
				softc->dev_cfg[i].data |= all_data;
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
			if (range_start_id != -1) {
				device_printf(softc->dev,
				    "Unexpected start-of-range device entry\n");
				return (EINVAL);
			}
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
			if (range_start_id == -1) {
				device_printf(softc->dev,
				    "Unexpected end-of-range device entry\n");
				return (EINVAL);
			}
			range_end_id = de->Id;
			if (range_end_id < range_start_id) {
				device_printf(softc->dev,
				    "Device entry range going backward\n");
				return (EINVAL);
			}
			ivhd_dev_add_entry(softc, range_start_id, range_end_id,
			    range_data | all_data, range_enable_ats);
			range_start_id = range_end_id = -1;
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

		if (de->Type < 0x40)
			p += sizeof(ACPI_IVRS_DEVICE4);
		else if (de->Type < 0x80)
			p += sizeof(ACPI_IVRS_DEVICE8A);
		else {
			printf("Variable size IVHD type 0x%x not supported\n",
			    de->Type);
			break;
		}
	}

	return (0);
}

static bool
ivhd_is_newer(ACPI_IVRS_HEADER *old, ACPI_IVRS_HEADER  *new)
{
	if (old->DeviceId == new->DeviceId) {
		/*
		 * Newer IVRS header type take precedence.
		 */
		if (old->Type == IVRS_TYPE_HARDWARE_LEGACY &&
		    ((new->Type == IVRS_TYPE_HARDWARE_EFR) ||
		    (new->Type == IVRS_TYPE_HARDWARE_MIXED)))
			return (true);

		/*
		 * Mixed format IVHD header type take precedence
		 * over fixed format IVHD header types.
		 */
		if (old->Type == IVRS_TYPE_HARDWARE_EFR &&
		    new->Type == IVRS_TYPE_HARDWARE_MIXED)
			return (true);
	}

	return (false);
}

static void
ivhd_identify(driver_t *driver, device_t parent)
{
	ACPI_TABLE_IVRS *ivrs;
	ACPI_IVRS_HARDWARE1 *ivhd;
	ACPI_STATUS status;
	int i, j, count = 0;
	uint32_t ivrs_ivinfo;

	if (acpi_disabled("ivhd"))
		return;

	status = AcpiGetTable(ACPI_SIG_IVRS, 1, (ACPI_TABLE_HEADER **)&ivrs);
	if (ACPI_FAILURE(status))
		return;

	if (ivrs->Header.Length == 0) {
		return;
	}

	ivrs_ivinfo = ivrs->Info;
	printf("AMD-Vi: IVRS Info VAsize = %d PAsize = %d GVAsize = %d"
	       " flags:%b\n",
		REG_BITS(ivrs_ivinfo, 21, 15), REG_BITS(ivrs_ivinfo, 14, 8), 
		REG_BITS(ivrs_ivinfo, 7, 5), REG_BITS(ivrs_ivinfo, 22, 22),
		"\020\001EFRSup");

	ivrs_hdr_iterate_tbl(ivhd_count_iter, &count);
	if (!count)
		return;

	ivhd_hdrs = malloc(sizeof(void *) * count, M_DEVBUF,
		M_WAITOK | M_ZERO);
	for (i = 0; i < count; i++) {
		ivhd = ivhd_find_by_index(i);
		KASSERT(ivhd, ("ivhd%d is NULL\n", i));

		/*
		 * Scan for presence of legacy and non-legacy device type
		 * for same IOMMU device and override the old one.
		 *
		 * If there is no existing IVHD to the same IOMMU device,
		 * the IVHD header pointer is appended.
		 */
		for (j = 0; j < ivhd_count; j++) {
			if (ivhd_is_newer(&ivhd_hdrs[j]->Header, &ivhd->Header))
				break;
		}
		ivhd_hdrs[j] = ivhd;
		if (j == ivhd_count)
			ivhd_count++;
	}

	ivhd_devs = malloc(sizeof(device_t) * ivhd_count, M_DEVBUF,
		M_WAITOK | M_ZERO);
	for (i = 0, j = 0; i < ivhd_count; i++) {
		ivhd = ivhd_hdrs[i];
		KASSERT(ivhd, ("ivhd%d is NULL\n", i));

		/*
		 * Use a high order to ensure that this driver is probed after
		 * the Host-PCI bridge and the root PCI bus.
		 */
		ivhd_devs[i] = BUS_ADD_CHILD(parent,
		    ACPI_DEV_BASE_ORDER + 10 * 10, "ivhd", i);

		/*
		 * XXX: In case device was not destroyed before, add will fail.
		 * locate the old device instance.
		 */
		if (ivhd_devs[i] == NULL) {
			ivhd_devs[i] = device_find_child(parent, "ivhd", i);
			if (ivhd_devs[i] == NULL) {
				printf("AMD-Vi: can't find ivhd%d\n", i);
				break;
			}
		}
		j++;
	}

	/*
	 * Update device count in case failed to attach.
	 */
	ivhd_count = j;
}

static int
ivhd_probe(device_t dev)
{
	ACPI_IVRS_HARDWARE1 *ivhd;
	int unit;

	if (acpi_get_handle(dev) != NULL)
		return (ENXIO);

	unit = device_get_unit(dev);
	KASSERT((unit < ivhd_count), 
		("ivhd unit %d > count %d", unit, ivhd_count));
	ivhd = ivhd_hdrs[unit];
	KASSERT(ivhd, ("ivhd is NULL"));

	switch (ivhd->Header.Type) {
	case IVRS_TYPE_HARDWARE_EFR:
		device_set_desc(dev, "AMD-Vi/IOMMU ivhd with EFR");
		break;

	case IVRS_TYPE_HARDWARE_MIXED:
		device_set_desc(dev, "AMD-Vi/IOMMU ivhd in mixed format");
		break;

	case IVRS_TYPE_HARDWARE_LEGACY:
        default:
		device_set_desc(dev, "AMD-Vi/IOMMU ivhd");
		break;
	}

	return (BUS_PROBE_NOWILDCARD);
}

static void
ivhd_print_flag(device_t dev, enum IvrsType ivhd_type, uint8_t flag)
{
	/*
	 * IVHD lgeacy type has two extra high bits in flag which has
	 * been moved to EFR for non-legacy device.
	 */
	switch (ivhd_type) {
	case IVRS_TYPE_HARDWARE_LEGACY:
		device_printf(dev, "Flag:%b\n", flag,
			"\020"
			"\001HtTunEn"
			"\002PassPW"
			"\003ResPassPW"
			"\004Isoc"
			"\005IotlbSup"
			"\006Coherent"
			"\007PreFSup"
			"\010PPRSup");
		break;

	case IVRS_TYPE_HARDWARE_EFR:
	case IVRS_TYPE_HARDWARE_MIXED:
		device_printf(dev, "Flag:%b\n", flag,
			"\020"
			"\001HtTunEn"
			"\002PassPW"
			"\003ResPassPW"
			"\004Isoc"
			"\005IotlbSup"
			"\006Coherent");
		break;

	default:
		device_printf(dev, "Can't decode flag of ivhd type :0x%x\n",
			ivhd_type);
		break;
	}
}

/*
 * Feature in legacy IVHD type(0x10) and attribute in newer type(0x11 and 0x40).
 */
static void
ivhd_print_feature(device_t dev, enum IvrsType ivhd_type, uint32_t feature) 
{
	switch (ivhd_type) {
	case IVRS_TYPE_HARDWARE_LEGACY:
		device_printf(dev, "Features(type:0x%x) HATS = %d GATS = %d"
			" MsiNumPPR = %d PNBanks= %d PNCounters= %d\n",
			ivhd_type,
			REG_BITS(feature, 31, 30),
			REG_BITS(feature, 29, 28),
			REG_BITS(feature, 27, 23),
			REG_BITS(feature, 22, 17),
			REG_BITS(feature, 16, 13));
		device_printf(dev, "max PASID = %d GLXSup = %d Feature:%b\n",
			REG_BITS(feature, 12, 8),
			REG_BITS(feature, 4, 3),
			feature,
			"\020"
			"\002NXSup"
			"\003GTSup"
			"\004<b4>"
			"\005IASup"
			"\006GASup"
			"\007HESup");
		break;

	/* Fewer features or attributes are reported in non-legacy type. */
	case IVRS_TYPE_HARDWARE_EFR:
	case IVRS_TYPE_HARDWARE_MIXED:
		device_printf(dev, "Features(type:0x%x) MsiNumPPR = %d"
			" PNBanks= %d PNCounters= %d\n",
			ivhd_type,
			REG_BITS(feature, 27, 23),
			REG_BITS(feature, 22, 17),
			REG_BITS(feature, 16, 13));
		break;

	default: /* Other ivhd type features are not decoded. */
		device_printf(dev, "Can't decode ivhd type :0x%x\n", ivhd_type);
	}
}

/* Print extended features of IOMMU. */
static void
ivhd_print_ext_feature(device_t dev, uint64_t ext_feature)
{
	uint32_t ext_low, ext_high;

	if (!ext_feature)
		return;

	ext_low = ext_feature;
	device_printf(dev, "Extended features[31:0]:%b "
		"HATS = 0x%x GATS = 0x%x "
		"GLXSup = 0x%x SmiFSup = 0x%x SmiFRC = 0x%x "
		"GAMSup = 0x%x DualPortLogSup = 0x%x DualEventLogSup = 0x%x\n",
		(int)ext_low,
		"\020"
		"\001PreFSup"
		"\002PPRSup"
		"\003<b2>"
		"\004NXSup"
		"\005GTSup"
		"\006<b5>"
		"\007IASup"
		"\010GASup"
		"\011HESup"
		"\012PCSup",
		REG_BITS(ext_low, 11, 10),
		REG_BITS(ext_low, 13, 12),
		REG_BITS(ext_low, 15, 14),
		REG_BITS(ext_low, 17, 16),
		REG_BITS(ext_low, 20, 18),
		REG_BITS(ext_low, 23, 21),
		REG_BITS(ext_low, 25, 24),
		REG_BITS(ext_low, 29, 28));

	ext_high = ext_feature >> 32;
	device_printf(dev, "Extended features[62:32]:%b "
		"Max PASID: 0x%x DevTblSegSup = 0x%x "
		"MarcSup = 0x%x\n",
		(int)(ext_high),
		"\020"
		"\006USSup"
		"\011PprOvrflwEarlySup"
		"\012PPRAutoRspSup"
		"\015BlKStopMrkSup"
		"\016PerfOptSup"
		"\017MsiCapMmioSup"
		"\021GIOSup"
		"\022HASup"
		"\023EPHSup"
		"\024AttrFWSup"
		"\025HDSup"
		"\027InvIotlbSup",
	    	REG_BITS(ext_high, 5, 0),
	    	REG_BITS(ext_high, 8, 7),
	    	REG_BITS(ext_high, 11, 10));
}

static int
ivhd_print_cap(struct amdvi_softc *softc, ACPI_IVRS_HARDWARE1 * ivhd)
{
	device_t dev;
	int max_ptp_level;

	dev = softc->dev;

	ivhd_print_flag(dev, softc->ivhd_type, softc->ivhd_flag);
	ivhd_print_feature(dev, softc->ivhd_type, softc->ivhd_feature);
	ivhd_print_ext_feature(dev, softc->ext_feature);
	max_ptp_level = 7;
	/* Make sure device support minimum page level as requested by user. */
	if (max_ptp_level < amdvi_ptp_level) {
		device_printf(dev, "insufficient PTP level:%d\n",
			max_ptp_level);
		return (EINVAL);
	} else {
		device_printf(softc->dev, "supported paging level:%d, will use only: %d\n",
	    		max_ptp_level, amdvi_ptp_level);
	}

	return (0);
}

static int
ivhd_attach(device_t dev)
{
	ACPI_IVRS_HARDWARE1 *ivhd;
	ACPI_IVRS_HARDWARE2 *ivhd_efr;
	struct amdvi_softc *softc;
	int status, unit;

	unit = device_get_unit(dev);
	KASSERT((unit < ivhd_count), 
		("ivhd unit %d > count %d", unit, ivhd_count));
	/* Make sure its same device for which attach is called. */
	KASSERT((ivhd_devs[unit] == dev),
		("Not same device old %p new %p", ivhd_devs[unit], dev));

	softc = device_get_softc(dev);
	softc->dev = dev;
	ivhd = ivhd_hdrs[unit];
	KASSERT(ivhd, ("ivhd is NULL"));
	softc->pci_dev = pci_find_dbsf(ivhd->PciSegmentGroup,
	    PCI_RID2BUS(ivhd->Header.DeviceId),
	    PCI_RID2SLOT(ivhd->Header.DeviceId),
	    PCI_RID2FUNC(ivhd->Header.DeviceId));

	softc->ivhd_type = ivhd->Header.Type;
	softc->pci_seg = ivhd->PciSegmentGroup;
	softc->pci_rid = ivhd->Header.DeviceId;
	softc->ivhd_flag = ivhd->Header.Flags;
	/* 
	 * On lgeacy IVHD type(0x10), it is documented as feature
	 * but in newer type it is attribute.
	 */
	softc->ivhd_feature = ivhd->FeatureReporting;
	/* 
	 * PCI capability has more capabilities that are not part of IVRS.
	 */
	softc->cap_off = ivhd->CapabilityOffset;

#ifdef notyet
	/* IVHD Info bit[4:0] is event MSI/X number. */
	softc->event_msix = ivhd->Info & 0x1F;
#endif
	switch (ivhd->Header.Type) {
	case IVRS_TYPE_HARDWARE_EFR:
	case IVRS_TYPE_HARDWARE_MIXED:
		ivhd_efr = (ACPI_IVRS_HARDWARE2 *)ivhd;
		softc->ext_feature = ivhd_efr->EfrRegisterImage;
		break;
	}

	softc->ctrl = (struct amdvi_ctrl *) PHYS_TO_DMAP(ivhd->BaseAddress);
	status = ivhd_dev_parse(ivhd, softc);
	if (status != 0) {
		device_printf(dev,
		    "endpoint device parsing error=%d\n", status);
		goto fail;
	}

	status = ivhd_print_cap(softc, ivhd);
	if (status != 0)
		goto fail;

	status = amdvi_setup_hw(softc);
	if (status != 0) {
		device_printf(dev, "couldn't be initialised, error=%d\n", 
		    status);
		goto fail;
	}

	return (0);

fail:
	free(softc->dev_cfg, M_DEVBUF);
	return (status);
}

static int
ivhd_detach(device_t dev)
{
	struct amdvi_softc *softc;

	softc = device_get_softc(dev);

	amdvi_teardown_hw(softc);
	free(softc->dev_cfg, M_DEVBUF);

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

/*
 * Load this module at the end after PCI re-probing to configure interrupt.
 */
DRIVER_MODULE_ORDERED(ivhd, acpi, ivhd_driver, 0, 0, SI_ORDER_ANY);
MODULE_DEPEND(ivhd, acpi, 1, 1, 1);
MODULE_DEPEND(ivhd, pci, 1, 1, 1);
