/*-
 * Copyright 1998 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*-
 * Copyright 2001 by Thomas Moestl <tmm@FreeBSD.org>.  All rights reserved.
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
 *
 * 	from: FreeBSD: src/sys/i386/i386/nexus.c,v 1.43 2001/02/09
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/cons.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/rman.h>

#include <machine/intr_machdep.h>

/*
 * Device interface
 */
static int	nexus_probe(device_t);
static int	nexus_attach(device_t);
static int	nexus_activate_resource(device_t, device_t, int, int,
    struct resource *);
static int	nexus_deactivate_resource(device_t, device_t, int, int,
    struct resource *);

static int	nexus_config_intr(device_t, int, enum intr_trigger,
    enum intr_polarity);
static int	nexus_setup_intr(device_t, device_t, struct resource *, int,
    driver_filter_t *, driver_intr_t *, void *, void **);
static int	nexus_teardown_intr(device_t, device_t, struct resource *,
    void *);

static device_method_t nexus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		nexus_probe),
	DEVMETHOD(device_attach,	nexus_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface. Resource management is business of the children... */
	DEVMETHOD(bus_add_child,	bus_generic_add_child),
	DEVMETHOD(bus_probe_nomatch,	NULL),
	DEVMETHOD(bus_read_ivar,	NULL),
	DEVMETHOD(bus_write_ivar,	NULL),
	DEVMETHOD(bus_config_intr,	nexus_config_intr),
	DEVMETHOD(bus_setup_intr,	nexus_setup_intr),
	DEVMETHOD(bus_teardown_intr,	nexus_teardown_intr),
	DEVMETHOD(bus_alloc_resource,	NULL),
	DEVMETHOD(bus_activate_resource,	nexus_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	nexus_deactivate_resource),
	DEVMETHOD(bus_release_resource,	NULL),

	DEVMETHOD_END
};

static driver_t nexus_driver = {
	"nexus",
	nexus_methods
};

static devclass_t nexus_devclass;

DRIVER_MODULE(nexus, root, nexus_driver, nexus_devclass, 0, 0);

static int
nexus_probe(device_t dev)
{

	if (!bootverbose)
		device_quiet(dev);
	return (BUS_PROBE_DEFAULT);
}

static int
nexus_attach(device_t dev)
{

	bus_generic_probe(dev);
	bus_generic_attach(dev);
	return (0);
}

static int
nexus_activate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *res)
{

	/* Not much to be done yet... */
	return (rman_activate_resource(res));
}

static int
nexus_deactivate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *res)
{

	/* Not much to be done yet... */
	return (rman_deactivate_resource(res));
}

static int
nexus_config_intr(device_t bus, int irq, enum intr_trigger trig,
    enum intr_polarity pol)
{

	return (powerpc_config_intr(irq, trig, pol));
}

static int
nexus_setup_intr(device_t bus, device_t child, struct resource *res, int flags,
    driver_filter_t *ifilt, driver_intr_t *ihand, void *arg, void **cookiep)
{
	int error;

	*cookiep = NULL;

	/* somebody tried to setup an irq that failed to allocate! */
	if (res == NULL)
		return (EINVAL);

	if ((rman_get_flags(res) & RF_SHAREABLE) == 0)
		flags |= INTR_EXCL;

	/* We depend on rman_activate_resource() being idempotent. */
	error = rman_activate_resource(res);
	if (error)
		return (error);

	error = powerpc_setup_intr(device_get_nameunit(child),
	    rman_get_start(res), ifilt, ihand, arg, flags, cookiep);
	return (error);
}

static int
nexus_teardown_intr(device_t bus, device_t child, struct resource *res,
    void *cookie)
{
	int error;

	if (res == NULL)
		return (EINVAL);

	error = powerpc_teardown_intr(cookie);
	return (error);
}
