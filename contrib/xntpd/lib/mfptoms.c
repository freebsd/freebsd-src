/* mfptoms.c,v 3.1 1993/07/06 01:08:31 jbj Exp
 * mfptoms - Return an asciized signed LONG fp number in milliseconds
 */
#include "ntp_fp.h"
#include "ntp_stdlib.h"

char *
mfptoms(fpi, fpf, ndec)
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

	return dolfptoa(fpi, fpf, isneg, ndec, 1);
}
