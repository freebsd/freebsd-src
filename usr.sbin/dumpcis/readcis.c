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

#include <pccard/cardinfo.h>
#include <pccard/cis.h>

#include "readcis.h"

static int ck_linktarget(int, off_t, int);
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

static void *
xmalloc(int sz)
{
	void   *p;

	sz = (sz + 7) & ~7;
	p = malloc(sz);
	if (p)
		bzero(p, sz);
	else
		errx(1, "malloc");
	return (p);
}

/*
 *	After reading the tuples, decode the relevant ones.
 */
struct tuple_list *
readcis(int fd)
{

	return (read_tuples(fd));
}

/*
 *	free_cis - delete cis entry.
 */
void
freecis(struct tuple_list *tlist)
{
	struct tuple_list *tl;
	struct tuple *tp;

	while ((tl = tlist) != 0) {
		tlist = tl->next;
		while ((tp = tl->tuples) != 0) {
			tl->tuples = tp->next;
			free(tp->data);
			free(tp);
		}
		free(tl);
	}
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
			printf("Checking long link at %zd (%s memory)\n",
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
		offs = 0;
#ifdef	DEBUG
		printf("Reading long link at %zd (%s memory)\n",
		    offs, flag ? "Attribute" : "Common");
#endif
		tlist->next = read_one_tuplelist(fd, 0, offs);
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
	lseek(fd, offs, SEEK_SET);
	do {
		if (read(fd, &code, 1) != 1) {
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
			if (read(fd, &length, 1) != 1) {
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
			if (read(fd, tp->data, length) != length) {
				warn("CIS read");
				break;
			}
		}

		/*
		 * Check the tuple, and ignore it if it isn't in the table
		 * or the length is illegal.
		 */
		tinfo = get_tuple_info(code);
		if (tinfo != NULL && (tinfo->length != 255 && tinfo->length > length)) {
			printf("code %s (%d) ignored\n", tuple_name(code), code);
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
	if (read(fd, blk, 5) != 5)
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
	return (0);
}

const char *
tuple_name(unsigned char code)
{
	struct tuple_info *tp;

	tp = get_tuple_info(code);
	if (tp)
		return (tp->name);
	return ("Unknown");
}
