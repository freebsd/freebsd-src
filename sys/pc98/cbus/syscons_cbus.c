/*-
 * Copyright (c) 1999 FreeBSD(98) Porting Team.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include "opt_syscons.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/cons.h>
#include <sys/consio.h>
#include <sys/sysctl.h>

#include <machine/clock.h>

#include <pc98/pc98/pc98.h>
#include <pc98/pc98/pc98_machdep.h>

#include <dev/syscons/syscons.h>

#include <i386/isa/timerreg.h>

#include <isa/isavar.h>

static devclass_t	sc_devclass;

static sc_softc_t main_softc;
#ifdef SC_NO_SUSPEND_VTYSWITCH
static int sc_no_suspend_vtswitch = 1;
#else
static int sc_no_suspend_vtswitch = 0;
#endif
static int sc_cur_scr;

TUNABLE_INT("hw.syscons.sc_no_suspend_vtswitch", (int *)&sc_no_suspend_vtswitch);
SYSCTL_DECL(_hw_syscons);
SYSCTL_INT(_hw_syscons, OID_AUTO, sc_no_suspend_vtswitch, CTLFLAG_RW,
	&sc_no_suspend_vtswitch, 0, "Disable VT switch before suspend.");

static void
scidentify (driver_t *driver, device_t parent)
{
	BUS_ADD_CHILD(parent, ISA_ORDER_SPECULATIVE, "sc", 0);
}

static int
scprobe(device_t dev)
{
	/* No pnp support */
	if (isa_get_vendorid(dev))
		return (ENXIO);

	device_set_desc(dev, "System console");
	return sc_probe_unit(device_get_unit(dev), device_get_flags(dev));
}

static int
scattach(device_t dev)
{
	return sc_attach_unit(device_get_unit(dev), device_get_flags(dev));
}

static int
scsuspend(device_t dev)
{
	int		retry = 10;
	static int	dummy;
	sc_softc_t	*sc;

	sc = &main_softc;

	if (sc->cur_scp == NULL)
		return (0);

	sc_cur_scr = sc->cur_scp->index;

	if (sc_no_suspend_vtswitch)
		return (0);

	do {
		sc_switch_scr(sc, 0);
		if (!sc->switch_in_progress) {
			break;
		}
		tsleep(&dummy, 0, "scsuspend", 100);
	} while (retry--);

	return (0);
}

static int
scresume(device_t dev)
{
	sc_softc_t	*sc;

	if (sc_no_suspend_vtswitch)
		return (0);

	sc = &main_softc;
	sc_switch_scr(sc, sc_cur_scr);

	return (0);
}

int
sc_max_unit(void)
{
	return devclass_get_maxunit(sc_devclass);
}

sc_softc_t
*sc_get_softc(int unit, int flags)
{
	sc_softc_t *sc;

	if (unit < 0)
		return NULL;
	if (flags & SC_KERNEL_CONSOLE) {
		/* FIXME: clear if it is wired to another unit! */
		sc = &main_softc;
	} else {
	        sc = (sc_softc_t *)device_get_softc(devclass_get_device(sc_devclass, unit));
		if (sc == NULL)
			return NULL;
	}
	sc->unit = unit;
	if (!(sc->flags & SC_INIT_DONE)) {
		sc->keyboard = -1;
		sc->adapter = -1;
		sc->mouse_char = SC_MOUSE_CHAR;
	}
	return sc;
}

sc_softc_t
*sc_find_softc(struct video_adapter *adp, struct keyboard *kbd)
{
	sc_softc_t *sc;
	int units;
	int i;

	sc = &main_softc;
	if (((adp == NULL) || (adp == sc->adp))
	    && ((kbd == NULL) || (kbd == sc->kbd)))
		return sc;
	units = devclass_get_maxunit(sc_devclass);
	for (i = 0; i < units; ++i) {
	        sc = (sc_softc_t *)device_get_softc(devclass_get_device(sc_devclass, i));
		if (sc == NULL)
			continue;
		if (((adp == NULL) || (adp == sc->adp))
		    && ((kbd == NULL) || (kbd == sc->kbd)))
			return sc;
	}
	return NULL;
}

int
sc_get_cons_priority(int *unit, int *flags)
{
	const char *at;
	int u, f;

	*unit = -1;
	for (u = 0; u < 16; u++) {
		if (resource_disabled(SC_DRIVER_NAME, u))
			continue;
		if (resource_string_value(SC_DRIVER_NAME, u, "at", &at) != 0)
			continue;
		if (resource_int_value(SC_DRIVER_NAME, u, "flags", &f) != 0)
			f = 0;
		if (f & SC_KERNEL_CONSOLE) {
			/* the user designates this unit to be the console */
			*unit = u;
			*flags = f;
			break;
		}
		if (*unit < 0) {
			/* ...otherwise remember the first found unit */
			*unit = u;
			*flags = f;
		}
	}
	if (*unit < 0)
		return CN_DEAD;
	return CN_INTERNAL;
}

void
sc_get_bios_values(bios_values_t *values)
{
	values->cursor_start = 15;
	values->cursor_end = 16;
	values->shift_state = 0;
	if (pc98_machine_type & M_8M)
		values->bell_pitch = BELL_PITCH_8M;
	else
		values->bell_pitch = BELL_PITCH_5M;
}

int
sc_tone(int herz)
{
	int pitch;

	if (herz) {
		/* enable counter 1 */
		outb(0x35, inb(0x35) & 0xf7);
		/* set command for counter 1, 2 byte write */
		if (acquire_timer1(TIMER_16BIT | TIMER_SQWAVE))
			return EBUSY;
		/* set pitch */
		pitch = timer_freq/herz;
		outb(TIMER_CNTR1, pitch);
		outb(TIMER_CNTR1, pitch >> 8);
	} else {
		/* disable counter 1 */
		outb(0x35, inb(0x35) | 0x08);
		release_timer1();
	}
	return 0;
}

static device_method_t sc_methods[] = {
	DEVMETHOD(device_identify,	scidentify),
	DEVMETHOD(device_probe,         scprobe),
	DEVMETHOD(device_attach,        scattach),
	DEVMETHOD(device_suspend,       scsuspend),
	DEVMETHOD(device_resume,        scresume),
	{ 0, 0 }
};

static driver_t sc_driver = {
	SC_DRIVER_NAME,
	sc_methods,
	sizeof(sc_softc_t),
};

DRIVER_MODULE(sc, isa, sc_driver, sc_devclass, 0, 0);
