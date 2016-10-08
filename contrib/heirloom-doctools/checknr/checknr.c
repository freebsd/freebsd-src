/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved. The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	from OpenSolaris "checknr.c	1.8	05/06/02 SMI"	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 */
#if __GNUC__ >= 3 && __GNUC_MINOR__ >= 4 || __GNUC__ >= 4
#define	USED	__attribute__ ((used))
#elif defined __GNUC__
#define	USED	__attribute__ ((unused))
#else
#define	USED
#endif
static const char sccsid[] USED = "@(#)/usr/ucb/checknr.sl	1.3 (gritter) 11/6/05";

/*
 * checknr: check an nroff/troff input file for matching macro calls.
 * we also attempt to match size and font changes, but only the embedded
 * kind.  These must end in \s0 and \fP resp.  Maybe more sophistication
 * later but for now think of these restrictions as contributions to
 * structured typesetting.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include "global.h"

static	int	maxstk;	/* Stack size */
#define	MAXBR	100	/* Max number of bracket pairs known */
#define	MAXCMDS	500	/* Max number of commands known */

/*
 * The stack on which we remember what we've seen so far.
 */
static struct stkstr {
	int opno;	/* number of opening bracket */
	int pl;		/* '+', '-', ' ' for \s, 1 for \f, 0 for .ft */
	int parm;	/* parm to size, font, etc */
	int lno;	/* line number the thing came in in */
} *stk;
static int stktop;

/*
 * The kinds of opening and closing brackets.
 */
static struct brstr {
	char *opbr;
	char *clbr;
} br[MAXBR] = {
	/* A few bare bones troff commands */
#define	SZ	0
	{ "sz",	"sz" },	/* also \s */
#define	FT	1
	{ "ft",	"ft" },	/* also \f */
	/* the -mm package */
	{ "AL",	"LE" },
	{ "AS",	"AE" },
	{ "BL",	"LE" },
	{ "BS",	"BE" },
	{ "DF",	"DE" },
	{ "DL",	"LE" },
	{ "DS",	"DE" },
	{ "FS",	"FE" },
	{ "ML",	"LE" },
	{ "NS",	"NE" },
	{ "RL",	"LE" },
	{ "VL",	"LE" },
	/* the -ms package */
	{ "AB",	"AE" },
	{ "BD",	"DE" },
	{ "CD",	"DE" },
	{ "DS",	"DE" },
	{ "FS",	"FE" },
	{ "ID",	"DE" },
	{ "KF",	"KE" },
	{ "KS",	"KE" },
	{ "LD",	"DE" },
	{ "LG",	"NL" },
	{ "QS",	"QE" },
	{ "RS",	"RE" },
	{ "SM",	"NL" },
	{ "XA",	"XE" },
	{ "XS",	"XE" },
	/* The -me package */
	{ "(b",	")b" },
	{ "(c",	")c" },
	{ "(d",	")d" },
	{ "(f",	")f" },
	{ "(l",	")l" },
	{ "(q",	")q" },
	{ "(x",	")x" },
	{ "(z",	")z" },
	/* Things needed by preprocessors */
	{ "EQ",	"EN" },
	{ "TS",	"TE" },
	/* Refer */
	{ "[",	"]" },
	{ NULL,	NULL }
};

/*
 * All commands known to nroff, plus macro packages.
 * Used so we can complain about unrecognized commands.
 */
static char *knowncmds[MAXCMDS] = {
"$c", "$f", "$h", "$p", "$s", "(b", "(c", "(d", "(f", "(l", "(q", "(t",
"(x", "(z", ")b", ")c", ")d", ")f", ")l", ")q", ")t", ")x", ")z", "++",
"+c", "1C", "1c", "2C", "2c", "@(", "@)", "@C", "@D", "@F", "@I", "@M",
"@c", "@e", "@f", "@h", "@m", "@n", "@o", "@p", "@r", "@t", "@z", "AB",
"AE", "AF", "AI", "AL", "AM", "AS", "AT", "AU", "AX", "B",  "B1", "B2",
"BD", "BE", "BG", "BL", "BS", "BT", "BX", "C1", "C2", "CD", "CM", "CT",
"D",  "DA", "DE", "DF", "DL", "DS", "DT", "EC", "EF", "EG", "EH", "EM",
"EN", "EQ", "EX", "FA", "FD", "FE", "FG", "FJ", "FK", "FL", "FN", "FO",
"FQ", "FS", "FV", "FX", "H",  "HC", "HD", "HM", "HO", "HU", "I",  "ID",
"IE", "IH", "IM", "IP", "IX", "IZ", "KD", "KE", "KF", "KQ", "KS", "LB",
"LC", "LD", "LE", "LG", "LI", "LP", "MC", "ME", "MF", "MH", "ML", "MR",
"MT", "ND", "NE", "NH", "NL", "NP", "NS", "OF", "OH", "OK", "OP", "P",
"P1", "PF", "PH", "PP", "PT", "PX", "PY", "QE", "QP", "QS", "R",  "RA",
"RC", "RE", "RL", "RP", "RQ", "RS", "RT", "S",  "S0", "S2", "S3", "SA",
"SG", "SH", "SK", "SM", "SP", "SY", "T&", "TA", "TB", "TC", "TD", "TE",
"TH", "TL", "TM", "TP", "TQ", "TR", "TS", "TX", "UL", "US", "UX", "VL",
"WC", "WH", "XA", "XD", "XE", "XF", "XK", "XP", "XS", "[",  "[-", "[0",
"[1", "[2", "[3", "[4", "[5", "[<", "[>", "[]", "]",  "]-", "]<", "]>",
"][", "ab", "ac", "ad", "af", "am", "ar", "as", "b",  "ba", "bc", "bd",
"bi", "bl", "bp", "br", "bx", "c.", "c2", "cc", "ce", "cf", "ch",
"chop", "cs", "ct", "cu", "da", "de", "di", "dl", "dn", "do", "ds",
"dt", "dw", "dy", "ec", "ef", "eh", "el", "em", "eo", "ep", "ev",
"evc", "ex", "fallback", "fc", "feature", "fi", "fl", "flig", "fo",
"fp", "ft", "ftr", "fz", "fzoom", "hc", "he", "hidechar", "hl", "hp",
"ht", "hw", "hx", "hy", "hylang", "i", "ie", "if", "ig", "in", "ip",
"it", "ix", "kern", "kernafter", "kernbefore", "kernpair", "lc", "lg",
"lhang", "lc_ctype", "li", "ll", "ln", "lo", "lp", "ls", "lt", "m1",
"m2", "m3", "m4", "mc", "mk", "mo", "n1", "n2", "na", "ne", "nf", "nh",
"nl", "nm", "nn", "np", "nr", "ns", "nx", "of", "oh", "os", "pa",
"papersize", "pc", "pi", "pl", "pm", "pn", "po", "pp", "ps", "q",
"r",  "rb", "rd", "re", "recursionlimit", "return", "rhang", "rm",
"rn", "ro", "rr", "rs", "rt", "sb", "sc", "sh", "shift", "sk", "so",
"sp", "ss", "st", "sv", "sz", "ta", "tc", "th", "ti", "tl", "tm", "tp",
"tr", "track", "u",  "uf", "uh", "ul", "vs", "wh", "xflag", "xp", "yr",
0
};

static	int	lineno;		/* current line number in input file */
static	char	*line;		/* the current line */
static	size_t	linesize;	/* allocated size of current line */
static	char	*cfilename;	/* name of current file */
static	int	nfiles;		/* number of files to process */
static	int	fflag;		/* -f: ignore \f */
static	int	sflag;		/* -s: ignore \s */
static	int	ncmds;		/* size of knowncmds */
static	int	slot;		/* slot in knowncmds found by binsrch */

static void growstk(void);
static void usage(void);
static void process(FILE *f);
static void complain(int i);
static void prop(int i);
static void chkcmd(char *line, char *mac);
static void nomatch(char *mac);
static int eq(char *s1, char *s2);
static void pe(int lineno);
static void checkknown(char *mac);
static void addcmd(char *line);
static void addmac(char *mac);
static int binsrch(char *mac);
static char *fgetline(char **line, size_t *linesize, size_t *llen, FILE *fp);

static void
growstk(void)
{
	stktop++;
	if (stktop >= maxstk) {
		maxstk *= 2;
		stk = realloc(stk, sizeof *stk * maxstk);
	}
}

int
main(int argc, char **argv)
{
	FILE *f;
	int i;
	char *cp, *cq, c;

	stk = calloc(sizeof *stk, maxstk = 100);
	/* Figure out how many known commands there are */
	while (knowncmds[ncmds])
		ncmds++;
	while (argc > 1 && argv[1][0] == '-') {
		switch (argv[1][1]) {

		/* -a: add pairs of macros */
		case 'a':
			/* look for empty macro slots */
			for (i = 0; br[i].opbr; i++)
				;
			cp = &argv[1][3];
			while (*cp) {
				size_t s;
				if (i >= MAXBR - 3) {
					printf("Only %d known pairs allowed\n",
							MAXBR/2);
					exit(1);
				}
				for (cq = cp; *cq && *cq != '.'; cq++);
				if (*cq != '.')
					usage();
				*cq = 0;
				s = cq - cp + 1;
				br[i].opbr = malloc(s);
				n_strcpy(br[i].opbr, cp, s);
				*cq = '.';
				cp = &cq[1];
				for (cq = cp; *cq && *cq != '.'; cq++);
				c = *cq;
				*cq = 0;
				s = cq - cp + 1;
				br[i].clbr = malloc(s);
				n_strcpy(br[i].clbr, cp, s);
				*cq = c;
				cp = c ? &cq[1] : cq;
				/* knows pairs are also known cmds */
				addmac(br[i].opbr);
				addmac(br[i].clbr);
				i++;
			}
			break;

		/* -c: add known commands */
		case 'c':
			cp = &argv[1][3];
			while (*cp) {
				for (cq = cp; *cq && *cq != '.'; cq++);
				c = *cq;
				*cq = 0;
				addmac(cp);
				*cq = c;
				cp = c ? &cq[1] : cq;
			}
			break;

		/* -f: ignore font changes */
		case 'f':
			fflag = 1;
			break;

		/* -s: ignore size changes */
		case 's':
			sflag = 1;
			break;
		default:
			usage();
		}
		argc--; argv++;
	}

	nfiles = argc - 1;

	if (nfiles > 0) {
		for (i = 1; i < argc; i++) {
			cfilename = argv[i];
			f = fopen(cfilename, "r");
			if (f == NULL) {
				perror(cfilename);
				exit(1);
				}
			else {
				process(f);
				fclose(f);
			}
		}
	} else {
		cfilename = "stdin";
		process(stdin);
	}
	return (0);
}

static void
usage(void)
{
	printf("Usage: checknr -s -f -a.xx.yy.xx.yy... -c.xx.xx.xx...\n");
	exit(1);
}

static void
process(FILE *f)
{
	int i, n;
	char mac[512];	/* The current macro or nroff command */
	int pl;

	stktop = -1;
	for (lineno = 1; fgetline(&line, &linesize, NULL, f); lineno++) {
		if (line[0] == '.') {
			/*
			 * find and isolate the macro/command name.
			 */
			strncpy(mac, line+1, sizeof mac-1)[sizeof mac-1] = 0;
			if (isspace(mac[0]&0377)) {
				pe(lineno);
				printf("Empty command\n");
			} else {
				for (i = 1; mac[i]; i++)
					if (isspace(mac[i]&0377)) {
						mac[i] = 0;
						break;
					}
			}

			/*
			 * Is it a known command?
			 */
			checkknown(mac);

			/*
			 * Should we add it?
			 */
			if (eq(mac, "de"))
				addcmd(line);

			chkcmd(line, mac);
		}

		/*
		 * At this point we process the line looking
		 * for \s and \f.
		 */
		for (i = 0; line[i]; i++)
			if (line[i] == '\\' && (i == 0 || line[i-1] != '\\')) {
				if (!sflag && line[++i] == 's') {
					pl = line[++i]&0377;
					if (isdigit(pl)) {
						n = pl - '0';
						pl = ' ';
					} else
						n = 0;
					while (isdigit(line[++i]&0377))
						n = 10 * n + line[i] - '0';
					i--;
					if (n == 0) {
						if (stk[stktop].opno == SZ) {
							stktop--;
						} else {
							pe(lineno);
							printf(
						"unmatched \\s0\n");
						}
					} else {
						growstk();
						stk[stktop].opno = SZ;
						stk[stktop].pl = pl;
						stk[stktop].parm = n;
						stk[stktop].lno = lineno;
					}
				} else if (!fflag && line[i] == 'f') {
					n = line[++i];
					if (n == 'P') {
						if (stk[stktop].opno == FT) {
							stktop--;
						} else {
							pe(lineno);
							printf(
						"unmatched \\fP\n");
						}
					} else {
						growstk();
						stk[stktop].opno = FT;
						stk[stktop].pl = 1;
						stk[stktop].parm = n;
						stk[stktop].lno = lineno;
					}
				}
			}
	}
	/*
	 * We've hit the end and look at all this stuff that hasn't been
	 * matched yet!  Complain, complain.
	 */
	for (i = stktop; i >= 0; i--) {
		complain(i);
	}
}

static void
complain(int i)
{
	pe(stk[i].lno);
	printf("Unmatched ");
	prop(i);
	printf("\n");
}

static void
prop(int i)
{
	if (stk[i].pl == 0)
		printf(".%s", br[stk[i].opno].opbr);
	else switch (stk[i].opno) {
	case SZ:
		printf("\\s%c%d", stk[i].pl, stk[i].parm);
		break;
	case FT:
		printf("\\f%c", stk[i].parm);
		break;
	default:
		printf("Bug: stk[%d].opno = %d = .%s, .%s",
			i, stk[i].opno, br[stk[i].opno].opbr,
			br[stk[i].opno].clbr);
	}
}

/* ARGSUSED */
static void
chkcmd(char *line, char *mac)
{
	int i;

	/*
	 * Check to see if it matches top of stack.
	 */
	if (stktop >= 0 && eq(mac, br[stk[stktop].opno].clbr))
		stktop--;	/* OK. Pop & forget */
	else {
		/* No. Maybe it's an opener */
		for (i = 0; br[i].opbr; i++) {
			if (eq(mac, br[i].opbr)) {
				/* Found. Push it. */
				growstk();
				stk[stktop].opno = i;
				stk[stktop].pl = 0;
				stk[stktop].parm = 0;
				stk[stktop].lno = lineno;
				break;
			}
			/*
			 * Maybe it's an unmatched closer.
			 * NOTE: this depends on the fact
			 * that none of the closers can be
			 * openers too.
			 */
			if (eq(mac, br[i].clbr)) {
				nomatch(mac);
				break;
			}
		}
	}
}

static void
nomatch(char *mac)
{
	int i, j;

	/*
	 * Look for a match further down on stack
	 * If we find one, it suggests that the stuff in
	 * between is supposed to match itself.
	 */
	for (j = stktop; j >= 0; j--)
		if (eq(mac, br[stk[j].opno].clbr)) {
			/* Found.  Make a good diagnostic. */
			if (j == stktop-2) {
				/*
				 * Check for special case \fx..\fR and don't
				 * complain.
				 */
				if (stk[j+1].opno == FT &&
				    stk[j+1].parm != 'R' &&
				    stk[j+2].opno == FT &&
				    stk[j+2].parm == 'R') {
					stktop = j -1;
					return;
				}
				/*
				 * We have two unmatched frobs.  Chances are
				 * they were intended to match, so we mention
				 * them together.
				 */
				pe(stk[j+1].lno);
				prop(j+1);
				printf(" does not match %d: ", stk[j+2].lno);
				prop(j+2);
				printf("\n");
			} else for (i = j+1; i <= stktop; i++) {
				complain(i);
			}
			stktop = j-1;
			return;
		}
	/* Didn't find one.  Throw this away. */
	pe(lineno);
	printf("Unmatched .%s\n", mac);
}

/* eq: are two strings equal? */
static int
eq(char *s1, char *s2)
{
	return (strcmp(s1, s2) == 0);
}

/* print the first part of an error message, given the line number */
static void
pe(int lineno)
{
	if (nfiles > 1)
		printf("%s: ", cfilename);
	printf("%d: ", lineno);
}

static void
checkknown(char *mac)
{

	if (eq(mac, "."))
		return;
	if (binsrch(mac) >= 0)
		return;
	if (mac[0] == '\\' && mac[1] == '"')	/* comments */
		return;

	pe(lineno);
	printf("Unknown command: .%s\n", mac);
}

/*
 * We have a .de xx line in "line".  Add xx to the list of known commands.
 */
static void
addcmd(char *line)
{
	char *mac;

	/* grab the macro being defined */
	mac = line+4;
	while (isspace(*mac&0377))
		mac++;
	if (*mac == 0) {
		pe(lineno);
		printf("illegal define: %s\n", line);
		return;
	}
	mac[2] = 0;
	if (isspace(mac[1]&0377) || mac[1] == '\\')
		mac[1] = 0;
	addmac(mac);
}

/*
 * Add mac to the list.  We should really have some kind of tree
 * structure here but this is a quick-and-dirty job and I just don't
 * have time to mess with it.  (I wonder if this will come back to haunt
 * me someday?)  Anyway, I claim that .de is fairly rare in user
 * nroff programs, and the loop below is pretty fast.
 */
static void
addmac(char *mac)
{
	char **src, **dest, **loc;
	size_t s;

	if (binsrch(mac) >= 0) {	/* it's OK to redefine something */
#ifdef DEBUG
		printf("binsrch(%s) -> already in table\n", mac);
#endif
		return;
	}
	/* binsrch sets slot as a side effect */
#ifdef DEBUG
printf("binsrch(%s) -> %d\n", mac, slot);
#endif
	if (ncmds >= MAXCMDS) {
		printf("Only %d known commands allowed\n", MAXCMDS);
		exit(1);
	}
	loc = &knowncmds[slot];
	src = &knowncmds[ncmds-1];
	dest = src+1;
	while (dest > loc)
		*dest-- = *src--;
	s = strlen(mac) + 1;
	*loc = malloc(s);
	n_strcpy(*loc, mac, s);
	ncmds++;
#ifdef DEBUG
	printf("after: %s %s %s %s %s, %d cmds\n",
	    knowncmds[slot-2], knowncmds[slot-1], knowncmds[slot],
	    knowncmds[slot+1], knowncmds[slot+2], ncmds);
#endif
}

/*
 * Do a binary search in knowncmds for mac.
 * If found, return the index.  If not, return -1.
 */
static int
binsrch(char *mac)
{
	char *p;	/* pointer to current cmd in list */
	int d;		/* difference if any */
	int mid;	/* mid point in binary search */
	int top, bot;	/* boundaries of bin search, inclusive */

	top = ncmds-1;
	bot = 0;
	while (top >= bot) {
		mid = (top+bot)/2;
		p = knowncmds[mid];
		d = p[0] - mac[0];
		if (d == 0)
			d = strcmp(&p[1], &mac[1]);
		if (d == 0)
			return (mid);
		if (d < 0)
			bot = mid + 1;
		else
			top = mid - 1;
	}
	slot = bot;	/* place it would have gone */
	return (-1);
}

#define	LSIZE	256

static char *
fgetline(char **line, size_t *linesize, size_t *llen, FILE *fp)
{
	int c;
	size_t n = 0;

	if (*line == NULL || *linesize < LSIZE + n + 1)
		*line = realloc(*line, *linesize = LSIZE + n + 1);
	for (;;) {
		if (n >= *linesize - LSIZE / 2)
			*line = realloc(*line, *linesize += LSIZE);
		c = getc(fp);
		if (c != EOF) {
			(*line)[n++] = c;
			(*line)[n] = '\0';
			if (c == '\n')
				break;
		} else {
			if (n > 0)
				break;
			else
				return NULL;
		}
	}
	if (llen)
		*llen = n;
	return *line;
}
