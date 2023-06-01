/*
 * mstolfp - convert an ascii string in milliseconds to an l_fp number
 */
#include <config.h>
#include <stdio.h>
#include <ctype.h>

#include "ntp_fp.h"
#include "ntp_stdlib.h"

int
mstolfp(
	const char *str,
	l_fp *lfp
	)
{
	int        ch, neg = 0; 
	u_int32    q, r;

	/*
	 * We understand numbers of the form:
	 *
	 * [spaces][-|+][digits][.][digits][spaces|\n|\0]
	 *
	 * This is kinda hack.  We use 'atolfp' to do the basic parsing
	 * (after some initial checks) and then divide the result by
	 * 1000.  The original implementation avoided that by
	 * hacking up the input string to move the decimal point, but
	 * that needed string manipulations prone to buffer overruns.
	 * To avoid that trouble we do the conversion first and adjust
	 * the result.
	 */
	
	while (isspace(ch = *(const unsigned char*)str))
		++str;
	
	switch (ch) {
	    case '-': neg = TRUE;
	    case '+': ++str;
	    default : break;
	}
	
	if (!isdigit(ch = *(const unsigned char*)str) && (ch != '.'))
		return 0;
	if (!atolfp(str, lfp))
		return 0;

	/* now do a chained/overlapping division by 1000 to get from
	 * seconds to msec. 1000 is small enough to go with temporary
	 * 32bit accus for Q and R.
	 */
	q = lfp->l_ui / 1000u;
	r = lfp->l_ui - (q * 1000u);
	lfp->l_ui = q;

	r = (r << 16) | (lfp->l_uf >> 16);
	q = r / 1000u;
	r = ((r - q * 1000) << 16) | (lfp->l_uf & 0x0FFFFu);
	lfp->l_uf = q << 16;
	q = r / 1000;
	lfp->l_uf |= q;
	r -= q * 1000u;

	/* fix sign */
	if (neg)
		L_NEG(lfp);
	/* round */
	if (r >= 500)
		L_ADDF(lfp, (neg ? -1 : 1));
	return 1;
}
