/*-
 * Copyright (c) 2018 Stormshield.
 * Copyright (c) 2018 Semihalf.
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
 */

#include <sys/cdefs.h>
#include <sys/param.h>

#include <machine/atomic.h>

#include "tpm20.h"
#include "tpm_irq.h"
#include "tpm_if.h"

/*
 * TIS register space as defined in
 * TCG_PC_Client_Platform_TPM_Profile_PTP_2.0_r1.03_v22
 */
#define	TPM_ACCESS			0x0
#define	TPM_INT_ENABLE			0x8
#define	TPM_INT_VECTOR			0xc
#define	TPM_INT_STS			0x10
#define	TPM_INTF_CAPS			0x14
#define	TPM_STS				0x18
#define	TPM_DATA_FIFO			0x24
#define	TPM_INTF_ID			0x30
#define	TPM_XDATA_FIFO			0x80
#define	TPM_DID_VID			0xF00
#define	TPM_RID				0xF04

#define	TPM_ACCESS_LOC_REQ		BIT(1)
#define	TPM_ACCESS_LOC_Seize		BIT(3)
#define	TPM_ACCESS_LOC_ACTIVE		BIT(5)
#define	TPM_ACCESS_LOC_RELINQUISH	BIT(5)
#define	TPM_ACCESS_VALID		BIT(7)

#define	TPM_INT_ENABLE_GLOBAL_ENABLE	BIT(31)
#define	TPM_INT_ENABLE_CMD_RDY		BIT(7)
#define	TPM_INT_ENABLE_LOC_CHANGE	BIT(2)
#define	TPM_INT_ENABLE_STS_VALID	BIT(1)
#define	TPM_INT_ENABLE_DATA_AVAIL	BIT(0)

#define	TPM_INT_STS_CMD_RDY		BIT(7)
#define	TPM_INT_STS_LOC_CHANGE		BIT(2)
#define	TPM_INT_STS_VALID		BIT(1)
#define	TPM_INT_STS_DATA_AVAIL		BIT(0)

#define	TPM_INTF_CAPS_VERSION		0x70000000
#define	TPM_INTF_CAPS_TPM20		0x30000000

#define	TPM_STS_VALID			BIT(7)
#define	TPM_STS_CMD_RDY			BIT(6)
#define	TPM_STS_CMD_START		BIT(5)
#define	TPM_STS_DATA_AVAIL		BIT(4)
#define	TPM_STS_DATA_EXPECTED		BIT(3)
#define	TPM_STS_BURST_MASK		0xFFFF00
#define	TPM_STS_BURST_OFFSET		0x8

static int tpmtis_transmit(device_t dev, struct tpm_priv *priv, size_t length);

static int tpmtis_attach_finish(struct tpm_sc *sc);
static void tpmtis_delayed_attach(void *arg);
static int tpmtis_detach(device_t dev);
static int tpmtis_suspend(device_t dev);

static void tpmtis_intr_handler(void *arg);
static void tpmtis_intr_task(void *arg, int pending);
static int tpmtis_fallback(struct tpm_sc *sc);
static void tpmtis_arm_intr(struct tpm_sc *sc);
static bool tpmtis_mask_intr(struct tpm_sc *sc);
static void tpmtis_finish_intr(struct tpm_sc *sc);

static bool tpmtis_program_intr(struct tpm_sc *sc, bool enable);
static int tpmtis_setup_intr(struct tpm_sc *sc);
static int tpmtis_resume(device_t dev);

static bool tpmtis_read_bytes(struct tpm_sc *sc, size_t count, uint8_t *buf);
static bool tpmtis_write_bytes(struct tpm_sc *sc, size_t count, uint8_t *buf);
static bool tpmtis_request_locality(struct tpm_sc *sc, int locality);
static void tpmtis_relinquish_locality(struct tpm_sc *sc);
static bool tpmtis_go_ready(struct tpm_sc *sc);

static bool tpm_wait_for_reg(struct tpm_sc *sc, bus_size_t off,
    uint32_t mask, uint32_t val, int32_t timeout, int intr_type,
    bool reg32);

static uint16_t tpmtis_wait_for_burst(struct tpm_sc *sc);

int
tpmtis_attach(device_t dev)
{
	struct tpm_sc *sc;
	void *intr_cookie;
	bool configured;
	int result;
	int poll = 0;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->intr_hook.ich_state = ICHS_DONE;
	sc->intr_generation = 0;
	sc->interrupts = false;
	sc->intr_cookie = NULL;
	sc->irq_res = NULL;
	sc->intr_testing = false;
	sc->intr_configuring = false;
	sc->intr_fault = false;
	sc->intr_armed = false;
	sc->intr_received = false;
	sc->locality = false;
	sc->irq_config.valid = false;
	sc->dying = false;
	TASK_INIT(&sc->intr_task, 0, tpmtis_intr_task, sc);

	sx_init(&sc->dev_lock, "TPM driver lock");
	sx_init(&sc->intr_io_lock, "TPM interrupt I/O lock");
	mtx_init(&sc->intr_lock, "TPM interrupt lock", NULL, MTX_DEF);
	cv_init(&sc->intr_cv, "tpmtis_intr");

	/*
	 * Firmware may have left interrupt delivery enabled.  Quiesce the TPM
	 * before installing a handler or starting the entropy harvester, even
	 * when we will use polling or cannot use the advertised IRQ.
	 */
	sx_xlock(&sc->dev_lock);
	configured = tpmtis_program_intr(sc, false);
	if (!configured) {
		sx_xunlock(&sc->dev_lock);
		device_printf(dev, "failed to quiesce TPM interrupts\n");
		tpmtis_detach(dev);
		return (ENXIO);
	}

	resource_int_value("tpm", device_get_unit(dev), "use_polling", &poll);
	if (poll != 0) {
		device_printf(dev,
		    "Using poll method to get TPM operation status\n");
		goto skip_irq;
	}

	sc->irq_rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &sc->irq_rid,
		    RF_ACTIVE);
	if (sc->irq_res == NULL) {
		device_printf(dev, "IRQ resource unavailable; using polling\n");
		goto skip_irq;
	}
	if (TPM_CONFIGURE_IRQ(dev, sc->irq_res, &sc->irq_config, false) != 0 ||
	    !tpm_irq_supported(TPM_READ_4(dev, TPM_INTF_CAPS), &sc->irq_config))
		goto release_irq;

	/* Recovery can sleep with the host source masked; sharing is unsafe. */
	/* Failed setup may leave a stale cookie after removing its handler. */
	intr_cookie = NULL;
	result = bus_setup_intr(dev, sc->irq_res,
		    INTR_TYPE_MISC | INTR_MPSAFE | INTR_EXCL | INTR_SLEEPABLE,
		    NULL, tpmtis_intr_handler, sc, &intr_cookie);
	if (result == 0) {
		sc->intr_cookie = intr_cookie;
		goto skip_irq;
	}
release_irq:
	device_printf(dev, "IRQ route unavailable; using polling\n");
	if (bus_release_resource(dev, SYS_RES_IRQ, sc->irq_rid,
	    sc->irq_res) == 0)
		sc->irq_res = NULL;
skip_irq:
	/* IRQ validation needs timers and a runnable interrupt thread. */
	if (cold && sc->intr_cookie != NULL) {
		sx_xunlock(&sc->dev_lock);
		sc->intr_hook.ich_func = tpmtis_delayed_attach;
		sc->intr_hook.ich_arg = sc;
		if (config_intrhook_establish(&sc->intr_hook) == 0)
			return (0);
		sc->intr_hook.ich_state = ICHS_DONE;
		result = ENXIO;
	} else {
		result = tpmtis_attach_finish(sc);
		sx_xunlock(&sc->dev_lock);
	}
	if (result != 0) {
		tpmtis_detach(dev);
		return (result);
	}
	return (0);
}

static int
tpmtis_attach_finish(struct tpm_sc *sc)
{

	sx_assert(&sc->dev_lock, SA_XLOCKED);
	/* Keep fault cleanup out until validation and publication finish. */
	if (tpmtis_setup_intr(sc) != 0)
		return (ENXIO);
	return (tpm20_init(sc));
}

static void
tpmtis_delayed_attach(void *arg)
{
	struct tpm_sc *sc = arg;
	int error;

	sx_xlock(&sc->dev_lock);
	error = tpmtis_attach_finish(sc);
	if (error != 0) {
		mtx_lock(&sc->intr_lock);
		atomic_store_bool(&sc->dying, true);
		mtx_unlock(&sc->intr_lock);
		device_printf(sc->dev, "deferred attach failed: %d\n", error);
	}
	sx_xunlock(&sc->dev_lock);
	config_intrhook_disestablish(&sc->intr_hook);
}

/* Deferred initialization can fail before the harvester is initialized. */
static int
tpmtis_suspend(device_t dev)
{
	struct tpm_sc *sc = device_get_softc(dev);

	if (!sc->common_initialized)
		return (0);
	return (tpm20_suspend(dev));
}

static int
tpmtis_detach(device_t dev)
{
	struct tpm_sc *sc;

	sc = device_get_softc(dev);
	config_intrhook_drain(&sc->intr_hook);
	/*
	 * Stop new commands before quiescing hardware with the lock still live.
	 */
	mtx_lock(&sc->intr_lock);
	atomic_store_bool(&sc->dying, true);
	mtx_unlock(&sc->intr_lock);
	taskqueue_drain(taskqueue_thread, &sc->intr_task);
	sx_xlock(&sc->dev_lock);
	mtx_lock(&sc->intr_lock);
	sc->interrupts = false;
	mtx_unlock(&sc->intr_lock);
	if (!tpmtis_program_intr(sc, false))
		device_printf(dev,
		    "failed to quiesce TPM interrupts during detach\n");
	sx_xunlock(&sc->dev_lock);
	tpm20_release(sc);

	if (sc->intr_cookie != NULL)
		bus_teardown_intr(dev, sc->irq_res, sc->intr_cookie);
	cv_destroy(&sc->intr_cv);
	mtx_destroy(&sc->intr_lock);
	sx_destroy(&sc->intr_io_lock);

	if (sc->irq_res != NULL)
		bus_release_resource(dev, SYS_RES_IRQ,
		    sc->irq_rid, sc->irq_res);

	if (sc->mem_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    sc->mem_rid, sc->mem_res);

	return (0);
}

/* Called with intr_lock held; the worker may wait for the command lock. */
static void
tpmtis_intr_failed(struct tpm_sc *sc)
{

	sc->interrupts = false;
	if (!sc->intr_fault && !sc->dying) {
		sc->intr_fault = true;
		taskqueue_enqueue(taskqueue_thread, &sc->intr_task);
	}
	cv_broadcast(&sc->intr_cv);
}

/* The caller owns locality and serializes register I/O with the handler. */
static bool
tpmtis_mask_intr(struct tpm_sc *sc)
{
	uint32_t reg;
	bool disabled;

	sx_assert(&sc->intr_io_lock, SA_XLOCKED);
	reg = TPM_READ_4(sc->dev, TPM_INT_ENABLE);
	TPM_WRITE_4(sc->dev, TPM_INT_ENABLE,
	    reg & ~(TPM_INT_ENABLE_GLOBAL_ENABLE | TPM_IRQ_EVENTS));
	TPM_WRITE_BARRIER(sc->dev, TPM_INT_ENABLE, 4);
	disabled = (TPM_READ_4(sc->dev, TPM_INT_ENABLE) &
	    TPM_INT_ENABLE_GLOBAL_ENABLE) == 0;
	reg = TPM_READ_4(sc->dev, TPM_INT_STS);
	TPM_WRITE_4(sc->dev, TPM_INT_STS, reg);
	TPM_WRITE_BARRIER(sc->dev, TPM_INT_STS, 4);
	return (disabled);
}

static bool
tpmtis_program_intr(struct tpm_sc *sc, bool enable)
{
	struct tpm_irq_config config;
	bool configured;
	int error;

	sx_assert(&sc->dev_lock, SA_XLOCKED);
	if (!tpmtis_request_locality(sc, 0)) {
		/* Interrupt enable can be read without owning locality. */
		return (!enable && (TPM_READ_4(sc->dev, TPM_INT_ENABLE) &
		    TPM_INT_ENABLE_GLOBAL_ENABLE) == 0);
	}

	sx_xlock(&sc->intr_io_lock);
	configured = tpmtis_mask_intr(sc);
	if (configured && enable) {
		KASSERT(sc->intr_cookie != NULL,
		    ("TPM interrupts configured without a handler"));
		config = sc->irq_config;
		mtx_lock(&sc->intr_lock);
		sc->intr_configuring = true;
		mtx_unlock(&sc->intr_lock);
		error = TPM_CONFIGURE_IRQ(sc->dev, sc->irq_res, &config, true);
		/* Firmware may have changed interrupt enables in _SRS. */
		configured = tpmtis_mask_intr(sc);
		mtx_lock(&sc->intr_lock);
		sc->intr_configuring = false;
		mtx_unlock(&sc->intr_lock);
		if (error != 0 || !tpm_irq_supported(
		    TPM_READ_4(sc->dev, TPM_INTF_CAPS), &config))
			configured = false;
		if (configured) {
			sc->irq_config = config;
			if (config.sirq != TPM_IRQ_SIRQ_UNCHANGED)
				TPM_WRITE_1(sc->dev, TPM_INT_VECTOR,
				    config.sirq);
			sc->intr_mask = config.mode | TPM_INT_ENABLE_DATA_AVAIL;
			TPM_WRITE_4(sc->dev, TPM_INT_ENABLE, config.mode);
			TPM_WRITE_BARRIER(sc->dev, TPM_INT_ENABLE, 4);
			configured = (TPM_READ_4(sc->dev, TPM_INT_ENABLE) &
			    (TPM_INT_ENABLE_GLOBAL_ENABLE |
			    TPM_IRQ_MODE_MASK)) == config.mode;
		}
	}
	sx_xunlock(&sc->intr_io_lock);
	tpmtis_relinquish_locality(sc);
	return (configured);
}

static int
tpmtis_fallback(struct tpm_sc *sc)
{
	bool quiesced;

	sx_assert(&sc->dev_lock, SA_XLOCKED);
	mtx_lock(&sc->intr_lock);
	sc->interrupts = false;
	sc->intr_testing = false;
	sc->intr_armed = false;
	mtx_unlock(&sc->intr_lock);
	quiesced = tpmtis_program_intr(sc, false);
	if (!quiesced) {
		sc->suspended = true;
		device_printf(sc->dev,
		    "failed to quiesce TPM interrupts; suspending device\n");
	}
	if (sc->intr_cookie != NULL) {
		bus_teardown_intr(sc->dev, sc->irq_res, sc->intr_cookie);
		sc->intr_cookie = NULL;
		if (quiesced)
			device_printf(sc->dev,
			    "interrupts unavailable; using polling\n");
	}
	if (sc->irq_res != NULL && bus_release_resource(sc->dev, SYS_RES_IRQ,
	    sc->irq_rid, sc->irq_res) == 0)
		sc->irq_res = NULL;
	return (quiesced ? 0 : EIO);
}

static void
tpmtis_intr_task(void *arg, int pending)
{
	struct tpm_sc *sc = arg;

	sx_xlock(&sc->dev_lock);
	if (!sc->dying)
		tpmtis_fallback(sc);
	sx_xunlock(&sc->dev_lock);
}

/* Require a complete tagged response and a fresh DATA_AVAIL interrupt. */
static int
tpmtis_setup_intr(struct tpm_sc *sc)
{
	struct tpm_priv *priv;
	bool working;
	int error;
	static const uint8_t cmd[] = {
		0x80, 0x01,		/* TPM_ST_NO_SESSIONS */
		0x00, 0x00, 0x00, 0x0c,	/* Command length */
		0x00, 0x00, 0x01, 0x7b,	/* TPM_CC_GetRandom */
		0x00, 0x01		/* One byte */
	};

	sx_assert(&sc->dev_lock, SA_XLOCKED);
	if (sc->intr_cookie == NULL)
		return (0);
	priv = malloc(sizeof(*priv), M_TPM20, M_WAITOK | M_ZERO);
	mtx_lock(&sc->intr_lock);
	sc->interrupts = false;
	working = !sc->intr_fault && !sc->dying;
	mtx_unlock(&sc->intr_lock);
	if (!working || !tpmtis_program_intr(sc, true))
		goto fallback;
	mtx_lock(&sc->intr_lock);
	sc->intr_testing = true;
	sc->intr_received = false;
	mtx_unlock(&sc->intr_lock);
	memcpy(priv->buf, cmd, sizeof(cmd));
	error = tpmtis_transmit(sc->dev, priv, sizeof(cmd));
	mtx_lock(&sc->intr_lock);
	sc->intr_testing = false;
	working = error == 0 && be16dec(priv->buf) == 0x8001 &&
	    sc->intr_received && !sc->intr_fault;
	sc->interrupts = working;
	mtx_unlock(&sc->intr_lock);
	if (working) {
		device_printf(sc->dev, "using verified interrupts\n");
		error = 0;
		goto out;
	}
fallback:
	error = tpmtis_fallback(sc);
out:
	free(priv, M_TPM20);
	return (error);
}

static int
tpmtis_resume(device_t dev)
{
	struct tpm_sc *sc;
	bool quiesced;
	int error;

	sc = device_get_softc(dev);
	if (!sc->common_initialized)
		return (ENXIO);
	sx_xlock(&sc->dev_lock);
	mtx_lock(&sc->intr_lock);
	sc->interrupts = false;
	mtx_unlock(&sc->intr_lock);
	quiesced = tpmtis_program_intr(sc, false);
	sx_xunlock(&sc->dev_lock);
	if (!quiesced) {
		device_printf(dev,
		    "failed to quiesce interrupts during resume\n");
		return (EIO);
	}
	/* Complete Startup through polling before testing the route again. */
	error = tpm20_resume(dev);
	if (error == 0) {
		sx_xlock(&sc->dev_lock);
		error = tpmtis_setup_intr(sc);
		sx_xunlock(&sc->dev_lock);
	}
	return (error);
}

static void
tpmtis_arm_intr(struct tpm_sc *sc)
{
	bool enable;

	sx_xlock(&sc->intr_io_lock);
	mtx_lock(&sc->intr_lock);
	enable = (sc->interrupts || sc->intr_testing) && !sc->intr_fault;
	sc->intr_received = false;
	mtx_unlock(&sc->intr_lock);
	if (enable) {
		/* Stale response status must not validate this command. */
		if (tpmtis_mask_intr(sc) && (TPM_READ_4(sc->dev, TPM_INT_STS) &
		    TPM_INT_STS_DATA_AVAIL) == 0) {
			mtx_lock(&sc->intr_lock);
			sc->intr_armed = true;
			mtx_unlock(&sc->intr_lock);
			TPM_WRITE_4(sc->dev, TPM_INT_ENABLE,
			    sc->intr_mask | TPM_INT_ENABLE_GLOBAL_ENABLE);
			TPM_WRITE_BARRIER(sc->dev, TPM_INT_ENABLE, 4);
		} else {
			mtx_lock(&sc->intr_lock);
			tpmtis_intr_failed(sc);
			mtx_unlock(&sc->intr_lock);
		}
	}
	sx_xunlock(&sc->intr_io_lock);
}

/* Give an already pending handler time to run before consuming its response. */
static void
tpmtis_finish_intr(struct tpm_sc *sc)
{
	sbintime_t deadline;

	deadline = sbinuptime() + mstosbt(TPM_IRQ_GRACE_MS);
	mtx_lock(&sc->intr_lock);
	while (sc->intr_armed && !sc->intr_received && !sc->intr_fault) {
		if (cv_timedwait_sbt(&sc->intr_cv, &sc->intr_lock,
		    deadline, 0, C_ABSOLUTE | C_HARDCLOCK) == EWOULDBLOCK) {
			tpmtis_intr_failed(sc);
			break;
		}
	}
	mtx_unlock(&sc->intr_lock);
}

static void
tpmtis_intr_handler(void *arg)
{
	struct tpm_sc *sc = arg;
	uint32_t status, enable;
	bool disabled;

	/* Register access may sleep on SPI; do not hold intr_lock across it. */
	sx_xlock(&sc->intr_io_lock);
	status = TPM_READ_4(sc->dev, TPM_INT_STS);
	enable = TPM_READ_4(sc->dev, TPM_INT_ENABLE);
	if ((enable & TPM_INT_ENABLE_GLOBAL_ENABLE) == 0 ||
	    (status & enable & TPM_IRQ_EVENTS) == 0) {
		sx_xunlock(&sc->intr_io_lock);
		return;
	}

	/* Mask on the first event so a stuck source cannot retrigger. */
	disabled = sc->locality && tpmtis_mask_intr(sc);
	mtx_lock(&sc->intr_lock);
	if (disabled && sc->intr_armed &&
	    (status & TPM_INT_STS_DATA_AVAIL) != 0) {
		sc->intr_received = true;
		sc->intr_generation++;
	} else if (!disabled || !sc->intr_configuring) {
		tpmtis_intr_failed(sc);
	}
	sc->intr_armed = false;
	cv_broadcast(&sc->intr_cv);
	mtx_unlock(&sc->intr_lock);
	sx_xunlock(&sc->intr_io_lock);
	/* Keep the host source masked while command and cleanup threads run. */
	if (!disabled)
		pause("tpmirq", 1);
}

static uint32_t
tpmtis_read_wait_reg(struct tpm_sc *sc, bus_size_t off, bool reg32)
{

	if (reg32)
		return (TPM_READ_4(sc->dev, off));
	return (TPM_READ_1(sc->dev, off));
}

static bool
tpm_wait_for_reg(struct tpm_sc *sc, bus_size_t off, uint32_t mask,
    uint32_t val, int32_t timeout, int intr_type, bool reg32)
{
	sbintime_t deadline, wakeup;
	uint32_t generation;
	bool interrupts, result;

	sx_assert(&sc->dev_lock, SA_XLOCKED);
	deadline = sbinuptime() + ustosbt(timeout);
	mtx_lock(&sc->intr_lock);
	generation = sc->intr_generation;
	interrupts = sc->interrupts && intr_type == TPM_INT_STS_DATA_AVAIL;
	mtx_unlock(&sc->intr_lock);

	for (;;) {
		result = (tpmtis_read_wait_reg(sc, off, reg32) & mask) == val;
		if (result)
			break;
		if (sbinuptime() >= deadline)
			break;
		if (!interrupts) {
			pause("TPM POLLING", 1);
			mtx_lock(&sc->intr_lock);
			generation = sc->intr_generation;
			interrupts = sc->interrupts &&
			    intr_type == TPM_INT_STS_DATA_AVAIL;
			mtx_unlock(&sc->intr_lock);
			continue;
		}

		/*
		 * Register access may sleep for a SPI TPM, so evaluate the
		 * predicate without intr_lock.  The generation check closes the
		 * resulting window before cv_timedwait_sbt() atomically sleeps.
		 */
		mtx_lock(&sc->intr_lock);
		wakeup = MIN(deadline, sbinuptime() + mstosbt(TPM_IRQ_POLL_MS));
		if (generation == sc->intr_generation)
			(void)cv_timedwait_sbt(&sc->intr_cv, &sc->intr_lock,
			    wakeup, 0, C_ABSOLUTE | C_HARDCLOCK);
		generation = sc->intr_generation;
		interrupts = sc->interrupts &&
		    intr_type == TPM_INT_STS_DATA_AVAIL;
		mtx_unlock(&sc->intr_lock);
	}

	mtx_lock(&sc->intr_lock);
	if (!result && intr_type == TPM_INT_STS_DATA_AVAIL && sc->intr_armed)
		tpmtis_intr_failed(sc);
	mtx_unlock(&sc->intr_lock);
	return (result);
}

static uint16_t
tpmtis_wait_for_burst(struct tpm_sc *sc)
{
	int timeout;
	uint16_t burst_count;

	timeout = TPM_TIMEOUT_A;

	while (timeout-- > 0) {
		burst_count = (TPM_READ_4(sc->dev, TPM_STS) & TPM_STS_BURST_MASK) >>
		    TPM_STS_BURST_OFFSET;
		if (burst_count > 0)
			break;

		DELAY(1);
	}
	return (burst_count);
}

static bool
tpmtis_read_bytes(struct tpm_sc *sc, size_t count, uint8_t *buf)
{
	uint16_t burst_count;

	while (count > 0) {
		burst_count = tpmtis_wait_for_burst(sc);
		if (burst_count == 0)
			return (false);

		burst_count = MIN(burst_count, count);
		count -= burst_count;

		while (burst_count-- > 0)
			*buf++ = TPM_READ_1(sc->dev, TPM_DATA_FIFO);
	}

	return (true);
}

static bool
tpmtis_write_bytes(struct tpm_sc *sc, size_t count, uint8_t *buf)
{
	uint16_t burst_count;

	while (count > 0) {
		burst_count = tpmtis_wait_for_burst(sc);
		if (burst_count == 0)
			return (false);

		burst_count = MIN(burst_count, count);
		count -= burst_count;

		while (burst_count-- > 0)
			TPM_WRITE_1(sc->dev, TPM_DATA_FIFO, *buf++);
	}

	return (true);
}

static bool
tpmtis_request_locality(struct tpm_sc *sc, int locality)
{
	uint8_t mask;

	sx_assert(&sc->dev_lock, SA_XLOCKED);
	/* Currently we only support Locality 0 */
	if (locality != 0)
		return (false);

	mask = TPM_ACCESS_LOC_ACTIVE | TPM_ACCESS_VALID;

	TPM_WRITE_1(sc->dev, TPM_ACCESS, TPM_ACCESS_LOC_REQ);
	TPM_WRITE_BARRIER(sc->dev, TPM_ACCESS, 1);
	if (tpm_wait_for_reg(sc, TPM_ACCESS, mask, mask, TPM_TIMEOUT_A,
	    TPM_INT_STS_LOC_CHANGE, false)) {
		sx_xlock(&sc->intr_io_lock);
		sc->locality = true;
		sx_xunlock(&sc->intr_io_lock);
		return (true);
	}

	/* Cancel the request so a later grant cannot leave locality owned. */
	TPM_WRITE_1(sc->dev, TPM_ACCESS, TPM_ACCESS_LOC_RELINQUISH);
	TPM_WRITE_BARRIER(sc->dev, TPM_ACCESS, 1);
	return (false);
}

static void
tpmtis_relinquish_locality(struct tpm_sc *sc)
{
	bool disabled;

	sx_assert(&sc->dev_lock, SA_XLOCKED);
	/* No delivery or handler register writes after releasing locality. */
	sx_xlock(&sc->intr_io_lock);
	disabled = tpmtis_mask_intr(sc);
	mtx_lock(&sc->intr_lock);
	sc->intr_armed = false;
	if (!disabled)
		tpmtis_intr_failed(sc);
	mtx_unlock(&sc->intr_lock);
	TPM_WRITE_1(sc->dev, TPM_ACCESS, TPM_ACCESS_LOC_RELINQUISH);
	TPM_WRITE_BARRIER(sc->dev, TPM_ACCESS, 1);
	sc->locality = false;
	sx_xunlock(&sc->intr_io_lock);
}

static bool
tpmtis_go_ready(struct tpm_sc *sc)
{
	uint32_t mask;

	mask = TPM_STS_CMD_RDY;

	TPM_WRITE_4(sc->dev, TPM_STS, TPM_STS_CMD_RDY);
	TPM_WRITE_BARRIER(sc->dev, TPM_STS, 4);
	if (!tpm_wait_for_reg(sc, TPM_STS, mask, mask, TPM_TIMEOUT_B,
	    TPM_INT_STS_CMD_RDY, true))
		return (false);

	return (true);
}

static int
tpmtis_transmit(device_t dev, struct tpm_priv *priv, size_t length)
{
	struct tpm_sc *sc;
	size_t bytes_available;
	uint32_t mask, curr_cmd;
	int error, timeout;
	bool locality;

	sc = device_get_softc(dev);
	sx_assert(&sc->dev_lock, SA_XLOCKED);
	locality = false;

	if (!tpmtis_request_locality(sc, 0)) {
		device_printf(dev,
		    "Failed to obtain locality\n");
		return (EIO);
	}
	locality = true;
	if (!tpmtis_go_ready(sc)) {
		device_printf(dev,
		    "Failed to switch to ready state\n");
		error = EIO;
		goto out;
	}
	if (!tpmtis_write_bytes(sc, length, priv->buf)) {
		device_printf(dev,
		    "Failed to write cmd to device\n");
		error = EIO;
		goto out;
	}

	mask = TPM_STS_VALID;
	if (!tpm_wait_for_reg(sc, TPM_STS, mask, mask, TPM_TIMEOUT_C,
	    TPM_INT_STS_VALID, true)) {
		device_printf(dev,
		    "Timeout while waiting for valid bit\n");
		error = EIO;
		goto out;
	}
	if (TPM_READ_4(dev, TPM_STS) & TPM_STS_DATA_EXPECTED) {
		device_printf(dev,
		    "Device expects more data even though we already"
		    " sent everything we had\n");
		error = EIO;
		goto out;
	}

	/*
	 * Calculate timeout for current command.
	 * Command code is passed in bytes 6-10.
	 */
	curr_cmd = be32toh(*(uint32_t *) (&priv->buf[6]));
	timeout = tpm20_get_timeout(curr_cmd);

	tpmtis_arm_intr(sc);
	TPM_WRITE_4(dev, TPM_STS, TPM_STS_CMD_START);
	TPM_WRITE_BARRIER(dev, TPM_STS, 4);

	mask = TPM_STS_DATA_AVAIL | TPM_STS_VALID;
	if (!tpm_wait_for_reg(sc, TPM_STS, mask, mask, timeout,
	    TPM_INT_STS_DATA_AVAIL, true)) {
		device_printf(dev,
		    "Timeout while waiting for device to process cmd\n");
		/*
		 * Switching to ready state also cancels processing
		 * current command
		 */
		if (!tpmtis_go_ready(sc)) {
			error = EIO;
			goto out;
		}

		/*
		 * After canceling a command we should get a response,
		 * check if there is one.
		 */
		if (!tpm_wait_for_reg(sc, TPM_STS, mask, mask, TPM_TIMEOUT_C,
		    TPM_INT_STS_DATA_AVAIL, true)) {
			error = EIO;
			goto out;
		}
	}
	tpmtis_finish_intr(sc);
	/* Read response header. Length is passed in bytes 2 - 6. */
	if (!tpmtis_read_bytes(sc, TPM_HEADER_SIZE, priv->buf)) {
		device_printf(dev,
		    "Failed to read response header\n");
		error = EIO;
		goto out;
	}
	bytes_available = be32toh(*(uint32_t *) (&priv->buf[2]));

	if (bytes_available > TPM_BUFSIZE || bytes_available < TPM_HEADER_SIZE) {
		device_printf(dev,
		    "Incorrect response size: %zu\n",
		    bytes_available);
		error = EIO;
		goto out;
	}
	if (!tpmtis_read_bytes(sc, bytes_available - TPM_HEADER_SIZE,
	    &priv->buf[TPM_HEADER_SIZE])) {
		device_printf(dev,
		    "Failed to read response\n");
		error = EIO;
		goto out;
	}
	priv->offset = 0;
	priv->len = bytes_available;
	error = 0;

out:
	/*
	 * Per TIS 1.3 section 5.6.12, write commandReady after every command
	 * attempt so the TPM can discard a partial FIFO transaction and free
	 * its internal resources.  The next tpmtis_go_ready() provides the
	 * second write the spec mentions and waits for the state transition.
	 */
	if (locality) {
		TPM_WRITE_4(sc->dev, TPM_STS, TPM_STS_CMD_RDY);
		TPM_WRITE_BARRIER(sc->dev, TPM_STS, 4);
		tpmtis_relinquish_locality(sc);
	}

	return (error);
}

/* ACPI Driver */
static device_method_t tpmtis_methods[] = {
	DEVMETHOD(device_attach,	tpmtis_attach),
	DEVMETHOD(device_detach,	tpmtis_detach),
	DEVMETHOD(device_shutdown,	tpm20_shutdown),
	DEVMETHOD(device_suspend,	tpmtis_suspend),
	DEVMETHOD(device_resume,	tpmtis_resume),
	DEVMETHOD(tpm_transmit,		tpmtis_transmit),
	DEVMETHOD_END
};

DEFINE_CLASS_0(tpmtis, tpmtis_driver, tpmtis_methods, sizeof(struct tpm_sc));
