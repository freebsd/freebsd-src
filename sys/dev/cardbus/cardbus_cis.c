/*
 * Copyright (c) 2000,2001 Jonathan Chen.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

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

#include <sys/pciio.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <dev/cardbus/cardbusreg.h>
#include <dev/cardbus/cardbusvar.h>
#include <dev/cardbus/cardbus_cis.h>

#include "card_if.h"

extern int cardbus_cis_debug;

#define	DPRINTF(a) if (cardbus_cis_debug) printf a
#define	DEVPRINTF(x) if (cardbus_cis_debug) device_printf x

#define	DECODE_PARAMS							\
		(device_t cbdev, device_t child, int id, int len,	\
		 u_int8_t *tupledata, u_int32_t start, u_int32_t *off,	\
		 struct tuple_callbacks *info)

struct tuple_callbacks {
	int	id;
	char	*name;
	int	(*func) DECODE_PARAMS;
};

#define	DECODE_PROTOTYPE(NAME) static int decode_tuple_ ## NAME DECODE_PARAMS
DECODE_PROTOTYPE(generic);
DECODE_PROTOTYPE(nothing);
DECODE_PROTOTYPE(copy);
DECODE_PROTOTYPE(linktarget);
DECODE_PROTOTYPE(vers_1);
DECODE_PROTOTYPE(funcid);
DECODE_PROTOTYPE(manfid);
DECODE_PROTOTYPE(funce);
DECODE_PROTOTYPE(bar);
DECODE_PROTOTYPE(unhandled);
DECODE_PROTOTYPE(end);
static int	cardbus_read_tuple_conf(device_t cbdev, device_t child,
		    u_int32_t start, u_int32_t *off, int *tupleid, int *len,
		    u_int8_t *tupledata);
static int	cardbus_read_tuple_mem(device_t cbdev, struct resource *res,
		    u_int32_t start, u_int32_t *off, int *tupleid, int *len,
		    u_int8_t *tupledata);
static int	cardbus_read_tuple(device_t cbdev, device_t child,
		    struct resource *res, u_int32_t start, u_int32_t *off,
		    int *tupleid, int *len, u_int8_t *tupledata);
static void	cardbus_read_tuple_finish(device_t cbdev, device_t child,
		    int rid, struct resource *res);
static struct resource	*cardbus_read_tuple_init(device_t cbdev, device_t child,
		    u_int32_t *start, int *rid);
static int	decode_tuple(device_t cbdev, device_t child, int tupleid,
		    int len, u_int8_t *tupledata, u_int32_t start,
		    u_int32_t *off, struct tuple_callbacks *callbacks);
static int	cardbus_parse_cis(device_t cbdev, device_t child,
		    struct tuple_callbacks *callbacks);
static int	barsort(const void *a, const void *b);
static int	cardbus_alloc_resources(device_t cbdev, device_t child);
static void	cardbus_add_map(device_t cbdev, device_t child, int reg);
static void	cardbus_pickup_maps(device_t cbdev, device_t child);


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

struct cardbus_quirk {
	u_int32_t devid;	/* Vendor/device of the card */
	int	type;
#define	CARDBUS_QUIRK_MAP_REG	1 /* PCI map register in weird place */
	int	arg1;
	int	arg2;
};

struct cardbus_quirk cardbus_quirks[] = {
	{ 0 }
};

static struct cis_tupleinfo *cisread_buf;
static int ncisread_buf;

/*
 * Handler functions for various CIS tuples
 */

DECODE_PROTOTYPE(generic)
{
#ifdef CARDBUS_DEBUG
	int i;

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
#endif
	return (0);
}

DECODE_PROTOTYPE(nothing)
{
	return (0);
}

DECODE_PROTOTYPE(copy)
{
	struct cis_tupleinfo *tmpbuf;

	tmpbuf = malloc(sizeof(struct cis_tupleinfo) * (ncisread_buf+1),
	    M_DEVBUF, M_WAITOK);
	if (ncisread_buf > 0) {
		memcpy(tmpbuf, cisread_buf,
		    sizeof(struct cis_tupleinfo) * ncisread_buf);
		free(cisread_buf, M_DEVBUF);
	}
	cisread_buf = tmpbuf;

	cisread_buf[ncisread_buf].id = id;
	cisread_buf[ncisread_buf].len = len;
	cisread_buf[ncisread_buf].data = malloc(len, M_DEVBUF, M_WAITOK);
	memcpy(cisread_buf[ncisread_buf].data, tupledata, len);
	ncisread_buf++;
	return (0);
}

DECODE_PROTOTYPE(linktarget)
{
#ifdef CARDBUS_DEBUG
	int i;

	printf("TUPLE: %s [%d]:", info->name, len);

	for (i = 0; i < len; i++) {
		if (i % 0x10 == 0 && len > 0x10)
			printf("\n       0x%02x:", i);
		printf(" %02x", tupledata[i]);
	}
	printf("\n");
#endif
	if (len != 3 || tupledata[0] != 'C' || tupledata[1] != 'I' ||
	    tupledata[2] != 'S') {
		printf("Invalid data for CIS Link Target!\n");
		decode_tuple_generic(cbdev, child, id, len, tupledata,
		    start, off, info);
		return (EINVAL);
	}
	return (0);
}

DECODE_PROTOTYPE(vers_1)
{
	int i;
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
	return (0);
}

DECODE_PROTOTYPE(funcid)
{
	int i;
	int numnames = sizeof(funcnames) / sizeof(funcnames[0]);

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
	return (0);
}

DECODE_PROTOTYPE(manfid)
{
	int i;
	printf("Manufacturer ID: ");
	for (i = 0; i < len; i++)
		printf("%02x", tupledata[i]);
	printf("\n");
	return (0);
}

DECODE_PROTOTYPE(funce)
{
	int i;
	printf("Function Extension: ");
	for (i = 0; i < len; i++)
		printf("%02x", tupledata[i]);
	printf("\n");
	return (0);
}

DECODE_PROTOTYPE(bar)
{
	if (len != 6) {
		printf("*** ERROR *** BAR length not 6 (%d)\n", len);
		return (EINVAL);
	} else {
		struct cardbus_devinfo *dinfo = device_get_ivars(child);
		int type;
		int reg;
		u_int32_t bar;

		reg = *(u_int16_t*)tupledata;
		len = *(u_int32_t*)(tupledata + 2);
		if (reg & TPL_BAR_REG_AS) {
			type = SYS_RES_IOPORT;
		} else {
			type = SYS_RES_MEMORY;
		}
		bar = (reg & TPL_BAR_REG_ASI_MASK) - 1;
		if (bar < 0 || bar > 5 ||
		    (type == SYS_RES_IOPORT && bar == 5)) {
			device_printf(cbdev, "Invalid BAR number: %02x(%02x)\n",
			    reg, bar);
			return (0);
		}
		bar = CARDBUS_BASE0_REG + bar * 4;
		if (type == SYS_RES_MEMORY) {
			if (bar & TPL_BAR_REG_PREFETCHABLE)
				dinfo->mprefetchable |= BARBIT(bar);
			if (bar & TPL_BAR_REG_BELOW1MB)
				dinfo->mbelow1mb |= BARBIT(bar);
		} else if (type == SYS_RES_IOPORT) {
			if (bar & TPL_BAR_REG_BELOW1MB)
				dinfo->ibelow1mb |= BARBIT(bar);
		}
		DEVPRINTF((cbdev, "Opening BAR: type=%s, bar=%02x, "
		    "len=%04x%s%s\n",
		    (type==SYS_RES_MEMORY)?"MEM":"IO", bar, len,
		    (type==SYS_RES_MEMORY&&dinfo->mprefetchable&BARBIT(bar))?
		    " (Prefetchable)":"",
		    type==SYS_RES_MEMORY?
		    ((dinfo->mbelow1mb&BARBIT(bar))?" (Below 1Mb)":"")
		    :(dinfo->ibelow1mb&BARBIT(bar))?" (Below 1Mb)":""
		    ));

		resource_list_add(&dinfo->pci.resources, type, bar, 0UL, ~0UL, len);
	}
	return (0);
}

DECODE_PROTOTYPE(unhandled)
{
	printf("TUPLE: %s [%d] is unhandled! Bailing...", info->name, len);
	return (-1);
}

DECODE_PROTOTYPE(end)
{
	printf("CIS reading done\n");
	return (0);
}

/*
 * Functions to read the a tuple from the card
 */

static int
cardbus_read_tuple_conf(device_t cbdev, device_t child, u_int32_t start,
    u_int32_t *off, int *tupleid, int *len, u_int8_t *tupledata)
{
	int i, j;
	u_int32_t e;
	u_int32_t loc;

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
cardbus_read_tuple_mem(device_t cbdev, struct resource *res, u_int32_t start,
    u_int32_t *off, int *tupleid, int *len, u_int8_t *tupledata)
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
    u_int32_t start, u_int32_t *off, int *tupleid, int *len,
    u_int8_t *tupledata)
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
cardbus_read_tuple_init(device_t cbdev, device_t child, u_int32_t *start,
    int *rid)
{
	u_int32_t testval;
	u_int32_t size;
	struct resource *res;

	switch (CARDBUS_CIS_SPACE(*start)) {
	case CARDBUS_CIS_ASI_TUPLE:
		/* CIS in tuple space need no initialization */
		return ((struct resource*)~0UL);
	case CARDBUS_CIS_ASI_BAR0:
	case CARDBUS_CIS_ASI_BAR1:
	case CARDBUS_CIS_ASI_BAR2:
	case CARDBUS_CIS_ASI_BAR3:
	case CARDBUS_CIS_ASI_BAR4:
	case CARDBUS_CIS_ASI_BAR5:
		*rid = CARDBUS_BASE0_REG + (CARDBUS_CIS_SPACE(*start) - 1) * 4;
		pci_write_config(child, *rid, ~0UL, 4);
		break;
	case CARDBUS_CIS_ASI_ROM:
		*rid = CARDBUS_ROM_REG;
		pci_write_config(child, *rid, CARDBUS_ROM_ADDRMASK, 4);
		break;
	default:
		device_printf(cbdev, "Unable to read CIS: Unknown space: %d\n",
		    CARDBUS_CIS_SPACE(*start));
		return (NULL);
	}

	/* figure out how much space we need */
	testval = pci_read_config(child, *rid, 4);
	if (testval & 1) {
		device_printf(cbdev, "CIS Space is IO, expecting memory.\n");
		return (NULL);
	}
	size = CARDBUS_MAPREG_MEM_SIZE(testval);
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
		int imagenum;
		u_int32_t imagesize;
		int mystart = 0;
		int romnum = 0;
		int dataptr;

		bt = rman_get_bustag(res);
		bh = rman_get_bushandle(res);

		imagenum = CARDBUS_CIS_ASI_ROM_IMAGE(*start);
		for (romnum = 0;; romnum++) {
			if (bus_space_read_2(bt, bh,
			    mystart+CARDBUS_EXROM_SIGNATURE) != 0xaa55) {
				device_printf(cbdev, "Bad header in rom %d: "
				    "[%x] %04x\n", romnum, mystart + 
				    CARDBUS_EXROM_SIGNATURE,
				    bus_space_read_2(bt, bh,
				    mystart+CARDBUS_EXROM_SIGNATURE));
				bus_release_resource(cbdev, SYS_RES_MEMORY,
				    *rid, res);
				*rid = 0;
				return (NULL);
			}
			dataptr = mystart + bus_space_read_2(bt, bh,
			    mystart + CARDBUS_EXROM_DATA_PTR);
			imagesize = bus_space_read_2(bt, bh,
			    dataptr + CARDBUS_EXROM_DATA_IMAGE_LENGTH);

			if (imagesize == 0) {
				/*
				 * XXX some ROMs seem to have this as zero,
				 * can we assume this means 1 block?
				 */
				imagesize = 1;
			}
			imagesize <<= 9;

			if (romnum == imagenum)
				break;
			if ((bus_space_read_1(bt, bh, mystart + 
			    CARDBUS_EXROM_DATA_INDICATOR) & 0x80) == 0) {
				device_printf(cbdev, "Cannot read CIS: "
				    "Not enough images of rom\n");
				return (NULL);
			}
			mystart += imagesize;
		}
		*start = mystart + CARDBUS_CIS_ADDR(*start);
	} else {
		*start = CARDBUS_CIS_SPACE(*start);
	}
	return (res);
}

/*
 * Dispatch the right handler function per tuple
 */

static int
decode_tuple(device_t cbdev, device_t child, int tupleid, int len,
    u_int8_t *tupledata, u_int32_t start, u_int32_t *off,
    struct tuple_callbacks *callbacks)
{
	int i;
	for (i = 0; callbacks[i].id != CISTPL_GENERIC; i++) {
		if (tupleid == callbacks[i].id)
			return (callbacks[i].func(cbdev, child, tupleid, len,
			    tupledata, start, off, &callbacks[i]));
	}

	if (tupleid < CISTPL_CUSTOMSTART) {
		device_printf(cbdev, "Undefined tuple encountered, "
		    "CIS parsing terminated\n");
		return (EINVAL);
	}
	return (callbacks[i].func(cbdev, child, tupleid, len,
	    tupledata, start, off, NULL));
}

static int
cardbus_parse_cis(device_t cbdev, device_t child,
    struct tuple_callbacks *callbacks)
{
	u_int8_t tupledata[MAXTUPLESIZE];
	int tupleid;
	int len;
	int expect_linktarget;
	u_int32_t start, off;
	struct resource *res;
	int rid;

	bzero(tupledata, MAXTUPLESIZE);
	expect_linktarget = TRUE;
	start = pci_read_config(child, CARDBUS_CIS_REG, 4);
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

static int
barsort(const void *a, const void *b)
{
	return ((*(const struct resource_list_entry **)b)->count -
	    (*(const struct resource_list_entry **)a)->count);
}

static int
cardbus_alloc_resources(device_t cbdev, device_t child)
{
	struct cardbus_devinfo *dinfo = device_get_ivars(child);
	int count;
	struct resource_list_entry *rle;
	struct resource_list_entry **barlist;
	int tmp;
	u_int32_t mem_psize = 0, mem_nsize = 0, io_size = 0;
	struct resource *res;
	u_int32_t start,end;
	int rid, flags;

	count = 0;
	SLIST_FOREACH(rle, &dinfo->pci.resources, link) {
		count++;
	}
	if (count == 0)
		return (0);
	barlist = malloc(sizeof(struct resource_list_entry*) * count, M_DEVBUF,
	    M_WAITOK);
	count = 0;
	SLIST_FOREACH(rle, &dinfo->pci.resources, link) {
		barlist[count] = rle;
		if (rle->type == SYS_RES_IOPORT) {
			io_size += rle->count;
		} else if (rle->type == SYS_RES_MEMORY) {
			if (dinfo->mprefetchable & BARBIT(rle->rid))
				mem_psize += rle->count;
			else
				mem_nsize += rle->count;
		}
		count++;
	}

	/*
	 * We want to allocate the largest resource first, so that our
	 * allocated memory is packed.
	 */
	qsort(barlist, count, sizeof(struct resource_list_entry*), barsort);

	/* Allocate prefetchable memory */
	flags = 0;
	for (tmp = 0; tmp < count; tmp++) {
		if (barlist[tmp]->res == NULL &&
		    barlist[tmp]->type == SYS_RES_MEMORY &&
		    dinfo->mprefetchable & BARBIT(barlist[tmp]->rid)) {
			flags = rman_make_alignment_flags(barlist[tmp]->count);
			break;
		}
	}
	if (flags > 0) { /* If any prefetchable memory is requested... */
		/*
		 * First we allocate one big space for all resources of this
		 * type.  We do this because our parent, pccbb, needs to open
		 * a window to forward all addresses within the window, and
		 * it would be best if nobody else has resources allocated
		 * within the window.
		 * (XXX: Perhaps there might be a better way to do this?)
		 */
		rid = 0;
		res = bus_alloc_resource(cbdev, SYS_RES_MEMORY, &rid, 0,
		    (dinfo->mprefetchable & dinfo->mbelow1mb)?0xFFFFF:~0UL,
		    mem_psize, flags);
		start = rman_get_start(res);
		end = rman_get_end(res);
		DEVPRINTF((cbdev, "Prefetchable memory at %x-%x\n", start, end));
		/*
		 * Now that we know the region is free, release it and hand it
		 * out piece by piece.
		 */
		bus_release_resource(cbdev, SYS_RES_MEMORY, rid, res);
		for (tmp = 0; tmp < count; tmp++) {
			if (barlist[tmp]->res == NULL &&
			    barlist[tmp]->type == SYS_RES_MEMORY &&
			    dinfo->mprefetchable & BARBIT(barlist[tmp]->rid)) {
				barlist[tmp]->res = bus_alloc_resource(cbdev,
				    barlist[tmp]->type,
				    &barlist[tmp]->rid, start, end,
				    barlist[tmp]->count,
				    rman_make_alignment_flags(
				    barlist[tmp]->count));
				if (barlist[tmp]->res == NULL) {
					mem_nsize += barlist[tmp]->count;
					dinfo->mprefetchable &=
					    ~BARBIT(barlist[tmp]->rid);
					DEVPRINTF((cbdev, "Cannot pre-allocate "
					    "prefetchable memory, will try as "
					    "non-prefetchable.\n"));
				} else {
					barlist[tmp]->start =
					    rman_get_start(barlist[tmp]->res);
					barlist[tmp]->end =
					    rman_get_end(barlist[tmp]->res);
					pci_write_config(child,
					    barlist[tmp]->rid,
					    barlist[tmp]->start, 4);
					DEVPRINTF((cbdev, "Prefetchable memory "
					    "rid=%x at %lx-%lx\n",
					    barlist[tmp]->rid,
					    barlist[tmp]->start,
					    barlist[tmp]->end));
				}
			}
		}
	}

	/* Allocate non-prefetchable memory */
	flags = 0;
	for (tmp = 0; tmp < count; tmp++) {
		if (barlist[tmp]->res == NULL &&
		    barlist[tmp]->type == SYS_RES_MEMORY) {
			flags = rman_make_alignment_flags(barlist[tmp]->count);
			break;
		}
	}
	if (flags > 0) { /* If any non-prefetchable memory is requested... */
		/*
		 * First we allocate one big space for all resources of this
		 * type.  We do this because our parent, pccbb, needs to open
		 * a window to forward all addresses within the window, and
		 * it would be best if nobody else has resources allocated
		 * within the window.
		 * (XXX: Perhaps there might be a better way to do this?)
		 */
		rid = 0;
		res = bus_alloc_resource(cbdev, SYS_RES_MEMORY, &rid, 0,
		    ((~dinfo->mprefetchable) & dinfo->mbelow1mb)?0xFFFFF:~0UL,
		    mem_nsize, flags);
		start = rman_get_start(res);
		end = rman_get_end(res);
		DEVPRINTF((cbdev, "Non-prefetchable memory at %x-%x\n",
		    start, end));
		/*
		 * Now that we know the region is free, release it and hand it
		 * out piece by piece.
		 */
		bus_release_resource(cbdev, SYS_RES_MEMORY, rid, res);
		for (tmp = 0; tmp < count; tmp++) {
			if (barlist[tmp]->res == NULL &&
			    barlist[tmp]->type == SYS_RES_MEMORY) {
				barlist[tmp]->res = bus_alloc_resource(cbdev,
				    barlist[tmp]->type, &barlist[tmp]->rid,
				    start, end, barlist[tmp]->count,
				    rman_make_alignment_flags(
				    barlist[tmp]->count));
				if (barlist[tmp]->res == NULL) {
					DEVPRINTF((cbdev, "Cannot pre-allocate "
					    "memory for cardbus device\n"));
					return (ENOMEM);
				}
				barlist[tmp]->start =
				    rman_get_start(barlist[tmp]->res);
				barlist[tmp]->end = rman_get_end(
					barlist[tmp]->res);
				pci_write_config(child, barlist[tmp]->rid,
				    barlist[tmp]->start, 4);
				DEVPRINTF((cbdev, "Non-prefetchable memory "
				    "rid=%x at %lx-%lx (%lx)\n",
				    barlist[tmp]->rid, barlist[tmp]->start,
				    barlist[tmp]->end, barlist[tmp]->count));
			}
		}
	}

	/* Allocate IO ports */
	flags = 0;
	for (tmp = 0; tmp < count; tmp++) {
		if (barlist[tmp]->res == NULL &&
		    barlist[tmp]->type == SYS_RES_IOPORT) {
			flags = rman_make_alignment_flags(barlist[tmp]->count);
			break;
		}
	}
	if (flags > 0) { /* If any IO port is requested... */
		/*
		 * First we allocate one big space for all resources of this
		 * type.  We do this because our parent, pccbb, needs to open
		 * a window to forward all addresses within the window, and
		 * it would be best if nobody else has resources allocated
		 * within the window.
		 * (XXX: Perhaps there might be a better way to do this?)
		 */
		rid = 0;
		res = bus_alloc_resource(cbdev, SYS_RES_IOPORT, &rid, 0,
		    (dinfo->ibelow1mb)?0xFFFFF:~0UL, io_size, flags);
		start = rman_get_start(res);
		end = rman_get_end(res);
		DEVPRINTF((cbdev, "IO port at %x-%x\n", start, end));
		/*
		 * Now that we know the region is free, release it and hand it
		 * out piece by piece.
		 */
		bus_release_resource(cbdev, SYS_RES_IOPORT, rid, res);
		for (tmp = 0; tmp < count; tmp++) {
			if (barlist[tmp]->res == NULL &&
			    barlist[tmp]->type == SYS_RES_IOPORT) {
				barlist[tmp]->res = bus_alloc_resource(cbdev,
				    barlist[tmp]->type, &barlist[tmp]->rid,
				    start, end, barlist[tmp]->count,
				    rman_make_alignment_flags(
				    barlist[tmp]->count));
				if (barlist[tmp]->res == NULL) {
					DEVPRINTF((cbdev, "Cannot pre-allocate "
					    "IO port for cardbus device\n"));
					return (ENOMEM);
				}
				barlist[tmp]->start =
				    rman_get_start(barlist[tmp]->res);
				barlist[tmp]->end =
				    rman_get_end(barlist[tmp]->res);
			pci_write_config(child, barlist[tmp]->rid,
			    barlist[tmp]->start, 4);
			DEVPRINTF((cbdev, "IO port rid=%x at %lx-%lx\n",
			    barlist[tmp]->rid, barlist[tmp]->start,
			    barlist[tmp]->end));
			}
		}
	}

	/* Allocate IRQ */
	/* XXX: Search CIS for IRQ description */
	rid = 0;
	res = bus_alloc_resource(cbdev, SYS_RES_IRQ, &rid, 0, ~0UL, 1,
	    RF_SHAREABLE);
	resource_list_add(&dinfo->pci.resources, SYS_RES_IRQ, rid,
	    rman_get_start(res), rman_get_end(res), 1);
	rle = resource_list_find(&dinfo->pci.resources, SYS_RES_IRQ, rid);
	rle->res = res;
	dinfo->pci.cfg.intline = rman_get_start(res);
	pci_write_config(child, PCIR_INTLINE, rman_get_start(res), 1);

	return (0);
}

/*
 * Adding a memory/io resource (sans CIS)
 */

static void
cardbus_add_map(device_t cbdev, device_t child, int reg)
{
	struct cardbus_devinfo *dinfo = device_get_ivars(child);
	struct resource_list_entry *rle;
	u_int32_t size;
	u_int32_t testval;
	int type;

	SLIST_FOREACH(rle, &dinfo->pci.resources, link) {
		if (rle->rid == reg)
			return;
	}

	if (reg == CARDBUS_ROM_REG)
		testval = CARDBUS_ROM_ADDRMASK;
	else
		testval = ~0;

	pci_write_config(child, reg, testval, 4);
	testval = pci_read_config(child, reg, 4);

	if (testval == ~0 || testval == 0)
		return;

	if ((testval & 1) == 0)
		type = SYS_RES_MEMORY;
	else
		type = SYS_RES_IOPORT;

	size = CARDBUS_MAPREG_MEM_SIZE(testval);
	device_printf(cbdev, "Resource not specified in CIS: id=%x, size=%x\n",
	    reg, size);
	resource_list_add(&dinfo->pci.resources, type, reg, 0UL, ~0UL, size);
}

static void
cardbus_pickup_maps(device_t cbdev, device_t child)
{
	struct cardbus_devinfo *dinfo = device_get_ivars(child);
	struct cardbus_quirk *q;
	int reg;

	/*
	 * Try to pick up any resources that was not specified in CIS.
	 * Some devices (eg, 3c656) does not list all resources required by
	 * the driver in its CIS.
	 * XXX: should we do this or use quirks?
	 */
	for (reg = 0; reg < dinfo->pci.cfg.nummaps; reg++) {
		cardbus_add_map(cbdev, child, PCIR_MAPS + reg * 4);
	}

	for (q = &cardbus_quirks[0]; q->devid; q++) {
		if (q->devid == ((dinfo->pci.cfg.device << 16) | dinfo->pci.cfg.vendor)
		    && q->type == CARDBUS_QUIRK_MAP_REG) {
			cardbus_add_map(cbdev, child, q->arg1);
		}
	}
}

int
cardbus_cis_read(device_t cbdev, device_t child, u_int8_t id,
    struct cis_tupleinfo **buff, int *nret)
{
	struct tuple_callbacks cisread_callbacks[] = {
		MAKETUPLE(NULL,			nothing),
		/* first entry will be overwritten */
		MAKETUPLE(NULL,			nothing),
		MAKETUPLE(DEVICE,		nothing),
		MAKETUPLE(LONG_LINK_CB,		unhandled),
		MAKETUPLE(INDIRECT,		unhandled),
		MAKETUPLE(CONFIG_CB,		nothing),
		MAKETUPLE(CFTABLE_ENTRY_CB,	nothing),
		MAKETUPLE(LONGLINK_MFC,		unhandled),
		MAKETUPLE(BAR,			nothing),
		MAKETUPLE(PWR_MGMNT,		nothing),
		MAKETUPLE(EXTDEVICE,		nothing),
		MAKETUPLE(CHECKSUM,		nothing),
		MAKETUPLE(LONGLINK_A,		unhandled),
		MAKETUPLE(LONGLINK_C,		unhandled),
		MAKETUPLE(LINKTARGET,		nothing),
		MAKETUPLE(NO_LINK,		nothing),
		MAKETUPLE(VERS_1,		nothing),
		MAKETUPLE(ALTSTR,		nothing),
		MAKETUPLE(DEVICE_A,		nothing),
		MAKETUPLE(JEDEC_C,		nothing),
		MAKETUPLE(JEDEC_A,		nothing),
		MAKETUPLE(CONFIG,		nothing),
		MAKETUPLE(CFTABLE_ENTRY,	nothing),
		MAKETUPLE(DEVICE_OC,		nothing),
		MAKETUPLE(DEVICE_OA,		nothing),
		MAKETUPLE(DEVICE_GEO,		nothing),
		MAKETUPLE(DEVICE_GEO_A,		nothing),
		MAKETUPLE(MANFID,		nothing),
		MAKETUPLE(FUNCID,		nothing),
		MAKETUPLE(FUNCE,		nothing),
		MAKETUPLE(SWIL,			nothing),
		MAKETUPLE(VERS_2,		nothing),
		MAKETUPLE(FORMAT,		nothing),
		MAKETUPLE(GEOMETRY,		nothing),
		MAKETUPLE(BYTEORDER,		nothing),
		MAKETUPLE(DATE,			nothing),
		MAKETUPLE(BATTERY,		nothing),
		MAKETUPLE(ORG,			nothing),
		MAKETUPLE(END,			end),
		MAKETUPLE(GENERIC,		nothing),
	};
	int ret;

	cisread_callbacks[0].id = id;
	cisread_callbacks[0].name = "COPY";
	cisread_callbacks[0].func = decode_tuple_copy;
	ncisread_buf = 0;
	cisread_buf = NULL;
	ret = cardbus_parse_cis(cbdev, child, cisread_callbacks);

	*buff = cisread_buf;
	*nret = ncisread_buf;
	return (ret);
}

void
cardbus_cis_free(device_t cbdev, struct cis_tupleinfo *buff, int *nret)
{
	int i;
	for (i = 0; i < *nret; i++)
		free(buff[i].data, M_DEVBUF);
	if (*nret > 0)
		free(buff, M_DEVBUF);
}

int
cardbus_do_cis(device_t cbdev, device_t child)
{
	int ret;
	struct tuple_callbacks init_callbacks[] = {
		MAKETUPLE(NULL,			generic),
		MAKETUPLE(DEVICE,		generic),
		MAKETUPLE(LONG_LINK_CB,		unhandled),
		MAKETUPLE(INDIRECT,		unhandled),
		MAKETUPLE(CONFIG_CB,		generic),
		MAKETUPLE(CFTABLE_ENTRY_CB,	generic),
		MAKETUPLE(LONGLINK_MFC,		unhandled),
		MAKETUPLE(BAR,			bar),
		MAKETUPLE(PWR_MGMNT,		generic),
		MAKETUPLE(EXTDEVICE,		generic),
		MAKETUPLE(CHECKSUM,		generic),
		MAKETUPLE(LONGLINK_A,		unhandled),
		MAKETUPLE(LONGLINK_C,		unhandled),
		MAKETUPLE(LINKTARGET,		linktarget),
		MAKETUPLE(NO_LINK,		generic),
		MAKETUPLE(VERS_1,		vers_1),
		MAKETUPLE(ALTSTR,		generic),
		MAKETUPLE(DEVICE_A,		generic),
		MAKETUPLE(JEDEC_C,		generic),
		MAKETUPLE(JEDEC_A,		generic),
		MAKETUPLE(CONFIG,		generic),
		MAKETUPLE(CFTABLE_ENTRY,	generic),
		MAKETUPLE(DEVICE_OC,		generic),
		MAKETUPLE(DEVICE_OA,		generic),
		MAKETUPLE(DEVICE_GEO,		generic),
		MAKETUPLE(DEVICE_GEO_A,		generic),
		MAKETUPLE(MANFID,		manfid),
		MAKETUPLE(FUNCID,		funcid),
		MAKETUPLE(FUNCE,		funce),
		MAKETUPLE(SWIL,			generic),
		MAKETUPLE(VERS_2,		generic),
		MAKETUPLE(FORMAT,		generic),
		MAKETUPLE(GEOMETRY,		generic),
		MAKETUPLE(BYTEORDER,		generic),
		MAKETUPLE(DATE,			generic),
		MAKETUPLE(BATTERY,		generic),
		MAKETUPLE(ORG,			generic),
		MAKETUPLE(END,			end),
		MAKETUPLE(GENERIC,		generic),
	};

	ret = cardbus_parse_cis(cbdev, child, init_callbacks);
	if (ret < 0)
		return (ret);
	cardbus_pickup_maps(cbdev, child);
	return (cardbus_alloc_resources(cbdev, child));
}
