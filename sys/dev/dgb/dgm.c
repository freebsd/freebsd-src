/*-
 * $FreeBSD$
 *
 *  This driver and the associated header files support the ISA PC/Xem
 *  Digiboards.  Its evolutionary roots are described below.
 *  Jack O'Neill <jack@diamond.xtalwind.net>
 *
 *  Digiboard driver.
 *
 *  Stage 1. "Better than nothing".
 *  Stage 2. "Gee, it works!".
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions of binary code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, in the accompanying documentation.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *  Written by Sergey Babkin,
 *      Joint Stock Commercial Bank "Chelindbank"
 *      (Chelyabinsk, Russia)
 *      babkin@freebsd.org
 *
 *  Assorted hacks to make it more functional and working under 3.0-current.
 *  Fixed broken routines to prevent processes hanging on closed (thanks
 *  to Bruce for his patience and assistance). Thanks also to Maxim Bolotin
 *  <max@run.net> for his patches which did most of the work to get this
 *  running under 2.2/3.0-current.
 *  Implemented ioctls: TIOCMSDTRWAIT, TIOCMGDTRWAIT, TIOCTIMESTAMP &
 *  TIOCDCDTIMESTAMP.
 *  Sysctl debug flag is now a bitflag, to filter noise during debugging.
 *	David L. Nugent <davidn@blaze.net.au>
 *
 * New-busification by Brian Somers <brian@Awfulhak.org>
 *
 * There was a copyright confusion: I thought that having read the
 * GLPed drivers makes me mentally contaminated but in fact it does
 * not. Since the Linux driver by Troy De Jongh <troyd@digibd.com> or
 * <troyd@skypoint.com> was used only to learn the Digi's interface,
 * I've returned this driver to a BSD-style license. I tried to contact
 * all the contributors and those who replied agreed with license
 * change. If you did any contribution when the driver was GPLed and do
 * not agree with the BSD-style re-licensing please contact me.
 *  -SB
 */

/* How often to run dgmpoll */
#define POLLSPERSEC 25

/* How many charactes can we write to input tty rawq */
#define DGB_IBUFSIZE (TTYHOG - 100)

/* the overall number of ports controlled by this driver */

#include <sys/param.h>

#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/dkstat.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <sys/tty.h>
#include <sys/bus.h>
#include <sys/kobj.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <machine/clock.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <dev/dgb/dgmfep.h>
#include <dev/dgb/dgmbios.h>
#include <dev/dgb/dgmreg.h>

#define	CALLOUT_MASK		0x40000
#define	CONTROL_MASK		0xC0
#define	CONTROL_INIT_STATE	0x40
#define	CONTROL_LOCK_STATE	0x80
#define UNIT_MASK		0x30000
#define PORT_MASK		0x3F
#define	DEV_TO_UNIT(dev)	(MINOR_TO_UNIT(minor(dev)))
#define	MINOR_MAGIC_MASK	(CALLOUT_MASK | CONTROL_MASK)
#define	MINOR_TO_UNIT(mynor)	(((mynor) & UNIT_MASK) >> 16)
#define MINOR_TO_PORT(mynor)	((mynor) & PORT_MASK)
#define IO_SIZE			0x04
#define MEM_SIZE		0x8000

struct dgm_softc;

/* digiboard port structure */
struct dgm_p {
	unsigned enabled : 1;

	struct dgm_softc *sc;  /* parent softc */
	u_char pnum;           /* port number */
	u_char omodem;         /* FEP output modem status     */
	u_char imodem;         /* FEP input modem status      */
	u_char modemfake;      /* Modem values to be forced   */
	u_char modem;          /* Force values                */
	u_char hflow;
	u_char dsr;
	u_char dcd;
	u_char stopc;
	u_char startc;
	u_char stopca;
	u_char startca;
	u_char fepstopc;
	u_char fepstartc;
	u_char fepstopca;
	u_char fepstartca;
	u_char txwin;
	u_char rxwin;
	ushort fepiflag;
	ushort fepcflag;
	ushort fepoflag;
	ushort txbufhead;
	ushort txbufsize;
	ushort rxbufhead;
	ushort rxbufsize;
	int close_delay;
	u_char *txptr;
	u_char *rxptr;
	volatile struct board_chan *brdchan;
	struct tty *tty;

	u_char	active_out;	/* nonzero if the callout device is open */
	u_int	wopeners;	/* # processes waiting for DCD in open() */

	/* Initial state. */
	struct termios	it_in;	/* should be in struct tty */
	struct termios	it_out;

	/* Lock state. */
	struct termios	lt_in;	/* should be in struct tty */
	struct termios	lt_out;

	unsigned do_timestamp : 1;
	unsigned do_dcd_timestamp : 1;
	struct timeval	timestamp;
	struct timeval	dcd_timestamp;

	/* flags of state, are used in sleep() too */
	u_char closing;	/* port is being closed now */
	u_char draining; /* port is being drained now */
	u_char used;	/* port is being used now */
	u_char mustdrain; /* data must be waited to drain in dgmparam() */
};

/* Digiboard per-board structure */
struct dgm_softc {
	/* struct board_info */
	unsigned enabled : 1;
	u_char unit;			/* unit number */
	u_char type;			/* type of card: PCXE, PCXI, PCXEVE */
	u_char altpin;			/* do we need alternate pin setting ? */
	int numports;			/* number of ports on card */
	u_long port;			/* I/O port */
	u_char *vmem;			/* virtual memory address */
	u_long pmem;			/* physical memory address */
	int mem_seg;			/* internal memory segment */
	struct dgm_p *ports;		/* ptr to array of port descriptors */
	struct tty *ttys;		/* ptr to array of TTY structures */
	volatile struct global_data *mailbox;
	struct resource *io_res;
	struct resource *mem_res;
	int iorid;
	int mrid;
	struct callout_handle toh;	/* poll timeout handle */
};

static void	dgmpoll(void *);
static int	dgmprobe(device_t);
static int	dgmattach(device_t);
static int	dgmdetach(device_t);
static int	dgmshutdown(device_t);
static void	fepcmd(struct dgm_p *, unsigned, unsigned, unsigned, unsigned,
		    unsigned);
static void	dgmstart(struct tty *);
static void	dgmstop(struct tty *, int);
static int	dgmparam(struct tty *, struct termios *);
static void	dgmhardclose(struct dgm_p *);
static void	dgm_drain_or_flush(struct dgm_p *);
static int	dgmdrain(struct dgm_p *);
static void	dgm_pause(void *);
static void	wakeflush(void *);
static void	disc_optim(struct tty *, struct termios *);

static	d_open_t	dgmopen;
static	d_close_t	dgmclose;
static	d_ioctl_t	dgmioctl;

static device_method_t dgmmethods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, dgmprobe),
	DEVMETHOD(device_attach, dgmattach),
	DEVMETHOD(device_detach, dgmdetach),
	DEVMETHOD(device_shutdown, dgmshutdown),
	{ 0, 0 }
};

static driver_t dgmdriver = {
	"dgm",
	dgmmethods,
	sizeof (struct dgm_softc),
};

static devclass_t dgmdevclass;

#define	CDEV_MAJOR	101
static struct cdevsw dgm_cdevsw = {
	/* open */	dgmopen,
	/* close */	dgmclose,
	/* read */	ttyread,
	/* write */	ttywrite,
	/* ioctl */	dgmioctl,
	/* poll */	ttypoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"dgm",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	D_TTY | D_KQFILTER,
	/* bmaj */	-1,
	/* kqfilter */	ttykqfilter,
};

static int
dgmmodhandler(module_t mod, int event, void *arg)
{
	int res = 0;

	switch (event) {
	case MOD_LOAD:
		cdevsw_add(&dgm_cdevsw);
		break;

	case MOD_UNLOAD:
		cdevsw_remove(&dgm_cdevsw);
		break;
	}

	return res;
}

DRIVER_MODULE(dgm, isa, dgmdriver, dgmdevclass, dgmmodhandler, 0);

static	speed_t	dgmdefaultrate = TTYDEF_SPEED;

static	struct speedtab dgmspeedtab[] = {
	{ 0,		FEP_B0 }, /* old (sysV-like) Bx codes */
	{ 50,		FEP_B50 },
	{ 75,		FEP_B75 },
	{ 110,		FEP_B110 },
	{ 134,		FEP_B134 },
	{ 150,		FEP_B150 },
	{ 200,		FEP_B200 },
	{ 300,		FEP_B300 },
	{ 600,		FEP_B600 },
	{ 1200,		FEP_B1200 },
	{ 1800,		FEP_B1800 },
	{ 2400,		FEP_B2400 },
	{ 4800,		FEP_B4800 },
	{ 9600,		FEP_B9600 },
	{ 19200,	FEP_B19200 },
	{ 38400,	FEP_B38400 },
	{ 57600,	(FEP_FASTBAUD|FEP_B50) }, /* B50 & fast baud table */
	{ 115200,	(FEP_FASTBAUD|FEP_B110) }, /* B100 & fast baud table */
	{ -1,	-1 }
};

static struct dbgflagtbl {
	tcflag_t in_mask;
	tcflag_t in_val;
	tcflag_t out_val;
} dgm_cflags[] = {
	{ PARODD,   PARODD,	FEP_PARODD  },
	{ PARENB,   PARENB,	FEP_PARENB  },
	{ CSTOPB,   CSTOPB,	FEP_CSTOPB  },
	{ CSIZE,    CS5,	FEP_CS6     },
	{ CSIZE,    CS6,	FEP_CS6     },
	{ CSIZE,    CS7,	FEP_CS7     },
	{ CSIZE,    CS8,	FEP_CS8     },
	{ CLOCAL,   CLOCAL,	FEP_CLOCAL  },
	{ (tcflag_t)-1 }
}, dgm_iflags[] = {
	{ IGNBRK,   IGNBRK,     FEP_IGNBRK  },
	{ BRKINT,   BRKINT,	FEP_BRKINT  },
	{ IGNPAR,   IGNPAR,	FEP_IGNPAR  },
	{ PARMRK,   PARMRK,	FEP_PARMRK  },
	{ INPCK,    INPCK,	FEP_INPCK   },
	{ ISTRIP,   ISTRIP,	FEP_ISTRIP  },
	{ IXON,     IXON,	FEP_IXON    },
	{ IXOFF,    IXOFF,	FEP_IXOFF   },
	{ IXANY,    IXANY,	FEP_IXANY   },
	{ (tcflag_t)-1 }
}, dgm_flow[] = {
	{ CRTSCTS,  CRTSCTS,	CTS|RTS },
	{ CRTSCTS,  CCTS_OFLOW, CTS	},
	{ CRTSCTS,  CRTS_IFLOW, RTS	},
	{ (tcflag_t)-1 }
};

/* xlat bsd termios flags to dgm sys-v style */
static tcflag_t
dgmflags(struct dbgflagtbl *tbl, tcflag_t input)
{
	tcflag_t output = 0;
	int i;

	for (i = 0; tbl[i].in_mask != (tcflag_t)-1; i++)
		if ((input & tbl[i].in_mask) == tbl[i].in_val)
			output |= tbl[i].out_val;

	return output;
}

static int dgmdebug = 0;
SYSCTL_INT(_debug, OID_AUTO, dgm_debug, CTLFLAG_RW, &dgmdebug, 0, "");

static __inline int setwin(struct dgm_softc *, unsigned);
static __inline void hidewin(struct dgm_softc *);
static __inline void towin(struct dgm_softc *, int);

/*Helg: to allow recursive dgm...() calls */
typedef struct {
	/* If we were called and don't want to disturb we need: */
	int port;	/* write to this port */
	u_char data;	/* this data on exit */
	/* or DATA_WINOFF  to close memory window on entry */
} BoardMemWinState;	/* so several channels and even boards can coexist */

#define DATA_WINOFF 0
static BoardMemWinState bmws;

static u_long validio[] = { 0x104, 0x114, 0x124, 0x204, 0x224, 0x304, 0x324 };
static u_long validmem[] = {
	0x80000, 0x88000, 0x90000, 0x98000, 0xa0000, 0xa8000, 0xb0000, 0xb8000,
	0xc0000, 0xc8000, 0xd0000, 0xd8000, 0xe0000, 0xe8000, 0xf0000, 0xf8000,
	0xf0000000, 0xf1000000, 0xf2000000, 0xf3000000, 0xf4000000, 0xf5000000,
	0xf6000000, 0xf7000000, 0xf8000000, 0xf9000000, 0xfa000000, 0xfb000000,
	0xfc000000, 0xfd000000, 0xfe000000, 0xff000000
};

/* return current memory window state and close window */
static BoardMemWinState
bmws_get(void)
{
	BoardMemWinState bmwsRet = bmws;

	if (bmws.data != DATA_WINOFF)
		outb(bmws.port, bmws.data = DATA_WINOFF);
	return bmwsRet;
}

/* restore memory window state */
static void
bmws_set(BoardMemWinState ws)
{
	if (ws.data != bmws.data || ws.port != bmws.port) {
		if (bmws.data != DATA_WINOFF)
			outb(bmws.port, DATA_WINOFF);
		if (ws.data != DATA_WINOFF)
			outb(ws.port, ws.data);
		bmws = ws;
	}
}

static __inline int
setwin(struct dgm_softc *sc, unsigned int addr)
{
	outb(bmws.port = sc->port + 1, bmws.data = FEPWIN|(addr >> 15));
	return (addr & 0x7FFF);
}

static __inline void
hidewin(struct dgm_softc *sc)
{
	bmws.data = 0;
	outb(bmws.port = sc->port + 1, bmws.data);
}

static __inline void
towin(struct dgm_softc *sc, int win)
{
	outb(bmws.port = sc->port + 1, bmws.data = win);
}

static int
dgmprobe(device_t dev)
{
	struct dgm_softc *sc = device_get_softc(dev);
	int i, v;

	sc->unit = device_get_unit(dev);

	/* Check that we've got a valid i/o address */
	if ((sc->port = bus_get_resource_start(dev, SYS_RES_IOPORT, 0)) == 0)
		return (ENXIO);
	for (i = sizeof (validio) / sizeof (validio[0]) - 1; i >= 0; i--)
		if (sc->port == validio[i])
			break;
	if (i == -1) {
		device_printf(dev, "0x%03lx: Invalid i/o address\n", sc->port);
		return (ENXIO);
	}

	/* Ditto for our memory address */
	if ((sc->pmem = bus_get_resource_start(dev, SYS_RES_MEMORY, 0)) == 0)
		return (ENXIO);
	for (i = sizeof (validmem) / sizeof (validmem[0]) - 1; i >= 0; i--)
		if (sc->pmem == validmem[i])
			break;
	if (i == -1) {
		device_printf(dev, "0x%lx: Invalid memory address\n", sc->pmem);
		return (ENXIO);
	}
	if ((sc->pmem & 0xFFFFFFul) != sc->pmem) {
		device_printf(dev, "0x%lx: Memory address not supported\n",
		    sc->pmem);
		return (ENXIO);
	}
	sc->vmem = (u_char *)sc->pmem;

	DPRINT4(DB_INFO, "dgm%d: port 0x%lx mem 0x%lx\n", sc->unit,
	    sc->port, sc->pmem);

	/* Temporarily map our io ports */
	sc->iorid = 0;
	sc->io_res = bus_alloc_resource(dev, SYS_RES_IOPORT, &sc->iorid,
	    0ul, ~0ul, IO_SIZE, RF_ACTIVE);
	if (sc->io_res == NULL)
		return (ENXIO);

	outb(sc->port, FEPRST);
	sc->enabled = 0;

	for (i = 0; i < 1000; i++) {
		DELAY(1);
		if ((inb(sc->port) & FEPMASK) == FEPRST) {
			sc->enabled = 1;
			DPRINT3(DB_EXCEPT, "dgm%d: got reset after %d us\n",
			    sc->unit, i);
			break;
		}
	}

	if (!sc->enabled) {
		DPRINT2(DB_EXCEPT, "dgm%d: failed to respond\n", sc->unit);
		bus_release_resource(dev, SYS_RES_IOPORT, sc->iorid, sc->io_res);
		return (ENXIO);
	}

	/* check type of card and get internal memory characteristics */

	v = inb(sc->port);

	if (!(v & 0x1)) {
		int second;

		outb(sc->port, 1);
		second = inb(sc->port);
		printf("dgm%d: PC/Xem (type %d, %d)\n", sc->unit, v, second);
	} else
		printf("dgm%d: PC/Xem (type %d)\n", sc->unit, v);

	sc->type = PCXEM;
	sc->mem_seg = 0x8000;

	/* Temporarily map our memory too */
	sc->mrid = 0;
	sc->mem_res = bus_alloc_resource(dev, SYS_RES_MEMORY, &sc->mrid,
	    0ul, ~0ul, MEM_SIZE, RF_ALLOCATED);
	if (sc->mem_res == NULL) {
		device_printf(dev, "0x%lx: Memory range is in use\n", sc->pmem);
		bus_release_resource(dev, SYS_RES_IOPORT, sc->iorid, sc->io_res);
		return (ENXIO);
	}

	outb(sc->port, FEPCLR);	/* drop RESET */
	hidewin(sc);	/* Helg: to set initial bmws state */

	bus_release_resource(dev, SYS_RES_MEMORY, sc->mrid, sc->mem_res);
	bus_release_resource(dev, SYS_RES_IOPORT, sc->iorid, sc->io_res);

	bus_set_resource(dev, SYS_RES_IOPORT, 0, sc->port, IO_SIZE);
	bus_set_resource(dev, SYS_RES_MEMORY, 0, sc->pmem, MEM_SIZE);

	DPRINT2(DB_INFO, "dgm%d: Probe returns 0\n", sc->unit);

	return (0);
}

static int
dgmattach(device_t dev)
{
	struct dgm_softc *sc = device_get_softc(dev);
	int i, t;
	u_char *mem;
	u_char *ptr;
	int addr;
	struct dgm_p *port;
	volatile struct board_chan *bc;
	int shrinkmem;
	int lowwater;
	u_long msize, iosize;

	DPRINT2(DB_INFO, "dbg%d: attaching\n", device_get_unit(dev));

	sc->unit = device_get_unit(dev);
	bus_get_resource(dev, SYS_RES_IOPORT, 0, &sc->port, &iosize);
	bus_get_resource(dev, SYS_RES_MEMORY, 0, &sc->pmem, &msize);
	sc->altpin = !!(device_get_flags(dev) & DGBFLAG_ALTPIN);
	sc->type = PCXEM;
	sc->mem_seg = 0x8000;
	sc->enabled = 1;
	sc->type = PCXEM;
	sc->mem_seg = 0x8000;

	/* Allocate resources (should have been verified in dgmprobe()) */
	sc->iorid = 0;
	sc->io_res = bus_alloc_resource(dev, SYS_RES_IOPORT, &sc->iorid,
	    0ul, ~0ul, iosize, RF_ACTIVE);
	if (sc->io_res == NULL)
		return (ENXIO);
	sc->mrid = 0;
	sc->mem_res = bus_alloc_resource(dev, SYS_RES_MEMORY, &sc->mrid,
	    0ul, ~0ul, msize, RF_ACTIVE);
	if (sc->mem_res == NULL) {
		device_printf(dev, "0x%lx: Memory range is in use\n", sc->pmem);
		bus_release_resource(dev, SYS_RES_IOPORT, sc->iorid, sc->io_res);
		return (ENXIO);
	}

	/* map memory */
	mem = sc->vmem = pmap_mapdev(sc->pmem, msize);

	DPRINT3(DB_INFO, "dgm%d: internal memory segment 0x%x\n", sc->unit,
	    sc->mem_seg);

	outb(sc->port, FEPRST);
	DELAY(1);

	for (i = 0; (inb(sc->port) & FEPMASK) != FEPRST; i++) {
		if (i > 10000) {
			device_printf(dev, "1st reset failed\n");
			sc->enabled = 0;
			hidewin(sc);
			bus_release_resource(dev, SYS_RES_MEMORY, sc->mrid, sc->mem_res);
			bus_release_resource(dev, SYS_RES_IOPORT, sc->iorid, sc->io_res);
			return (ENXIO);
		}
		DELAY(1);
	}

	DPRINT3(DB_INFO, "dgm%d: got reset after %d us\n", sc->unit, i);

	t = sc->pmem >> 8;	/* disable windowing */
	outb(sc->port + 2, t & 0xFF);
	outb(sc->port + 3, t >> 8);

	mem = sc->vmem;

	/* very short memory test */
	DPRINT2(DB_INFO, "dbg%d: short memory test\n", sc->unit);

	addr = setwin(sc, BOTWIN);
	*(u_long *)(mem + addr) = 0xA55A3CC3;
	if (*(u_long *)(mem + addr) != 0xA55A3CC3) {
		device_printf(dev, "1st memory test failed\n");
		sc->enabled = 0;
		hidewin(sc);
		bus_release_resource(dev, SYS_RES_MEMORY, sc->mrid, sc->mem_res);
		bus_release_resource(dev, SYS_RES_IOPORT, sc->iorid, sc->io_res);
		return (ENXIO);
	}

	DPRINT2(DB_INFO, "dbg%d: 1st memory test ok\n", sc->unit);

	addr = setwin(sc, TOPWIN);
	*(u_long *)(mem + addr) = 0x5AA5C33C;
	if (*(u_long *)(mem + addr) != 0x5AA5C33C) {
		device_printf(dev, "2nd memory test failed\n");
		sc->enabled = 0;
		hidewin(sc);
		bus_release_resource(dev, SYS_RES_MEMORY, sc->mrid, sc->mem_res);
		bus_release_resource(dev, SYS_RES_IOPORT, sc->iorid, sc->io_res);
		return (ENXIO);
	}

	DPRINT2(DB_INFO, "dbg%d: 2nd memory test ok\n", sc->unit);

	addr = setwin(sc, BIOSCODE + ((0xF000 - sc->mem_seg) << 4));
	*(u_long *)(mem + addr) = 0x5AA5C33C;
	if (*(u_long *)(mem + addr) != 0x5AA5C33C)
		device_printf(dev, "3rd (BIOS) memory test failed\n");

	DPRINT2(DB_INFO, "dbg%d: 3rd memory test ok\n", sc->unit);

	addr = setwin(sc, MISCGLOBAL);
	for (i = 0; i < 16; i++)
		mem[addr + i] = 0;

	addr = setwin(sc, BIOSOFFSET);
	ptr = mem + addr;
	for (i = 0; ptr < mem + msize; i++)
		*ptr++ = pcem_bios[i];

	ptr = mem + BIOSOFFSET;
	for (i = 0; ptr < mem + msize; i++) {
		if (*ptr++ != pcem_bios[i]) {
			printf("Low BIOS load failed\n");
			sc->enabled = 0;
			hidewin(sc);
			bus_release_resource(dev, SYS_RES_MEMORY, sc->mrid, sc->mem_res);
			bus_release_resource(dev, SYS_RES_IOPORT, sc->iorid, sc->io_res);
			return (ENXIO);
		}
	}
	DPRINT2(DB_INFO, "dbg%d: pcem_bios seg 1 loaded\n", sc->unit);

	addr = setwin(sc, msize);
	ptr = mem + addr;
	for (;i < pcem_nbios; i++)
		*ptr++ = pcem_bios[i];

	ptr = mem;
	for (i = msize - BIOSOFFSET; i < pcem_nbios; i++) {
		if (*ptr++ != pcem_bios[i]) {
			printf("High BIOS load failed\n");
			sc->enabled = 0;
			hidewin(sc);
			bus_release_resource(dev, SYS_RES_MEMORY, sc->mrid, sc->mem_res);
			bus_release_resource(dev, SYS_RES_IOPORT, sc->iorid, sc->io_res);
			return (ENXIO);
		}
	}
	DPRINT2(DB_INFO, "dbg%d: pcem_bios seg 2 loaded\n", sc->unit);
	device_printf(dev, "DigiBIOS loaded, initializing");

	addr = setwin(sc, 0);

	*(u_int *)(mem + addr) = 0x0bf00401;
	*(u_int *)(mem + addr + 4) = 0;
	*(ushort *)(mem + addr + 0xc00) = 0;
	outb(sc->port, 0);

	for (i = 0; *(u_char *)(mem + addr + 0xc00) != 0x47; i++) {
		DELAY(10000);
		if (i > 3000) {
			printf("\nBIOS initialize failed(1)\n");
			sc->enabled = 0;
			hidewin(sc);
			bus_release_resource(dev, SYS_RES_MEMORY, sc->mrid, sc->mem_res);
			bus_release_resource(dev, SYS_RES_IOPORT, sc->iorid, sc->io_res);
			return (ENXIO);
		}
	}

	if (*(u_char *)(mem + addr + 0xc01) != 0x44) {
		printf("\nBIOS initialize failed(2)\n");
		sc->enabled = 0;
		hidewin(sc);
		bus_release_resource(dev, SYS_RES_MEMORY, sc->mrid, sc->mem_res);
		bus_release_resource(dev, SYS_RES_IOPORT, sc->iorid, sc->io_res);
		return (ENXIO);
	}
	printf(", DigiBIOS running\n");

	DELAY(10000);

	addr = setwin(sc, BIOSOFFSET);
	ptr = mem + addr;
	for (i = 0; i < pcem_ncook; i++)
		*ptr++ = pcem_cook[i];

	ptr = mem + BIOSOFFSET;
	for (i = 0; i < pcem_ncook; i++) {
		if (*ptr++ != pcem_cook[i]) {
			printf("FEP/OS load failed\n");
			sc->enabled = 0;
			hidewin(sc);
			bus_release_resource(dev, SYS_RES_MEMORY, sc->mrid, sc->mem_res);
			bus_release_resource(dev, SYS_RES_IOPORT, sc->iorid, sc->io_res);
			return (ENXIO);
		}
	}
	device_printf(dev, "FEP/OS loaded, initializing");

	addr = setwin(sc, 0);
	*(ushort *)(mem + addr + 0xd20) = 0;
	*(u_int *)(mem + addr + 0xc34) = 0xbfc01004;
	*(u_int *)(mem + addr + 0xc30) = 0x3L;
	outb(sc->port, 0);

	for (i = 0; *(u_char *)(mem + addr + 0xd20) != 'O'; i++) {
		DELAY(10000);
		if (i > 3000) {
			printf("\nFEP/OS initialize failed(1)\n");
			sc->enabled = 0;
			hidewin(sc);
			bus_release_resource(dev, SYS_RES_MEMORY, sc->mrid, sc->mem_res);
			bus_release_resource(dev, SYS_RES_IOPORT, sc->iorid, sc->io_res);
			return (ENXIO);
		}
	}

	if (*(u_char *)(mem + addr + 0xd21) != 'S') {
		printf("\nFEP/OS initialize failed(2)\n");
		sc->enabled = 0;
		hidewin(sc);
		bus_release_resource(dev, SYS_RES_MEMORY, sc->mrid, sc->mem_res);
		bus_release_resource(dev, SYS_RES_IOPORT, sc->iorid, sc->io_res);
		return (ENXIO);
	}
	printf(", FEP/OS running\n");

	sc->numports = *(ushort *)(mem + setwin(sc, NPORT));
	device_printf(dev, "%d ports attached\n", sc->numports);

	if (sc->numports > MAX_DGM_PORTS) {
		printf("dgm%d: too many ports\n", sc->unit);
		sc->enabled = 0;
		hidewin(sc);
		bus_release_resource(dev, SYS_RES_MEMORY, sc->mrid, sc->mem_res);
		bus_release_resource(dev, SYS_RES_IOPORT, sc->iorid, sc->io_res);
		return (ENXIO);
	}

	MALLOC(sc->ports, struct dgm_p *, sizeof (*sc->ports) * sc->numports,
	    M_TTYS, M_WAITOK|M_ZERO);
	MALLOC(sc->ttys, struct tty *, sizeof (*sc->ttys) * sc->numports,
	    M_TTYS, M_WAITOK|M_ZERO);

	DPRINT3(DB_INFO, "dgm%d: enable %d ports\n", sc->unit, sc->numports);
	for (i = 0; i < sc->numports; i++)
		sc->ports[i].enabled = 1;

	/* We should now init per-port structures */
	setwin(sc, 0);
	bc = (volatile struct board_chan *)(mem + CHANSTRUCT);
	sc->mailbox = (volatile struct global_data *)(mem + FEP_GLOBAL);

	if (sc->numports < 3)
		shrinkmem = 1;
	else
		shrinkmem = 0;

	for (i = 0; i < sc->numports; i++, bc++) {
		DPRINT3(DB_INFO, "dgm%d: Set up port %d\n", sc->unit, i);
		port = &sc->ports[i];
		port->sc = sc;

		port->tty = &sc->ttys[i];

		port->brdchan = bc;

		port->dcd = CD;
		port->dsr = DSR;
		port->pnum = i;

		DPRINT3(DB_INFO, "dgm%d port %d: shrinkmem ?\n", sc->unit, i);
		if (shrinkmem) {
			DPRINT2(DB_INFO, "dgm%d: shrinking memory\n", sc->unit);
			fepcmd(port, SETBUFFER, 32, 0, 0, 0);
			shrinkmem = 0;
		}

		DPRINT3(DB_INFO, "dgm%d port %d: assign ptrs\n", sc->unit, i);
		port->txptr = mem + ((bc->tseg << 4) & 0x7FFF);
		port->rxptr = mem + ((bc->rseg << 4) & 0x7FFF);
		port->txwin = FEPWIN | (bc->tseg >> 11);
		port->rxwin = FEPWIN | (bc->rseg >> 11);

		port->txbufhead = 0;
		port->rxbufhead = 0;
		port->txbufsize = bc->tmax + 1;
		port->rxbufsize = bc->rmax + 1;

		lowwater = (port->txbufsize >= 2000) ?
		    1024 : (port->txbufsize / 2);

		setwin(sc, 0);
		DPRINT4(DB_INFO, "dgm%d port %d: fepcmd STXLWATER %d\n",
		    sc->unit, i, lowwater);
		fepcmd(port, STXLWATER, lowwater, 0, 10, 0);
		DPRINT4(DB_INFO, "dgm%d port %d: fepcmd SRXLWATER %d\n",
		    sc->unit, i, port->rxbufsize / 4);
		fepcmd(port, SRXLWATER, port->rxbufsize / 4, 0, 10, 0);
		DPRINT4(DB_INFO, "dgm%d port %d: fepcmd SRXHWATER %d\n",
		    sc->unit, i, 3 * port->rxbufsize / 4);
		fepcmd(port, SRXHWATER, 3 * port->rxbufsize / 4, 0, 10, 0);

		bc->edelay = 100;
		bc->idata = 1;

		port->startc = bc->startc;
		port->startca = bc->startca;
		port->stopc = bc->stopc;
		port->stopca = bc->stopca;

		/* port->close_delay = 50; */
		port->close_delay = 3 * hz;
		port->do_timestamp = 0;
		port->do_dcd_timestamp = 0;

		DPRINT3(DB_INFO, "dgm%d port %d: setup flags\n", sc->unit, i);
		/*
		 * We don't use all the flags from <sys/ttydefaults.h> since
		 * they are only relevant for logins.  It's important to have
		 * echo off initially so that the line doesn't start
		 * blathering before the echo flag can be turned off.
		 */
		port->it_in.c_iflag = TTYDEF_IFLAG;
		port->it_in.c_oflag = TTYDEF_OFLAG;
		port->it_in.c_cflag = TTYDEF_CFLAG;
		port->it_in.c_lflag = TTYDEF_LFLAG;
		termioschars(&port->it_in);
		port->it_in.c_ispeed = port->it_in.c_ospeed = dgmdefaultrate;
		port->it_out = port->it_in;

		DPRINT3(DB_INFO, "dgm%d port %d: make devices\n", sc->unit, i);
		make_dev(&dgm_cdevsw, (sc->unit*65536) + i, UID_ROOT,
		    GID_WHEEL, 0600, "ttyM%d%x", sc->unit, i + 0xa0);
		make_dev(&dgm_cdevsw, sc->unit * 65536 + i + 64, UID_ROOT,
		    GID_WHEEL, 0600, "ttyiM%d%x", sc->unit, i + 0xa0);
		make_dev(&dgm_cdevsw, sc->unit * 65536 + i + 128, UID_ROOT,
		    GID_WHEEL, 0600, "ttylM%d%x", sc->unit, i + 0xa0);
		make_dev(&dgm_cdevsw, sc->unit * 65536 + i + 262144, UID_UUCP,
		    GID_DIALER, 0660, "cuaM%d%x", sc->unit, i + 0xa0);
		make_dev(&dgm_cdevsw, sc->unit * 65536 + i + 262208, UID_UUCP,
		    GID_DIALER, 0660, "cuaiM%d%x", sc->unit, i + 0xa0);
		make_dev(&dgm_cdevsw, sc->unit * 65536 + i + 262272, UID_UUCP,
		    GID_DIALER, 0660, "cualM%d%x", sc->unit, i + 0xa0);
	}

	DPRINT3(DB_INFO, "dgm%d: %d device nodes created\n", sc->unit, sc->numports);

	hidewin(sc);

	/* start the polling function */
	sc->toh = timeout(dgmpoll, (void *)(int)sc->unit, hz / POLLSPERSEC);

	DPRINT2(DB_INFO, "dgm%d: poll thread started\n", sc->unit);

	return (0);
}

static int
dgmdetach(device_t dev)
{
	struct dgm_softc *sc = device_get_softc(dev);
	int i;

	for (i = 0; i < sc->numports; i++)
		if (sc->ttys[i].t_state & TS_ISOPEN)
			return (EBUSY);

	DPRINT2(DB_INFO, "dgm%d: detach\n", sc->unit);

	for (i = 0; i < sc->numports; i++) {
		destroy_dev(makedev(CDEV_MAJOR, sc->unit * 65536 + i));
		destroy_dev(makedev(CDEV_MAJOR, sc->unit * 65536 + i + 64));
		destroy_dev(makedev(CDEV_MAJOR, sc->unit * 65536 + i + 128));
		destroy_dev(makedev(CDEV_MAJOR, sc->unit * 65536 + i + 262144));
		destroy_dev(makedev(CDEV_MAJOR, sc->unit * 65536 + i + 262208));
		destroy_dev(makedev(CDEV_MAJOR, sc->unit * 65536 + i + 262272));
	}

	untimeout(dgmpoll, (void *)(int)sc->unit, sc->toh);
	callout_handle_init(&sc->toh);

	bus_release_resource(dev, SYS_RES_MEMORY, sc->mrid, sc->mem_res);
	bus_release_resource(dev, SYS_RES_IOPORT, sc->iorid, sc->io_res);

	FREE(sc->ports, M_TTYS);
	FREE(sc->ttys, M_TTYS);

	return (0);
}

int
dgmshutdown(device_t dev)
{
#ifdef DEBUG
	struct dgm_softc *sc = device_get_softc(dev);

	DPRINT2(DB_INFO, "dgm%d: shutdown\n", sc->unit);
#endif

	return 0;
}

/* ARGSUSED */
static int
dgmopen(dev_t dev, int flag, int mode, struct proc *p)
{
	struct dgm_softc *sc;
	struct tty *tp;
	int unit;
	int mynor;
	int pnum;
	struct dgm_p *port;
	int s, cs;
	int error;
	volatile struct board_chan *bc;

	error = 0;
	mynor = minor(dev);
	unit = MINOR_TO_UNIT(mynor);
	pnum = MINOR_TO_PORT(mynor);

	sc = devclass_get_softc(dgmdevclass, unit);
	if (sc == NULL) {
		DPRINT2(DB_EXCEPT, "dgm%d: try to open a nonexisting card\n",
		    unit);
		return ENXIO;
	}

	DPRINT2(DB_INFO, "dgm%d: open\n", sc->unit);

	if (!sc->enabled) {
		DPRINT2(DB_EXCEPT, "dgm%d: try to open a disabled card\n",
		    unit);
		return ENXIO;
	}

	if (pnum >= sc->numports) {
		DPRINT3(DB_EXCEPT, "dgm%d: try to open non-existing port %d\n",
		    unit, pnum);
		return ENXIO;
	}

	if (mynor & CONTROL_MASK)
		return 0;

	tp = &sc->ttys[pnum];
	dev->si_tty = tp;
	port = &sc->ports[pnum];
	bc = port->brdchan;

open_top:
	s = spltty();

	while (port->closing) {
		error = tsleep(&port->closing, TTOPRI|PCATCH, "dgocl", 0);

		if (error) {
			DPRINT4(DB_OPEN, "dgm%d: port%d: tsleep(dgocl)"
			    " error = %d\n", unit, pnum, error);
			goto out;
		}
	}

	if (tp->t_state & TS_ISOPEN) {
		/*
		 * The device is open, so everything has been initialized.
		 * Handle conflicts.
		 */
		if (mynor & CALLOUT_MASK) {
			if (!port->active_out) {
				error = EBUSY;
				DPRINT4(DB_OPEN, "dgm%d: port%d:"
				    " BUSY error = %d\n", unit, pnum, error);
				goto out;
			}
		} else if (port->active_out) {
			if (flag & O_NONBLOCK) {
				error = EBUSY;
				DPRINT4(DB_OPEN, "dgm%d: port%d:"
				    " BUSY error = %d\n", unit, pnum, error);
				goto out;
			}
			error = tsleep(&port->active_out,
			    TTIPRI | PCATCH, "dgmi", 0);
			if (error != 0) {
				DPRINT4(DB_OPEN, "dgm%d: port%d: tsleep(dgmi)"
				    " error = %d\n", unit, pnum, error);
				goto out;
			}
			splx(s);
			goto open_top;
		}
		if (tp->t_state & TS_XCLUDE && suser(p)) {
			error = EBUSY;
			goto out;
		}
	} else {
		/*
		 * The device isn't open, so there are no conflicts.
		 * Initialize it.  Initialization is done twice in many
		 * cases: to preempt sleeping callin opens if we are
		 * callout, and to complete a callin open after DCD rises.
		 */
		tp->t_oproc = dgmstart;
		tp->t_param = dgmparam;
		tp->t_stop = dgmstop;
		tp->t_dev = dev;
		tp->t_termios= (mynor & CALLOUT_MASK) ?
							port->it_out :
							port->it_in;

		cs = splclock();
		setwin(sc, 0);
		port->imodem = bc->mstat;
		bc->rout = bc->rin; /* clear input queue */
		bc->idata = 1;
#ifdef PRINT_BUFSIZE
		printf("dgm buffers tx = %x:%x rx = %x:%x\n",
		    bc->tseg, bc->tmax, bc->rseg, bc->rmax);
#endif

		hidewin(sc);
		splx(cs);

		port->wopeners++;
		error = dgmparam(tp, &tp->t_termios);
		port->wopeners--;

		if (error != 0) {
			DPRINT4(DB_OPEN, "dgm%d: port%d: dgmparam error = %d\n",
			    unit, pnum, error);
			goto out;
		}

		/* handle fake DCD for callout devices */
		/* and initial DCD */

		if ((port->imodem & port->dcd) || mynor & CALLOUT_MASK)
			linesw[tp->t_line].l_modem(tp, 1);
	}

	/*
	 * Wait for DCD if necessary.
	 */
	if (!(tp->t_state & TS_CARR_ON) && !(mynor & CALLOUT_MASK)
	    && !(tp->t_cflag & CLOCAL) && !(flag & O_NONBLOCK)) {
		++port->wopeners;
		error = tsleep(TSA_CARR_ON(tp), TTIPRI | PCATCH, "dgdcd", 0);
		--port->wopeners;
		if (error != 0) {
			DPRINT4(DB_OPEN, "dgm%d: port%d: tsleep(dgdcd)"
			    " error = %d\n", unit, pnum, error);
			goto out;
		}
		splx(s);
		goto open_top;
	}
	error = linesw[tp->t_line].l_open(dev, tp);
	disc_optim(tp, &tp->t_termios);
	DPRINT4(DB_OPEN, "dgm%d: port%d: l_open error = %d\n",
	    unit, pnum, error);

	if (tp->t_state & TS_ISOPEN && mynor & CALLOUT_MASK)
		port->active_out = 1;

	port->used = 1;

	/* If any port is open (i.e. the open() call is completed for it)
	 * the device is busy
	 */

out:
	disc_optim(tp, &tp->t_termios);
	splx(s);

	if (!(tp->t_state & TS_ISOPEN) && port->wopeners == 0)
		dgmhardclose(port);

	DPRINT4(DB_OPEN, "dgm%d: port%d: open() returns %d\n",
	    unit, pnum, error);

	return error;
}

/*ARGSUSED*/
static int
dgmclose(dev_t dev, int flag, int mode, struct proc *p)
{
	int		mynor;
	struct tty	*tp;
	int unit, pnum;
	struct dgm_softc *sc;
	struct dgm_p *port;
	int s;
	int i;

	mynor = minor(dev);
	if (mynor & CONTROL_MASK)
		return 0;
	unit = MINOR_TO_UNIT(mynor);
	pnum = MINOR_TO_PORT(mynor);

	sc = devclass_get_softc(dgmdevclass, unit);
	tp = &sc->ttys[pnum];
	port = sc->ports + pnum;

	DPRINT3(DB_CLOSE, "dgm%d: port%d: closing\n", unit, pnum);

	DPRINT3(DB_CLOSE, "dgm%d: port%d: draining port\n", unit, pnum);
        dgm_drain_or_flush(port);

	s = spltty();

	port->closing = 1;
	DPRINT3(DB_CLOSE, "dgm%d: port%d: closing line disc\n", unit, pnum);
	linesw[tp->t_line].l_close(tp, flag);
	disc_optim(tp, &tp->t_termios);

	DPRINT3(DB_CLOSE, "dgm%d: port%d: hard closing\n", unit, pnum);
	dgmhardclose(port);
	DPRINT3(DB_CLOSE, "dgm%d: port%d: closing tty\n", unit, pnum);
	ttyclose(tp);
	port->closing = 0;
	wakeup(&port->closing);
	port->used = 0;

	/* mark the card idle when all ports are closed */

	for (i = 0; i < sc->numports; i++)
		if (sc->ports[i].used)
			break;

	splx(s);

	DPRINT3(DB_CLOSE, "dgm%d: port%d: closed\n", unit, pnum);

	wakeup(TSA_CARR_ON(tp));
	wakeup(&port->active_out);
	port->active_out = 0;

	DPRINT3(DB_CLOSE, "dgm%d: port%d: close exit\n", unit, pnum);

	return 0;
}

static void
dgmhardclose(struct dgm_p *port)
{
	volatile struct board_chan *bc = port->brdchan;
	struct dgm_softc *sc;
	int cs;

	sc = devclass_get_softc(dgmdevclass, port->sc->unit);
	DPRINT2(DB_INFO, "dgm%d: dgmhardclose\n", sc->unit);
	cs = splclock();
	port->do_timestamp = 0;
	setwin(sc, 0);

	bc->idata = 0;
	bc->iempty = 0;
	bc->ilow = 0;
	if (port->tty->t_cflag & HUPCL) {
		port->omodem &= ~(RTS|DTR);
		fepcmd(port, SETMODEM, 0, DTR|RTS, 0, 1);
	}

	hidewin(sc);
	splx(cs);

	timeout(dgm_pause, &port->brdchan, hz/2);
	tsleep(&port->brdchan, TTIPRI | PCATCH, "dgclo", 0);
}

static void
dgm_pause(void *chan)
{
	wakeup((caddr_t)chan);
}

static void
dgmpoll(void *unit_c)
{
	int unit = (int)unit_c;
	int pnum;
	struct dgm_p *port;
	struct dgm_softc *sc;
	int head, tail;
	u_char *eventbuf;
	int event, mstat, lstat;
	volatile struct board_chan *bc;
	struct tty *tp;
	int rhead, rtail;
	int whead, wtail;
	int size;
	u_char *ptr;
	int ocount;
	int ibuf_full, obuf_full;
	BoardMemWinState ws = bmws_get();

	sc = devclass_get_softc(dgmdevclass, unit);
	DPRINT2(DB_INFO, "dgm%d: poll\n", sc->unit);
	callout_handle_init(&sc->toh);

	if (!sc->enabled) {
		printf("dgm%d: polling of disabled board stopped\n", unit);
		return;
	}

	setwin(sc, 0);

	head = sc->mailbox->ein;
	tail = sc->mailbox->eout;

	while (head != tail) {
		if (head >= FEP_IMAX - FEP_ISTART
		|| tail >= FEP_IMAX - FEP_ISTART
		|| (head|tail) & 03 ) {
			printf("dgm%d: event queue's head or tail is wrong!"
			    " hd = %d, tl = %d\n", unit, head, tail);
			break;
		}

		eventbuf = sc->vmem + tail + FEP_ISTART;
		pnum = eventbuf[0];
		event = eventbuf[1];
		mstat = eventbuf[2];
		lstat = eventbuf[3];

		port = &sc->ports[pnum];
		bc = port->brdchan;
		tp = &sc->ttys[pnum];

		if (pnum >= sc->numports || !port->enabled) {
			printf("dgm%d: port%d: got event on nonexisting port\n",
			    unit, pnum);
		} else if (port->used || port->wopeners > 0 ) {

			int wrapmask = port->rxbufsize - 1;

			if (!(event & ALL_IND))
				printf("dgm%d: port%d: ? event 0x%x mstat 0x%x lstat 0x%x\n",
					unit, pnum, event, mstat, lstat);

			if (event & DATA_IND) {
				DPRINT3(DB_DATA, "dgm%d: port%d: DATA_IND\n",
				    unit, pnum);

				rhead = bc->rin & wrapmask;
				rtail = bc->rout & wrapmask;

				if (!(tp->t_cflag & CREAD) || !port->used ) {
					bc->rout = rhead;
					goto end_of_data;
				}

				if (bc->orun) {
					printf("dgm%d: port%d: overrun\n", unit, pnum);
					bc->orun = 0;
				}

				if (!(tp->t_state & TS_ISOPEN))
					goto end_of_data;

				for (ibuf_full = FALSE; rhead != rtail && !ibuf_full;) {
					DPRINT5(DB_RXDATA, "dgm%d: port%d:"
					    " p rx head = %d tail = %d\n", unit,
					    pnum, rhead, rtail);

					if (rhead > rtail)
						size = rhead - rtail;
					else
						size = port->rxbufsize - rtail;

					ptr = port->rxptr + rtail;

/* Helg: */
					if (tp->t_rawq.c_cc + size > DGB_IBUFSIZE ) {
						size = DGB_IBUFSIZE - tp->t_rawq.c_cc;
						DPRINT1(DB_RXDATA, "*");
						ibuf_full = TRUE;
					}

					if (size) {
						if (tp->t_state & TS_CAN_BYPASS_L_RINT) {
							DPRINT1(DB_RXDATA, "!");
							towin(sc, port->rxwin);
							tk_nin += size;
							tk_rawcc += size;
							tp->t_rawcc += size;
							b_to_q(ptr, size,
							    &tp->t_rawq);
							setwin(sc, 0);
						} else {
							int i = size;
							unsigned char chr;
							do {
								towin(sc, port->rxwin);
								chr = *ptr++;
								hidewin(sc);
							       (*linesw[tp->t_line].l_rint)(chr, tp);
							} while (--i > 0 );
							setwin(sc, 0);
						}
	 				}
					rtail= (rtail + size) & wrapmask;
					bc->rout = rtail;
					rhead = bc->rin & wrapmask;
					hidewin(sc);
					ttwakeup(tp);
					setwin(sc, 0);
				}
			end_of_data: ;
			}

			if (event & MODEMCHG_IND) {
				DPRINT3(DB_MODEM, "dgm%d: port%d: "
				    "MODEMCHG_IND\n", unit, pnum);
				port->imodem = mstat;
				if (mstat & port->dcd) {
					hidewin(sc);
					linesw[tp->t_line].l_modem(tp, 1);
					setwin(sc, 0);
					wakeup(TSA_CARR_ON(tp));
				} else {
					hidewin(sc);
					linesw[tp->t_line].l_modem(tp, 0);
					setwin(sc, 0);
					if (port->draining) {
						port->draining = 0;
						wakeup(&port->draining);
					}
				}
			}

			if (event & BREAK_IND) {
				if ((tp->t_state & TS_ISOPEN) && (tp->t_iflag & IGNBRK))	{
				        DPRINT3(DB_BREAK, "dgm%d: port%d:"
					    " BREAK_IND\n", unit, pnum);
				        hidewin(sc);
				        linesw[tp->t_line].l_rint(TTY_BI, tp);
				        setwin(sc, 0);
			        }
			}

/* Helg: with output flow control */

			if (event & (LOWTX_IND | EMPTYTX_IND) ) {
				DPRINT3(DB_TXDATA, "dgm%d: port%d:"
				    " LOWTX_IND or EMPTYTX_IND\n", unit, pnum);

				if ((event & EMPTYTX_IND ) &&
				    tp->t_outq.c_cc == 0 && port->draining) {
					port->draining = 0;
					wakeup(&port->draining);
					bc->ilow = 0;
					bc->iempty = 0;
				} else {

					int wrapmask = port->txbufsize - 1;

					for (obuf_full = FALSE;
					    tp->t_outq.c_cc != 0 && !obuf_full;
					    ) {
						int s;
						/* add "last-minute" data to write buffer */
						if (!(tp->t_state & TS_BUSY)) {
							hidewin(sc);
#ifndef TS_ASLEEP	/* post 2.0.5 FreeBSD */
					                ttwwakeup(tp);
#else
					                if (tp->t_outq.c_cc <= tp->t_lowat) {
						                if (tp->t_state & TS_ASLEEP) {
							                tp->t_state &= ~TS_ASLEEP;
							                wakeup(TSA_OLOWAT(tp));
						                }
						                /* selwakeup(&tp->t_wsel); */
					                }
#endif
					                setwin(sc, 0);
				                }
						s = spltty();

					whead = bc->tin & wrapmask;
					wtail = bc->tout & wrapmask;

					if (whead < wtail)
						size = wtail - whead - 1;
					else {
						size = port->txbufsize - whead;
						if (wtail == 0)
							size--;
					}

					if (size == 0) {
						DPRINT5(DB_WR, "dgm: head = %d tail = %d size = %d full = %d\n",
							whead, wtail, size, obuf_full);
						bc->iempty = 1;
						bc->ilow = 1;
						obuf_full = TRUE;
						splx(s);
						break;
					}

					towin(sc, port->txwin);

					ocount = q_to_b(&tp->t_outq, port->txptr + whead, size);
					whead += ocount;

					setwin(sc, 0);
					bc->tin = whead;
					bc->tin = whead & wrapmask;
					splx(s);
				}

				if (obuf_full) {
					DPRINT1(DB_WR, " +BUSY\n");
					tp->t_state |= TS_BUSY;
				} else {
					DPRINT1(DB_WR, " -BUSY\n");
					hidewin(sc);
#ifndef TS_ASLEEP	/* post 2.0.5 FreeBSD */
					/* should clear TS_BUSY before ttwwakeup */
					if (tp->t_state & TS_BUSY)	{
						tp->t_state &= ~TS_BUSY;
						linesw[tp->t_line].l_start(tp);
				                ttwwakeup(tp);
					}
#else
				if (tp->t_state & TS_ASLEEP) {
					tp->t_state &= ~TS_ASLEEP;
					wakeup(TSA_OLOWAT(tp));
				}
				tp->t_state &= ~TS_BUSY;
#endif
				        setwin(sc, 0);
				        }
			        }
			}
			bc->idata = 1;   /* require event on incoming data */

		} else {
			bc = port->brdchan;
			DPRINT4(DB_EXCEPT, "dgm%d: port%d: got event 0x%x on closed port\n",
				unit, pnum, event);
			bc->rout = bc->rin;
			bc->idata = bc->iempty = bc->ilow = 0;
		}

		tail = (tail + 4) & (FEP_IMAX - FEP_ISTART - 4);
	}

	sc->mailbox->eout = tail;
	bmws_set(ws);

	sc->toh = timeout(dgmpoll, unit_c, hz / POLLSPERSEC);

	DPRINT2(DB_INFO, "dgm%d: poll done\n", sc->unit);
}

static int
dgmioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct dgm_softc *sc;
	int unit, pnum;
	struct dgm_p *port;
	int mynor;
	struct tty *tp;
	volatile struct board_chan *bc;
	int error;
	int s, cs;
	int tiocm_xxx;

#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
	u_long		oldcmd;
	struct termios	term;
#endif

	BoardMemWinState ws = bmws_get();

	mynor = minor(dev);
	unit = MINOR_TO_UNIT(mynor);
	pnum = MINOR_TO_PORT(mynor);

	sc = devclass_get_softc(dgmdevclass, unit);
	port = &sc->ports[pnum];
	tp = &sc->ttys[pnum];
	bc = port->brdchan;

	if (mynor & CONTROL_MASK) {
		struct termios *ct;

		switch (mynor & CONTROL_MASK) {
		case CONTROL_INIT_STATE:
			ct = mynor & CALLOUT_MASK ? &port->it_out : &port->it_in;
			break;
		case CONTROL_LOCK_STATE:
			ct = mynor & CALLOUT_MASK ? &port->lt_out : &port->lt_in;
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

#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
	term = tp->t_termios;
	if (cmd == TIOCSETA || cmd == TIOCSETAW || cmd == TIOCSETAF) {
	  DPRINT6(DB_PARAM, "dgm%d: port%d: dgmioctl-ISNOW c = 0x%x i = 0x%x l = 0x%x\n", unit, pnum, term.c_cflag, term.c_iflag, term.c_lflag);
	}
	oldcmd = cmd;
	error = ttsetcompat(tp, &cmd, data, &term);
	if (error != 0)
		return (error);
	if (cmd != oldcmd)
		data = (caddr_t)&term;
#endif

	if (cmd == TIOCSETA || cmd == TIOCSETAW || cmd == TIOCSETAF) {
		int	cc;
		struct termios *dt = (struct termios *)data;
		struct termios *lt = mynor & CALLOUT_MASK
				     ? &port->lt_out : &port->lt_in;

		DPRINT6(DB_PARAM, "dgm%d: port%d: dgmioctl-TOSET c = 0x%x i = 0x%x l = 0x%x\n", unit, pnum, dt->c_cflag, dt->c_iflag, dt->c_lflag);
		dt->c_iflag = (tp->t_iflag & lt->c_iflag)
			      | (dt->c_iflag & ~lt->c_iflag);
		dt->c_oflag = (tp->t_oflag & lt->c_oflag)
			      | (dt->c_oflag & ~lt->c_oflag);
		dt->c_cflag = (tp->t_cflag & lt->c_cflag)
			      | (dt->c_cflag & ~lt->c_cflag);
		dt->c_lflag = (tp->t_lflag & lt->c_lflag)
			      | (dt->c_lflag & ~lt->c_lflag);
		for (cc = 0; cc < NCCS; ++cc)
			if (lt->c_cc[cc] != 0)
				dt->c_cc[cc] = tp->t_cc[cc];
		if (lt->c_ispeed != 0)
			dt->c_ispeed = tp->t_ispeed;
		if (lt->c_ospeed != 0)
			dt->c_ospeed = tp->t_ospeed;
	}

	if (cmd == TIOCSTOP) {
		cs = splclock();
		setwin(sc, 0);
		fepcmd(port, PAUSETX, 0, 0, 0, 0);
		bmws_set(ws);
		splx(cs);
		return 0;
	} else if (cmd == TIOCSTART) {
		cs = splclock();
		setwin(sc, 0);
		fepcmd(port, RESUMETX, 0, 0, 0, 0);
		bmws_set(ws);
		splx(cs);
		return 0;
	}

	if (cmd == TIOCSETAW || cmd == TIOCSETAF)
		port->mustdrain = 1;

	error = linesw[tp->t_line].l_ioctl(tp, cmd, data, flag, p);
	if (error != ENOIOCTL)
		return error;
	s = spltty();
	error = ttioctl(tp, cmd, data, flag);
	disc_optim(tp, &tp->t_termios);
	port->mustdrain = 0;
	if (error != ENOIOCTL) {
		splx(s);
		if (cmd == TIOCSETA || cmd == TIOCSETAW || cmd == TIOCSETAF) {
			DPRINT6(DB_PARAM, "dgm%d: port%d: dgmioctl-RES c = 0x%x i = 0x%x l = 0x%x\n", unit, pnum, tp->t_cflag, tp->t_iflag, tp->t_lflag);
		}
		return error;
	}

	switch (cmd) {
	case TIOCSBRK:
#if 0
		error = dgmdrain(port);

		if (error != 0) {
			splx(s);
			return error;
		}
#endif

		cs = splclock();
		setwin(sc, 0);

		/* now it sends 400 millisecond break because I don't know */
		/* how to send an infinite break */

		fepcmd(port, SENDBREAK, 400, 0, 10, 0);
		hidewin(sc);
		splx(cs);
		break;
	case TIOCCBRK:
		/* now it's empty */
		break;
	case TIOCSDTR:
		DPRINT3(DB_MODEM, "dgm%d: port%d: set DTR\n", unit, pnum);
		port->omodem |= DTR;
		cs = splclock();
		setwin(sc, 0);
		fepcmd(port, SETMODEM, port->omodem, RTS, 0, 1);

		if (!(bc->mstat & DTR))
			DPRINT3(DB_MODEM, "dgm%d: port%d: DTR is off\n", unit, pnum);

		hidewin(sc);
		splx(cs);
		break;
	case TIOCCDTR:
		DPRINT3(DB_MODEM, "dgm%d: port%d: reset DTR\n", unit, pnum);
		port->omodem &= ~DTR;
		cs = splclock();
		setwin(sc, 0);
		fepcmd(port, SETMODEM, port->omodem, RTS|DTR, 0, 1);

		if (bc->mstat & DTR) {
			DPRINT3(DB_MODEM, "dgm%d: port%d: DTR is on\n", unit, pnum);
		}

		hidewin(sc);
		splx(cs);
		break;
	case TIOCMSET:
		if (*(int *)data & TIOCM_DTR)
			port->omodem |= DTR;
		else
			port->omodem &= ~DTR;

		if (*(int *)data & TIOCM_RTS)
			port->omodem |= RTS;
		else
			port->omodem &= ~RTS;

		cs = splclock();
		setwin(sc, 0);
		fepcmd(port, SETMODEM, port->omodem, RTS|DTR, 0, 1);
		hidewin(sc);
		splx(cs);
		break;
	case TIOCMBIS:
		if (*(int *)data & TIOCM_DTR)
			port->omodem |= DTR;

		if (*(int *)data & TIOCM_RTS)
			port->omodem |= RTS;

		cs = splclock();
		setwin(sc, 0);
		fepcmd(port, SETMODEM, port->omodem, RTS|DTR, 0, 1);
		hidewin(sc);
		splx(cs);
		break;
	case TIOCMBIC:
		if (*(int *)data & TIOCM_DTR)
			port->omodem &= ~DTR;

		if (*(int *)data & TIOCM_RTS)
			port->omodem &= ~RTS;

		cs = splclock();
		setwin(sc, 0);
		fepcmd(port, SETMODEM, port->omodem, RTS|DTR, 0, 1);
		hidewin(sc);
		splx(cs);
		break;
	case TIOCMGET:
		setwin(sc, 0);
		port->imodem = bc->mstat;
		hidewin(sc);

		tiocm_xxx = TIOCM_LE;	/* XXX - always enabled while open */

		DPRINT3(DB_MODEM, "dgm%d: port%d: modem stat -- ", unit, pnum);

		if (port->imodem & DTR) {
			DPRINT1(DB_MODEM, "DTR ");
			tiocm_xxx |= TIOCM_DTR;
		}
		if (port->imodem & RTS) {
			DPRINT1(DB_MODEM, "RTS ");
			tiocm_xxx |= TIOCM_RTS;
		}
		if (port->imodem & CTS) {
			DPRINT1(DB_MODEM, "CTS ");
			tiocm_xxx |= TIOCM_CTS;
		}
		if (port->imodem & port->dcd) {
			DPRINT1(DB_MODEM, "DCD ");
			tiocm_xxx |= TIOCM_CD;
		}
		if (port->imodem & port->dsr) {
			DPRINT1(DB_MODEM, "DSR ");
			tiocm_xxx |= TIOCM_DSR;
		}
		if (port->imodem & RI) {
			DPRINT1(DB_MODEM, "RI ");
			tiocm_xxx |= TIOCM_RI;
		}
		*(int *)data = tiocm_xxx;
		DPRINT1(DB_MODEM, "--\n");
		break;
	case TIOCMSDTRWAIT:
		/* must be root since the wait applies to following logins */
		error = suser(p);
		if (error != 0) {
			splx(s);
			return (error);
		}
		port->close_delay = *(int *)data * hz / 100;
		break;
	case TIOCMGDTRWAIT:
		*(int *)data = port->close_delay * 100 / hz;
		break;
	case TIOCTIMESTAMP:
		port->do_timestamp = 1;
		*(struct timeval *)data = port->timestamp;
		break;
	case TIOCDCDTIMESTAMP:
		port->do_dcd_timestamp = 1;
		*(struct timeval *)data = port->dcd_timestamp;
		break;
	default:
		bmws_set(ws);
		splx(s);
		return ENOTTY;
	}
	bmws_set(ws);
	splx(s);

	return 0;
}

static void
wakeflush(void *p)
{
	struct dgm_p *port = p;

	wakeup(&port->draining);
}

/* wait for the output to drain */

static int
dgmdrain(struct dgm_p *port)
{
	volatile struct board_chan *bc = port->brdchan;
	struct dgm_softc *sc;
	int error;
	int head, tail;
	BoardMemWinState ws = bmws_get();

	sc = devclass_get_softc(dgmdevclass, port->sc->unit);

	setwin(sc, 0);

	bc->iempty = 1;
	tail = bc->tout;
	head = bc->tin;

	while (tail != head) {
		DPRINT5(DB_WR, "dgm%d: port%d: drain: head = %d tail = %d\n",
			port->sc->unit, port->pnum, head, tail);

		hidewin(sc);
		port->draining = 1;
		timeout(wakeflush, port, hz);
		error = tsleep(&port->draining, TTIPRI | PCATCH, "dgdrn", 0);
		port->draining = 0;
		setwin(sc, 0);

		if (error != 0) {
			DPRINT4(DB_WR, "dgm%d: port%d: tsleep(dgdrn) error = %d\n",
				port->sc->unit, port->pnum, error);

			bc->iempty = 0;
			bmws_set(ws);
			return error;
		}

		tail = bc->tout;
		head = bc->tin;
	}
	DPRINT5(DB_WR, "dgm%d: port%d: drain: head = %d tail = %d\n",
		port->sc->unit, port->pnum, head, tail);
	bmws_set(ws);
	return 0;
}

/* wait for the output to drain */
/* or simply clear the buffer it it's stopped */

static void
dgm_drain_or_flush(struct dgm_p *port)
{
	volatile struct board_chan *bc = port->brdchan;
	struct tty *tp = port->tty;
	struct dgm_softc *sc;
	int error;
	int lasttail;
	int head, tail;

	sc = devclass_get_softc(dgmdevclass, port->sc->unit);
	setwin(sc, 0);

	lasttail = -1;
	bc->iempty = 1;
	tail = bc->tout;
	head = bc->tin;

	while (tail != head /* && tail != lasttail */ ) {
		DPRINT5(DB_WR, "dgm%d: port%d: flush: head = %d tail = %d\n",
			port->sc->unit, port->pnum, head, tail);

		/* if there is no carrier simply clean the buffer */
		if (!(tp->t_state & TS_CARR_ON)) {
			bc->tout = bc->tin = 0;
			bc->iempty = 0;
			hidewin(sc);
			return;
		}

		hidewin(sc);
		port->draining = 1;
		timeout(wakeflush, port, hz);
		error = tsleep(&port->draining, TTIPRI | PCATCH, "dgfls", 0);
		port->draining = 0;
		setwin(sc, 0);

		if (error != 0) {
			DPRINT4(DB_WR, "dgm%d: port%d: tsleep(dgfls)"
			    " error = %d\n", port->sc->unit, port->pnum, error);

			/* silently clean the buffer */

			bc->tout = bc->tin = 0;
			bc->iempty = 0;
			hidewin(sc);
			return;
		}

		lasttail = tail;
		tail = bc->tout;
		head = bc->tin;
	}
	hidewin(sc);
	DPRINT5(DB_WR, "dgm%d: port%d: flush: head = %d tail = %d\n",
			port->sc->unit, port->pnum, head, tail);
}

static int
dgmparam(struct tty *tp, struct termios *t)
{
	int unit = MINOR_TO_UNIT(minor(tp->t_dev));
	int pnum = MINOR_TO_PORT(minor(tp->t_dev));
	volatile struct board_chan *bc;
	struct dgm_softc *sc;
	struct dgm_p *port;
	int cflag;
	int head;
	int mval;
	int iflag;
	int hflow;
	int cs;
	BoardMemWinState ws = bmws_get();

	sc = devclass_get_softc(dgmdevclass, unit);
	port = &sc->ports[pnum];
	bc = port->brdchan;

	DPRINT6(DB_PARAM, "dgm%d: port%d: dgmparm c = 0x%x i = 0x%x l = 0x%x\n", unit, pnum, t->c_cflag, t->c_iflag, t->c_lflag);

	if (port->mustdrain) {
		DPRINT3(DB_PARAM, "dgm%d: port%d: must call dgmdrain()\n", unit, pnum);
		dgmdrain(port);
	}

	cflag = ttspeedtab(t->c_ospeed, dgmspeedtab);

	if (t->c_ispeed == 0)
		t->c_ispeed = t->c_ospeed;

	if (cflag < 0 /* || cflag > 0 && t->c_ispeed != t->c_ospeed */) {
		DPRINT4(DB_PARAM, "dgm%d: port%d: invalid cflag = 0%o\n", unit, pnum, cflag);
		return (EINVAL);
	}

	cs = splclock();
	setwin(sc, 0);

	if (cflag == 0) { /* hangup */
		DPRINT3(DB_PARAM, "dgm%d: port%d: hangup\n", unit, pnum);
		head = bc->rin;
		bc->rout = head;
		head = bc->tin;
		fepcmd(port, STOUT, (unsigned)head, 0, 0, 0);
		mval= port->omodem & ~(DTR|RTS);
	} else {
		cflag |= dgmflags(dgm_cflags, t->c_cflag);

		if (cflag != port->fepcflag) {
			port->fepcflag = cflag;
			DPRINT5(DB_PARAM, "dgm%d: port%d: set cflag = 0x%x c = 0x%x\n",
					unit, pnum, cflag, t->c_cflag&~CRTSCTS);
			fepcmd(port, SETCTRLFLAGS, (unsigned)cflag, 0, 0, 0);
		}
		mval= port->omodem | (DTR|RTS);
	}

	iflag = dgmflags(dgm_iflags, t->c_iflag);
	if (iflag != port->fepiflag) {
		port->fepiflag = iflag;
		DPRINT5(DB_PARAM, "dgm%d: port%d: set iflag = 0x%x c = 0x%x\n", unit, pnum, iflag, t->c_iflag);
		fepcmd(port, SETIFLAGS, (unsigned)iflag, 0, 0, 0);
	}

	bc->mint = port->dcd;

	hflow = dgmflags(dgm_flow, t->c_cflag);
	if (hflow != port->hflow) {
		port->hflow = hflow;
		DPRINT5(DB_PARAM, "dgm%d: port%d: set hflow = 0x%x f = 0x%x\n", unit, pnum, hflow, t->c_cflag&CRTSCTS);
		fepcmd(port, SETHFLOW, (unsigned)hflow, 0xff, 0, 1);
	}

	if (port->omodem != mval) {
		DPRINT5(DB_PARAM, "dgm%d: port%d: setting modem parameters 0x%x was 0x%x\n",
			unit, pnum, mval, port->omodem);
		port->omodem = mval;
		fepcmd(port, SETMODEM, (unsigned)mval, RTS|DTR, 0, 1);
	}

	if (port->fepstartc != t->c_cc[VSTART] ||
	    port->fepstopc != t->c_cc[VSTOP]) {
		DPRINT5(DB_PARAM, "dgm%d: port%d: set startc = %d, stopc = %d\n", unit, pnum, t->c_cc[VSTART], t->c_cc[VSTOP]);
		port->fepstartc = t->c_cc[VSTART];
		port->fepstopc = t->c_cc[VSTOP];
		fepcmd(port, SONOFFC, port->fepstartc, port->fepstopc, 0, 1);
	}

	bmws_set(ws);
	splx(cs);

	return 0;

}

static void
dgmstart(struct tty *tp)
{
	int unit;
	int pnum;
	struct dgm_p *port;
	struct dgm_softc *sc;
	volatile struct board_chan *bc;
	int head, tail;
	int size, ocount;
	int s;
	int wmask;

	BoardMemWinState ws = bmws_get();

	unit = MINOR_TO_UNIT(minor(tp->t_dev));
	pnum = MINOR_TO_PORT(minor(tp->t_dev));
	sc = devclass_get_softc(dgmdevclass, unit);
	port = &sc->ports[pnum];
	bc = port->brdchan;

	wmask = port->txbufsize - 1;

	s = spltty();

	while (tp->t_outq.c_cc != 0) {
		int cs;
#ifndef TS_ASLEEP	/* post 2.0.5 FreeBSD */
		ttwwakeup(tp);
#else
		if (tp->t_outq.c_cc <= tp->t_lowat) {
			if (tp->t_state & TS_ASLEEP) {
				tp->t_state &= ~TS_ASLEEP;
				wakeup(TSA_OLOWAT(tp));
			}
			/*selwakeup(&tp->t_wsel);*/
		}
#endif
		cs = splclock();
		setwin(sc, 0);

		head = bc->tin & wmask;

		do { tail = bc->tout; } while (tail != bc->tout);
		tail = bc->tout & wmask;

		DPRINT5(DB_WR, "dgm%d: port%d: s tx head = %d tail = %d\n", unit, pnum, head, tail);

#ifdef LEAVE_FREE_CHARS
		if (tail > head) {
			size = tail - head - LEAVE_FREE_CHARS;
			if (size < 0)
			        size = 0;
		        else {
			        size = port->txbufsize - head;
			        if (tail + port->txbufsize < head)
				        size = 0;
		        }
		}
#else
		if (tail > head)
			size = tail - head - 1;
		else {
			size = port->txbufsize - head;
			if (tail == 0)
				size--;
		}
#endif

		if (size == 0) {
			bc->iempty = 1;
			bc->ilow = 1;
			splx(cs);
			bmws_set(ws);
			tp->t_state |= TS_BUSY;
			splx(s);
			return;
		}

		towin(sc, port->txwin);

		ocount = q_to_b(&tp->t_outq, port->txptr + head, size);
		head += ocount;
		if (head >= port->txbufsize)
			head -= port->txbufsize;

		setwin(sc, 0);
		bc->tin = head;

		DPRINT5(DB_WR, "dgm%d: port%d: tx avail = %d count = %d\n",
		    unit, pnum, size, ocount);
		hidewin(sc);
		splx(cs);
	}

	bmws_set(ws);
	splx(s);

#ifndef TS_ASLEEP	/* post 2.0.5 FreeBSD */
	if (tp->t_state & TS_BUSY) {
		tp->t_state &= ~TS_BUSY;
		linesw[tp->t_line].l_start(tp);
		ttwwakeup(tp);
	}
#else
	if (tp->t_state & TS_ASLEEP) {
		tp->t_state &= ~TS_ASLEEP;
		wakeup(TSA_OLOWAT(tp));
	}
	tp->t_state& = ~TS_BUSY;
#endif
}

void
dgmstop(struct tty *tp, int rw)
{
	int unit;
	int pnum;
	struct dgm_p *port;
	struct dgm_softc *sc;
	volatile struct board_chan *bc;
	int s;

	BoardMemWinState ws = bmws_get();

	unit = MINOR_TO_UNIT(minor(tp->t_dev));
	pnum = MINOR_TO_PORT(minor(tp->t_dev));

	sc = devclass_get_softc(dgmdevclass, unit);
	port = &sc->ports[pnum];
	bc = port->brdchan;

	DPRINT3(DB_WR, "dgm%d: port%d: stop\n", port->sc->unit, port->pnum);

	s = spltty();
	setwin(sc, 0);

	if (rw & FWRITE) {
		/* clear output queue */
		bc->tout = bc->tin = 0;
		bc->ilow = 0;
		bc->iempty = 0;
	}
	if (rw & FREAD) {
		/* clear input queue */
		bc->rout = bc->rin;
		bc->idata = 1;
	}
	hidewin(sc);
	bmws_set(ws);
	splx(s);
	dgmstart(tp);
}

static void
fepcmd(struct dgm_p *port,
	unsigned cmd,
	unsigned op1,
	unsigned op2,
	unsigned ncmds,
	unsigned bytecmd)
{
	u_char *mem;
	unsigned tail, head;
	int count, n;

	KASSERT(port->sc, ("Couldn't (re)obtain driver softc"));
	mem = port->sc->vmem;

	if (!port->enabled) {
		printf("dgm%d: port%d: FEP command on disabled port\n",
			port->sc->unit, port->pnum);
		return;
	}

	/* setwin(port->sc, 0); Require this to be set by caller */
	head = port->sc->mailbox->cin;

	if (head >= FEP_CMAX - FEP_CSTART || (head & 3)) {
		printf("dgm%d: port%d: wrong pointer head of command queue : 0x%x\n",
			port->sc->unit, port->pnum, head);
		return;
	}

	mem[head + FEP_CSTART] = cmd;
	mem[head + FEP_CSTART + 1] = port->pnum;
	if (bytecmd) {
		mem[head + FEP_CSTART + 2] = op1;
		mem[head + FEP_CSTART + 3] = op2;
	} else {
		mem[head + FEP_CSTART + 2] = op1 & 0xff;
		mem[head + FEP_CSTART + 3] = (op1 >> 8) & 0xff;
	}

	DPRINT7(DB_FEP, "dgm%d: port%d: %s cmd = 0x%x op1 = 0x%x op2 = 0x%x\n", port->sc->unit, port->pnum,
			(bytecmd)?"byte":"word", cmd, mem[head + FEP_CSTART + 2], mem[head + FEP_CSTART + 3]);

	head = (head + 4) & (FEP_CMAX - FEP_CSTART - 4);
	port->sc->mailbox->cin = head;

	count = FEPTIMEOUT;

	while (count-- != 0) {
		head = port->sc->mailbox->cin;
		tail = port->sc->mailbox->cout;

		n = (head - tail) & (FEP_CMAX - FEP_CSTART - 4);
		if (n <= ncmds * (sizeof(ushort)*4))
			return;
	}
	printf("dgm%d(%d): timeout on FEP cmd = 0x%x\n", port->sc->unit, port->pnum, cmd);
}

static void
disc_optim(struct tty *tp, struct termios *t)
{
	if (!(t->c_iflag & (ICRNL | IGNCR | IMAXBEL | INLCR | ISTRIP | IXON))
	    && (!(t->c_iflag & BRKINT) || (t->c_iflag & IGNBRK))
	    && (!(t->c_iflag & PARMRK)
		|| (t->c_iflag & (IGNPAR | IGNBRK)) == (IGNPAR | IGNBRK))
	    && !(t->c_lflag & (ECHO | ICANON | IEXTEN | ISIG | PENDIN))
	    && linesw[tp->t_line].l_rint == ttyinput)
		tp->t_state |= TS_CAN_BYPASS_L_RINT;
	else
		tp->t_state &= ~TS_CAN_BYPASS_L_RINT;
}
