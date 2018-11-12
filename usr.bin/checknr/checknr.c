/*
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 */

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1980, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#if 0
#ifndef lint
static char sccsid[] = "@(#)checknr.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * checknr: check an nroff/troff input file for matching macro calls.
 * we also attempt to match size and font changes, but only the embedded
 * kind.  These must end in \s0 and \fP resp.  Maybe more sophistication
 * later but for now think of these restrictions as contributions to
 * structured typesetting.
 */
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAXSTK	100	/* Stack size */
#define MAXBR	100	/* Max number of bracket pairs known */
#define MAXCMDS	600	/* Max number of commands known */

static void addcmd(char *);
static void addmac(const char *);
static int binsrch(const char *);
static void checkknown(const char *);
static void chkcmd(const char *, const char *);
static void complain(int);
static int eq(const char *, const char *);
static void nomatch(const char *);
static void pe(int);
static void process(FILE *);
static void prop(int);
static void usage(void);

/*
 * The stack on which we remember what we've seen so far.
 */
static struct stkstr {
	int opno;	/* number of opening bracket */
	int pl;		/* '+', '-', ' ' for \s, 1 for \f, 0 for .ft */
	int parm;	/* parm to size, font, etc */
	int lno;	/* line number */
} stk[MAXSTK];
static int stktop;

/*
 * The kinds of opening and closing brackets.
 */
static struct brstr {
	const char *opbr;
	const char *clbr;
} br[MAXBR] = {
	/* A few bare bones troff commands */
#define SZ	0
	{"sz",	"sz"},	/* also \s */
#define FT	1
	{"ft",	"ft"},	/* also \f */
	/* the -mm package */
	{"AL",	"LE"},
	{"AS",	"AE"},
	{"BL",	"LE"},
	{"BS",	"BE"},
	{"DF",	"DE"},
	{"DL",	"LE"},
	{"DS",	"DE"},
	{"FS",	"FE"},
	{"ML",	"LE"},
	{"NS",	"NE"},
	{"RL",	"LE"},
	{"VL",	"LE"},
	/* the -ms package */
	{"AB",	"AE"},
	{"BD",	"DE"},
	{"CD",	"DE"},
	{"DS",	"DE"},
	{"FS",	"FE"},
	{"ID",	"DE"},
	{"KF",	"KE"},
	{"KS",	"KE"},
	{"LD",	"DE"},
	{"LG",	"NL"},
	{"QS",	"QE"},
	{"RS",	"RE"},
	{"SM",	"NL"},
	{"XA",	"XE"},
	{"XS",	"XE"},
	/* The -me package */
	{"(b",	")b"},
	{"(c",	")c"},
	{"(d",	")d"},
	{"(f",	")f"},
	{"(l",	")l"},
	{"(q",	")q"},
	{"(x",	")x"},
	{"(z",	")z"},
	/* The -mdoc package */
	{"Ao",  "Ac"},
	{"Bd",  "Ed"},
	{"Bk",  "Ek"},
	{"Bo",  "Bc"},
	{"Do",  "Dc"},
	{"Fo",  "Fc"},
	{"Oo",  "Oc"},
	{"Po",  "Pc"},
	{"Qo",  "Qc"},
	{"Rs",  "Re"},
	{"So",  "Sc"},
	{"Xo",  "Xc"},
	/* Things needed by preprocessors */
	{"EQ",	"EN"},
	{"TS",	"TE"},
	/* Refer */
	{"[",	"]"},
	{0,	0}
};

/*
 * All commands known to nroff, plus macro packages.
 * Used so we can complain about unrecognized commands.
 */
static const char *knowncmds[MAXCMDS] = {
"$c", "$f", "$h", "$p", "$s", "%A", "%B", "%C", "%D", "%I", "%J", "%N", "%O",
"%P", "%Q", "%R", "%T", "%V", "(b", "(c", "(d", "(f", "(l", "(q", "(t", "(x",
"(z", ")b", ")c", ")d", ")f", ")l", ")q", ")t", ")x", ")z", "++", "+c", "1C",
"1c", "2C", "2c", "@(", "@)", "@C", "@D", "@F", "@I", "@M", "@c", "@e", "@f",
"@h", "@m", "@n", "@o", "@p", "@r", "@t", "@z", "AB", "AE", "AF", "AI", "AL",
"AM", "AS", "AT", "AU", "AX", "Ac", "Ad", "An", "Ao", "Ap", "Aq", "Ar", "At",
"B", "B" , "B1", "B2", "BD", "BE", "BG", "BL", "BS", "BT", "BX", "Bc", "Bd",
"Bf", "Bk", "Bl", "Bo", "Bq", "Bsx", "Bx", "C1", "C2", "CD", "CM", "CT", "Cd",
"Cm", "D", "D" , "D1", "DA", "DE", "DF", "DL", "DS", "DT", "Db", "Dc", "Dd",
"Dl", "Do", "Dq", "Dt", "Dv", "EC", "EF", "EG", "EH", "EM", "EN", "EQ", "EX",
"Ec", "Ed", "Ef", "Ek", "El", "Em", "Eo", "Er", "Ev", "FA", "FD", "FE", "FG",
"FJ", "FK", "FL", "FN", "FO", "FQ", "FS", "FV", "FX", "Fa", "Fc", "Fd", "Fl",
"Fn", "Fo", "Ft", "Fx", "H", "H" , "HC", "HD", "HM", "HO", "HU", "I", "I" ,
"ID", "IE", "IH", "IM", "IP", "IX", "IZ", "Ic", "In", "It", "KD", "KE", "KF",
"KQ", "KS", "LB", "LC", "LD", "LE", "LG", "LI", "LP", "Lb", "Li", "MC", "ME",
"MF", "MH", "ML", "MR", "MT", "ND", "NE", "NH", "NL", "NP", "NS", "Nd", "Nm",
"No", "Ns", "Nx", "OF", "OH", "OK", "OP", "Oc", "Oo", "Op", "Os", "Ot", "Ox",
"P", "P" , "P1", "PF", "PH", "PP", "PT", "PX", "PY", "Pa", "Pc", "Pf", "Po",
"Pp", "Pq", "QE", "QP", "QS", "Qc", "Ql", "Qo", "Qq", "R", "R" , "RA", "RC",
"RE", "RL", "RP", "RQ", "RS", "RT", "Re", "Rs", "S", "S" , "S0", "S2", "S3",
"SA", "SG", "SH", "SK", "SM", "SP", "SY", "Sc", "Sh", "Sm", "So", "Sq", "Ss",
"St", "Sx", "Sy", "T&", "TA", "TB", "TC", "TD", "TE", "TH", "TL", "TM", "TP",
"TQ", "TR", "TS", "TX", "Tn", "UL", "US", "UX", "Ud", "Ux", "VL", "Va", "Vt",
"WC", "WH", "XA", "XD", "XE", "XF", "XK", "XP", "XS", "Xc", "Xo", "Xr", "[",
"[" , "[-", "[0", "[1", "[2", "[3", "[4", "[5", "[<", "[>", "[]", "\\{", "\\}",
"]", "]" , "]-", "]<", "]>", "][", "ab", "ac", "ad", "af", "am", "ar", "as",
"b", "b" , "ba", "bc", "bd", "bi", "bl", "bp", "br", "bx", "c.", "c2", "cc",
"ce", "cf", "ch", "chop", "cs", "ct", "cu", "da", "de", "di", "dl", "dn", "do",
"ds", "dt", "dw", "dy", "ec", "ef", "eh", "el", "em", "eo", "ep", "ev", "evc",
"ex", "fallback", "fc", "feature", "fi", "fl", "flig", "fo", "fp", "ft", "ftr",
"fz", "fzoom", "hc", "he", "hidechar", "hl", "hp", "ht", "hw", "hx", "hy",
"hylang", "i", "i" , "ie", "if", "ig", "in", "ip", "it", "ix", "kern",
"kernafter", "kernbefore", "kernpair", "lc", "lc_ctype", "lg", "lhang", "li",
"ll", "ln", "lo", "lp", "ls", "lt", "m1", "m2", "m3", "m4", "mc", "mk", "mo",
"n1", "n2", "na", "ne", "nf", "nh", "nl", "nm", "nn", "np", "nr", "ns", "nx",
"of", "oh", "os", "pa", "papersize", "pc", "pi", "pl", "pm", "pn", "po", "pp",
"ps", "q", "q" , "r", "r" , "rb", "rd", "re", "recursionlimit", "return",
"rhang", "rm", "rn", "ro", "rr", "rs", "rt", "sb", "sc", "sh", "shift", "sk",
"so", "sp", "ss", "st", "sv", "sz", "ta", "tc", "th", "ti", "tl", "tm", "tp",
"tr", "track", "u", "uf", "uh", "ul", "vs", "wh", "xflag", "xp", "yr",
0
};

static int	lineno;		/* current line number in input file */
static const char *cfilename;	/* name of current file */
static int	nfiles;		/* number of files to process */
static int	fflag;		/* -f: ignore \f */
static int	sflag;		/* -s: ignore \s */
static int	ncmds;		/* size of knowncmds */
static int	slot;		/* slot in knowncmds found by binsrch */

int
main(int argc, char **argv)
{
	FILE *f;
	int i;
	char *cp;
	char b1[4];

	/* Figure out how many known commands there are */
	while (knowncmds[ncmds])
		ncmds++;
	while (argc > 1 && argv[1][0] == '-') {
		switch(argv[1][1]) {

		/* -a: add pairs of macros */
		case 'a':
			i = strlen(argv[1]) - 2;
			if (i % 6 != 0)
				usage();
			/* look for empty macro slots */
			for (i=0; br[i].opbr; i++)
				;
			for (cp=argv[1]+3; cp[-1]; cp += 6) {
				char *tmp;

				if (i >= MAXBR)
					errx(1, "too many pairs");
				if ((tmp = malloc(3)) == NULL)
					err(1, "malloc");
				strlcpy(tmp, cp, 3);
				br[i].opbr = tmp;
				if ((tmp = malloc(3)) == NULL)
					err(1, "malloc");
				strlcpy(tmp, cp+3, 3);
				br[i].clbr = tmp;
				addmac(br[i].opbr);	/* knows pairs are also known cmds */
				addmac(br[i].clbr);
				i++;
			}
			break;

		/* -c: add known commands */
		case 'c':
			i = strlen(argv[1]) - 2;
			if (i % 3 != 0)
				usage();
			for (cp=argv[1]+3; cp[-1]; cp += 3) {
				if (cp[2] && cp[2] != '.')
					usage();
				strncpy(b1, cp, 2);
				b1[2] = '\0';
				addmac(b1);
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
			if (f == NULL)
				warn("%s", cfilename);
			else {
				process(f);
				fclose(f);
			}
		}
	} else {
		cfilename = "stdin";
		process(stdin);
	}
	exit(0);
}

static void
usage(void)
{
	fprintf(stderr,
	"usage: checknr [-a.xx.yy.xx.yy...] [-c.xx.xx.xx...] [-s] [-f] file\n");
	exit(1);
}

static void
process(FILE *f)
{
	int i, n;
	char mac[64];	/* The current macro or nroff command */
	char *line;
	size_t linecap;
	int pl;

	line = NULL;
	linecap = 0;
	stktop = -1;
	for (lineno = 1; getline(&line, &linecap, f) > 0; lineno++) {
		if (line[0] == '.') {
			/*
			 * find and isolate the macro/command name.
			 */
			strncpy(mac, line+1, 4);
			if (isspace(mac[0])) {
				pe(lineno);
				printf("Empty command\n");
			} else if (isspace(mac[1])) {
				mac[1] = 0;
			} else if (isspace(mac[2])) {
				mac[2] = 0;
			} else if (mac[0] != '\\' || mac[1] != '\"') {
				pe(lineno);
				printf("Command too long\n");
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
					pl = line[++i];
					if (isdigit(pl)) {
						n = pl - '0';
						pl = ' ';
					} else
						n = 0;
					while (isdigit(line[++i]))
						n = 10 * n + line[i] - '0';
					i--;
					if (n == 0) {
						if (stktop >= 0 &&
						    stk[stktop].opno == SZ) {
							stktop--;
						} else {
							pe(lineno);
							printf("unmatched \\s0\n");
						}
					} else {
						stk[++stktop].opno = SZ;
						stk[stktop].pl = pl;
						stk[stktop].parm = n;
						stk[stktop].lno = lineno;
					}
				} else if (!fflag && line[i] == 'f') {
					n = line[++i];
					if (n == 'P') {
						if (stktop >= 0 && 
						    stk[stktop].opno == FT) {
							stktop--;
						} else {
							pe(lineno);
							printf("unmatched \\fP\n");
						}
					} else {
						stk[++stktop].opno = FT;
						stk[stktop].pl = 1;
						stk[stktop].parm = n;
						stk[stktop].lno = lineno;
					}
				}
			}
	}
	free(line);
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
	else switch(stk[i].opno) {
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

static void
chkcmd(const char *line __unused, const char *mac)
{
	int i;

	/*
	 * Check to see if it matches top of stack.
	 */
	if (stktop >= 0 && eq(mac, br[stk[stktop].opno].clbr))
		stktop--;	/* OK. Pop & forget */
	else {
		/* No. Maybe it's an opener */
		for (i=0; br[i].opbr; i++) {
			if (eq(mac, br[i].opbr)) {
				/* Found. Push it. */
				stktop++;
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
nomatch(const char *mac)
{
	int i, j;

	/*
	 * Look for a match further down on stack
	 * If we find one, it suggests that the stuff in
	 * between is supposed to match itself.
	 */
	for (j=stktop; j>=0; j--)
		if (eq(mac,br[stk[j].opno].clbr)) {
			/* Found.  Make a good diagnostic. */
			if (j == stktop-2) {
				/*
				 * Check for special case \fx..\fR and don't
				 * complain.
				 */
				if (stk[j+1].opno==FT && stk[j+1].parm!='R'
				 && stk[j+2].opno==FT && stk[j+2].parm=='R') {
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
			} else for (i=j+1; i <= stktop; i++) {
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
eq(const char *s1, const char *s2)
{
	return (strcmp(s1, s2) == 0);
}

/* print the first part of an error message, given the line number */
static void
pe(int linen)
{
	if (nfiles > 1)
		printf("%s: ", cfilename);
	printf("%d: ", linen);
}

static void
checkknown(const char *mac)
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
	while (isspace(*mac))
		mac++;
	if (*mac == 0) {
		pe(lineno);
		printf("illegal define: %s\n", line);
		return;
	}
	mac[2] = 0;
	if (isspace(mac[1]) || mac[1] == '\\')
		mac[1] = 0;
	if (ncmds >= MAXCMDS) {
		printf("Only %d known commands allowed\n", MAXCMDS);
		exit(1);
	}
	addmac(mac);
}

/*
 * Add mac to the list.  We should really have some kind of tree
 * structure here but this is a quick-and-dirty job and I just don't
 * have time to mess with it.  (I wonder if this will come back to haunt
 * me someday?)  Anyway, I claim that .de is fairly rare in user
 * nroff programs, and the register loop below is pretty fast.
 */
static void
addmac(const char *mac)
{
	const char **src, **dest, **loc;

	if (binsrch(mac) >= 0){	/* it's OK to redefine something */
#ifdef DEBUG
		printf("binsrch(%s) -> already in table\n", mac);
#endif
		return;
	}
	/* binsrch sets slot as a side effect */
#ifdef DEBUG
	printf("binsrch(%s) -> %d\n", mac, slot);
#endif
	loc = &knowncmds[slot];
	src = &knowncmds[ncmds-1];
	dest = src+1;
	while (dest > loc)
		*dest-- = *src--;
	if ((*loc = strdup(mac)) == NULL)
		err(1, "strdup");
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
binsrch(const char *mac)
{
	const char *p;	/* pointer to current cmd in list */
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
			d = p[1] - mac[1];
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
