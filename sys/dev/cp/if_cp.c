/*-
 * Cronyx-Tau-PCI adapter driver for FreeBSD.
 * Supports PPP/HDLC, Cisco/HDLC and FrameRelay protocol in synchronous mode,
 * and asyncronous channels with full modem control.
 * Keepalive protocol implemented in both Cisco and PPP modes.
 *
 * Copyright (C) 1999-2004 Cronyx Engineering.
 * Author: Kurakin Roman, <rik@cronyx.ru>
 *
 * Copyright (C) 1999-2002 Cronyx Engineering.
 * Author: Serge Vakulenko, <vak@cronyx.ru>
 *
 * This software is distributed with NO WARRANTIES, not even the implied
 * warranties for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Authors grant any other persons or organisations a permission to use,
 * modify and redistribute this software in source and binary forms,
 * as long as this message is kept with the software, all derivative
 * works or modified versions.
 *
 * Cronyx Id: if_cp.c,v 1.1.2.41 2004/06/23 17:09:13 rik Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>

#if __FreeBSD_version >= 500000
#   define NPCI 1
#else
#   include "pci.h"
#endif

#if NPCI > 0

#include <sys/ucred.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/tty.h>
#include <sys/bus.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <net/if.h>
#if __FreeBSD_version > 501000
#   include <dev/pci/pcivar.h>
#   include <dev/pci/pcireg.h>
#else
#   include <pci/pcivar.h>
#   include <pci/pcireg.h>
#endif
#include <machine/bus.h>
#include <sys/rman.h>
#include "opt_ng_cronyx.h"
#ifdef NETGRAPH_CRONYX
#   include "opt_netgraph.h"
#   ifndef NETGRAPH
#	error #option	NETGRAPH missed from configuration
#   endif
#   include <netgraph/ng_message.h>
#   include <netgraph/netgraph.h>
#   include <dev/cp/ng_cp.h>
#else
#   include <net/if_sppp.h>
#   define PP_CISCO IFF_LINK2
#   if __FreeBSD_version < 500000
#	include <bpf.h>
#   endif
#   include <net/bpf.h>
#   define NBPFILTER NBPF
#endif
#include <dev/cx/machdep.h>
#include <dev/cp/cpddk.h>
#include <machine/cserial.h>
#include <machine/resource.h>
#include <machine/pmap.h>

/* If we don't have Cronyx's sppp version, we don't have fr support via sppp */
#ifndef PP_FR
#define PP_FR 0
#endif

#define CP_DEBUG(d,s)	({if (d->chan->debug) {\
				printf ("%s: ", d->name); printf s;}})
#define CP_DEBUG2(d,s)	({if (d->chan->debug>1) {\
				printf ("%s: ", d->name); printf s;}})

#define CDEV_MAJOR	134

static	int cp_probe		__P((device_t));
static	int cp_attach		__P((device_t));
static	int cp_detach		__P((device_t));

static	device_method_t cp_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		cp_probe),
	DEVMETHOD(device_attach,	cp_attach),
	DEVMETHOD(device_detach,	cp_detach),

	{0, 0}
};

typedef struct _cp_dma_mem_t {
	unsigned long	phys;
	void		*virt;
	size_t		size;
#if __FreeBSD_version >= 500000
	bus_dma_tag_t	dmat;
	bus_dmamap_t	mapp;
#endif
} cp_dma_mem_t;

typedef struct _drv_t {
	char name [8];
	cp_chan_t *chan;
	cp_board_t *board;
	cp_dma_mem_t dmamem;
	int running;
#ifdef NETGRAPH
	char	nodename [NG_NODELEN+1];
	hook_p	hook;
	hook_p	debug_hook;
	node_p	node;
	struct	ifqueue queue;
	struct	ifqueue hi_queue;
	short	timeout;
	struct	callout_handle timeout_handle;
#else
	struct sppp pp;
#endif
	struct cdev *devt;
} drv_t;

typedef	struct _bdrv_t {
	cp_board_t	*board;
	struct resource *cp_res;
	struct resource *cp_irq;
	void		*cp_intrhand;
	cp_dma_mem_t	dmamem;
	drv_t		channel [NCHAN];
} bdrv_t;

static	driver_t cp_driver = {
	"cp",
	cp_methods,
	sizeof(bdrv_t),
};

static	devclass_t cp_devclass;

static void cp_receive (cp_chan_t *c, unsigned char *data, int len);
static void cp_transmit (cp_chan_t *c, void *attachment, int len);
static void cp_error (cp_chan_t *c, int data);
static void cp_up (drv_t *d);
static void cp_start (drv_t *d);
static void cp_down (drv_t *d);
static void cp_watchdog (drv_t *d);
#ifdef NETGRAPH
extern struct ng_type typestruct;
#else
static void cp_ifstart (struct ifnet *ifp);
static void cp_tlf (struct sppp *sp);
static void cp_tls (struct sppp *sp);
static void cp_ifwatchdog (struct ifnet *ifp);
static int cp_sioctl (struct ifnet *ifp, u_long cmd, caddr_t data);
static void cp_initialize (void *softc);
#endif

static cp_board_t *adapter [NBRD];
static drv_t *channel [NBRD*NCHAN];
static struct callout_handle led_timo [NBRD];
static struct callout_handle timeout_handle;

static int cp_destroy = 0;

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
static struct mbuf *makembuf (void *buf, unsigned len)
{
	struct mbuf *m;

	MGETHDR (m, M_DONTWAIT, MT_DATA);
	if (! m)
		return 0;
	MCLGET (m, M_DONTWAIT);
	if (! (m->m_flags & M_EXT)) {
		m_freem (m);
		return 0;
	}
	m->m_pkthdr.len = m->m_len = len;
	bcopy (buf, mtod (m, caddr_t), len);
	return m;
}

static int cp_probe (device_t dev)
{
	if ((pci_get_vendor (dev) == cp_vendor_id) &&
	    (pci_get_device (dev) == cp_device_id)) {
		device_set_desc (dev, "Cronyx-Tau-PCI serial adapter");
		return 0;
	}
	return ENXIO;
}

static void cp_timeout (void *arg)
{
	drv_t *d;
	int s, i;

	for (i=0; i<NBRD*NCHAN; ++i) {
		s = splimp ();
		if (cp_destroy) {
			splx (s);
			return;
		}
		d = channel[i];
		if (!d) {
			splx (s);
			continue;
		}
		switch (d->chan->type) {
		case T_G703:
			cp_g703_timer (d->chan);
			break;
		case T_E1:
			cp_e1_timer (d->chan);
			break;
		case T_E3:
		case T_T3:
		case T_STS1:
			cp_e3_timer (d->chan);
			break;
		default:
			break;
		}
		splx (s);
	}
	s = splimp ();
	if (!cp_destroy)
		timeout_handle = timeout (cp_timeout, 0, hz);
	splx (s);
}

static void cp_led_off (void *arg)
{
	cp_board_t *b = arg;
	int s = splimp ();
	if (cp_destroy) {
		splx (s);
		return;
	}
	cp_led (b, 0);
	led_timo[b->num].callout = 0;
	splx (s);
}

static void cp_intr (void *arg)
{
	bdrv_t *bd = arg;
	cp_board_t *b = bd->board;
	int s = splimp ();
	if (cp_destroy) {
		splx (s);
		return;
	}
	/* Turn LED on. */
	cp_led (b, 1);

	cp_interrupt (b);

	/* Turn LED off 50 msec later. */
	if (!led_timo[b->num].callout)
		led_timo[b->num] = timeout (cp_led_off, b, hz/20);
	splx (s);
}

extern struct cdevsw cp_cdevsw;

#if __FreeBSD_version >= 500000
static void
cp_bus_dmamap_addr (void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	unsigned long *addr;

	if (error)
		return;

	KASSERT(nseg == 1, ("too many DMA segments, %d should be 1", nseg));
	addr = arg;
	*addr = segs->ds_addr;
}

static int
cp_bus_dma_mem_alloc (int bnum, int cnum, cp_dma_mem_t *dmem)
{
	int error;

	error = bus_dma_tag_create (NULL, 16, 0, BUS_SPACE_MAXADDR_32BIT,
		BUS_SPACE_MAXADDR, NULL, NULL, dmem->size, 1,
		dmem->size, 0, NULL, NULL, &dmem->dmat);
	if (error) {
		if (cnum >= 0)	printf ("cp%d-%d: ", bnum, cnum);
		else		printf ("cp%d: ", bnum);
		printf ("couldn't allocate tag for dma memory\n");
 		return 0;
	}
	error = bus_dmamem_alloc (dmem->dmat, (void **)&dmem->virt,
		BUS_DMA_NOWAIT | BUS_DMA_ZERO, &dmem->mapp);
	if (error) {
		if (cnum >= 0)	printf ("cp%d-%d: ", bnum, cnum);
		else		printf ("cp%d: ", bnum);
		printf ("couldn't allocate mem for dma memory\n");
		bus_dma_tag_destroy (dmem->dmat);
 		return 0;
	}
	error = bus_dmamap_load (dmem->dmat, dmem->mapp, dmem->virt,
		dmem->size, cp_bus_dmamap_addr, &dmem->phys, 0);
	if (error) {
		if (cnum >= 0)	printf ("cp%d-%d: ", bnum, cnum);
		else		printf ("cp%d: ", bnum);
		printf ("couldn't load mem map for dma memory\n");
		bus_dmamem_free (dmem->dmat, dmem->virt, dmem->mapp);
		bus_dma_tag_destroy (dmem->dmat);
 		return 0;
	}
	return 1;
}

static void
cp_bus_dma_mem_free (cp_dma_mem_t *dmem)
{
	bus_dmamap_unload (dmem->dmat, dmem->mapp);
	bus_dmamem_free (dmem->dmat, dmem->virt, dmem->mapp);
	bus_dma_tag_destroy (dmem->dmat);
}
#else
static int
cp_bus_dma_mem_alloc (int bnum, int cnum, cp_dma_mem_t *dmem)
{
	dmem->virt = contigmalloc (dmem->size, M_DEVBUF, M_WAITOK,
				   0x100000, 0xffffffff, 16, 0);
	if (dmem->virt == NULL) {
		if (cnum >= 0)	printf ("cp%d-%d: ", bnum, cnum);
		else		printf ("cp%d: ", bnum);
		printf ("couldn't allocate memory for dma memory\n", unit);
 		return 0;
	}
	dmem->phys = vtophys (dmem->virt);
	return 1;
}

static void
cp_bus_dma_mem_free (cp_dma_mem_t *dmem)
{
	contigfree (dmem->virt, dmem->size, M_DEVBUF);
}
#endif

/*
 * Called if the probe succeeded.
 */
static int cp_attach (device_t dev)
{
	bdrv_t *bd = device_get_softc (dev);
	int unit = device_get_unit (dev);
	unsigned short res;
	vm_offset_t vbase;
	int rid, error;
	cp_board_t *b;
	cp_chan_t *c;
	drv_t *d;
	int s = splimp ();

	b = malloc (sizeof(cp_board_t), M_DEVBUF, M_WAITOK);
	if (!b) {
		printf ("cp%d: couldn't allocate memory\n", unit);		
		splx (s);
		return (ENXIO);
	}
	adapter[unit] = b;
	bzero (b, sizeof(cp_board_t));

	bd->board = b;
	b->sys = bd;
	rid = PCIR_BAR(0);
	bd->cp_res = bus_alloc_resource (dev, SYS_RES_MEMORY, &rid,
			0, ~0, 1, RF_ACTIVE);
	if (! bd->cp_res) {
		printf ("cp%d: cannot map memory\n", unit);
		free (b, M_DEVBUF);
		splx (s);
		return (ENXIO);
	}
	vbase = (vm_offset_t) rman_get_virtual (bd->cp_res);

	res = cp_init (b, unit, (u_char*) vbase);
	if (res) {
		printf ("cp%d: can't init, error code:%x\n", unit, res);
		bus_release_resource (dev, SYS_RES_MEMORY, PCIR_BAR(0), bd->cp_res);
		free (b, M_DEVBUF);
		splx (s);
 		return (ENXIO);
	}

	bd->dmamem.size = sizeof(cp_qbuf_t);
	if (! cp_bus_dma_mem_alloc (unit, -1, &bd->dmamem)) {
		free (b, M_DEVBUF);
		splx (s);
 		return (ENXIO);
	}
	cp_reset (b, bd->dmamem.virt, bd->dmamem.phys);

	rid = 0;
	bd->cp_irq = bus_alloc_resource (dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
			RF_SHAREABLE | RF_ACTIVE);
	if (! bd->cp_irq) {
		printf ("cp%d: cannot map interrupt\n", unit);
		bus_release_resource (dev, SYS_RES_MEMORY,
				PCIR_BAR(0), bd->cp_res);
		free (b, M_DEVBUF);
		splx (s);
		return (ENXIO);
	}
	error  = bus_setup_intr (dev, bd->cp_irq, INTR_TYPE_NET, cp_intr, bd,
				&bd->cp_intrhand);
	if (error) {
		printf ("cp%d: cannot set up irq\n", unit);
		bus_release_resource (dev, SYS_RES_MEMORY,
				PCIR_BAR(0), bd->cp_res);
		bus_release_resource (dev, SYS_RES_IRQ, 0, bd->cp_irq);
		free (b, M_DEVBUF);
		splx (s);
		return (ENXIO);
	}
	printf ("cp%d: %s, clock %ld MHz\n", unit, b->name, b->osc / 1000000);

	for (c=b->chan; c<b->chan+NCHAN; ++c) {
		if (! c->type)
			continue;
		d = &bd->channel[c->num];
		d->dmamem.size = sizeof(cp_buf_t);
		if (! cp_bus_dma_mem_alloc (unit, c->num, &d->dmamem))
			continue;
		channel [b->num*NCHAN + c->num] = d;
		sprintf (d->name, "cp%d.%d", b->num, c->num);
		d->board = b;
		d->chan = c;
		c->sys = d;
#ifdef NETGRAPH
		if (ng_make_node_common (&typestruct, &d->node) != 0) {
			printf ("%s: cannot make common node\n", d->name);
			d->node = NULL;
			continue;
		}
#if __FreeBSD_version >= 500000
		NG_NODE_SET_PRIVATE (d->node, d);
#else
		d->node->private = d;
#endif
		sprintf (d->nodename, "%s%d", NG_CP_NODE_TYPE,
			 c->board->num*NCHAN + c->num);
		if (ng_name_node (d->node, d->nodename)) {
			printf ("%s: cannot name node\n", d->nodename);
#if __FreeBSD_version >= 500000
			NG_NODE_UNREF (d->node);
#else
			ng_rmnode (d->node);
			ng_unref (d->node);
#endif
			continue;
		}
		d->queue.ifq_maxlen = IFQ_MAXLEN;
		d->hi_queue.ifq_maxlen = IFQ_MAXLEN;
#if __FreeBSD_version >= 500000
		mtx_init (&d->queue.ifq_mtx, "cp_queue", NULL, MTX_DEF);
		mtx_init (&d->hi_queue.ifq_mtx, "cp_queue_hi", NULL, MTX_DEF);
#endif		
#else /*NETGRAPH*/
		d->pp.pp_if.if_softc	= d;
#if __FreeBSD_version > 501000
		if_initname (&d->pp.pp_if, "cp", b->num * NCHAN + c->num);
#else
		d->pp.pp_if.if_unit	= b->num * NCHAN + c->num;
		d->pp.pp_if.if_name	= "cp";
#endif
		d->pp.pp_if.if_mtu	= PP_MTU;
		d->pp.pp_if.if_flags	= IFF_POINTOPOINT | IFF_MULTICAST |
				IFF_NEEDSGIANT;
		d->pp.pp_if.if_ioctl	= cp_sioctl;
		d->pp.pp_if.if_start	= cp_ifstart;
		d->pp.pp_if.if_watchdog	= cp_ifwatchdog;
		d->pp.pp_if.if_init	= cp_initialize;
		sppp_attach (&d->pp.pp_if);
		if_attach (&d->pp.pp_if);
		d->pp.pp_tlf		= cp_tlf;
		d->pp.pp_tls		= cp_tls;
		/* If BPF is in the kernel, call the attach for it.
		 * The header size of PPP or Cisco/HDLC is 4 bytes. */
		bpfattach (&d->pp.pp_if, DLT_PPP, 4);
#endif /*NETGRAPH*/
		cp_start_e1 (c);
		cp_start_chan (c, 1, 1, d->dmamem.virt, d->dmamem.phys);

		/* Register callback functions. */
		cp_register_transmit (c, &cp_transmit);
		cp_register_receive (c, &cp_receive);
		cp_register_error (c, &cp_error);
		d->devt = make_dev (&cp_cdevsw, b->num*NCHAN+c->num, UID_ROOT,
				GID_WHEEL, 0600, "cp%d", b->num*NCHAN+c->num);
	}
	splx (s);
	return 0;
}

static int cp_detach (device_t dev)
{
	bdrv_t *bd = device_get_softc (dev);
	cp_board_t *b = bd->board;
	cp_chan_t *c;
	int s = splimp ();

	/* Check if the device is busy (open). */
	for (c=b->chan; c<b->chan+NCHAN; ++c) {
		drv_t *d = (drv_t*) c->sys;

		if (! d || ! d->chan->type)
			continue;
		if (d->running) {
			splx (s);
			return EBUSY;
		}
	}

	/* Ok, we can unload driver */
	/* At first we should stop all channels */
	for (c=b->chan; c<b->chan+NCHAN; ++c) {
		drv_t *d = (drv_t*) c->sys;

		if (! d || ! d->chan->type)
			continue;

		cp_stop_chan (c);
		cp_stop_e1 (c);
		cp_set_dtr (d->chan, 0);
		cp_set_rts (d->chan, 0);
	}

	/* Reset the adapter. */
	cp_destroy = 1;
	cp_interrupt_poll (b, 1);
	cp_led_off (b);
	cp_reset (b, 0 ,0);
	if (led_timo[b->num].callout)
		untimeout (cp_led_off, b, led_timo[b->num]);

	for (c=b->chan; c<b->chan+NCHAN; ++c) {
		drv_t *d = (drv_t*) c->sys;

		if (! d || ! d->chan->type)
			continue;
#ifndef NETGRAPH
#if __FreeBSD_version >= 410000 && NBPFILTER > 0
		/* Detach from the packet filter list of interfaces. */
		bpfdetach (&d->pp.pp_if);
#endif
		/* Detach from the sync PPP list. */
		sppp_detach (&d->pp.pp_if);

		/* Detach from the system list of interfaces. */
		if_detach (&d->pp.pp_if);
#else
#if __FreeBSD_version >= 500000
		if (d->node) {
			ng_rmnode_self (d->node);
			NG_NODE_UNREF (d->node);
			d->node = NULL;
		}
		mtx_destroy (&d->queue.ifq_mtx);
		mtx_destroy (&d->hi_queue.ifq_mtx);
#else
		ng_rmnode (d->node);
		d->node = 0;
#endif
#endif
		destroy_dev (d->devt);
	}

	/* Disable the interrupt request. */
	bus_teardown_intr (dev, bd->cp_irq, bd->cp_intrhand);
	bus_deactivate_resource (dev, SYS_RES_IRQ, 0, bd->cp_irq);
	bus_release_resource (dev, SYS_RES_IRQ, 0, bd->cp_irq);
	bus_release_resource (dev, SYS_RES_MEMORY, PCIR_BAR(0), bd->cp_res);
	cp_led_off (b);
	if (led_timo[b->num].callout)
		untimeout (cp_led_off, b, led_timo[b->num]);
	splx (s);

	s = splimp ();
	for (c=b->chan; c<b->chan+NCHAN; ++c) {
		drv_t *d = (drv_t*) c->sys;

		if (! d || ! d->chan->type)
			continue;
		channel [b->num*NCHAN + c->num] = 0;
		/* Deallocate buffers. */
		cp_bus_dma_mem_free (&d->dmamem);
	}
	adapter [b->num] = 0;
	cp_bus_dma_mem_free (&bd->dmamem);
	free (b, M_DEVBUF);
	splx (s);
	return 0;
}

#ifndef NETGRAPH
static void cp_ifstart (struct ifnet *ifp)
{
	drv_t *d = ifp->if_softc;

	cp_start (d);
}

static void cp_ifwatchdog (struct ifnet *ifp)
{
	drv_t *d = ifp->if_softc;

	cp_watchdog (d);
}

static void cp_tlf (struct sppp *sp)
{
	drv_t *d = sp->pp_if.if_softc;

	CP_DEBUG2 (d, ("cp_tlf\n"));
/*	cp_set_dtr (d->chan, 0);*/
/*	cp_set_rts (d->chan, 0);*/
	sp->pp_down (sp);
}

static void cp_tls (struct sppp *sp)
{
	drv_t *d = sp->pp_if.if_softc;

	CP_DEBUG2 (d, ("cp_tls\n"));
	sp->pp_up (sp);
}

/*
 * Process an ioctl request.
 */
static int cp_sioctl (struct ifnet *ifp, u_long cmd, caddr_t data)
{
	drv_t *d = ifp->if_softc;
	int error, s, was_up, should_be_up;

	was_up = (ifp->if_flags & IFF_RUNNING) != 0;
	error = sppp_ioctl (ifp, cmd, data);

	if (error)
		return error;

	if (! (ifp->if_flags & IFF_DEBUG))
		d->chan->debug = 0;
	else if (! d->chan->debug)
		d->chan->debug = 1;

	switch (cmd) {
	default:	   CP_DEBUG2 (d, ("ioctl 0x%lx\n", cmd));   return 0;
	case SIOCADDMULTI: CP_DEBUG2 (d, ("ioctl SIOCADDMULTI\n")); return 0;
	case SIOCDELMULTI: CP_DEBUG2 (d, ("ioctl SIOCDELMULTI\n")); return 0;
	case SIOCSIFFLAGS: CP_DEBUG2 (d, ("ioctl SIOCSIFFLAGS\n")); break;
	case SIOCSIFADDR:  CP_DEBUG2 (d, ("ioctl SIOCSIFADDR\n"));  break;
	}

	/* We get here only in case of SIFFLAGS or SIFADDR. */
	s = splimp ();
	should_be_up = (ifp->if_flags & IFF_RUNNING) != 0;
	if (! was_up && should_be_up) {
		/* Interface goes up -- start it. */
		cp_up (d);
		cp_start (d);
	} else if (was_up && ! should_be_up) {
		/* Interface is going down -- stop it. */
/*		if ((d->pp.pp_flags & PP_FR) || (ifp->if_flags & PP_CISCO))*/
		cp_down (d);
	}
	CP_DEBUG (d, ("ioctl 0x%lx p4\n", cmd));
	splx (s);
	return 0;
}

/*
 * Initialization of interface.
 * It seems to be never called by upper level?
 */
static void cp_initialize (void *softc)
{
	drv_t *d = softc;

	CP_DEBUG (d, ("cp_initialize\n"));
}
#endif /*NETGRAPH*/

/*
 * Stop the interface.  Called on splimp().
 */
static void cp_down (drv_t *d)
{
	CP_DEBUG (d, ("cp_down\n"));
	/* Interface is going down -- stop it. */
	cp_set_dtr (d->chan, 0);
	cp_set_rts (d->chan, 0);

	d->running = 0;
}

/*
 * Start the interface.  Called on splimp().
 */
static void cp_up (drv_t *d)
{
	CP_DEBUG (d, ("cp_up\n"));
	cp_set_dtr (d->chan, 1);
	cp_set_rts (d->chan, 1);
	d->running = 1;
}

/*
 * Start output on the interface.  Get another datagram to send
 * off of the interface queue, and copy it to the interface
 * before starting the output.
 */
static void cp_send (drv_t *d)
{
	struct mbuf *m;
	u_short len;

	CP_DEBUG2 (d, ("cp_send, tn=%d te=%d\n", d->chan->tn, d->chan->te));

	/* No output if the interface is down. */
	if (! d->running)
		return;

	/* No output if the modem is off. */
	if (! (d->chan->lloop || d->chan->type != T_SERIAL ||
		cp_get_dsr (d->chan)))
		return;

	while (cp_transmit_space (d->chan)) {
		/* Get the packet to send. */
#ifdef NETGRAPH
		IF_DEQUEUE (&d->hi_queue, m);
		if (! m)
			IF_DEQUEUE (&d->queue, m);
#else
		m = sppp_dequeue (&d->pp.pp_if);
#endif
		if (! m)
			return;
#ifndef NETGRAPH
		if (d->pp.pp_if.if_bpf)
#if __FreeBSD_version >= 500000
			BPF_MTAP (&d->pp.pp_if, m);
#else
			bpf_mtap (&d->pp.pp_if, m);
#endif
#endif
		len = m->m_pkthdr.len;
		if (len >= BUFSZ)
			printf ("%s: too long packet: %d bytes: ",
				d->name, len);
		else if (! m->m_next)
			cp_send_packet (d->chan, (u_char*) mtod (m, caddr_t), len, 0);
		else {
			u_char *buf = d->chan->tbuf[d->chan->te];
			m_copydata (m, 0, len, buf);
			cp_send_packet (d->chan, buf, len, 0);
		}
		m_freem (m);
		/* Set up transmit timeout, if the transmit ring is not empty.*/
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
static void cp_start (drv_t *d)
{
	if (d->running) {
		if (! d->chan->dtr)
			cp_set_dtr (d->chan, 1);
		if (! d->chan->rts)
			cp_set_rts (d->chan, 1);
		cp_send (d);
	}
}

/*
 * Handle transmit timeouts.
 * Recover after lost transmit interrupts.
 * Always called on splimp().
 */
static void cp_watchdog (drv_t *d)
{
	CP_DEBUG (d, ("device timeout\n"));
	if (d->running) {
		int s = splimp ();

		cp_stop_chan (d->chan);
		cp_stop_e1 (d->chan);
		cp_start_e1 (d->chan);
		cp_start_chan (d->chan, 1, 1, 0, 0);
		cp_set_dtr (d->chan, 1);
		cp_set_rts (d->chan, 1);
		cp_start (d);
		splx (s);
	}
}

static void cp_transmit (cp_chan_t *c, void *attachment, int len)
{
	drv_t *d = c->sys;

#ifdef NETGRAPH
	d->timeout = 0;
#else
	++d->pp.pp_if.if_opackets;
	d->pp.pp_if.if_flags &= ~IFF_OACTIVE;
	d->pp.pp_if.if_timer = 0;
#endif
	cp_start (d);
}

static void cp_receive (cp_chan_t *c, unsigned char *data, int len)
{
	drv_t *d = c->sys;
	struct mbuf *m;
#if __FreeBSD_version >= 500000 && defined NETGRAPH
	int error;
#endif

	if (! d->running)
		return;

	m = makembuf (data, len);
	if (! m) {
		CP_DEBUG (d, ("no memory for packet\n"));
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
	/* Check if there's a BPF listener on this interface.
	 * If so, hand off the raw packet to bpf. */
	if (d->pp.pp_if.if_bpf)
#if __FreeBSD_version >= 500000
		BPF_TAP (&d->pp.pp_if, data, len);
#else
		bpf_tap (&d->pp.pp_if, data, len);
#endif
	sppp_input (&d->pp.pp_if, m);
#endif
}

static void cp_error (cp_chan_t *c, int data)
{
	drv_t *d = c->sys;

	switch (data) {
	case CP_FRAME:
		CP_DEBUG (d, ("frame error\n"));
#ifndef NETGRAPH
		++d->pp.pp_if.if_ierrors;
#endif
		break;
	case CP_CRC:
		CP_DEBUG (d, ("crc error\n"));
#ifndef NETGRAPH
		++d->pp.pp_if.if_ierrors;
#endif
		break;
	case CP_OVERRUN:
		CP_DEBUG (d, ("overrun error\n"));
#ifndef NETGRAPH
		++d->pp.pp_if.if_collisions;
		++d->pp.pp_if.if_ierrors;
#endif
		break;
	case CP_OVERFLOW:
		CP_DEBUG (d, ("overflow error\n"));
#ifndef NETGRAPH
		++d->pp.pp_if.if_ierrors;
#endif
		break;
	case CP_UNDERRUN:
		CP_DEBUG (d, ("underrun error\n"));
#ifdef NETGRAPH
		d->timeout = 0;
#else
		++d->pp.pp_if.if_oerrors;
		d->pp.pp_if.if_flags &= ~IFF_OACTIVE;
		d->pp.pp_if.if_timer = 0;
#endif
		cp_start (d);
		break;
	default:
		CP_DEBUG (d, ("error #%d\n", data));
		break;
	}
}

/*
 * You also need read, write, open, close routines.
 * This should get you started
 */
#if __FreeBSD_version < 500000
static int cp_open (dev_t dev, int oflags, int devtype, struct proc *p)
#else
static int cp_open (struct cdev *dev, int oflags, int devtype, struct thread *td)
#endif
{
	int unit = minor (dev);
	drv_t *d;

	if (unit >= NBRD*NCHAN || ! (d = channel[unit]))
		return ENXIO;
	CP_DEBUG2 (d, ("cp_open\n"));
	return 0;
}

/*
 * Only called on the LAST close.
 */
#if __FreeBSD_version < 500000
static int cp_close (dev_t dev, int fflag, int devtype, struct proc *p)
#else
static int cp_close (struct cdev *dev, int fflag, int devtype, struct thread *td)
#endif
{
	drv_t *d = channel [minor (dev)];

	CP_DEBUG2 (d, ("cp_close\n"));
	return 0;
}

static int cp_modem_status (cp_chan_t *c)
{
	drv_t *d = c->sys;
	int status, s;

	status = d->running ? TIOCM_LE : 0;
	s = splimp ();
	if (cp_get_cd  (c)) status |= TIOCM_CD;
	if (cp_get_cts (c)) status |= TIOCM_CTS;
	if (cp_get_dsr (c)) status |= TIOCM_DSR;
	if (c->dtr)	    status |= TIOCM_DTR;
	if (c->rts)	    status |= TIOCM_RTS;
	splx (s);
	return status;
}

#if __FreeBSD_version < 500000
static int cp_ioctl (dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
#else
static int cp_ioctl (struct cdev *dev, u_long cmd, caddr_t data, int flag, struct thread *td)
#endif
{
	drv_t *d = channel [minor (dev)];
	cp_chan_t *c = d->chan;
	struct serial_statistics *st;
	struct e1_statistics *opte1;
	struct e3_statistics *opte3;
	int error, s;
	char mask[16];

	switch (cmd) {
	case SERIAL_GETREGISTERED:
		CP_DEBUG2 (d, ("ioctl: getregistered\n"));
		bzero (mask, sizeof(mask));
		for (s=0; s<NBRD*NCHAN; ++s)
			if (channel [s])
				mask [s/8] |= 1 << (s & 7);
		bcopy (mask, data, sizeof (mask));
		return 0;

#ifndef NETGRAPH
	case SERIAL_GETPROTO:
		CP_DEBUG2 (d, ("ioctl: getproto\n"));
		strcpy ((char*)data, (d->pp.pp_flags & PP_FR) ? "fr" :
			(d->pp.pp_if.if_flags & PP_CISCO) ? "cisco" : "ppp");
		return 0;

	case SERIAL_SETPROTO:
		CP_DEBUG2 (d, ("ioctl: setproto\n"));
		/* Only for superuser! */
#if __FreeBSD_version < 500000
		error = suser (p);
#else /* __FreeBSD_version >= 500000 */
		error = suser (td);
#endif /* __FreeBSD_version >= 500000 */
		if (error)
			return error;
		if (d->pp.pp_if.if_flags & IFF_RUNNING)
			return EBUSY;
		if (! strcmp ("cisco", (char*)data)) {
			d->pp.pp_flags &= ~(PP_FR);
			d->pp.pp_flags |= PP_KEEPALIVE;
			d->pp.pp_if.if_flags |= PP_CISCO;
		} else if (! strcmp ("fr", (char*)data) && !PP_FR) {
			d->pp.pp_if.if_flags &= ~(PP_CISCO);
			d->pp.pp_flags |= PP_FR | PP_KEEPALIVE;
		} else if (! strcmp ("ppp", (char*)data)) {
			d->pp.pp_flags &= ~PP_FR;
			d->pp.pp_flags &= ~PP_KEEPALIVE;
			d->pp.pp_if.if_flags &= ~(PP_CISCO);
		} else
			return EINVAL;
		return 0;

	case SERIAL_GETKEEPALIVE:
		CP_DEBUG2 (d, ("ioctl: getkeepalive\n"));
		if ((d->pp.pp_flags & PP_FR) ||
			(d->pp.pp_if.if_flags & PP_CISCO))
			return EINVAL;
		*(int*)data = (d->pp.pp_flags & PP_KEEPALIVE) ? 1 : 0;
		return 0;

	case SERIAL_SETKEEPALIVE:
		CP_DEBUG2 (d, ("ioctl: setkeepalive\n"));
		/* Only for superuser! */
#if __FreeBSD_version < 500000
		error = suser (p);
#else
		error = suser (td);
#endif
		if (error)
			return error;
		if ((d->pp.pp_flags & PP_FR) ||
			(d->pp.pp_if.if_flags & PP_CISCO))
			return EINVAL;
		s = splimp ();
		if (*(int*)data)
			d->pp.pp_flags |= PP_KEEPALIVE;
		else
			d->pp.pp_flags &= ~PP_KEEPALIVE;
		splx (s);
		return 0;
#endif /*NETGRAPH*/

	case SERIAL_GETMODE:
		CP_DEBUG2 (d, ("ioctl: getmode\n"));
		*(int*)data = SERIAL_HDLC;
		return 0;

	case SERIAL_SETMODE:
		/* Only for superuser! */
#if __FreeBSD_version < 500000
		error = suser (p);
#else
		error = suser (td);
#endif
		if (error)
			return error;
		if (*(int*)data != SERIAL_HDLC)
			return EINVAL;
		return 0;

	case SERIAL_GETCFG:
		CP_DEBUG2 (d, ("ioctl: getcfg\n"));
		if (c->type != T_E1 || c->unfram)
			return EINVAL;
		*(char*)data = c->board->mux ? 'c' : 'a';
		return 0;

	case SERIAL_SETCFG:
		CP_DEBUG2 (d, ("ioctl: setcfg\n"));
#if __FreeBSD_version < 500000
		error = suser (p);
#else
		error = suser (td);
#endif
		if (error)
			return error;
		if (c->type != T_E1)
			return EINVAL;
		s = splimp ();
		cp_set_mux (c->board, *((char*)data) == 'c');
		splx (s);
		return 0;

	case SERIAL_GETSTAT:
		CP_DEBUG2 (d, ("ioctl: getstat\n"));
		st = (struct serial_statistics*) data;
		st->rintr  = c->rintr;
		st->tintr  = c->tintr;
		st->mintr  = 0;
		st->ibytes = c->ibytes;
		st->ipkts  = c->ipkts;
		st->obytes = c->obytes;
		st->opkts  = c->opkts;
		st->ierrs  = c->overrun + c->frame + c->crc;
		st->oerrs  = c->underrun;
		return 0;

	case SERIAL_GETESTAT:
		CP_DEBUG2 (d, ("ioctl: getestat\n"));
		if (c->type != T_E1 && c->type != T_G703)
			return EINVAL;
		opte1 = (struct e1_statistics*) data;
		opte1->status	    = c->status;
		opte1->cursec	    = c->cursec;
		opte1->totsec	    = c->totsec + c->cursec;

		opte1->currnt.bpv   = c->currnt.bpv;
		opte1->currnt.fse   = c->currnt.fse;
		opte1->currnt.crce  = c->currnt.crce;
		opte1->currnt.rcrce = c->currnt.rcrce;
		opte1->currnt.uas   = c->currnt.uas;
		opte1->currnt.les   = c->currnt.les;
		opte1->currnt.es    = c->currnt.es;
		opte1->currnt.bes   = c->currnt.bes;
		opte1->currnt.ses   = c->currnt.ses;
		opte1->currnt.oofs  = c->currnt.oofs;
		opte1->currnt.css   = c->currnt.css;
		opte1->currnt.dm    = c->currnt.dm;

		opte1->total.bpv    = c->total.bpv   + c->currnt.bpv;
		opte1->total.fse    = c->total.fse   + c->currnt.fse;
		opte1->total.crce   = c->total.crce  + c->currnt.crce;
		opte1->total.rcrce  = c->total.rcrce + c->currnt.rcrce;
		opte1->total.uas    = c->total.uas   + c->currnt.uas;
		opte1->total.les    = c->total.les   + c->currnt.les;
		opte1->total.es	    = c->total.es    + c->currnt.es;
		opte1->total.bes    = c->total.bes   + c->currnt.bes;
		opte1->total.ses    = c->total.ses   + c->currnt.ses;
		opte1->total.oofs   = c->total.oofs  + c->currnt.oofs;
		opte1->total.css    = c->total.css   + c->currnt.css;
		opte1->total.dm	    = c->total.dm    + c->currnt.dm;
		for (s=0; s<48; ++s) {
			opte1->interval[s].bpv   = c->interval[s].bpv;
			opte1->interval[s].fse   = c->interval[s].fse;
			opte1->interval[s].crce  = c->interval[s].crce;
			opte1->interval[s].rcrce = c->interval[s].rcrce;
			opte1->interval[s].uas   = c->interval[s].uas;
			opte1->interval[s].les   = c->interval[s].les;
			opte1->interval[s].es	 = c->interval[s].es;
			opte1->interval[s].bes   = c->interval[s].bes;
			opte1->interval[s].ses   = c->interval[s].ses;
			opte1->interval[s].oofs  = c->interval[s].oofs;
			opte1->interval[s].css   = c->interval[s].css;
			opte1->interval[s].dm	 = c->interval[s].dm;
		}
		return 0;

	case SERIAL_GETE3STAT:
		CP_DEBUG2 (d, ("ioctl: gete3stat\n"));
		if (c->type != T_E3 && c->type != T_T3 && c->type != T_STS1)
			return EINVAL;
		opte3 = (struct e3_statistics*) data;

		opte3->status = c->e3status;
		opte3->cursec = (c->e3csec_5 * 2 + 1) / 10;
		opte3->totsec = c->e3tsec + opte3->cursec;

		opte3->ccv = c->e3ccv;
		opte3->tcv = c->e3tcv + opte3->ccv;

		for (s = 0; s < 48; ++s) {
			opte3->icv[s] = c->e3icv[s];
		}
		return 0;
		
	case SERIAL_CLRSTAT:
		CP_DEBUG2 (d, ("ioctl: clrstat\n"));
		/* Only for superuser! */
#if __FreeBSD_version < 500000
		error = suser (p);
#else
		error = suser (td);
#endif
		if (error)
			return error;
		c->rintr    = 0;
		c->tintr    = 0;
		c->ibytes   = 0;
		c->obytes   = 0;
		c->ipkts    = 0;
		c->opkts    = 0;
		c->overrun  = 0;
		c->frame    = 0;
		c->crc	    = 0;
		c->underrun = 0;
		bzero (&c->currnt, sizeof (c->currnt));
		bzero (&c->total, sizeof (c->total));
		bzero (c->interval, sizeof (c->interval));
		c->e3ccv    = 0;
		c->e3tcv    = 0;
		bzero (c->e3icv, sizeof (c->e3icv));
		return 0;

	case SERIAL_GETBAUD:
		CP_DEBUG2 (d, ("ioctl: getbaud\n"));
		*(long*)data = c->baud;
		return 0;

	case SERIAL_SETBAUD:
		CP_DEBUG2 (d, ("ioctl: setbaud\n"));
		/* Only for superuser! */
#if __FreeBSD_version < 500000
		error = suser (p);
#else
		error = suser (td);
#endif
		if (error)
			return error;
		s = splimp ();
		cp_set_baud (c, *(long*)data);
		splx (s);
		return 0;

	case SERIAL_GETLOOP:
		CP_DEBUG2 (d, ("ioctl: getloop\n"));
		*(int*)data = c->lloop;
		return 0;

	case SERIAL_SETLOOP:
		CP_DEBUG2 (d, ("ioctl: setloop\n"));
		/* Only for superuser! */
#if __FreeBSD_version < 500000
		error = suser (p);
#else
		error = suser (td);
#endif
		if (error)
			return error;
		s = splimp ();
		cp_set_lloop (c, *(int*)data);
		splx (s);
		return 0;

	case SERIAL_GETDPLL:
		CP_DEBUG2 (d, ("ioctl: getdpll\n"));
		if (c->type != T_SERIAL)
			return EINVAL;
		*(int*)data = c->dpll;
		return 0;

	case SERIAL_SETDPLL:
		CP_DEBUG2 (d, ("ioctl: setdpll\n"));
		/* Only for superuser! */
#if __FreeBSD_version < 500000
		error = suser (p);
#else
		error = suser (td);
#endif
		if (error)
			return error;
		if (c->type != T_SERIAL)
			return EINVAL;
		s = splimp ();
		cp_set_dpll (c, *(int*)data);
		splx (s);
		return 0;

	case SERIAL_GETNRZI:
		CP_DEBUG2 (d, ("ioctl: getnrzi\n"));
		if (c->type != T_SERIAL)
			return EINVAL;
		*(int*)data = c->nrzi;
		return 0;

	case SERIAL_SETNRZI:
		CP_DEBUG2 (d, ("ioctl: setnrzi\n"));
		/* Only for superuser! */
#if __FreeBSD_version < 500000
		error = suser (p);
#else
		error = suser (td);
#endif
		if (error)
			return error;
		if (c->type != T_SERIAL)
			return EINVAL;
		s = splimp ();
		cp_set_nrzi (c, *(int*)data);
		splx (s);
		return 0;

	case SERIAL_GETDEBUG:
		CP_DEBUG2 (d, ("ioctl: getdebug\n"));
		*(int*)data = d->chan->debug;
		return 0;

	case SERIAL_SETDEBUG:
		CP_DEBUG2 (d, ("ioctl: setdebug\n"));
		/* Only for superuser! */
#if __FreeBSD_version < 500000
		error = suser (p);
#else
		error = suser (td);
#endif
		if (error)
			return error;
		d->chan->debug = *(int*)data;
#ifndef	NETGRAPH
		if (d->chan->debug)
			d->pp.pp_if.if_flags |= IFF_DEBUG;
		else
			d->pp.pp_if.if_flags &= ~IFF_DEBUG;
#endif
		return 0;

	case SERIAL_GETHIGAIN:
		CP_DEBUG2 (d, ("ioctl: gethigain\n"));
		if (c->type != T_E1)
			return EINVAL;
		*(int*)data = c->higain;
		return 0;

	case SERIAL_SETHIGAIN:
		CP_DEBUG2 (d, ("ioctl: sethigain\n"));
		/* Only for superuser! */
#if __FreeBSD_version < 500000
		error = suser (p);
#else
		error = suser (td);
#endif
		if (error)
			return error;
		if (c->type != T_E1)
			return EINVAL;
		s = splimp ();
		cp_set_higain (c, *(int*)data);
		splx (s);
		return 0;

	case SERIAL_GETPHONY:
		CP_DEBUG2 (d, ("ioctl: getphony\n"));
		if (c->type != T_E1)
			return EINVAL;
		*(int*)data = c->phony;
		return 0;

	case SERIAL_SETPHONY:
		CP_DEBUG2 (d, ("ioctl: setphony\n"));
		/* Only for superuser! */
#if __FreeBSD_version < 500000
		error = suser (p);
#else
		error = suser (td);
#endif
		if (error)
			return error;
		if (c->type != T_E1)
			return EINVAL;
		s = splimp ();
		cp_set_phony (c, *(int*)data);
		splx (s);
		return 0;

	case SERIAL_GETUNFRAM:
		CP_DEBUG2 (d, ("ioctl: getunfram\n"));
		if (c->type != T_E1)
			return EINVAL;
		*(int*)data = c->unfram;
		return 0;

	case SERIAL_SETUNFRAM:
		CP_DEBUG2 (d, ("ioctl: setunfram\n"));
		/* Only for superuser! */
#if __FreeBSD_version < 500000
		error = suser (p);
#else
		error = suser (td);
#endif
		if (error)
			return error;
		if (c->type != T_E1)
			return EINVAL;
		s = splimp ();
		cp_set_unfram (c, *(int*)data);
		splx (s);
		return 0;

	case SERIAL_GETSCRAMBLER:
		CP_DEBUG2 (d, ("ioctl: getscrambler\n"));
		if (c->type != T_G703 && !c->unfram)
			return EINVAL;
		*(int*)data = c->scrambler;
		return 0;

	case SERIAL_SETSCRAMBLER:
		CP_DEBUG2 (d, ("ioctl: setscrambler\n"));
		/* Only for superuser! */
#if __FreeBSD_version < 500000
		error = suser (p);
#else
		error = suser (td);
#endif
		if (error)
			return error;
		if (c->type != T_G703 && !c->unfram)
			return EINVAL;
		s = splimp ();
		cp_set_scrambler (c, *(int*)data);
		splx (s);
		return 0;

	case SERIAL_GETMONITOR:
		CP_DEBUG2 (d, ("ioctl: getmonitor\n"));
		if (c->type != T_E1 &&
		    c->type != T_E3 &&
		    c->type != T_T3 &&
		    c->type != T_STS1)
			return EINVAL;
		*(int*)data = c->monitor;
		return 0;

	case SERIAL_SETMONITOR:
		CP_DEBUG2 (d, ("ioctl: setmonitor\n"));
		/* Only for superuser! */
#if __FreeBSD_version < 500000
		error = suser (p);
#else
		error = suser (td);
#endif
		if (error)
			return error;
		if (c->type != T_E1)
			return EINVAL;
		s = splimp ();
		cp_set_monitor (c, *(int*)data);
		splx (s);
		return 0;

	case SERIAL_GETUSE16:
		CP_DEBUG2 (d, ("ioctl: getuse16\n"));
		if (c->type != T_E1 || c->unfram)
			return EINVAL;
		*(int*)data = c->use16;
		return 0;

	case SERIAL_SETUSE16:
		CP_DEBUG2 (d, ("ioctl: setuse16\n"));
		/* Only for superuser! */
#if __FreeBSD_version < 500000
		error = suser (p);
#else
		error = suser (td);
#endif
		if (error)
			return error;
		if (c->type != T_E1)
			return EINVAL;
		s = splimp ();
		cp_set_use16 (c, *(int*)data);
		splx (s);
		return 0;

	case SERIAL_GETCRC4:
		CP_DEBUG2 (d, ("ioctl: getcrc4\n"));
		if (c->type != T_E1 || c->unfram)
			return EINVAL;
		*(int*)data = c->crc4;
		return 0;

	case SERIAL_SETCRC4:
		CP_DEBUG2 (d, ("ioctl: setcrc4\n"));
		/* Only for superuser! */
#if __FreeBSD_version < 500000
		error = suser (p);
#else
		error = suser (td);
#endif
		if (error)
			return error;
		if (c->type != T_E1)
			return EINVAL;
		s = splimp ();
		cp_set_crc4 (c, *(int*)data);
		splx (s);
		return 0;

	case SERIAL_GETCLK:
		CP_DEBUG2 (d, ("ioctl: getclk\n"));
		if (c->type != T_E1 &&
		    c->type != T_G703 &&
		    c->type != T_E3 &&
		    c->type != T_T3 &&
		    c->type != T_STS1)
			return EINVAL;
		switch (c->gsyn) {
		default:	*(int*)data = E1CLK_INTERNAL;		break;
		case GSYN_RCV:	*(int*)data = E1CLK_RECEIVE;		break;
		case GSYN_RCV0:	*(int*)data = E1CLK_RECEIVE_CHAN0;	break;
		case GSYN_RCV1:	*(int*)data = E1CLK_RECEIVE_CHAN1;	break;
		case GSYN_RCV2:	*(int*)data = E1CLK_RECEIVE_CHAN2;	break;
		case GSYN_RCV3:	*(int*)data = E1CLK_RECEIVE_CHAN3;	break;
		}
		return 0;

	case SERIAL_SETCLK:
		CP_DEBUG2 (d, ("ioctl: setclk\n"));
		/* Only for superuser! */
#if __FreeBSD_version < 500000
		error = suser (p);
#else
		error = suser (td);
#endif
		if (error)
			return error;
		if (c->type != T_E1 &&
		    c->type != T_G703 &&
		    c->type != T_E3 &&
		    c->type != T_T3 &&
		    c->type != T_STS1)
			return EINVAL;
		s = splimp ();
		switch (*(int*)data) {
		default:		  cp_set_gsyn (c, GSYN_INT);  break;
		case E1CLK_RECEIVE:	  cp_set_gsyn (c, GSYN_RCV);  break;
		case E1CLK_RECEIVE_CHAN0: cp_set_gsyn (c, GSYN_RCV0); break;
		case E1CLK_RECEIVE_CHAN1: cp_set_gsyn (c, GSYN_RCV1); break;
		case E1CLK_RECEIVE_CHAN2: cp_set_gsyn (c, GSYN_RCV2); break;
		case E1CLK_RECEIVE_CHAN3: cp_set_gsyn (c, GSYN_RCV3); break;
		}
		splx (s);
		return 0;

	case SERIAL_GETTIMESLOTS:
		CP_DEBUG2 (d, ("ioctl: gettimeslots\n"));
		if ((c->type != T_E1 || c->unfram) && c->type != T_DATA)
			return EINVAL;
		*(u_long*)data = c->ts;
		return 0;

	case SERIAL_SETTIMESLOTS:
		CP_DEBUG2 (d, ("ioctl: settimeslots\n"));
		/* Only for superuser! */
#if __FreeBSD_version < 500000
		error = suser (p);
#else
		error = suser (td);
#endif
		if (error)
			return error;
		if ((c->type != T_E1 || c->unfram) && c->type != T_DATA)
			return EINVAL;
		s = splimp ();
		cp_set_ts (c, *(u_long*)data);
		splx (s);
		return 0;

	case SERIAL_GETINVCLK:
		CP_DEBUG2 (d, ("ioctl: getinvclk\n"));
#if 1
		return EINVAL;
#else
		if (c->type != T_SERIAL)
			return EINVAL;
		*(int*)data = c->invtxc;
		return 0;
#endif

	case SERIAL_SETINVCLK:
		CP_DEBUG2 (d, ("ioctl: setinvclk\n"));
		/* Only for superuser! */
#if __FreeBSD_version < 500000
		error = suser (p);
#else
		error = suser (td);
#endif
		if (error)
			return error;
		if (c->type != T_SERIAL)
			return EINVAL;
		s = splimp ();
		cp_set_invtxc (c, *(int*)data);
		cp_set_invrxc (c, *(int*)data);
		splx (s);
		return 0;

	case SERIAL_GETINVTCLK:
		CP_DEBUG2 (d, ("ioctl: getinvtclk\n"));
		if (c->type != T_SERIAL)
			return EINVAL;
		*(int*)data = c->invtxc;
		return 0;

	case SERIAL_SETINVTCLK:
		CP_DEBUG2 (d, ("ioctl: setinvtclk\n"));
		/* Only for superuser! */
#if __FreeBSD_version < 500000
		error = suser (p);
#else
		error = suser (td);
#endif
		if (error)
			return error;
		if (c->type != T_SERIAL)
			return EINVAL;
		s = splimp ();
		cp_set_invtxc (c, *(int*)data);
		splx (s);
		return 0;

	case SERIAL_GETINVRCLK:
		CP_DEBUG2 (d, ("ioctl: getinvrclk\n"));
		if (c->type != T_SERIAL)
			return EINVAL;
		*(int*)data = c->invrxc;
		return 0;

	case SERIAL_SETINVRCLK:
		CP_DEBUG2 (d, ("ioctl: setinvrclk\n"));
		/* Only for superuser! */
#if __FreeBSD_version < 500000
		error = suser (p);
#else
		error = suser (td);
#endif
		if (error)
			return error;
		if (c->type != T_SERIAL)
			return EINVAL;
		s = splimp ();
		cp_set_invrxc (c, *(int*)data);
		splx (s);
		return 0;

	case SERIAL_GETLEVEL:
		CP_DEBUG2 (d, ("ioctl: getlevel\n"));
		if (c->type != T_G703)
			return EINVAL;
		s = splimp ();
		*(int*)data = cp_get_lq (c);
		splx (s);
		return 0;

#if 0
	case SERIAL_RESET:
		CP_DEBUG2 (d, ("ioctl: reset\n"));
		/* Only for superuser! */
#if __FreeBSD_version < 500000
		error = suser (p);
#else
		error = suser (td);
#endif
		if (error)
			return error;
		s = splimp ();
		cp_reset (c->board, 0, 0);
		splx (s);
		return 0;

	case SERIAL_HARDRESET:
		CP_DEBUG2 (d, ("ioctl: hardreset\n"));
		/* Only for superuser! */
#if __FreeBSD_version < 500000
		error = suser (p);
#else
		error = suser (td);
#endif
		if (error)
			return error;
		s = splimp ();
		/* hard_reset (c->board); */
		splx (s);
		return 0;
#endif

	case SERIAL_GETCABLE:
		CP_DEBUG2 (d, ("ioctl: getcable\n"));
		if (c->type != T_SERIAL)
			return EINVAL;
		s = splimp ();
		*(int*)data = cp_get_cable (c);
		splx (s);
		return 0;

	case SERIAL_GETDIR:
		CP_DEBUG2 (d, ("ioctl: getdir\n"));
		if (c->type != T_E1 && c->type != T_DATA)
			return EINVAL;
		*(int*)data = c->dir;
		return 0;

	case SERIAL_SETDIR:
		CP_DEBUG2 (d, ("ioctl: setdir\n"));
		/* Only for superuser! */
#if __FreeBSD_version < 500000
		error = suser (p);
#else
		error = suser (td);
#endif
		if (error)
			return error;
		s = splimp ();
		cp_set_dir (c, *(int*)data);
		splx (s);
		return 0;

	case SERIAL_GETRLOOP:
		CP_DEBUG2 (d, ("ioctl: getrloop\n"));
		if (c->type != T_G703 &&
		    c->type != T_E3 &&
		    c->type != T_T3 &&
		    c->type != T_STS1)
			return EINVAL;
		*(int*)data = cp_get_rloop (c);
		return 0;

	case SERIAL_SETRLOOP:
		CP_DEBUG2 (d, ("ioctl: setloop\n"));
		if (c->type != T_E3 && c->type != T_T3 && c->type != T_STS1)
			return EINVAL;
		/* Only for superuser! */
#if __FreeBSD_version < 500000
		error = suser (p);
#else
		error = suser (td);
#endif
		if (error)
			return error;
		s = splimp ();
		cp_set_rloop (c, *(int*)data);
		splx (s);
		return 0;

	case SERIAL_GETCABLEN:
		CP_DEBUG2 (d, ("ioctl: getcablen\n"));
		if (c->type != T_T3 && c->type != T_STS1)
			return EINVAL;
		*(int*)data = c->cablen;
		return 0;

	case SERIAL_SETCABLEN:
		CP_DEBUG2 (d, ("ioctl: setloop\n"));
		if (c->type != T_T3 && c->type != T_STS1)
			return EINVAL;
		/* Only for superuser! */
#if __FreeBSD_version < 500000
		error = suser (p);
#else
		error = suser (td);
#endif
		if (error)
			return error;
		s = splimp ();
		cp_set_cablen (c, *(int*)data);
		splx (s);
		return 0;

	case TIOCSDTR:	/* Set DTR */
		s = splimp ();
		cp_set_dtr (c, 1);
		splx (s);
		return 0;

	case TIOCCDTR:	/* Clear DTR */
		s = splimp ();
		cp_set_dtr (c, 0);
		splx (s);
		return 0;

	case TIOCMSET:	/* Set DTR/RTS */
		s = splimp ();
		cp_set_dtr (c, (*(int*)data & TIOCM_DTR) ? 1 : 0);
		cp_set_rts (c, (*(int*)data & TIOCM_RTS) ? 1 : 0);
		splx (s);
		return 0;

	case TIOCMBIS:	/* Add DTR/RTS */
		s = splimp ();
		if (*(int*)data & TIOCM_DTR) cp_set_dtr (c, 1);
		if (*(int*)data & TIOCM_RTS) cp_set_rts (c, 1);
		splx (s);
		return 0;

	case TIOCMBIC:	/* Clear DTR/RTS */
		s = splimp ();
		if (*(int*)data & TIOCM_DTR) cp_set_dtr (c, 0);
		if (*(int*)data & TIOCM_RTS) cp_set_rts (c, 0);
		splx (s);
		return 0;

	case TIOCMGET:	/* Get modem status */
		*(int*)data = cp_modem_status (c);
		return 0;
	}
	return ENOTTY;
}

#if __FreeBSD_version < 500000
static struct cdevsw cp_cdevsw = {
	cp_open,	cp_close,	noread,		nowrite,
	cp_ioctl,	nopoll,		nommap,		nostrategy,
	"cp",		CDEV_MAJOR,	nodump,		nopsize,
	D_NAGGED,	-1
	};
#elif __FreeBSD_version == 500000
static struct cdevsw cp_cdevsw = {
	cp_open,	cp_close,	noread,		nowrite,
	cp_ioctl,	nopoll,		nommap,		nostrategy,
	"cp",		CDEV_MAJOR,	nodump,		nopsize,
	D_NAGGED,
	};
#elif __FreeBSD_version <= 501000
static struct cdevsw cp_cdevsw = {
	.d_open     = cp_open,
	.d_close    = cp_close,
	.d_read     = noread,
	.d_write    = nowrite,
	.d_ioctl    = cp_ioctl,
	.d_poll     = nopoll,
	.d_mmap	    = nommap,
	.d_strategy = nostrategy,
	.d_name     = "cp",
	.d_maj      = CDEV_MAJOR,
	.d_dump     = nodump,
	.d_flags    = D_NAGGED,
};
#elif __FreeBSD_version < 502103
static struct cdevsw cp_cdevsw = {
	.d_open     = cp_open,
	.d_close    = cp_close,
	.d_ioctl    = cp_ioctl,
	.d_name     = "cp",
	.d_maj      = CDEV_MAJOR,
	.d_flags    = D_NAGGED,
};
#else /* __FreeBSD_version >= 502103 */
static struct cdevsw cp_cdevsw = {
	.d_version  = D_VERSION,
	.d_open     = cp_open,
	.d_close    = cp_close,
	.d_ioctl    = cp_ioctl,
	.d_name     = "cp",
	.d_maj      = CDEV_MAJOR,
	.d_flags    = D_NEEDGIANT,
};
#endif

#ifdef NETGRAPH
#if __FreeBSD_version >= 500000
static int ng_cp_constructor (node_p node)
{
	drv_t *d = NG_NODE_PRIVATE (node);
#else
static int ng_cp_constructor (node_p *node)
{
	drv_t *d = (*node)->private;
#endif
	CP_DEBUG (d, ("Constructor\n"));
	return EINVAL;
}

static int ng_cp_newhook (node_p node, hook_p hook, const char *name)
{
	int s;
#if __FreeBSD_version >= 500000
	drv_t *d = NG_NODE_PRIVATE (node);
#else
	drv_t *d = node->private;
#endif

	CP_DEBUG (d, ("Newhook\n"));
	/* Attach debug hook */
	if (strcmp (name, NG_CP_HOOK_DEBUG) == 0) {
#if __FreeBSD_version >= 500000
		NG_HOOK_SET_PRIVATE (hook, NULL);
#else
		hook->private = 0;
#endif
		d->debug_hook = hook;
		return 0;
	}

	/* Check for raw hook */
	if (strcmp (name, NG_CP_HOOK_RAW) != 0)
		return EINVAL;

#if __FreeBSD_version >= 500000
	NG_HOOK_SET_PRIVATE (hook, d);
#else
	hook->private = d;
#endif
	d->hook = hook;
	s = splimp ();
	cp_up (d);
	splx (s);
	return 0;
}

static char *format_timeslots (u_long s)
{
	static char buf [100];
	char *p = buf;
	int i;

	for (i=1; i<32; ++i)
		if ((s >> i) & 1) {
			int prev = (i > 1)  & (s >> (i-1));
			int next = (i < 31) & (s >> (i+1));

			if (prev) {
				if (next)
					continue;
				*p++ = '-';
			} else if (p > buf)
				*p++ = ',';

			if (i >= 10)
				*p++ = '0' + i / 10;
			*p++ = '0' + i % 10;
		}
	*p = 0;
	return buf;
}

static int print_modems (char *s, cp_chan_t *c, int need_header)
{
	int status = cp_modem_status (c);
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

static int print_stats (char *s, cp_chan_t *c, int need_header)
{
	int length = 0;

	if (need_header)
		length += sprintf (s + length, "  Rintr   Tintr   Mintr   Ibytes   Ipkts   Ierrs   Obytes   Opkts   Oerrs\n");
	length += sprintf (s + length, "%7ld %7ld %7ld %8lu %7ld %7ld %8lu %7ld %7ld\n",
		c->rintr, c->tintr, 0l, (unsigned long) c->ibytes,
		c->ipkts, c->overrun + c->frame + c->crc,
		(unsigned long) c->obytes, c->opkts, c->underrun);
	return length;
}

static char *format_e1_status (u_char status)
{
	static char buf [80];

	if (status & E1_NOALARM)
		return "Ok";
	buf[0] = 0;
	if (status & E1_LOS)     strcat (buf, ",LOS");
	if (status & E1_AIS)     strcat (buf, ",AIS");
	if (status & E1_LOF)     strcat (buf, ",LOF");
	if (status & E1_LOMF)    strcat (buf, ",LOMF");
	if (status & E1_FARLOF)  strcat (buf, ",FARLOF");
	if (status & E1_AIS16)   strcat (buf, ",AIS16");
	if (status & E1_FARLOMF) strcat (buf, ",FARLOMF");
	if (status & E1_TSTREQ)  strcat (buf, ",TSTREQ");
	if (status & E1_TSTERR)  strcat (buf, ",TSTERR");
	if (buf[0] == ',')
		return buf+1;
	return "Unknown";
}

static int print_frac (char *s, int leftalign, u_long numerator, u_long divider)
{
	int n, length = 0;

	if (numerator < 1 || divider < 1) {
		length += sprintf (s+length, leftalign ? "/-   " : "    -");
		return length;
	}
	n = (int) (0.5 + 1000.0 * numerator / divider);
	if (n < 1000) {
		length += sprintf (s+length, leftalign ? "/.%-3d" : " .%03d", n);
		return length;
	}
	*(s + length) = leftalign ? '/' : ' ';
	length ++;

	if      (n >= 1000000) n = (n+500) / 1000 * 1000;
	else if (n >= 100000)  n = (n+50)  / 100 * 100;
	else if (n >= 10000)   n = (n+5)   / 10 * 10;

	switch (n) {
	case 1000:    length += printf (s+length, ".999"); return length;
	case 10000:   n = 9990;   break;
	case 100000:  n = 99900;  break;
	case 1000000: n = 999000; break;
	}
	if (n < 10000)	      length += sprintf (s+length, "%d.%d", n/1000, n/10%100);
	else if (n < 100000)  length += sprintf (s+length, "%d.%d", n/1000, n/100%10);
	else if (n < 1000000) length += sprintf (s+length, "%d.", n/1000);
	else		      length += sprintf (s+length, "%d", n/1000);

	return length;
}

static int print_e1_stats (char *s, cp_chan_t *c)
{
	struct e1_counters total;
	u_long totsec;
	int length = 0;

	totsec		= c->totsec + c->cursec;
	total.bpv	= c->total.bpv   + c->currnt.bpv;
	total.fse	= c->total.fse   + c->currnt.fse;
	total.crce	= c->total.crce  + c->currnt.crce;
	total.rcrce	= c->total.rcrce + c->currnt.rcrce;
	total.uas	= c->total.uas   + c->currnt.uas;
	total.les	= c->total.les   + c->currnt.les;
	total.es	= c->total.es    + c->currnt.es;
	total.bes	= c->total.bes   + c->currnt.bes;
	total.ses	= c->total.ses   + c->currnt.ses;
	total.oofs	= c->total.oofs  + c->currnt.oofs;
	total.css	= c->total.css   + c->currnt.css;
	total.dm	= c->total.dm    + c->currnt.dm;

	length += sprintf (s + length, " Unav/Degr  Bpv/Fsyn  CRC/RCRC  Err/Lerr  Sev/Bur   Oof/Slp  Status\n");

	/* Unavailable seconds, degraded minutes */
	length += print_frac (s + length, 0, c->currnt.uas, c->cursec);
	length += print_frac (s + length, 1, 60 * c->currnt.dm, c->cursec);

	/* Bipolar violations, frame sync errors */
	length += print_frac (s + length, 0, c->currnt.bpv, c->cursec);
	length += print_frac (s + length, 1, c->currnt.fse, c->cursec);

	/* CRC errors, remote CRC errors (E-bit) */
	length += print_frac (s + length, 0, c->currnt.crce, c->cursec);
	length += print_frac (s + length, 1, c->currnt.rcrce, c->cursec);

	/* Errored seconds, line errored seconds */
	length += print_frac (s + length, 0, c->currnt.es, c->cursec);
	length += print_frac (s + length, 1, c->currnt.les, c->cursec);

	/* Severely errored seconds, burst errored seconds */
	length += print_frac (s + length, 0, c->currnt.ses, c->cursec);
	length += print_frac (s + length, 1, c->currnt.bes, c->cursec);

	/* Out of frame seconds, controlled slip seconds */
	length += print_frac (s + length, 0, c->currnt.oofs, c->cursec);
	length += print_frac (s + length, 1, c->currnt.css, c->cursec);

	length += sprintf (s + length, " %s\n", format_e1_status (c->status));

	/* Print total statistics. */
	length += print_frac (s + length, 0, total.uas, totsec);
	length += print_frac (s + length, 1, 60 * total.dm, totsec);

	length += print_frac (s + length, 0, total.bpv, totsec);
	length += print_frac (s + length, 1, total.fse, totsec);

	length += print_frac (s + length, 0, total.crce, totsec);
	length += print_frac (s + length, 1, total.rcrce, totsec);

	length += print_frac (s + length, 0, total.es, totsec);
	length += print_frac (s + length, 1, total.les, totsec);

	length += print_frac (s + length, 0, total.ses, totsec);
	length += print_frac (s + length, 1, total.bes, totsec);

	length += print_frac (s + length, 0, total.oofs, totsec);
	length += print_frac (s + length, 1, total.css, totsec);

	length += sprintf (s + length, " -- Total\n");
	return length;
}

static int print_chan (char *s, cp_chan_t *c)
{
	drv_t *d = c->sys;
	int length = 0;

	length += sprintf (s + length, "cp%d", c->board->num * NCHAN + c->num);
	if (d->chan->debug)
		length += sprintf (s + length, " debug=%d", d->chan->debug);

	if (c->board->mux) {
		length += sprintf (s + length, " cfg=C");
	} else {
		length += sprintf (s + length, " cfg=A");
	}

	if (c->baud)
		length += sprintf (s + length, " %ld", c->baud);
	else
		length += sprintf (s + length, " extclock");

	if (c->type == T_E1 || c->type == T_G703)
		switch (c->gsyn) {
		case GSYN_INT   : length += sprintf (s + length, " syn=int");     break;
		case GSYN_RCV   : length += sprintf (s + length, " syn=rcv");     break;
		case GSYN_RCV0  : length += sprintf (s + length, " syn=rcv0");    break;
		case GSYN_RCV1  : length += sprintf (s + length, " syn=rcv1");    break;
		case GSYN_RCV2  : length += sprintf (s + length, " syn=rcv2");    break;
		case GSYN_RCV3  : length += sprintf (s + length, " syn=rcv3");    break;
		}
	if (c->type == T_SERIAL) {
		length += sprintf (s + length, " dpll=%s",   c->dpll   ? "on" : "off");
		length += sprintf (s + length, " nrzi=%s",   c->nrzi   ? "on" : "off");
		length += sprintf (s + length, " invclk=%s", c->invtxc ? "on" : "off");
	}
	if (c->type == T_E1)
		length += sprintf (s + length, " higain=%s", c->higain ? "on" : "off");

	length += sprintf (s + length, " loop=%s", c->lloop ? "on" : "off");

	if (c->type == T_E1)
		length += sprintf (s + length, " ts=%s", format_timeslots (c->ts));
	if (c->type == T_G703) {
		int lq, x;

		x = splimp ();
		lq = cp_get_lq (c);
		splx (x);
		length += sprintf (s + length, " (level=-%.1fdB)", lq / 10.0);
	}
	length += sprintf (s + length, "\n");
	return length;
}

#if __FreeBSD_version >= 500000
static int ng_cp_rcvmsg (node_p node, item_p item, hook_p lasthook)
{
	drv_t *d = NG_NODE_PRIVATE (node);
	struct ng_mesg *msg;
#else
static int ng_cp_rcvmsg (node_p node, struct ng_mesg *msg,
	const char *retaddr, struct ng_mesg **rptr)
{
	drv_t *d = node->private;
#endif
	struct ng_mesg *resp = NULL;
	int error = 0;

	CP_DEBUG (d, ("Rcvmsg\n"));
#if __FreeBSD_version >= 500000
	NGI_GET_MSG (item, msg);
#endif
	switch (msg->header.typecookie) {
	default:
		error = EINVAL;
		break;

	case NGM_CP_COOKIE:
		printf ("Not implemented yet\n");
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
			bzero (resp, dl);
#endif
			s = (resp)->data;
			if (d) {
			l += print_chan (s + l, d->chan);
			l += print_stats (s + l, d->chan, 1);
			l += print_modems (s + l, d->chan, 1);
			l += print_e1_stats (s + l, d->chan);
			} else
				l += sprintf (s + l, "Error: node not connect to channel");
#if __FreeBSD_version < 500000
			(resp)->header.version = NG_VERSION;
			(resp)->header.arglen = strlen (s) + 1;
			(resp)->header.token = msg->header.token;
			(resp)->header.typecookie = NGM_CP_COOKIE;
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
static int ng_cp_rcvdata (hook_p hook, item_p item)
{
	drv_t *d = NG_NODE_PRIVATE (NG_HOOK_NODE(hook));
	struct mbuf *m;
	struct ng_tag_prio *ptag;
#else
static int ng_cp_rcvdata (hook_p hook, struct mbuf *m, meta_p meta)
{
	drv_t *d = hook->node->private;
#endif
	struct ifqueue *q;
	int s;

	CP_DEBUG2 (d, ("Rcvdata\n"));
#if __FreeBSD_version >= 500000
	NGI_GET_M (item, m);
	NG_FREE_ITEM (item);
	if (! NG_HOOK_PRIVATE (hook) || ! d) {
		NG_FREE_M (m);
#else
	if (! hook->private || ! d) {
		NG_FREE_DATA (m,meta);
#endif
		return ENETDOWN;
	}

	/* Check for high priority data */
	if ((ptag = (struct ng_tag_prio *)m_tag_locate(m, NGM_GENERIC_COOKIE,
	    NG_TAG_PRIO, NULL)) != NULL && (ptag->priority > NG_PRIO_CUTOFF) )
		q = &d->hi_queue;
	else
		q = &d->queue;

	s = splimp ();
#if __FreeBSD_version >= 500000
	IF_LOCK (q);
	if (_IF_QFULL (q)) {
		_IF_DROP (q);
		IF_UNLOCK (q);
		splx (s);
		NG_FREE_M (m);
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
	cp_start (d);
	splx (s);
	return 0;
}

static int ng_cp_rmnode (node_p node)
{
#if __FreeBSD_version >= 500000
	drv_t *d = NG_NODE_PRIVATE (node);

	CP_DEBUG (d, ("Rmnode\n"));
	if (d && d->running) {
		int s = splimp ();
		cp_down (d);
		splx (s);
	}
#ifdef	KLD_MODULE
	if (node->nd_flags & NGF_REALLY_DIE) {
		NG_NODE_SET_PRIVATE (node, NULL);
		NG_NODE_UNREF (node);
	}
	NG_NODE_REVIVE(node);		/* Persistant node */
#endif
#else /* __FreeBSD_version < 500000 */
	drv_t *d = node->private;

	if (d && d->running) {
		int s = splimp ();
		cp_down (d);
	splx (s);
	}

	node->flags |= NG_INVALID;
	ng_cutlinks (node);
#ifdef	KLD_MODULE
	ng_unname (node);
	ng_unref (node);
#endif
#endif
	return 0;
}

static void ng_cp_watchdog (void *arg)
{
	drv_t *d = arg;

	if (d) {
		if (d->timeout == 1)
			cp_watchdog (d);
		if (d->timeout)
			d->timeout--;
		d->timeout_handle = timeout (ng_cp_watchdog, d, hz);
	}
}

static int ng_cp_connect (hook_p hook)
{
#if __FreeBSD_version >= 500000
	drv_t *d = NG_NODE_PRIVATE (NG_HOOK_NODE (hook));
#else
	drv_t *d = hook->node->private;
#endif

	if (d) {
		CP_DEBUG (d, ("Connect\n"));
		d->timeout_handle = timeout (ng_cp_watchdog, d, hz);
	}
	
	return 0;
}

static int ng_cp_disconnect (hook_p hook)
{
#if __FreeBSD_version >= 500000
	drv_t *d = NG_NODE_PRIVATE (NG_HOOK_NODE (hook));
#else
	drv_t *d = hook->node->private;
#endif

	if (d) {
		CP_DEBUG (d, ("Disconnect\n"));
#if __FreeBSD_version >= 500000
		if (NG_HOOK_PRIVATE (hook))
#else
		if (hook->private)
#endif
		{
			int s = splimp ();
			cp_down (d);
			splx (s);
		}
		untimeout (ng_cp_watchdog, d, d->timeout_handle);
	}
	return 0;
}
#endif

static int cp_modevent (module_t mod, int type, void *unused)
{
	struct cdev *dev;
	static int load_count = 0;
	struct cdevsw *cdsw;

#if __FreeBSD_version >= 502103
	dev = findcdev (makedev(CDEV_MAJOR, 0));
#else
	dev = makedev (CDEV_MAJOR, 0);
#endif
	switch (type) {
	case MOD_LOAD:
		if (dev != NULL &&
		    (cdsw = devsw (dev)) &&
		    cdsw->d_maj == CDEV_MAJOR) {
			printf ("Tau-PCI driver is already in system\n");
			return (ENXIO);
		}
#if __FreeBSD_version >= 500000 && defined NETGRAPH
		if (ng_newtype (&typestruct))
			printf ("Failed to register ng_cp\n");
#endif
		++load_count;
#if __FreeBSD_version <= 500000
		cdevsw_add (&cp_cdevsw);
#endif
		timeout_handle = timeout (cp_timeout, 0, hz*5);
		break;
	case MOD_UNLOAD:
		if (load_count == 1) {
			printf ("Removing device entry for Tau-PCI\n");
#if __FreeBSD_version <= 500000
			cdevsw_remove (&cp_cdevsw);
#endif
#if __FreeBSD_version >= 500000 && defined NETGRAPH
			ng_rmtype (&typestruct);
#endif			
		}
		untimeout (cp_timeout, 0, timeout_handle);
		--load_count;
		break;
	case MOD_SHUTDOWN:
		break;
	}
	return 0;
}

#ifdef NETGRAPH
static struct ng_type typestruct = {
	.version	= NG_ABI_VERSION,
	.name		= NG_CP_NODE_TYPE,
	.constructor	= ng_cp_constructor,
	.rcvmsg		= ng_cp_rcvmsg,
	.shutdown	= ng_cp_rmnode,
	.newhook	= ng_cp_newhook,
	.connect	= ng_cp_connect,
	.rcvdata	= ng_cp_rcvdata,
	.disconnect	= ng_cp_disconnect,
};
#endif /*NETGRAPH*/

#if __FreeBSD_version >= 500000
#ifdef NETGRAPH
MODULE_DEPEND (ng_cp, netgraph, NG_ABI_VERSION, NG_ABI_VERSION, NG_ABI_VERSION);
#else
MODULE_DEPEND (cp, sppp, 1, 1, 1);
#endif
#ifdef KLD_MODULE
DRIVER_MODULE (cpmod, pci, cp_driver, cp_devclass, cp_modevent, NULL);
#else
DRIVER_MODULE (cp, pci, cp_driver, cp_devclass, cp_modevent, NULL);
#endif
#elif __FreeBSD_version >= 400000
#ifdef NETGRAPH
DRIVER_MODULE (cp, pci, cp_driver, cp_devclass, ng_mod_event, &typestruct);
#else
DRIVER_MODULE (cp, pci, cp_driver, cp_devclass, cp_modevent, NULL);
#endif
#endif /* __FreeBSD_version >= 400000 */
#endif /* NPCI */
