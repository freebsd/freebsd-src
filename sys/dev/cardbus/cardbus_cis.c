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

#define CARDBUS_DEBUG

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <sys/bus.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <pci/pcivar.h>

#include <dev/cardbus/cardbusreg.h>
#include <dev/cardbus/cardbus_cis.h>

#include "card_if.h"

#if defined CARDBUS_DEBUG
#define STATIC
#define DPRINTF(a) printf a
#define DEVPRINTF(x) device_printf x
#else
#define STATIC static
#define DPRINTF(a)
#define DEVPRINTF(x)
#endif

#if !defined(lint)
static const char rcsid[] =
  "$FreeBSD$";
#endif

struct tuple_callbacks;

static int cardbus_read_tuple_conf(device_t dev, device_t child,
				   u_int32_t *start, u_int32_t *off,
				   int *tupleid, int *len, u_int8_t *tupledata);
static int cardbus_read_tuple_exrom(device_t dev, struct resource *mem,
				    u_int32_t *start, u_int32_t *off,
				    int *tupleid, int *len, u_int8_t *tupledata);
static int cardbus_read_tuple_mem(device_t dev, device_t child, u_int32_t *start,
				  u_int32_t *off, int *tupleid, int *len,
				  u_int8_t *tupledata);
static int cardbus_read_tuple(device_t dev, device_t child, u_int32_t *start,
			      u_int32_t *off, int *tupleid, int *len,
			      u_int8_t *tupledata);
static int decode_tuple(device_t dev, device_t child, int tupleid, int len,
			u_int8_t *tupledata, u_int32_t *start, u_int32_t *off,
			struct tuple_callbacks *callbacks);
static int cardbus_parse_cis(device_t dev, device_t child,
			     struct tuple_callbacks *callbacks);

#define DECODE_PARAMS							\
		(device_t dev, device_t child, int id, int len,		\
		 u_int8_t *tupledata, u_int32_t *start, u_int32_t *off,	\
		 struct tuple_callbacks *info)
#define DECODE_PROTOTYPE(NAME) static int decode_tuple_ ## NAME DECODE_PARAMS
DECODE_PROTOTYPE(generic);
DECODE_PROTOTYPE(nothing);
DECODE_PROTOTYPE(copy);
DECODE_PROTOTYPE(bar);
DECODE_PROTOTYPE(linktarget);
DECODE_PROTOTYPE(vers_1);
DECODE_PROTOTYPE(manfid);
DECODE_PROTOTYPE(funcid);
DECODE_PROTOTYPE(funce);
DECODE_PROTOTYPE(end);
DECODE_PROTOTYPE(unhandled);

struct tuple_callbacks {
	int id;
	char* name;
	int (*func) DECODE_PARAMS;
};
#define MAKETUPLE(NAME,FUNC) { CISTPL_ ## NAME, #NAME, decode_tuple_ ## FUNC }

static char* funcnames[] = {
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

static struct cis_tupleinfo* cisread_buf;
static int ncisread_buf;

DECODE_PROTOTYPE(generic)
{
#ifdef CARDBUS_DEBUG
	int i;

	if (info)
		printf ("TUPLE: %s [%d]:", info->name, len);
	else
		printf ("TUPLE: Unknown(0x%02x) [%d]:", id, len);

	for (i = 0; i < len; i++) {
		if (i % 0x10 == 0 && len > 0x10)
			printf ("\n       0x%02x:", i);
		printf (" %02x", tupledata[i]);
	}
	printf ("\n");
#endif
	return 0;
}

DECODE_PROTOTYPE(nothing)
{
	return 0;
}

DECODE_PROTOTYPE(copy)
{
	struct cis_tupleinfo* tmpbuf;

	tmpbuf = malloc(sizeof(struct cis_tupleinfo)*(ncisread_buf+1),
			M_DEVBUF, M_WAITOK);
	if (ncisread_buf > 0) {
		memcpy(tmpbuf, cisread_buf,
		       sizeof(struct cis_tupleinfo)*ncisread_buf);
		free(cisread_buf, M_DEVBUF);
	}
	cisread_buf = tmpbuf;

	cisread_buf[ncisread_buf].id = id;
	cisread_buf[ncisread_buf].len = len;
	cisread_buf[ncisread_buf].data = malloc(len, M_DEVBUF, M_WAITOK);
	memcpy (cisread_buf[ncisread_buf].data, tupledata, len);
	ncisread_buf++;
	return 0;
}

DECODE_PROTOTYPE(linktarget)
{
#ifdef CARDBUS_DEBUG
	int i;

	printf ("TUPLE: %s [%d]:", info->name, len);

	for (i = 0; i < len; i++) {
		if (i % 0x10 == 0 && len > 0x10)
			printf ("\n       0x%02x:", i);
		printf (" %02x", tupledata[i]);
	}
	printf ("\n");
#endif
	if (len != 3 || tupledata[0] != 'C' || tupledata[1] != 'I' ||
	    tupledata[2] != 'S') {
		printf("Invalid data for CIS Link Target!\n");
		decode_tuple_generic(dev, child, id, len, tupledata,
				     start, off, info);
		return EINVAL;
	}
	return 0;
}

DECODE_PROTOTYPE(vers_1)
{
	int i;
	printf("Product version: %d.%d\n", tupledata[0], tupledata[1]);
	printf("Product name: ");
	for (i = 2; i < len; i++) {
		if (tupledata[i] == '\0')
			printf (" | ");
		else if (tupledata[i] == 0xff)
			break;
		else
			printf("%c", tupledata[i]);
	}
	printf("\n");
	return 0;
}

DECODE_PROTOTYPE(funcid)
{
	int i;
	int numnames = sizeof(funcnames)/sizeof(funcnames[0]);

	printf("Functions: ");
	for(i = 0; i < len; i++) {
		if (tupledata[i] < numnames)
			printf ("%s", funcnames[tupledata[i]]);
		else
			printf ("Unknown(%d)", tupledata[i]);
		if (i < len-1) printf(", ");
	}
	printf ("\n");
	return 0;
}

DECODE_PROTOTYPE(manfid)
{
	int i;
	printf ("Manufacturer ID: ");
	for (i = 0; i < len; i++)
		printf("%02x", tupledata[i]);
	printf("\n");
	return 0;
}

DECODE_PROTOTYPE(funce)
{
	int i;
	printf ("Function Extension: ");
	for (i = 0; i < len; i++)
		printf("%02x", tupledata[i]);
	printf("\n");
	return 0;
}

DECODE_PROTOTYPE(bar)
{
	if (len != 6) {
		printf ("*** ERROR *** BAR length not 6 (%d)\n", len);
		return EINVAL;
	} else {
		int type;
		int reg;
		u_int32_t bar;
		u_int32_t len;
		struct resource *res;

		reg = *(u_int16_t*)tupledata;
		len = *(u_int32_t*)(tupledata+2);
		if (reg & TPL_BAR_REG_AS) {
			type = SYS_RES_IOPORT;
		} else {
			type = SYS_RES_MEMORY;
		}
		bar = (reg & TPL_BAR_REG_ASI_MASK) - 1;
		if (bar < 0 || bar > 5 || (type == SYS_RES_IOPORT && bar == 5)) {
			device_printf(dev, "Invalid BAR number: %02x(%02x)\n",
				      reg, bar);
			return 0;
		}
		bar = CARDBUS_BASE0_REG + bar * 4;
		DEVPRINTF((dev, "Opening BAR: type=%s, bar=%02x, len=%04x\n",
			   (type==SYS_RES_MEMORY)?"MEM":"IO", bar, len));
		res = bus_generic_alloc_resource(child, child, type, &bar, 0,
			 ~0, len, rman_make_alignment_flags(len) | RF_ACTIVE);
		if (res == NULL) {
			device_printf(dev, "Cannot allocate BAR %02x\n", bar);
		}
	}
	return 0;
}

DECODE_PROTOTYPE(unhandled)
{
	printf ("TUPLE: %s [%d] is unhandled! Bailing...", info->name, len);
	return -1;
}

DECODE_PROTOTYPE(end)
{
	printf("CIS reading done\n");
	return 0;
}

static int
cardbus_read_tuple_conf(device_t dev, device_t child, u_int32_t *start,
			 u_int32_t *off, int *tupleid, int *len,
			 u_int8_t *tupledata)
{
	int i, j;
	u_int32_t e;
	u_int32_t loc;

	loc = CARDBUS_CIS_ADDR(*start) + *off;

	e = pci_read_config(child, loc - loc%4, 4);
	for (j = loc % 4; j>0; j--)
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
	*off += *len+2;
	return 0;
}


static int
cardbus_read_tuple_exrom(device_t dev, struct resource *mem, u_int32_t *start,
			 u_int32_t *off, int *tupleid, int *len,
			 u_int8_t *tupledata)
{
#define READROM(rom, type, offset)				       \
	(*((u_int ## type ##_t *)(((unsigned char*)rom) + offset)))

	int romnum = 0;
	unsigned char *data;
	u_int32_t imagesize;
	unsigned char *image;
	int imagenum;

	image = (unsigned char*)rman_get_virtual(mem);
	imagenum = CARDBUS_CIS_ASI_ROM_IMAGE(*start);
	do {
		if (READROM(image, 16, CARDBUS_EXROM_SIGNATURE) != 0xaa55) {
			device_printf (dev, "Bad header in rom %d: %04x\n",
				       romnum, *(u_int16_t*)(image +
				       CARDBUS_EXROM_SIGNATURE));
			return ENXIO;
		}
		data = image + READROM(image, 16, CARDBUS_EXROM_DATA_PTR);
		imagesize = READROM(data, 16, CARDBUS_EXROM_DATA_IMAGE_LENGTH);

		if (imagesize == 0) {
			/*
			 * XXX some ROMs seem to have this as zero,
			 * can we assume this means 1 block?
			 */
			imagesize = 1;
		}
		imagesize <<= 9;

		if (imagenum == romnum) {
			image += CARDBUS_CIS_ADDR(*start) + *off;
			*tupleid = image[0];
			*len = image[1];
			memcpy(tupledata, image+2, *len);
			*off += *len+2;
			return 0;
		}
		image += imagesize;
		romnum++;
	} while ((READROM(data, 8, CARDBUS_EXROM_DATA_INDICATOR) & 0x80) == 0);
	device_printf(dev, "Cannot read CIS: Not enough images of rom\n");
	return ENOENT;
#undef READROM
}

static int
cardbus_read_tuple_mem(device_t dev, device_t child, u_int32_t *start,
		       u_int32_t *off, int *tupleid, int *len,
		       u_int8_t *tupledata)
{
	struct resource *mem;
	bus_space_tag_t bt;
	bus_space_handle_t bh;
	int rid;
	int ret;

	if (CARDBUS_CIS_SPACE(*start) == CARDBUS_CIS_ASI_ROM) {
		rid = CARDBUS_ROM_REG;
	} else {
		rid = CARDBUS_BASE0_REG + (CARDBUS_CIS_SPACE(*start) - 1) * 4;
	}

	mem = bus_alloc_resource(child, SYS_RES_MEMORY, &rid, 0, ~0,
				 1, RF_ACTIVE);
	bt = rman_get_bustag(mem);
	bh = rman_get_bushandle(mem);
	if (mem == NULL) {
		device_printf(dev, "Failed to get memory for CIS reading\n");
		return ENOMEM;
	}

	if(CARDBUS_CIS_SPACE(*start) == CARDBUS_CIS_ASI_ROM) {
		ret = cardbus_read_tuple_exrom(dev, mem, start, off, tupleid,
					       len, tupledata);
	} else {
		*tupleid = bus_space_read_1(bt, bh,
		    CARDBUS_CIS_ADDR(*start) + *off);
		*len = bus_space_read_1(bt, bh,
		    CARDBUS_CIS_ADDR(*start) + *off + 1);
		bus_space_read_multi_1(rman_get_bustag(mem),
		    rman_get_bushandle(mem),
		    *off + CARDBUS_CIS_ADDR(*start), tupledata, *len);
		ret = 0;
		*off += *len+2;
	}
	bus_release_resource(child, SYS_RES_MEMORY, rid, mem);
	return ret;
}

static int
cardbus_read_tuple(device_t dev, device_t child, u_int32_t *start,
		   u_int32_t *off, int *tupleid, int *len,
		   u_int8_t *tupledata)
{
	switch(CARDBUS_CIS_SPACE(*start)) {
	case CARDBUS_CIS_ASI_TUPLE:
		return cardbus_read_tuple_conf(dev, child, start, off,
					       tupleid, len, tupledata);
	case CARDBUS_CIS_ASI_BAR0:
	case CARDBUS_CIS_ASI_BAR1:
	case CARDBUS_CIS_ASI_BAR2:
	case CARDBUS_CIS_ASI_BAR3:
	case CARDBUS_CIS_ASI_BAR4:
	case CARDBUS_CIS_ASI_BAR5:
	case CARDBUS_CIS_ASI_ROM:
		return cardbus_read_tuple_mem(dev, child, start, off,
					      tupleid, len, tupledata);
	default:
		device_printf(dev, "Unable to read CIS: Unknown space: %d\n",
			      CARDBUS_CIS_SPACE(*start));
		return EINVAL;
	}
}

static int
decode_tuple(device_t dev, device_t child, int tupleid, int len,
	     u_int8_t *tupledata, u_int32_t *start, u_int32_t *off,
	     struct tuple_callbacks *callbacks)
{
	int i;
	for (i = 0; callbacks[i].id != CISTPL_GENERIC; i++) {
		if (tupleid == callbacks[i].id)
			return callbacks[i].func(dev, child, tupleid, len,
						tupledata, start, off,
						&callbacks[i]);
	}

	if (tupleid < CISTPL_CUSTOMSTART) {
		device_printf(dev, "Undefined tuple encountered, CIS parsing terminated\n");
		return EINVAL;
	}
	return callbacks[i].func(dev, child, tupleid, len,
				 tupledata, start, off,
				 NULL);
}

static int
cardbus_parse_cis(device_t dev, device_t child,
		  struct tuple_callbacks *callbacks)
{
	u_int8_t tupledata[MAXTUPLESIZE];
	int tupleid;
	int len;
	int expect_linktarget;
	u_int32_t start, off;

	bzero(tupledata, MAXTUPLESIZE);
	expect_linktarget = TRUE;
	start = pci_read_config(child, CARDBUS_CIS_REG, 4);
	off = 0;
	do {
		cardbus_read_tuple(dev, child, &start, &off, &tupleid, &len,
				   tupledata);

		if (expect_linktarget && tupleid != CISTPL_LINKTARGET) {
			device_printf(dev, "Expecting link target, got 0x%x\n",
				      tupleid);
			return EINVAL;
		}
		expect_linktarget = decode_tuple(dev, child, tupleid, len,
						 tupledata, &start, &off,
						 callbacks);
		if (expect_linktarget != 0)
			return expect_linktarget;
	} while (tupleid != CISTPL_END);
	return 0;
}

int
cardbus_cis_read(device_t dev, device_t child, u_int8_t id,
		     struct cis_tupleinfo** buff, int* nret)
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
	ret = cardbus_parse_cis(dev, child, cisread_callbacks);

	*buff = cisread_buf;
	*nret = ncisread_buf;
	return ret;
}

void
cardbus_cis_free(device_t dev, struct cis_tupleinfo *buff, int* nret)
{
	int i;
	for (i = 0; i < *nret; i++)
		free(buff[i].data, M_DEVBUF);
	if (*nret > 0)
		free(buff, M_DEVBUF);
}

int
cardbus_do_cis(device_t dev, device_t child)
{
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
	return cardbus_parse_cis(dev, child, init_callbacks);
}
