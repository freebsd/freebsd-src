/*-
 * Copyright (c) 2001 Brian Somers <brian@Awfulhak.org>
 *                    based on work by Slawa Olhovchenkov
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
 *	Allow altpin setups again
 *	Figure out what the con bios stuff is supposed to do
 *	Test with *LOTS* more cards - I only have a PCI8r and an ISA Xem.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/reboot.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/dkstat.h>
#include <sys/file.h>
#include <sys/linker.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/tty.h>
#include <sys/syslog.h>

#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/bus.h>

#include <machine/clock.h>

#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include <dev/digi/digireg.h>
#include <dev/digi/digiio.h>
#include <dev/digi/digi.h>
#include <dev/digi/digi_mod.h>
#include <dev/digi/digi_pci.h>

#include <machine/ipl.h>

#define	CDEV_MAJOR	162

#ifdef SMP
#include <sys/lock.h>
#include <machine/smptests.h>	/* USE_COMLOCK ? */
#ifdef USE_COMLOCK
extern struct mtx	com_mtx;
#define	COM_LOCK()      mtx_lock_spin(&com_mtx)
#define	COM_UNLOCK()    mtx_unlock_spin(&com_mtx)
#else
#define	COM_LOCK()
#define	COM_UNLOCK()
#endif /* USE_COMLOCK */
#else
#define	COM_LOCK()
#define	COM_UNLOCK()
#endif /* SMP */

/*#define DIGI_INTERRUPT*/

#define	CTRL_DEV		0x800000
#define	CALLOUT_MASK		0x400000
#define	CONTROL_INIT_STATE	0x100000
#define	CONTROL_LOCK_STATE	0x200000
#define	CONTROL_MASK		(CTRL_DEV|CONTROL_INIT_STATE|CONTROL_LOCK_STATE)
#define UNIT_MASK		0x030000
#define PORT_MASK		0x0000FF
#define	DEV_TO_UNIT(dev)	(MINOR_TO_UNIT(minor(dev)))
#define	MINOR_MAGIC_MASK	(CALLOUT_MASK | CONTROL_MASK)
#define	MINOR_TO_UNIT(mynor)	(((mynor) & UNIT_MASK)>>16)
#define MINOR_TO_PORT(mynor)	((mynor) & PORT_MASK)

#ifdef SMP
#define disable_intr()		COM_DISABLE_INTR()
#define enable_intr()		COM_ENABLE_INTR()
#endif				/* SMP */

static d_open_t		digiopen;
static d_close_t	digiclose;
static d_read_t		digiread;
static d_write_t	digiwrite;
static d_ioctl_t	digiioctl;

static void	digistop(struct tty *tp, int rw);
static int	digimctl(struct digi_p *port, int bits, int how);
static void	digi_poll(void *ptr);
static void	digi_freemoduledata(struct digi_softc *);
static void	fepcmd(struct digi_p *port, int cmd, int op, int ncmds);
static void	digistart(struct tty *tp);
static int	digiparam(struct tty *tp, struct termios *t);
static void	digihardclose(struct digi_p *port);
static void	digi_intr(void *);
static int	digi_init(struct digi_softc *_sc);
static int	digi_loadmoduledata(struct digi_softc *);
static int	digi_inuse(struct digi_softc *);
static void	digi_free_state(struct digi_softc *);

#define	fepcmd_b(port, cmd, op1, op2, ncmds) \
	fepcmd(port, cmd, (op2 << 8) | op1, ncmds)
#define	fepcmd_w	fepcmd


static speed_t digidefaultrate = TTYDEF_SPEED;

struct con_bios {
	struct con_bios *next;
	u_char *bios;
	size_t size;
};

struct con_bios *con_bios_list;
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

static struct cdevsw digi_sw = {
	digiopen,		/* open */
	digiclose,		/* close */
	digiread,		/* read */
	digiwrite,		/* write */
	digiioctl,		/* ioctl */
	ttypoll,		/* poll */
	nommap,			/* mmap */
	nostrategy,		/* strategy */
	driver_name,		/* name */
	CDEV_MAJOR,		/* maj */
	nodump,			/* dump */
	nopsize,		/* psize */
	D_TTY | D_KQFILTER,	/* flags */
	ttykqfilter		/* bmaj */
};

int
digi_modhandler(module_t mod, int event, void *arg)
{
	static int ref = 0;

	switch (event) {
	case MOD_LOAD:
		if (ref++ == 0)
			cdevsw_add(&digi_sw);
		break;

	case MOD_UNLOAD:
		if (--ref == 0)
			cdevsw_remove(&digi_sw);
		break;
	}

	return (0);
}

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

static int
digi_init(struct digi_softc *sc)
{
	int i, cnt, resp;
	u_char *ptr;
	int lowwater;
	struct digi_p *port;
	volatile struct board_chan *bc;

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
		if (sc->model == PCIXR)
			PCIPORT = FEPRST;
		else
			outb(sc->port, FEPRST | FEPMEM);

		for (i = 0; ((sc->model == PCIXR ? PCIPORT : inb(sc->port)) &
		    FEPMASK) != FEPRST; i++) {
			if (i > 1000) {
				log(LOG_ERR, "digi%d: init reset failed\n",
				    sc->res.unit);
				return (EIO);
			}
			tsleep(sc, PUSER | PCATCH, "digiinit0", 1);
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

	if (sc->model == PCIXR) {
		PCIPORT = FEPCLR;
		resp = FEPRST;
	} else if (sc->model == PCXEVE) {
		outb(sc->port, FEPCLR);
		resp = FEPRST;
	} else {
		outb(sc->port, FEPCLR | FEPMEM);
		resp = FEPRST | FEPMEM;
	}

	for (i = 0; ((sc->model == PCIXR ? PCIPORT : inb(sc->port)) & FEPMASK)
	    == resp; i++) {
		if (i > 1000) {
			log(LOG_ERR, "digi%d: BIOS start failed\n",
			    sc->res.unit);
			return (EIO);
		}
		tsleep(sc, PUSER | PCATCH, "digibios0", 1);
	}

	DLOG(DIGIDB_INIT, (sc->dev, "BIOS started after %d us\n", i));

	for (i = 0; vW(ptr) != *(u_short *)"GD"; i++) {
		if (i > 2000) {
			log(LOG_ERR, "digi%d: BIOS boot failed "
			    "(0x%02x != 0x%02x)\n",
			    sc->res.unit, vW(ptr), *(u_short *)"GD");
			return (EIO);
		}
		tsleep(sc, PUSER | PCATCH, "digibios1", 1);
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
			if (i > 10) {
				log(LOG_ERR, "digi%d: FEP/OS move failed\n",
				    sc->res.unit);
				sc->hidewin(sc);
				return (EIO);
			}
			tsleep(sc, PUSER | PCATCH, "digifep0", 1);
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
		if (i > 2000) {
			log(LOG_ERR, "digi%d: FEP/OS start failed "
			    "(0x%02x != 0x%02x)\n",
			    sc->res.unit, vW(ptr), *(u_short *)"OS");
			sc->hidewin(sc);
			return (EIO);
		}
		tsleep(sc, PUSER | PCATCH, "digifep1", 1);
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

	if (sc->numports > 256) {
		/* Our minor numbering scheme is broken for more than 256 */
		device_printf(sc->dev, "%s, 256 ports (%d ports found)\n",
		    sc->name, sc->numports);
		sc->numports = 256;
	} else
		device_printf(sc->dev, "%s, %d ports found\n", sc->name,
		    sc->numports);

	if (sc->ports)
		free(sc->ports, M_TTYS);
	MALLOC(sc->ports, struct digi_p *,
	    sizeof(struct digi_p) * sc->numports, M_TTYS, M_WAIT | M_ZERO);

	if (sc->ttys)
		free(sc->ttys, M_TTYS);
	MALLOC(sc->ttys, struct tty *, sizeof(struct tty) * sc->numports,
	    M_TTYS, M_WAIT | M_ZERO);

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
		port->tp = sc->ttys + i;
		port->bc = bc;

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
		port->dtr_wait = 3 * hz;

		/*
		 * We don't use all the flags from <sys/ttydefaults.h> since
		 * they are only relevant for logins.  It's important to have
		 * echo off initially so that the line doesn't start blathering
		 * before the echo flag can be turned off.
		 */
		port->it_in.c_iflag = 0;
		port->it_in.c_oflag = 0;
		port->it_in.c_cflag = TTYDEF_CFLAG;
		port->it_in.c_lflag = 0;
		termioschars(&port->it_in);
		port->it_in.c_ispeed = port->it_in.c_ospeed = digidefaultrate;
		port->it_out = port->it_in;
		port->send_ring = 1;	/* Default action on signal RI */

		port->dev[0] = make_dev(&digi_sw, (sc->res.unit << 16) + i,
		    UID_ROOT, GID_WHEEL, 0600, "ttyD%d.%d", sc->res.unit, i);
		port->dev[1] = make_dev(&digi_sw, ((sc->res.unit << 16) + i) |
		    CONTROL_INIT_STATE, UID_ROOT, GID_WHEEL,
		    0600, "ttyiD%d.%d", sc->res.unit, i);
		port->dev[2] = make_dev(&digi_sw, ((sc->res.unit << 16) + i) |
		    CONTROL_LOCK_STATE, UID_ROOT, GID_WHEEL,
		    0600, "ttylD%d.%d", sc->res.unit, i);
		port->dev[3] = make_dev(&digi_sw, ((sc->res.unit << 16) + i) |
		    CALLOUT_MASK, UID_UUCP, GID_DIALER,
		    0660, "cuaD%d.%d", sc->res.unit, i);
		port->dev[4] = make_dev(&digi_sw, ((sc->res.unit << 16) + i) |
		    CALLOUT_MASK | CONTROL_INIT_STATE, UID_UUCP, GID_DIALER,
		    0660, "cuaiD%d.%d", sc->res.unit, i);
		port->dev[5] = make_dev(&digi_sw, ((sc->res.unit << 16) + i) |
		    CALLOUT_MASK | CONTROL_LOCK_STATE, UID_UUCP, GID_DIALER,
		    0660, "cualD%d.%d", sc->res.unit, i);
	}

	sc->hidewin(sc);
	sc->inttest = timeout(digi_int_test, sc, hz);
	/* fepcmd_w(&sc->ports[0], 0xff, 0, 0); */
	sc->status = DIGI_STATUS_ENABLED;

	return (0);
}

static int
digimctl(struct digi_p *port, int bits, int how)
{
	int mstat;

	if (how == DMGET) {
		port->sc->setwin(port->sc, 0);
		mstat = port->bc->mstat;
		port->sc->hidewin(port->sc);
		bits = TIOCM_LE;
		if (mstat & port->sc->csigs->rts)
			bits |= TIOCM_RTS;
		if (mstat & port->sc->csigs->cd)
			bits |= TIOCM_CD;
		if (mstat & port->sc->csigs->dsr)
			bits |= TIOCM_DSR;
		if (mstat & port->sc->csigs->cts)
			bits |= TIOCM_CTS;
		if (mstat & port->sc->csigs->ri)
			bits |= TIOCM_RI;
		if (mstat & port->sc->csigs->dtr)
			bits |= TIOCM_DTR;
		return (bits);
	}

	/* Only DTR and RTS may be set */
	mstat = 0;
	if (bits & TIOCM_DTR)
		mstat |= port->sc->csigs->dtr;
	if (bits & TIOCM_RTS)
		mstat |= port->sc->csigs->rts;

	switch (how) {
	case DMSET:
		fepcmd_b(port, SETMODEM, mstat, ~mstat, 0);
		break;
	case DMBIS:
		fepcmd_b(port, SETMODEM, mstat, 0, 0);
		break;
	case DMBIC:
		fepcmd_b(port, SETMODEM, 0, mstat, 0);
		break;
	}

	return (0);
}

static void
digi_disc_optim(struct tty *tp, struct termios *t, struct digi_p *port)
{
	if (!(t->c_iflag & (ICRNL | IGNCR | IMAXBEL | INLCR | ISTRIP)) &&
	    (!(t->c_iflag & BRKINT) || (t->c_iflag & IGNBRK)) &&
	    (!(t->c_iflag & PARMRK) ||
	    (t->c_iflag & (IGNPAR | IGNBRK)) == (IGNPAR | IGNBRK)) &&
	    !(t->c_lflag & (ECHO | ICANON | IEXTEN | ISIG | PENDIN)) &&
	    linesw[tp->t_line].l_rint == ttyinput)
		tp->t_state |= TS_CAN_BYPASS_L_RINT;
	else
		tp->t_state &= ~TS_CAN_BYPASS_L_RINT;
}

int
digiopen(dev_t dev, int flag, int mode, struct proc *p)
{
	struct digi_softc *sc;
	struct tty *tp;
	int unit;
	int pnum;
	struct digi_p *port;
	int s;
	int error, mynor;
	volatile struct board_chan *bc;

	error = 0;
	mynor = minor(dev);
	unit = MINOR_TO_UNIT(minor(dev));
	pnum = MINOR_TO_PORT(minor(dev));

	sc = (struct digi_softc *)devclass_get_softc(digi_devclass, unit);
	if (!sc)
		return (ENXIO);

	if (sc->status != DIGI_STATUS_ENABLED) {
		DLOG(DIGIDB_OPEN, (sc->dev, "Cannot open a disabled card\n"));
		return (ENXIO);
	}
	if (pnum >= sc->numports) {
		DLOG(DIGIDB_OPEN, (sc->dev, "port%d: Doesn't exist\n", pnum));
		return (ENXIO);
	}
	if (mynor & (CTRL_DEV | CONTROL_MASK)) {
		sc->opencnt++;
		return (0);
	}
	port = &sc->ports[pnum];
	tp = dev->si_tty = port->tp;
	bc = port->bc;

	s = spltty();

open_top:
	while (port->status & DIGI_DTR_OFF) {
		port->wopeners++;
		error = tsleep(&port->dtr_wait, TTIPRI | PCATCH, "digidtr", 0);
		port->wopeners--;
		if (error)
			goto out;
	}

	if (tp->t_state & TS_ISOPEN) {
		/*
		 * The device is open, so everything has been initialized.
		 * Handle conflicts.
		 */
		if (mynor & CALLOUT_MASK) {
			if (!port->active_out) {
				error = EBUSY;
				DLOG(DIGIDB_OPEN, (sc->dev, "port %d:"
				    " BUSY error = %d\n", pnum, error));
				goto out;
			}
		} else if (port->active_out) {
			if (flag & O_NONBLOCK) {
				error = EBUSY;
				DLOG(DIGIDB_OPEN, (sc->dev,
				    "port %d: BUSY error = %d\n", pnum, error));
				goto out;
			}
			port->wopeners++;
			error = tsleep(&port->active_out, TTIPRI | PCATCH,
			    "digibi", 0);
			port->wopeners--;
			if (error != 0) {
				DLOG(DIGIDB_OPEN, (sc->dev,
				    "port %d: tsleep(digibi) error = %d\n",
				    pnum, error));
				goto out;
			}
			goto open_top;
		}
		if (tp->t_state & TS_XCLUDE && p->p_ucred->cr_uid != 0) {
			error = EBUSY;
			goto out;
		}
	} else {
		/*
		 * The device isn't open, so there are no conflicts.
		 * Initialize it.  Initialization is done twice in many
		 * cases: to preempt sleeping callin opens if we are callout,
		 * and to complete a callin open after DCD rises.
		 */
		tp->t_oproc = digistart;
		tp->t_param = digiparam;
		tp->t_stop = digistop;
		tp->t_dev = dev;
		tp->t_termios = (mynor & CALLOUT_MASK) ?
		    port->it_out : port->it_in;
		sc->setwin(sc, 0);

		bc->rout = bc->rin;	/* clear input queue */
		bc->idata = 1;
		bc->iempty = 1;
		bc->ilow = 1;
		bc->mint = port->sc->csigs->cd | port->sc->csigs->ri;
		bc->tin = bc->tout;
		port->wopeners++;			/* XXX required ? */
		error = digiparam(tp, &tp->t_termios);
		port->wopeners--;

		if (error != 0) {
			DLOG(DIGIDB_OPEN, (sc->dev,
			    "port %d: cxpparam error = %d\n", pnum, error));
			goto out;
		}
		ttsetwater(tp);

		/* handle fake and initial DCD for callout devices */

		if (bc->mstat & port->sc->csigs->cd || mynor & CALLOUT_MASK)
			linesw[tp->t_line].l_modem(tp, 1);
	}

	/* Wait for DCD if necessary */
	if (!(tp->t_state & TS_CARR_ON) && !(mynor & CALLOUT_MASK) &&
	    !(tp->t_cflag & CLOCAL) && !(flag & O_NONBLOCK)) {
		port->wopeners++;
		error = tsleep(TSA_CARR_ON(tp), TTIPRI | PCATCH, "digidcd", 0);
		port->wopeners--;
		if (error != 0) {
			DLOG(DIGIDB_OPEN, (sc->dev,
			    "port %d: tsleep(digidcd) error = %d\n",
			    pnum, error));
			goto out;
		}
		goto open_top;
	}
	error = linesw[tp->t_line].l_open(dev, tp);
	DLOG(DIGIDB_OPEN, (sc->dev, "port %d: l_open error = %d\n",
	    pnum, error));

	digi_disc_optim(tp, &tp->t_termios, port);

	if (tp->t_state & TS_ISOPEN && mynor & CALLOUT_MASK)
		port->active_out = TRUE;

	if (tp->t_state & TS_ISOPEN)
		sc->opencnt++;
out:
	splx(s);

	if (!(tp->t_state & TS_ISOPEN))
		digihardclose(port);

	DLOG(DIGIDB_OPEN, (sc->dev, "port %d: open() returns %d\n",
	    pnum, error));

	return (error);
}

int
digiclose(dev_t dev, int flag, int mode, struct proc *p)
{
	int mynor;
	struct tty *tp;
	int unit, pnum;
	struct digi_softc *sc;
	struct digi_p *port;
	int s;

	mynor = minor(dev);
	unit = MINOR_TO_UNIT(mynor);
	pnum = MINOR_TO_PORT(mynor);

	sc = (struct digi_softc *)devclass_get_softc(digi_devclass, unit);
	KASSERT(sc, ("digi%d: softc not allocated in digiclose\n", unit));

	if (mynor & (CTRL_DEV | CONTROL_MASK)) {
		sc->opencnt--;
		return (0);
	}

	port = sc->ports + pnum;
	tp = port->tp;

	DLOG(DIGIDB_CLOSE, (sc->dev, "port %d: closing\n", pnum));

	s = spltty();
	linesw[tp->t_line].l_close(tp, flag);
	digi_disc_optim(tp, &tp->t_termios, port);
	digistop(tp, FREAD | FWRITE);
	digihardclose(port);
	ttyclose(tp);
	if (--sc->opencnt == 0)
		splx(s);
	return (0);
}

static void
digidtrwakeup(void *chan)
{
	struct digi_p *port = chan;

	port->status &= ~DIGI_DTR_OFF;
	wakeup(&port->dtr_wait);
	port->wopeners--;
}

static void
digihardclose(struct digi_p *port)
{
	volatile struct board_chan *bc;
	int s;

	bc = port->bc;

	s = spltty();
	port->sc->setwin(port->sc, 0);
	bc->idata = 0;
	bc->iempty = 0;
	bc->ilow = 0;
	bc->mint = 0;
	if ((port->tp->t_cflag & HUPCL) ||
	    (!port->active_out && !(bc->mstat & port->sc->csigs->cd) &&
	    !(port->it_in.c_cflag & CLOCAL)) ||
	    !(port->tp->t_state & TS_ISOPEN)) {
		digimctl(port, TIOCM_DTR | TIOCM_RTS, DMBIC);
		if (port->dtr_wait != 0) {
			/* Schedule a wakeup of any callin devices */
			port->wopeners++;
			timeout(&digidtrwakeup, port, port->dtr_wait);
			port->status |= DIGI_DTR_OFF;
		}
	}
	port->active_out = FALSE;
	wakeup(&port->active_out);
	wakeup(TSA_CARR_ON(port->tp));
	splx(s);
}

int
digiread(dev_t dev, struct uio *uio, int flag)
{
	int mynor;
	struct tty *tp;
	int error, unit, pnum;
	struct digi_softc *sc;

	mynor = minor(dev);
	if (mynor & CONTROL_MASK)
		return (ENODEV);

	unit = MINOR_TO_UNIT(mynor);
	pnum = MINOR_TO_PORT(mynor);

	sc = (struct digi_softc *)devclass_get_softc(digi_devclass, unit);
	KASSERT(sc, ("digi%d: softc not allocated in digiclose\n", unit));
	tp = &sc->ttys[pnum];

	error = linesw[tp->t_line].l_read(tp, uio, flag);
	DLOG(DIGIDB_READ, (sc->dev, "port %d: read() returns %d\n",
	    pnum, error));

	return (error);
}

int
digiwrite(dev_t dev, struct uio *uio, int flag)
{
	int mynor;
	struct tty *tp;
	int error, unit, pnum;
	struct digi_softc *sc;

	mynor = minor(dev);
	if (mynor & CONTROL_MASK)
		return (ENODEV);

	unit = MINOR_TO_UNIT(mynor);
	pnum = MINOR_TO_PORT(mynor);

	sc = (struct digi_softc *)devclass_get_softc(digi_devclass, unit);
	KASSERT(sc, ("digi%d: softc not allocated in digiclose\n", unit));
	tp = &sc->ttys[pnum];

	error = linesw[tp->t_line].l_write(tp, uio, flag);
	DLOG(DIGIDB_WRITE, (sc->dev, "port %d: write() returns %d\n",
	    pnum, error));

	return (error);
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
	int res, symlen;

	KASSERT(sc->bios.data == NULL, ("Uninitialised BIOS variable"));
	KASSERT(sc->fep.data == NULL, ("Uninitialised FEP variable"));
	KASSERT(sc->link.data == NULL, ("Uninitialised LINK variable"));
	KASSERT(sc->module != NULL, ("Uninitialised module name"));

	/*-
	 * XXX: It'd be nice to have something like linker_search_path()
	 *	here.  For the moment we hardcode things - the comments
	 *	in linker_load_module() before the call to
	 *	linker_search_path() suggests that ``there will be a
	 *	system...''.
	 */
	modfile = malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
	snprintf(modfile, MAXPATHLEN, "/boot/kernel/digi_%s.ko", sc->module);
	if ((res = linker_load_file(modfile, &lf)) != 0)
		printf("%s: Failed %d to load module\n", modfile, res);
	free(modfile, M_TEMP);
	if (res != 0)
		return (res);

	symlen = strlen(sc->module) + 10;
	sym = malloc(symlen, M_TEMP, M_WAITOK);
	snprintf(sym, symlen, "digi_mod_%s", sc->module);
	if ((symptr = linker_file_lookup_symbol(lf, sym, 0)) == NULL)
		printf("digi_%s.ko: Symbol `%s' not found\n", sc->module, sym);
	free(sym, M_TEMP);

	digi_mod = (struct digi_mod *)symptr;
	if (digi_mod->dm_version != DIGI_MOD_VERSION) {
		printf("digi_%s.ko: Invalid version %d (need %d)\n",
		    sc->module, digi_mod->dm_version, DIGI_MOD_VERSION);
		linker_file_unload(lf);
		return (EINVAL);
	}

	sc->bios.size = digi_mod->dm_bios.size;
	if (sc->bios.size != 0 && digi_mod->dm_bios.data != NULL) {
		sc->bios.data = malloc(sc->bios.size, M_TTYS, M_WAIT);
		bcopy(digi_mod->dm_bios.data, sc->bios.data, sc->bios.size);
	}

	sc->fep.size = digi_mod->dm_fep.size;
	if (sc->fep.size != 0 && digi_mod->dm_fep.data != NULL) {
		sc->fep.data = malloc(sc->fep.size, M_TTYS, M_WAIT);
		bcopy(digi_mod->dm_fep.data, sc->fep.data, sc->fep.size);
	}

	sc->link.size = digi_mod->dm_link.size;
	if (sc->link.size != 0 && digi_mod->dm_link.data != NULL) {
		sc->link.data = malloc(sc->link.size, M_TTYS, M_WAIT);
		bcopy(digi_mod->dm_link.data, sc->link.data, sc->link.size);
	}

	linker_file_unload(lf);

	return (0);
}

static int
digiioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	int unit, pnum, mynor, error, s;
	struct digi_softc *sc;
	struct digi_p *port;
	struct tty *tp;
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
	int oldcmd;
	struct termios term;
#endif

	mynor = minor(dev);
	unit = MINOR_TO_UNIT(mynor);
	pnum = MINOR_TO_PORT(mynor);

	sc = (struct digi_softc *)devclass_get_softc(digi_devclass, unit);
	KASSERT(sc, ("digi%d: softc not allocated in digiioctl\n", unit));

	if (sc->status == DIGI_STATUS_DISABLED)
		return (ENXIO);

	if (mynor & CTRL_DEV) {
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
			*(digiModel_t *)data = sc->model;
			return (0);

		case DIGIIO_IDENT:
			return (copyout(sc->name, *(char **)data,
			    strlen(sc->name) + 1));
		}
	}

	if (pnum >= sc->numports)
		return (ENXIO);

	port = sc->ports + pnum;
	if (!(port->status & ENABLED))
		return (ENXIO);

	tp = port->tp;

	if (mynor & CONTROL_MASK) {
		struct termios *ct;

		switch (mynor & CONTROL_MASK) {
		case CONTROL_INIT_STATE:
			ct = (mynor & CALLOUT_MASK) ?
			    &port->it_out : &port->it_in;
			break;
		case CONTROL_LOCK_STATE:
			ct = (mynor & CALLOUT_MASK) ?
			    &port->lt_out : &port->lt_in;
			break;
		default:
			return (ENODEV);	/* /dev/nodev */
		}

		switch (cmd) {
		case TIOCSETA:
			error = suser(p);
			if (error != 0)
				return (error);
			*ct = *(struct termios *)data;

			return (0);
		case TIOCGETA:
			*(struct termios *)data = *ct;

			return (0);
		case TIOCGETD:
			*(int *)data = TTYDISC;
			return (0);
		case TIOCGWINSZ:
			bzero(data, sizeof(struct winsize));
			return (0);
		default:
			return (ENOTTY);
		}
	}
	tp = port->tp;
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
	term = tp->t_termios;
	oldcmd = cmd;
	error = ttsetcompat(tp, &cmd, data, &term);
	if (error != 0)
		return (error);
	if (cmd != oldcmd)
		data = (caddr_t) & term;
#endif
	if (cmd == TIOCSETA || cmd == TIOCSETAW || cmd == TIOCSETAF) {
		int cc;
		struct termios *dt;
		struct termios *lt;

		dt = (struct termios *)data;
		lt = (mynor & CALLOUT_MASK) ? &port->lt_out : &port->lt_in;

		dt->c_iflag =
		    (tp->t_iflag & lt->c_iflag) | (dt->c_iflag & ~lt->c_iflag);
		dt->c_oflag =
		    (tp->t_oflag & lt->c_oflag) | (dt->c_oflag & ~lt->c_oflag);
		dt->c_cflag =
		    (tp->t_cflag & lt->c_cflag) | (dt->c_cflag & ~lt->c_cflag);
		dt->c_lflag =
		    (tp->t_lflag & lt->c_lflag) | (dt->c_lflag & ~lt->c_lflag);
		port->c_iflag = dt->c_iflag & (IXOFF | IXON | IXANY);
		dt->c_iflag &= ~(IXOFF | IXON | IXANY);
		for (cc = 0; cc < NCCS; ++cc)
			if (lt->c_cc[cc] != 0)
				dt->c_cc[cc] = tp->t_cc[cc];
		if (lt->c_ispeed != 0)
			dt->c_ispeed = tp->t_ispeed;
		if (lt->c_ospeed != 0)
			dt->c_ospeed = tp->t_ospeed;
	}
	error = linesw[tp->t_line].l_ioctl(tp, cmd, data, flag, p);
	if (error == 0 && cmd == TIOCGETA)
		((struct termios *)data)->c_iflag |= port->c_iflag;

	if (error >= 0 && error != ENOIOCTL)
		return (error);
	s = spltty();
	error = ttioctl(tp, cmd, data, flag);
	if (error == 0 && cmd == TIOCGETA)
		((struct termios *)data)->c_iflag |= port->c_iflag;

	digi_disc_optim(tp, &tp->t_termios, port);
	if (error >= 0 && error != ENOIOCTL) {
		splx(s);
		return (error);
	}
	sc->setwin(sc, 0);
	switch (cmd) {
	case DIGIIO_RING:
		port->send_ring = *(u_char *)data;
		break;
	case TIOCSBRK:
		/*
		 * now it sends 250 millisecond break because I don't know
		 * how to send an infinite break
		 */
		fepcmd_w(port, SENDBREAK, 250, 10);
		break;
	case TIOCCBRK:
		/* now it's empty */
		break;
	case TIOCSDTR:
		digimctl(port, TIOCM_DTR, DMBIS);
		break;
	case TIOCCDTR:
		digimctl(port, TIOCM_DTR, DMBIC);
		break;
	case TIOCMSET:
		digimctl(port, *(int *)data, DMSET);
		break;
	case TIOCMBIS:
		digimctl(port, *(int *)data, DMBIS);
		break;
	case TIOCMBIC:
		digimctl(port, *(int *)data, DMBIC);
		break;
	case TIOCMGET:
		*(int *)data = digimctl(port, 0, DMGET);
		break;
	case TIOCMSDTRWAIT:
		error = suser(p);
		if (error != 0) {
			splx(s);
			return (error);
		}
		port->dtr_wait = *(int *)data *hz / 100;

		break;
	case TIOCMGDTRWAIT:
		*(int *)data = port->dtr_wait * 100 / hz;
		break;
#ifdef DIGI_INTERRUPT
	case TIOCTIMESTAMP:
		*(struct timeval *)data = sc->intr_timestamp;

		break;
#endif
	default:
		splx(s);
		return (ENOTTY);
	}
	splx(s);
	return (0);
}

static int
digiparam(struct tty *tp, struct termios *t)
{
	int mynor;
	int unit;
	int pnum;
	struct digi_softc *sc;
	struct digi_p *port;
	int cflag;
	int iflag;
	int hflow;
	int s;
	int window;

	mynor = minor(tp->t_dev);
	unit = MINOR_TO_UNIT(mynor);
	pnum = MINOR_TO_PORT(mynor);

	sc = (struct digi_softc *)devclass_get_softc(digi_devclass, unit);
	KASSERT(sc, ("digi%d: softc not allocated in digiparam\n", unit));

	port = &sc->ports[pnum];

	DLOG(DIGIDB_SET, (sc->dev, "port%d: setting parameters\n", pnum));

	if (t->c_ispeed == 0)
		t->c_ispeed = t->c_ospeed;

	cflag = ttspeedtab(t->c_ospeed, digispeedtab);

	if (cflag < 0 || (cflag > 0 && t->c_ispeed != t->c_ospeed))
		return (EINVAL);

	s = splclock();

	window = sc->window;
	sc->setwin(sc, 0);

	if (cflag == 0) {				/* hangup */
		DLOG(DIGIDB_SET, (sc->dev, "port%d: hangup\n", pnum));
		digimctl(port, TIOCM_DTR | TIOCM_RTS, DMBIC);
	} else {
		digimctl(port, TIOCM_DTR | TIOCM_RTS, DMBIS);

		DLOG(DIGIDB_SET, (sc->dev, "port%d: CBAUD = %d\n", pnum,
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
		DLOG(DIGIDB_SET, (sc->dev, "port%d: CFLAG = 0x%x\n", pnum,
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

	DLOG(DIGIDB_SET, (sc->dev, "port%d: set iflag = 0x%x\n", pnum, iflag));
	fepcmd_w(port, SETIFLAGS, (unsigned)iflag, 0);

	hflow = 0;
	if (t->c_cflag & CDTR_IFLOW)
		hflow |= sc->csigs->dtr;
	if (t->c_cflag & CRTS_IFLOW)
		hflow |= sc->csigs->rts;
	if (t->c_cflag & CCTS_OFLOW)
		hflow |= sc->csigs->cts;
	if (t->c_cflag & CDSR_OFLOW)
		hflow |= sc->csigs->dsr;
	if (t->c_cflag & CCAR_OFLOW)
		hflow |= sc->csigs->cd;

	DLOG(DIGIDB_SET, (sc->dev, "port%d: set hflow = 0x%x\n", pnum, hflow));
	fepcmd_w(port, SETHFLOW, 0xff00 | (unsigned)hflow, 0);

	DLOG(DIGIDB_SET, (sc->dev, "port%d: set startc(0x%x), stopc(0x%x)\n",
	    pnum, t->c_cc[VSTART], t->c_cc[VSTOP]));
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

	COM_LOCK();
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

		if (!(tp->t_state & TS_ISOPEN) && !port->wopeners) {
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
					linesw[tp->t_line].
					    l_rint(port->rxbuf[tail], tp);
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

			if ((event.mstat ^ event.lstat) & port->sc->csigs->cd) {
				sc->hidewin(sc);
				linesw[tp->t_line].l_modem
				    (tp, event.mstat & port->sc->csigs->cd);
				sc->setwin(sc, 0);
				wakeup(TSA_CARR_ON(tp));
			}

			if (event.mstat & sc->csigs->ri) {
				DLOG(DIGIDB_RI, (sc->dev, "port %d: RING\n",
				    event.pnum));
				if (port->send_ring) {
					linesw[tp->t_line].l_rint('R', tp);
					linesw[tp->t_line].l_rint('I', tp);
					linesw[tp->t_line].l_rint('N', tp);
					linesw[tp->t_line].l_rint('G', tp);
					linesw[tp->t_line].l_rint('\r', tp);
					linesw[tp->t_line].l_rint('\n', tp);
				}
			}
		}
		if (event.event & BREAK_IND) {
			DLOG(DIGIDB_MODEM, (sc->dev, "port %d: BREAK_IND\n",
			    event.pnum));
			linesw[tp->t_line].l_rint(TTY_BI, tp);
		}
		if (event.event & (LOWTX_IND | EMPTYTX_IND)) {
			DLOG(DIGIDB_IRQ, (sc->dev, "port %d:%s%s\n",
			    event.pnum,
			    event.event & LOWTX_IND ? " LOWTX" : "",
			    event.event & EMPTYTX_IND ?  " EMPTYTX" : ""));
			(*linesw[tp->t_line].l_start)(tp);
		}
	}
	sc->gdata->eout = etail;
eoi:
	if (sc->window != 0)
		sc->towin(sc, 0);
	if (window != 0)
		sc->towin(sc, window);
	COM_UNLOCK();
}

static void
digistart(struct tty *tp)
{
	int unit;
	int pnum;
	struct digi_p *port;
	struct digi_softc *sc;
	volatile struct board_chan *bc;
	int head, tail;
	int size, ocount, totcnt = 0;
	int s;
	int wmask;

	unit = MINOR_TO_UNIT(minor(tp->t_dev));
	pnum = MINOR_TO_PORT(minor(tp->t_dev));

	sc = (struct digi_softc *)devclass_get_softc(digi_devclass, unit);
	KASSERT(sc, ("digi%d: softc not allocated in digistart\n", unit));

	port = &sc->ports[pnum];
	bc = port->bc;

	wmask = port->txbufsize - 1;

	s = spltty();
	port->lcc = tp->t_outq.c_cc;
	sc->setwin(sc, 0);
	if (!(tp->t_state & TS_TBLOCK)) {
		if (port->status & PAUSE_RX) {
			DLOG(DIGIDB_RX, (sc->dev, "port %d: resume RX\n",
			    pnum));
			/*
			 * CAREFUL - braces are needed here if the DLOG is
			 * optimised out!
			 */
		}
		port->status &= ~PAUSE_RX;
		bc->idata = 1;
	}
	if (!(tp->t_state & TS_TTSTOP) && port->status & PAUSE_TX) {
		DLOG(DIGIDB_TX, (sc->dev, "port %d: resume TX\n", pnum));
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
		    pnum, head, tail));

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
	DLOG(DIGIDB_INT, (sc->dev, "port%d: s total cnt = %d\n", pnum, totcnt));
	ttwwakeup(tp);
	splx(s);
}

static void
digistop(struct tty *tp, int rw)
{
	struct digi_softc *sc;
	int unit;
	int pnum;
	struct digi_p *port;

	unit = MINOR_TO_UNIT(minor(tp->t_dev));
	pnum = MINOR_TO_PORT(minor(tp->t_dev));

	sc = (struct digi_softc *)devclass_get_softc(digi_devclass, unit);
	KASSERT(sc, ("digi%d: softc not allocated in digistop\n", unit));
	port = sc->ports + pnum;

	DLOG(DIGIDB_TX, (sc->dev, "port %d: pause TX\n", pnum));
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
	*(ushort *)(mem + head + 2) = op1;

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
	sc->res.ctldev = make_dev(&digi_sw,
	    (sc->res.unit << 16) | CTRL_DEV, UID_ROOT, GID_WHEEL,
	    0600, "digi%r.ctl", sc->res.unit);

	digi_loadmoduledata(sc);
	digi_init(sc);
	digi_freemoduledata(sc);

	return (0);
}

static int
digi_inuse(struct digi_softc *sc)
{
	int i;

	for (i = 0; i < sc->numports; i++)
		if (sc->ttys[i].t_state & TS_ISOPEN) {
			DLOG(DIGIDB_INIT, (sc->dev, "port%d: busy\n", i));
			return (1);
		} else if (sc->ports[i].wopeners || sc->ports[i].opencnt) {
			DLOG(DIGIDB_INIT, (sc->dev, "port%d: blocked in open\n",
			    i));
			return (1);
		}
	return (0);
}

static void
digi_free_state(struct digi_softc *sc)
{
	int d, i;

	/* Blow it all away */

	for (i = 0; i < sc->numports; i++)
		for (d = 0; d < 6; d++)
			destroy_dev(sc->ports[i].dev[d]);

	untimeout(digi_poll, sc, sc->callout);
	callout_handle_init(&sc->callout);
	untimeout(digi_int_test, sc, sc->inttest);
	callout_handle_init(&sc->inttest);

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
		KASSERT(sc->ttys, ("digi%d: Lost my ttys ?", sc->res.unit));
		FREE(sc->ports, M_TTYS);
		sc->ports = NULL;
		FREE(sc->ttys, M_TTYS);
		sc->ttys = NULL;
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

	destroy_dev(makedev(CDEV_MAJOR,
	    (sc->res.unit << 16) | CTRL_DEV));

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
