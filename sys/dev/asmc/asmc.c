/*-
 * Copyright (c) 2007 Rui Paulo <rpaulo@FreeBSD.org>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/taskqueue.h>
#include <isa/isavar.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/asmc/asmcvar.h>

/*
 * Device interface.
 */
static void 	asmc_identify(driver_t *driver, device_t parent);
static int 	asmc_probe(device_t dev);
static int 	asmc_attach(device_t dev);
static int 	asmc_detach(device_t dev);

/*
 * SMC functions.
 */
static int 	asmc_init(device_t dev);
static int 	asmc_wait(device_t dev, uint8_t val);
static int 	asmc_key_write(device_t dev, const char *key, uint8_t *buf,
    uint8_t len);
static int 	asmc_key_read(device_t dev, const char *key, uint8_t *buf,
    uint8_t);
static int 	asmc_fan_count(device_t dev);
static int 	asmc_fan_getvalue(device_t dev, const char *key, int fan);
static int 	asmc_temp_getvalue(device_t dev, const char *key);
static int 	asmc_sms_read(device_t, const char *key, int16_t *val);
static void 	asmc_sms_calibrate(device_t dev);
static int 	asmc_sms_intrfast(void *arg);
#ifdef INTR_FILTER
static void 	asmc_sms_handler(void *arg);
#endif
static void 	asmc_sms_printintr(device_t dev, uint8_t);
static void 	asmc_sms_task(void *arg, int pending);

/*
 * Model functions.
 */
static int 	asmc_mb_sysctl_fanspeed(SYSCTL_HANDLER_ARGS);
static int 	asmc_mb_sysctl_fansafespeed(SYSCTL_HANDLER_ARGS);
static int 	asmc_mb_sysctl_fanminspeed(SYSCTL_HANDLER_ARGS);
static int 	asmc_mb_sysctl_fanmaxspeed(SYSCTL_HANDLER_ARGS);
static int 	asmc_mb_sysctl_fantargetspeed(SYSCTL_HANDLER_ARGS);
static int 	asmc_temp_sysctl(SYSCTL_HANDLER_ARGS);
static int 	asmc_mb_sysctl_sms_x(SYSCTL_HANDLER_ARGS);
static int 	asmc_mb_sysctl_sms_y(SYSCTL_HANDLER_ARGS);
static int 	asmc_mb_sysctl_sms_z(SYSCTL_HANDLER_ARGS);
static int 	asmc_mbp_sysctl_light_left(SYSCTL_HANDLER_ARGS);
static int 	asmc_mbp_sysctl_light_right(SYSCTL_HANDLER_ARGS);

struct asmc_model {
	const char 	 *smc_model;	/* smbios.system.product env var. */
	const char 	 *smc_desc;	/* driver description */

	/* Helper functions */
	int (*smc_sms_x)(SYSCTL_HANDLER_ARGS);
	int (*smc_sms_y)(SYSCTL_HANDLER_ARGS);
	int (*smc_sms_z)(SYSCTL_HANDLER_ARGS);
	int (*smc_fan_speed)(SYSCTL_HANDLER_ARGS);
	int (*smc_fan_safespeed)(SYSCTL_HANDLER_ARGS);
	int (*smc_fan_minspeed)(SYSCTL_HANDLER_ARGS);
	int (*smc_fan_maxspeed)(SYSCTL_HANDLER_ARGS);
	int (*smc_fan_targetspeed)(SYSCTL_HANDLER_ARGS);
	int (*smc_light_left)(SYSCTL_HANDLER_ARGS);
	int (*smc_light_right)(SYSCTL_HANDLER_ARGS);

	const char 	*smc_temps[8];
	const char 	*smc_tempnames[8];
	const char 	*smc_tempdescs[8];
};

static struct asmc_model *asmc_match(device_t dev);

#define ASMC_SMS_FUNCS	asmc_mb_sysctl_sms_x, asmc_mb_sysctl_sms_y, \
			asmc_mb_sysctl_sms_z

#define ASMC_FAN_FUNCS	asmc_mb_sysctl_fanspeed, asmc_mb_sysctl_fansafespeed, \
			asmc_mb_sysctl_fanminspeed, \
			asmc_mb_sysctl_fanmaxspeed, \
			asmc_mb_sysctl_fantargetspeed
#define ASMC_LIGHT_FUNCS asmc_mbp_sysctl_light_left, \
			 asmc_mbp_sysctl_light_right

struct asmc_model asmc_models[] = {
	{ 
	  "MacBook1,1", "Apple SMC MacBook Core Duo",
	  ASMC_SMS_FUNCS, ASMC_FAN_FUNCS, NULL, NULL,
	  ASMC_MB_TEMPS, ASMC_MB_TEMPNAMES, ASMC_MB_TEMPDESCS
	},

	{ 
	  "MacBook2,1", "Apple SMC MacBook Core 2 Duo",
	  ASMC_SMS_FUNCS, ASMC_FAN_FUNCS, NULL, NULL,
	  ASMC_MB_TEMPS, ASMC_MB_TEMPNAMES, ASMC_MB_TEMPDESCS
	},

	{ 
	  "MacBookPro1,1", "Apple SMC MacBook Pro Core Duo (15-inch)",
	  ASMC_SMS_FUNCS, ASMC_FAN_FUNCS, ASMC_LIGHT_FUNCS,
	  ASMC_MBP_TEMPS, ASMC_MBP_TEMPNAMES, ASMC_MBP_TEMPDESCS
	},

	{ 
	  "MacBookPro1,2", "Apple SMC MacBook Pro Core Duo (17-inch)",
	  ASMC_SMS_FUNCS, ASMC_FAN_FUNCS, ASMC_LIGHT_FUNCS,
	  ASMC_MBP_TEMPS, ASMC_MBP_TEMPNAMES, ASMC_MBP_TEMPDESCS
	},

	{ 
	  "MacBookPro2,1", "Apple SMC MacBook Pro Core 2 Duo (17-inch)",
	  ASMC_SMS_FUNCS, ASMC_FAN_FUNCS, ASMC_LIGHT_FUNCS,
	  ASMC_MBP_TEMPS, ASMC_MBP_TEMPNAMES, ASMC_MBP_TEMPDESCS
	},

	{ 
	  "MacBookPro2,2", "Apple SMC MacBook Pro Core 2 Duo (15-inch)",
	  ASMC_SMS_FUNCS, ASMC_FAN_FUNCS, ASMC_LIGHT_FUNCS,
	  ASMC_MBP_TEMPS, ASMC_MBP_TEMPNAMES, ASMC_MBP_TEMPDESCS
	},

	{ 
	  "MacBookPro3,1", "Apple SMC MacBook Pro Core 2 Duo (15-inch LED)",
	  ASMC_SMS_FUNCS, ASMC_FAN_FUNCS, ASMC_LIGHT_FUNCS,
	  ASMC_MBP_TEMPS, ASMC_MBP_TEMPNAMES, ASMC_MBP_TEMPDESCS
	},

	{ 
	  "MacBookPro3,2", "Apple SMC MacBook Pro Core 2 Duo (17-inch HD)",
	  ASMC_SMS_FUNCS, ASMC_FAN_FUNCS, ASMC_LIGHT_FUNCS,
	  ASMC_MBP_TEMPS, ASMC_MBP_TEMPNAMES, ASMC_MBP_TEMPDESCS
	},
	
	/* The Mac Mini has no SMS */
	{ 
	  "Macmini1,1", "Apple SMC Mac Mini",
	  NULL, NULL, NULL,
	  NULL, NULL,
	  ASMC_FAN_FUNCS,
	  ASMC_MM_TEMPS, ASMC_MM_TEMPNAMES, ASMC_MM_TEMPDESCS
	},

	{ NULL, NULL }
};

#undef ASMC_SMS_FUNCS
#undef ASMC_FAN_FUNCS
#undef ASMC_LIGHT_FUNCS

/*
 * Driver methods.
 */
static device_method_t	asmc_methods[] = {
	DEVMETHOD(device_identify,	asmc_identify),
	DEVMETHOD(device_probe,		asmc_probe),
	DEVMETHOD(device_attach,	asmc_attach),
	DEVMETHOD(device_detach,	asmc_detach),

	{ 0, 0 }
};

static driver_t	asmc_driver = {
	"asmc",
	asmc_methods,
	sizeof(struct asmc_softc)
};

static devclass_t asmc_devclass;

DRIVER_MODULE(asmc, isa, asmc_driver, asmc_devclass, NULL, NULL);

static void
asmc_identify(driver_t *driver, device_t parent)
{
	if (device_find_child(parent, "asmc", -1) == NULL &&
	    asmc_match(parent))
		BUS_ADD_CHILD(parent, 0, "asmc", -1);
}

static struct asmc_model *
asmc_match(device_t dev)
{
	int i;
	char *model;

	model = getenv("smbios.system.product");
	for (i = 0; asmc_models[i].smc_model; i++) {
		if (!strncmp(model, asmc_models[i].smc_model, strlen(model))) {
			freeenv(model);
			return (&asmc_models[i]);
		}
	}
	freeenv(model);

	return (NULL);
}

static int
asmc_probe(device_t dev)
{
	struct asmc_model *model;

	if (resource_disabled("asmc", 0))
		return (ENXIO);
	model = asmc_match(dev);
	if (!model)
		return (ENXIO);
	if (isa_get_irq(dev) == -1)
		bus_set_resource(dev, SYS_RES_IRQ, 0, ASMC_IRQ, 1);
	device_set_desc(dev, model->smc_desc);

	return (BUS_PROBE_DEFAULT);
}

static int
asmc_attach(device_t dev)
{
	int i, j;
	int ret;
	char name[2];
	struct asmc_softc *sc = device_get_softc(dev);
	struct sysctl_ctx_list *sysctlctx;
	struct sysctl_oid *sysctlnode;
	struct asmc_model *model;

	sysctlctx  = device_get_sysctl_ctx(dev);
	sysctlnode = device_get_sysctl_tree(dev);
	
	model = asmc_match(dev);

	mtx_init(&sc->sc_mtx, "asmc", NULL, MTX_SPIN);

	sc->sc_model = model;
	asmc_init(dev);

	/*
	 * dev.asmc.n.fan.* tree.
	 */
	sc->sc_fan_tree[0] = SYSCTL_ADD_NODE(sysctlctx,
	    SYSCTL_CHILDREN(sysctlnode), OID_AUTO, "fan",
	    CTLFLAG_RD, 0, "Fan Root Tree");

	for (i = 1; i <= sc->sc_nfan; i++) {
		j = i - 1;
		name[0] = '0' + j;
		name[1] = 0;
		sc->sc_fan_tree[i] = SYSCTL_ADD_NODE(sysctlctx,
		    SYSCTL_CHILDREN(sc->sc_fan_tree[0]),
		    OID_AUTO, name, CTLFLAG_RD, 0,
		    "Fan Subtree");

		SYSCTL_ADD_PROC(sysctlctx,
		    SYSCTL_CHILDREN(sc->sc_fan_tree[i]),
		    OID_AUTO, "speed", CTLTYPE_INT | CTLFLAG_RD,
		    dev, j, model->smc_fan_speed, "I",
		    "Fan speed in RPM");

		SYSCTL_ADD_PROC(sysctlctx,
		    SYSCTL_CHILDREN(sc->sc_fan_tree[i]),
		    OID_AUTO, "safespeed",
		    CTLTYPE_INT | CTLFLAG_RD,
		    dev, j, model->smc_fan_safespeed, "I",
		    "Fan safe speed in RPM");

		SYSCTL_ADD_PROC(sysctlctx,
		    SYSCTL_CHILDREN(sc->sc_fan_tree[i]),
		    OID_AUTO, "minspeed",
		    CTLTYPE_INT | CTLFLAG_RD,
		    dev, j, model->smc_fan_minspeed, "I",
		    "Fan minimum speed in RPM");

		SYSCTL_ADD_PROC(sysctlctx,
		    SYSCTL_CHILDREN(sc->sc_fan_tree[i]),
		    OID_AUTO, "maxspeed",
		    CTLTYPE_INT | CTLFLAG_RD,
		    dev, j, model->smc_fan_maxspeed, "I",
		    "Fan maximum speed in RPM");

		SYSCTL_ADD_PROC(sysctlctx,
		    SYSCTL_CHILDREN(sc->sc_fan_tree[i]),
		    OID_AUTO, "targetspeed",
		    CTLTYPE_INT | CTLFLAG_RD,
		    dev, j, model->smc_fan_targetspeed, "I",
		    "Fan target speed in RPM");
	}

	/*
	 * dev.asmc.n.temp tree.
	 */
	sc->sc_temp_tree = SYSCTL_ADD_NODE(sysctlctx,
	    SYSCTL_CHILDREN(sysctlnode), OID_AUTO, "temp",
	    CTLFLAG_RD, 0, "Temperature sensors");

	for (i = 0; model->smc_temps[i]; i++) {
		SYSCTL_ADD_PROC(sysctlctx,
		    SYSCTL_CHILDREN(sc->sc_temp_tree),
		    OID_AUTO, model->smc_tempnames[i],
		    CTLTYPE_INT | CTLFLAG_RD,
		    dev, i, asmc_temp_sysctl, "I",
		    model->smc_tempdescs[i]);
	}

	if (model->smc_sms_x == NULL)
		goto nosms;

	/*
	 * dev.asmc.n.sms tree.
	 */
	sc->sc_sms_tree = SYSCTL_ADD_NODE(sysctlctx,
	    SYSCTL_CHILDREN(sysctlnode), OID_AUTO, "sms",
	    CTLFLAG_RD, 0, "Sudden Motion Sensor");

	SYSCTL_ADD_PROC(sysctlctx,
	    SYSCTL_CHILDREN(sc->sc_sms_tree),
	    OID_AUTO, "x", CTLTYPE_INT | CTLFLAG_RD,
	    dev, 0, model->smc_sms_x, "I",
	    "Sudden Motion Sensor X value");

	SYSCTL_ADD_PROC(sysctlctx,
	    SYSCTL_CHILDREN(sc->sc_sms_tree),
	    OID_AUTO, "y", CTLTYPE_INT | CTLFLAG_RD,
	    dev, 0, model->smc_sms_y, "I",
	    "Sudden Motion Sensor Y value");

	SYSCTL_ADD_PROC(sysctlctx,
	    SYSCTL_CHILDREN(sc->sc_sms_tree),
	    OID_AUTO, "z", CTLTYPE_INT | CTLFLAG_RD,
	    dev, 0, model->smc_sms_z, "I",
	    "Sudden Motion Sensor Z value");

	/*
	 * dev.asmc.n.light
	 */
	if (model->smc_light_left) {
		sc->sc_light_tree = SYSCTL_ADD_NODE(sysctlctx,
		    SYSCTL_CHILDREN(sysctlnode), OID_AUTO, "light",
		    CTLFLAG_RD, 0, "Keyboard backlight sensors");
		
		SYSCTL_ADD_PROC(sysctlctx,
		    SYSCTL_CHILDREN(sc->sc_light_tree),
		    OID_AUTO, "left", CTLTYPE_INT | CTLFLAG_RW,
		    dev, 0, model->smc_light_left, "I",
		    "Keyboard backlight left sensor");
	
		SYSCTL_ADD_PROC(sysctlctx,
		    SYSCTL_CHILDREN(sc->sc_light_tree),
		    OID_AUTO, "right", CTLTYPE_INT | CTLFLAG_RW,
		    dev, 0, model->smc_light_right, "I",
		    "Keyboard backlight right sensor");
	}

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
#ifndef INTR_FILTER
	TASK_INIT(&sc->sc_sms_task, 0, asmc_sms_task, sc);
	sc->sc_sms_tq = taskqueue_create_fast("asmc_taskq", M_WAITOK,
	    taskqueue_thread_enqueue, &sc->sc_sms_tq);
	taskqueue_start_threads(&sc->sc_sms_tq, 1, PI_REALTIME, "%s sms taskq",
	    device_get_nameunit(dev));
#endif
	/*
	 * Allocate an IRQ for the SMS.
	 */
	sc->sc_rid = 0;
	sc->sc_res = bus_alloc_resource(dev, SYS_RES_IRQ, &sc->sc_rid,
	    ASMC_IRQ, ASMC_IRQ, 1, RF_ACTIVE);
	if (sc->sc_res == NULL) {
		device_printf(dev, "unable to allocate IRQ resource\n");
		ret = ENXIO;
		goto err2;
	}

	ret = bus_setup_intr(dev, sc->sc_res, 
	          INTR_TYPE_MISC | INTR_MPSAFE,
#ifdef INTR_FILTER
	    asmc_sms_intrfast, asmc_sms_handler,
#else
	    asmc_sms_intrfast, NULL,
#endif
	    dev, &sc->sc_cookie);

	if (ret) {
		device_printf(dev, "unable to setup SMS IRQ\n");
		goto err1;
	}
nosms:
	return (0);
err1:
	bus_release_resource(dev, SYS_RES_IRQ, sc->sc_rid, sc->sc_res);
err2:
	mtx_destroy(&sc->sc_mtx);
	if (sc->sc_sms_tq)
		taskqueue_free(sc->sc_sms_tq);

	return (ret);
}

static int
asmc_detach(device_t dev)
{
	struct asmc_softc *sc = device_get_softc(dev);

	if (sc->sc_sms_tq) {
		taskqueue_drain(sc->sc_sms_tq, &sc->sc_sms_task);
		taskqueue_free(sc->sc_sms_tq);
	}
	if (sc->sc_cookie)
		bus_teardown_intr(dev, sc->sc_res, sc->sc_cookie);
	if (sc->sc_res)
		bus_release_resource(dev, SYS_RES_IRQ, sc->sc_rid, sc->sc_res);
	mtx_destroy(&sc->sc_mtx);

	return (0);
}

static int
asmc_init(device_t dev)
{
	struct asmc_softc *sc = device_get_softc(dev);
	int i, error = 1;
	uint8_t buf[4];

	if (sc->sc_model->smc_sms_x == NULL)
		goto nosms;

	/*
	 * We are ready to recieve interrupts from the SMS.
	 */
	buf[0] = 0x01;
	asmc_key_write(dev, ASMC_KEY_INTOK, buf, 1);
	DELAY(50);

	/* 
	 * Initiate the polling intervals.
	 */
	buf[0] = 20; /* msecs */
	asmc_key_write(dev, ASMC_KEY_SMS_LOW_INT, buf, 1);
	DELAY(200);

	buf[0] = 20; /* msecs */
	asmc_key_write(dev, ASMC_KEY_SMS_HIGH_INT, buf, 1);
	DELAY(200);

	buf[0] = 0x00;
	buf[1] = 0x60;
	asmc_key_write(dev, ASMC_KEY_SMS_LOW, buf, 2);
	DELAY(200);

	buf[0] = 0x01;
	buf[1] = 0xc0;
	asmc_key_write(dev, ASMC_KEY_SMS_HIGH, buf, 2);
	DELAY(200);

	/*
	 * I'm not sure what this key does, but it seems to be
	 * required.
	 */
	buf[0] = 0x01;
	asmc_key_write(dev, ASMC_KEY_SMS_FLAG, buf, 1);
	DELAY(50);

	/*
	 * Wait up to 5 seconds for SMS initialization.
	 */
	for (i = 0; i < 10000; i++) {
		if (asmc_key_read(dev, ASMC_KEY_SMS, buf, 2) == 0 && 
		    (buf[0] != 0x00 || buf[1] != 0x00)) {
			error = 0;
			goto nosms;
		}
	
		buf[0] = ASMC_SMS_INIT1;
		buf[1] = ASMC_SMS_INIT2;
		asmc_key_write(dev, ASMC_KEY_SMS, buf, 2);
		DELAY(50);
	}

	asmc_sms_calibrate(dev);
nosms:
	sc->sc_nfan = asmc_fan_count(dev);
	if (sc->sc_nfan > ASMC_MAXFANS) {
		device_printf(dev, "more than %d fans were detected. Please "
		    "report this.\n", ASMC_MAXFANS);
		sc->sc_nfan = ASMC_MAXFANS;
	}

	if (bootverbose) {
		/*
		 * XXX: The number of keys is a 32 bit buffer, but
		 * right now Apple only uses the last 8 bit.
		 */
		asmc_key_read(dev, ASMC_NKEYS, buf, 4);
		device_printf(dev, "number of keys: %d\n", buf[3]);
	}	      

	return (error);
}

/*
 * We need to make sure that the SMC acks the byte sent.
 * Just wait up to 100 ms.
 */
static int
asmc_wait(device_t dev, uint8_t val)
{
	u_int i;

	val = val & ASMC_STATUS_MASK;

	for (i = 0; i < 1000; i++) {
		if ((inb(ASMC_CMDPORT) & ASMC_STATUS_MASK) == val)
			return (0);
		DELAY(10);
	}

	device_printf(dev, "%s failed: 0x%x, 0x%x\n", __func__, val,
		      inb(ASMC_CMDPORT));
	
	return (1);
}

static int
asmc_key_read(device_t dev, const char *key, uint8_t *buf, uint8_t len)
{
	int i, error = 1;
	struct asmc_softc *sc = device_get_softc(dev);

	mtx_lock_spin(&sc->sc_mtx);

	outb(ASMC_CMDPORT, ASMC_CMDREAD);
	if (asmc_wait(dev, 0x0c))
		goto out;

	for (i = 0; i < 4; i++) {
		outb(ASMC_DATAPORT, key[i]);
		if (asmc_wait(dev, 0x04))
			goto out;
	}

	outb(ASMC_DATAPORT, len);

	for (i = 0; i < len; i++) {
		if (asmc_wait(dev, 0x05))
			goto out;
		buf[i] = inb(ASMC_DATAPORT);
	}

	error = 0;
out:
	mtx_unlock_spin(&sc->sc_mtx);

	return (error);
}

static int
asmc_key_write(device_t dev, const char *key, uint8_t *buf, uint8_t len)
{
	int i, error = -1;
	struct asmc_softc *sc = device_get_softc(dev);

	mtx_lock_spin(&sc->sc_mtx);

	outb(ASMC_CMDPORT, ASMC_CMDWRITE);
	if (asmc_wait(dev, 0x0c))
		goto out;

	for (i = 0; i < 4; i++) {
		outb(ASMC_DATAPORT, key[i]);
		if (asmc_wait(dev, 0x04))
			goto out;
	}

	outb(ASMC_DATAPORT, len);

	for (i = 0; i < len; i++) {
		if (asmc_wait(dev, 0x04))
			goto out;
		outb(ASMC_DATAPORT, buf[i]);
	}

	error = 0;
out:
	mtx_unlock_spin(&sc->sc_mtx);

	return (error);

}

/*
 * Fan control functions.
 */
static int
asmc_fan_count(device_t dev)
{
	uint8_t buf[1];

	if (asmc_key_read(dev, ASMC_KEY_FANCOUNT, buf, 1) < 0)
		return (-1);

	return (buf[0]);
}

static int
asmc_fan_getvalue(device_t dev, const char *key, int fan)
{
	int speed;
	uint8_t buf[2];
	char fankey[5];

	snprintf(fankey, sizeof(fankey), key, fan);
	if (asmc_key_read(dev, fankey, buf, 2) < 0)
		return (-1);
	speed = (buf[0] << 6) | (buf[1] >> 2);

	return (speed);
}

static int
asmc_mb_sysctl_fanspeed(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t) arg1;
	int fan = arg2;
	int error;
	int32_t v;

	v = asmc_fan_getvalue(dev, ASMC_KEY_FANSPEED, fan);
	error = sysctl_handle_int(oidp, &v, 0, req);

	return (error);
}

static int
asmc_mb_sysctl_fansafespeed(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t) arg1;
	int fan = arg2;
	int error;
	int32_t v;

	v = asmc_fan_getvalue(dev, ASMC_KEY_FANSAFESPEED, fan);
	error = sysctl_handle_int(oidp, &v, 0, req);

	return (error);
}


static int
asmc_mb_sysctl_fanminspeed(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t) arg1;
	int fan = arg2;
	int error;
	int32_t v;

	v = asmc_fan_getvalue(dev, ASMC_KEY_FANMINSPEED, fan);
	error = sysctl_handle_int(oidp, &v, 0, req);

	return (error);
}

static int
asmc_mb_sysctl_fanmaxspeed(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t) arg1;
	int fan = arg2;
	int error;
	int32_t v;

	v = asmc_fan_getvalue(dev, ASMC_KEY_FANMAXSPEED, fan);
	error = sysctl_handle_int(oidp, &v, 0, req);

	return (error);
}

static int
asmc_mb_sysctl_fantargetspeed(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t) arg1;
	int fan = arg2;
	int error;
	int32_t v;

	v = asmc_fan_getvalue(dev, ASMC_KEY_FANTARGETSPEED, fan);
	error = sysctl_handle_int(oidp, &v, 0, req);

	return (error);
}

/*
 * Temperature functions.
 */
static int
asmc_temp_getvalue(device_t dev, const char *key)
{
	uint8_t buf[2];

	/*
	 * Check for invalid temperatures.
	 */
	if (asmc_key_read(dev, key, buf, 2) < 0)
		return (-1);

	return (buf[0]);
}

static int
asmc_temp_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t) arg1;
	struct asmc_softc *sc = device_get_softc(dev);
	int error, val;

	val = asmc_temp_getvalue(dev, sc->sc_model->smc_temps[arg2]);
	error = sysctl_handle_int(oidp, &val, 0, req);

	return (error);
}

/*
 * Sudden Motion Sensor functions.
 */
static int
asmc_sms_read(device_t dev, const char *key, int16_t *val)
{
	uint8_t buf[2];
	int error;

	/* no need to do locking here as asmc_key_read() already does it */ 
	switch (key[3]) {
	case 'X':
	case 'Y':
	case 'Z':
		error =	asmc_key_read(dev, key, buf, 2);
		break;
	default:
		device_printf(dev, "%s called with invalid argument %s\n",
			      __func__, key);
		error = 1;
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
	device_t dev = (device_t) arg;
	struct asmc_softc *sc = device_get_softc(dev);

	mtx_lock_spin(&sc->sc_mtx);
	type = inb(ASMC_INTPORT);
	mtx_unlock_spin(&sc->sc_mtx);

	sc->sc_sms_intrtype = type;
	asmc_sms_printintr(dev, type);

#ifdef INTR_FILTER
	return (FILTER_SCHEDULE_THREAD | FILTER_HANDLED);
#else
	taskqueue_enqueue(sc->sc_sms_tq, &sc->sc_sms_task);
#endif
	return (FILTER_HANDLED);
}

#ifdef INTR_FILTER
static void
asmc_sms_handler(void *arg)
{
	struct asmc_softc *sc = device_get_softc(arg);
	
	asmc_sms_task(sc, 0);
}
#endif


static void
asmc_sms_printintr(device_t dev, uint8_t type)
{

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
	default:
		device_printf(dev, "%s unknown interrupt\n", __func__);
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
	devctl_notify("ISA", "asmc", "SMS", notify); 
}

static int
asmc_mb_sysctl_sms_x(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t) arg1;
	int error;
	int16_t val;
	int32_t v;

	asmc_sms_read(dev, ASMC_KEY_SMS_X, &val);
	v = (int32_t) val;
	error = sysctl_handle_int(oidp, &v, 0, req);

	return (error);
}

static int
asmc_mb_sysctl_sms_y(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t) arg1;
	int error;
	int16_t val;
	int32_t v;

	asmc_sms_read(dev, ASMC_KEY_SMS_Y, &val);
	v = (int32_t) val;
	error = sysctl_handle_int(oidp, &v, 0, req);

	return (error);
}

static int
asmc_mb_sysctl_sms_z(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t) arg1;
	int error;
	int16_t val;
	int32_t v;

	asmc_sms_read(dev, ASMC_KEY_SMS_Z, &val);
	v = (int32_t) val;
	error = sysctl_handle_int(oidp, &v, sizeof(v), req);

	return (error);
}

static int
asmc_mbp_sysctl_light_left(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t) arg1;
	uint8_t buf[6];
	int error;
	unsigned int level;
	int32_t v;

	asmc_key_read(dev, ASMC_KEY_LIGHTLEFT, buf, 6);
	v = buf[2];
	error = sysctl_handle_int(oidp, &v, sizeof(v), req);
	if (error == 0 && req->newptr != NULL) {
		level = *(unsigned int *)req->newptr;
		if (level > 255)
			return (EINVAL);
		buf[0] = level;
		buf[1] = 0x00;
		asmc_key_write(dev, ASMC_KEY_LIGHTVALUE, buf, 2);
	}

	return (error);
}

static int
asmc_mbp_sysctl_light_right(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t) arg1;
	uint8_t buf[6];
	int error;
	unsigned int level;
	int32_t v;
	
	asmc_key_read(dev, ASMC_KEY_LIGHTRIGHT, buf, 6);
	v = buf[2];
	error = sysctl_handle_int(oidp, &v, sizeof(v), req);
	if (error == 0 && req->newptr != NULL) {
		level = *(unsigned int *)req->newptr;
		if (level > 255)
			return (EINVAL);
		buf[0] = level;
		buf[1] = 0x00;
		asmc_key_write(dev, ASMC_KEY_LIGHTVALUE, buf, 2);
	}
	
	return (error);
}
