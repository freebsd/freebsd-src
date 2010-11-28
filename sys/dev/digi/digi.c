/*-
 * Copyright (c) 2001 Brian Somers <brian@Awfulhak.org>
 *   based on work by Slawa Olhovchenkov
 *                    John Prince <johnp@knight-trosoft.com>
 *                    Eric Hernes
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
 *
 * $FreeBSD$
 */

/*-
 * TODO:
 *	Figure out what the con bios stuff is supposed to do
 *	Test with *LOTS* more cards - I only have a PCI8r and an ISA Xem.
 */

#include "opt_compat.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/linker.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/tty.h>
#include <sys/syslog.h>
#include <sys/fcntl.h>
#include <sys/serial.h>
#include <sys/bus.h>
#include <machine/resource.h>

#include <sys/digiio.h>
#include <dev/digi/digireg.h>
#include <dev/digi/digi.h>
#include <dev/digi/digi_mod.h>
#include <dev/digi/digi_pci.h>

static t_open_t		digiopen;
static d_open_t		digicopen;
static d_close_t	digicclose;
static t_ioctl_t	digiioctl;
static d_ioctl_t	digisioctl;
static d_ioctl_t	digicioctl;

static void	digistop(struct tty *tp, int rw);
static void	digibreak(struct tty *tp, int brk);
static int	digimodem(struct tty *tp, int sigon, int sigoff);
static void	digi_poll(void *ptr);
static void	digi_freemoduledata(struct digi_softc *);
static void	fepcmd(struct digi_p *port, int cmd, int op, int ncmds);
static void	digistart(struct tty *tp);
static int	digiparam(struct tty *tp, struct termios *t);
static void	digiclose(struct tty *tp);
static void	digi_intr(void *);
static int	digi_init(struct digi_softc *_sc);
static int	digi_loadmoduledata(struct digi_softc *);
static int	digi_inuse(struct digi_softc *);
static void	digi_free_state(struct digi_softc *);

#define	fepcmd_b(port, cmd, op1, op2, ncmds) \
	fepcmd(port, cmd, (op2 << 8) | op1, ncmds)
#define	fepcmd_w	fepcmd

struct con_bios {
	struct con_bios *next;
	u_char *bios;
	size_t size;
};

static struct con_bios *con_bios_list;
devclass_t	 digi_devclass;
static char	 driver_name[] = "digi";
unsigned 	 digi_debug = 0;

static struct speedtab digispeedtab[] = {
	{ 0,		0},			/* old (sysV-like) Bx codes */
	{ 50,		1},
	{ 75,		2},
	{ 110,		3},
	{ 134,		4},
	{ 150,		5},
	{ 200,		6},
	{ 300,		7},
	{ 600,		8},
	{ 1200,		9},
	{ 1800,		10},
	{ 2400,		11},
	{ 4800,		12},
	{ 9600,		13},
	{ 19200,	14},
	{ 38400,	15},
	{ 57600,	(02000 | 1)},
	{ 76800,	(02000 | 2)},
	{ 115200,	(02000 | 3)},
	{ 230400,	(02000 | 6)},
	{ -1,		-1}
};

const struct digi_control_signals digi_xixe_signals = {
	0x02, 0x08, 0x10, 0x20, 0x40, 0x80
};

const struct digi_control_signals digi_normal_signals = {
	0x02, 0x80, 0x20, 0x10, 0x40, 0x01
};

static struct cdevsw digi_csw = {
	.d_version =	D_VERSION,
	.d_open =	digicopen,
	.d_close =	digicclose,
	.d_ioctl =	digicioctl,
	.d_name =	driver_name,
	.d_flags =	D_TTY | D_NEEDGIANT,
};

static void
digi_poll(void *ptr)
{
	struct digi_softc *sc;

	sc = (struct digi_softc *)ptr;
	callout_handle_init(&sc->callout);
	digi_intr(sc);
	sc->callout = timeout(digi_poll, sc, (hz >= 200) ? hz / 100 : 1);
}

static void
digi_int_test(void *v)
{
	struct digi_softc *sc = v;

	callout_handle_init(&sc->inttest);
#ifdef DIGI_INTERRUPT
	if (sc->intr_timestamp.tv_sec || sc->intr_timestamp.tv_usec) {
		/* interrupt OK! */
		return;
	}
	log(LOG_ERR, "digi%d: Interrupt didn't work, use polled mode\n", unit);
#endif
	sc->callout = timeout(digi_poll, sc, (hz >= 200) ? hz / 100 : 1);
}

static void
digi_freemoduledata(struct digi_softc *sc)
{
	if (sc->fep.data != NULL) {
		free(sc->fep.data, M_TTYS);
		sc->fep.data = NULL;
	}
	if (sc->link.data != NULL) {
		free(sc->link.data, M_TTYS);
		sc->link.data = NULL;
	}
	if (sc->bios.data != NULL) {
		free(sc->bios.data, M_TTYS);
		sc->bios.data = NULL;
	}
}

static int
digi_bcopy(const void *vfrom, void *vto, size_t sz)
{
	volatile const char *from = (volatile const char *)vfrom;
	volatile char *to = (volatile char *)vto;
	size_t i;

	for (i = 0; i < sz; i++)
		*to++ = *from++;

	from = (const volatile char *)vfrom;
	to = (volatile char *)vto;
	for (i = 0; i < sz; i++)
		if (*to++ != *from++)
			return (0);
	return (1);
}

void
digi_delay(struct digi_softc *sc, const char *txt, u_long timo)
{
	if (cold)
		DELAY(timo * 1000000 / hz);
	else
		tsleep(sc, PUSER | PCATCH, txt, timo);
}

static int
digi_init(struct digi_softc *sc)
{
	int i, cnt, resp;
	u_char *ptr;
	int lowwater;
	struct digi_p *port;
	volatile struct board_chan *bc;
	struct tty *tp;

	ptr = NULL;

	if (sc->status == DIGI_STATUS_DISABLED) {
		log(LOG_ERR, "digi%d: Cannot init a disabled card\n",
		    sc->res.unit);
		return (EIO);
	}
	if (sc->bios.data == NULL) {
		log(LOG_ERR, "digi%d: Cannot init without BIOS\n",
		    sc->res.unit);
		return (EIO);
	}
#if 0
	if (sc->link.data == NULL && sc->model >= PCCX) {
		log(LOG_ERR, "digi%d: Cannot init without link info\n",
		    sc->res.unit);
		return (EIO);
	}
#endif
	if (sc->fep.data == NULL) {
		log(LOG_ERR, "digi%d: Cannot init without fep code\n",
		    sc->res.unit);
		return (EIO);
	}
	sc->status = DIGI_STATUS_NOTINIT;

	if (sc->numports) {
		/*
		 * We're re-initialising - maybe because someone's attached
		 * another port module.  For now, we just re-initialise
		 * everything.
		 */
		if (digi_inuse(sc))
			return (EBUSY);

		digi_free_state(sc);
	}

	ptr = sc->setwin(sc, MISCGLOBAL);
	for (i = 0; i < 16; i += 2)
		vW(ptr + i) = 0;

	switch (sc->model) {
	case PCXEVE:
		outb(sc->wport, 0xff);		/* window 7 */
		ptr = sc->vmem + (BIOSCODE & 0x1fff);

		if (!digi_bcopy(sc->bios.data, ptr, sc->bios.size)) {
			device_printf(sc->dev, "BIOS upload failed\n");
			return (EIO);
		}

		outb(sc->port, FEPCLR);
		break;

	case PCXE:
	case PCXI:
	case PCCX:
		ptr = sc->setwin(sc, BIOSCODE + ((0xf000 - sc->mem_seg) << 4));
		if (!digi_bcopy(sc->bios.data, ptr, sc->bios.size)) {
			device_printf(sc->dev, "BIOS upload failed\n");
			return (EIO);
		}
		break;

	case PCXEM:
	case PCIEPCX:
	case PCIXR:
		if (sc->pcibus)
			PCIPORT = FEPRST;
		else
			outb(sc->port, FEPRST | FEPMEM);

		for (i = 0; ((sc->pcibus ? PCIPORT : inb(sc->port)) &
		    FEPMASK) != FEPRST; i++) {
			if (i > hz) {
				log(LOG_ERR, "digi%d: %s init reset failed\n",
				    sc->res.unit, sc->name);
				return (EIO);
			}
			digi_delay(sc, "digiinit0", 5);
		}
		DLOG(DIGIDB_INIT, (sc->dev, "Got init reset after %d us\n", i));

		/* Now upload the BIOS */
		cnt = (sc->bios.size < sc->win_size - BIOSOFFSET) ?
		    sc->bios.size : sc->win_size - BIOSOFFSET;

		ptr = sc->setwin(sc, BIOSOFFSET);
		if (!digi_bcopy(sc->bios.data, ptr, cnt)) {
			device_printf(sc->dev, "BIOS upload (1) failed\n");
			return (EIO);
		}

		if (cnt != sc->bios.size) {
			/* and the second part */
			ptr = sc->setwin(sc, sc->win_size);
			if (!digi_bcopy(sc->bios.data + cnt, ptr,
			    sc->bios.size - cnt)) {
				device_printf(sc->dev, "BIOS upload failed\n");
				return (EIO);
			}
		}

		ptr = sc->setwin(sc, 0);
		vW(ptr + 0) = 0x0401;
		vW(ptr + 2) = 0x0bf0;
		vW(ptr + 4) = 0x0000;
		vW(ptr + 6) = 0x0000;

		break;
	}

	DLOG(DIGIDB_INIT, (sc->dev, "BIOS uploaded\n"));

	ptr = sc->setwin(sc, MISCGLOBAL);
	W(ptr) = 0;

	if (sc->pcibus) {
		PCIPORT = FEPCLR;
		resp = FEPRST;
	} else if (sc->model == PCXEVE) {
		outb(sc->port, FEPCLR);
		resp = FEPRST;
	} else {
		outb(sc->port, FEPCLR | FEPMEM);
		resp = FEPRST | FEPMEM;
	}

	for (i = 0; ((sc->pcibus ? PCIPORT : inb(sc->port)) & FEPMASK)
	    == resp; i++) {
		if (i > hz) {
			log(LOG_ERR, "digi%d: BIOS start failed\n",
			    sc->res.unit);
			return (EIO);
		}
		digi_delay(sc, "digibios0", 5);
	}

	DLOG(DIGIDB_INIT, (sc->dev, "BIOS started after %d us\n", i));

	for (i = 0; vW(ptr) != *(u_short *)"GD"; i++) {
		if (i > 5*hz) {
			log(LOG_ERR, "digi%d: BIOS boot failed "
			    "(0x%02x != 0x%02x)\n",
			    sc->res.unit, vW(ptr), *(u_short *)"GD");
			return (EIO);
		}
		digi_delay(sc, "digibios1", 5);
	}

	DLOG(DIGIDB_INIT, (sc->dev, "BIOS booted after %d iterations\n", i));

	if (sc->link.data != NULL) {
		DLOG(DIGIDB_INIT, (sc->dev, "Loading link data\n"));
		ptr = sc->setwin(sc, 0xcd0);
		digi_bcopy(sc->link.data, ptr, 21);	/* XXX 21 ? */
	}

	/* load FEP/OS */

	switch (sc->model) {
	case PCXE:
	case PCXEVE:
	case PCXI:
		ptr = sc->setwin(sc, sc->model == PCXI ? 0x2000 : 0x0);
		digi_bcopy(sc->fep.data, ptr, sc->fep.size);

		/* A BIOS request to move our data to 0x2000 */
		ptr = sc->setwin(sc, MBOX);
		vW(ptr + 0) = 2;
		vW(ptr + 2) = sc->mem_seg + FEPCODESEG;
		vW(ptr + 4) = 0;
		vW(ptr + 6) = FEPCODESEG;
		vW(ptr + 8) = 0;
		vW(ptr + 10) = sc->fep.size;

		/* Run the BIOS request */
		outb(sc->port, FEPREQ | FEPMEM);
		outb(sc->port, FEPCLR | FEPMEM);

		for (i = 0; W(ptr); i++) {
			if (i > hz) {
				log(LOG_ERR, "digi%d: FEP/OS move failed\n",
				    sc->res.unit);
				sc->hidewin(sc);
				return (EIO);
			}
			digi_delay(sc, "digifep0", 5);
		}
		DLOG(DIGIDB_INIT,
		    (sc->dev, "FEP/OS moved after %d iterations\n", i));

		/* Clear the confirm word */
		ptr = sc->setwin(sc, FEPSTAT);
		vW(ptr + 0) = 0;

		/* A BIOS request to execute the FEP/OS */
		ptr = sc->setwin(sc, MBOX);
		vW(ptr + 0) = 0x01;
		vW(ptr + 2) = FEPCODESEG;
		vW(ptr + 4) = 0x04;

		/* Run the BIOS request */
		outb(sc->port, FEPREQ);
		outb(sc->port, FEPCLR);

		ptr = sc->setwin(sc, FEPSTAT);

		break;

	case PCXEM:
	case PCIEPCX:
	case PCIXR:
		DLOG(DIGIDB_INIT, (sc->dev, "Loading FEP/OS\n"));

		cnt = (sc->fep.size < sc->win_size - BIOSOFFSET) ?
		    sc->fep.size : sc->win_size - BIOSOFFSET;

		ptr = sc->setwin(sc, BIOSOFFSET);
		digi_bcopy(sc->fep.data, ptr, cnt);

		if (cnt != sc->fep.size) {
			ptr = sc->setwin(sc, BIOSOFFSET + cnt);
			digi_bcopy(sc->fep.data + cnt, ptr,
			    sc->fep.size - cnt);
		}

		DLOG(DIGIDB_INIT, (sc->dev, "FEP/OS loaded\n"));

		ptr = sc->setwin(sc, 0xc30);
		W(ptr + 4) = 0x1004;
		W(ptr + 6) = 0xbfc0;
		W(ptr + 0) = 0x03;
		W(ptr + 2) = 0x00;

		/* Clear the confirm word */
		ptr = sc->setwin(sc, FEPSTAT);
		W(ptr + 0) = 0;

		if (sc->port)
			outb(sc->port, 0);		/* XXX necessary ? */

		break;

	case PCCX:
		ptr = sc->setwin(sc, 0xd000);
		digi_bcopy(sc->fep.data, ptr, sc->fep.size);

		/* A BIOS request to execute the FEP/OS */
		ptr = sc->setwin(sc, 0xc40);
		W(ptr + 0) = 1;
		W(ptr + 2) = FEPCODE >> 4;
		W(ptr + 4) = 4;

		/* Clear the confirm word */
		ptr = sc->setwin(sc, FEPSTAT);
		W(ptr + 0) = 0;

		/* Run the BIOS request */
		outb(sc->port, FEPREQ | FEPMEM); /* send interrupt to BIOS */
		outb(sc->port, FEPCLR | FEPMEM);
		break;
	}

	/* Now wait 'till the FEP/OS has booted */
	for (i = 0; vW(ptr) != *(u_short *)"OS"; i++) {
		if (i > 2*hz) {
			log(LOG_ERR, "digi%d: FEP/OS start failed "
			    "(0x%02x != 0x%02x)\n",
			    sc->res.unit, vW(ptr), *(u_short *)"OS");
			sc->hidewin(sc);
			return (EIO);
		}
		digi_delay(sc, "digifep1", 5);
	}

	DLOG(DIGIDB_INIT, (sc->dev, "FEP/OS started after %d iterations\n", i));

	if (sc->model >= PCXEM) {
		ptr = sc->setwin(sc, 0xe04);
		vW(ptr) = 2;
		ptr = sc->setwin(sc, 0xc02);
		sc->numports = vW(ptr);
	} else {
		ptr = sc->setwin(sc, 0xc22);
		sc->numports = vW(ptr);
	}

	if (sc->numports == 0) {
		device_printf(sc->dev, "%s, 0 ports found\n", sc->name);
		sc->hidewin(sc);
		return (0);
	}

	device_printf(sc->dev, "%s, %d ports found\n", sc->name, sc->numports);

	if (sc->ports)
		free(sc->ports, M_TTYS);
	sc->ports = malloc(sizeof(struct digi_p) * sc->numports,
	    M_TTYS, M_WAITOK | M_ZERO);

	/*
	 * XXX Should read port 0xc90 for an array of 2byte values, 1 per
	 * port.  If the value is 0, the port is broken....
	 */

	ptr = sc->setwin(sc, 0);

	/* We should now init per-port structures */
	bc = (volatile struct board_chan *)(ptr + CHANSTRUCT);
	sc->gdata = (volatile struct global_data *)(ptr + FEP_GLOBAL);

	sc->memcmd = ptr + sc->gdata->cstart;
	sc->memevent = ptr + sc->gdata->istart;

	for (i = 0; i < sc->numports; i++, bc++) {
		port = sc->ports + i;
		port->pnum = i;
		port->sc = sc;
		port->status = ENABLED;
		port->bc = bc;
		tp = port->tp = ttyalloc();
		tp->t_oproc = digistart;
		tp->t_param = digiparam;
		tp->t_modem = digimodem;
		tp->t_break = digibreak;
		tp->t_stop = digistop;
		tp->t_cioctl = digisioctl;
		tp->t_ioctl = digiioctl;
		tp->t_open = digiopen;
		tp->t_close = digiclose;
		tp->t_sc = port;

		if (sc->model == PCXEVE) {
			port->txbuf = ptr +
			    (((bc->tseg - sc->mem_seg) << 4) & 0x1fff);
			port->rxbuf = ptr +
			    (((bc->rseg - sc->mem_seg) << 4) & 0x1fff);
			port->txwin = FEPWIN | ((bc->tseg - sc->mem_seg) >> 9);
			port->rxwin = FEPWIN | ((bc->rseg - sc->mem_seg) >> 9);
		} else if (sc->model == PCXI || sc->model == PCXE) {
			port->txbuf = ptr + ((bc->tseg - sc->mem_seg) << 4);
			port->rxbuf = ptr + ((bc->rseg - sc->mem_seg) << 4);
			port->txwin = port->rxwin = 0;
		} else {
			port->txbuf = ptr +
			    (((bc->tseg - sc->mem_seg) << 4) % sc->win_size);
			port->rxbuf = ptr +
			    (((bc->rseg - sc->mem_seg) << 4) % sc->win_size);
			port->txwin = FEPWIN |
			    (((bc->tseg - sc->mem_seg) << 4) / sc->win_size);
			port->rxwin = FEPWIN |
			    (((bc->rseg - sc->mem_seg) << 4) / sc->win_size);
		}
		port->txbufsize = bc->tmax + 1;
		port->rxbufsize = bc->rmax + 1;

		lowwater = port->txbufsize >> 2;
		if (lowwater > 1024)
			lowwater = 1024;
		sc->setwin(sc, 0);
		fepcmd_w(port, STXLWATER, lowwater, 10);
		fepcmd_w(port, SRXLWATER, port->rxbufsize >> 2, 10);
		fepcmd_w(port, SRXHWATER, (3 * port->rxbufsize) >> 2, 10);

		bc->edelay = 100;

		ttyinitmode(tp, 0, 0);
		port->send_ring = 1;	/* Default action on signal RI */
		ttycreate(tp, NULL, 0, MINOR_CALLOUT, "D%r%r", sc->res.unit, i);
	}

	sc->hidewin(sc);
	sc->inttest = timeout(digi_int_test, sc, hz);
	/* fepcmd_w(&sc->ports[0], 0xff, 0, 0); */
	sc->status = DIGI_STATUS_ENABLED;

	return (0);
}

static int
digimodem(struct tty *tp, int sigon, int sigoff)
{
	struct digi_softc *sc;
	struct digi_p *port;
	int bitand, bitor, mstat;

	port = tp->t_sc;
	sc = port->sc;

	if (sigon == 0 && sigoff == 0) {
		port->sc->setwin(port->sc, 0);
		mstat = port->bc->mstat;
		port->sc->hidewin(port->sc);
		if (mstat & port->sc->csigs->rts)
			sigon |= SER_RTS;
		if (mstat & port->cd)
			sigon |= SER_DCD;
		if (mstat & port->dsr)
			sigon |= SER_DSR;
		if (mstat & port->sc->csigs->cts)
			sigon |= SER_CTS;
		if (mstat & port->sc->csigs->ri)
			sigon |= SER_RI;
		if (mstat & port->sc->csigs->dtr)
			sigon |= SER_DTR;
		return (sigon);
	}

	bitand = 0;
	bitor = 0;

	if (sigoff & SER_DTR)
		bitand |= port->sc->csigs->dtr;
	if (sigoff & SER_RTS)
		bitand |= port->sc->csigs->rts;
	if (sigon & SER_DTR)
		bitor |= port->sc->csigs->dtr;
	if (sigon & SER_RTS)
		bitor |= port->sc->csigs->rts;
	fepcmd_b(port, SETMODEM, bitor, ~bitand, 0);
	return (0);
}

static int
digicopen(struct cdev *dev, int flag, int mode, struct thread *td)
{
	struct digi_softc *sc;

	sc = dev->si_drv1;
	if (sc->status != DIGI_STATUS_ENABLED) {
		DLOG(DIGIDB_OPEN, (sc->dev, "Cannot open a disabled card\n"));
		return (ENXIO);
	}
	sc->opencnt++;
	return (0);
}

static int
digiopen(struct tty *tp, struct cdev *dev)
{
	int error;
	struct digi_softc *sc;
	struct digi_p *port;
	volatile struct board_chan *bc;

	port = tp->t_sc;
	sc = port->sc;

	if (sc->status != DIGI_STATUS_ENABLED) {
		DLOG(DIGIDB_OPEN, (sc->dev, "Cannot open a disabled card\n"));
		return (ENXIO);
	}
	bc = port->bc;

	/*
	 * The device isn't open, so there are no conflicts.
	 * Initialize it.  Initialization is done twice in many
	 * cases: to preempt sleeping callin opens if we are callout,
	 * and to complete a callin open after DCD rises.
	 */
	sc->setwin(sc, 0);

	bc->rout = bc->rin;	/* clear input queue */
	bc->idata = 1;
	bc->iempty = 1;
	bc->ilow = 1;
	bc->mint = port->cd | port->sc->csigs->ri;
	bc->tin = bc->tout;
	if (port->ialtpin) {
		port->cd = sc->csigs->dsr;
		port->dsr = sc->csigs->cd;
	} else {
		port->cd = sc->csigs->cd;
		port->dsr = sc->csigs->dsr;
	}
	tp->t_wopeners++;			/* XXX required ? */
	error = digiparam(tp, &tp->t_termios);
	tp->t_wopeners--;

	return (error);
}

static int
digicclose(struct cdev *dev, int flag, int mode, struct thread *td)
{
	struct digi_softc *sc;

	sc = dev->si_drv1;
	sc->opencnt--;
	return (0);
}

static void
digidtrwakeup(void *chan)
{
	struct digi_p *port = chan;

	port->status &= ~DIGI_DTR_OFF;
	wakeup(&port->tp->t_dtr_wait);
	port->tp->t_wopeners--;
}

static void
digiclose(struct tty *tp)
{
	volatile struct board_chan *bc;
	struct digi_p *port;
	int s;

	port = tp->t_sc;
	bc = port->bc;

	s = spltty();
	port->sc->setwin(port->sc, 0);
	bc->idata = 0;
	bc->iempty = 0;
	bc->ilow = 0;
	bc->mint = 0;
	if ((tp->t_cflag & HUPCL) ||
	    (!tp->t_actout && !(bc->mstat & port->cd) &&
	    !(tp->t_init_in.c_cflag & CLOCAL)) ||
	    !(tp->t_state & TS_ISOPEN)) {
		digimodem(tp, 0, SER_DTR | SER_RTS);
		if (tp->t_dtr_wait != 0) {
			/* Schedule a wakeup of any callin devices */
			tp->t_wopeners++;
			timeout(&digidtrwakeup, port, tp->t_dtr_wait);
			port->status |= DIGI_DTR_OFF;
		}
	}
	tp->t_actout = FALSE;
	wakeup(&tp->t_actout);
	wakeup(TSA_CARR_ON(tp));
	splx(s);
}

/*
 * Load module "digi_<mod>.ko" and look for a symbol called digi_mod_<mod>.
 *
 * Populate sc->bios, sc->fep, and sc->link from this data.
 *
 * sc->fep.data, sc->bios.data and sc->link.data are malloc()d according
 * to their respective sizes.
 *
 * The module is unloaded when we're done.
 */
static int
digi_loadmoduledata(struct digi_softc *sc)
{
	struct digi_mod *digi_mod;
	linker_file_t lf;
	char *modfile, *sym;
	caddr_t symptr;
	int modlen, res;

	KASSERT(sc->bios.data == NULL, ("Uninitialised BIOS variable"));
	KASSERT(sc->fep.data == NULL, ("Uninitialised FEP variable"));
	KASSERT(sc->link.data == NULL, ("Uninitialised LINK variable"));
	KASSERT(sc->module != NULL, ("Uninitialised module name"));

	modlen = strlen(sc->module);
	modfile = malloc(modlen + 6, M_TEMP, M_WAITOK);
	snprintf(modfile, modlen + 6, "digi_%s", sc->module);
	if ((res = linker_reference_module(modfile, NULL, &lf)) != 0)
		printf("%s: Failed %d to autoload module\n", modfile, res);
	free(modfile, M_TEMP);
	if (res != 0)
		return (res);

	sym = malloc(modlen + 10, M_TEMP, M_WAITOK);
	snprintf(sym, modlen + 10, "digi_mod_%s", sc->module);
	symptr = linker_file_lookup_symbol(lf, sym, 0);
	free(sym, M_TEMP);
	if (symptr == NULL) {
		printf("digi_%s.ko: Symbol `%s' not found\n", sc->module, sym);
		linker_release_module(NULL, NULL, lf);
		return (EINVAL);
	}

	digi_mod = (struct digi_mod *)symptr;
	if (digi_mod->dm_version != DIGI_MOD_VERSION) {
		printf("digi_%s.ko: Invalid version %d (need %d)\n",
		    sc->module, digi_mod->dm_version, DIGI_MOD_VERSION);
		linker_release_module(NULL, NULL, lf);
		return (EINVAL);
	}

	sc->bios.size = digi_mod->dm_bios.size;
	if (sc->bios.size != 0 && digi_mod->dm_bios.data != NULL) {
		sc->bios.data = malloc(sc->bios.size, M_TTYS, M_WAITOK);
		bcopy(digi_mod->dm_bios.data, sc->bios.data, sc->bios.size);
	}

	sc->fep.size = digi_mod->dm_fep.size;
	if (sc->fep.size != 0 && digi_mod->dm_fep.data != NULL) {
		sc->fep.data = malloc(sc->fep.size, M_TTYS, M_WAITOK);
		bcopy(digi_mod->dm_fep.data, sc->fep.data, sc->fep.size);
	}

	sc->link.size = digi_mod->dm_link.size;
	if (sc->link.size != 0 && digi_mod->dm_link.data != NULL) {
		sc->link.data = malloc(sc->link.size, M_TTYS, M_WAITOK);
		bcopy(digi_mod->dm_link.data, sc->link.data, sc->link.size);
	}

	linker_release_module(NULL, NULL, lf);

	return (0);
}

static int
digisioctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag, struct thread *td)
{
	struct digi_p *port;
	struct digi_softc *sc;

	port = dev->si_drv1;
	sc = port->sc;

	switch (cmd) {
	case DIGIIO_GETALTPIN:
		if (ISINIT(dev))
			*(int *)data = port->ialtpin;
		else if (ISLOCK(dev))
			*(int *)data = port->laltpin;
		else
			return (ENOTTY);
		break;
	case DIGIIO_SETALTPIN:
		if (ISINIT(dev)) {
			if (!port->laltpin) {
				port->ialtpin = !!*(int *)data;
				DLOG(DIGIDB_SET, (sc->dev,
				    "port%d: initial ALTPIN %s\n", port->pnum,
				    port->ialtpin ? "set" : "cleared"));
			}
		} else if (ISLOCK(dev)) {
			port->laltpin = !!*(int *)data;
			DLOG(DIGIDB_SET, (sc->dev,
			    "port%d: ALTPIN %slocked\n",
			    port->pnum, port->laltpin ? "" : "un"));
		} else
			return (ENOTTY);
		break;
	default:
		return (ENOTTY);
	}
	return (0);
}

static int
digicioctl(struct cdev *dev, u_long cmd, caddr_t data, int flag, struct thread *td)
{
	int error;
	struct digi_softc *sc;

	sc = dev->si_drv1;

	if (sc->status == DIGI_STATUS_DISABLED)
		return (ENXIO);

	switch (cmd) {
	case DIGIIO_DEBUG:
#ifdef DEBUG
		digi_debug = *(int *)data;
		return (0);
#else
		device_printf(sc->dev, "DEBUG not defined\n");
		return (ENXIO);
#endif
	case DIGIIO_REINIT:
		digi_loadmoduledata(sc);
		error = digi_init(sc);
		digi_freemoduledata(sc);
		return (error);

	case DIGIIO_MODEL:
		*(enum digi_model *)data = sc->model;
		return (0);

	case DIGIIO_IDENT:
		return (copyout(sc->name, *(char **)data,
		    strlen(sc->name) + 1));
	default:
		return (ENOIOCTL);
	}
}

static int
digiioctl(struct tty *tp, u_long cmd, void *data, int flag, struct thread *td)
{
	struct digi_softc *sc;
	struct digi_p *port;
	int ival;

	port = tp->t_sc;
	sc = port->sc;
	if (sc->status == DIGI_STATUS_DISABLED)
		return (ENXIO);

	if (!(port->status & ENABLED))
		return (ENXIO);

	switch (cmd) {
	case DIGIIO_GETALTPIN:
		*(int *)data = !!(port->dsr == sc->csigs->cd);
		return (0);

	case DIGIIO_SETALTPIN:
		if (!port->laltpin) {
			if (*(int *)data) {
				DLOG(DIGIDB_SET, (sc->dev,
				    "port%d: ALTPIN set\n", port->pnum));
				port->cd = sc->csigs->dsr;
				port->dsr = sc->csigs->cd;
			} else {
				DLOG(DIGIDB_SET, (sc->dev,
				    "port%d: ALTPIN cleared\n", port->pnum));
				port->cd = sc->csigs->cd;
				port->dsr = sc->csigs->dsr;
			}
		}
		return (0);
	case _IO('e', 'C'):
		ival = IOCPARM_IVAL(data);
		data = &ival;
		/* FALLTHROUGH */
	case DIGIIO_RING:
		port->send_ring = (u_char)*(int *)data;
		break;
	default:
		return (ENOTTY);
	}
	return (0);
}

static void
digibreak(struct tty *tp, int brk)
{
	struct digi_p *port;

	port = tp->t_sc;

	/*
	 * now it sends 400 millisecond break because I don't know
	 * how to send an infinite break
	 */
	if (brk)
		fepcmd_w(port, SENDBREAK, 400, 10);
}

static int
digiparam(struct tty *tp, struct termios *t)
{
	struct digi_softc *sc;
	struct digi_p *port;
	int cflag;
	int iflag;
	int hflow;
	int s;
	int window;

	port = tp->t_sc;
	sc = port->sc;
	DLOG(DIGIDB_SET, (sc->dev, "port%d: setting parameters\n", port->pnum));

	if (t->c_ispeed == 0)
		t->c_ispeed = t->c_ospeed;

	cflag = ttspeedtab(t->c_ospeed, digispeedtab);

	if (cflag < 0 || (cflag > 0 && t->c_ispeed != t->c_ospeed))
		return (EINVAL);

	s = splclock();

	window = sc->window;
	sc->setwin(sc, 0);

	if (cflag == 0) {				/* hangup */
		DLOG(DIGIDB_SET, (sc->dev, "port%d: hangup\n", port->pnum));
		digimodem(port->tp, 0, SER_DTR | SER_RTS);
	} else {
		digimodem(port->tp, SER_DTR | SER_RTS, 0);

		DLOG(DIGIDB_SET, (sc->dev, "port%d: CBAUD = %d\n", port->pnum,
		    cflag));

#if 0
		/* convert flags to sysV-style values */
		if (t->c_cflag & PARODD)
			cflag |= 0x0200;
		if (t->c_cflag & PARENB)
			cflag |= 0x0100;
		if (t->c_cflag & CSTOPB)
			cflag |= 0x0080;
#else
		/* convert flags to sysV-style values */
		if (t->c_cflag & PARODD)
			cflag |= FEP_PARODD;
		if (t->c_cflag & PARENB)
			cflag |= FEP_PARENB;
		if (t->c_cflag & CSTOPB)
			cflag |= FEP_CSTOPB;
		if (t->c_cflag & CLOCAL)
			cflag |= FEP_CLOCAL;
#endif

		cflag |= (t->c_cflag & CSIZE) >> 4;
		DLOG(DIGIDB_SET, (sc->dev, "port%d: CFLAG = 0x%x\n", port->pnum,
		    cflag));
		fepcmd_w(port, SETCFLAGS, (unsigned)cflag, 0);
	}

	iflag =
	    t->c_iflag & (IGNBRK | BRKINT | IGNPAR | PARMRK | INPCK | ISTRIP);
	if (port->c_iflag & IXON)
		iflag |= 0x400;
	if (port->c_iflag & IXANY)
		iflag |= 0x800;
	if (port->c_iflag & IXOFF)
		iflag |= 0x1000;

	DLOG(DIGIDB_SET, (sc->dev, "port%d: set iflag = 0x%x\n", port->pnum, iflag));
	fepcmd_w(port, SETIFLAGS, (unsigned)iflag, 0);

	hflow = 0;
	if (t->c_cflag & CDTR_IFLOW)
		hflow |= sc->csigs->dtr;
	if (t->c_cflag & CRTS_IFLOW)
		hflow |= sc->csigs->rts;
	if (t->c_cflag & CCTS_OFLOW)
		hflow |= sc->csigs->cts;
	if (t->c_cflag & CDSR_OFLOW)
		hflow |= port->dsr;
	if (t->c_cflag & CCAR_OFLOW)
		hflow |= port->cd;

	DLOG(DIGIDB_SET, (sc->dev, "port%d: set hflow = 0x%x\n", port->pnum, hflow));
	fepcmd_w(port, SETHFLOW, 0xff00 | (unsigned)hflow, 0);

	DLOG(DIGIDB_SET, (sc->dev, "port%d: set startc(0x%x), stopc(0x%x)\n",
	    port->pnum, t->c_cc[VSTART], t->c_cc[VSTOP]));
	fepcmd_b(port, SONOFFC, t->c_cc[VSTART], t->c_cc[VSTOP], 0);

	if (sc->window != 0)
		sc->towin(sc, 0);
	if (window != 0)
		sc->towin(sc, window);
	splx(s);

	return (0);
}

static void
digi_intr(void *vp)
{
	struct digi_p *port;
	char *cxcon;
	struct digi_softc *sc;
	int ehead, etail;
	volatile struct board_chan *bc;
	struct tty *tp;
	int head, tail;
	int wrapmask;
	int size, window;
	struct event {
		u_char pnum;
		u_char event;
		u_char mstat;
		u_char lstat;
	} event;

	sc = vp;

	if (sc->status != DIGI_STATUS_ENABLED) {
		DLOG(DIGIDB_IRQ, (sc->dev, "interrupt on disabled board !\n"));
		return;
	}

#ifdef DIGI_INTERRUPT
	microtime(&sc->intr_timestamp);
#endif

	window = sc->window;
	sc->setwin(sc, 0);

	if (sc->model >= PCXEM && W(sc->vmem + 0xd00)) {
		struct con_bios *con = con_bios_list;
		register u_char *ptr;

		ptr = sc->vmem + W(sc->vmem + 0xd00);
		while (con) {
			if (ptr[1] && W(ptr + 2) == W(con->bios + 2))
				/* Not first block -- exact match */
				break;

			if (W(ptr + 4) >= W(con->bios + 4) &&
			    W(ptr + 4) <= W(con->bios + 6))
				/* Initial search concetrator BIOS */
				break;
		}

		if (con == NULL) {
			log(LOG_ERR, "digi%d: wanted bios LREV = 0x%04x"
			    " not found!\n", sc->res.unit, W(ptr + 4));
			W(ptr + 10) = 0;
			W(sc->vmem + 0xd00) = 0;
			goto eoi;
		}
		cxcon = con->bios;
		W(ptr + 4) = W(cxcon + 4);
		W(ptr + 6) = W(cxcon + 6);
		if (ptr[1] == 0)
			W(ptr + 2) = W(cxcon + 2);
		W(ptr + 8) = (ptr[1] << 6) + W(cxcon + 8);
		size = W(cxcon + 10) - (ptr[1] << 10);
		if (size <= 0) {
			W(ptr + 8) = W(cxcon + 8);
			W(ptr + 10) = 0;
		} else {
			if (size > 1024)
				size = 1024;
			W(ptr + 10) = size;
			bcopy(cxcon + (ptr[1] << 10), ptr + 12, size);
		}
		W(sc->vmem + 0xd00) = 0;
		goto eoi;
	}

	ehead = sc->gdata->ein;
	etail = sc->gdata->eout;
	if (ehead == etail) {
#ifdef DEBUG
		sc->intr_count++;
		if (sc->intr_count % 6000 == 0) {
			DLOG(DIGIDB_IRQ, (sc->dev,
			    "6000 useless polls %x %x\n", ehead, etail));
			sc->intr_count = 0;
		}
#endif
		goto eoi;
	}
	while (ehead != etail) {
		event = *(volatile struct event *)(sc->memevent + etail);

		etail = (etail + 4) & sc->gdata->imax;

		if (event.pnum >= sc->numports) {
			log(LOG_ERR, "digi%d: port %d: got event"
			    " on nonexisting port\n", sc->res.unit,
			    event.pnum);
			continue;
		}
		port = &sc->ports[event.pnum];
		bc = port->bc;
		tp = port->tp;

		if (!(tp->t_state & TS_ISOPEN) && !tp->t_wopeners) {
			DLOG(DIGIDB_IRQ, (sc->dev,
			    "port %d: event 0x%x on closed port\n",
			    event.pnum, event.event));
			bc->rout = bc->rin;
			bc->idata = 0;
			bc->iempty = 0;
			bc->ilow = 0;
			bc->mint = 0;
			continue;
		}
		if (event.event & ~ALL_IND)
			log(LOG_ERR, "digi%d: port%d: ? event 0x%x mstat 0x%x"
			    " lstat 0x%x\n", sc->res.unit, event.pnum,
			    event.event, event.mstat, event.lstat);

		if (event.event & DATA_IND) {
			DLOG(DIGIDB_IRQ, (sc->dev, "port %d: DATA_IND\n",
			    event.pnum));
			wrapmask = port->rxbufsize - 1;
			head = bc->rin;
			tail = bc->rout;

			size = 0;
			if (!(tp->t_state & TS_ISOPEN)) {
				bc->rout = head;
				goto end_of_data;
			}
			while (head != tail) {
				int top;

				DLOG(DIGIDB_INT, (sc->dev,
				    "port %d: p rx head = %d tail = %d\n",
				    event.pnum, head, tail));
				top = (head > tail) ? head : wrapmask + 1;
				sc->towin(sc, port->rxwin);
				size = top - tail;
				if (tp->t_state & TS_CAN_BYPASS_L_RINT) {
					size = b_to_q((char *)port->rxbuf +
					    tail, size, &tp->t_rawq);
					tail = top - size;
					ttwakeup(tp);
				} else for (; tail < top;) {
					ttyld_rint(tp, port->rxbuf[tail]);
					sc->towin(sc, port->rxwin);
					size--;
					tail++;
					if (tp->t_state & TS_TBLOCK)
						break;
				}
				tail &= wrapmask;
				sc->setwin(sc, 0);
				bc->rout = tail;
				head = bc->rin;
				if (size)
					break;
			}

			if (bc->orun) {
				CE_RECORD(port, CE_OVERRUN);
				log(LOG_ERR, "digi%d: port%d: %s\n",
				    sc->res.unit, event.pnum,
				    digi_errortxt(CE_OVERRUN));
				bc->orun = 0;
			}
end_of_data:
			if (size) {
				tp->t_state |= TS_TBLOCK;
				port->status |= PAUSE_RX;
				DLOG(DIGIDB_RX, (sc->dev, "port %d: pause RX\n",
				    event.pnum));
			} else {
				bc->idata = 1;
			}
		}

		if (event.event & MODEMCHG_IND) {
			DLOG(DIGIDB_MODEM, (sc->dev, "port %d: MODEMCHG_IND\n",
			    event.pnum));

			if ((event.mstat ^ event.lstat) & port->cd) {
				sc->hidewin(sc);
				ttyld_modem(tp, event.mstat & port->cd);
				sc->setwin(sc, 0);
				wakeup(TSA_CARR_ON(tp));
			}

			if (event.mstat & sc->csigs->ri) {
				DLOG(DIGIDB_RI, (sc->dev, "port %d: RING\n",
				    event.pnum));
				if (port->send_ring) {
					ttyld_rint(tp, 'R');
					ttyld_rint(tp, 'I');
					ttyld_rint(tp, 'N');
					ttyld_rint(tp, 'G');
					ttyld_rint(tp, '\r');
					ttyld_rint(tp, '\n');
				}
			}
		}
		if (event.event & BREAK_IND) {
			DLOG(DIGIDB_MODEM, (sc->dev, "port %d: BREAK_IND\n",
			    event.pnum));
			ttyld_rint(tp, TTY_BI);
		}
		if (event.event & (LOWTX_IND | EMPTYTX_IND)) {
			DLOG(DIGIDB_IRQ, (sc->dev, "port %d:%s%s\n",
			    event.pnum,
			    event.event & LOWTX_IND ? " LOWTX" : "",
			    event.event & EMPTYTX_IND ?  " EMPTYTX" : ""));
			ttyld_start(tp);
		}
	}
	sc->gdata->eout = etail;
eoi:
	if (sc->window != 0)
		sc->towin(sc, 0);
	if (window != 0)
		sc->towin(sc, window);
}

static void
digistart(struct tty *tp)
{
	struct digi_p *port;
	struct digi_softc *sc;
	volatile struct board_chan *bc;
	int head, tail;
	int size, ocount, totcnt = 0;
	int s;
	int wmask;

	port = tp->t_sc;
	sc = port->sc;
	bc = port->bc;

	wmask = port->txbufsize - 1;

	s = spltty();
	port->lcc = tp->t_outq.c_cc;
	sc->setwin(sc, 0);
	if (!(tp->t_state & TS_TBLOCK)) {
		if (port->status & PAUSE_RX) {
			DLOG(DIGIDB_RX, (sc->dev, "port %d: resume RX\n",
			    port->pnum));
			/*
			 * CAREFUL - braces are needed here if the DLOG is
			 * optimised out!
			 */
		}
		port->status &= ~PAUSE_RX;
		bc->idata = 1;
	}
	if (!(tp->t_state & TS_TTSTOP) && port->status & PAUSE_TX) {
		DLOG(DIGIDB_TX, (sc->dev, "port %d: resume TX\n", port->pnum));
		port->status &= ~PAUSE_TX;
		fepcmd_w(port, RESUMETX, 0, 10);
	}
	if (tp->t_outq.c_cc == 0)
		tp->t_state &= ~TS_BUSY;
	else
		tp->t_state |= TS_BUSY;

	head = bc->tin;
	while (tp->t_outq.c_cc != 0) {
		tail = bc->tout;
		DLOG(DIGIDB_INT, (sc->dev, "port%d: s tx head = %d tail = %d\n",
		    port->pnum, head, tail));

		if (head < tail)
			size = tail - head - 1;
		else {
			size = port->txbufsize - head;
			if (tail == 0)
				size--;
		}

		if (size == 0)
			break;
		sc->towin(sc, port->txwin);
		ocount = q_to_b(&tp->t_outq, port->txbuf + head, size);
		totcnt += ocount;
		head += ocount;
		head &= wmask;
		sc->setwin(sc, 0);
		bc->tin = head;
		bc->iempty = 1;
		bc->ilow = 1;
	}
	port->lostcc = tp->t_outq.c_cc;
	tail = bc->tout;
	if (head < tail)
		size = port->txbufsize - tail + head;
	else
		size = head - tail;

	port->lbuf = size;
	DLOG(DIGIDB_INT, (sc->dev, "port%d: s total cnt = %d\n", port->pnum, totcnt));
	ttwwakeup(tp);
	splx(s);
}

static void
digistop(struct tty *tp, int rw)
{
	struct digi_softc *sc;
	struct digi_p *port;

	port = tp->t_sc;
	sc = port->sc;

	DLOG(DIGIDB_TX, (sc->dev, "port %d: pause TX\n", port->pnum));
	port->status |= PAUSE_TX;
	fepcmd_w(port, PAUSETX, 0, 10);
}

static void
fepcmd(struct digi_p *port, int cmd, int op1, int ncmds)
{
	u_char *mem;
	unsigned tail, head;
	int count, n;

	mem = port->sc->memcmd;

	port->sc->setwin(port->sc, 0);

	head = port->sc->gdata->cin;
	mem[head + 0] = cmd;
	mem[head + 1] = port->pnum;
	*(u_short *)(mem + head + 2) = op1;

	head = (head + 4) & port->sc->gdata->cmax;
	port->sc->gdata->cin = head;

	for (count = FEPTIMEOUT; count > 0; count--) {
		head = port->sc->gdata->cin;
		tail = port->sc->gdata->cout;
		n = (head - tail) & port->sc->gdata->cmax;

		if (n <= ncmds * sizeof(short) * 4)
			break;
	}
	if (count == 0)
		log(LOG_ERR, "digi%d: port%d: timeout on FEP command\n",
		    port->sc->res.unit, port->pnum);
}

const char *
digi_errortxt(int id)
{
	static const char *error_desc[] = {
		"silo overflow",
		"interrupt-level buffer overflow",
		"tty-level buffer overflow",
	};

	KASSERT(id >= 0 && id < sizeof(error_desc) / sizeof(error_desc[0]),
	    ("Unexpected digi error id %d\n", id));

	return (error_desc[id]);
}

int
digi_attach(struct digi_softc *sc)
{
	sc->res.ctldev = make_dev(&digi_csw,
	    sc->res.unit << 16, UID_ROOT, GID_WHEEL,
	    0600, "digi%r.ctl", sc->res.unit);
	sc->res.ctldev->si_drv1 = sc;

	digi_loadmoduledata(sc);
	digi_init(sc);
	digi_freemoduledata(sc);

	return (0);
}

static int
digi_inuse(struct digi_softc *sc)
{
	int i;
	struct digi_p *port;

	port = &sc->ports[0];
	for (i = 0; i < sc->numports; i++, port++)
		if (port->tp->t_state & TS_ISOPEN) {
			DLOG(DIGIDB_INIT, (sc->dev, "port%d: busy\n", i));
			return (1);
		} else if (port->tp->t_wopeners || port->opencnt) {
			DLOG(DIGIDB_INIT, (sc->dev, "port%d: blocked in open\n",
			    i));
			return (1);
		}
	return (0);
}

static void
digi_free_state(struct digi_softc *sc)
{
	int i;

	/* Blow it all away */

	for (i = 0; i < sc->numports; i++)
		ttygone(sc->ports[i].tp);

	/* XXX: this might be better done as a ttypurge method */
	untimeout(digi_poll, sc, sc->callout);
	callout_handle_init(&sc->callout);
	untimeout(digi_int_test, sc, sc->inttest);
	callout_handle_init(&sc->inttest);

	for (i = 0; i < sc->numports; i++)
		ttyfree(sc->ports[i].tp);

	bus_teardown_intr(sc->dev, sc->res.irq, sc->res.irqHandler);
#ifdef DIGI_INTERRUPT
	if (sc->res.irq != NULL) {
		bus_release_resource(dev, SYS_RES_IRQ, sc->res.irqrid,
		    sc->res.irq);
		sc->res.irq = NULL;
	}
#endif
	if (sc->numports) {
		KASSERT(sc->ports, ("digi%d: Lost my ports ?", sc->res.unit));
		free(sc->ports, M_TTYS);
		sc->ports = NULL;
		sc->numports = 0;
	}

	sc->status = DIGI_STATUS_NOTINIT;
}

int
digi_detach(device_t dev)
{
	struct digi_softc *sc = device_get_softc(dev);

	DLOG(DIGIDB_INIT, (sc->dev, "detaching\n"));

	/* If we're INIT'd, numports must be 0 */
	KASSERT(sc->numports == 0 || sc->status != DIGI_STATUS_NOTINIT,
	    ("digi%d: numports(%d) & status(%d) are out of sync",
	    sc->res.unit, sc->numports, (int)sc->status));

	if (digi_inuse(sc))
		return (EBUSY);

	digi_free_state(sc);

	destroy_dev(sc->res.ctldev);

	if (sc->res.mem != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, sc->res.mrid,
		    sc->res.mem);
		sc->res.mem = NULL;
	}
	if (sc->res.io != NULL) {
		bus_release_resource(dev, SYS_RES_IOPORT, sc->res.iorid,
		    sc->res.io);
		sc->res.io = NULL;
	}

	return (0);
}

int
digi_shutdown(device_t dev)
{
	return (0);
}

MODULE_VERSION(digi, 1);
