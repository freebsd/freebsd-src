/* makeIPFP.c,v 3.1 1993/07/06 01:04:58 jbj Exp
 * makeIPFP - make fast DES IP and FP tables
 */

#include <stdio.h>
#include <sys/types.h>

#include "ntp_stdlib.h"

#define	STREQ(a, b)	(*(a) == *(b) && strcmp((a), (b)) == 0)

U_LONG IPL[256];
U_LONG FPL[256];

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
	int ind4, ind5, ind6, ind7;
	int octbits;

	bzero((char *)bits, sizeof bits);

	/*
	 * Do the rounds for the IP table.  We save the results of
	 * this as well as printing them.  Note that this is the
	 * left-half table, the right half table will be identical.
	 */
	printf("static U_LONG IP[256] = {");
	for (tabno = 0; tabno < 4; tabno++) {
		i = tabno * 8;
		ind7 = IPLbits[i] - 1;
		ind6 = IPLbits[i+1] - 1;
		ind5 = IPLbits[i+2] - 1;
		ind4 = IPLbits[i+3] - 1;
		ind3 = IPLbits[i+4] - 1;
		ind2 = IPLbits[i+5] - 1;
		ind1 = IPLbits[i+6] - 1;
		ind0 = IPLbits[i+7] - 1;
		for (octbits = 0; octbits < 256; octbits++) {
			if (octbits & (1 << 7))
				bits[ind7] = 1;
			if (octbits & (1 << 6))
				bits[ind6] = 1;
			if (octbits & (1 << 5))
				bits[ind5] = 1;
			if (octbits & (1 << 4))
				bits[ind4] = 1;
			if (octbits & (1 << 3))
				bits[ind3] = 1;
			if (octbits & (1 << 2))
				bits[ind2] = 1;
			if (octbits & (1 << 1))
				bits[ind1] = 1;
			if (octbits & 1)
				bits[ind0] = 1;
			perm(bits, IP, &left, &right);
			bits[ind7] = 0;
			bits[ind6] = 0;
			bits[ind5] = 0;
			bits[ind4] = 0;
			bits[ind3] = 0;
			bits[ind2] = 0;
			bits[ind1] = 0;
			bits[ind0] = 0;
			if (right != 0) {
				fprintf(stderr,
				    "IP tabno %d oct %d right not zero\n",
				    tabno, octbits);
				exit(1);
			}
			if (tabno > 0) {
				if ((IPL[octbits] << tabno) != left) {
					fprintf(stderr,
			"IP tabno %d oct %d IP %d left %d, IP != left\n",
					    tabno, octbits, IPL[octbits], left);
					exit (1);
				}
			} else {
				IPL[octbits] = left;
				if (octbits == 255) {
					printf(" 0x%08x", left);
				} else if (octbits & 0x3) {
					printf(" 0x%08x,", left);
				} else {
					printf("\n\t0x%08x,", left);
				}
			}
		}
		if (tabno == 0)
			printf("\n};\n\n");
	}

	/*
	 * Next is the FP table, in big endian order
	 */
	printf("#if BYTE_ORDER == LITTLE_ENDIAN\nstatic U_LONG FP[256] = {");
	for (tabno = 3; tabno >= 0; tabno--) {
		i = tabno * 8;
		ind7 = FPLbits[i] - 1;
		ind6 = FPLbits[i+1] - 1;
		ind5 = FPLbits[i+2] - 1;
		ind4 = FPLbits[i+3] - 1;
		ind3 = FPLbits[i+4] - 1;
		ind2 = FPLbits[i+5] - 1;
		ind1 = FPLbits[i+6] - 1;
		ind0 = FPLbits[i+7] - 1;
		for (octbits = 0; octbits < 256; octbits++) {
			if (octbits & (1 << 7))
				bits[ind7] = 1;
			if (octbits & (1 << 6))
				bits[ind6] = 1;
			if (octbits & (1 << 5))
				bits[ind5] = 1;
			if (octbits & (1 << 4))
				bits[ind4] = 1;
			if (octbits & (1 << 3))
				bits[ind3] = 1;
			if (octbits & (1 << 2))
				bits[ind2] = 1;
			if (octbits & (1 << 1))
				bits[ind1] = 1;
			if (octbits & 1)
				bits[ind0] = 1;
			perm(bits, FP, &left, &right);
			bits[ind7] = 0;
			bits[ind6] = 0;
			bits[ind5] = 0;
			bits[ind4] = 0;
			bits[ind3] = 0;
			bits[ind2] = 0;
			bits[ind1] = 0;
			bits[ind0] = 0;
			if (right != 0) {
				fprintf(stderr,
				    "FP tabno %d oct %d right not zero\n",
				    tabno, octbits);
				exit(1);
			}
			if (tabno != 3) {
				if ((FPL[octbits] << ((3-tabno)<<1)) != left) {
					fprintf(stderr,
			"FP tabno %d oct %d FP %x left %x, FP != left\n",
					    tabno, octbits, FPL[octbits], left);
					exit (1);
				}
			} else {
				FPL[octbits] = left;
				if (octbits == 255) {
					printf(" 0x%08x", left);
				} else if (octbits & 0x3) {
					printf(" 0x%08x,", left);
				} else {
					printf("\n\t0x%08x,", left);
				}
			}
		}
		if (tabno == 3)
			printf("\n};\n");
	}

	/*
	 * Now reouput the FP table in order appropriate for little
	 * endian machines
	 */
	printf("#else\nstatic U_LONG FP[256] = {");
	for (octbits = 0; octbits < 256; octbits++) {
		left = ((FPL[octbits] >> 24) & 0x000000ff)
		     | ((FPL[octbits] >>  8) & 0x0000ff00)
		     | ((FPL[octbits] <<  8) & 0x00ff0000)
		     | ((FPL[octbits] << 24) & 0xff000000);
		if (octbits == 255) {
			printf(" 0x%08x", left);
		} else if (octbits & 0x3) {
			printf(" 0x%08x,", left);
		} else {
			printf("\n\t0x%08x,", left);
		}
	}
	printf("\n};\n#endif\n");
}
