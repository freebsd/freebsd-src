/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021-2023 Val Packett <val@packett.cool>
 * Copyright (c) 2023 Vladimir Kondratyev <wulf@FreeBSD.org>
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

#include "opt_acpi.h"
#include "opt_hid.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/sx.h>
#include <sys/taskqueue.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acevents.h>
#include <dev/acpica/acpivar.h>
#include <dev/acpica/acpiio.h>

#include <dev/backlight/backlight.h>

#include <dev/evdev/input.h>

#define	HID_DEBUG_VAR atopcase_debug
#include <dev/hid/hid.h>
#include <dev/hid/hidquirk.h>

#include <dev/spibus/spi.h>
#include <dev/spibus/spibusvar.h>

#include "backlight_if.h"
#include "hid_if.h"

#include "atopcase_reg.h"
#include "atopcase_var.h"

/*
 * XXX: The Linux driver only supports ACPI GPEs, but we only receive
 * interrupts in this driver on a MacBookPro 12,1 and 14,1. This is because
 * Linux responds to _OSI("Darwin") while we don't!
 *
 * ACPI GPE is enabled on FreeBSD by addition of following lines to
 * /boot/loader.conf:
 * hw.acpi.install_interface="Darwin"
 * hw.acpi.remove_interface="Windows 2009, Windows 2012"
 */

static const char *atopcase_ids[] = { "APP000D", NULL };

static device_probe_t	atopcase_acpi_probe;
static device_attach_t	atopcase_acpi_attach;
static device_detach_t	atopcase_acpi_detach;
static device_suspend_t	atopcase_acpi_suspend;
static device_resume_t	atopcase_acpi_resume;

static bool
acpi_is_atopcase(ACPI_HANDLE handle)
{
	const char **ids;
	UINT32 sta;

	for (ids = atopcase_ids; *ids != NULL; ids++) {
		if (acpi_MatchHid(handle, *ids))
			break;
	}
	if (*ids == NULL)
		return (false);

	/*
	 * If no _STA method or if it failed, then assume that
	 * the device is present.
	 */
	if (ACPI_FAILURE(acpi_GetInteger(handle, "_STA", &sta)) ||
	    ACPI_DEVICE_PRESENT(sta))
		return (true);

	return (false);
}

static int
atopcase_acpi_set_comm_enabled(struct atopcase_softc *sc, char *prop,
    const bool on)
{
	ACPI_OBJECT argobj;
	ACPI_OBJECT_LIST args;

	argobj.Type = ACPI_TYPE_INTEGER;
	argobj.Integer.Value = on;
	args.Count = 1;
	args.Pointer = &argobj;

	if (ACPI_FAILURE(
	    AcpiEvaluateObject(sc->sc_handle, prop, &args, NULL)))
		return (ENXIO);

	DELAY(100);

	return (0);
}

static int
atopcase_acpi_test_comm_enabled(ACPI_HANDLE handle, char *prop, int *enabled)
{
	if (ACPI_FAILURE(acpi_GetInteger(handle, prop, enabled)))
		return (ENXIO);

	return (0);
}

static void
atopcase_acpi_task(void *ctx, int pending __unused)
{
	struct atopcase_softc *sc = ctx;
	int err = EAGAIN;

	DPRINTFN(ATOPCASE_LLEVEL_DEBUG, "Timer event\n");

	sx_xlock(&sc->sc_write_sx);
	err = atopcase_receive_packet(sc);
	sx_xunlock(&sc->sc_write_sx);

	/* Rearm timer */
	taskqueue_enqueue_timeout(sc->sc_tq, &sc->sc_task,
	    hz / (err == EAGAIN ? 10 : 120));
}

static void
atopcase_acpi_gpe_task(void *ctx)
{
	struct atopcase_softc *sc = ctx;

	DPRINTFN(ATOPCASE_LLEVEL_DEBUG, "GPE event\n");

	sx_xlock(&sc->sc_sx);
	(void)atopcase_intr(sc);
	sx_xunlock(&sc->sc_sx);

	/* Rearm GPE */
	if (ACPI_FAILURE(AcpiFinishGpe(NULL, sc->sc_gpe_bit)))
		device_printf(sc->sc_dev, "GPE rearm failed\n");
}

static UINT32
atopcase_acpi_notify(ACPI_HANDLE h __unused, UINT32 notify __unused, void *ctx)
{
	AcpiOsExecute(OSL_GPE_HANDLER, atopcase_acpi_gpe_task, ctx);
	return (0);
}

static void
atopcase_acpi_intr(void *ctx)
{
	struct atopcase_softc *sc = ctx;

	DPRINTFN(ATOPCASE_LLEVEL_DEBUG, "Interrupt event\n");

	mtx_lock(&sc->sc_mtx);
	sc->sc_intr_cnt++;
	(void)atopcase_intr(sc);
	mtx_unlock(&sc->sc_mtx);
}

static int
atopcase_acpi_probe(device_t dev)
{
	ACPI_HANDLE handle;
	int usb_enabled;

	if (acpi_disabled("atopcase"))
		return (ENXIO);

	handle = acpi_get_handle(dev);
	if (handle == NULL)
		return (ENXIO);

	if (!acpi_is_atopcase(handle))
		return (ENXIO);

	/* If USB interface exists and is enabled, use USB driver */
	if (atopcase_acpi_test_comm_enabled(handle, "UIST", &usb_enabled) == 0
	    && usb_enabled != 0)
		return (ENXIO);

	device_set_desc(dev, "Apple MacBook SPI Topcase");

	return (BUS_PROBE_DEFAULT);
}

static int
atopcase_acpi_attach(device_t dev)
{
	struct atopcase_softc *sc = device_get_softc(dev);
	ACPI_DEVICE_INFO *device_info;
	uint32_t cs_delay;
	int spi_enabled, err;

	sc->sc_dev = dev;
	sc->sc_handle = acpi_get_handle(dev);

	if (atopcase_acpi_test_comm_enabled(sc->sc_handle, "SIST",
	    &spi_enabled) != 0) {
		device_printf(dev, "can't test SPI communication\n");
		return (ENXIO);
	}

	/* Turn SPI off if enabled to force "booted" packet to appear */
	if (spi_enabled != 0 &&
	    atopcase_acpi_set_comm_enabled(sc, "SIEN", false) != 0) {
		device_printf(dev, "can't disable SPI communication\n");
		return (ENXIO);
	}

	if (atopcase_acpi_set_comm_enabled(sc, "SIEN", true) != 0) {
		device_printf(dev, "can't enable SPI communication\n");
		return (ENXIO);
	}

	/*
	 * Apple encodes a CS delay in ACPI properties, but
	 * - they're encoded in a non-standard way that predates _DSD, and
	 * - they're only exported if you respond to _OSI(Darwin) which we don't
	 *   - because that has more side effects than we're prepared to handle
	 * - Apple makes a Windows driver and Windows is not Darwin
	 *   - so presumably that one uses hardcoded values too
	 */
	spibus_get_cs_delay(sc->sc_dev, &cs_delay);
	if (cs_delay == 0)
		spibus_set_cs_delay(sc->sc_dev, 10);

	/* Retrieve ACPI _HID */
	if (ACPI_FAILURE(AcpiGetObjectInfo(sc->sc_handle, &device_info)))
		return (ENXIO);
	if (device_info->Valid & ACPI_VALID_HID)
		strlcpy(sc->sc_hid, device_info->HardwareId.String,
		    sizeof(sc->sc_hid));
	AcpiOsFree(device_info);

	sx_init(&sc->sc_write_sx, "atc_wr");
	sx_init(&sc->sc_sx, "atc_sx");
	mtx_init(&sc->sc_mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	err = ENXIO;
	sc->sc_irq_res = bus_alloc_resource_any(sc->sc_dev,
	    SYS_RES_IRQ, &sc->sc_irq_rid, RF_ACTIVE);
	if (sc->sc_irq_res != NULL) {
		if (bus_setup_intr(dev, sc->sc_irq_res,
		      INTR_TYPE_MISC | INTR_MPSAFE, NULL,
		      atopcase_acpi_intr, sc, &sc->sc_irq_ih) != 0) {
			device_printf(dev, "can't setup interrupt handler\n");
			goto err;
		}
		device_printf(dev, "Using interrupts.\n");
		/*
		 * On some hardware interrupts are not acked by SPI read for
		 * unknown reasons that leads to interrupt storm due to level
		 * triggering. GPE does not suffer from this problem.
		 *
		 * TODO: Find out what Windows driver does to ack IRQ.
		 */
		pause("atopcase", hz / 5);
		DPRINTF("interrupts asserted: %u\n", sc->sc_intr_cnt);
		if (sc->sc_intr_cnt > 2 || sc->sc_intr_cnt == 0) {
			bus_teardown_intr(dev, sc->sc_irq_res, sc->sc_irq_ih);
			sc->sc_irq_ih = NULL;
			device_printf(dev, "Interrupt storm detected. "
			    "Falling back to polling\n");
			sc->sc_tq = taskqueue_create("atc_tq", M_WAITOK|M_ZERO,
			    taskqueue_thread_enqueue, &sc->sc_tq);
			TIMEOUT_TASK_INIT(sc->sc_tq, &sc->sc_task, 0,
			    atopcase_acpi_task, sc);
			taskqueue_start_threads(&sc->sc_tq, 1, PI_TTY,
			    "%s taskq", device_get_nameunit(dev));
		}
		/*
		 * Interrupts does not work at all. It may happen if kernel
		 * erroneously detected stray irq at bus_teardown_intr() and
		 * completelly disabled it after than.
		 * Fetch "booted" packet manually to pass communication check.
		 */
		if (sc->sc_intr_cnt == 0)
			atopcase_receive_packet(sc);
	} else {
		if (bootverbose)
			device_printf(dev, "can't allocate IRQ resource\n");
		if (ACPI_FAILURE(acpi_GetInteger(sc->sc_handle, "_GPE",
		    &sc->sc_gpe_bit))) {
			device_printf(dev, "can't allocate nor IRQ nor GPE\n");
			goto err;
		}
		if (ACPI_FAILURE(AcpiInstallGpeHandler(NULL, sc->sc_gpe_bit,
		    ACPI_GPE_LEVEL_TRIGGERED, atopcase_acpi_notify, sc))) {
			device_printf(dev, "can't install ACPI GPE handler\n");
			goto err;
		}
		if (ACPI_FAILURE(AcpiEnableGpe(NULL, sc->sc_gpe_bit))) {
			device_printf(dev, "can't enable ACPI notification\n");
			goto err;
		}
		device_printf(dev, "Using ACPI GPE.\n");
		if (bootverbose)
			device_printf(dev, "GPE int %d\n", sc->sc_gpe_bit);
	}

	err = atopcase_init(sc);

err:
	if (err != 0)
		atopcase_acpi_detach(dev);
	return (err);
}

static int
atopcase_acpi_detach(device_t dev)
{
	struct atopcase_softc *sc = device_get_softc(dev);
	int err;

	err = atopcase_destroy(sc);
	if (err != 0)
		return (err);

	if (sc->sc_irq_ih)
		bus_teardown_intr(dev, sc->sc_irq_res, sc->sc_irq_ih);
	if (sc->sc_irq_res != NULL)
		bus_release_resource(dev, SYS_RES_IRQ,
		    sc->sc_irq_rid, sc->sc_irq_res);

	if (sc->sc_tq != NULL) {
		while (taskqueue_cancel_timeout(sc->sc_tq, &sc->sc_task, NULL))
			taskqueue_drain_timeout(sc->sc_tq, &sc->sc_task);
		taskqueue_free(sc->sc_tq);
	}

	if (sc->sc_gpe_bit != 0 && ACPI_FAILURE(AcpiRemoveGpeHandler(NULL,
	    sc->sc_gpe_bit, atopcase_acpi_notify)))
		device_printf(dev, "can't remove ACPI GPE handler\n");

	if (atopcase_acpi_set_comm_enabled(sc, "SIEN", false) != 0)
		device_printf(dev, "can't disable SPI communication\n");

	mtx_destroy(&sc->sc_mtx);
	sx_destroy(&sc->sc_sx);
	sx_destroy(&sc->sc_write_sx);

	return (0);
}

static int
atopcase_acpi_suspend(device_t dev)
{
	struct atopcase_softc *sc = device_get_softc(dev);
	int err;

	err = bus_generic_suspend(dev);
	if (err)
		return (err);

	if (sc->sc_gpe_bit != 0)
		AcpiDisableGpe(NULL, sc->sc_gpe_bit);

	if (sc->sc_tq != NULL)
		while (taskqueue_cancel_timeout(sc->sc_tq, &sc->sc_task, NULL))
			taskqueue_drain_timeout(sc->sc_tq, &sc->sc_task);

	if (atopcase_acpi_set_comm_enabled(sc, "SIEN", false) != 0)
		device_printf(dev, "can't disable SPI communication\n");

	return (0);
}

static int
atopcase_acpi_resume(device_t dev)
{
	struct atopcase_softc *sc = device_get_softc(dev);

	if (sc->sc_gpe_bit != 0)
		AcpiEnableGpe(NULL, sc->sc_gpe_bit);

	if (sc->sc_tq != NULL)
		taskqueue_enqueue_timeout(sc->sc_tq, &sc->sc_task, hz / 120);

	if (atopcase_acpi_set_comm_enabled(sc, "SIEN", true) != 0) {
		device_printf(dev, "can't enable SPI communication\n");
		return (ENXIO);
	}

	return (bus_generic_resume(dev));
}

static device_method_t atopcase_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		atopcase_acpi_probe),
	DEVMETHOD(device_attach,	atopcase_acpi_attach),
	DEVMETHOD(device_detach,	atopcase_acpi_detach),
	DEVMETHOD(device_suspend,	atopcase_acpi_suspend),
	DEVMETHOD(device_resume,	atopcase_acpi_resume),

	/* HID interrupt interface */
	DEVMETHOD(hid_intr_setup,	atopcase_intr_setup),
	DEVMETHOD(hid_intr_unsetup,	atopcase_intr_unsetup),
	DEVMETHOD(hid_intr_start,	atopcase_intr_start),
	DEVMETHOD(hid_intr_stop,	atopcase_intr_stop),
	DEVMETHOD(hid_intr_poll,	atopcase_intr_poll),

	/* HID interface */
	DEVMETHOD(hid_get_rdesc,	atopcase_get_rdesc),
	DEVMETHOD(hid_set_report,	atopcase_set_report),

	/* Backlight interface */
	DEVMETHOD(backlight_update_status, atopcase_backlight_update_status),
	DEVMETHOD(backlight_get_status,	atopcase_backlight_get_status),
	DEVMETHOD(backlight_get_info,	atopcase_backlight_get_info),

	DEVMETHOD_END
};

static driver_t atopcase_driver = {
	"atopcase",
	atopcase_methods,
	sizeof(struct atopcase_softc),
};

DRIVER_MODULE(atopcase, spibus, atopcase_driver, 0, 0);
MODULE_DEPEND(atopcase, acpi, 1, 1, 1);
MODULE_DEPEND(atopcase, backlight, 1, 1, 1);
MODULE_DEPEND(atopcase, hid, 1, 1, 1);
MODULE_DEPEND(atopcase, hidbus, 1, 1, 1);
MODULE_DEPEND(atopcase, spibus, 1, 1, 1);
MODULE_VERSION(atopcase, 1);
SPIBUS_ACPI_PNP_INFO(atopcase_ids);
