/*
 * fptoa - return an asciized representation of an s_fp number
 */
#include "ntp_fp.h"
#include "ntp_stdlib.h"

char *
fptoa(
	s_fp fpv,
	short ndec
	)
{
	u_fp plusfp;
	int neg;

	if (fpv < 0) {
		plusfp = (u_fp)(-fpv);
		neg = 1;
	} else {
		plusfp = (u_fp)fpv;
		neg = 0;
	}

	return dofptoa(plusfp, neg, ndec, 0);
}
