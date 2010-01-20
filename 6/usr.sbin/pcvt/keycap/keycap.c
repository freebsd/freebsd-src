/*-
 * Copyright (c) 1992, 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Holger Veit
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif

static char *id =
	"@(#)keycap.c, 3.20, Last Edit-Date: [Tue Dec 20 14:51:50 1994]";

/*---------------------------------------------------------------------------*
 *
 *	keycap.c	Keyboard capabilities database handling
 *	-------------------------------------------------------
 *
 *	converted from printcap by Holger Veit (veit@du9ds3.uni-duisburg.de)
 *
 *	BUG:	Should use a "last" pointer in tbuf, so that searching
 *		for capabilities alphabetically would not be a n**2/2
 *		process when large numbers of capabilities are given.
 *
 *	Note:	If we add a last pointer now we will screw up the
 *		tc capability. We really should compile termcap.
 *
 *	modified by Hellmuth Michaelis (hm@hcshh.hcs.de) to fit into the
 *	vt220 driver pcvt 2.0 distribution
 *
 *	-hm	header conversion & cosmetic changes for pcvt 2.0 distribution
 *	-hm	debugging remapping
 *	-hm	cleaning up from termcap ....
 *	-hm	split off header file keycap.h
 *
 *---------------------------------------------------------------------------*/

#include <stdio.h>
#include <ctype.h>
#include <unistd.h>

#include "keycap.h"

#define	KEYCAP_BUFSIZ	1024

#define MAXHOP	32		/* max number of tc= indirections */

char	*getenv();

static	FILE *pfp = NULL;	/* keycap data base file pointer */
static	char *tbuf;
static	int hopcount;		/* detect infinite loops in keycap, init 0 */

static int knchktc();
static int knamatch();
static char *kdecode();

/*---------------------------------------------------------------------------*
 *	match a name
 *---------------------------------------------------------------------------*/
static char *nmatch(id,cstr)
char *id,*cstr;
{
	register n = strlen(id);
	register char *c = cstr+n;

	if (strncmp(id,cstr,n)==0 &&
	    (*c==':' || *c=='|' || *c=='=' || *c=='#') || *c=='@')
	    	return c;
	return 0;
}

/*---------------------------------------------------------------------------*
 * Get an entry for keyboard name in buffer bp from the keycap file.
 * Parse is very rudimentary, we just notice escaped newlines.
 *---------------------------------------------------------------------------*/
kgetent(bp, name)
char *bp, *name;
{
	register char *cp;
	register int c;
	register int i = 0, cnt = 0;
	char ibuf[KEYCAP_BUFSIZ];
	char *cp2;
	int tf;

	tbuf = bp;
	tf = 0;

	tf = open(KEYCAP_PATH, 0);

	if (tf < 0)
		return (-1);
	for (;;) {
		cp = bp;
		for (;;) {
			if (i == cnt) {
				cnt = read(tf, ibuf, KEYCAP_BUFSIZ);
				if (cnt <= 0) {
					close(tf);
					return (0);
				}
				i = 0;
			}
			c = ibuf[i++];
			if (c == '\n') {
				if (cp > bp && cp[-1] == '\\'){
					cp--;
					continue;
				}
				break;
			}
			if (cp >= bp+KEYCAP_BUFSIZ) {
				write(STDERR_FILENO, "Keycap entry too long\n", 23);
				break;
			} else
				*cp++ = c;
		}
		*cp = 0;

		/*
		 * The real work for the match.
		 */
		if (knamatch(name)) {
			close(tf);
			return(knchktc());
		}
	}
}

/*---------------------------------------------------------------------------*
 * knchktc: check the last entry, see if it's tc=xxx. If so, recursively
 * find xxx and append that entry (minus the names) to take the place of
 * the tc=xxx entry. Note that this works because of the left to right scan.
 *---------------------------------------------------------------------------*/
static int knchktc()
{
	register char *p, *q;
	char tcname[16];	/* name of similar keyboard */
	char tcbuf[KEYCAP_BUFSIZ];
	char *holdtbuf = tbuf;
	int l;

	p = tbuf + strlen(tbuf) - 2;	/* before the last colon */
	while (*--p != ':')
		if (p<tbuf) {
			write(STDERR_FILENO, "Bad keycap entry\n", 18);
			return (0);
		}
	p++;
	/* p now points to beginning of last field */
	if (p[0] != 't' || p[1] != 'c')
		return(1);
	strcpy(tcname,p+3);
	q = tcname;
	while (q && *q != ':')
		q++;
	*q = 0;
	if (++hopcount > MAXHOP) {
		write(STDERR_FILENO, "Infinite tc= loop\n", 18);
		return (0);
	}
	if (kgetent(tcbuf, tcname) != 1)
		return(0);
	for (q=tcbuf; *q != ':'; q++)
		;
	l = p - holdtbuf + strlen(q);
	if (l > KEYCAP_BUFSIZ) {
		write(STDERR_FILENO, "Keycap entry too long\n", 23);
		q[KEYCAP_BUFSIZ - (p-tbuf)] = 0;
	}
	strcpy(p, q+1);
	tbuf = holdtbuf;
	return(1);
}

/*---------------------------------------------------------------------------*
 * knamatch deals with name matching.  The first field of the keycap entry
 * is a sequence of names separated by |'s, so we compare against each such
 * name. The normal : terminator after the last name (before the first field)
 * stops us.
 *---------------------------------------------------------------------------*/
static int knamatch(np)
char *np;
{
	register char *Np, *Bp;

	Bp = tbuf;
	if (*Bp == '#' || *Bp == 0)
		return(0);
	for (;;) {
		for (Np = np; *Np && *Bp == *Np; Bp++, Np++)
			continue;
		if (*Np == 0 && (*Bp == '|' || *Bp == ':' || *Bp == 0))
			return (1);
		while (*Bp && *Bp != ':' && *Bp != '|')
			Bp++;
		if (*Bp == 0 || *Bp == ':')
			return (0);
		Bp++;
	}
}

/*---------------------------------------------------------------------------*
 * Skip to the next field. Notice that this is very dumb, not knowing about
 * \: escapes or any such. If necessary, :'s can be put into the keycap file
 * in octal.
 *---------------------------------------------------------------------------*/
static char *kskip(bp)
char *bp;
{
	while (*bp && *bp != ':')
		bp++;
	if (*bp == ':')
		bp++;
	return (bp);
}

/*---------------------------------------------------------------------------*
 * Return the (numeric) option id. Numeric options look like 'li#80' i.e.
 * the option string is separated from the numeric value by a # character.
 * If the option is not found we return -1. Note that we handle octal
 * numbers beginning with 0.
 *---------------------------------------------------------------------------*/
int kgetnum(id)
char *id;
{
	register int i, base;
	register char *bp = tbuf,*xp;

	for (;;) {
		bp = kskip(bp);
		if (*bp == 0)
			return (-1);
		if ((xp=nmatch(id,bp)) == 0)
			continue;
		bp = xp;	/* we have an entry */
		if (*bp == '@')
			return(-1);
		if (*bp != '#')
			continue;
		bp++;
		base = 10;
		if (*bp == '0')
			base = 8;
		i = 0;
		while (isdigit(*bp))
			i *= base, i += *bp++ - '0';
		return (i);
	}
}

/*---------------------------------------------------------------------------*
 * Handle a flag option. Flag options are given "naked", i.e. followed by
 * a : or the end of the buffer.  Return 1 if we find the option, or 0 if
 * it is not given.
 *---------------------------------------------------------------------------*/
int kgetflag(id)
char *id;
{
	register char *bp = tbuf,*xp;

	for (;;) {
		bp = kskip(bp);
		if (!*bp)
			return (0);
		if ((xp=nmatch(id,bp)) != 0) {
			bp = xp;
			if (!*bp || *bp == ':')
				return (1);
			else if (*bp == '@')
				return(0);
		}
	}
}

/*---------------------------------------------------------------------------*
 * Get a string valued option. These are given as 'cl=^Z'. Much decoding
 * is done on the strings, and the strings are placed in area, which is a
 * ref parameter which is updated. No checking on area overflow.
 *---------------------------------------------------------------------------*/
char *kgetstr(id, area)
char *id;
char **area;
{
	register char *bp = tbuf,*xp;

	for (;;) {
		bp = kskip(bp);
		if (!*bp)
			return (0);
		if ((xp = nmatch(id,bp)) == 0)
			continue;
		bp = xp;
		if (*bp == '@')
			return(0);
		if (*bp != '=')
			continue;
		bp++;
		return (kdecode(bp, area));
	}
}

/*---------------------------------------------------------------------------*
 * kdecode does the grung work to decode the string capability escapes.
 *---------------------------------------------------------------------------*/
static char *kdecode(str, area)
char *str;
char **area;
{
	register char *cp;
	register int c;
	register char *dp;
	int i;

	cp = *area;
	while ((c = *str++) && c != ':') {
		switch (c) {

		case '^':
			c = *str++ & 037;
			break;

		case '\\':
			dp = "E\033^^\\\\::n\nr\rt\tb\bf\f";
			c = *str++;
nextc:
			if (*dp++ == c) {
				c = *dp++;
				break;
			}
			dp++;
			if (*dp)
				goto nextc;
			if (isdigit(c)) {
				c -= '0', i = 2;
				do
					c <<= 3, c |= *str++ - '0';
				while (--i && isdigit(*str));
			}
			break;
		}
		*cp++ = c;
	}
	*cp++ = 0;
	str = *area;
	*area = cp;
	return (str);
}

/*-------------------------------- EOF --------------------------------------*/
