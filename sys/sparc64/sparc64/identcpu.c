/*
 * Initial implementation:
 * Copyright (c) 2001 Robert Drehmel
 * All rights reserved.
 *
 * As long as the above copyright statement and this notice remain
 * unchanged, you can do what ever you want with this file.
 *
 * $FreeBSD$
 */
#include <sys/param.h>
#include <sys/systm.h>

#include <machine/cpufunc.h>
#include <machine/md_var.h>
#include <machine/ver.h>

void
cpu_identify(unsigned int freq)
{
	const char *manus;
	const char *impls;
	unsigned long vers;

	manus = NULL;
	impls = NULL;
	vers = rdpr(ver);

	switch (VER_MANUF(vers)) {
	case 0x04:
		manus = "HAL";
		break;
	case 0x13:
	case 0x17:
		manus = "Sun Microsystems";
		break;
	}
	switch (VER_IMPL(vers)) {
	case 0x01:
		impls = "SPARC64";
		break;
	case 0x10:
		impls = "UltraSparc-I";
		break;
	case 0x11:
		impls = "UltraSparc-II";
		break;
	case 0x12:
		impls = "UltraSparc-IIi";
		break;
	case 0x13:
		/* V9 Manual says `UltraSparc-e'.  I assume this is wrong. */
		impls = "UltraSparc-IIe";
		break;
	}
	if (manus == NULL || impls == NULL) {
		printf(
		    "CPU: unknown; please e-mail the following value together\n"
		    "     with the exact name of your processor to "
		    "<freebsd-sparc@FreeBSD.org>.\n"
		    "     version register: <0x%lx>\n", vers);
		return;
	}

	printf("CPU: %s %s Processor (%d.%02d MHZ CPU)\n", manus, impls,
	    (freq + 4999) / 1000000, ((freq + 4999) / 10000) % 100);
	if (bootverbose) {
		printf("  mask=0x%lx maxtl=%ld maxwin=%ld\n", VER_MASK(vers),
		    VER_MAXTL(vers), VER_MAXWIN(vers));
	}
}
