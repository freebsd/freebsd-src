/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020 Alexander Motin <mav@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_acpi.h"
#include "opt_pci.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/callout.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/queue.h>
#include <sys/rman.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/aclocal.h>
#include <contrib/dev/acpica/include/actables.h>

#include <dev/acpica/acpivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

struct apei_ge {
	union {
		ACPI_HEST_GENERIC v1;
		ACPI_HEST_GENERIC_V2 v2;
	};
	int		 res_type;
	int		 res_rid;
	struct resource	*res;
	int		 res2_type;
	int		 res2_rid;
	struct resource	*res2;
	uint8_t		*buf, *copybuf;
	TAILQ_ENTRY(apei_ge) link;
	struct callout	 poll;
	void		*swi_ih;
} *apei_nmi_ge;

struct apei_softc {
	ACPI_TABLE_HEST *hest;
	TAILQ_HEAD(, apei_ge) ges;
};

struct apei_mem_error {
	uint64_t	ValidationBits;
	uint64_t	ErrorStatus;
	uint64_t	PhysicalAddress;
	uint64_t	PhysicalAddressMask;
	uint16_t	Node;
	uint16_t	Card;
	uint16_t	Module;
	uint16_t	Bank;
	uint16_t	Device;
	uint16_t	Row;
	uint16_t	Column;
	uint16_t	BitPosition;
	uint64_t	RequesterID;
	uint64_t	ResponderID;
	uint64_t	TargetID;
	uint8_t		MemoryErrorType;
	uint8_t		Extended;
	uint16_t	RankNumber;
	uint16_t	CardHandle;
	uint16_t	ModuleHandle;
};

struct apei_pcie_error {
	uint64_t	ValidationBits;
	uint32_t	PortType;
	uint32_t	Version;
	uint32_t	CommandStatus;
	uint32_t	Reserved;
	uint8_t		DeviceID[16];
	uint8_t		DeviceSerialNumber[8];
	uint8_t		BridgeControlStatus[4];
	uint8_t		CapabilityStructure[60];
	uint8_t		AERInfo[96];
};

#ifdef __i386__
static __inline uint64_t
apei_bus_read_8(struct resource *res, bus_size_t offset)
{
	return (bus_read_4(res, offset) |
	    ((uint64_t)bus_read_4(res, offset + 4)) << 32);
}
static __inline void
apei_bus_write_8(struct resource *res, bus_size_t offset, uint64_t val)
{
	bus_write_4(res, offset, val);
	bus_write_4(res, offset + 4, val >> 32);
}
#define	READ8(r, o)	apei_bus_read_8((r), (o))
#define	WRITE8(r, o, v)	apei_bus_write_8((r), (o), (v))
#else
#define	READ8(r, o)	bus_read_8((r), (o))
#define	WRITE8(r, o, v)	bus_write_8((r), (o), (v))
#endif

#define GED_SIZE(ged)	((ged)->Revision >= 0x300 ? \
    sizeof(ACPI_HEST_GENERIC_DATA_V300) : sizeof(ACPI_HEST_GENERIC_DATA))
#define GED_DATA(ged)	((uint8_t *)(ged) + GED_SIZE(ged))

int apei_nmi_handler(void);

static const char *
apei_severity(uint32_t s)
{
	switch (s) {
	case ACPI_HEST_GEN_ERROR_RECOVERABLE:
	    return ("Recoverable");
	case ACPI_HEST_GEN_ERROR_FATAL:
	    return ("Fatal");
	case ACPI_HEST_GEN_ERROR_CORRECTED:
	    return ("Corrected");
	case ACPI_HEST_GEN_ERROR_NONE:
	    return ("Informational");
	}
	return ("???");
}

static int
apei_mem_handler(ACPI_HEST_GENERIC_DATA *ged)
{
	struct apei_mem_error *p = (struct apei_mem_error *)GED_DATA(ged);

	printf("APEI %s Memory Error:\n", apei_severity(ged->ErrorSeverity));
	if (p->ValidationBits & 0x01)
		printf(" Error Status: 0x%jx\n", p->ErrorStatus);
	if (p->ValidationBits & 0x02)
		printf(" Physical Address: 0x%jx\n", p->PhysicalAddress);
	if (p->ValidationBits & 0x04)
		printf(" Physical Address Mask: 0x%jx\n", p->PhysicalAddressMask);
	if (p->ValidationBits & 0x08)
		printf(" Node: %u\n", p->Node);
	if (p->ValidationBits & 0x10)
		printf(" Card: %u\n", p->Card);
	if (p->ValidationBits & 0x20)
		printf(" Module: %u\n", p->Module);
	if (p->ValidationBits & 0x40)
		printf(" Bank: %u\n", p->Bank);
	if (p->ValidationBits & 0x80)
		printf(" Device: %u\n", p->Device);
	if (p->ValidationBits & 0x100)
		printf(" Row: %u\n", p->Row);
	if (p->ValidationBits & 0x200)
		printf(" Column: %u\n", p->Column);
	if (p->ValidationBits & 0x400)
		printf(" Bit Position: %u\n", p->BitPosition);
	if (p->ValidationBits & 0x800)
		printf(" Requester ID: 0x%jx\n", p->RequesterID);
	if (p->ValidationBits & 0x1000)
		printf(" Responder ID: 0x%jx\n", p->ResponderID);
	if (p->ValidationBits & 0x2000)
		printf(" Target ID: 0x%jx\n", p->TargetID);
	if (p->ValidationBits & 0x4000)
		printf(" Memory Error Type: %u\n", p->MemoryErrorType);
	if (p->ValidationBits & 0x8000)
		printf(" Rank Number: %u\n", p->RankNumber);
	if (p->ValidationBits & 0x10000)
		printf(" Card Handle: 0x%x\n", p->CardHandle);
	if (p->ValidationBits & 0x20000)
		printf(" Module Handle: 0x%x\n", p->ModuleHandle);
	if (p->ValidationBits & 0x40000)
		printf(" Extended Row: %u\n",
		    (uint32_t)(p->Extended & 0x3) << 16 | p->Row);
	if (p->ValidationBits & 0x80000)
		printf(" Bank Group: %u\n", p->Bank >> 8);
	if (p->ValidationBits & 0x100000)
		printf(" Bank Address: %u\n", p->Bank & 0xff);
	if (p->ValidationBits & 0x200000)
		printf(" Chip Identification: %u\n", (p->Extended >> 5) & 0x7);

	return (0);
}

static int
apei_pcie_handler(ACPI_HEST_GENERIC_DATA *ged)
{
	struct apei_pcie_error *p = (struct apei_pcie_error *)GED_DATA(ged);
	int h = 0, off;
#ifdef DEV_PCI
	device_t dev;
	int sev;

	if ((p->ValidationBits & 0x8) == 0x8) {
		mtx_lock(&Giant);
		dev = pci_find_dbsf((uint32_t)p->DeviceID[10] << 8 |
		    p->DeviceID[9], p->DeviceID[11], p->DeviceID[8],
		    p->DeviceID[7]);
		if (dev != NULL) {
			switch (ged->ErrorSeverity) {
			case ACPI_HEST_GEN_ERROR_FATAL:
				sev = PCIEM_STA_FATAL_ERROR;
				break;
			case ACPI_HEST_GEN_ERROR_RECOVERABLE:
				sev = PCIEM_STA_NON_FATAL_ERROR;
				break;
			default:
				sev = PCIEM_STA_CORRECTABLE_ERROR;
				break;
			}
			pcie_apei_error(dev, sev,
			    (p->ValidationBits & 0x80) ? p->AERInfo : NULL);
			h = 1;
		}
		mtx_unlock(&Giant);
	}
	if (h)
		return (h);
#endif

	printf("APEI %s PCIe Error:\n", apei_severity(ged->ErrorSeverity));
	if (p->ValidationBits & 0x01)
		printf(" Port Type: %u\n", p->PortType);
	if (p->ValidationBits & 0x02)
		printf(" Version: %x\n", p->Version);
	if (p->ValidationBits & 0x04)
		printf(" Command Status: 0x%08x\n", p->CommandStatus);
	if (p->ValidationBits & 0x08) {
		printf(" DeviceID:");
		for (off = 0; off < sizeof(p->DeviceID); off++)
			printf(" %02x", p->DeviceID[off]);
		printf("\n");
	}
	if (p->ValidationBits & 0x10) {
		printf(" Device Serial Number:");
		for (off = 0; off < sizeof(p->DeviceSerialNumber); off++)
			printf(" %02x", p->DeviceSerialNumber[off]);
		printf("\n");
	}
	if (p->ValidationBits & 0x20) {
		printf(" Bridge Control Status:");
		for (off = 0; off < sizeof(p->BridgeControlStatus); off++)
			printf(" %02x", p->BridgeControlStatus[off]);
		printf("\n");
	}
	if (p->ValidationBits & 0x40) {
		printf(" Capability Structure:\n");
		for (off = 0; off < sizeof(p->CapabilityStructure); off++) {
			printf(" %02x", p->CapabilityStructure[off]);
			if ((off % 16) == 15 ||
			    off + 1 == sizeof(p->CapabilityStructure))
				printf("\n");
		}
	}
	if (p->ValidationBits & 0x80) {
		printf(" AER Info:\n");
		for (off = 0; off < sizeof(p->AERInfo); off++) {
			printf(" %02x", p->AERInfo[off]);
			if ((off % 16) == 15 || off + 1 == sizeof(p->AERInfo))
				printf("\n");
		}
	}
	return (h);
}

static void
apei_ged_handler(ACPI_HEST_GENERIC_DATA *ged)
{
	ACPI_HEST_GENERIC_DATA_V300 *ged3 = (ACPI_HEST_GENERIC_DATA_V300 *)ged;
	/* A5BC1114-6F64-4EDE-B863-3E83ED7C83B1 */
	static uint8_t mem_uuid[ACPI_UUID_LENGTH] = {
		0x14, 0x11, 0xBC, 0xA5, 0x64, 0x6F, 0xDE, 0x4E,
		0xB8, 0x63, 0x3E, 0x83, 0xED, 0x7C, 0x83, 0xB1
	};
	/* D995E954-BBC1-430F-AD91-B44DCB3C6F35 */
	static uint8_t pcie_uuid[ACPI_UUID_LENGTH] = {
		0x54, 0xE9, 0x95, 0xD9, 0xC1, 0xBB, 0x0F, 0x43,
		0xAD, 0x91, 0xB4, 0x4D, 0xCB, 0x3C, 0x6F, 0x35
	};
	uint8_t *t;
	int h = 0, off;

	if (memcmp(mem_uuid, ged->SectionType, ACPI_UUID_LENGTH) == 0) {
		h = apei_mem_handler(ged);
	} else if (memcmp(pcie_uuid, ged->SectionType, ACPI_UUID_LENGTH) == 0) {
		h = apei_pcie_handler(ged);
	} else {
		t = ged->SectionType;
		printf("APEI %s Error %02x%02x%02x%02x-%02x%02x-"
		    "%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x:\n",
		    apei_severity(ged->ErrorSeverity),
		    t[3], t[2], t[1], t[0], t[5], t[4], t[7], t[6],
		    t[8], t[9], t[10], t[11], t[12], t[13], t[14], t[15]);
		printf(" Error Data:\n");
		t = (uint8_t *)GED_DATA(ged);
		for (off = 0; off < ged->ErrorDataLength; off++) {
			printf(" %02x", t[off]);
			if ((off % 16) == 15 || off + 1 == ged->ErrorDataLength)
				printf("\n");
		}
	}
	if (h)
		return;

	printf(" Flags: 0x%x\n", ged->Flags);
	if (ged->ValidationBits & ACPI_HEST_GEN_VALID_FRU_ID) {
		t = ged->FruId;
		printf(" FRU Id: %02x%02x%02x%02x-%02x%02x-%02x%02x-"
		    "%02x%02x-%02x%02x%02x%02x%02x%02x\n",
		    t[3], t[2], t[1], t[0], t[5], t[4], t[7], t[6],
		    t[8], t[9], t[10], t[11], t[12], t[13], t[14], t[15]);
	}
	if (ged->ValidationBits & ACPI_HEST_GEN_VALID_FRU_STRING)
		printf(" FRU Text: %.20s\n", ged->FruText);
	if (ged->Revision >= 0x300 &&
	    ged->ValidationBits & ACPI_HEST_GEN_VALID_TIMESTAMP)
		printf(" Timestamp: %016jx\n", ged3->TimeStamp);
}

static int
apei_ge_handler(struct apei_ge *ge, bool copy)
{
	uint8_t *buf = copy ? ge->copybuf : ge->buf;
	ACPI_HEST_GENERIC_STATUS *ges = (ACPI_HEST_GENERIC_STATUS *)buf;
	ACPI_HEST_GENERIC_DATA *ged;
	uint32_t sev;
	int i, c, off;

	if (ges == NULL || ges->BlockStatus == 0)
		return (0);

	c = (ges->BlockStatus >> 4) & 0x3ff;
	sev = ges->ErrorSeverity;

	/* Process error entries. */
	for (off = i = 0; i < c && off + sizeof(*ged) <= ges->DataLength; i++) {
		ged = (ACPI_HEST_GENERIC_DATA *)&buf[sizeof(*ges) + off];
		apei_ged_handler(ged);
		off += GED_SIZE(ged) + ged->ErrorDataLength;
	}

	/* Acknowledge the error has been processed. */
	ges->BlockStatus = 0;
	if (!copy && ge->v1.Header.Type == ACPI_HEST_TYPE_GENERIC_ERROR_V2 &&
	    ge->res2) {
		uint64_t val = READ8(ge->res2, 0);
		val &= ge->v2.ReadAckPreserve;
		val |= ge->v2.ReadAckWrite;
		WRITE8(ge->res2, 0, val);
	}

	/* If ACPI told the error is fatal -- make it so. */
	if (sev == ACPI_HEST_GEN_ERROR_FATAL)
		panic("APEI Fatal Hardware Error!");

	return (1);
}

static void
apei_nmi_swi(void *arg)
{
	struct apei_ge *ge = arg;

	apei_ge_handler(ge, true);
}

int
apei_nmi_handler(void)
{
	struct apei_ge *ge = apei_nmi_ge;
	ACPI_HEST_GENERIC_STATUS *ges, *gesc;

	if (ge == NULL)
		return (0);

	ges = (ACPI_HEST_GENERIC_STATUS *)ge->buf;
	if (ges == NULL || ges->BlockStatus == 0)
		return (0);

	/* If ACPI told the error is fatal -- make it so. */
	if (ges->ErrorSeverity == ACPI_HEST_GEN_ERROR_FATAL)
		panic("APEI Fatal Hardware Error!");

	/* Copy the buffer for later processing. */
	gesc = (ACPI_HEST_GENERIC_STATUS *)ge->copybuf;
	if (gesc->BlockStatus == 0)
		memcpy(ge->copybuf, ge->buf, ge->v1.ErrorBlockLength);

	/* Acknowledge the error has been processed. */
	ges->BlockStatus = 0;
	if (ge->v1.Header.Type == ACPI_HEST_TYPE_GENERIC_ERROR_V2 &&
	    ge->res2) {
		uint64_t val = READ8(ge->res2, 0);
		val &= ge->v2.ReadAckPreserve;
		val |= ge->v2.ReadAckWrite;
		WRITE8(ge->res2, 0, val);
	}

	/* Schedule SWI for real handling. */
	swi_sched(ge->swi_ih, SWI_FROMNMI);

	return (1);
}

static void
apei_callout_handler(void *context)
{
	struct apei_ge *ge = context;

	apei_ge_handler(ge, false);
	callout_schedule(&ge->poll, ge->v1.Notify.PollInterval * hz / 1000);
}

static void
apei_notify_handler(ACPI_HANDLE h, UINT32 notify, void *context)
{
	device_t dev = context;
	struct apei_softc *sc = device_get_softc(dev);
	struct apei_ge *ge;

	TAILQ_FOREACH(ge, &sc->ges, link) {
		if (ge->v1.Notify.Type == ACPI_HEST_NOTIFY_SCI ||
		    ge->v1.Notify.Type == ACPI_HEST_NOTIFY_GPIO ||
		    ge->v1.Notify.Type == ACPI_HEST_NOTIFY_GSIV)
			apei_ge_handler(ge, false);
	}
}

static int
hest_parse_structure(struct apei_softc *sc, void *addr, int remaining)
{
	ACPI_HEST_HEADER *hdr = addr;
	struct apei_ge *ge;

	if (remaining < (int)sizeof(ACPI_HEST_HEADER))
		return (-1);

	switch (hdr->Type) {
	case ACPI_HEST_TYPE_IA32_CHECK: {
		ACPI_HEST_IA_MACHINE_CHECK *s = addr;
		return (sizeof(*s) + s->NumHardwareBanks *
		    sizeof(ACPI_HEST_IA_ERROR_BANK));
	}
	case ACPI_HEST_TYPE_IA32_CORRECTED_CHECK: {
		ACPI_HEST_IA_CORRECTED *s = addr;
		return (sizeof(*s) + s->NumHardwareBanks *
		    sizeof(ACPI_HEST_IA_ERROR_BANK));
	}
	case ACPI_HEST_TYPE_IA32_NMI: {
		ACPI_HEST_IA_NMI *s = addr;
		return (sizeof(*s));
	}
	case ACPI_HEST_TYPE_AER_ROOT_PORT: {
		ACPI_HEST_AER_ROOT *s = addr;
		return (sizeof(*s));
	}
	case ACPI_HEST_TYPE_AER_ENDPOINT: {
		ACPI_HEST_AER *s = addr;
		return (sizeof(*s));
	}
	case ACPI_HEST_TYPE_AER_BRIDGE: {
		ACPI_HEST_AER_BRIDGE *s = addr;
		return (sizeof(*s));
	}
	case ACPI_HEST_TYPE_GENERIC_ERROR: {
		ACPI_HEST_GENERIC *s = addr;
		ge = malloc(sizeof(*ge), M_DEVBUF, M_WAITOK | M_ZERO);
		ge->v1 = *s;
		TAILQ_INSERT_TAIL(&sc->ges, ge, link);
		return (sizeof(*s));
	}
	case ACPI_HEST_TYPE_GENERIC_ERROR_V2: {
		ACPI_HEST_GENERIC_V2 *s = addr;
		ge = malloc(sizeof(*ge), M_DEVBUF, M_WAITOK | M_ZERO);
		ge->v2 = *s;
		TAILQ_INSERT_TAIL(&sc->ges, ge, link);
		return (sizeof(*s));
	}
	case ACPI_HEST_TYPE_IA32_DEFERRED_CHECK: {
		ACPI_HEST_IA_DEFERRED_CHECK *s = addr;
		return (sizeof(*s) + s->NumHardwareBanks *
		    sizeof(ACPI_HEST_IA_ERROR_BANK));
	}
	default:
		return (-1);
	}
}

static void
hest_parse_table(struct apei_softc *sc)
{
	ACPI_TABLE_HEST *hest = sc->hest;
	char *cp;
	int remaining, consumed;

	remaining = hest->Header.Length - sizeof(ACPI_TABLE_HEST);
	while (remaining > 0) {
		cp = (char *)hest + hest->Header.Length - remaining;
		consumed = hest_parse_structure(sc, cp, remaining);
		if (consumed <= 0)
			break;
		else
			remaining -= consumed;
	}
}

static char *apei_ids[] = { "PNP0C33", NULL };
static devclass_t apei_devclass;

static ACPI_STATUS
apei_find(ACPI_HANDLE handle, UINT32 level, void *context,
    void **status)
{
	int *found = (int *)status;
	char **ids;

	for (ids = apei_ids; *ids != NULL; ids++) {
		if (acpi_MatchHid(handle, *ids)) {
			*found = 1;
			break;
		}
	}
	return (AE_OK);
}

static void
apei_identify(driver_t *driver, device_t parent)
{
	device_t	child;
	int		found;
	ACPI_TABLE_HEADER *hest;
	ACPI_STATUS	status;

	if (acpi_disabled("apei"))
		return;

	/* Without HEST table we have nothing to do. */
	status = AcpiGetTable(ACPI_SIG_HEST, 0, &hest);
	if (ACPI_FAILURE(status))
		return;
	AcpiPutTable(hest);

	/* Only one APEI device can exist. */
	if (devclass_get_device(apei_devclass, 0))
		return;

	/* Search for ACPI error device to be used. */
	found = 0;
	AcpiWalkNamespace(ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT,
	    100, apei_find, NULL, NULL, (void *)&found);
	if (found)
		return;

	/* If not found - create a fake one. */
	child = BUS_ADD_CHILD(parent, 2, "apei", 0);
	if (child == NULL)
		printf("%s: can't add child\n", __func__);
}

static int
apei_probe(device_t dev)
{
	ACPI_TABLE_HEADER *hest;
	ACPI_STATUS	status;
	int rv;

	if (acpi_disabled("apei"))
		return (ENXIO);

	if (acpi_get_handle(dev) != NULL) {
		rv = (ACPI_ID_PROBE(device_get_parent(dev), dev, apei_ids) == NULL);
		if (rv > 0)
			return (rv);
	} else
		rv = 0;

	/* Without HEST table we have nothing to do. */
	status = AcpiGetTable(ACPI_SIG_HEST, 0, &hest);
	if (ACPI_FAILURE(status))
		return (ENXIO);
	AcpiPutTable(hest);

	device_set_desc(dev, "ACPI Platform Error Interface");
	return (rv);
}

static int
apei_attach(device_t dev)
{
	struct apei_softc *sc = device_get_softc(dev);
	struct apei_ge *ge;
	ACPI_STATUS status;
	int rid;

	TAILQ_INIT(&sc->ges);

	/* Search and parse HEST table. */
	status = AcpiGetTable(ACPI_SIG_HEST, 0, (ACPI_TABLE_HEADER **)&sc->hest);
	if (ACPI_FAILURE(status))
		return (ENXIO);
	hest_parse_table(sc);
	AcpiPutTable((ACPI_TABLE_HEADER *)sc->hest);

	rid = 0;
	TAILQ_FOREACH(ge, &sc->ges, link) {
		ge->res_rid = rid++;
		acpi_bus_alloc_gas(dev, &ge->res_type, &ge->res_rid,
		    &ge->v1.ErrorStatusAddress, &ge->res, 0);
		if (ge->res) {
			ge->buf = pmap_mapdev_attr(READ8(ge->res, 0),
			    ge->v1.ErrorBlockLength, VM_MEMATTR_WRITE_COMBINING);
		} else {
			device_printf(dev, "Can't allocate status resource.\n");
		}
		if (ge->v1.Header.Type == ACPI_HEST_TYPE_GENERIC_ERROR_V2) {
			ge->res2_rid = rid++;
			acpi_bus_alloc_gas(dev, &ge->res2_type, &ge->res2_rid,
			    &ge->v2.ReadAckRegister, &ge->res2, 0);
			if (ge->res2 == NULL)
				device_printf(dev, "Can't allocate ack resource.\n");
		}
		if (ge->v1.Notify.Type == ACPI_HEST_NOTIFY_POLLED) {
			callout_init(&ge->poll, 1);
			callout_reset(&ge->poll,
			    ge->v1.Notify.PollInterval * hz / 1000,
			    apei_callout_handler, ge);
		} else if (ge->v1.Notify.Type == ACPI_HEST_NOTIFY_NMI) {
			ge->copybuf = malloc(ge->v1.ErrorBlockLength,
			    M_DEVBUF, M_WAITOK | M_ZERO);
			swi_add(&clk_intr_event, "apei", apei_nmi_swi, ge,
			    SWI_CLOCK, INTR_MPSAFE, &ge->swi_ih);
			apei_nmi_ge = ge;
			apei_nmi = apei_nmi_handler;
		}
	}

	if (acpi_get_handle(dev) != NULL) {
		AcpiInstallNotifyHandler(acpi_get_handle(dev),
		    ACPI_DEVICE_NOTIFY, apei_notify_handler, dev);
	}
	return (0);
}

static int
apei_detach(device_t dev)
{
	struct apei_softc *sc = device_get_softc(dev);
	struct apei_ge *ge;

	apei_nmi = NULL;
	apei_nmi_ge = NULL;
	if (acpi_get_handle(dev) != NULL) {
		AcpiRemoveNotifyHandler(acpi_get_handle(dev),
		    ACPI_DEVICE_NOTIFY, apei_notify_handler);
	}

	while ((ge = TAILQ_FIRST(&sc->ges)) != NULL) {
		TAILQ_REMOVE(&sc->ges, ge, link);
		if (ge->res) {
			bus_release_resource(dev, ge->res_type,
			    ge->res_rid, ge->res);
		}
		if (ge->res2) {
			bus_release_resource(dev, ge->res2_type,
			    ge->res2_rid, ge->res2);
		}
		if (ge->v1.Notify.Type == ACPI_HEST_NOTIFY_POLLED) {
			callout_drain(&ge->poll);
		} else if (ge->v1.Notify.Type == ACPI_HEST_NOTIFY_NMI) {
			swi_remove(&ge->swi_ih);
			free(ge->copybuf, M_DEVBUF);
		}
		if (ge->buf) {
			pmap_unmapdev((vm_offset_t)ge->buf,
			    ge->v1.ErrorBlockLength);
		}
		free(ge, M_DEVBUF);
	}
	return (0);
}

static device_method_t apei_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify, apei_identify),
	DEVMETHOD(device_probe, apei_probe),
	DEVMETHOD(device_attach, apei_attach),
	DEVMETHOD(device_detach, apei_detach),
	DEVMETHOD_END
};

static driver_t	apei_driver = {
	"apei",
	apei_methods,
	sizeof(struct apei_softc),
};

DRIVER_MODULE(apei, acpi, apei_driver, apei_devclass, 0, 0);
MODULE_DEPEND(apei, acpi, 1, 1, 1);
