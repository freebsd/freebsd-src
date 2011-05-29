/*-
 * Copyright (c) 2007 Yahoo!, Inc.
 * All rights reserved.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/types.h>

#include <err.h>
#include <stdio.h>
#include <sys/agpio.h>
#include <sys/pciio.h>

#include <dev/agp/agpreg.h>
#include <dev/pci/pcireg.h>

#include "pciconf.h"

static void	list_ecaps(int fd, struct pci_conf *p);

static void
cap_power(int fd, struct pci_conf *p, uint8_t ptr)
{
	uint16_t cap, status;

	cap = read_config(fd, &p->pc_sel, ptr + PCIR_POWER_CAP, 2);
	status = read_config(fd, &p->pc_sel, ptr + PCIR_POWER_STATUS, 2);
	printf("powerspec %d  supports D0%s%s D3  current D%d",
	    cap & PCIM_PCAP_SPEC,
	    cap & PCIM_PCAP_D1SUPP ? " D1" : "",
	    cap & PCIM_PCAP_D2SUPP ? " D2" : "",
	    status & PCIM_PSTAT_DMASK);
}

static void
cap_agp(int fd, struct pci_conf *p, uint8_t ptr)
{
	uint32_t status, command;

	status = read_config(fd, &p->pc_sel, ptr + AGP_STATUS, 4);
	command = read_config(fd, &p->pc_sel, ptr + AGP_CAPID, 4);
	printf("AGP ");
	if (AGP_MODE_GET_MODE_3(status)) {
		printf("v3 ");
		if (AGP_MODE_GET_RATE(status) & AGP_MODE_V3_RATE_8x)
			printf("8x ");
		if (AGP_MODE_GET_RATE(status) & AGP_MODE_V3_RATE_4x)
			printf("4x ");
	} else {
		if (AGP_MODE_GET_RATE(status) & AGP_MODE_V2_RATE_4x)
			printf("4x ");
		if (AGP_MODE_GET_RATE(status) & AGP_MODE_V2_RATE_2x)
			printf("2x ");
		if (AGP_MODE_GET_RATE(status) & AGP_MODE_V2_RATE_1x)
			printf("1x ");
	}
	if (AGP_MODE_GET_SBA(status))
		printf("SBA ");
	if (AGP_MODE_GET_AGP(command)) {
		printf("enabled at ");
		if (AGP_MODE_GET_MODE_3(command)) {
			printf("v3 ");
			switch (AGP_MODE_GET_RATE(command)) {
			case AGP_MODE_V3_RATE_8x:
				printf("8x ");
				break;
			case AGP_MODE_V3_RATE_4x:
				printf("4x ");
				break;
			}
		} else
			switch (AGP_MODE_GET_RATE(command)) {
			case AGP_MODE_V2_RATE_4x:
				printf("4x ");
				break;
			case AGP_MODE_V2_RATE_2x:
				printf("2x ");
				break;
			case AGP_MODE_V2_RATE_1x:
				printf("1x ");
				break;
			}
		if (AGP_MODE_GET_SBA(command))
			printf("SBA ");
	} else
		printf("disabled");
}

static void
cap_vpd(int fd, struct pci_conf *p, uint8_t ptr)
{

	printf("VPD");
}

static void
cap_msi(int fd, struct pci_conf *p, uint8_t ptr)
{
	uint16_t ctrl;
	int msgnum;

	ctrl = read_config(fd, &p->pc_sel, ptr + PCIR_MSI_CTRL, 2);
	msgnum = 1 << ((ctrl & PCIM_MSICTRL_MMC_MASK) >> 1);
	printf("MSI supports %d message%s%s%s ", msgnum,
	    (msgnum == 1) ? "" : "s",
	    (ctrl & PCIM_MSICTRL_64BIT) ? ", 64 bit" : "",
	    (ctrl & PCIM_MSICTRL_VECTOR) ? ", vector masks" : "");
	if (ctrl & PCIM_MSICTRL_MSI_ENABLE) {
		msgnum = 1 << ((ctrl & PCIM_MSICTRL_MME_MASK) >> 4);
		printf("enabled with %d message%s", msgnum,
		    (msgnum == 1) ? "" : "s");
	}
}

static void
cap_pcix(int fd, struct pci_conf *p, uint8_t ptr)
{
	uint32_t status;
	int comma, max_splits, max_burst_read;

	status = read_config(fd, &p->pc_sel, ptr + PCIXR_STATUS, 4);
	printf("PCI-X ");
	if (status & PCIXM_STATUS_64BIT)
		printf("64-bit ");
	if ((p->pc_hdr & PCIM_HDRTYPE) == 1)
		printf("bridge ");
	if ((p->pc_hdr & PCIM_HDRTYPE) != 1 || (status & (PCIXM_STATUS_133CAP |
	    PCIXM_STATUS_266CAP | PCIXM_STATUS_533CAP)) != 0)
		printf("supports");
	comma = 0;
	if (status & PCIXM_STATUS_133CAP) {
		printf("%s 133MHz", comma ? "," : "");
		comma = 1;
	}
	if (status & PCIXM_STATUS_266CAP) {
		printf("%s 266MHz", comma ? "," : "");
		comma = 1;
	}
	if (status & PCIXM_STATUS_533CAP) {
		printf("%s 533MHz", comma ? "," : "");
		comma = 1;
	}
	if ((p->pc_hdr & PCIM_HDRTYPE) == 1)
		return;
	switch (status & PCIXM_STATUS_MAX_READ) {
	case PCIXM_STATUS_MAX_READ_512:
		max_burst_read = 512;
		break;
	case PCIXM_STATUS_MAX_READ_1024:
		max_burst_read = 1024;
		break;
	case PCIXM_STATUS_MAX_READ_2048:
		max_burst_read = 2048;
		break;
	case PCIXM_STATUS_MAX_READ_4096:
		max_burst_read = 4096;
		break;
	}
	switch (status & PCIXM_STATUS_MAX_SPLITS) {
	case PCIXM_STATUS_MAX_SPLITS_1:
		max_splits = 1;
		break;
	case PCIXM_STATUS_MAX_SPLITS_2:
		max_splits = 2;
		break;
	case PCIXM_STATUS_MAX_SPLITS_3:
		max_splits = 3;
		break;
	case PCIXM_STATUS_MAX_SPLITS_4:
		max_splits = 4;
		break;
	case PCIXM_STATUS_MAX_SPLITS_8:
		max_splits = 8;
		break;
	case PCIXM_STATUS_MAX_SPLITS_12:
		max_splits = 12;
		break;
	case PCIXM_STATUS_MAX_SPLITS_16:
		max_splits = 16;
		break;
	case PCIXM_STATUS_MAX_SPLITS_32:
		max_splits = 32;
		break;
	}
	printf("%s %d burst read, %d split transaction%s", comma ? "," : "",
	    max_burst_read, max_splits, max_splits == 1 ? "" : "s");
}

static void
cap_ht(int fd, struct pci_conf *p, uint8_t ptr)
{
	uint32_t reg;
	uint16_t command;

	command = read_config(fd, &p->pc_sel, ptr + PCIR_HT_COMMAND, 2);
	printf("HT ");
	if ((command & 0xe000) == PCIM_HTCAP_SLAVE)
		printf("slave");
	else if ((command & 0xe000) == PCIM_HTCAP_HOST)
		printf("host");
	else
		switch (command & PCIM_HTCMD_CAP_MASK) {
		case PCIM_HTCAP_SWITCH:
			printf("switch");
			break;
		case PCIM_HTCAP_INTERRUPT:
			printf("interrupt");
			break;
		case PCIM_HTCAP_REVISION_ID:
			printf("revision ID");
			break;
		case PCIM_HTCAP_UNITID_CLUMPING:
			printf("unit ID clumping");
			break;
		case PCIM_HTCAP_EXT_CONFIG_SPACE:
			printf("extended config space");
			break;
		case PCIM_HTCAP_ADDRESS_MAPPING:
			printf("address mapping");
			break;
		case PCIM_HTCAP_MSI_MAPPING:
			printf("MSI %saddress window %s at 0x",
			    command & PCIM_HTCMD_MSI_FIXED ? "fixed " : "",
			    command & PCIM_HTCMD_MSI_ENABLE ? "enabled" :
			    "disabled");
			if (command & PCIM_HTCMD_MSI_FIXED)
				printf("fee00000");
			else {
				reg = read_config(fd, &p->pc_sel,
				    ptr + PCIR_HTMSI_ADDRESS_HI, 4);
				if (reg != 0)
					printf("%08x", reg);
				reg = read_config(fd, &p->pc_sel,
				    ptr + PCIR_HTMSI_ADDRESS_LO, 4);
				printf("%08x", reg);
			}
			break;
		case PCIM_HTCAP_DIRECT_ROUTE:
			printf("direct route");
			break;
		case PCIM_HTCAP_VCSET:
			printf("VC set");
			break;
		case PCIM_HTCAP_RETRY_MODE:
			printf("retry mode");
			break;
		case PCIM_HTCAP_X86_ENCODING:
			printf("X86 encoding");
			break;
		default:
			printf("unknown %02x", command);
			break;
		}
}

static void
cap_vendor(int fd, struct pci_conf *p, uint8_t ptr)
{
	uint8_t length;

	length = read_config(fd, &p->pc_sel, ptr + PCIR_VENDOR_LENGTH, 1);
	printf("vendor (length %d)", length);
	if (p->pc_vendor == 0x8086) {
		/* Intel */
		uint8_t version;

		version = read_config(fd, &p->pc_sel, ptr + PCIR_VENDOR_DATA,
		    1);
		printf(" Intel cap %d version %d", version >> 4, version & 0xf);
		if (version >> 4 == 1 && length == 12) {
			/* Feature Detection */
			uint32_t fvec;
			int comma;

			comma = 0;
			fvec = read_config(fd, &p->pc_sel, ptr +
			    PCIR_VENDOR_DATA + 5, 4);
			printf("\n\t\t features:");
			if (fvec & (1 << 0)) {
				printf(" AMT");
				comma = 1;
			}
			fvec = read_config(fd, &p->pc_sel, ptr +
			    PCIR_VENDOR_DATA + 1, 4);
			if (fvec & (1 << 21)) {
				printf("%s Quick Resume", comma ? "," : "");
				comma = 1;
			}
			if (fvec & (1 << 18)) {
				printf("%s SATA RAID-5", comma ? "," : "");
				comma = 1;
			}
			if (fvec & (1 << 9)) {
				printf("%s Mobile", comma ? "," : "");
				comma = 1;
			}
			if (fvec & (1 << 7)) {
				printf("%s 6 PCI-e x1 slots", comma ? "," : "");
				comma = 1;
			} else {
				printf("%s 4 PCI-e x1 slots", comma ? "," : "");
				comma = 1;
			}
			if (fvec & (1 << 5)) {
				printf("%s SATA RAID-0/1/10", comma ? "," : "");
				comma = 1;
			}
			if (fvec & (1 << 3)) {
				printf("%s SATA AHCI", comma ? "," : "");
				comma = 1;
			}
		}
	}
}

static void
cap_debug(int fd, struct pci_conf *p, uint8_t ptr)
{
	uint16_t debug_port;

	debug_port = read_config(fd, &p->pc_sel, ptr + PCIR_DEBUG_PORT, 2);
	printf("EHCI Debug Port at offset 0x%x in map 0x%x", debug_port &
	    PCIM_DEBUG_PORT_OFFSET, PCIR_BAR(debug_port >> 13));
}

static void
cap_subvendor(int fd, struct pci_conf *p, uint8_t ptr)
{
	uint32_t id;

	id = read_config(fd, &p->pc_sel, ptr + PCIR_SUBVENDCAP_ID, 4);
	printf("PCI Bridge card=0x%08x", id);
}

#define	MAX_PAYLOAD(field)		(128 << (field))

static void
cap_express(int fd, struct pci_conf *p, uint8_t ptr)
{
	uint32_t val;
	uint16_t flags;

	flags = read_config(fd, &p->pc_sel, ptr + PCIR_EXPRESS_FLAGS, 2);
	printf("PCI-Express %d ", flags & PCIM_EXP_FLAGS_VERSION);
	switch (flags & PCIM_EXP_FLAGS_TYPE) {
	case PCIM_EXP_TYPE_ENDPOINT:
		printf("endpoint");
		break;
	case PCIM_EXP_TYPE_LEGACY_ENDPOINT:
		printf("legacy endpoint");
		break;
	case PCIM_EXP_TYPE_ROOT_PORT:
		printf("root port");
		break;
	case PCIM_EXP_TYPE_UPSTREAM_PORT:
		printf("upstream port");
		break;
	case PCIM_EXP_TYPE_DOWNSTREAM_PORT:
		printf("downstream port");
		break;
	case PCIM_EXP_TYPE_PCI_BRIDGE:
		printf("PCI bridge");
		break;
	case PCIM_EXP_TYPE_PCIE_BRIDGE:
		printf("PCI to PCIe bridge");
		break;
	case PCIM_EXP_TYPE_ROOT_INT_EP:
		printf("root endpoint");
		break;
	case PCIM_EXP_TYPE_ROOT_EC:
		printf("event collector");
		break;
	default:
		printf("type %d", (flags & PCIM_EXP_FLAGS_TYPE) >> 4);
		break;
	}
	if (flags & PCIM_EXP_FLAGS_IRQ)
		printf(" IRQ %d", (flags & PCIM_EXP_FLAGS_IRQ) >> 8);
	val = read_config(fd, &p->pc_sel, ptr + PCIR_EXPRESS_DEVICE_CAP, 4);
	flags = read_config(fd, &p->pc_sel, ptr + PCIR_EXPRESS_DEVICE_CTL, 2);
	printf(" max data %d(%d)",
	    MAX_PAYLOAD((flags & PCIM_EXP_CTL_MAX_PAYLOAD) >> 5),
	    MAX_PAYLOAD(val & PCIM_EXP_CAP_MAX_PAYLOAD));
	val = read_config(fd, &p->pc_sel, ptr + PCIR_EXPRESS_LINK_CAP, 4);
	flags = read_config(fd, &p->pc_sel, ptr+ PCIR_EXPRESS_LINK_STA, 2);
	printf(" link x%d(x%d)", (flags & PCIM_LINK_STA_WIDTH) >> 4,
	    (val & PCIM_LINK_CAP_MAX_WIDTH) >> 4);
}

static void
cap_msix(int fd, struct pci_conf *p, uint8_t ptr)
{
	uint32_t val;
	uint16_t ctrl;
	int msgnum, table_bar, pba_bar;

	ctrl = read_config(fd, &p->pc_sel, ptr + PCIR_MSIX_CTRL, 2);
	msgnum = (ctrl & PCIM_MSIXCTRL_TABLE_SIZE) + 1;
	val = read_config(fd, &p->pc_sel, ptr + PCIR_MSIX_TABLE, 4);
	table_bar = PCIR_BAR(val & PCIM_MSIX_BIR_MASK);
	val = read_config(fd, &p->pc_sel, ptr + PCIR_MSIX_PBA, 4);
	pba_bar = PCIR_BAR(val & PCIM_MSIX_BIR_MASK);	
	printf("MSI-X supports %d message%s ", msgnum,
	    (msgnum == 1) ? "" : "s");
	if (table_bar == pba_bar)
		printf("in map 0x%x", table_bar);
	else
		printf("in maps 0x%x and 0x%x", table_bar, pba_bar);
	if (ctrl & PCIM_MSIXCTRL_MSIX_ENABLE)
		printf(" enabled");
}

static void
cap_sata(int fd, struct pci_conf *p, uint8_t ptr)
{

	printf("SATA Index-Data Pair");
}

static void
cap_pciaf(int fd, struct pci_conf *p, uint8_t ptr)
{
	uint8_t cap;

	cap = read_config(fd, &p->pc_sel, ptr + PCIR_PCIAF_CAP, 1);
	printf("PCI Advanced Features:%s%s",
	    cap & PCIM_PCIAFCAP_FLR ? " FLR" : "",
	    cap & PCIM_PCIAFCAP_TP  ? " TP"  : "");
}

void
list_caps(int fd, struct pci_conf *p)
{
	int express;
	uint16_t sta;
	uint8_t ptr, cap;

	/* Are capabilities present for this device? */
	sta = read_config(fd, &p->pc_sel, PCIR_STATUS, 2);
	if (!(sta & PCIM_STATUS_CAPPRESENT))
		return;

	switch (p->pc_hdr & PCIM_HDRTYPE) {
	case PCIM_HDRTYPE_NORMAL:
	case PCIM_HDRTYPE_BRIDGE:
		ptr = PCIR_CAP_PTR;
		break;
	case PCIM_HDRTYPE_CARDBUS:
		ptr = PCIR_CAP_PTR_2;
		break;
	default:
		errx(1, "list_caps: bad header type");
	}

	/* Walk the capability list. */
	express = 0;
	ptr = read_config(fd, &p->pc_sel, ptr, 1);
	while (ptr != 0 && ptr != 0xff) {
		cap = read_config(fd, &p->pc_sel, ptr + PCICAP_ID, 1);
		printf("    cap %02x[%02x] = ", cap, ptr);
		switch (cap) {
		case PCIY_PMG:
			cap_power(fd, p, ptr);
			break;
		case PCIY_AGP:
			cap_agp(fd, p, ptr);
			break;
		case PCIY_VPD:
			cap_vpd(fd, p, ptr);
			break;
		case PCIY_MSI:
			cap_msi(fd, p, ptr);
			break;
		case PCIY_PCIX:
			cap_pcix(fd, p, ptr);
			break;
		case PCIY_HT:
			cap_ht(fd, p, ptr);
			break;
		case PCIY_VENDOR:
			cap_vendor(fd, p, ptr);
			break;
		case PCIY_DEBUG:
			cap_debug(fd, p, ptr);
			break;
		case PCIY_SUBVENDOR:
			cap_subvendor(fd, p, ptr);
			break;
		case PCIY_EXPRESS:
			express = 1;
			cap_express(fd, p, ptr);
			break;
		case PCIY_MSIX:
			cap_msix(fd, p, ptr);
			break;
		case PCIY_SATA:
			cap_sata(fd, p, ptr);
			break;
		case PCIY_PCIAF:
			cap_pciaf(fd, p, ptr);
			break;
		default:
			printf("unknown");
			break;
		}
		printf("\n");
		ptr = read_config(fd, &p->pc_sel, ptr + PCICAP_NEXTPTR, 1);
	}

	if (express)
		list_ecaps(fd, p);
}

/* From <sys/systm.h>. */
static __inline uint32_t
bitcount32(uint32_t x)
{

	x = (x & 0x55555555) + ((x & 0xaaaaaaaa) >> 1);
	x = (x & 0x33333333) + ((x & 0xcccccccc) >> 2);
	x = (x + (x >> 4)) & 0x0f0f0f0f;
	x = (x + (x >> 8));
	x = (x + (x >> 16)) & 0x000000ff;
	return (x);
}

static void
ecap_aer(int fd, struct pci_conf *p, uint16_t ptr, uint8_t ver)
{
	uint32_t sta, mask;

	printf("AER %d", ver);
	if (ver != 1)
		return;
	sta = read_config(fd, &p->pc_sel, ptr + PCIR_AER_UC_STATUS, 4);
	mask = read_config(fd, &p->pc_sel, ptr + PCIR_AER_UC_SEVERITY, 4);
	printf(" %d fatal", bitcount32(sta & mask));
	printf(" %d non-fatal", bitcount32(sta & ~mask));
	sta = read_config(fd, &p->pc_sel, ptr + PCIR_AER_COR_STATUS, 4);
	printf(" %d corrected", bitcount32(sta));
}

static void
ecap_vc(int fd, struct pci_conf *p, uint16_t ptr, uint8_t ver)
{
	uint32_t cap1;

	printf("VC %d", ver);
	if (ver != 1)
		return;
	cap1 = read_config(fd, &p->pc_sel, ptr + PCIR_VC_CAP1, 4);
	printf(" max VC%d", cap1 & PCIM_VC_CAP1_EXT_COUNT);
	if ((cap1 & PCIM_VC_CAP1_LOWPRI_EXT_COUNT) != 0)
		printf(" lowpri VC0-VC%d",
		    (cap1 & PCIM_VC_CAP1_LOWPRI_EXT_COUNT) >> 4);
}

static void
ecap_sernum(int fd, struct pci_conf *p, uint16_t ptr, uint8_t ver)
{
	uint32_t high, low;

	printf("Serial %d", ver);
	if (ver != 1)
		return;
	low = read_config(fd, &p->pc_sel, ptr + PCIR_SERIAL_LOW, 4);
	high = read_config(fd, &p->pc_sel, ptr + PCIR_SERIAL_HIGH, 4);
	printf(" %08x%08x", high, low);
}

static void
list_ecaps(int fd, struct pci_conf *p)
{
	uint32_t ecap;
	uint16_t ptr;

	ptr = PCIR_EXTCAP;
	ecap = read_config(fd, &p->pc_sel, ptr, 4);
	if (ecap == 0xffffffff || ecap == 0)
		return;
	for (;;) {
		printf("ecap %04x[%03x] = ", PCI_EXTCAP_ID(ecap), ptr);
		switch (PCI_EXTCAP_ID(ecap)) {
		case PCIZ_AER:
			ecap_aer(fd, p, ptr, PCI_EXTCAP_VER(ecap));
			break;
		case PCIZ_VC:
			ecap_vc(fd, p, ptr, PCI_EXTCAP_VER(ecap));
			break;
		case PCIZ_SERNUM:
			ecap_sernum(fd, p, ptr, PCI_EXTCAP_VER(ecap));
			break;
		default:
			printf("unknown %d", PCI_EXTCAP_VER(ecap));
			break;
		}
		printf("\n");
		ptr = PCI_EXTCAP_NEXTPTR(ecap);
		if (ptr == 0)
			break;
		ecap = read_config(fd, &p->pc_sel, ptr, 4);
	}
}
