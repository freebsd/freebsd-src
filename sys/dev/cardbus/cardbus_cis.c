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

#include <sys/bus.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <pci/pcivar.h>

#include <dev/cardbus/cardbusreg.h>
#include <dev/cardbus/cardbus_cis.h>

#include "pccbb_if.h"

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

struct tupleinfo;

static int decode_tuples(device_t dev, device_t child,
			 u_int8_t *tuples, int len);
static int cardbus_read_exrom_cis(device_t dev, struct resource *res,
				  int cis, u_int8_t* tuple, int len);
static int cardbus_read_tuples_conf(device_t dev, device_t child,
				    u_int32_t cis_ptr, u_int8_t *tuples,
				    int len);
static int cardbus_read_tuples_mem(device_t dev, device_t child, int space,
				   u_int32_t cis_ptr, u_int8_t *tuples,
				   int len);
static int cardbus_read_tuples(device_t dev, device_t child, u_int8_t *tuples,
			       int len);

#define DECODE_PROTOTYPE(NAME) static int decode_tuple_ ## NAME		\
		(device_t dev, device_t child, int id, int len, 	\
		 u_int8_t *buff, struct tupleinfo *info)
DECODE_PROTOTYPE(generic);
DECODE_PROTOTYPE(bar);
DECODE_PROTOTYPE(linktarget);
DECODE_PROTOTYPE(vers_1);
DECODE_PROTOTYPE(manfid);
DECODE_PROTOTYPE(funcid);
DECODE_PROTOTYPE(funce);
DECODE_PROTOTYPE(end);
DECODE_PROTOTYPE(unhandled);

static struct tupleinfo {
	u_int8_t id;
	char* name;
	int (*func)(device_t dev, device_t child, int id, int len,
		    u_int8_t *buff, struct tupleinfo *info);
} tupleinfo[] = {
#define MAKETUPLE(NAME,FUNC) { CISTPL_ ## NAME, #NAME, decode_tuple_ ## FUNC }
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
	MAKETUPLE(END,			 end),
#undef MAKETUPLE
};

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
		printf (" %02x", buff[i]);
	}
	printf ("\n");
#endif
	return 0;
}

DECODE_PROTOTYPE(linktarget)
{
	if (len != 3 || buff[0] != 'C' || buff[1] != 'I' || buff[2] != 'S') {
		printf("Invalid data for CIS Link Target!\n");
		decode_tuple_generic(dev, child, id, len, buff, info);
		return EINVAL;
	}
	return 0;
}

DECODE_PROTOTYPE(vers_1)
{
	int i;
	printf("Product version: %d.%d\n", buff[0], buff[1]);
	printf("Product name: ");
	for (i = 2; i < len; i++) {
		if (buff[i] == '\0')
			printf (" | ");
		else if (buff[i] == 0xff)
			break;
		else
			printf("%c", buff[i]);
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
		if (buff[i] < numnames)
			printf ("%s", funcnames[buff[i]]);
		else
			printf ("Unknown(%d)", buff[i]);
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
		printf("%02x", buff[i]);
	printf("\n");
	return 0;
}

DECODE_PROTOTYPE(funce)
{
	int i;
	printf ("Function Extension: ");
	for (i = 0; i < len; i++)
		printf("%02x", buff[i]);
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
		u_int32_t start, len;
		struct resource *res;

		reg = *(u_int16_t*)buff;
		len = *(u_int32_t*)(buff+2);
		if (reg & TPL_BAR_REG_AS) {
			type = SYS_RES_IOPORT;
		} else {
			type = SYS_RES_MEMORY;
		}
		bar = (reg & TPL_BAR_REG_ASI_MASK) - 1;
		if (bar < 0 || bar > 6) {
			device_printf(dev, "Invalid BAR number: %02x(%02x)\n",
				      reg, bar);
			return EINVAL;
		}
		bar = CARDBUS_BASE0_REG + bar * 4;
		DEVPRINTF((dev, "Opening BAR: type=%s, bar=%02x, len=%04x\n",
			   (type==SYS_RES_MEMORY)?"MEM":"IO", bar, len));
		res = bus_generic_alloc_resource(child, child, type, &reg, 0,
			 ~0, len, rman_make_alignment_flags(len) | RF_ACTIVE);
		if (res == NULL) {
			device_printf(dev, "Cannot allocate BAR %02x\n", reg);
		} else {
			start = rman_get_start(res);
			if (reg == CARDBUS_ROM_REG) start |= 1;
			pci_write_config(child, reg, start, 4);
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
	return -1;
}

static int decode_tuples(device_t dev, device_t child,
			 u_int8_t *tuples, int len)
{
	int ret = 0;
	if (CISTPL_LINKTARGET != *tuples) {
		device_printf(dev, "CIS does not start with link target\n");
		return EINVAL;
	}
	do {
		int i;
		int numtupleids = sizeof(tupleinfo)/sizeof(tupleinfo[0]);
		for (i = 0; i < numtupleids; i++) {
			if (tuples[0] == tupleinfo[i].id) {
				ret = tupleinfo[i].func(dev, child, tuples[0],
							tuples[1], tuples+2,
							&tupleinfo[i]);
				break;
			}
		}
		if (i == numtupleids)
			ret = decode_tuple_generic(dev, child, tuples[0],
						   tuples[1], tuples+2, NULL);

		len -= (tuples[1]+2);
		tuples += tuples[1]+2;
	} while (len > 0 && ret == 0);

	if (ret < 0) return 0;
	else if (ret != 0) return ret;
	else {
		device_printf(dev, "CIS too long or END not encountered!\n");
		return EFBIG;
	}
}

static int
cardbus_read_exrom_cis(device_t dev, struct resource *res, int cis,
		       u_int8_t* tuple, int len)
{
#define READROM(rom, type, offset)				       \
	(*((u_int ## type ##_t *)(((unsigned char*)rom) + offset)))

	u_int32_t addr = 0; /* offset of current rom image */
	int romnum = 0;
	unsigned char *data;
	u_int32_t imagesize;
	unsigned char *image;
	int imagenum;

	image = (unsigned char*)rman_get_virtual(res);
	imagenum = CARDBUS_CIS_ASI_ROM_IMAGE(cis);
	do {
		if (READROM(image, 16, CARDBUS_EXROM_SIGNATURE) != 0xaa55) {
			device_printf (dev, "Bad header in rom %d: %04x\n",
				       romnum, *(u_int16_t*)(image +
				       CARDBUS_EXROM_SIGNATURE));
			return ENXIO;
		}
		data = image + READROM(image, 16, CARDBUS_EXROM_DATA_PTR);
		imagesize = READROM(data, 16, CARDBUS_EXROM_DATA_IMAGE_LENGTH);

		if(imagesize == 0)
			/*
			 * XXX some ROMs seem to have this as zero,
			 * can we assume this means 1 block?
			 */
			imagesize = 1;
		imagesize <<= 9;

		if (imagenum == romnum) {
			romnum = -1;
			memcpy(tuple, image+CARDBUS_CIS_ADDR(cis), len);
			return 0;
		}

		addr += imagesize;
		romnum++;
	} while ((READROM(data, 8, CARDBUS_EXROM_DATA_INDICATOR) & 0x80) == 0);
	device_printf(dev, "Cannot read CIS: Not enough images of rom\n");
	return ENOENT;
#undef READROM
}

static int
cardbus_read_tuples_conf(device_t dev, device_t child, u_int32_t cis_ptr,
			 u_int8_t *tuples, int len)
{
	int i, j;

	DEVPRINTF((dev, "reading CIS data from configuration space\n"));
	for (i = cis_ptr, j = 0; i < len; i += 4) {
		u_int32_t e = pci_read_config(child, i, 4);
		tuples[j] = 0xff & e;
		e >>= 8;
		tuples[j + 1] = 0xff & e;
		e >>= 8;
		tuples[j + 2] = 0xff & e;
		e >>= 8;
		tuples[j + 3] = 0xff & e;
		j += 4;
	}
	return 0;
}

static int
cardbus_read_tuples_mem(device_t dev, device_t child, int space,
			u_int32_t cis_ptr, u_int8_t *tuples, int len)
{
	struct resource *mem;
	int rid;
	int ret;

	if(space == CARDBUS_CIS_ASI_ROM) {
		rid = CARDBUS_ROM_REG;
		DEVPRINTF((dev, "reading CIS data from ROM\n"));
	} else {
		rid = CARDBUS_BASE0_REG + (space - 1) * 4;
		DEVPRINTF((dev, "reading CIS data from BAR%d\n", space - 1));
	}
	mem = bus_alloc_resource(child, SYS_RES_MEMORY, &rid, 0, ~0,
				 1, RF_ACTIVE);
	if (mem == NULL) {
		device_printf(dev, "Failed to get memory for CIS reading\n");
		return ENOMEM;
	}

	if(space == CARDBUS_CIS_ASI_ROM) {
		int s;
		s = splhigh();
		ret = cardbus_read_exrom_cis(dev, mem, cis_ptr, tuples, len);
		splx(s);
	} else {
		/* XXX byte order? */
		memcpy(tuples, (unsigned char*)rman_get_virtual(mem)+cis_ptr,
		       len);
		ret = 0;
	}
	bus_release_resource(child, SYS_RES_MEMORY, rid, mem);
	return ret;
}

static int
cardbus_read_tuples(device_t dev, device_t child, u_int8_t *tuples, int len)
{
	u_int32_t cis_ptr = pci_read_config(child, CARDBUS_CIS_REG, 4);
	int cardbus_space = cis_ptr & CARDBUS_CIS_ASIMASK;
	int ret = 0;
	cis_ptr = cis_ptr & CARDBUS_CIS_ADDRMASK;

	switch(cardbus_space) {
	case CARDBUS_CIS_ASI_TUPLE:
		ret = cardbus_read_tuples_conf(dev, child, cis_ptr, tuples,
					       len);
		break;
	case CARDBUS_CIS_ASI_BAR0:
	case CARDBUS_CIS_ASI_BAR1:
	case CARDBUS_CIS_ASI_BAR2:
	case CARDBUS_CIS_ASI_BAR3:
	case CARDBUS_CIS_ASI_BAR4:
	case CARDBUS_CIS_ASI_BAR5:
	case CARDBUS_CIS_ASI_ROM:
		ret = cardbus_read_tuples_mem(dev, child, cardbus_space,
					      cis_ptr, tuples, len);
		break;
	default:
		device_printf(dev, "Unable to read CIS: Unknown space: %d\n",
			      cardbus_space);
		ret = EINVAL;
	}
	return ret;
}

int
cardbus_do_cis(device_t dev, device_t child)
{
	u_int8_t tupledata[MAXTUPLESIZE];
	int ret;

	bzero(tupledata, MAXTUPLESIZE);

	ret = cardbus_read_tuples(dev, child, tupledata, MAXTUPLESIZE);
	if (ret != 0) return ret;
	return decode_tuples(dev, child, tupledata, MAXTUPLESIZE);
}

