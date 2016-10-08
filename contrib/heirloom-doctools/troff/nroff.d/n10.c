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


/*	from OpenSolaris "n10.c	1.15	05/06/08 SMI"	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)n10.c	1.33 (gritter) 12/25/06
 *
 * Portions Copyright (c) 2014 Carsten Kunze <carsten.kunze@arcor.de>
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
n10.c

Device interfaces
*/

#include <stdio.h>
#include <limits.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <stdlib.h>
#ifdef EUC
#include <wchar.h>
#endif
#include "tdef.h"
#include "ext.h"
#include "tw.h"
#include "pt.h"
#include "bst.h"

struct t t;	/* terminal characteristics */

int	dtab;
int	plotmode;
int	esct;

int	nchtab;

int	*bdtab;
int	*fontlab;

int	Inch;
int	Hor;
int	Vert;
int	nfonts	= 4;	/* R, I, B, S */

/* these characters are used as various signals or values
 * in miscellaneous places.
 * values are set in specnames in t10.c
 */

int	c_hyphen;
int	c_emdash;
int	c_rule;
int	c_minus;
int	c_fi;
int	c_fl;
int	c_ff;
int	c_ffi;
int	c_ffl;
int	c_acute;
int	c_grave;
int	c_under;
int	c_rooten;
int	c_boxrule;
int	c_lefthand;
int	c_dagger;
int	c_isalnum;

int	utf8;
int	tlp;

static int bst_intcmp(union bst_val, union bst_val);
static int bst_strcmp(union bst_val, union bst_val);

static int nch;

static struct bst utf8oc  = { NULL, bst_intcmp };
static struct bst chnames = { NULL, bst_strcmp };

#define UTF8OC_KEY(c) ((union bst_val)(int)c)
#define UTF8OC_VAL(s) ((union bst_val)(void *)s)

void
ptinit(void)
{
	int i, j;
	char *p, c, *p2;
	char *tt;
	size_t ttl;
	int nread, fd;
	struct stat stbuf;
	char *check;
	extern int initbdtab[], initfontlab[];
	int nl;
	size_t codsiz;
	char *codestr;
	char *code;

	check = malloc(1024);
	bdtab = initbdtab;
	fontlab = initfontlab;
	ttl = strlen(termtab) + strlen(devname) + 1;
	tt = malloc(ttl);
	n_strcpy(tt, termtab, ttl);
	if (strcmp(devname, "locale")) {
		n_strcat(tt, devname, ttl);
		if (!strcmp(devname, "lp"))
			tlp = 1;
	}
	else {
#ifdef	EUC
		wchar_t	wc;
		if (mb_cur_max > 1 && mbtowc(&wc, "\303\266", 2) == 2 &&
		    wc == 0xF6 && mbtowc(&wc, "\342\202\254", 3) == 3 &&
		    wc == 0x20AC) {
			csi_width[0] = 0;
			utf8 = 1;
			n_strcat(tt, "utf8", ttl); /* shorter than "locale" */
			avl_add(&utf8oc, UTF8OC_KEY('-'),
			    UTF8OC_VAL(strdup("\xe2\x80\x90")));
			avl_add(&utf8oc, UTF8OC_KEY('`'),
			    UTF8OC_VAL(strdup("\xe2\x80\x98")));
			avl_add(&utf8oc, UTF8OC_KEY('\''),
			    UTF8OC_VAL(strdup("\xe2\x80\x99")));
		} else
		{
#endif
			tlp = 1;
			n_strcat(tt, "lp", ttl); /* shorter than "locale" */
#ifdef	EUC
		}
#endif
	}
	if ((fd = open(tt, O_RDONLY)) < 0) {
		errprint("cannot open %s", tt);
		exit(-1);
	}
	fstat(fd, &stbuf);
	codestr = malloc(stbuf.st_size + 1);
	nread = read(fd, codestr, stbuf.st_size);
	close(fd);
	codestr[stbuf.st_size] = 0;

	p = codestr;
	codsiz = 0;
	while (1) {
		i = 0;
		while ((c = *p) && c != '\n') {
			check[i++] = c;
			p++;
		}
		check[i] = 0;
		if (!c) {
			errprint("Unexpected end of %s", tt);
			exit(1);
		}
		while (*p == '\n') p++;
		if (!strcmp(check, "charset")) break;
		j = 0;
		while (j < i &&  check[j] != ' ' && check[j] != '\t' ) j++;
		while (j < i && (check[j] == ' ' || check[j] == '\t')) j++;
		codsiz += i-j;
	}
	while (1) {
		char c0;
		int cmt;
		i = 0;
		while ((c = *p) && c != ' ' && c != '\t' && c != '\n') {
			p++;
			if (!i++) c0 = c;
		}
		if (i == 1 && c0 == '#' && c == ' ') {
			cmt = 1;
		} else {
			cmt = 0;
		}
		if (!cmt) {
			while ((c = *p) && (c == ' ' || c == '\t')) p++;
			while ((c = *p) && c >= '0' && c <= '9') p++;
			while ((c = *p) && (c == ' ' || c == '\t')) p++;
			while ((c = *p) && c != ' ' && c != '\t' && c != '\n') {
				p++;
				codsiz++;
			}
			codsiz++;
		}
		while ((c = *p) && c != '\n') p++;
		if (!c) break;
		while (*p == '\n') p++;
	}

	t.codetab = calloc(NROFFCHARS-_SPECCHAR_ST, sizeof *t.codetab);
	t.width = calloc(NROFFCHARS, sizeof *t.width);
	code = malloc(codsiz);

	p = codestr;
	p2 = code;
	p = skipstr(p);		/* skip over type, could check */
	p = skipstr(p); p = getint(p, &t.bset);
	p = skipstr(p); p = getint(p, &t.breset);
	p = skipstr(p); p = getint(p, &t.Hor);
	p = skipstr(p); p = getint(p, &t.Vert);
	p = skipstr(p); p = getint(p, &t.Newline);
	p = skipstr(p); p = getint(p, &t.Char);
	p = skipstr(p); p = getint(p, &t.Em);
	p = skipstr(p); p = getint(p, &t.Halfline);
	p = skipstr(p); p = getint(p, &t.Adj);
	p = skipstr(p); p = getstr(p, t.twinit  = p2); p2 += strlen(p2)+1;
	p = skipstr(p); p = getstr(p, t.twrest  = p2); p2 += strlen(p2)+1;
	p = skipstr(p); p = getstr(p, t.twnl    = p2); p2 += strlen(p2)+1;
	p = skipstr(p); p = getstr(p, t.hlr     = p2); p2 += strlen(p2)+1;
	p = skipstr(p); p = getstr(p, t.hlf     = p2); p2 += strlen(p2)+1;
	p = skipstr(p); p = getstr(p, t.flr     = p2); p2 += strlen(p2)+1;
	p = skipstr(p); p = getstr(p, t.bdon    = p2); p2 += strlen(p2)+1;
	p = skipstr(p); p = getstr(p, t.bdoff   = p2); p2 += strlen(p2)+1;
	p = skipstr(p); p = getstr(p, t.iton    = p2); p2 += strlen(p2)+1;
	p = skipstr(p); p = getstr(p, t.itoff   = p2); p2 += strlen(p2)+1;
	p = skipstr(p); p = getstr(p, t.ploton  = p2); p2 += strlen(p2)+1;
	p = skipstr(p); p = getstr(p, t.plotoff = p2); p2 += strlen(p2)+1;
	p = skipstr(p); p = getstr(p, t.up      = p2); p2 += strlen(p2)+1;
	p = skipstr(p); p = getstr(p, t.down    = p2); p2 += strlen(p2)+1;
	p = skipstr(p); p = getstr(p, t.right   = p2); p2 += strlen(p2)+1;
	p = skipstr(p); p = getstr(p, t.left    = p2); p2 += strlen(p2)+1;

	p = getstr(p, check);
	if (strcmp(check, "charset") != 0) {
		errprint("device table apparently curdled");
		exit(1);
	}

	for (i = 0; i < _SPECCHAR_ST; i++)
		t.width[i] = 1;	/* default widths */

	i = 0;
	nl = 1;
next_line:
	while (p < codestr + nread) {
		char *s;
		while ((c = *p) == ' ' || c == '\t' || c == '\n') {
			if (c == '\n') nl = 1;
			p++;
		}
		if (c == '#' && !nl) {
			while (*p && *p != '\n') p++;
			while (*p == '\n') p++;
		}
		if (!*p) break; /* last line ends with comment */
		nl = 0;
		if (i + _SPECCHAR_ST >= NROFFCHARS) {
			errprint("too many names in charset for %s", tt);
			exit(1);
		}
		s = p;
		j = 0;
		while ((c = *p) != ' ' && c != '\t') {
			p++;
			j++;
		}
		if (j == 1 && p[-1] == '#' && c == ' ') {
			while (*p && *p != '\n') p++;
			goto next_line;
		}
		*p++ = '\0';
		while (*p == ' ' || *p == '\t')
			p++;
		t.width[i+_SPECCHAR_ST] = *p++ - '0';
		while (*p == ' ' || *p == '\t')
			p++;
		t.codetab[i] = p2;
		p = getstr(p, p2);	/* compress string */
		p2 += strlen(p2) + 1;
		p++;
		i++;
		addch(s);
	}
	nchtab = nch;
	sps = EM;
	ses = EM;
	ics = EM * 2;
	dtab = 8 * t.Em;
	for (i = 0; i < 16; i++)
		tabtab[i] = dtab * (i + 1);
	pl = 11 * INCH;
	po = PO;
	spacesz = SS;
	sesspsz = SSS;
	lss = lss1 = VS;
	ll = ll1 = lt = lt1 = LL;
	smnt = nfonts = 5;	/* R I B BI S */
	specnames();	/* install names like "hyphen", etc. */
	if (eqflg)
		t.Adj = t.Hor;
	free(codestr);
	free(tt);
	free(check);
}

char *
skipstr (	/* skip over leading space plus string */
    char *s
)
{
	while (*s == ' ' || *s == '\t' || *s == '\n')
		s++;
	while (*s != ' ' && *s != '\t' && *s != '\n')
		if (*s++ == '\\')
			s++;
	return s;
}

char *
getstr (	/* find next string in s, copy to t */
    char *s,
    char *t
)
{
	int quote = 0;

	while (*s == ' ' || *s == '\t' || *s == '\n')
		s++;
	if (*s == '"') {
		s++;
		quote = 1;
	}
	for (;;) {
		if (quote && *s == '"') {
			s++;
			break;
		}
		if (!quote && (*s == ' ' || *s == '\t' || *s == '\n'))
			break;
		if (*s != '\\')
			*t++ = *s++;
		else {
			s++;	/* skip \\ */
			if (isdigit((unsigned char)s[0]) &&
			    isdigit((unsigned char)s[1]) &&
			    isdigit((unsigned char)s[2])) {
				*t++ = (s[0]-'0')<<6 | (s[1]-'0')<<3 | (s[2]-'0');
				s += 2;
			} else if (s[0] == 'x' &&
			           isxdigit((unsigned char)s[1]) &&
			           isxdigit((unsigned char)s[2])) {
				*t++ = hex2nibble(s[1]) << 4 |
				       hex2nibble(s[2])      ;
				s += 2;
			} else if (isdigit((unsigned char)s[0])) {
				*t++ = *s - '0';
			} else if (*s == 'b') {
				*t++ = '\b';
			} else if (*s == 'n') {
				*t++ = '\n';
			} else if (*s == 'r') {
				*t++ = '\r';
			} else if (*s == 't') {
				*t++ = '\t';
			} else {
				*t++ = *s;
			}
			s++;
		}
	}
	*t = '\0';
	return s;
}

char *
getint (	/* find integer at s */
    char *s,
    int *pn
)
{
	int base;

	while (*s == ' ' || *s == '\t' || *s == '\n')
		s++;
	base = (*s == '0') ? 8 : 10;
	*pn = 0;
	while (isdigit((unsigned char)*s))
		*pn = base * *pn + *s++ - '0';
	return s;
}

void
specnames(void)
{
	static struct {
		int	*n;
		char	*v;
	} spnames[] = {
		{ &c_hyphen, "hy"	},
		{ &c_emdash, "em"	},
		{ &c_rule, "ru"		},
		{ &c_minus, "\\-"	},
		{ &c_fi, "fi"		},
		{ &c_fl, "fl"		},
		{ &c_ff, "ff"		},
		{ &c_ffi, "Fi"		},
		{ &c_ffl, "Fl"		},
		{ &c_acute, "aa"	},
		{ &c_grave, "ga"	},
		{ &c_under, "ul"	},
		{ &c_rooten, "rn"	},
		{ &c_boxrule, "br"	},
		{ &c_lefthand, "lh"	},
		{ &c_isalnum, "__"	},
		{ 0, 0 }
	};
	int	i;

	for (i = 0; spnames[i].n; i++)
		*spnames[i].n = findch(spnames[i].v);
	if (c_isalnum == 0)
		c_isalnum = NROFFCHARS;
}


int 
findch(char *s) {
	struct bst_node *n;
	if (bst_srch(&chnames, (union bst_val)(void *)s, &n))
		return 0;
	return n->data.i + _SPECCHAR_ST;
}

int
addch(char *s) {
	avl_add(&chnames, (union bst_val)(void *)strdup(s),
	    (union bst_val)(int)nch);
	return nch++ + _SPECCHAR_ST;
}

void
twdone(void)
{
	int waitf;

	obufp = obuf;
	if (t.twrest)		/* has ptinit() been done yet? */
		oputs(t.twrest);
	flusho();
	if (pipeflg != -1) {
		close(ptid);
		waitpid(pipeflg, &waitf, 0);
	}
	restore_tty();
}


void
ptout(tchar i)
{
	if (cbits(i) == FILLER)
		return;
	if (isadjspc(i))
		return;
	if (olinep >= &oline[olinesz]) {
		tchar	*k;
		olinesz += 100;
		k = realloc(oline, olinesz * sizeof *oline);
		olinep = (tchar *)((char *)olinep + ((char *)k-(char *)oline));
		oline = k;
	}
	*olinep++ = i;
	if (cbits(i) != '\n')
		return;
	olinep--;
	lead += dip->blss + lss - t.Newline;
	dip->blss = 0;
	esct = esc = 0;
	if (olinep > oline) {
		move();
		ptout1();
		oputs(t.twnl);
	} else {
		lead += t.Newline;
		move();
	}
	lead += dip->alss;
	dip->alss = 0;
	olinep = oline;
}


void
ptout1(void)
{
	register int k;
	register char	*codep;
	char	*savep;
#ifdef EUC
	register char *qq;
#endif /* EUC */
	int	w, j, phyw;
#ifdef EUC
	int jj;
#endif /* EUC */
	tchar * q, i;
	static int oxfont = FT;	/* start off in roman */
	struct bst_node *uconv;

	for (q = oline; q < olinep; q++) {
		i = *q;
		if (ismot(i)) {
			j = absmot(i);
			if (isnmot(i))
				j = -j;
			if (isvmot(i))
				lead += j;
			else 
				esc += j;
			continue;
		}
		if ((k = cbits(i)) <= 040) {
			switch (k) {
			case ' ': /*space*/
				esc += t.Char;
				break;
			case '\033':
			case '\007':
			case '\016':
			case '\017':
				oput(k);
				break;
			}
			continue;
		}
#ifdef EUC
		if (multi_locale && (k >= nchtab + _SPECCHAR_ST)) {
			jj = tr2un(k, fbits(i));
			if ((jj = wcwidth(jj)) < 0)
				jj = 0;
			phyw = w = t.Char * csi_width[jj];
			if (iszbit(i))
				w = 0;
		} else {
#endif /* EUC */
		phyw = w = t.Char * t.width[k];
		if (iszbit(i))
			w = 0;
#ifdef EUC
		}
#endif /* EUC */
		if (esc || lead)
			move();
		esct += w;
		xfont = fbits(i);
		if (xfont != oxfont) {
			if (oxfont == ulfont || oxfont == BIFONT)
				oputs(t.itoff);
			if (bdtab[oxfont])
				oputs(t.bdoff);
			if (xfont == ulfont || xfont == BIFONT)
				oputs(t.iton);
			if (bdtab[xfont])
				oputs(t.bdon);
			oxfont = xfont;
		}
		if ((xfont == ulfont || xfont == BIFONT) && !(*t.iton & 0377)) {
			for (j = w / t.Char; j > 0; j--)
				oput('_');
			for (j = w / t.Char; j > 0; j--)
				oput('\b');
		}
		if ((j = bdtab[xfont]) && !(*t.bdon & 0377))
			j++;
		else
			j = 1;	/* number of overstrikes for bold */
#ifdef	EUC
		if (utf8 && !bst_srch(&utf8oc, UTF8OC_KEY(k), &uconv)) {
			for (savep = uconv->data.p; *savep; savep++)
				oput(*savep);
		} else
#endif
			if (k < 128) {	/* ordinary ascii */
			oput(k);
			while (--j > 0) {
				oput('\b');
				oput(k);
			}
#ifdef EUC
		} else if (multi_locale && (k >= nchtab + _SPECCHAR_ST)) {
			int	n;
			char	mb[MB_LEN_MAX+1];
			jj = tr2un(k, fbits(i));
			if ((n = wctomb(mb, jj)) > 0) {
				for (qq = mb; qq < &mb[n];)
					oput(*qq++);
				while (--j > 0) {
					for (jj = w / t.Char; jj > 0; jj--)
						oput('\b');
					for (qq = mb; qq < &mb[n];)
						oput(*qq++);
				}
			}
		} else if (k < 256) {
			/*
			 * Invalid character for C locale or
			 * non-printable 8-bit single byte
			 * character such as <no-break-sp>
			 * in ISO-8859-1
			 */
			oput(k);
			while (--j > 0) {
				oput('\b');
				oput(k);
			}
#endif /* EUC */
		} else if (k >= nchtab + _SPECCHAR_ST) {
			oput(k - nchtab - _SPECCHAR_ST);
		} else {
			int oj, isesc, allesc;
			savep = t.codetab[k-_SPECCHAR_ST];
		loop:	codep = savep;
			allesc = 1;
			oj = j;
			while (*codep != 0) {
				if (*codep & 0200) {
					codep = plot(codep);
					oput(' ');
					allesc = 0;
				} else {
					if ((isesc = *codep=='%')) /* escape */
						codep++;
					else
						allesc = 0;
					oput(*codep);
					if (*codep == '\033')
						oput(*++codep);
					else if (*codep != '\b' && !isesc)
						for (j = oj; --j > 0; ) {
							oput('\b');
							oput(*codep);
						}
					codep++;
				}
			}
			if (allesc && --j > 0) {
				oput('\b');
				goto loop;
			}
		}
		if (!w)
			for (j = phyw / t.Char; j > 0; j--)
			{
				oput('\b');
			}
	}
}


char *
plot(char *x)
{
	register int	i;
	register char	*j, *k;

	oputs(t.ploton);
	k = x;
	if ((*k & 0377) == 0200)
		k++;
	for (; *k; k++) {
		if (*k == '%') {	/* quote char within plot mode */
			oput(*++k);
		} else if (*k & 0200) {
			if (*k & 0100) {
				if (*k & 040)
					j = t.up; 
				else 
					j = t.down;
			} else {
				if (*k & 040)
					j = t.left; 
				else 
					j = t.right;
			}
			if ((i = *k & 037) == 0) {	/* 2nd 0200 turns it off */
				++k;
				break;
			}
			while (i--)
				oputs(j);
		} else 
			oput(*k);
	}
	oputs(t.plotoff);
	return(k);
}


void
move(void)
{
	register int k;
	register char	*i, *j;
	char	*p, *q;
	int	iesct, dt;

	iesct = esct;
	if (esct += esc)
		i = "\0"; 
	else 
		i = "\n\0";
	j = t.hlf;
	p = t.right;
	q = t.down;
	if (lead) {
		if (lead < 0) {
			lead = -lead;
			i = t.flr;
			/*	if(!esct)i = t.flr; else i = "\0";*/
			j = t.hlr;
			q = t.up;
		}
		if (*i & 0377) {
			k = lead / t.Newline;
			lead = lead % t.Newline;
			while (k--)
				oputs(i);
		}
		if (*j & 0377) {
			k = lead / t.Halfline;
			lead = lead % t.Halfline;
			while (k--)
				oputs(j);
		} else { /* no half-line forward, not at line begining */
			k = lead / t.Newline;
			lead = lead % t.Newline;
			if (k > 0) 
				esc = esct;
			i = "\n";
			while (k--) 
				oputs(i);
		}
	}
	if (esc) {
		if (esc < 0) {
			esc = -esc;
			j = "\b";
			p = t.left;
		} else {
			j = " ";
			if (hflg)
				while ((dt = dtab - (iesct % dtab)) <= esc) {
					if (dt % t.Em)
						break;
					oput(TAB);
					esc -= dt;
					iesct += dt;
				}
		}
		k = esc / t.Em;
		esc = esc % t.Em;
		while (k--)
			oputs(j);
	}
	if ((*t.ploton & 0377) && (esc || lead)) {
		oputs(t.ploton);
		esc /= t.Hor;
		lead /= t.Vert;
		while (esc--)
			oputs(p);
		while (lead--)
			oputs(q);
		oputs(t.plotoff);
	}
	esc = lead = 0;
}


void
ptlead(void)
{
	move();
}


void
dostop(void)
{
	char	junk;

	flusho();
	read(2, &junk, 1);
}


/*ARGSUSED*/
void
newpage(int unused)
{
	realpage++;
}

void
pttrailer(void){;}

void
caseutf8conv(void) {
#ifdef EUC
	tchar tc;
	int i, o;
	struct bst_node *n;
	char mb[MB_LEN_MAX+1];
	if (skip(1)) return;
	i = cbits(getch());
	if (skip(0)) {
		if (!bst_srch(&utf8oc, UTF8OC_KEY(i), &n)) {
			free(n->data.p);
			avl_del(&utf8oc, UTF8OC_KEY(i));
		}
	} else {
		char *s;
		o = cbits(tc = getch());
		if (o < 256) {
			s = malloc(2);
			s[0] = o;
			s[1] = 0;
		} else if (multi_locale && (o >= nchtab + _SPECCHAR_ST)) {
			mb[wctomb(mb, tr2un(o, fbits(tc)))] = 0;
			s = strdup(mb);
		} else {
			s = strdup(t.codetab[o-_SPECCHAR_ST]);
		}
		if (bst_srch(&utf8oc, UTF8OC_KEY(i), &n)) {
			avl_add(&utf8oc, UTF8OC_KEY(i), UTF8OC_VAL(s));
		} else {
			free(n->data.p);
			n->data.p = s;
		}
	}
#endif /* EUC */
}

static int
bst_intcmp(union bst_val a, union bst_val b) {
	return a.i < b.i ? -1 :
	       a.i > b.i ?  1 :
	                    0 ;
}

static int
bst_strcmp(union bst_val a, union bst_val b) {
	return strcmp(a.p, b.p);
}
