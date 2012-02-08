/*
 * Copyright (c) 2010
 *	Ben Gray <ben.r.gray@gmail.com>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Ben Gray.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BEN GRAY ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BEN GRAY BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 *	SCM - System Control Module
 *
 *	Hopefully in the end this module will contain a bunch of utility functions
 *	for configuring and querying the general system control registers, but for
 *	now it only does pin(pad) multiplexing.
 *
 *	This is different from the GPIO module in that it is used to configure the
 *	pins between modules not just GPIO input/output.
 *
 *	This file contains the generic top level driver, however it relies on chip
 *	specific settings and therefore expects an array of omap_scm_padconf structs
 *	call omap_padconf_devmap to be located somewhere in the kernel.
 *
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/frame.h>
#include <machine/resource.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "omap_scm.h"
#include "omap_if.h"

static struct resource_spec omap_scm_res_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },	/* Control memory window */
	{ -1, 0 }
};

static struct omap_scm_softc *omap_scm_sc;

#define	omap_scm_read_2(sc, reg)		\
    bus_space_read_2((sc)->sc_bst, (sc)->sc_bsh, (reg))
#define	omap_scm_write_2(sc, reg, val)		\
    bus_space_write_2((sc)->sc_bst, (sc)->sc_bsh, (reg), (val))

/**
 *	omap_padconf_devmap - Array of pins, should be defined one per SoC 
 *
 *	This array is typically defined in one of the targeted omap??_scm_pinumx.c
 *	files and is specific to the given SoC platform.  Each entry in the array
 *	corresponds to an individual pin.
 */
extern const struct omap_scm_padconf omap_padconf_devmap[];

/**
 *	omap_scm_padconf_from_name - searches the list of pads and returns entry
 *	                             with matching ball name.
 *	@ballname: the name of the ball
 *
 *	RETURNS:
 *	A pointer to the matching padconf or NULL if the ball wasn't found.
 */
static const struct omap_scm_padconf*
omap_scm_padconf_from_name(const char *ballname)
{
	const struct omap_scm_padconf *padconf;

	padconf = omap_padconf_devmap;
	while (padconf->ballname != NULL) {
		if (strcmp(ballname, padconf->ballname) == 0)
			return(padconf);
		padconf++;
	}
	
	return (NULL);
}

/**
 *	omap_scm_padconf_set_internal - sets the muxmode and state for a pad/pin
 *	@padconf: pointer to the pad structure
 *	@muxmode: the name of the mode to use for the pin, i.e. "uart1_rx"
 *	@state: the state to put the pad/pin in, i.e. PADCONF_PIN_???
 *	
 *
 *	LOCKING:
 *	Internally locks it's own context.
 *
 *	RETURNS:
 *	0 on success.
 *	EINVAL if pin requested is outside valid range or already in use.
 */
static int
omap_scm_padconf_set_internal(struct omap_scm_softc *sc,
    const struct omap_scm_padconf *padconf,
    const char *muxmode, unsigned int state)
{
	unsigned int mode;
	uint16_t reg_val;

	/* populate the new value for the PADCONF register */
	reg_val = (uint16_t)(state & CONTROL_PADCONF_SATE_MASK);

	/* find the new mode requested */
	for (mode = 0; mode < 8; mode++) {
		if ((padconf->muxmodes[mode] != NULL) && 
		    (strcmp(padconf->muxmodes[mode], muxmode) == 0)) {
			break;
		}
	}

	/* couldn't find the mux mode */
	if (mode >= 8)
		return (EINVAL);

	/* set the mux mode */
	reg_val |= (uint16_t)(mode & CONTROL_PADCONF_MUXMODE_MASK);
	
	printf("setting internal %x for %s\n", reg_val, muxmode);
	/* write the register value (16-bit writes) */
	omap_scm_write_2(sc, padconf->reg_off, reg_val);
	
	return (0);
}

/**
 *	omap_scm_padconf_set - sets the muxmode and state for a pad/pin
 *	@padname: the name of the pad, i.e. "c12"
 *	@muxmode: the name of the mode to use for the pin, i.e. "uart1_rx"
 *	@state: the state to put the pad/pin in, i.e. PADCONF_PIN_???
 *	
 *
 *	LOCKING:
 *	Internally locks it's own context.
 *
 *	RETURNS:
 *	0 on success.
 *	EINVAL if pin requested is outside valid range or already in use.
 */
int
omap_scm_padconf_set(const char *padname, const char *muxmode, unsigned int state)
{
	const struct omap_scm_padconf *padconf;

	if (!omap_scm_sc)
		return (ENXIO);

	/* find the pin in the devmap */
	padconf = omap_scm_padconf_from_name(padname);
	if (padconf == NULL)
		return (EINVAL);
	
	return (omap_scm_padconf_set_internal(omap_scm_sc, padconf, muxmode, state));
}

/**
 *	omap_scm_padconf_get - gets the muxmode and state for a pad/pin
 *	@padname: the name of the pad, i.e. "c12"
 *	@muxmode: upon return will contain the name of the muxmode of the pin
 *	@state: upon return will contain the state of the the pad/pin
 *	
 *
 *	LOCKING:
 *	Internally locks it's own context.
 *
 *	RETURNS:
 *	0 on success.
 *	EINVAL if pin requested is outside valid range or already in use.
 */
int
omap_scm_padconf_get(const char *padname, const char **muxmode,
    unsigned int *state)
{
	const struct omap_scm_padconf *padconf;
	uint16_t reg_val;

	if (!omap_scm_sc)
		return (ENXIO);

	/* find the pin in the devmap */
	padconf = omap_scm_padconf_from_name(padname);
	if (padconf == NULL)
		return (EINVAL);
	
	/* read the register value (16-bit reads) */
	reg_val = omap_scm_read_2(omap_scm_sc, padconf->reg_off);

	/* save the state */
	if (state)
		*state = (reg_val & CONTROL_PADCONF_SATE_MASK);

	/* save the mode */
	if (muxmode)
		*muxmode = padconf->muxmodes[(reg_val & CONTROL_PADCONF_MUXMODE_MASK)];
	
	return (0);
}

/**
 *	omap_scm_padconf_set_gpiomode - converts a pad to GPIO mode.
 *	@gpio: the GPIO pin number (0-195)
 *	@state: the state to put the pad/pin in, i.e. PADCONF_PIN_???
 *
 *	
 *
 *	LOCKING:
 *	Internally locks it's own context.
 *
 *	RETURNS:
 *	0 on success.
 *	EINVAL if pin requested is outside valid range or already in use.
 */
int
omap_scm_padconf_set_gpiomode(uint32_t gpio, unsigned int state)
{
	const struct omap_scm_padconf *padconf;
	uint16_t reg_val;

	if (!omap_scm_sc)
		return (ENXIO);
	
	/* find the gpio pin in the padconf array */
	padconf = omap_padconf_devmap;
	while (padconf->ballname != NULL) {
		if (padconf->gpio_pin == gpio)
			break;
		padconf++;
	}
	if (padconf->ballname == NULL)
		return (EINVAL);

	/* populate the new value for the PADCONF register */
	reg_val = (uint16_t)(state & CONTROL_PADCONF_SATE_MASK);

	/* set the mux mode */
	reg_val |= (uint16_t)(padconf->gpio_mode & CONTROL_PADCONF_MUXMODE_MASK);

	/* write the register value (16-bit writes) */
	omap_scm_write_2(omap_scm_sc, padconf->reg_off, reg_val);

	return (0);
}

/**
 *	omap_scm_padconf_get_gpiomode - gets the current GPIO mode of the pin
 *	@gpio: the GPIO pin number (0-195)
 *	@state: upon return will contain the state
 *
 *	
 *
 *	LOCKING:
 *	Internally locks it's own context.
 *
 *	RETURNS:
 *	0 on success.
 *	EINVAL if pin requested is outside valid range or not configured as GPIO.
 */
int
omap_scm_padconf_get_gpiomode(uint32_t gpio, unsigned int *state)
{
	const struct omap_scm_padconf *padconf;
	uint16_t reg_val;

	if (!omap_scm_sc)
		return (ENXIO);
	
	/* find the gpio pin in the padconf array */
	padconf = omap_padconf_devmap;
	while (padconf->ballname != NULL) {
		if (padconf->gpio_pin == gpio)
			break;
		padconf++;
	}
	if (padconf->ballname == NULL)
		return (EINVAL);

	/* read the current register settings */
	reg_val = omap_scm_read_2(omap_scm_sc, padconf->reg_off);
	
	/* check to make sure the pins is configured as GPIO in the first state */
	if ((reg_val & CONTROL_PADCONF_MUXMODE_MASK) != padconf->gpio_mode)
		return (EINVAL);
	
	/* read and store the reset of the state, i.e. pull-up, pull-down, etc */
	if (state)
		*state = (reg_val & CONTROL_PADCONF_SATE_MASK);
	
	return (0);
}

/**
 *	omap_scm_padconf_init_from_hints - processes the hints for padconf
 *	@sc: the driver soft context
 *
 *	
 *
 *	LOCKING:
 *	Internally locks it's own context.
 *
 *	RETURNS:
 *	0 on success.
 *	EINVAL if pin requested is outside valid range or already in use.
 */
static int
omap_scm_padconf_init_from_fdt(struct omap_scm_softc *sc)
{
	const struct omap_scm_padconf *padconf;
	int err;


	phandle_t node;
	int len;
	char *fdt_pad_config;
	int i;
	char *padname, *muxname, *padstate;

	node = ofw_bus_get_node(sc->sc_dev);
	len = OF_getproplen(node, "omap-pad-config");
        OF_getprop_alloc(node, "omap-pad-config", 1, (void **)&fdt_pad_config);

	i = len;
	while (i > 0) {
		padname = fdt_pad_config;
		fdt_pad_config += strlen(padname) + 1;
		i -= strlen(padname) + 1;
		if (i <= 0)
			break;

		muxname = fdt_pad_config;
		fdt_pad_config += strlen(muxname) + 1;
		i -= strlen(muxname) + 1;
		if (i <= 0)
			break;

		padstate = fdt_pad_config;
		fdt_pad_config += strlen(padstate) + 1;
		i -= strlen(padstate) + 1;
		if (i < 0)
			break;

		/* This is very inefficent ... basically we look up every possible pad name
		 * in the hints.  Unfortunatly there doesn't seem to be any way to iterate
		 * over all the hints for a given device, so instead we have to manually
		 * probe for the existance of every possible pad.
		 */
		padconf = omap_padconf_devmap;
		while (padconf->ballname != NULL) {
			if (strcmp(padconf->ballname, padname) == 0) {
				if (strcmp(padstate, "output") == 0)
					err = omap_scm_padconf_set_internal(sc,
					    padconf, muxname, PADCONF_PIN_OUTPUT);
				else if (strcmp(padstate, "input") == 0)
					err = omap_scm_padconf_set_internal(sc,
					    padconf, muxname, PADCONF_PIN_INPUT);
				else if (strcmp(padstate, "input_pullup") == 0)
					err = omap_scm_padconf_set_internal(sc,
					    padconf, muxname,
					    PADCONF_PIN_INPUT_PULLUP);
				else if (strcmp(padstate, "input_pulldown") == 0)
					err = omap_scm_padconf_set_internal(sc,
					    padconf, muxname, 
					    PADCONF_PIN_INPUT_PULLDOWN);
				else
					device_printf(sc->sc_dev, "err: padconf hint for pin \"%s\""
						      "has incorrectly formated state, ignoring hint.\n",
						      padconf->ballname);
			}
			padconf++;
		}
	}
	
	return (0);
}

/*
 * Device part of OMAP SCM driver
 */

static int
omap_scm_probe(device_t dev)
{
	if (!ofw_bus_is_compatible(dev, "ti,omap_scm"))
		return (ENXIO);

	device_set_desc(dev, "TI OMAP Control Module");
	return (BUS_PROBE_DEFAULT);
}

/**
 *	omap_scm_attach - attaches the timer to the simplebus
 *	@dev: new device
 *
 *	Reserves memory and interrupt resources, stores the softc structure
 *	globally and registers both the timecount and eventtimer objects.
 *
 *	RETURNS
 *	Zero on sucess or ENXIO if an error occuried.
 */
static int
omap_scm_attach(device_t dev)
{
	struct omap_scm_softc *sc = device_get_softc(dev);

	if (omap_scm_sc)
		return (ENXIO);

	sc->sc_dev = dev;

	if (bus_alloc_resources(dev, omap_scm_res_spec, sc->sc_res)) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}

	/* Global timer interface */
	sc->sc_bst = rman_get_bustag(sc->sc_res[0]);
	sc->sc_bsh = rman_get_bushandle(sc->sc_res[0]);

	omap_scm_sc = sc;

	omap_scm_padconf_init_from_fdt(sc);

	return (0);
}


static device_method_t omap_scm_methods[] = {
	DEVMETHOD(device_probe,		omap_scm_probe),
	DEVMETHOD(device_attach,	omap_scm_attach),
	{ 0, 0 }
};

static driver_t omap_scm_driver = {
	"omap_scm",
	omap_scm_methods,
	sizeof(struct omap_scm_softc),
};

static devclass_t omap_scm_devclass;

DRIVER_MODULE(omap_scm, simplebus, omap_scm_driver, omap_scm_devclass, 0, 0);
