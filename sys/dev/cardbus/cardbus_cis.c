/*-
 * Copyright (c) 2000,2001 Jonathan Chen.
 * All rights reserved.
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

/*
 * CIS Handling for the Cardbus Bus
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <sys/bus.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>
#include <sys/endian.h>

#include <sys/pciio.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <dev/pccard/pccardvar.h>
#include <dev/pccard/pccard_cis.h>

#include <dev/cardbus/cardbusreg.h>
#include <dev/cardbus/cardbusvar.h>
#include <dev/cardbus/cardbus_cis.h>

extern int cardbus_cis_debug;

#define	DPRINTF(a) if (cardbus_cis_debug) printf a
#define	DEVPRINTF(x) if (cardbus_cis_debug) device_printf x

struct tuple_callbacks;

typedef int (tuple_cb) (device_t cbdev, device_t child, int id, int len,
		 uint8_t *tupledata, uint32_t start, uint32_t *off,
		 struct tuple_callbacks *info);

struct tuple_callbacks {
	int	id;
	char	*name;
	tuple_cb *func;
};

static int decode_tuple_generic(device_t cbdev, device_t child, int id,
    int len, uint8_t *tupledata, uint32_t start, uint32_t *off,
    struct tuple_callbacks *info);
static int decode_tuple_linktarget(device_t cbdev, device_t child, int id,
    int len, uint8_t *tupledata, uint32_t start, uint32_t *off,
    struct tuple_callbacks *info);
static int decode_tuple_vers_1(device_t cbdev, device_t child, int id,
    int len, uint8_t *tupledata, uint32_t start, uint32_t *off,
    struct tuple_callbacks *info);
static int decode_tuple_funcid(device_t cbdev, device_t child, int id,
    int len, uint8_t *tupledata, uint32_t start, uint32_t *off,
    struct tuple_callbacks *info);
static int decode_tuple_manfid(device_t cbdev, device_t child, int id,
    int len, uint8_t *tupledata, uint32_t start, uint32_t *off,
    struct tuple_callbacks *info);
static int decode_tuple_funce(device_t cbdev, device_t child, int id,
    int len, uint8_t *tupledata, uint32_t start, uint32_t *off,
    struct tuple_callbacks *info);
static int decode_tuple_bar(device_t cbdev, device_t child, int id,
    int len, uint8_t *tupledata, uint32_t start, uint32_t *off,
    struct tuple_callbacks *info);
static int decode_tuple_unhandled(device_t cbdev, device_t child, int id,
    int len, uint8_t *tupledata, uint32_t start, uint32_t *off,
    struct tuple_callbacks *info);
static int decode_tuple_end(device_t cbdev, device_t child, int id,
    int len, uint8_t *tupledata, uint32_t start, uint32_t *off,
    struct tuple_callbacks *info);

static int	cardbus_read_tuple_conf(device_t cbdev, device_t child,
		    uint32_t start, uint32_t *off, int *tupleid, int *len,
		    uint8_t *tupledata);
static int	cardbus_read_tuple_mem(device_t cbdev, struct resource *res,
		    uint32_t start, uint32_t *off, int *tupleid, int *len,
		    uint8_t *tupledata);
static int	cardbus_read_tuple(device_t cbdev, device_t child,
		    struct resource *res, uint32_t start, uint32_t *off,
		    int *tupleid, int *len, uint8_t *tupledata);
static void	cardbus_read_tuple_finish(device_t cbdev, device_t child,
		    int rid, struct resource *res);
static struct resource	*cardbus_read_tuple_init(device_t cbdev, device_t child,
		    uint32_t *start, int *rid);
static int	decode_tuple(device_t cbdev, device_t child, int tupleid,
		    int len, uint8_t *tupledata, uint32_t start,
		    uint32_t *off, struct tuple_callbacks *callbacks);
static int	cardbus_parse_cis(device_t cbdev, device_t child,
		    struct tuple_callbacks *callbacks);

#define	MAKETUPLE(NAME,FUNC) { CISTPL_ ## NAME, #NAME, decode_tuple_ ## FUNC }

static char *funcnames[] = {
	"Multi-Functioned",
	"Memory",
	"Serial Port",
	"Parallel Port",
	"Fixed Disk",
	"Video Adaptor",
	"Network Adaptor",
	"AIMS",
	"SCSI",
	"Security"
};

/*
 * Handler functions for various CIS tuples
 */

static int
decode_tuple_generic(device_t cbdev, device_t child, int id,
    int len, uint8_t *tupledata, uint32_t start, uint32_t *off,
    struct tuple_callbacks *info)
{
	int i;

	if (cardbus_cis_debug) {
		if (info)
			printf("TUPLE: %s [%d]:", info->name, len);
		else
			printf("TUPLE: Unknown(0x%02x) [%d]:", id, len);

		for (i = 0; i < len; i++) {
			if (i % 0x10 == 0 && len > 0x10)
				printf("\n       0x%02x:", i);
			printf(" %02x", tupledata[i]);
		}
		printf("\n");
	}
	return (0);
}

static int
decode_tuple_linktarget(device_t cbdev, device_t child, int id,
    int len, uint8_t *tupledata, uint32_t start, uint32_t *off,
    struct tuple_callbacks *info)
{
	int i;

	if (cardbus_cis_debug) {
		printf("TUPLE: %s [%d]:", info->name, len);

		for (i = 0; i < len; i++) {
			if (i % 0x10 == 0 && len > 0x10)
				printf("\n       0x%02x:", i);
			printf(" %02x", tupledata[i]);
		}
		printf("\n");
	}
	if (len != 3 || tupledata[0] != 'C' || tupledata[1] != 'I' ||
	    tupledata[2] != 'S') {
		printf("Invalid data for CIS Link Target!\n");
		decode_tuple_generic(cbdev, child, id, len, tupledata,
		    start, off, info);
		return (EINVAL);
	}
	return (0);
}

static int
decode_tuple_vers_1(device_t cbdev, device_t child, int id,
    int len, uint8_t *tupledata, uint32_t start, uint32_t *off,
    struct tuple_callbacks *info)
{
	int i;

	if (cardbus_cis_debug) {
		printf("Product version: %d.%d\n", tupledata[0], tupledata[1]);
		printf("Product name: ");
		for (i = 2; i < len; i++) {
			if (tupledata[i] == '\0')
				printf(" | ");
			else if (tupledata[i] == 0xff)
				break;
			else
				printf("%c", tupledata[i]);
		}
		printf("\n");
	}
	return (0);
}

static int
decode_tuple_funcid(device_t cbdev, device_t child, int id,
    int len, uint8_t *tupledata, uint32_t start, uint32_t *off,
    struct tuple_callbacks *info)
{
	struct cardbus_devinfo *dinfo = device_get_ivars(child);
	int numnames = sizeof(funcnames) / sizeof(funcnames[0]);
	int i;

	if (cardbus_cis_debug) {
		printf("Functions: ");
		for (i = 0; i < len; i++) {
			if (tupledata[i] < numnames)
				printf("%s", funcnames[tupledata[i]]);
			else
				printf("Unknown(%d)", tupledata[i]);
			if (i < len-1)
				printf(", ");
		}
		printf("\n");
	}
	if (len > 0)
		dinfo->funcid = tupledata[0];		/* use first in list */
	return (0);
}

static int
decode_tuple_manfid(device_t cbdev, device_t child, int id,
    int len, uint8_t *tupledata, uint32_t start, uint32_t *off,
    struct tuple_callbacks *info)
{
	struct cardbus_devinfo *dinfo = device_get_ivars(child);
	int i;

	if (cardbus_cis_debug) {
		printf("Manufacturer ID: ");
		for (i = 0; i < len; i++)
			printf("%02x", tupledata[i]);
		printf("\n");
	}

	if (len == 5) {
		dinfo->mfrid = tupledata[1] | (tupledata[2] << 8);
		dinfo->prodid = tupledata[3] | (tupledata[4] << 8);
	}
	return (0);
}

static int
decode_tuple_funce(device_t cbdev, device_t child, int id,
    int len, uint8_t *tupledata, uint32_t start, uint32_t *off,
    struct tuple_callbacks *info)
{
	struct cardbus_devinfo *dinfo = device_get_ivars(child);
	int type, i;

	if (cardbus_cis_debug) {
		printf("Function Extension: ");
		for (i = 0; i < len; i++)
			printf("%02x", tupledata[i]);
		printf("\n");
	}
	if (len < 2)			/* too short */
		return (0);
	type = tupledata[0];		/* XXX <32 always? */
	switch (dinfo->funcid) {
	case PCCARD_FUNCTION_NETWORK:
		switch (type) {
		case PCCARD_TPLFE_TYPE_LAN_NID:
			if (tupledata[1] > sizeof(dinfo->funce.lan.nid)) {
				/* ignore, warning? */
				return (0);
			}
			bcopy(tupledata + 2, dinfo->funce.lan.nid,
			    tupledata[1]);
			break;
		}
		dinfo->fepresent |= 1<<type;
		break;
	}
	return (0);
}

static int
decode_tuple_bar(device_t cbdev, device_t child, int id,
    int len, uint8_t *tupledata, uint32_t start, uint32_t *off,
    struct tuple_callbacks *info)
{
	struct cardbus_devinfo *dinfo = device_get_ivars(child);
	int type;
	uint8_t reg;
	uint32_t bar, pci_bar;

	if (len != 6) {
		device_printf(cbdev, "CIS BAR length not 6 (%d)\n", len);
		return (EINVAL);
	}

	reg = *tupledata;
	len = le32toh(*(uint32_t*)(tupledata + 2));
	if (reg & TPL_BAR_REG_AS) {
		type = SYS_RES_IOPORT;
	} else {
		type = SYS_RES_MEMORY;
	}

	bar = reg & TPL_BAR_REG_ASI_MASK;
	if (bar == 0) {
		device_printf(cbdev, "Invalid BAR type 0 in CIS\n");
		return (EINVAL);	/* XXX Return an error? */
	} else if (bar == 7) {
		/* XXX Should we try to map in Option ROMs? */
		return (0);
	}

	/* Convert from BAR type to BAR offset */
	bar = CARDBUS_BASE0_REG + (bar - 1) * 4;

	if (type == SYS_RES_MEMORY) {
		if (reg & TPL_BAR_REG_PREFETCHABLE)
			dinfo->mprefetchable |= BARBIT(bar);
#if 0
		/*
		 * XXX: It appears from a careful reading of the spec
		 * that we're not supposed to honor this when the bridge
		 * is not on the main system bus.  PCI spec doesn't appear
		 * to allow for memory ranges not listed in the bridge's
		 * decode range to be decoded.  The PC Card spec seems to
		 * indicate that this should only be done on x86 based
		 * machines, which seems to imply that on non-x86 machines
		 * the adddresses can be anywhere.  This further implies that
		 * since the hardware can do it on non-x86 machines, it should
		 * be able to do it on x86 machines.  Therefore, we can and
		 * should ignore this hint.  Furthermore, the PC Card spec
		 * recommends always allocating memory above 1MB, contradicting
		 * the other part of the PC Card spec.
		 *
		 * NetBSD ignores this bit, but it also ignores the
		 * prefetchable bit too, so that's not an indication of
		 * correctness.
		 */
		if (reg & TPL_BAR_REG_BELOW1MB)
			dinfo->mbelow1mb |= BARBIT(bar);
#endif
	}

	/*
	 * Sanity check the BAR length reported in the CIS with the length
	 * encoded in the PCI BAR.  The latter seems to be more reliable.
	 * XXX - This probably belongs elsewhere.
	 */
	pci_write_config(child, bar, 0xffffffff, 4);
	pci_bar = pci_read_config(child, bar, 4);
	if ((pci_bar != 0x0) && (pci_bar != 0xffffffff)) {
		if (type == SYS_RES_MEMORY) {
			pci_bar &= ~0xf;
		} else {
			pci_bar &= ~0x3;
		}
		len = 1 << (ffs(pci_bar) - 1);
	}

	DEVPRINTF((cbdev, "Opening BAR: type=%s, bar=%02x, len=%04x%s%s\n",
	    (type == SYS_RES_MEMORY) ? "MEM" : "IO", bar, len,
	    (type == SYS_RES_MEMORY && dinfo->mprefetchable & BARBIT(bar)) ?
	    " (Prefetchable)" : "", type == SYS_RES_MEMORY ?
	    ((dinfo->mbelow1mb & BARBIT(bar)) ? " (Below 1Mb)" : "") : ""));

	resource_list_add(&dinfo->pci.resources, type, bar, 0UL, ~0UL, len);

	/*
	 * Mark the appropriate bit in the PCI command register so that
	 * device drivers will know which type of BARs can be used.
	 */
	pci_enable_io(child, type);
	return (0);
}

static int
decode_tuple_unhandled(device_t cbdev, device_t child, int id,
    int len, uint8_t *tupledata, uint32_t start, uint32_t *off,
    struct tuple_callbacks *info)
{
	/* Make this message suck less XXX */
	printf("TUPLE: %s [%d] is unhandled! Bailing...", info->name, len);
	return (-1);
}

static int
decode_tuple_end(device_t cbdev, device_t child, int id,
    int len, uint8_t *tupledata, uint32_t start, uint32_t *off,
    struct tuple_callbacks *info)
{
	if (cardbus_cis_debug) {
		printf("CIS reading done\n");
	}
	return (0);
}

/*
 * Functions to read the a tuple from the card
 */

static int
cardbus_read_tuple_conf(device_t cbdev, device_t child, uint32_t start,
    uint32_t *off, int *tupleid, int *len, uint8_t *tupledata)
{
	int i, j;
	uint32_t e;
	uint32_t loc;

	loc = start + *off;

	e = pci_read_config(child, loc - loc % 4, 4);
	for (j = loc % 4; j > 0; j--)
		e >>= 8;
	*len = 0;
	for (i = loc, j = -2; j < *len; j++, i++) {
		if (i % 4 == 0)
			e = pci_read_config(child, i, 4);
		if (j == -2)
			*tupleid = 0xff & e;
		else if (j == -1)
			*len = 0xff & e;
		else
			tupledata[j] = 0xff & e;
		e >>= 8;
	}
	*off += *len + 2;
	return (0);
}

static int
cardbus_read_tuple_mem(device_t cbdev, struct resource *res, uint32_t start,
    uint32_t *off, int *tupleid, int *len, uint8_t *tupledata)
{
	bus_space_tag_t bt;
	bus_space_handle_t bh;
	int ret;

	bt = rman_get_bustag(res);
	bh = rman_get_bushandle(res);

	*tupleid = bus_space_read_1(bt, bh, start + *off);
	*len = bus_space_read_1(bt, bh, start + *off + 1);
	bus_space_read_region_1(bt, bh, *off + start + 2, tupledata, *len);
	ret = 0;
	*off += *len + 2;
	return (ret);
}

static int
cardbus_read_tuple(device_t cbdev, device_t child, struct resource *res,
    uint32_t start, uint32_t *off, int *tupleid, int *len,
    uint8_t *tupledata)
{
	if (res == (struct resource*)~0UL) {
		return (cardbus_read_tuple_conf(cbdev, child, start, off,
		    tupleid, len, tupledata));
	} else {
		return (cardbus_read_tuple_mem(cbdev, res, start, off,
		    tupleid, len, tupledata));
	}
}

static void
cardbus_read_tuple_finish(device_t cbdev, device_t child, int rid,
    struct resource *res)
{
	if (res != (struct resource*)~0UL) {
		bus_release_resource(cbdev, SYS_RES_MEMORY, rid, res);
		pci_write_config(child, rid, 0, 4);
		PCI_DISABLE_IO(cbdev, child, SYS_RES_MEMORY);
	}
}

static struct resource *
cardbus_read_tuple_init(device_t cbdev, device_t child, uint32_t *start,
    int *rid)
{
	uint32_t testval;
	uint32_t size;
	struct resource *res;

	switch (CARDBUS_CIS_SPACE(*start)) {
	case CARDBUS_CIS_ASI_TUPLE:
		/* CIS in PCI config space need no initialization */
		return ((struct resource*)~0UL);
	case CARDBUS_CIS_ASI_BAR0:
	case CARDBUS_CIS_ASI_BAR1:
	case CARDBUS_CIS_ASI_BAR2:
	case CARDBUS_CIS_ASI_BAR3:
	case CARDBUS_CIS_ASI_BAR4:
	case CARDBUS_CIS_ASI_BAR5:
		*rid = CARDBUS_BASE0_REG + (CARDBUS_CIS_SPACE(*start) - 1) * 4;
		break;
	case CARDBUS_CIS_ASI_ROM:
		*rid = CARDBUS_ROM_REG;
#if 0
		/*
		 * This mask doesn't contain the bit that actually enables
		 * the Option ROM.
		 */
		pci_write_config(child, *rid, CARDBUS_ROM_ADDRMASK, 4);
#endif
		break;
	default:
		device_printf(cbdev, "Unable to read CIS: Unknown space: %d\n",
		    CARDBUS_CIS_SPACE(*start));
		return (NULL);
	}

	/* figure out how much space we need */
	pci_write_config(child, *rid, 0xffffffff, 4);
	testval = pci_read_config(child, *rid, 4);

	/*
	 * This bit has a different meaning depending if we are dealing
	 * with a normal BAR or an Option ROM BAR.
	 */
	if (((testval & 0x1) == 0x1) && (*rid != CARDBUS_ROM_REG)) {
		device_printf(cbdev, "CIS Space is IO, expecting memory.\n");
		return (NULL);
	}

	size = CARDBUS_MAPREG_MEM_SIZE(testval);
	/* XXX Is this some kind of hack? */
	if (size < 4096)
		size = 4096;
	/* allocate the memory space to read CIS */
	res = bus_alloc_resource(cbdev, SYS_RES_MEMORY, rid, 0, ~0, size,
	    rman_make_alignment_flags(size) | RF_ACTIVE);
	if (res == NULL) {
		device_printf(cbdev, "Unable to allocate resource "
		    "to read CIS.\n");
		return (NULL);
	}
	pci_write_config(child, *rid,
	    rman_get_start(res) | ((*rid == CARDBUS_ROM_REG)?
		CARDBUS_ROM_ENABLE : 0),
	    4);
	PCI_ENABLE_IO(cbdev, child, SYS_RES_MEMORY);

	/* Flip to the right ROM image if CIS is in ROM */
	if (CARDBUS_CIS_SPACE(*start) == CARDBUS_CIS_ASI_ROM) {
		bus_space_tag_t bt;
		bus_space_handle_t bh;
		uint32_t imagesize;
		uint32_t imagebase = 0;
		uint32_t pcidata;
		uint16_t romsig;
		int romnum = 0;
		int imagenum;

		bt = rman_get_bustag(res);
		bh = rman_get_bushandle(res);

		imagenum = CARDBUS_CIS_ASI_ROM_IMAGE(*start);
		for (romnum = 0;; romnum++) {
			romsig = bus_space_read_2(bt, bh,
			    imagebase + CARDBUS_EXROM_SIGNATURE);
			if (romsig != 0xaa55) {
				device_printf(cbdev, "Bad header in rom %d: "
				    "[%x] %04x\n", romnum, imagebase +
				    CARDBUS_EXROM_SIGNATURE, romsig);
				bus_release_resource(cbdev, SYS_RES_MEMORY,
				    *rid, res);
				*rid = 0;
				return (NULL);
			}

			/*
			 * If this was the Option ROM image that we were
			 * looking for, then we are done.
			 */
			if (romnum == imagenum)
				break;

			/* Find out where the next Option ROM image is */
			pcidata = imagebase + bus_space_read_2(bt, bh,
			    imagebase + CARDBUS_EXROM_DATA_PTR);
			imagesize = bus_space_read_2(bt, bh,
			    pcidata + CARDBUS_EXROM_DATA_IMAGE_LENGTH);

			if (imagesize == 0) {
				/*
				 * XXX some ROMs seem to have this as zero,
				 * can we assume this means 1 block?
				 */
				device_printf(cbdev, "Warning, size of Option "
				    "ROM image %d is 0 bytes, assuming 512 "
				    "bytes.\n", romnum);
				imagesize = 1;
			}

			/* Image size is in 512 byte units */
			imagesize <<= 9;

			if ((bus_space_read_1(bt, bh, pcidata +
			    CARDBUS_EXROM_DATA_INDICATOR) & 0x80) != 0) {
				device_printf(cbdev, "Cannot find CIS in "
				    "Option ROM\n");
				bus_release_resource(cbdev, SYS_RES_MEMORY,
				    *rid, res);
				*rid = 0;
				return (NULL);
			}
			imagebase += imagesize;
		}
		*start = imagebase + CARDBUS_CIS_ADDR(*start);
	} else {
		*start = CARDBUS_CIS_ADDR(*start);
	}

	return (res);
}

/*
 * Dispatch the right handler function per tuple
 */

static int
decode_tuple(device_t cbdev, device_t child, int tupleid, int len,
    uint8_t *tupledata, uint32_t start, uint32_t *off,
    struct tuple_callbacks *callbacks)
{
	int i;
	for (i = 0; callbacks[i].id != CISTPL_GENERIC; i++) {
		if (tupleid == callbacks[i].id)
			return (callbacks[i].func(cbdev, child, tupleid, len,
			    tupledata, start, off, &callbacks[i]));
	}
	return (callbacks[i].func(cbdev, child, tupleid, len,
	    tupledata, start, off, NULL));
}

static int
cardbus_parse_cis(device_t cbdev, device_t child,
    struct tuple_callbacks *callbacks)
{
	uint8_t tupledata[MAXTUPLESIZE];
	int tupleid;
	int len;
	int expect_linktarget;
	uint32_t start, off;
	struct resource *res;
	int rid;

	bzero(tupledata, MAXTUPLESIZE);
	expect_linktarget = TRUE;
	if ((start = pci_read_config(child, CARDBUS_CIS_REG, 4)) == 0)
		return (ENXIO);
	off = 0;
	res = cardbus_read_tuple_init(cbdev, child, &start, &rid);
	if (res == NULL)
		return (ENXIO);

	do {
		if (0 != cardbus_read_tuple(cbdev, child, res, start, &off,
		    &tupleid, &len, tupledata)) {
			device_printf(cbdev, "Failed to read CIS.\n");
			cardbus_read_tuple_finish(cbdev, child, rid, res);
			return (ENXIO);
		}

		if (expect_linktarget && tupleid != CISTPL_LINKTARGET) {
			device_printf(cbdev, "Expecting link target, got 0x%x\n",
			    tupleid);
			cardbus_read_tuple_finish(cbdev, child, rid, res);
			return (EINVAL);
		}
		expect_linktarget = decode_tuple(cbdev, child, tupleid, len,
		    tupledata, start, &off, callbacks);
		if (expect_linktarget != 0) {
			cardbus_read_tuple_finish(cbdev, child, rid, res);
			return (expect_linktarget);
		}
	} while (tupleid != CISTPL_END);
	cardbus_read_tuple_finish(cbdev, child, rid, res);
	return (0);
}

int
cardbus_do_cis(device_t cbdev, device_t child)
{
	int ret;
	struct tuple_callbacks init_callbacks[] = {
		MAKETUPLE(LONGLINK_CB,		unhandled),
		MAKETUPLE(INDIRECT,		unhandled),
		MAKETUPLE(LONGLINK_MFC,		unhandled),
		MAKETUPLE(BAR,			bar),
		MAKETUPLE(LONGLINK_A,		unhandled),
		MAKETUPLE(LONGLINK_C,		unhandled),
		MAKETUPLE(LINKTARGET,		linktarget),
		MAKETUPLE(VERS_1,		vers_1),
		MAKETUPLE(MANFID,		manfid),
		MAKETUPLE(FUNCID,		funcid),
		MAKETUPLE(FUNCE,		funce),
		MAKETUPLE(END,			end),
		MAKETUPLE(GENERIC,		generic),
	};

	ret = cardbus_parse_cis(cbdev, child, init_callbacks);
	if (ret < 0)
		return (ret);
	return 0;
}
