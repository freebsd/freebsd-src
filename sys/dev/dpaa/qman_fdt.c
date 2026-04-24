/*-
 * Copyright (c) 2011-2012 Semihalf.
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

#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/smp.h>

#include <machine/bus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/ofw_subr.h>

#include "qman.h"
#include "portals.h"
#include "qman_var.h"
#include "qman_portal_if.h"

#define	FQMAN_DEVSTR	"Freescale Queue Manager"

static int qman_fdt_probe(device_t);

static device_method_t qman_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		qman_fdt_probe),
	DEVMETHOD(device_attach,	qman_attach),
	DEVMETHOD(device_detach,	qman_detach),

	DEVMETHOD(device_suspend,	qman_suspend),
	DEVMETHOD(device_resume,	qman_resume),
	DEVMETHOD(device_shutdown,	qman_shutdown),

	DEVMETHOD_END
};

DEFINE_CLASS_0(qman, qman_driver, qman_methods, sizeof(struct qman_softc));
EARLY_DRIVER_MODULE(qman, simplebus, qman_driver, 0, 0, BUS_PASS_SUPPORTDEV);

static int
qman_fdt_probe(device_t dev)
{

	if (!ofw_bus_is_compatible(dev, "fsl,qman"))
		return (ENXIO);

	device_set_desc(dev, FQMAN_DEVSTR);

	return (BUS_PROBE_DEFAULT);
}

/*
 * QMAN Portals
 */
#define	QMAN_PORT_DEVSTR	"Freescale Queue Manager - Portal"

static int portal_ncpus;
static device_probe_t qman_portal_fdt_probe;
static device_attach_t qman_portal_fdt_attach;

static device_method_t qman_portal_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		qman_portal_fdt_probe),
	DEVMETHOD(device_attach,	qman_portal_fdt_attach),
	DEVMETHOD(device_detach,	qman_portal_detach),

	DEVMETHOD(qman_portal_enqueue,	qman_portal_fq_enqueue),
	DEVMETHOD(qman_portal_mc_send_raw,	qman_portal_mc_send_raw),
	DEVMETHOD(qman_portal_static_dequeue_channel,
					qman_portal_static_dequeue_channel),
	DEVMETHOD(qman_portal_static_dequeue_rm_channel,
					qman_portal_static_dequeue_rm_channel),

	DEVMETHOD_END
};

DEFINE_CLASS_0(qman_portal, qman_portal_driver, qman_portal_methods,
    sizeof(struct qman_softc));
EARLY_DRIVER_MODULE(qman_portal, simplebus, qman_portal_driver, 0, 0,
    BUS_PASS_SUPPORTDEV + BUS_PASS_ORDER_MIDDLE);

static int
qman_portal_fdt_probe(device_t dev)
{

	if (!ofw_bus_is_compatible(dev, "fsl,qman-portal"))
		return (ENXIO);

	device_set_desc(dev, QMAN_PORT_DEVSTR);

	return (BUS_PROBE_DEFAULT);
}

static int
qman_portal_fdt_attach(device_t dev)
{
	int portal_cpu = portal_ncpus;

	/* Don't attach to more portals than we have CPUs */
	if (mp_ncpus == portal_ncpus)
		return (ENXIO);

	portal_ncpus++;

	return (qman_portal_attach(dev, portal_cpu));
}
