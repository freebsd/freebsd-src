/****************************************************************
Copyright 1990 by AT&T, Lucent Technologies and Bellcore.

Permission to use, copy, modify, and distribute this software
and its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all
copies and that both that the copyright notice and this
permission notice and warranty disclaimer appear in supporting
documentation, and that the names of AT&T, Bell Laboratories,
Lucent or Bellcore or any of their entities not be used in
advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

AT&T, Lucent and Bellcore disclaim all warranties with regard to
this software, including all implied warranties of
merchantability and fitness.  In no event shall AT&T, Lucent or
Bellcore be liable for any special, indirect or consequential
damages or any damages whatsoever resulting from loss of use,
data or profits, whether in an action of contract, negligence or
other tortious action, arising out of or in connection with the
use or performance of this software.
****************************************************************/

/* This is for the benefit of people whose systems don't provide
 * memset, memcpy, and memcmp.  If yours is such a system, adjust
 * the makefile by adding memset.o to the "OBJECTS =" assignment.
 * WARNING: the memcpy below is adequate for f2c, but is not a
 * general memcpy routine (which must correctly handle overlapping
 * fields).
 */

 int
memcmp(s1, s2, n)
 register char *s1, *s2;
 int n;
{
	register char *se;

	for(se = s1 + n; s1 < se; s1++, s2++)
		if (*s1 != *s2)
			return *s1 - *s2;
	return 0;
	}

 char *
memcpy(s1, s2, n)
 register char *s1, *s2;
 int n;
{
	register char *s0 = s1, *se = s1 + n;

	while(s1 < se)
		*s1++ = *s2++;
	return s0;
	}

memset(s, c, n)
 register char *s;
 register int c;
 int n;
{
	register char *se = s + n;

	while(s < se)
		*s++ = c;
	}
