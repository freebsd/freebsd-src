/*-
 * Initial implementation:
 * Copyright (c) 2001 Robert Drehmel
 * All rights reserved.
 *
 * As long as the above copyright statement and this notice remain
 * unchanged, you can do what ever you want with this file.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/sparc64/sparc64/identcpu.c,v 1.17 2007/05/20 13:47:36 marius Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <machine/cpufunc.h>
#include <machine/md_var.h>
#include <machine/ver.h>

char machine[] = MACHINE;
SYSCTL_STRING(_hw, HW_MACHINE, machine, CTLFLAG_RD,
    machine, 0, "Machine class");

static char cpu_model[128];
SYSCTL_STRING(_hw, HW_MODEL, model, CTLFLAG_RD,
    cpu_model, 0, "Machine model");

int cpu_impl;

void setPQL2(int *const size, int *const ways);

void
setPQL2(int *const size, int *const ways)
{
#ifdef SUN4V
/* XXX hardcoding is lame */
	*size = 3*1024;
	*ways = 12;
#endif
	return;
}

void
cpu_identify(u_long vers, u_int freq, u_int id)
{
	const char *manus;
	const char *impls;

	switch (VER_MANUF(vers)) {
	case 0x04:
		manus = "HAL";
		break;
	case 0x13:
	case 0x17:
	case 0x22:
	case 0x3e:
		manus = "Sun Microsystems";
		break;
	default:
		manus = NULL;
		break;
	}
	switch (VER_IMPL(vers)) {
	case CPU_IMPL_SPARC64:
		impls = "SPARC64";
		break;
	case CPU_IMPL_ULTRASPARCI:
		impls = "UltraSparc-I";
		break;
	case CPU_IMPL_ULTRASPARCII:
		impls = "UltraSparc-II";
		break;
	case CPU_IMPL_ULTRASPARCIIi:
		impls = "UltraSparc-IIi";
		break;
	case CPU_IMPL_ULTRASPARCIIe:
		/* V9 Manual says `UltraSparc-e'.  I assume this is wrong. */
		impls = "UltraSparc-IIe";
		break;
	case CPU_IMPL_ULTRASPARCIII:
		impls = "UltraSparc-III";
		break;
	case CPU_IMPL_ULTRASPARCIIIp:
		impls = "UltraSparc-III+";
		break;
	case CPU_IMPL_ULTRASPARCIIIi:
		impls = "UltraSparc-IIIi";
		break;
	case CPU_IMPL_ULTRASPARCIV:
		impls = "UltraSparc-IV";
		break;
	case CPU_IMPL_ULTRASPARCIVp:
		impls = "UltraSparc-IV+";
		break;
	case CPU_IMPL_ULTRASPARCIIIip:
		impls = "UltraSparc-IIIi+";
		break;
	default:
		impls = NULL;
		break;
	}
	if (manus == NULL || impls == NULL) {
		printf(
		    "CPU: unknown; please e-mail the following value together\n"
		    "     with the exact name of your processor to "
		    "<freebsd-sparc64@FreeBSD.org>.\n"
		    "     version register: <0x%lx>\n", vers);
		return;
	}

	snprintf(cpu_model, sizeof(cpu_model), "%s %s", manus, impls);
	printf("cpu%d: %s %s Processor (%d.%02d MHz CPU)\n", id, manus, impls,
	    (freq + 4999) / 1000000, ((freq + 4999) / 10000) % 100);
	if (bootverbose) {
		printf("  mask=0x%lx maxtl=%ld maxwin=%ld\n", VER_MASK(vers),
		    VER_MAXTL(vers), VER_MAXWIN(vers));
	}
}
