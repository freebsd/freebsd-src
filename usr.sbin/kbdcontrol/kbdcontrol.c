/*-
 * Copyright (c) 1994-1995 Søren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
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

#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/kbio.h>
#include <sys/consio.h>
#include "path.h"
#include "lex.h"

/*
 * PASTE isn't defined in 4.x, but we need it to bridge to 5.0-current
 * so define it here as a stop gap transition measure.
 */
#ifndef PASTE
#define PASTE		0xa3		/* paste from cut-paste buffer */
#endif

char ctrl_names[32][4] = {
	"nul", "soh", "stx", "etx", "eot", "enq", "ack", "bel",
	"bs ", "ht ", "nl ", "vt ", "ff ", "cr ", "so ", "si ",
	"dle", "dc1", "dc2", "dc3", "dc4", "nak", "syn", "etb",
	"can", "em ", "sub", "esc", "fs ", "gs ", "rs ", "us "
	};

char acc_names[15][5] = {
	"dgra", "dacu", "dcir", "dtil", "dmac", "dbre", "ddot",
	"duml", "dsla", "drin", "dced", "dapo", "ddac", "dogo", 
	"dcar",
	};

char acc_names_u[15][5] = {
	"DGRA", "DACU", "DCIR", "DTIL", "DMAC", "DBRE", "DDOT",
	"DUML", "DSLA", "DRIN", "DCED", "DAPO", "DDAC", "DOGO", 
	"DCAR",
	};

char fkey_table[96][MAXFK] = {
/* 01-04 */	"\033[M", "\033[N", "\033[O", "\033[P",
/* 05-08 */	"\033[Q", "\033[R", "\033[S", "\033[T",
/* 09-12 */	"\033[U", "\033[V", "\033[W", "\033[X",
/* 13-16 */	"\033[Y", "\033[Z", "\033[a", "\033[b",
/* 17-20 */	"\033[c", "\033[d", "\033[e", "\033[f",
/* 21-24 */	"\033[g", "\033[h", "\033[i", "\033[j",
/* 25-28 */	"\033[k", "\033[l", "\033[m", "\033[n",
/* 29-32 */	"\033[o", "\033[p", "\033[q", "\033[r",
/* 33-36 */	"\033[s", "\033[t", "\033[u", "\033[v",
/* 37-40 */	"\033[w", "\033[x", "\033[y", "\033[z",
/* 41-44 */	"\033[@", "\033[[", "\033[\\","\033[]",
/* 45-48 */     "\033[^", "\033[_", "\033[`", "\033[{",
/* 49-52 */	"\033[H", "\033[A", "\033[I", "-"     ,
/* 53-56 */	"\033[D", "\033[E", "\033[C", "+"     ,
/* 57-60 */	"\033[F", "\033[B", "\033[G", "\033[L",
/* 61-64 */     "\177",   "\033[J", "\033[~", "\033[}",
/* 65-68 */	""      , ""      , ""      , ""      ,
/* 69-72 */	""      , ""      , ""      , ""      ,
/* 73-76 */	""      , ""      , ""      , ""      ,
/* 77-80 */	""      , ""      , ""      , ""      ,
/* 81-84 */	""      , ""      , ""      , ""      ,
/* 85-88 */	""      , ""      , ""      , ""      ,
/* 89-92 */	""      , ""      , ""      , ""      ,
/* 93-96 */	""      , ""      , ""      , ""      ,
	};

const int	delays[]  = {250, 500, 750, 1000};
const int	repeats[] = { 34,  38,  42,  46,  50,  55,  59,  63,
			      68,  76,  84,  92, 100, 110, 118, 126,
			     136, 152, 168, 184, 200, 220, 236, 252,
			     272, 304, 336, 368, 400, 440, 472, 504};
const int	ndelays = (sizeof(delays) / sizeof(int));
const int	nrepeats = (sizeof(repeats) / sizeof(int));
int 		hex = 0;
int 		number;
char 		letter;
int		token;

static void usage __P((void));

char *
nextarg(int ac, char **av, int *indp, int oc)
{
	if (*indp < ac)
		return(av[(*indp)++]);
	warnx("option requires two arguments -- %c", oc);
	usage();
	return("");
}


char *
mkfullname(const char *s1, const char *s2, const char *s3)
{
	static char	*buf = NULL;
	static int	bufl = 0;
	int		f;

	f = strlen(s1) + strlen(s2) + strlen(s3) + 1;
	if (f > bufl)
		if (buf)
			buf = (char *)realloc(buf, f);
		else
			buf = (char *)malloc(f);
	if (!buf) {
		bufl = 0;
		return(NULL);
	}

	bufl = f;
	strcpy(buf, s1);
	strcat(buf, s2);
	strcat(buf, s3);
	return(buf);
}


int
get_entry()
{
	switch ((token = yylex())) {
	case TNOP:
		return NOP | 0x100;
	case TLSH:
		return LSH | 0x100;
	case TRSH:
		return RSH | 0x100;
	case TCLK:
		return CLK | 0x100;
	case TNLK:
		return NLK | 0x100;
	case TSLK:
		return SLK | 0x100;
	case TBTAB:
		return BTAB | 0x100;
	case TLALT:
		return LALT | 0x100;
	case TLCTR:
		return LCTR | 0x100;
	case TNEXT:
		return NEXT | 0x100;
	case TPREV:
		return PREV | 0x100;
	case TRCTR:
		return RCTR | 0x100;
	case TRALT:
		return RALT | 0x100;
	case TALK:
		return ALK | 0x100;
	case TASH:
		return ASH | 0x100;
	case TMETA:
		return META | 0x100;
	case TRBT:
		return RBT | 0x100;
	case TDBG:
		return DBG | 0x100;
	case TSUSP:
		return SUSP | 0x100;
	case TSPSC:
		return SPSC | 0x100;
	case TPANIC:
		return PNC | 0x100;
	case TLSHA:
		return LSHA | 0x100;
	case TRSHA:
		return RSHA | 0x100;
	case TLCTRA:
		return LCTRA | 0x100;
	case TRCTRA:
		return RCTRA | 0x100;
	case TLALTA:
		return LALTA | 0x100;
	case TRALTA:
		return RALTA | 0x100;
	case THALT:
		return HALT | 0x100;
	case TPDWN:
		return PDWN | 0x100;
	case TPASTE:
		return PASTE | 0x100;
	case TACC:
		if (ACC(number) > L_ACC)
			return -1;
		return ACC(number) | 0x100;
	case TFUNC:
		if (F(number) > L_FN)
			return -1;
		return F(number) | 0x100;
	case TSCRN:
		if (S(number) > L_SCR)
			return -1;
		return S(number) | 0x100;
	case TLET:
		return (unsigned char)letter;
	case TNUM:
		if (number < 0 || number > 255)
			return -1;
		return number;
	default:
		return -1;
	}
}

int
get_definition_line(FILE *fd, keymap_t *keymap, accentmap_t *accentmap)
{
	int c;

	yyin = fd;

	if (token < 0)
		token = yylex();
	switch (token) { 
	case TNUM:
		c = get_key_definition_line(keymap);
		if (c < 0)
			errx(1, "invalid key definition");
		if (c > keymap->n_keys)
			keymap->n_keys = c;
		break;
	case TACC:
		c = get_accent_definition_line(accentmap);
		if (c < 0)
			errx(1, "invalid accent key definition");
		if (c > accentmap->n_accs)
			accentmap->n_accs = c;
		break;
	case 0:
		/* EOF */
		return -1;
	default:
		errx(1, "illegal definition line");
	}
	return c;
}

int
get_key_definition_line(keymap_t *map)
{
	int i, def, scancode;

	/* check scancode number */
	if (number < 0 || number >= NUM_KEYS)
		return -1;
	scancode = number;

	/* get key definitions */
	map->key[scancode].spcl = 0;
	for (i=0; i<NUM_STATES; i++) {
		if ((def = get_entry()) == -1)
			return -1;
		if (def & 0x100)
			map->key[scancode].spcl |= (0x80 >> i);
		map->key[scancode].map[i] = def & 0xFF;
	}
	/* get lock state key def */
	if ((token = yylex()) != TFLAG)
		return -1;
	map->key[scancode].flgs = number;
	token = yylex();
	return (scancode + 1);
}

int
get_accent_definition_line(accentmap_t *map)
{
	int accent;
	int c1, c2;
	int i;

	if (ACC(number) < F_ACC || ACC(number) > L_ACC)
		/* number out of range */
		return -1;
	accent = number;
	if (map->acc[accent].accchar != 0) {
		/* this entry has already been defined before! */
		errx(1, "duplicated accent key definition");
	}

	switch ((token = yylex())) {
	case TLET:
		map->acc[accent].accchar = letter;
		break;
	case TNUM:
		map->acc[accent].accchar = number;
		break;
	default:
		return -1;
	}

	for (i = 0; (token = yylex()) == '(';) {
		switch ((token = yylex())) {
		case TLET:
			c1 = letter;
			break;
		case TNUM:
			c1 = number;
			break;
		default:
			return -1;
		}
		switch ((token = yylex())) {
		case TLET:
			c2 = letter;
			break;
		case TNUM:
			c2 = number;
			break;
		default:
			return -1;
		}
		if ((token = yylex()) != ')')
			return -1;
		if (i >= NUM_ACCENTCHARS) {
			warnx("too many accented characters, ignored");
			continue;
		}
		map->acc[accent].map[i][0] = c1;
		map->acc[accent].map[i][1] = c2;
		++i;
	}
	return (accent + 1);
}

void
print_entry(FILE *fp, int value)
{
	int val = value & 0xFF;

	switch (value) {
	case NOP | 0x100:
		fprintf(fp, " nop   ");
		break;
	case LSH | 0x100:
		fprintf(fp, " lshift");
		break;
	case RSH | 0x100:
		fprintf(fp, " rshift");
		break;
	case CLK | 0x100:
		fprintf(fp, " clock ");
		break;
	case NLK | 0x100:
		fprintf(fp, " nlock ");
		break;
	case SLK | 0x100:
		fprintf(fp, " slock ");
		break;
	case BTAB | 0x100:
		fprintf(fp, " btab  ");
		break;
	case LALT | 0x100:
		fprintf(fp, " lalt  ");
		break;
	case LCTR | 0x100:
		fprintf(fp, " lctrl ");
		break;
	case NEXT | 0x100:
		fprintf(fp, " nscr  ");
		break;
	case PREV | 0x100:
		fprintf(fp, " pscr  ");
		break;
	case RCTR | 0x100:
		fprintf(fp, " rctrl ");
		break;
	case RALT | 0x100:
		fprintf(fp, " ralt  ");
		break;
	case ALK | 0x100:
		fprintf(fp, " alock ");
		break;
	case ASH | 0x100:
		fprintf(fp, " ashift");
		break;
	case META | 0x100:
		fprintf(fp, " meta  ");
		break;
	case RBT | 0x100:
		fprintf(fp, " boot  ");
		break;
	case DBG | 0x100:
		fprintf(fp, " debug ");
		break;
	case SUSP | 0x100:
		fprintf(fp, " susp  ");
		break;
	case SPSC | 0x100:
		fprintf(fp, " saver ");
		break;
	case PNC | 0x100:
		fprintf(fp, " panic ");
		break;
	case LSHA | 0x100:
		fprintf(fp, " lshifta");
		break;
	case RSHA | 0x100:
		fprintf(fp, " rshifta");
		break;
	case LCTRA | 0x100:
		fprintf(fp, " lctrla");
		break;
	case RCTRA | 0x100:
		fprintf(fp, " rctrla");
		break;
	case LALTA | 0x100:
		fprintf(fp, " lalta ");
		break;
	case RALTA | 0x100:
		fprintf(fp, " ralta ");
		break;
	case HALT | 0x100:
		fprintf(fp, " halt  ");
		break;
	case PDWN | 0x100:
		fprintf(fp, " pdwn  ");
		break;
	case PASTE | 0x100:
		fprintf(fp, " paste ");
		break;
	default:
		if (value & 0x100) {
		 	if (val >= F_FN && val <= L_FN)
				fprintf(fp, " fkey%02d", val - F_FN + 1);
		 	else if (val >= F_SCR && val <= L_SCR)
				fprintf(fp, " scr%02d ", val - F_SCR + 1);
		 	else if (val >= F_ACC && val <= L_ACC)
				fprintf(fp, " %-6s", acc_names[val - F_ACC]);
			else if (hex)
				fprintf(fp, " 0x%02x  ", val);
			else
				fprintf(fp, " %3d   ", val);
		}
		else {
			if (val < ' ')
				fprintf(fp, " %s   ", ctrl_names[val]);
			else if (val == 127)
				fprintf(fp, " del   ");
			else if (isascii(val) && isprint(val))
				fprintf(fp, " '%c'   ", val);
			else if (hex)
				fprintf(fp, " 0x%02x  ", val);
			else
				fprintf(fp, " %3d   ", val);
		}
	}
}


void
print_key_definition_line(FILE *fp, int scancode, struct keyent_t *key)
{
	int i;

	/* print scancode number */
	if (hex)
		fprintf(fp, " 0x%02x  ", scancode);
	else
		fprintf(fp, "  %03d  ", scancode);

	/* print key definitions */
	for (i=0; i<NUM_STATES; i++) {
		if (key->spcl & (0x80 >> i))
			print_entry(fp, key->map[i] | 0x100);
		else
			print_entry(fp, key->map[i]);
	}

	/* print lock state key def */
	switch (key->flgs) {
	case 0:
		fprintf(fp, "  O\n");
		break;
	case 1:
		fprintf(fp, "  C\n");
		break;
	case 2:
		fprintf(fp, "  N\n");
		break;
	case 3:
		fprintf(fp, "  B\n");
		break;
	}
}

void
print_accent_definition_line(FILE *fp, int accent, struct acc_t *key)
{
	int c;
	int i;

	if (key->accchar == 0)
		return;

	/* print accent number */
	fprintf(fp, "  %-6s", acc_names[accent]);
	if (isascii(key->accchar) && isprint(key->accchar))
		fprintf(fp, "'%c'  ", key->accchar);
	else if (hex)
		fprintf(fp, "0x%02x ", key->accchar);
	else
		fprintf(fp, "%03d  ", key->accchar);

	for (i = 0; i < NUM_ACCENTCHARS; ++i) {
		c = key->map[i][0];
		if (c == 0)
			break;
		if ((i > 0) && ((i % 4) == 0))
			fprintf(fp, "\n             ");
		if (isascii(c) && isprint(c))
			fprintf(fp, "( '%c' ", c);
		else if (hex)
			fprintf(fp, "(0x%02x ", c);
		else
			fprintf(fp, "( %03d ", c);
		c = key->map[i][1];
		if (isascii(c) && isprint(c))
			fprintf(fp, "'%c' ) ", c);
		else if (hex)
			fprintf(fp, "0x%02x) ", c);
		else
			fprintf(fp, "%03d ) ", c);
	}
	fprintf(fp, "\n");
}

void
dump_entry(int value)
{
	if (value & 0x100) {
		value &= 0x00ff;
		switch (value) {
		case NOP:
			printf("  NOP, ");
			break;
		case LSH:
			printf("  LSH, ");
			break;
		case RSH:
			printf("  RSH, ");
			break;
		case CLK:
			printf("  CLK, ");
			break;
		case NLK:
			printf("  NLK, ");
			break;
		case SLK:
			printf("  SLK, ");
			break;
		case BTAB:
			printf(" BTAB, ");
			break;
		case LALT:
			printf(" LALT, ");
			break;
		case LCTR:
			printf(" LCTR, ");
			break;
		case NEXT:
			printf(" NEXT, ");
			break;
		case PREV:
			printf(" PREV, ");
			break;
		case RCTR:
			printf(" RCTR, ");
			break;
		case RALT:
			printf(" RALT, ");
			break;
		case ALK:
			printf("  ALK, ");
			break;
		case ASH:
			printf("  ASH, ");
			break;
		case META:
			printf(" META, ");
			break;
		case RBT:
			printf("  RBT, ");
			break;
		case DBG:
			printf("  DBG, ");
			break;
		case SUSP:
			printf(" SUSP, ");
			break;
		case SPSC:
			printf(" SPSC, ");
			break;
		case PNC:
			printf("  PNC, ");
			break;
		case LSHA:
			printf(" LSHA, ");
			break;
		case RSHA:
			printf(" RSHA, ");
			break;
		case LCTRA:
			printf("LCTRA, ");
			break;
		case RCTRA:
			printf("RCTRA, ");
			break;
		case LALTA:
			printf("LALTA, ");
			break;
		case RALTA:
			printf("RALTA, ");
			break;
		case HALT:
			printf(" HALT, ");
			break;
		case PDWN:
			printf(" PDWN, ");
			break;
		case PASTE:
			printf("PASTE, ");
			break;
		default:
	 		if (value >= F_FN && value <= L_FN)
				printf(" F(%2d),", value - F_FN + 1);
	 		else if (value >= F_SCR && value <= L_SCR)
				printf(" S(%2d),", value - F_SCR + 1);
	 		else if (value >= F_ACC && value <= L_ACC)
				printf(" %-4s, ", acc_names_u[value - F_ACC]);
			else
				printf(" 0x%02X, ", value);
			break;
		}
	} else if (value == '\'') {
		printf(" '\\'', ");
	} else if (value == '\\') {
		printf(" '\\\\', ");
	} else if (isascii(value) && isprint(value)) {
		printf("  '%c', ", value);
	} else {
		printf(" 0x%02X, ", value);
	}
}

void
dump_key_definition(char *name, keymap_t *keymap)
{
	int	i, j;

	printf("static keymap_t keymap_%s = { 0x%02x, {\n",
	       name, (unsigned)keymap->n_keys);
	printf(
"/*                                                         alt\n"
" * scan                       cntrl          alt    alt   cntrl\n"
" * code  base   shift  cntrl  shift   alt   shift  cntrl  shift    spcl flgs\n"
" * ---------------------------------------------------------------------------\n"
" */\n");
	for (i = 0; i < keymap->n_keys; i++) {
		printf("/*%02x*/{{", i);
		for (j = 0; j < NUM_STATES; j++) {
			if (keymap->key[i].spcl & (0x80 >> j))
				dump_entry(keymap->key[i].map[j] | 0x100);
			else
				dump_entry(keymap->key[i].map[j]);
		}
		printf("}, 0x%02X,0x%02X },\n",
		       (unsigned)keymap->key[i].spcl, 
		       (unsigned)keymap->key[i].flgs);
	}
	printf("} };\n\n");
}

void
dump_accent_definition(char *name, accentmap_t *accentmap)
{
	int i, j;
	int c;

	printf("static accentmap_t accentmap_%s = { %d",
		name, accentmap->n_accs); 
	if (accentmap->n_accs <= 0) {
		printf(" };\n\n");
		return;
	}
	printf(", {\n");
	for (i = 0; i < NUM_DEADKEYS; i++) {
		printf("    /* %s=%d */\n    {", acc_names[i], i);
		c = accentmap->acc[i].accchar;
		if (c == '\'')
			printf(" '\\'', {");
		else if (c == '\\')
			printf(" '\\\\', {");
		else if (isascii(c) && isprint(c))
			printf("  '%c', {", c);
		else if (c == 0) {
			printf(" 0x00 }, \n");
			continue;
		} else
			printf(" 0x%02x, {", c);
		for (j = 0; j < NUM_ACCENTCHARS; j++) {
			c = accentmap->acc[i].map[j][0]; 
			if (c == 0)
				break;
			if ((j > 0) && ((j % 4) == 0))
				printf("\n\t     ");
			if (isascii(c) && isprint(c))
				printf(" {  '%c',", c);
			else
				printf(" { 0x%02x,", c); 
			printf("0x%02x },", accentmap->acc[i].map[j][1]);
		}
		printf(" }, },\n");
	}
	printf("} };\n\n");
}

void
load_keymap(char *opt, int dumponly)
{
	keymap_t keymap;
	accentmap_t accentmap;
	FILE	*fd;
	int	i, j;
	char	*name, *cp;
	char	*prefix[]  = {"", "", KEYMAP_PATH, NULL};
	char	*postfix[] = {"", ".kbd", NULL};

	cp = getenv("KEYMAP_PATH");
	if (cp != NULL)
		asprintf(&(prefix[0]), "%s/", cp);

	fd = NULL;
	for (i=0; prefix[i] && fd == NULL; i++) {
		for (j=0; postfix[j] && fd == NULL; j++) {
			name = mkfullname(prefix[i], opt, postfix[j]);
			fd = fopen(name, "r");
		}
	}
	if (fd == NULL) {
		warn("keymap file not found");
		return;
	}
	memset(&keymap, 0, sizeof(keymap));
	memset(&accentmap, 0, sizeof(accentmap));
	token = -1;
	while (1) {
		if (get_definition_line(fd, &keymap, &accentmap) < 0)
			break;
    	}
	if (dumponly) {
		/* fix up the filename to make it a valid C identifier */
		for (cp = opt; *cp; cp++)
			if (!isalpha(*cp) && !isdigit(*cp)) *cp = '_';
		printf("/*\n"
		       " * Automatically generated from %s.\n"
	               " * DO NOT EDIT!\n"
		       " */\n", name);
		dump_key_definition(opt, &keymap);
		dump_accent_definition(opt, &accentmap);
		return;
	}
	if ((keymap.n_keys > 0) && (ioctl(0, PIO_KEYMAP, &keymap) < 0)) {
		warn("setting keymap");
		fclose(fd);
		return;
	}
	if ((accentmap.n_accs > 0) 
		&& (ioctl(0, PIO_DEADKEYMAP, &accentmap) < 0)) {
		warn("setting accentmap");
		fclose(fd);
		return;
	}
}

void
print_keymap()
{
	keymap_t keymap;
	accentmap_t accentmap;
	int i;

	if (ioctl(0, GIO_KEYMAP, &keymap) < 0)
		err(1, "getting keymap");
	if (ioctl(0, GIO_DEADKEYMAP, &accentmap) < 0)
		memset(&accentmap, 0, sizeof(accentmap));
    	printf(
"#                                                         alt\n"
"# scan                       cntrl          alt    alt   cntrl lock\n"
"# code  base   shift  cntrl  shift  alt    shift  cntrl  shift state\n"
"# ------------------------------------------------------------------\n"
    	);
	for (i=0; i<keymap.n_keys; i++)
		print_key_definition_line(stdout, i, &keymap.key[i]);

	printf("\n");
	for (i = 0; i < NUM_DEADKEYS; i++)
		print_accent_definition_line(stdout, i, &accentmap.acc[i]);

}


void
load_default_functionkeys()
{
	fkeyarg_t fkey;
	int i;

	for (i=0; i<NUM_FKEYS; i++) {
		fkey.keynum = i;
		strcpy(fkey.keydef, fkey_table[i]);
		fkey.flen = strlen(fkey_table[i]);
		if (ioctl(0, SETFKEY, &fkey) < 0)
			warn("setting function key");
	}
}

void
set_functionkey(char *keynumstr, char *string)
{
	fkeyarg_t fkey;

	if (!strcmp(keynumstr, "load") && !strcmp(string, "default")) {
		load_default_functionkeys();
		return;
	}
	fkey.keynum = atoi(keynumstr);
	if (fkey.keynum < 1 || fkey.keynum > NUM_FKEYS) {
		warnx("function key number must be between 1 and %d",
			NUM_FKEYS);
		return;
	}
	if ((fkey.flen = strlen(string)) > MAXFK) {
		warnx("function key string too long (%d > %d)",
			fkey.flen, MAXFK);
		return;
	}
	strcpy(fkey.keydef, string);
	fkey.keynum -= 1;
	if (ioctl(0, SETFKEY, &fkey) < 0)
		warn("setting function key");
}


void
set_bell_values(char *opt)
{
	int bell, duration, pitch;

	bell = 0;
	if (!strncmp(opt, "quiet.", 6)) {
		bell = 2;
		opt += 6;
	}
	if (!strcmp(opt, "visual"))
		bell |= 1;
	else if (!strcmp(opt, "normal"))
		duration = 5, pitch = 800;
	else if (!strcmp(opt, "off"))
		duration = 0, pitch = 0;
	else {
		char		*v1;

		bell = 0;
		duration = strtol(opt, &v1, 0);
		if ((duration < 0) || (*v1 != '.'))
			goto badopt;
		opt = ++v1;
		pitch = strtol(opt, &v1, 0);
		if ((pitch < 0) || (*opt == '\0') || (*v1 != '\0')) {
badopt:
			warnx("argument to -b must be DURATION.PITCH");
			return;
		}
		if (pitch != 0)
			pitch = 1193182 / pitch;	/* in Hz */
		duration /= 10;	/* in 10 m sec */
	}

	ioctl(0, CONS_BELLTYPE, &bell);
	if ((bell & ~2) == 0)
		fprintf(stderr, "[=%d;%dB", pitch, duration);
}


void
set_keyrates(char *opt)
{
	int arg[2];
	int repeat;
	int delay;
	int r, d;

	if (!strcmp(opt, "slow")) {
		delay = 1000, repeat = 500;
		d = 3, r = 31;
	} else if (!strcmp(opt, "normal")) {
		delay = 500, repeat = 125;
		d = 1, r = 15;
	} else if (!strcmp(opt, "fast")) {
		delay = repeat = 0;
		d = r = 0;
	} else {
		int		n;
		char		*v1;

		delay = strtol(opt, &v1, 0);
		if ((delay < 0) || (*v1 != '.'))
			goto badopt;
		opt = ++v1;
		repeat = strtol(opt, &v1, 0);
		if ((repeat < 0) || (*opt == '\0') || (*v1 != '\0')) {
badopt:
			warnx("argument to -r must be delay.repeat");
			return;
		}
		for (n = 0; n < ndelays - 1; n++)
			if (delay <= delays[n])
				break;
		d = n;
		for (n = 0; n < nrepeats - 1; n++)
			if (repeat <= repeats[n])
				break;
		r = n;
	}

	arg[0] = delay;
	arg[1] = repeat;
	if (ioctl(0, KDSETREPEAT, arg)) {
		if (ioctl(0, KDSETRAD, (d << 5) | r))
			warn("setting keyboard rate");
	}
}


void
set_history(char *opt)
{
	int size;

	size = atoi(opt);
	if ((*opt == '\0') || size < 0) {
		warnx("argument must be a positive number");
		return;
	}
	if (ioctl(0, CONS_HISTORY, &size) == -1)
		warn("setting history buffer size");
}

void
clear_history()
{

#ifdef CONS_CLRHIST
	if (ioctl(0, CONS_CLRHIST) == -1)
		warn("clear history buffer");
#else
	warnx("clearing history not supported");
#endif
}

static char
*get_kbd_type_name(int type)
{
	static struct {
		int type;
		char *name;
	} name_table[] = {
		{ KB_84,	"AT 84" },
		{ KB_101,	"AT 101/102" },
		{ KB_OTHER,	"generic" },
	};
	int i;

	for (i = 0; i < sizeof(name_table)/sizeof(name_table[0]); ++i) {
		if (type == name_table[i].type)
			return name_table[i].name;
	}
	return "unknown";
}

void
show_kbd_info(void)
{
	keyboard_info_t info;

	if (ioctl(0, KDGKBINFO, &info) == -1) {
		warn("unable to obtain keyboard information");
		return;
	}
	printf("kbd%d:\n", info.kb_index);
	printf("    %.*s%d, type:%s (%d)\n",
		sizeof(info.kb_name), info.kb_name, info.kb_unit,
		get_kbd_type_name(info.kb_type), info.kb_type);
}


void
set_keyboard(char *device)
{
	keyboard_info_t info;
	int fd;

	fd = open(device, O_RDONLY);
	if (fd < 0) {
		warn("cannot open %s", device);
		return;
	}
	if (ioctl(fd, KDGKBINFO, &info) == -1) {
		warn("unable to obtain keyboard information");
		close(fd);
		return;
	}
	/*
	 * The keyboard device driver won't release the keyboard by
	 * the following ioctl, but it automatically will, when the device 
	 * is closed.  So, we don't check error here.
	 */
	ioctl(fd, CONS_RELKBD, 0);
	close(fd);
#if 1
	printf("kbd%d\n", info.kb_index);
	printf("    %.*s%d, type:%s (%d)\n",
		sizeof(info.kb_name), info.kb_name, info.kb_unit,
		get_kbd_type_name(info.kb_type), info.kb_type);
#endif

	if (ioctl(0, CONS_SETKBD, info.kb_index) == -1)
		warn("unable to set keyboard");
}


void
release_keyboard(void)
{
	keyboard_info_t info;

	/*
	 * If stdin is not associated with a keyboard, the following ioctl
	 * will fail.
	 */
	if (ioctl(0, KDGKBINFO, &info) == -1) {
		warn("unable to obtain keyboard information");
		return;
	}
#if 1
	printf("kbd%d\n", info.kb_index);
	printf("    %.*s%d, type:%s (%d)\n",
		sizeof(info.kb_name), info.kb_name, info.kb_unit,
		get_kbd_type_name(info.kb_type), info.kb_type);
#endif
	if (ioctl(0, CONS_RELKBD, 0) == -1)
		warn("unable to release the keyboard");
}


static void
usage()
{
	fprintf(stderr, "%s\n%s\n%s\n",
"usage: kbdcontrol [-cdFKix] [-b duration.pitch | [quiet.]belltype]",
"                  [-r delay.repeat | speed] [-l mapfile] [-f # string]",
"                  [-h size] [-k device] [-L mapfile]");
	exit(1);
}


int
main(int argc, char **argv)
{
	int		opt;

	while((opt = getopt(argc, argv, "b:cdf:h:iKk:Fl:L:r:x")) != -1)
		switch(opt) {
			case 'b':
				set_bell_values(optarg);
				break;
			case 'c':
				clear_history();
				break;
			case 'd':
				print_keymap();
				break;
			case 'l':
				load_keymap(optarg, 0);
				break;
			case 'L':
				load_keymap(optarg, 1);
				break;
			case 'f':
				set_functionkey(optarg,
					nextarg(argc, argv, &optind, 'f'));
				break;
			case 'F':
				load_default_functionkeys();
				break;
			case 'h':
				set_history(optarg);
				break;
			case 'i':
				show_kbd_info();
				break;
			case 'K':
				release_keyboard();
				break;
			case 'k':
				set_keyboard(optarg);
				break;
			case 'r':
				set_keyrates(optarg);
				break;
			case 'x':
				hex = 1;
				break;
			default:
				usage();
		}
	if ((optind != argc) || (argc == 1))
		usage();
	exit(0);
}
