/*
 * $FreeBSD: src/sys/boot/alpha/libalpha/getsecs.c,v 1.2 1999/08/28 00:39:27 peter Exp $
 * From:	$NetBSD: getsecs.c,v 1.5 1998/01/05 07:02:49 perry Exp $	
 */

#include <sys/param.h>
#include <machine/prom.h>
#include <machine/rpb.h>

int
getsecs()
{
	static long tnsec;
	static long lastpcc, wrapsecs;
	long curpcc;

	if (tnsec == 0) {
		tnsec = 1;
		lastpcc = alpha_rpcc() & 0xffffffff;
		wrapsecs = (0xffffffff /
		    ((struct rpb *)HWRPB_ADDR)->rpb_cc_freq) + 1;

#if 0
		printf("getsecs: cc freq = %d, time to wrap = %d\n",
		    ((struct rpb *)HWRPB_ADDR)->rpb_cc_freq, wrapsecs);
#endif
	}

	curpcc = alpha_rpcc() & 0xffffffff;
	if (curpcc < lastpcc)
		curpcc += 0x100000000;

	tnsec += ((curpcc - lastpcc) * 1000000000) / ((struct rpb *)HWRPB_ADDR)->rpb_cc_freq;
	lastpcc = curpcc;

	return (tnsec / 1000000000);
}
