/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 1989 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*	from OpenSolaris "makedev.c	1.6	05/06/08 SMI"	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)makedev.c	1.16 (gritter) 9/22/06
 */

/*
 * University Copyright- Copyright (c) 1982, 1986, 1988
 * The Regents of the University of California
 * All Rights Reserved
 *
 * University Acknowledgment- Portions of this document are derived from
 * software developed by the University of California, Berkeley, and its
 * contributors.
 */

/* Note added 9/25/83
	Setting the parameter biggestfont in the DESC file
	to be at least as big as the number of characters
	in the largest font for a particular device
	eliminates the "font X too big for position Y"
	message from troff.
	Thanks to Dave Stephens, WECo.
*/
/*
  makedev:
	read text info about a particular device
	(e.g., cat, 202, aps5) from file, convert
	it into internal (binary) form suitable for
	fast reading by troff initialization (ptinit()).

	Usage:

	makedev DESC [ F ... ]
		uses DESC to create a description file
		using the information therein.
		It creates the file DESC.out.

	makedev F ...
		makes the font tables for fonts F only,
		creates files F.out.

	DESC.out contains:
	dev structure with fundamental sizes
	list of sizes (nsizes+1) terminated by 0, as short's
	indices of char names (nchtab * sizeof(short))
	char names as hy\0em\0... (lchname)
	nfonts occurrences of
		widths (nwidth)
		kerning (nwidth) [ascender+descender only so far]
		codes (nwidth) to drive actual typesetter
		fitab (nchtab+128-32)
	each of these is an array of char.

	dev.filesize contains the number of bytes
	in the file, excluding the dev part itself.

	F.out contains the font header, width, kern, codes, and fitab.
	Width, kern and codes are parallel arrays.
	(Which suggests that they ought to be together?)
	Later, we might allow for codes which are actually
	sequences of formatting info so characters can be drawn.

	As of 9/4/05, troff reads the text files directly, and this
	code is called directly during initialization to fill the
	binary structures in core.
*/

#include	"string.h"
#include	"sys/types.h"
#include	"sys/stat.h"
#include	"fcntl.h"
#include	"unistd.h"
#include	"stdlib.h"
#include	"ctype.h"
#include	"dev.h"

#ifndef	EOF
#define	EOF	(-1)
#endif

extern void	errprint(const char *, ...);

struct mfile {
	char	*buf;
	size_t	size;
	size_t	pos;
	int	delc;
};

#define	BYTEMASK	0377
#define	skipline(mp)	while ((mp)->delc != '\n' && \
				(mp)->buf[(mp)->pos++] != '\n' && \
				(mp)->buf[(mp)->pos-1] != 0)

static struct	dev	dev;
static struct	Font	font;

#define	NSIZE	100	/* maximum number of sizes */
static int	size[NSIZE];
#define	NCH	512	/* max number of characters with funny names */
static char	chname[5*NCH];	/* character names, including \0 for each */
static short	chtab[NCH];	/* index of character in chname */

#define	NFITAB	(NCH + 128-32)	/* includes ascii chars, but not non-graphics */
static char	fitab[NFITAB];	/* font index table: position of char i on this font. */
			/* zero if not there */

#define	FSIZE	256	/* size of a physical font (e.g., 102 for cat) */
static char	width[FSIZE];	/* width table for a physical font */
static char	kern[FSIZE];	/* ascender+descender info */
static char	code[FSIZE];	/* actual device codes for a physical font */

#define	NFONT	60	/* max number of default fonts */
static char	fname[NFONT][10];	/* temp space to hold default font names */

static void *_readfont(const char *, size_t *, int);
static int getlig(struct mfile *, int);
static struct mfile *mopen(const char *);
static void mclose(struct mfile *);
static char *sget(struct mfile *);
#define	dget(mp, ip)	iget(mp, ip, 10)
#define	oget(mp, ip)	iget(mp, ip, 8)
static int iget(struct mfile *, int *, int);
static int cget(struct mfile *);
static int peek(struct mfile *);

void *
readdesc(const char *name)
{
	char *cmd, *p, *q;
	int i, totfont, v;
	char *cpout, *fpout;
	size_t sz, fsz;
	char *dir, *dp, *dq;
	struct mfile *mp;
	size_t l;

	memset(&dev, 0, sizeof dev);
	if ((mp = mopen(name)) == NULL) {
		errprint("can't open tables for %s", name);
		return NULL;
	}
	while ((cmd = sget(mp)) != NULL) {
		if (cmd[0] == '#')	/* comment */
			skipline(mp);
		else if (strcmp(cmd, "res") == 0) {
			dget(mp, &dev.res);
		} else if (strcmp(cmd, "hor") == 0) {
			dget(mp, &dev.hor);
		} else if (strcmp(cmd, "vert") == 0) {
			dget(mp, &dev.vert);
		} else if (strcmp(cmd, "unitwidth") == 0) {
			dget(mp, &dev.unitwidth);
		} else if (strcmp(cmd, "sizescale") == 0) {
			dget(mp, &dev.sizescale);
		} else if (strcmp(cmd, "paperwidth") == 0) {
			dget(mp, &dev.paperwidth);
		} else if (strcmp(cmd, "paperlength") == 0) {
			dget(mp, &dev.paperlength);
		} else if (strcmp(cmd, "biggestfont") == 0) {
			dget(mp, &dev.biggestfont);
		} else if (strcmp(cmd, "spare2") == 0) {
			dget(mp, &dev.spare2);
		} else if (strcmp(cmd, "encoding") == 0) {
			dget(mp, &dev.encoding);
		} else if (strcmp(cmd, "allpunct") == 0) {
			dev.allpunct = 1;
		} else if (strcmp(cmd, "anysize") == 0) {
			dev.anysize = 1;
		} else if (strcmp(cmd, "afmfonts") == 0) {
			dev.afmfonts = 1;
		} else if (strcmp(cmd, "lc_ctype") == 0) {
			dev.lc_ctype = 1;
		} else if (strcmp(cmd, "sizes") == 0) {
			dev.nsizes = 0;
			while (dget(mp, &v) != EOF && v != 0)
				size[dev.nsizes++] = v;
			size[dev.nsizes] = 0;	/* need an extra 0 at the end */
		} else if (strcmp(cmd, "fonts") == 0) {
			dget(mp, &dev.nfonts);
			for (i = 0; i < dev.nfonts; i++)
				if ((p = sget(mp)) != NULL)
					strncpy(fname[i], p,
							sizeof fname[i] - 1);
		} else if (strcmp(cmd, "charset") == 0) {
			p = chname;
			dev.nchtab = 0;
			while ((q = sget(mp)) != NULL) {
				n_strcpy(p, q, sizeof(chname) - (p - chname));
				chtab[dev.nchtab++] = p - chname;
				while (*p++)	/* skip to end of name */
					;
			}
			dev.lchname = p - chname;
			chtab[dev.nchtab++] = 0;	/* terminate properly */
		} else
			errprint("Unknown command %s in %s", cmd, name);
	}
	cpout = calloc(1, sz = sizeof dev +
			sizeof *size * (dev.nsizes+1) +
			sizeof *chtab * dev.nchtab +
			sizeof *chname * dev.lchname);
	memcpy(cpout, &dev, sizeof dev);
	v = sizeof dev;
	memcpy(&cpout[v], size, sizeof *size * (dev.nsizes+1));
	v += sizeof *size * (dev.nsizes+1);
	memcpy(&cpout[v], chtab, sizeof *chtab * dev.nchtab);
	v += sizeof *chtab * dev.nchtab;
	memcpy(&cpout[v], chname, sizeof *chname * dev.lchname);
	v += sizeof *chname * dev.lchname;
	l = strlen(name) + sizeof fname[0] + 2;
	dp = dir = malloc(l);
	n_strcpy(dir, name, l);
	for (dq = dir; *dq; dq++)
		if (*dq == '/')
			dp = &dq[1];
	totfont = 0;
	for (i = 0; i < dev.nfonts; i++) {
		n_strcpy(dp, fname[i], l - (dp - dir));
		if ((fpout = _readfont(dir, &fsz, 1)) == NULL) {
			mclose(mp);
			return NULL;
		}
		sz += fsz;
		cpout = realloc(cpout, sz);
		memcpy(&cpout[v], fpout, fsz);
		v += fsz;
		free(fpout);
		totfont += fsz;
	}
	/* back to beginning to install proper size */
	dev.filesize =		/* excluding dev struct itself */
		(dev.nsizes+1) * sizeof(size[0])
		+ dev.nchtab * sizeof(chtab[0])
		+ dev.lchname * sizeof(chname[0])
		+ totfont * sizeof(char);
	memcpy(cpout, &dev, sizeof dev);
	mclose(mp);
	return cpout;
}

void *
readfont(const char *name, struct dev *dp, int warn)
{
	dev = *dp;
	return _readfont(name, NULL, warn);
}

static void *
_readfont(const char *name, size_t *szp, int warn)	/* create fitab and width tab for font */
{
	struct mfile *mp;
	int i, nw = 0, spacewidth, n = 0, v;
	char *ch, *cmd;
	char *cpout;

	if ((mp = mopen(name)) == NULL) {
		if (warn)
			errprint("Can't load font %s", name);
		return NULL;
	}
	for (i = 0; i < NFITAB; i++)
		fitab[i] = 0;
	for (i = 0; i < FSIZE; i++)
		width[i] = kern[i] = code[i] = 0;
	font.specfont = font.ligfont = spacewidth = 0;
	while ((cmd = sget(mp)) != NULL) {
		if (cmd[0] == '#')
			skipline(mp);
		else if (strcmp(cmd, "name") == 0) {
			if ((ch = sget(mp)) != NULL)
				strncpy(font.namefont, ch,
						sizeof font.namefont - 1);
		} else if (strcmp(cmd, "internalname") == 0) {
			if ((ch = sget(mp)) != NULL)
				strncpy(font.intname, ch,
						sizeof font.intname - 1);
		} else if (strcmp(cmd, "special") == 0)
			font.specfont = 1;
		else if (strcmp(cmd, "spare1") == 0)
			cget(mp);
		else if (strcmp(cmd, "ligatures") == 0) {
			font.ligfont = getlig(mp, warn);
		} else if (strcmp(cmd, "spacewidth") == 0) {
			dget(mp, &spacewidth);
			width[0] = spacewidth;	/* width of space on this font */
		} else if (strcmp(cmd, "charset") == 0) {
			skipline(mp);
			nw = 0;
			/* widths are origin 1 so fitab==0 can mean "not there" */
			while ((ch = sget(mp)) != NULL) {
				if (peek(mp) != '"') {	/* it's a genuine new character */
					nw++;
					dget(mp, &i);
					width[nw] = i;
					dget(mp, &i);
					kern[nw] = i;
					iget(mp, &i, 0);
					code[nw] = i;
				}
				/* otherwise it's a synonym for previous character,
				* so leave previous values intact
				*/
				if (strlen(ch) == 1)	/* it's ascii */
					fitab[ch[0] - 32] = nw;	/* fitab origin omits non-graphics */
				else if (strcmp(ch, "---") != 0) {	/* it has a 2-char name */
					for (i = 0; i < dev.nchtab; i++)
						if (strcmp(&chname[chtab[i]], ch) == 0) {
							fitab[i + 128-32] = nw;	/* starts after the ascii */
							break;
						}
					if (i >= dev.nchtab && warn)
						errprint("font %s: %s not in charset\n", name, ch);
				}
				skipline(mp);
			}
			nw++;
			if (dev.biggestfont >= nw)
				n = dev.biggestfont;
			else {
				if (dev.biggestfont > 0 && warn)
					errprint("font %s too big\n", name);
				n = nw;
			}
			font.nwfont = n;
		}
	}
	if (spacewidth == 0)
		width[0] = dev.res * dev.unitwidth / 72 / 3;

	cpout = calloc(1, sizeof font +
			sizeof *width * (font.nwfont & BYTEMASK) +
			sizeof *kern * (font.nwfont & BYTEMASK) +
			sizeof *code * (font.nwfont & BYTEMASK) +
			sizeof *fitab * dev.nchtab+128-32);
	memcpy(cpout, &font, sizeof font);
	v = sizeof font;
	memcpy(&cpout[v], width, sizeof *width * (font.nwfont & BYTEMASK));
	v +=  sizeof *width * (font.nwfont & BYTEMASK);
	memcpy(&cpout[v], kern, sizeof *kern * (font.nwfont & BYTEMASK));
	v += sizeof *kern * (font.nwfont & BYTEMASK);
	memcpy(&cpout[v], code, sizeof *code * (font.nwfont & BYTEMASK));
	v += sizeof *code * (font.nwfont & BYTEMASK);
	memcpy(&cpout[v], fitab, sizeof *fitab * dev.nchtab+128-32);
	v += sizeof *fitab * dev.nchtab+128-32;
	if (szp)
		*szp = v;
	mclose(mp);
	return cpout;
}

static int
getlig(struct mfile *mp, int warn)	/* pick up ligature list */
{
	int lig;
	char *temp;

	lig = 0;
	while ((temp = sget(mp)) != NULL && strcmp(temp, "0") != 0) {
		if (strcmp(temp, "fi") == 0)
			lig |= LFI;
		else if (strcmp(temp, "fl") == 0)
			lig |= LFL;
		else if (strcmp(temp, "ff") == 0)
			lig |= LFF;
		else if (strcmp(temp, "ffi") == 0)
			lig |= LFFI;
		else if (strcmp(temp, "ffl") == 0)
			lig |= LFFL;
		else if (warn)
			errprint("illegal ligature %s\n", temp);
	}
	skipline(mp);
	return lig;
}

static struct mfile *
mopen(const char *name)
{
	struct stat	st;
	int	fd;
	struct mfile	*mp;

	if ((mp = calloc(1, sizeof *mp)) == NULL ||
			(fd = open(name, O_RDONLY)) < 0 ||
			fstat(fd, &st) < 0 ||
			(mp->buf = malloc(mp->size = st.st_size + 1)) == NULL ||
			read(fd, mp->buf, st.st_size) != st.st_size ||
			close(fd) < 0)
		return NULL;
	mp->buf[mp->size - 1] = 0;
	return mp;
}

static void
mclose(struct mfile *mp)
{
	free(mp->buf);
	free(mp);
}

static char *
sget(struct mfile *mp)
{
	int	c;
	char	*rp;

	if (mp->pos < mp->size - 1) {
		do
			c = mp->buf[mp->pos++]&0377;
		while (isspace(c));
		rp = &mp->buf[mp->pos-1];
		if (c != 0) do {
			c = mp->buf[mp->pos++]&0377;
		} while (c != 0 && !isspace(c));
		mp->delc = mp->buf[mp->pos-1] & 0377;
		mp->buf[mp->pos-1] = 0;
	} else
		rp = NULL;
	return rp;
}

static int
iget(struct mfile *mp, int *ip, int base)
{
	char	*xp;
	int	gotit;

	if (mp->pos >= mp->size - 1)
		return EOF;
	*ip = strtol(&mp->buf[mp->pos], &xp, base);
	gotit = xp > &mp->buf[mp->pos] && isdigit(xp[-1]&0377);
	mp->pos = xp - mp->buf;
	mp->delc = 0;
	return gotit;
}

static int
cget(struct mfile *mp)
{
	if (mp->pos >= mp->size - 1)
		return EOF;
	mp->delc = 0;
	return mp->buf[mp->pos++] & 0377;
}

static int
peek(struct mfile *mp)
{
	char	*cp;

	if (mp->pos >= mp->size - 1)
		return EOF;
	cp = &mp->buf[mp->pos];
	while (isspace(*cp&0377))
		cp++;
	return *cp & 0377;
}
