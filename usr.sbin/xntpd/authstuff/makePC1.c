/* makePC1.c,v 3.1 1993/07/06 01:04:59 jbj Exp
 * makePC1 - build custom permutted choice 1 tables
 */

#include <stdio.h>
#include <sys/types.h>

#include "ntp_stdlib.h"

#define	STREQ(a, b)	(*(a) == *(b) && strcmp((a), (b)) == 0)

char *progname;
int debug;

static	void	permute	P((u_char *, U_LONG *, U_LONG *));
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
 * Permuted choice 1 table, to produce the initial C.  This table
 * has had 1 subtracted from it to give it a zero base.
 */
static u_char PC1_C[28] = {
	56, 48, 40, 32, 24, 16,  8,
	 0, 57, 49, 41, 33, 25, 17,
	 9,  1, 58, 50, 42, 34, 26,
	18, 10,  2, 59, 51, 43, 35
};

/*
 * Permuted choice 1 table, to produce the initial D.  Again, 1 has
 * been subtracted to match C language zero base arrays.
 */
static u_char PC1_D[28] = {
	62, 54, 46, 38, 30, 22, 14,
	 6, 61, 53, 45, 37, 29, 21,
	13,  5, 60, 52, 44, 36, 28,
	20, 12,  4, 27, 19, 11,  3
};

/*
 * permute - produce c and d for the given bits
 */
static void
permute(bits, cp, dp)
	u_char *bits;
	U_LONG *cp;
	U_LONG *dp;
{
	register int i;
	register U_LONG mask;
	u_char c[28];
	u_char d[28];

	memset((char *)c, 0, sizeof c);
	memset((char *)d, 0, sizeof d);

	for (i = 0; i < 28; i++) {
		c[i] = bits[PC1_C[i]];
		d[i] = bits[PC1_D[i]];
	}

	mask = 0x10000000;
	*cp = *dp = 0;
	for (i = 0; i < 28; i++) {
		mask >>= 1;
		if (c[i])
			*cp |= mask;
		if (d[i])
			*dp |= mask;
	}
}


/*
 * bits from the left part of the key used to form the C subkey
 */
static	int	lc3[4] = { 0, 8, 16, 24 };

/*
 * bits from the left part of the key used to form the D subkey
 */
static	int	ld4[4] = { 3, 11, 19, 27 };

/*
 * bits from the right part of the key used to form the C subkey
 */
static	int	rc4[4] = { 32, 40, 48, 56 };

/*
 * bits from the right part of the key used to form the D subkey
 */
static	int	rd3[4] = { 36, 44, 52, 60 };

static	U_LONG	PC_CL[8];
static	U_LONG	PC_DL[16];
static	U_LONG	PC_CR[16];
static	U_LONG	PC_DR[8];


/*
 * doit - compute and print the four PC1 tables
 */
static void
doit()
{
	int i;
	int comb;
	U_LONG c;
	U_LONG d;
	u_char bits[64];

	memset((char *)bits, 0, sizeof bits);

	printf("static U_LONG PC1_CL[8] = {");
	for (i = 0; i < 4; i++) {
		for (comb = 0; comb < 8; comb++) {
			if (comb & 0x4)
				bits[lc3[i]] = 1;
			if (comb & 0x2)
				bits[lc3[i]+1] = 1;
			if (comb & 0x1)
				bits[lc3[i]+2] = 1;
			permute(bits, &c, &d);
			bits[lc3[i]] = 0;
			bits[lc3[i]+1] = 0;
			bits[lc3[i]+2] = 0;
			if (d != 0) {
				(void) fprintf(stderr,
				    "Error PC_CL i %d comb %d\n", i, comb);
			}
			if (i == 0) {
				PC_CL[comb] = c;
				if ((comb & 0x3) == 0)
					printf("\n\t0x%08x,", c);
				else if (comb == 7)
					printf(" 0x%08x\n};\n\n", c);
				else
					printf(" 0x%08x,", c);
			} else {
				if (c != PC_CL[comb] << i)
					(void) fprintf(stderr,
					    "Error PC_CL 0x%08x c 0x%08x\n",
					    PC_CL[comb], c);
			}
		}
	}

	printf("static U_LONG PC1_DL[16] = {");
	for (i = 0; i < 4; i++) {
		for (comb = 0; comb < 16; comb++) {
			if (comb & 0x8)
				bits[ld4[i]] = 1;
			if (comb & 0x4)
				bits[ld4[i]+1] = 1;
			if (comb & 0x2)
				bits[ld4[i]+2] = 1;
			if (comb & 0x1)
				bits[ld4[i]+3] = 1;
			permute(bits, &c, &d);
			bits[ld4[i]] = 0;
			bits[ld4[i]+1] = 0;
			bits[ld4[i]+2] = 0;
			bits[ld4[i]+3] = 0;
			if (c != 0) {
				(void) fprintf(stderr,
				    "Error PC_DL i %d comb %d\n", i, comb);
			}
			if (i == 0) {
				PC_DL[comb] = d;
				if ((comb & 0x3) == 0)
					printf("\n\t0x%08x,", d);
				else if (comb == 15)
					printf(" 0x%08x\n};\n\n", d);
				else
					printf(" 0x%08x,", d);
			} else {
				if (d != PC_DL[comb] << i)
					(void) fprintf(stderr,
					    "Error PC_DL 0x%08x c 0x%08x\n",
					    PC_DL[comb], d);
			}
		}
	}

	printf("static U_LONG PC1_CR[16] = {");
	for (i = 0; i < 4; i++) {
		for (comb = 0; comb < 16; comb++) {
			if (comb & 0x8)
				bits[rc4[i]] = 1;
			if (comb & 0x4)
				bits[rc4[i]+1] = 1;
			if (comb & 0x2)
				bits[rc4[i]+2] = 1;
			if (comb & 0x1)
				bits[rc4[i]+3] = 1;
			permute(bits, &c, &d);
			bits[rc4[i]] = 0;
			bits[rc4[i]+1] = 0;
			bits[rc4[i]+2] = 0;
			bits[rc4[i]+3] = 0;
			if (d != 0) {
				(void) fprintf(stderr,
				    "Error PC_CR i %d comb %d\n", i, comb);
			}
			if (i == 0) {
				PC_CR[comb] = c;
				if ((comb & 0x3) == 0)
					printf("\n\t0x%08x,", c);
				else if (comb == 15)
					printf(" 0x%08x\n};\n\n", c);
				else
					printf(" 0x%08x,", c);
			} else {
				if (c != PC_CR[comb] << i)
					(void) fprintf(stderr,
					    "Error PC_CR 0x%08x c 0x%08x\n",
					    PC_CR[comb], c);
			}
		}
	}

	printf("static U_LONG PC1_DR[8] = {");
	for (i = 0; i < 4; i++) {
		for (comb = 0; comb < 8; comb++) {
			if (comb & 0x4)
				bits[rd3[i]] = 1;
			if (comb & 0x2)
				bits[rd3[i]+1] = 1;
			if (comb & 0x1)
				bits[rd3[i]+2] = 1;
			permute(bits, &c, &d);
			bits[rd3[i]] = 0;
			bits[rd3[i]+1] = 0;
			bits[rd3[i]+2] = 0;
			if (c != 0) {
				(void) fprintf(stderr,
				    "Error PC_DR i %d comb %d\n", i, comb);
			}
			if (i == 0) {
				PC_DR[comb] = d;
				if ((comb & 0x3) == 0)
					printf("\n\t0x%08x,", d);
				else if (comb == 7)
					printf(" 0x%08x\n};\n\n", d);
				else
					printf(" 0x%08x,", d);
			} else {
				if (d != PC_DR[comb] << i)
					(void) fprintf(stderr,
					    "Error PC_DR 0x%08x c 0x%08x\n",
					    PC_DR[comb], d);
			}
		}
	}
}
