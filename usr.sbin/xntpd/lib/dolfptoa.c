/* dolfptoa.c,v 3.1 1993/07/06 01:08:14 jbj Exp
 * dolfptoa - do the grunge work of converting an l_fp number to decimal
 */
#include <stdio.h>

#include "ntp_fp.h"
#include "lib_strbuf.h"
#include "ntp_string.h"
#include "ntp_stdlib.h"

char *
dolfptoa(fpi, fpv, neg, ndec, msec)
	U_LONG fpi;
	U_LONG fpv;
	int neg;
	int ndec;
	int msec;
{
	register u_char *cp, *cpend;
	register U_LONG work_i;
	register int dec;
	u_char cbuf[24];
	u_char *cpdec;
	char *buf;
	char *bp;

	/*
	 * Get a string buffer before starting
	 */
	LIB_GETBUF(buf);

	/*
	 * Zero the character buffer
	 */
	bzero((char *) cbuf, sizeof(cbuf));

	/*
	 * Work on the integral part.  This is biased by what I know
	 * compiles fairly well for a 68000.
	 */
	cp = cpend = &cbuf[10];
	work_i = fpi;
	if (work_i & 0xffff0000) {
		register U_LONG lten = 10;
		register U_LONG ltmp;

		do {
			ltmp = work_i;
			work_i /= lten;
			ltmp -= (work_i<<3) + (work_i<<1);
			*--cp = (u_char)ltmp;
		} while (work_i & 0xffff0000);
	}
	if (work_i != 0) {
		register u_short sten = 10;
		register u_short stmp;
		register u_short swork = (u_short)work_i;

		do {
			stmp = swork;
			swork /= sten;
			stmp -= (swork<<3) + (swork<<1);
			*--cp = (u_char)stmp;
		} while (swork != 0);
	}

	/*
	 * Done that, now deal with the problem of the fraction.  First
	 * determine the number of decimal places.
	 */
	if (msec) {
		dec = ndec + 3;
		if (dec < 3)
			dec = 3;
		cpdec = &cbuf[13];
	} else {
		dec = ndec;
		if (dec < 0)
			dec = 0;
		cpdec = &cbuf[10];
	}
	if (dec > 12)
		dec = 12;
	
	/*
	 * If there's a fraction to deal with, do so.
	 */
	if (fpv != 0) {
		register U_LONG work_f;

		work_f = fpv;
		while (dec > 0) {
			register U_LONG tmp_i;
			register U_LONG tmp_f;

			dec--;
			/*
			 * The scheme here is to multiply the
			 * fraction (0.1234...) by ten.  This moves
			 * a junk of BCD into the units part.
			 * record that and iterate.
			 */
			work_i = 0;
			M_LSHIFT(work_i, work_f);
			tmp_i = work_i;
			tmp_f = work_f;
			M_LSHIFT(work_i, work_f);
			M_LSHIFT(work_i, work_f);
			M_ADD(work_i, work_f, tmp_i, tmp_f);
			*cpend++ = (u_char)work_i;
			if (work_f == 0)
				break;
		}

		/*
		 * Rounding is rotten
		 */
		if (work_f & 0x80000000) {
			register u_char *tp = cpend;

			*(--tp) += 1;
			while (*tp >= 10) {
				*tp = 0;
				*(--tp) += 1;
			};
			if (tp < cp)
				cp = tp;
		}
	}
	cpend += dec;


	/*
	 * We've now got the fraction in cbuf[], with cp pointing at
	 * the first character, cpend pointing past the last, and
	 * cpdec pointing at the first character past the decimal.
	 * Remove leading zeros, then format the number into the
	 * buffer.
	 */
	while (cp < cpdec) {
		if (*cp != 0)
			break;
		cp++;
	}
	if (cp == cpdec)
		--cp;

	bp = buf;
	if (neg)
		*bp++ = '-';
	while (cp < cpend) {
		if (cp == cpdec)
			*bp++ = '.';
		*bp++ = (char)(*cp++ + '0');	/* ascii dependent? */
	}
	*bp = '\0';

	/*
	 * Done!
	 */
	return buf;
}
