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
#include <sys/proc.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>
#include <sys/syslog.h>
#include <sys/tty.h>

#include <ofw/openfirm.h>
#include <sparc64/ebus/ebusvar.h>

#include <dev/sab/sab82532reg.h>

#define	KTR_SAB		KTR_CT3

#define	CDEV_MAJOR		168

#define	SABTTY_RBUF_SIZE	1024

#define	SAB_READ(sc, r) \
	bus_space_read_1((sc)->sc_bt, (sc)->sc_bh, (r))
#define	SAB_WRITE(sc, r, v) \
	bus_space_write_1((sc)->sc_bt, (sc)->sc_bh, (r), (v))

struct sabtty_softc {
	device_t		sc_dev;
	bus_space_tag_t		sc_bt;
	bus_space_handle_t	sc_bh;
	dev_t			sc_si;
	struct tty		*sc_tty;
	uint8_t			*sc_rend;
	uint8_t			*sc_rput;
	uint8_t			*sc_rget;
	uint8_t			*sc_txp;
	int			sc_txc;
	int			sc_openflags;
	int			sc_flags;
#define	SABTTYF_STOP		0x01
#define	SABTTYF_DONE		0x02
#define	SABTTYF_RINGOVERFLOW	0x04
#define	SABTTYF_CDCHG		0x08
#define	SABTTYF_CONS		0x10
#define	SABTTYF_TXDRAIN		0x20
	uint8_t			sc_pvr_dtr;
	uint8_t			sc_pvr_dsr;
	uint8_t			sc_imr0;
	uint8_t			sc_imr1;
	uint8_t			sc_rbuf[SABTTY_RBUF_SIZE];
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
};

static int sab_probe(device_t dev);
static int sab_attach(device_t dev);
static int sab_detach(device_t dev);

static void sab_intr(void *vsc);
static void sab_softintr(void *vsc);

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
static void sabtty_console_flags(struct sabtty_softc *sc);

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
	for (i = 0; i < SAB_NCHAN; i++)
		child[i] = device_add_child(dev, "sabtty", i);
	bus_generic_attach(dev);
	for (i = 0; i < SAB_NCHAN; i++)
		sc->sc_child[i] = device_get_softc(child[i]);

	/* Set all pins, except DTR pins to be inputs */
	SAB_WRITE(sc, SAB_PCR, ~(SAB_PVR_DTR_A | SAB_PVR_DTR_B));
	/* Disable port interrupts */
	SAB_WRITE(sc, SAB_PIM, 0xff);
	SAB_WRITE(sc, SAB_PVR, SAB_PVR_DTR_A | SAB_PVR_DTR_B | SAB_PVR_MAGIC);
	SAB_WRITE(sc, SAB_IPC, SAB_IPC_ICPL);

	swi_add(&tty_ithd, "tty:sab", sab_softintr, sc, SWI_TTY,
	    INTR_TYPE_TTY, &sc->sc_softih);

	if (sabtty_cons != NULL)
		cninit();

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
sab_intr(void *vsc)
{
	struct sab_softc *sc = vsc;
	int needsoft = 0;
	uint8_t gis;

	gis = SAB_READ(sc, SAB_GIS);

	/* channel A */
	if ((gis & (SAB_GIS_ISA1 | SAB_GIS_ISA0)) != 0 &&
	    sc->sc_child[0] != NULL)
		needsoft |= sabtty_intr(sc->sc_child[0]);

	/* channel B */
	if ((gis & (SAB_GIS_ISB1 | SAB_GIS_ISB0)) != 0 &&
	    sc->sc_child[1] != NULL)
		needsoft |= sabtty_intr(sc->sc_child[1]);

	if (needsoft && sc->sc_softih != NULL)
		swi_sched(sc->sc_softih, 0);
}

static void
sab_softintr(void *vsc)
{
	struct sab_softc *sc = vsc;

	if (sc->sc_child[0] != NULL)
		sabtty_softintr(sc->sc_child[0]);
	if (sc->sc_child[1] != NULL)
		sabtty_softintr(sc->sc_child[1]);
}

static int
sabtty_probe(device_t dev)
{

	switch (device_get_unit(dev)) {
	case 0:
		device_set_desc(dev, "ttya");
		break;
	case 1:
		device_set_desc(dev, "ttyb");
		break;
	default:
		return (ENXIO);
	}
	return (0);
}

static int
sabtty_attach(device_t dev)
{
	struct sab_softc *parent;
	struct sabtty_softc *sc;
	struct termios t;

	sc = device_get_softc(dev);
	sc->sc_tty = ttymalloc(NULL);
	sc->sc_si = make_dev(&sabtty_cdevsw, device_get_unit(dev),
	    UID_ROOT, GID_WHEEL, 0600, device_get_desc(dev));
	sc->sc_tty->t_oproc = sabttystart;
	sc->sc_tty->t_param = sabttyparam;
	sc->sc_tty->t_stop = sabttystop;
	sc->sc_tty->t_dev = sc->sc_si;
	sc->sc_si->si_tty = sc->sc_tty;
	sc->sc_si->si_drv1 = sc;

	sc->sc_dev = dev;
	parent = device_get_softc(device_get_parent(dev));
	sc->sc_bt = parent->sc_bt;
	sc->sc_rend = sc->sc_rbuf + SABTTY_RBUF_SIZE;

	switch (device_get_unit(dev)) {
	case 0:	/* port A */
		sc->sc_pvr_dtr = SAB_PVR_DTR_A;
		sc->sc_pvr_dsr = SAB_PVR_DSR_A;
		sc->sc_bh = parent->sc_bh + SAB_CHAN_A;
		break;
	case 1:	/* port B */
		sc->sc_pvr_dtr = SAB_PVR_DTR_B;
		sc->sc_pvr_dsr = SAB_PVR_DSR_B;
		sc->sc_bh = parent->sc_bh + SAB_CHAN_B;
		break;
	default:
		return (ENXIO);
	}

	sabtty_console_flags(sc);

	if ((sc->sc_flags & SABTTYF_CONS) != 0) {
		/* Let current output drain */
		DELAY(100000);

		t.c_ispeed = 0;
		t.c_ospeed = 9600;
		t.c_cflag = CREAD | CS8 | HUPCL;
		sc->sc_tty->t_ospeed = 0;
		sabtty_param(sc, sc->sc_tty, &t);

		sabtty_cons = sc;
	} else {
		sabtty_reset(sc);
	}

	return (0);
}

static int
sabtty_detach(device_t dev)
{

	return (0);
}

static int
sabtty_intr(struct sabtty_softc *sc)
{
	uint8_t isr0, isr1;
	int i, len = 0, needsoft = 0, clearfifo = 0;

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
	if (isr0 & SAB_ISR0_RFO) {
		sc->sc_flags |= SABTTYF_RINGOVERFLOW;
		clearfifo = 1;
	}
	if (len != 0) {
		u_int8_t *ptr;

		ptr = sc->sc_rput;
		for (i = 0; i < len; i++) {
			*ptr++ = SAB_READ(sc, SAB_RFIFO);
			if (ptr == sc->sc_rend)
				ptr = sc->sc_rbuf;
			if (ptr == sc->sc_rget) {
				if (ptr == sc->sc_rbuf)
					ptr = sc->sc_rend;
				ptr--;
				sc->sc_flags |= SABTTYF_RINGOVERFLOW;
			}
		}
		sc->sc_rput = ptr;
		needsoft = 1;
	}

	if (clearfifo) {
		sabtty_cec_wait(sc);
		SAB_WRITE(sc, SAB_CMDR, SAB_CMDR_RMC);
	}

	if (isr0 & SAB_ISR0_CDSC) {
		sc->sc_flags |= SABTTYF_CDCHG;
		needsoft = 1;
	}

#if defined(DDB) && defined(BREAK_TO_DEBUGGER)
	if ((isr1 & SAB_ISR1_BRKT) && (sc->sc_flags & SABTTYF_CONS))
		Debugger("break");
#endif

	if (isr1 & SAB_ISR1_ALLS) {
		if (sc->sc_flags & SABTTYF_TXDRAIN)
			wakeup(sc);
		sc->sc_flags &= ~SABTTYF_STOP;
		sc->sc_flags |= SABTTYF_DONE;
		sc->sc_imr1 |= SAB_IMR1_ALLS;
		SAB_WRITE(sc, SAB_IMR1, sc->sc_imr1);
		needsoft = 1;
	}

	if (isr1 & SAB_ISR1_XPR) {
		if ((sc->sc_flags & SABTTYF_STOP) == 0) {
			if (sc->sc_txc < 32)
				len = sc->sc_txc;
			else
				len = 32;
			for (i = 0; i < len; i++) {
				SAB_WRITE(sc, SAB_XFIFO + i, *sc->sc_txp);
				sc->sc_txp++;
				sc->sc_txc--;
			}
			if (i != 0) {
				sabtty_cec_wait(sc);
				SAB_WRITE(sc, SAB_CMDR, SAB_CMDR_XF);
			}
		}

		if ((sc->sc_txc == 0) || (sc->sc_flags & SABTTYF_STOP)) {
			sc->sc_imr1 |= SAB_IMR1_XPR;
			sc->sc_imr1 &= ~SAB_IMR1_ALLS;
			SAB_WRITE(sc, SAB_IMR1, sc->sc_imr1);
		}
	}

	return (needsoft);
}

static void
sabtty_softintr(struct sabtty_softc *sc)
{
	struct tty *tp = sc->sc_tty;
	int s, flags;
	uint8_t r;

	if ((tp->t_state & TS_ISOPEN) == 0)
		return;

	while (sc->sc_rget != sc->sc_rput) {
		int data;
		u_int8_t stat;

		data = sc->sc_rget[0];
		stat = sc->sc_rget[1];
		sc->sc_rget += 2;
		if (stat & SAB_RSTAT_PE)
			data |= TTY_PE;
		if (stat & SAB_RSTAT_FE)
			data |= TTY_FE;
		if (sc->sc_rget == sc->sc_rend)
			sc->sc_rget = sc->sc_rbuf;

		(*linesw[tp->t_line].l_rint)(data, tp);
	}

	s = splhigh();
	flags = sc->sc_flags;
	sc->sc_flags &= ~(SABTTYF_DONE|SABTTYF_CDCHG|SABTTYF_RINGOVERFLOW);
	splx(s);

	if (flags & SABTTYF_CDCHG) {
		s = spltty();
		r = SAB_READ(sc, SAB_VSTR) & SAB_VSTR_CD;
		splx(s);

		(*linesw[tp->t_line].l_modem)(tp, r);
	}

	if (flags & SABTTYF_RINGOVERFLOW)
		log(LOG_WARNING, "%s: ring overflow\n",
		    device_get_name(sc->sc_dev));

	if (flags & SABTTYF_DONE) {
		ndflush(&tp->t_outq, (char *)sc->sc_txp - tp->t_outq.c_cf);
		tp->t_state &= ~TS_BUSY;
		(*linesw[tp->t_line].l_start)(tp);
	}
}

static int
sabttyopen(dev_t dev, int flags, int mode, struct thread *td)
{
	struct sabtty_softc *sc;
	struct tty *tp;
	int s, s1;

	sc = dev->si_drv1;
	tp = dev->si_tty;

	if ((tp->t_state & TS_ISOPEN) == 0) {
#if 0
		tp->t_state |= TS_WOPEN;
#endif

		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG;
#if 0
		if (sc->sc_openflags & TIOCFLAG_CLOCAL)
			tp->t_cflag |= CLOCAL;
		if (sc->sc_openflags & TIOCFLAG_CRTSCTS)
			tp->t_cflag |= CRTSCTS;
		if (sc->sc_openflags & TIOCFLAG_MDMBUF)
			tp->t_cflag |= MDMBUF;
#endif
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;

		sc->sc_rput = sc->sc_rget = sc->sc_rbuf;

		s = spltty();

		ttsetwater(tp);

		s1 = splhigh();
		sabtty_reset(sc);
		sabtty_param(sc, tp, &tp->t_termios);
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
		splx(s1);

		sabtty_flush(sc);

		if (SAB_READ(sc, SAB_VSTR) & SAB_VSTR_CD)
			tp->t_state |= TS_CARR_ON;
		else
			tp->t_state &= ~TS_CARR_ON;
	} else if ((tp->t_state & TS_XCLUDE) && (!suser(td))) {
		return (EBUSY);
	} else {
		s = spltty();
	}

	if ((flags & O_NONBLOCK) == 0) {
		while ((tp->t_cflag & CLOCAL) == 0 &&
		    (tp->t_state & TS_CARR_ON) == 0) {
			int error;

#if 0
			tp->t_state |= TS_WOPEN;
#endif
			error = ttysleep(tp, &tp->t_rawq, TTIPRI | PCATCH,
			    "sabttycd", 0);
			if (error != 0) {
				splx(s);
#if 0
				tp->t_state &= ~TS_WOPEN;
#endif
				return (error);
			}
		}
	}

	splx(s);

	s = (*linesw[tp->t_line].l_open)(dev, tp);
	if (s != 0) {
		if (tp->t_state & TS_ISOPEN)
			return (s);

		if (tp->t_cflag & HUPCL) {
			sabtty_mdmctrl(sc, 0, DMSET);
			(void)tsleep(sc, TTIPRI, "ttclos", hz);
		}

		if ((sc->sc_flags & SABTTYF_CONS) == 0) {
			/* Flush and power down if we're not the console */
			sabtty_flush(sc);
			sabtty_reset(sc);
		}
	}
	return (s);
}

static int
sabttyclose(dev_t dev, int flags, int mode, struct thread *td)
{
	struct sabtty_softc *sc;
	struct tty *tp;
	int s;

	sc = dev->si_drv1;
	tp = dev->si_tty;

	(*linesw[tp->t_line].l_close)(tp, flags);

	s = spltty();

	if ((tp->t_state & TS_ISOPEN) == 0) {
		/* Wait for output drain */
		sc->sc_imr1 &= ~SAB_IMR1_ALLS;
		SAB_WRITE(sc, SAB_IMR1, sc->sc_imr1);
		sc->sc_flags |= SABTTYF_TXDRAIN;
		(void)tsleep(sc, TTIPRI, "ttclos", 5 * hz);
		sc->sc_imr1 |= SAB_IMR1_ALLS;
		SAB_WRITE(sc, SAB_IMR1, sc->sc_imr1);
		sc->sc_flags &= ~SABTTYF_TXDRAIN;

		if (tp->t_cflag & HUPCL) {
			sabtty_mdmctrl(sc, 0, DMSET);
			(void)tsleep(sc, TTIPRI, "ttclos", hz);
		}

		if ((sc->sc_flags & SABTTYF_CONS) == 0) {
			/* Flush and power down if we're not the console */
			sabtty_flush(sc);
			sabtty_reset(sc);
		}
	}

	ttyclose(tp);
	splx(s);

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
#if 0
	case TIOCGFLAGS:
		*((int *)data) = sc->sc_openflags;
		break;
	case TIOCSFLAGS:
		if (suser(td))
			error = EPERM;
		else
			sc->sc_openflags = *((int *)data) &
			    (TIOCFLAG_SOFTCAR | TIOCFLAG_CLOCAL |
			     TIOCFLAG_CRTSCTS | TIOCFLAG_MDMBUF);
		break;
#endif
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
	int s;

	sc = tp->t_dev->si_drv1;

	s = spltty();
	if ((tp->t_state & (TS_TTSTOP | TS_TIMEOUT | TS_BUSY)) == 0) {
		if (tp->t_outq.c_cc) {
			sc->sc_txc = tp->t_outq.c_cc;
			sc->sc_txp = tp->t_outq.c_cf;
			tp->t_state |= TS_BUSY;
			sc->sc_imr1 &= ~SAB_IMR1_XPR;
			SAB_WRITE(sc, SAB_IMR1, sc->sc_imr1);
		}
	}
	ttwwakeup(tp);
	splx(s);
}

static void
sabttystop(struct tty *tp, int rw)
{
	struct sabtty_softc *sc;
	int s;

	sc = tp->t_dev->si_drv1;

	s = spltty();
	if (tp->t_state & TS_BUSY) {
		if ((tp->t_state & TS_TTSTOP) == 0)
			tp->t_state |= TS_FLUSH;
		sc->sc_flags |= SABTTYF_STOP;
		sc->sc_imr1 &= ~SAB_IMR1_ALLS;
		SAB_WRITE(sc, SAB_IMR1, sc->sc_imr1);
	}
	splx(s);
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
	int s;

	s = spltty();
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
	splx(s);
	return (bits);
}

int
sabtty_param(struct sabtty_softc *sc, struct tty *tp, struct termios *t)
{
	int s, ospeed;
	tcflag_t cflag;
	u_int8_t dafo, r;

	ospeed = sabtty_speed(t->c_ospeed);
	if (ospeed < 0 || (t->c_ispeed && t->c_ispeed != t->c_ospeed))
		return (EINVAL);

	s = spltty();

	/* hang up line if ospeed is zero, otherwise raise dtr */
	sabtty_mdmctrl(sc, TIOCM_DTR,
	    (t->c_ospeed == 0) ? DMBIC : DMBIS);

	dafo = SAB_READ(sc, SAB_DAFO);

	cflag = t->c_cflag;

	if ((sc->sc_flags & SABTTYF_CONS) != 0) {
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

	tp->t_cflag = cflag;

	splx(s);
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

void
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

void
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

static int
sabtty_cngetc(struct sabtty_softc *sc)
{
	uint8_t len;
	uint8_t r;

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
	return (r);
}

static int
sabtty_cncheckc(struct sabtty_softc *sc)
{

	if ((SAB_READ(sc, SAB_STAR) & SAB_STAR_RFNE) != 0)
		return (sabtty_cngetc(sc));
	return (-1);
}

static void
sabtty_cnputc(struct sabtty_softc *sc, int c)
{

	sabtty_tec_wait(sc);
	SAB_WRITE(sc, SAB_TIC, c);
	sabtty_tec_wait(sc);
}

static void
sabtty_console_flags(struct sabtty_softc *sc)
{
	phandle_t chosen;
	char output[255];
	char input[255];
	int channel;

	channel = -1;
	chosen = OF_finddevice("/options");
	if (OF_getprop(chosen, "input-device", input, sizeof(input)) != -1 &&
	    OF_getprop(chosen, "output-device", output, sizeof(output)) != -1) {
		/*
		 * Assume input and output on different ports is a useless
		 * pathalogical case.
		 */
		if (strcmp(input, "ttya") == 0 && strcmp(output, "ttya") == 0)
			channel = 0;
		if (strcmp(input, "ttyb") == 0 && strcmp(output, "ttyb") == 0)
			channel = 1;
		if (channel == device_get_unit(sc->sc_dev))
			sc->sc_flags |= SABTTYF_CONS;
	}
}
