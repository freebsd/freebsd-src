/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2007, 2008 Rui Paulo <rpaulo@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * Driver for Apple's System Management Console (SMC).
 * SMC can be found on the MacBook, MacBook Pro and Mac Mini.
 *
 * Inspired by the Linux applesmc driver.
 */

#include "opt_asmc.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/eventhandler.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/reboot.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/taskqueue.h>
#include <sys/rman.h>

#include <machine/resource.h>
#include <netinet/in.h>

#include <contrib/dev/acpica/include/acpi.h>

#include <dev/acpica/acpivar.h>
#include <dev/asmc/asmcvar.h>
#include <dev/asmc/asmcmmio.h>
#include <dev/asmc/asmc_pio.h>
#include <dev/asmc/asmc_sysctl.h>
#include <dev/asmc/asmc_debug.h>

#include <dev/backlight/backlight.h>
#include "backlight_if.h"

/*
 * Device interface.
 */
static int 	asmc_probe(device_t dev);
static int 	asmc_attach(device_t dev);
static int 	asmc_detach(device_t dev);
static int 	asmc_resume(device_t dev);
static void	asmc_shutdown(void *arg, int howto);

/*
 * Backlight interface.
 */
static int	asmc_backlight_update_status(device_t dev,
    struct backlight_props *props);
static int	asmc_backlight_get_status(device_t dev,
    struct backlight_props *props);
static int	asmc_backlight_get_info(device_t dev,
    struct backlight_info *info);

/*
 * SMC functions.
 */
static int 	asmc_init(device_t dev);
static int 	asmc_fan_count(device_t dev);
static void 	asmc_sms_calibrate(device_t dev);
static int 	asmc_sms_intrfast(void *arg);
static void 	asmc_sms_printintr(device_t dev, uint8_t);
static void 	asmc_sms_task(void *arg, int pending);
static void	asmc_sms_init(device_t dev);
static void	asmc_detect_capabilities(device_t dev);


/* Voltage/Current/Power/Light sensor support */
static int	asmc_detect_sensors(device_t);
static int	asmc_key_search(device_t, const char *, unsigned int *);


/*
 * Driver methods.
 */
static device_method_t	asmc_methods[] = {
	DEVMETHOD(device_probe,		asmc_probe),
	DEVMETHOD(device_attach,	asmc_attach),
	DEVMETHOD(device_detach,	asmc_detach),
	DEVMETHOD(device_resume,	asmc_resume),

	/* Backlight interface */
	DEVMETHOD(backlight_update_status, asmc_backlight_update_status),
	DEVMETHOD(backlight_get_status, asmc_backlight_get_status),
	DEVMETHOD(backlight_get_info, asmc_backlight_get_info),

	DEVMETHOD_END
};

static driver_t	asmc_driver = {
	"asmc",
	asmc_methods,
	sizeof(struct asmc_softc)
};

/*
 * Debugging
 */
#define	_COMPONENT	ACPI_OEM
ACPI_MODULE_NAME("ASMC")

/* NB: can't be const */
static char *asmc_ids[] = { "APP0001", NULL };

ACPI_PNP_INFO(asmc_ids);
DRIVER_MODULE(asmc, acpi, asmc_driver, NULL, NULL);
MODULE_DEPEND(asmc, acpi, 1, 1, 1);
MODULE_DEPEND(asmc, backlight, 1, 1, 1);

static int
asmc_probe(device_t dev)
{
	char *product;
	int rv;

	if (resource_disabled("asmc", 0))
		return (ENXIO);
	rv = ACPI_ID_PROBE(device_get_parent(dev), dev, asmc_ids, NULL);
	if (rv > 0)
		return (rv);
	product = kern_getenv("smbios.system.product");
	device_set_descf(dev, "Apple %s",
	    product != NULL ? product : "SMC");
	freeenv(product);
	return (rv);
}

/*
 * Try MMIO first; the legacy PIO range can be claimable but dead.
 * Fall back to PIO if MMIO probe fails or the resource is absent.
 */
static int
asmc_try_probe(device_t dev)
{
	struct asmc_softc *sc = device_get_softc(dev);

	sc->sc_rid_mem = 0;
	sc->sc_iomem = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->sc_rid_mem, RF_ACTIVE);
	if (sc->sc_iomem != NULL) {
		if (asmc_mmio_probe(dev) == 0) {
			sc->sc_is_mmio = true;
			if (bootverbose)
				device_printf(dev, "using MMIO backend\n");
			return (0);
		}
		bus_release_resource(dev, SYS_RES_MEMORY,
		    sc->sc_rid_mem, sc->sc_iomem);
		sc->sc_iomem = NULL;
	}

	sc->sc_ioport = bus_alloc_resource_any(dev, SYS_RES_IOPORT,
	    &sc->sc_rid_port, RF_ACTIVE);
	if (sc->sc_ioport != NULL)
		return (0);

	device_printf(dev, "unable to allocate IO port or MMIO\n");
	return (ENOMEM);
}



/* Every key asmc_sms_init() touches; all must be present for SMS. */
static const char * const asmc_sms_required_keys[] = {
	ASMC_KEY_SMS,
	ASMC_KEY_SMS_X,
	ASMC_KEY_SMS_Y,
	ASMC_KEY_SMS_Z,
	ASMC_KEY_SMS_LOW,
	ASMC_KEY_SMS_HIGH,
	ASMC_KEY_SMS_LOW_INT,
	ASMC_KEY_SMS_HIGH_INT,
	ASMC_KEY_SMS_FLAG,
	ASMC_KEY_INTOK,
};




static int
asmc_attach(device_t dev)
{
	int ret;
	struct asmc_softc *sc = device_get_softc(dev);
	struct sysctl_ctx_list *sysctlctx;
	struct sysctl_oid *sysctlnode;

	ret = asmc_try_probe(dev);
	if (ret != 0)
		goto err;

	sysctlctx = device_get_sysctl_ctx(dev);
	sysctlnode = device_get_sysctl_tree(dev);

	/* Mutex may already be initialized by asmc_mmio_probe() */
	if (!mtx_initialized(&sc->sc_mtx))
		mtx_init(&sc->sc_mtx, "asmc", NULL, MTX_SPIN);

	/* Read SMC revision, key count, fan count */
	ret = asmc_init(dev);
	if (ret != 0) {
		device_printf(dev, "SMC not responding\n");
		goto err;
	}

	/* Probe SMC keys to detect capabilities */
	asmc_detect_capabilities(dev);

	/* Auto-detect voltage/current/power/ambient/temp sensors */
	asmc_detect_sensors(dev);

	asmc_sysctl_add_fan(dev, sysctlctx);
	asmc_sysctl_add_temp(dev, sysctlctx);

	if (sc->sc_has_light) {
		asmc_sysctl_add_light(dev, sysctlctx);

		sc->sc_kbd_bkl = backlight_register("asmc", dev);
		if (sc->sc_kbd_bkl == NULL) {
			device_printf(dev, "Can not register backlight\n");
			ret = ENXIO;
			goto err;
		}
	}

	asmc_debug_sysctl_add(dev, sysctlctx);

	/*
	 * Battery charge limit (T2 Macs).
	 */
	if (sc->sc_is_t2 &&
	    asmc_key_getinfo(dev, ASMC_KEY_BCLM, NULL, NULL) == 0) {
		SYSCTL_ADD_PROC(sysctlctx,
		    SYSCTL_CHILDREN(sysctlnode), OID_AUTO, "battery_charge_limit",
		    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
		    dev, 0, asmc_bclm_sysctl, "I",
		    "Battery charge limit (0-100)");
	}

	/* System state / board identity subtree. */
	if (asmc_sysctl_add_system(dev, sysctlctx) != 0)
		goto nosms;

	if (asmc_key_getinfo(dev, ASMC_KEY_MBSS, NULL, NULL) == 0)
		sc->sc_shutdown_tag = EVENTHANDLER_REGISTER(shutdown_pre_sync,
		    asmc_shutdown, dev, SHUTDOWN_PRI_FIRST);

	if (!sc->sc_has_sms)
		goto nosms;

	/*
	 * Initialize SMS hardware.
	 */
	asmc_sms_init(dev);

	asmc_sysctl_add_sms(dev, sysctlctx);

	/*
	 * Need a taskqueue to send devctl_notify() events
	 * when the SMS interrupt us.
	 *
	 * PI_REALTIME is used due to the sensitivity of the
	 * interrupt. An interrupt from the SMS means that the
	 * disk heads should be turned off as quickly as possible.
	 *
	 * We only need to do this for the non INTR_FILTER case.
	 */
	sc->sc_sms_tq = NULL;
	TASK_INIT(&sc->sc_sms_task, 0, asmc_sms_task, sc);
	sc->sc_sms_tq = taskqueue_create_fast("asmc_taskq", M_WAITOK,
	    taskqueue_thread_enqueue, &sc->sc_sms_tq);
	taskqueue_start_threads(&sc->sc_sms_tq, 1, PI_REALTIME, "%s sms taskq",
	    device_get_nameunit(dev));
	/*
	 * Allocate an IRQ for the SMS.
	 */
	sc->sc_rid_irq = 0;
	sc->sc_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &sc->sc_rid_irq,
	    RF_ACTIVE);
	if (sc->sc_irq == NULL) {
		device_printf(dev, "unable to allocate IRQ resource\n");
		ret = ENXIO;
		goto err;
	}

	ret = bus_setup_intr(dev, sc->sc_irq, INTR_TYPE_MISC | INTR_MPSAFE,
	    asmc_sms_intrfast, NULL, dev, &sc->sc_cookie);
	if (ret != 0) {
		device_printf(dev, "unable to setup SMS IRQ\n");
		goto err;
	}

nosms:
	return (0);

err:
	asmc_detach(dev);

	return (ret);
}

static int
asmc_detach(device_t dev)
{
	struct asmc_softc *sc = device_get_softc(dev);

	if (sc->sc_shutdown_tag != NULL) {
		EVENTHANDLER_DEREGISTER(shutdown_pre_sync, sc->sc_shutdown_tag);
		sc->sc_shutdown_tag = NULL;
	}

	if (sc->sc_kbd_bkl != NULL)
		backlight_destroy(sc->sc_kbd_bkl);

	/* Free temperature sensor key arrays */
	for (int i = 0; i < sc->sc_temp_count; i++)
		free(sc->sc_temp_sensors[i], M_DEVBUF);

	/* Free sensor key arrays */
	for (int i = 0; i < sc->sc_voltage_count; i++)
		free(sc->sc_voltage_sensors[i], M_DEVBUF);
	for (int i = 0; i < sc->sc_current_count; i++)
		free(sc->sc_current_sensors[i], M_DEVBUF);
	for (int i = 0; i < sc->sc_power_count; i++)
		free(sc->sc_power_sensors[i], M_DEVBUF);
	for (int i = 0; i < sc->sc_light_count; i++)
		free(sc->sc_light_sensors[i], M_DEVBUF);

	if (sc->sc_sms_tq) {
		taskqueue_drain(sc->sc_sms_tq, &sc->sc_sms_task);
		taskqueue_free(sc->sc_sms_tq);
		sc->sc_sms_tq = NULL;
	}
	if (sc->sc_cookie) {
		bus_teardown_intr(dev, sc->sc_irq, sc->sc_cookie);
		sc->sc_cookie = NULL;
	}
	if (sc->sc_irq) {
		bus_release_resource(dev, SYS_RES_IRQ, sc->sc_rid_irq,
		    sc->sc_irq);
		sc->sc_irq = NULL;
	}
	if (sc->sc_ioport) {
		bus_release_resource(dev, SYS_RES_IOPORT, sc->sc_rid_port,
		    sc->sc_ioport);
		sc->sc_ioport = NULL;
	}
	asmc_mmio_detach(dev, sc);
	if (mtx_initialized(&sc->sc_mtx)) {
		mtx_destroy(&sc->sc_mtx);
	}

	return (0);
}

static int
asmc_resume(device_t dev)
{
	struct asmc_softc *sc = device_get_softc(dev);
	uint8_t buf[2];

	buf[0] = sc->sc_light_control;
	buf[1] = 0x00;
	asmc_key_write(dev, ASMC_KEY_LIGHTVALUE, buf, sizeof(buf));

	return (0);
}

static void
asmc_shutdown(void *arg, int howto)
{
	device_t dev = (device_t)arg;
	uint8_t buf[2];

	be16enc(buf, (howto & (RB_POWEROFF | RB_HALT)) != 0 ?
	    ASMC_MBSS_POWEROFF : ASMC_MBSS_RESTART);
	asmc_key_write(dev, ASMC_KEY_MBSS, buf, sizeof(buf));
}

/*
 * Initialize SMC: read revision, key count, fan count.
 * SMS initialization is handled separately in asmc_sms_init().
 */
static int
asmc_init(device_t dev)
{
	struct asmc_softc *sc = device_get_softc(dev);
	struct sysctl_ctx_list *sysctlctx;
	uint8_t buf[6];
	int error;

	sysctlctx = device_get_sysctl_ctx(dev);

	error = asmc_key_read(dev, ASMC_KEY_REV, buf, 6);
	if (error != 0) {
		/*
		 * Could not read REV key; T2 Macs may not have it.
		 * Use #KEY as a liveness check instead.
		 */
		if (sc->sc_is_t2) {
			error = asmc_key_read(dev, ASMC_NKEYS, buf, 4);
			if (error != 0)
				goto out;
			device_printf(dev, "T2 SMC: %d keys\n",
			    be32dec(buf));
		} else {
			goto out;
		}
	} else {
		device_printf(dev, "SMC revision: %x.%x%x%x\n",
		    buf[0], buf[1], buf[2],
		    ntohs(*(uint16_t *)buf + 4));
	}

	asmc_sysctl_add_platform(dev, sysctlctx);

	sc->sc_nfan = asmc_fan_count(dev);
	if (sc->sc_nfan > ASMC_MAXFANS) {
		device_printf(dev,
		    "more than %d fans were detected. Please report this.\n",
		    ASMC_MAXFANS);
		sc->sc_nfan = ASMC_MAXFANS;
	}

	/*
	 * Read and cache the number of SMC keys (32 bit buffer)
	 */
	if (asmc_key_read(dev, ASMC_NKEYS, buf, 4) == 0) {
		sc->sc_nkeys = be32dec(buf);
		if (bootverbose)
			device_printf(dev, "number of keys: %d\n",
			    sc->sc_nkeys);
	} else {
		sc->sc_nkeys = 0;
	}

out:
	asmc_dumpall(dev);
	return (error);
}

/*
 * Initialize the Sudden Motion Sensor hardware.
 * Called from asmc_attach() after capabilities are detected.
 */
static void
asmc_sms_init(device_t dev)
{
	struct asmc_softc *sc = device_get_softc(dev);
	uint8_t buf[2];

	/*
	 * We are ready to receive interrupts from the SMS.
	 */
	buf[0] = 0x01;
	ASMC_DPRINTF(("intok key\n"));
	asmc_key_write(dev, ASMC_KEY_INTOK, buf, 1);
	DELAY(50);

	/*
	 * Initiate the polling intervals.
	 */
	buf[0] = 20; /* msecs */
	ASMC_DPRINTF(("low int key\n"));
	asmc_key_write(dev, ASMC_KEY_SMS_LOW_INT, buf, 1);
	DELAY(200);

	buf[0] = 20; /* msecs */
	ASMC_DPRINTF(("high int key\n"));
	asmc_key_write(dev, ASMC_KEY_SMS_HIGH_INT, buf, 1);
	DELAY(200);

	buf[0] = 0x00;
	buf[1] = 0x60;
	ASMC_DPRINTF(("sms low key\n"));
	asmc_key_write(dev, ASMC_KEY_SMS_LOW, buf, 2);
	DELAY(200);

	buf[0] = 0x01;
	buf[1] = 0xc0;
	ASMC_DPRINTF(("sms high key\n"));
	asmc_key_write(dev, ASMC_KEY_SMS_HIGH, buf, 2);
	DELAY(200);

	/*
	 * I'm not sure what this key does, but it seems to be
	 * required.
	 */
	buf[0] = 0x01;
	ASMC_DPRINTF(("sms flag key\n"));
	asmc_key_write(dev, ASMC_KEY_SMS_FLAG, buf, 1);
	DELAY(100);

	sc->sc_sms_intr_works = 0;

	/*
	 * Retry SMS initialization 1000 times
	 * (takes approx. 2 seconds in worst case)
	 */
	for (int i = 0; i < 1000; i++) {
		if (asmc_key_read(dev, ASMC_KEY_SMS, buf, 2) == 0 &&
		    (buf[0] == ASMC_SMS_INIT1 && buf[1] == ASMC_SMS_INIT2)) {
			sc->sc_sms_intr_works = 1;
			goto done;
		}
		buf[0] = ASMC_SMS_INIT1;
		buf[1] = ASMC_SMS_INIT2;
		ASMC_DPRINTF(("sms key\n"));
		asmc_key_write(dev, ASMC_KEY_SMS, buf, 2);
		DELAY(50);
	}
	device_printf(dev, "WARNING: Sudden Motion Sensor not initialized!\n");

done:
	asmc_sms_calibrate(dev);
}

/*
 * Probe SMC keys to detect hardware capabilities.
 */
static void
asmc_detect_capabilities(device_t dev)
{
	struct asmc_softc *sc = device_get_softc(dev);
	uint8_t len;
	char type[ASMC_TYPELEN + 1];

	/* SMS: require all keys used by asmc_sms_init() */
	sc->sc_has_sms = 1;
	for (unsigned int k = 0; k < nitems(asmc_sms_required_keys); k++) {
		if (asmc_key_getinfo(dev, asmc_sms_required_keys[k],
		    &len, type) != 0) {
			sc->sc_has_sms = 0;
			break;
		}
	}

	/* Light sensor: require ALV0 (len 6 or 10) and LKSB */
	if (asmc_key_getinfo(dev, ASMC_KEY_LIGHTLEFT,
	    &len, type) == 0 &&
	    (len == ASMC_LIGHT_SHORTLEN || len == ASMC_LIGHT_LONGLEN) &&
	    asmc_key_getinfo(dev, ASMC_KEY_LIGHTVALUE,
	    NULL, NULL) == 0) {
		sc->sc_has_light = 1;
		sc->sc_light_len = len;
	} else {
		sc->sc_has_light = 0;
		sc->sc_light_len = 0;
	}

	/* Fan safe speed */
	sc->sc_has_safespeed =
	    (asmc_key_getinfo(dev, ASMC_KEY_FANSAFESPEED0,
	    &len, type) == 0);

	/* Ambient light interrupt source */
	sc->sc_has_alsl =
	    (asmc_key_getinfo(dev, ASMC_KEY_LIGHTSRC,
	    &len, type) == 0);

	if (bootverbose)
		device_printf(dev,
		    "capabilities: sms=%d light=%d (len=%d) safespeed=%d alsl=%d\n",
		    sc->sc_has_sms, sc->sc_has_light, sc->sc_light_len,
		    sc->sc_has_safespeed, sc->sc_has_alsl);
}

int
asmc_key_read(device_t dev, const char *key, uint8_t *buf, uint8_t len)
{
	struct asmc_softc *sc = device_get_softc(dev);

	if (sc->sc_is_mmio)
		return (asmc_mmio_key_read(dev, key, buf, len));

	return (asmc_pio_key_read(dev, key, buf, len));
}

int
asmc_key_getinfo(device_t dev, const char *key, uint8_t *len, char *type)
{
	struct asmc_softc *sc = device_get_softc(dev);

	if (sc->sc_is_mmio)
		return (asmc_mmio_key_getinfo(dev, key, len, type));

	return (asmc_pio_key_getinfo(dev, key, len, type));
}

/* SMC sensor type table: type string to fixed-point divisor. */
static const struct {
	const char	type[5];
	int		divisor;
} asmc_sensor_types[] = {
	{ "sp78",  256 },
	{ "sp87",  128 },
	{ "sp4b", 2048 },
	{ "sp5a", 1024 },
	{ "sp69",  512 },
	{ "sp96",   64 },
	{ "sp2d", 8192 },
	{ "ui16",    1 },
	{ "",        0 },
};

/* Convert a 2-byte SMC value to milli-units. */
static bool
asmc_sensor_convert(const char *type, const uint8_t *buf, int *millivalue)
{
	for (int i = 0; asmc_sensor_types[i].divisor != 0; i++) {
		if (strncmp(type, asmc_sensor_types[i].type, 4) != 0)
			continue;
		if (asmc_sensor_types[i].divisor == 1)
			*millivalue = be16dec(buf);
		else
			*millivalue = ((int)(int16_t)be16dec(buf) * 1000) /
			    asmc_sensor_types[i].divisor;
		return (true);
	}
	return (false);
}

static bool
asmc_sensor_type_supported(const char *type)
{
	for (int i = 0; asmc_sensor_types[i].divisor != 0; i++)
		if (strncmp(type, asmc_sensor_types[i].type, 4) == 0)
			return (true);
	return (false);
}

/*
 * Generic sensor value reader with automatic type conversion.
 * Reads an SMC key, detects its type, and converts to millivalue.
 */
int
asmc_sensor_read(device_t dev, const char *key, int *millivalue)
{
	uint8_t buf[2];
	char type[ASMC_TYPELEN + 1];
	uint8_t len;
	int error;

	error = asmc_key_getinfo(dev, key, &len, type);
	if (error != 0)
		return (error);

	if (len != 2) {
		if (bootverbose)
			device_printf(dev,
			    "%s: key %s unexpected length %d\n",
			    __func__, key, len);
		return (ENXIO);
	}

	error = asmc_key_read(dev, key, buf, sizeof(buf));
	if (error != 0)
		return (error);

	if (!asmc_sensor_convert(type, buf, millivalue)) {
		if (bootverbose)
			device_printf(dev,
			    "%s: unknown type '%s' for key %s\n",
			    __func__, type, key);
		return (ENXIO);
	}

	return (0);
}


static bool
asmc_match_sp78(const char *key __unused, const char *type)
{

	return (strncmp(type, "sp78", 4) == 0);
}

static bool
asmc_match_supported(const char *key __unused, const char *type)
{

	return (asmc_sensor_type_supported(type));
}

static bool
asmc_match_light(const char *key, const char *type)
{

	return (key[1] == 'L' && (key[2] == 'V' || key[2] == 'S') &&
	    asmc_sensor_type_supported(type));
}

/* Scan key indices for 2-byte keys passing prefix (if set) and match. */
static void
asmc_scan_sensor_range(device_t dev, unsigned int start,
    unsigned int end, char prefix,
    bool (*match)(const char *key, const char *type), int *countp,
    char **sensors, int maxcount)
{
	char key[ASMC_KEYLEN + 1];
	char type[ASMC_TYPELEN + 1];
	uint8_t len;
	char *sensor_key;

	for (unsigned int i = start; i < end; i++) {
		if (asmc_key_dump_by_index(dev, i, key, type, &len) != 0)
			continue;
		if (len != 2 || (prefix != '\0' && key[0] != prefix))
			continue;
		if (!match(key, type))
			continue;
		if (*countp >= maxcount)
			break;
		sensor_key = malloc(ASMC_KEYLEN + 1,
		    M_DEVBUF, M_WAITOK);
		memcpy(sensor_key, key, ASMC_KEYLEN + 1);
		sensors[(*countp)++] = sensor_key;
	}
}

static int
asmc_detect_sensors(device_t dev)
{
	struct asmc_softc *sc = device_get_softc(dev);
	struct sysctl_ctx_list *sysctlctx;
	struct sysctl_oid *tree_node;
	unsigned int start, end, i;
	int error;

	sc->sc_voltage_count = 0;
	sc->sc_current_count = 0;
	sc->sc_power_count = 0;
	sc->sc_light_count = 0;
	sc->sc_temp_count = 0;

	if (sc->sc_nkeys == 0)
		return (0);

	/*
	 * Temperature sensors: binary search for T..U range,
	 * then filter by type sp78.
	 */
	error = asmc_key_search(dev, "T\0\0\0", &start);
	if (error == 0)
		error = asmc_key_search(dev, "U\0\0\0", &end);
	if (error == 0)
		asmc_scan_sensor_range(dev, start, end, '\0', asmc_match_sp78,
		    &sc->sc_temp_count, sc->sc_temp_sensors, ASMC_TEMP_MAX);

	/* Voltage/Current/Power sensors */
	static const struct {
		const char	*range_start;
		const char	*range_end;
		char		prefix;
	} sensor_ranges[] = {
		{ "V\0\0\0", "W\0\0\0", 'V' },	/* Voltage */
		{ "I\0\0\0", "J\0\0\0", 'I' },	/* Current */
		{ "P\0\0\0", "Q\0\0\0", 'P' },	/* Power */
	};
	static const size_t nsensor_ranges = nitems(sensor_ranges);

	int *sensor_counts[] = {
	    &sc->sc_voltage_count, &sc->sc_current_count,
	    &sc->sc_power_count };
	char **sensor_arrays[] = {
	    sc->sc_voltage_sensors, sc->sc_current_sensors,
	    sc->sc_power_sensors };

	for (unsigned int r = 0; r < nsensor_ranges; r++) {
		error = asmc_key_search(dev, sensor_ranges[r].range_start,
		    &start);
		if (error == 0)
			error = asmc_key_search(dev,
			    sensor_ranges[r].range_end, &end);
		if (error == 0)
			asmc_scan_sensor_range(dev, start, end,
			    sensor_ranges[r].prefix, asmc_match_supported,
			    sensor_counts[r], sensor_arrays[r],
			    ASMC_MAX_SENSORS);
	}

	/* Ambient light sensors: AL* in A..B range */
	error = asmc_key_search(dev, "A\0\0\0", &start);
	if (error == 0)
		error = asmc_key_search(dev, "B\0\0\0", &end);
	if (error == 0)
		asmc_scan_sensor_range(dev, start, end, 'A', asmc_match_light,
		    &sc->sc_light_count, sc->sc_light_sensors,
		    ASMC_MAX_SENSORS);

	if (bootverbose)
		device_printf(dev,
		    "detected %d temp, %d voltage, %d current, "
		    "%d power, %d light sensors\n",
		    sc->sc_temp_count, sc->sc_voltage_count,
		    sc->sc_current_count,
		    sc->sc_power_count, sc->sc_light_count);

	/* Register sysctls for detected sensors */
	sysctlctx = device_get_sysctl_ctx(dev);

	static const struct {
		const char	*node_name;
		const char	*node_desc;
		char		tag;
		const char	*leaf_desc;
	} sensor_sysctl[] = {
		{ "voltage", "Voltage sensors (millivolts)",  'V',
		    "Voltage sensor (millivolts)" },
		{ "current", "Current sensors (milliamps)",   'I',
		    "Current sensor (milliamps)" },
		{ "power",   "Power sensors (milliwatts)",    'P',
		    "Power sensor (milliwatts)" },
		{ "ambient", "Ambient light sensors",         'L',
		    "Light sensor value" },
	};

	int *sysctl_counts[] = {
	    &sc->sc_voltage_count, &sc->sc_current_count,
	    &sc->sc_power_count, &sc->sc_light_count };
	char **sysctl_arrays[] = {
	    sc->sc_voltage_sensors, sc->sc_current_sensors,
	    sc->sc_power_sensors, sc->sc_light_sensors };

	for (unsigned int s = 0; s < nitems(sensor_sysctl); s++) {
		int count = *sysctl_counts[s];
		if (count <= 0)
			continue;
		tree_node = SYSCTL_ADD_NODE(sysctlctx,
		    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
		    sensor_sysctl[s].node_name,
		    CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
		    sensor_sysctl[s].node_desc);
		for (i = 0; i < count; i++) {
			SYSCTL_ADD_PROC(sysctlctx,
			    SYSCTL_CHILDREN(tree_node),
			    OID_AUTO, sysctl_arrays[s][i],
			    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE,
			    dev, (sensor_sysctl[s].tag << 8) | i,
			    asmc_sensor_sysctl, "I",
			    sensor_sysctl[s].leaf_desc);
		}
	}

	return (0);
}

/*
 * Helper function to get key info by index (for sensor detection).
 */
int
asmc_key_dump_by_index(device_t dev, int index, char *key_out,
    char *type_out, uint8_t *len_out)
{
	struct asmc_softc *sc = device_get_softc(dev);
	int error;

	if (sc->sc_is_mmio) {
		error = asmc_mmio_key_getbyindex(dev, index, key_out);
		if (error != 0)
			return (error);
		return (asmc_mmio_key_getinfo(dev, key_out, len_out,
		    type_out));
	}

	return (asmc_pio_key_dump_by_index(dev, index, key_out, type_out,
	    len_out));
}

/*
 * Binary search for the first key index >= prefix.
 * SMC keys are sorted, so this finds the lower bound efficiently.
 */
static int
asmc_key_search(device_t dev, const char *prefix, unsigned int *idx)
{
	struct asmc_softc *sc = device_get_softc(dev);
	unsigned int lo, hi, mid;
	char key[ASMC_KEYLEN + 1];
	char type[ASMC_TYPELEN + 1];
	uint8_t len;
	int error;

	lo = 0;
	hi = sc->sc_nkeys;
	while (lo < hi) {
		mid = lo + (hi - lo) / 2;
		error = asmc_key_dump_by_index(dev, mid,
		    key, type, &len);
		if (error != 0)
			return (error);
		if (strncmp(key, prefix, ASMC_KEYLEN) < 0)
			lo = mid + 1;
		else
			hi = mid;
	}
	*idx = lo;
	return (0);
}

int
asmc_key_write(device_t dev, const char *key, uint8_t *buf, uint8_t len)
{
	struct asmc_softc *sc = device_get_softc(dev);

	if (sc->sc_is_mmio)
		return (asmc_mmio_key_write(dev, key, buf, len));

	return (asmc_pio_key_write(dev, key, buf, len));
}

/*
 * Fan control functions.
 */
static int
asmc_fan_count(device_t dev)
{
	uint8_t buf[1];

	if (asmc_key_read(dev, ASMC_KEY_FANCOUNT, buf, sizeof(buf)) != 0)
		return (-1);

	return (buf[0]);
}



/*
 * Sudden Motion Sensor functions.
 */
int
asmc_sms_read(device_t dev, const char *key, int16_t *val)
{
	uint8_t buf[2];
	int error;

	/* no need to do locking here as asmc_key_read() already does it */
	switch (key[3]) {
	case 'X':
	case 'Y':
	case 'Z':
		error = asmc_key_read(dev, key, buf, sizeof(buf));
		break;
	default:
		device_printf(dev, "%s called with invalid argument %s\n",
		    __func__, key);
		error = EINVAL;
		goto out;
	}
	*val = ((int16_t)buf[0] << 8) | buf[1];
out:
	return (error);
}

static void
asmc_sms_calibrate(device_t dev)
{
	struct asmc_softc *sc = device_get_softc(dev);

	asmc_sms_read(dev, ASMC_KEY_SMS_X, &sc->sms_rest_x);
	asmc_sms_read(dev, ASMC_KEY_SMS_Y, &sc->sms_rest_y);
	asmc_sms_read(dev, ASMC_KEY_SMS_Z, &sc->sms_rest_z);
}

static int
asmc_sms_intrfast(void *arg)
{
	uint8_t type;
	device_t dev = (device_t)arg;
	struct asmc_softc *sc = device_get_softc(dev);
	if (!sc->sc_sms_intr_works)
		return (FILTER_HANDLED);

	mtx_lock_spin(&sc->sc_mtx);
	type = ASMC_INTPORT_READ(sc);
	mtx_unlock_spin(&sc->sc_mtx);

	sc->sc_sms_intrtype = type;
	asmc_sms_printintr(dev, type);

	/* Don't queue SMS task for ambient light interrupts */
	if (type == ASMC_ALSL_INT2A && sc->sc_has_alsl)
		return (FILTER_HANDLED);

	taskqueue_enqueue(sc->sc_sms_tq, &sc->sc_sms_task);
	return (FILTER_HANDLED);
}

static void
asmc_sms_printintr(device_t dev, uint8_t type)
{
	struct asmc_softc *sc = device_get_softc(dev);

	switch (type) {
	case ASMC_SMS_INTFF:
		device_printf(dev, "WARNING: possible free fall!\n");
		break;
	case ASMC_SMS_INTHA:
		device_printf(dev, "WARNING: high acceleration detected!\n");
		break;
	case ASMC_SMS_INTSH:
		device_printf(dev, "WARNING: possible shock!\n");
		break;
	case ASMC_ALSL_INT2A:
		/*
		 * This suppresses console and log messages for the ambient
		 * light sensor interrupt on models that have ALSL.
		 */
		if (sc->sc_has_alsl)
			break;
		/* FALLTHROUGH */
	default:
		device_printf(dev, "unknown interrupt: 0x%x\n", type);
	}
}

static void
asmc_sms_task(void *arg, int pending)
{
	struct asmc_softc *sc = (struct asmc_softc *)arg;
	char notify[16];
	int type;

	switch (sc->sc_sms_intrtype) {
	case ASMC_SMS_INTFF:
		type = 2;
		break;
	case ASMC_SMS_INTHA:
		type = 1;
		break;
	case ASMC_SMS_INTSH:
		type = 0;
		break;
	default:
		type = 255;
	}

	snprintf(notify, sizeof(notify), " notify=0x%x", type);
	devctl_notify("ACPI", "asmc", "SMS", notify);
}



static int
asmc_backlight_update_status(device_t dev, struct backlight_props *props)
{
	struct asmc_softc *sc = device_get_softc(dev);
	uint8_t buf[2];

	sc->sc_kbd_bkl_level = props->brightness;
	sc->sc_light_control = props->brightness * 255 / 100;
	buf[0] = sc->sc_light_control;
	buf[1] = 0x00;
	asmc_key_write(dev, ASMC_KEY_LIGHTVALUE, buf, sizeof(buf));

	return (0);
}

static int
asmc_backlight_get_status(device_t dev, struct backlight_props *props)
{
	struct asmc_softc *sc = device_get_softc(dev);

	props->brightness = sc->sc_kbd_bkl_level;
	props->nlevels = 0;

	return (0);
}

static int
asmc_backlight_get_info(device_t dev, struct backlight_info *info)
{
	info->type = BACKLIGHT_TYPE_KEYBOARD;
	strlcpy(info->name, "Apple MacBook Keyboard", BACKLIGHTMAXNAMELENGTH);

	return (0);
}

