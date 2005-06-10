/*-
 * Granch SBNI16 G.SHDSL Modem driver
 * Written by Denis I. Timofeev, 2002-2003.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/random.h>
#include <machine/clock.h>
#include <machine/stdarg.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <net/bpf.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/sbsh/if_sbshreg.h>

/* -------------------------------------------------------------------------- */

struct sbni16_hw_regs {
	u_int8_t  CR, CRB, SR, IMR, CTDR, LTDR, CRDR, LRDR;
};

struct hw_descr {
	u_int32_t  address;
	u_int32_t  length;
};

struct cx28975_cmdarea {
	u_int8_t  intr_host;
	u_int8_t  intr_8051;
	u_int8_t  map_version;

	u_int8_t  in_dest;
	u_int8_t  in_opcode;
	u_int8_t  in_zero;
	u_int8_t  in_length;
	u_int8_t  in_csum;
	u_int8_t  in_data[75];
	u_int8_t  in_datasum;

	u_int8_t  out_dest;
	u_int8_t  out_opcode;
	u_int8_t  out_ack;
	u_int8_t  out_length;
	u_int8_t  out_csum;
	u_int8_t  out_data[75];
	u_int8_t  out_datasum;
};

#define XQLEN	8
#define RQLEN	8

struct sbsh_softc {
	struct ifnet	*ifp;

	struct resource	*mem_res;
	struct resource	*irq_res;
	void		*intr_hand;

	void		*mem_base;		/* mapped memory address */

	volatile struct sbni16_hw_regs	*regs;
	volatile struct hw_descr	*tbd;
	volatile struct hw_descr	*rbd;
	volatile struct cx28975_cmdarea	*cmdp;

	/* SBNI16 controller statistics */
	struct sbni16_stats {
		u_int32_t  sent_pkts, rcvd_pkts;
		u_int32_t  crc_errs, ufl_errs, ofl_errs, attempts, last_time;
	} in_stats;

	/* transmit and reception queues */
	struct mbuf	*xq[XQLEN], *rq[RQLEN];
	unsigned	head_xq, tail_xq, head_rq, tail_rq;

	/* the descriptors mapped onto the first buffers in xq and rq */
	unsigned	head_tdesc, head_rdesc;
	u_int8_t	state;
};

struct cx28975_cfg {
	u_int8_t   *firmw_image;
	u_int32_t  firmw_len;
	u_int32_t  lrate: 10;
	u_int32_t  master: 1;
	u_int32_t  mod: 2;
	u_int32_t  crc16: 1;
	u_int32_t  fill_7e: 1;
	u_int32_t  inv: 1;
	u_int32_t  rburst: 1;
	u_int32_t  wburst: 1;
	u_int32_t  : 14;
};

/* SHDSL transceiver statistics */
struct dsl_stats {
	u_int8_t	status_1, status_3;
	u_int8_t	attenuat, nmr, tpbo, rpbo;
	u_int16_t	losw, segd, crc, sega, losd;
};

enum State { NOT_LOADED, DOWN, ACTIVATION, ACTIVE };

#define	SIOCLOADFIRMW	_IOWR('i', 67, struct ifreq)
#define SIOCGETSTATS	_IOWR('i', 68, struct ifreq)
#define SIOCCLRSTATS	_IOWR('i', 69, struct ifreq)

static int	sbsh_probe(device_t);
static int	sbsh_attach(device_t);
static int	sbsh_detach(device_t);
static int	sbsh_ioctl(struct ifnet	*, u_long, caddr_t);
static void	sbsh_shutdown(device_t);
static int	sbsh_suspend(device_t);
static int	sbsh_resume(device_t);
static void	sbsh_watchdog(struct ifnet *);

static void	sbsh_start(struct ifnet *);
static void	sbsh_init(void *);
static void	sbsh_stop(struct sbsh_softc *);
static void	init_card(struct sbsh_softc *);
static void	sbsh_intr(void *);
static void	resume_tx(struct sbsh_softc *);
static void	start_xmit_frames(struct sbsh_softc *);
static void	encap_frame(struct sbsh_softc *, struct mbuf *);
static struct mbuf *	repack(struct sbsh_softc *, struct mbuf *);
static void	free_sent_buffers(struct sbsh_softc *);
static void	alloc_rx_buffers(struct sbsh_softc *);
static void	indicate_frames(struct sbsh_softc *);
static void	drop_queues(struct sbsh_softc *);
static void	activate(struct sbsh_softc *);
static void	deactivate(struct sbsh_softc *);
static void	cx28975_interrupt(struct sbsh_softc *);
static int	start_cx28975(struct sbsh_softc *, struct cx28975_cfg);
static int	download_firmware(struct sbsh_softc *, u_int8_t *, u_int32_t);
static int	issue_cx28975_cmd(struct sbsh_softc *, u_int8_t,
					u_int8_t *, u_int8_t);

static device_method_t sbsh_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		sbsh_probe),
	DEVMETHOD(device_attach,	sbsh_attach),
	DEVMETHOD(device_detach,	sbsh_detach),
	DEVMETHOD(device_shutdown,	sbsh_shutdown),
	DEVMETHOD(device_suspend,	sbsh_suspend),
	DEVMETHOD(device_resume,	sbsh_resume),

	{ 0, 0 }
};

static driver_t sbsh_driver = {
	"sbsh",
	sbsh_methods,
	sizeof(struct sbsh_softc)
};

static devclass_t sbsh_devclass;

DRIVER_MODULE(sbsh, pci, sbsh_driver, sbsh_devclass, 0, 0);
MODULE_DEPEND(sbsh, pci, 1, 1, 1);

static int
sbsh_probe(device_t dev)
{
	if (pci_get_vendor(dev) != SBNI16_VENDOR
	    || pci_get_device(dev) != SBNI16_DEVICE
	    || pci_get_subdevice(dev) != SBNI16_SUBDEV)
		return (ENXIO);

	device_set_desc(dev, "Granch SBNI16 G.SHDSL Modem");
	return (BUS_PROBE_DEFAULT);
}

static int
sbsh_attach(device_t dev)
{
	struct sbsh_softc	*sc;
	struct ifnet		*ifp;
	int			unit, error = 0, rid, s;
	u_char			eaddr[6];

	s = splimp();

	sc = device_get_softc(dev);
	unit = device_get_unit(dev);

	rid = PCIR_BAR(1);
	sc->mem_res = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid,
					0, ~0, 4096, RF_ACTIVE);

	if (sc->mem_res == NULL) {
		printf ("sbsh%d: couldn't map memory\n", unit);
		error = ENXIO;
		goto fail;
	}

	rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
						RF_SHAREABLE | RF_ACTIVE);

	if (sc->irq_res == NULL) {
		printf("sbsh%d: couldn't map interrupt\n", unit);
		bus_release_resource(dev, SYS_RES_MEMORY,
					PCIR_BAR(1), sc->mem_res);
		error = ENXIO;
		goto fail;
	}

	sc->mem_base = rman_get_virtual(sc->mem_res);
	init_card(sc);

	error = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_NET,
				sbsh_intr, sc, &sc->intr_hand);
	if (error) {
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->irq_res);
		bus_release_resource(dev, SYS_RES_MEMORY,
					PCIR_BAR(1), sc->mem_res);
		printf("sbsh%d: couldn't set up irq\n", unit);
		goto fail;
	}

	/* generate ethernet MAC address */
	*(u_int32_t *)eaddr = htonl(0x00ff0192);
	read_random(eaddr + 4, 2);

	ifp = sc->ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->irq_res);
		bus_release_resource(dev, SYS_RES_MEMORY,
					PCIR_BAR(1), sc->mem_res);
		bus_teardown_intr(dev, sc->irq_res, sc->intr_hand);
		printf("sbsh%d: can not if_alloc()\n", unit);
		goto fail;
	}
	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = sbsh_ioctl;
	ifp->if_start = sbsh_start;
	ifp->if_watchdog = sbsh_watchdog;
	ifp->if_init = sbsh_init;
	ifp->if_baudrate = 4600000;
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;

	ether_ifattach(ifp, eaddr);

fail:
	splx(s);
	return (error);
}

static int
sbsh_detach(device_t dev)
{
	struct sbsh_softc	*sc;
	struct ifnet		*ifp;
	int			s;

	s = splimp();

	sc = device_get_softc(dev);
	ifp = sc->ifp;

	sbsh_stop(sc);
	ether_ifdetach(ifp);
	if_free(ifp);

	bus_teardown_intr(dev, sc->irq_res, sc->intr_hand);
	bus_release_resource(dev, SYS_RES_IRQ, 0, sc->irq_res);
	bus_release_resource(dev, SYS_RES_MEMORY, PCIR_BAR(1), sc->mem_res);

	splx(s);
	return (0);
}


static void
sbsh_start(struct ifnet *ifp)
{
	struct sbsh_softc  *sc = ifp->if_softc;
	int  s;

	if (sc->state != ACTIVE)
		return;

	s = splimp();
	start_xmit_frames(ifp->if_softc);
	splx(s);
}


static void
sbsh_init(void *xsc)
{
	struct sbsh_softc	*sc = xsc;
	struct ifnet		*ifp = sc->ifp;
	int			s;
	u_int8_t		t;

	if ((ifp->if_flags & IFF_RUNNING) || sc->state == NOT_LOADED)
		return;

	s = splimp();

	bzero(&sc->in_stats, sizeof(struct sbni16_stats));
	sc->head_xq = sc->tail_xq = sc->head_rq = sc->tail_rq = 0;
	sc->head_tdesc = sc->head_rdesc = 0;

	sc->regs->IMR = EXT;
	t = 2;
	issue_cx28975_cmd(sc, _DSL_CLEAR_ERROR_CTRS, &t, 1);
	if (issue_cx28975_cmd(sc, _DSL_ACTIVATION, &t, 1) == 0) {
		sc->state = ACTIVATION;

		ifp->if_flags |= IFF_RUNNING;
		ifp->if_flags &= ~IFF_OACTIVE;
	}

	splx(s);
}


static void
sbsh_stop(struct sbsh_softc *sc)
{
	int  s;
	u_int8_t  t;

	s = splimp();
	sc->regs->IMR = EXT;

	t = 0;
	issue_cx28975_cmd(sc, _DSL_ACTIVATION, &t, 1);
	if (sc->state == ACTIVE) {
		t = 1;
		issue_cx28975_cmd(sc, _DSL_FORCE_DEACTIVATE, &t, 1);
		/* FIX! activation manager state */

		/* Is it really must be done here? It calls from intr handler */
		deactivate(sc);
	}

	sc->regs->IMR = 0;
	sc->state = DOWN;
	splx(s);
}


static void
init_card(struct sbsh_softc *sc)
{
	sc->state = NOT_LOADED;
	sc->tbd  = (struct hw_descr *) sc->mem_base;
	sc->rbd  = (struct hw_descr *) ((u_int8_t *)sc->mem_base + 0x400);
	sc->regs = (struct sbni16_hw_regs *) ((u_int8_t *)sc->mem_base + 0x800);
	sc->cmdp = (struct cx28975_cmdarea *) ((u_int8_t *)sc->mem_base + 0xc00);

	sc->regs->CR = 0;
	sc->regs->SR = 0xff;
	sc->regs->IMR = 0;
}


static int
sbsh_ioctl(struct ifnet	*ifp, u_long cmd, caddr_t data)
{
	struct sbsh_softc	*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *) data;
	struct cx28975_cfg	cfg;
	struct dsl_stats	ds;

	int			s, error = 0;
	u_int8_t		t;

	s = splimp();

	switch(cmd) {
	case SIOCLOADFIRMW:
		if ((error = suser(curthread)) != 0)
			break;
		if (ifp->if_flags & IFF_UP)
			error = EBUSY;

		bcopy((caddr_t)ifr->ifr_data, (caddr_t)&cfg, sizeof cfg);
		if (start_cx28975(sc, cfg) == 0) {
			static char  *modstr[] = {
				"TCPAM32", "TCPAM16", "TCPAM8", "TCPAM4" };
			if_printf(sc->ifp, "%s, rate %d, %s\n",
				cfg.master ? "master" : "slave",
				cfg.lrate << 3, modstr[cfg.mod]);
		} else {
			if_printf(sc->ifp,
				"unable to load firmware\n");
			error = EIO;
		}
		break;

	case  SIOCGETSTATS :
		if ((error = suser(curthread)) != 0)
			break;

		t = 0;
		if (issue_cx28975_cmd(sc, _DSL_FAR_END_ATTEN, &t, 1))
			error = EIO;
		ds.attenuat = sc->cmdp->out_data[0];

		if (issue_cx28975_cmd(sc, _DSL_NOISE_MARGIN, &t, 1))
			error = EIO;
		ds.nmr = sc->cmdp->out_data[0];

		if (issue_cx28975_cmd(sc, _DSL_POWER_BACK_OFF_RESULT, &t, 1))
			error = EIO;
		ds.tpbo = sc->cmdp->out_data[0];
		ds.rpbo = sc->cmdp->out_data[1];

		if (!issue_cx28975_cmd(sc, _DSL_HDSL_PERF_ERR_CTRS, &t, 1)) {
			int i;
			for (i = 0; i < 10; ++i)
				((u_int8_t *) &ds.losw)[i] =
					sc->cmdp->out_data[i];
		} else
			error = EIO;

		ds.status_1 = ((volatile u_int8_t *)sc->cmdp)[0x3c0];
		ds.status_3 = ((volatile u_int8_t *)sc->cmdp)[0x3c2];

		bcopy(&sc->in_stats, ifr->ifr_data, sizeof(struct sbni16_stats));
		bcopy(&ds, ifr->ifr_data + sizeof(struct sbni16_stats),
		    sizeof(struct dsl_stats));
		break;

	case  SIOCCLRSTATS :
		if (!(error = suser(curthread))) {
			bzero(&sc->in_stats, sizeof(struct sbni16_stats));
			t = 2;
			if (issue_cx28975_cmd(sc, _DSL_CLEAR_ERROR_CTRS, &t, 1))
				error = EIO;
		}
		break;

	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (!(ifp->if_flags & IFF_RUNNING)) {
				if (sc->state == NOT_LOADED) {
					if_printf(ifp, "firmware wasn't loaded\n");
					error = EBUSY;
				} else
					sbsh_init(sc);
			}
		} else {
			if (ifp->if_flags & IFF_RUNNING) {
				sbsh_stop(sc);
				ifp->if_flags &= ~IFF_RUNNING;
			}
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = 0;
		break;

	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}

	splx(s);
	return (error);
}


static void
sbsh_shutdown(device_t dev)
{
	struct sbsh_softc	*sc = device_get_softc(dev);

	sbsh_stop(sc);
}

static int
sbsh_suspend(device_t dev)
{
	struct sbsh_softc	*sc = device_get_softc(dev);
	int			s;

	s = splimp();
	sbsh_stop(sc);
	splx(s);

	return (0);
}

static int
sbsh_resume(device_t dev)
{
	struct sbsh_softc	*sc = device_get_softc(dev);
	struct ifnet		*ifp;
	int			s;

	s = splimp();
	ifp = sc->ifp;

	if (ifp->if_flags & IFF_UP)
		sbsh_init(sc);

	splx(s);
	return (0);
}


static void
sbsh_watchdog(struct ifnet *ifp)
{
	struct sbsh_softc	*sc = ifp->if_softc;

	if_printf(ifp, "transmit timeout\n");

	if (sc->regs->SR & TXS) {
		sc->regs->SR = TXS;
		if_printf(ifp, "interrupt posted but not delivered\n");
	}
	free_sent_buffers(sc);
}

/* -------------------------------------------------------------------------- */

static void
sbsh_intr(void *arg)
{
	struct sbsh_softc  *sc = (struct sbsh_softc *)arg;
	u_int8_t  status = sc->regs->SR;

	if (status == 0)
		return;

	if (status & EXT) {
		cx28975_interrupt(sc);
		sc->regs->SR = EXT;
	}

	if (status & UFL) {
		resume_tx(sc);
		sc->regs->SR = UFL;
		++sc->in_stats.ufl_errs;
		++sc->ifp->if_oerrors;
	}

	if (status & RXS) {
		sc->regs->SR = RXS;
		indicate_frames(sc);
		alloc_rx_buffers(sc);
	}

	if (status & TXS) {
		sc->regs->SR = TXS;
		free_sent_buffers(sc);
	}

	if (status & CRC) {
		++sc->in_stats.crc_errs;
		++sc->ifp->if_ierrors;
		sc->regs->SR = CRC;
	}

	if (status & OFL) {
		++sc->in_stats.ofl_errs;
		++sc->ifp->if_ierrors;
		sc->regs->SR = OFL;
	}
}

/*
 * Look for a first descriptor of a next packet, and write it's number
 * into CTDR. Then enable the transmitter.
 */
static void
resume_tx(struct sbsh_softc *sc)
{
	u_int32_t	cur_tbd = sc->regs->CTDR;

	while (cur_tbd != sc->regs->LTDR
		&& (sc->tbd[cur_tbd++].length & LAST_FRAG) == 0)
		;
	sc->regs->CTDR = cur_tbd;
	sc->regs->CR |= TXEN;
}

static void
start_xmit_frames(struct sbsh_softc *sc)
{
	struct ifnet	*ifp = sc->ifp;
	struct mbuf	*m;

	/*
	 * Check if we have any free descriptor(s) and free space in
	 * our transmit queue.
	 */
	while (sc->tail_xq != ((sc->head_xq - 1) & (XQLEN - 1))
	    && sc->regs->LTDR != ((sc->head_tdesc - 1) & 0x7f)) {

		IF_DEQUEUE(&ifp->if_snd, m);
		if (!m)
			break;
		if (m->m_pkthdr.len) {
			BPF_MTAP(ifp, m);
			encap_frame(sc, m);
		} else
			m_freem(m);
	}

	if (sc->regs->CTDR != sc->regs->LTDR)
		ifp->if_flags |= IFF_OACTIVE;
	else
		ifp->if_flags &= ~IFF_OACTIVE;
}


/*
 * MUST be called at splimp
 */
static void
encap_frame(struct sbsh_softc *sc, struct mbuf *m_head)
{
	struct mbuf	*m;
	u_int32_t	cur_tbd;
	int  done;

look_for_nonzero:
	for (m = m_head; !m->m_len; m = m->m_next)
		;

	cur_tbd = sc->regs->LTDR & 0x7f;
	done = 0;
	do {
		if (m->m_len < 5 || cur_tbd == ((sc->head_tdesc - 1) & 0x7f)) {
			if ((m_head = repack(sc, m_head)) != NULL)
				goto look_for_nonzero;
			else
				return;
		}

		sc->tbd[cur_tbd].address = vtophys(mtod(m, vm_offset_t));
		sc->tbd[cur_tbd].length  = m->m_len;

		do {
			m = m->m_next;
		} while (m && !m->m_len);

		if (!m) {	/* last fragment has been reached */
			sc->tbd[cur_tbd].length |= LAST_FRAG;
			done = 1;
		}

		++cur_tbd;
		cur_tbd &= 0x7f;
	} while (!done);

	sc->xq[sc->tail_xq++] = m_head;
	sc->tail_xq &= (XQLEN - 1);

	sc->regs->LTDR = cur_tbd;
	++sc->in_stats.sent_pkts;
	++sc->ifp->if_opackets;
}

static struct mbuf *
repack(struct sbsh_softc *sc, struct mbuf *m)
{
	struct mbuf  *m_new;

	MGETHDR(m_new, M_DONTWAIT, MT_DATA);
	if (!m_new) {
		if_printf (sc->ifp,
			   "unable to get mbuf.\n");
		return (NULL);
	}

	if (m->m_pkthdr.len > MHLEN) {
		MCLGET(m_new, M_DONTWAIT);
		if (!(m_new->m_flags & M_EXT)) {
			m_freem(m_new);
			if_printf (sc->ifp,
				   "unable to get mbuf cluster.\n");
			return (NULL);
		}
	}

	m_copydata(m, 0, m->m_pkthdr.len, mtod(m_new, caddr_t));
	m_new->m_pkthdr.len = m_new->m_len = m->m_pkthdr.len;
	m_freem(m);
	return (m_new);
}

static void
free_sent_buffers(struct sbsh_softc *sc)
{
	u_int32_t  cur_tbd;

	cur_tbd = sc->regs->CTDR;

	while (sc->head_tdesc != cur_tbd) {
		/*
		 * Be careful! one element in xq may correspond to
		 * multiple descriptors.
		 */
		if (sc->tbd[sc->head_tdesc].length & LAST_FRAG) {
			m_freem(sc->xq[sc->head_xq++]);
			sc->head_xq &= (XQLEN - 1);
		}

		sc->tbd[sc->head_tdesc].length = 0;
		sc->head_tdesc = (sc->head_tdesc + 1) & 0x7f;
	}

	start_xmit_frames(sc);
}

/*
 * DON'T use free_sent_buffers to drop the queue!
 */
static void
alloc_rx_buffers(struct sbsh_softc *sc)
{
	unsigned	cur_rbd = sc->regs->LRDR & 0x7f;
	struct mbuf	*m;

	while (sc->tail_rq != ((sc->head_rq - 1) & (RQLEN - 1))) {
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (!m) {
			if_printf (sc->ifp,
				   "unable to get mbuf.\n");
			return;
		}

		if (SBNI16_MAX_FRAME > MHLEN) {
			MCLGET(m, M_DONTWAIT);
			if (!(m->m_flags & M_EXT)) {
				m_freem(m);
				if_printf (sc->ifp,
					   "unable to get mbuf cluster.\n");
				return;
			}
			m->m_pkthdr.len = m->m_len = MCLBYTES;
		}

		m_adj(m, 2);	/* align ip on longword boundaries */

		sc->rq[sc->tail_rq++] = m;
		sc->tail_rq &= (RQLEN - 1);

		sc->rbd[cur_rbd].address = vtophys(mtod(m, vm_offset_t));
		sc->rbd[cur_rbd].length  = 0;
		sc->regs->LRDR = cur_rbd = (cur_rbd + 1) & 0x7f;
	}
}

static void
indicate_frames(struct sbsh_softc *sc)
{
	unsigned  cur_rbd = sc->regs->CRDR & 0x7f;

	while (sc->head_rdesc != cur_rbd) {
		struct mbuf  *m = sc->rq[sc->head_rq++];
		sc->head_rq &= (RQLEN - 1);

		m->m_pkthdr.len = m->m_len =
				sc->rbd[sc->head_rdesc].length & 0x7ff;
		m->m_pkthdr.rcvif = sc->ifp;

		(*sc->ifp->if_input)(sc->ifp, m);
		++sc->in_stats.rcvd_pkts;
		++sc->ifp->if_ipackets;

		sc->head_rdesc = (sc->head_rdesc + 1) & 0x7f;
	}
}

static void
drop_queues(struct sbsh_softc *sc)
{
	while (sc->head_rq != sc->tail_rq) {
		m_freem(sc->rq[sc->head_rq++]);
		sc->head_rq &= (RQLEN - 1);
	}

	while (sc->head_xq != sc->tail_xq) {
		m_freem(sc->xq[sc->head_xq++]);
		sc->head_xq &= (XQLEN - 1);
	}
}

/* -------------------------------------------------------------------------- */

static void
activate(struct sbsh_softc *sc)
{
	struct timeval	tv;

	sc->regs->SR   = 0xff;		/* clear it! */
	sc->regs->CTDR = sc->regs->LTDR = sc->regs->CRDR = sc->regs->LRDR = 0;

	sc->head_tdesc = sc->head_rdesc = 0;
	alloc_rx_buffers(sc);

	sc->regs->CRB &= ~RXDE;
	sc->regs->IMR = EXT | RXS | TXS | CRC | OFL | UFL;
	sc->regs->CR |= TXEN | RXEN;

	sc->state = ACTIVE;
	++sc->in_stats.attempts;
	microtime(&tv);
	sc->in_stats.last_time = tv.tv_sec;
	start_xmit_frames(sc);
}

static void
deactivate(struct sbsh_softc *sc)
{
	sc->regs->CR &= ~(RXEN | TXEN);
	sc->regs->CRB |= RXDE;
	sc->regs->IMR  = EXT;
	sc->regs->CTDR = sc->regs->LTDR;
	sc->regs->CRDR = sc->regs->LRDR;
	sc->state = ACTIVATION;

	drop_queues(sc);
}

/* -------------------------------------------------------------------------- */

static void
cx28975_interrupt(struct sbsh_softc *sc)
{
	volatile struct cx28975_cmdarea  *p = sc->cmdp;
	u_int8_t  t;

	if (p->intr_host != 0xfe)
		return;

	if (p->out_ack & 0x80) {
		if (*((volatile u_int8_t *)p + 0x3c7) & 2) {
			if (sc->state != ACTIVE
			    && (*((volatile u_int8_t *)p + 0x3c0) & 0xc0) == 0x40) {
				activate(sc);
				if_printf(sc->ifp, "connected to peer\n");
			} else if (sc->state == ACTIVE
				 && (*((volatile u_int8_t *)p + 0x3c0) & 0xc0) != 0x40) {
				deactivate(sc);
				if_printf(sc->ifp, "carrier lost\n");
			}
		}

		p->intr_host = 0;
		t = p->intr_host;
		p->out_ack = 0;
	} else {
		wakeup(sc);

		p->intr_host = 0;
		t = p->intr_host;
	}
}

/* -------------------------------------------------------------------------- */

static int
start_cx28975(struct sbsh_softc *sc, struct cx28975_cfg cfg)
{
	static char  thresh[] = { +8, -4, -16, -40 };

	volatile struct cx28975_cmdarea  *p = sc->cmdp;
	u_int8_t  t, parm[12];

	p->intr_host = 0;
	t = p->intr_host;

	/* reset chip set */
	sc->regs->IMR = EXT;
	sc->regs->CR  = 0;
	sc->regs->SR  = 0xff;
	DELAY(2);
	sc->regs->CR = XRST;
	if (cfg.crc16)
		sc->regs->CR |= CMOD;
	if (cfg.fill_7e)
		sc->regs->CR |= FMOD;
	if (cfg.inv)
		sc->regs->CR |= PMOD;

	sc->regs->CRB |= RODD | RXDE;
	if (cfg.rburst)
		sc->regs->CRB |= RDBE;
	if (cfg.wburst)
		sc->regs->CRB |= WTBE;

	tsleep(sc, PWAIT, "sbsh", 0);
	if ((p->out_ack & 0x1f) != _ACK_BOOT_WAKE_UP)
		return (-1);

	if (download_firmware(sc, cfg.firmw_image, cfg.firmw_len))
		return (-1);

	tsleep(sc, PWAIT, "sbsh", 0);
	if ((p->out_ack & 0x1f) != _ACK_OPER_WAKE_UP)
		return (-1);

	t = cfg.master ? 1 : 9;
	if (issue_cx28975_cmd(sc, _DSL_SYSTEM_ENABLE, &t, 1))
		return (-1);

	t = 0x63;
	if (issue_cx28975_cmd(sc, _DSL_SYSTEM_CONFIG, &t, 1))
		return (-1);

	*(u_int16_t *)parm = cfg.lrate >> 3;
	parm[2] = parm[3] = parm[0];
	parm[5] = cfg.lrate & 7;
	parm[4] = parm[7] = 1;
	parm[6] = 0;
	if (issue_cx28975_cmd(sc, _DSL_MULTI_RATE_CONFIG, parm, 8))
		return (-1);

	parm[0] = 0x02 | (cfg.mod << 4);
	parm[1] = 0;
	if (issue_cx28975_cmd(sc, _DSL_TRAINING_MODE, parm, 2))
		return (-1);

	bzero(parm, 12);
	parm[0] = 0x04;		/* pre-activation: G.hs */
	parm[4] = 0x04;		/* no remote configuration */
	parm[7] = 0x01;		/* annex A (default) */
	parm[8] = 0xff;		/* i-bit mask (all bits) */
	if (issue_cx28975_cmd(sc, _DSL_PREACTIVATION_CFG, parm, 12))
		return (-1);

	parm[0] = 0x03;		/* dying gasp time - 3 frames */
	parm[1] = thresh[cfg.mod];
	parm[2] = 0xff;		/* attenuation */
	parm[3] = 0x04;		/* line probe NMR (+2 dB) */
	parm[4] = 0x00;		/* reserved */
	parm[5] = 0x00;
	if (issue_cx28975_cmd(sc, _DSL_THRESHOLDS, parm, 6))
		return (-1);

	t = cfg.master ? 0x23 : 0x21;
	if (issue_cx28975_cmd(sc, _DSL_FR_PCM_CONFIG, &t, 1))
		return (-1);

	t = 0x02;
	if (issue_cx28975_cmd(sc, _DSL_INTR_HOST_MASK, &t, 1))
		return (-1);

	sc->state = DOWN;
	return (0);
}

static int
download_firmware(struct sbsh_softc *sc, u_int8_t *img, u_int32_t img_len)
{
	u_int32_t	t;
	int		i;
	u_int8_t	cksum = 0;

	for (i = 0; i < img_len; ++i)
		cksum += img[i];

	t = img_len;
	if (issue_cx28975_cmd(sc, _DSL_DOWNLOAD_START, (u_int8_t *) &t, 4))
		return (-1);

	for (i = 0; img_len >= 75; i += 75, img_len -= 75) {
		if (issue_cx28975_cmd(sc, _DSL_DOWNLOAD_DATA, img + i, 75))
			return (-1);
	}

	if (img_len
	    &&  issue_cx28975_cmd(sc, _DSL_DOWNLOAD_DATA, img + i, img_len))
		return (-1);

	t = (cksum ^ 0xff) + 1;
	if (issue_cx28975_cmd(sc, _DSL_DOWNLOAD_END, (u_int8_t *) &t, 1))
		return (-1);

	return (0);
}

static int
issue_cx28975_cmd(struct sbsh_softc *sc, u_int8_t cmd,
			u_int8_t *data, u_int8_t size)
{
	volatile struct cx28975_cmdarea  *p = sc->cmdp;
	u_int8_t  *databuf = p->in_data;
	int  i;

	u_int8_t  cksum = 0;

	p->in_dest	= 0xf0;
	p->in_opcode	= cmd;
	p->in_zero	= 0;
	p->in_length	= --size;
	p->in_csum	= 0xf0 ^ cmd ^ size ^ 0xaa;

	for (i = 0; i <= size; ++i) {
		cksum ^= *data;
		*databuf++ = *data++;	/* only 1 byte per cycle! */
	}

	p->in_datasum	= cksum ^ 0xaa;
	p->out_ack	= _ACK_NOT_COMPLETE;
	p->intr_8051	= 0xfe;

	if (tsleep(sc, PWAIT, "sbsh", hz << 3))
		return (-1);

	while (p->out_ack == _ACK_NOT_COMPLETE)
		;					/* FIXME ! */

	if ((p->out_ack & 0x1f) == _ACK_PASS) {
		p->out_ack = 0;
		return (0);
	} else {
		p->out_ack = 0;
		return (-1);
	}
}
