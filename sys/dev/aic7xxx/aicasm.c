/*
 * Adaptec 274x device driver for Linux.
 * Copyright (c) 1994 The University of Calgary Department of Computer Science.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Comments are started by `#' and continue to the end of the line; lines
 *  may be of the form:
 *
 *	<label>*
 *	<label>*  <undef-sym> = <value>
 *	<label>*  <opcode> <operand>*
 *
 *  A <label> is an <undef-sym> ending in a colon.  Spaces, tabs, and commas
 *  are token separators.
 *	
 *	$Id: aic7xxx.c,v 1.5 1995/01/22 00:46:52 gibbs Exp $
 */

/* #define _POSIX_SOURCE	1 */
#define _POSIX_C_SOURCE	2

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define MEMORY		512		/* 2^9 29-bit words */
#define MAXLINE		1024
#define MAXTOKEN	32
#define ADOTOUT		"a.out"
#define NOVALUE		-1

/*
 *  AIC-7770 register definitions
 */
#define R_SINDEX	0x65
#define R_ALLONES	0x69
#define R_ALLZEROS	0x6a
#define R_NONE		0x6a

static
char sccsid[] =
    "@(#)aic7770.c 1.10 94/07/22 jda";

int debug;
int lineno, LC;
char *filename;
FILE *ifp, *ofp;
unsigned char M[MEMORY][4];

void error(char *s)
{
	fprintf(stderr, "%s: %s at line %d\n", filename, s, lineno);
	exit(EXIT_FAILURE);
}

void *Malloc(size_t size)
{
	void *p = malloc(size);
	if (!p)
		error("out of memory");
	return(p);
}

void *Realloc(void *ptr, size_t size)
{
	void *p = realloc(ptr, size);
	if (!p)
		error("out of memory");
	return(p);
}

char *Strdup(char *s)
{
	char *p = (char *)Malloc(strlen(s) + 1);
	strcpy(p, s);
	return(p);
}

typedef struct sym_t {
	struct sym_t *next;		/* MUST BE FIRST */
	char *name;
	int value;
	int npatch, *patch;
} sym_t;

sym_t *head;

void define(char *name, int value)
{
	sym_t *p, *q;

	for (p = head, q = (sym_t *)&head; p; p = p->next) {
		if (!strcmp(p->name, name))
			error("redefined symbol");
		q = p;
	}

	p = q->next = (sym_t *)Malloc(sizeof(sym_t));
	p->next = NULL;
	p->name = Strdup(name);
	p->value = value;
	p->npatch = 0;
	p->patch = NULL;

	if (debug) {
		fprintf(stderr, "\"%s\" ", p->name);
		if (p->value != NOVALUE)
			fprintf(stderr, "defined as 0x%x\n", p->value);
		else
			fprintf(stderr, "undefined\n");
	}
}

sym_t *lookup(char *name)
{
	sym_t *p;

	for (p = head; p; p = p->next)
		if (!strcmp(p->name, name))
			return(p);
	return(NULL);
}

void patch(sym_t *p, int location)
{
	p->npatch += 1;
	p->patch = (int *)Realloc(p->patch, p->npatch * sizeof(int *));

	p->patch[p->npatch - 1] = location;
}

void backpatch(void)
{
	int i;
	sym_t *p;

	for (p = head; p; p = p->next) {

		if (p->value == NOVALUE) {
			fprintf(stderr,
				"%s: undefined symbol \"%s\"\n",
				filename, p->name);
			exit(EXIT_FAILURE);
		}

		if (p->npatch) {
			if (debug)
				fprintf(stderr,
					"\"%s\" (0x%x) patched at",
					p->name, p->value);

			for (i = 0; i < p->npatch; i++) {
				M[p->patch[i]][0] &= ~1;
				M[p->patch[i]][0] |= ((p->value >> 8) & 1);
				M[p->patch[i]][1] = p->value & 0xff;

				if (debug)
					fprintf(stderr, " 0x%x", p->patch[i]);
			}

			if (debug)
				fputc('\n', stderr);
		}
	}
}

/*
 *  Output words in byte-reversed order (least significant first)
 *  since the sequencer RAM is loaded that way.
 */
void output(FILE *fp)
{
	int i;

	for (i = 0; i < LC; i++)
		fprintf(fp, "\t0x%02x, 0x%02x, 0x%02x, 0x%02x,\n",
			M[i][3],
			M[i][2],
			M[i][1],
			M[i][0]);
	printf("%d out of %d instructions used.\n", LC, MEMORY);
}

char **getl(int *n)
{
	int i;
	char *p, *quote;
	static char buf[MAXLINE];
	static char *a[MAXTOKEN];

	i = 0;

	while (fgets(buf, sizeof(buf), ifp)) {

		lineno += 1;

		if (buf[strlen(buf)-1] != '\n')
			error("line too long");

		p = strchr(buf, '#');
		if (p)
			*p = '\0';
		p = buf;
rescan:
		quote = strchr(p, '\"');
		if (quote)
			*quote = '\0';
		for (p = strtok(p, ", \t\n"); p; p = strtok(NULL, ", \t\n"))
			if (i < MAXTOKEN-1)
				a[i++] = p;
			else
				error("too many tokens");
		if (quote) {
			quote++; 
			p = strchr(quote, '\"');
			if (!p)
				error("unterminated string constant");
			else if (i < MAXTOKEN-1) {
				a[i++] = quote;
				*p = '\0';
				p++;
			}
			else
				error("too many tokens");
			goto rescan;
		}		
		if (i) {
			*n = i;
			return(a);
		}
	}
	return(NULL);
}

#define A	0x8000		/* `A'ccumulator ok */
#define I	0x4000		/* use as immediate value */
#define SL	0x2000		/* shift left */
#define SR	0x1000		/* shift right */
#define RL	0x0800		/* rotate left */
#define RR	0x0400		/* rotate right */
#define LO	0x8000		/* lookup: ori-{jmp,jc,jnc,call} */
#define LA	0x4000		/* lookup: and-{jz,jnz} */
#define LX	0x2000		/* lookup: xor-{je,jne} */
#define NA	-1		/* not applicable */

struct {
	char *name;
	int n;			/* number of operands, including opcode */
	unsigned int op;	/* immediate or L?|pos_from_0 */
	unsigned int dest;	/* NA, pos_from_0, or I|immediate */
	unsigned int src;	/* NA, pos_from_0, or I|immediate */
	unsigned int imm;	/* pos_from_0, A|pos_from_0, or I|immediate */
	unsigned int addr;	/* NA or pos_from_0 */
	int fmt;		/* instruction format - 1, 2, or 3 */
} instr[] = {
/*
 *		N  OP	 DEST		SRC		IMM	ADDR FMT
 */
	"mov",	3, 1,	 1,		2,		I|0xff,	NA,  1,
	"mov",	4, LO|2, NA,		1,		I|0,	3,   3,
	"mvi",	3, 0,	 1,		I|R_ALLZEROS,	A|2,	NA,  1,
	"mvi",	4, LO|2, NA,		I|R_ALLZEROS,	1,	3,   3,
	"not",	2, 2,	 1,		1,		I|0xff,	NA,  1,
	"not",	3, 2,	 1,		2,		I|0xff,	NA,  1,
	"and",	3, 1,	 1,		1,		A|2,	NA,  1,
	"and",  4, 1,	 1,		3,		A|2,	NA,  1,
	"or",	3, 0,	 1,		1,		A|2,	NA,  1,
	"or",	4, 0,	 1,		3,		A|2,	NA,  1,
	"or",	5, LO|3, NA,		1,		2,	4,   3,
	"xor",	3, 2,	 1,		1,		A|2,	NA,  1,
	"xor",	4, 2,	 1,		3,		A|2,	NA,  1,
	"nop",	1, 1,	 I|R_NONE,	I|R_ALLZEROS,	I|0xff,	NA,  1,
	"inc",	2, 3,	 1,		1,		I|1,	NA,  1,
	"inc",	3, 3,	 1,		2,		I|1,	NA,  1,
	"dec",	2, 3,	 1,		1,		I|0xff,	NA,  1,
	"dec",	3, 3,	 1,		2,		I|0xff,	NA,  1,
	"jmp",	2, LO|0, NA,		I|R_SINDEX,	I|0,	1,   3,
	"jc",	2, LO|0, NA,		I|R_SINDEX,	I|0,	1,   3,
	"jnc",	2, LO|0, NA,		I|R_SINDEX,	I|0,	1,   3,
	"call",	2, LO|0, NA,		I|R_SINDEX,	I|0,	1,   3,
	"test",	5, LA|3, NA,		1,		A|2,	4,   3,
	"cmp",	5, LX|3, NA,		1,		A|2,	4,   3,
	"ret",	1, 1,	 I|R_NONE,	I|R_ALLZEROS,	I|0xff,	NA,  1,
	"clc",	1, 3,	 I|R_NONE,	I|R_ALLZEROS,	I|1,	NA,  1,
	"clc",	4, 3,	 2,		I|R_ALLZEROS,	A|3,	NA,  1,
	"stc",	1, 3,	 I|R_NONE,	I|R_ALLONES,	I|1,	NA,  1,
	"stc",	2, 3,	 1,		I|R_ALLONES,	I|1,	NA,  1,
	"add",	3, 3,	 1,		1,		A|2,	NA,  1,
	"add",	4, 3,	 1,		3,		A|2,	NA,  1,
	"adc",	3, 4,	 1,		1,		A|2,	NA,  1,
	"adc",	4, 4,	 1,		3,		A|2,	NA,  1,
	"shl",	3, 5,	 1,		1,		SL|2,	NA,  2,
	"shl",	4, 5,	 1,		2,		SL|3,	NA,  2,
	"shr",	3, 5,	 1,		1,		SR|2,	NA,  2,
	"shr",	4, 5,	 1,		2,		SR|3,	NA,  2,
	"rol",	3, 5,	 1,		1,		RL|2,	NA,  2,
	"rol",	4, 5,	 1,		2,		RL|3,	NA,  2,
	"ror",	3, 5,	 1,		1,		RR|2,	NA,  2,
	"ror",	4, 5,	 1,		2,		RR|3,	NA,  2,
	/*
	 *  Extensions (note also that mvi allows A)
	 */
 	"clr",	2, 1,	 1,		I|R_ALLZEROS,	I|0xff,	NA,  1,
	0
};

int eval_operand(char **a, int spec)
{
	int i;
	unsigned int want = spec & (LO|LA|LX);

	static struct {
		unsigned int what;
		char *name;
		int value;
	} jmptab[] = {
		LO,	"jmp",		8,
		LO,	"jc",		9,
		LO,	"jnc",		10,
		LO,	"call",		11,
		LA,	"jz",		15,
		LA,	"jnz",		13,
		LX,	"je",		14,
		LX,	"jne",		12,
	};

	spec &= ~(LO|LA|LX);

	for (i = 0; i < sizeof(jmptab)/sizeof(jmptab[0]); i++)
		if (jmptab[i].what == want &&
		    !strcmp(jmptab[i].name, a[spec]))
		{
			return(jmptab[i].value);
		}

	if (want)
		error("invalid jump");

	return(spec);		/* "case 0" - no flags set */
}

int eval_sdi(char **a, int spec)
{
	sym_t *p;
	unsigned val;

	if (spec == NA)
		return(NA);

	switch (spec & (A|I|SL|SR|RL|RR)) {
	    case SL:
	    case SR:
	    case RL:
	    case RR:
		if (isdigit(*a[spec &~ (SL|SR|RL|RR)]))
			val = strtol(a[spec &~ (SL|SR|RL|RR)], NULL, 0);
		else {
			p = lookup(a[spec &~ (SL|SR|RL|RR)]);
			if (!p)
				error("undefined symbol used");
			val = p->value;
		}

		switch (spec & (SL|SR|RL|RR)) {		/* blech */
		    case SL:
			if (val > 7)
				return(0xf0);
			return(((val % 8) << 4) |
			       (val % 8));
		    case SR:
			if (val > 7)
				return(0xf0);
			return(((val % 8) << 4) |
			       (1 << 3) |
			       ((8 - (val % 8)) % 8));
		    case RL:
			return(val % 8);
		    case RR:
			return((8 - (val % 8)) % 8);
		}
	    case I:
		return(spec &~ I);
	    case A:
		/*
		 *  An immediate field of zero selects
		 *  the accumulator.  Vigorously object
		 *  if zero is given otherwise - it's
		 *  most likely an error.
		 */
		spec &= ~A;
		if (!strcmp("A", a[spec]))
			return(0);
		if (isdigit(*a[spec]) &&
		    strtol(a[spec], NULL, 0) == 0)
		{
			error("immediate value of zero selects accumulator");
		}
		/* falls through */
	    case 0:
		if (isdigit(*a[spec]))
			return(strtol(a[spec], NULL, 0));
		p = lookup(a[spec]);
		if (p)
			return(p->value);
		error("undefined symbol used");
	}

	return(NA);		/* shut the compiler up */
}

int eval_addr(char **a, int spec)
{
	sym_t *p;

	if (spec == NA)
		return(NA);
	if (isdigit(*a[spec]))
		return(strtol(a[spec], NULL, 0));

	p = lookup(a[spec]);

	if (p) {
		if (p->value != NOVALUE)
			return(p->value);
		patch(p, LC);
	} else {
		define(a[spec], NOVALUE);
		p = lookup(a[spec]);
		patch(p, LC);
	}

	return(NA);		/* will be patched in later */
}

int crack(char **a, int n)
{
	int i;
	int I_imm, I_addr;
	int I_op, I_dest, I_src, I_ret;

	/*
	 *  Check for "ret" at the end of the line; remove
	 *  it unless it's "ret" alone - we still want to
	 *  look it up in the table.
	 */
	I_ret = (strcmp(a[n-1], "ret") ? 0 : !0);
	if (I_ret && n > 1)
		n -= 1;

	for (i = 0; instr[i].name; i++) {
		/*
		 *  Look for match in table given constraints,
		 *  currently just the name and the number of
		 *  operands.
		 */
		if (!strcmp(instr[i].name, *a) && instr[i].n == n)
			break;
	}
	if (!instr[i].name)
		error("unknown opcode or wrong number of operands");

	I_op	= eval_operand(a, instr[i].op);
	I_src	= eval_sdi(a, instr[i].src);
	I_imm	= eval_sdi(a, instr[i].imm);
	I_dest	= eval_sdi(a, instr[i].dest);
	I_addr	= eval_addr(a, instr[i].addr);

	switch (instr[i].fmt) {
	    case 1:
	    case 2:
		M[LC][0] = (I_op << 1) | I_ret;
		M[LC][1] = I_dest;
		M[LC][2] = I_src;
		M[LC][3] = I_imm;
		break;
	    case 3:
		if (I_ret)
			error("illegal use of \"ret\"");
		M[LC][0] = (I_op << 1) | ((I_addr >> 8) & 1);
		M[LC][1] = I_addr & 0xff;
		M[LC][2] = I_src;
		M[LC][3] = I_imm;
		break;
	}

	return(1);		/* no two-byte instructions yet */
}

#undef SL
#undef SR
#undef RL
#undef RR
#undef LX
#undef LA
#undef LO
#undef I
#undef A

void assemble(void)
{
	int n;
	char **a;
	sym_t *p;

	while ((a = getl(&n))) {

		while (a[0][strlen(*a)-1] == ':') {
			a[0][strlen(*a)-1] = '\0';
			p = lookup(*a);
			if (p)
				p->value = LC;
			else
				define(*a, LC);
			a += 1;
			n -= 1;
		}

		if (!n)			/* line was all labels */
			continue;

		if (n == 3 && !strcmp("VERSION", *a))
			fprintf(ofp, "#define %s \"%s\"\n", a[1], a[2]);
		else {
			if (n == 3 && !strcmp("=", a[1]))
				define(*a, strtol(a[2], NULL, 0));
			else
				LC += crack(a, n);
		}
	}

	backpatch();
	output(ofp);

	if (debug)
		output(stderr);
}

int main(int argc, char **argv)
{
	int c;

	while ((c = getopt(argc, argv, "dho:")) != EOF) {
		switch (c) {
		    case 'd':
			debug = !0;
			break;
		    case 'o':
		        ofp = fopen(optarg, "w");
			if (!ofp) {
				perror(optarg);
				exit(EXIT_FAILURE);
			}
			break;
		    case 'h':
			printf("usage: %s [-d] [-ooutput] input\n", *argv);
			exit(EXIT_SUCCESS);
		    case NULL:
			/*
			 *  An impossible option to shut the compiler
			 *  up about sccsid[].
			 */
			exit((int)sccsid);
		    default:
			exit(EXIT_FAILURE);
		}
	}

	if (argc - optind != 1) {
		fprintf(stderr, "%s: must have one input file\n", *argv);
		exit(EXIT_FAILURE);
	}
	filename = argv[optind];

	ifp = fopen(filename, "r");
	if (!ifp) {
		perror(filename);
		exit(EXIT_FAILURE);
	}

	if (!ofp) {
		ofp = fopen(ADOTOUT, "w");
		if (!ofp) {
			perror(ADOTOUT);
			exit(EXIT_FAILURE);
		}
	}

	assemble();
	exit(EXIT_SUCCESS);
}
