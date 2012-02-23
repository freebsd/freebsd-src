/*-
 * Device driver for Specialix range (SI/XIO) of serial line multiplexors.
 *
 * Copyright (C) 1990, 1992, 1998 Specialix International,
 * Copyright (C) 1993, Andy Rutter <andy@acronym.co.uk>
 * Copyright (C) 2000, Peter Wemm <peter@netplex.com.au>
 *
 * Originally derived from:	SunOS 4.x version
 * Ported from BSDI version to FreeBSD by Peter Wemm.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notices, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notices, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Andy Rutter of
 *	Advanced Methods and Tools Ltd. based on original information
 *	from Specialix International.
 * 4. Neither the name of Advanced Methods and Tools, nor Specialix
 *    International may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL THE AUTHORS BE LIABLE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifndef lint
static const char si_copyright1[] =  "@(#) Copyright (C) Specialix International, 1990,1992,1998",
		  si_copyright2[] =  "@(#) Copyright (C) Andy Rutter 1993",
		  si_copyright3[] =  "@(#) Copyright (C) Peter Wemm 2000";
#endif	/* not lint */

#include "opt_compat.h"
#include "opt_debug_si.h"
#include "opt_eisa.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/serial.h>
#include <sys/tty.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/priv.h>
#include <sys/sysctl.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>


#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/stdarg.h>

#include <dev/si/sireg.h>
#include <dev/si/sivar.h>
#include <dev/si/si.h>

/*
 * This device driver is designed to interface the Specialix International
 * SI, XIO and SX range of serial multiplexor cards to FreeBSD on an ISA,
 * EISA or PCI bus machine.
 *
 * The controller is interfaced to the host via dual port RAM
 * and an interrupt.
 *
 * The code for the Host 1 (very old ISA cards) has not been tested.
 */

#undef	POLL		/* turn on poller to scan for lost interrupts */
#if 0
#define REALPOLL	/* on each poll, scan for work regardless */
#endif
#define POLLHZ	(hz/10)	/* 10 times per second */
#define SI_I_HIGH_WATER	(TTYHOG - 2 * SI_BUFFERSIZE)
#define INT_COUNT 25000		/* max of 125 ints per second */
#define JET_INT_COUNT 100	/* max of 100 ints per second */
#define RXINT_COUNT 1	/* one rxint per 10 milliseconds */

static void si_command(struct si_port *, int, int);
static int si_Sioctl(struct cdev *, u_long, caddr_t, int, struct thread *);
/* static void si_stop(struct tty *, int); */
#if 0
static timeout_t si_lstart;
#endif

static tsw_outwakeup_t si_start;
static tsw_ioctl_t siioctl;
static tsw_close_t siclose;
static tsw_modem_t simodem;
static tsw_open_t siopen;
static tsw_param_t siparam;

static void	si_modem_state(struct si_port *pp, struct tty *tp, int hi_ip);
static char *	si_modulename(int host_type, int uart_type);

static struct cdevsw si_Scdevsw = {
	.d_version =	D_VERSION,
	.d_ioctl =	si_Sioctl,
	.d_name =	"si",
	.d_flags =	D_TTY | D_NEEDGIANT,
};

static int si_Nports;
static int si_Nmodules;
static int si_debug;

SYSCTL_INT(_machdep, OID_AUTO, si_debug, CTLFLAG_RW, &si_debug, 0, "");
TUNABLE_INT("machdep.si_debug", &si_debug);

static int si_numunits;

devclass_t si_devclass;

struct si_speedtab {
	int sp_speed;			/* Speed. */
	int sp_code;			/* Code. */
};

#ifndef B2000	/* not standard, but the hardware knows it. */
# define B2000 2000
#endif
static struct si_speedtab bdrates[] = {
	{ B75,		CLK75, },	/* 0x0 */
	{ B110,		CLK110, },	/* 0x1 */
	{ B150,		CLK150, },	/* 0x3 */
	{ B300,		CLK300, },	/* 0x4 */
	{ B600,		CLK600, },	/* 0x5 */
	{ B1200,	CLK1200, },	/* 0x6 */
	{ B2000,	CLK2000, },	/* 0x7 */
	{ B2400,	CLK2400, },	/* 0x8 */
	{ B4800,	CLK4800, },	/* 0x9 */
	{ B9600,	CLK9600, },	/* 0xb */
	{ B19200,	CLK19200, },	/* 0xc */
	{ B38400,	CLK38400, },	/* 0x2 (out of order!) */
	{ B57600,	CLK57600, },	/* 0xd */
	{ B115200,	CLK110, },	/* 0x1 (dupe!, 110 baud on "si") */
	{ -1,		-1 },
};


#ifdef POLL
static int si_pollrate;			/* in addition to irq */
static int si_realpoll = 0;		/* poll HW on timer */

SYSCTL_INT(_machdep, OID_AUTO, si_pollrate, CTLFLAG_RW, &si_pollrate, 0, "");
SYSCTL_INT(_machdep, OID_AUTO, si_realpoll, CTLFLAG_RW, &si_realpoll, 0, "");

static int init_finished = 0;
static void si_poll(void *);
#endif

/*
 * Array of adapter types and the corresponding RAM size. The order of
 * entries here MUST match the ordinal of the adapter type.
 */
static const char *si_type[] = {
	"EMPTY",
	"SIHOST",
	"SIMCA",		/* FreeBSD does not support Microchannel */
	"SIHOST2",
	"SIEISA",
	"SIPCI",
	"SXPCI",
	"SXISA",
};

#ifdef SI_DEBUG
static char *
si_cmdname(int cmd)
{
	static char buf[32];

	switch (cmd) {
	case IDLE_OPEN:		return("IDLE_OPEN");
	case LOPEN:		return("LOPEN");
	case MOPEN:		return("MOPEN");
	case MPEND:		return("MPEND");
	case CONFIG:		return("CONFIG");
	case CLOSE:		return("CLOSE");
	case SBREAK:		return("SBREAK");
	case EBREAK:		return("EBREAK");
	case IDLE_CLOSE:	return("IDLE_CLOSE");
	case IDLE_BREAK:	return("IDLE_BREAK");
	case FCLOSE:		return("FCLOSE");
	case RESUME:		return("RESUME");
	case WFLUSH:		return("WFLUSH");
	case RFLUSH:		return("RFLUSH");
	default:
		sprintf(buf, "?cmd:0x%x?", cmd);
		return (buf);
	}
}
#endif

/*
 * We have to make an 8 bit version of bcopy, since some cards can't
 * deal with 32 bit I/O
 */
static void __inline
si_bcopy(const void *src, void *dst, size_t len)
{
	u_char *d;
	const u_char *s;

	d = dst;
	s = src;
	while (len--)
		*d++ = *s++;
}
static void __inline
si_vbcopy(const volatile void *src, void *dst, size_t len)
{
	u_char *d;
	const volatile u_char *s;

	d = dst;
	s = src;
	while (len--)
		*d++ = *s++;
}
static void __inline
si_bcopyv(const void *src, volatile void *dst, size_t len)
{
	volatile u_char *d;
	const u_char *s;

	d = dst;
	s = src;
	while (len--)
		*d++ = *s++;
}

static int
si_speedtab(int speed, struct si_speedtab *table)
{
	for ( ; table->sp_speed != -1; table++)
		if (table->sp_speed == speed)
			return (table->sp_code);
	return (-1);
}


static struct ttydevsw si_tty_class = {
	.tsw_flags	= TF_INITLOCK|TF_CALLOUT,
	.tsw_open	= siopen,
	.tsw_close	= siclose,
	.tsw_outwakeup	= si_start,
	/* .tsw_stop = si_stop */
	.tsw_ioctl	= siioctl,
	.tsw_param	= siparam,
	.tsw_modem	= simodem,
};


/*
 * Attach the device.  Initialize the card.
 */
int
siattach(device_t dev)
{
	int unit;
	struct si_softc *sc;
	struct si_port *pp;
	struct tty *tp;
	volatile struct si_channel *ccbp;
	volatile struct si_reg *regp;
	volatile caddr_t maddr;
	struct si_module *modp;
	int nmodule, nport, x, y;
	int uart_type;

	sc = device_get_softc(dev);
	unit = device_get_unit(dev);

	sc->sc_typename = si_type[sc->sc_type];
	if (si_numunits < unit + 1)
		si_numunits = unit + 1;

	DPRINT((0, DBG_AUTOBOOT, "si%d: siattach\n", unit));

#ifdef POLL
	if (si_pollrate == 0) {
		si_pollrate = POLLHZ;		/* in addition to irq */
#ifdef REALPOLL
		si_realpoll = 1;		/* scan always */
#endif
	}
#endif

	DPRINT((0, DBG_AUTOBOOT, "si%d: type: %s paddr: %x maddr: %x\n", unit,
		sc->sc_typename, sc->sc_paddr, sc->sc_maddr));

	sc->sc_ports = NULL;			/* mark as uninitialised */

	maddr = sc->sc_maddr;

	/* Stop the CPU first so it won't stomp around while we load */

	switch (sc->sc_type) {
#ifdef DEV_EISA
		case SIEISA:
			outb(sc->sc_iobase + 2, sc->sc_irq << 4);
#endif
		break;
		case SIPCI:
			*(maddr+SIPCIRESET) = 0;
		break;
		case SIJETPCI: /* fall through to JET ISA */
		case SIJETISA:
			*(maddr+SIJETCONFIG) = 0;
		break;
		case SIHOST2:
			*(maddr+SIPLRESET) = 0;
		break;
		case SIHOST:
			*(maddr+SIRESET) = 0;
		break;
		default: /* this should never happen */
			printf("si%d: unsupported configuration\n", unit);
			return EINVAL;
		break;
	}

	/* OK, now lets download the download code */

	if (SI_ISJET(sc->sc_type)) {
		DPRINT((0, DBG_DOWNLOAD, "si%d: jet_download: nbytes %d\n",
			unit, si3_t225_dsize));
		si_bcopy(si3_t225_download, maddr + si3_t225_downloadaddr,
			si3_t225_dsize);
		DPRINT((0, DBG_DOWNLOAD,
			"si%d: jet_bootstrap: nbytes %d -> %x\n",
			unit, si3_t225_bsize, si3_t225_bootloadaddr));
		si_bcopy(si3_t225_bootstrap, maddr + si3_t225_bootloadaddr,
			si3_t225_bsize);
	} else {
		DPRINT((0, DBG_DOWNLOAD, "si%d: si_download: nbytes %d\n",
			unit, si2_z280_dsize));
		si_bcopy(si2_z280_download, maddr + si2_z280_downloadaddr,
			si2_z280_dsize);
	}

	/* Now start the CPU */

	switch (sc->sc_type) {
#ifdef DEV_EISA
	case SIEISA:
		/* modify the download code to tell it that it's on an EISA */
		*(maddr + 0x42) = 1;
		outb(sc->sc_iobase + 2, (sc->sc_irq << 4) | 4);
		(void)inb(sc->sc_iobase + 3); /* reset interrupt */
		break;
#endif
	case SIPCI:
		/* modify the download code to tell it that it's on a PCI */
		*(maddr+0x42) = 1;
		*(maddr+SIPCIRESET) = 1;
		*(maddr+SIPCIINTCL) = 0;
		break;
	case SIJETPCI:
		*(maddr+SIJETRESET) = 0;
		*(maddr+SIJETCONFIG) = SIJETBUSEN|SIJETIRQEN;
		break;
	case SIJETISA:
		*(maddr+SIJETRESET) = 0;
		switch (sc->sc_irq) {
		case 9:
			*(maddr+SIJETCONFIG) = SIJETBUSEN|SIJETIRQEN|0x90;
			break;
		case 10:
			*(maddr+SIJETCONFIG) = SIJETBUSEN|SIJETIRQEN|0xa0;
			break;
		case 11:
			*(maddr+SIJETCONFIG) = SIJETBUSEN|SIJETIRQEN|0xb0;
			break;
		case 12:
			*(maddr+SIJETCONFIG) = SIJETBUSEN|SIJETIRQEN|0xc0;
			break;
		case 15:
			*(maddr+SIJETCONFIG) = SIJETBUSEN|SIJETIRQEN|0xf0;
			break;
		}
		break;
	case SIHOST:
		*(maddr+SIRESET_CL) = 0;
		*(maddr+SIINTCL_CL) = 0;
		break;
	case SIHOST2:
		*(maddr+SIPLRESET) = 0x10;
		switch (sc->sc_irq) {
		case 11:
			*(maddr+SIPLIRQ11) = 0x10;
			break;
		case 12:
			*(maddr+SIPLIRQ12) = 0x10;
			break;
		case 15:
			*(maddr+SIPLIRQ15) = 0x10;
			break;
		}
		*(maddr+SIPLIRQCLR) = 0x10;
		break;
	default: /* this should _REALLY_ never happen */
		printf("si%d: Uh, it was supported a second ago...\n", unit);
		return EINVAL;
	}

	DELAY(1000000);			/* wait around for a second */

	regp = (struct si_reg *)maddr;
	y = 0;
					/* wait max of 5 sec for init OK */
	while (regp->initstat == 0 && y++ < 10) {
		DELAY(500000);
	}
	switch (regp->initstat) {
	case 0:
		printf("si%d: startup timeout - aborting\n", unit);
		sc->sc_type = SIEMPTY;
		return EINVAL;
	case 1:
		if (SI_ISJET(sc->sc_type)) {
			/* set throttle to 100 times per second */
			regp->int_count = JET_INT_COUNT;
			/* rx_intr_count is a NOP in Jet */
		} else {
			/* set throttle to 125 times per second */
			regp->int_count = INT_COUNT;
			/* rx intr max of 25 times per second */
			regp->rx_int_count = RXINT_COUNT;
		}
		regp->int_pending = 0;		/* no intr pending */
		regp->int_scounter = 0;	/* reset counter */
		break;
	case 0xff:
		/*
		 * No modules found, so give up on this one.
		 */
		printf("si%d: %s - no ports found\n", unit,
			si_type[sc->sc_type]);
		return 0;
	default:
		printf("si%d: download code version error - initstat %x\n",
			unit, regp->initstat);
		return EINVAL;
	}

	/*
	 * First time around the ports just count them in order
	 * to allocate some memory.
	 */
	nport = 0;
	modp = (struct si_module *)(maddr + 0x80);
	for (;;) {
		DPRINT((0, DBG_DOWNLOAD, "si%d: ccb addr 0x%x\n", unit, modp));
		switch (modp->sm_type) {
		case TA4:
			DPRINT((0, DBG_DOWNLOAD,
				"si%d: Found old TA4 module, 4 ports\n",
				unit));
			x = 4;
			break;
		case TA8:
			DPRINT((0, DBG_DOWNLOAD,
				"si%d: Found old TA8 module, 8 ports\n",
				unit));
			x = 8;
			break;
		case TA4_ASIC:
			DPRINT((0, DBG_DOWNLOAD,
				"si%d: Found ASIC TA4 module, 4 ports\n",
				unit));
			x = 4;
			break;
		case TA8_ASIC:
			DPRINT((0, DBG_DOWNLOAD,
				"si%d: Found ASIC TA8 module, 8 ports\n",
				unit));
			x = 8;
			break;
		case MTA:
			DPRINT((0, DBG_DOWNLOAD,
				"si%d: Found CD1400 module, 8 ports\n",
				unit));
			x = 8;
			break;
		case SXDC:
			DPRINT((0, DBG_DOWNLOAD,
				"si%d: Found SXDC module, 8 ports\n",
				unit));
			x = 8;
			break;
		default:
			printf("si%d: unknown module type %d\n",
				unit, modp->sm_type);
			goto try_next;
		}

		/* this was limited in firmware and is also a driver issue */
		if ((nport + x) > SI_MAXPORTPERCARD) {
			printf("si%d: extra ports ignored\n", unit);
			goto try_next;
		}

		nport += x;
		si_Nports += x;
		si_Nmodules++;

try_next:
		if (modp->sm_next == 0)
			break;
		modp = (struct si_module *)
			(maddr + (unsigned)(modp->sm_next & 0x7fff));
	}
	sc->sc_ports = (struct si_port *)malloc(sizeof(struct si_port) * nport,
		M_DEVBUF, M_NOWAIT | M_ZERO);
	if (sc->sc_ports == 0) {
		printf("si%d: fail to malloc memory for port structs\n",
			unit);
		return EINVAL;
	}
	sc->sc_nport = nport;

	/*
	 * Scan round the ports again, this time initialising.
	 */
	pp = sc->sc_ports;
	nmodule = 0;
	modp = (struct si_module *)(maddr + 0x80);
	uart_type = 1000;	/* arbitary, > uchar_max */
	for (;;) {
		switch (modp->sm_type) {
		case TA4:
			nport = 4;
			break;
		case TA8:
			nport = 8;
			break;
		case TA4_ASIC:
			nport = 4;
			break;
		case TA8_ASIC:
			nport = 8;
			break;
		case MTA:
			nport = 8;
			break;
		case SXDC:
			nport = 8;
			break;
		default:
			goto try_next2;
		}
		nmodule++;
		ccbp = (struct si_channel *)((char *)modp + 0x100);
		if (uart_type == 1000)
			uart_type = ccbp->type;
		else if (uart_type != ccbp->type)
			printf("si%d: Warning: module %d mismatch! (%d%s != %d%s)\n",
			    unit, nmodule,
			    ccbp->type, si_modulename(sc->sc_type, ccbp->type),
			    uart_type, si_modulename(sc->sc_type, uart_type));

		for (x = 0; x < nport; x++, pp++, ccbp++) {
			pp->sp_ccb = ccbp;	/* save the address */
			pp->sp_pend = IDLE_CLOSE;
			pp->sp_state = 0;	/* internal flag */
#ifdef SI_DEBUG
			sprintf(pp->sp_name, "si%r%r", unit,
			    (int)(pp - sc->sc_ports));
#endif
			tp = pp->sp_tty = tty_alloc_mutex(&si_tty_class, pp, &Giant);
			tty_makedev(tp, NULL, "A%r%r", unit, (int)(pp - sc->sc_ports));
		}
try_next2:
		if (modp->sm_next == 0) {
			printf("si%d: card: %s, ports: %d, modules: %d, type: %d%s\n",
				unit,
				sc->sc_typename,
				sc->sc_nport,
				nmodule,
				uart_type,
				si_modulename(sc->sc_type, uart_type));
			break;
		}
		modp = (struct si_module *)
			(maddr + (unsigned)(modp->sm_next & 0x7fff));
	}

	if (unit == 0)
		make_dev(&si_Scdevsw, 0, UID_ROOT, GID_WHEEL, 0600,
		    "si_control");
	return (0);
}

static	int
siopen(struct tty *tp)
{

	DPRINT((0, DBG_ENTRY|DBG_OPEN, "siopen()\n"));
	mtx_assert(&Giant, MA_OWNED);
#ifdef	POLL
	/*
	 * We've now got a device, so start the poller.
	 */
	if (init_finished == 0) {
		timeout(si_poll, (caddr_t)0L, si_pollrate);
		init_finished = 1;
	}
#endif
	DPRINT((0, DBG_EXIT|DBG_OPEN, "siopen() finished\n"));
	return(0);
}

static void
siclose(struct tty *tp)
{
	struct si_port *pp;

	DPRINT((0, DBG_ENTRY|DBG_CLOSE, "siclose()\n"));
	mtx_assert(&Giant, MA_OWNED);
	pp = tty_softc(tp);
	(void) si_command(pp, FCLOSE, SI_WAIT);
	DPRINT((0, DBG_EXIT|DBG_CLOSE, "siclose() finished\n"));
}

static int
siioctl(struct tty *tp, u_long cmd, caddr_t data, struct thread *td)
{
	struct si_port *pp;

	DPRINT((0, DBG_ENTRY|DBG_IOCTL, "siioctl(0x%lx,0x%x)\n", cmd, data));
	mtx_assert(&Giant, MA_OWNED);
	pp = tty_softc(tp);
	switch (cmd) {
	case TIOCSBRK:
		si_command(pp, SBREAK, SI_WAIT);
		return (0);
	case TIOCCBRK:
		si_command(pp, EBREAK, SI_WAIT);
		return (0);
	}
	return (ENOIOCTL);	/* Let the common tty ioctl handler do it */
}

/*
 * Handle the Specialix ioctls on the control dev.
 */
static int
si_Sioctl(struct cdev *dev, u_long cmd, caddr_t data, int flag, struct thread *td)
{
	struct si_softc *xsc;
	struct si_port *xpp;
	volatile struct si_reg *regp;
	struct si_tcsi *dp;
	struct si_pstat *sps;
	int *ip, error = 0;
	int oldspl;
	int card, port;

	DPRINT((0, DBG_ENTRY|DBG_IOCTL, "si_Sioctl(%s,0x%lx,0x%x)\n",
		devtoname(dev), cmd, data));
	mtx_assert(&Giant, MA_OWNED);

#if 1
	DPRINT((0, DBG_IOCTL, "TCSI_PORT=%x\n", TCSI_PORT));
	DPRINT((0, DBG_IOCTL, "TCSI_CCB=%x\n", TCSI_CCB));
#endif

	oldspl = spltty();	/* better safe than sorry */

	ip = (int *)data;

#define SUCHECK if ((error = priv_check(td, PRIV_DRIVER))) goto out

	switch (cmd) {
	case TCSIPORTS:
		*ip = si_Nports;
		goto out;
	case TCSIMODULES:
		*ip = si_Nmodules;
		goto out;
	case TCSISDBG_ALL:
		SUCHECK;
		si_debug = *ip;
		goto out;
	case TCSIGDBG_ALL:
		*ip = si_debug;
		goto out;
	default:
		/*
		 * Check that a controller for this port exists
		 */

		/* may also be a struct si_pstat, a superset of si_tcsi */

		dp = (struct si_tcsi *)data;
		sps = (struct si_pstat *)data;
		card = dp->tc_card;
		xsc = devclass_get_softc(si_devclass, card);	/* check.. */
		if (xsc == NULL || xsc->sc_type == SIEMPTY) {
			error = ENOENT;
			goto out;
		}
		/*
		 * And check that a port exists
		 */
		port = dp->tc_port;
		if (port < 0 || port >= xsc->sc_nport) {
			error = ENOENT;
			goto out;
		}
		xpp = xsc->sc_ports + port;
		regp = (struct si_reg *)xsc->sc_maddr;
	}

	switch (cmd) {
	case TCSIDEBUG:
#ifdef	SI_DEBUG
		SUCHECK;
		if (xpp->sp_debug)
			xpp->sp_debug = 0;
		else {
			xpp->sp_debug = DBG_ALL;
			DPRINT((xpp, DBG_IOCTL, "debug toggled %s\n",
				(xpp->sp_debug&DBG_ALL)?"ON":"OFF"));
		}
		break;
#else
		error = ENODEV;
		goto out;
#endif
	case TCSISDBG_LEVEL:
	case TCSIGDBG_LEVEL:
#ifdef	SI_DEBUG
		if (cmd == TCSIGDBG_LEVEL) {
			dp->tc_dbglvl = xpp->sp_debug;
		} else {
			SUCHECK;
			xpp->sp_debug = dp->tc_dbglvl;
		}
		break;
#else
		error = ENODEV;
		goto out;
#endif
	case TCSIGRXIT:
		dp->tc_int = regp->rx_int_count;
		break;
	case TCSIRXIT:
		SUCHECK;
		regp->rx_int_count = dp->tc_int;
		break;
	case TCSIGIT:
		dp->tc_int = regp->int_count;
		break;
	case TCSIIT:
		SUCHECK;
		regp->int_count = dp->tc_int;
		break;
	case TCSISTATE:
		dp->tc_int = xpp->sp_ccb->hi_ip;
		break;
	/* these next three use a different structure */
	case TCSI_PORT:
		SUCHECK;
		si_bcopy(xpp, &sps->tc_siport, sizeof(sps->tc_siport));
		break;
	case TCSI_CCB:
		SUCHECK;
		si_vbcopy(xpp->sp_ccb, &sps->tc_ccb, sizeof(sps->tc_ccb));
		break;
	default:
		error = EINVAL;
		goto out;
	}
out:
	splx(oldspl);
	return(error);		/* success */
}

/*
 *	siparam()	: Configure line params
 *	called at spltty();
 *	this may sleep, does not flush, nor wait for drain, nor block writes
 *	caller must arrange this if it's important..
 */
static int
siparam(struct tty *tp, struct termios *t)
{
	struct si_port *pp = tty_softc(tp);
	volatile struct si_channel *ccbp;
	int oldspl, cflag, iflag, oflag, lflag;
	int error = 0;		/* shutup gcc */
	int ispeed = 0;		/* shutup gcc */
	int ospeed = 0;		/* shutup gcc */
	BYTE val;

	DPRINT((pp, DBG_ENTRY|DBG_PARAM, "siparam(%x,%x)\n", tp, t));
	mtx_assert(&Giant, MA_OWNED);
	cflag = t->c_cflag;
	iflag = t->c_iflag;
	oflag = t->c_oflag;
	lflag = t->c_lflag;
	DPRINT((pp, DBG_PARAM, "OFLAG 0x%x CFLAG 0x%x IFLAG 0x%x LFLAG 0x%x\n",
		oflag, cflag, iflag, lflag));

	/* XXX - if Jet host and SXDC module, use extended baud rates */

	/* if not hung up.. */
	if (t->c_ospeed != 0) {
		/* translate baud rate to firmware values */
		ospeed = si_speedtab(t->c_ospeed, bdrates);
		ispeed = t->c_ispeed ?
			 si_speedtab(t->c_ispeed, bdrates) : ospeed;
		/* enforce legit baud rate */
		if (ospeed < 0 || ispeed < 0)
			return (EINVAL);
	}

	oldspl = spltty();

	ccbp = pp->sp_ccb;

	/* ========== set hi_break ========== */
	val = 0;
	if (iflag & IGNBRK)		/* Breaks */
		val |= BR_IGN;
	if (iflag & BRKINT)		/* Interrupt on break? */
		val |= BR_INT;
	if (iflag & PARMRK)		/* Parity mark? */
		val |= BR_PARMRK;
	if (iflag & IGNPAR)		/* Ignore chars with parity errors? */
		val |= BR_PARIGN;
	ccbp->hi_break = val;

	/* ========== set hi_csr ========== */
	/* if not hung up.. */
	if (t->c_ospeed != 0) {
		/* Set I/O speeds */
		 val = (ispeed << 4) | ospeed;
	}
	ccbp->hi_csr = val;

	/* ========== set hi_mr2 ========== */
	val = 0;
	if (cflag & CSTOPB)				/* Stop bits */ 
		val |= MR2_2_STOP;
	else
		val |= MR2_1_STOP;

	/*
	 * Enable H/W RTS/CTS handshaking. The default TA/MTA is
	 * a DCE, hence the reverse sense of RTS and CTS
	 */
	/* Output Flow - RTS must be raised before data can be sent */
	if (cflag & CCTS_OFLOW)
		val |= MR2_RTSCONT;

	ccbp->hi_mr2 = val;

	/* ========== set hi_mr1 ========== */
	val = 0;
	if (!(cflag & PARENB))				/* Parity */
		val |= MR1_NONE;
	else
		val |= MR1_WITH;
	if (cflag & PARODD)
		val |= MR1_ODD;

	if ((cflag & CS8) == CS8)			/* 8 data bits? */
		val |= MR1_8_BITS;
	else if ((cflag & CS7) == CS7)			/* 7 data bits? */
		val |= MR1_7_BITS;
	else if ((cflag & CS6) == CS6)			/* 6 data bits? */
		val |= MR1_6_BITS;
	else						/* Must be 5 */
		val |= MR1_5_BITS;

	/*
	 * Enable H/W RTS/CTS handshaking. The default TA/MTA is
	 * a DCE, hence the reverse sense of RTS and CTS
	 */
	/* Input Flow - CTS is raised when port is ready to receive data */
	if (cflag & CRTS_IFLOW)
		val |= MR1_CTSCONT;

	ccbp->hi_mr1 = val;

	/* ========== set hi_mask ========== */
	val = 0xff;
	if ((cflag & CS8) == CS8) {			/* 8 data bits? */
		val &= 0xFF;
	} else if ((cflag & CS7) == CS7) {		/* 7 data bits? */
		val &= 0x7F;
	} else if ((cflag & CS6) == CS6) {		/* 6 data bits? */
		val &= 0x3F;
	} else {					/* Must be 5 */
		val &= 0x1F;
	}
	if (iflag & ISTRIP)
		val &= 0x7F;

	ccbp->hi_mask = val;

	/* ========== set hi_prtcl ========== */
	val = SP_DCEN;		/* Monitor DCD always, or TIOCMGET misses it */
	if (iflag & IXANY)
		val |= SP_TANY;
	if (iflag & IXON)
		val |= SP_TXEN;
	if (iflag & IXOFF)
		val |= SP_RXEN;
	if (iflag & INPCK)
		val |= SP_PAEN;

	ccbp->hi_prtcl = val;


	/* ========== set hi_{rx|tx}{on|off} ========== */
	/* XXX: the card TOTALLY shields us from the flow control... */
	ccbp->hi_txon = t->c_cc[VSTART];
	ccbp->hi_txoff = t->c_cc[VSTOP];

	ccbp->hi_rxon = t->c_cc[VSTART];
	ccbp->hi_rxoff = t->c_cc[VSTOP];

	/* ========== send settings to the card ========== */
	/* potential sleep here */
	if (ccbp->hi_stat == IDLE_CLOSE)		/* Not yet open */
		si_command(pp, LOPEN, SI_WAIT);		/* open it */
	else
		si_command(pp, CONFIG, SI_WAIT);	/* change params */

	/* ========== set DTR etc ========== */
	/* Hangup if ospeed == 0 */
	if (t->c_ospeed == 0) {
		(void) simodem(tp, 0, SER_DTR | SER_RTS);
	} else {
		/*
		 * If the previous speed was 0, may need to re-enable
		 * the modem signals
		 */
		(void) simodem(tp, SER_DTR | SER_RTS, 0);
	}

	DPRINT((pp, DBG_PARAM, "siparam, complete: MR1 %x MR2 %x HI_MASK %x PRTCL %x HI_BREAK %x HI_CSR %x\n",
		ccbp->hi_mr1, ccbp->hi_mr2, ccbp->hi_mask, ccbp->hi_prtcl, ccbp->hi_break, ccbp->hi_csr));

	splx(oldspl);
	return(error);
}

/*
 * Set/Get state of modem control lines.
 * Due to DCE-like behaviour of the adapter, some signals need translation:
 *	TIOCM_DTR	DSR
 *	TIOCM_RTS	CTS
 */
static int
simodem(struct tty *tp, int sigon, int sigoff)
{
	struct si_port *pp;
	volatile struct si_channel *ccbp;
	int x;

	pp = tty_softc(tp);
	DPRINT((pp, DBG_ENTRY|DBG_MODEM, "simodem(%x,%x)\n", sigon, sigoff));
	mtx_assert(&Giant, MA_OWNED);
	ccbp = pp->sp_ccb;		/* Find channel address */
	if (sigon == 0 && sigoff == 0) {
		x = ccbp->hi_ip;
		/*
		 * XXX: not sure this is correct, should it be CTS&DSR ?
		 * XXX: or do we (just) miss CTS & DSR ?
		 */
		if (x & IP_DCD)		sigon |= SER_DCD;
		if (x & IP_DTR)		sigon |= SER_DTR;
		if (x & IP_RTS)		sigon |= SER_RTS;
		if (x & IP_RI)		sigon |= SER_RI;
		return (sigon);
	}

	x = ccbp->hi_op;
	if (sigon & SER_DTR)
		x |= OP_DSR;
	if (sigoff & SER_DTR)
		x &= ~OP_DSR;
	if (sigon & SER_RTS)
		x |= OP_CTS;
	if (sigoff & SER_RTS)
		x &= ~OP_CTS;
	ccbp->hi_op = x;
	return 0;
}

/*
 * Handle change of modem state
 */
static void
si_modem_state(struct si_port *pp, struct tty *tp, int hi_ip)
{
							/* if a modem dev */
	mtx_assert(&Giant, MA_OWNED);
	if (hi_ip & IP_DCD) {
		if (!(pp->sp_last_hi_ip & IP_DCD)) {
			DPRINT((pp, DBG_INTR, "modem carr on%d\n"));
			(void)ttydisc_modem(tp, 1);
		}
	} else {
		if (pp->sp_last_hi_ip & IP_DCD) {
			DPRINT((pp, DBG_INTR, "modem carr off\n"));
#if 0	/* XXX mpsafetty ttyld_modem used to tell us to shutdown the port or not */
			if (ttydisc_modem(tp, 0))
				(void) simodem(tp, 0, SER_DTR | SER_RTS);
#else
			ttydisc_modem(tp, 0);
#endif
		}
	}
	pp->sp_last_hi_ip = hi_ip;

}

/*
 * Poller to catch missed interrupts.
 *
 * Note that the SYSV Specialix drivers poll at 100 times per second to get
 * better response.  We could really use a "periodic" version timeout(). :-)
 */
#ifdef POLL
static void
si_poll(void *nothing)
{
	struct si_softc *sc;
	int i;
	volatile struct si_reg *regp;
	struct si_port *pp;
	int lost, oldspl, port;

	DPRINT((0, DBG_POLL, "si_poll()\n"));
	oldspl = spltty();
	mtx_assert(&Giant, MA_OWNED);
	lost = 0;
	for (i = 0; i < si_numunits; i++) {
		sc = devclass_get_softc(si_devclass, i);
		if (sc == NULL || sc->sc_type == SIEMPTY)
			continue;
		regp = (struct si_reg *)sc->sc_maddr;

		/*
		 * See if there has been a pending interrupt for 2 seconds
		 * or so. The test (int_scounter >= 200) won't correspond
		 * to 2 seconds if int_count gets changed.
		 */
		if (regp->int_pending != 0) {
			if (regp->int_scounter >= 200 &&
			    regp->initstat == 1) {
				printf("si%d: lost intr\n", i);
				lost++;
			}
		} else {
			regp->int_scounter = 0;
		}

		/*
		 * gripe about no input flow control..
		 */
		pp = sc->sc_ports;
		for (port = 0; port < sc->sc_nport; pp++, port++) {
			if (pp->sp_delta_overflows > 0) {
				printf("si%d: %d tty level buffer overflows\n",
					i, pp->sp_delta_overflows);
				pp->sp_delta_overflows = 0;
			}
		}
	}
	if (lost || si_realpoll)
		si_intr(NULL);	/* call intr with fake vector */
	splx(oldspl);

	timeout(si_poll, (caddr_t)0L, si_pollrate);
}
#endif	/* ifdef POLL */

/*
 * The interrupt handler polls ALL ports on ALL adapters each time
 * it is called.
 */

static BYTE si_rxbuf[SI_BUFFERSIZE];	/* input staging area */
static BYTE si_txbuf[SI_BUFFERSIZE];	/* output staging area */

void
si_intr(void *arg)
{
	struct si_softc *sc;
	struct si_port *pp;
	volatile struct si_channel *ccbp;
	struct tty *tp;
	volatile caddr_t maddr;
	BYTE op, ip;
	int x, card, port, n, i, isopen;
	volatile BYTE *z;
	BYTE c;

	sc = arg;
	mtx_assert(&Giant, MA_OWNED);

	DPRINT((0, arg == NULL ? DBG_POLL:DBG_INTR, "si_intr\n"));

	/*
	 * When we get an int we poll all the channels and do ALL pending
	 * work, not just the first one we find. This allows all cards to
	 * share the same vector.
	 *
	 * XXX - But if we're sharing the vector with something that's NOT
	 * a SI/XIO/SX card, we may be making more work for ourselves.
	 */
	for (card = 0; card < si_numunits; card++) {
		sc = devclass_get_softc(si_devclass, card);
		if (sc == NULL || sc->sc_type == SIEMPTY)
			continue;

		/*
		 * First, clear the interrupt
		 */
		switch(sc->sc_type) {
		case SIHOST:
			maddr = sc->sc_maddr;
			((volatile struct si_reg *)maddr)->int_pending = 0;
							/* flag nothing pending */
			*(maddr+SIINTCL) = 0x00;	/* Set IRQ clear */
			*(maddr+SIINTCL_CL) = 0x00;	/* Clear IRQ clear */
			break;
		case SIHOST2:
			maddr = sc->sc_maddr;
			((volatile struct si_reg *)maddr)->int_pending = 0;
			*(maddr+SIPLIRQCLR) = 0x00;
			*(maddr+SIPLIRQCLR) = 0x10;
			break;
		case SIPCI:
			maddr = sc->sc_maddr;
			((volatile struct si_reg *)maddr)->int_pending = 0;
			*(maddr+SIPCIINTCL) = 0x0;
			break;
		case SIJETPCI:	/* fall through to JETISA case */
		case SIJETISA:
			maddr = sc->sc_maddr;
			((volatile struct si_reg *)maddr)->int_pending = 0;
			*(maddr+SIJETINTCL) = 0x0;
			break;
#ifdef DEV_EISA
		case SIEISA:
			maddr = sc->sc_maddr;
			((volatile struct si_reg *)maddr)->int_pending = 0;
			(void)inb(sc->sc_iobase + 3);
			break;
#endif
		case SIEMPTY:
		default:
			continue;
		}
		((volatile struct si_reg *)maddr)->int_scounter = 0;

		/*
		 * check each port
		 */
		for (pp = sc->sc_ports, port = 0; port < sc->sc_nport;
		     pp++, port++) {
			ccbp = pp->sp_ccb;
			tp = pp->sp_tty;
			tty_lock(tp);

			/*
			 * See if a command has completed ?
			 */
			if (ccbp->hi_stat != pp->sp_pend) {
				DPRINT((pp, DBG_INTR,
					"si_intr hi_stat = %s, pend = %s\n",
					si_cmdname(ccbp->hi_stat),
					si_cmdname(pp->sp_pend)));
				switch(pp->sp_pend) {
				case LOPEN:
				case MPEND:
				case MOPEN:
				case FCLOSE:
				case CONFIG:
				case SBREAK:
				case EBREAK:
					/* sleeping in si_command */
					DPRINT((pp, DBG_INTR, "do wakeup\n"));
					wakeup(&pp->sp_state);
					break;
				}
				pp->sp_pend = ccbp->hi_stat;
			}

			/*
			 * Continue on if it's closed
			 */
			if (ccbp->hi_stat == IDLE_CLOSE) {
				tty_unlock(tp);
				continue;
			}

			/*
			 * Do modem state change if not a local device
			 */
			si_modem_state(pp, tp, ccbp->hi_ip);

			/*
			 * Check to see if we should 'receive' characters.
			 */
			isopen = tty_opened(tp);

			/*
			 * Do input break processing
			 */
			if (ccbp->hi_state & ST_BREAK) {
				if (isopen)
					ttydisc_rint(tp, 0, TRE_BREAK);
				ccbp->hi_state &= ~ST_BREAK;   /* A Bit iffy this */
				DPRINT((pp, DBG_INTR, "si_intr break\n"));
			}

			/*
			 * Do RX stuff - if not open then dump any characters.
			 * XXX: This is VERY messy and needs to be cleaned up.
			 *
			 * XXX: can we leave data in the host adapter buffer
			 * when the clists are full?  That may be dangerous
			 * if the user cannot get an interrupt signal through.
			 */

	more_rx:

			if (!isopen) {
				DPRINT((pp, DBG_INTR, "intr1: not open\n"));
				ccbp->hi_rxopos = ccbp->hi_rxipos;
				goto end_rx;
			}

#if 0 /* XXXMPSAFETTY */
			/*
			 * If the tty input buffers are blocked, stop emptying
			 * the incoming buffers and let the auto flow control
			 * assert..
			 */
			if (tp->t_state & TS_TBLOCK)
				goto end_rx;
#endif

			/*
			 * Process read characters if not skipped above
			 */
			op = ccbp->hi_rxopos;
			ip = ccbp->hi_rxipos;
			c = ip - op;
			if (c == 0)
				goto end_rx;

			n = c & 0xff;
			if (n > 250)
				n = 250;

			DPRINT((pp, DBG_INTR, "n = %d, op = %d, ip = %d\n",
						n, op, ip));

			/*
			 * Suck characters out of host card buffer into the
			 * "input staging buffer" - so that we dont leave the
			 * host card in limbo while we're possibly echoing
			 * characters and possibly flushing input inside the
			 * ldisc l_rint() routine.
			 */
			if (n <= SI_BUFFERSIZE - op) {

				z = ccbp->hi_rxbuf + op;
				si_vbcopy(z, si_rxbuf, n);

				op += n;
			} else {
				x = SI_BUFFERSIZE - op;

				z = ccbp->hi_rxbuf + op;
				si_vbcopy(z, si_rxbuf, x);

				z = ccbp->hi_rxbuf;
				si_vbcopy(z, si_rxbuf + x, n - x);

				op += n;
			}

			/* clear collected characters from buffer */
			ccbp->hi_rxopos = op;

			/*
			 * at this point...
			 * n = number of chars placed in si_rxbuf
			 */

			if (0 && ttydisc_can_bypass(tp)) {

				i =  ttydisc_rint_bypass(tp, (char *)si_rxbuf, n);
				if (i < n)
					pp->sp_delta_overflows += (n - i);

			} else {
				/*
				 * It'd be nice to not have to go through the
				 * function call overhead for each char here.
				 * It'd be nice to block input it, saving a
				 * loop here and the call/return overhead.
				 */
				for(x = 0; x < n; x++) {
					i = si_rxbuf[x];
					if (ttydisc_rint(tp, i, 0) == -1)
						pp->sp_delta_overflows++;
				}
			}
			goto more_rx;	/* try for more until RXbuf is empty */

	end_rx:

			ttydisc_rint_done(tp);

			/*
			 * Do TX stuff
			 */
			si_start(tp);
			tty_unlock(tp);

		} /* end of for (all ports on this controller) */
	} /* end of for (all controllers) */

	DPRINT((0, arg == NULL ? DBG_POLL:DBG_INTR, "end si_intr\n"));
}

/*
 * Nudge the transmitter...
 *
 * XXX: I inherited some funny code here.  It implies the host card only
 * interrupts when the transmit buffer reaches the low-water-mark, and does
 * not interrupt when it's actually hits empty.  In some cases, we have
 * processes waiting for complete drain, and we need to simulate an interrupt
 * about when we think the buffer is going to be empty (and retry if not).
 * I really am not certain about this...  I *need* the hardware manuals.
 */
static void
si_start(struct tty *tp)
{
	struct si_port *pp;
	volatile struct si_channel *ccbp;
	BYTE ipos, count;
#if 0
	int nchar;
#endif
	int oldspl, n, amount;

	oldspl = spltty();
	mtx_assert(&Giant, MA_OWNED);

	pp = tty_softc(tp);

	DPRINT((pp, DBG_ENTRY|DBG_START,
		"si_start(%x) sp_state %x\n",
		tp, pp->sp_state));

	ccbp = pp->sp_ccb;

	while ((count = (int)ccbp->hi_txipos - (int)ccbp->hi_txopos) < 255) {
		DPRINT((pp, DBG_START, "txbuf pend count %d\n", (BYTE)count));
		ipos = (unsigned int)ccbp->hi_txipos;
		if ((int)ccbp->hi_txopos <= ipos)
			amount = SI_BUFFERSIZE - ipos;
		else
			amount = 255 - count;
		DPRINT((pp, DBG_START, "spaceleft amount %d\n", amount));
		if (amount == 0)
			break;
		n = ttydisc_getc(tp, si_txbuf, amount);
		DPRINT((pp, DBG_START, "getc n=%d\n", n));
		if (n == 0)
			break;
		si_bcopyv(si_txbuf, &ccbp->hi_txbuf[ipos], n);
		ccbp->hi_txipos += n;
	}

#if 0
	/*
	 * See if there are any characters still to come.  If so, we can
	 * depend on si_start being called again.
	 *
	 * XXX the manual is vague on this.  It implies we get an interrupt
	 * when the transmit queue reaches the 25% low water mark, but NOT
	 * when it hits empty.
	 */
	nchar = ttyoutq_getsize(&tp->t_outq) - ttyoutq_bytesleft(&tp->t_outq);
	DPRINT((pp, DBG_START, "count %d, nchar %d\n",
		(BYTE)count, nchar));

	if (count != 0 && nchar == 0) {
		int time;

		/* XXX lame. Ticks per character. used to be a table. */
		time = (tp->t_termios.c_ospeed + 9) / 10;

		if (time > 0) {
			if (time < nchar)
				time = nchar / time;
			else
				time = 2;
		} else {
			DPRINT((pp, DBG_START,
				"bad char time value! %d\n", time));
			time = hz/10;
		}

		if ((pp->sp_state & SS_LSTART) != 0)
			untimeout(si_lstart, (caddr_t)pp, pp->lstart_ch);
		DPRINT((pp, DBG_START, "arming lstart, time=%d\n", time));
		pp->sp_state |= SS_LSTART;
		pp->lstart_ch = timeout(si_lstart, (caddr_t)pp, time);
	}
#endif

	splx(oldspl);
	DPRINT((pp, DBG_EXIT|DBG_START, "leave si_start()\n"));
}

#if 0
/*
 * Note: called at splsoftclock from the timeout code
 * This has to deal with two things...  cause wakeups while waiting for
 * tty drains on last process exit, and call l_start at about the right
 * time for protocols like ppp.
 */
static void
si_lstart(void *arg)
{
	struct si_port *pp = arg;
	struct tty *tp;
	int oldspl;

	DPRINT((pp, DBG_ENTRY|DBG_LSTART, "si_lstart(%x) sp_state %x\n",
		pp, pp->sp_state));

	oldspl = spltty();
	mtx_assert(&Giant, MA_OWNED);
	pp->sp_state &= ~SS_LSTART;
	tp = pp->sp_tty;

	si_start(tp);

	splx(oldspl);
}
#endif

#if 0 /* XXX mpsafetty */
/*
 * Stop output on a line. called at spltty();
 */
static void
si_stop(struct tty *tp, int rw)
{
	volatile struct si_channel *ccbp;
	struct si_port *pp;

	mtx_assert(&Giant, MA_OWNED);
	pp = tty_softc(tp);
	ccbp = pp->sp_ccb;

	DPRINT((pp, DBG_ENTRY|DBG_STOP, "si_stop(%x,%x)\n", tp, rw));

	/* XXX: must check (rw & FWRITE | FREAD) etc flushing... */
	if (rw & FWRITE) {
		/* what level are we meant to be flushing anyway? */
		if (tp->t_state & TS_BUSY) {
			si_command(pp, WFLUSH, SI_NOWAIT);
			tp->t_state &= ~TS_BUSY;
			ttwwakeup(tp);	/* Bruce???? */
		}
	}
#if 1	/* XXX: this doesn't work right yet.. */
	/* XXX: this may have been failing because we used to call l_rint()
	 * while we were looping based on these two counters. Now, we collect
	 * the data and then loop stuffing it into l_rint(), making this
	 * useless.  Should we cause this to blow away the staging buffer?
	 */
	if (rw & FREAD) {
		ccbp->hi_rxopos = ccbp->hi_rxipos;
	}
#endif
}
#endif

/*
 * Issue a command to the host card CPU.
 *
 * XXX This is all just so WRONG!.  Ed says we're not supposed to sleep
 * here anyway.  We sort of get away with it for now by using Giant.
 * Something better will have to be done.
 * Linux does a busy spin here waiting for the 8-bit cpu to notice the
 * posted command and respond to it.  I'm not sure I like that either.
 */
static void
si_command(struct si_port *pp, int cmd, int waitflag)
{
	int oldspl;
	volatile struct si_channel *ccbp = pp->sp_ccb;
	int x;
	int err;

	DPRINT((pp, DBG_ENTRY|DBG_PARAM, "si_command(%x,%s,%d): hi_stat %s, sp_pend: %s\n",
		pp, si_cmdname(cmd), waitflag, si_cmdname(ccbp->hi_stat),
		si_cmdname(pp->sp_pend)));

	oldspl = spltty();		/* Keep others out */
	mtx_assert(&Giant, MA_OWNED);

	/* wait until it's finished what it was doing.. */
	/* XXX: sits in IDLE_BREAK until something disturbs it or break
	 * is turned off. */
	while((x = ccbp->hi_stat) != IDLE_OPEN &&
			x != IDLE_CLOSE &&
			x != IDLE_BREAK &&
			x != cmd) {
		DPRINT((pp, DBG_PARAM, "sicmd1 old cmd pending (going to tsleep): hi_stat (%s)\n", si_cmdname(ccbp->hi_stat)));
		err = tsleep(&pp->sp_state, (PSOCK+1)|PCATCH, "sicmd1", hz/4);
		if (err) {
			DPRINT((pp, DBG_PARAM, "sicmd1 timeout: hi_stat (%s)\n",
				si_cmdname(ccbp->hi_stat)));
			/* This is very very bad.  The card has crashed. */
			/* XXX the driver breaks at this point */
			if (err == ETIMEDOUT)
				DPRINT(("%s: tsleep1 timeout. hi_stat %s, sp_pend %s\n", pp->sp_name, si_cmdname(ccbp->hi_stat), si_cmdname(pp->sp_pend)));
			splx(oldspl);
			return;
		}
	}
	/* it should now be in IDLE_{OPEN|CLOSE|BREAK}, or "cmd" */
	DPRINT((pp, DBG_PARAM, "sicmd1 now in: hi_stat (%s) sp_pend (%s)\n", si_cmdname(ccbp->hi_stat), si_cmdname(pp->sp_pend)));

	/* if there was a pending command, cause a state-change wakeup */
	switch(pp->sp_pend) {
	case LOPEN:
	case MPEND:
	case MOPEN:
	case FCLOSE:
	case CONFIG:
	case SBREAK:
	case EBREAK:
		DPRINT((pp, DBG_PARAM, "si_command: sp_pend %s, doing wakeup\n", si_cmdname(pp->sp_pend)));
		wakeup(&pp->sp_state);
		break;
	default:
		break;
	}

	pp->sp_pend = cmd;		/* New command pending */
	ccbp->hi_stat = cmd;		/* Post it */
	DPRINT((pp, DBG_PARAM, "sicmd now posted: hi_stat (%s) sp_pend (%s)\n", si_cmdname(ccbp->hi_stat), si_cmdname(pp->sp_pend)));

	if (waitflag) {
		while((x = ccbp->hi_stat) != IDLE_OPEN &&
			     x != IDLE_CLOSE &&
			     x != IDLE_BREAK) {
			DPRINT((pp, DBG_PARAM, "sicmd2 now waiting: hi_stat (%s) sp_pend (%s) (going to tsleep)\n", si_cmdname(ccbp->hi_stat), si_cmdname(pp->sp_pend)));
			err = tsleep(&pp->sp_state, (PSOCK+1)|PCATCH, "sicmd2", hz);
			if (err) {
				DPRINT((pp, DBG_PARAM, "sicmd2 tsleep error: hi_stat (%s) sp_pend (%s)\n", si_cmdname(ccbp->hi_stat), si_cmdname(pp->sp_pend)));
				if (err == ETIMEDOUT) {
					DPRINT(("%s: tsleep2 timeout. hi_stat %s, sp_pend %s\n", pp->sp_name, si_cmdname(ccbp->hi_stat), si_cmdname(pp->sp_pend)));
				}
				break;
			}
		}
	}
	DPRINT((pp, DBG_PARAM, "sicmd2 finished: hi_stat (%s) sp_pend (%s)\n", si_cmdname(ccbp->hi_stat), si_cmdname(pp->sp_pend)));
	splx(oldspl);
}


#ifdef	SI_DEBUG

void
si_dprintf(struct si_port *pp, int flags, const char *fmt, ...)
{
	va_list ap;

	if ((pp == NULL && (si_debug&flags)) ||
	    (pp != NULL && ((pp->sp_debug&flags) || (si_debug&flags)))) {
		if (pp != NULL)
			printf("%s: ", pp->sp_name);
		va_start(ap, fmt);
		vprintf(fmt, ap);
		va_end(ap);
	}
}

#endif	/* DEBUG */

static char *
si_modulename(int host_type, int uart_type)
{
	switch (host_type) {
	/* Z280 based cards */
#ifdef DEV_EISA
	case SIEISA:
#endif
	case SIHOST2:
	case SIHOST:
	case SIPCI:
		switch (uart_type) {
		case 0:
			return(" (XIO)");
		case 1:
			return(" (SI)");
		}
		break;
	/* T225 based hosts */
	case SIJETPCI:
	case SIJETISA:
		switch (uart_type) {
		case 0:
			return(" (SI)");
		case 40:
			return(" (XIO)");
		case 72:
			return(" (SXDC)");
		}
		break;
	}
	return("");
}
