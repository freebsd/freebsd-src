/*-
 * Copyright (c) 2003 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/fcntl.h>
#include <sys/interrupt.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/reboot.h>
#include <sys/sysctl.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>
#include <machine/stdarg.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_bus.h>
#include <dev/uart/uart_cpu.h>

#include "uart_if.h"

devclass_t uart_devclass;
const char uart_driver_name[] = "uart";

SLIST_HEAD(uart_devinfo_list, uart_devinfo) uart_sysdevs =
    SLIST_HEAD_INITIALIZER(uart_sysdevs);

static MALLOC_DEFINE(M_UART, "UART", "UART driver");

#ifndef	UART_POLL_FREQ
#define	UART_POLL_FREQ		50
#endif
static int uart_poll_freq = UART_POLL_FREQ;
TUNABLE_INT("debug.uart_poll_freq", &uart_poll_freq);

#define	PPS_MODE_DISABLED	0
#define	PPS_MODE_CTS		1
#define	PPS_MODE_DCD		2

static inline int
uart_pps_signal(int pps_mode)
{

	switch(pps_mode) {
	case PPS_MODE_CTS:
		return (SER_CTS);
	case PPS_MODE_DCD:
		return (SER_DCD);
	}
	return (0);
}
static inline int
uart_pps_mode_valid(int pps_mode)
{

	switch(pps_mode) {
	case PPS_MODE_DISABLED:
	case PPS_MODE_CTS:
	case PPS_MODE_DCD:
		return (true);
	}
	return (false);
}

static const char *
uart_pps_mode_name(int pps_mode)
{
	switch(pps_mode) {
	case PPS_MODE_DISABLED:
		return ("disabled");
	case PPS_MODE_CTS:
		return ("CTS");
	case PPS_MODE_DCD:
		return ("DCD");
	}
	return ("invalid");
}

static int
uart_pps_mode_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct uart_softc *sc;
	int err, tmp;

	sc = arg1;
	tmp = sc->sc_pps_mode;
	err = sysctl_handle_int(oidp, &tmp, 0, req);
	if (err != 0 || req->newptr == NULL)
		return (err);
	if (!uart_pps_mode_valid(tmp))
		return (EINVAL);
	sc->sc_pps_mode = tmp;
	return(0);
}

static void
uart_pps_init(struct uart_softc *sc)
{
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree;

	ctx = device_get_sysctl_ctx(sc->sc_dev);
	tree = device_get_sysctl_tree(sc->sc_dev);

	/*
	 * The historical default for pps capture mode is either DCD or CTS,
	 * depending on the UART_PPS_ON_CTS kernel option.  Start with that,
	 * then try to fetch the tunable that overrides the mode for all uart
	 * devices, then try to fetch the sysctl-tunable that overrides the mode
	 * for one specific device.
	 */
#ifdef UART_PPS_ON_CTS
	sc->sc_pps_mode = PPS_MODE_CTS;
#else
	sc->sc_pps_mode = PPS_MODE_DCD;
#endif
	TUNABLE_INT_FETCH("hw.uart.pps_mode", &sc->sc_pps_mode);
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "pps_mode",
	    CTLTYPE_INT | CTLFLAG_RWTUN, sc, 0, uart_pps_mode_sysctl, "I",
	    "pulse capturing mode - 0/1/2 - disabled/CTS/DCD");

	if (!uart_pps_mode_valid(sc->sc_pps_mode)) {
		device_printf(sc->sc_dev, 
		    "Invalid pps_mode %d configured; disabling PPS capture\n",
		    sc->sc_pps_mode);
		sc->sc_pps_mode = PPS_MODE_DISABLED;
	} else if (bootverbose) {
		device_printf(sc->sc_dev, "PPS capture mode %d (%s)\n",
		    sc->sc_pps_mode, uart_pps_mode_name(sc->sc_pps_mode));
	}

	sc->sc_pps.ppscap = PPS_CAPTUREBOTH;
	sc->sc_pps.driver_mtx = uart_tty_getlock(sc);
	sc->sc_pps.driver_abi = PPS_ABI_VERSION;
	pps_init_abi(&sc->sc_pps);
}

void
uart_add_sysdev(struct uart_devinfo *di)
{
	SLIST_INSERT_HEAD(&uart_sysdevs, di, next);
}

const char *
uart_getname(struct uart_class *uc)
{
	return ((uc != NULL) ? uc->name : NULL);
}

struct uart_ops *
uart_getops(struct uart_class *uc)
{
	return ((uc != NULL) ? uc->uc_ops : NULL);
}

int
uart_getrange(struct uart_class *uc)
{
	return ((uc != NULL) ? uc->uc_range : 0);
}

/*
 * Schedule a soft interrupt. We do this on the 0 to !0 transition
 * of the TTY pending interrupt status.
 */
void
uart_sched_softih(struct uart_softc *sc, uint32_t ipend)
{
	uint32_t new, old;

	do {
		old = sc->sc_ttypend;
		new = old | ipend;
	} while (!atomic_cmpset_32(&sc->sc_ttypend, old, new));

	if ((old & SER_INT_MASK) == 0)
		swi_sched(sc->sc_softih, 0);
}

/*
 * A break condition has been detected. We treat the break condition as
 * a special case that should not happen during normal operation. When
 * the break condition is to be passed to higher levels in the form of
 * a NUL character, we really want the break to be in the right place in
 * the input stream. The overhead to achieve that is not in relation to
 * the exceptional nature of the break condition, so we permit ourselves
 * to be sloppy.
 */
static __inline int
uart_intr_break(void *arg)
{
	struct uart_softc *sc = arg;

#if defined(KDB)
	if (sc->sc_sysdev != NULL && sc->sc_sysdev->type == UART_DEV_CONSOLE) {
		if (kdb_break())
			return (0);
	}
#endif
	if (sc->sc_opened)
		uart_sched_softih(sc, SER_INT_BREAK);
	return (0);
}

/*
 * Handle a receiver overrun situation. We lost at least 1 byte in the
 * input stream and it's our job to contain the situation. We grab as
 * much of the data we can, but otherwise flush the receiver FIFO to
 * create some breathing room. The net effect is that we avoid the
 * overrun condition to happen for the next X characters, where X is
 * related to the FIFO size at the cost of losing data right away.
 * So, instead of having multiple overrun interrupts in close proximity
 * to each other and possibly pessimizing UART interrupt latency for
 * other UARTs in a multiport configuration, we create a longer segment
 * of missing characters by freeing up the FIFO.
 * Each overrun condition is marked in the input buffer by a token. The
 * token represents the loss of at least one, but possible more bytes in
 * the input stream.
 */
static __inline int
uart_intr_overrun(void *arg)
{
	struct uart_softc *sc = arg;

	if (sc->sc_opened) {
		UART_RECEIVE(sc);
		if (uart_rx_put(sc, UART_STAT_OVERRUN))
			sc->sc_rxbuf[sc->sc_rxput] = UART_STAT_OVERRUN;
		uart_sched_softih(sc, SER_INT_RXREADY);
	}
	UART_FLUSH(sc, UART_FLUSH_RECEIVER);
	return (0);
}

/*
 * Received data ready.
 */
static __inline int
uart_intr_rxready(void *arg)
{
	struct uart_softc *sc = arg;
	int rxp;

	rxp = sc->sc_rxput;
	UART_RECEIVE(sc);
#if defined(KDB)
	if (sc->sc_sysdev != NULL && sc->sc_sysdev->type == UART_DEV_CONSOLE) {
		while (rxp != sc->sc_rxput) {
			kdb_alt_break(sc->sc_rxbuf[rxp++], &sc->sc_altbrk);
			if (rxp == sc->sc_rxbufsz)
				rxp = 0;
		}
	}
#endif
	if (sc->sc_opened)
		uart_sched_softih(sc, SER_INT_RXREADY);
	else
		sc->sc_rxput = sc->sc_rxget;	/* Ignore received data. */
	return (1);
}

/*
 * Line or modem status change (OOB signalling).
 * We pass the signals to the software interrupt handler for further
 * processing. Note that we merge the delta bits, but set the state
 * bits. This is to avoid losing state transitions due to having more
 * than 1 hardware interrupt between software interrupts.
 */
static __inline int
uart_intr_sigchg(void *arg)
{
	struct uart_softc *sc = arg;
	int new, old, pps_sig, sig;

	sig = UART_GETSIG(sc);

	/*
	 * Time pulse counting support. Note that both CTS and DCD are
	 * active-low signals. The status bit is high to indicate that
	 * the signal on the line is low, which corresponds to a PPS
	 * clear event.
	 */
	if (sc->sc_pps.ppsparam.mode & PPS_CAPTUREBOTH) {
		pps_sig = uart_pps_signal(sc->sc_pps_mode);
		if (sig & SER_DELTA(pps_sig)) {
			pps_capture(&sc->sc_pps);
			pps_event(&sc->sc_pps, (sig & pps_sig) ?
			    PPS_CAPTURECLEAR : PPS_CAPTUREASSERT);
		}
	}

	/*
	 * Keep track of signal changes, even when the device is not
	 * opened. This allows us to inform upper layers about a
	 * possible loss of DCD and thus the existence of a (possibly)
	 * different connection when we have DCD back, during the time
	 * that the device was closed.
	 */
	do {
		old = sc->sc_ttypend;
		new = old & ~SER_MASK_STATE;
		new |= sig & SER_INT_SIGMASK;
	} while (!atomic_cmpset_32(&sc->sc_ttypend, old, new));

	if (sc->sc_opened)
		uart_sched_softih(sc, SER_INT_SIGCHG);
	return (1);
}

/*
 * The transmitter can accept more data.
 */
static __inline int
uart_intr_txidle(void *arg)
{
	struct uart_softc *sc = arg;

	if (sc->sc_txbusy) {
		sc->sc_txbusy = 0;
		uart_sched_softih(sc, SER_INT_TXIDLE);
	}
	return (0);
}

static int
uart_intr(void *arg)
{
	struct uart_softc *sc = arg;
	int cnt, ipend, testintr;

	if (sc->sc_leaving)
		return (FILTER_STRAY);

	cnt = 0;
	testintr = sc->sc_testintr;
	while ((!testintr || cnt < 20) && (ipend = UART_IPEND(sc)) != 0) {
		cnt++;
		if (ipend & SER_INT_OVERRUN)
			uart_intr_overrun(sc);
		if (ipend & SER_INT_BREAK)
			uart_intr_break(sc);
		if (ipend & SER_INT_RXREADY)
			uart_intr_rxready(sc);
		if (ipend & SER_INT_SIGCHG)
			uart_intr_sigchg(sc);
		if (ipend & SER_INT_TXIDLE)
			uart_intr_txidle(sc);
	}

	if (sc->sc_polled) {
		callout_reset(&sc->sc_timer, hz / uart_poll_freq,
		    (timeout_t *)uart_intr, sc);
	}

	return ((cnt == 0) ? FILTER_STRAY :
	    ((testintr && cnt == 20) ? FILTER_SCHEDULE_THREAD :
	    FILTER_HANDLED));
}

serdev_intr_t *
uart_bus_ihand(device_t dev, int ipend)
{

	switch (ipend) {
	case SER_INT_BREAK:
		return (uart_intr_break);
	case SER_INT_OVERRUN:
		return (uart_intr_overrun);
	case SER_INT_RXREADY:
		return (uart_intr_rxready);
	case SER_INT_SIGCHG:
		return (uart_intr_sigchg);
	case SER_INT_TXIDLE:
		return (uart_intr_txidle);
	}
	return (NULL);
}

int
uart_bus_ipend(device_t dev)
{
	struct uart_softc *sc;

	sc = device_get_softc(dev);
	return (UART_IPEND(sc));
}

int
uart_bus_sysdev(device_t dev)
{
	struct uart_softc *sc;

	sc = device_get_softc(dev);
	return ((sc->sc_sysdev != NULL) ? 1 : 0);
}

int
uart_bus_probe(device_t dev, int regshft, int rclk, int rid, int chan)
{
	struct uart_softc *sc;
	struct uart_devinfo *sysdev;
	int error;

	sc = device_get_softc(dev);

	/*
	 * All uart_class references are weak. Check that the needed
	 * class has been compiled-in. Fail if not.
	 */
	if (sc->sc_class == NULL)
		return (ENXIO);

	/*
	 * Initialize the instance. Note that the instance (=softc) does
	 * not necessarily match the hardware specific softc. We can't do
	 * anything about it now, because we may not attach to the device.
	 * Hardware drivers cannot use any of the class specific fields
	 * while probing.
	 */
	kobj_init((kobj_t)sc, (kobj_class_t)sc->sc_class);
	sc->sc_dev = dev;
	if (device_get_desc(dev) == NULL)
		device_set_desc(dev, uart_getname(sc->sc_class));

	/*
	 * Allocate the register resource. We assume that all UARTs have
	 * a single register window in either I/O port space or memory
	 * mapped I/O space. Any UART that needs multiple windows will
	 * consequently not be supported by this driver as-is. We try I/O
	 * port space first because that's the common case.
	 */
	sc->sc_rrid = rid;
	sc->sc_rtype = SYS_RES_IOPORT;
	sc->sc_rres = bus_alloc_resource(dev, sc->sc_rtype, &sc->sc_rrid,
	    0, ~0, uart_getrange(sc->sc_class), RF_ACTIVE);
	if (sc->sc_rres == NULL) {
		sc->sc_rrid = rid;
		sc->sc_rtype = SYS_RES_MEMORY;
		sc->sc_rres = bus_alloc_resource(dev, sc->sc_rtype,
		    &sc->sc_rrid, 0, ~0, uart_getrange(sc->sc_class),
		    RF_ACTIVE);
		if (sc->sc_rres == NULL)
			return (ENXIO);
	}

	/*
	 * Fill in the bus access structure and compare this device with
	 * a possible console device and/or a debug port. We set the flags
	 * in the softc so that the hardware dependent probe can adjust
	 * accordingly. In general, you don't want to permanently disrupt
	 * console I/O.
	 */
	sc->sc_bas.bsh = rman_get_bushandle(sc->sc_rres);
	sc->sc_bas.bst = rman_get_bustag(sc->sc_rres);
	sc->sc_bas.chan = chan;
	sc->sc_bas.regshft = regshft;
	sc->sc_bas.rclk = (rclk == 0) ? sc->sc_class->uc_rclk : rclk;

	SLIST_FOREACH(sysdev, &uart_sysdevs, next) {
		if (chan == sysdev->bas.chan &&
		    uart_cpu_eqres(&sc->sc_bas, &sysdev->bas)) {
			/* XXX check if ops matches class. */
			sc->sc_sysdev = sysdev;
			sysdev->bas.rclk = sc->sc_bas.rclk;
		}
	}

	error = UART_PROBE(sc);
	bus_release_resource(dev, sc->sc_rtype, sc->sc_rrid, sc->sc_rres);
	return ((error) ? error : BUS_PROBE_DEFAULT);
}

int
uart_bus_attach(device_t dev)
{
	struct uart_softc *sc, *sc0;
	const char *sep;
	int error, filt;

	/*
	 * The sc_class field defines the type of UART we're going to work
	 * with and thus the size of the softc. Replace the generic softc
	 * with one that matches the UART now that we're certain we handle
	 * the device.
	 */
	sc0 = device_get_softc(dev);
	if (sc0->sc_class->size > sizeof(*sc)) {
		sc = malloc(sc0->sc_class->size, M_UART, M_WAITOK|M_ZERO);
		bcopy(sc0, sc, sizeof(*sc));
		device_set_softc(dev, sc);
	} else
		sc = sc0;

	/*
	 * Now that we know the softc for this device, connect the back
	 * pointer from the sysdev for this device, if any
	 */
	if (sc->sc_sysdev != NULL)
		sc->sc_sysdev->sc = sc;

	/*
	 * Protect ourselves against interrupts while we're not completely
	 * finished attaching and initializing. We don't expect interrupts
	 * until after UART_ATTACH(), though.
	 */
	sc->sc_leaving = 1;

	mtx_init(&sc->sc_hwmtx_s, "uart_hwmtx", NULL, MTX_SPIN);
	if (sc->sc_hwmtx == NULL)
		sc->sc_hwmtx = &sc->sc_hwmtx_s;

	/*
	 * Re-allocate. We expect that the softc contains the information
	 * collected by uart_bus_probe() intact.
	 */
	sc->sc_rres = bus_alloc_resource(dev, sc->sc_rtype, &sc->sc_rrid,
	    0, ~0, uart_getrange(sc->sc_class), RF_ACTIVE);
	if (sc->sc_rres == NULL) {
		mtx_destroy(&sc->sc_hwmtx_s);
		return (ENXIO);
	}
	sc->sc_bas.bsh = rman_get_bushandle(sc->sc_rres);
	sc->sc_bas.bst = rman_get_bustag(sc->sc_rres);

	/*
	 * Ensure there is room for at least three full FIFOs of data in the
	 * receive buffer (handles the case of low-level drivers with huge
	 * FIFOs), and also ensure that there is no less than the historical
	 * size of 384 bytes (handles the typical small-FIFO case).
	 */
	sc->sc_rxbufsz = MAX(384, sc->sc_rxfifosz * 3);
	sc->sc_rxbuf = malloc(sc->sc_rxbufsz * sizeof(*sc->sc_rxbuf),
	    M_UART, M_WAITOK);
	sc->sc_txbuf = malloc(sc->sc_txfifosz * sizeof(*sc->sc_txbuf),
	    M_UART, M_WAITOK);

	error = UART_ATTACH(sc);
	if (error)
		goto fail;

	if (sc->sc_hwiflow || sc->sc_hwoflow) {
		sep = "";
		device_print_prettyname(dev);
		if (sc->sc_hwiflow) {
			printf("%sRTS iflow", sep);
			sep = ", ";
		}
		if (sc->sc_hwoflow) {
			printf("%sCTS oflow", sep);
			sep = ", ";
		}
		printf("\n");
	}

	if (sc->sc_sysdev != NULL) {
		if (sc->sc_sysdev->baudrate == 0) {
			if (UART_IOCTL(sc, UART_IOCTL_BAUD,
			    (intptr_t)&sc->sc_sysdev->baudrate) != 0)
				sc->sc_sysdev->baudrate = -1;
		}
		switch (sc->sc_sysdev->type) {
		case UART_DEV_CONSOLE:
			device_printf(dev, "console");
			break;
		case UART_DEV_DBGPORT:
			device_printf(dev, "debug port");
			break;
		case UART_DEV_KEYBOARD:
			device_printf(dev, "keyboard");
			break;
		default:
			device_printf(dev, "unknown system device");
			break;
		}
		printf(" (%d,%c,%d,%d)\n", sc->sc_sysdev->baudrate,
		    "noems"[sc->sc_sysdev->parity], sc->sc_sysdev->databits,
		    sc->sc_sysdev->stopbits);
	}

	sc->sc_leaving = 0;
	sc->sc_testintr = 1;
	filt = uart_intr(sc);
	sc->sc_testintr = 0;

	/*
	 * Don't use interrupts if we couldn't clear any pending interrupt
	 * conditions. We may have broken H/W and polling is probably the
	 * safest thing to do.
	 */
	if (filt != FILTER_SCHEDULE_THREAD) {
		sc->sc_irid = 0;
		sc->sc_ires = bus_alloc_resource_any(dev, SYS_RES_IRQ,
		    &sc->sc_irid, RF_ACTIVE | RF_SHAREABLE);
	}
	if (sc->sc_ires != NULL) {
		error = bus_setup_intr(dev, sc->sc_ires, INTR_TYPE_TTY,
		    uart_intr, NULL, sc, &sc->sc_icookie);
		sc->sc_fastintr = (error == 0) ? 1 : 0;

		if (!sc->sc_fastintr)
			error = bus_setup_intr(dev, sc->sc_ires,
			    INTR_TYPE_TTY | INTR_MPSAFE, NULL,
			    (driver_intr_t *)uart_intr, sc, &sc->sc_icookie);

		if (error) {
			device_printf(dev, "could not activate interrupt\n");
			bus_release_resource(dev, SYS_RES_IRQ, sc->sc_irid,
			    sc->sc_ires);
			sc->sc_ires = NULL;
		}
	}
	if (sc->sc_ires == NULL) {
		/* No interrupt resource. Force polled mode. */
		sc->sc_polled = 1;
		callout_init(&sc->sc_timer, 1);
	}

	if (bootverbose && (sc->sc_fastintr || sc->sc_polled)) {
		sep = "";
		device_print_prettyname(dev);
		if (sc->sc_fastintr) {
			printf("%sfast interrupt", sep);
			sep = ", ";
		}
		if (sc->sc_polled) {
			printf("%spolled mode (%dHz)", sep, uart_poll_freq);
			sep = ", ";
		}
		printf("\n");
	}

	if (sc->sc_sysdev != NULL && sc->sc_sysdev->attach != NULL) {
		if ((error = sc->sc_sysdev->attach(sc)) != 0)
			goto fail;
	} else {
		if ((error = uart_tty_attach(sc)) != 0)
			goto fail;
		uart_pps_init(sc);
	}

	if (sc->sc_sysdev != NULL)
		sc->sc_sysdev->hwmtx = sc->sc_hwmtx;

	return (0);

 fail:
	free(sc->sc_txbuf, M_UART);
	free(sc->sc_rxbuf, M_UART);

	if (sc->sc_ires != NULL) {
		bus_teardown_intr(dev, sc->sc_ires, sc->sc_icookie);
		bus_release_resource(dev, SYS_RES_IRQ, sc->sc_irid,
		    sc->sc_ires);
	}
	bus_release_resource(dev, sc->sc_rtype, sc->sc_rrid, sc->sc_rres);

	mtx_destroy(&sc->sc_hwmtx_s);

	return (error);
}

int
uart_bus_detach(device_t dev)
{
	struct uart_softc *sc;

	sc = device_get_softc(dev);

	sc->sc_leaving = 1;

	if (sc->sc_sysdev != NULL)
		sc->sc_sysdev->hwmtx = NULL;

	UART_DETACH(sc);

	if (sc->sc_sysdev != NULL && sc->sc_sysdev->detach != NULL)
		(*sc->sc_sysdev->detach)(sc);
	else
		uart_tty_detach(sc);

	free(sc->sc_txbuf, M_UART);
	free(sc->sc_rxbuf, M_UART);

	if (sc->sc_ires != NULL) {
		bus_teardown_intr(dev, sc->sc_ires, sc->sc_icookie);
		bus_release_resource(dev, SYS_RES_IRQ, sc->sc_irid,
		    sc->sc_ires);
	}
	bus_release_resource(dev, sc->sc_rtype, sc->sc_rrid, sc->sc_rres);

	mtx_destroy(&sc->sc_hwmtx_s);

	if (sc->sc_class->size > sizeof(*sc)) {
		device_set_softc(dev, NULL);
		free(sc, M_UART);
	} else
		device_set_softc(dev, NULL);

	return (0);
}

int
uart_bus_resume(device_t dev)
{
	struct uart_softc *sc;

	sc = device_get_softc(dev);
	return (UART_ATTACH(sc));
}

void
uart_grab(struct uart_devinfo *di)
{

	if (di->sc)
		UART_GRAB(di->sc);
}

void
uart_ungrab(struct uart_devinfo *di)
{

	if (di->sc)
		UART_UNGRAB(di->sc);
}
