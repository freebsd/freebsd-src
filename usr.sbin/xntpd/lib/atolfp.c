/* atolfp.c,v 3.1 1993/07/06 01:07:40 jbj Exp
 * atolfp - convert an ascii string to an l_fp number
 */
#include <stdio.h>
#include <ctype.h>

#include "ntp_fp.h"
#include "ntp_string.h"

/*
 * Powers of 10
 */
static U_LONG ten_to_the_n[10] = {
		   0,
		  10,
		 100,
		1000,
	       10000,
	      100000,
	     1000000,
	    10000000,
	   100000000,
	  1000000000,
};


int
atolfp(str, lfp)
	const char *str;
	l_fp *lfp;
{
	register const char *cp;
	register U_LONG dec_i;
	register U_LONG dec_f;
	char *ind;
	int ndec;
	int isneg;
	static char *digits = "0123456789";

	isneg = 0;
	dec_i = dec_f = 0;
	ndec = 0;
	cp = str;

	/*
	 * We understand numbers of the form:
	 *
	 * [spaces][-|+][digits][.][digits][spaces|\n|\0]
	 */
	while (isspace(*cp))
		cp++;
	
	if (*cp == '-') {
		cp++;
		isneg = 1;
	}
	
	if (*cp == '+')
		cp++;

	if (*cp != '.' && !isdigit(*cp))
		return 0;

	while (*cp != '\0' && (ind = strchr(digits, *cp)) != NULL) {
		dec_i = (dec_i << 3) + (dec_i << 1);	/* multiply by 10 */
		dec_i += (ind - digits);
		cp++;
	}

	if (*cp != '\0' && !isspace(*cp)) {
		if (*cp++ != '.')
			return 0;
	
		while (ndec < 9 && *cp != '\0'
		    && (ind = strchr(digits, *cp)) != NULL) {
			ndec++;
			dec_f = (dec_f << 3) + (dec_f << 1);	/* *10 */
			dec_f += (ind - digits);
			cp++;
		}

		while (isdigit(*cp))
			cp++;
		
		if (*cp != '\0' && !isspace(*cp))
			return 0;
	}

	if (ndec > 0) {
		register U_LONG tmp;
		register U_LONG bit;
		register U_LONG ten_fact;

		ten_fact = ten_to_the_n[ndec];

		tmp = 0;
		bit = 0x80000000;
		while (bit != 0) {
			dec_f <<= 1;
			if (dec_f >= ten_fact) {
				tmp |= bit;
				dec_f -= ten_fact;
			}
			bit >>= 1;
		}
		if ((dec_f << 1) > ten_fact)
			tmp++;
		dec_f = tmp;
	}

	if (isneg)
		M_NEG(dec_i, dec_f);
	
	lfp->l_ui = dec_i;
	lfp->l_uf = dec_f;
	return 1;
}
