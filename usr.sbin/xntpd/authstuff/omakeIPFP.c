/* omakeIPFP.c,v 3.1 1993/07/06 01:05:10 jbj Exp
 * makeIPFP - make fast DES IP and FP tables
 *
 * This is an older version which generated tables half the size of
 * the current version, but which took about double the CPU time to
 * compute permutations from these tables.  Since the CPU spent on the
 * permutations is small compared to the CPU spent in the cipher code,
 * I may go back to the smaller tables to save the space some day.
 */

#include <stdio.h>
#include <sys/types.h>

#include "ntp_stdlib.h"

#define	STREQ(a, b)	(*(a) == *(b) && strcmp((a), (b)) == 0)

U_LONG IPL[8][16];
U_LONG FPL[8][16];

char *progname;
int debug;

static	void	perm	P((u_char *, u_char *, U_LONG *, U_LONG *));
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
	extern int optind;
	extern char *optarg;

	progname = argv[0];
	while ((c = getopt_l(argc, argv, "d")) != EOF)
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
 * Initial permutation table
 */
u_char IP[64] = {
	58, 50, 42, 34, 26, 18, 10,  2,
	60, 52, 44, 36, 28, 20, 12,  4,
	62, 54, 46, 38, 30, 22, 14,  6,
	64, 56, 48, 40, 32, 24, 16,  8,
	57, 49, 41, 33, 25, 17,  9,  1,
	59, 51, 43, 35, 27, 19, 11,  3,
	61, 53, 45, 37, 29, 21, 13,  5,
	63, 55, 47, 39, 31, 23, 15,  7
};

/*
 * Inverse initial permutation table
 */
u_char FP[64] = {
	40,  8, 48, 16, 56, 24, 64, 32,
	39,  7, 47, 15, 55, 23, 63, 31,
	38,  6, 46, 14, 54, 22, 62, 30,
	37,  5, 45, 13, 53, 21, 61, 29,
	36,  4, 44, 12, 52, 20, 60, 28,
	35,  3, 43, 11, 51, 19, 59, 27,
	34,  2, 42, 10, 50, 18, 58, 26,
	33,  1, 41,  9, 49, 17, 57, 25
};


/*
 * Bit order after the operation
 *
 * ((left & 0x55555555) << 1) | (right & 0x55555555)
 */
u_char IPLbits[32] = {
	 2, 34,  4, 36,  6, 38,  8, 40,
	10, 42, 12, 44, 14, 46, 16, 48,
	18, 50, 20, 52, 22, 54, 24, 56,
	26, 58, 28, 60, 30, 62, 32, 64
};


/*
 * Bit order after the operation
 *
 * (left & 0xaaaaaaaa) | ((right & 0xaaaaaaaa) >> 1)
 */
u_char IPRbits[32] = {
	 1, 33,  3, 35,  5, 37,  7, 39,
	 9, 41, 11, 43, 13, 45, 15, 47,
	17, 49, 19, 51, 21, 53, 23, 55,
	25, 57, 27, 59, 29, 61, 31, 63
};


/*
 * Bit order after the operation
 *
 * ((left & 0x0f0f0f0f) << 4) | (right & 0x0f0f0f0f)
 */
u_char FPLbits[32] = {
	 5,  6,  7,  8, 37, 38, 39, 40,
	13, 14, 15, 16, 45, 46, 47, 48,
	21, 22, 23, 24, 53, 54, 55, 56,
	29, 30, 31, 32, 61, 62, 63, 64
};


/*
 * Bit order after the operation
 *
 * (left & 0xf0f0f0f0) | ((right & 0xf0f0f0f0) >> 4)
 */
u_char FPRbits[32] = {
	 1,  2,  3,  4, 33, 34, 35, 36,
	 9, 10, 11, 12, 41, 42, 43, 44,
	17, 18, 19, 20, 49, 50, 51, 52,
	25, 26, 27, 28, 57, 58, 59, 60
};


/*
 * perm - do a permutation with the given table
 */
static void
perm(databits, permtab, leftp, rightp)
	u_char *databits;
	u_char *permtab;
	U_LONG *leftp;
	U_LONG *rightp;
{
	register U_LONG left;
	register U_LONG right;
	register u_char *PT;
	register u_char *bits;
	register int i;

	left = right = 0;
	PT = permtab;
	bits = databits;

	for (i = 0; i < 32; i++) {
		left <<= 1;
		if (bits[PT[i]-1])
			left |= 1;
	}

	for (i = 32; i < 64; i++) {
		right <<= 1;
		if (bits[PT[i]-1])
			right |= 1;
	}

	*leftp = left;
	*rightp = right;
}


/*
 * doit - make up the tables
 */
static void
doit()
{
	u_char bits[64];
	U_LONG left;
	U_LONG right;
	int tabno;
	int i;
	int ind0, ind1, ind2, ind3;
	int quadbits;

	bzero((char *)bits, sizeof bits);

	/*
	 * Do the rounds for the IPL table.  We save the results of
	 * this as well as printing them.  Note that this is the
	 * left-half table.
	 */
	printf("static U_LONG IP[8][16] = {");
	for (tabno = 0; tabno < 8; tabno++) {
		i = tabno * 4;
		ind3 = IPLbits[i] - 1;
		ind2 = IPLbits[i+1] - 1;
		ind1 = IPLbits[i+2] - 1;
		ind0 = IPLbits[i+3] - 1;
		for (quadbits = 0; quadbits < 16; quadbits++) {
			if (quadbits & (1 << 3))
				bits[ind3] = 1;
			if (quadbits & (1 << 2))
				bits[ind2] = 1;
			if (quadbits & (1 << 1))
				bits[ind1] = 1;
			if (quadbits & 1)
				bits[ind0] = 1;
			perm(bits, IP, &left, &right);
			bits[ind3] = 0;
			bits[ind2] = 0;
			bits[ind1] = 0;
			bits[ind0] = 0;
			if (right != 0) {
				fprintf(stderr,
				    "IPL tabno %d quad %d right not zero\n",
				    tabno, quadbits);
				exit(1);
			}
			IPL[tabno][quadbits] = left;
			if (quadbits == 15 && tabno == 7) {
				printf(" 0x%08x", left);
			} else if (quadbits & 0x3) {
				printf(" 0x%08x,", left);
			} else {
				printf("\n\t0x%08x,", left);
			}
		}
		if (tabno == 7)
			printf("\n};\n");
		printf("\n");
	}

	/*
	 * Compute the right half of the same table.  I noticed this table
	 * was the same as the previous one, just by luck, so we don't
	 * actually have to do this.  Do it anyway just for a check.
	 */
	for (tabno = 0; tabno < 8; tabno++) {
		i = tabno * 4;
		ind3 = IPRbits[i] - 1;
		ind2 = IPRbits[i+1] - 1;
		ind1 = IPRbits[i+2] - 1;
		ind0 = IPRbits[i+3] - 1;
		for (quadbits = 0; quadbits < 16; quadbits++) {
			if (quadbits & (1 << 3))
				bits[ind3] = 1;
			if (quadbits & (1 << 2))
				bits[ind2] = 1;
			if (quadbits & (1 << 1))
				bits[ind1] = 1;
			if (quadbits & 1)
				bits[ind0] = 1;
			perm(bits, IP, &left, &right);
			bits[ind3] = 0;
			bits[ind2] = 0;
			bits[ind1] = 0;
			bits[ind0] = 0;
			if (left != 0) {
				fprintf(stderr,
				    "IPR tabno %d quad %d left not zero\n",
				    tabno, quadbits);
				exit(1);
			}
			if (right != IPL[tabno][quadbits]) {
				fprintf(stderr,
			"IPR tabno %d quad %d: 0x%08x not same as 0x%08x\n",
				   tabno, quadbits, right,IPL[tabno][quadbits]);
				exit(1);
			}
		}
	}

	/*
	 * Next are the FP tables
	 */
	printf("static U_LONG FP[8][16] = {");
	for (tabno = 0; tabno < 8; tabno++) {
		i = tabno * 4;
		ind3 = FPLbits[i] - 1;
		ind2 = FPLbits[i+1] - 1;
		ind1 = FPLbits[i+2] - 1;
		ind0 = FPLbits[i+3] - 1;
		for (quadbits = 0; quadbits < 16; quadbits++) {
			if (quadbits & (1 << 3))
				bits[ind3] = 1;
			if (quadbits & (1 << 2))
				bits[ind2] = 1;
			if (quadbits & (1 << 1))
				bits[ind1] = 1;
			if (quadbits & 1)
				bits[ind0] = 1;
			perm(bits, FP, &left, &right);
			bits[ind3] = 0;
			bits[ind2] = 0;
			bits[ind1] = 0;
			bits[ind0] = 0;
			if (right != 0) {
				fprintf(stderr,
				    "FPL tabno %d quad %d right not zero\n",
				    tabno, quadbits);
				exit(1);
			}
			FPL[tabno][quadbits] = left;
			if (quadbits == 15 && tabno == 7) {
				printf(" 0x%08x", left);
			} else if (quadbits & 0x3) {
				printf(" 0x%08x,", left);
			} else {
				printf("\n\t0x%08x,", left);
			}
		}
		if (tabno == 7)
			printf("\n};");
		printf("\n");
	}

	/*
	 * Right half of same set of tables.  This was symmetric too.
	 * Amazing!
	 */
	for (tabno = 0; tabno < 8; tabno++) {
		i = tabno * 4;
		ind3 = FPRbits[i] - 1;
		ind2 = FPRbits[i+1] - 1;
		ind1 = FPRbits[i+2] - 1;
		ind0 = FPRbits[i+3] - 1;
		for (quadbits = 0; quadbits < 16; quadbits++) {
			if (quadbits & (1 << 3))
				bits[ind3] = 1;
			if (quadbits & (1 << 2))
				bits[ind2] = 1;
			if (quadbits & (1 << 1))
				bits[ind1] = 1;
			if (quadbits & 1)
				bits[ind0] = 1;
			perm(bits, FP, &left, &right);
			bits[ind3] = 0;
			bits[ind2] = 0;
			bits[ind1] = 0;
			bits[ind0] = 0;
			if (left != 0) {
				fprintf(stderr,
				    "FPR tabno %d quad %d left not zero\n",
				    tabno, quadbits);
				exit(1);
			}
			if (right != FPL[tabno][quadbits]) {
				fprintf(stderr,
			"FPR tabno %d quad %d: 0x%08x not same as 0x%08x\n",
				   tabno, quadbits, right,FPL[tabno][quadbits]);
				exit(1);
			}
		}
	}
}
