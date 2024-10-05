/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2009 Oleksandr Tymoshenko <gonzo@freebsd.org>
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
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/gpio.h>
#include <sys/ioccom.h>
#include <sys/filio.h>
#include <sys/fcntl.h>
#include <sys/sigio.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/uio.h>
#include <sys/poll.h>
#include <sys/selinfo.h>
#include <sys/module.h>

#include <dev/gpio/gpiobusvar.h>

#include "gpio_if.h"
#include "gpiobus_if.h"

#undef GPIOC_DEBUG
#ifdef GPIOC_DEBUG
#define dprintf printf
#define ddevice_printf device_printf
#else
#define dprintf(x, arg...)
#define ddevice_printf(dev, x, arg...)
#endif

struct gpioc_softc {
	device_t		sc_dev;		/* gpiocX dev */
	device_t		sc_pdev;	/* gpioX dev */
	struct cdev		*sc_ctl_dev;	/* controller device */
	int			sc_unit;
	int			sc_npins;
	struct gpioc_pin_intr	*sc_pin_intr;
};

struct gpioc_pin_intr {
	struct gpioc_softc				*sc;
	gpio_pin_t					pin;
	bool						config_locked;
	int						intr_rid;
	struct resource					*intr_res;
	void						*intr_cookie;
	struct mtx					mtx;
	SLIST_HEAD(gpioc_privs_list, gpioc_privs)	privs;
};


struct gpioc_cdevpriv {
	struct gpioc_softc			*sc;
	struct selinfo				selinfo;
	bool					async;
	uint8_t					report_option;
	struct sigio				*sigio;
	struct mtx				mtx;
	struct gpioc_pin_event			*events;
	int					numevents;
	int					evidx_head;
	int					evidx_tail;
	SLIST_HEAD(gpioc_pins_list, gpioc_pins)	pins;
};

struct gpioc_privs {
	struct gpioc_cdevpriv		*priv;
	SLIST_ENTRY(gpioc_privs)	next;
};

struct gpioc_pins {
	struct gpioc_pin_intr	*pin;
	int			eventcount;
	int			firstevent;
	SLIST_ENTRY(gpioc_pins)	next;
};

struct gpioc_pin_event {
	struct gpioc_pins	*privpin;
	sbintime_t		event_time;
	bool			event_pin_state;
};

static MALLOC_DEFINE(M_GPIOC, "gpioc", "gpioc device data");

static int	gpioc_allocate_pin_intr(struct gpioc_pin_intr*, uint32_t);
static int	gpioc_release_pin_intr(struct gpioc_pin_intr*);
static int	gpioc_attach_priv_pin(struct gpioc_cdevpriv*,
		    struct gpioc_pin_intr*);
static int	gpioc_detach_priv_pin(struct gpioc_cdevpriv*,
		    struct gpioc_pin_intr*);
static bool	gpioc_intr_reconfig_allowed(struct gpioc_cdevpriv*,
		    struct gpioc_pin_intr *intr_conf);
static uint32_t	gpioc_get_intr_config(struct gpioc_softc*,
		    struct gpioc_cdevpriv*, uint32_t pin);
static int	gpioc_set_intr_config(struct gpioc_softc*,
		    struct gpioc_cdevpriv*, uint32_t, uint32_t);
static void	gpioc_interrupt_handler(void*);

static int	gpioc_kqread(struct knote*, long);
static void	gpioc_kqdetach(struct knote*);

static int	gpioc_probe(device_t dev);
static int	gpioc_attach(device_t dev);
static int	gpioc_detach(device_t dev);

static void	gpioc_cdevpriv_dtor(void*);

static d_open_t		gpioc_open;
static d_read_t		gpioc_read;
static d_ioctl_t	gpioc_ioctl;
static d_poll_t		gpioc_poll;
static d_kqfilter_t	gpioc_kqfilter;

static struct cdevsw gpioc_cdevsw = {
	.d_version	= D_VERSION,
	.d_open		= gpioc_open,
	.d_read		= gpioc_read,
	.d_ioctl	= gpioc_ioctl,
	.d_poll		= gpioc_poll,
	.d_kqfilter	= gpioc_kqfilter,
	.d_name		= "gpioc",
};

static struct filterops gpioc_read_filterops = {
	.f_isfd =	true,
	.f_attach =	NULL,
	.f_detach =	gpioc_kqdetach,
	.f_event =	gpioc_kqread,
	.f_touch =	NULL
};

static struct gpioc_pin_event *
next_head_event(struct gpioc_cdevpriv *priv)
{
	struct gpioc_pin_event *rv;

	rv = &priv->events[priv->evidx_head++];
	if (priv->evidx_head == priv->numevents)
		priv->evidx_head = 0;
	return (rv);
}

static struct gpioc_pin_event *
next_tail_event(struct gpioc_cdevpriv *priv)
{
	struct gpioc_pin_event *rv;

	rv = &priv->events[priv->evidx_tail++];
	if (priv->evidx_tail == priv->numevents)
		priv->evidx_tail = 0;
	return (rv);
}

static size_t
number_of_events(struct gpioc_cdevpriv *priv)
{
	if (priv->evidx_head >= priv->evidx_tail)
		return (priv->evidx_head - priv->evidx_tail);
	else
		return (priv->numevents + priv->evidx_head - priv->evidx_tail);
}

static int
gpioc_allocate_pin_intr(struct gpioc_pin_intr *intr_conf, uint32_t flags)
{
	int err;

	intr_conf->config_locked = true;
	mtx_unlock(&intr_conf->mtx);

	intr_conf->intr_res = gpio_alloc_intr_resource(intr_conf->pin->dev,
	    &intr_conf->intr_rid, RF_ACTIVE, intr_conf->pin, flags);
	if (intr_conf->intr_res == NULL) {
		err = ENXIO;
		goto error_exit;
	}

	err = bus_setup_intr(intr_conf->pin->dev, intr_conf->intr_res,
	    INTR_TYPE_MISC | INTR_MPSAFE, NULL, gpioc_interrupt_handler,
	    intr_conf, &intr_conf->intr_cookie);
	if (err != 0)
		goto error_exit;

	intr_conf->pin->flags = flags;

error_exit:
	mtx_lock(&intr_conf->mtx);
	intr_conf->config_locked = false;
	wakeup(&intr_conf->config_locked);

	return (err);
}

static int
gpioc_release_pin_intr(struct gpioc_pin_intr *intr_conf)
{
	int err;

	intr_conf->config_locked = true;
	mtx_unlock(&intr_conf->mtx);

	if (intr_conf->intr_cookie != NULL) {
		err = bus_teardown_intr(intr_conf->pin->dev,
		    intr_conf->intr_res, intr_conf->intr_cookie);
		if (err != 0)
			goto error_exit;
		else
			intr_conf->intr_cookie = NULL;
	}

	if (intr_conf->intr_res != NULL) {
		err = bus_release_resource(intr_conf->pin->dev, SYS_RES_IRQ,
		    intr_conf->intr_rid, intr_conf->intr_res);
		if (err != 0)
			goto error_exit;
		else {
			intr_conf->intr_rid = 0;
			intr_conf->intr_res = NULL;
		}
	}

	intr_conf->pin->flags = 0;
	err = 0;

error_exit:
	mtx_lock(&intr_conf->mtx);
	intr_conf->config_locked = false;
	wakeup(&intr_conf->config_locked);

	return (err);
}

static int
gpioc_attach_priv_pin(struct gpioc_cdevpriv *priv,
    struct gpioc_pin_intr *intr_conf)
{
	struct gpioc_privs	*priv_link;
	struct gpioc_pins	*pin_link;
	unsigned int		consistency_a __diagused;
	unsigned int		consistency_b __diagused;

	consistency_a = 0;
	consistency_b = 0;
	mtx_assert(&intr_conf->mtx, MA_OWNED);
	mtx_lock(&priv->mtx);
	SLIST_FOREACH(priv_link, &intr_conf->privs, next) {
		if (priv_link->priv == priv)
			consistency_a++;
	}
	KASSERT(consistency_a <= 1,
	    ("inconsistent links between pin config and cdevpriv"));
	SLIST_FOREACH(pin_link, &priv->pins, next) {
		if (pin_link->pin == intr_conf)
			consistency_b++;
	}
	KASSERT(consistency_a == consistency_b,
	    ("inconsistent links between pin config and cdevpriv"));
	if (consistency_a == 1 && consistency_b == 1) {
		mtx_unlock(&priv->mtx);
		return (EEXIST);
	}
	priv_link = malloc(sizeof(struct gpioc_privs), M_GPIOC,
	    M_NOWAIT | M_ZERO);
	if (priv_link == NULL)
	{
		mtx_unlock(&priv->mtx);
		return (ENOMEM);
	}
	pin_link = malloc(sizeof(struct gpioc_pins), M_GPIOC,
	    M_NOWAIT | M_ZERO);
	if (pin_link == NULL) {
		mtx_unlock(&priv->mtx);
		return (ENOMEM);
	}
	priv_link->priv = priv;
	pin_link->pin = intr_conf;
	SLIST_INSERT_HEAD(&intr_conf->privs, priv_link, next);
	SLIST_INSERT_HEAD(&priv->pins, pin_link, next);
	mtx_unlock(&priv->mtx);

	return (0);
}

static int
gpioc_detach_priv_pin(struct gpioc_cdevpriv *priv,
    struct gpioc_pin_intr *intr_conf)
{
	struct gpioc_privs	*priv_link, *priv_link_temp;
	struct gpioc_pins	*pin_link, *pin_link_temp;
	unsigned int		consistency_a __diagused;
	unsigned int		consistency_b __diagused;

	consistency_a = 0;
	consistency_b = 0;
	mtx_assert(&intr_conf->mtx, MA_OWNED);
	mtx_lock(&priv->mtx);
	SLIST_FOREACH_SAFE(priv_link, &intr_conf->privs, next, priv_link_temp) {
		if (priv_link->priv == priv) {
			SLIST_REMOVE(&intr_conf->privs, priv_link, gpioc_privs,
			    next);
			free(priv_link, M_GPIOC);
			consistency_a++;
		}
	}
	KASSERT(consistency_a <= 1,
	    ("inconsistent links between pin config and cdevpriv"));
	SLIST_FOREACH_SAFE(pin_link, &priv->pins, next, pin_link_temp) {
		if (pin_link->pin == intr_conf) {
			/*
			 * If the pin we're removing has events in the priv's
			 * event fifo, we can't leave dangling pointers from
			 * those events to the gpioc_pins struct we're about to
			 * free.  We also can't remove random items and leave
			 * holes in the events fifo, so just empty it out.
			 */
			if (pin_link->eventcount > 0) {
				priv->evidx_head = priv->evidx_tail = 0;
			}
			SLIST_REMOVE(&priv->pins, pin_link, gpioc_pins, next);
			free(pin_link, M_GPIOC);
			consistency_b++;
		}
	}
	KASSERT(consistency_a == consistency_b,
	    ("inconsistent links between pin config and cdevpriv"));
	mtx_unlock(&priv->mtx);

	return (0);
}

static bool
gpioc_intr_reconfig_allowed(struct gpioc_cdevpriv *priv,
    struct gpioc_pin_intr *intr_conf)
{
	struct gpioc_privs	*priv_link;

	mtx_assert(&intr_conf->mtx, MA_OWNED);

	if (SLIST_EMPTY(&intr_conf->privs))
		return (true);

	SLIST_FOREACH(priv_link, &intr_conf->privs, next) {
		if (priv_link->priv != priv)
			return (false);
	}

	return (true);
}


static uint32_t
gpioc_get_intr_config(struct gpioc_softc *sc, struct gpioc_cdevpriv *priv,
    uint32_t pin)
{
	struct gpioc_pin_intr	*intr_conf = &sc->sc_pin_intr[pin];
	struct gpioc_privs	*priv_link;
	uint32_t		flags;

	flags = intr_conf->pin->flags;

	if (flags == 0)
		return (0);

	mtx_lock(&intr_conf->mtx);
	SLIST_FOREACH(priv_link, &intr_conf->privs, next) {
		if (priv_link->priv == priv) {
			flags |= GPIO_INTR_ATTACHED;
			break;
		}
	}
	mtx_unlock(&intr_conf->mtx);

	return (flags);
}

static int
gpioc_set_intr_config(struct gpioc_softc *sc, struct gpioc_cdevpriv *priv,
    uint32_t pin, uint32_t flags)
{
	struct gpioc_pin_intr *intr_conf = &sc->sc_pin_intr[pin];
	int res;

	res = 0;
	if (intr_conf->pin->flags == 0 && flags == 0) {
		/* No interrupt configured and none requested: Do nothing. */
		return (0);
	}
	mtx_lock(&intr_conf->mtx);
	while (intr_conf->config_locked == true)
		mtx_sleep(&intr_conf->config_locked, &intr_conf->mtx, 0,
		    "gpicfg", 0);
	if (intr_conf->pin->flags == 0 && flags != 0) {
		/*
		 * No interrupt is configured, but one is requested: Allocate
		 * and setup interrupt on the according pin.
		 */
		res = gpioc_allocate_pin_intr(intr_conf, flags);
		if (res == 0)
			res = gpioc_attach_priv_pin(priv, intr_conf);
		if (res == EEXIST)
			res = 0;
	} else if (intr_conf->pin->flags == flags) {
		/*
		 * Same interrupt requested as already configured: Attach the
		 * cdevpriv to the corresponding pin.
		 */
		res = gpioc_attach_priv_pin(priv, intr_conf);
		if (res == EEXIST)
			res = 0;
	} else if (intr_conf->pin->flags != 0 && flags == 0) {
		/*
		 * Interrupt configured, but none requested: Teardown and
		 * release the pin when no other cdevpriv is attached. Otherwise
		 * just detach pin and cdevpriv from each other.
		 */
		if (gpioc_intr_reconfig_allowed(priv, intr_conf)) {
			res = gpioc_release_pin_intr(intr_conf);
		}
		if (res == 0)
			res = gpioc_detach_priv_pin(priv, intr_conf);
	} else {
		/*
		 * Other flag requested than configured: Reconfigure when no
		 * other cdevpriv is are attached to the pin.
		 */
		if (!gpioc_intr_reconfig_allowed(priv, intr_conf))
			res = EBUSY;
		else {
			res = gpioc_release_pin_intr(intr_conf);
			if (res == 0)
				res = gpioc_allocate_pin_intr(intr_conf, flags);
			if (res == 0)
				res = gpioc_attach_priv_pin(priv, intr_conf);
			if (res == EEXIST)
				res = 0;
		}
	}
	mtx_unlock(&intr_conf->mtx);

	return (res);
}

static void
gpioc_interrupt_handler(void *arg)
{
	struct gpioc_pin_intr *intr_conf;
	struct gpioc_privs *privs;
	struct gpioc_softc *sc;
	sbintime_t evtime;
	uint32_t pin_state;

	intr_conf = arg;
	sc = intr_conf->sc;

	/* Capture time and pin state first. */
	evtime = sbinuptime();
	if (intr_conf->pin->flags & GPIO_INTR_EDGE_BOTH)
		GPIO_PIN_GET(sc->sc_pdev, intr_conf->pin->pin, &pin_state);
	else if (intr_conf->pin->flags & GPIO_INTR_EDGE_RISING)
		pin_state = true;
	else
		pin_state = false;

	mtx_lock(&intr_conf->mtx);

	if (intr_conf->config_locked == true) {
		ddevice_printf(sc->sc_dev, "Interrupt configuration in "
		    "progress. Discarding interrupt on pin %d.\n",
		    intr_conf->pin->pin);
		mtx_unlock(&intr_conf->mtx);
		return;
	}

	if (SLIST_EMPTY(&intr_conf->privs)) {
		ddevice_printf(sc->sc_dev, "No file descriptor associated with "
		    "occurred interrupt on pin %d.\n", intr_conf->pin->pin);
		mtx_unlock(&intr_conf->mtx);
		return;
	}

	SLIST_FOREACH(privs, &intr_conf->privs, next) {
		struct gpioc_cdevpriv *priv = privs->priv;
		struct gpioc_pins *privpin;
		struct gpioc_pin_event *event;
		mtx_lock(&priv->mtx);
		SLIST_FOREACH(privpin, &priv->pins, next) {
			if (privpin->pin == intr_conf)
				break;
		}
		if (privpin == NULL) {
			/* Should be impossible. */
			ddevice_printf(sc->sc_dev, "Cannot find privpin\n");
			mtx_unlock(&priv->mtx);
			continue;
		}

		if (priv->report_option == GPIO_EVENT_REPORT_DETAIL) {
			event = next_head_event(priv);
			/* If head is overtaking tail, advance tail. */
			if (priv->evidx_head == priv->evidx_tail)
				next_tail_event(priv);
		} else {
			if (privpin->eventcount > 0)
				event = &priv->events[privpin->firstevent + 1];
			else {
				privpin->firstevent = priv->evidx_head;
				event = next_head_event(priv);
				event->privpin = privpin;
				event->event_time = evtime;
				event->event_pin_state = pin_state;
				event = next_head_event(priv);
			}
			++privpin->eventcount;
		}
		event->privpin = privpin;
		event->event_time = evtime;
		event->event_pin_state = pin_state;
		wakeup(priv);
		selwakeup(&priv->selinfo);
		KNOTE_LOCKED(&priv->selinfo.si_note, 0);
		if (priv->async == true && priv->sigio != NULL)
			pgsigio(&priv->sigio, SIGIO, 0);
		mtx_unlock(&priv->mtx);
	}

	mtx_unlock(&intr_conf->mtx);
}

static int
gpioc_probe(device_t dev)
{
	device_set_desc(dev, "GPIO controller");
	return (0);
}

static int
gpioc_attach(device_t dev)
{
	int err;
	struct gpioc_softc *sc;
	struct make_dev_args devargs;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	sc->sc_pdev = device_get_parent(dev);
	sc->sc_unit = device_get_unit(dev);

	err = GPIO_PIN_MAX(sc->sc_pdev, &sc->sc_npins);
	sc->sc_npins++; /* Number of pins is one more than max pin number. */
	if (err != 0)
		return (err);
	sc->sc_pin_intr = malloc(sizeof(struct gpioc_pin_intr) * sc->sc_npins,
	    M_GPIOC, M_WAITOK | M_ZERO);
	for (int i = 0; i < sc->sc_npins; i++) {
		sc->sc_pin_intr[i].pin = malloc(sizeof(struct gpiobus_pin),
		    M_GPIOC, M_WAITOK | M_ZERO);
		sc->sc_pin_intr[i].sc = sc;
		sc->sc_pin_intr[i].pin->pin = i;
		sc->sc_pin_intr[i].pin->dev = sc->sc_pdev;
		mtx_init(&sc->sc_pin_intr[i].mtx, "gpioc pin", NULL, MTX_DEF);
		SLIST_INIT(&sc->sc_pin_intr[i].privs);
	}

	make_dev_args_init(&devargs);
	devargs.mda_devsw = &gpioc_cdevsw;
	devargs.mda_uid = UID_ROOT;
	devargs.mda_gid = GID_WHEEL;
	devargs.mda_mode = 0600;
	devargs.mda_si_drv1 = sc;
	err = make_dev_s(&devargs, &sc->sc_ctl_dev, "gpioc%d", sc->sc_unit);
	if (err != 0) {
		device_printf(dev, "Failed to create gpioc%d", sc->sc_unit);
		return (ENXIO);
	}

	return (0);
}

static int
gpioc_detach(device_t dev)
{
	struct gpioc_softc *sc = device_get_softc(dev);
	int err;

	if (sc->sc_ctl_dev)
		destroy_dev(sc->sc_ctl_dev);

	for (int i = 0; i < sc->sc_npins; i++) {
		mtx_destroy(&sc->sc_pin_intr[i].mtx);
		free(sc->sc_pin_intr[i].pin, M_GPIOC);
	}
	free(sc->sc_pin_intr, M_GPIOC);

	if ((err = bus_generic_detach(dev)) != 0)
		return (err);

	return (0);
}

static void
gpioc_cdevpriv_dtor(void *data)
{
	struct gpioc_cdevpriv	*priv;
	struct gpioc_privs	*priv_link, *priv_link_temp;
	struct gpioc_pins	*pin_link, *pin_link_temp;
	unsigned int		consistency __diagused;

	priv = data;

	SLIST_FOREACH_SAFE(pin_link, &priv->pins, next, pin_link_temp) {
		consistency = 0;
		mtx_lock(&pin_link->pin->mtx);
		while (pin_link->pin->config_locked == true)
			mtx_sleep(&pin_link->pin->config_locked,
			    &pin_link->pin->mtx, 0, "gpicfg", 0);
		SLIST_FOREACH_SAFE(priv_link, &pin_link->pin->privs, next,
		    priv_link_temp) {
			if (priv_link->priv == priv) {
				SLIST_REMOVE(&pin_link->pin->privs, priv_link,
				    gpioc_privs, next);
				free(priv_link, M_GPIOC);
				consistency++;
			}
		}
		KASSERT(consistency == 1,
		    ("inconsistent links between pin config and cdevpriv"));
		if (gpioc_intr_reconfig_allowed(priv, pin_link->pin)) {
			gpioc_release_pin_intr(pin_link->pin);
		}
		mtx_unlock(&pin_link->pin->mtx);
		SLIST_REMOVE(&priv->pins, pin_link, gpioc_pins, next);
		free(pin_link, M_GPIOC);
	}

	wakeup(&priv);
	knlist_clear(&priv->selinfo.si_note, 0);
	seldrain(&priv->selinfo);
	knlist_destroy(&priv->selinfo.si_note);
	funsetown(&priv->sigio);

	mtx_destroy(&priv->mtx);
	free(priv->events, M_GPIOC);
	free(data, M_GPIOC);
}

static int
gpioc_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	struct gpioc_cdevpriv *priv;
	int err = 0;

	priv = malloc(sizeof(*priv), M_GPIOC, M_WAITOK | M_ZERO);
	priv->sc = dev->si_drv1;

	mtx_init(&priv->mtx, "gpioc priv", NULL, MTX_DEF);
	knlist_init_mtx(&priv->selinfo.si_note, &priv->mtx);

	priv->async = false;
	priv->report_option = GPIO_EVENT_REPORT_DETAIL;
	priv->sigio = NULL;

	/*
	 * Allocate a circular buffer for events.  The scheme we use for summary
	 * reporting assumes there will always be a pair of events available to
	 * record the first/last events on any pin, so we allocate 2 * npins.
	 * Even though we actually default to detailed event reporting, 2 *
	 * npins isn't a horrible fifo size for that either.
	 */
	priv->numevents = priv->sc->sc_npins * 2;
	priv->events = malloc(priv->numevents * sizeof(struct gpio_event_detail),
	    M_GPIOC, M_WAITOK | M_ZERO);

	priv->evidx_head = priv->evidx_tail = 0;
	SLIST_INIT(&priv->pins);

	err = devfs_set_cdevpriv(priv, gpioc_cdevpriv_dtor);
	if (err != 0)
		gpioc_cdevpriv_dtor(priv);
	return (err);
}

static int
gpioc_read(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct gpioc_cdevpriv *priv;
	struct gpioc_pin_event *event;
	union {
		struct gpio_event_summary sum;
		struct gpio_event_detail  evt;
		uint8_t 		  data[1];
	} recbuf;
	size_t recsize;
	int err;

	if ((err = devfs_get_cdevpriv((void **)&priv)) != 0)
		return (err);

	if (priv->report_option == GPIO_EVENT_REPORT_SUMMARY)
		recsize = sizeof(struct gpio_event_summary);
	else
		recsize = sizeof(struct gpio_event_detail);

	if (uio->uio_resid < recsize)
		return (EINVAL);

	mtx_lock(&priv->mtx);
	while (priv->evidx_head == priv->evidx_tail) {
		if (SLIST_EMPTY(&priv->pins)) {
			err = ENXIO;
			break;
		} else if (ioflag & O_NONBLOCK) {
			err = EWOULDBLOCK;
			break;
		} else {
			err = mtx_sleep(priv, &priv->mtx, PCATCH, "gpintr", 0);
			if (err != 0)
				break;
		}
	}

	while (err == 0 && uio->uio_resid >= recsize &&
           priv->evidx_tail != priv->evidx_head) {
		event = next_tail_event(priv);
		if (priv->report_option == GPIO_EVENT_REPORT_SUMMARY) {
			recbuf.sum.gp_first_time = event->event_time;
			recbuf.sum.gp_pin = event->privpin->pin->pin->pin;
			recbuf.sum.gp_count = event->privpin->eventcount;
			recbuf.sum.gp_first_state = event->event_pin_state;
			event = next_tail_event(priv);
			recbuf.sum.gp_last_time = event->event_time;
			recbuf.sum.gp_last_state = event->event_pin_state;
			event->privpin->eventcount = 0;
			event->privpin->firstevent = 0;
		} else {
			recbuf.evt.gp_time = event->event_time;
			recbuf.evt.gp_pin = event->privpin->pin->pin->pin;
			recbuf.evt.gp_pinstate = event->event_pin_state;
		}
		mtx_unlock(&priv->mtx);
		err = uiomove(recbuf.data, recsize, uio);
		mtx_lock(&priv->mtx);
	}
	mtx_unlock(&priv->mtx);
	return (err);
}

static int 
gpioc_ioctl(struct cdev *cdev, u_long cmd, caddr_t arg, int fflag, 
    struct thread *td)
{
	device_t bus;
	int max_pin, res;
	struct gpioc_softc *sc = cdev->si_drv1;
	struct gpioc_cdevpriv *priv;
	struct gpio_pin pin;
	struct gpio_req req;
	struct gpio_access_32 *a32;
	struct gpio_config_32 *c32;
	struct gpio_event_config *evcfg;
	uint32_t caps, intrflags;

	bus = GPIO_GET_BUS(sc->sc_pdev);
	if (bus == NULL)
		return (EINVAL);
	switch (cmd) {
	case GPIOMAXPIN:
		max_pin = -1;
		res = GPIO_PIN_MAX(sc->sc_pdev, &max_pin);
		bcopy(&max_pin, arg, sizeof(max_pin));
		break;
	case GPIOGETCONFIG:
		bcopy(arg, &pin, sizeof(pin));
		dprintf("get config pin %d\n", pin.gp_pin);
		res = GPIO_PIN_GETFLAGS(sc->sc_pdev, pin.gp_pin,
		    &pin.gp_flags);
		/* Fail early */
		if (res)
			break;
		res = devfs_get_cdevpriv((void **)&priv);
		if (res)
			break;
		pin.gp_flags |= gpioc_get_intr_config(sc, priv,
		    pin.gp_pin);
		GPIO_PIN_GETCAPS(sc->sc_pdev, pin.gp_pin, &pin.gp_caps);
		GPIOBUS_PIN_GETNAME(bus, pin.gp_pin, pin.gp_name);
		bcopy(&pin, arg, sizeof(pin));
		break;
	case GPIOSETCONFIG:
		bcopy(arg, &pin, sizeof(pin));
		dprintf("set config pin %d\n", pin.gp_pin);
		res = devfs_get_cdevpriv((void **)&priv);
		if (res != 0)
			break;
		res = GPIO_PIN_GETCAPS(sc->sc_pdev, pin.gp_pin, &caps);
		if (res != 0)
			break;
		res = gpio_check_flags(caps, pin.gp_flags);
		if (res != 0)
			break;
		intrflags = pin.gp_flags & GPIO_INTR_MASK;
		/*
		 * We can do only edge interrupts, and only if the
		 * hardware supports that interrupt type on that pin.
		 */
		switch (intrflags) {
		case GPIO_INTR_NONE:
			break;
		case GPIO_INTR_EDGE_RISING:
		case GPIO_INTR_EDGE_FALLING:
		case GPIO_INTR_EDGE_BOTH:
			if ((intrflags & caps) == 0)
				res = EOPNOTSUPP;
			break;
		default:
			res = EINVAL;
			break;
		}
		if (res != 0)
			break;
		res = GPIO_PIN_SETFLAGS(sc->sc_pdev, pin.gp_pin,
		    (pin.gp_flags & ~GPIO_INTR_MASK));
		if (res != 0)
			break;
		res = gpioc_set_intr_config(sc, priv, pin.gp_pin,
		    intrflags);
		break;
	case GPIOGET:
		bcopy(arg, &req, sizeof(req));
		res = GPIO_PIN_GET(sc->sc_pdev, req.gp_pin,
		    &req.gp_value);
		dprintf("read pin %d -> %d\n", 
		    req.gp_pin, req.gp_value);
		bcopy(&req, arg, sizeof(req));
		break;
	case GPIOSET:
		bcopy(arg, &req, sizeof(req));
		res = GPIO_PIN_SET(sc->sc_pdev, req.gp_pin, 
		    req.gp_value);
		dprintf("write pin %d -> %d\n", 
		    req.gp_pin, req.gp_value);
		break;
	case GPIOTOGGLE:
		bcopy(arg, &req, sizeof(req));
		dprintf("toggle pin %d\n", 
		    req.gp_pin);
		res = GPIO_PIN_TOGGLE(sc->sc_pdev, req.gp_pin);
		break;
	case GPIOSETNAME:
		bcopy(arg, &pin, sizeof(pin));
		dprintf("set name on pin %d\n", pin.gp_pin);
		res = GPIOBUS_PIN_SETNAME(bus, pin.gp_pin,
		    pin.gp_name);
		break;
	case GPIOACCESS32:
		a32 = (struct gpio_access_32 *)arg;
		res = GPIO_PIN_ACCESS_32(sc->sc_pdev, a32->first_pin,
		    a32->clear_pins, a32->change_pins, &a32->orig_pins);
		break;
	case GPIOCONFIG32:
		c32 = (struct gpio_config_32 *)arg;
		res = GPIO_PIN_CONFIG_32(sc->sc_pdev, c32->first_pin,
		    c32->num_pins, c32->pin_flags);
		break;
	case GPIOCONFIGEVENTS:
		evcfg = (struct gpio_event_config *)arg;
		res = devfs_get_cdevpriv((void **)&priv);
		if (res != 0)
			break;
		/* If any pins have been configured, changes aren't allowed. */
		if (!SLIST_EMPTY(&priv->pins)) {
			res = EINVAL;
			break;
		}
		if (evcfg->gp_report_type != GPIO_EVENT_REPORT_DETAIL &&
		    evcfg->gp_report_type != GPIO_EVENT_REPORT_SUMMARY) {
			res = EINVAL;
			break;
		}
		priv->report_option = evcfg->gp_report_type;
		/* Reallocate the events buffer if the user wants it bigger. */
		if (priv->report_option == GPIO_EVENT_REPORT_DETAIL &&
		    priv->numevents < evcfg->gp_fifo_size) {
			free(priv->events, M_GPIOC);
			priv->numevents = evcfg->gp_fifo_size;
			priv->events = malloc(priv->numevents *
			    sizeof(struct gpio_event_detail), M_GPIOC,
			    M_WAITOK | M_ZERO);
			priv->evidx_head = priv->evidx_tail = 0;
		}
		break;
	case FIONBIO:
		/*
		 * This dummy handler is necessary to prevent fcntl()
		 * from failing. The actual handling of non-blocking IO
		 * is done using the O_NONBLOCK ioflag passed to the
		 * read() syscall.
		 */
		res = 0;
		break;
	case FIOASYNC:
		res = devfs_get_cdevpriv((void **)&priv);
		if (res == 0) {
			if (*(int *)arg == FASYNC)
				priv->async = true;
			else
				priv->async = false;
		}
		break;
	case FIOGETOWN:
		res = devfs_get_cdevpriv((void **)&priv);
		if (res == 0)
			*(int *)arg = fgetown(&priv->sigio);
		break;
	case FIOSETOWN:
		res = devfs_get_cdevpriv((void **)&priv);
		if (res == 0)
			res = fsetown(*(int *)arg, &priv->sigio);
		break;
	default:
		return (ENOTTY);
		break;
	}

	return (res);
}

static int
gpioc_poll(struct cdev *dev, int events, struct thread *td)
{
	struct gpioc_cdevpriv *priv;
	int err;
	int revents;

	revents = 0;

	err = devfs_get_cdevpriv((void **)&priv);
	if (err != 0) {
		revents = POLLERR;
		return (revents);
	}

	if (SLIST_EMPTY(&priv->pins)) {
		revents = POLLHUP;
		return (revents);
	}

	if (events & (POLLIN | POLLRDNORM)) {
		if (priv->evidx_head != priv->evidx_tail)
			revents |= events & (POLLIN | POLLRDNORM);
		else
			selrecord(td, &priv->selinfo);
	}

	return (revents);
}

static int
gpioc_kqfilter(struct cdev *dev, struct knote *kn)
{
	struct gpioc_cdevpriv *priv;
	struct knlist *knlist;
	int err;

	err = devfs_get_cdevpriv((void **)&priv);
	if (err != 0)
		return err;

	if (SLIST_EMPTY(&priv->pins))
		return (ENXIO);

	switch(kn->kn_filter) {
	case EVFILT_READ:
		kn->kn_fop = &gpioc_read_filterops;
		kn->kn_hook = (void *)priv;
		break;
	default:
		return (EOPNOTSUPP);
	}

	knlist = &priv->selinfo.si_note;
	knlist_add(knlist, kn, 0);

	return (0);
}

static int
gpioc_kqread(struct knote *kn, long hint)
{
	struct gpioc_cdevpriv *priv = kn->kn_hook;
	size_t recsize;


	if (SLIST_EMPTY(&priv->pins)) {
		kn->kn_flags |= EV_EOF;
		return (1);
	} else {
		if (priv->evidx_head != priv->evidx_tail) {
			if (priv->report_option == GPIO_EVENT_REPORT_SUMMARY)
				recsize = sizeof(struct gpio_event_summary);
			else
				recsize = sizeof(struct gpio_event_detail);
			kn->kn_data = recsize * number_of_events(priv);
			return (1);
		}
	}
	return (0);
}

static void
gpioc_kqdetach(struct knote *kn)
{
	struct gpioc_cdevpriv *priv = kn->kn_hook;
	struct knlist *knlist = &priv->selinfo.si_note;

	knlist_remove(knlist, kn, 0);
}

static device_method_t gpioc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		gpioc_probe),
	DEVMETHOD(device_attach,	gpioc_attach),
	DEVMETHOD(device_detach,	gpioc_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	DEVMETHOD_END
};

driver_t gpioc_driver = {
	"gpioc",
	gpioc_methods,
	sizeof(struct gpioc_softc)
};

DRIVER_MODULE(gpioc, gpio, gpioc_driver, 0, 0);
MODULE_VERSION(gpioc, 1);
