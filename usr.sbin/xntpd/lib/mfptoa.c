/* mfptoa.c,v 3.1 1993/07/06 01:08:30 jbj Exp
 * mfptoa - Return an asciized representation of a signed LONG fp number
 */
#include "ntp_fp.h"
#include "ntp_stdlib.h"

char *
mfptoa(fpi, fpf, ndec)
	U_LONG fpi;
	U_LONG fpf;
	int ndec;
{
	int isneg;

	if (M_ISNEG(fpi, fpf)) {
		isneg = 1;
		M_NEG(fpi, fpf);
	} else
		isneg = 0;

	return dolfptoa(fpi, fpf, isneg, ndec, 0);
}
