/* Rubin encoder/decoder header       */
/* work started at   : aug   3, 1994  */
/* last modification : aug  15, 1994  */
/* $Id: compr_rubin.h,v 1.5 2001/02/26 13:50:01 dwmw2 Exp $ */

#include "pushpull.h"

#define RUBIN_REG_SIZE   16
#define UPPER_BIT_RUBIN    (((long) 1)<<(RUBIN_REG_SIZE-1))
#define LOWER_BITS_RUBIN   ((((long) 1)<<(RUBIN_REG_SIZE-1))-1)


struct rubin_state {
	unsigned long p;		
	unsigned long q;	
	unsigned long rec_q;
	long bit_number;
	struct pushpull pp;
	int bit_divider;
	int bits[8];
};


void init_rubin (struct rubin_state *rs, int div, int *bits);
int encode (struct rubin_state *, long, long, int);
void end_rubin (struct rubin_state *);
void init_decode (struct rubin_state *, int div, int *bits);
int decode (struct rubin_state *, long, long);
