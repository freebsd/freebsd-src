/*
 * Copyright (c) 1995 Andrew McRae.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

/*
 * Code cleanup, bug-fix and extension
 * by Tatsumi Hosokawa <hosokawa@mt.cs.keio.ac.jp>
 */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <pccard/cardinfo.h>
#include <pccard/cis.h>

#include "readcis.h"

#ifdef RATOCLAN
static int	rex5588 = 0;
#endif

static int read_attr(int, char *, int);
static int ck_linktarget(int, off_t, int);
static void cis_info(struct cis *, unsigned char *, int);
static void device_desc(unsigned char *, int, struct dev_mem *);
static void config_map(struct cis *, unsigned char *, int);
static void cis_config(struct cis *, unsigned char *, int);
static void cis_manuf_id(struct cis *, unsigned char *, int);
static void cis_func_id(struct cis *, unsigned char *, int);
static void cis_network_ext(struct cis *, unsigned char *, int);
static struct tuple_list *read_one_tuplelist(int, int, off_t);
static struct tuple_list *read_tuples(int);
static struct tuple *find_tuple_in_list(struct tuple_list *, unsigned char);
static struct tuple_info *get_tuple_info(unsigned char);

static struct tuple_info tuple_info[] = {
	{"Null tuple", 0x00, 0},
	{"Common memory descriptor", 0x01, 255},
	{"Long link to next chain for CardBus", 0x02, 255},
	{"Indirect access", 0x03, 255},
	{"Configuration map for CardBus", 0x04, 255},
	{"Configuration entry for CardBus", 0x05, 255},
	{"Long link to next chain for MFC", 0x06, 255},
	{"Base address register for CardBus", 0x07, 6},
	{"Checksum", 0x10, 5},
	{"Long link to attribute memory", 0x11, 4},
	{"Long link to common memory", 0x12, 4},
	{"Link target", 0x13, 3},
	{"No link", 0x14, 0},
	{"Version 1 info", 0x15, 255},
	{"Alternate language string", 0x16, 255},
	{"Attribute memory descriptor", 0x17, 255},
	{"JEDEC descr for common memory", 0x18, 255},
	{"JEDEC descr for attribute memory", 0x19, 255},
	{"Configuration map", 0x1A, 255},
	{"Configuration entry", 0x1B, 255},
	{"Other conditions for common memory", 0x1C, 255},
	{"Other conditions for attribute memory", 0x1D, 255},
	{"Geometry info for common memory", 0x1E, 255},
	{"Geometry info for attribute memory", 0x1F, 255},
	{"Manufacturer ID", 0x20, 4},
	{"Functional ID", 0x21, 2},
	{"Functional EXT", 0x22, 255},
	{"Software interleave", 0x23, 2},
	{"Version 2 Info", 0x40, 255},
	{"Data format", 0x41, 255},
	{"Geometry", 0x42, 4},
	{"Byte order", 0x43, 2},
	{"Card init date", 0x44, 4},
	{"Battery replacement", 0x45, 4},
	{"Organization", 0x46, 255},
	{"Terminator", 0xFF, 0},
	{0, 0, 0}
};

/*
 *	After reading the tuples, decode the relevant ones.
 */
struct cis *
readcis(int fd)
{
	struct tuple_list *tl;
	struct tuple *tp;
	struct cis *cp;

	cp = xmalloc(sizeof(*cp));
	cp->tlist = read_tuples(fd);
	if (cp->tlist == 0)
		return (NULL);

	for (tl = cp->tlist; tl; tl = tl->next)
		for (tp = tl->tuples; tp; tp = tp->next) {
#if 0
			printf("tuple code = 0x%02x, data is\n", tp->code);
			dump(tp->data, tp->length);
#endif
			switch (tp->code) {
			case CIS_MEM_COMMON:	/* 0x01 */
				device_desc(tp->data, tp->length, &cp->common_mem);
				break;
			case CIS_INFO_V1:	/* 0x15 */
				cis_info(cp, tp->data, tp->length);
				break;
			case CIS_MEM_ATTR:	/* 0x17 */
				device_desc(tp->data, tp->length, &cp->attr_mem);
				break;
			case CIS_CONF_MAP:	/* 0x1A */
				config_map(cp, tp->data, tp->length);
				break;
			case CIS_CONFIG:	/* 0x1B */
				cis_config(cp, tp->data, tp->length);
				break;
			case CIS_MANUF_ID:	/* 0x20 */
				cis_manuf_id(cp, tp->data, tp->length);
				break;
			case CIS_FUNC_ID:	/* 0x21 */
				cis_func_id(cp, tp->data, tp->length);
				break;
			case CIS_FUNC_EXT:	/* 0x22 */
				if (cp->func_id1 == 6)	/* LAN adaptor */
					cis_network_ext(cp, tp->data, tp->length);
				break;
			}
		}
	return (cp);
}

/*
 *	free_cis - delete cis entry.
 */
void
freecis(struct cis *cp)
{
	struct cis_ioblk *io;
	struct cis_memblk *mem;
	struct cis_config *conf;
	struct tuple *tp;
	struct tuple_list *tl;

	while ((tl = cp->tlist) != 0) {
		cp->tlist = tl->next;
		while ((tp = tl->tuples) != 0) {
			tl->tuples = tp->next;
			if (tp->data)
				free(tp->data);
		}
	}

	while ((conf = cp->conf) != 0) {
		cp->conf = conf->next;
		while ((io = conf->io) != 0) {
			conf->io = io->next;
			free(io);
		}
		while ((mem = conf->mem) != 0) {
			conf->mem = mem->next;
			free(mem);
		}
		free(conf);
	}
	free(cp);
}

/*
 *	Fills in CIS version data.
 */
static void
cis_info(struct cis *cp, unsigned char *p, int len)
{
	cp->maj_v = *p++;
	cp->min_v = *p++;
	len -= 2;
	if (cp->manuf) {
		free(cp->manuf);
		cp->manuf = NULL;
	}
	if (len > 1 && *p != 0xff) {
		cp->manuf = strdup(p);
		len -= strlen(p) + 1;
		p += strlen(p) + 1;
	}
	if (cp->vers) {
		free(cp->vers);
		cp->vers = NULL;
	}
	if (len > 1 && *p != 0xff) {
		cp->vers = strdup(p);
		len -= strlen(p) + 1;
		p += strlen(p) + 1;
	} else {
		cp->vers = strdup("[none]");
	}
	if (cp->add_info1) {
		free(cp->add_info1);
		cp->add_info1 = NULL;
	}
	if (len > 1 && *p != 0xff) {
		cp->add_info1 = strdup(p);
		len -= strlen(p) + 1;
		p += strlen(p) + 1;
	} else {
		cp->add_info1 = strdup("[none]");
	}
	if (cp->add_info2) {
		free(cp->add_info2);
		cp->add_info2 = NULL;
	}
	if (len > 1 && *p != 0xff)
		cp->add_info2 = strdup(p);
	else
		cp->add_info2 = strdup("[none]");
}

static void
cis_manuf_id(struct cis *cp, unsigned char *p, int len)
{
	if (len > 4) {
		cp->manufacturer = tpl16(p);
		cp->product = tpl16(p+2);
		if (len == 5)
			cp->prodext = *(p+4); /* For xe driver */
	} else {
		cp->manufacturer=0;
		cp->product=0;
		cp->prodext=0;
	}
}
/*
 *	Fills in CIS function ID.
 */
static void
cis_func_id(struct cis *cp, unsigned char *p, int len)
{
	cp->func_id1 = *p++;
	cp->func_id2 = *p++;
}

static void
cis_network_ext(struct cis *cp, unsigned char *p, int len)
{
	int i;

	switch (p[0]) {
	case 4:		/* Node ID */
		if (len <= 2 || len < p[1] + 2)
			return;

		if (cp->lan_nid)
			free(cp->lan_nid);
		cp->lan_nid = xmalloc(p[1]);

		for (i = 0; i <= p[1]; i++)
			cp->lan_nid[i] = p[i + 1];
		break;
	}
}

/*
 *	"FUJITSU LAN Card (FMV-J182)" has broken CIS
 */
static int
fmvj182_check(unsigned char *p)
{
	char    manuf[BUFSIZ], vers[BUFSIZ];

	p++;			/* major version */
	p++;			/* minor version */
	strncpy(manuf, p, sizeof(manuf) - 1);
	while (*p++);
	strncpy(vers, p, sizeof(vers) - 1);
	if (!strcmp(manuf, "FUJITSU") && !strcmp(vers, "LAN Card(FMV-J182)"))
		return 1;
	else
		return 0;
}

#ifdef RATOCLAN
/*
 *	"RATOC LAN Card (REX-5588)" has broken CIS
 */
static int
rex5588_check(unsigned char *p)
{
	char    manuf[BUFSIZ], vers[BUFSIZ];

	p++;			/* major version */
	p++;			/* minor version */
	strncpy(manuf, p, sizeof(manuf) - 1);
	while (*p++);
	strncpy(vers, p, sizeof(manuf) - 1);
	if (!strcmp(manuf, "PCMCIA LAN MBH10304  ES"))
		return 1;
	else
		return 0;
}
#endif

#ifdef HSSYNTH
/*
 *	Broken CIS for "HITACHI MICROCOMPUTER SYSTEM LTD." "MSSHVPC02"
 */
static int
hss_check(unsigned char *p)
{
	char    manuf[BUFSIZ], vers[BUFSIZ];

	p++;			/* major version */
	p++;			/* minor version */
	strncpy(manuf, p, sizeof(manuf) - 1);
	while (*p++);
	strncpy(vers, p, sizeof(vers) - 1);
	if (!strcmp(manuf, "HITACHI MICROCOMPUTER SYSTEMS LTD.")
	 && !strcmp(vers, "MSSHVPC02"))
		return 1;
	else
		return 0;
}
#endif	/* HSSYNTH */

/*
 *	device_desc - decode device descriptor.
 */
static void
device_desc(unsigned char *p, int len, struct dev_mem *dp)
{
	while (len > 0 && *p != 0xFF) {
		dp->valid = 1;
		dp->type = (*p & 0xF0) >> 4;
		dp->wps = !!(*p & 0x8);
		dp->speed = *p & 7;
		p++;
		if (*p != 0xFF) {
			dp->addr = (*p >> 3) & 0xF;
			dp->units = *p & 7;
		}
		p++;
		len -= 2;
	}
}

/*
 *	configuration map of card control register.
 */
static void
config_map(struct cis *cp, unsigned char *p, int len)
{
	unsigned char *p1;
	int rlen = (*p & 3) + 1;

	p1 = p + 1;
	cp->last_config = *p1++ & 0x3F;
	cp->reg_addr = parse_num(rlen | 0x10, p1, &p1, 0);
	cp->ccrs = *p1;
}

/*
 *	Parse variable length value.
 */
u_int
parse_num(int sz, u_char *p, u_char **q, int ofs)
{
	u_int num = 0;

	switch (sz) {	
	case 0:
	case 0x10:
		break;
	case 1:
	case 0x11:
		num = (*p++) + ofs;
		break;
	case 2:
	case 0x12:
		num = tpl16(p) + ofs;
		p += 2;
		break;
	case 0x13:
		num = tpl24(p) + ofs;
		p += 3;
		break;
	case 3:
	case 0x14:
		num = tpl32(p) + ofs;
		p += 4;
		break;
	}
	if (q)
		*q = p;
	return num;
}

/*
 *	CIS config entry - Decode and build configuration entry.
 */
static void
cis_config(struct cis *cp, unsigned char *p, int len)
{
	int     x;
	int     i, j;
	struct cis_config *conf, *last;
	unsigned char feat;

	conf = xmalloc(sizeof(*conf));
	if ((last = cp->conf) != 0) {
		while (last->next)
			last = last->next;
		last->next = conf;
	} else
		cp->conf = conf;
 	conf->id = *p & 0x3F;	/* Config index */
#ifdef RATOCLAN
	if (rex5588 && conf->id >= 0x08 && conf->id <= 0x1d)
		conf->id |= 0x20;
#endif
 	if (*p & 0x40)		/* Default flag */
		cp->def_config = conf;
	if (*p++ & 0x80)
 		p++;		/* Interface byte skip */
 	feat = *p++;		/* Features byte */
	for (i = 0; i < CIS_FEAT_POWER(feat); i++) {
		unsigned char parms = *p++;

		conf->pwr = 1;
		for (j = 0; j < 8; j++)
			if (parms & (1 << j))
				while (*p++ & 0x80);
	}
	if (feat & CIS_FEAT_TIMING) {
		conf->timing = 1;
		i = *p++;
		if (CIS_WAIT_SCALE(i) != 3)
			p++;
		if (CIS_READY_SCALE(i) != 7)
			p++;
		if (CIS_RESERVED_SCALE(i) != 7)
			p++;
	}
	if (feat & CIS_FEAT_I_O) {
		conf->iospace = 1;
		if (CIS_IO_RANGE & *p)
			conf->io_blks = CIS_IO_BLKS(p[1]) + 1;
		conf->io_addr = CIS_IO_ADDR(*p);
		conf->io_bus = (*p >> 5) & 3; /* CIS_IO_8BIT | CIS_IO_16BIT */
		if (*p++ & CIS_IO_RANGE) {
			struct cis_ioblk *io;
			struct cis_ioblk *last_io = NULL;

			i = CIS_IO_ADSZ(*p);
			j = CIS_IO_BLKSZ(*p++);
			for (x = 0; x < conf->io_blks; x++) {
				io = xmalloc(sizeof(*io));
				if (last_io)
					last_io->next = io;
				else
					conf->io = io;
				last_io = io;
				io->addr = parse_num(i, p, &p, 0);
				io->size = parse_num(j, p, &p, 1);
			}
		}
	}
	if (feat & CIS_FEAT_IRQ) {
		conf->irq = 1;
		conf->irqlevel = *p & 0xF;
		conf->irq_flags = *p & 0xF0;
		if (*p++ & CIS_IRQ_MASK) {
			conf->irq_mask = tpl16(p);
			p += 2;
		}
	}
	switch (CIS_FEAT_MEMORY(feat)) {
	case CIS_FEAT_MEM_NONE:
		break;
	case CIS_FEAT_MEM_LEN:
		conf->memspace = 1;
		conf->mem = xmalloc(sizeof(*conf->mem));
		conf->mem->length = tpl16(p) << 8;
		break;
	case CIS_FEAT_MEM_ADDR:
		conf->memspace = 1;
		conf->mem = xmalloc(sizeof(*conf->mem));
		conf->mem->length = tpl16(p) << 8;
		conf->mem->address = tpl16(p + 2) << 8;
		break;
	case CIS_FEAT_MEM_WIN: {
		struct cis_memblk *mem;
		struct cis_memblk *last_mem = NULL;

		conf->memspace = 1;
		x = *p++;
		conf->memwins = CIS_MEM_WINS(x);
		for (i = 0; i < conf->memwins; i++) {
			mem = xmalloc(sizeof(*mem));
			if (last_mem)
				last_mem->next = mem;
			else
				conf->mem = mem;
			last_mem = mem;
			mem->length = parse_num(CIS_MEM_LENSZ(x) | 0x10, p, &p, 0) << 8;
			mem->address = parse_num(CIS_MEM_ADDRSZ(x) | 0x10, p, &p, 0) << 8;
			if (x & CIS_MEM_HOST) {
				mem->host_address = parse_num(CIS_MEM_ADDRSZ(x) | 0x10,
							      p, &p, 0) << 8;
			}
		}
		break;
	    }
	}
	if (feat & CIS_FEAT_MISC) {
		conf->misc_valid = 1;
		conf->misc = *p++;
	}
}

/*
 *	Read the tuples from the card.
 *	The processing of tuples is as follows:
 *		- Read tuples at attribute memory, offset 0.
 *		- If a CIS_END is the first tuple, look for
 *		  a tuple list at common memory offset 0; this list
 *		  must start with a LINKTARGET.
 *		- If a long link tuple was encountered, execute the long
 *		  link.
 *		- If a no-link tuple was seen, terminate processing.
 *		- If no no-link tuple exists, and no long link tuple
 *		  exists while processing the primary tuple list,
 *		  then look for a LINKTARGET tuple in common memory.
 *		- If a long link tuple is found in any list, then process
 *		  it. Only one link is allowed per list.
 */
static struct tuple_list *tlist;

static struct tuple_list *
read_tuples(int fd)
{
	struct tuple_list *tl = 0, *last_tl;
	struct tuple *tp;
	int     flag;
	off_t   offs;

	tlist = 0;
	last_tl = tlist = read_one_tuplelist(fd, MDF_ATTR, (off_t) 0);

	/* Now start processing the links (if any). */
	do {
		flag = MDF_ATTR;
		tp = find_tuple_in_list(last_tl, CIS_LONGLINK_A);
		if (tp == 0) {
			flag = 0;
			tp = find_tuple_in_list(last_tl, CIS_LONGLINK_C);
		}
		if (tp && tp->length == 4) {
			offs = tpl32(tp->data);
#ifdef	DEBUG
			printf("Checking long link at %qd (%s memory)\n",
			    offs, flag ? "Attribute" : "Common");
#endif
			/* If a link was found, read the tuple list from it. */
			if (ck_linktarget(fd, offs, flag)) {
				tl = read_one_tuplelist(fd, flag, offs);
				last_tl->next = tl;
				last_tl = tl;
			}
		} else
			tl = 0;
	} while (tl);

	/*
	 * If the primary list had no NOLINK tuple, and no LINKTARGET,
	 * then try to read a tuple list at common memory (offset 0).
	 */
	if (find_tuple_in_list(tlist, CIS_NOLINK) == 0 && tlist->next == 0 &&
	    ck_linktarget(fd, (off_t) 0, 0)) {
#ifdef	DEBUG
		printf("Reading long link at %qd (%s memory)\n",
		    offs, flag ? "Attribute" : "Common");
#endif
		tlist->next = read_one_tuplelist(fd, 0, (off_t) 0);
	}
	return (tlist);
}

/*
 *	Read one tuple list from the card.
 */
static struct tuple_list *
read_one_tuplelist(int fd, int flags, off_t offs)
{
	struct tuple *tp, *last_tp = 0;
	struct tuple_list *tl;
	struct tuple_info *tinfo;
	int     total = 0;
	unsigned char code, length;
	int     fmvj182 = 0;
#ifdef HSSYNTH
	int     hss = 0;
#endif	/* HSSYNTH */

	/* Check to see if this memory has already been scanned. */
	for (tl = tlist; tl; tl = tl->next)
		if (tl->offs == offs && tl->flags == (flags & MDF_ATTR))
			return (0);
	tl = xmalloc(sizeof(*tl));
	tl->offs = offs;
	tl->flags = flags & MDF_ATTR;
	ioctl(fd, PIOCRWFLAG, &flags);
	lseek(fd, offs, SEEK_SET);
	do {
		if (read_attr(fd, &code, 1) != 1) {
			warn("CIS code read");
			break;
		}
		total++;
		if (code == CIS_NULL)
			continue;
		tp = xmalloc(sizeof(*tp));
		tp->code = code;
		if (code == CIS_END)
			length = 0;
		else {
			if (read_attr(fd, &length, 1) != 1) {
				warn("CIS len read");
				break;
			}
			total++;
			if (fmvj182 && (code == 0x1b) && (length == 25))
				length = 31;
		}
		tp->length = length;
#ifdef	DEBUG
		printf("Tuple code = 0x%x, len = %d\n", code, length);
#endif
		if (length == 0xFF) {
			length = tp->length = 0;
			code = CIS_END;
		}
		if (length != 0) {
			total += length;
			tp->data = xmalloc(length);
			if (read_attr(fd, tp->data, length) != length) {
				warn("CIS read");
				break;
			}
		}

		/*
		 * Check the tuple, and ignore it if it isn't in the table
		 * or the length is illegal.
		 */
		tinfo = get_tuple_info(code);
		if (code == CIS_INFO_V1) {
			/* Hack for broken CIS of FMV-J182 Ethernet card */
			fmvj182 = fmvj182_check(tp->data);
#ifdef RATOCLAN
			/* Hack for RATOC LAN card */
			rex5588 = rex5588_check(tp->data);
#endif /* RATOCLAN */
#ifdef	HSSYNTH
			/* Hack for Hitachi Speech Synthesis card */
			hss = hss_check(tp->data);
#endif	/* HSSYNTH */
		}
		if (tinfo == NULL || (tinfo->length != 255 && tinfo->length > length)) {
			printf("code %s ignored\n", tuple_name(code));
			tp->code = CIS_NULL;
		}
		if (tl->tuples == NULL)
			tl->tuples = tp;
		else
			last_tp->next = tp;
		last_tp = tp;
	} while (code != CIS_END && total < 1024);
	return (tl);
}

/*
 *	return true if the offset points to a LINKTARGET tuple.
 */
static int
ck_linktarget(int fd, off_t offs, int flag)
{
	char    blk[5];

	ioctl(fd, PIOCRWFLAG, &flag);
	lseek(fd, offs, SEEK_SET);
	if (read_attr(fd, blk, 5) != 5)
		return (0);
	if (blk[0] == 0x13 &&
	    blk[1] == 0x3 &&
	    blk[2] == 'C' &&
	    blk[3] == 'I' &&
	    blk[4] == 'S')
		return (1);
	return (0);
}

/*
 *	find_tuple_in_list - find a tuple within a
 *	single tuple list.
 */
static struct tuple *
find_tuple_in_list(struct tuple_list *tl, unsigned char code)
{
	struct tuple *tp;

	for (tp = tl->tuples; tp; tp = tp->next)
		if (tp->code == code)
			break;
	return (tp);
}

static int
read_attr(int fd, char *bp, int len)
{
	char    blk[1024], *p = blk;
	int     i, l;

	if (len > sizeof(blk) / 2)
		len = sizeof(blk) / 2;
	l = i = read(fd, blk, len * 2);
	if (i <= 0) {
		printf("Read return %d bytes (expected %d)\n", i, len * 2);
		return (i);
	}
	while (i > 0) {
		*bp++ = *p++;
		p++;
		i -= 2;
	}
	return (l / 2);
}

/*
 *	return table entry for code.
 */
static struct tuple_info *
get_tuple_info(unsigned char code)
{
	struct tuple_info *tp;

	for (tp = tuple_info; tp->name; tp++)
		if (tp->code == code)
			return (tp);
	printf("Code %d not found\n", code);
	return (0);
}

char *
tuple_name(unsigned char code)
{
	struct tuple_info *tp;

	tp = get_tuple_info(code);
	if (tp)
		return (tp->name);
	return ("Unknown");
}
