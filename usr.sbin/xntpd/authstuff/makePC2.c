/* makePC2.c,v 3.1 1993/07/06 01:05:01 jbj Exp
 * makePC2 - build custom permutted choice 2 tables
 */

#include <stdio.h>
#include <sys/types.h>

#include "ntp_stdlib.h"

#define	STREQ(a, b)	(*(a) == *(b) && strcmp((a), (b)) == 0)

char *progname;
int debug;

static	void	permc	P((u_char *, U_LONG *));
static	void	permd	P((u_char *, U_LONG *));
static	void	doit	P((void));

/*
 * main - parse arguments and handle options
 */
void
main(argc, argv)
int argc;
char *argv[];
{
	int c;
	int errflg = 0;
	extern int ntp_optind;
	extern char *ntp_optarg;

	progname = argv[0];
	while ((c = ntp_getopt(argc, argv, "d")) != EOF)
		switch (c) {
		case 'd':
			++debug;
			break;
		default:
			errflg++;
			break;
		}
	if (errflg) {
		(void) fprintf(stderr, "usage: %s [-d]\n", progname);
		exit(2);
	}
	doit();
	exit(0);
}

/*
 * Permuted choice 2 table.  This actually produces the low order 24
 * bits of the subkey Ki from the 28 bit value of Ci.  This has had
 * 1 subtracted from it to give a zero base.
 */
static u_char PC2_C[24] = {
	13, 16, 10, 23,  0,  4,
	 2, 27, 14,  5, 20,  9,
	22, 18, 11,  3, 25,  7,
	15,  6, 26, 19, 12,  1
};

/*
 * Permuted choice 2 table, operating on the 28 Di bits to produce the
 * high order 24 bits of subkey Ki.  This has had 29 subtracted from
 * it to give it a zero base into our D bit array.
 */
static u_char PC2_D[24] = {
	12, 23,  2,  8, 18, 26,
	 1, 11, 22, 16,  4, 19,
	15, 20, 10, 27,  5, 24,
	17, 13, 21,  7,  0,  3
};

U_LONG masks[4] = { 0x40000000, 0x400000, 0x4000, 0x40 };


/*
 * permc - permute C, producing a four byte result
 */
static void
permc(bits, resp)
	u_char *bits;
	U_LONG *resp;
{
	register int part;
	register int i;
	register U_LONG mask;
	u_char res[24];

	memset((char *)res, 0, sizeof res);

	for (i = 0; i < 24; i++) {
		res[i] = bits[PC2_C[i]];
	}

	*resp = 0;
	for (part = 0; part < 4; part++) {
		mask = masks[part];
		for (i = part*6; i < (part+1)*6; i++) {
			mask >>= 1;
			if (res[i])
				*resp |= mask;
		}
	}
}

/*
 * permd - permute D, producing a four byte result
 */
static void
permd(bits, resp)
	u_char *bits;
	U_LONG *resp;
{
	register int part;
	register int i;
	register U_LONG mask;
	u_char res[24];

	memset((char *)res, 0, sizeof res);

	for (i = 0; i < 24; i++) {
		res[i] = bits[PC2_D[i]];
	}

	*resp = 0;
	for (part = 0; part < 4; part++) {
		mask = masks[part];
		for (i = part*6; i < (part+1)*6; i++) {
			mask >>= 1;
			if (res[i])
				*resp |= mask;
		}
	}
}


/*
 * bits used for each round in C
 */
static	int	cbits[4][6] = {
	0, 1, 2, 3, 4, 5,
	6, 7, 9, 10, 11, 12,
	13, 14, 15, 16, 22, 23,
	18, 19, 20, 25, 26, 27
};


/*
 * bits used for each round in D
 */
static	int	dbits[4][6] = {
	0, 1, 2, 3, 4, 5,
	7, 8, 10, 11, 12, 13,
	15, 16, 17, 18, 19, 20,
	21, 22, 23, 24, 26, 27
};


/*
 * doit - compute and print the four PC1 tables
 */
static void
doit()
{
	int i;
	int comb;
	U_LONG res;
	u_char bits[28];

	memset((char *)bits, 0, sizeof bits);

	printf("static U_LONG PC2_C[4][64] = {");
	for (i = 0; i < 4; i++) {
		for (comb = 0; comb < 64; comb++) {
			if (comb & 0x20)
				bits[cbits[i][0]] = 1;
			if (comb & 0x10)
				bits[cbits[i][1]] = 1;
			if (comb & 0x8)
				bits[cbits[i][2]] = 1;
			if (comb & 0x4)
				bits[cbits[i][3]] = 1;
			if (comb & 0x2)
				bits[cbits[i][4]] = 1;
			if (comb & 0x1)
				bits[cbits[i][5]] = 1;
			permc(bits, &res);
			bits[cbits[i][0]] = 0;
			bits[cbits[i][1]] = 0;
			bits[cbits[i][2]] = 0;
			bits[cbits[i][3]] = 0;
			bits[cbits[i][4]] = 0;
			bits[cbits[i][5]] = 0;
			if ((comb & 0x3) == 0)
				printf("\n\t0x%08x,", res);
			else if (comb == 63 && i == 3)
				printf(" 0x%08x\n};\n\n", res);
			else if (comb == 63)
				printf(" 0x%08x,\n", res);
			else
				printf(" 0x%08x,", res);
		}
	}

	printf("static U_LONG PC2_D[4][64] = {");
	for (i = 0; i < 4; i++) {
		for (comb = 0; comb < 64; comb++) {
			if (comb & 0x20)
				bits[dbits[i][0]] = 1;
			if (comb & 0x10)
				bits[dbits[i][1]] = 1;
			if (comb & 0x8)
				bits[dbits[i][2]] = 1;
			if (comb & 0x4)
				bits[dbits[i][3]] = 1;
			if (comb & 0x2)
				bits[dbits[i][4]] = 1;
			if (comb & 0x1)
				bits[dbits[i][5]] = 1;
			permd(bits, &res);
			bits[dbits[i][0]] = 0;
			bits[dbits[i][1]] = 0;
			bits[dbits[i][2]] = 0;
			bits[dbits[i][3]] = 0;
			bits[dbits[i][4]] = 0;
			bits[dbits[i][5]] = 0;
			if ((comb & 0x3) == 0)
				printf("\n\t0x%08x,", res);
			else if (comb == 63 && i == 3)
				printf(" 0x%08x\n};\n\n", res);
			else if (comb == 63)
				printf(" 0x%08x,\n", res);
			else
				printf(" 0x%08x,", res);
		}
	}
}
