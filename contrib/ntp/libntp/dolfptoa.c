/*
 * dolfptoa - do the grunge work of converting an l_fp number to decimal
 */
#include <stdio.h>

#include "ntp_fp.h"
#include "lib_strbuf.h"
#include "ntp_string.h"
#include "ntp_stdlib.h"

char *
dolfptoa(
	u_long fpi,
	u_long fpv,
	int neg,
	int ndec,
	int msec
	)
{
	register u_char *cp, *cpend;
	register u_long lwork;
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
	memset((char *) cbuf, 0, sizeof(cbuf));

	/*
	 * Work on the integral part.  This is biased by what I know
	 * compiles fairly well for a 68000.
	 */
	cp = cpend = &cbuf[10];
	lwork = fpi;
	if (lwork & 0xffff0000) {
		register u_long lten = 10;
		register u_long ltmp;

		do {
			ltmp = lwork;
			lwork /= lten;
			ltmp -= (lwork << 3) + (lwork << 1);
			*--cp = (u_char)ltmp;
		} while (lwork & 0xffff0000);
	}
	if (lwork != 0) {
		register u_short sten = 10;
		register u_short stmp;
		register u_short swork = (u_short)lwork;

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
		l_fp work;

		work.l_ui = 0;
		work.l_uf = fpv;
		while (dec > 0) {
			l_fp ftmp;

			dec--;
			/*
			 * The scheme here is to multiply the
			 * fraction (0.1234...) by ten.  This moves
			 * a junk of BCD into the units part.
			 * record that and iterate.
			 */
			work.l_ui = 0;
			L_LSHIFT(&work);
			ftmp = work;
			L_LSHIFT(&work);
			L_LSHIFT(&work);
			L_ADD(&work, &ftmp);
			*cpend++ = (u_char)work.l_ui;
			if (work.l_uf == 0)
			    break;
		}

		/*
		 * Rounding is rotten
		 */
		if (work.l_uf & 0x80000000) {
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
