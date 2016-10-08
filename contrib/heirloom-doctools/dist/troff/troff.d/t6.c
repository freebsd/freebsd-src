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
 * Copyright 2003 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*	from OpenSolaris "t6.c	1.9	05/06/08 SMI"	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)t6.c	1.194 (gritter) 2/7/10
 */

/*
 * Changes Copyright (c) 2014 Carsten Kunze <carsten.kunze at arcor.de>
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

/*
 * t6.c
 * 
 * width functions, sizes and fonts
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "tdef.h"
#include "dev.h"
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "ext.h"
#include "afm.h"
#include "pt.h"
#include "troff.h"
#include "fontmap.h"

/* fitab[f][c] is 0 if c is not on font f */
	/* if it's non-zero, c is in fontab[f] at position
	 * fitab[f][c].
	 */
int	*fontlab;
int	*pstab;
int	*cstab;
int	*ccstab;
int	**fallbacktab;
float	*zoomtab;
int	*bdtab;
int	**lhangtab;
int	**rhangtab;
int	**kernafter;
int	**kernbefore;
int	**ftrtab;
struct lgtab	**lgtab;
int	***lgrevtab;
struct tracktab	*tracktab;
int	sbold = 0;
int	kern = 0;
struct box	mediasize, bleedat, trimat, cropat;
int	psmaxcode;
struct ref	*anchors, *links, *ulinks;
static int	_minflg;
int	lastrst;
int	lastrsb;
int	spacewidth;

static void	kernsingle(int **);
static int	_ps2cc(const char *name, int create);

int
width(register tchar j)
{
	register int i, k;

	_minflg = minflg;
	minflg = minspc = 0;
	lasttrack = 0;
	rawwidth = 0;
	lastrst = lastrsb = 0;
	if (isadjspc(j))
		return(0);
	if (j & (ZBIT|MOT)) {
		if (iszbit(j))
			return(0);
		if (isvmot(j))
			return(0);
		k = absmot(j);
		if (isnmot(j))
			k = -k;
		return(k);
	}
	i = cbits(j);
	if (html && i >= NCHARS)
		i = ' ';
	if (i < ' ') {
		if (i == '\b')
			return(-widthp);
		if (i == PRESC)
			i = eschar;
		else if (iscontrol(i))
			return(0);
		else if (isxfunc(j, CHAR)) {
			k = charout[sbits(j)].width + lettrack;
			lastrst = charout[sbits(j)].height;
			lastrsb = -charout[sbits(j)].depth;
			goto set;
		}
	} else if (i == ' ' && issentsp(j))
	{
		_minflg = 0;
		return(ses);
	}
	if (i==ohc)
		return(0);
	if (!xflag || !isdi(j)) {
		if (i == STRETCH)
			setcbits(j, ' ');
		i = trtab[i];
		i = ftrans(fbits(j), i);
	}
	if (i < 32)
		return(0);
	lasttrack = 0;
	if (sfbits(j) == oldbits) {
		xfont = pfont;
		xpts = ppts;
	} else 
		xbits(j, 0);
	if (widcache[i-32].fontpts == xfont + (xpts<<8) &&
			(i > 32 || widcache[i-32].evid == evname) &&
			!setwdf && !_minflg && !horscale) {
		rawwidth = widcache[i-32].width - widcache[i-32].track;
		k = widcache[i-32].width + lettrack;
		lastrst = widcache[i-32].rst;
		lastrsb = widcache[i-32].rsb;
		lasttrack = widcache[i-32].track;
	} else {
		if (_minflg && i == 32 && cbits(j) != 32)
			_minflg = 0;
		k = getcw(i-32);
		if (i == 32 && _minflg && !cs) {
			_minflg = 0;
			minspc = getcw(0) - k;
		}
		_minflg = 0;
	set:	if (bd && !fmtchar)
			k += (bd - 1) * HOR;
		if (cs && !fmtchar)
			k = cs;
	}
	widthp = k;
	return(k);
}

/*
 * clear width cache-- s means just space
 */
void
zapwcache(int s)
{
	register int i;

	if (s) {
		widcache[0].fontpts = 0;
		return;
	}
	for (i=0; i<NWIDCACHE; i++)
		widcache[i].fontpts = 0;
}

int
getcw(register int i)
{
	register int	k;
	register int	*p;
	register int	x, j;
	int nocache = 0;
	int	ofont = xfont;
	int	s, t;
	double	z = 1, zv;
	struct afmtab	*a;
	int	cd = 0;

	bd = 0;
	if (i >= nchtab + 128-32) {
		if (afmtab && fontbase[xfont]->afmpos - 1 >= 0) {
			cd = nchtab + 128;
			i -= cd;
		} else {
			j = abscw(i + 32 - (nchtab+128));
			goto g0;
		}
	}
	if (i == 0) {	/* a blank */
		if (_minflg) {
			j = minspsz;
			nocache = 1;
		} else
			j = spacesz;
		if (fontbase[xfont]->cspacewidth >= 0)
			k = fontbase[xfont]->cspacewidth;
		else if (spacewidth || gflag)
			k = fontbase[xfont]->spacewidth;
		else
			k = fontab[xfont][0];
		k = (k * j + 6) / 12;
		lastrst = lastrsb = 0;
		/* this nonsense because .ss cmd uses 1/36 em as its units */
		/* and default is 12 */
		goto g1;
	}
	if ((j = fitab[xfont][i]) == 0) {	/* it's not on current font */
		int ii, jj, t;
		/* search through search list of xfont
		 * to see what font it ought to be on.
		 * first searches explicit fallbacks, then
		 * searches S, then remaining fonts in wraparound order.
		 */
		nocache = 1;
		if (fallbacktab[xfont]) {
			for (jj = 0; fallbacktab[xfont][jj] != 0; jj++) {
				if ((ii = findft(fallbacktab[xfont][jj],0)) < 0)
					continue;
				t = ftrans(ii, i + 32) - 32;
				j = fitab[ii][t];
				if (j != 0) {
					xfont = ii;
					goto found;
				}
			}
		}
		if (smnt) {
			for (ii=smnt, jj=0; jj < nfonts; jj++, ii=ii % nfonts + 1) {
				if (fontbase[ii] == NULL)
					continue;
				t = ftrans(ii, i + 32) - 32;
				j = fitab[ii][t];
				if (j != 0) {
					/*
					 * troff traditionally relies on the
					 * device postprocessor to find the
					 * appropriate character since it
					 * searches the fonts in the same
					 * order. This does not work with the
					 * new requests anymore, so change
					 * the font explicitly.
					 */
					if (xflag)
						xfont = ii;
				found:	p = fontab[ii];
					k = *(p + j);
					if (afmtab &&
					    (t=fontbase[ii]->afmpos-1)>=0) {
						a = afmtab[t];
						if (a->bbtab[j]) {
							lastrst = a->bbtab[j][3];
							lastrsb = a->bbtab[j][1];
						} else {
							lastrst = a->ascender;
							lastrsb = a->descender;
						}
					}
					if (xfont == sbold)
						bd = bdtab[ii];
					if (setwdf)
						numtab[CT].val |= kerntab[ii][j];
					goto g1;
				}
			}
		}
		k = fontab[xfont][0];	/* leave a space-size space */
		lastrst = lastrsb = 0;
		goto g1;
	}
 g0:
	p = fontab[xfont];
	if (setwdf)
		numtab[CT].val |= kerntab[xfont][j];
	if (afmtab && (t = fontbase[xfont]->afmpos-1) >= 0) {
		a = afmtab[t];
		if (a->bbtab[j]) {
			lastrst = a->bbtab[j][3];
			lastrsb = a->bbtab[j][1];
		} else {
			/*
			 * Avoid zero values by all means. In many use
			 * cases, endless loops will result unless values
			 * are non-zero.
			 */
			lastrst = a->ascender;
			lastrsb = a->descender;
		}
	}
	k = *(p + j);
	if (dev.anysize == 0 || xflag == 0 || (z = zoomtab[xfont]) == 0)
		z = 1;
 g1:
	zv = z;
	if (horscale) {
		z *= horscale;
		nocache = 1;
	}
	if (!bd)
		bd = bdtab[ofont];
	if ((cs = cstab[ofont])) {
		nocache = 1;
		if ((ccs = ccstab[ofont]))
			x = pts2u(ccs); 
		else 
			x = xpts;
		cs = (cs * EMPTS(x)) / 36;
	}
	k = (k * z * u2pts(xpts) + (Unitwidth / 2)) / Unitwidth;
	lastrst = (lastrst * zv * u2pts(xpts) + (Unitwidth / 2)) / Unitwidth;
	lastrsb = (lastrsb * zv * u2pts(xpts) + (Unitwidth / 2)) / Unitwidth;
	rawwidth = k;
	s = xpts;
	lasttrack = 0;
	if (s <= tracktab[ofont].s1 && tracktab[ofont].n1)
		lasttrack = tracktab[ofont].n1;
	else if (s >= tracktab[ofont].s2 && tracktab[ofont].n2)
		lasttrack = tracktab[ofont].n2;
	else if (s > tracktab[ofont].s1 && s < tracktab[ofont].s2) {
		int	r;
		r = (s * tracktab[ofont].n2 - s * tracktab[ofont].n1
				+ tracktab[ofont].s2 * tracktab[ofont].n1
				- tracktab[ofont].s1 * tracktab[ofont].n2)
			/ (tracktab[ofont].s2 - tracktab[ofont].s1);
		if (r != 0)
			lasttrack = r;
	}
	k += lasttrack + lettrack;
	i += cd;
	if (nocache|bd)
		widcache[i].fontpts = 0;
	else {
		widcache[i].fontpts = xfont + (xpts<<8);
		widcache[i].width = k - lettrack;
		widcache[i].rst = lastrst;
		widcache[i].rsb = lastrsb;
		widcache[i].track = lasttrack;
		widcache[i].evid = evname;
	}
	return(k);
	/* Unitwidth is Units/Point, where
	 * Units is the fundamental digitization
	 * of the character set widths, and
	 * Point is the number of goobies in a point
	 * e.g., for cat, Units=36, Point=6, so Unitwidth=36/6=6
	 * In effect, it's the size at which the widths
	 * translate directly into units.
	 */
}

int
abscw(int n)	/* return index of abs char n in fontab[], etc. */
{	register int i, ncf;

	ncf = fontbase[xfont]->nwfont & BYTEMASK;
	for (i = 0; i < ncf; i++)
		if (codetab[xfont][i] == n)
			return i;
	return 0;
}

int
onfont(tchar c)
{
	int	k = cbits(c);
	int	f = fbits(c);

	if (k <= ' ')
		return 1;
	k -= 32;
	if (k >= nchtab + 128-32) {
		if (afmtab && fontbase[f]->afmpos - 1 >= 0)
			k -= nchtab + 128;
		else
			return abscw(k + 32 - (nchtab+128)) != 0;
	}
	return fitab[f][k] != 0;
}

static int
fvert2pts(int f, int s, int k)
{
	double	z;

	if (k != 0) {
		k = (k * u2pts(s) + (Unitwidth / 2)) / Unitwidth;
		if (dev.anysize && xflag && (z = zoomtab[f]) != 0)
			k *= z;
	}
	return k;
}

int
getascender(void)
{
	struct afmtab	*a;
	int	n;

	if ((n = fontbase[font]->afmpos - 1) >= 0) {
		a = afmtab[n];
		return fvert2pts(font, pts, a->ascender);
	} else
		return 0;
}

int
getdescender(void)
{
	struct afmtab	*a;
	int	n;

	if ((n = fontbase[font]->afmpos - 1) >= 0) {
		a = afmtab[n];
		return fvert2pts(font, pts, a->descender);
	} else
		return 0;
}

int
kernadjust(tchar c, tchar d)
{
	lastkern = 0;
	if (!kern || ismot(c) || ismot(d) || html)
		return 0;
	if (!isdi(c)) {
		c = trtab[cbits(c)] | (c & SFMASK);
		c = ftrans(fbits(c), cbits(c)) | (c & SFMASK);
	}
	if (!isdi(d)) {
		d = trtab[cbits(d)] | (d & SFMASK);
		d = ftrans(fbits(d), cbits(d)) | (d & SFMASK);
	}
	return getkw(c, d);
}

#define	kprime		1021
#define	khash(c, d)	(((2654435769U * (c) * (d) >> 16)&0x7fffffff) % kprime)

static struct knode {
	struct knode	*next;
	tchar	c;
	tchar	d;
	int	n;
} **ktable;

static void
kadd(tchar c, tchar d, int n)
{
	struct knode	*kn;
	int	h;

	if (ktable == NULL)
		ktable = calloc(kprime, sizeof *ktable);
	h = khash(c, d);
	kn = calloc(1, sizeof *kn);
	kn->c = c;
	kn->d = d;
	kn->n = n;
	kn->next = ktable[h];
	ktable[h] = kn;
}

static void
kzap(int f)
{
	struct knode	*kp;
	int	i;

	if (ktable == NULL)
		return;
	for (i = 0; i < kprime; i++)
		for (kp = ktable[i]; kp; kp = kp->next)
			if (fbits(kp->c) == f || fbits(kp->d) == f)
				kp->n = INT_MAX;
}

static tchar
findchar(tchar c)
{
	int	f, i;

	f = fbits(c);
	c = cbits(c);
	i = c - 32;
	if (c != ' ' && i > 0 && i < nchtab + 128 - 32 && fitab[f][i] == 0) {
		int	ii, jj;
		if (fallbacktab[f]) {
			for (jj = 0; fallbacktab[f][jj] != 0; jj++) {
				if ((ii = findft(fallbacktab[f][jj], 0)) < 0)
					continue;
				if (fitab[ii][i] != 0) {
					f = ii;
					goto found;
				}
			}
		}
		if (smnt) {
			for (ii=smnt, jj=0; jj < nfonts; jj++, ii=ii % nfonts + 1) {
				if (fontbase[ii] == NULL)
					continue;
				if (fitab[ii][i] != 0) {
					f = ii;
					goto found;
				}
			}
		}
		return 0;
	}
found:
	setfbits(c, f);
	return c;
}

static struct knode *
klook(tchar c, tchar d)
{
	struct knode	*kp;
	int	h;

	c = findchar(c);
	d = findchar(d);
	h = khash(c, d);
	for (kp = ktable[h]; kp; kp = kp->next)
		if (kp->c == c && kp->d == d)
			break;
	return kp && kp->n != INT_MAX ? kp : NULL;
}

int
getkw(tchar c, tchar d)
{
	struct knode	*kp;
	struct afmtab	*a;
	int	f, g, i, j, k, n, s, I, J;
	double	z;

	if (isxfunc(c, CHAR))
		c = charout[sbits(c)].ch;
	if (isxfunc(d, CHAR))
		d = charout[sbits(d)].ch;
	lastkern = 0;
	if (!kern || iszbit(c) || iszbit(d) || ismot(c) || ismot(d))
		return 0;
	if (sbits(c) != sbits(d))
		return 0;
	f = fbits(c);
	g = fbits(d);
	if ((s = sbits(c)) == 0) {
		s = xpts;
		if (f == 0)
			f = xfont;
	}
	i = cbits(c);
	j = cbits(d);
	if (i == SLANT || j == SLANT || i == XFUNC || j == XFUNC || cstab[f])
		return 0;
	k = 0;
	if (i >= 32 && j >= 32) {
		if (ktable != NULL && (kp = klook(c, d)) != NULL)
			k = kp->n;
		else if ((n = (fontbase[f]->afmpos)-1) >= 0 &&
				n == (fontbase[g]->afmpos)-1 &&
				fontbase[f]->kernfont >= 0) {
			a = afmtab[n];
			I = i - 32;
			J = j - 32;
			if (I >= nchtab + 128)
				I -= nchtab + 128;
			if (J >= nchtab + 128)
				J -= nchtab + 128;
			k = afmgetkern(a, I, J);
			if (abs(k) < fontbase[f]->kernfont)
				k = 0;
		}
		if (j>32 && kernafter != NULL && kernafter[fbits(c)] != NULL)
			k += kernafter[fbits(c)][i];
		if (i>32 && kernbefore != NULL && kernbefore[fbits(d)] != NULL)
			k += kernbefore[fbits(d)][j];
	}
	if (k != 0) {
		k = (k * u2pts(s) + (Unitwidth / 2)) / Unitwidth;
		if (dev.anysize && xflag && (z = zoomtab[f]) != 0)
			k *= z;
	}
	lastkern = k;
	return k;
}

void
xbits(register tchar i, int bitf)
{
	register int k;

	xfont = fbits(i);
	k = sbits(i);
	if (k) {
		xpts = dev.anysize && xflag ? k : pstab[--k];
		oldbits = sfbits(i);
		pfont = xfont;
		ppts = xpts;
		return;
	}
	switch (bitf) {
	case 0:
		xfont = font;
		xpts = pts;
		break;
	case 1:
		xfont = pfont;
		xpts = ppts;
		break;
	case 2:
		xfont = mfont;
		xpts = mpts;
	}
}

static tchar
postchar1(const char *temp, int f)
{
	struct namecache	*np;
	struct afmtab	*a;
	int	i;

	if (afmtab && (i = (fontbase[f]->afmpos) - 1) >= 0) {
		a = afmtab[i];
		np = afmnamelook(a, temp);
		if (np->afpos != 0) {
			if (np->fival[0] != NOCODE &&
					fitab[f][np->fival[0]])
				return np->fival[0] + 32 + nchtab + 128;
			else if (np->fival[1] != NOCODE &&
					fitab[f][np->fival[1]])
				return np->fival[1] + 32 + nchtab + 128;
			else
				return 0;
		}
	}
	return(0);
}

static tchar
postchar(const char *temp, int *fp)
{
	int	i, j;
	tchar	c;

	*fp = font;
	if ((c = postchar1(temp, *fp)) != 0)
		return c;
	if (fallbacktab[font]) {
		for (j = 0; fallbacktab[font][j] != 0; j++) {
			if ((i = findft(fallbacktab[font][j], 0)) < 0)
				continue;
			if ((c = postchar1(temp, i)) != 0 && fchartab[c] == 0) {
				*fp = i;
				return c;
			}
		}
	}
	if (smnt) {
		for (i=smnt, j=0; j < nfonts; j++, i=i % nfonts + 1) {
			if (fontbase[i] == NULL)
				continue;
			if ((c = postchar1(temp, i)) != 0 && fchartab[c] == 0) {
				*fp = i;
				return c;
			}
		}
	}
	return 0;
}

const struct amap {
	char *alias;
	char *trname;
} amap[] = {
	{ "lq", "``" },
	{ "rq", "''" },
	{ NULL, NULL }
};

tchar
setch(int delim) {
	register int j;
	char	temp[NC];
	tchar	c, d[2] = {0, 0};
	int	f, n;
	const struct amap *ap;

	n = 0;
	if (delim == 'C')
		d[0] = getach();
	do {
		c = getach();
		if (c == 0 && n < 2)
			return(0);
		if (n >= sizeof temp) {
			temp[n-1] = 0;
			break;
		}
		if ((delim == '[' && c == ']') || (delim == 'C' && c == d[0])) {
			temp[n] = 0;
			break;
		}
		temp[n++] = c;
		if (delim == '(' && n == 2) {
			temp[n] = 0;
			break;
		}
	} while (c);
	for (ap = amap; ap->alias; ap++)
		if (!strcmp(ap->alias, temp)) {
			size_t l;
			char *s = ap->trname;
			if ((l = strlen(s) + 1) > NC) {
				fprintf(stderr, "%s %i: strlen(%s)+1 > %d\n",
				    __FILE__, __LINE__, s, NC);
				break;
			}
			memcpy(temp, s, l);
			break;
		}
	if (delim == '[' && c != ']') {
		nodelim(']');
		return ' ';
	}
	if (delim == 'C' && c != d[0]) {
		nodelim(d[0]);
		return ' ';
	}
	c = 0;
	if (delim == '[' || delim == 'C') {
		size_t l = strlen(temp);
		if (gemu) {
			if (l == 5 && *temp == 'u'
			    && isxdigit((unsigned)temp[1])
			    && isxdigit((unsigned)temp[2])
			    && isxdigit((unsigned)temp[3])
			    && isxdigit((unsigned)temp[4])) {
				int n;
				n = strtol(temp + 1, NULL, 16);
				if (n)
					c = setuc0(n);
			} else if ((l == 6 || (l == 7
			    && isdigit((unsigned)temp[6])))
			    && isdigit((unsigned)temp[5])
			    && isdigit((unsigned)temp[4])
			    && !strncmp(temp, "char", 4)) {
				int i = atoi(temp + 4);
				if (i <= 127)
					c = i + nchtab + 128;
			}
		}
		if (!c && (c = postchar(temp, &f))) {
			c |= chbits & ~FMASK;
			setfbits(c, f);
		}
	}
	if (c == 0)
		for (j = 0; j < nchtab; j++)
			if (strcmp(&chname[chtab[j]], temp) == 0) {
				c = (j + 128) | chbits;
				break;
			}
	if (c == 0 && delim == '(')
		if ((c = postchar(temp, &f)) != 0) {
			c |= chbits & ~FMASK;
			setfbits(c, f);
		}
	if (c == 0 && (c = _ps2cc(temp, 0)) != 0) {
		c += nchtab + 128 + 32 + 128 - 32 + nchtab;
		if (chartab[c] == NULL)
			c = 0;
	}
	if (c == 0 && warn & WARN_CHAR)
		errprint("missing glyph \\%c%s%s%s%s", delim, d, temp, d,
				delim == '[' ? "]" : "");
	if (c == 0 && !tryglf)
		c = ' ';
	return c;
}

tchar setabs(void)		/* set absolute char from \C'...' */
{
	int n;

	getch();
	n = 0;
	n = inumb(&n);
	getch();
	if (nonumb || n + nchtab + 128 >= NCHARS)
		return 0;
	return n + nchtab + 128;
}



int
findft(register int i, int required)
{
	register int k;
	int nk;
	char	*mn, *mp;

	if ((k = i - '0') >= 0 && k <= nfonts && k < smnt && fontbase[k])
		return(k);
	for (k = 0; k > nfonts || fontlab[k] != i; k++)
		if (k > nfonts) {
			mn = macname(i);
			nk = k;
			if ((k = strtol(mn, &mp, 10)) >= 0 && *mp == 0 &&
					mp > mn && k <= nfonts && fontbase[k])
				break;
			if (setfp(nk, i, NULL) == -1)
				return -1;
			else {
				fontlab[nk] = i;
				return nk;
			}
			if (required && warn & WARN_FONT)
				errprint("%s: no such font", mn);
			return(-1);
		}
	return(k);
}

void
caseps(void)
{
	register int i;

	if (skip(0))
		i = apts1;
	else {
		if (xflag == 0) {
			noscale++;
			apts = u2pts(apts);
		} else {
			dfact = INCH;
			dfactd = 72;
			res = VERT;
		}
		i = inumb(&apts);
		if (xflag == 0) {
			noscale--;
			i = pts2u(i);
			apts = pts2u(apts);
		}
		if (nonumb)
			return;
	}
	casps1(i);
}

void
casps1(register int i)
{

/*
 * in olden times, it used to ignore changes to 0 or negative.
 * this is meant to allow the requested size to be anything,
 * in particular so eqn can generate lots of \s-3's and still
 * get back by matching \s+3's.

	if (i <= 0)
		return;
*/
	apts1 = apts;
	apts = i;
	pts1 = pts;
	pts = findps(i);
	mchbits();
}

int
findps(register int i)
{
	register int j, k;

	if (dev.anysize && xflag) {
		if (i <= 0)
			i = pstab[0];
		return i;
	}
	for (j=k=0 ; pstab[j] != 0 ; j++)
		if (abs(pstab[j]-i) < abs(pstab[k]-i))
			k = j;

	return(pstab[k]);
}

void
mchbits(void)
{
	register int i, j, k;

	i = pts;
	if (dev.anysize && xflag)
		j = i - 1;
	else for (j = 0; i > (k = pstab[j]); j++)
		if (!k) {
			k = pstab[--j];
			break;
		}
	chbits = 0;
	setsbits(chbits, ++j);
	setfbits(chbits, font);
	zapwcache(1);
	if (minspsz) {
		k = spacesz;
		spacesz = minspsz;
		minsps = width(' ' | chbits);
		spacesz = k;
		zapwcache(1);
	}
	if (letspsz) {
		k = spacesz;
		spacesz = letspsz;
		letsps = width(' ' | chbits);
		spacesz = k;
		zapwcache(1);
	}
	k = spacesz;
	spacesz = sesspsz;
	ses = width(' ' | chbits);
	spacesz = k;
	zapwcache(1);
	sps = width(' ' | chbits);
	zapwcache(1);
}

void
setps(void)
{
	tchar	c;
	register int i, j = 0;
	int	k;

	i = cbits(c = getch());
	if (ismot(c) && xflag)
		return;
	if (ischar(i) && isdigit(i)) {		/* \sd or \sdd */
		i -= '0';
		if (i == 0)		/* \s0 */
			j = apts1;
		else if (i <= 3 && ischar(j = cbits(ch = getch())) &&
		    isdigit(j)) {	/* \sdd */
			j = 10 * i + j - '0';
			ch = 0;
			j = pts2u(j);
		} else		/* \sd */
			j = pts2u(i);
	} else if (i == '(') {		/* \s(dd */
		j = cbits(getch()) - '0';
		j = 10 * j + cbits(getch()) - '0';
		if (j == 0)		/* \s(00 */
			j = apts1;
		else
			j = pts2u(j);
	} else if (i == '+' || i == '-') {	/* \s+, \s- */
		j = cbits(c = getch());
		if (ischar(j) && isdigit(j)) {		/* \s+d, \s-d */
			j -= '0';
			j = pts2u(j);
		} else if (j == '(') {		/* \s+(dd, \s-(dd */
			j = cbits(getch()) - '0';
			j = 10 * j + cbits(getch()) - '0';
			j = pts2u(j);
		} else if (xflag) {	/* \s+[dd], */
			k = j == '[' ? ']' : j;			/* \s-'dd' */
			setcbits(c, k);
			dfact = INCH;
			dfactd = 72;
			res = HOR;
			j = hatoi();
			res = dfactd = dfact = 1;
			if (nonumb)
				return;
			if (!issame(getch(), c))
				nodelim(k);
		}
		if (i == '-')
			j = -j;
		j += apts;
	} else if (xflag) {	/* \s'+dd', \s[dd] */
		if (i == '[') {
			i = ']';
			setcbits(c, i);
		}
		dfact = INCH;
		dfactd = 72;
		res = HOR;
		j = inumb2(&apts, &k);
		if (nonumb)
			return;
		if (j == 0 && k == 0)
			j = apts1;
		if (!issame(getch(), c))
			nodelim(i);
	}
	casps1(j);
}


tchar setht(void)		/* set character height from \H'...' */
{
	int n;
	tchar c;

	getch();
	dfact = INCH;
	dfactd = 72;
	res = VERT;
	n = inumb(&apts);
	getch();
	if (n == 0 || nonumb)
		n = apts;	/* does this work? */
	c = CHARHT;
	c |= ZBIT;
	setfbits(c, font);
	setsbits(c, n);
	return(c);
}

tchar setslant(void)		/* set slant from \S'...' */
{
	int n;
	tchar c;

	getch();
	n = 0;
	n = inumb(&n);
	getch();
	if (nonumb)
		n = 0;
	c = SLANT;
	c |= ZBIT;
	setfbits(c, font);
	setsbits(c, n+180);
	return(c);
}

void
caseft(void)
{
	skip(0);
	setfont(1);
}

void
setfont(int a)
{
	register int i, j;

	if (a)
		i = getrq(3);
	else 
		i = getsn(1);
	if (!i || i == 'P') {
		j = font1;
		goto s0;
	}
	if (/* i == 'S' || */ i == '0')
		return;
	if ((j = findft(i, 0)) == -1)
		if ((j = setfp(0, i, 0)) == -1) { /* try to put it in position 0 */
			if (xflag) {
				font1 = font;
			}
			return;
		}
s0:
	font1 = font;
	font = j;
	mchbits();
}

void
setwd(void)
{
	register int base, wid;
	register tchar i;
	tchar	delim, lasti = 0;
	int	emsz, k;
	int	savhp, savapts, savapts1, savfont, savfont1, savpts, savpts1;
	int	savlgf;
	int	rst = 0, rsb = 0;
	int	n;

	base = numtab[SB].val = numtab[ST].val = wid = numtab[CT].val = 0;
	if (ismot(i = getch()))
		return;
	delim = i;
	argdelim = delim;
	n = noschr;
	noschr = 0;
	savhp = numtab[HP].val;
	numtab[HP].val = 0;
	savapts = apts;
	savapts1 = apts1;
	savfont = font;
	savfont1 = font1;
	savpts = pts;
	savpts1 = pts1;
	savlgf = lgf;
	lgf = 0;
	setwdf++;
	while (i = getch(), !issame(i, delim) && !nlflg) {
		k = width(i);
		k += kernadjust(lasti, i);
		lasti = i;
		wid += k;
		numtab[HP].val += k;
		if (!ismot(i)) {
			emsz = POINT * u2pts(xpts);
		} else if (isvmot(i)) {
			k = absmot(i);
			if (isnmot(i))
				k = -k;
			base -= k;
			emsz = 0;
		} else 
			continue;
		if (base < numtab[SB].val)
			numtab[SB].val = base;
		if ((k = base + emsz) > numtab[ST].val)
			numtab[ST].val = k;
		if (lastrst > rst)
			rst = lastrst;
		if (lastrsb < rsb)
			rsb = lastrsb;
	}
	if (!issame(i, delim))
		nodelim(delim);
	argdelim = 0;
	noschr = n;
	setn1(wid, 0, (tchar) 0);
	prwatchn(&numtab[CT]);
	prwatchn(&numtab[SB]);
	prwatchn(&numtab[ST]);
	setnr("rst", rst, 0);
	setnr("rsb", rsb, 0);
	numtab[HP].val = savhp;
	apts = savapts;
	apts1 = savapts1;
	font = savfont;
	font1 = savfont1;
	pts = savpts;
	pts1 = savpts1;
	lgf = savlgf;
	mchbits();
	setwdf = 0;
}


tchar vmot(void)
{
	dfact = lss;
	vflag++;
	return(mot());
}


tchar hmot(void)
{
	dfact = EM;
	return(mot());
}


tchar mot(void)
{
	register int j, n;
	register tchar i;
	tchar c, delim;

	j = HOR;
	delim = getch(); /*eat delim*/
	if ((n = hatoi())) {
		if (vflag)
			j = VERT;
		i = makem(quant(n, j));
	} else
		i = 0;
	c = getch();
	if (!issame(c, delim))
		nodelim(delim);
	vflag = 0;
	dfact = 1;
	return(i);
}


tchar sethl(int k)
{
	register int j;
	tchar i;

	j = EM / 2;
	if (k == 'u')
		j = -j;
	else if (k == 'r')
		j = -2 * j;
	vflag++;
	i = makem(j);
	vflag = 0;
	return(i);
}


tchar makem(register int i)
{
	register tchar j;

	if ((j = i) < 0)
		j = -j;
	j = sabsmot(j) | MOT;
	if (i < 0)
		j |= NMOT;
	if (vflag)
		j |= VMOT;
	return(j);
}


tchar getlg(tchar i)
{
	tchar j, k, pb[NC];
	struct lgtab *lp;
	int c, f, n, lgn;

	f = fbits(i);
	if (lgtab[f] == NULL)	/* font lacks ligatures */
		return(i);
	c = cbits(i);
	lp = &lgtab[f][c];
	if (lp->from != c || (lp = lp->link) == NULL)
		return(i);
	k = i;
	n = 1;
	lgn = lg == 2 ? 2 : 1000;
	for (;;) {
		j = getch0();
		if (n < sizeof pb)
			pb[n-1] = j;
		c = cbits(j);
		while (lp != NULL && lp->from != c)
			lp = lp->next;
		if (lp == NULL || lp->to == 0) {
			pbbuf[pbp++] = j;
			return(k);
		}
		if (lp->to == -1) {	/* fdeferlig request */
			pb[n < sizeof pb ? n : sizeof pb - 1] = 0;
			pushback(pb);
			return(i);
		}
		k = (i & SFMASK) | lp->to | AUTOLIG;
		if (lp->link == NULL || ++n > lgn)
			return(k);
		lp = lp->link;
	}
}

int
strlg(int f, int *tp, int n)
{
	struct lgtab	*lp;
	int	i;

	if (n == 1)
		return tp[0];
	if (lgtab[f] == NULL)
		return 0;
	lp = &lgtab[f][tp[0]];
	if (lp->from != tp[0])
		return 0;
	for (i = 1; i < n; i++) {
		if ((lp = lp->link) == NULL)
			return 0;
		while (lp != NULL && lp->from != tp[i])
			lp = lp->next;
		if (lp == NULL || lp->to == 0)
			return 0;
	}
	return lp->to > 0 ? lp->to : 0;
}

void
caselg(void)
{

	lg = 1;
	if (skip(0))
		return;
	lg = hatoi();
}

static void
addlig(int f, tchar *from, int to)
{
	int	i, j;
	struct lgtab	*lp;

	if (from[0] == 0 || from[1] == 0) {
		if (warn & WARN_FONT)
			errprint("short ligature has no effect");
		return;
	}
	if (lgtab[f] == NULL)
		lgtab[f] = calloc(NCHARS, sizeof **lgtab);
	i = cbits(from[0]);
	gchtab[i] |= LGBIT;
	lp = &lgtab[f][i];
	lp->from = i;
	j = 1;
	for (;;) {
		i = cbits(from[j]);
		if (lp->link == NULL) {
			if (from[j+1] != 0) {
				if (warn & WARN_FONT)
					errprint("ligature step missing");
				return;
			}
			lp->link = calloc(1, sizeof *lp->link);
			lp = lp->link;
			lp->from = i;
			lp->to = to;
			break;
		}
		lp = lp->link;
		if (++j >= 4) {
			if (warn & WARN_FONT)
				errprint("ignoring ligature of length >4");
			return;
		}
		while (lp->from != i && lp->next)
			lp = lp->next;
		if (lp->from != i) {
			if (from[j] != 0) {
				if (warn & WARN_FONT)
					errprint("ligature step missing");
				return;
			}
			lp->next = calloc(1, sizeof *lp->next);
			lp = lp->next;
			lp->from = i;
		}
		if (from[j] == 0) {
			lp->to = to;
			break;
		}
	}
	if (to >= 0) {
		if (lgrevtab[f] == NULL)
			lgrevtab[f] = calloc(NCHARS, sizeof **lgrevtab);
		lgrevtab[f][to] = malloc((j+2) * sizeof ***lgrevtab);
		j = 0;
		while ((lgrevtab[f][to][j] = cbits(from[j])))
			j++;
	}
	/*
	 * If the font still contains the charlib substitutes for ff,
	 * Fi, and Fl, hide them. The ".flig" request is intended for
	 * use in combination with expert fonts only.
	 */
	if ((to == LIG_FF || (cbits(from[0]) == 'f' && cbits(from[1]) == 'f' &&
			      cbits(from[2]) == 0)) &&
			fitab[f][LIG_FF-32] != NOCODE)
		if (codetab[f][fitab[f][LIG_FF-32]] < 32)
			fitab[f][LIG_FF-32] = 0;
	if ((to == LIG_FFI || (cbits(from[0]) == 'f' && cbits(from[1]) == 'f' &&
			       cbits(from[2]) == 'i' && cbits(from[3]) == 0)) &&
			fitab[f][LIG_FFI-32] != NOCODE)
		if (codetab[f][fitab[f][LIG_FFI-32]] < 32)
			fitab[f][LIG_FFI-32] = 0;
	if ((to == LIG_FFL || (cbits(from[0]) == 'f' && cbits(from[1]) == 'f' &&
			       cbits(from[2]) == 'l' && cbits(from[3]) == 0)) &&
			fitab[f][LIG_FFL-32] != NOCODE)
		if (codetab[f][fitab[f][LIG_FFL-32]] < 32)
			fitab[f][LIG_FFL-32] = 0;
}

static void
dellig(int f, tchar *from)
{
	struct lgtab	*lp, *lq;
	int	i, j;

	if (from[0] == 0 || from[1] == 0)
		return;
	if (lgtab[f] == NULL)
		return;
	i = cbits(from[0]);
	lp = lq = &lgtab[f][i];
	j = 1;
	for (;;) {
		i = cbits(from[j]);
		if (lp->link == NULL)
			break;
		lq = lp;
		lp = lp->link;
		while (lp->from != i && lp->next) {
			lq = lp;
			lp = lp->next;
		}
		if (lp->from != i)
			break;
		if (from[++j] == 0) {
			if (lq->link == lp)
				lq->link = lp->next;
			else if (lq->next == lp)
				lq->next = lp->next;
			if (lp->link)
				if (warn & WARN_FONT)
					errprint("deleted ligature cuts chain");
			free(lgrevtab[f][lp->to]);
			lgrevtab[f][lp->to] = NULL;
			free(lp);
			break;
		}
	}
}

void
setlig(int f, int j)
{
	tchar	from[4], to;

	free(lgrevtab[f]);
	lgrevtab[f] = NULL;
	free(lgtab[f]);
	lgtab[f] = NULL;
	from[0] = 'f';
	from[2] = from[3] = 0;
	from[1] = 'f';
	to = LIG_FF;
	if (j & LFF)
		addlig(f, from, to);
	from[1] = 'i';
	to = LIG_FI;
	if (j & LFI)
		addlig(f, from, to);
	from[1] = 'l';
	to = LIG_FL;
	if (j & LFL)
		addlig(f, from, to);
	from[1] = 'f';
	from[2] = 'i';
	to = LIG_FFI;
	if (j & LFFI)
		addlig(f, from, to);
	from[2] = 'l';
	to = LIG_FFL;
	if (j & LFFL)
		addlig(f, from, to);
}

static int
getflig(int f, int mode)
{
	int	delete, allnum;
	tchar	from[NC], to;
	int	c, i, j;
	char	number[NC];

	if (skip(0))
		return 0;
	switch (cbits(c = getch())) {
	case '-':
		c = getch();
		delete = 1;
		break;
	case '+':
		c = getch();
		/*FALLTHRU*/
	default:
		delete = 0;
		break;
	case 0:
		return 0;
	}
	allnum = 1;
	for (i = 0; i < sizeof from - 2; i++) {
		from[i] = c;
		j = cbits(c);
		if (c == 0 || ismot(c) || j == ' ' || j == '\n') {
			from[i] = 0;
			ch = j;
			break;
		}
		if (j < '0' || j > '9')
			allnum = 0;
		c = getch();
	}
	if (mode == 0 && allnum == 1) {	/* backwards compatibility */
		if (skip(0) == 0)
			goto new;
		for (j = 0; j <= i+1; j++)
			number[j] = cbits(from[j]);
		j = strtol(number, NULL, 10);
		setlig(f, j);
		return 0;
	}
	if (delete == 0) {
		if (mode >= 0) {
			if (skip(1))
				return 0;
		new:	to = cbits(getch());
		} else
			to = -1;
		addlig(f, from, to);
	} else
		dellig(f, from);
	return 1;
}

void
caseflig(int defer)
{
	int	i, j;

	lgf++;
	if (skip(1))
		return;
	i = getrq(2);
	if ((j = findft(i, 1)) < 0)
		return;
	i = 0;
	while (getflig(j, defer ? -1 : i++) != 0);
}

void
casefdeferlig(void)
{
	caseflig(1);
}

void
casefp(int spec)
{
	register int i, j;
	char *file, *supply;

	lgf++;
	skip(0);
	if ((i = xflag ? hatoi() : cbits(getch()) - '0') < 0 || i > 255)
	bad:	errprint("fp: bad font position %d", i);
	else if (skip(0) || !(j = getrq(3)))
		errprint("fp: no font name");
	else {
		if (skip(0) || !getname()) {
			if (i == 0)
				goto bad;
			setfp(i, j, 0);
		} else {		/* 3rd argument = filename */
			size_t l;
			l = strlen(nextf) + 1;
			file = malloc(l);
			n_strcpy(file, nextf, l);
			if (!skip(0) && getname()) {
				l = strlen(nextf) + 1;
				supply = malloc(l);
				n_strcpy(supply, nextf, l);
			} else
				supply = NULL;
			if (loadafm(i?i:-1, j, file, supply, 0, spec) == 0) {
				if (i == 0) {
					if (warn & WARN_FONT)
						errprint("fp: cannot mount %s",
								file);
				} else
					setfp(i, j, file);
			}
			free(file);
			free(supply);
		}
	}
}

void
casefps(void)
{
	const struct {
		enum spec	spec;
		const char	*name;
	} tab[] = {
		{ SPEC_MATH,	"math" },
		{ SPEC_GREEK,	"greek" },
		{ SPEC_PUNCT,	"punct" },
		{ SPEC_LARGE,	"large" },
		{ SPEC_S1,	"S1" },
		{ SPEC_S,	"S" },
		{ SPEC_NONE,	NULL }
	};
	char	name[NC];
	int	c = 0, i;
	enum spec	s = SPEC_NONE;

	if (skip(1))
		return;
	do {
		for (i = 0; i < sizeof name - 2; i++) {
			if ((c = getach()) == 0 || c == ':' || c == ',')
				break;
			name[i] = c;
		}
		name[i] = 0;
		for (i = 0; tab[i].name; i++)
			if (strcmp(tab[i].name, name) == 0) {
				s |= tab[i].spec;
				break;
			}
		if (tab[i].name == NULL)
			errprint("fps: unknown special set %s", name);
	} while (c);
	casefp(s);
}

int
setfp(int pos, int f, char *truename)	/* mount font f at position pos[0...nfonts] */
{
	char longname[4096], *shortname, *ap;
	char *fpout;
	int i, nw;

	zapwcache(0);
	if (truename)
		shortname = truename;
	else
		shortname = macname(f);
	shortname = mapft(shortname);
	snprintf(longname, sizeof longname, "%s/dev%s/%s",
			fontfile, devname, shortname);
	if ((fpout = readfont(longname, &dev, warn & WARN_FONT)) == NULL)
		return(-1);
	if (pos >= Nfont)
		growfonts(pos+1);
	if (pos > nfonts)
		nfonts = pos;
	fontbase[pos] = (struct Font *)fpout;
	if ((ap = strstr(fontbase[pos]->namefont, ".afm")) != NULL) {
		*ap = 0;
		if (ap == &fontbase[pos]->namefont[1])
			f &= BYTEMASK;
		loadafm(pos, f, fontbase[pos]->namefont, NULL, 1, SPEC_NONE);
		free(fpout);
	} else {
		nw = fontbase[pos]->nwfont & BYTEMASK;
		makefont(pos, &((char *)fontbase[pos])[sizeof(struct Font)],
			&((char *)fontbase[pos])[sizeof(struct Font) + nw],
			&((char *)fontbase[pos])[sizeof(struct Font) + 2*nw],
			&((char *)fontbase[pos])[sizeof(struct Font) + 3*nw],
			nw);
		setlig(pos, fontbase[pos]->ligfont);
	}
	if (pos == smnt) {
		smnt = 0; 
		sbold = 0; 
	}
	if ((fontlab[pos] = f) == 'S')
		smnt = pos;
	bdtab[pos] = cstab[pos] = ccstab[pos] = 0;
	zoomtab[pos] = 0;
	fallbacktab[pos] = NULL;
	free(lhangtab[pos]);
	lhangtab[pos] = NULL;
	free(rhangtab[pos]);
	rhangtab[pos] = NULL;
	memset(&tracktab[pos], 0, sizeof tracktab[pos]);
	for (i = 0; i < NCHARS; i++)
		ftrtab[pos][i] = i;
	kzap(pos);
		/* if there is a directory, no place to store its name. */
		/* if position isn't zero, no place to store its value. */
		/* only time a FONTPOS is pushed back is if it's a */
		/* standard font on position 0 (i.e., mounted implicitly. */
		/* there's a bug here:  if there are several input lines */
		/* that look like .ft XX in short successtion, the output */
		/* will all be in the last one because the "x font ..." */
		/* comes out too soon.  pushing back FONTPOS doesn't work */
		/* with .ft commands because input is flushed after .xx cmds */
	if (realpage && ap == NULL)
		ptfpcmd(pos, shortname, NULL, 0);
	if (pos == 0)
		ch = (tchar) FONTPOS | (tchar) f << 22;
	return(pos);
}

int
nextfp(void)
{
	int	i;

	for (i = 1; i <= nfonts; i++)
		if (fontbase[i] == NULL)
			return i;
	if (i <= 255)
		return i;
	return 0;
}

void
casecs(void)
{
	register int i, j;

	noscale++;
	if (skip(1))
		goto rtn;
	if (!(i = getrq(2)))
		goto rtn;
	if ((i = findft(i, 1)) < 0)
		goto rtn;
	skip(0);
	cstab[i] = hatoi();
	skip(0);
	j = hatoi();
	if (nonumb)
		ccstab[i] = 0;
	else
		ccstab[i] = findps(j);
rtn:
	zapwcache(0);
	noscale = 0;
}

void
casebd(void)
{
	register int i, j = 0, k;

	zapwcache(0);
	k = 0;
bd0:
	if (skip(1) || !(i = getrq(2)) || (j = findft(i, 1)) == -1) {
		if (k)
			goto bd1;
		else 
			return;
	}
	if (j == smnt) {
		k = smnt;
		goto bd0;
	}
	if (k) {
		sbold = j;
		j = k;
	}
bd1:
	skip(0);
	noscale++;
	bdtab[j] = hatoi();
	noscale = 0;
}

void
casevs(void)
{
	register int i;

	skip(0);
	vflag++;
	dfact = INCH; /* default scaling is points! */
	dfactd = 72;
	res = VERT;
	i = inumb(&lss);
	if (nonumb)
		i = lss1;
	if (xflag && i < 0) {
		if (warn & WARN_RANGE)
			errprint("negative vertical spacing ignored");
		i = lss1;
	}
	if (i < VERT) 
		i = VERT;
	lss1 = lss;
	lss = i;
}

void
casess(int flg)
{
	register int i, j;

	noscale++;
	if (skip(flg == 0))
		minsps = minspsz = 0;
	else if ((i = hatoi()) != 0 && !nonumb) {
		if (xflag && flg == 0 && !skip(0)) {
			j = hatoi();
			if (!nonumb) {
				sesspsz = j & 0177;
				spacesz = sesspsz;
				zapwcache(1);
				ses = width(' ' | chbits);
			}
		}
		if (flg) {
			j = spacesz;
			minspsz = i & 0177;
			spacesz = minspsz;
			zapwcache(1);
			minsps = width(' ' | chbits);
			spacesz = j;
			zapwcache(0);
			sps = width(' ' | chbits);
		} else {
			spacesz = i & 0177;
			zapwcache(0);
			sps = width(' ' | chbits);
			if (minspsz > spacesz)
				minsps = minspsz = 0;
		}
	}
	noscale = 0;
}

void
caseminss(void)
{
	casess(1);
}

void
caseletadj(void)
{
	int	s, n, x, l, h;

	dfact = LAFACT / 100;
	if (skip(0) || (n = hatoi()) == 0) {
		letspsz = 0;
		letsps = 0;
		lspmin = 0;
		lspmax = 0;
		lshmin = 0;
		lshmax = 0;
		goto ret;
	}
	if (skip(1))
		goto ret;
	dfact = LAFACT / 100;
	l = hatoi();
	if (skip(1))
		goto ret;
	noscale++;
	s = hatoi();
	noscale--;
	if (skip(1))
		goto ret;
	dfact = LAFACT / 100;
	x = hatoi();
	if (skip(1))
		goto ret;
	dfact = LAFACT / 100;
	h = hatoi();
	letspsz = s;
	lspmin = LAFACT - n;
	lspmax = x - LAFACT;
	lshmin = LAFACT - l;
	lshmax = h - LAFACT;
	s = spacesz;
	spacesz = letspsz;
	zapwcache(1);
	letsps = width(' ' | chbits);
	spacesz = s;
	zapwcache(1);
	sps = width(' ' | chbits);
	zapwcache(1);
ret:
	dfact = 1;
}

void
casefspacewidth(void)
{
	int	f, n, i;

	lgf++;
	if (skip(1))
		return;
	i = getrq(2);
	if ((f = findft(i, 1)) < 0)
		return;
	if (skip(0)) {
		fontbase[f]->cspacewidth = -1;
		fontab[f][0] = fontbase[f]->spacewidth;
	} else {
		noscale++;
		n = hatoi();
		noscale--;
		unitsPerEm = 1000;
		if (n >= 0)
			fontbase[f]->cspacewidth = fontab[f][0] = _unitconv(n);
		else if (warn & WARN_RANGE)
			errprint("ignoring negative space width %d", n);
	}
	zapwcache(1);
}

void
casespacewidth(void)
{
	noscale++;
	spacewidth = skip(0) || hatoi();
	noscale--;
}

tchar xlss(void)
{
	/* stores \x'...' into
	 * two successive tchars.
	 * the first contains HX, the second the value,
	 * encoded as a vertical motion.
	 * decoding is done in n2.c by pchar().
	 */
	int	i;

	getch();
	dfact = lss;
	i = quant(hatoi(), VERT);
	dfact = 1;
	getch();
	if (i >= 0)
		pbbuf[pbp++] = MOT | VMOT | sabsmot(i);
	else
		pbbuf[pbp++] = MOT | VMOT | NMOT | sabsmot(-i);
	return(HX);
}

struct afmtab **afmtab;
int nafm;

char *
onefont(char *prefix, char *file, char *type)
{
	char	*path, *fp, *tp;
	size_t	l;

	l = strlen(prefix) + strlen(file) + 2;
	path = malloc(l);
	n_strcpy(path, prefix, l);
	n_strcat(path, "/", l);
	n_strcat(path, file, l);
	if (type) {
		for (fp = file; *fp; fp++);
		for (tp = type; *tp; tp++);
		while (tp >= type && fp >= file && *fp-- == *tp--);
		if (tp >= type) {
			l = strlen(path) + strlen(type) + 2;
			tp = malloc(l);
			n_strcpy(tp, path, l);
			n_strcat(tp, ".", l);
			n_strcat(tp, type, l);
			free(path);
			path = tp;
		}
	}
	return path;
}

static char *
getfontpath(char *file, char *type)
{
	char	*path, *troffonts, *tp, *tq, c;
	size_t	l;

	if ((troffonts = getenv("TROFFONTS")) != NULL) {
		l = strlen(troffonts) + 1;
		tp = malloc(l);
		n_strcpy(tp, troffonts, l);
		troffonts = tp;
		do {
			for (tq = tp; *tq && *tq != ':'; tq++);
			c = *tq;
			*tq = 0;
			path = onefont(tp, file, type);
			if (access(path, 0) == 0) {
				free(troffonts);
				return path;
			}
			free(path);
			tp = &tq[1];
		} while (c);
		free(troffonts);
	}
	l = strlen(fontfile) + strlen(devname) + 10;
	tp = malloc(l);
	snprintf(tp, l, "%s/dev%s", fontfile, devname);
	path = onefont(tp, file, type);
	free(tp);
	return path;
}

static void
checkenminus(int f)
{
	/*
	 * A fix for a very special case: If the font supplies punctuation
	 * characters but is not S1, only one of \- and \(en is present
	 * since the PostScript character "endash" is mapped to both of
	 * them.
	 */
	enum spec	spec;
	int	i;

	if (afmtab == NULL || (i = fontbase[f]->afmpos - 1) < 0)
		return;
	if (c_endash == 0 || c_minus == 0)
		specnames();
	spec = afmtab[i]->spec;
	if ((spec&(SPEC_PUNCT|SPEC_S1)) == SPEC_PUNCT) {
		if (fitab[f][c_endash-32] == 0 && ftrtab[f][c_minus-32])
			ftrtab[f][c_endash] = c_minus;
		else if (fitab[f][c_endash-32] && ftrtab[f][c_minus-32] != 0)
			ftrtab[f][c_minus] = c_endash;
	}
}

int
loadafm(int nf, int rq, char *file, char *supply, int required, enum spec spec)
{
	struct stat	st;
	int	fd;
	char	*path, *contents;
	struct afmtab	*a;
	int	i, have = 0;
	struct namecache	*np;
	size_t	l;

	zapwcache(0);
	if (nf < 0)
		nf = nextfp();
	path = getfontpath(file, "afm");
	if (access(path, 0) < 0) {
		path = getfontpath(file, "otf");
		if (access(path, 0) < 0)
			path = getfontpath(file, "ttf");
	}
	if (dev.allpunct)
		spec |= SPEC_PUNCT;
	a = calloc(1, sizeof *a);
	for (i = 0; i < nafm; i++)
		if (strcmp(afmtab[i]->path, path) == 0 &&
				afmtab[i]->spec == spec) {
			*a = *afmtab[i];
			have = 1;
			break;
		}
	a->path = path;
	l = strlen(file) + 1;
	a->file = malloc(l);
	n_strcpy(a->file, file, l);
	a->spec = spec;
	a->rq = rq;
	a->Font.namefont[0] = rq&0377;
	a->Font.namefont[1] = (rq>>8)&0377;
	snprintf(a->Font.intname, sizeof(a->Font.intname), "%d", nf);
	if (have)
		goto done;
	if ((fd = open(path, O_RDONLY)) < 0) {
		if (required)
			errprint("Can't open %s", path);
		free(a->file);
		free(a);
		free(path);
		return 0;
	}
	if (fstat(fd, &st) < 0) {
		errprint("Can't stat %s", path);
		free(a->file);
		free(a);
		free(path);
		return -1;
	}
	contents = malloc(st.st_size + 1);
	if (read(fd, contents, st.st_size) != st.st_size) {
		errprint("Can't read %s", path);
		free(a->file);
		free(a);
		free(path);
		free(contents);
		return -1;
	}
	contents[st.st_size] = 0;
	close(fd);
	if (afmget(a, contents, st.st_size) < 0) {
		free(path);
		free(contents);
		return -1;
	}
	free(contents);
	morechars(a->nchars+32+1+128-32+nchtab+32+nchtab+128+psmaxcode+1);
done:	afmtab = realloc(afmtab, (nafm+1) * sizeof *afmtab);
	afmtab[nafm] = a;
	if (nf >= Nfont)
		growfonts(nf+1);
	a->Font.afmpos = nafm+1;
	if ((np = afmnamelook(a, "space")) != NULL)
		a->Font.spacewidth = a->fontab[np->afpos];
	else
		a->Font.spacewidth = a->fontab[0];
	a->Font.cspacewidth = -1;
	fontbase[nf] = &afmtab[nafm]->Font;
	fontlab[nf] = rq;
	free(fontab[nf]);
	free(kerntab[nf]);
	free(codetab[nf]);
	free(fitab[nf]);
	fontab[nf] = malloc(a->nchars * sizeof *fontab[nf]);
	kerntab[nf] = malloc(a->nchars * sizeof *kerntab[nf]);
	codetab[nf] = malloc(a->nchars * sizeof *codetab[nf]);
	fitab[nf] = calloc(NCHARS, sizeof *fitab[nf]);
	memcpy(fontab[nf], a->fontab, a->nchars * sizeof *fontab[nf]);
	memcpy(kerntab[nf], a->kerntab, a->nchars * sizeof *kerntab[nf]);
	memcpy(codetab[nf], a->codetab, a->nchars * sizeof *codetab[nf]);
	memcpy(fitab[nf], a->fitab, a->fichars * sizeof *fitab[nf]);
	bdtab[nf] = cstab[nf] = ccstab[nf] = 0;
	zoomtab[nf] = 0;
	fallbacktab[nf] = NULL;
	free(lhangtab[nf]);
	lhangtab[nf] = NULL;
	free(rhangtab[nf]);
	rhangtab[nf] = NULL;
	memset(&tracktab[nf], 0, sizeof tracktab[nf]);
	setlig(nf, a->Font.ligfont);
	for (i = 0; i < NCHARS; i++)
		ftrtab[nf][i] = i;
	kzap(nf);
	nafm++;
	if (nf > nfonts)
		nfonts = nf;
	if (supply) {
		char	*data;
		if (strcmp(supply, "pfb") == 0 || strcmp(supply, "pfa") == 0 ||
				strcmp(supply, "t42") == 0 ||
				strcmp(supply, "otf") == 0 ||
				strcmp(supply, "ttf") == 0)
			data = getfontpath(file, supply);
		else
			data = getfontpath(supply, NULL);
		a->supply = afmencodepath(data);
		free(data);
		if (realpage)
			ptsupplyfont(a->fontname, a->supply);
	}
	checkenminus(nf);
	if (realpage)
		ptfpcmd(nf, macname(fontlab[nf]), a->path, (int)a->spec);
	return 1;
}

int
tracknum(void)
{
	skip(1);
	dfact = INCH;
	dfactd = 72;
	res = VERT;
	return inumb(NULL);
}

void
casetrack(void)
{
	int	i, j, s1, n1, s2, n2;

	if (skip(1))
		return;
	i = getrq(2);
	if ((j = findft(i, 1)) < 0)
		return;
	s1 = tracknum();
	if (!nonumb) {
		n1 = tracknum();
		if (!nonumb) {
			s2 = tracknum();
			if (!nonumb) {
				n2 = tracknum();
				if (!nonumb) {
					tracktab[j].s1 = s1;
					tracktab[j].n1 = n1;
					tracktab[j].s2 = s2;
					tracktab[j].n2 = n2;
					zapwcache(0);
				}
			}
		}
	}
}

void
casefallback(void)
{
	int	*fb = NULL;
	int	i, j, n = 0;

	if (skip(1))
		return;
	i = getrq(2);
	if ((j = findft(i, 1)) < 0)
		return;
	do {
		skip(0);
		i = getrq(2);
		fb = realloc(fb, (n+2) * sizeof *fb);
		fb[n++] = i;
	} while (i);
	fallbacktab[j] = fb;
}

void
casehidechar(void)
{
	int	savfont = font, savfont1 = font1;
	int	i, j;
	tchar	k;

	if (skip(1))
		return;
	i = getrq(2);
	if ((j = findft(i, 1)) < 0)
		return;
	font = font1 = j;
	mchbits();
	while ((i = cbits(k = getch())) != '\n') {
		if (fbits(k) != j || ismot(k) || i == ' ')
			continue;
		if (i >= nchtab + 128-32 && afmtab &&
				fontbase[j]->afmpos - 1 >= 0)
			i -= nchtab + 128;
		fitab[j][i - 32] = 0;
	}
	font = savfont;
	font1 = savfont1;
	mchbits();
	zapwcache(0);
}

void
casefzoom(void)
{
	int	i, j;
	float	f;

	if (skip(1))
		return;
	i = getrq(2);
	if ((j = findft(i, 1)) < 0)
		return;
	skip(1);
	f = atof();
	if (!nonumb && f >= 0) {
		zoomtab[j] = f;
		zapwcache(0);
		if (realpage && j == xfont && !ascii)
			ptps();
	}
}

double
getfzoom(void)
{
	return zoomtab[font];
}

void
casekern(void)
{
	kern = skip(0) || hatoi() ? 1 : 0;
}

void
casefkern(void)
{
	int	f, i, j;

	lgf++;
	if (skip(1))
		return;
	i = getrq(2);
	if ((f = findft(i, 1)) < 0)
		return;
	if (skip(0))
		fontbase[f]->kernfont = 0;
	else {
		j = hatoi();
		if (!nonumb)
			fontbase[f]->kernfont = j ? j : -1;
	}
}

static void
setpapersize(int setmedia)
{
	const struct {
		char	*name;
		int	width;
		int	heigth;
	} papersizes[] = {
		{ "executive",	 518,	 756 },
		{ "letter",	 612,	 792 },
		{ "legal",	 612,	 992 },
		{ "ledger",	1224,	 792 },
		{ "tabloid",	 792,	1224 },
		{ "a0",		2384,	3370 },
		{ "a1",		1684,	2384 },
		{ "a2",		1191,	1684 },
		{ "a3",		 842,	1191 },
		{ "a4",		 595,	 842 },
		{ "a5",		 420,	 595 },
		{ "a6",		 298,	 420 },
		{ "a7",		 210,	 298 },
		{ "a8",		 147,	 210 },
		{ "a9",		 105,	 147 },
		{ "a10",	  74,	 105 },
		{ "b0",		2835,	4008 },
		{ "b1",		2004,	2835 },
		{ "b2",		1417,	2004 },
		{ "b3",		1000,	1417 },
		{ "b4",		 709,	1000 },
		{ "b5",		 499,	 709 },
		{ "b6",		 354,	 499 },
		{ "b7",		 249,	 354 },
		{ "b8",		 176,	 249 },
		{ "b9",		 125,	 176 },
		{ "b10",	  87,	 125 },
		{ "c0",		2599,	3677 },
		{ "c1",		1837,	2599 },
		{ "c2",		1298,	1837 },
		{ "c3",		 918,	1298 },
		{ "c4",		 649,	 918 },
		{ "c5",		 459,	 649 },
		{ "c6",		 323,	 459 },
		{ "c7",		 230,	 323 },
		{ "c8",		 162,	 230 },
		{ NULL,		   0,	   0 }
	};
	int	c;
	int	x = 0, y = 0, n;
	char	buf[NC];

	lgf++;
	if (skip(1))
		return;
	c = cbits(ch);
	if (isdigit(c) || c == '(') {
		x = hatoi();
		if (!nonumb) {
			skip(1);
			y = hatoi();
		}
		if (nonumb || x == 0 || y == 0)
			return;
	} else {
		n = 0;
		do {
			c = getach();
			if (n+1 < sizeof buf)
				buf[n++] = c;
		} while (c);
		buf[n] = 0;
		for (n = 0; papersizes[n].name != NULL; n++)
			if (strcmp(buf, papersizes[n].name) == 0) {
				x = papersizes[n].width * INCH / 72;
				y = papersizes[n].heigth * INCH / 72;
				break;
			}
		if (x == 0 || y == 0) {
			errprint("Unknown paper size %s", buf);
			return;
		}
	}
	pl = defaultpl = y;
	if (numtab[NL].val > pl) {
		numtab[NL].val = pl;
		prwatchn(&numtab[NL]);
	}
	po = x > 6 * PO ? PO : x / 8;
	ll = ll1 = lt = lt1 = x - 2 * po;
	setnel();
	mediasize.val[2] = x;
	mediasize.val[3] = y;
	mediasize.flag |= 1;
	if (setmedia)
		mediasize.flag |= 2;
	if (realpage)
		ptpapersize();
}

void
casepapersize(void)
{
	setpapersize(0);
}

void
casemediasize(void)
{
	setpapersize(1);
}

static void
cutat(struct box *bp)
{
	int	c[4], i;

	for (i = 0; i < 4; i++) {
		if (skip(1))
			return;
		dfact = INCH;
		dfactd = 72;
		c[i] = inumb(NULL);
		if (nonumb)
			return;
	}
	for (i = 0; i < 4; i++)
		bp->val[i] = c[i];
	bp->flag |= 1;
	if (realpage)
		ptcut();
}

void
casetrimat(void)
{
	cutat(&trimat);
}

void
casebleedat(void)
{
	cutat(&bleedat);
}

void
casecropat(void)
{
	cutat(&cropat);
}

void
caselhang(void)
{
	kernsingle(lhangtab);
}

void
caserhang(void)
{
	kernsingle(rhangtab);
}

void
casekernpair(void)
{
	int	savfont = font, savfont1 = font1;
	int	f, g, i, j, n;
	tchar	c, d, *cp = NULL, *dp = NULL;
	int	a = 0, b = 0;

	lgf++;
	if (skip(1))
		return;
	i = getrq(2);
	if ((f = findft(i, 1)) < 0)
		return;
	font = font1 = f;
	mchbits();
	if (skip(1))
		goto done;
	while ((j = cbits(c = getch())) > ' ' || j == UNPAD) {
		if (fbits(c) != f) {
			if (warn & WARN_CHAR)
				errprint("glyph %C not in font %s",
					c, macname(i));
			continue;
		}
		cp = realloc(cp, ++a * sizeof *cp);
		cp[a-1] = c;
	}
	if (a == 0 || skip(1))
		goto done;
	i = getrq(2);
	if ((g = findft(i, 1)) < 0)
		goto done;
	font = font1 = g;
	mchbits();
	if (skip(1))
		goto done;
	while ((j = cbits(c = getch())) > ' ' || j == UNPAD) {
		if (fbits(c) != g) {
			if (warn & WARN_CHAR)
				errprint("glyph %C not in font %s",
					c, macname(i));
			continue;
		}
		dp = realloc(dp, ++b * sizeof *dp);
		dp[b-1] = c;
	}
	if (b == 0 || skip(1))
		goto done;
	noscale++;
	n = hatoi();
	noscale--;
	unitsPerEm = 1000;
	n = _unitconv(n);
	for (i = 0; i < a; i++)
		for (j = 0; j < b; j++) {
			if ((c = cbits(cp[i])) == 0)
				continue;
			if (c == UNPAD)
				c = ' ';
			setfbits(c, f);
			if ((d = cbits(dp[j])) == 0)
				continue;
			if (d == UNPAD)
				d = ' ';
			setfbits(d, g);
			kadd(c, d, n);
		}
done:
	free(cp);
	free(dp);
	font = savfont;
	font1 = savfont1;
	mchbits();
}

static void
kernsingle(int **tp)
{
	int     savfont = font, savfont1 = font1;
	int	f, i, j, n;
	int	twice = 0;
	tchar	c, *cp = NULL;
	int	a;

	lgf++;
	if (skip(1))
		return;
	i = getrq(2);
	if ((f = findft(i, 1)) < 0)
		return;
	font = font1 = f;
	mchbits();
	while (!skip(twice++ == 0)) {
		a = 0;
		while ((j = cbits(c = getch())) > ' ') {
			if (fbits(c) != f) {
				if (warn & WARN_CHAR)
					errprint("glyph %C not in font %s",
						c, macname(i));
				continue;
			}
			cp = realloc(cp, ++a * sizeof *cp);
			cp[a-1] = c;
		}
		if (skip(1))
			break;
		noscale++;
		n = hatoi();
		noscale--;
		if (tp[f] == NULL)
			tp[f] = calloc(NCHARS, sizeof *tp);
		unitsPerEm = 1000;
		n = _unitconv(n);
		for (j = 0; j < a; j++)
			tp[f][cbits(cp[j])] = n;
	}
	free(cp);
	font = savfont;
	font1 = savfont1;
	mchbits();
}

void
casekernafter(void)
{
	kernsingle(kernafter);
}

void
casekernbefore(void)
{
	kernsingle(kernbefore);
}

void
caseftr(void)
{
	int	savfont = font, savfont1 = font1;
	int	f, i, j;
	tchar	k;

	lgf++;
	if (skip(1))
		return;
	i = getrq(2);
	if ((f = findft(i, 1)) < 0)
		return;
	font = font1 = f;
	mchbits();
	if (skip(1))
		goto done;
	while ((i = cbits(k=getch())) != '\n') {
		if (ismot(k))
			goto done;
		if (ismot(k = getch()))
			goto done;
		if ((j = cbits(k)) == '\n')
			j = ' ';
		ftrtab[f][i] = j;
	}
done:
	checkenminus(f);
	font = savfont;
	font1 = savfont1;
	mchbits();
}

static int
getfeature(struct afmtab *a, int f)
{
	char	name[NC];
	int	ch1, ch2, c, i, j, minus;
	struct feature	*fp;

	if (skip(0))
		return 0;
	switch (c = getach()) {
	case '-':
		c = getach();
		minus = 1;
		break;
	case '+':
		c = getach();
		/*FALLTHRU*/
	default:
		minus = 0;
		break;
	case 0:
		return 0;
	}
	for (i = 0; i < sizeof name - 2; i++) {
		name[i] = c;
		if ((c = getach()) == 0)
			break;
	}
	name[i+1] = 0;
	for (i = 0; (fp = a->features[i]); i++)
		if (strcmp(fp->name, name) == 0) {
			for (j = 0; j < fp->npairs; j++) {
				ch1 = fp->pairs[j].ch1;
				ch2 = fp->pairs[j].ch2;
				if (minus) {
					if (ftrtab[f][ch1] == ch2)
						ftrtab[f][ch1] = ch1;
				} else {
					ftrtab[f][ch1] = ch2;
				}
			}
			break;
		}
	if (fp == NULL)
		errprint("no feature named %s in font %s", name, a->fontname);
	return 1;
}

void
casefeature(void)
{
	struct afmtab	*a;
	int	f, i, j;

	lgf++;
	if (skip(1))
		return;
	i = getrq(2);
	if ((f = findft(i, 1)) < 0)
		return;
	if ((j = (fontbase[f]->afmpos) - 1) < 0 ||
			((a = afmtab[j])->type != TYPE_OTF &&
			a->type != TYPE_TTF)) {
		errprint("font %s is not an OpenType font", macname(i));
		return;
	}
	if (a->features == NULL) {
		errprint("font %s has no OpenType features", a->fontname);
		return;
	}
	while (getfeature(a, f) != 0);
}

#include "unimap.h"

static int
ufmap(int c, int f, int *fp)
{
	struct unimap	*up, ***um;
	struct afmtab	*a;
	int	i;

	if ((c&~0xffff) == 0 &&
			(i = (fontbase[f]->afmpos) - 1) >= 0 &&
			(um = (a = afmtab[i])->unimap) != NULL &&
			um[c>>8] != NULL &&
			(up = um[c>>8][c&0377]) != NULL) {
		*fp = f;
		return up->u.code;
	}
	return 0;
}

int
un2tr(int c, int *fp)
{
	extern char	ifilt[];
	struct unimap	*um, *up;
	int	i, j;

	switch (c) {
	case 0x00A0:
		*fp = font;
		return UNPAD;
	case 0x00AD:
		*fp = font;
		return ohc;
	case 0x2002:
		return makem((int)(EM)/2);
	case 0x2003:
		return makem((int)EM);
	case 0x2004:
		return makem((int)EM/3);
	case 0x2005:
		return makem((int)EM/4);
	case 0x2006:
		return makem((int)EM/6);
	case 0x2007:
		return makem(width('0' | chbits));
	case 0x2008:
		return makem(width('.' | chbits));
	case 0x2009:
		return makem((int)EM/6);
	case 0x200A:
		return makem((int)EM/12);
	case 0x2010:
		*fp = font;
		return '-';
	case 0x2027:
		*fp = font;
		return OHC | BLBIT;
	case 0x2060:
		*fp = font;
		return FILLER;
	default:
		if ((i = ufmap(c, font, fp)) != 0)
			return i;
		if ((c&~0xffff) == 0 && unimap[c>>8] != NULL &&
				(um = unimap[c>>8][c&0377]) != NULL) {
			up = um;
			do
				if ((j = postchar1(up->u.psc, font)) != 0) {
					*fp = font;
					return j;
				}
			while ((up = up->next) != NULL);
			up = um;
			do
				if ((j = postchar(up->u.psc, fp)) != 0)
					return j;
			while ((up = up->next) != NULL);
			up = um;
			do
				if ((j = _ps2cc(up->u.psc, 0)) != 0) {
					j += nchtab + 128 + 32 +
						128 - 32 + nchtab;
					if (chartab[j] != NULL)
						return j;
				}
			while ((up = up->next) != NULL);
		}
		if (fallbacktab[font])
			for (j = 0; fallbacktab[font][j] != 0; j++) {
				if ((i = findft(fallbacktab[font][j], 0)) < 0)
					continue;
				if ((i = ufmap(c, i, fp)) != 0)
					return i;
			}
		if (smnt)
			for (i = smnt, j=0; j < nfonts; j++, i = i % nfonts + 1) {
				if (fontbase[i] == NULL)
					continue;
				if ((i = ufmap(c, i, fp)) != 0)
					return i;
			}
		*fp = font;
		if ((c < 040 && c == ifilt[c]) || (c >= 040 && c < 0177))
			return c;
		else if ((c & ~0177) == 0) {
			illseq(c, NULL, 0);
			return 0;
		} else if (defcf && (c & ~0xffff) == 0) {
			char	buf[20];
			snprintf(buf, sizeof(buf), "[uni%04X]", c);
			cpushback(buf);
			unadd(c, NULL);
			return WORDSP;
		} else if (html) {
			return c;
		} else {
			if (warn & WARN_CHAR)
				errprint("no glyph available for %U", c);
			return tryglf ? 0 : ' ';
		}
	}
}

int
tr2un(tchar i, int f)
{
	struct afmtab	*a;
	int	c, n;

	if (i < 32)
		return -1;
	else if (i < 128)
		return i;
	if ((n = (fontbase[f]->afmpos) - 1) >= 0) {
		a = afmtab[n];
		if (a->unitab && i < a->nunitab && a->unitab[i])
			return a->unitab[i];
		if (i - 32 >= nchtab + 128)
			i -= nchtab + 128;
		if ((n = a->fitab[i - 32]) < a->nchars &&
				a->nametab[n] != NULL)
			for (c = 0; rawunimap[c].psc; c++)
				if (strcmp(rawunimap[c].psc, a->nametab[n])==0)
					return rawunimap[c].code;
	}
	return -1;
}

tchar
setuc0(int n)
{
	int	f;
	tchar	c;

	if ((c = un2tr(n, &f)) != 0 && !ismot(c)) {
		c |= chbits & ~FMASK;
		setfbits(c, f);
	}
	return c;
}

static char *
getref(void)
{
	int	a = 0, i, c, delim;
	char	*np = NULL;

	if ((delim = getach()) != 0) {
		for (i = 0; ; i++) {
			if (i + 1 >= a)
				np = realloc(np, a += 32);
			if ((c = getach()) == 0) {
				if (cbits(ch) == ' ') {
					ch = 0;
					c = ' ';
				} else {
					nodelim(delim);
					break;
				}
			}
			if (c == delim)
				break;
			np[i] = c;
		}
		np[i] = 0;
	}
	return np;
}

tchar
setanchor(void)
{
	static int	cnt;
	struct ref	*rp;
	char	*np;

	if ((np = getref()) != NULL) {
		rp = calloc(1, sizeof *rp);
		rp->cnt = ++cnt;
		rp->name = np;
		rp->next = anchors;
		anchors = rp;
		return mkxfunc(ANCHOR, cnt);
	} else
		return mkxfunc(ANCHOR, 0);
}

static tchar
_setlink(struct ref **rstart, int oncode, int offcode, int *cnt)
{
	struct ref	*rp;
	char	*np;
	int	sv;

	sv = linkin;
	if ((linkin = !linkin)) {
		if ((np = getref()) != NULL) {
			rp = calloc(1, sizeof *rp);
			rp->cnt = ++*cnt;
			rp->name = np;
			rp->next = *rstart;
			*rstart = rp;
			linkin = *cnt;
			return mkxfunc(oncode, *cnt);
		} else {
			linkin = -1;
			return mkxfunc(oncode, 0);
		}
	} else
		return mkxfunc(offcode, sv > 0 ? sv : 0);
}

tchar
setlink(void)
{
	static int	cnt;

	return _setlink(&links, LINKON, LINKOFF, &cnt);
}

tchar
setulink(void)
{
	static int	cnt;

	return _setlink(&ulinks, ULINKON, ULINKOFF, &cnt);
}

int
pts2u(int p)
{
	return p * INCH / 72;
}

double
u2pts(int u)
{
	return u * 72.0 / INCH;
}

#define	psnprime	1021

static struct psnnode {
	struct psnnode	*next;
	const char	*name;
	int	code;
} **psntable;

static int
_ps2cc(const char *name, int create)
{
	struct psnnode	*pp;
	unsigned	h;

	if (psntable == NULL)
		psntable = calloc(psnprime, sizeof *psntable);
	h = pjw(name) % psnprime;
	for (pp = psntable[h]; pp; pp = pp->next)
		if (strcmp(name, pp->name) == 0)
			return pp->code;
	if (create == 0)
		return 0;
	pp = calloc(1, sizeof *pp);
	pp->name = strdup(name);
	pp->next = psntable[h];
	psntable[h] = pp;
	return pp->code = ++psmaxcode;
}

int
ps2cc(const char *name)
{
	return _ps2cc(name, 1);
}
