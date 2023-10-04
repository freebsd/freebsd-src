/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Ruslan Bukin <br@bsdpad.com>
 * Copyright (c) 2023 Arm Ltd
 *
 * This work was supported by Innovate UK project 105694, "Digital Security
 * by Design (DSbD) Technology Platform Prototype".
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

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>

#include <dev/clk/clk.h>
#include <dev/fdt/simplebus.h>
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "dev/mailbox/arm/arm_doorbell.h"

#include "scmi.h"
#include "scmi_protocols.h"

struct scmi_mailbox_softc {
	struct scmi_softc	base;
	struct arm_doorbell	*db;
	int			req_done;
};

static void
scmi_mailbox_callback(void *arg)
{
	struct scmi_mailbox_softc *sc;

	sc = arg;

	dprintf("%s sc %p\n", __func__, sc);

	SCMI_LOCK(&sc->base);
	sc->req_done = 1;
	wakeup(sc);
	SCMI_UNLOCK(&sc->base);
}

static int
scmi_mailbox_xfer_msg(device_t dev)
{
	struct scmi_mailbox_softc *sc;
	int timeout;

	sc = device_get_softc(dev);
	SCMI_ASSERT_LOCKED(&sc->base);

	sc->req_done = 0;

	/* Interrupt SCP firmware. */
	arm_doorbell_set(sc->db);

	timeout = 200;

	dprintf("%s: request\n", __func__);

	do {
		if (cold) {
			if (arm_doorbell_get(sc->db))
				break;
			DELAY(10000);
		} else {
			msleep(sc, &sc->base.mtx, 0, "scmi", hz / 10);
			if (sc->req_done)
				break;
		}
	} while (timeout--);

	if (timeout <= 0)
		return (-1);

	dprintf("%s: got reply, timeout %d\n", __func__, timeout);

	return (0);
}

static int
scmi_mailbox_probe(device_t dev)
{

	if (!ofw_bus_is_compatible(dev, "arm,scmi"))
		return (ENXIO);

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	device_set_desc(dev, "ARM SCMI interface driver");

	return (BUS_PROBE_DEFAULT);
}

static int
scmi_mailbox_attach(device_t dev)
{
	struct scmi_mailbox_softc *sc;
	int ret;

	sc = device_get_softc(dev);

	/* TODO: Support other mailbox devices */
	sc->db = arm_doorbell_ofw_get(dev, "tx");
	if (sc->db == NULL) {
		device_printf(dev, "Doorbell device not found.\n");
		return (ENXIO);
	}

	arm_doorbell_set_handler(sc->db, scmi_mailbox_callback, sc);

	ret = scmi_attach(dev);
	if (ret != 0)
		arm_doorbell_set_handler(sc->db, NULL, NULL);

	return (ret);
}

static int
scmi_mailbox_detach(device_t dev)
{
	struct scmi_mailbox_softc *sc;

	sc = device_get_softc(dev);

	arm_doorbell_set_handler(sc->db, NULL, NULL);

	return (0);
}

static device_method_t scmi_mailbox_methods[] = {
	DEVMETHOD(device_probe,		scmi_mailbox_probe),
	DEVMETHOD(device_attach,	scmi_mailbox_attach),
	DEVMETHOD(device_detach,	scmi_mailbox_detach),

	/* SCMI interface */
	DEVMETHOD(scmi_xfer_msg,	scmi_mailbox_xfer_msg),

	DEVMETHOD_END
};

DEFINE_CLASS_1(scmi_mailbox, scmi_mailbox_driver, scmi_mailbox_methods,
    sizeof(struct scmi_mailbox_softc), scmi_driver);

DRIVER_MODULE(scmi_mailbox, simplebus, scmi_mailbox_driver, 0, 0);
MODULE_VERSION(scmi_mailbox, 1);
