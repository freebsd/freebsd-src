/*
 * tsftomsu - convert from a time stamp fraction to milliseconds
 */
#include "ntp_fp.h"
#include "ntp_stdlib.h"

int
tsftomsu(
	u_long tsf,
	int round
	)
{
	register long val_ui, val_uf;
	register long tmp_ui, tmp_uf;
	register int i;

	/*
	 * Essentially, multiply by 10 three times in l_fp form.
	 * The integral part is the milliseconds.
	 */
	val_ui = 0;
	val_uf = tsf;
	for (i = 3; i > 0; i--) {
		M_LSHIFT(val_ui, val_uf);
		tmp_ui = val_ui;
		tmp_uf = val_uf;
		M_LSHIFT(val_ui, val_uf);
		M_LSHIFT(val_ui, val_uf);
		M_ADD(val_ui, val_uf, tmp_ui, tmp_uf);
	}

	/*
	 * Round the value if need be, then return it.
	 */
	if (round && (val_uf & 0x80000000))
	    val_ui++;
	return (int)val_ui;
}
