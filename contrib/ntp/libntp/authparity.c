/*
 * auth_parity - set parity on a key/check for odd parity
 */
#include "ntp_stdlib.h"

int
DESauth_parity(
	u_int32 *key
	)
{
	u_int32 mask;
	int parity_err;
	int bitcount;
	int half;
	int byte;
	int i;

	/*
	 * Go through counting bits in each byte.  Check to see if
	 * each parity bit was set correctly.  If not, note the error
	 * and set it right.
	 */
	parity_err = 0;
	for (half = 0; half < 2; half++) {		/* two halves of key */
		mask = 0x80000000;
		for (byte = 0; byte < 4; byte++) {	/* 4 bytes per half */
			bitcount = 0;
			for (i = 0; i < 7; i++) {	/* 7 data bits / byte */
				if (key[half] & mask)
				    bitcount++;
				mask >>= 1;
			}

			/*
			 * If bitcount is even, parity must be set.  If
			 * bitcount is odd, parity must be clear.
			 */
			if ((bitcount & 0x1) == 0) {
				if (!(key[half] & mask)) {
					parity_err++;
					key[half] |= mask;
				}
			} else {
				if (key[half] & mask) {
					parity_err++;
					key[half] &= ~mask;
				}
			}
			mask >>= 1;
		}
	}

	/*
	 * Return the result of the parity check.
	 */
	return (parity_err == 0);
}
