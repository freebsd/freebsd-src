/*-
 * Copyright (c) 2018 Stormshield
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/gpio.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include "gpio_if.h"

#include "lewisburg_gpiocm.h"
#include "p2sb.h"

#define PADBAR	0x00c

#define PADCFG0_GPIORXDIS	(1<<9)
#define PADCFG0_GPIOTXDIS	(1<<8)
#define PADCFG0_GPIORXSTATE	(1<<1)
#define PADCFG0_GPIOTXSTATE	(1<<0)

#define MAX_PAD_PER_GROUP	24

#define LBGGPIOCM_READ(sc, reg) p2sb_port_read_4(sc->p2sb, sc->port, reg)
#define LBGGPIOCM_WRITE(sc, reg, val) \
	p2sb_port_write_4(sc->p2sb, sc->port, reg, val)
#define LBGGPIOCM_LOCK(sc) p2sb_lock(sc->p2sb)
#define LBGGPIOCM_UNLOCK(sc) p2sb_unlock(sc->p2sb)

struct lbggroup {
	int groupid;
	int npins;
	int pins_off;
	device_t dev;
	char grpname;
};

struct lbgcommunity {
	uint8_t npins;
	const char *name;
	uint32_t pad_off;
	struct lbggroup groups[3];
	int ngroups;
	const char *grpnames;
};
#define LBG_COMMUNITY(n, np, g) \
{ \
	.name = n, \
	.npins = np, \
	.grpnames = g, \
}

static struct lbgcommunity lbg_communities[] = {
	LBG_COMMUNITY("LewisBurg GPIO Community 0", 72, "ABF"),
	LBG_COMMUNITY("LewisBurg GPIO Community 1", 61, "CDE"),
	LBG_COMMUNITY("LewisBurg GPIO Community 2", 0, ""),
	LBG_COMMUNITY("LewisBurg GPIO Community 3", 12, "I"),
	LBG_COMMUNITY("LewisBurg GPIO Community 4", 36, "JK"),
	LBG_COMMUNITY("LewisBurg GPIO Community 5", 66, "GHL"),
};

struct lbggpiocm_softc
{
	int port;
	device_t p2sb;
	struct lbgcommunity *community;
};

static struct lbggroup *lbggpiocm_get_group(struct lbggpiocm_softc *sc,
    device_t child);

static __inline struct lbggroup *
lbggpiocm_get_group(struct lbggpiocm_softc *sc, device_t child)
{
	int i;

	for (i = 0; i < sc->community->ngroups; ++i)
		if (sc->community->groups[i].dev == child)
			return (&sc->community->groups[i]);
	return (NULL);
}


static __inline uint32_t
lbggpiocm_getpad(struct lbggpiocm_softc *sc, uint32_t pin)
{

	if (pin >= sc->community->npins)
		return (0);

	return (sc->community->pad_off + 2 * 4 * pin);
}

int
lbggpiocm_get_group_npins(device_t dev, device_t child)
{
	struct lbggpiocm_softc *sc = device_get_softc(dev);
	struct lbggroup *group;

	group = lbggpiocm_get_group(sc, child);
	if (group != NULL)
		return (group->npins);
	return (-1);
}

char
lbggpiocm_get_group_name(device_t dev, device_t child)
{
	struct lbggpiocm_softc *sc = device_get_softc(dev);
	struct lbggroup *group;

	group = lbggpiocm_get_group(sc, child);
	if (group != NULL)
		return (group->grpname);
	return ('\0');
}

static int
lbggpiocm_pin2cpin(struct lbggpiocm_softc *sc, device_t child, uint32_t pin)
{
	struct lbggroup *group;

	group = lbggpiocm_get_group(sc, child);
	if (group != NULL)
		return (pin + group->pins_off);
	return (-1);
}

int
lbggpiocm_pin_setflags(device_t dev, device_t child, uint32_t pin, uint32_t flags)
{
	struct lbggpiocm_softc *sc = device_get_softc(dev);
	uint32_t padreg, padval;
	int rpin;

	if ((flags & (GPIO_PIN_INPUT | GPIO_PIN_OUTPUT)) ==
	    (GPIO_PIN_INPUT | GPIO_PIN_OUTPUT))
		return (EINVAL);

	if ((flags & (GPIO_PIN_INPUT | GPIO_PIN_OUTPUT)) == 0)
		return (EINVAL);

	rpin = lbggpiocm_pin2cpin(sc, child, pin);
	if (rpin < 0)
		return (EINVAL);

	padreg = lbggpiocm_getpad(sc, rpin);

	LBGGPIOCM_LOCK(sc);
	padval = LBGGPIOCM_READ(sc, padreg);

	if (flags & GPIO_PIN_INPUT) {
		padval &= ~PADCFG0_GPIORXDIS;
		padval |= PADCFG0_GPIOTXDIS;
	} else if (flags & GPIO_PIN_OUTPUT) {
		padval &= ~PADCFG0_GPIOTXDIS;
		padval |= PADCFG0_GPIORXDIS;
	}

	LBGGPIOCM_WRITE(sc, padreg, padval);
	LBGGPIOCM_UNLOCK(sc);

	return (0);
}

int
lbggpiocm_pin_get(device_t dev, device_t child, uint32_t pin, uint32_t *value)
{
	struct lbggpiocm_softc *sc = device_get_softc(dev);
	uint32_t padreg, val;
	int rpin;

	if (value == NULL)
		return (EINVAL);

	rpin = lbggpiocm_pin2cpin(sc, child, pin);
	if (rpin < 0)
		return (EINVAL);

	padreg = lbggpiocm_getpad(sc, rpin);

	LBGGPIOCM_LOCK(sc);
	val = LBGGPIOCM_READ(sc, padreg);
	LBGGPIOCM_UNLOCK(sc);

	if (!(val & PADCFG0_GPIOTXDIS))
		*value = !!(val & PADCFG0_GPIOTXSTATE);
	else
		*value = !!(val & PADCFG0_GPIORXSTATE);

	return (0);
}

int
lbggpiocm_pin_set(device_t dev, device_t child, uint32_t pin, uint32_t value)
{
	struct lbggpiocm_softc *sc = device_get_softc(dev);
	uint32_t padreg, padcfg;
	int rpin;

	rpin = lbggpiocm_pin2cpin(sc, child, pin);
	if (rpin < 0)
		return (EINVAL);

	padreg = lbggpiocm_getpad(sc, rpin);

	LBGGPIOCM_LOCK(sc);

	padcfg = LBGGPIOCM_READ(sc, padreg);
	if (value)
		padcfg |= PADCFG0_GPIOTXSTATE;
	else
		padcfg &= ~PADCFG0_GPIOTXSTATE;
	LBGGPIOCM_WRITE(sc, padreg, padcfg);

	LBGGPIOCM_UNLOCK(sc);

	return (0);
}

int
lbggpiocm_pin_toggle(device_t dev, device_t child, uint32_t pin)
{
	struct lbggpiocm_softc *sc = device_get_softc(dev);
	uint32_t padreg, padcfg;
	int rpin;

	rpin = lbggpiocm_pin2cpin(sc, child, pin);
	if (rpin < 0)
		return (EINVAL);

	padreg = lbggpiocm_getpad(sc, rpin);

	LBGGPIOCM_LOCK(sc);
	padcfg = LBGGPIOCM_READ(sc, padreg);
	padcfg ^= PADCFG0_GPIOTXSTATE;
	LBGGPIOCM_WRITE(sc, padreg, padcfg);

	LBGGPIOCM_UNLOCK(sc);

	return (0);
}

static int
lbggpiocm_probe(device_t dev)
{
	struct lbggpiocm_softc *sc = device_get_softc(dev);
	int unit;

	sc->p2sb = device_get_parent(dev);
	unit = device_get_unit(dev);
	KASSERT(unit < nitems(lbg_communities), ("Wrong number of devices or communities"));
	sc->port = p2sb_get_port(sc->p2sb, unit);
	sc->community = &lbg_communities[unit];
	if (sc->port < 0)
		return (ENXIO);

	device_set_desc(dev, sc->community->name);
	return (BUS_PROBE_DEFAULT);
}

static int
lbggpiocm_attach(device_t dev)
{
	uint32_t npins;
	struct lbggpiocm_softc *sc;
	struct lbggroup *group;
	int i;

	sc = device_get_softc(dev);
	if (sc->community->npins == 0)
		return (ENXIO);

	LBGGPIOCM_LOCK(sc);
	sc->community->pad_off = LBGGPIOCM_READ(sc, PADBAR);
	LBGGPIOCM_UNLOCK(sc);

	npins = sc->community->npins;
	for (i = 0; i < nitems(sc->community->groups) && npins > 0; ++i) {
		group = &sc->community->groups[i];

		group->groupid = i;
		group->grpname = sc->community->grpnames[i];
		group->pins_off = i * MAX_PAD_PER_GROUP;
		group->npins = npins < MAX_PAD_PER_GROUP ? npins :
			MAX_PAD_PER_GROUP;
		npins -= group->npins;
		group->dev = device_add_child(dev, "gpio", -1);
	}
	sc->community->ngroups = i;
	return (bus_generic_attach(dev));
}

static int
lbggpiocm_detach(device_t dev)
{
	int error;

	error = device_delete_children(dev);
	if (error)
		return (error);

	return (bus_generic_detach(dev));
}

static device_method_t lbggpiocm_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		lbggpiocm_probe),
	DEVMETHOD(device_attach,	lbggpiocm_attach),
	DEVMETHOD(device_detach,	lbggpiocm_detach),

	DEVMETHOD_END
};

static driver_t lbggpiocm_driver = {
	"lbggpiocm",
	lbggpiocm_methods,
	sizeof(struct lbggpiocm_softc)
};

DRIVER_MODULE(lbggpiocm, p2sb, lbggpiocm_driver, NULL, NULL);
