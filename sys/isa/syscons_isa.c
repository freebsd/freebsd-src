/*-
 * Copyright (c) 1999 Kazutaka YOKOTA <yokota@zodiac.mech.utsunomiya-u.ac.jp>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_syscons.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/cons.h>
#include <sys/kbio.h>
#include <sys/consio.h>
#include <sys/sysctl.h>

#if defined(__i386__) || defined(__amd64__)

#include <machine/clock.h>
#include <machine/md_var.h>
#include <machine/pc/bios.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_param.h>

#define BIOS_CLKED	(1 << 6)
#define BIOS_NLKED	(1 << 5)
#define BIOS_SLKED	(1 << 4)
#define BIOS_ALKED	0

#endif

#include <dev/syscons/syscons.h>

#include <isa/isavar.h>

#include "opt_xbox.h"

#ifdef XBOX
#include <machine/xbox.h>
#endif

static devclass_t	sc_devclass;

static sc_softc_t	main_softc;
#ifdef SC_NO_SUSPEND_VTYSWITCH
static int sc_no_suspend_vtswitch = 1;
#else
static int sc_no_suspend_vtswitch = 0;
#endif
static int sc_cur_scr;

TUNABLE_INT("hw.syscons.sc_no_suspend_vtswitch", &sc_no_suspend_vtswitch);
SYSCTL_DECL(_hw_syscons);
SYSCTL_INT(_hw_syscons, OID_AUTO, sc_no_suspend_vtswitch, CTLFLAG_RW,
    &sc_no_suspend_vtswitch, 0, "Disable VT switch before suspend.");

static void
scidentify(driver_t *driver, device_t parent)
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
	return (sc_probe_unit(device_get_unit(dev), device_get_flags(dev)));
}

static int
scattach(device_t dev)
{

	return (sc_attach_unit(device_get_unit(dev), device_get_flags(dev) |
	    SC_AUTODETECT_KBD));
}

static int
scsuspend(device_t dev)
{
	int		retry = 10;
	sc_softc_t	*sc;

	sc = &main_softc;

	if (sc->cur_scp == NULL)
		return (0);

	if (sc->suspend_in_progress == 0) {
		sc_cur_scr = sc->cur_scp->index;
		if (!sc_no_suspend_vtswitch && sc_cur_scr != 0)
			do {
				sc_switch_scr(sc, 0);
				if (!sc->switch_in_progress)
					break;
				pause("scsuspend", hz);
			} while (retry--);
	}
	sc->suspend_in_progress++;

	return (0);
}

static int
scresume(device_t dev)
{
	sc_softc_t	*sc;

	sc = &main_softc;

	sc->suspend_in_progress--;
	if (sc->suspend_in_progress == 0)
		if (!sc_no_suspend_vtswitch && sc_cur_scr != 0)
			sc_switch_scr(sc, sc_cur_scr);

	return (0);
}

int
sc_max_unit(void)
{

	return (devclass_get_maxunit(sc_devclass));
}

sc_softc_t
*sc_get_softc(int unit, int flags)
{
	sc_softc_t *sc;

	if (unit < 0)
		return (NULL);
	if ((flags & SC_KERNEL_CONSOLE) != 0) {
		/* FIXME: clear if it is wired to another unit! */
		sc = &main_softc;
	} else {
	        sc = device_get_softc(devclass_get_device(sc_devclass, unit));
		if (sc == NULL)
			return (NULL);
	}
	sc->unit = unit;
	if ((sc->flags & SC_INIT_DONE) == 0) {
		sc->keyboard = -1;
		sc->adapter = -1;
		sc->cursor_char = SC_CURSOR_CHAR;
		sc->mouse_char = SC_MOUSE_CHAR;
	}
	return (sc);
}

sc_softc_t
*sc_find_softc(struct video_adapter *adp, struct keyboard *kbd)
{
	sc_softc_t *sc;
	int i;
	int units;

	sc = &main_softc;
	if ((adp == NULL || adp == sc->adp) &&
	    (kbd == NULL || kbd == sc->kbd))
		return (sc);
	units = devclass_get_maxunit(sc_devclass);
	for (i = 0; i < units; ++i) {
	        sc = device_get_softc(devclass_get_device(sc_devclass, i));
		if (sc == NULL)
			continue;
		if ((adp == NULL || adp == sc->adp) &&
		    (kbd == NULL || kbd == sc->kbd))
			return (sc);
	}
	return (NULL);
}

int
sc_get_cons_priority(int *unit, int *flags)
{
	const char *at;
	int f, u;

#ifdef XBOX
	/*
	 * The XBox Loader does not support hints, which makes our initial
	 * console probe fail. Therefore, if an XBox is found, we hardcode the
	 * existence of the console, as it is always there anyway.
	 */
	if (arch_i386_is_xbox) {
		*unit = 0;
		*flags = SC_KERNEL_CONSOLE;
		return (CN_INTERNAL);
	}
#endif

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
	if (*unit < 0) {
		*unit = 0;
		*flags = 0;
	}
#if 0
	return ((*flags & SC_KERNEL_CONSOLE) != 0 ? CN_INTERNAL : CN_NORMAL);
#endif
	return (CN_INTERNAL);
}

void
sc_get_bios_values(bios_values_t *values)
{
#if defined(__i386__) || defined(__amd64__)
	uint8_t shift;

	values->cursor_start = *(uint8_t *)BIOS_PADDRTOVADDR(0x461);
	values->cursor_end = *(uint8_t *)BIOS_PADDRTOVADDR(0x460);
	shift = *(uint8_t *)BIOS_PADDRTOVADDR(0x417);
	values->shift_state = ((shift & BIOS_CLKED) != 0 ? CLKED : 0) |
	    ((shift & BIOS_NLKED) != 0 ? NLKED : 0) |
	    ((shift & BIOS_SLKED) != 0 ? SLKED : 0) |
	    ((shift & BIOS_ALKED) != 0 ? ALKED : 0);
#else
	values->cursor_start = 0;
	values->cursor_end = 32;
	values->shift_state = 0;
#endif
	values->bell_pitch = BELL_PITCH;
}

int
sc_tone(int herz)
{

#if defined(HAS_TIMER_SPKR)
	if (herz) {
		if (timer_spkr_acquire())
			return (EBUSY);
		timer_spkr_setfreq(herz);
	} else
		timer_spkr_release();
#endif

	return (0);
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

static devclass_t	scpm_devclass;

static void
scpm_identify(driver_t *driver, device_t parent)
{

	device_add_child(parent, "scpm", 0);
}

static int
scpm_probe(device_t dev)
{

	device_set_desc(dev, SC_DRIVER_NAME " suspend/resume");
	device_quiet(dev);

	return (BUS_PROBE_DEFAULT);
}

static int
scpm_attach(device_t dev)
{

	bus_generic_probe(dev);
	bus_generic_attach(dev);

	return (0);
}

static int
scpm_suspend(device_t dev)
{
	int error;

	error = bus_generic_suspend(dev);
	if (error != 0)
		return (error);

	return (scsuspend(dev));
}

static int
scpm_resume(device_t dev)
{

	scresume(dev);

	return (bus_generic_resume(dev));
}

static device_method_t scpm_methods[] = {
	DEVMETHOD(device_identify,	scpm_identify),
	DEVMETHOD(device_probe,		scpm_probe),
	DEVMETHOD(device_attach,	scpm_attach),
	DEVMETHOD(device_suspend,	scpm_suspend),
	DEVMETHOD(device_resume,	scpm_resume),
	{ 0, 0 }
};

static driver_t scpm_driver = {
	"scpm",
	scpm_methods,
	0
};

DRIVER_MODULE(scpm, vgapm, scpm_driver, scpm_devclass, 0, 0);
