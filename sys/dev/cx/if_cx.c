/*
 * Cronyx-Sigma adapter driver for FreeBSD.
 * Supports PPP/HDLC and Cisco/HDLC protocol in synchronous mode,
 * and asyncronous channels with full modem control.
 * Keepalive protocol implemented in both Cisco and PPP modes.
 *
 * Copyright (C) 1994-2002 Cronyx Engineering.
 * Author: Serge Vakulenko, <vak@cronyx.ru>
 *
 * Copyright (C) 1999-2003 Cronyx Engineering.
 * Rewritten on DDK, ported to NETGRAPH, rewritten for FreeBSD 3.x-5.x by
 * Kurakin Roman, <rik@cronyx.ru>
 *
 * This software is distributed with NO WARRANTIES, not even the implied
 * warranties for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Authors grant any other persons or organisations a permission to use,
 * modify and redistribute this software in source and binary forms,
 * as long as this message is kept with the software, all derivative
 * works or modified versions.
 *
 * Cronyx Id: if_cx.c,v 1.1.2.18 2003/11/27 14:30:03 rik Exp $
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>

#if __FreeBSD_version >= 500000
#   define NCX 1
#else
#   include "cx.h"
#endif

#if NCX > 0
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/sockio.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/tty.h>
#if __FreeBSD_version >= 400000
#   include <sys/bus.h>
#   include <machine/bus.h>
#   include <sys/rman.h>
#   include <isa/isavar.h>
#endif
#include <sys/fcntl.h>
#include <sys/interrupt.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <net/if.h>
#include <machine/cpufunc.h>
#include <machine/cserial.h>
#include <machine/clock.h>
#if __FreeBSD_version < 500000
#include <machine/ipl.h>
#include <i386/isa/isa_device.h>
#endif
#if __FreeBSD_version >= 400000
#   include <machine/resource.h>
#if __FreeBSD_version <= 501000
#       include <i386/isa/intr_machdep.h>
#   endif
#endif
#if __FreeBSD_version >= 500000
#   include <dev/cx/machdep.h>
#   include <dev/cx/cxddk.h>
#   include <dev/cx/cronyxfw.h>
#else
#   include <i386/isa/cronyx/machdep.h>
#   include <i386/isa/cronyx/cxddk.h>
#   include <i386/isa/cronyx/cronyxfw.h>
#endif
#include "opt_ng_cronyx.h"
#ifdef NETGRAPH_CRONYX
#   include "opt_netgraph.h"
#   include <netgraph/ng_message.h>
#   include <netgraph/netgraph.h>
#   if __FreeBSD_version >= 500000
#       include <dev/cx/ng_cx.h>
#   else
#       include <netgraph/ng_cx.h>
#   endif
#else
#   include <net/if_types.h>
#   if __FreeBSD_version < 500000
#   include "sppp.h"
#   if NSPPP <= 0
#	error The device cp requires sppp or netgraph.
#   endif
#   endif
#   include <net/if_sppp.h>
#   define PP_CISCO IFF_LINK2
#if __FreeBSD_version < 400000
#   include <bpfilter.h>
#   if NBPFILTER > 0
#      include <net/bpf.h>
#   endif
#else
#   if __FreeBSD_version < 500000
#       include <bpf.h>
#   endif
#   include <net/bpf.h>
#   define NBPFILTER NBPF
#endif
#endif

#define CX_DEBUG(d,s)	({if (d->chan->debug) {\
				printf ("%s: ", d->name); printf s;}})
#define CX_DEBUG2(d,s)	({if (d->chan->debug>1) {\
				printf ("%s: ", d->name); printf s;}})

#define UNIT(d)         (minor(d) & 0x3f)
#define IF_CUNIT(d)     (minor(d) & 0x40)
#define UNIT_CTL        0x3f
#define CALLOUT(d)      (minor(d) & 0x80)
#define CDEV_MAJOR	42

typedef struct _async_q {
	int beg;
	int end;
	#define BF_SZ 14400
	int buf[BF_SZ+1];
} async_q;

#define AQ_GSZ(q)	((BF_SZ + (q)->end - (q)->beg)%BF_SZ)
#define AQ_PUSH(q,c)	{*((q)->buf + (q)->end) = c;\
			(q)->end = ((q)->end + 1)%BF_SZ;}
#define AQ_POP(q,c)	{c = *((q)->buf + (q)->beg);\
			(q)->beg = ((q)->beg + 1)%BF_SZ;}

#if __FreeBSD_version >= 400000
static void cx_identify		__P((driver_t *, device_t));
static int cx_probe		__P((device_t));
static int cx_attach		__P((device_t));
static int cx_detach		__P((device_t));

static device_method_t cx_isa_methods [] = {
	DEVMETHOD(device_identify,	cx_identify),
	DEVMETHOD(device_probe,		cx_probe),
	DEVMETHOD(device_attach,	cx_attach),
	DEVMETHOD(device_detach,	cx_detach),
	{0, 0}
};

typedef struct _bdrv_t {
	cx_board_t	*board;
	struct resource	*base_res;
	struct resource	*drq_res;
	struct resource	*irq_res;
	int		base_rid;
	int		drq_rid;
	int		irq_rid;
	void		*intrhand;
} bdrv_t;

static driver_t cx_isa_driver = {
	"cx",
	cx_isa_methods,
	sizeof (bdrv_t),
};

static devclass_t cx_devclass;
#endif

typedef struct _drv_t {
	char name [8];
	cx_chan_t *chan;
	cx_board_t *board;
	cx_buf_t buf;
	struct tty tty;
	struct callout_handle dcd_timeout_handle;
	unsigned dtrwait;
	unsigned dtroff;
	unsigned callout;
	unsigned lock;
	int open_dev;
	int cd;
	int running;
	struct	callout_handle dtr_timeout_handle;
#ifdef NETGRAPH
	char	nodename [NG_NODELEN+1];
	hook_p	hook;
	hook_p	debug_hook;
	node_p	node;
	struct	ifqueue lo_queue;
	struct	ifqueue hi_queue;
	short	timeout;
	struct	callout_handle timeout_handle;
#else
	struct sppp pp;
#endif
#if __FreeBSD_version >= 400000
	dev_t  devt[3];
#endif
	async_q aqueue;
	#define CX_READ 1
	#define CX_WRITE 2
	int intr_action;
	short atimeout;
} drv_t;

extern long csigma_fw_len;
extern const char *csigma_fw_version;
extern const char *csigma_fw_date;
extern const char *csigma_fw_copyright;
extern const cr_dat_tst_t csigma_fw_tvec[];
extern const u_char csigma_fw_data[];
static void cx_oproc (struct tty *tp);
static int cx_param (struct tty *tp, struct termios *t);
static void cx_stop (struct tty *tp, int flag);
static void cx_dtrwakeup (void *a);
static void cx_receive (cx_chan_t *c, char *data, int len);
static void cx_transmit (cx_chan_t *c, void *attachment, int len);
static void cx_error (cx_chan_t *c, int data);
static void cx_modem (cx_chan_t *c);
static void cx_up (drv_t *d);
static void cx_start (drv_t *d);
static void disc_optim(struct tty *tp, struct termios *t);
#if __FreeBSD_version < 500000
static swihand_t cx_softintr;
#else
static void cx_softintr (void *);
static void *cx_slow_ih;
static void *cx_fast_ih;
#endif
static void cx_down (drv_t *d);
static void cx_watchdog (drv_t *d);
static void cx_carrier (void *arg);

#ifdef NETGRAPH
extern struct ng_type typestruct;
#else
static void cx_ifstart (struct ifnet *ifp);
static void cx_tlf (struct sppp *sp);
static void cx_tls (struct sppp *sp);
static void cx_ifwatchdog (struct ifnet *ifp);
static int cx_sioctl (struct ifnet *ifp, u_long cmd, caddr_t data);
static void cx_initialize (void *softc);
#endif

static cx_board_t *adapter [NCX];
static drv_t *channel [NCX*NCHAN];
static struct callout_handle led_timo [NCX];
static struct callout_handle timeout_handle;
#if __FreeBSD_version >= 400000
	extern struct cdevsw cx_cdevsw;
#endif

static int MY_SOFT_INTR;

/*
 * Print the mbuf chain, for debug purposes only.
 */
static void printmbuf (struct mbuf *m)
{
	printf ("mbuf:");
	for (; m; m=m->m_next) {
		if (m->m_flags & M_PKTHDR)
			printf (" HDR %d:", m->m_pkthdr.len);
		if (m->m_flags & M_EXT)
			printf (" EXT:");
		printf (" %d", m->m_len);
	}
	printf ("\n");
}

/*
 * Make an mbuf from data.
 */
static struct mbuf *makembuf (void *buf, u_int len)
{
	struct mbuf *m, *o, *p;

	MGETHDR (m, M_DONTWAIT, MT_DATA);

	if (! m)
		return 0;

	if (len >= MINCLSIZE)
		MCLGET (m, M_DONTWAIT);

	m->m_pkthdr.len = len;
	m->m_len = 0;

	p = m;
	while (len) {
		u_int n = M_TRAILINGSPACE (p);
		if (n > len)
			n = len;
		if (! n) {
			/* Allocate new mbuf. */
			o = p;
			MGET (p, M_DONTWAIT, MT_DATA);
			if (! p) {
				m_freem (m);
				return 0;
			}
			if (len >= MINCLSIZE)
				MCLGET (p, M_DONTWAIT);
			p->m_len = 0;
			o->m_next = p;

			n = M_TRAILINGSPACE (p);
			if (n > len)
				n = len;
		}
		bcopy (buf, mtod (p, caddr_t) + p->m_len, n);

		p->m_len += n;
		buf = n + (char*) buf;
		len -= n;
	}
	return m;
}

/*
 * Recover after lost transmit interrupts.
 */
static void cx_timeout (void *arg)
{
	drv_t *d;
	int s, i;

	for (i=0; i<NCX*NCHAN; ++i) {
		d = channel[i];
		if (! d)
			continue;
		s = splhigh ();
		if (d->atimeout == 1 && d->tty.t_state & TS_BUSY) {
			d->tty.t_state &= ~TS_BUSY;
			if (d->tty.t_dev) {
				d->intr_action |= CX_WRITE;
				MY_SOFT_INTR = 1;
#if __FreeBSD_version >= 500000
				swi_sched (cx_fast_ih, 0);
#else
				setsofttty ();
#endif
			}
			CX_DEBUG (d, ("cx_timeout\n"));
		}
		if (d->atimeout)
			d->atimeout--;
		splx (s);
	}
	timeout_handle = timeout (cx_timeout, 0, hz*5);
}

static void cx_led_off (void *arg)
{
	cx_board_t *b = arg;
	int s = splhigh ();

	cx_led (b, 0);
	led_timo[b->num].callout = 0;
	splx (s);
}

/*
 * Activate interupt handler from DDK.
 */
#if __FreeBSD_version >= 400000
static void cx_intr (void *arg)
{
	bdrv_t *bd = arg;
	cx_board_t *b = bd->board;
#else
static void cx_intr (int bnum)
{
	cx_board_t *b = adapter [bnum];
#endif
	int s = splhigh ();

	/* Turn LED on. */
	cx_led (b, 1);

	cx_int_handler (b);

	/* Turn LED off 50 msec later. */
	if (! led_timo[b->num].callout)
		led_timo[b->num] = timeout (cx_led_off, b, hz/20);
	splx (s);
}

static int probe_irq (cx_board_t *b, int irq)
{
	int mask, busy, cnt;

	/* Clear pending irq, if any. */
	cx_probe_irq (b, -irq);
	DELAY (100);
	for (cnt=0; cnt<5; ++cnt) {
		/* Get the mask of pending irqs, assuming they are busy.
		 * Activate the adapter on given irq. */
		busy = cx_probe_irq (b, irq);
		DELAY (100);

		/* Get the mask of active irqs.
		 * Deactivate our irq. */
		mask = cx_probe_irq (b, -irq);
		DELAY (100);
		if ((mask & ~busy) == 1 << irq) {
			cx_probe_irq (b, 0);
			/* printf ("cx%d: irq %d ok, mask=0x%04x, busy=0x%04x\n",
				b->num, irq, mask, busy); */
			return 1;
		}
	}
	/* printf ("cx%d: irq %d not functional, mask=0x%04x, busy=0x%04x\n",
		b->num, irq, mask, busy); */
	cx_probe_irq (b, 0);
	return 0;
}

static short porttab [] = {
	0x200, 0x220, 0x240, 0x260, 0x280, 0x2a0, 0x2c0, 0x2e0,
	0x300, 0x320, 0x340, 0x360, 0x380, 0x3a0, 0x3c0, 0x3e0, 0
};
static char dmatab [] = { 7, 6, 5, 0 };
static char irqtab [] = { 5, 10, 11, 7, 3, 15, 12, 0 };

#if __FreeBSD_version >= 400000
static int cx_is_free_res (device_t dev, int rid, int type, u_long start,
	u_long end, u_long count)
{
	struct resource *res;
	
	if (!(res = bus_alloc_resource (dev, type, &rid, start, end, count,
	    RF_ALLOCATED)))
		return 0;
		
	bus_release_resource (dev, type, rid, res);
	
	return 1;
}

static void cx_identify (driver_t *driver, device_t dev)
{
	u_long iobase, rescount;
	int devcount;
	device_t *devices;
	device_t child;
	devclass_t my_devclass;
	int i, k;

	if ((my_devclass = devclass_find ("cx")) == NULL)
		return;

	devclass_get_devices (my_devclass, &devices, &devcount);

	if (devcount == 0) {
		/* We should find all devices by our self. We could alter other
		 * devices, but we don't have a choise
		 */
		for (i = 0; (iobase = porttab [i]) != 0; i++) {
			if (!cx_is_free_res (dev, 1, SYS_RES_IOPORT,
			    iobase, iobase + NPORT, NPORT))
				continue;
			if (cx_probe_board (iobase, -1, -1) == 0)
				continue;
			
			devcount++;

			child = BUS_ADD_CHILD (dev, ISA_ORDER_SPECULATIVE, "cx",
			    -1);

			if (child == NULL)
				return;

			device_set_desc_copy (child, "Cronyx Sigma");
			device_set_driver (child, driver);
			bus_set_resource (child, SYS_RES_IOPORT, 0,
			    iobase, NPORT);

			if (devcount >= NCX)
				break;
		}
	} else {
		static short porttab [] = {
			0x200, 0x220, 0x240, 0x260, 0x280, 0x2a0, 0x2c0, 0x2e0,
			0x300, 0x320, 0x340, 0x360, 0x380, 0x3a0, 0x3c0, 0x3e0, 0
		};
		/* Lets check user choise.
		 */
		for (k = 0; k < devcount; k++) {
			if (bus_get_resource (devices[k], SYS_RES_IOPORT, 0,
			    &iobase, &rescount) != 0)
				continue;

			for (i = 0; porttab [i] != 0; i++) {
				if (porttab [i] != iobase)
					continue;
				if (!cx_is_free_res (devices[k], 1, SYS_RES_IOPORT,
				    iobase, iobase + NPORT, NPORT))
					continue;
				if (cx_probe_board (iobase, -1, -1) == 0)
					continue;
				porttab [i] = -1;
				device_set_desc_copy (devices[k], "Cronyx Sigma");
				break;
			}

			if (porttab [i] == 0) {
				device_delete_child (
				    device_get_parent (devices[k]),
				    devices [k]);
				devices[k] = 0;
				continue;
			}
		}
		for (k = 0; k < devcount; k++) {
			if (devices[k] == 0)
				continue;
			if (bus_get_resource (devices[k], SYS_RES_IOPORT, 0,
			    &iobase, &rescount) == 0)
				continue;
			for (i = 0; (iobase = porttab [i]) != 0; i++) {
				if (porttab [i] == -1) {
					continue;
				}
				if (!cx_is_free_res (devices[k], 1, SYS_RES_IOPORT,
				    iobase, iobase + NPORT, NPORT))
					continue;
				if (cx_probe_board (iobase, -1, -1) == 0)
					continue;
			
				bus_set_resource (devices[k], SYS_RES_IOPORT, 0,
				    iobase, NPORT);
				porttab [i] = -1;
				device_set_desc_copy (devices[k], "Cronyx Sigma");
				break;
			}
			if (porttab [i] == 0) {
				device_delete_child (
				    device_get_parent (devices[k]),
				    devices [k]);
			}
		}		
		free (devices, M_TEMP);
	}
	
	return;
}

static int cx_probe (device_t dev)
{
	int unit = device_get_unit (dev);
	int i;
	u_long iobase, rescount;

	if (!device_get_desc (dev) ||
	    strcmp (device_get_desc (dev), "Cronyx Sigma"))
		return ENXIO;
	
	if (bus_get_resource (dev, SYS_RES_IOPORT, 0, &iobase, &rescount) != 0) {
		printf ("cx%d: Couldn't get IOPORT\n", unit);
		return ENXIO;
	}

	if (!cx_is_free_res (dev, 1, SYS_RES_IOPORT,
	    iobase, iobase + NPORT, NPORT)) {
		printf ("cx%d: Resource IOPORT isn't free %lx\n", unit, iobase);
		return ENXIO;
	}
		
	for (i = 0; porttab [i] != 0; i++) {
		if (porttab [i] == iobase) {
			porttab [i] = -1;
			break;
		}
	}
	
	if (porttab [i] == 0) {
		return ENXIO;
	}
	
	if (!cx_probe_board (iobase, -1, -1)) {
		printf ("cx%d: probing for Sigma at %lx faild\n", unit, iobase);
		return ENXIO;
	}
	
	return 0;
}
#else /* __FreeBSD_version < 400000 */
static int cx_probe (struct isa_device *id)
{
	cx_board_t *b;
	int i;

#ifndef NETGRAPH
	if (! sppp_attach) {
		printf ("cx%d: no synchronous PPP driver configured\n",
			id->id_unit);
		return 0;
	}
#endif
	if (id->id_iobase < 0) {
		/* Autodetect the adapter. */
		for (i=0; ; i++) {
			if (! porttab[i]) {
				id->id_iobase = -1;
				return 0;
			}
			id->id_iobase = porttab[i];
			if (id->id_unit > 0 && adapter[0] && adapter[0]->port == id->id_iobase)
				continue;
			if (id->id_unit > 1 && adapter[1] && adapter[1]->port == id->id_iobase)
				continue;
			if (! haveseen_isadev (id, CC_IOADDR | CC_QUIET) &&
 			    cx_probe_board (id->id_iobase, -1, -1))
 				break;
		}
	} else if (! cx_probe_board (id->id_iobase, -1, -1))
		return 0;

	if (id->id_drq < 0) {
		/* Find available 16-bit DRQ. */

		for (i=0; ; ++i) {
			if (! dmatab[i]) {
				printf ("cx%d: no available drq found\n",
					id->id_unit);
				id->id_drq = -1;
				return 0;
			}
			id->id_drq = dmatab[i];
			if (! haveseen_isadev (id, CC_DRQ | CC_QUIET)
			    && !isa_dma_acquire (id->id_drq))
 				break;
		}
	}

	b = malloc (sizeof (cx_board_t), M_DEVBUF, M_WAITOK);
	if (!b) {
		printf ("cx:%d: Couldn't allocate memory\n", id->id_unit);
		return (ENXIO);
	}
	adapter[id->id_unit] = b;
	bzero (b, sizeof(cx_board_t));

	if (! cx_open_board (b, id->id_unit, id->id_iobase,
	    id->id_irq ? ffs (id->id_irq) - 1 : -1, id->id_drq)) {
		printf ("cx%d: cannot initialize adapter\n", id->id_unit);
		isa_dma_release (id->id_drq);
		adapter[id->id_unit] = 0;
		free (b, M_DEVBUF);
		return 0;
	}

	if (id->id_irq) {
		if (! probe_irq (b, ffs (id->id_irq) - 1))
			printf ("cx%d: irq %d not functional\n",
				id->id_unit, ffs (id->id_irq) - 1);
	} else {
		/* Find available IRQ. */

		for (i=0; ; ++i) {
			if (! irqtab[i]) {
				printf ("cx%d: no available irq found\n",
					id->id_unit);
				id->id_irq = -1;
				isa_dma_release (id->id_drq);
				adapter[id->id_unit] = 0;
				free (b, M_DEVBUF);
				return 0;
			}
			id->id_irq = 1 << irqtab[i];
			if (haveseen_isadev (id, CC_IRQ | CC_QUIET))
				continue;
#ifdef KLD_MODULE
			if (register_intr (irqtab[i], 0, 0, (inthand2_t*)
			    cx_intr, &net_imask, id->id_unit) != 0)
				continue;
			unregister_intr (irqtab[i], (inthand2_t*) cx_intr);
#endif
 			if (probe_irq (b, irqtab[i]))
 				break;
		}
	}
	cx_init (b, b->num, b->port, ffs (id->id_irq) - 1, b->dma);
	cx_setup_board (b, 0, 0, 0);

	return 1;
}
#endif /* __FreeBSD_version < 400000 */

/*
 * The adapter is present, initialize the driver structures.
 */
#if __FreeBSD_version < 400000
static int cx_attach (struct isa_device *id)
{
#else
static int cx_attach (device_t dev)
{
	bdrv_t *bd = device_get_softc (dev);
	u_long iobase, drq, irq, rescount;
	int unit = device_get_unit (dev);
	int i;
	int s;
#endif
	cx_board_t *b;
	cx_chan_t *c;
	drv_t *d;

#if __FreeBSD_version >= 400000
	KASSERT ((bd != NULL), ("cx%d: NULL device softc\n", unit));
	
	bus_get_resource (dev, SYS_RES_IOPORT, 0, &iobase, &rescount);
	bd->base_rid = 0;
	bd->base_res = bus_alloc_resource (dev, SYS_RES_IOPORT, &bd->base_rid,
		iobase, iobase + NPORT, NPORT, RF_ACTIVE);
	if (! bd->base_res) {
		printf ("cx%d: cannot allocate base address\n", unit);
		return ENXIO;
	}
	
	if (bus_get_resource (dev, SYS_RES_DRQ, 0, &drq, &rescount) != 0) {
		for (i = 0; (drq = dmatab [i]) != 0; i++) {
			if (!cx_is_free_res (dev, 1, SYS_RES_DRQ,
			    drq, drq + 1, 1))
				continue;
			bus_set_resource (dev, SYS_RES_DRQ, 0, drq, 1);
			break;
		}
		
		if (dmatab[i] == 0) {	
			bus_release_resource (dev, SYS_RES_IOPORT, bd->base_rid,
				bd->base_res);
			printf ("cx%d: Couldn't get DRQ\n", unit);
			return ENXIO;
		}
	}
	
	bd->drq_rid = 0;
	bd->drq_res = bus_alloc_resource (dev, SYS_RES_DRQ, &bd->drq_rid,
		drq, drq + 1, 1, RF_ACTIVE);
	if (! bd->drq_res) {
		printf ("cx%d: cannot allocate drq\n", unit);
		bus_release_resource (dev, SYS_RES_IOPORT, bd->base_rid,
			bd->base_res);
		return ENXIO;
	}	
	
	if (bus_get_resource (dev, SYS_RES_IRQ, 0, &irq, &rescount) != 0) {
		for (i = 0; (irq = irqtab [i]) != 0; i++) {
			if (!cx_is_free_res (dev, 1, SYS_RES_IRQ,
			    irq, irq + 1, 1))
				continue;
			bus_set_resource (dev, SYS_RES_IRQ, 0, irq, 1);
			break;
		}
		
		if (irqtab[i] == 0) {	
			bus_release_resource (dev, SYS_RES_DRQ, bd->drq_rid,
				bd->drq_res);
			bus_release_resource (dev, SYS_RES_IOPORT, bd->base_rid,
				bd->base_res);
			printf ("cx%d: Couldn't get IRQ\n", unit);
			return ENXIO;
		}
	}
	
	bd->irq_rid = 0;
	bd->irq_res = bus_alloc_resource (dev, SYS_RES_IRQ, &bd->irq_rid,
		irq, irq + 1, 1, RF_ACTIVE);
	if (! bd->irq_res) {
		printf ("cx%d: Couldn't allocate irq\n", unit);
		bus_release_resource (dev, SYS_RES_DRQ, bd->drq_rid,
			bd->drq_res);
		bus_release_resource (dev, SYS_RES_IOPORT, bd->base_rid,
			bd->base_res);
		return ENXIO;
	}
	
	b = malloc (sizeof (cx_board_t), M_DEVBUF, M_WAITOK);
	if (!b) {
		printf ("cx:%d: Couldn't allocate memory\n", unit);
		return (ENXIO);
	}
	adapter[unit] = b;
	bzero (b, sizeof(cx_board_t));
	
	if (! cx_open_board (b, unit, iobase, irq, drq)) {
		printf ("cx%d: error loading firmware\n", unit);
		free (b, M_DEVBUF);
		bus_release_resource (dev, SYS_RES_IRQ, bd->irq_rid,
			bd->irq_res);
		bus_release_resource (dev, SYS_RES_DRQ, bd->drq_rid,
			bd->drq_res);
		bus_release_resource (dev, SYS_RES_IOPORT, bd->base_rid,
			bd->base_res);
 		return ENXIO;
	}

	bd->board = b;
	
	if (! probe_irq (b, irq)) {
		printf ("cx%d: irq %ld not functional\n", unit, irq);
		bd->board = 0;
		adapter [unit] = 0;
		free (b, M_DEVBUF);
		bus_release_resource (dev, SYS_RES_IRQ, bd->irq_rid,
			bd->irq_res);
		bus_release_resource (dev, SYS_RES_DRQ, bd->drq_rid,
			bd->drq_res);
		bus_release_resource (dev, SYS_RES_IOPORT, bd->base_rid,
			bd->base_res);
 		return ENXIO;
	}
	
	s = splhigh ();
	if (bus_setup_intr (dev, bd->irq_res, INTR_TYPE_NET, cx_intr, bd,
	    &bd->intrhand)) {
		printf ("cx%d: Can't setup irq %ld\n", unit, irq);
		bd->board = 0;
		adapter [unit] = 0;
		free (b, M_DEVBUF);
		bus_release_resource (dev, SYS_RES_IRQ, bd->irq_rid,
			bd->irq_res);
		bus_release_resource (dev, SYS_RES_DRQ, bd->drq_rid,
			bd->drq_res);
		bus_release_resource (dev, SYS_RES_IOPORT, bd->base_rid,
			bd->base_res);
		splx (s);
 		return ENXIO;		
	}
	
	cx_init (b, b->num, b->port, irq, drq);
	cx_setup_board (b, 0, 0, 0);
#else /* __FreeBSD_version >= 400000 */
	b = adapter[id->id_unit];
#endif /* __FreeBSD_version >= 400000 */

	printf ("cx%d: <Cronyx-Sigma-%s>\n", b->num, b->name);
#if __FreeBSD_version < 400000
	id->id_ointr = cx_intr;
#endif

	for (c=b->chan; c<b->chan+NCHAN; ++c) {
#if __FreeBSD_version >= 400000
		char *dnmt="tty %x";
		char *dnmc="cua %x";
#endif
		if (c->type == T_NONE)
			continue;
		d = contigmalloc (sizeof(drv_t), M_DEVBUF, M_WAITOK,
			0x100000, 0x1000000, 16, 0);
		channel [b->num*NCHAN + c->num] = d;
		bzero (d, sizeof(drv_t));
		sprintf (d->name, "cx%d.%d", b->num, c->num);
		d->board = b;
		d->chan = c;
		d->tty.t_oproc = cx_oproc;
		d->tty.t_param = cx_param;
#if __FreeBSD_version >= 400000
		d->tty.t_stop  = cx_stop;
#endif
		d->dtrwait = 3 * hz;	/* Default DTR off timeout is 3 seconds. */
		d->open_dev = 0;
		c->sys = d;

		switch (c->type) {
		case T_SYNC_RS232:
		case T_SYNC_V35:
		case T_SYNC_RS449:
		case T_UNIV:
		case T_UNIV_RS232:
		case T_UNIV_RS449:
		case T_UNIV_V35:
#ifdef NETGRAPH
		if (ng_make_node_common (&typestruct, &d->node) != 0) {
			printf ("%s: cannot make common node\n", d->name);
			channel [b->num*NCHAN + c->num] = 0;
			c->sys = 0;
			contigfree (d, sizeof (d), M_DEVBUF);
			continue;
		}
#if __FreeBSD_version >= 500000
		NG_NODE_SET_PRIVATE (d->node, d);
#else
		d->node->private = d;
#endif
		sprintf (d->nodename, "%s%d", NG_CX_NODE_TYPE,
			 c->board->num*NCHAN + c->num);
		if (ng_name_node (d->node, d->nodename)) {
			printf ("%s: cannot name node\n", d->nodename);
#if __FreeBSD_version >= 500000
			NG_NODE_UNREF (d->node);
#else
			ng_rmnode (d->node);
			ng_unref (d->node);
#endif
			channel [b->num*NCHAN + c->num] = 0;
			c->sys = 0;
			contigfree (d, sizeof (d), M_DEVBUF);
			continue;
		}
		d->lo_queue.ifq_maxlen = IFQ_MAXLEN;
		d->hi_queue.ifq_maxlen = IFQ_MAXLEN;
#if __FreeBSD_version >= 500000
		mtx_init (&d->lo_queue.ifq_mtx, "cx_queue_lo", NULL, MTX_DEF);
		mtx_init (&d->hi_queue.ifq_mtx, "cx_queue_hi", NULL, MTX_DEF);
#endif		
#else /*NETGRAPH*/
		d->pp.pp_if.if_softc    = d;
#if __FreeBSD_version > 501000
		if_initname (&d->pp.pp_if, "cx", b->num * NCHAN + c->num);
#else
		d->pp.pp_if.if_unit     = b->num * NCHAN + c->num;
		d->pp.pp_if.if_name     = "cx";
#endif
		d->pp.pp_if.if_mtu      = PP_MTU;
		d->pp.pp_if.if_flags    = IFF_POINTOPOINT | IFF_MULTICAST;
		d->pp.pp_if.if_ioctl    = cx_sioctl;
		d->pp.pp_if.if_start    = cx_ifstart;
		d->pp.pp_if.if_watchdog = cx_ifwatchdog;
		d->pp.pp_if.if_init     = cx_initialize;
		sppp_attach (&d->pp.pp_if);
		if_attach (&d->pp.pp_if);
		d->pp.pp_tlf            = cx_tlf;
		d->pp.pp_tls		= cx_tls;
#if __FreeBSD_version >= 400000 || NBPFILTER > 0
		/* If BPF is in the kernel, call the attach for it.
		 * Size of PPP header is 4 bytes. */
		bpfattach (&d->pp.pp_if, DLT_PPP, 4);
#endif
#endif /*NETGRAPH*/
		}
		cx_start_chan (c, &d->buf, vtophys (&d->buf));
		cx_register_receive (c, &cx_receive);
		cx_register_transmit (c, &cx_transmit);
		cx_register_error (c, &cx_error);
		cx_register_modem (c, &cx_modem);
#if __FreeBSD_version >= 400000
		dnmt[3] = 'x'+b->num;
		dnmc[3] = 'x'+b->num;
		d->devt[0] = make_dev (&cx_cdevsw, b->num*NCHAN + c->num, UID_ROOT, GID_WHEEL, 0644, dnmt, b->num*NCHAN + c->num);
		d->devt[1] = make_dev (&cx_cdevsw, b->num*NCHAN + c->num + 64, UID_ROOT, GID_WHEEL, 0600, "cx%d", b->num*NCHAN + c->num);
		d->devt[2] = make_dev (&cx_cdevsw, b->num*NCHAN + c->num + 128, UID_ROOT, GID_WHEEL, 0660, dnmc, b->num*NCHAN + c->num);
	}
	splx (s);

	return 0;
#else /* __FreeBSD_version < 400000 */
	}

	return 1;
#endif
}

#if __FreeBSD_version >= 400000
static int cx_detach (device_t dev)
{
	bdrv_t *bd = device_get_softc (dev);
	cx_board_t *b = bd->board;
	cx_chan_t *c;
	int s = splhigh ();
	
	/* Check if the device is busy (open). */
	for (c = b->chan; c < b->chan + NCHAN; ++c) {
		drv_t *d = (drv_t*) c->sys;

		if (!d || d->chan->type == T_NONE)
			continue;
		if (d->lock) {
			splx (s);
			return EBUSY;
		}
		if (c->mode == M_ASYNC && (d->tty.t_state & TS_ISOPEN) &&
		    (d->open_dev|0x2)) {
			splx (s);
			return EBUSY;
		}
		if (d->running) {
			splx (s);
			return EBUSY;
		}
	}

	/* Deactivate the timeout routine. And soft interrupt*/
	if (led_timo[b->num].callout)
		untimeout (cx_led_off, b, led_timo[b->num]);

	for (c = b->chan; c < b->chan + NCHAN; ++c) {
		drv_t *d = c->sys;

		if (!d || d->chan->type == T_NONE)
			continue;

		if (d->dtr_timeout_handle.callout)
			untimeout (cx_dtrwakeup, d, d->dtr_timeout_handle);
		if (d->dcd_timeout_handle.callout)
			untimeout (cx_carrier, c, d->dcd_timeout_handle);
	}
	bus_teardown_intr (dev, bd->irq_res, bd->intrhand);
	bus_deactivate_resource (dev, SYS_RES_IRQ, bd->irq_rid, bd->irq_res);
	bus_release_resource (dev, SYS_RES_IRQ, bd->irq_rid, bd->irq_res);
	
	bus_deactivate_resource (dev, SYS_RES_DRQ, bd->drq_rid, bd->drq_res);
	bus_release_resource (dev, SYS_RES_DRQ, bd->drq_rid, bd->drq_res);
	
	bus_deactivate_resource (dev, SYS_RES_IOPORT, bd->base_rid, bd->irq_res);
	bus_release_resource (dev, SYS_RES_IOPORT, bd->base_rid, bd->base_res);

	cx_close_board (b);

	/* Detach the interfaces, free buffer memory. */
	for (c = b->chan; c < b->chan + NCHAN; ++c) {
		drv_t *d = (drv_t*) c->sys;

		if (!d || d->chan->type == T_NONE)
			continue;
#ifdef NETGRAPH
#if __FreeBSD_version >= 500000
		if (d->node) {
			ng_rmnode_self (d->node);
			NG_NODE_UNREF (d->node);
			d->node = NULL;
		}
		mtx_destroy (&d->lo_queue.ifq_mtx);
		mtx_destroy (&d->hi_queue.ifq_mtx);
#else
		ng_rmnode (d->node);
		d->node = NULL;
#endif		
#else
#if __FreeBSD_version >= 410000 && NBPFILTER > 0
		/* Detach from the packet filter list of interfaces. */
		bpfdetach (&d->pp.pp_if);
#endif
		/* Detach from the sync PPP list. */
		sppp_detach (&d->pp.pp_if);

		if_detach (&d->pp.pp_if);
#endif		
		destroy_dev (d->devt[0]);
		destroy_dev (d->devt[1]);
		destroy_dev (d->devt[2]);
	}

	cx_led_off (b);
	if (led_timo[b->num].callout)
		untimeout (cx_led_off, b, led_timo[b->num]);
	splx (s);
	
	s = splhigh ();
	for (c = b->chan; c < b->chan + NCHAN; ++c) {
		drv_t *d = (drv_t*) c->sys;

		if (!d || d->chan->type == T_NONE)
			continue;
		
		/* Deallocate buffers. */
		contigfree (d, sizeof(d), M_DEVBUF);
	}
	bd->board = 0;
	adapter [b->num] = 0;
	free (b, M_DEVBUF);
	splx (s);
	
	return 0;	
}
#endif

#ifndef NETGRAPH
static void cx_ifstart (struct ifnet *ifp)
{
        drv_t *d = ifp->if_softc;

	cx_start (d);
}

static void cx_ifwatchdog (struct ifnet *ifp)
{
        drv_t *d = ifp->if_softc;

	cx_watchdog (d);
}

static void cx_tlf (struct sppp *sp)
{
	drv_t *d = sp->pp_if.if_softc;

	CX_DEBUG (d, ("cx_tlf\n"));
/*	cx_set_dtr (d->chan, 0);*/
/*	cx_set_rts (d->chan, 0);*/
	sp->pp_down (sp);
}

static void cx_tls (struct sppp *sp)
{
	drv_t *d = sp->pp_if.if_softc;

	CX_DEBUG (d, ("cx_tls\n"));
	sp->pp_up (sp);
}

/*
 * Initialization of interface.
 * It seems to be never called by upper level.
 */
static void cx_initialize (void *softc)
{
	drv_t *d = softc;

	CX_DEBUG (d, ("cx_initialize\n"));
}

/*
 * Process an ioctl request.
 */
static int cx_sioctl (struct ifnet *ifp, u_long cmd, caddr_t data)
{
	drv_t *d = ifp->if_softc;
	int error, s, was_up, should_be_up;

	/* No socket ioctls while the channel is in async mode. */
	if (d->chan->type == T_NONE || d->chan->mode == M_ASYNC)
		return EBUSY;

	/* Socket ioctls on slave subchannels are not allowed. */
	was_up = (ifp->if_flags & IFF_RUNNING) != 0;
	error = sppp_ioctl (ifp, cmd, data);
	if (error)
		return error;

	if (! (ifp->if_flags & IFF_DEBUG))
		d->chan->debug = 0;
	else if (! d->chan->debug)
		d->chan->debug = 1;

	switch (cmd) {
	default:           CX_DEBUG2 (d, ("ioctl 0x%lx\n", cmd)); return 0;
	case SIOCADDMULTI: CX_DEBUG2 (d, ("SIOCADDMULTI\n"));     return 0;
	case SIOCDELMULTI: CX_DEBUG2 (d, ("SIOCDELMULTI\n"));     return 0;
	case SIOCSIFFLAGS: CX_DEBUG2 (d, ("SIOCSIFFLAGS\n"));     break;
	case SIOCSIFADDR:  CX_DEBUG2 (d, ("SIOCSIFADDR\n"));      break;
	}

	/* We get here only in case of SIFFLAGS or SIFADDR. */
	s = splhigh ();
	should_be_up = (ifp->if_flags & IFF_RUNNING) != 0;
	if (!was_up && should_be_up) {
		/* Interface goes up -- start it. */
		cx_up (d);
		cx_start (d);
	} else if (was_up && !should_be_up) {
		/* Interface is going down -- stop it. */
		/* if ((d->pp.pp_flags & PP_FR) || (ifp->if_flags & PP_CISCO))*/
		cx_down (d);
	}
	splx (s);
	return 0;
}
#endif /*NETGRAPH*/

/*
 * Stop the interface.  Called on splimp().
 */
static void cx_down (drv_t *d)
{
	int s = splhigh ();
	CX_DEBUG (d, ("cx_down\n"));
	cx_set_dtr (d->chan, 0);
	cx_set_rts (d->chan, 0);
	d->running = 0;
	splx (s);
}

/*
 * Start the interface.  Called on splimp().
 */
static void cx_up (drv_t *d)
{
	int s = splhigh ();
	CX_DEBUG (d, ("cx_up\n"));
	cx_set_dtr (d->chan, 1);
	cx_set_rts (d->chan, 1);
	d->running = 1;
	splx (s);
}

/*
 * Start output on the (slave) interface.  Get another datagram to send
 * off of the interface queue, and copy it to the interface
 * before starting the output.
 */
static void cx_send (drv_t *d)
{
	struct mbuf *m;
	u_short len;

	CX_DEBUG2 (d, ("cx_send\n"));

	/* No output if the interface is down. */
	if (! d->running)
		return;

	/* No output if the modem is off. */
	if (! cx_get_dsr (d->chan) && ! cx_get_loop(d->chan))
		return;

	if (cx_buf_free (d->chan)) {
		/* Get the packet to send. */
#ifdef NETGRAPH
		IF_DEQUEUE (&d->hi_queue, m);
		if (! m)
			IF_DEQUEUE (&d->lo_queue, m);
#else
		m = sppp_dequeue (&d->pp.pp_if);
#endif
		if (! m)
			return;
#if (__FreeBSD_version >= 400000 || NBPFILTER > 0) && !defined (NETGRAPH)
		if (d->pp.pp_if.if_bpf)
#if __FreeBSD_version >= 500000
			BPF_MTAP (&d->pp.pp_if, m);
#else
			bpf_mtap (&d->pp.pp_if, m);
#endif
#endif
		len = m->m_pkthdr.len;
		if (! m->m_next)
			cx_send_packet (d->chan, (u_char*)mtod (m, caddr_t),
				len, 0);
		else {
			u_char buf [DMABUFSZ];
			m_copydata (m, 0, len, buf);
			cx_send_packet (d->chan, buf, len, 0);
		}
		m_freem (m);

		/* Set up transmit timeout, 10 seconds. */
#ifdef NETGRAPH
		d->timeout = 10;
#else
		d->pp.pp_if.if_timer = 10;
#endif
	}
#ifndef NETGRAPH
	d->pp.pp_if.if_flags |= IFF_OACTIVE;
#endif
}

/*
 * Start output on the interface.
 * Always called on splimp().
 */
static void cx_start (drv_t *d)
{
	int s = splhigh ();
	if (d->running) {
		if (! d->chan->dtr)
			cx_set_dtr (d->chan, 1);
		if (! d->chan->rts)
			cx_set_rts (d->chan, 1);
		cx_send (d);
	}
	splx (s);
}

/*
 * Handle transmit timeouts.
 * Recover after lost transmit interrupts.
 * Always called on splimp().
 */
static void cx_watchdog (drv_t *d)
{
	int s = splhigh ();
	CX_DEBUG (d, ("device timeout\n"));
	if (d->running) {
		cx_setup_chan (d->chan);
		cx_start_chan (d->chan, 0, 0);
		cx_set_dtr (d->chan, 1);
		cx_set_rts (d->chan, 1);
		cx_start (d);
	}
	splx (s);
}

/*
 * Transmit callback function.
 */
static void cx_transmit (cx_chan_t *c, void *attachment, int len)
{
	drv_t *d = c->sys;

	if (!d)
		return;
		
	if (c->mode == M_ASYNC) {
		d->tty.t_state &= ~(TS_BUSY | TS_FLUSH);
		d->atimeout = 0;
		if (d->tty.t_dev) {
			d->intr_action |= CX_WRITE;
			MY_SOFT_INTR = 1;
#if __FreeBSD_version >= 500000
			swi_sched (cx_fast_ih, 0);
#else
			setsofttty ();
#endif
		}
		return;
	}
#ifdef NETGRAPH
	d->timeout = 0;
#else
	++d->pp.pp_if.if_opackets;
	d->pp.pp_if.if_flags &= ~IFF_OACTIVE;
	d->pp.pp_if.if_timer = 0;
#endif
	cx_start (d);
}

/*
 * Process the received packet.
 */
static void cx_receive (cx_chan_t *c, char *data, int len)
{
	drv_t *d = c->sys;
	struct mbuf *m;
	char *cc = data;
#if __FreeBSD_version >= 500000 && defined NETGRAPH
	int error;
#endif

	if (!d)
		return;
		
	if (c->mode == M_ASYNC) {
		if (d->tty.t_state & TS_ISOPEN) {
			async_q *q = &d->aqueue;
			int size = BF_SZ - 1 - AQ_GSZ (q);

			if (len <= 0 && !size)
				return;

			if (len > size) {
			        c->ierrs++;
				cx_error (c, CX_OVERRUN);
				len = size - 1;
			}

			while (len--) {
				AQ_PUSH (q, *(unsigned char *)cc);
				cc++;
			}

			d->intr_action |= CX_READ;
			MY_SOFT_INTR = 1;
#if __FreeBSD_version >= 500000
			swi_sched (cx_fast_ih, 0);
#else
			setsofttty ();
#endif
		}
		return;
	}
	if (! d->running)
		return;

	m = makembuf (data, len);
	if (! m) {
		CX_DEBUG (d, ("no memory for packet\n"));
#ifndef NETGRAPH
		++d->pp.pp_if.if_iqdrops;
#endif
		return;
	}
	if (c->debug > 1)
		printmbuf (m);
#ifdef NETGRAPH
	m->m_pkthdr.rcvif = 0;
#if __FreeBSD_version >= 500000
	NG_SEND_DATA_ONLY (error, d->hook, m);
#else
	ng_queue_data (d->hook, m, 0);
#endif
#else
	++d->pp.pp_if.if_ipackets;
	m->m_pkthdr.rcvif = &d->pp.pp_if;
#if __FreeBSD_version >= 400000 || NBPFILTER > 0
	/* Check if there's a BPF listener on this interface.
	 * If so, hand off the raw packet to bpf. */
	if (d->pp.pp_if.if_bpf)
#if __FreeBSD_version >= 500000
		BPF_TAP (&d->pp.pp_if, data, len);
#else
		bpf_tap (&d->pp.pp_if, data, len);
#endif
#endif
	sppp_input (&d->pp.pp_if, m);
#endif
}

#define CONDITION(t,tp) (!(t->c_iflag & (ICRNL | IGNCR | IMAXBEL | INLCR | ISTRIP | IXON))\
	    && (!(tp->t_iflag & BRKINT) || (tp->t_iflag & IGNBRK))\
	    && (!(tp->t_iflag & PARMRK)\
		|| (tp->t_iflag & (IGNPAR | IGNBRK)) == (IGNPAR | IGNBRK))\
	    && !(t->c_lflag & (ECHO | ICANON | IEXTEN | ISIG | PENDIN))\
	    && linesw[tp->t_line].l_rint == ttyinput)

/*
 * Error callback function.
 */
static void cx_error (cx_chan_t *c, int data)
{
	drv_t *d = c->sys;
	async_q *q;

	if (!d)
		return;

	q = &(d->aqueue);

	switch (data) {
	case CX_FRAME:
		CX_DEBUG (d, ("frame error\n"));
		if (c->mode == M_ASYNC && (d->tty.t_state & TS_ISOPEN)
			&& (AQ_GSZ (q) < BF_SZ - 1)
			&& (!CONDITION((&d->tty.t_termios), (&d->tty))
			|| !(d->tty.t_iflag & (IGNPAR | PARMRK)))) {
			AQ_PUSH (q, TTY_FE);
			d->intr_action |= CX_READ;
			MY_SOFT_INTR = 1;
#if __FreeBSD_version >= 500000
			swi_sched (cx_fast_ih, 0);
#else
			setsofttty ();
#endif
		}
#ifndef NETGRAPH
		else
			++d->pp.pp_if.if_ierrors;
#endif
		break;
	case CX_CRC:
		CX_DEBUG (d, ("crc error\n"));
		if (c->mode == M_ASYNC && (d->tty.t_state & TS_ISOPEN)
			&& (AQ_GSZ (q) < BF_SZ - 1)
			&& (!CONDITION((&d->tty.t_termios), (&d->tty))
			|| !(d->tty.t_iflag & INPCK)
			|| !(d->tty.t_iflag & (IGNPAR | PARMRK)))) {
			AQ_PUSH (q, TTY_PE);
			d->intr_action |= CX_READ;
			MY_SOFT_INTR = 1;
#if __FreeBSD_version >= 500000
			swi_sched (cx_fast_ih, 0);
#else
			setsofttty ();
#endif
		}
#ifndef NETGRAPH
		else
			++d->pp.pp_if.if_ierrors;
#endif
		break;
	case CX_OVERRUN:
		CX_DEBUG (d, ("overrun error\n"));
#ifdef TTY_OE
		if (c->mode == M_ASYNC && (d->tty.t_state & TS_ISOPEN)
			&& (AQ_GSZ (q) < BF_SZ - 1)
			&& (!CONDITION((&d->tty.t_termios), (&d->tty)))) {
			AQ_PUSH (q, TTY_OE);
			d->intr_action |= CX_READ;
			MY_SOFT_INTR = 1;
#if __FreeBSD_version >= 500000
			swi_sched (cx_fast_ih, 0);
#else
			setsofttty ();
#endif
		}
#endif
#ifndef NETGRAPH
		else {
			++d->pp.pp_if.if_collisions;
			++d->pp.pp_if.if_ierrors;
		}
#endif
		break;
	case CX_OVERFLOW:
		CX_DEBUG (d, ("overflow error\n"));
#ifndef NETGRAPH
		if (c->mode != M_ASYNC)
			++d->pp.pp_if.if_ierrors;
#endif
		break;
	case CX_UNDERRUN:
		CX_DEBUG (d, ("underrun error\n"));
		if (c->mode != M_ASYNC) {
#ifdef NETGRAPH
			d->timeout = 0;
#else
			++d->pp.pp_if.if_oerrors;
			d->pp.pp_if.if_flags &= ~IFF_OACTIVE;
			d->pp.pp_if.if_timer = 0;
			cx_start (d);
#endif
		}
		break;
	case CX_BREAK:
		CX_DEBUG (d, ("break error\n"));
		if (c->mode == M_ASYNC && (d->tty.t_state & TS_ISOPEN)
			&& (AQ_GSZ (q) < BF_SZ - 1)
			&& (!CONDITION((&d->tty.t_termios), (&d->tty))
			|| !(d->tty.t_iflag & (IGNBRK | BRKINT | PARMRK)))) {
			AQ_PUSH (q, TTY_BI);
			d->intr_action |= CX_READ;
			MY_SOFT_INTR = 1;
#if __FreeBSD_version >= 500000
			swi_sched (cx_fast_ih, 0);
#else
			setsofttty ();
#endif
		}
#ifndef NETGRAPH
		else
			++d->pp.pp_if.if_ierrors;
#endif
		break;
	default:
		CX_DEBUG (d, ("error #%d\n", data));
	}
}

#if __FreeBSD_version < 500000
static int cx_open (dev_t dev, int flag, int mode, struct proc *p)
#else
static int cx_open (dev_t dev, int flag, int mode, struct thread *td)
#endif
{
	int unit = UNIT (dev);
	drv_t *d;
	int error;

	if (unit >= NCX*NCHAN || ! (d = channel[unit]))
		return ENXIO;
	CX_DEBUG2 (d, ("cx_open unit=%d, flag=0x%x, mode=0x%x\n",
		    unit, flag, mode));

	if (d->chan->mode != M_ASYNC || IF_CUNIT(dev)) {
		d->open_dev |= 0x1;
		return 0;
	}
#if __FreeBSD_version >= 400000
	dev->si_tty = &d->tty;
#endif
	d->tty.t_dev = dev;
again:
	if (d->dtroff) {
		error = tsleep (&d->dtrwait, TTIPRI | PCATCH, "cxdtr", 0);
		if (error)
			return error;
		goto again;
	}

	if ((d->tty.t_state & TS_ISOPEN) && (d->tty.t_state & TS_XCLUDE) &&
#if __FreeBSD_version >= 500000
		suser (td))
#else
	    p->p_ucred->cr_uid != 0)
#endif
		return EBUSY;

	if (d->tty.t_state & TS_ISOPEN) {
		/*
		 * Cannot open /dev/cua if /dev/tty already opened.
		 */
		if (CALLOUT (dev) && ! d->callout)
			return EBUSY;

		/*
		 * Opening /dev/tty when /dev/cua is already opened.
		 * Wait for close, then try again.
		 */
		if (! CALLOUT (dev) && d->callout) {
			if (flag & O_NONBLOCK)
				return EBUSY;
			error = tsleep (d, TTIPRI | PCATCH, "cxbi", 0);
			if (error)
				return error;
			goto again;
		}
	} else if (d->lock && ! CALLOUT (dev) && (flag & O_NONBLOCK))
		/*
		 * We try to open /dev/tty in non-blocking mode
		 * while somebody is already waiting for carrier on it.
		 */
		return EBUSY;
	else {
		ttychars (&d->tty);
		if (d->tty.t_ispeed == 0) {
			d->tty.t_iflag = 0;
			d->tty.t_oflag = 0;
			d->tty.t_lflag = 0;
			d->tty.t_cflag = CREAD | CS8 | HUPCL;
			d->tty.t_ispeed = d->chan->rxbaud;
			d->tty.t_ospeed = d->chan->txbaud;
		}
		if (CALLOUT (dev))
			d->tty.t_cflag |= CLOCAL;
		else
			d->tty.t_cflag &= ~CLOCAL;
		cx_param (&d->tty, &d->tty.t_termios);
		ttsetwater (&d->tty);
	}

	splhigh ();
	if (! (d->tty.t_state & TS_ISOPEN)) {
		cx_start_chan (d->chan, 0, 0);
		cx_set_dtr (d->chan, 1);
		cx_set_rts (d->chan, 1);
		d->cd = cx_get_cd (d->chan);
		if (CALLOUT (dev) || cx_get_cd (d->chan))
			(*linesw[d->tty.t_line].l_modem) (&d->tty, 1);
	}

	if (! (flag & O_NONBLOCK) && ! (d->tty.t_cflag & CLOCAL) &&
	    ! (d->tty.t_state & TS_CARR_ON)) {
		/* Lock the channel against cxconfig while we are
		 * waiting for carrier. */
		d->lock++;
		error = tsleep (&d->tty.t_rawq, TTIPRI | PCATCH, "cxdcd", 0);
		/* Unlock the channel. */
		d->lock--;
		spl0 ();
		if (error)
			goto failed;
		goto again;
	}

	error = (*linesw[d->tty.t_line].l_open) (dev, &d->tty);
	disc_optim (&d->tty, &d->tty.t_termios);
	spl0 ();
	if (error) {
failed:         if (! (d->tty.t_state & TS_ISOPEN)) {
			splhigh ();
			cx_set_dtr (d->chan, 0);
			cx_set_rts (d->chan, 0);
			if (d->dtrwait) {
				d->dtr_timeout_handle =
				    timeout (cx_dtrwakeup, d, d->dtrwait);
				d->dtroff = 1;
			}
			spl0 ();
		}
		return error;
	}

	if (d->tty.t_state & TS_ISOPEN)
		d->callout = CALLOUT (dev) ? 1 : 0;

	d->open_dev |= 0x2;
	CX_DEBUG2 (d, ("cx_open done\n"));
	return 0;
}

#if __FreeBSD_version < 500000
static int cx_close (dev_t dev, int flag, int mode, struct proc *p)
#else
static int cx_close (dev_t dev, int flag, int mode, struct thread *td)
#endif
{
	drv_t *d = channel [UNIT (dev)];
	int s;

	CX_DEBUG2 (d, ("cx_close\n"));
	if ((!(d->open_dev&0x2)) || IF_CUNIT(dev)){
		d->open_dev &= ~0x1;
		return 0;
	}
	s = splhigh ();
	(*linesw[d->tty.t_line].l_close) (&d->tty, flag);
	disc_optim (&d->tty, &d->tty.t_termios);

	/* Disable receiver.
	 * Transmitter continues sending the queued data. */
	cx_enable_receive (d->chan, 0);

	/* Clear DTR and RTS. */
	if ((d->tty.t_cflag & HUPCL) || ! (d->tty.t_state & TS_ISOPEN)) {
		cx_set_dtr (d->chan, 0);
		cx_set_rts (d->chan, 0);
		if (d->dtrwait) {
			d->dtr_timeout_handle =
			    timeout (cx_dtrwakeup, d, d->dtrwait);
			d->dtroff = 1;
		}
	}
	ttyclose (&d->tty);
	splx (s);
	d->callout = 0;

	/* Wake up bidirectional opens. */
	wakeup (d);
	d->open_dev &= ~0x2;

	return 0;
}

static int cx_read (dev_t dev, struct uio *uio, int flag)
{
	drv_t *d = channel [UNIT (dev)];

	if (d)	CX_DEBUG2 (d, ("cx_read\n"));
	if (!d || d->chan->mode != M_ASYNC || IF_CUNIT(dev))
		return EBADF;

	return (*linesw[d->tty.t_line].l_read) (&d->tty, uio, flag);
}

static int cx_write (dev_t dev, struct uio *uio, int flag)
{
	drv_t *d = channel [UNIT (dev)];

	if (d) CX_DEBUG2 (d, ("cx_write\n"));
	if (!d || d->chan->mode != M_ASYNC || IF_CUNIT(dev))
		return EBADF;

	return (*linesw[d->tty.t_line].l_write) (&d->tty, uio, flag);
}

static int cx_modem_status (drv_t *d)
{
	int status = 0, s = splhigh ();
	/* Already opened by someone or network interface is up? */
	if ((d->chan->mode == M_ASYNC && (d->tty.t_state & TS_ISOPEN) &&
	    (d->open_dev|0x2)) || (d->chan->mode != M_ASYNC && d->running))
		status = TIOCM_LE;      /* always enabled while open */

	if (cx_get_dsr (d->chan)) status |= TIOCM_DSR;
	if (cx_get_cd  (d->chan)) status |= TIOCM_CD;
	if (cx_get_cts (d->chan)) status |= TIOCM_CTS;
	if (d->chan->dtr)	  status |= TIOCM_DTR;
	if (d->chan->rts)	  status |= TIOCM_RTS;
	splx (s);
	return status;
}

#if __FreeBSD_version < 500000
static int cx_ioctl (dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
#else
static int cx_ioctl (dev_t dev, u_long cmd, caddr_t data, int flag, struct thread *td)
#endif
{
	drv_t *d = channel [UNIT (dev)];
	cx_chan_t *c;
	struct serial_statistics *st;
	int error, s;
	char mask[16];

	if (!d || !(c = d->chan))
		return EINVAL;
		
	switch (cmd) {
	case SERIAL_GETREGISTERED:
	        CX_DEBUG2 (d, ("ioctl: getregistered\n"));
		bzero (mask, sizeof(mask));
		for (s=0; s<NCX*NCHAN; ++s)
			if (channel [s])
				mask [s/8] |= 1 << (s & 7);
	        bcopy (mask, data, sizeof (mask));
		return 0;

	case SERIAL_GETPORT:
	        CX_DEBUG2 (d, ("ioctl: getport\n"));
		s = splhigh ();
	        *(int *)data = cx_get_port (c);
		splx (s);
		if (*(int *)data<0)
			return (EINVAL);
		else
			return 0;

	case SERIAL_SETPORT:
	        CX_DEBUG2 (d, ("ioctl: setproto\n"));
	        /* Only for superuser! */
#if __FreeBSD_version < 400000
	        error = suser (p->p_ucred, &p->p_acflag);
#elif __FreeBSD_version < 500000
	        error = suser (p);
#else /* __FreeBSD_version >= 500000 */
	        error = suser (td);
#endif /* __FreeBSD_version >= 500000 */
	        if (error)
	                return error;

		s = splhigh ();
		cx_set_port (c, *(int *)data);
		splx (s);
	        return 0;

#ifndef NETGRAPH
	case SERIAL_GETPROTO:
	        CX_DEBUG2 (d, ("ioctl: getproto\n"));
		s = splhigh ();
	        strcpy ((char*)data, (c->mode == M_ASYNC) ? "async" :
			/*(d->pp.pp_flags & PP_FR) ? "fr" :*/
			(d->pp.pp_if.if_flags & PP_CISCO) ? "cisco" : "ppp");
		splx (s);
	        return 0;

	case SERIAL_SETPROTO:
	        CX_DEBUG2 (d, ("ioctl: setproto\n"));
	        /* Only for superuser! */
#if __FreeBSD_version < 400000
	        error = suser (p->p_ucred, &p->p_acflag);
#elif __FreeBSD_version < 500000
	        error = suser (p);
#else /* __FreeBSD_version >= 500000 */
	        error = suser (td);
#endif /* __FreeBSD_version >= 500000 */
	        if (error)
	                return error;
	        if (c->mode == M_ASYNC)
	                return EBUSY;
		if (d->pp.pp_if.if_flags & IFF_RUNNING)
			return EBUSY;
	        if (! strcmp ("cisco", (char*)data)) {
/*	                d->pp.pp_flags &= ~(PP_FR);*/
	                d->pp.pp_flags |= PP_KEEPALIVE;
	                d->pp.pp_if.if_flags |= PP_CISCO;
/*	        } else if (! strcmp ("fr", (char*)data)) {*/
/*	                d->pp.pp_if.if_flags &= ~(PP_CISCO);*/
/*	                d->pp.pp_flags |= PP_FR | PP_KEEPALIVE;*/
	        } else if (! strcmp ("ppp", (char*)data)) {
	                d->pp.pp_flags &= ~(/*PP_FR |*/ PP_KEEPALIVE);
	                d->pp.pp_if.if_flags &= ~(PP_CISCO);
	        } else
			return EINVAL;
	        return 0;

	case SERIAL_GETKEEPALIVE:
	        CX_DEBUG2 (d, ("ioctl: getkeepalive\n"));
	        if (/*(d->pp.pp_flags & PP_FR) ||*/
		    (d->pp.pp_if.if_flags & PP_CISCO) ||
		    (c->mode == M_ASYNC))
			return EINVAL;
		s = splhigh ();
	        *(int*)data = (d->pp.pp_flags & PP_KEEPALIVE) ? 1 : 0;
		splx (s);
	        return 0;

	case SERIAL_SETKEEPALIVE:
	        CX_DEBUG2 (d, ("ioctl: setkeepalive\n"));
	        /* Only for superuser! */
#if __FreeBSD_version < 400000
	        error = suser (p->p_ucred, &p->p_acflag);
#elif __FreeBSD_version < 500000
	        error = suser (p);
#else /* __FreeBSD_version >= 500000 */
	        error = suser (td);
#endif /* __FreeBSD_version >= 500000 */
	        if (error)
	                return error;
	        if (/*(d->pp.pp_flags & PP_FR) ||*/
			(d->pp.pp_if.if_flags & PP_CISCO))
			return EINVAL;
		s = splhigh ();
	        if (*(int*)data)
	                d->pp.pp_flags |= PP_KEEPALIVE;
		else
	                d->pp.pp_flags &= ~PP_KEEPALIVE;
		splx (s);
	        return 0;
#endif /*NETGRAPH*/

	case SERIAL_GETMODE:
	        CX_DEBUG2 (d, ("ioctl: getmode\n"));
		s = splhigh ();
		*(int*)data = (c->mode == M_ASYNC) ?
			SERIAL_ASYNC : SERIAL_HDLC;
		splx (s);
	        return 0;

	case SERIAL_SETMODE:
	        CX_DEBUG2 (d, ("ioctl: setmode\n"));
	        /* Only for superuser! */
#if __FreeBSD_version < 400000
	        error = suser (p->p_ucred, &p->p_acflag);
#elif __FreeBSD_version < 500000
	        error = suser (p);
#else /* __FreeBSD_version >= 500000 */
	        error = suser (td);
#endif /* __FreeBSD_version >= 500000 */
	        if (error)
	                return error;

	        /* Somebody is waiting for carrier? */
	        if (d->lock)
	                return EBUSY;
	        /* /dev/ttyXX is already opened by someone? */
	        if (c->mode == M_ASYNC && (d->tty.t_state & TS_ISOPEN) &&
		    (d->open_dev|0x2))
	                return EBUSY;
	        /* Network interface is up?
	         * Cannot change to async mode. */
	        if (c->mode != M_ASYNC && d->running &&
	            (*(int*)data == SERIAL_ASYNC))
	                return EBUSY;

		s = splhigh ();
		if (c->mode == M_HDLC && *(int*)data == SERIAL_ASYNC) {
			cx_set_mode (c, M_ASYNC);
			cx_enable_receive (c, 0);
			cx_enable_transmit (c, 0);
		} else if (c->mode == M_ASYNC && *(int*)data == SERIAL_HDLC) {
			cx_set_mode (c, M_HDLC);
			cx_enable_receive (c, 1);
			cx_enable_transmit (c, 1);
		}
	        splx (s);
		return 0;

	case SERIAL_GETSTAT:
	        CX_DEBUG2 (d, ("ioctl: getestat\n"));
	        st = (struct serial_statistics*) data;
		s = splhigh ();
	        st->rintr  = c->rintr;
	        st->tintr  = c->tintr;
	        st->mintr  = c->mintr;
	        st->ibytes = c->ibytes;
	        st->ipkts  = c->ipkts;
	        st->ierrs  = c->ierrs;
	        st->obytes = c->obytes;
	        st->opkts  = c->opkts;
	        st->oerrs  = c->oerrs;
		splx (s);
	        return 0;

	case SERIAL_CLRSTAT:
	        CX_DEBUG2 (d, ("ioctl: clrstat\n"));
	        /* Only for superuser! */
#if __FreeBSD_version < 400000
	        error = suser (p->p_ucred, &p->p_acflag);
#elif __FreeBSD_version < 500000
	        error = suser (p);
#else /* __FreeBSD_version >= 500000 */
	        error = suser (td);
#endif /* __FreeBSD_version >= 500000 */
	        if (error)
	                return error;
		s = splhigh ();
		c->rintr = 0;
	        c->tintr = 0;
	        c->mintr = 0;
	        c->ibytes = 0;
	        c->ipkts = 0;
	        c->ierrs = 0;
	        c->obytes = 0;
	        c->opkts = 0;
	        c->oerrs = 0;
		splx (s);
	        return 0;

	case SERIAL_GETBAUD:
	        CX_DEBUG2 (d, ("ioctl: getbaud\n"));
	        if (c->mode == M_ASYNC)
	                return EINVAL;
		s = splhigh ();
	        *(long*)data = cx_get_baud(c);
		splx (s);
	        return 0;

	case SERIAL_SETBAUD:
	        CX_DEBUG2 (d, ("ioctl: setbaud\n"));
	        /* Only for superuser! */
#if __FreeBSD_version < 400000
	        error = suser (p->p_ucred, &p->p_acflag);
#elif __FreeBSD_version < 500000
	        error = suser (p);
#else /* __FreeBSD_version >= 500000 */
	        error = suser (td);
#endif /* __FreeBSD_version >= 500000 */
	        if (error)
	                return error;
	        if (c->mode == M_ASYNC)
	                return EINVAL;
		s = splhigh ();
	        cx_set_baud (c, *(long*)data);
	        splx (s);
	        return 0;

	case SERIAL_GETLOOP:
	        CX_DEBUG2 (d, ("ioctl: getloop\n"));
	        if (c->mode == M_ASYNC)
	                return EINVAL;
		s = splhigh ();
	        *(int*)data = cx_get_loop (c);
		splx (s);
	        return 0;

	case SERIAL_SETLOOP:
	        CX_DEBUG2 (d, ("ioctl: setloop\n"));
	        /* Only for superuser! */
#if __FreeBSD_version < 400000
	        error = suser (p->p_ucred, &p->p_acflag);
#elif __FreeBSD_version < 500000
	        error = suser (p);
#else /* __FreeBSD_version >= 500000 */
	        error = suser (td);
#endif /* __FreeBSD_version >= 500000 */
	        if (error)
	                return error;
	        if (c->mode == M_ASYNC)
	                return EINVAL;
		s = splhigh ();
	        cx_set_loop (c, *(int*)data);
	        splx (s);
	        return 0;

	case SERIAL_GETDPLL:
	        CX_DEBUG2 (d, ("ioctl: getdpll\n"));
	        if (c->mode == M_ASYNC)
	                return EINVAL;
		s = splhigh ();
	        *(int*)data = cx_get_dpll (c);
		splx (s);
	        return 0;

	case SERIAL_SETDPLL:
	        CX_DEBUG2 (d, ("ioctl: setdpll\n"));
	        /* Only for superuser! */
#if __FreeBSD_version < 400000
	        error = suser (p->p_ucred, &p->p_acflag);
#elif __FreeBSD_version < 500000
	        error = suser (p);
#else /* __FreeBSD_version >= 500000 */
	        error = suser (td);
#endif /* __FreeBSD_version >= 500000 */
	        if (error)
	                return error;
	        if (c->mode == M_ASYNC)
	                return EINVAL;
		s = splhigh ();
	        cx_set_dpll (c, *(int*)data);
	        splx (s);
	        return 0;

	case SERIAL_GETNRZI:
	        CX_DEBUG2 (d, ("ioctl: getnrzi\n"));
	        if (c->mode == M_ASYNC)
	                return EINVAL;
		s = splhigh ();
	        *(int*)data = cx_get_nrzi (c);
		splx (s);
	        return 0;

	case SERIAL_SETNRZI:
	        CX_DEBUG2 (d, ("ioctl: setnrzi\n"));
	        /* Only for superuser! */
#if __FreeBSD_version < 400000
	        error = suser (p->p_ucred, &p->p_acflag);
#elif __FreeBSD_version < 500000
	        error = suser (p);
#else /* __FreeBSD_version >= 500000 */
	        error = suser (td);
#endif /* __FreeBSD_version >= 500000 */
	        if (error)
	                return error;
	        if (c->mode == M_ASYNC)
	                return EINVAL;
		s = splhigh ();
	        cx_set_nrzi (c, *(int*)data);
	        splx (s);
	        return 0;

	case SERIAL_GETDEBUG:
	        CX_DEBUG2 (d, ("ioctl: getdebug\n"));
		s = splhigh ();
	        *(int*)data = c->debug;
		splx (s);
	        return 0;

	case SERIAL_SETDEBUG:
	        CX_DEBUG2 (d, ("ioctl: setdebug\n"));
	        /* Only for superuser! */
#if __FreeBSD_version < 400000
	        error = suser (p->p_ucred, &p->p_acflag);
#elif __FreeBSD_version < 500000
	        error = suser (p);
#else /* __FreeBSD_version >= 500000 */
	        error = suser (td);
#endif /* __FreeBSD_version >= 500000 */
	        if (error)
	                return error;
		s = splhigh ();
	        c->debug = *(int*)data;
		splx (s);
#ifndef	NETGRAPH
		if (d->chan->debug)
			d->pp.pp_if.if_flags |= IFF_DEBUG;
		else
			d->pp.pp_if.if_flags &= (~IFF_DEBUG);
#endif
	        return 0;
	}

	if (c->mode == M_ASYNC) {
#if __FreeBSD_version >= 500000
		error = (*linesw[d->tty.t_line].l_ioctl) (&d->tty, cmd, data, flag, td);
#else
		error = (*linesw[d->tty.t_line].l_ioctl) (&d->tty, cmd, data, flag, p);
#endif
		disc_optim (&d->tty, &d->tty.t_termios);
		if (error != ENOIOCTL) {
			if (error)
			CX_DEBUG2 (d, ("l_ioctl: 0x%lx, error %d\n", cmd, error));
			return error;
		}
		error = ttioctl (&d->tty, cmd, data, flag);
		disc_optim (&d->tty, &d->tty.t_termios);
		if (error != ENOIOCTL) {
			if (error)
			CX_DEBUG2 (d, ("ttioctl: 0x%lx, error %d\n", cmd, error));
			return error;
		}
	}

	switch (cmd) {
	case TIOCSBRK:          /* Start sending line break */
	        CX_DEBUG2 (d, ("ioctl: tiocsbrk\n"));
		s = splhigh ();
		cx_send_break (c, 500);
		splx (s);
	        return 0;

	case TIOCCBRK:          /* Stop sending line break */
	        CX_DEBUG2 (d, ("ioctl: tioccbrk\n"));
	        return 0;

	case TIOCSDTR:          /* Set DTR */
	        CX_DEBUG2 (d, ("ioctl: tiocsdtr\n"));
		s = splhigh ();
		cx_set_dtr (c, 1);
		splx (s);
	        return 0;

	case TIOCCDTR:          /* Clear DTR */
	        CX_DEBUG2 (d, ("ioctl: tioccdtr\n"));
		s = splhigh ();
		cx_set_dtr (c, 0);
		splx (s);
	        return 0;

	case TIOCMSET:          /* Set DTR/RTS */
	        CX_DEBUG2 (d, ("ioctl: tiocmset\n"));
		s = splhigh ();
		cx_set_dtr (c, (*(int*)data & TIOCM_DTR) ? 1 : 0);
		cx_set_rts (c, (*(int*)data & TIOCM_RTS) ? 1 : 0);
		splx (s);
	        return 0;

	case TIOCMBIS:          /* Add DTR/RTS */
	        CX_DEBUG2 (d, ("ioctl: tiocmbis\n"));
		s = splhigh ();
		if (*(int*)data & TIOCM_DTR) cx_set_dtr (c, 1);
		if (*(int*)data & TIOCM_RTS) cx_set_rts (c, 1);
		splx (s);
	        return 0;

	case TIOCMBIC:          /* Clear DTR/RTS */
	        CX_DEBUG2 (d, ("ioctl: tiocmbic\n"));
		s = splhigh ();
		if (*(int*)data & TIOCM_DTR) cx_set_dtr (c, 0);
		if (*(int*)data & TIOCM_RTS) cx_set_rts (c, 0);
		splx (s);
	        return 0;

	case TIOCMGET:          /* Get modem status */
	        CX_DEBUG2 (d, ("ioctl: tiocmget\n"));
		*(int*)data = cx_modem_status (d);
	        return 0;

#ifdef TIOCMSDTRWAIT
	case TIOCMSDTRWAIT:
	        CX_DEBUG2 (d, ("ioctl: tiocmsdtrwait\n"));
	        /* Only for superuser! */
#if __FreeBSD_version < 400000
	        error = suser (p->p_ucred, &p->p_acflag);
#elif __FreeBSD_version < 500000
	        error = suser (p);
#else /* __FreeBSD_version >= 500000 */
	        error = suser (td);
#endif /* __FreeBSD_version >= 500000 */
		if (error)
			return error;
		s = splhigh ();
		d->dtrwait = *(int*)data * hz / 100;
		splx (s);
	        return 0;
#endif

#ifdef TIOCMGDTRWAIT
	case TIOCMGDTRWAIT:
	        CX_DEBUG2 (d, ("ioctl: tiocmgdtrwait\n"));
		s = splhigh ();
		*(int*)data = d->dtrwait * 100 / hz;
		splx (s);
	        return 0;
#endif
	}
        CX_DEBUG2 (d, ("ioctl: 0x%lx\n", cmd));
	return ENOTTY;
}

/*
 * Wake up opens() waiting for DTR ready.
 */
static void cx_dtrwakeup (void *arg)
{
	drv_t *d = arg;

	d->dtroff = 0;
	wakeup (&d->dtrwait);
}


static void
disc_optim(tp, t)
	struct tty	*tp;
	struct termios	*t;
{
	if (CONDITION(t,tp))
		tp->t_state |= TS_CAN_BYPASS_L_RINT;
	else
		tp->t_state &= ~TS_CAN_BYPASS_L_RINT;
}

#if __FreeBSD_version >= 500000
void cx_softintr (void *unused)
#else
void cx_softintr ()
#endif
{
	drv_t *d;
	async_q *q;
	int i, s, ic, k;
	while (MY_SOFT_INTR) {
		MY_SOFT_INTR = 0;
		for (i=0; i<NCX*NCHAN; ++i) {
			d = channel [i];
			if (!d || !d->chan || d->chan->type == T_NONE
			    || d->chan->mode != M_ASYNC || !d->tty.t_dev)
				continue;
			s = splhigh ();
			if (d->intr_action & CX_READ) {
				q = &(d->aqueue);
				if (d->tty.t_state & TS_CAN_BYPASS_L_RINT) {
					k = AQ_GSZ(q);
					if (d->tty.t_rawq.c_cc + k >
						d->tty.t_ihiwat
					    && (d->tty.t_cflag & CRTS_IFLOW
						|| d->tty.t_iflag & IXOFF)
					    && !(d->tty.t_state & TS_TBLOCK))
						ttyblock(&d->tty);
					d->tty.t_rawcc += k;
					while (k>0) {
						k--;
						AQ_POP (q, ic);
						splx (s);
						putc (ic, &d->tty.t_rawq);
						s = splhigh ();
					}
					ttwakeup(&d->tty);
					if (d->tty.t_state & TS_TTSTOP
					    && (d->tty.t_iflag & IXANY
						|| d->tty.t_cc[VSTART] ==
						d->tty.t_cc[VSTOP])) {
						d->tty.t_state &= ~TS_TTSTOP;
						d->tty.t_lflag &= ~FLUSHO;
						d->intr_action |= CX_WRITE;
					}
				} else {
					while (q->end != q->beg) {
						AQ_POP (q, ic);
						splx (s);
						(*linesw[d->tty.t_line].l_rint)
							(ic, &d->tty);
						s = splhigh ();
					}
				}
				d->intr_action &= ~CX_READ;
			}
			splx (s);

			s = splhigh ();
			if (d->intr_action & CX_WRITE) {
				if (d->tty.t_line)
					(*linesw[d->tty.t_line].l_start) (&d->tty);
				else
					cx_oproc (&d->tty);
				d->intr_action &= ~CX_WRITE;
			}
			splx (s);

		}
	}
}

/*
 * Fill transmitter buffer with data.
 */
static void cx_oproc (struct tty *tp)
{
	int s = splhigh (), k;
	drv_t *d = channel [UNIT (tp->t_dev)];
	static u_char buf[DMABUFSZ];
	u_char *p;
	u_short len = 0, sublen = 0;

	if (!d) {
		splx (s);
		return;
	}
		
	CX_DEBUG2 (d, ("cx_oproc\n"));
	if (tp->t_cflag & CRTSCTS && (tp->t_state & TS_TBLOCK) && d->chan->rts)
		cx_set_rts (d->chan, 0);
	else if (tp->t_cflag & CRTSCTS && ! (tp->t_state & TS_TBLOCK) && ! d->chan->rts)
		cx_set_rts (d->chan, 1);

	if (! (tp->t_state & (TS_TIMEOUT | TS_TTSTOP))) {
		/* Start transmitter. */
		cx_enable_transmit (d->chan, 1);

		/* Is it busy? */
		if (! cx_buf_free (d->chan)) {
			tp->t_state |= TS_BUSY;
			splx (s);
			return;
		}
		if (tp->t_iflag & IXOFF) {
			p = (buf + (DMABUFSZ/2));
			sublen = q_to_b (&tp->t_outq, p, (DMABUFSZ/2));
			k = sublen;
			while (k--) {
				/* Send XON/XOFF out of band. */
				if (*p == tp->t_cc[VSTOP]) {
					cx_xflow_ctl (d->chan, 0);
					p++;
					continue;
				}
				if (*p == tp->t_cc[VSTART]) {
					cx_xflow_ctl (d->chan, 1);
					p++;
					continue;
				}
				buf[len] = *p;
				len++;
				p++;
			}
		} else {
			p = buf;
			len = q_to_b (&tp->t_outq, p, (DMABUFSZ/2));
		}
		if (len) {
			cx_send_packet (d->chan, buf, len, 0);
			tp->t_state |= TS_BUSY;
			d->atimeout = 10;
			CX_DEBUG2 (d, ("out %d bytes\n", len));
		}
	}
	ttwwakeup (tp);
	splx (s);
}

static int cx_param (struct tty *tp, struct termios *t)
{
	drv_t *d = channel [UNIT (tp->t_dev)];
	int s, bits, parity;

	if (!d)
		return EINVAL;
		
	s = splhigh ();
	if (t->c_ospeed == 0) {
		/* Clear DTR and RTS. */
		cx_set_dtr (d->chan, 0);
		splx (s);
		CX_DEBUG2 (d, ("cx_param (hangup)\n"));
		return 0;
	}
	CX_DEBUG2 (d, ("cx_param\n"));

	/* Check requested parameters. */
	if (t->c_ospeed < 300 || t->c_ospeed > 256*1024) {
		splx (s);
                return EINVAL;
	}
	if (t->c_ispeed && (t->c_ispeed < 300 || t->c_ispeed > 256*1024)) {
		splx (s);
                return EINVAL;
	}

  	/* And copy them to tty and channel structures. */
	tp->t_ispeed = t->c_ispeed = tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;

	/* Set character length and parity mode. */
	switch (t->c_cflag & CSIZE) {
	default:
	case CS8: bits = 8; break;
	case CS7: bits = 7; break;
	case CS6: bits = 6; break;
	case CS5: bits = 5; break;
	}

	parity = ((t->c_cflag & PARENB) ? 1 : 0) *
		 (1 + ((t->c_cflag & PARODD) ? 0 : 1));

	/* Set current channel number. */
	if (! d->chan->dtr)
		cx_set_dtr (d->chan, 1);

	disc_optim (&d->tty, &d->tty.t_termios);
	cx_set_async_param (d->chan, t->c_ospeed, bits, parity, (t->c_cflag & CSTOPB),
		!(t->c_cflag & PARENB), (t->c_cflag & CRTSCTS),
		(t->c_iflag & IXON), (t->c_iflag & IXANY),
		t->c_cc[VSTART], t->c_cc[VSTOP]);
	splx (s);
	return 0;
}

#if __FreeBSD_version < 400000
static struct tty *cx_devtotty (dev_t dev)
{
	int unit = UNIT (dev);

	if (unit == UNIT_CTL || unit >= NCX*NCHAN || ! channel[unit])
		return 0;
	return &channel[unit]->tty;
}
#endif

/*
 * Stop output on a line
 */
static void cx_stop (struct tty *tp, int flag)
{
	drv_t *d = channel [UNIT (tp->t_dev)];
	int s;

	if (!d)
		return;
		
	s = splhigh ();

	if (tp->t_state & TS_BUSY) {
		/* Stop transmitter */
		CX_DEBUG2 (d, ("cx_stop\n"));
		cx_transmitter_ctl (d->chan, 0);
	}
	splx (s);
}

/*
 * Process the (delayed) carrier signal setup.
 */
static void cx_carrier (void *arg)
{
	drv_t *d = arg;
	cx_chan_t *c = d->chan;
	int s, cd;

	s = splhigh ();
	cd = cx_get_cd (c);
	if (d->cd != cd) {
		if (cd) {
			CX_DEBUG (d, ("carrier on\n"));
			d->cd = 1;
			splx (s);
			(*linesw[d->tty.t_line].l_modem) (&d->tty, 1);
		} else {
			CX_DEBUG (d, ("carrier loss\n"));
			d->cd = 0;
			splx (s);
			(*linesw[d->tty.t_line].l_modem) (&d->tty, 0);
		}
	}
}

/*
 * Modem signal callback function.
 */
static void cx_modem (cx_chan_t *c)
{
	drv_t *d = c->sys;

	if (!d || c->mode != M_ASYNC)
		return;
	/* Handle carrier detect/loss. */
	untimeout (cx_carrier, c, d->dcd_timeout_handle);
	/* Carrier changed - delay processing DCD for a while
	 * to give both sides some time to initialize. */
	d->dcd_timeout_handle = timeout (cx_carrier, d, hz/2);
}

#if __FreeBSD_version < 400000
struct isa_driver cxdriver = { cx_probe, cx_attach, "cx" };
static struct cdevsw cx_cdevsw = {
	cx_open,	cx_close,	cx_read,	cx_write,
	cx_ioctl,	cx_stop,	noreset,	cx_devtotty,
	ttpoll,		nommap,		NULL,		"cx",
	NULL,		-1,
};
#elif  __FreeBSD_version < 500000
static struct cdevsw cx_cdevsw = {
	cx_open,	cx_close,	cx_read,	cx_write,
	cx_ioctl,	ttypoll,	nommap,		nostrategy,
	"cx",		CDEV_MAJOR,	nodump,		nopsize,
	D_TTY,		-1
};
#elif __FreeBSD_version == 500000
static struct cdevsw cx_cdevsw = {
	cx_open,	cx_close,	cx_read,	cx_write,
	cx_ioctl,	ttypoll,	nommap,		nostrategy,
	"cx",		CDEV_MAJOR,	nodump,		nopsize,
	D_TTY,
	};
#elif __FreeBSD_version <= 501000 
static struct cdevsw cx_cdevsw = {
	.d_open     = cx_open,
	.d_close    = cx_close,
	.d_read     = cx_read,
	.d_write    = cx_write,
	.d_ioctl    = cx_ioctl,
	.d_poll     = ttypoll,
	.d_mmap	    = nommap,
	.d_strategy = nostrategy,
	.d_name     = "cx",
	.d_maj      = CDEV_MAJOR,
	.d_dump     = nodump,
	.d_flags    = D_TTY,
};
#else /* __FreeBSD_version > 501000 */
static struct cdevsw cx_cdevsw = {
	.d_open     = cx_open,
	.d_close    = cx_close,
	.d_read     = cx_read,
	.d_write    = cx_write,
	.d_ioctl    = cx_ioctl,
	.d_poll     = ttypoll,
	.d_name     = "cx",
	.d_maj      = CDEV_MAJOR,
	.d_flags    = D_TTY,
};
#endif

#ifdef NETGRAPH
#if __FreeBSD_version >= 500000
static int ng_cx_constructor (node_p node)
{
	drv_t *d = NG_NODE_PRIVATE (node);
#else
static int ng_cx_constructor (node_p *node)
{
	drv_t *d = (*node)->private;
#endif
	CX_DEBUG (d, ("Constructor\n"));
	return EINVAL;
}

static int ng_cx_newhook (node_p node, hook_p hook, const char *name)
{
	int s;
#if __FreeBSD_version >= 500000
	drv_t *d = NG_NODE_PRIVATE (node);
#else
	drv_t *d = node->private;
#endif

	if (d->chan->mode == M_ASYNC)
		return EINVAL;

	/* Attach debug hook */
	if (strcmp (name, NG_CX_HOOK_DEBUG) == 0) {
#if __FreeBSD_version >= 500000
		NG_HOOK_SET_PRIVATE (hook, NULL);
#else
		hook->private = 0;
#endif
		d->debug_hook = hook;
		return 0;
	}

	/* Check for raw hook */
	if (strcmp (name, NG_CX_HOOK_RAW) != 0)
		return EINVAL;

#if __FreeBSD_version >= 500000
	NG_HOOK_SET_PRIVATE (hook, d);
#else
	hook->private = d;
#endif
	d->hook = hook;
	s = splhigh ();
	cx_up (d);
	splx (s);
	return 0;
}

static int print_modems (char *s, cx_chan_t *c, int need_header)
{
	int status = cx_modem_status (c->sys);
	int length = 0;

	if (need_header)
		length += sprintf (s + length, "  LE   DTR  DSR  RTS  CTS  CD\n");
	length += sprintf (s + length, "%4s %4s %4s %4s %4s %4s\n",
		status & TIOCM_LE  ? "On" : "-",
		status & TIOCM_DTR ? "On" : "-",
		status & TIOCM_DSR ? "On" : "-",
		status & TIOCM_RTS ? "On" : "-",
		status & TIOCM_CTS ? "On" : "-",
		status & TIOCM_CD  ? "On" : "-");
	return length;
}

static int print_stats (char *s, cx_chan_t *c, int need_header)
{
	int length = 0;

	if (need_header)
		length += sprintf (s + length, "  Rintr   Tintr   Mintr   Ibytes   Ipkts   Ierrs   Obytes   Opkts   Oerrs\n");
	length += sprintf (s + length, "%7ld %7ld %7ld %8ld %7ld %7ld %8ld %7ld %7ld\n",
		c->rintr, c->tintr, c->mintr, c->ibytes, c->ipkts,
		c->ierrs, c->obytes, c->opkts, c->oerrs);
	return length;
}

static int print_chan (char *s, cx_chan_t *c)
{
	drv_t *d = c->sys;
	int length = 0;

	length += sprintf (s + length, "cx%d", c->board->num * NCHAN + c->num);
	if (d->chan->debug)
		length += sprintf (s + length, " debug=%d", d->chan->debug);

	if (cx_get_baud (c))
		length += sprintf (s + length, " %ld", cx_get_baud (c));
	else
		length += sprintf (s + length, " extclock");

	if (c->mode == M_HDLC) {
		length += sprintf (s + length, " dpll=%s", cx_get_dpll (c) ? "on" : "off");
		length += sprintf (s + length, " nrzi=%s", cx_get_nrzi (c) ? "on" : "off");
	}

	length += sprintf (s + length, " loop=%s", cx_get_loop (c) ? "on\n" : "off\n");
	return length;
}

#if __FreeBSD_version >= 500000
static int ng_cx_rcvmsg (node_p node, item_p item, hook_p lasthook)
{
	drv_t *d = NG_NODE_PRIVATE (node);
	struct ng_mesg *msg;
#else
static int ng_cx_rcvmsg (node_p node, struct ng_mesg *msg,
	const char *retaddr, struct ng_mesg **rptr)
{
	drv_t *d = node->private;
#endif
	struct ng_mesg *resp = NULL;
	int error = 0;

	if (!d)
		return EINVAL;
		
	CX_DEBUG (d, ("Rcvmsg\n"));
#if __FreeBSD_version >= 500000
	NGI_GET_MSG (item, msg);
#endif
	switch (msg->header.typecookie) {
	default:
		error = EINVAL;
		break;

	case NGM_CX_COOKIE:
		printf ("Don't forget to implement\n");
		error = EINVAL;
		break;

	case NGM_GENERIC_COOKIE:
		switch (msg->header.cmd) {
		default:
			error = EINVAL;
			break;

		case NGM_TEXT_STATUS: {
			char *s;
			int l = 0;
			int dl = sizeof (struct ng_mesg) + 730;

#if __FreeBSD_version >= 500000	
			NG_MKRESPONSE (resp, msg, dl, M_NOWAIT);
			if (! resp) {
				error = ENOMEM;
				break;
			}
#else
			MALLOC (resp, struct ng_mesg *, dl,
				M_NETGRAPH, M_NOWAIT);
			if (! resp) {
				error = ENOMEM;
				break;
			}
#endif
			bzero (resp, dl);
			s = (resp)->data;
			l += print_chan (s + l, d->chan);
			l += print_stats (s + l, d->chan, 1);
			l += print_modems (s + l, d->chan, 1);
#if __FreeBSD_version < 500000
			(resp)->header.version = NG_VERSION;
			(resp)->header.arglen = strlen (s) + 1;
			(resp)->header.token = msg->header.token;
			(resp)->header.typecookie = NGM_CX_COOKIE;
			(resp)->header.cmd = msg->header.cmd;
#endif
			strncpy ((resp)->header.cmdstr, "status", NG_CMDSTRLEN);
			}
			break;
		}
		break;
	}
#if __FreeBSD_version >= 500000
	NG_RESPOND_MSG (error, node, item, resp);
	NG_FREE_MSG (msg);
#else
	*rptr = resp;
	FREE (msg, M_NETGRAPH);
#endif
	return error;
}

#if __FreeBSD_version >= 500000
static int ng_cx_rcvdata (hook_p hook, item_p item)
{
	drv_t *d = NG_NODE_PRIVATE (NG_HOOK_NODE(hook));
	struct mbuf *m;
	meta_p meta;
#else
static int ng_cx_rcvdata (hook_p hook, struct mbuf *m, meta_p meta)
{
	drv_t *d = hook->node->private;
#endif
	struct ifqueue *q;
	int s;

#if __FreeBSD_version >= 500000
	NGI_GET_M (item, m);
	NGI_GET_META (item, meta);
	NG_FREE_ITEM (item);
	if (! NG_HOOK_PRIVATE (hook) || ! d) {
		NG_FREE_M (m);
		NG_FREE_META (meta);
#else
	if (! hook->private || ! d) {
		NG_FREE_DATA (m,meta);
#endif
		return ENETDOWN;
	}
	q = (meta && meta->priority > 0) ? &d->hi_queue : &d->lo_queue;
	s = splhigh ();
#if __FreeBSD_version >= 500000
	IF_LOCK (q);
	if (_IF_QFULL (q)) {
		_IF_DROP (q);
		IF_UNLOCK (q);
		splx (s);
		NG_FREE_M (m);
		NG_FREE_META (meta);
		return ENOBUFS;
	}
	_IF_ENQUEUE (q, m);
	IF_UNLOCK (q);
#else
	if (IF_QFULL (q)) {
		IF_DROP (q);
		splx (s);
		NG_FREE_DATA (m, meta);
		return ENOBUFS;
	}
	IF_ENQUEUE (q, m);
#endif
	cx_start (d);
	splx (s);
	return 0;
}

static int ng_cx_rmnode (node_p node)
{
#if __FreeBSD_version >= 500000
	drv_t *d = NG_NODE_PRIVATE (node);

	CX_DEBUG (d, ("Rmnode\n"));
	if (d && d->running) {
		int s = splhigh ();
		cx_down (d);
		splx (s);
	}
#ifdef	KLD_MODULE
	if (node->nd_flags & NG_REALLY_DIE) {
		NG_NODE_SET_PRIVATE (node, NULL);
		NG_NODE_UNREF (node);
	}
	node->nd_flags &= ~NG_INVALID;
#endif
#else /* __FreeBSD_version < 500000 */
	drv_t *d = node->private;
	int s;

	s = splhigh ();
	cx_down (d);
	splx (s);
	node->flags |= NG_INVALID;
	ng_cutlinks (node);
#ifdef	KLD_MODULE
	ng_unname (node);
	ng_unref (node);
#else
	node->flags &= ~NG_INVALID;
#endif
#endif
	return 0;
}

static void ng_cx_watchdog (void *arg)
{
	drv_t *d = arg;

	if (d->timeout == 1)
		cx_watchdog (d);
	if (d->timeout)
		d->timeout--;
	d->timeout_handle = timeout (ng_cx_watchdog, d, hz);
}

static int ng_cx_connect (hook_p hook)
{
#if __FreeBSD_version >= 500000
	drv_t *d = NG_NODE_PRIVATE (NG_HOOK_NODE (hook));
#else
	drv_t *d = hook->node->private;
#endif

	d->timeout_handle = timeout (ng_cx_watchdog, d, hz);
	return 0;
}

static int ng_cx_disconnect (hook_p hook)
{
#if __FreeBSD_version >= 500000
	drv_t *d = NG_NODE_PRIVATE (NG_HOOK_NODE (hook));
#else
	drv_t *d = hook->node->private;
#endif
	int s;

	s = splhigh ();
#if __FreeBSD_version >= 500000
	if (NG_HOOK_PRIVATE (hook))
#else
	if (hook->private)
#endif
		cx_down (d);
	splx (s);
	untimeout (ng_cx_watchdog, d, d->timeout_handle);
	return 0;
}
#endif /*NETGRAPH*/

#ifdef KLD_MODULE
#if __FreeBSD_version < 400000
/*
 * Function called when loading the driver.
 */
static int cx_load (void)
{
	int i;

	for (i=0;i<NCX; ++i) {
		struct isa_device id = {-1, &cxdriver, -1, 0, -1, 0, 0, (inthand2_t *)cx_intr, i, 0, 0, 0, 0 ,0 ,1 ,0 ,0};

		disable_intr();
		if (!cx_probe (&id)) {
			enable_intr();
			break;
		}
		cx_attach (&id);
		register_intr ((adapter [i])->irq, 0, 0, (inthand2_t*) cx_intr,
				&net_imask, id.id_unit);
		enable_intr();
	}
	if (!i) {
		/* Deactivate the timeout routine. And soft interrupt*/
		untimeout (cx_timeout, 0, timeout_handle);
#if __FreeBSD_version >= 500000
		ithread_remove_handler (cx_fast_ih);
		ithread_remove_handler (cx_slow_ih);
#else
		unregister_swi (SWI_TTY, cx_softintr);
#endif
		return ENXIO;
	}
	return 0;
}

/*
 * Function called when unloading the driver.
 */
static int cx_unload (void)
{
	int i, s;

	/* Check if the device is busy (open). */
	for (i=0; i<NCX*NCHAN; ++i) {
		drv_t *d = channel[i];
		cx_chan_t *c;

		if (!d || (c=d->chan)->type == T_NONE)
			continue;
		if (d->lock)
			return EBUSY;
		if (c->mode == M_ASYNC && (d->tty.t_state & TS_ISOPEN) &&
			(d->open_dev|0x2))
				return EBUSY;
		if (d->running)
				return EBUSY;

	}

	s = splhigh ();

	/* Deactivate the timeout routine. And soft interrupt*/
	untimeout (cx_timeout, 0, timeout_handle);
	unregister_swi (SWI_TTY, cx_softintr);

	for (i=0; i<NCX*NCHAN; ++i) {
		drv_t *d = channel[i];
		cx_chan_t *c;

		if (!d || (c=d->chan)->type == T_NONE)
			continue;

		if (d->dtr_timeout_handle.callout)
			untimeout (cx_dtrwakeup, d, d->dtr_timeout_handle);
		if (d->dcd_timeout_handle.callout)
			untimeout (cx_carrier, c, d->dcd_timeout_handle);
	}

	/* Close all active boards. */
	for (i=0; i<NCX; ++i) {
		cx_board_t *b = adapter [i];

		if (!b || ! b->port)
			continue;

		cx_close_board (b);
	}

	for (i=0; i<NCX; ++i) {
		cx_board_t *b = adapter [i];

		if (!b || ! b->port)
			continue;

		if (led_timo[i].callout)
			untimeout (cx_led_off, b, led_timo[i]);
	}

	/* OK to unload the driver, unregister the interrupt first. */
	for (i=0; i<NCX; ++i) {
		cx_board_t *b = adapter [i];

		if (!b || ! b->port)
			continue;
		/* Disable the interrupt request. */
		disable_intr();
		unregister_intr (b->irq, (inthand2_t *)cx_intr);
		isa_dma_release (b->dma);
		enable_intr();
	}
	splx (s);

	s = splhigh ();
	/* Detach the interfaces, free buffer memory. */
	for (i=0; i<NCX*NCHAN; ++i) {
		drv_t *d = channel[i];
		cx_chan_t *c;

		if (!d || (c=d->chan)->type == T_NONE)
			continue;

#ifndef NETGRAPH
#if NBPFILTER > 0
		/* Detach from the packet filter list of interfaces. */
		{
			struct bpf_if *q, **b = &bpf_iflist;

			while ((q = *b)) {
				if (q->bif_ifp == d->pp.pp_if) {
					*b = q->bif_next;
					free (q, M_DEVBUF);
				}
				b = &(q->bif_next);
			}
		}
#endif /* NBPFILTER */
		/* Detach from the sync PPP list. */
		sppp_detach (&d->pp.pp_if);

		/* Detach from the system list of interfaces. */
		{
			struct ifaddr *ifa;
			TAILQ_FOREACH (ifa, &d->pp.pp_if.if_addrhead, ifa_link) {
				TAILQ_REMOVE (&d->pp.pp_if.if_addrhead, ifa, ifa_link);
				free (ifa, M_IFADDR);
			}
			TAILQ_REMOVE (&ifnet, &d->pp.pp_if, if_link);
		}
#endif /* !NETGRAPH */
		/* Deallocate buffers. */
/*		free (d, M_DEVBUF);*/
	}

	for (i=0; i<NCX; ++i) {
		cx_board_t *b = adapter [i];
		adapter [b->num] = 0;
		free (b, M_DEVBUF);
	}

	splx (s);

	return 0;
}

#define devsw(a)	cdevsw[major((a))]
#endif /* __FreeBSD_version < 400000 */
#endif /* KLD_MODULE */

#if __FreeBSD_version < 400000
#ifdef KLD_MODULE
static int cx_modevent (module_t mod, int type, void *unused)
{
        dev_t dev;
	int result;
	static int load_count = 0;

	dev = makedev (CDEV_MAJOR, 0);
	switch (type) {
	case MOD_LOAD:
		if (devsw(dev))
			return (ENXIO);
		load_count ++;
		cdevsw_add (&dev, &cx_cdevsw, NULL);
 		timeout_handle = timeout (cx_timeout, 0, hz*5);

		/* Software interrupt. */
		register_swi (SWI_TTY, cx_softintr);

		result = cx_load ();
 		return result;
	case MOD_UNLOAD:
		result = cx_unload ();
		if (result)
			return result;
		if (devsw(dev)&&!(load_count-1)) {
		cdevsw_add (&dev, NULL, NULL);
		}
		load_count --;
		return result;
	case MOD_SHUTDOWN:
		break;
	}
	return 0;
}
#endif /* KLD_MODULE */
#else /* __FreeBSD_version >= 400000 */
static int cx_modevent (module_t mod, int type, void *unused)
{
        dev_t dev;
	static int load_count = 0;
	struct cdevsw *cdsw;

	dev = makedev (CDEV_MAJOR, 0);
	switch (type) {
	case MOD_LOAD:
		if ((cdsw = devsw (dev)) && cdsw->d_maj == CDEV_MAJOR) {
			printf ("Sigma driver is already in system\n");
			return (EEXIST);
		}
#if __FreeBSD_version >= 500000 && defined NETGRAPH
		if (ng_newtype (&typestruct))
			printf ("Failed to register ng_cx\n");
#endif
		++load_count;
#if __FreeBSD_version <= 500000
		cdevsw_add (&cx_cdevsw);
#endif
		timeout_handle = timeout (cx_timeout, 0, hz*5);
		/* Software interrupt. */
#if __FreeBSD_version < 500000
		register_swi (SWI_TTY, cx_softintr);
#else
		swi_add(&tty_ithd, "tty:cx", cx_softintr, NULL, SWI_TTY, 0,
		    &cx_fast_ih);
		swi_add(&clk_ithd, "tty:cx", cx_softintr, NULL, SWI_TTY, 0,
		    &cx_slow_ih);
#endif
		break;
	case MOD_UNLOAD:
		if (load_count == 1) {
			printf ("Removing device entry for Sigma\n");
#if __FreeBSD_version <= 500000
			cdevsw_remove (&cx_cdevsw);
#endif
#if __FreeBSD_version >= 500000 && defined NETGRAPH
			ng_rmtype (&typestruct);
#endif			
		}
		if (timeout_handle.callout)
			untimeout (cx_timeout, 0, timeout_handle);
#if __FreeBSD_version >= 500000
		ithread_remove_handler (cx_fast_ih);
		ithread_remove_handler (cx_slow_ih);
#else
		unregister_swi (SWI_TTY, cx_softintr);
#endif
		--load_count;
		break;
	case MOD_SHUTDOWN:
		break;
	}
	return 0;
}
#endif  /* __FreeBSD_version >= 400000 */

#ifdef NETGRAPH
static struct ng_type typestruct = {
#if __FreeBSD_version >= 500000
	NG_ABI_VERSION,
#else
	NG_VERSION,
#endif
	NG_CX_NODE_TYPE,
#if __FreeBSD_version < 500000 && defined KLD_MODULE
	cx_modevent,
#else
	NULL,
#endif
	ng_cx_constructor,
	ng_cx_rcvmsg,
	ng_cx_rmnode,
	ng_cx_newhook,
	NULL,
	ng_cx_connect,
	ng_cx_rcvdata,
#if __FreeBSD_version < 500000
	NULL,
#endif
	ng_cx_disconnect
};

#if __FreeBSD_version < 400000
NETGRAPH_INIT_ORDERED (cx, &typestruct, SI_SUB_DRIVERS,\
	SI_ORDER_MIDDLE + CDEV_MAJOR);
#endif
#endif /*NETGRAPH*/

#if __FreeBSD_version >= 500000
#ifdef NETGRAPH
MODULE_DEPEND (ng_cx, netgraph, NG_ABI_VERSION, NG_ABI_VERSION, NG_ABI_VERSION);
#else
MODULE_DEPEND (isa_cx, sppp, 1, 1, 1);
#endif
#ifdef KLD_MODULE
DRIVER_MODULE (cxmod, isa, cx_isa_driver, cx_devclass, cx_modevent, NULL);
#else
DRIVER_MODULE (cx, isa, cx_isa_driver, cx_devclass, cx_modevent, NULL);
#endif
#elif __FreeBSD_version >= 400000
#ifdef NETGRAPH
DRIVER_MODULE(cx, isa, cx_isa_driver, cx_devclass, ng_mod_event, &typestruct);
#else
DRIVER_MODULE(cx, isa, cx_isa_driver, cx_devclass, cx_modevent, 0);
#endif
#else /* __FreeBSD_version < 400000 */
#ifdef KLD_MODULE
#ifndef NETGRAPH
static moduledata_t cxmod = { "cx", cx_modevent, NULL};
DECLARE_MODULE (cx, cxmod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE + CDEV_MAJOR);
#endif
#else /* KLD_MODULE */

/*
 * Now for some driver initialisation.
 * Occurs ONCE during boot (very early).
 * This is if we are NOT a loadable module.
 */
static void cx_drvinit (void *unused)
{
#if __FreeBSD_version < 400000
        dev_t dev;

	dev = makedev (CDEV_MAJOR, 0);
	cdevsw_add (&dev, &cx_cdevsw, NULL);
#else
	cdevsw_add (&cx_cdevsw);
#endif

	/* Activate the timeout routine. */
	timeout_handle = timeout (cx_timeout, 0, hz*5);

	/* Software interrupt. */
	register_swi (SWI_TTY, cx_softintr);
#ifdef NETGRAPH
#if 0
	/* Register our node type in netgraph */
	if (ng_newtype (&typestruct))
		printf ("Failed to register ng_cx\n");
#endif
#endif
}

SYSINIT (cxdev, SI_SUB_DRIVERS, SI_ORDER_MIDDLE+CDEV_MAJOR, cx_drvinit, 0)

#endif /* KLD_MODULE */
#endif /*  __FreeBSD_version < 400000 */
#endif /* NCX */
