/*	$OpenBSD: sab.c,v 1.7 2002/04/08 17:49:42 jason Exp $	*/

/*
 * Copyright (c) 2001 Jason L. Wright (jason@thought.net)
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Jason L. Wright
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * Effort sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F30602-01-2-0537.
 *
 * $FreeBSD$
 */

/*
 * SAB82532 Dual UART driver
 */

#include "opt_ddb.h"
#include "opt_comconsole.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/fcntl.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>
#include <sys/syslog.h>
#include <sys/tty.h>

#include <ddb/ddb.h>

#include <ofw/openfirm.h>
#include <sparc64/ebus/ebusvar.h>

#include <dev/sab/sab82532reg.h>

#define	CDEV_MAJOR		168

#define	SAB_READ(sc, r) \
	bus_space_read_1((sc)->sc_bt, (sc)->sc_bh, (r))
#define	SAB_WRITE(sc, r, v) \
	bus_space_write_1((sc)->sc_bt, (sc)->sc_bh, (r), (v))

#define	SABTTY_LOCK(sc)		mtx_lock_spin(&sc->sc_mtx)
#define	SABTTY_UNLOCK(sc)	mtx_unlock_spin(&sc->sc_mtx)

struct sabtty_softc {
	device_t		sc_dev;
	struct sab_softc	*sc_parent;
	bus_space_tag_t		sc_bt;
	bus_space_handle_t	sc_bh;
	dev_t			sc_si;
	struct tty		*sc_tty;
	int			sc_channel;
	int			sc_icnt;
	uint8_t			*sc_iput;
	uint8_t			*sc_iget;
	int			sc_ocnt;
	uint8_t			*sc_oget;
	int			sc_alt_break_state;
	struct mtx		sc_mtx;
	uint8_t			sc_console;
	uint8_t			sc_tx_done;
	uint8_t			sc_pvr_dtr;
	uint8_t			sc_pvr_dsr;
	uint8_t			sc_imr0;
	uint8_t			sc_imr1;
	uint8_t			sc_ibuf[CBLOCK];
	uint8_t			sc_obuf[CBLOCK];
};

struct sab_softc {
	device_t		sc_dev;
	bus_space_tag_t		sc_bt;
	bus_space_handle_t	sc_bh;
	struct sabtty_softc	*sc_child[SAB_NCHAN];
	void			*sc_ih;
	void			*sc_softih;
	struct resource		*sc_irqres;
	int			sc_irqrid;
	struct resource		*sc_iores;
	int			sc_iorid;
	uint8_t			sc_ipc;
};

static int sab_probe(device_t dev);
static int sab_attach(device_t dev);
static int sab_detach(device_t dev);

static void sab_intr(void *vsc);
static void sab_softintr(void *vsc);
static void sab_shutdown(void *vsc);

static int sabtty_probe(device_t dev);
static int sabtty_attach(device_t dev);
static int sabtty_detach(device_t dev);

static int sabtty_intr(struct sabtty_softc *sc);
static void sabtty_softintr(struct sabtty_softc *sc);
static int sabtty_mdmctrl(struct sabtty_softc *sc, int bits, int how);
static int sabtty_param(struct sabtty_softc *sc, struct tty *tp,
    struct termios *t);
static void sabtty_cec_wait(struct sabtty_softc *sc);
static void sabtty_tec_wait(struct sabtty_softc *sc);
static void sabtty_reset(struct sabtty_softc *sc);
static void sabtty_flush(struct sabtty_softc *sc);
static int sabtty_speed(int);
static int sabtty_console(device_t dev, char *mode, int len);

static cn_probe_t sab_cnprobe;
static cn_init_t sab_cninit;
static cn_term_t sab_cnterm;
static cn_getc_t sab_cngetc;
static cn_checkc_t sab_cncheckc;
static cn_putc_t sab_cnputc;
static cn_dbctl_t sab_cndbctl;

static int sabtty_cngetc(struct sabtty_softc *sc);
static int sabtty_cncheckc(struct sabtty_softc *sc);
static void sabtty_cnputc(struct sabtty_softc *sc, int c);

static d_open_t sabttyopen;
static d_close_t sabttyclose;
static d_ioctl_t sabttyioctl;

static void sabttystart(struct tty *tp);
static void sabttystop(struct tty *tp, int rw);
static int sabttyparam(struct tty *tp, struct termios *t);

static struct cdevsw sabtty_cdevsw = {
	/* open */	sabttyopen,
	/* close */	sabttyclose,
	/* read */	ttyread,
	/* write */	ttywrite,
	/* ioctl */	sabttyioctl,
	/* poll */	ttypoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"sabtty",
	/* major */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	D_TTY | D_KQFILTER,
	/* kqfilter */	ttykqfilter,
};

static device_method_t sab_methods[] = {
	DEVMETHOD(device_probe,		sab_probe),
	DEVMETHOD(device_attach,	sab_attach),
	DEVMETHOD(device_detach,	sab_detach),

	DEVMETHOD(bus_print_child,	bus_generic_print_child),

	{ 0, 0 }
};

static device_method_t sabtty_methods[] = {
	DEVMETHOD(device_probe,		sabtty_probe),
	DEVMETHOD(device_attach,	sabtty_attach),
	DEVMETHOD(device_detach,	sabtty_detach),

	{ 0, 0 }
};

static driver_t sab_driver = {
	"sab",
	sab_methods,
	sizeof(struct sab_softc),
};

static driver_t sabtty_driver = {
	"sabtty",
	sabtty_methods,
	sizeof(struct sabtty_softc),
};

static devclass_t sab_devclass;
static devclass_t sabtty_devclass;

static struct sabtty_softc *sabtty_cons;

DRIVER_MODULE(sab, ebus, sab_driver, sab_devclass, 0, 0);
DRIVER_MODULE(sabtty, sab, sabtty_driver, sabtty_devclass, 0, 0);

CONS_DRIVER(sab, sab_cnprobe, sab_cninit, sab_cnterm, sab_cngetc,
    sab_cncheckc, sab_cnputc, sab_cndbctl);

struct sabtty_rate {
	int baud;
	int n, m;
};

struct sabtty_rate sabtty_baudtable[] = {
	{      50,	35,     10 },
	{      75,	47,	9 },
	{     110,	32,	9 },
	{     134,	53,	8 },
	{     150,	47,	8 },
	{     200,	35,	8 },
	{     300,	47,	7 },
	{     600,	47,	6 },
	{    1200,	47,	5 },
	{    1800,	31,	5 },
	{    2400,	47,	4 },
	{    4800,	47,	3 },
	{    9600,	47,	2 },
	{   19200,	47,	1 },
	{   38400,	23,	1 },
	{   57600,	15,	1 },
	{  115200,	 7,	1 },
	{  230400,	 3,	1 },
	{  460800,	 1,	1 },
	{   76800,	11,	1 },
	{  153600,	 5,	1 },
	{  307200,	 3,	1 },
	{  614400,	 3,	0 },
	{  921600,	 0,	1 },
};

static int
sab_probe(device_t dev)
{
	struct resource *res;
	bus_space_handle_t handle;
	bus_space_tag_t tag;
	uint8_t r;
	int rid;

	if (strcmp(ebus_get_name(dev), "se") != 0)
		return (ENXIO);
	rid = 0;
	res = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0, 1,
	    RF_ACTIVE);
	if (res == NULL)
		return (ENXIO);
	tag = rman_get_bustag(res);
	handle = rman_get_bushandle(res);
	r = bus_space_read_1(tag, handle, SAB_VSTR) & SAB_VSTR_VMASK;
	switch (r) {
	case SAB_VSTR_V_1:
		device_set_desc(dev, "Siemens SAB 82532 v1");
		break;
	case SAB_VSTR_V_2:
		device_set_desc(dev, "Siemens SAB 82532 v2");
		break;
	case SAB_VSTR_V_32:
		device_set_desc(dev, "Siemens SAB 82532 v3.2");
		break;
	default:
		device_set_desc(dev, "Siemens SAB 82532 ???");
		break;
	}
	bus_release_resource(dev, SYS_RES_IOPORT, rid, res);
	return (0);
}

static int
sab_attach(device_t dev)
{
	struct device *child[SAB_NCHAN];
	struct resource *irqres;
	struct resource *iores;
	struct sab_softc *sc;
	int irqrid;
	int iorid;
	int i;

	iorid = 0;
	irqrid = 0;
	irqres = NULL;
	sc = device_get_softc(dev);
	iores = bus_alloc_resource(dev, SYS_RES_IOPORT, &iorid, 0, ~0, 1,
	    RF_ACTIVE);
	if (iores == NULL)
		goto error;
	irqres = bus_alloc_resource(dev, SYS_RES_IRQ, &irqrid, 0, ~0, 1,
	    RF_ACTIVE);
	if (irqres == NULL)
		goto error;
	if (bus_setup_intr(dev, irqres, INTR_TYPE_TTY | INTR_FAST, sab_intr,
	    sc, &sc->sc_ih) != 0)
		goto error;
	sc->sc_dev = dev;
	sc->sc_irqres = irqres;
	sc->sc_iores = iores;
	sc->sc_iorid = iorid;
	sc->sc_irqres = irqres;
	sc->sc_irqrid = irqrid;
	sc->sc_bt = rman_get_bustag(iores);
	sc->sc_bh = rman_get_bushandle(iores);

	/* Set all pins, except DTR pins to be inputs */
	SAB_WRITE(sc, SAB_PCR, ~(SAB_PVR_DTR_A | SAB_PVR_DTR_B));
	/* Disable port interrupts */
	SAB_WRITE(sc, SAB_PIM, 0xff);
	SAB_WRITE(sc, SAB_PVR, SAB_PVR_DTR_A | SAB_PVR_DTR_B | SAB_PVR_MAGIC);
	sc->sc_ipc = SAB_IPC_ICPL | SAB_IPC_VIS;
	SAB_WRITE(sc, SAB_IPC, sc->sc_ipc);

	for (i = 0; i < SAB_NCHAN; i++)
		child[i] = device_add_child(dev, "sabtty", i);
	bus_generic_attach(dev);
	for (i = 0; i < SAB_NCHAN; i++)
		sc->sc_child[i] = device_get_softc(child[i]);

	swi_add(&tty_ithd, "tty:sab", sab_softintr, sc, SWI_TTY,
	    INTR_TYPE_TTY, &sc->sc_softih);

	if (sabtty_cons != NULL) {
		DELAY(50000);
		cninit();
	}

	EVENTHANDLER_REGISTER(shutdown_final, sab_shutdown, sc,
	    SHUTDOWN_PRI_DEFAULT);

	return (0);

error:
	if (irqres != NULL)
		bus_release_resource(dev, SYS_RES_IRQ, irqrid, irqres);
	if (iores != NULL)
		bus_release_resource(dev, SYS_RES_IRQ, iorid, iores);
	return (ENXIO);
}

static int
sab_detach(device_t dev)
{
	struct sab_softc *sc;

	sc = device_get_softc(dev);
	bus_generic_detach(dev);
	if (sc->sc_iores != NULL)
		bus_release_resource(dev, SYS_RES_IOPORT, sc->sc_iorid,
		    sc->sc_iores);
	return (0);
}

static void
sab_intr(void *v)
{
	struct sab_softc *sc = v;
	int needsoft = 0;
	uint8_t gis;

	gis = SAB_READ(sc, SAB_GIS);

	/* channel A */
	if ((gis & (SAB_GIS_ISA1 | SAB_GIS_ISA0)) != 0)
		needsoft |= sabtty_intr(sc->sc_child[0]);

	/* channel B */
	if ((gis & (SAB_GIS_ISB1 | SAB_GIS_ISB0)) != 0)
		needsoft |= sabtty_intr(sc->sc_child[1]);

	if (needsoft)
		swi_sched(sc->sc_softih, 0);
}

static void
sab_softintr(void *v)
{
	struct sab_softc *sc = v;

	sabtty_softintr(sc->sc_child[0]);
	sabtty_softintr(sc->sc_child[1]);
}

static void
sab_shutdown(void *v)
{
	struct sab_softc *sc = v;

	SAB_WRITE(sc, SAB_IPC, SAB_IPC_ICPL | SAB_IPC_VIS);
	SAB_WRITE(sc, SAB_RFC, SAB_READ(sc, SAB_RFC) & ~SAB_RFC_RFDF);
}

static int
sabtty_probe(device_t dev)
{

	if ((device_get_unit(dev) & 1) == 0)
		device_set_desc(dev, "ttya");
	else
		device_set_desc(dev, "ttyb");
	return (0);
}

static int
sabtty_attach(device_t dev)
{
	struct sabtty_softc *sc;
	struct tty *tp;
	char mode[32];
	int baud;
	int clen;
	char parity;
	int stop;
	char c;

	sc = device_get_softc(dev);
	mtx_init(&sc->sc_mtx, "sabtty", NULL, MTX_SPIN);
	sc->sc_dev = dev;
	sc->sc_parent = device_get_softc(device_get_parent(dev));
	sc->sc_bt = sc->sc_parent->sc_bt;
	sc->sc_channel = device_get_unit(dev) & 1;
	sc->sc_iput = sc->sc_iget = sc->sc_ibuf;
	sc->sc_oget = sc->sc_obuf;

	switch (sc->sc_channel) {
	case 0:	/* port A */
		sc->sc_pvr_dtr = SAB_PVR_DTR_A;
		sc->sc_pvr_dsr = SAB_PVR_DSR_A;
		bus_space_subregion(sc->sc_bt, sc->sc_parent->sc_bh,
		    SAB_CHAN_A, SAB_CHANLEN, &sc->sc_bh);
		break;
	case 1:	/* port B */
		sc->sc_pvr_dtr = SAB_PVR_DTR_B;
		sc->sc_pvr_dsr = SAB_PVR_DSR_B;
		bus_space_subregion(sc->sc_bt, sc->sc_parent->sc_bh,
		    SAB_CHAN_B, SAB_CHANLEN, &sc->sc_bh);
		break;
	default:
		return (ENXIO);
	}

	tp = ttymalloc(NULL);
	sc->sc_si = make_dev(&sabtty_cdevsw, device_get_unit(dev),
	    UID_ROOT, GID_WHEEL, 0600, "%s", device_get_desc(dev));
	sc->sc_si->si_drv1 = sc;
	sc->sc_si->si_tty = tp;
	tp->t_dev = sc->sc_si;
	sc->sc_tty = tp;

	tp->t_oproc = sabttystart;
	tp->t_param = sabttyparam;
	tp->t_stop = sabttystop;
	tp->t_iflag = TTYDEF_IFLAG;
	tp->t_oflag = TTYDEF_OFLAG;
	tp->t_lflag = TTYDEF_LFLAG;
	tp->t_cflag = CREAD | CLOCAL | CS8;
	tp->t_ospeed = TTYDEF_SPEED;
	tp->t_ispeed = TTYDEF_SPEED;

	if (sabtty_console(dev, mode, sizeof(mode))) {
		ttychars(tp);
		if (sscanf(mode, "%d,%d,%c,%d,%c", &baud, &clen, &parity,
		    &stop, &c) == 5) {
			tp->t_ospeed = baud;
			tp->t_ispeed = baud;
			tp->t_cflag = CREAD | CLOCAL;

			switch (clen) {
			case 5:
				tp->t_cflag |= CS5;
				break;
			case 6:
				tp->t_cflag |= CS6;
				break;
			case 7:
				tp->t_cflag |= CS7;
				break;
			case 8:
			default:
				tp->t_cflag |= CS8;
				break;
			}

			if (parity == 'e')
				tp->t_cflag |= PARENB;
			else if (parity == 'o')
				tp->t_cflag |= PARENB | PARODD;

			if (stop == 2)
				tp->t_cflag |= CSTOPB;
		}
		device_printf(dev, "console %s\n", mode);
		sc->sc_console = 1;
		sabtty_cons = sc;
	}

	return (0);
}

static int
sabtty_detach(device_t dev)
{

	return (bus_generic_detach(dev));
}

static int
sabtty_intr(struct sabtty_softc *sc)
{
	int clearfifo;
	int needsoft;
	uint8_t isr0;
	uint8_t isr1;
	uint8_t c;
	int brk;
	int len;
	int i;

	brk = 0;
	len = 0;
	clearfifo = 0;
	needsoft = 0;

	SABTTY_LOCK(sc);

	isr0 = SAB_READ(sc, SAB_ISR0);
	isr1 = SAB_READ(sc, SAB_ISR1);

	if (isr0 & SAB_ISR0_RPF) {
		len = 32;
		clearfifo = 1;
	}
	if (isr0 & SAB_ISR0_TCD) {
		len = (32 - 1) & SAB_READ(sc, SAB_RBCL);
		clearfifo = 1;
	}
	if (isr0 & SAB_ISR0_TIME) {
		sabtty_cec_wait(sc);
		SAB_WRITE(sc, SAB_CMDR, SAB_CMDR_RFRD);
	}
	if (isr0 & SAB_ISR0_RFO)
		clearfifo = 1;
	if (len != 0) {
		for (i = 0; i < len; i++) {
			c = SAB_READ(sc, SAB_RFIFO);
#if defined(DDB) && defined(ALT_BREAK_TO_DEBUGGER)
			if (sc->sc_console != 0 && (i & 1) == 0)
				brk = db_alt_break(c, &sc->sc_alt_break_state);
#endif
			*sc->sc_iput++ = c;
			if (sc->sc_iput == sc->sc_ibuf + sizeof(sc->sc_ibuf))
				sc->sc_iput = sc->sc_ibuf;
		}
		needsoft = 1;
	}

	if (clearfifo) {
		sabtty_cec_wait(sc);
		SAB_WRITE(sc, SAB_CMDR, SAB_CMDR_RMC);
	}

	if (isr1 & SAB_ISR1_ALLS) {
		sc->sc_tx_done = 1;
		sc->sc_imr1 |= SAB_IMR1_ALLS;
		SAB_WRITE(sc, SAB_IMR1, sc->sc_imr1);
		needsoft = 1;
	}

	if (isr1 & SAB_ISR1_XPR) {
		if (sc->sc_ocnt > 0) {
			len = min(sc->sc_ocnt, 32);
			for (i = 0; i < len; i++)
				SAB_WRITE(sc, SAB_XFIFO + i, *sc->sc_oget++);
			sc->sc_ocnt -= len;
			sabtty_cec_wait(sc);
			SAB_WRITE(sc, SAB_CMDR, SAB_CMDR_XF);
		}
		if (sc->sc_ocnt == 0) {
			sc->sc_imr1 |= SAB_IMR1_XPR;
			sc->sc_imr1 &= ~SAB_IMR1_ALLS;
			SAB_WRITE(sc, SAB_IMR1, sc->sc_imr1);
		}
	}

	SABTTY_UNLOCK(sc);

	if (brk != 0)
		breakpoint();
	
	return (needsoft);
}

static void
sabtty_softintr(struct sabtty_softc *sc)
{
	struct tty *tp = sc->sc_tty;
	int data;
	int stat;

	if ((tp->t_state & TS_ISOPEN) == 0)
		return;

	while (sc->sc_iget != sc->sc_iput) {
		data = *sc->sc_iget++;
		stat = *sc->sc_iget++;
		if (stat & SAB_RSTAT_PE)
			data |= TTY_PE;
		if (stat & SAB_RSTAT_FE)
			data |= TTY_FE;
		if (sc->sc_iget == sc->sc_ibuf + sizeof(sc->sc_ibuf))
			sc->sc_iget = sc->sc_ibuf;

		(*linesw[tp->t_line].l_rint)(data, tp);
	}

	if (sc->sc_tx_done != 0) {
		sc->sc_tx_done = 0;
		tp->t_state &= ~TS_BUSY;
		(*linesw[tp->t_line].l_start)(tp);
	}
}

static int
sabttyopen(dev_t dev, int flags, int mode, struct thread *td)
{
	struct sabtty_softc *sc;
	struct tty *tp;
	int error;

	sc = dev->si_drv1;
	tp = dev->si_tty;

	if ((tp->t_state & TS_ISOPEN) != 0 &&
	    (tp->t_state & TS_XCLUDE) != 0 &&
	    !suser(td))
		return (EBUSY);

	if ((tp->t_state & TS_ISOPEN) == 0) {
		struct termios t;

		sc->sc_iput = sc->sc_iget = sc->sc_ibuf;

		/*
		 * Initialize the termios status to the defaults.  Add in the
		 * sticky bits from TIOCSFLAGS.
		 */
		t.c_ispeed = 0;
		t.c_ospeed = TTYDEF_SPEED;
		t.c_cflag = TTYDEF_CFLAG;
		/* Make sure zstty_param() will do something. */
		tp->t_ospeed = 0;
		(void)sabtty_param(sc, tp, &t);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		ttychars(tp);
		ttsetwater(tp);

		SABTTY_LOCK(sc);

		sabtty_reset(sc);
		sc->sc_parent->sc_ipc = SAB_IPC_ICPL;
		SAB_WRITE(sc->sc_parent, SAB_IPC, sc->sc_parent->sc_ipc);
		sc->sc_imr0 = SAB_IMR0_PERR | SAB_IMR0_FERR | SAB_IMR0_PLLA;
		SAB_WRITE(sc, SAB_IMR0, sc->sc_imr0);
		sc->sc_imr1 = SAB_IMR1_BRK | SAB_IMR1_ALLS | SAB_IMR1_XDU |
		    SAB_IMR1_TIN | SAB_IMR1_CSC | SAB_IMR1_XMR | SAB_IMR1_XPR;
		SAB_WRITE(sc, SAB_IMR1, sc->sc_imr1);
		SAB_WRITE(sc, SAB_CCR0, SAB_READ(sc, SAB_CCR0) | SAB_CCR0_PU);
		sabtty_cec_wait(sc);
		SAB_WRITE(sc, SAB_CMDR, SAB_CMDR_XRES);
		sabtty_cec_wait(sc);
		SAB_WRITE(sc, SAB_CMDR, SAB_CMDR_RRES);
		sabtty_cec_wait(sc);

		sabtty_flush(sc);

		SABTTY_UNLOCK(sc);

		/* XXX turn on DTR */

		/* XXX handle initial DCD */
	}

	error = ttyopen(dev, tp);
	if (error != 0)
		return (error);

	error = (*linesw[tp->t_line].l_open)(dev, tp);
	if (error != 0)
		return (error);

	return (0);
}

static int
sabttyclose(dev_t dev, int flags, int mode, struct thread *td)
{
	struct tty *tp;

	tp = dev->si_tty;

	if ((tp->t_state & TS_ISOPEN) == 0)
		return (0);

	(*linesw[tp->t_line].l_close)(tp, flags);
	ttyclose(tp);

	return (0);
}

static int
sabttyioctl(dev_t dev, u_long cmd, caddr_t data, int flags, struct thread *td)
{
	struct sabtty_softc *sc;
	struct tty *tp;
	int error;

	sc = dev->si_drv1;
	tp = dev->si_tty;

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flags, td);
	if (error != ENOIOCTL)
		return (error);

	error = ttioctl(tp, cmd, data, flags);
	if (error != ENOIOCTL)
		return (error);

	error = 0;
	switch (cmd) {
	case TIOCSBRK:
		SAB_WRITE(sc, SAB_DAFO,
		    SAB_READ(sc, SAB_DAFO) | SAB_DAFO_XBRK);
		break;
	case TIOCCBRK:
		SAB_WRITE(sc, SAB_DAFO,
		    SAB_READ(sc, SAB_DAFO) & ~SAB_DAFO_XBRK);
		break;
	case TIOCSDTR:
		sabtty_mdmctrl(sc, TIOCM_DTR, DMBIS);
		break;
	case TIOCCDTR:
		sabtty_mdmctrl(sc, TIOCM_DTR, DMBIC);
		break;
	case TIOCMBIS:
		sabtty_mdmctrl(sc, *((int *)data), DMBIS);
		break;
	case TIOCMBIC:
		sabtty_mdmctrl(sc, *((int *)data), DMBIC);
		break;
	case TIOCMGET:
		*((int *)data) = sabtty_mdmctrl(sc, 0, DMGET);
		break;
	case TIOCMSET:
		sabtty_mdmctrl(sc, *((int *)data), DMSET);
		break;
	default:
		error = ENOTTY;
		break;
	}

	return (error);
}

static void
sabttystart(struct tty *tp)
{
	struct sabtty_softc *sc;

	sc = tp->t_dev->si_drv1;

	if ((tp->t_state & TS_TBLOCK) != 0)
		/* XXX clear RTS */;
	else
		/* XXX set RTS */;

	if ((tp->t_state & (TS_BUSY | TS_TIMEOUT | TS_TTSTOP)) != 0) {
		ttwwakeup(tp);
		return;
	}

	if (tp->t_outq.c_cc <= tp->t_olowat) {
		if ((tp->t_state & TS_SO_OLOWAT) != 0) {
			tp->t_state &= ~TS_SO_OLOWAT;
			wakeup(TSA_OLOWAT(tp));
		}
		selwakeup(&tp->t_wsel);
		if (tp->t_outq.c_cc == 0) {
			if ((tp->t_state & (TS_BUSY | TS_SO_OCOMPLETE)) ==
			    TS_SO_OCOMPLETE && tp->t_outq.c_cc == 0) {
				tp->t_state &= ~TS_SO_OCOMPLETE;
				wakeup(TSA_OCOMPLETE(tp));
			}
			return;
		}
	}

	sc->sc_ocnt = q_to_b(&tp->t_outq, sc->sc_obuf, sizeof(sc->sc_obuf));
	if (sc->sc_ocnt == 0)
		return;
	sc->sc_oget = sc->sc_obuf;

	tp->t_state |= TS_BUSY;

	sc->sc_imr1 &= ~SAB_IMR1_XPR;
	SAB_WRITE(sc, SAB_IMR1, sc->sc_imr1);

	ttwwakeup(tp);
}

static void
sabttystop(struct tty *tp, int flag)
{
	struct sabtty_softc *sc;

	sc = tp->t_dev->si_drv1;

	if ((flag & FREAD) != 0) {
		/* XXX stop reading, anything to do? */;
	}

	if ((flag & FWRITE) != 0) {
		if ((tp->t_state & TS_BUSY) != 0) {
			/* XXX do what? */
			if ((tp->t_state & TS_TTSTOP) == 0)
				tp->t_state |= TS_FLUSH;
		}
	}
}

static int
sabttyparam(struct tty *tp, struct termios *t)
{
	struct sabtty_softc *sc;

	sc = tp->t_dev->si_drv1;
	return (sabtty_param(sc, tp, t));
}

int
sabtty_mdmctrl(struct sabtty_softc *sc, int bits, int how)
{
	u_int8_t r;

	switch (how) {
	case DMGET:
		bits = 0;
		if (SAB_READ(sc, SAB_STAR) & SAB_STAR_CTS)
			bits |= TIOCM_CTS;
		if ((SAB_READ(sc, SAB_VSTR) & SAB_VSTR_CD) == 0)
			bits |= TIOCM_CD;

		r = SAB_READ(sc, SAB_PVR);
		if ((r & sc->sc_pvr_dtr) == 0)
			bits |= TIOCM_DTR;
		if ((r & sc->sc_pvr_dsr) == 0)
			bits |= TIOCM_DSR;

		r = SAB_READ(sc, SAB_MODE);
		if ((r & (SAB_MODE_RTS|SAB_MODE_FRTS)) == SAB_MODE_RTS)
			bits |= TIOCM_RTS;
		break;
	case DMSET:
		r = SAB_READ(sc, SAB_MODE);
		if (bits & TIOCM_RTS) {
			r &= ~SAB_MODE_FRTS;
			r |= SAB_MODE_RTS;
		} else
			r |= SAB_MODE_FRTS | SAB_MODE_RTS;
		SAB_WRITE(sc, SAB_MODE, r);

		r = SAB_READ(sc, SAB_PVR);
		if (bits & TIOCM_DTR)
			r &= ~sc->sc_pvr_dtr;
		else
			r |= sc->sc_pvr_dtr;
		SAB_WRITE(sc, SAB_PVR, r);
		break;
	case DMBIS:
		if (bits & TIOCM_RTS) {
			r = SAB_READ(sc, SAB_MODE);
			r &= ~SAB_MODE_FRTS;
			r |= SAB_MODE_RTS;
			SAB_WRITE(sc, SAB_MODE, r);
		}
		if (bits & TIOCM_DTR) {
			r = SAB_READ(sc, SAB_PVR);
			r &= ~sc->sc_pvr_dtr;
			SAB_WRITE(sc, SAB_PVR, r);
		}
		break;
	case DMBIC:
		if (bits & TIOCM_RTS) {
			r = SAB_READ(sc, SAB_MODE);
			r |= SAB_MODE_FRTS | SAB_MODE_RTS;
			SAB_WRITE(sc, SAB_MODE, r);
		}
		if (bits & TIOCM_DTR) {
			r = SAB_READ(sc, SAB_PVR);
			r |= sc->sc_pvr_dtr;
			SAB_WRITE(sc, SAB_PVR, r);
		}
		break;
	}
	return (bits);
}

int
sabtty_param(struct sabtty_softc *sc, struct tty *tp, struct termios *t)
{
	int ospeed;
	tcflag_t cflag;
	u_int8_t dafo, r;

	ospeed = sabtty_speed(t->c_ospeed);
	if (ospeed < 0 || (t->c_ispeed && t->c_ispeed != t->c_ospeed))
		return (EINVAL);

	/*
	 * If there were no changes, don't do anything.  This avoids dropping
	 * input and improves performance when all we did was frob things like
	 * VMIN and VTIME.
	 */
	if (tp->t_ospeed == t->c_ospeed &&
	    tp->t_cflag == t->c_cflag)
		return (0);

	/* hang up line if ospeed is zero, otherwise raise dtr */
	sabtty_mdmctrl(sc, TIOCM_DTR,
	    (t->c_ospeed == 0) ? DMBIC : DMBIS);

	dafo = SAB_READ(sc, SAB_DAFO);

	cflag = t->c_cflag;

	if (sc->sc_console != 0) {
		cflag |= CLOCAL;
		cflag &= ~HUPCL;
	}

	if (cflag & CSTOPB)
		dafo |= SAB_DAFO_STOP;
	else
		dafo &= ~SAB_DAFO_STOP;

	dafo &= ~SAB_DAFO_CHL_CSIZE;
	switch (cflag & CSIZE) {
	case CS5:
		dafo |= SAB_DAFO_CHL_CS5;
		break;
	case CS6:
		dafo |= SAB_DAFO_CHL_CS6;
		break;
	case CS7:
		dafo |= SAB_DAFO_CHL_CS7;
		break;
	default:
		dafo |= SAB_DAFO_CHL_CS8;
		break;
	}

	dafo &= ~SAB_DAFO_PARMASK;
	if (cflag & PARENB) {
		if (cflag & PARODD)
			dafo |= SAB_DAFO_PAR_ODD;
		else
			dafo |= SAB_DAFO_PAR_EVEN;
	} else
		dafo |= SAB_DAFO_PAR_NONE;

	tp->t_ispeed = 0;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = cflag;

	ttsetwater(tp);

	SABTTY_LOCK(sc);

	if (ospeed != 0) {
		SAB_WRITE(sc, SAB_BGR, ospeed & 0xff);
		r = SAB_READ(sc, SAB_CCR2);
		r &= ~(SAB_CCR2_BR9 | SAB_CCR2_BR8);
		r |= (ospeed >> 2) & (SAB_CCR2_BR9 | SAB_CCR2_BR8);
		SAB_WRITE(sc, SAB_CCR2, r);
	}

	r = SAB_READ(sc, SAB_MODE);
	r |= SAB_MODE_RAC;
	if (cflag & CRTSCTS) {
		r &= ~(SAB_MODE_RTS | SAB_MODE_FCTS);
		r |= SAB_MODE_FRTS;
		sc->sc_imr1 &= ~SAB_IMR1_CSC;
	} else {
		r |= SAB_MODE_RTS | SAB_MODE_FCTS;
		r &= ~SAB_MODE_FRTS;
		sc->sc_imr1 |= SAB_IMR1_CSC;
	}
	SAB_WRITE(sc, SAB_MODE, r);
	SAB_WRITE(sc, SAB_IMR1, sc->sc_imr1);

	SABTTY_UNLOCK(sc);

	return (0);
}

static void
sabtty_cec_wait(struct sabtty_softc *sc)
{
	int i = 50000;

	for (;;) {
		if ((SAB_READ(sc, SAB_STAR) & SAB_STAR_CEC) == 0)
			return;
		if (--i == 0)
			break;
		DELAY(1);
	}
}

static void
sabtty_tec_wait(struct sabtty_softc *sc)
{
	int i = 200000;

	for (;;) {
		if ((SAB_READ(sc, SAB_STAR) & SAB_STAR_TEC) == 0)
			return;
		if (--i == 0)
			break;
		DELAY(1);
	}
}

static void
sabtty_reset(struct sabtty_softc *sc)
{
	/* power down */
	SAB_WRITE(sc, SAB_CCR0, 0);

	/* set basic configuration */
	SAB_WRITE(sc, SAB_CCR0,
	    SAB_CCR0_MCE | SAB_CCR0_SC_NRZ | SAB_CCR0_SM_ASYNC);
	SAB_WRITE(sc, SAB_CCR1, SAB_CCR1_ODS | SAB_CCR1_BCR | SAB_CCR1_CM_7);
	SAB_WRITE(sc, SAB_CCR2, SAB_CCR2_BDF | SAB_CCR2_SSEL | SAB_CCR2_TOE);
	SAB_WRITE(sc, SAB_CCR3, 0);
	SAB_WRITE(sc, SAB_CCR4, SAB_CCR4_MCK4 | SAB_CCR4_EBRG);
	SAB_WRITE(sc, SAB_MODE, SAB_MODE_RTS | SAB_MODE_FCTS | SAB_MODE_RAC);
	SAB_WRITE(sc, SAB_RFC,
	    SAB_RFC_DPS | SAB_RFC_RFDF | SAB_RFC_RFTH_32CHAR);

	/* clear interrupts */
	sc->sc_imr0 = sc->sc_imr1 = 0xff;
	SAB_WRITE(sc, SAB_IMR0, sc->sc_imr0);
	SAB_WRITE(sc, SAB_IMR1, sc->sc_imr1);
	SAB_READ(sc, SAB_ISR0);
	SAB_READ(sc, SAB_ISR1);
}

static void
sabtty_flush(struct sabtty_softc *sc)
{

	/* clear rx fifo */
	sabtty_cec_wait(sc);
	SAB_WRITE(sc, SAB_CMDR, SAB_CMDR_RRES);

	/* clear tx fifo */
	sabtty_cec_wait(sc);
	SAB_WRITE(sc, SAB_CMDR, SAB_CMDR_XRES);
}

static int
sabtty_speed(int rate)
{
	int i, len, r;

	if (rate == 0)
		return (0);
	len = sizeof(sabtty_baudtable)/sizeof(sabtty_baudtable[0]);
	for (i = 0; i < len; i++) {
		if (rate == sabtty_baudtable[i].baud) {
			r = sabtty_baudtable[i].n |
			    (sabtty_baudtable[i].m << 6);
			return (r);
		}
	}
	return (-1);
}

static void
sab_cnprobe(struct consdev *cn)
{
	struct sabtty_softc *sc = sabtty_cons;

	if (sc == NULL)
		cn->cn_pri = CN_DEAD;
	else {
		cn->cn_pri = CN_REMOTE;
		cn->cn_dev = sc->sc_si;
		cn->cn_tp = sc->sc_tty;
	}
}

static void
sab_cninit(struct consdev *cn)
{
}

static void
sab_cnterm(struct consdev *cn)
{
}

static int
sab_cngetc(dev_t dev)
{
	struct sabtty_softc *sc = sabtty_cons;

	if (sc == NULL)
		return (-1);
	return (sabtty_cngetc(sc));
}

static int
sab_cncheckc(dev_t dev)
{
	struct sabtty_softc *sc = sabtty_cons;

	if (sc == NULL)
		return (-1);
	return (sabtty_cncheckc(sc));
}

static void
sab_cnputc(dev_t dev, int c)
{
	struct sabtty_softc *sc = sabtty_cons;

	if (sc == NULL)
		return;
	sabtty_cnputc(sc, c);
}

static void
sab_cndbctl(dev_t dev, int c)
{
}

static void
sabtty_cnopen(struct sabtty_softc *sc)
{

	SAB_WRITE(sc, SAB_IMR0, 0xff);
	SAB_WRITE(sc, SAB_IMR1, 0xff);
	SAB_WRITE(sc->sc_parent, SAB_IPC, sc->sc_parent->sc_ipc | SAB_IPC_VIS);
}

static void
sabtty_cnclose(struct sabtty_softc *sc)
{

	SAB_WRITE(sc, SAB_IMR0, sc->sc_imr0);
	SAB_WRITE(sc, SAB_IMR1, sc->sc_imr1);
	SAB_WRITE(sc->sc_parent, SAB_IPC, sc->sc_parent->sc_ipc);
}

static int
sabtty_cngetc(struct sabtty_softc *sc)
{
	uint8_t len;
	uint8_t r;

	sabtty_cnopen(sc);
again:
	do {
		r = SAB_READ(sc, SAB_STAR);
	} while ((r & SAB_STAR_RFNE) == 0);

	/*
	 * Ok, at least one byte in RFIFO, ask for permission to access RFIFO
	 * (I hate this chip... hate hate hate).
	 */
	sabtty_cec_wait(sc);
	SAB_WRITE(sc, SAB_CMDR, SAB_CMDR_RFRD);

	/* Wait for RFIFO to come ready */
	do {
		r = SAB_READ(sc, SAB_ISR0);
	} while ((r & SAB_ISR0_TCD) == 0);

	len = SAB_READ(sc, SAB_RBCL) & (32 - 1);
	if (len == 0)
		goto again;	/* Shouldn't happen... */

	r = SAB_READ(sc, SAB_RFIFO);

	/*
	 * Blow away everything left in the FIFO...
	 */
	sabtty_cec_wait(sc);
	SAB_WRITE(sc, SAB_CMDR, SAB_CMDR_RMC);
	sabtty_cnclose(sc);
	return (r);
}

static int
sabtty_cncheckc(struct sabtty_softc *sc)
{
	int8_t r;

	r = -1;
	sabtty_cnopen(sc);
	if ((SAB_READ(sc, SAB_STAR) & SAB_STAR_RFNE) != 0)
		r = sabtty_cngetc(sc);
	sabtty_cnclose(sc);
	return (r);
}

static void
sabtty_cnputc(struct sabtty_softc *sc, int c)
{

	sabtty_cnopen(sc);
	sabtty_tec_wait(sc);
	SAB_WRITE(sc, SAB_TIC, c);
	sabtty_tec_wait(sc);
	sabtty_cnclose(sc);
}

static int
sabtty_console(device_t dev, char *mode, int len)
{
	device_t parent;
	phandle_t chosen;
	phandle_t options;
	ihandle_t stdin;
	ihandle_t stdout;
	char output[32];
	char input[32];
	char name[32];

	parent = device_get_parent(dev);
	chosen = OF_finddevice("/chosen");
	options = OF_finddevice("/options");
	if (OF_getprop(chosen, "stdin", &stdin, sizeof(stdin)) == -1 ||
	    OF_getprop(chosen, "stdout", &stdout, sizeof(stdout)) == -1 ||
	    OF_getprop(options, "input-device", input, sizeof(input)) == -1 ||
	    OF_getprop(options, "output-device", output, sizeof(output)) == -1)
		return (0);
	if (ebus_get_node(parent) != OF_instance_to_package(stdin) ||
	    ebus_get_node(parent) != OF_instance_to_package(stdout))
		return (0);
	if ((strcmp(input, device_get_desc(dev)) == 0 &&
	     strcmp(output, device_get_desc(dev)) == 0) ||
	    (strcmp(input, "keyboard") == 0 && strcmp(output, "screen") == 0 &&
	     (device_get_unit(dev) & 1) == 0)) {
		if (mode != NULL) {
			sprintf(name, "%s-mode", device_get_desc(dev));
			return (OF_getprop(options, name, mode, len) != -1);
		} else
			return (1);
	}
	return (0);
}
