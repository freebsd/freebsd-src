/*	$NetBSD: rf_geniq.c,v 1.3 1999/02/05 00:06:12 oster Exp $	*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Daniel Stodolsky
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/* rf_geniq.c
 *  code which implements Reed-Solomon encoding for RAID level 6
 */


#define RF_UTILITY 1
#include <dev/raidframe/rf_pqdeg.h>

/*
   five bit lfsr
   poly - feedback connections

   val  = value;
*/
int 
lsfr_shift(val, poly)
	unsigned val, poly;
{
	unsigned new;
	unsigned int i;
	unsigned high = (val >> 4) & 1;
	unsigned bit;

	new = (poly & 1) ? high : 0;

	for (i = 1; i <= 4; i++) {
		bit = (val >> (i - 1)) & 1;
		if (poly & (1 << i))	/* there is a feedback connection */
			new = new | ((bit ^ high) << i);
		else
			new = new | (bit << i);
	}
	return new;
}
/* generate Q matricies for the data */

RF_ua32_t rf_qfor[32];

void 
main()
{
	unsigned int i, j, l, a, b;
	unsigned int val;
	unsigned int r;
	unsigned int m, p, q;

	RF_ua32_t k;

	printf("/*\n");
	printf(" * rf_invertq.h\n");
	printf(" */\n");
	printf("/*\n");
	printf(" * GENERATED FILE -- DO NOT EDIT\n");
	printf(" */\n");
	printf("\n");
	printf("#ifndef _RF__RF_INVERTQ_H_\n");
	printf("#define _RF__RF_INVERTQ_H_\n");
	printf("\n");
	printf("/*\n");
	printf(" * rf_geniq.c must include rf_archs.h before including\n");
	printf(" * this file (to get VPATH magic right with the way we\n");
	printf(" * generate this file in kernel trees)\n");
	printf(" */\n");
	printf("/* #include \"rf_archs.h\" */\n");
	printf("\n");
	printf("#if (RF_INCLUDE_PQ > 0) || (RF_INCLUDE_RAID6 > 0)\n");
	printf("\n");
	printf("#define RF_Q_COLS 32\n");
	printf("RF_ua32_t rf_rn = {\n");
	k[0] = 1;
	for (j = 0; j < 31; j++)
		k[j + 1] = lsfr_shift(k[j], 5);
	for (j = 0; j < 32; j++)
		printf("%d, ", k[j]);
	printf("};\n");

	printf("RF_ua32_t rf_qfor[32] = {\n");
	for (i = 0; i < 32; i++) {
		printf("/* i = %d */ { 0, ", i);
		rf_qfor[i][0] = 0;
		for (j = 1; j < 32; j++) {
			val = j;
			for (l = 0; l < i; l++)
				val = lsfr_shift(val, 5);
			rf_qfor[i][j] = val;
			printf("%d, ", val);
		}
		printf("},\n");
	}
	printf("};\n");
	printf("#define RF_Q_DATA_COL(col_num) rf_rn[col_num],rf_qfor[28-(col_num)]\n");

	/* generate the inverse tables. (i,j,p,q) */
	/* The table just stores a. Get b back from the parity */
	printf("#ifdef KERNEL\n");
	printf("RF_ua1024_t rf_qinv[1];        /* don't compile monster table into kernel */\n");
	printf("#elif defined(NO_PQ)\n");
	printf("RF_ua1024_t rf_qinv[29*29];\n");
	printf("#else /* !KERNEL && NO_PQ */\n");
	printf("RF_ua1024_t rf_qinv[29*29] = {\n");
	for (i = 0; i < 29; i++) {
		for (j = 0; j < 29; j++) {
			printf("/* i %d, j %d */{ ", i, j);
			if (i == j)
				for (l = 0; l < 1023; l++)
					printf("0, ");
			else {
				for (p = 0; p < 32; p++)
					for (q = 0; q < 32; q++) {
						/* What are a, b such that a ^
						 * b =  p; and qfor[(28-i)][a
						 * ^ rf_rn[i+1]] ^
						 * qfor[(28-j)][b ^
						 * rf_rn[j+1]] =  q. Solve by
						 * guessing a. Then testing. */
						for (a = 0; a < 32; a++) {
							b = a ^ p;
							if ((rf_qfor[28 - i][a ^ k[i + 1]] ^ rf_qfor[28 - j][b ^ k[j + 1]]) == q)
								break;
						}
						if (a == 32)
							printf("unable to solve %d %d %d %d\n", i, j, p, q);
						printf("%d,", a);
					}
			}
			printf("},\n");
		}
	}
	printf("};\n");
	printf("\n#endif /* (RF_INCLUDE_PQ > 0) || (RF_INCLUDE_RAID6 > 0) */\n\n");
	printf("#endif /* !KERNEL && NO_PQ */\n");
	printf("#endif /* !_RF__RF_INVERTQ_H_ */\n");
	exit(0);
}
