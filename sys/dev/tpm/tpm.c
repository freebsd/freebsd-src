/*
 * Copyright (c) 2008, 2009 Michael Shalayeff
 * Copyright (c) 2009, 2010 Hans-Joerg Hoexer
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* #define	TPM_DEBUG */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/random.h>

#include <sys/module.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <machine/md_var.h>

#include <isa/isareg.h>
#include <isa/isavar.h>
#include <dev/tpm/tpmvar.h>
#include <dev/tpm/tpm_irq.h>

#include "tpm_if.h"

#if defined TPM_HARVEST || defined RANDOM_ENABLE_TPM
#include <dev/random/randomdev.h>
#include <dev/random/random_harvestq.h>
#endif

MALLOC_DEFINE(M_TPM, "tpm12_buffer", "buffer for TPM 1.2 responses");

#if defined TPM_HARVEST || defined RANDOM_ENABLE_TPM
static const struct random_source random_tpm12 = {
	.rs_ident = "TPM",
	.rs_source = RANDOM_PURE_TPM,
};
#endif

#define	TPM_BUFSIZ	1024

#define TPM_HDRSIZE	10

#define TPM_PARAM_SIZE	0x0001

#define	TPM_TAG_RQU_COMMAND	0x00c1
#define	TPM_TAG_RSP_COMMAND	0x00c4
#define	TPM_ORD_SAVESTATE	0x00000098
#define	TPM_ORD_GETRANDOM	0x00000046
#define	TPM_WARN_RETRY		0x00000800

#define	TPM_SAVESTATE_RETRIES	50
#define	TPM_SAVESTATE_RETRY_MS	100

#define	TPM_HARVEST_SIZE	16
#define	TPM_HARVEST_INTERVAL	10

#define	TPM_ACCESS			0x0000	/* access register */
#define	TPM_ACCESS_ESTABLISHMENT	0x01	/* establishment */
#define	TPM_ACCESS_REQUEST_USE		0x02	/* request using locality */
#define	TPM_ACCESS_REQUEST_PENDING	0x04	/* pending request */
#define	TPM_ACCESS_SEIZE		0x08	/* request locality seize */
#define	TPM_ACCESS_SEIZED		0x10	/* locality has been seized */
#define	TPM_ACCESS_ACTIVE_LOCALITY	0x20	/* locality is active */
#define	TPM_ACCESS_VALID		0x80	/* bits are valid */
#define	TPM_ACCESS_BITS	\
    "\020\01EST\02REQ\03PEND\04SEIZE\05SEIZED\06ACT\010VALID"

#define	TPM_INTERRUPT_ENABLE	0x0008
#define	TPM_GLOBAL_INT_ENABLE	0x80000000	/* enable ints */
#define	TPM_CMD_READY_INT	0x00000080	/* cmd ready enable */
#define	TPM_INT_EDGE_FALLING	0x00000018
#define	TPM_INT_EDGE_RISING	0x00000010
#define	TPM_INT_LEVEL_LOW	0x00000008
#define	TPM_INT_LEVEL_HIGH	0x00000000
#define	TPM_LOCALITY_CHANGE_INT	0x00000004	/* locality change enable */
#define	TPM_STS_VALID_INT	0x00000002	/* int on TPM_STS_VALID is set */
#define	TPM_DATA_AVAIL_INT	0x00000001	/* int on TPM_STS_DATA_AVAIL is set */
#define	TPM_INTERRUPT_ENABLE_BITS \
    "\020\040ENA\010RDY\03LOCH\02STSV\01DRDY"

#define	TPM_INT_VECTOR		0x000c	/* 8 bit reg for 4 bit irq vector */
#define	TPM_INT_STATUS		0x0010	/* bits are & 0x87 from TPM_INTERRUPT_ENABLE */

#define	TPM_INTF_CAPABILITIES		0x0014	/* capability register */
#define	TPM_INTF_BURST_COUNT_STATIC	0x0100	/* TPM_STS_BMASK static */
#define	TPM_INTF_CMD_READY_INT		0x0080	/* int on ready supported */
#define	TPM_INTF_INT_EDGE_FALLING	0x0040	/* falling edge ints supported */
#define	TPM_INTF_INT_EDGE_RISING	0x0020	/* rising edge ints supported */
#define	TPM_INTF_INT_LEVEL_LOW		0x0010	/* level-low ints supported */
#define	TPM_INTF_INT_LEVEL_HIGH		0x0008	/* level-high ints supported */
#define	TPM_INTF_LOCALITY_CHANGE_INT	0x0004	/* locality-change int (mb 1) */
#define	TPM_INTF_STS_VALID_INT		0x0002	/* TPM_STS_VALID int supported */
#define	TPM_INTF_DATA_AVAIL_INT		0x0001	/* TPM_STS_DATA_AVAIL int supported (mb 1) */
#define	TPM_CAPSREQ \
  (TPM_INTF_DATA_AVAIL_INT|TPM_INTF_LOCALITY_CHANGE_INT|TPM_INTF_INT_LEVEL_LOW)
#define	TPM_CAPBITS \
  "\020\01IDRDY\02ISTSV\03ILOCH\04IHIGH\05ILOW\06IEDGE\07IFALL\010IRDY\011BCST"

#define	TPM_STS			0x0018		/* status register */
#define TPM_STS_MASK		0x000000ff	/* status bits */
#define	TPM_STS_BMASK		0x00ffff00	/* ro io burst size */
#define	TPM_STS_VALID		0x00000080	/* ro other bits are valid */
#define	TPM_STS_CMD_READY	0x00000040	/* rw chip/signal ready */
#define	TPM_STS_GO		0x00000020	/* wo start the command */
#define	TPM_STS_DATA_AVAIL	0x00000010	/* ro data available */
#define	TPM_STS_DATA_EXPECT	0x00000008	/* ro more data to be written */
#define	TPM_STS_RESP_RETRY	0x00000002	/* wo resend the response */
#define	TPM_STS_BITS	"\020\010VALID\07RDY\06GO\05DRDY\04EXPECT\02RETRY"

#define	TPM_DATA	0x0024
#define	TPM_ID		0x0f00
#define	TPM_REV		0x0f04
#define	TPM_SIZE	0x5000		/* five pages of the above */

#define	TPM_ACCESS_TMO	2000		/* 2sec */
#define	TPM_READY_TMO	2000		/* 2sec */
#define	TPM_READ_TMO	120000		/* 2 minutes */
#define TPM_BURST_TMO	2000		/* 2sec */

#define	TPM_LEGACY_BUSY	0x01
#define	TPM_LEGACY_ABRT	0x01
#define	TPM_LEGACY_DA	0x02
#define	TPM_LEGACY_RE	0x04
#define	TPM_LEGACY_LAST	0x04
#define	TPM_LEGACY_BITS	"\020\01BUSY\2DA\3RE\4LAST"
#define	TPM_LEGACY_TMO		(2*60)	/* sec */
#define	TPM_LEGACY_SLEEP	5	/* ticks */
#define	TPM_LEGACY_DELAY	100

/* Set when enabling legacy interface in host bridge. */
int tpm_enabled;

#define	TPMSOFTC(dev) \
	((struct tpm_softc *)dev->si_drv1)

d_open_t	tpmopen;
d_read_t	tpmread;
d_write_t	tpmwrite;
d_ioctl_t	tpmioctl;
static d_priv_dtor_t tpm_dtor;

static struct cdevsw tpm_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	tpmopen,
	.d_read =	tpmread,
	.d_write =	tpmwrite,
	.d_ioctl =	tpmioctl,
	.d_name =	"tpm",
};

const struct {
	u_int32_t devid;
	char name[32];
	int flags;
#define TPM_DEV_NOINTS	0x0001
} tpm_devs[] = {
	{ 0x000615d1, "IFX SLD 9630 TT 1.1", 0 },
	{ 0x000b15d1, "IFX SLB 9635 TT 1.2", 0 },
	{ 0x100214e4, "Broadcom BCM0102", TPM_DEV_NOINTS },
	{ 0x00fe1050, "WEC WPCT200", 0 },
	{ 0x687119fa, "SNS SSX35", 0 },
	{ 0x2e4d5453, "STM ST19WP18", 0 },
	{ 0x32021114, "ATML 97SC3203", TPM_DEV_NOINTS },
	{ 0x10408086, "INTEL INTC0102", 0 },
	{ 0, "", TPM_DEV_NOINTS },
};

int tpm_tis12_irqinit(struct tpm_softc *, int);
int tpm_tis12_init(struct tpm_softc *, const char *);
int tpm_tis12_start(struct tpm_softc *, int);
int tpm_tis12_read(struct tpm_softc *, void *, int, size_t *, int);
int tpm_tis12_write(struct tpm_softc *, void *, int);
int tpm_tis12_end(struct tpm_softc *, int, int);

void tpm_intr(void *);

int tpm_waitfor_poll(struct tpm_softc *, u_int8_t, sbintime_t);
int tpm_waitfor_int(struct tpm_softc *, u_int8_t, sbintime_t);
int tpm_waitfor(struct tpm_softc *, u_int8_t, int);
int tpm_request_locality(struct tpm_softc *, int);
int tpm_getburst(struct tpm_softc *, int *);
u_int8_t tpm_status(struct tpm_softc *);

int tpm_legacy_probe(bus_space_tag_t, bus_addr_t);
int tpm_legacy_init(struct tpm_softc *, const char *);
int tpm_legacy_start(struct tpm_softc *, int);
int tpm_legacy_read(struct tpm_softc *, void *, int, size_t *, int);
int tpm_legacy_write(struct tpm_softc *, void *, int);
int tpm_legacy_end(struct tpm_softc *, int, int);

static int tpm_attach_finish(struct tpm_softc *);
static void tpm_delayed_attach(void *);
static int tpm_transmit_header(struct tpm_softc *, uint32_t, uint32_t *);
#if defined TPM_HARVEST || defined RANDOM_ENABLE_TPM
static int tpm_get_random(struct tpm_softc *, uint8_t *, size_t *);
static void tpm_harvest(void *, int);
static void tpm_harvest_enqueue(const uint8_t *, size_t);
#endif
static void tpm_tis12_abort(struct tpm_softc *);
static int tpm_tis12_devid_index(uint32_t);
static int tpm_tis12_quiesce(struct tpm_softc *);
static void tpm_tis12_release_locality_hw(bus_space_tag_t, bus_space_handle_t);
static void tpm_tis12_relinquish_locality(struct tpm_softc *);
static int tpm_tis12_request_locality(struct tpm_softc *, int);
static int tpm_tis12_resume(struct tpm_softc *);
static int tpm_tis12_setup_intr(struct tpm_softc *);
static int tpm_tis12_fallback(struct tpm_softc *);
static void tpm_tis12_intr_task(void *, int);
static void tpm_tis12_intr_failed(struct tpm_softc *);
static bool tpm_tis12_mask_intr(struct tpm_softc *);
static void tpm_tis12_arm_intr(struct tpm_softc *);
static void tpm_tis12_finish_intr(struct tpm_softc *);


/*
 * FreeBSD specific code for probing and attaching TPM to device tree.
 */
#if 0
static void
tpm_identify(driver_t *driver, device_t parent)
{
	BUS_ADD_CHILD(parent, ISA_ORDER_SPECULATIVE, "tpm", 0);
}
#endif

int
tpm_attach(device_t dev)
{
	struct tpm_softc *sc;
	void *intr_cookie;
	int error;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	sc->sc_intr_hook.ich_state = ICHS_DONE;
	sx_init(&sc->sc_lock, "TPM driver lock");
	mtx_init(&sc->sc_intr_lock, "TPM interrupt lock", NULL, MTX_DEF);
	cv_init(&sc->sc_intr_cv, "tpm_intr");
	sc->intr_cookie = NULL;
	sc->irq_res = NULL;
	sc->sc_cdev = NULL;
	sc->sc_capabilities = 0;
	sc->sc_flags = 0;
	sc->sc_suspend = 0;
	sc->sc_dying = false;
	sc->sc_locality = false;
	sc->sc_command_pending = false;
	sc->sc_irq_usable = false;
	sc->sc_interrupts = false;
	sc->sc_intr_testing = false;
	sc->sc_intr_configuring = false;
	sc->sc_intr_fault = false;
	sc->sc_intr_armed = false;
	sc->sc_intr_received = false;
	sc->sc_irq_config.valid = false;
	TASK_INIT(&sc->sc_intr_task, 0, tpm_tis12_intr_task, sc);
#if defined TPM_HARVEST || defined RANDOM_ENABLE_TPM
	sc->sc_harvest_registered = false;
#endif

	sc->mem_rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->mem_rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL) {
		error = ENXIO;
		goto fail;
	}

	sc->sc_bt = rman_get_bustag(sc->mem_res);
	sc->sc_bh = rman_get_bushandle(sc->mem_res);

	sc->irq_rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &sc->irq_rid,
	    RF_ACTIVE);
	if (tpm_legacy_probe(sc->sc_bt, sc->sc_bh)) {
		sc->sc_init = tpm_legacy_init;
		sc->sc_start = tpm_legacy_start;
		sc->sc_read = tpm_legacy_read;
		sc->sc_write = tpm_legacy_write;
		sc->sc_end = tpm_legacy_end;
	} else {
		sc->sc_init = tpm_tis12_init;
		sc->sc_start = tpm_tis12_start;
		sc->sc_read = tpm_tis12_read;
		sc->sc_write = tpm_tis12_write;
		sc->sc_end = tpm_tis12_end;
	}

	printf("%s", device_get_name(dev));
	sx_xlock(&sc->sc_lock);
	error = sc->sc_init(sc, "tpm");
	if (error != 0) {
		sx_xunlock(&sc->sc_lock);
		error = ENXIO;
		goto fail;
	}

	if (sc->sc_init == tpm_tis12_init && sc->irq_res != NULL) {
		if (sc->sc_irq_usable && (TPM_CONFIGURE_IRQ(dev, sc->irq_res,
		    &sc->sc_irq_config, false) != 0 ||
		    !tpm_irq_supported(sc->sc_capabilities,
		    &sc->sc_irq_config))) {
			sc->sc_irq_usable = false;
			device_printf(dev,
			    "IRQ route unavailable; using polling\n");
		}
		if (sc->sc_irq_usable) {
			/* Recovery sleeps with an exclusive source masked. */
			/* Failed setup may leave a stale output cookie. */
			intr_cookie = NULL;
			error = bus_setup_intr(dev, sc->irq_res,
			    INTR_TYPE_TTY | INTR_MPSAFE | INTR_EXCL |
			    INTR_SLEEPABLE, NULL, tpm_intr, sc, &intr_cookie);
			if (error == 0)
				sc->intr_cookie = intr_cookie;
			else
				device_printf(dev,
				    "IRQ setup failed; using polling\n");
		}
		if (sc->intr_cookie == NULL)
			error = tpm_tis12_fallback(sc);
		if (error != 0) {
			sx_xunlock(&sc->sc_lock);
			error = ENXIO;
			goto fail;
		}
	}
	sx_xunlock(&sc->sc_lock);

	/* IRQ validation needs timers and a runnable interrupt thread. */
	if (cold && sc->intr_cookie != NULL) {
		sc->sc_intr_hook.ich_func = tpm_delayed_attach;
		sc->sc_intr_hook.ich_arg = sc;
		if (config_intrhook_establish(&sc->sc_intr_hook) == 0)
			return (0);
		sc->sc_intr_hook.ich_state = ICHS_DONE;
		error = ENXIO;
		goto fail;
	}
	sx_xlock(&sc->sc_lock);
	error = tpm_attach_finish(sc);
	sx_xunlock(&sc->sc_lock);
	if (error != 0)
		goto fail;
	return (0);

fail:
	tpm_detach(dev);
	return (error);
}

/* Publish the device only after the route test has finished. */
static int
tpm_attach_finish(struct tpm_softc *sc)
{
	struct make_dev_args args;
	int error;

	sx_assert(&sc->sc_lock, SA_XLOCKED);
	if (sc->intr_cookie != NULL) {
		error = tpm_tis12_setup_intr(sc);
		if (error != 0)
			return (ENXIO);
	}
	make_dev_args_init(&args);
	args.mda_devsw = &tpm_cdevsw;
	args.mda_unit = device_get_unit(sc->sc_dev);
	args.mda_uid = UID_ROOT;
	args.mda_gid = GID_WHEEL;
	args.mda_mode = 0600;
	args.mda_si_drv1 = sc;
	error = make_dev_s(&args, &sc->sc_cdev, "tpm");
	if (error != 0)
		return (error);

#if defined TPM_HARVEST || defined RANDOM_ENABLE_TPM
	TIMEOUT_TASK_INIT(taskqueue_thread, &sc->sc_harvest_task, 0,
	    tpm_harvest, sc);
	random_source_register(&random_tpm12);
	sc->sc_harvest_registered = true;
	taskqueue_enqueue_timeout(taskqueue_thread, &sc->sc_harvest_task, 0);
#endif

	return (0);
}

static void
tpm_delayed_attach(void *arg)
{
	struct tpm_softc *sc = arg;
	int error;

	sx_xlock(&sc->sc_lock);
	error = tpm_attach_finish(sc);
	if (error != 0) {
		mtx_lock(&sc->sc_intr_lock);
		sc->sc_dying = true;
		mtx_unlock(&sc->sc_intr_lock);
		device_printf(sc->sc_dev, "deferred attach failed: %d\n", error);
	}
	sx_xunlock(&sc->sc_lock);
	config_intrhook_disestablish(&sc->sc_intr_hook);
}

int
tpm_detach(device_t dev)
{
	struct tpm_softc *sc;

	sc = device_get_softc(dev);
	config_intrhook_drain(&sc->sc_intr_hook);
	sx_xlock(&sc->sc_lock);
	mtx_lock(&sc->sc_intr_lock);
	sc->sc_dying = true;
	mtx_unlock(&sc->sc_intr_lock);
	sx_xunlock(&sc->sc_lock);
	taskqueue_drain(taskqueue_thread, &sc->sc_intr_task);

#if defined TPM_HARVEST || defined RANDOM_ENABLE_TPM
	if (sc->sc_harvest_registered) {
		taskqueue_drain_timeout(taskqueue_thread,
		    &sc->sc_harvest_task);
		random_source_deregister(&random_tpm12);
		sc->sc_harvest_registered = false;
	}
#endif

	/*
	 * Prevent new methods from touching the transport.  Do not hold the
	 * lock while destroy_dev() drains methods and cdevpriv destructors
	 * which also acquire it.
	 */
	if (sc->sc_cdev != NULL) {
		destroy_dev(sc->sc_cdev);
		sc->sc_cdev = NULL;
	}
	sx_xlock(&sc->sc_lock);
	if (sc->mem_res != NULL && sc->sc_init == tpm_tis12_init) {
		/* Identified TIS devices may enable delivery while polling. */
		if (sc->sc_capabilities != 0 && tpm_tis12_quiesce(sc) != 0)
			device_printf(dev,
			    "failed to quiesce TPM interrupts during detach\n");
		tpm_tis12_abort(sc);
	}
	sx_xunlock(&sc->sc_lock);
	if (sc->intr_cookie != NULL) {
		bus_teardown_intr(dev, sc->irq_res, sc->intr_cookie);
		sc->intr_cookie = NULL;
	}
	if (sc->mem_res != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, sc->mem_rid,
		    sc->mem_res);
		sc->mem_res = NULL;
	}
	if (sc->irq_res != NULL) {
		bus_release_resource(dev, SYS_RES_IRQ,
		    sc->irq_rid, sc->irq_res);
		sc->irq_res = NULL;
	}
	cv_destroy(&sc->sc_intr_cv);
	mtx_destroy(&sc->sc_intr_lock);
	sx_destroy(&sc->sc_lock);

	return (0);
}

/* Probe TPM using TIS 1.2 interface. */
int
tpm_tis12_probe(bus_space_tag_t bt, bus_space_handle_t bh)
{
	u_int32_t r;
	u_int8_t reg;
	bool requested;
	int to;

	r = bus_space_read_4(bt, bh, TPM_INTF_CAPABILITIES);
	if (r == 0xffffffff)
		return 0;

#ifdef TPM_DEBUG
	printf("tpm: caps=%b\n", r, TPM_CAPBITS);
#endif
	if ((r & TPM_CAPSREQ) != TPM_CAPSREQ ||
	    !(r & (TPM_INTF_INT_EDGE_RISING | TPM_INTF_INT_LEVEL_LOW))) {
#ifdef TPM_DEBUG
		printf("tpm: caps too low (caps=%b)\n", r, TPM_CAPBITS);
#endif
		return 0;
	}

	reg = bus_space_read_1(bt, bh, TPM_ACCESS);
	requested = false;
	if ((reg & (TPM_ACCESS_VALID | TPM_ACCESS_ACTIVE_LOCALITY)) !=
	    (TPM_ACCESS_VALID | TPM_ACCESS_ACTIVE_LOCALITY)) {
		requested = true;
		bus_space_write_1(bt, bh, TPM_ACCESS,
		    TPM_ACCESS_REQUEST_USE);
		bus_space_barrier(bt, bh, TPM_ACCESS, 1,
		    BUS_SPACE_BARRIER_WRITE);
		to = TPM_ACCESS_TMO;	/* Steps of one millisecond. */
		do {
			reg = bus_space_read_1(bt, bh, TPM_ACCESS);
			if ((reg & (TPM_ACCESS_VALID |
			    TPM_ACCESS_ACTIVE_LOCALITY)) ==
			    (TPM_ACCESS_VALID | TPM_ACCESS_ACTIVE_LOCALITY))
				break;
			DELAY(1000);
		} while (--to != 0);
	}

	if ((reg & (TPM_ACCESS_VALID | TPM_ACCESS_ACTIVE_LOCALITY)) ==
	    (TPM_ACCESS_VALID | TPM_ACCESS_ACTIVE_LOCALITY))
		r = bus_space_read_4(bt, bh, TPM_ID);
	else
		r = UINT32_MAX;
	if (requested)
		tpm_tis12_release_locality_hw(bt, bh);
	return (r != UINT32_MAX);
}

/* Leave delivery disabled, preserving any locality owned by the caller. */
static int
tpm_tis12_quiesce(struct tpm_softc *sc)
{
	uint32_t reg;
	bool release;
	int error;

	sx_assert(&sc->sc_lock, SA_XLOCKED);
	release = !sc->sc_locality;
	if (release) {
		error = tpm_tis12_request_locality(sc, 0);
		if (error != 0) {
			/* This register can be read without owning locality. */
			reg = bus_space_read_4(sc->sc_bt, sc->sc_bh,
			    TPM_INTERRUPT_ENABLE);
			return ((reg & TPM_GLOBAL_INT_ENABLE) == 0 ? 0 : error);
		}
	}

	mtx_lock(&sc->sc_intr_lock);
	error = tpm_tis12_mask_intr(sc) ? 0 : EIO;
	sc->sc_intr_armed = false;
	mtx_unlock(&sc->sc_intr_lock);
	if (release)
		tpm_tis12_relinquish_locality(sc);
	return (error);
}

/* Quiesce before handler installation; keep the known-device quirks. */
int
tpm_tis12_irqinit(struct tpm_softc *sc, int idx)
{
	int error;

	sx_assert(&sc->sc_lock, SA_XLOCKED);
	KASSERT(sc->sc_locality, ("%s: locality not owned", __func__));
	error = tpm_tis12_quiesce(sc);
	if (error != 0)
		return (error);
	sc->sc_irq_usable = sc->irq_res != NULL &&
	    (tpm_devs[idx].flags & TPM_DEV_NOINTS) == 0;
	return (0);
}

static int
tpm_tis12_devid_index(uint32_t devid)
{
	int i;

	for (i = 0; tpm_devs[i].devid != 0; i++)
		if (tpm_devs[i].devid == devid)
			break;
	return (i);
}

/* Called with sc_intr_lock held.  Never take sc_lock in the handler. */
static void
tpm_tis12_intr_failed(struct tpm_softc *sc)
{

	sc->sc_interrupts = false;
	if (!sc->sc_intr_fault && !sc->sc_dying) {
		sc->sc_intr_fault = true;
		taskqueue_enqueue(taskqueue_thread, &sc->sc_intr_task);
	}
	cv_broadcast(&sc->sc_intr_cv);
}

static bool
tpm_tis12_mask_intr(struct tpm_softc *sc)
{
	uint32_t reg;
	bool disabled;

	mtx_assert(&sc->sc_intr_lock, MA_OWNED);
	reg = bus_space_read_4(sc->sc_bt, sc->sc_bh, TPM_INTERRUPT_ENABLE);
	bus_space_write_4(sc->sc_bt, sc->sc_bh, TPM_INTERRUPT_ENABLE,
	    reg & ~(TPM_GLOBAL_INT_ENABLE | TPM_IRQ_EVENTS));
	bus_space_barrier(sc->sc_bt, sc->sc_bh, TPM_INTERRUPT_ENABLE, 4,
	    BUS_SPACE_BARRIER_WRITE);
	disabled = (bus_space_read_4(sc->sc_bt, sc->sc_bh,
	    TPM_INTERRUPT_ENABLE) & TPM_GLOBAL_INT_ENABLE) == 0;
	bus_space_write_4(sc->sc_bt, sc->sc_bh, TPM_INT_STATUS,
	    bus_space_read_4(sc->sc_bt, sc->sc_bh, TPM_INT_STATUS));
	bus_space_barrier(sc->sc_bt, sc->sc_bh, TPM_INT_STATUS, 4,
	    BUS_SPACE_BARRIER_WRITE);
	return (disabled);
}

static void
tpm_tis12_arm_intr(struct tpm_softc *sc)
{

	mtx_lock(&sc->sc_intr_lock);
	sc->sc_intr_received = false;
	if ((sc->sc_interrupts || sc->sc_intr_testing) && !sc->sc_intr_fault) {
		if (tpm_tis12_mask_intr(sc) && (bus_space_read_4(sc->sc_bt,
		    sc->sc_bh, TPM_INT_STATUS) & TPM_DATA_AVAIL_INT) == 0) {
			sc->sc_intr_armed = true;
			bus_space_write_4(sc->sc_bt, sc->sc_bh,
			    TPM_INTERRUPT_ENABLE, sc->sc_intr_mode |
			    TPM_DATA_AVAIL_INT | TPM_GLOBAL_INT_ENABLE);
			bus_space_barrier(sc->sc_bt, sc->sc_bh,
			    TPM_INTERRUPT_ENABLE, 4, BUS_SPACE_BARRIER_WRITE);
		} else {
			tpm_tis12_intr_failed(sc);
		}
	}
	mtx_unlock(&sc->sc_intr_lock);
}

static void
tpm_tis12_finish_intr(struct tpm_softc *sc)
{
	sbintime_t deadline;

	deadline = sbinuptime() + mstosbt(TPM_IRQ_GRACE_MS);
	mtx_lock(&sc->sc_intr_lock);
	while (sc->sc_intr_armed && !sc->sc_intr_received &&
	    !sc->sc_intr_fault) {
		if (cv_timedwait_sbt(&sc->sc_intr_cv, &sc->sc_intr_lock,
		    deadline, 0, C_ABSOLUTE | C_HARDCLOCK) == EWOULDBLOCK) {
			tpm_tis12_intr_failed(sc);
			break;
		}
	}
	mtx_unlock(&sc->sc_intr_lock);
}

static int
tpm_tis12_fallback(struct tpm_softc *sc)
{
	int error;

	sx_assert(&sc->sc_lock, SA_XLOCKED);
	mtx_lock(&sc->sc_intr_lock);
	sc->sc_interrupts = false;
	sc->sc_intr_testing = false;
	sc->sc_intr_armed = false;
	mtx_unlock(&sc->sc_intr_lock);
	error = tpm_tis12_quiesce(sc);
	if (error != 0) {
		sc->sc_suspend = 1;
		device_printf(sc->sc_dev,
		    "failed to quiesce TPM interrupts; suspending device\n");
	}
	if (sc->intr_cookie != NULL) {
		bus_teardown_intr(sc->sc_dev, sc->irq_res, sc->intr_cookie);
		sc->intr_cookie = NULL;
		if (error == 0)
			device_printf(sc->sc_dev,
			    "interrupts unavailable; using polling\n");
	}
	if (sc->irq_res != NULL && bus_release_resource(sc->sc_dev, SYS_RES_IRQ,
	    sc->irq_rid, sc->irq_res) == 0)
		sc->irq_res = NULL;
	sc->sc_irq_usable = false;
	return (error);
}

static void
tpm_tis12_intr_task(void *arg, int pending)
{
	struct tpm_softc *sc = arg;

	sx_xlock(&sc->sc_lock);
	if (!sc->sc_dying)
		tpm_tis12_fallback(sc);
	sx_xunlock(&sc->sc_lock);
}

static int
tpm_tis12_setup_intr(struct tpm_softc *sc)
{
	struct tpm_irq_config config;
	uint8_t response[15];
	uint32_t length;
	bool working;
	int error, quiesce_error;
	static const uint8_t cmd[] = {
		0x00, 0xc1,		/* TPM_TAG_RQU_COMMAND */
		0x00, 0x00, 0x00, 0x0e,	/* Command length */
		0x00, 0x00, 0x00, 0x46,	/* TPM_ORD_GetRandom */
		0x00, 0x00, 0x00, 0x01	/* One byte */
	};

	sx_assert(&sc->sc_lock, SA_XLOCKED);
	mtx_lock(&sc->sc_intr_lock);
	sc->sc_interrupts = false;
	working = !sc->sc_intr_fault && !sc->sc_dying && sc->sc_irq_usable;
	mtx_unlock(&sc->sc_intr_lock);
	if (!working || tpm_tis12_request_locality(sc, 0) != 0)
		goto fallback;
	config = sc->sc_irq_config;
	mtx_lock(&sc->sc_intr_lock);
	sc->sc_intr_configuring = true;
	mtx_unlock(&sc->sc_intr_lock);
	error = TPM_CONFIGURE_IRQ(sc->sc_dev, sc->irq_res, &config, true);
	/* _SRS may alter the enable register, including on failure. */
	quiesce_error = tpm_tis12_quiesce(sc);
	mtx_lock(&sc->sc_intr_lock);
	sc->sc_intr_configuring = false;
	mtx_unlock(&sc->sc_intr_lock);
	if (quiesce_error != 0 || error != 0 ||
	    !tpm_irq_supported(sc->sc_capabilities, &config)) {
		tpm_tis12_relinquish_locality(sc);
		goto fallback;
	}
	if (config.sirq != TPM_IRQ_SIRQ_UNCHANGED)
		bus_space_write_1(sc->sc_bt, sc->sc_bh, TPM_INT_VECTOR,
		    config.sirq);
	sc->sc_intr_mode = config.mode;
	sc->sc_irq_config = config;
	bus_space_write_4(sc->sc_bt, sc->sc_bh, TPM_INTERRUPT_ENABLE,
	    config.mode);
	bus_space_barrier(sc->sc_bt, sc->sc_bh, TPM_INTERRUPT_ENABLE, 4,
	    BUS_SPACE_BARRIER_WRITE);
	working = (bus_space_read_4(sc->sc_bt, sc->sc_bh,
	    TPM_INTERRUPT_ENABLE) & (TPM_GLOBAL_INT_ENABLE |
	    TPM_IRQ_MODE_MASK)) == config.mode;
	tpm_tis12_relinquish_locality(sc);
	if (!working)
		goto fallback;

	mtx_lock(&sc->sc_intr_lock);
	sc->sc_intr_testing = true;
	sc->sc_intr_received = false;
	mtx_unlock(&sc->sc_intr_lock);
	error = tpm_tis12_start(sc, UIO_WRITE);
	if (error == 0)
		error = tpm_tis12_write(sc, __DECONST(void *, cmd),
		    sizeof(cmd));
	if (error == 0)
		error = tpm_tis12_end(sc, UIO_WRITE, 0);
	if (error == 0)
		error = tpm_tis12_start(sc, UIO_READ);
	if (error == 0)
		error = tpm_tis12_read(sc, response, 10, NULL, TPM_PARAM_SIZE);
	if (error == 0) {
		length = be32dec(response + 2);
		if (be16dec(response) != TPM_TAG_RSP_COMMAND ||
		    length < 10 || length > sizeof(response))
			error = EIO;
		else
			error = tpm_tis12_read(sc, response + 10, length - 10,
			    NULL, TPM_PARAM_SIZE);
	}
	if (error == 0)
		error = tpm_tis12_end(sc, UIO_READ, 0);
	else
		tpm_tis12_abort(sc);
	mtx_lock(&sc->sc_intr_lock);
	sc->sc_intr_testing = false;
	working = error == 0 && sc->sc_intr_received && !sc->sc_intr_fault;
	sc->sc_interrupts = working;
	mtx_unlock(&sc->sc_intr_lock);
	if (working) {
		device_printf(sc->sc_dev, "using verified interrupts\n");
		return (0);
	}
fallback:
	return (tpm_tis12_fallback(sc));
}

/* Setup TPM using TIS 1.2 interface. */
int
tpm_tis12_init(struct tpm_softc *sc, const char *name)
{
	u_int32_t r;
	int error, i;

	sx_assert(&sc->sc_lock, SA_XLOCKED);
	r = bus_space_read_4(sc->sc_bt, sc->sc_bh, TPM_INTF_CAPABILITIES);
#ifdef TPM_DEBUG
	printf(" caps=%b ", r, TPM_CAPBITS);
#endif
	if ((r & TPM_CAPSREQ) != TPM_CAPSREQ ||
	    !(r & (TPM_INTF_INT_EDGE_RISING | TPM_INTF_INT_LEVEL_LOW))) {
		printf(": capabilities too low (caps=%b)\n", r, TPM_CAPBITS);
		return 1;
	}
	sc->sc_capabilities = r;

	sc->sc_devid = bus_space_read_4(sc->sc_bt, sc->sc_bh, TPM_ID);
	sc->sc_rev = bus_space_read_1(sc->sc_bt, sc->sc_bh, TPM_REV);

	i = tpm_tis12_devid_index(sc->sc_devid);

	if (tpm_devs[i].devid)
		printf(": %s rev 0x%x\n", tpm_devs[i].name, sc->sc_rev);
	else
		printf(": device 0x%08x rev 0x%x\n", sc->sc_devid, sc->sc_rev);

	error = tpm_tis12_request_locality(sc, 0);
	if (error != 0)
		return 1;

	error = tpm_tis12_irqinit(sc, i);
	if (error != 0) {
		printf("%s: failed to quiesce interrupts during attach: %d\n",
		    name, error);
		goto out;
	}

	/* Abort whatever it thought it was doing. */
	bus_space_write_1(sc->sc_bt, sc->sc_bh, TPM_STS, TPM_STS_CMD_READY);
	error = tpm_waitfor(sc, TPM_STS_CMD_READY, TPM_READY_TMO);

out:
	tpm_tis12_relinquish_locality(sc);
	return (error != 0);
}

/* Restore TIS state which is not guaranteed to survive S3. */
static int
tpm_tis12_resume(struct tpm_softc *sc)
{
	uint32_t capabilities, devid;
	int error, i;

	sx_assert(&sc->sc_lock, SA_XLOCKED);
	mtx_lock(&sc->sc_intr_lock);
	sc->sc_interrupts = false;
	mtx_unlock(&sc->sc_intr_lock);
	error = tpm_tis12_quiesce(sc);
	if (error != 0)
		return (error);
	capabilities = bus_space_read_4(sc->sc_bt, sc->sc_bh,
	    TPM_INTF_CAPABILITIES);
	if ((capabilities & TPM_CAPSREQ) != TPM_CAPSREQ ||
	    (capabilities & (TPM_INTF_INT_EDGE_RISING |
	    TPM_INTF_INT_LEVEL_LOW)) == 0)
		return (ENXIO);
	devid = bus_space_read_4(sc->sc_bt, sc->sc_bh, TPM_ID);
	if (devid == UINT32_MAX || devid != sc->sc_devid)
		return (ENXIO);

	sc->sc_capabilities = capabilities;
	i = tpm_tis12_devid_index(devid);
	error = tpm_tis12_request_locality(sc, 0);
	if (error != 0)
		return (error);
	error = tpm_tis12_irqinit(sc, i);
	if (error != 0)
		goto out;

	/* Abort firmware residue and leave the command FIFO ready. */
	bus_space_write_1(sc->sc_bt, sc->sc_bh, TPM_STS,
	    TPM_STS_CMD_READY);
	error = tpm_waitfor(sc, TPM_STS_CMD_READY, TPM_READY_TMO);
out:
	tpm_tis12_relinquish_locality(sc);
	if (error == 0 && sc->intr_cookie != NULL)
		error = tpm_tis12_setup_intr(sc);
	return (error);
}

int
tpm_request_locality(struct tpm_softc *sc, int l)
{

	sx_assert(&sc->sc_lock, SA_XLOCKED);
	if (l != 0)
		return EINVAL;
	return (tpm_tis12_request_locality(sc, PCATCH));
}

/* Lifecycle callers wait through signals; command callers pass PCATCH. */
static int
tpm_tis12_request_locality(struct tpm_softc *sc, int flags)
{
	sbintime_t deadline;
	u_int32_t r;
	int rv;

	sx_assert(&sc->sc_lock, SA_XLOCKED);
	KASSERT(!sc->sc_locality, ("%s: locality already owned", __func__));

	if ((bus_space_read_1(sc->sc_bt, sc->sc_bh, TPM_ACCESS) &
	    (TPM_ACCESS_VALID | TPM_ACCESS_ACTIVE_LOCALITY)) ==
	    (TPM_ACCESS_VALID | TPM_ACCESS_ACTIVE_LOCALITY)) {
		mtx_lock(&sc->sc_intr_lock);
		sc->sc_locality = true;
		mtx_unlock(&sc->sc_intr_lock);
		return 0;
	}
	mtx_lock(&sc->sc_intr_lock);
	sc->sc_locality = false;
	mtx_unlock(&sc->sc_intr_lock);

	bus_space_write_1(sc->sc_bt, sc->sc_bh, TPM_ACCESS,
	    TPM_ACCESS_REQUEST_USE);
	bus_space_barrier(sc->sc_bt, sc->sc_bh, TPM_ACCESS, 1,
	    BUS_SPACE_BARRIER_WRITE);

	deadline = sbinuptime() + mstosbt(TPM_ACCESS_TMO);

	while ((r = bus_space_read_1(sc->sc_bt, sc->sc_bh, TPM_ACCESS) &
	    (TPM_ACCESS_VALID | TPM_ACCESS_ACTIVE_LOCALITY)) !=
	    (TPM_ACCESS_VALID | TPM_ACCESS_ACTIVE_LOCALITY)) {
		if (sbinuptime() >= deadline)
			break;
		rv = pause_sbt("tpm_locality", tick_sbt, 0,
		    C_HARDCLOCK | ((flags & PCATCH) != 0 ? C_CATCH : 0));
		if (rv &&  rv != EWOULDBLOCK) {
#ifdef TPM_DEBUG
			printf("tpm_request_locality: interrupted %d\n", rv);
#endif
			tpm_tis12_release_locality_hw(sc->sc_bt, sc->sc_bh);
			return rv;
		}
	}

	if ((r & (TPM_ACCESS_VALID | TPM_ACCESS_ACTIVE_LOCALITY)) !=
	    (TPM_ACCESS_VALID | TPM_ACCESS_ACTIVE_LOCALITY)) {
#ifdef TPM_DEBUG
		printf("tpm_request_locality: access %b\n", r, TPM_ACCESS_BITS);
#endif
		tpm_tis12_release_locality_hw(sc->sc_bt, sc->sc_bh);
		return EBUSY;
	}

	mtx_lock(&sc->sc_intr_lock);
	sc->sc_locality = true;
	mtx_unlock(&sc->sc_intr_lock);
	return 0;
}

/* Release an active locality or cancel a request that has not been granted. */
static void
tpm_tis12_release_locality_hw(bus_space_tag_t bt, bus_space_handle_t bh)
{

	bus_space_write_1(bt, bh, TPM_ACCESS, TPM_ACCESS_ACTIVE_LOCALITY);
	bus_space_barrier(bt, bh, TPM_ACCESS, 1, BUS_SPACE_BARRIER_WRITE);
}

/* Release the driver's locality and clear its associated command state. */
static void
tpm_tis12_relinquish_locality(struct tpm_softc *sc)
{

	sx_assert(&sc->sc_lock, SA_XLOCKED);
	sc->sc_command_pending = false;
	if (!sc->sc_locality)
		return;
	mtx_lock(&sc->sc_intr_lock);
	if (!tpm_tis12_mask_intr(sc))
		tpm_tis12_intr_failed(sc);
	sc->sc_intr_armed = false;
	tpm_tis12_release_locality_hw(sc->sc_bt, sc->sc_bh);
	sc->sc_locality = false;
	mtx_unlock(&sc->sc_intr_lock);
}

static void
tpm_tis12_abort(struct tpm_softc *sc)
{

	sx_assert(&sc->sc_lock, SA_XLOCKED);
	if (sc->sc_locality)
		bus_space_write_1(sc->sc_bt, sc->sc_bh, TPM_STS,
		    TPM_STS_CMD_READY);
	tpm_tis12_relinquish_locality(sc);
}

int
tpm_getburst(struct tpm_softc *sc, int *burst)
{
	sbintime_t deadline;
	int rv;

	sx_assert(&sc->sc_lock, SA_XLOCKED);
	deadline = sbinuptime() + mstosbt(TPM_BURST_TMO);
	for (;;) {
		/* Read bits 8 to 23 without touching the status byte. */
		*burst = bus_space_read_1(sc->sc_bt, sc->sc_bh, TPM_STS + 1);
		*burst |= bus_space_read_1(sc->sc_bt, sc->sc_bh, TPM_STS + 2)
		    << 8;
		if (*burst != 0)
			return (0);
		if (sbinuptime() >= deadline)
			return (EIO);
		rv = pause_sig("tpm_getburst", 1);
		if (rv != 0 && rv != EWOULDBLOCK)
			return (rv);
	}
}

u_int8_t
tpm_status(struct tpm_softc *sc)
{
	u_int8_t status;

	status = bus_space_read_1(sc->sc_bt, sc->sc_bh, TPM_STS) &
	    TPM_STS_MASK;

	return status;
}


/*
 * Transmit a command with no parameters and consume its header-only reply.
 */
static int
tpm_transmit_header(struct tpm_softc *sc, uint32_t ordinal, uint32_t *tpm_rc)
{
	uint8_t buf[TPM_HDRSIZE];
	size_t count;
	int end_error, error;

	sx_assert(&sc->sc_lock, SA_XLOCKED);
	be16enc(buf, TPM_TAG_RQU_COMMAND);
	be32enc(buf + 2, sizeof(buf));
	be32enc(buf + 6, ordinal);

	error = sc->sc_start(sc, UIO_WRITE);
	if (error != 0)
		return (error);
	error = sc->sc_write(sc, buf, sizeof(buf));
	end_error = sc->sc_end(sc, UIO_WRITE, error);
	if (error == 0)
		error = end_error;
	if (error != 0)
		return (error);

	error = sc->sc_start(sc, UIO_READ);
	if (error != 0)
		return (error);
	count = 0;
	error = sc->sc_read(sc, buf, sizeof(buf), &count, TPM_PARAM_SIZE);
	end_error = sc->sc_end(sc, UIO_READ, error);
	if (error == 0)
		error = end_error;
	if (error != 0)
		return (error);

	if (count != sizeof(buf) || be16dec(buf) != TPM_TAG_RSP_COMMAND ||
	    be32dec(buf + 2) != sizeof(buf))
		return (EPROTO);
	*tpm_rc = be32dec(buf + 6);
	return (0);
}

#if defined TPM_HARVEST || defined RANDOM_ENABLE_TPM
/*
 * Fetch random bytes as one command/response transaction.  The caller owns
 * sc_lock so a character-device request cannot be interposed between the
 * write and read phases.
 */
static int
tpm_get_random(struct tpm_softc *sc, uint8_t *entropy, size_t *entropy_size)
{
	uint8_t command[TPM_HDRSIZE + sizeof(uint32_t)];
	uint8_t response[TPM_HDRSIZE + sizeof(uint32_t) + TPM_HARVEST_SIZE];
	size_t count, response_size;
	uint32_t random_size, tpm_rc;
	int end_error, error;

	sx_assert(&sc->sc_lock, SA_XLOCKED);
	*entropy_size = 0;
	random_size = 0;

	be16enc(command, TPM_TAG_RQU_COMMAND);
	be32enc(command + 2, sizeof(command));
	be32enc(command + 6, TPM_ORD_GETRANDOM);
	be32enc(command + TPM_HDRSIZE, TPM_HARVEST_SIZE);

	error = sc->sc_start(sc, UIO_WRITE);
	if (error != 0)
		return (error);
	error = sc->sc_write(sc, command, sizeof(command));
	end_error = sc->sc_end(sc, UIO_WRITE, error);
	if (error == 0)
		error = end_error;
	if (error != 0)
		return (error);

	error = sc->sc_start(sc, UIO_READ);
	if (error != 0)
		return (error);
	count = 0;
	error = sc->sc_read(sc, response, TPM_HDRSIZE, &count,
	    TPM_PARAM_SIZE);
	if (error != 0)
		goto end;
	if (count != TPM_HDRSIZE ||
	    be16dec(response) != TPM_TAG_RSP_COMMAND) {
		error = EPROTO;
		goto end;
	}

	response_size = be32dec(response + 2);
	if (response_size < TPM_HDRSIZE || response_size > sizeof(response)) {
		error = EPROTO;
		goto end;
	}
	if (response_size > TPM_HDRSIZE) {
		count = 0;
		error = sc->sc_read(sc, response + TPM_HDRSIZE,
		    (int)(response_size - TPM_HDRSIZE), &count,
		    TPM_PARAM_SIZE);
		if (error != 0)
			goto end;
		if (count != response_size - TPM_HDRSIZE) {
			error = EIO;
			goto end;
		}
	}

	tpm_rc = be32dec(response + 6);
	if (tpm_rc != 0) {
		error = EIO;
		goto end;
	}
	if (response_size < TPM_HDRSIZE + sizeof(uint32_t)) {
		error = EPROTO;
		goto end;
	}
	random_size = be32dec(response + TPM_HDRSIZE);
	if (random_size > response_size - TPM_HDRSIZE - sizeof(uint32_t)) {
		error = EPROTO;
		goto end;
	}
	random_size = MIN(random_size, TPM_HARVEST_SIZE);

end:
	end_error = sc->sc_end(sc, UIO_READ, error);
	if (error == 0)
		error = end_error;
	if (error == 0) {
		memcpy(entropy, response + TPM_HDRSIZE + sizeof(uint32_t),
		    random_size);
		*entropy_size = random_size;
	}
	return (error);
}

static void
tpm_harvest_enqueue(const uint8_t *entropy, size_t entropy_size)
{
	size_t chunk_size;

	/* Preserve all RNG bytes rather than squashing an oversized event. */
	chunk_size = sizeof(((struct harvest_event *)0)->he_entropy);
	while (entropy_size != 0) {
		size_t size;

		size = MIN(entropy_size, chunk_size);
		random_harvest_queue(entropy, (u_int)size, RANDOM_PURE_TPM);
		entropy += size;
		entropy_size -= size;
	}
}

static void
tpm_harvest(void *arg, int pending)
{
	struct tpm_softc *sc;
	uint8_t entropy[TPM_HARVEST_SIZE];
	size_t entropy_size;

	sc = arg;
	entropy_size = 0;
	sx_xlock(&sc->sc_lock);
	if (sc->sc_dying || sc->sc_suspend != 0)
		goto out;

	/* Do not interpose between a user's command and response. */
	if ((sc->sc_flags & TPM_OPEN) == 0 && !sc->sc_command_pending)
		(void)tpm_get_random(sc, entropy, &entropy_size);
	taskqueue_enqueue_timeout(taskqueue_thread, &sc->sc_harvest_task,
	    hz * TPM_HARVEST_INTERVAL);
out:
	sx_xunlock(&sc->sc_lock);
	if (entropy_size != 0)
		tpm_harvest_enqueue(entropy, entropy_size);
}
#endif

/* Save TPM state on suspend. */
int
tpm_suspend(device_t dev)
{
	struct tpm_softc *sc;
	uint32_t tpm_rc;
	int error, tries;

	/*
	 * A TPM may report RETRY for several seconds when firmware issued
	 * SaveState before the driver loaded.  Any subsequent command can
	 * invalidate that saved state, so retry SaveState before entering S3.
	 */
	sc = device_get_softc(dev);
#if defined TPM_HARVEST || defined RANDOM_ENABLE_TPM
	if (sc->sc_harvest_registered)
		taskqueue_drain_timeout(taskqueue_thread,
		    &sc->sc_harvest_task);
#endif
	sx_xlock(&sc->sc_lock);
	if (sc->sc_dying) {
		error = ENXIO;
		goto out;
	}
	if (sc->sc_suspend != 0) {
		error = 0;
		goto out;
	}
	for (tries = 0; tries < TPM_SAVESTATE_RETRIES; tries++) {
		error = tpm_transmit_header(sc, TPM_ORD_SAVESTATE, &tpm_rc);
		if (error != 0 || tpm_rc != TPM_WARN_RETRY)
			break;
		pause("tpmsave", MAX(hz * TPM_SAVESTATE_RETRY_MS / 1000, 1));
	}
	if (error != 0) {
		device_printf(dev, "failed to save state: %d\n", error);
		goto out;
	}
	if (tpm_rc != 0) {
		device_printf(dev, "SaveState failed: TPM error 0x%x\n",
		    tpm_rc);
		error = EIO;
		goto out;
	}
	if (tries != 0)
		device_printf(dev, "SaveState required %d retries\n", tries);
#ifdef TPM_DEBUG
	device_printf(dev, "suspend: %d -> 1\n", sc->sc_suspend);
#endif
	sc->sc_suspend = 1;
	error = 0;

out:
	sx_xunlock(&sc->sc_lock);
#if defined TPM_HARVEST || defined RANDOM_ENABLE_TPM
	if (error != 0 && sc->sc_harvest_registered)
		taskqueue_enqueue_timeout(taskqueue_thread,
		    &sc->sc_harvest_task, hz * TPM_HARVEST_INTERVAL);
#endif
	return (error);
}

/* Handle resume after firmware has restored the saved TPM state. */
int
tpm_resume(device_t dev)
{
	struct tpm_softc *sc;
	int error;

	sc = device_get_softc(dev);
	sx_xlock(&sc->sc_lock);
	if (sc->sc_dying) {
		error = ENXIO;
		goto out;
	}
	error = 0;
	if (sc->sc_suspend != 0 && sc->sc_init == tpm_tis12_init)
		error = tpm_tis12_resume(sc);
#ifdef TPM_DEBUG
	device_printf(dev, "resume: %d -> 0\n", sc->sc_suspend);
#endif
	if (error == 0)
		sc->sc_suspend = 0;
	if (error != 0)
		device_printf(dev, "failed to restore TIS state: %d\n", error);

out:
	sx_xunlock(&sc->sc_lock);
#if defined TPM_HARVEST || defined RANDOM_ENABLE_TPM
	if (error == 0 && sc->sc_harvest_registered)
		taskqueue_enqueue_timeout(taskqueue_thread,
		    &sc->sc_harvest_task, hz * TPM_HARVEST_INTERVAL);
#endif
	return (error);
}

/* Dispatch suspend and resume events. */

/* Wait for given status bits using polling. */
int
tpm_waitfor_poll(struct tpm_softc *sc, u_int8_t mask, sbintime_t deadline)
{
	int rv;

	sx_assert(&sc->sc_lock, SA_XLOCKED);
	for (;;) {
		sc->sc_stat = tpm_status(sc);
		if ((sc->sc_stat & mask) == mask)
			return (0);
		if (sbinuptime() >= deadline)
			return (EWOULDBLOCK);
		rv = pause_sig("tpm_poll", 1);
		if (rv != 0 && rv != EWOULDBLOCK)
			return (rv);
	}
}

/* Use bounded sleeps so a lost interrupt still permits polling progress. */
int
tpm_waitfor_int(struct tpm_softc *sc, u_int8_t mask, sbintime_t deadline)
{
	int rv;

	sx_assert(&sc->sc_lock, SA_XLOCKED);
	mtx_lock(&sc->sc_intr_lock);
	for (;;) {
		sc->sc_stat = tpm_status(sc);
		if ((sc->sc_stat & mask) == mask) {
			rv = 0;
			break;
		}
		if (sbinuptime() >= deadline) {
			tpm_tis12_intr_failed(sc);
			rv = EWOULDBLOCK;
			break;
		}
		rv = cv_timedwait_sig_sbt(&sc->sc_intr_cv, &sc->sc_intr_lock,
		    MIN(deadline, sbinuptime() + mstosbt(TPM_IRQ_POLL_MS)), 0,
		    C_ABSOLUTE | C_HARDCLOCK);
		if (rv != 0 && rv != EWOULDBLOCK)
			break;
	}
	mtx_unlock(&sc->sc_intr_lock);
	return (rv);
}

/*
 * Wait on given status bits, uses interrupts where possible, otherwise polls.
 */
int
tpm_waitfor(struct tpm_softc *sc, u_int8_t mask, int tmo)
{
	sbintime_t deadline;
	bool interrupts;
	int retries, rv;

	sx_assert(&sc->sc_lock, SA_XLOCKED);
	mtx_lock(&sc->sc_intr_lock);
	interrupts = sc->sc_interrupts;
	mtx_unlock(&sc->sc_intr_lock);
	/* Keep interrupt timeouts out of the polling response-retry loop. */
	if (interrupts && (mask & TPM_STS_DATA_AVAIL) != 0) {
		deadline = sbinuptime() + mstosbt(tmo);
		return (tpm_waitfor_int(sc, mask, deadline));
	}
	for (retries = 3;; retries--) {
		deadline = sbinuptime() + mstosbt(tmo);
		rv = tpm_waitfor_poll(sc, mask, deadline);
		if (rv != EWOULDBLOCK)
			return (rv);
		if (retries == 0 || (mask & TPM_STS_VALID) == 0)
			return (EIO);
		/* Retry the complete response, including its VALID indication. */
		bus_space_write_1(sc->sc_bt, sc->sc_bh, TPM_STS,
		    TPM_STS_RESP_RETRY);
	}
}

/* Start transaction. */
int
tpm_tis12_start(struct tpm_softc *sc, int flag)
{
	int rv;

	sx_assert(&sc->sc_lock, SA_XLOCKED);
	if (flag == UIO_READ) {
		if (!sc->sc_locality || !sc->sc_command_pending) {
			tpm_tis12_abort(sc);
			return (EIO);
		}
		rv = tpm_waitfor(sc, TPM_STS_DATA_AVAIL | TPM_STS_VALID,
		    TPM_READ_TMO);
		if (rv != 0)
			tpm_tis12_abort(sc);
		else
			tpm_tis12_finish_intr(sc);
		return (rv);
	}

	/* Abort an incomplete command before starting another one. */
	tpm_tis12_abort(sc);

	/* Own our (0th) locality. */
	if ((rv = tpm_request_locality(sc, 0)) != 0)
		return (rv);

	sc->sc_stat = tpm_status(sc);
	if (sc->sc_stat & TPM_STS_CMD_READY) {
#ifdef TPM_DEBUG
		printf("tpm_tis12_start: UIO_WRITE status %b\n", sc->sc_stat,
		   TPM_STS_BITS);
#endif
		return 0;
	}

#ifdef TPM_DEBUG
	printf("tpm_tis12_start: UIO_WRITE readying chip\n");
#endif

	/* Abort previous and restart. */
	bus_space_write_1(sc->sc_bt, sc->sc_bh, TPM_STS, TPM_STS_CMD_READY);
	if ((rv = tpm_waitfor(sc, TPM_STS_CMD_READY, TPM_READY_TMO))) {
#ifdef TPM_DEBUG
		printf("tpm_tis12_start: UIO_WRITE readying failed %d\n", rv);
#endif
		tpm_tis12_abort(sc);
		return (rv);
	}

#ifdef TPM_DEBUG
	printf("tpm_tis12_start: UIO_WRITE readying done\n");
#endif

	return 0;
}

int
tpm_tis12_read(struct tpm_softc *sc, void *buf, int len, size_t *count,
    int flags)
{
	u_int8_t *p = buf;
	size_t cnt;
	int rv, n, bcnt;

#ifdef TPM_DEBUG
	printf("tpm_tis12_read: len %d\n", len);
#endif
	sx_assert(&sc->sc_lock, SA_XLOCKED);
	cnt = 0;
	while (len > 0) {
		if ((rv = tpm_waitfor(sc, TPM_STS_DATA_AVAIL | TPM_STS_VALID,
		    TPM_READ_TMO)))
			return rv;

		rv = tpm_getburst(sc, &bcnt);
		if (rv != 0)
			return (rv);
		n = MIN(len, bcnt);
#ifdef TPM_DEBUG
		printf("tpm_tis12_read: fetching %d, burst is %d\n", n, bcnt);
#endif
		for (; n--; len--) {
			*p++ = bus_space_read_1(sc->sc_bt, sc->sc_bh, TPM_DATA);
			cnt++;
		}

		if ((flags & TPM_PARAM_SIZE) == 0 && cnt >= 6)
			break;
	}
#ifdef TPM_DEBUG
	printf("tpm_tis12_read: read %zd bytes, len %d\n", cnt, len);
#endif

	if (count)
		*count = cnt;

	return 0;
}

int
tpm_tis12_write(struct tpm_softc *sc, void *buf, int len)
{
	u_int8_t *p = buf;
	size_t cnt;
	int rv, r;

#ifdef TPM_DEBUG
	printf("tpm_tis12_write: sc %p buf %p len %d\n", sc, buf, len);
#endif

	sx_assert(&sc->sc_lock, SA_XLOCKED);
	if (!sc->sc_locality)
		return (EIO);

	cnt = 0;
	while (cnt < len - 1) {
		rv = tpm_getburst(sc, &r);
		if (rv != 0)
			return (rv);
		for (; r > 0 && cnt < len - 1; r--) {
			bus_space_write_1(sc->sc_bt, sc->sc_bh, TPM_DATA, *p++);
			cnt++;
		}
		if ((rv = tpm_waitfor(sc, TPM_STS_VALID, TPM_READ_TMO))) {
#ifdef TPM_DEBUG
			printf("tpm_tis12_write: failed burst rv %d\n", rv);
#endif
			return rv;
		}
		sc->sc_stat = tpm_status(sc);
		if (!(sc->sc_stat & TPM_STS_DATA_EXPECT)) {
#ifdef TPM_DEBUG
			printf("tpm_tis12_write: failed rv %d stat=%b\n", rv,
			    sc->sc_stat, TPM_STS_BITS);
#endif
			return EIO;
		}
	}

	bus_space_write_1(sc->sc_bt, sc->sc_bh, TPM_DATA, *p++);
	cnt++;

	if ((rv = tpm_waitfor(sc, TPM_STS_VALID, TPM_READ_TMO))) {
#ifdef TPM_DEBUG
		printf("tpm_tis12_write: failed last byte rv %d\n", rv);
#endif
		return rv;
	}
	if ((sc->sc_stat & TPM_STS_DATA_EXPECT) != 0) {
#ifdef TPM_DEBUG
		printf("tpm_tis12_write: failed rv %d stat=%b\n", rv,
		    sc->sc_stat, TPM_STS_BITS);
#endif
		return EIO;
	}

#ifdef TPM_DEBUG
	printf("tpm_tis12_write: wrote %d byte\n", cnt);
#endif

	return 0;
}

/* Finish transaction. */
int
tpm_tis12_end(struct tpm_softc *sc, int flag, int err)
{
	int rv = 0;

	sx_assert(&sc->sc_lock, SA_XLOCKED);
	if (flag == UIO_READ) {
		if (!sc->sc_locality) {
			tpm_tis12_abort(sc);
			return (err != 0 ? 0 : EIO);
		}
		if (err == 0)
			rv = tpm_waitfor(sc, TPM_STS_VALID, TPM_READ_TMO);

		/* Still more data? */
		if (err == 0 && rv == 0)
			sc->sc_stat = tpm_status(sc);
		if (err == 0 && rv == 0 &&
		    (sc->sc_stat & TPM_STS_DATA_AVAIL) != 0) {
#ifdef TPM_DEBUG
			printf("tpm_tis12_end: read failed stat=%b\n",
			    sc->sc_stat, TPM_STS_BITS);
#endif
			rv = EIO;
		}
		tpm_tis12_abort(sc);
	} else {
		if (!sc->sc_locality) {
			tpm_tis12_abort(sc);
			return (err != 0 ? 0 : EIO);
		}
		/* Hungry for more? */
		sc->sc_stat = tpm_status(sc);
		if (!err && (sc->sc_stat & TPM_STS_DATA_EXPECT)) {
#ifdef TPM_DEBUG
			printf("tpm_tis12_end: write failed stat=%b\n",
			    sc->sc_stat, TPM_STS_BITS);
#endif
			rv = EIO;
		}

		if (err != 0 || rv != 0)
			tpm_tis12_abort(sc);
		else {
			tpm_tis12_arm_intr(sc);
			bus_space_write_1(sc->sc_bt, sc->sc_bh, TPM_STS,
			    TPM_STS_GO);
			sc->sc_command_pending = true;
		}
	}

	return rv;
}

void
tpm_intr(void *v)
{
	struct tpm_softc *sc = v;
	uint32_t status, enable;
	bool disabled;

	mtx_lock(&sc->sc_intr_lock);
	status = bus_space_read_4(sc->sc_bt, sc->sc_bh, TPM_INT_STATUS);
	enable = bus_space_read_4(sc->sc_bt, sc->sc_bh, TPM_INTERRUPT_ENABLE);
	if ((enable & TPM_GLOBAL_INT_ENABLE) == 0 ||
	    (status & enable & TPM_IRQ_EVENTS) == 0) {
		mtx_unlock(&sc->sc_intr_lock);
		return;
	}
	/* Mask on the first event before acknowledging a stuck source. */
	disabled = sc->sc_locality && tpm_tis12_mask_intr(sc);
	if (disabled && sc->sc_intr_armed &&
	    (status & TPM_DATA_AVAIL_INT) != 0)
		sc->sc_intr_received = true;
	else if (!disabled || !sc->sc_intr_configuring)
		tpm_tis12_intr_failed(sc);
	sc->sc_intr_armed = false;
	cv_broadcast(&sc->sc_intr_cv);
	mtx_unlock(&sc->sc_intr_lock);
	/* Keep the host source masked while command and cleanup threads run. */
	if (!disabled)
		pause("tpmirq", 1);
}

/* Read single byte using legacy interface. */
static inline u_int8_t
tpm_legacy_in(bus_space_tag_t iot, bus_space_handle_t ioh, int reg)
{
	bus_space_write_1(iot, ioh, 0, reg);
	return bus_space_read_1(iot, ioh, 1);
}

#if 0
/* Write single byte using legacy interface. */
static inline void
tpm_legacy_out(bus_space_tag_t iot, bus_space_handle_t ioh, int reg, u_int8_t v)
{
	bus_space_write_1(iot, ioh, 0, reg);
	bus_space_write_1(iot, ioh, 1, v);
}
#endif

/* Probe for TPM using legacy interface. */
int
tpm_legacy_probe(bus_space_tag_t iot, bus_addr_t iobase)
{
	bus_space_handle_t ioh;
	u_int8_t r, v;
	int i, rv = 0;
	char id[8];

	if (!tpm_enabled || iobase == -1)
		return 0;

	if (bus_space_map(iot, iobase, 2, 0, &ioh))
		return 0;

	v = bus_space_read_1(iot, ioh, 0);
	if (v == 0xff) {
		bus_space_unmap(iot, ioh, 2);
		return 0;
	}
	r = bus_space_read_1(iot, ioh, 1);

	for (i = sizeof(id); i--; )
		id[i] = tpm_legacy_in(iot, ioh, TPM_ID + i);

#ifdef TPM_DEBUG
	printf("tpm_legacy_probe %.4s %d.%d.%d.%d\n",
	    &id[4], id[0], id[1], id[2], id[3]);
#endif
	/*
	 * The only chips using the legacy interface we are aware of are
	 * by Atmel.  For other chips more signature would have to be added.
	 */
	if (!bcmp(&id[4], "ATML", 4))
		rv = 1;

	if (!rv) {
		bus_space_write_1(iot, ioh, r, 1);
		bus_space_write_1(iot, ioh, v, 0);
	}
	bus_space_unmap(iot, ioh, 2);

	return rv;
}

/* Setup TPM using legacy interface. */
int
tpm_legacy_init(struct tpm_softc *sc, const char *name)
{
	char id[8];
	int i;

	sx_assert(&sc->sc_lock, SA_XLOCKED);
	if ((i = bus_space_map(sc->sc_batm, tpm_enabled, 2, 0, &sc->sc_bahm))) {
		printf(": cannot map tpm registers (%d)\n", i);
		tpm_enabled = 0;
		return 1;
	}

	for (i = sizeof(id); i--; )
		id[i] = tpm_legacy_in(sc->sc_bt, sc->sc_bh, TPM_ID + i);

	printf(": %.4s %d.%d @0x%x\n", &id[4], id[0], id[1], tpm_enabled);
	tpm_enabled = 0;

	return 0;
}

/* Start transaction. */
int
tpm_legacy_start(struct tpm_softc *sc, int flag)
{
	struct timeval tv;
	u_int8_t bits, r;
	int to, rv;

	sx_assert(&sc->sc_lock, SA_XLOCKED);
	bits = flag == UIO_READ ? TPM_LEGACY_DA : 0;
	tv.tv_sec = TPM_LEGACY_TMO;
	tv.tv_usec = 0;
	to = tvtohz(&tv) / TPM_LEGACY_SLEEP;
	while (((r = bus_space_read_1(sc->sc_batm, sc->sc_bahm, 1)) &
	    (TPM_LEGACY_BUSY|bits)) != bits && to--) {
		rv = tsleep(sc, PRIBIO | PCATCH, "legacy_tpm_start",
		    TPM_LEGACY_SLEEP);
		if (rv && rv != EWOULDBLOCK)
			return rv;
	}

	if ((r & (TPM_LEGACY_BUSY|bits)) != bits)
		return EIO;

	return 0;
}

int
tpm_legacy_read(struct tpm_softc *sc, void *buf, int len, size_t *count,
    int flags)
{
	u_int8_t *p;
	size_t cnt;
	int to, rv;

	sx_assert(&sc->sc_lock, SA_XLOCKED);
	cnt = rv = 0;
	for (p = buf; !rv && len > 0; len--) {
		for (to = 1000;
		    !(bus_space_read_1(sc->sc_batm, sc->sc_bahm, 1) &
		    TPM_LEGACY_DA); DELAY(1))
			if (!to--)
				return EIO;

		DELAY(TPM_LEGACY_DELAY);
		*p++ = bus_space_read_1(sc->sc_batm, sc->sc_bahm, 0);
		cnt++;
	}

	*count = cnt;
	return 0;
}

int
tpm_legacy_write(struct tpm_softc *sc, void *buf, int len)
{
	u_int8_t *p;
	int n;

	sx_assert(&sc->sc_lock, SA_XLOCKED);
	for (p = buf, n = len; n--; DELAY(TPM_LEGACY_DELAY)) {
		if (!n && len != TPM_BUFSIZ) {
			bus_space_write_1(sc->sc_batm, sc->sc_bahm, 1,
			    TPM_LEGACY_LAST);
			DELAY(TPM_LEGACY_DELAY);
		}
		bus_space_write_1(sc->sc_batm, sc->sc_bahm, 0, *p++);
	}

	return 0;
}

/* Finish transaction. */
int
tpm_legacy_end(struct tpm_softc *sc, int flag, int rv)
{
	struct timeval tv;
	u_int8_t r;
	int to;

	sx_assert(&sc->sc_lock, SA_XLOCKED);
	if (rv || flag == UIO_READ)
		bus_space_write_1(sc->sc_batm, sc->sc_bahm, 1, TPM_LEGACY_ABRT);
	else {
		tv.tv_sec = TPM_LEGACY_TMO;
		tv.tv_usec = 0;
		to = tvtohz(&tv) / TPM_LEGACY_SLEEP;
		while(((r = bus_space_read_1(sc->sc_batm, sc->sc_bahm, 1)) &
		    TPM_LEGACY_BUSY) && to--) {
			rv = tsleep(sc, PRIBIO | PCATCH, "legacy_tpm_end",
			    TPM_LEGACY_SLEEP);
			if (rv && rv != EWOULDBLOCK)
				return rv;
		}

		if (r & TPM_LEGACY_BUSY)
			return EIO;

		if (r & TPM_LEGACY_RE)
			return EIO;	/* XXX Retry the loop? */
	}

	return rv;
}

int
tpmopen(struct cdev *dev, int flag, int mode, struct thread *td)
{
	struct tpm_softc *sc;
	int error;

	sc = TPMSOFTC(dev);
	if (sc == NULL)
		return (ENXIO);

	sx_xlock(&sc->sc_lock);
	if (sc->sc_dying)
		error = ENXIO;
	else if (sc->sc_suspend != 0 || (sc->sc_flags & TPM_OPEN) != 0)
		error = EBUSY;
	else {
		error = devfs_set_cdevpriv(sc, tpm_dtor);
		if (error == 0)
			sc->sc_flags |= TPM_OPEN;
	}
	sx_xunlock(&sc->sc_lock);

	return (error);
}

static void
tpm_dtor(void *arg)
{
	struct tpm_softc *sc;

	sc = arg;
	sx_xlock(&sc->sc_lock);
	if (sc->sc_init == tpm_tis12_init)
		tpm_tis12_abort(sc);
	sc->sc_flags &= ~TPM_OPEN;
	sx_xunlock(&sc->sc_lock);
}

int
tpmread(struct cdev *dev, struct uio *uio, int flags)
{
	struct tpm_softc *sc;
	u_int8_t header[TPM_HDRSIZE], *buf;
	size_t cnt, len;
	int end_error, rv;

	sc = TPMSOFTC(dev);
	if (sc == NULL)
		return (ENXIO);
	buf = NULL;

	sx_xlock(&sc->sc_lock);
	if (sc->sc_dying) {
		rv = ENXIO;
		goto out;
	}
	if (sc->sc_suspend != 0) {
		rv = EBUSY;
		goto out;
	}
	rv = sc->sc_start(sc, UIO_READ);
	if (rv != 0)
		goto out;

#ifdef TPM_DEBUG
	printf("tpmread: getting header\n");
#endif
	rv = sc->sc_read(sc, header, sizeof(header), &cnt,
	    TPM_PARAM_SIZE);
	if (rv != 0)
		goto end;
	if (cnt != sizeof(header)) {
		rv = EIO;
		goto end;
	}

	len = be32dec(header + 2);
#ifdef TPM_DEBUG
	printf("tpmread: len %zu, io count %zd\n", len, uio->uio_resid);
#endif
	if (len < sizeof(header) || len > uio->uio_resid || len > INT_MAX) {
		rv = EIO;
#ifdef TPM_DEBUG
		printf("tpmread: invalid response length %zu\n", len);
#endif
		goto end;
	}

	/*
	 * Finish the device transaction before touching user memory.  Use a
	 * non-blocking allocation so lifecycle operations are not held up by
	 * memory pressure while waiting for the transaction lock.
	 */
	buf = malloc(len, M_TPM, M_NOWAIT);
	if (buf == NULL) {
		rv = ENOMEM;
		goto end;
	}
	memcpy(buf, header, sizeof(header));

	if (len > sizeof(header)) {
		rv = sc->sc_read(sc, buf + sizeof(header),
		    (int)(len - sizeof(header)), NULL, TPM_PARAM_SIZE);
		if (rv != 0)
			goto end;
	}

end:
	end_error = sc->sc_end(sc, UIO_READ, rv);
	if (rv == 0)
		rv = end_error;
out:
	sx_xunlock(&sc->sc_lock);
	if (rv == 0)
		rv = uiomove(buf, (int)len, uio);
	free(buf, M_TPM);
	return (rv);
}

int
tpmwrite(struct cdev *dev, struct uio *uio, int flags)
{
	struct tpm_softc *sc;
	u_int8_t buf[TPM_BUFSIZ];
	ssize_t resid;
	int end_error, n, rv;

	sc = TPMSOFTC(dev);
	if (sc == NULL)
		return (ENXIO);

	resid = uio->uio_resid;
	n = MIN(sizeof(buf), resid);
	rv = uiomove(buf, n, uio);
	if (rv != 0)
		return (rv);

	sx_xlock(&sc->sc_lock);
	if (sc->sc_dying) {
		rv = ENXIO;
		goto out;
	}
	if (sc->sc_suspend != 0) {
		rv = EBUSY;
		goto out;
	}

#ifdef TPM_DEBUG
	printf("tpmwrite: io count %d\n", n);
#endif

	rv = sc->sc_start(sc, UIO_WRITE);
	if (rv != 0)
		goto out;

	rv = sc->sc_write(sc, buf, n);
	end_error = sc->sc_end(sc, UIO_WRITE, rv);
	if (rv == 0)
		rv = end_error;

out:
	sx_xunlock(&sc->sc_lock);
	if (rv != 0)
		uio->uio_resid = resid;
	return (rv);
}

int
tpmioctl(struct cdev *dev, u_long cmd, caddr_t data, int flags,
    struct thread *td)
{
	return ENOTTY;
}
