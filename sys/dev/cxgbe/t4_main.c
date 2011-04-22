/*-
 * Copyright (c) 2011 Chelsio Communications, Inc.
 * All rights reserved.
 * Written by: Navdeep Parhar <np@FreeBSD.org>
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

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/priv.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>
#include <sys/pciio.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pci_private.h>
#include <sys/firmware.h>
#include <sys/sbuf.h>
#include <sys/smp.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>

#include "common/t4_hw.h"
#include "common/common.h"
#include "common/t4_regs.h"
#include "common/t4_regs_values.h"
#include "common/t4fw_interface.h"
#include "t4_ioctl.h"

/* T4 bus driver interface */
static int t4_probe(device_t);
static int t4_attach(device_t);
static int t4_detach(device_t);
static device_method_t t4_methods[] = {
	DEVMETHOD(device_probe,		t4_probe),
	DEVMETHOD(device_attach,	t4_attach),
	DEVMETHOD(device_detach,	t4_detach),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	{ 0, 0 }
};
static driver_t t4_driver = {
	"t4nex",
	t4_methods,
	sizeof(struct adapter)
};


/* T4 port (cxgbe) interface */
static int cxgbe_probe(device_t);
static int cxgbe_attach(device_t);
static int cxgbe_detach(device_t);
static device_method_t cxgbe_methods[] = {
	DEVMETHOD(device_probe,		cxgbe_probe),
	DEVMETHOD(device_attach,	cxgbe_attach),
	DEVMETHOD(device_detach,	cxgbe_detach),
	{ 0, 0 }
};
static driver_t cxgbe_driver = {
	"cxgbe",
	cxgbe_methods,
	sizeof(struct port_info)
};

static d_ioctl_t t4_ioctl;
static d_open_t t4_open;
static d_close_t t4_close;

static struct cdevsw t4_cdevsw = {
       .d_version = D_VERSION,
       .d_flags = 0,
       .d_open = t4_open,
       .d_close = t4_close,
       .d_ioctl = t4_ioctl,
       .d_name = "t4nex",
};

/* ifnet + media interface */
static void cxgbe_init(void *);
static int cxgbe_ioctl(struct ifnet *, unsigned long, caddr_t);
static void cxgbe_start(struct ifnet *);
static int cxgbe_transmit(struct ifnet *, struct mbuf *);
static void cxgbe_qflush(struct ifnet *);
static int cxgbe_media_change(struct ifnet *);
static void cxgbe_media_status(struct ifnet *, struct ifmediareq *);

MALLOC_DEFINE(M_CXGBE, "cxgbe", "Chelsio T4 Ethernet driver and services");

/*
 * Tunables.
 */
SYSCTL_NODE(_hw, OID_AUTO, cxgbe, CTLFLAG_RD, 0, "cxgbe driver parameters");

static int force_firmware_install = 0;
TUNABLE_INT("hw.cxgbe.force_firmware_install", &force_firmware_install);
SYSCTL_UINT(_hw_cxgbe, OID_AUTO, force_firmware_install, CTLFLAG_RDTUN,
    &force_firmware_install, 0, "install firmware on every attach.");

/*
 * Holdoff timer and packet counter values.
 */
static unsigned int intr_timer[SGE_NTIMERS] = {1, 5, 10, 50, 100, 200};
static unsigned int intr_pktcount[SGE_NCOUNTERS] = {1, 8, 16, 32}; /* 63 max */

/*
 * Max # of tx and rx queues to use for each 10G and 1G port.
 */
static unsigned int max_ntxq_10g = 8;
TUNABLE_INT("hw.cxgbe.max_ntxq_10G_port", &max_ntxq_10g);
SYSCTL_UINT(_hw_cxgbe, OID_AUTO, max_ntxq_10G_port, CTLFLAG_RDTUN,
    &max_ntxq_10g, 0, "maximum number of tx queues per 10G port.");

static unsigned int max_nrxq_10g = 8;
TUNABLE_INT("hw.cxgbe.max_nrxq_10G_port", &max_nrxq_10g);
SYSCTL_UINT(_hw_cxgbe, OID_AUTO, max_nrxq_10G_port, CTLFLAG_RDTUN,
    &max_nrxq_10g, 0, "maximum number of rxq's (per 10G port).");

static unsigned int max_ntxq_1g = 2;
TUNABLE_INT("hw.cxgbe.max_ntxq_1G_port", &max_ntxq_1g);
SYSCTL_UINT(_hw_cxgbe, OID_AUTO, max_ntxq_1G_port, CTLFLAG_RDTUN,
    &max_ntxq_1g, 0, "maximum number of tx queues per 1G port.");

static unsigned int max_nrxq_1g = 2;
TUNABLE_INT("hw.cxgbe.max_nrxq_1G_port", &max_nrxq_1g);
SYSCTL_UINT(_hw_cxgbe, OID_AUTO, max_nrxq_1G_port, CTLFLAG_RDTUN,
    &max_nrxq_1g, 0, "maximum number of rxq's (per 1G port).");

/*
 * Holdoff parameters for 10G and 1G ports.
 */
static unsigned int tmr_idx_10g = 1;
TUNABLE_INT("hw.cxgbe.holdoff_timer_idx_10G", &tmr_idx_10g);
SYSCTL_UINT(_hw_cxgbe, OID_AUTO, holdoff_timer_idx_10G, CTLFLAG_RDTUN,
    &tmr_idx_10g, 0,
    "default timer index for interrupt holdoff (10G ports).");

static int pktc_idx_10g = 2;
TUNABLE_INT("hw.cxgbe.holdoff_pktc_idx_10G", &pktc_idx_10g);
SYSCTL_UINT(_hw_cxgbe, OID_AUTO, holdoff_pktc_idx_10G, CTLFLAG_RDTUN,
    &pktc_idx_10g, 0,
    "default pkt counter index for interrupt holdoff (10G ports).");

static unsigned int tmr_idx_1g = 1;
TUNABLE_INT("hw.cxgbe.holdoff_timer_idx_1G", &tmr_idx_1g);
SYSCTL_UINT(_hw_cxgbe, OID_AUTO, holdoff_timer_idx_1G, CTLFLAG_RDTUN,
    &tmr_idx_1g, 0,
    "default timer index for interrupt holdoff (1G ports).");

static int pktc_idx_1g = 2;
TUNABLE_INT("hw.cxgbe.holdoff_pktc_idx_1G", &pktc_idx_1g);
SYSCTL_UINT(_hw_cxgbe, OID_AUTO, holdoff_pktc_idx_1G, CTLFLAG_RDTUN,
    &pktc_idx_1g, 0,
    "default pkt counter index for interrupt holdoff (1G ports).");

/*
 * Size (# of entries) of each tx and rx queue.
 */
static unsigned int qsize_txq = TX_EQ_QSIZE;
TUNABLE_INT("hw.cxgbe.qsize_txq", &qsize_txq);
SYSCTL_UINT(_hw_cxgbe, OID_AUTO, qsize_txq, CTLFLAG_RDTUN,
    &qsize_txq, 0, "default queue size of NIC tx queues.");

static unsigned int qsize_rxq = RX_IQ_QSIZE;
TUNABLE_INT("hw.cxgbe.qsize_rxq", &qsize_rxq);
SYSCTL_UINT(_hw_cxgbe, OID_AUTO, qsize_rxq, CTLFLAG_RDTUN,
    &qsize_rxq, 0, "default queue size of NIC rx queues.");

/*
 * Interrupt types allowed.
 */
static int intr_types = INTR_MSIX | INTR_MSI | INTR_INTX;
TUNABLE_INT("hw.cxgbe.interrupt_types", &intr_types);
SYSCTL_UINT(_hw_cxgbe, OID_AUTO, interrupt_types, CTLFLAG_RDTUN, &intr_types, 0,
    "interrupt types allowed (bits 0, 1, 2 = INTx, MSI, MSI-X respectively)");

/*
 * Force the driver to use interrupt forwarding.
 */
static int intr_fwd = 0;
TUNABLE_INT("hw.cxgbe.interrupt_forwarding", &intr_fwd);
SYSCTL_UINT(_hw_cxgbe, OID_AUTO, interrupt_forwarding, CTLFLAG_RDTUN,
    &intr_fwd, 0, "always use forwarded interrupts");

struct intrs_and_queues {
	int intr_type;		/* INTx, MSI, or MSI-X */
	int nirq;		/* Number of vectors */
	int intr_fwd;		/* Interrupts forwarded */
	int ntxq10g;		/* # of NIC txq's for each 10G port */
	int nrxq10g;		/* # of NIC rxq's for each 10G port */
	int ntxq1g;		/* # of NIC txq's for each 1G port */
	int nrxq1g;		/* # of NIC rxq's for each 1G port */
};

enum {
	MEMWIN0_APERTURE = 2048,
	MEMWIN0_BASE     = 0x1b800,
	MEMWIN1_APERTURE = 32768,
	MEMWIN1_BASE     = 0x28000,
	MEMWIN2_APERTURE = 65536,
	MEMWIN2_BASE     = 0x30000,
};

enum {
	XGMAC_MTU	= (1 << 0),
	XGMAC_PROMISC	= (1 << 1),
	XGMAC_ALLMULTI	= (1 << 2),
	XGMAC_VLANEX	= (1 << 3),
	XGMAC_UCADDR	= (1 << 4),
	XGMAC_MCADDRS	= (1 << 5),

	XGMAC_ALL	= 0xffff
};

static int map_bars(struct adapter *);
static void setup_memwin(struct adapter *);
static int cfg_itype_and_nqueues(struct adapter *, int, int,
    struct intrs_and_queues *);
static int prep_firmware(struct adapter *);
static int get_capabilities(struct adapter *, struct fw_caps_config_cmd *);
static int get_params(struct adapter *, struct fw_caps_config_cmd *);
static void t4_set_desc(struct adapter *);
static void build_medialist(struct port_info *);
static int update_mac_settings(struct port_info *, int);
static int cxgbe_init_locked(struct port_info *);
static int cxgbe_init_synchronized(struct port_info *);
static int cxgbe_uninit_locked(struct port_info *);
static int cxgbe_uninit_synchronized(struct port_info *);
static int first_port_up(struct adapter *);
static int last_port_down(struct adapter *);
static int t4_alloc_irq(struct adapter *, struct irq *, int rid,
    iq_intr_handler_t *, void *, char *);
static int t4_free_irq(struct adapter *, struct irq *);
static void reg_block_dump(struct adapter *, uint8_t *, unsigned int,
    unsigned int);
static void t4_get_regs(struct adapter *, struct t4_regdump *, uint8_t *);
static void cxgbe_tick(void *);
static int t4_sysctls(struct adapter *);
static int cxgbe_sysctls(struct port_info *);
static int sysctl_int_array(SYSCTL_HANDLER_ARGS);
static int sysctl_holdoff_tmr_idx(SYSCTL_HANDLER_ARGS);
static int sysctl_holdoff_pktc_idx(SYSCTL_HANDLER_ARGS);
static int sysctl_qsize_rxq(SYSCTL_HANDLER_ARGS);
static int sysctl_qsize_txq(SYSCTL_HANDLER_ARGS);
static int sysctl_handle_t4_reg64(SYSCTL_HANDLER_ARGS);
static inline void txq_start(struct ifnet *, struct sge_txq *);
static int t4_mod_event(module_t, int, void *);

struct t4_pciids {
	uint16_t device;
	uint8_t mpf;
	char *desc;
} t4_pciids[] = {
	{0xa000, 0, "Chelsio Terminator 4 FPGA"},
	{0x4400, 4, "Chelsio T440-dbg"},
	{0x4401, 4, "Chelsio T420-CR"},
	{0x4402, 4, "Chelsio T422-CR"},
	{0x4403, 4, "Chelsio T440-CR"},
	{0x4404, 4, "Chelsio T420-BCH"},
	{0x4405, 4, "Chelsio T440-BCH"},
	{0x4406, 4, "Chelsio T440-CH"},
	{0x4407, 4, "Chelsio T420-SO"},
	{0x4408, 4, "Chelsio T420-CX"},
	{0x4409, 4, "Chelsio T420-BT"},
	{0x440a, 4, "Chelsio T404-BT"},
};

static int
t4_probe(device_t dev)
{
	int i;
	uint16_t v = pci_get_vendor(dev);
	uint16_t d = pci_get_device(dev);

	if (v != PCI_VENDOR_ID_CHELSIO)
		return (ENXIO);

	for (i = 0; i < ARRAY_SIZE(t4_pciids); i++) {
		if (d == t4_pciids[i].device &&
		    pci_get_function(dev) == t4_pciids[i].mpf) {
			device_set_desc(dev, t4_pciids[i].desc);
			return (BUS_PROBE_DEFAULT);
		}
	}

	return (ENXIO);
}

static int
t4_attach(device_t dev)
{
	struct adapter *sc;
	int rc = 0, i, n10g, n1g, rqidx, tqidx;
	struct fw_caps_config_cmd caps;
	uint32_t p, v;
	struct intrs_and_queues iaq;
	struct sge *s;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->pf = pci_get_function(dev);
	sc->mbox = sc->pf;

	pci_enable_busmaster(dev);
	pci_set_max_read_req(dev, 4096);
	snprintf(sc->lockname, sizeof(sc->lockname), "%s",
	    device_get_nameunit(dev));
	mtx_init(&sc->sc_lock, sc->lockname, 0, MTX_DEF);

	rc = map_bars(sc);
	if (rc != 0)
		goto done; /* error message displayed already */

	memset(sc->chan_map, 0xff, sizeof(sc->chan_map));

	/* Prepare the adapter for operation */
	rc = -t4_prep_adapter(sc);
	if (rc != 0) {
		device_printf(dev, "failed to prepare adapter: %d.\n", rc);
		goto done;
	}

	/* Do this really early */
	sc->cdev = make_dev(&t4_cdevsw, device_get_unit(dev), UID_ROOT,
	    GID_WHEEL, 0600, "%s", device_get_nameunit(dev));
	sc->cdev->si_drv1 = sc;

	/* Prepare the firmware for operation */
	rc = prep_firmware(sc);
	if (rc != 0)
		goto done; /* error message displayed already */

	/* Get device capabilities and select which ones we'll use */
	rc = get_capabilities(sc, &caps);
	if (rc != 0) {
		device_printf(dev,
		    "failed to initialize adapter capabilities: %d.\n", rc);
		goto done;
	}

	/* Choose the global RSS mode. */
	rc = -t4_config_glbl_rss(sc, sc->mbox,
	    FW_RSS_GLB_CONFIG_CMD_MODE_BASICVIRTUAL,
	    F_FW_RSS_GLB_CONFIG_CMD_TNLMAPEN |
	    F_FW_RSS_GLB_CONFIG_CMD_HASHTOEPLITZ |
	    F_FW_RSS_GLB_CONFIG_CMD_TNLALLLKP);
	if (rc != 0) {
		device_printf(dev,
		    "failed to select global RSS mode: %d.\n", rc);
		goto done;
	}

	/* These are total (sum of all ports) limits for a bus driver */
	rc = -t4_cfg_pfvf(sc, sc->mbox, sc->pf, 0,
	    64,		/* max # of egress queues */
	    64,		/* max # of egress Ethernet or control queues */
	    64,		/* max # of ingress queues with fl/interrupt */
	    0,		/* max # of ingress queues without interrupt */
	    0,		/* PCIe traffic class */
	    4,		/* max # of virtual interfaces */
	    M_FW_PFVF_CMD_CMASK, M_FW_PFVF_CMD_PMASK, 16,
	    FW_CMD_CAP_PF, FW_CMD_CAP_PF);
	if (rc != 0) {
		device_printf(dev,
		    "failed to configure pf/vf resources: %d.\n", rc);
		goto done;
	}

	/* Need this before sge_init */
	for (i = 0; i < SGE_NTIMERS; i++)
		sc->sge.timer_val[i] = min(intr_timer[i], 200U);
	for (i = 0; i < SGE_NCOUNTERS; i++)
		sc->sge.counter_val[i] = min(intr_pktcount[i], M_THRESHOLD_0);

	/* Also need the cooked value of cclk before sge_init */
	p = (V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_DEV) |
	    V_FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_DEV_CCLK));
	rc = -t4_query_params(sc, sc->mbox, sc->pf, 0, 1, &p, &v);
	if (rc != 0) {
		device_printf(sc->dev,
		    "failed to obtain core clock value: %d.\n", rc);
		goto done;
	}
	sc->params.vpd.cclk = v;

	t4_sge_init(sc);

	/*
	 * XXX: This is the place to call t4_set_filter_mode()
	 */

	/* get basic stuff going */
	rc = -t4_early_init(sc, sc->mbox);
	if (rc != 0) {
		device_printf(dev, "early init failed: %d.\n", rc);
		goto done;
	}

	rc = get_params(sc, &caps);
	if (rc != 0)
		goto done; /* error message displayed already */

	/* These are finalized by FW initialization, load their values now */
	v = t4_read_reg(sc, A_TP_TIMER_RESOLUTION);
	sc->params.tp.tre = G_TIMERRESOLUTION(v);
	sc->params.tp.dack_re = G_DELAYEDACKRESOLUTION(v);
	t4_read_mtu_tbl(sc, sc->params.mtus, NULL);

	/* tweak some settings */
	t4_write_reg(sc, A_TP_SHIFT_CNT, V_SYNSHIFTMAX(6) | V_RXTSHIFTMAXR1(4) |
	    V_RXTSHIFTMAXR2(15) | V_PERSHIFTBACKOFFMAX(8) | V_PERSHIFTMAX(8) |
	    V_KEEPALIVEMAXR1(4) | V_KEEPALIVEMAXR2(9));
	t4_write_reg(sc, A_ULP_RX_TDDP_PSZ, V_HPZ0(PAGE_SHIFT - 12));

	setup_memwin(sc);

	rc = t4_create_dma_tag(sc);
	if (rc != 0)
		goto done; /* error message displayed already */

	/*
	 * First pass over all the ports - allocate VIs and initialize some
	 * basic parameters like mac address, port type, etc.  We also figure
	 * out whether a port is 10G or 1G and use that information when
	 * calculating how many interrupts to attempt to allocate.
	 */
	n10g = n1g = 0;
	for_each_port(sc, i) {
		struct port_info *pi;

		pi = malloc(sizeof(*pi), M_CXGBE, M_ZERO | M_WAITOK);
		sc->port[i] = pi;

		/* These must be set before t4_port_init */
		pi->adapter = sc;
		pi->port_id = i;

		/* Allocate the vi and initialize parameters like mac addr */
		rc = -t4_port_init(pi, sc->mbox, sc->pf, 0);
		if (rc != 0) {
			device_printf(dev, "unable to initialize port %d: %d\n",
			    i, rc);
			free(pi, M_CXGBE);
			sc->port[i] = NULL;	/* indicates init failed */
			continue;
		}

		snprintf(pi->lockname, sizeof(pi->lockname), "%sp%d",
		    device_get_nameunit(dev), i);
		mtx_init(&pi->pi_lock, pi->lockname, 0, MTX_DEF);

		if (is_10G_port(pi)) {
			n10g++;
			pi->tmr_idx = tmr_idx_10g;
			pi->pktc_idx = pktc_idx_10g;
		} else {
			n1g++;
			pi->tmr_idx = tmr_idx_1g;
			pi->pktc_idx = pktc_idx_1g;
		}

		pi->xact_addr_filt = -1;

		pi->qsize_rxq = max(qsize_rxq, 128);
		while (pi->qsize_rxq & 7)
			pi->qsize_rxq++;
		pi->qsize_txq = max(qsize_txq, 128);

		if (pi->qsize_rxq != qsize_rxq) {
			device_printf(dev,
			    "using %d instead of %d as the rx queue size.\n",
			    pi->qsize_rxq, qsize_rxq);
		}
		if (pi->qsize_txq != qsize_txq) {
			device_printf(dev,
			    "using %d instead of %d as the tx queue size.\n",
			    pi->qsize_txq, qsize_txq);
		}

		pi->dev = device_add_child(dev, "cxgbe", -1);
		if (pi->dev == NULL) {
			device_printf(dev,
			    "failed to add device for port %d.\n", i);
			rc = ENXIO;
			goto done;
		}
		device_set_softc(pi->dev, pi);

		setbit(&sc->registered_device_map, i);
	}

	if (sc->registered_device_map == 0) {
		device_printf(dev, "no usable ports\n");
		rc = ENXIO;
		goto done;
	}

	/*
	 * Interrupt type, # of interrupts, # of rx/tx queues, etc.
	 */
	rc = cfg_itype_and_nqueues(sc, n10g, n1g, &iaq);
	if (rc != 0)
		goto done; /* error message displayed already */

	sc->intr_type = iaq.intr_type;
	sc->intr_count = iaq.nirq;

	s = &sc->sge;
	s->nrxq = n10g * iaq.nrxq10g + n1g * iaq.nrxq1g;
	s->ntxq = n10g * iaq.ntxq10g + n1g * iaq.ntxq1g;
	s->neq = s->ntxq + s->nrxq;	/* the fl in an rxq is an eq */
	s->niq = s->nrxq + 1;		/* 1 extra for firmware event queue */
	if (iaq.intr_fwd) {
		sc->flags |= INTR_FWD;
		s->niq += NFIQ(sc);		/* forwarded interrupt queues */
		s->fiq = malloc(NFIQ(sc) * sizeof(struct sge_iq), M_CXGBE,
		    M_ZERO | M_WAITOK);
	}
	s->rxq = malloc(s->nrxq * sizeof(struct sge_rxq), M_CXGBE,
	    M_ZERO | M_WAITOK);
	s->txq = malloc(s->ntxq * sizeof(struct sge_txq), M_CXGBE,
	    M_ZERO | M_WAITOK);
	s->iqmap = malloc(s->niq * sizeof(struct sge_iq *), M_CXGBE,
	    M_ZERO | M_WAITOK);
	s->eqmap = malloc(s->neq * sizeof(struct sge_eq *), M_CXGBE,
	    M_ZERO | M_WAITOK);

	sc->irq = malloc(sc->intr_count * sizeof(struct irq), M_CXGBE,
	    M_ZERO | M_WAITOK);

	t4_sysctls(sc);

	/*
	 * Second pass over the ports.  This time we know the number of rx and
	 * tx queues that each port should get.
	 */
	rqidx = tqidx = 0;
	for_each_port(sc, i) {
		struct port_info *pi = sc->port[i];

		if (pi == NULL)
			continue;

		pi->first_rxq = rqidx;
		pi->nrxq = is_10G_port(pi) ? iaq.nrxq10g : iaq.nrxq1g;

		pi->first_txq = tqidx;
		pi->ntxq = is_10G_port(pi) ? iaq.ntxq10g : iaq.ntxq1g;

		rqidx += pi->nrxq;
		tqidx += pi->ntxq;
	}

	rc = bus_generic_attach(dev);
	if (rc != 0) {
		device_printf(dev,
		    "failed to attach all child ports: %d\n", rc);
		goto done;
	}

#ifdef INVARIANTS
	device_printf(dev,
	    "%p, %d ports (0x%x), %d intr_type, %d intr_count\n",
	    sc, sc->params.nports, sc->params.portvec,
	    sc->intr_type, sc->intr_count);
#endif
	t4_set_desc(sc);

done:
	if (rc != 0)
		t4_detach(dev);

	return (rc);
}

/*
 * Idempotent
 */
static int
t4_detach(device_t dev)
{
	struct adapter *sc;
	struct port_info *pi;
	int i;

	sc = device_get_softc(dev);

	if (sc->cdev)
		destroy_dev(sc->cdev);

	bus_generic_detach(dev);
	for (i = 0; i < MAX_NPORTS; i++) {
		pi = sc->port[i];
		if (pi) {
			t4_free_vi(pi->adapter, sc->mbox, sc->pf, 0, pi->viid);
			if (pi->dev)
				device_delete_child(dev, pi->dev);

			mtx_destroy(&pi->pi_lock);
			free(pi, M_CXGBE);
		}
	}

	if (sc->flags & FW_OK)
		t4_fw_bye(sc, sc->mbox);

	if (sc->intr_type == INTR_MSI || sc->intr_type == INTR_MSIX)
		pci_release_msi(dev);

	if (sc->regs_res)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->regs_rid,
		    sc->regs_res);

	if (sc->msix_res)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->msix_rid,
		    sc->msix_res);

	free(sc->irq, M_CXGBE);
	free(sc->sge.rxq, M_CXGBE);
	free(sc->sge.txq, M_CXGBE);
	free(sc->sge.fiq, M_CXGBE);
	free(sc->sge.iqmap, M_CXGBE);
	free(sc->sge.eqmap, M_CXGBE);
	t4_destroy_dma_tag(sc);
	mtx_destroy(&sc->sc_lock);

	bzero(sc, sizeof(*sc));

	return (0);
}


static int
cxgbe_probe(device_t dev)
{
	char buf[128];
	struct port_info *pi = device_get_softc(dev);

	snprintf(buf, sizeof(buf), "Port %d", pi->port_id);
	device_set_desc_copy(dev, buf);

	return (BUS_PROBE_DEFAULT);
}

#define T4_CAP (IFCAP_VLAN_HWTAGGING | IFCAP_VLAN_MTU | IFCAP_HWCSUM | \
    IFCAP_VLAN_HWCSUM | IFCAP_TSO | IFCAP_JUMBO_MTU | IFCAP_LRO | \
    IFCAP_VLAN_HWTSO)
#define T4_CAP_ENABLE (T4_CAP & ~IFCAP_TSO6)

static int
cxgbe_attach(device_t dev)
{
	struct port_info *pi = device_get_softc(dev);
	struct ifnet *ifp;

	/* Allocate an ifnet and set it up */
	ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(dev, "Cannot allocate ifnet\n");
		return (ENOMEM);
	}
	pi->ifp = ifp;
	ifp->if_softc = pi;

	callout_init(&pi->tick, CALLOUT_MPSAFE);
	pi->tq = taskqueue_create("cxgbe_taskq", M_NOWAIT,
	    taskqueue_thread_enqueue, &pi->tq);
	if (pi->tq == NULL) {
		device_printf(dev, "failed to allocate port task queue\n");
		if_free(pi->ifp);
		return (ENOMEM);
	}
	taskqueue_start_threads(&pi->tq, 1, PI_NET, "%s taskq",
	    device_get_nameunit(dev));

	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;

	ifp->if_init = cxgbe_init;
	ifp->if_ioctl = cxgbe_ioctl;
	ifp->if_start = cxgbe_start;
	ifp->if_transmit = cxgbe_transmit;
	ifp->if_qflush = cxgbe_qflush;

	ifp->if_snd.ifq_drv_maxlen = 1024;
	IFQ_SET_MAXLEN(&ifp->if_snd, ifp->if_snd.ifq_drv_maxlen);
	IFQ_SET_READY(&ifp->if_snd);

	ifp->if_capabilities = T4_CAP;
	ifp->if_capenable = T4_CAP_ENABLE;
	ifp->if_hwassist = CSUM_TCP | CSUM_UDP | CSUM_IP | CSUM_TSO;

	/* Initialize ifmedia for this port */
	ifmedia_init(&pi->media, IFM_IMASK, cxgbe_media_change,
	    cxgbe_media_status);
	build_medialist(pi);

	ether_ifattach(ifp, pi->hw_addr);

#ifdef INVARIANTS
	device_printf(dev, "%p, %d txq, %d rxq\n", pi, pi->ntxq, pi->nrxq);
#endif

	cxgbe_sysctls(pi);

	return (0);
}

static int
cxgbe_detach(device_t dev)
{
	struct port_info *pi = device_get_softc(dev);
	struct adapter *sc = pi->adapter;
	int rc;

	/* Tell if_ioctl and if_init that the port is going away */
	ADAPTER_LOCK(sc);
	SET_DOOMED(pi);
	wakeup(&sc->flags);
	while (IS_BUSY(sc))
		mtx_sleep(&sc->flags, &sc->sc_lock, 0, "t4detach", 0);
	SET_BUSY(sc);
	ADAPTER_UNLOCK(sc);

	rc = cxgbe_uninit_synchronized(pi);
	if (rc != 0)
		device_printf(dev, "port uninit failed: %d.\n", rc);

	taskqueue_free(pi->tq);

	ifmedia_removeall(&pi->media);
	ether_ifdetach(pi->ifp);
	if_free(pi->ifp);

	ADAPTER_LOCK(sc);
	CLR_BUSY(sc);
	wakeup_one(&sc->flags);
	ADAPTER_UNLOCK(sc);

	return (0);
}

static void
cxgbe_init(void *arg)
{
	struct port_info *pi = arg;
	struct adapter *sc = pi->adapter;

	ADAPTER_LOCK(sc);
	cxgbe_init_locked(pi); /* releases adapter lock */
	ADAPTER_LOCK_ASSERT_NOTOWNED(sc);
}

static int
cxgbe_ioctl(struct ifnet *ifp, unsigned long cmd, caddr_t data)
{
	int rc = 0, mtu, flags;
	struct port_info *pi = ifp->if_softc;
	struct adapter *sc = pi->adapter;
	struct ifreq *ifr = (struct ifreq *)data;
	uint32_t mask;

	switch (cmd) {
	case SIOCSIFMTU:
		ADAPTER_LOCK(sc);
		rc = IS_DOOMED(pi) ? ENXIO : (IS_BUSY(sc) ? EBUSY : 0);
		if (rc) {
fail:
			ADAPTER_UNLOCK(sc);
			return (rc);
		}

		mtu = ifr->ifr_mtu;
		if ((mtu < ETHERMIN) || (mtu > ETHERMTU_JUMBO)) {
			rc = EINVAL;
		} else {
			ifp->if_mtu = mtu;
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				t4_update_fl_bufsize(ifp);
				PORT_LOCK(pi);
				rc = update_mac_settings(pi, XGMAC_MTU);
				PORT_UNLOCK(pi);
			}
		}
		ADAPTER_UNLOCK(sc);
		break;

	case SIOCSIFFLAGS:
		ADAPTER_LOCK(sc);
		if (IS_DOOMED(pi)) {
			rc = ENXIO;
			goto fail;
		}
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				flags = pi->if_flags;
				if ((ifp->if_flags ^ flags) &
				    (IFF_PROMISC | IFF_ALLMULTI)) {
					if (IS_BUSY(sc)) {
						rc = EBUSY;
						goto fail;
					}
					PORT_LOCK(pi);
					rc = update_mac_settings(pi,
					    XGMAC_PROMISC | XGMAC_ALLMULTI);
					PORT_UNLOCK(pi);
				}
				ADAPTER_UNLOCK(sc);
			} else
				rc = cxgbe_init_locked(pi);
			pi->if_flags = ifp->if_flags;
		} else if (ifp->if_drv_flags & IFF_DRV_RUNNING)
			rc = cxgbe_uninit_locked(pi);
		else
			ADAPTER_UNLOCK(sc);

		ADAPTER_LOCK_ASSERT_NOTOWNED(sc);
		break;

	case SIOCADDMULTI:	
	case SIOCDELMULTI: /* these two can be called with a mutex held :-( */
		ADAPTER_LOCK(sc);
		rc = IS_DOOMED(pi) ? ENXIO : (IS_BUSY(sc) ? EBUSY : 0);
		if (rc)
			goto fail;

		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			PORT_LOCK(pi);
			rc = update_mac_settings(pi, XGMAC_MCADDRS);
			PORT_UNLOCK(pi);
		}
		ADAPTER_UNLOCK(sc);
		break;

	case SIOCSIFCAP:
		ADAPTER_LOCK(sc);
		rc = IS_DOOMED(pi) ? ENXIO : (IS_BUSY(sc) ? EBUSY : 0);
		if (rc)
			goto fail;

		mask = ifr->ifr_reqcap ^ ifp->if_capenable;
		if (mask & IFCAP_TXCSUM) {
			ifp->if_capenable ^= IFCAP_TXCSUM;
			ifp->if_hwassist ^= (CSUM_TCP | CSUM_UDP | CSUM_IP);

			if (IFCAP_TSO & ifp->if_capenable &&
			    !(IFCAP_TXCSUM & ifp->if_capenable)) {
				ifp->if_capenable &= ~IFCAP_TSO;
				ifp->if_hwassist &= ~CSUM_TSO;
				if_printf(ifp,
				    "tso disabled due to -txcsum.\n");
			}
		}
		if (mask & IFCAP_RXCSUM)
			ifp->if_capenable ^= IFCAP_RXCSUM;
		if (mask & IFCAP_TSO4) {
			ifp->if_capenable ^= IFCAP_TSO4;

			if (IFCAP_TSO & ifp->if_capenable) {
				if (IFCAP_TXCSUM & ifp->if_capenable)
					ifp->if_hwassist |= CSUM_TSO;
				else {
					ifp->if_capenable &= ~IFCAP_TSO;
					ifp->if_hwassist &= ~CSUM_TSO;
					if_printf(ifp,
					    "enable txcsum first.\n");
					rc = EAGAIN;
				}
			} else
				ifp->if_hwassist &= ~CSUM_TSO;
		}
		if (mask & IFCAP_LRO) {
#ifdef INET
			int i;
			struct sge_rxq *rxq;

			ifp->if_capenable ^= IFCAP_LRO;
			for_each_rxq(pi, i, rxq) {
				if (ifp->if_capenable & IFCAP_LRO)
					rxq->flags |= RXQ_LRO_ENABLED;
				else
					rxq->flags &= ~RXQ_LRO_ENABLED;
			}
#endif
		}
#ifndef TCP_OFFLOAD_DISABLE
		if (mask & IFCAP_TOE4) {
			rc = EOPNOTSUPP;
		}
#endif
		if (mask & IFCAP_VLAN_HWTAGGING) {
			ifp->if_capenable ^= IFCAP_VLAN_HWTAGGING;
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				PORT_LOCK(pi);
				rc = update_mac_settings(pi, XGMAC_VLANEX);
				PORT_UNLOCK(pi);
			}
		}
		if (mask & IFCAP_VLAN_MTU) {
			ifp->if_capenable ^= IFCAP_VLAN_MTU;

			/* Need to find out how to disable auto-mtu-inflation */
		}
		if (mask & IFCAP_VLAN_HWTSO)
			ifp->if_capenable ^= IFCAP_VLAN_HWTSO;
		if (mask & IFCAP_VLAN_HWCSUM)
			ifp->if_capenable ^= IFCAP_VLAN_HWCSUM;

#ifdef VLAN_CAPABILITIES
		VLAN_CAPABILITIES(ifp);
#endif
		ADAPTER_UNLOCK(sc);
		break;

	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		ifmedia_ioctl(ifp, ifr, &pi->media, cmd);
		break;

	default:
		rc = ether_ioctl(ifp, cmd, data);
	}

	return (rc);
}

static void
cxgbe_start(struct ifnet *ifp)
{
	struct port_info *pi = ifp->if_softc;
	struct sge_txq *txq;
	int i;

	for_each_txq(pi, i, txq) {
		if (TXQ_TRYLOCK(txq)) {
			txq_start(ifp, txq);
			TXQ_UNLOCK(txq);
		}
	}
}

static int
cxgbe_transmit(struct ifnet *ifp, struct mbuf *m)
{
	struct port_info *pi = ifp->if_softc;
	struct adapter *sc = pi->adapter;
	struct sge_txq *txq = &sc->sge.txq[pi->first_txq];
	struct buf_ring *br;
	int rc;

	M_ASSERTPKTHDR(m);

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
		m_freem(m);
		return (0);
	}

	if (m->m_flags & M_FLOWID)
		txq += (m->m_pkthdr.flowid % pi->ntxq);
	br = txq->eq.br;

	if (TXQ_TRYLOCK(txq) == 0) {
		/*
		 * XXX: make sure that this packet really is sent out.  There is
		 * a small race where t4_eth_tx may stop draining the drbr and
		 * goes away, just before we enqueued this mbuf.
		 */

		return (drbr_enqueue(ifp, br, m));
	}

	/*
	 * txq->m is the mbuf that is held up due to a temporary shortage of
	 * resources and it should be put on the wire first.  Then what's in
	 * drbr and finally the mbuf that was just passed in to us.
	 *
	 * Return code should indicate the fate of the mbuf that was passed in
	 * this time.
	 */

	TXQ_LOCK_ASSERT_OWNED(txq);
	if (drbr_needs_enqueue(ifp, br) || txq->m) {

		/* Queued for transmission. */

		rc = drbr_enqueue(ifp, br, m);
		m = txq->m ? txq->m : drbr_dequeue(ifp, br);
		(void) t4_eth_tx(ifp, txq, m);
		TXQ_UNLOCK(txq);
		return (rc);
	}

	/* Direct transmission. */
	rc = t4_eth_tx(ifp, txq, m);
	if (rc != 0 && txq->m)
		rc = 0;	/* held, will be transmitted soon (hopefully) */

	TXQ_UNLOCK(txq);
	return (rc);
}

static void
cxgbe_qflush(struct ifnet *ifp)
{
	struct port_info *pi = ifp->if_softc;
	struct sge_txq *txq;
	int i;
	struct mbuf *m;

	/* queues do not exist if !IFF_DRV_RUNNING. */
	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		for_each_txq(pi, i, txq) {
			TXQ_LOCK(txq);
			m_freem(txq->m);
			while ((m = buf_ring_dequeue_sc(txq->eq.br)) != NULL)
				m_freem(m);
			TXQ_UNLOCK(txq);
		}
	}
	if_qflush(ifp);
}

static int
cxgbe_media_change(struct ifnet *ifp)
{
	struct port_info *pi = ifp->if_softc;

	device_printf(pi->dev, "%s unimplemented.\n", __func__);

	return (EOPNOTSUPP);
}

static void
cxgbe_media_status(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct port_info *pi = ifp->if_softc;
	struct ifmedia_entry *cur = pi->media.ifm_cur;
	int speed = pi->link_cfg.speed;
	int data = (pi->port_type << 8) | pi->mod_type;

	if (cur->ifm_data != data) {
		build_medialist(pi);
		cur = pi->media.ifm_cur;
	}

	ifmr->ifm_status = IFM_AVALID;
	if (!pi->link_cfg.link_ok)
		return;

	ifmr->ifm_status |= IFM_ACTIVE;

	/* active and current will differ iff current media is autoselect. */
	if (IFM_SUBTYPE(cur->ifm_media) != IFM_AUTO)
		return;

	ifmr->ifm_active = IFM_ETHER | IFM_FDX;
	if (speed == SPEED_10000)
		ifmr->ifm_active |= IFM_10G_T;
	else if (speed == SPEED_1000)
		ifmr->ifm_active |= IFM_1000_T;
	else if (speed == SPEED_100)
		ifmr->ifm_active |= IFM_100_TX;
	else if (speed == SPEED_10)
		ifmr->ifm_active |= IFM_10_T;
	else
		KASSERT(0, ("%s: link up but speed unknown (%u)", __func__,
			    speed));
}

void
t4_fatal_err(struct adapter *sc)
{
	t4_set_reg_field(sc, A_SGE_CONTROL, F_GLOBALENABLE, 0);
	t4_intr_disable(sc);
	log(LOG_EMERG, "%s: encountered fatal error, adapter stopped.\n",
	    device_get_nameunit(sc->dev));
}

static int
map_bars(struct adapter *sc)
{
	sc->regs_rid = PCIR_BAR(0);
	sc->regs_res = bus_alloc_resource_any(sc->dev, SYS_RES_MEMORY,
	    &sc->regs_rid, RF_ACTIVE);
	if (sc->regs_res == NULL) {
		device_printf(sc->dev, "cannot map registers.\n");
		return (ENXIO);
	}
	sc->bt = rman_get_bustag(sc->regs_res);
	sc->bh = rman_get_bushandle(sc->regs_res);
	sc->mmio_len = rman_get_size(sc->regs_res);

	sc->msix_rid = PCIR_BAR(4);
	sc->msix_res = bus_alloc_resource_any(sc->dev, SYS_RES_MEMORY,
	    &sc->msix_rid, RF_ACTIVE);
	if (sc->msix_res == NULL) {
		device_printf(sc->dev, "cannot map MSI-X BAR.\n");
		return (ENXIO);
	}

	return (0);
}

static void
setup_memwin(struct adapter *sc)
{
	u_long bar0;

	bar0 = rman_get_start(sc->regs_res);

	t4_write_reg(sc, PCIE_MEM_ACCESS_REG(A_PCIE_MEM_ACCESS_BASE_WIN, 0),
	    	     (bar0 + MEMWIN0_BASE) | V_BIR(0) |
		     V_WINDOW(ilog2(MEMWIN0_APERTURE) - 10));

	t4_write_reg(sc, PCIE_MEM_ACCESS_REG(A_PCIE_MEM_ACCESS_BASE_WIN, 1),
		     (bar0 + MEMWIN1_BASE) | V_BIR(0) |
		     V_WINDOW(ilog2(MEMWIN1_APERTURE) - 10));

	t4_write_reg(sc, PCIE_MEM_ACCESS_REG(A_PCIE_MEM_ACCESS_BASE_WIN, 2),
		     (bar0 + MEMWIN2_BASE) | V_BIR(0) |
		     V_WINDOW(ilog2(MEMWIN2_APERTURE) - 10));
}

static int
cfg_itype_and_nqueues(struct adapter *sc, int n10g, int n1g,
    struct intrs_and_queues *iaq)
{
	int rc, itype, navail, nc, nrxq10g, nrxq1g;

	bzero(iaq, sizeof(*iaq));
	nc = mp_ncpus;	/* our snapshot of the number of CPUs */

	for (itype = INTR_MSIX; itype; itype >>= 1) {

		if ((itype & intr_types) == 0)
			continue;	/* not allowed */

		if (itype == INTR_MSIX)
			navail = pci_msix_count(sc->dev);
		else if (itype == INTR_MSI)
			navail = pci_msi_count(sc->dev);
		else
			navail = 1;

		if (navail == 0)
			continue;

		iaq->intr_type = itype;

		iaq->ntxq10g = min(nc, max_ntxq_10g);
		iaq->ntxq1g = min(nc, max_ntxq_1g);

		nrxq10g = min(nc, max_nrxq_10g);
		nrxq1g = min(nc, max_nrxq_1g);

		/* Extra 2 is for a) error interrupt b) firmware event */
		iaq->nirq = n10g * nrxq10g + n1g * nrxq1g + 2;
		if (iaq->nirq <= navail && intr_fwd == 0) {

			if (itype == INTR_MSI && !powerof2(iaq->nirq))
				goto fwd;

			/* One for err, one for fwq, and one for each rxq */

			iaq->intr_fwd = 0;
			iaq->nrxq10g = nrxq10g;
			iaq->nrxq1g = nrxq1g;

		} else {
fwd:
			iaq->intr_fwd = 1;

			if (navail > nc) {
				if (itype == INTR_MSIX)
					navail = nc + 1;

				/* navail is and must remain a pow2 for MSI */
				if (itype == INTR_MSI) {
					KASSERT(powerof2(navail),
					    ("%d not power of 2", navail));

					while (navail / 2 > nc)
						navail /= 2;
				}
			}
			iaq->nirq = navail;	/* total # of interrupts */

			/*
			 * If we have multiple vectors available reserve one
			 * exclusively for errors.  The rest will be shared by
			 * the fwq and data.
			 */
			if (navail > 1)
				navail--;
			iaq->nrxq10g = min(nrxq10g, navail);
			iaq->nrxq1g = min(nrxq1g, navail);
		}

		navail = iaq->nirq;
		rc = 0;
		if (itype == INTR_MSIX)
			rc = pci_alloc_msix(sc->dev, &navail);
		else if (itype == INTR_MSI)
			rc = pci_alloc_msi(sc->dev, &navail);

		if (rc == 0) {
			if (navail == iaq->nirq)
				return (0);

			/*
			 * Didn't get the number requested.  Use whatever number
			 * the kernel is willing to allocate (it's in navail).
			 */
			pci_release_msi(sc->dev);
			goto fwd;
		}

		device_printf(sc->dev,
		    "failed to allocate vectors:%d, type=%d, req=%d, rcvd=%d\n",
		    itype, rc, iaq->nirq, navail);
	}

	device_printf(sc->dev,
	    "failed to find a usable interrupt type.  "
	    "allowed=%d, msi-x=%d, msi=%d, intx=1", intr_types,
	    pci_msix_count(sc->dev), pci_msi_count(sc->dev));

	return (ENXIO);
}

/*
 * Install a compatible firmware (if required), establish contact with it,
 * become the master, and reset the device.
 */
static int
prep_firmware(struct adapter *sc)
{
	const struct firmware *fw;
	int rc;
	enum dev_state state;

	/* Check firmware version and install a different one if necessary */
	rc = t4_check_fw_version(sc);
	if (rc != 0 || force_firmware_install) {
		uint32_t v = 0;

		fw = firmware_get(T4_FWNAME);
		if (fw != NULL) {
			const struct fw_hdr *hdr = (const void *)fw->data;

			v = ntohl(hdr->fw_ver);

			/*
			 * The firmware module will not be used if it isn't the
			 * same major version as what the driver was compiled
			 * with.  This check trumps force_firmware_install.
			 */
			if (G_FW_HDR_FW_VER_MAJOR(v) != FW_VERSION_MAJOR) {
				device_printf(sc->dev,
				    "Found firmware image but version %d "
				    "can not be used with this driver (%d)\n",
				    G_FW_HDR_FW_VER_MAJOR(v), FW_VERSION_MAJOR);

				firmware_put(fw, FIRMWARE_UNLOAD);
				fw = NULL;
			}
		}

		if (fw == NULL && (rc < 0 || force_firmware_install)) {
			device_printf(sc->dev, "No usable firmware. "
			    "card has %d.%d.%d, driver compiled with %d.%d.%d, "
			    "force_firmware_install%s set",
			    G_FW_HDR_FW_VER_MAJOR(sc->params.fw_vers),
			    G_FW_HDR_FW_VER_MINOR(sc->params.fw_vers),
			    G_FW_HDR_FW_VER_MICRO(sc->params.fw_vers),
			    FW_VERSION_MAJOR, FW_VERSION_MINOR,
			    FW_VERSION_MICRO,
			    force_firmware_install ? "" : " not");
			return (EAGAIN);
		}

		/*
		 * Always upgrade, even for minor/micro/build mismatches.
		 * Downgrade only for a major version mismatch or if
		 * force_firmware_install was specified.
		 */
		if (fw != NULL && (rc < 0 || force_firmware_install ||
		    v > sc->params.fw_vers)) {
			device_printf(sc->dev,
			    "installing firmware %d.%d.%d.%d on card.\n",
			    G_FW_HDR_FW_VER_MAJOR(v), G_FW_HDR_FW_VER_MINOR(v),
			    G_FW_HDR_FW_VER_MICRO(v), G_FW_HDR_FW_VER_BUILD(v));

			rc = -t4_load_fw(sc, fw->data, fw->datasize);
			if (rc != 0) {
				device_printf(sc->dev,
				    "failed to install firmware: %d\n", rc);
				firmware_put(fw, FIRMWARE_UNLOAD);
				return (rc);
			} else {
				/* refresh */
				(void) t4_check_fw_version(sc);
			}
		}

		if (fw != NULL)
			firmware_put(fw, FIRMWARE_UNLOAD);
	}

	/* Contact firmware, request master */
	rc = t4_fw_hello(sc, sc->mbox, sc->mbox, MASTER_MUST, &state);
	if (rc < 0) {
		rc = -rc;
		device_printf(sc->dev,
		    "failed to connect to the firmware: %d.\n", rc);
		return (rc);
	}

	/* Reset device */
	rc = -t4_fw_reset(sc, sc->mbox, F_PIORSTMODE | F_PIORST);
	if (rc != 0) {
		device_printf(sc->dev, "firmware reset failed: %d.\n", rc);
		if (rc != ETIMEDOUT && rc != EIO)
			t4_fw_bye(sc, sc->mbox);
		return (rc);
	}

	snprintf(sc->fw_version, sizeof(sc->fw_version), "%u.%u.%u.%u",
	    G_FW_HDR_FW_VER_MAJOR(sc->params.fw_vers),
	    G_FW_HDR_FW_VER_MINOR(sc->params.fw_vers),
	    G_FW_HDR_FW_VER_MICRO(sc->params.fw_vers),
	    G_FW_HDR_FW_VER_BUILD(sc->params.fw_vers));
	sc->flags |= FW_OK;

	return (0);
}

static int
get_capabilities(struct adapter *sc, struct fw_caps_config_cmd *caps)
{
	int rc;

	bzero(caps, sizeof(*caps));
	caps->op_to_write = htobe32(V_FW_CMD_OP(FW_CAPS_CONFIG_CMD) |
	    F_FW_CMD_REQUEST | F_FW_CMD_READ);
	caps->retval_len16 = htobe32(FW_LEN16(*caps));

	rc = -t4_wr_mbox(sc, sc->mbox, caps, sizeof(*caps), caps);
	if (rc != 0)
		return (rc);

	if (caps->niccaps & htobe16(FW_CAPS_CONFIG_NIC_VM))
		caps->niccaps ^= htobe16(FW_CAPS_CONFIG_NIC_VM);

	caps->op_to_write = htobe32(V_FW_CMD_OP(FW_CAPS_CONFIG_CMD) |
	    F_FW_CMD_REQUEST | F_FW_CMD_WRITE);
	rc = -t4_wr_mbox(sc, sc->mbox, caps, sizeof(*caps), NULL);

	return (rc);
}

static int
get_params(struct adapter *sc, struct fw_caps_config_cmd *caps)
{
	int rc;
	uint32_t params[7], val[7];

#define FW_PARAM_DEV(param) \
	(V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_DEV) | \
	 V_FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_DEV_##param))
#define FW_PARAM_PFVF(param) \
	(V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_PFVF) | \
	 V_FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_PFVF_##param))

	params[0] = FW_PARAM_DEV(PORTVEC);
	params[1] = FW_PARAM_PFVF(IQFLINT_START);
	params[2] = FW_PARAM_PFVF(EQ_START);
	params[3] = FW_PARAM_PFVF(FILTER_START);
	params[4] = FW_PARAM_PFVF(FILTER_END);
	rc = -t4_query_params(sc, sc->mbox, sc->pf, 0, 5, params, val);
	if (rc != 0) {
		device_printf(sc->dev,
		    "failed to query parameters: %d.\n", rc);
		goto done;
	}

	sc->params.portvec = val[0];
	sc->params.nports = 0;
	while (val[0]) {
		sc->params.nports++;
		val[0] &= val[0] - 1;
	}

	sc->sge.iq_start = val[1];
	sc->sge.eq_start = val[2];
	sc->tids.ftid_base = val[3];
	sc->tids.nftids = val[4] - val[3] + 1;

	if (caps->toecaps) {
		/* query offload-related parameters */
		params[0] = FW_PARAM_DEV(NTID);
		params[1] = FW_PARAM_PFVF(SERVER_START);
		params[2] = FW_PARAM_PFVF(SERVER_END);
		params[3] = FW_PARAM_PFVF(TDDP_START);
		params[4] = FW_PARAM_PFVF(TDDP_END);
		params[5] = FW_PARAM_DEV(FLOWC_BUFFIFO_SZ);
		rc = -t4_query_params(sc, sc->mbox, sc->pf, 0, 6, params, val);
		if (rc != 0) {
			device_printf(sc->dev,
			    "failed to query TOE parameters: %d.\n", rc);
			goto done;
		}
		sc->tids.ntids = val[0];
		sc->tids.natids = min(sc->tids.ntids / 2, MAX_ATIDS);
		sc->tids.stid_base = val[1];
		sc->tids.nstids = val[2] - val[1] + 1;
		sc->vres.ddp.start = val[3];
		sc->vres.ddp.size = val[4] - val[3] + 1;
		sc->params.ofldq_wr_cred = val[5];
		sc->params.offload = 1;
	}
	if (caps->rdmacaps) {
		params[0] = FW_PARAM_PFVF(STAG_START);
		params[1] = FW_PARAM_PFVF(STAG_END);
		params[2] = FW_PARAM_PFVF(RQ_START);
		params[3] = FW_PARAM_PFVF(RQ_END);
		params[4] = FW_PARAM_PFVF(PBL_START);
		params[5] = FW_PARAM_PFVF(PBL_END);
		rc = -t4_query_params(sc, sc->mbox, sc->pf, 0, 6, params, val);
		if (rc != 0) {
			device_printf(sc->dev,
			    "failed to query RDMA parameters: %d.\n", rc);
			goto done;
		}
		sc->vres.stag.start = val[0];
		sc->vres.stag.size = val[1] - val[0] + 1;
		sc->vres.rq.start = val[2];
		sc->vres.rq.size = val[3] - val[2] + 1;
		sc->vres.pbl.start = val[4];
		sc->vres.pbl.size = val[5] - val[4] + 1;
	}
	if (caps->iscsicaps) {
		params[0] = FW_PARAM_PFVF(ISCSI_START);
		params[1] = FW_PARAM_PFVF(ISCSI_END);
		rc = -t4_query_params(sc, sc->mbox, sc->pf, 0, 2, params, val);
		if (rc != 0) {
			device_printf(sc->dev,
			    "failed to query iSCSI parameters: %d.\n", rc);
			goto done;
		}
		sc->vres.iscsi.start = val[0];
		sc->vres.iscsi.size = val[1] - val[0] + 1;
	}
#undef FW_PARAM_PFVF
#undef FW_PARAM_DEV

done:
	return (rc);
}

static void
t4_set_desc(struct adapter *sc)
{
	char buf[128];
	struct adapter_params *p = &sc->params;

	snprintf(buf, sizeof(buf),
	    "Chelsio %s (rev %d) %d port %sNIC PCIe-x%d %d %s, S/N:%s, E/C:%s",
	    p->vpd.id, p->rev, p->nports, is_offload(sc) ? "R" : "",
	    p->pci.width, sc->intr_count, sc->intr_type == INTR_MSIX ? "MSI-X" :
	    (sc->intr_type == INTR_MSI ? "MSI" : "INTx"), p->vpd.sn, p->vpd.ec);

	device_set_desc_copy(sc->dev, buf);
}

static void
build_medialist(struct port_info *pi)
{
	struct ifmedia *media = &pi->media;
	int data, m;

	PORT_LOCK(pi);

	ifmedia_removeall(media);

	m = IFM_ETHER | IFM_FDX;
	data = (pi->port_type << 8) | pi->mod_type;

	switch(pi->port_type) {
	case FW_PORT_TYPE_BT_XFI:
		ifmedia_add(media, m | IFM_10G_T, data, NULL);
		break;

	case FW_PORT_TYPE_BT_XAUI:
		ifmedia_add(media, m | IFM_10G_T, data, NULL);
		/* fall through */

	case FW_PORT_TYPE_BT_SGMII:
		ifmedia_add(media, m | IFM_1000_T, data, NULL);
		ifmedia_add(media, m | IFM_100_TX, data, NULL);
		ifmedia_add(media, IFM_ETHER | IFM_AUTO, data, NULL);
		ifmedia_set(media, IFM_ETHER | IFM_AUTO);
		break;

	case FW_PORT_TYPE_CX4:
		ifmedia_add(media, m | IFM_10G_CX4, data, NULL);
		ifmedia_set(media, m | IFM_10G_CX4);
		break;

	case FW_PORT_TYPE_SFP:
	case FW_PORT_TYPE_FIBER_XFI:
	case FW_PORT_TYPE_FIBER_XAUI:
		switch (pi->mod_type) {

		case FW_PORT_MOD_TYPE_LR:
			ifmedia_add(media, m | IFM_10G_LR, data, NULL);
			ifmedia_set(media, m | IFM_10G_LR);
			break;

		case FW_PORT_MOD_TYPE_SR:
			ifmedia_add(media, m | IFM_10G_SR, data, NULL);
			ifmedia_set(media, m | IFM_10G_SR);
			break;

		case FW_PORT_MOD_TYPE_LRM:
			ifmedia_add(media, m | IFM_10G_LRM, data, NULL);
			ifmedia_set(media, m | IFM_10G_LRM);
			break;

		case FW_PORT_MOD_TYPE_TWINAX_PASSIVE:
		case FW_PORT_MOD_TYPE_TWINAX_ACTIVE:
			ifmedia_add(media, m | IFM_10G_TWINAX, data, NULL);
			ifmedia_set(media, m | IFM_10G_TWINAX);
			break;

		case FW_PORT_MOD_TYPE_NONE:
			m &= ~IFM_FDX;
			ifmedia_add(media, m | IFM_NONE, data, NULL);
			ifmedia_set(media, m | IFM_NONE);
			break;

		case FW_PORT_MOD_TYPE_NA:
		case FW_PORT_MOD_TYPE_ER:
		default:
			ifmedia_add(media, m | IFM_UNKNOWN, data, NULL);
			ifmedia_set(media, m | IFM_UNKNOWN);
			break;
		}
		break;

	case FW_PORT_TYPE_KX4:
	case FW_PORT_TYPE_KX:
	case FW_PORT_TYPE_KR:
	default:
		ifmedia_add(media, m | IFM_UNKNOWN, data, NULL);
		ifmedia_set(media, m | IFM_UNKNOWN);
		break;
	}

	PORT_UNLOCK(pi);
}

/*
 * Program the port's XGMAC based on parameters in ifnet.  The caller also
 * indicates which parameters should be programmed (the rest are left alone).
 */
static int
update_mac_settings(struct port_info *pi, int flags)
{
	int rc;
	struct ifnet *ifp = pi->ifp;
	struct adapter *sc = pi->adapter;
	int mtu = -1, promisc = -1, allmulti = -1, vlanex = -1;

	PORT_LOCK_ASSERT_OWNED(pi);
	KASSERT(flags, ("%s: not told what to update.", __func__));

	if (flags & XGMAC_MTU)
		mtu = ifp->if_mtu;

	if (flags & XGMAC_PROMISC)
		promisc = ifp->if_flags & IFF_PROMISC ? 1 : 0;

	if (flags & XGMAC_ALLMULTI)
		allmulti = ifp->if_flags & IFF_ALLMULTI ? 1 : 0;

	if (flags & XGMAC_VLANEX)
		vlanex = ifp->if_capenable & IFCAP_VLAN_HWTAGGING ? 1 : 0;

	rc = -t4_set_rxmode(sc, sc->mbox, pi->viid, mtu, promisc, allmulti, 1,
	    vlanex, false);
	if (rc) {
		if_printf(ifp, "set_rxmode (%x) failed: %d\n", flags, rc);
		return (rc);
	}

	if (flags & XGMAC_UCADDR) {
		uint8_t ucaddr[ETHER_ADDR_LEN];

		bcopy(IF_LLADDR(ifp), ucaddr, sizeof(ucaddr));
		rc = t4_change_mac(sc, sc->mbox, pi->viid, pi->xact_addr_filt,
		    ucaddr, true, true);
		if (rc < 0) {
			rc = -rc;
			if_printf(ifp, "change_mac failed: %d\n", rc);
			return (rc);
		} else {
			pi->xact_addr_filt = rc;
			rc = 0;
		}
	}

	if (flags & XGMAC_MCADDRS) {
		const uint8_t *mcaddr;
		int del = 1;
		uint64_t hash = 0;
		struct ifmultiaddr *ifma;

		if_maddr_rlock(ifp);
		TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
			if (ifma->ifma_addr->sa_family != AF_LINK)
				continue;
			mcaddr = LLADDR((struct sockaddr_dl *)ifma->ifma_addr);

			rc = t4_alloc_mac_filt(sc, sc->mbox, pi->viid, del, 1,
			    &mcaddr, NULL, &hash, 0);
			if (rc < 0) {
				rc = -rc;
				if_printf(ifp, "failed to add mc address"
				    " %02x:%02x:%02x:%02x:%02x:%02x rc=%d\n",
				    mcaddr[0], mcaddr[1], mcaddr[2], mcaddr[3],
				    mcaddr[4], mcaddr[5], rc);
				goto mcfail;
			}
			del = 0;
		}

		rc = -t4_set_addr_hash(sc, sc->mbox, pi->viid, 0, hash, 0);
		if (rc != 0)
			if_printf(ifp, "failed to set mc address hash: %d", rc);
mcfail:
		if_maddr_runlock(ifp);
	}

	return (rc);
}

static int
cxgbe_init_locked(struct port_info *pi)
{
	struct adapter *sc = pi->adapter;
	int rc = 0;

	ADAPTER_LOCK_ASSERT_OWNED(sc);

	while (!IS_DOOMED(pi) && IS_BUSY(sc)) {
		if (mtx_sleep(&sc->flags, &sc->sc_lock, PCATCH, "t4init", 0)) {
			rc = EINTR;
			goto done;
		}
	}
	if (IS_DOOMED(pi)) {
		rc = ENXIO;
		goto done;
	}
	KASSERT(!IS_BUSY(sc), ("%s: controller busy.", __func__));

	/* Give up the adapter lock, port init code can sleep. */
	SET_BUSY(sc);
	ADAPTER_UNLOCK(sc);

	rc = cxgbe_init_synchronized(pi);

done:
	ADAPTER_LOCK(sc);
	KASSERT(IS_BUSY(sc), ("%s: controller not busy.", __func__));
	CLR_BUSY(sc);
	wakeup_one(&sc->flags);
	ADAPTER_UNLOCK(sc);
	return (rc);
}

static int
cxgbe_init_synchronized(struct port_info *pi)
{
	struct adapter *sc = pi->adapter;
	struct ifnet *ifp = pi->ifp;
	int rc = 0, i;
	uint16_t *rss;
	struct sge_rxq *rxq;

	ADAPTER_LOCK_ASSERT_NOTOWNED(sc);

	if (isset(&sc->open_device_map, pi->port_id)) {
		KASSERT(ifp->if_drv_flags & IFF_DRV_RUNNING,
		    ("mismatch between open_device_map and if_drv_flags"));
		return (0);	/* already running */
	}

	if (sc->open_device_map == 0 && ((rc = first_port_up(sc)) != 0))
		return (rc);	/* error message displayed already */

	/*
	 * Allocate tx/rx/fl queues for this port.
	 */
	rc = t4_setup_eth_queues(pi);
	if (rc != 0)
		goto done;	/* error message displayed already */

	/*
	 * Setup RSS for this port.
	 */
	rss = malloc(pi->nrxq * sizeof (*rss), M_CXGBE, M_ZERO | M_WAITOK);
	for_each_rxq(pi, i, rxq) {
		rss[i] = rxq->iq.abs_id;
	}
	rc = -t4_config_rss_range(sc, sc->mbox, pi->viid, 0, pi->rss_size, rss,
	    pi->nrxq);
	free(rss, M_CXGBE);
	if (rc != 0) {
		if_printf(ifp, "rss_config failed: %d\n", rc);
		goto done;
	}

	PORT_LOCK(pi);
	rc = update_mac_settings(pi, XGMAC_ALL);
	PORT_UNLOCK(pi);
	if (rc)
		goto done;	/* error message displayed already */

	rc = -t4_link_start(sc, sc->mbox, pi->tx_chan, &pi->link_cfg);
	if (rc != 0) {
		if_printf(ifp, "start_link failed: %d\n", rc);
		goto done;
	}

	rc = -t4_enable_vi(sc, sc->mbox, pi->viid, true, true);
	if (rc != 0) {
		if_printf(ifp, "enable_vi failed: %d\n", rc);
		goto done;
	}
	pi->flags |= VI_ENABLED;

	/* all ok */
	setbit(&sc->open_device_map, pi->port_id);
	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	callout_reset(&pi->tick, hz, cxgbe_tick, pi);
done:
	if (rc != 0)
		cxgbe_uninit_synchronized(pi);

	return (rc);
}

static int
cxgbe_uninit_locked(struct port_info *pi)
{
	struct adapter *sc = pi->adapter;
	int rc;

	ADAPTER_LOCK_ASSERT_OWNED(sc);

	while (!IS_DOOMED(pi) && IS_BUSY(sc)) {
		if (mtx_sleep(&sc->flags, &sc->sc_lock, PCATCH, "t4uninit", 0)) {
			rc = EINTR;
			goto done;
		}
	}
	if (IS_DOOMED(pi)) {
		rc = ENXIO;
		goto done;
	}
	KASSERT(!IS_BUSY(sc), ("%s: controller busy.", __func__));
	SET_BUSY(sc);
	ADAPTER_UNLOCK(sc);

	rc = cxgbe_uninit_synchronized(pi);

	ADAPTER_LOCK(sc);
	KASSERT(IS_BUSY(sc), ("%s: controller not busy.", __func__));
	CLR_BUSY(sc);
	wakeup_one(&sc->flags);
done:
	ADAPTER_UNLOCK(sc);
	return (rc);
}

/*
 * Idempotent.
 */
static int
cxgbe_uninit_synchronized(struct port_info *pi)
{
	struct adapter *sc = pi->adapter;
	struct ifnet *ifp = pi->ifp;
	int rc;

	/*
	 * taskqueue_drain may cause a deadlock if the adapter lock is held.
	 */
	ADAPTER_LOCK_ASSERT_NOTOWNED(sc);

	/*
	 * Clear this port's bit from the open device map, and then drain
	 * tasks and callouts.
	 */
	clrbit(&sc->open_device_map, pi->port_id);

	PORT_LOCK(pi);
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
	callout_stop(&pi->tick);
	PORT_UNLOCK(pi);
	callout_drain(&pi->tick);

	/*
	 * Stop and then free the queues' resources, including the queues
	 * themselves.
	 *
	 * XXX: we could just stop the queues here (on ifconfig down) and free
	 * them later (on port detach), but having up/down go through the entire
	 * allocate/activate/deactivate/free sequence is a good way to find
	 * leaks and bugs.
	 */
	rc = t4_teardown_eth_queues(pi);
	if (rc != 0)
		if_printf(ifp, "teardown failed: %d\n", rc);

	if (pi->flags & VI_ENABLED) {
		rc = -t4_enable_vi(sc, sc->mbox, pi->viid, false, false);
		if (rc)
			if_printf(ifp, "disable_vi failed: %d\n", rc);
		else
			pi->flags &= ~VI_ENABLED;
	}

	pi->link_cfg.link_ok = 0;
	pi->link_cfg.speed = 0;
	t4_os_link_changed(sc, pi->port_id, 0);

	if (sc->open_device_map == 0)
		last_port_down(sc);

	return (0);
}

#define T4_ALLOC_IRQ(sc, irqid, rid, handler, arg, name) do { \
	rc = t4_alloc_irq(sc, &sc->irq[irqid], rid, handler, arg, name); \
	if (rc != 0) \
		goto done; \
} while (0)
static int
first_port_up(struct adapter *sc)
{
	int rc, i;
	char name[8];

	ADAPTER_LOCK_ASSERT_NOTOWNED(sc);

	/*
	 * The firmware event queue and the optional forwarded interrupt queues.
	 */
	rc = t4_setup_adapter_iqs(sc);
	if (rc != 0)
		goto done;

	/*
	 * Setup interrupts.
	 */
	if (sc->intr_count == 1) {
		KASSERT(sc->flags & INTR_FWD,
		    ("%s: single interrupt but not forwarded?", __func__));
		T4_ALLOC_IRQ(sc, 0, 0, t4_intr_all, sc, "all");
	} else {
		/* Multiple interrupts.  The first one is always error intr */
		T4_ALLOC_IRQ(sc, 0, 1, t4_intr_err, sc, "err");

		if (sc->flags & INTR_FWD) {
			/* The rest are shared by the fwq and all data intr */
			for (i = 1; i < sc->intr_count; i++) {
				snprintf(name, sizeof(name), "mux%d", i - 1);
				T4_ALLOC_IRQ(sc, i, i + 1, t4_intr_fwd,
				    &sc->sge.fiq[i - 1], name);
			}
		} else {
			struct port_info *pi;
			int p, q;

			T4_ALLOC_IRQ(sc, 1, 2, t4_intr_evt, &sc->sge.fwq,
			    "evt");

			p = q = 0;
			pi = sc->port[p];
			for (i = 2; i < sc->intr_count; i++) {
				snprintf(name, sizeof(name), "p%dq%d", p, q);
				if (++q >= pi->nrxq) {
					p++;
					q = 0;
					pi = sc->port[p];
				}
				T4_ALLOC_IRQ(sc, i, i + 1, t4_intr_data,
				    &sc->sge.rxq[i - 2], name);
			}
		}
	}

	t4_intr_enable(sc);
	sc->flags |= FULL_INIT_DONE;

done:
	if (rc != 0)
		last_port_down(sc);

	return (rc);
}
#undef T4_ALLOC_IRQ

/*
 * Idempotent.
 */
static int
last_port_down(struct adapter *sc)
{
	int i;

	ADAPTER_LOCK_ASSERT_NOTOWNED(sc);

	t4_intr_disable(sc);

	t4_teardown_adapter_iqs(sc);

	for (i = 0; i < sc->intr_count; i++)
		t4_free_irq(sc, &sc->irq[i]);

	sc->flags &= ~FULL_INIT_DONE;

	return (0);
}

static int
t4_alloc_irq(struct adapter *sc, struct irq *irq, int rid,
    iq_intr_handler_t *handler, void *arg, char *name)
{
	int rc;

	irq->rid = rid;
	irq->res = bus_alloc_resource_any(sc->dev, SYS_RES_IRQ, &irq->rid,
	    RF_SHAREABLE | RF_ACTIVE);
	if (irq->res == NULL) {
		device_printf(sc->dev,
		    "failed to allocate IRQ for rid %d, name %s.\n", rid, name);
		return (ENOMEM);
	}

	rc = bus_setup_intr(sc->dev, irq->res, INTR_MPSAFE | INTR_TYPE_NET,
	    NULL, handler, arg, &irq->tag);
	if (rc != 0) {
		device_printf(sc->dev,
		    "failed to setup interrupt for rid %d, name %s: %d\n",
		    rid, name, rc);
	} else if (name)
		bus_describe_intr(sc->dev, irq->res, irq->tag, name);

	return (rc);
}

static int
t4_free_irq(struct adapter *sc, struct irq *irq)
{
	if (irq->tag)
		bus_teardown_intr(sc->dev, irq->res, irq->tag);
	if (irq->res)
		bus_release_resource(sc->dev, SYS_RES_IRQ, irq->rid, irq->res);

	bzero(irq, sizeof(*irq));

	return (0);
}

static void
reg_block_dump(struct adapter *sc, uint8_t *buf, unsigned int start,
    unsigned int end)
{
	uint32_t *p = (uint32_t *)(buf + start);

	for ( ; start <= end; start += sizeof(uint32_t))
		*p++ = t4_read_reg(sc, start);
}

static void
t4_get_regs(struct adapter *sc, struct t4_regdump *regs, uint8_t *buf)
{
	int i;
	static const unsigned int reg_ranges[] = {
		0x1008, 0x1108,
		0x1180, 0x11b4,
		0x11fc, 0x123c,
		0x1300, 0x173c,
		0x1800, 0x18fc,
		0x3000, 0x30d8,
		0x30e0, 0x5924,
		0x5960, 0x59d4,
		0x5a00, 0x5af8,
		0x6000, 0x6098,
		0x6100, 0x6150,
		0x6200, 0x6208,
		0x6240, 0x6248,
		0x6280, 0x6338,
		0x6370, 0x638c,
		0x6400, 0x643c,
		0x6500, 0x6524,
		0x6a00, 0x6a38,
		0x6a60, 0x6a78,
		0x6b00, 0x6b84,
		0x6bf0, 0x6c84,
		0x6cf0, 0x6d84,
		0x6df0, 0x6e84,
		0x6ef0, 0x6f84,
		0x6ff0, 0x7084,
		0x70f0, 0x7184,
		0x71f0, 0x7284,
		0x72f0, 0x7384,
		0x73f0, 0x7450,
		0x7500, 0x7530,
		0x7600, 0x761c,
		0x7680, 0x76cc,
		0x7700, 0x7798,
		0x77c0, 0x77fc,
		0x7900, 0x79fc,
		0x7b00, 0x7c38,
		0x7d00, 0x7efc,
		0x8dc0, 0x8e1c,
		0x8e30, 0x8e78,
		0x8ea0, 0x8f6c,
		0x8fc0, 0x9074,
		0x90fc, 0x90fc,
		0x9400, 0x9458,
		0x9600, 0x96bc,
		0x9800, 0x9808,
		0x9820, 0x983c,
		0x9850, 0x9864,
		0x9c00, 0x9c6c,
		0x9c80, 0x9cec,
		0x9d00, 0x9d6c,
		0x9d80, 0x9dec,
		0x9e00, 0x9e6c,
		0x9e80, 0x9eec,
		0x9f00, 0x9f6c,
		0x9f80, 0x9fec,
		0xd004, 0xd03c,
		0xdfc0, 0xdfe0,
		0xe000, 0xea7c,
		0xf000, 0x11190,
		0x19040, 0x19124,
		0x19150, 0x191b0,
		0x191d0, 0x191e8,
		0x19238, 0x1924c,
		0x193f8, 0x19474,
		0x19490, 0x194f8,
		0x19800, 0x19f30,
		0x1a000, 0x1a06c,
		0x1a0b0, 0x1a120,
		0x1a128, 0x1a138,
		0x1a190, 0x1a1c4,
		0x1a1fc, 0x1a1fc,
		0x1e040, 0x1e04c,
		0x1e240, 0x1e28c,
		0x1e2c0, 0x1e2c0,
		0x1e2e0, 0x1e2e0,
		0x1e300, 0x1e384,
		0x1e3c0, 0x1e3c8,
		0x1e440, 0x1e44c,
		0x1e640, 0x1e68c,
		0x1e6c0, 0x1e6c0,
		0x1e6e0, 0x1e6e0,
		0x1e700, 0x1e784,
		0x1e7c0, 0x1e7c8,
		0x1e840, 0x1e84c,
		0x1ea40, 0x1ea8c,
		0x1eac0, 0x1eac0,
		0x1eae0, 0x1eae0,
		0x1eb00, 0x1eb84,
		0x1ebc0, 0x1ebc8,
		0x1ec40, 0x1ec4c,
		0x1ee40, 0x1ee8c,
		0x1eec0, 0x1eec0,
		0x1eee0, 0x1eee0,
		0x1ef00, 0x1ef84,
		0x1efc0, 0x1efc8,
		0x1f040, 0x1f04c,
		0x1f240, 0x1f28c,
		0x1f2c0, 0x1f2c0,
		0x1f2e0, 0x1f2e0,
		0x1f300, 0x1f384,
		0x1f3c0, 0x1f3c8,
		0x1f440, 0x1f44c,
		0x1f640, 0x1f68c,
		0x1f6c0, 0x1f6c0,
		0x1f6e0, 0x1f6e0,
		0x1f700, 0x1f784,
		0x1f7c0, 0x1f7c8,
		0x1f840, 0x1f84c,
		0x1fa40, 0x1fa8c,
		0x1fac0, 0x1fac0,
		0x1fae0, 0x1fae0,
		0x1fb00, 0x1fb84,
		0x1fbc0, 0x1fbc8,
		0x1fc40, 0x1fc4c,
		0x1fe40, 0x1fe8c,
		0x1fec0, 0x1fec0,
		0x1fee0, 0x1fee0,
		0x1ff00, 0x1ff84,
		0x1ffc0, 0x1ffc8,
		0x20000, 0x2002c,
		0x20100, 0x2013c,
		0x20190, 0x201c8,
		0x20200, 0x20318,
		0x20400, 0x20528,
		0x20540, 0x20614,
		0x21000, 0x21040,
		0x2104c, 0x21060,
		0x210c0, 0x210ec,
		0x21200, 0x21268,
		0x21270, 0x21284,
		0x212fc, 0x21388,
		0x21400, 0x21404,
		0x21500, 0x21518,
		0x2152c, 0x2153c,
		0x21550, 0x21554,
		0x21600, 0x21600,
		0x21608, 0x21628,
		0x21630, 0x2163c,
		0x21700, 0x2171c,
		0x21780, 0x2178c,
		0x21800, 0x21c38,
		0x21c80, 0x21d7c,
		0x21e00, 0x21e04,
		0x22000, 0x2202c,
		0x22100, 0x2213c,
		0x22190, 0x221c8,
		0x22200, 0x22318,
		0x22400, 0x22528,
		0x22540, 0x22614,
		0x23000, 0x23040,
		0x2304c, 0x23060,
		0x230c0, 0x230ec,
		0x23200, 0x23268,
		0x23270, 0x23284,
		0x232fc, 0x23388,
		0x23400, 0x23404,
		0x23500, 0x23518,
		0x2352c, 0x2353c,
		0x23550, 0x23554,
		0x23600, 0x23600,
		0x23608, 0x23628,
		0x23630, 0x2363c,
		0x23700, 0x2371c,
		0x23780, 0x2378c,
		0x23800, 0x23c38,
		0x23c80, 0x23d7c,
		0x23e00, 0x23e04,
		0x24000, 0x2402c,
		0x24100, 0x2413c,
		0x24190, 0x241c8,
		0x24200, 0x24318,
		0x24400, 0x24528,
		0x24540, 0x24614,
		0x25000, 0x25040,
		0x2504c, 0x25060,
		0x250c0, 0x250ec,
		0x25200, 0x25268,
		0x25270, 0x25284,
		0x252fc, 0x25388,
		0x25400, 0x25404,
		0x25500, 0x25518,
		0x2552c, 0x2553c,
		0x25550, 0x25554,
		0x25600, 0x25600,
		0x25608, 0x25628,
		0x25630, 0x2563c,
		0x25700, 0x2571c,
		0x25780, 0x2578c,
		0x25800, 0x25c38,
		0x25c80, 0x25d7c,
		0x25e00, 0x25e04,
		0x26000, 0x2602c,
		0x26100, 0x2613c,
		0x26190, 0x261c8,
		0x26200, 0x26318,
		0x26400, 0x26528,
		0x26540, 0x26614,
		0x27000, 0x27040,
		0x2704c, 0x27060,
		0x270c0, 0x270ec,
		0x27200, 0x27268,
		0x27270, 0x27284,
		0x272fc, 0x27388,
		0x27400, 0x27404,
		0x27500, 0x27518,
		0x2752c, 0x2753c,
		0x27550, 0x27554,
		0x27600, 0x27600,
		0x27608, 0x27628,
		0x27630, 0x2763c,
		0x27700, 0x2771c,
		0x27780, 0x2778c,
		0x27800, 0x27c38,
		0x27c80, 0x27d7c,
		0x27e00, 0x27e04
	};

	regs->version = 4 | (sc->params.rev << 10);
	for (i = 0; i < ARRAY_SIZE(reg_ranges); i += 2)
		reg_block_dump(sc, buf, reg_ranges[i], reg_ranges[i + 1]);
}

static void
cxgbe_tick(void *arg)
{
	struct port_info *pi = arg;
	struct ifnet *ifp = pi->ifp;
	struct sge_txq *txq;
	int i, drops;
	struct port_stats *s = &pi->stats;

	PORT_LOCK(pi);
	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		PORT_UNLOCK(pi);
		return;	/* without scheduling another callout */
	}

	t4_get_port_stats(pi->adapter, pi->tx_chan, s);

	ifp->if_opackets = s->tx_frames;
	ifp->if_ipackets = s->rx_frames;
	ifp->if_obytes = s->tx_octets;
	ifp->if_ibytes = s->rx_octets;
	ifp->if_omcasts = s->tx_mcast_frames;
	ifp->if_imcasts = s->rx_mcast_frames;
	ifp->if_iqdrops = s->rx_ovflow0 + s->rx_ovflow1 + s->rx_ovflow2 +
	    s->rx_ovflow3;

	drops = s->tx_drop;
	for_each_txq(pi, i, txq)
		drops += txq->eq.br->br_drops;
	ifp->if_snd.ifq_drops = drops;

	ifp->if_oerrors = s->tx_error_frames;
	ifp->if_ierrors = s->rx_jabber + s->rx_runt + s->rx_too_long +
	    s->rx_fcs_err + s->rx_len_err;

	callout_schedule(&pi->tick, hz);
	PORT_UNLOCK(pi);
}

static int
t4_sysctls(struct adapter *sc)
{
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *oid;
	struct sysctl_oid_list *children;

	ctx = device_get_sysctl_ctx(sc->dev);
	oid = device_get_sysctl_tree(sc->dev);
	children = SYSCTL_CHILDREN(oid);

	SYSCTL_ADD_INT(ctx, children, OID_AUTO, "nports", CTLFLAG_RD,
	    &sc->params.nports, 0, "# of ports");

	SYSCTL_ADD_INT(ctx, children, OID_AUTO, "hw_revision", CTLFLAG_RD,
	    &sc->params.rev, 0, "chip hardware revision");

	SYSCTL_ADD_STRING(ctx, children, OID_AUTO, "firmware_version",
	    CTLFLAG_RD, &sc->fw_version, 0, "firmware version");

	SYSCTL_ADD_INT(ctx, children, OID_AUTO, "TOE", CTLFLAG_RD,
	    &sc->params.offload, 0, "hardware is capable of TCP offload");

	SYSCTL_ADD_INT(ctx, children, OID_AUTO, "core_clock", CTLFLAG_RD,
	    &sc->params.vpd.cclk, 0, "core clock frequency (in KHz)");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "holdoff_timers",
	    CTLTYPE_STRING | CTLFLAG_RD, &intr_timer, sizeof(intr_timer),
	    sysctl_int_array, "A", "interrupt holdoff timer values (us)");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "holdoff_pkt_counts",
	    CTLTYPE_STRING | CTLFLAG_RD, &intr_pktcount, sizeof(intr_pktcount),
	    sysctl_int_array, "A", "interrupt holdoff packet counter values");

	return (0);
}

static int
cxgbe_sysctls(struct port_info *pi)
{
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *oid;
	struct sysctl_oid_list *children;

	ctx = device_get_sysctl_ctx(pi->dev);

	/*
	 * dev.cxgbe.X.
	 */
	oid = device_get_sysctl_tree(pi->dev);
	children = SYSCTL_CHILDREN(oid);

	SYSCTL_ADD_INT(ctx, children, OID_AUTO, "nrxq", CTLFLAG_RD,
	    &pi->nrxq, 0, "# of rx queues");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, "ntxq", CTLFLAG_RD,
	    &pi->ntxq, 0, "# of tx queues");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, "first_rxq", CTLFLAG_RD,
	    &pi->first_rxq, 0, "index of first rx queue");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, "first_txq", CTLFLAG_RD,
	    &pi->first_txq, 0, "index of first tx queue");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "holdoff_tmr_idx",
	    CTLTYPE_INT | CTLFLAG_RW, pi, 0, sysctl_holdoff_tmr_idx, "I",
	    "holdoff timer index");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "holdoff_pktc_idx",
	    CTLTYPE_INT | CTLFLAG_RW, pi, 0, sysctl_holdoff_pktc_idx, "I",
	    "holdoff packet counter index");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "qsize_rxq",
	    CTLTYPE_INT | CTLFLAG_RW, pi, 0, sysctl_qsize_rxq, "I",
	    "rx queue size");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "qsize_txq",
	    CTLTYPE_INT | CTLFLAG_RW, pi, 0, sysctl_qsize_txq, "I",
	    "tx queue size");

	/*
	 * dev.cxgbe.X.stats.
	 */
	oid = SYSCTL_ADD_NODE(ctx, children, OID_AUTO, "stats", CTLFLAG_RD,
	    NULL, "port statistics");
	children = SYSCTL_CHILDREN(oid);

#define SYSCTL_ADD_T4_REG64(pi, name, desc, reg) \
	SYSCTL_ADD_OID(ctx, children, OID_AUTO, name, \
	    CTLTYPE_QUAD | CTLFLAG_RD, pi->adapter, reg, \
	    sysctl_handle_t4_reg64, "QU", desc)

	SYSCTL_ADD_T4_REG64(pi, "tx_octets", "# of octets in good frames",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_BYTES_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_frames", "total # of good frames",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_FRAMES_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_bcast_frames", "# of broadcast frames",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_BCAST_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_mcast_frames", "# of multicast frames",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_MCAST_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_ucast_frames", "# of unicast frames",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_UCAST_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_error_frames", "# of error frames",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_ERROR_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_frames_64",
	    "# of tx frames in this range",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_64B_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_frames_65_127",
	    "# of tx frames in this range",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_65B_127B_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_frames_128_255",
	    "# of tx frames in this range",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_128B_255B_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_frames_256_511",
	    "# of tx frames in this range",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_256B_511B_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_frames_512_1023",
	    "# of tx frames in this range",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_512B_1023B_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_frames_1024_1518",
	    "# of tx frames in this range",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_1024B_1518B_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_frames_1519_max",
	    "# of tx frames in this range",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_1519B_MAX_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_drop", "# of dropped tx frames",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_DROP_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_pause", "# of pause frames transmitted",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_PAUSE_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_ppp0", "# of PPP prio 0 frames transmitted",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_PPP0_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_ppp1", "# of PPP prio 1 frames transmitted",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_PPP1_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_ppp2", "# of PPP prio 2 frames transmitted",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_PPP2_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_ppp3", "# of PPP prio 3 frames transmitted",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_PPP3_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_ppp4", "# of PPP prio 4 frames transmitted",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_PPP4_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_ppp5", "# of PPP prio 5 frames transmitted",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_PPP5_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_ppp6", "# of PPP prio 6 frames transmitted",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_PPP6_L));
	SYSCTL_ADD_T4_REG64(pi, "tx_ppp7", "# of PPP prio 7 frames transmitted",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_TX_PORT_PPP7_L));

	SYSCTL_ADD_T4_REG64(pi, "rx_octets", "# of octets in good frames",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_BYTES_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_frames", "total # of good frames",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_FRAMES_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_bcast_frames", "# of broadcast frames",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_BCAST_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_mcast_frames", "# of multicast frames",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_MCAST_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_ucast_frames", "# of unicast frames",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_UCAST_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_too_long", "# of frames exceeding MTU",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_MTU_ERROR_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_jabber", "# of jabber frames",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_MTU_CRC_ERROR_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_fcs_err",
	    "# of frames received with bad FCS",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_CRC_ERROR_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_len_err",
	    "# of frames received with length error",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_LEN_ERROR_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_symbol_err", "symbol errors",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_SYM_ERROR_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_runt", "# of short frames received",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_LESS_64B_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_frames_64",
	    "# of rx frames in this range",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_64B_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_frames_65_127",
	    "# of rx frames in this range",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_65B_127B_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_frames_128_255",
	    "# of rx frames in this range",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_128B_255B_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_frames_256_511",
	    "# of rx frames in this range",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_256B_511B_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_frames_512_1023",
	    "# of rx frames in this range",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_512B_1023B_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_frames_1024_1518",
	    "# of rx frames in this range",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_1024B_1518B_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_frames_1519_max",
	    "# of rx frames in this range",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_1519B_MAX_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_pause", "# of pause frames received",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_PAUSE_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_ppp0", "# of PPP prio 0 frames received",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_PPP0_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_ppp1", "# of PPP prio 1 frames received",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_PPP1_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_ppp2", "# of PPP prio 2 frames received",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_PPP2_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_ppp3", "# of PPP prio 3 frames received",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_PPP3_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_ppp4", "# of PPP prio 4 frames received",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_PPP4_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_ppp5", "# of PPP prio 5 frames received",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_PPP5_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_ppp6", "# of PPP prio 6 frames received",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_PPP6_L));
	SYSCTL_ADD_T4_REG64(pi, "rx_ppp7", "# of PPP prio 7 frames received",
	    PORT_REG(pi->tx_chan, A_MPS_PORT_STAT_RX_PORT_PPP7_L));

#undef SYSCTL_ADD_T4_REG64

#define SYSCTL_ADD_T4_PORTSTAT(name, desc) \
	SYSCTL_ADD_QUAD(ctx, children, OID_AUTO, #name, CTLFLAG_RD, \
	    &pi->stats.name, desc)

	/* We get these from port_stats and they may be stale by upto 1s */
	SYSCTL_ADD_T4_PORTSTAT(rx_ovflow0,
	    "# drops due to buffer-group 0 overflows");
	SYSCTL_ADD_T4_PORTSTAT(rx_ovflow1,
	    "# drops due to buffer-group 1 overflows");
	SYSCTL_ADD_T4_PORTSTAT(rx_ovflow2,
	    "# drops due to buffer-group 2 overflows");
	SYSCTL_ADD_T4_PORTSTAT(rx_ovflow3,
	    "# drops due to buffer-group 3 overflows");
	SYSCTL_ADD_T4_PORTSTAT(rx_trunc0,
	    "# of buffer-group 0 truncated packets");
	SYSCTL_ADD_T4_PORTSTAT(rx_trunc1,
	    "# of buffer-group 1 truncated packets");
	SYSCTL_ADD_T4_PORTSTAT(rx_trunc2,
	    "# of buffer-group 2 truncated packets");
	SYSCTL_ADD_T4_PORTSTAT(rx_trunc3,
	    "# of buffer-group 3 truncated packets");

#undef SYSCTL_ADD_T4_PORTSTAT

	return (0);
}

static int
sysctl_int_array(SYSCTL_HANDLER_ARGS)
{
	int rc, *i;
	struct sbuf sb;

	sbuf_new(&sb, NULL, 32, SBUF_AUTOEXTEND);
	for (i = arg1; arg2; arg2 -= sizeof(int), i++)
		sbuf_printf(&sb, "%d ", *i);
	sbuf_trim(&sb);
	sbuf_finish(&sb);
	rc = sysctl_handle_string(oidp, sbuf_data(&sb), sbuf_len(&sb), req);
	sbuf_delete(&sb);
	return (rc);
}

static int
sysctl_holdoff_tmr_idx(SYSCTL_HANDLER_ARGS)
{
	struct port_info *pi = arg1;
	struct adapter *sc = pi->adapter;
	struct sge_rxq *rxq;
	int idx, rc, i;

	idx = pi->tmr_idx;

	rc = sysctl_handle_int(oidp, &idx, 0, req);
	if (rc != 0 || req->newptr == NULL)
		return (rc);

	if (idx < 0 || idx >= SGE_NTIMERS)
		return (EINVAL);

	ADAPTER_LOCK(sc);
	rc = IS_DOOMED(pi) ? ENXIO : (IS_BUSY(sc) ? EBUSY : 0);
	if (rc == 0) {
		for_each_rxq(pi, i, rxq) {
			rxq->iq.intr_params = V_QINTR_TIMER_IDX(idx) |
			    V_QINTR_CNT_EN(pi->pktc_idx != -1);
		}
		pi->tmr_idx = idx;
	}

	ADAPTER_UNLOCK(sc);
	return (rc);
}

static int
sysctl_holdoff_pktc_idx(SYSCTL_HANDLER_ARGS)
{
	struct port_info *pi = arg1;
	struct adapter *sc = pi->adapter;
	int idx, rc;

	idx = pi->pktc_idx;

	rc = sysctl_handle_int(oidp, &idx, 0, req);
	if (rc != 0 || req->newptr == NULL)
		return (rc);

	if (idx < -1 || idx >= SGE_NCOUNTERS)
		return (EINVAL);

	ADAPTER_LOCK(sc);
	rc = IS_DOOMED(pi) ? ENXIO : (IS_BUSY(sc) ? EBUSY : 0);
	if (rc == 0 && pi->ifp->if_drv_flags & IFF_DRV_RUNNING)
		rc = EBUSY; /* can be changed only when port is down */

	if (rc == 0)
		pi->pktc_idx = idx;

	ADAPTER_UNLOCK(sc);
	return (rc);
}

static int
sysctl_qsize_rxq(SYSCTL_HANDLER_ARGS)
{
	struct port_info *pi = arg1;
	struct adapter *sc = pi->adapter;
	int qsize, rc;

	qsize = pi->qsize_rxq;

	rc = sysctl_handle_int(oidp, &qsize, 0, req);
	if (rc != 0 || req->newptr == NULL)
		return (rc);

	if (qsize < 128 || (qsize & 7))
		return (EINVAL);

	ADAPTER_LOCK(sc);
	rc = IS_DOOMED(pi) ? ENXIO : (IS_BUSY(sc) ? EBUSY : 0);
	if (rc == 0 && pi->ifp->if_drv_flags & IFF_DRV_RUNNING)
		rc = EBUSY; /* can be changed only when port is down */

	if (rc == 0)
		pi->qsize_rxq = qsize;

	ADAPTER_UNLOCK(sc);
	return (rc);
}

static int
sysctl_qsize_txq(SYSCTL_HANDLER_ARGS)
{
	struct port_info *pi = arg1;
	struct adapter *sc = pi->adapter;
	int qsize, rc;

	qsize = pi->qsize_txq;

	rc = sysctl_handle_int(oidp, &qsize, 0, req);
	if (rc != 0 || req->newptr == NULL)
		return (rc);

	if (qsize < 128)
		return (EINVAL);

	ADAPTER_LOCK(sc);
	rc = IS_DOOMED(pi) ? ENXIO : (IS_BUSY(sc) ? EBUSY : 0);
	if (rc == 0 && pi->ifp->if_drv_flags & IFF_DRV_RUNNING)
		rc = EBUSY; /* can be changed only when port is down */

	if (rc == 0)
		pi->qsize_txq = qsize;

	ADAPTER_UNLOCK(sc);
	return (rc);
}

static int
sysctl_handle_t4_reg64(SYSCTL_HANDLER_ARGS)
{
	struct adapter *sc = arg1;
	int reg = arg2;
	uint64_t val;

	val = t4_read_reg64(sc, reg);

	return (sysctl_handle_quad(oidp, &val, 0, req));
}

static inline void
txq_start(struct ifnet *ifp, struct sge_txq *txq)
{
	struct buf_ring *br;
	struct mbuf *m;

	TXQ_LOCK_ASSERT_OWNED(txq);

	br = txq->eq.br;
	m = txq->m ? txq->m : drbr_dequeue(ifp, br);
	if (m)
		t4_eth_tx(ifp, txq, m);
}

void
cxgbe_txq_start(void *arg, int count)
{
	struct sge_txq *txq = arg;

	TXQ_LOCK(txq);
	if (txq->eq.flags & EQ_CRFLUSHED) {
		txq->eq.flags &= ~EQ_CRFLUSHED;
		txq_start(txq->ifp, txq);
	} else
		wakeup_one(txq);	/* txq is going away, wakeup free_txq */
	TXQ_UNLOCK(txq);
}

int
t4_os_find_pci_capability(struct adapter *sc, int cap)
{
	device_t dev;
	struct pci_devinfo *dinfo;
	pcicfgregs *cfg;
	uint32_t status;
	uint8_t ptr;

	dev = sc->dev;
	dinfo = device_get_ivars(dev);
	cfg = &dinfo->cfg;

	status = pci_read_config(dev, PCIR_STATUS, 2);
	if (!(status & PCIM_STATUS_CAPPRESENT))
		return (0);

	switch (cfg->hdrtype & PCIM_HDRTYPE) {
	case 0:
	case 1:
		ptr = PCIR_CAP_PTR;
		break;
	case 2:
		ptr = PCIR_CAP_PTR_2;
		break;
	default:
		return (0);
		break;
	}
	ptr = pci_read_config(dev, ptr, 1);

	while (ptr != 0) {
		if (pci_read_config(dev, ptr + PCICAP_ID, 1) == cap)
			return (ptr);
		ptr = pci_read_config(dev, ptr + PCICAP_NEXTPTR, 1);
	}

	return (0);
}

int
t4_os_pci_save_state(struct adapter *sc)
{
	device_t dev;
	struct pci_devinfo *dinfo;

	dev = sc->dev;
	dinfo = device_get_ivars(dev);

	pci_cfg_save(dev, dinfo, 0);
	return (0);
}

int
t4_os_pci_restore_state(struct adapter *sc)
{
	device_t dev;
	struct pci_devinfo *dinfo;

	dev = sc->dev;
	dinfo = device_get_ivars(dev);

	pci_cfg_restore(dev, dinfo);
	return (0);
}

void
t4_os_portmod_changed(const struct adapter *sc, int idx)
{
	struct port_info *pi = sc->port[idx];
	static const char *mod_str[] = {
		NULL, "LR", "SR", "ER", "TWINAX", "active TWINAX", "LRM"
	};

	if (pi->mod_type == FW_PORT_MOD_TYPE_NONE)
		if_printf(pi->ifp, "transceiver unplugged.\n");
	else if (pi->mod_type == FW_PORT_MOD_TYPE_UNKNOWN)
		if_printf(pi->ifp, "unknown transceiver inserted.\n");
	else if (pi->mod_type == FW_PORT_MOD_TYPE_NOTSUPPORTED)
		if_printf(pi->ifp, "unsupported transceiver inserted.\n");
	else if (pi->mod_type > 0 && pi->mod_type < ARRAY_SIZE(mod_str)) {
		if_printf(pi->ifp, "%s transceiver inserted.\n",
		    mod_str[pi->mod_type]);
	} else {
		if_printf(pi->ifp, "transceiver (type %d) inserted.\n",
		    pi->mod_type);
	}
}

void
t4_os_link_changed(struct adapter *sc, int idx, int link_stat)
{
	struct port_info *pi = sc->port[idx];
	struct ifnet *ifp = pi->ifp;

	if (link_stat) {
		ifp->if_baudrate = IF_Mbps(pi->link_cfg.speed);
		if_link_state_change(ifp, LINK_STATE_UP);
	} else
		if_link_state_change(ifp, LINK_STATE_DOWN);
}

static int
t4_open(struct cdev *dev, int flags, int type, struct thread *td)
{
       return (0);
}

static int
t4_close(struct cdev *dev, int flags, int type, struct thread *td)
{
       return (0);
}

static int
t4_ioctl(struct cdev *dev, unsigned long cmd, caddr_t data, int fflag,
    struct thread *td)
{
	int rc;
	struct adapter *sc = dev->si_drv1;

	rc = priv_check(td, PRIV_DRIVER);
	if (rc != 0)
		return (rc);

	switch (cmd) {
	case CHELSIO_T4_GETREG: {
		struct t4_reg *edata = (struct t4_reg *)data;

		if ((edata->addr & 0x3) != 0 || edata->addr >= sc->mmio_len)
			return (EFAULT);

		if (edata->size == 4)
			edata->val = t4_read_reg(sc, edata->addr);
		else if (edata->size == 8)
			edata->val = t4_read_reg64(sc, edata->addr);
		else
			return (EINVAL);

		break;
	}
	case CHELSIO_T4_SETREG: {
		struct t4_reg *edata = (struct t4_reg *)data;

		if ((edata->addr & 0x3) != 0 || edata->addr >= sc->mmio_len)
			return (EFAULT);

		if (edata->size == 4) {
			if (edata->val & 0xffffffff00000000)
				return (EINVAL);
			t4_write_reg(sc, edata->addr, (uint32_t) edata->val);
		} else if (edata->size == 8)
			t4_write_reg64(sc, edata->addr, edata->val);
		else
			return (EINVAL);
		break;
	}
	case CHELSIO_T4_REGDUMP: {
		struct t4_regdump *regs = (struct t4_regdump *)data;
		int reglen = T4_REGDUMP_SIZE;
		uint8_t *buf;

		if (regs->len < reglen) {
			regs->len = reglen; /* hint to the caller */
			return (ENOBUFS);
		}

		regs->len = reglen;
		buf = malloc(reglen, M_CXGBE, M_WAITOK | M_ZERO);
		t4_get_regs(sc, regs, buf);
		rc = copyout(buf, regs->data, reglen);
		free(buf, M_CXGBE);
		break;
	}
	default:
		rc = EINVAL;
	}

	return (rc);
}

static int
t4_mod_event(module_t mod, int cmd, void *arg)
{

	if (cmd == MOD_LOAD)
		t4_sge_modload();

	return (0);
}

static devclass_t t4_devclass;
static devclass_t cxgbe_devclass;

DRIVER_MODULE(t4nex, pci, t4_driver, t4_devclass, t4_mod_event, 0);
MODULE_VERSION(t4nex, 1);

DRIVER_MODULE(cxgbe, t4nex, cxgbe_driver, cxgbe_devclass, 0, 0);
MODULE_VERSION(cxgbe, 1);
