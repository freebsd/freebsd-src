/* Variant of s_cat that allows the target of a concatenation to */
/* appear on its right-hand side (contrary to the Fortran 77 Standard). */

#include "f2c.h"
#undef abs
#ifdef KR_headers
 extern char *malloc();
 extern void free();
#else
#include "stdlib.h"
#endif
#include "string.h"

 static VOID
#ifdef KR_headers
s_cat0(lp, rpp, rnp, n, ll) char *lp, *rpp[]; ftnlen rnp[], n, ll;
#else
s_cat0(char *lp, char *rpp[], ftnlen rnp[], ftnlen n, ftnlen ll)
#endif
{
	ftnlen i, nc;
	char *rp;

	for(i = 0 ; i < n ; ++i) {
		nc = ll;
		if(rnp[i] < nc)
			nc = rnp[i];
		ll -= nc;
		rp = rpp[i];
		while(--nc >= 0)
			*lp++ = *rp++;
		}
	while(--ll >= 0)
		*lp++ = ' ';
	}

 VOID
#ifdef KR_headers
s_cat(lp, rpp, rnp, np, ll) char *lp, *rpp[]; ftnlen rnp[], *np, ll;
#else
s_cat(char *lp, char *rpp[], ftnlen rnp[], ftnlen *np, ftnlen ll)
#endif
{
	ftnlen i, L, m, n;
	char *lpe, *rp;

	n = *np;
	lpe = lp;
	L = ll;
	i = 0;
	while(i < n) {
		rp = rpp[i];
		m = rnp[i++];
		if (rp >= lpe || rp + m <= lp) {
			if ((L -= m) <= 0) {
				n = i;
				break;
				}
			lpe += m;
			continue;
			}
		lpe = malloc(ll);
		s_cat0(lpe, rpp, rnp, n, ll);
		memcpy(lp, lpe, ll);
		free(lpe);
		return;
		}
	s_cat0(lp, rpp, rnp, n, ll);
	}
