/*
 *	Read/dump CIS tuples.
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

#include <pccard/card.h>
#include <pccard/cis.h>

#include "readcis.h"

static int read_attr(int fd, char *bp, int len);
struct tuple_list *read_one_tuplelist(int, int, off_t);
int     ck_linktarget(int, off_t, int);
void    cis_info(struct cis *cp, unsigned char *p, int len);
void    device_desc(unsigned char *p, int len, struct dev_mem *dp);
void    config_map(struct cis *cp, unsigned char *p, int len);
void    cis_config(struct cis *cp, unsigned char *p, int len);

struct tuple_info tuple_info[] = {
	{"Null tuple", 0x00, 0},
	{"Common memory descriptor", 0x01, 255},
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
	{"Functional ID", 0x21, 255},
	{"Functional EXT", 0x22, 255},
	{"Software interleave", 0x23, 2},
	{"Version 2 Info", 0x40, 255},
	{"Data format", 0x41, 255},
	{"Geometry", 0x42, 4},
	{"Byte order", 0x43, 2},
	{"Card init date", 0x44, 4},
	{"Battery replacement", 0x45, 4},
	{"Organisation", 0x46, 255},
	{"Terminator", 0xFF, 255},
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
void
cis_info(struct cis *cp, unsigned char *p, int len)
{
	cp->maj_v = *p++;
	cp->min_v = *p++;
	strncpy(cp->manuf, p, MAXSTR - 1);
	while (*p++);
	strncpy(cp->vers, p, MAXSTR - 1);
	while (*p++);
	strncpy(cp->add_info1, p, MAXSTR - 1);
	while (*p++);
	strncpy(cp->add_info2, p, MAXSTR - 1);
}

/*
 *	device_desc - decode device descriptor.
 */
void
device_desc(unsigned char *p, int len, struct dev_mem *dp)
{
	while (len > 0 && *p != 0xFF) {
		dp->valid = 1;
		dp->type = (*p & 0xF0) >> 4;
		dp->wps = !!(*p & 0x8);
		dp->speed = *p & 7;
		p++;
		if (*p != 0xFF) {
			dp->addr = *p >> 3;
			dp->units = *p & 7;
		}
		p++;
		len -= 2;
	}
}

/*
 *	configuration map of card control register.
 */
void
config_map(struct cis *cp, unsigned char *p, int len)
{
	unsigned char *p1;
	int     i;
	union {
		unsigned long l;
		unsigned char b[4];
	} u;

	p1 = p + 1;
	cp->last_config = *p1++ & 0x3F;
	u.l = 0;
	for (i = 0; i <= (*p & 3); i++)
		u.b[i] = *p1++;
	cp->reg_addr = u.l;
	cp->ccrs = *p1;
}

/*
 *	CIS config entry - Decode and build configuration entry.
 */
void
cis_config(struct cis *cp, unsigned char *p, int len)
{
	int     x;
	int     i, j;
	union {
		unsigned long l;
		unsigned char b[4];
	} u;
	struct cis_config *conf, *last;
	struct cis_memblk *mem;
	unsigned char feat;
	struct cis_memblk *lastmem = 0;

	conf = xmalloc(sizeof(*conf));
	if ((last = cp->conf) != 0) {
		while (last->next)
			last = last->next;
		last->next = conf;
	} else
		cp->conf = conf;
	conf->id = *p & 0x3F;
	if (*p & 0x40)
		cp->def_config = conf;
	if (*p++ & 0x80)
		p++;
	feat = *p++;
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
		conf->io_bus = (*p >> 5) & 3;
		if (*p++ & CIS_IO_RANGE) {
			struct cis_ioblk *io, *last = 0;
			i = CIS_IO_ADSZ(*p);
			j = CIS_IO_BLKSZ(*p++);
			for (x = 0; x < conf->io_blks; x++) {
				io = xmalloc(sizeof(*io));
				if (last)
					last->next = io;
				else
					conf->io = io;
				last = io;
				u.l = 0;
				switch (i) {
				case 0:
					break;
				case 1:
					u.b[0] = *p++;
					break;
				case 2:
					u.b[0] = *p++;
					u.b[1] = *p++;
					break;
				case 3:
					u.b[0] = *p++;
					u.b[1] = *p++;
					u.b[2] = *p++;
					u.b[3] = *p++;
					break;
				}
				io->addr = u.l;
				u.l = 0;
				switch (j) {
				case 0:
					break;
				case 1:
					u.b[0] = *p++;
					u.l++;
					break;
				case 2:
					u.b[0] = *p++;
					u.b[1] = *p++;
					u.l++;
					break;
				case 3:
					u.b[0] = *p++;
					u.b[1] = *p++;
					u.b[2] = *p++;
					u.b[3] = *p++;
					u.l++;
					break;
				}
				io->size = u.l;
			}
		}
	}
	if (feat & CIS_FEAT_IRQ) {
		conf->irq = 1;
		conf->irqlevel = *p & 0xF;
		conf->irq_flags = *p & 0xF0;
		if (*p++ & CIS_IRQ_MASK) {
			conf->irq_mask = (p[1] << 8) | p[0];
			p += 2;
		}
	}
	switch (CIS_FEAT_MEMORY(feat)) {
	case 0:
		break;
	case 1:
		conf->memspace = 1;
		conf->mem = xmalloc(sizeof(*conf->mem));
		conf->mem->length = ((p[1] << 8) | p[0]) << 8;
		break;
	case 2:
		conf->memspace = 1;
		conf->mem = xmalloc(sizeof(*conf->mem));
		conf->mem->length = ((p[1] << 8) | p[0]) << 8;
		conf->mem->address = ((p[3] << 8) | p[2]) << 8;
		break;
	case 3:
		conf->memspace = 1;
		x = *p++;
		conf->memwins = CIS_MEM_WINS(x);
		for (i = 0; i < conf->memwins; i++) {
			mem = xmalloc(sizeof(*mem));
			if (i == 0)
				conf->mem = mem;
			else
				lastmem->next = mem;
			lastmem = mem;
			u.l = 0;
			for (j = 0; j < CIS_MEM_LENSZ(x); j++)
				u.b[j] = *p++;
			mem->length = u.l << 8;
			u.l = 0;
			for (j = 0; j < CIS_MEM_ADDRSZ(x); j++)
				u.b[j] = *p++;
			mem->address = u.l << 8;
			if (x & CIS_MEM_HOST) {
				u.l = 0;
				for (j = 0; j < CIS_MEM_ADDRSZ(x); j++)
					u.b[j] = *p++;
				mem->host_address = u.l << 8;
			}
		}
		break;
	}
	if (feat & 0x80) {
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

struct tuple_list *
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
			offs = tp->data[0] |
			    (tp->data[1] << 8) |
			    (tp->data[2] << 16) |
			    (tp->data[3] << 24);
#ifdef	DEBUG
			printf("Checking long link at %ld (%s memory)\n",
			    offs, flag ? "Attribute" : "Common");
#endif
			/* If a link was found, read the tuple list from it. */
			if (ck_linktarget(fd, offs, flag)) {
				tl = read_one_tuplelist(fd, flag, offs);
				last_tl->next = tl;
				last_tl = tl;
			}
		}
	} while (tl);

	/*
	 * If the primary list had no NOLINK tuple, and no LINKTARGET,
	 * then try to read a tuple list at common memory (offset 0).
	 */
	if (find_tuple_in_list(tlist, CIS_NOLINK) == 0 && tlist->next == 0 &&
	    ck_linktarget(fd, (off_t) 0, 0)) {
#ifdef	DEBUG
		printf("Reading long link at %ld (%s memory)\n",
		    offs, flag ? "Attribute" : "Common");
#endif
		tlist->next = read_one_tuplelist(fd, 0, (off_t) 0);
	}
	return (tlist);
}

/*
 *	Read one tuple list from the card.
 */
struct tuple_list *
read_one_tuplelist(int fd, int flags, off_t offs)
{
	struct tuple *tp, *last_tp = 0;
	struct tuple_list *tl;
	struct tuple_info *tinfo;
	int     total = 0;
	unsigned char code, length;

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
			perror("CIS code read");
			break;
		}
		total++;
		if (code == CIS_NULL)
			continue;
		tp = xmalloc(sizeof(*tp));
		tp->code = code;
		if (read_attr(fd, &length, 1) != 1) {
			perror("CIS len read");
			break;
		}
		total++;
		tp->length = length;
#ifdef	DEBUG
		fprintf(stderr, "Tuple code = 0x%x, len = %d\n",
		    code, length);
#endif
		if (length == 0xFF) {
			length = tp->length = 0;
			code = CIS_END;
		}
		if (length != 0) {
			total += length;
			tp->data = xmalloc(length);
			if (read_attr(fd, tp->data, length) != length) {
				perror("CIS read");
				break;
			}
		}

		/*
		 * Check the tuple, and ignore it if it isn't in the table
		 * or the length is illegal.
		 */
		tinfo = get_tuple_info(code);
		if (tinfo == 0 || (tinfo->length != 255 && tinfo->length != length)) {
			printf("code %s ignored\n", tuple_name(code));
			tp->code = CIS_NULL;
		}
		if (tl->tuples == 0)
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
int
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
 *	find_tuple - find the indicated tuple in the CIS
 */
struct tuple *
find_tuple(struct cis *sp, unsigned char code)
{
	struct tuple_list *tl;
	struct tuple *tp;

	for (tl = sp->tlist; tl; tl = tl->next)
		if ((tp = find_tuple_in_list(tl, code)) != 0)
			return (tp);
	return (0);
}

/*
 *	find_tuple_in_list - find a tuple within a
 *	single tuple list.
 */
struct tuple *
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
struct tuple_info *
get_tuple_info(unsigned char code)
{
	struct tuple_info *tp;

	for (tp = tuple_info; tp->name; tp++)
		if (tp->code == code)
			return (tp);
	printf("Code %d not found\n", code);
	return (0);
}

char   *
tuple_name(unsigned char code)
{
	struct tuple_info *tp;

	tp = get_tuple_info(code);
	if (tp)
		return (tp->name);
	return ("Unknown");
}
