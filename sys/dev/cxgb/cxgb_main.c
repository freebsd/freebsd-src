/**************************************************************************

Copyright (c) 2007, Chelsio Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

2. Neither the name of the Chelsio Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

***************************************************************************/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/pciio.h>
#include <sys/conf.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus_dma.h>
#include <sys/rman.h>
#include <sys/ioccom.h>
#include <sys/mbuf.h>
#include <sys/linker.h>
#include <sys/firmware.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>

#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pci_private.h>

#ifdef CONFIG_DEFINED
#include <cxgb_include.h>
#else
#include <dev/cxgb/cxgb_include.h>
#endif

#ifdef PRIV_SUPPORTED
#include <sys/priv.h>
#endif

static int cxgb_setup_msix(adapter_t *, int);
static void cxgb_init(void *);
static void cxgb_init_locked(struct port_info *);
static void cxgb_stop_locked(struct port_info *);
static void cxgb_set_rxmode(struct port_info *);
static int cxgb_ioctl(struct ifnet *, unsigned long, caddr_t);
static void cxgb_start(struct ifnet *);
static void cxgb_start_proc(void *, int ncount);
static int cxgb_media_change(struct ifnet *);
static void cxgb_media_status(struct ifnet *, struct ifmediareq *);
static int setup_sge_qsets(adapter_t *);
static void cxgb_async_intr(void *);
static void cxgb_ext_intr_handler(void *, int);
static void cxgb_down(struct adapter *sc);
static void cxgb_tick(void *);
static void setup_rss(adapter_t *sc);

/* Attachment glue for the PCI controller end of the device.  Each port of
 * the device is attached separately, as defined later.
 */
static int cxgb_controller_probe(device_t);
static int cxgb_controller_attach(device_t);
static int cxgb_controller_detach(device_t);
static void cxgb_free(struct adapter *);
static __inline void reg_block_dump(struct adapter *ap, uint8_t *buf, unsigned int start,
    unsigned int end);
static void cxgb_get_regs(adapter_t *sc, struct ifconf_regs *regs, uint8_t *buf);
static int cxgb_get_regs_len(void);
static int offload_open(struct port_info *pi);
static int offload_close(struct toedev *tdev);



static device_method_t cxgb_controller_methods[] = {
	DEVMETHOD(device_probe,		cxgb_controller_probe),
	DEVMETHOD(device_attach,	cxgb_controller_attach),
	DEVMETHOD(device_detach,	cxgb_controller_detach),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	{ 0, 0 }
};

static driver_t cxgb_controller_driver = {
	"cxgbc",
	cxgb_controller_methods,
	sizeof(struct adapter)
};

static devclass_t	cxgb_controller_devclass;
DRIVER_MODULE(cxgbc, pci, cxgb_controller_driver, cxgb_controller_devclass, 0, 0);

/*
 * Attachment glue for the ports.  Attachment is done directly to the
 * controller device.
 */
static int cxgb_port_probe(device_t);
static int cxgb_port_attach(device_t);
static int cxgb_port_detach(device_t);

static device_method_t cxgb_port_methods[] = {
	DEVMETHOD(device_probe,		cxgb_port_probe),
	DEVMETHOD(device_attach,	cxgb_port_attach),
	DEVMETHOD(device_detach,	cxgb_port_detach),
	{ 0, 0 }
};

static driver_t cxgb_port_driver = {
	"cxgb",
	cxgb_port_methods,
	0
};

static d_ioctl_t cxgb_extension_ioctl;

static devclass_t	cxgb_port_devclass;
DRIVER_MODULE(cxgb, cxgbc, cxgb_port_driver, cxgb_port_devclass, 0, 0);

#define SGE_MSIX_COUNT (SGE_QSETS + 1)

extern int collapse_mbufs;
/*
 * The driver uses the best interrupt scheme available on a platform in the
 * order MSI-X, MSI, legacy pin interrupts.  This parameter determines which
 * of these schemes the driver may consider as follows:
 *
 * msi = 2: choose from among all three options
 * msi = 1 : only consider MSI and pin interrupts
 * msi = 0: force pin interrupts
 */
static int msi_allowed = 2;

TUNABLE_INT("hw.cxgb.msi_allowed", &msi_allowed);
SYSCTL_NODE(_hw, OID_AUTO, cxgb, CTLFLAG_RD, 0, "CXGB driver parameters");
SYSCTL_UINT(_hw_cxgb, OID_AUTO, msi_allowed, CTLFLAG_RDTUN, &msi_allowed, 0,
    "MSI-X, MSI, INTx selector");

/*
 * The driver enables offload as a default.
 * To disable it, use ofld_disable = 1.
 */
static int ofld_disable = 0;
TUNABLE_INT("hw.cxgb.ofld_disable", &ofld_disable);
SYSCTL_UINT(_hw_cxgb, OID_AUTO, ofld_disable, CTLFLAG_RDTUN, &ofld_disable, 0,
    "disable ULP offload");

/*
 * The driver uses an auto-queue algorithm by default.
 * To disable it and force a single queue-set per port, use singleq = 1.
 */
static int singleq = 1;
TUNABLE_INT("hw.cxgb.singleq", &singleq);
SYSCTL_UINT(_hw_cxgb, OID_AUTO, singleq, CTLFLAG_RDTUN, &singleq, 0,
    "use a single queue-set per port");

enum {
	MAX_TXQ_ENTRIES      = 16384,
	MAX_CTRL_TXQ_ENTRIES = 1024,
	MAX_RSPQ_ENTRIES     = 16384,
	MAX_RX_BUFFERS       = 16384,
	MAX_RX_JUMBO_BUFFERS = 16384,
	MIN_TXQ_ENTRIES      = 4,
	MIN_CTRL_TXQ_ENTRIES = 4,
	MIN_RSPQ_ENTRIES     = 32,
	MIN_FL_ENTRIES       = 32
};

#define PORT_MASK ((1 << MAX_NPORTS) - 1)

/* Table for probing the cards.  The desc field isn't actually used */
struct cxgb_ident {
	uint16_t	vendor;
	uint16_t	device;
	int		index;
	char		*desc;
} cxgb_identifiers[] = {
	{PCI_VENDOR_ID_CHELSIO, 0x0020, 0, "PE9000"},
	{PCI_VENDOR_ID_CHELSIO, 0x0021, 1, "T302E"},
	{PCI_VENDOR_ID_CHELSIO, 0x0022, 2, "T310E"},
	{PCI_VENDOR_ID_CHELSIO, 0x0023, 3, "T320X"},
	{PCI_VENDOR_ID_CHELSIO, 0x0024, 1, "T302X"},
	{PCI_VENDOR_ID_CHELSIO, 0x0025, 3, "T320E"},
	{PCI_VENDOR_ID_CHELSIO, 0x0026, 2, "T310X"},
	{PCI_VENDOR_ID_CHELSIO, 0x0030, 2, "T3B10"},
	{PCI_VENDOR_ID_CHELSIO, 0x0031, 3, "T3B20"},
	{PCI_VENDOR_ID_CHELSIO, 0x0032, 1, "T3B02"},
	{0, 0, 0, NULL}
};

static struct cxgb_ident *
cxgb_get_ident(device_t dev)
{
	struct cxgb_ident *id;

	for (id = cxgb_identifiers; id->desc != NULL; id++) {
		if ((id->vendor == pci_get_vendor(dev)) &&
		    (id->device == pci_get_device(dev))) {
			return (id);
		}
	}
	return (NULL);
}

static const struct adapter_info *
cxgb_get_adapter_info(device_t dev)
{
	struct cxgb_ident *id;
	const struct adapter_info *ai;
      
	id = cxgb_get_ident(dev);
	if (id == NULL)
		return (NULL);

	ai = t3_get_adapter_info(id->index);

	return (ai);
}

static int
cxgb_controller_probe(device_t dev)
{
	const struct adapter_info *ai;
	char *ports, buf[80];

	ai = cxgb_get_adapter_info(dev);
	if (ai == NULL)
		return (ENXIO);

	if (ai->nports == 1)
		ports = "port";
	else
		ports = "ports";

	snprintf(buf, sizeof(buf), "%s RNIC, %d %s", ai->desc, ai->nports, ports);
	device_set_desc_copy(dev, buf);
	return (BUS_PROBE_DEFAULT);
}

static int
upgrade_fw(adapter_t *sc)
{
	char buf[32];
#ifdef FIRMWARE_LATEST
	const struct firmware *fw;
#else
	struct firmware *fw;
#endif	
	int status;
	
	snprintf(&buf[0], sizeof(buf), "t3fw%d%d%d", FW_VERSION_MAJOR,
	    FW_VERSION_MINOR, FW_VERSION_MICRO);
	
	fw = firmware_get(buf);
	
	if (fw == NULL) {
		device_printf(sc->dev, "Could not find firmware image %s\n", buf);
		return (ENOENT);
	}
	
	status = t3_load_fw(sc, (const uint8_t *)fw->data, fw->datasize);

	firmware_put(fw, FIRMWARE_UNLOAD);

	return (status);	
}

static int
cxgb_controller_attach(device_t dev)
{
	device_t child;
	const struct adapter_info *ai;
	struct adapter *sc;
	int i, reg, msi_needed, error = 0;
	uint32_t vers;
	int port_qsets = 1;
	    
	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->msi_count = 0;
	
	/* find the PCIe link width and set max read request to 4KB*/
	if (pci_find_extcap(dev, PCIY_EXPRESS, &reg) == 0) {
		uint16_t lnk, pectl;
		lnk = pci_read_config(dev, reg + 0x12, 2);
		sc->link_width = (lnk >> 4) & 0x3f;
		
		pectl = pci_read_config(dev, reg + 0x8, 2);
		pectl = (pectl & ~0x7000) | (5 << 12);
		pci_write_config(dev, reg + 0x8, pectl, 2);
	}
	if (sc->link_width != 0 && sc->link_width <= 4) {
		device_printf(sc->dev,
		    "PCIe x%d Link, expect reduced performance\n",
		    sc->link_width);
	}
	
	pci_enable_busmaster(dev);

	/*
	 * Allocate the registers and make them available to the driver.
	 * The registers that we care about for NIC mode are in BAR 0
	 */
	sc->regs_rid = PCIR_BAR(0);
	if ((sc->regs_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->regs_rid, RF_ACTIVE)) == NULL) {
		device_printf(dev, "Cannot allocate BAR\n");
		return (ENXIO);
	}

	mtx_init(&sc->sge.reg_lock, "SGE reg lock", NULL, MTX_DEF);
	mtx_init(&sc->lock, "cxgb controller lock", NULL, MTX_DEF);
	mtx_init(&sc->mdio_lock, "cxgb mdio", NULL, MTX_DEF);
	
	sc->bt = rman_get_bustag(sc->regs_res);
	sc->bh = rman_get_bushandle(sc->regs_res);
	sc->mmio_len = rman_get_size(sc->regs_res);

	ai = cxgb_get_adapter_info(dev);
	if (t3_prep_adapter(sc, ai, 1) < 0) {
		error = ENODEV;
		goto out;
	}
	
	/* Allocate the BAR for doing MSI-X.  If it succeeds, try to allocate
	 * enough messages for the queue sets.  If that fails, try falling
	 * back to MSI.  If that fails, then try falling back to the legacy
	 * interrupt pin model.
	 */
#ifdef MSI_SUPPORTED

	sc->msix_regs_rid = 0x20;
	if ((msi_allowed >= 2) &&
	    (sc->msix_regs_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->msix_regs_rid, RF_ACTIVE)) != NULL) {

		msi_needed = sc->msi_count = SGE_MSIX_COUNT;

		if (((error = pci_alloc_msix(dev, &sc->msi_count)) != 0) ||
		    (sc->msi_count != msi_needed)) {
			device_printf(dev, "msix allocation failed - msi_count = %d"
			    " msi_needed=%d will try msi err=%d\n", sc->msi_count,
			    msi_needed, error);
			sc->msi_count = 0;
			pci_release_msi(dev);
			bus_release_resource(dev, SYS_RES_MEMORY,
			    sc->msix_regs_rid, sc->msix_regs_res);
			sc->msix_regs_res = NULL;
		} else {
			sc->flags |= USING_MSIX;
			sc->cxgb_intr = t3_intr_msix;
		}
	}

	if ((msi_allowed >= 1) && (sc->msi_count == 0)) {
		sc->msi_count = 1;
		if (pci_alloc_msi(dev, &sc->msi_count)) {
			device_printf(dev, "alloc msi failed - will try INTx\n");
			sc->msi_count = 0;
			pci_release_msi(dev);
		} else {
			sc->flags |= USING_MSI;
			sc->irq_rid = 1;
			sc->cxgb_intr = t3_intr_msi;
		}
	}
#endif
	if (sc->msi_count == 0) {
		device_printf(dev, "using line interrupts\n");
		sc->irq_rid = 0;
		sc->cxgb_intr = t3b_intr;
	}


	/* Create a private taskqueue thread for handling driver events */
#ifdef TASKQUEUE_CURRENT	
	sc->tq = taskqueue_create("cxgb_taskq", M_NOWAIT,
	    taskqueue_thread_enqueue, &sc->tq);
#else
	sc->tq = taskqueue_create_fast("cxgb_taskq", M_NOWAIT,
	    taskqueue_thread_enqueue, &sc->tq);
#endif	
	if (sc->tq == NULL) {
		device_printf(dev, "failed to allocate controller task queue\n");
		goto out;
	}
		
	taskqueue_start_threads(&sc->tq, 1, PI_NET, "%s taskq",
	    device_get_nameunit(dev));
	TASK_INIT(&sc->ext_intr_task, 0, cxgb_ext_intr_handler, sc);

	
	/* Create a periodic callout for checking adapter status */
	callout_init_mtx(&sc->cxgb_tick_ch, &sc->lock, CALLOUT_RETURNUNLOCKED);
	
	if (t3_check_fw_version(sc) != 0) {
		/*
		 * Warn user that a firmware update will be attempted in init.
		 */
		device_printf(dev, "firmware needs to be updated to version %d.%d.%d\n",
		    FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_MICRO);
		sc->flags &= ~FW_UPTODATE;
	} else {
		sc->flags |= FW_UPTODATE;
	}
	
	if ((sc->flags & USING_MSIX) && !singleq)
		port_qsets = min((SGE_QSETS/(sc)->params.nports), mp_ncpus);

	/*
	 * Create a child device for each MAC.  The ethernet attachment
	 * will be done in these children.
	 */	
	for (i = 0; i < (sc)->params.nports; i++) {
		if ((child = device_add_child(dev, "cxgb", -1)) == NULL) {
			device_printf(dev, "failed to add child port\n");
			error = EINVAL;
			goto out;
		}
		sc->portdev[i] = child;
		sc->port[i].adapter = sc;
		sc->port[i].nqsets = port_qsets;
		sc->port[i].first_qset = i*port_qsets;
		sc->port[i].port = i;
		device_set_softc(child, &sc->port[i]);
	}
	if ((error = bus_generic_attach(dev)) != 0)
		goto out;

	/*
	 * XXX need to poll for link status
	 */
	sc->params.stats_update_period = 1;

	/* initialize sge private state */
	t3_sge_init_sw(sc);

	t3_led_ready(sc);
	
	cxgb_offload_init();
	if (is_offload(sc)) {
		setbit(&sc->registered_device_map, OFFLOAD_DEVMAP_BIT);
		cxgb_adapter_ofld(sc);
        }
	error = t3_get_fw_version(sc, &vers);
	if (error)
		goto out;

	snprintf(&sc->fw_version[0], sizeof(sc->fw_version), "%d.%d.%d",
	    G_FW_VERSION_MAJOR(vers), G_FW_VERSION_MINOR(vers),
	    G_FW_VERSION_MICRO(vers));

	t3_add_sysctls(sc);
out:
	if (error)
		cxgb_free(sc);

	return (error);
}

static int
cxgb_controller_detach(device_t dev)
{
	struct adapter *sc;

	sc = device_get_softc(dev);

	cxgb_free(sc);

	return (0);
}

static void
cxgb_free(struct adapter *sc)
{
	int i;

	cxgb_down(sc);
	
#ifdef MSI_SUPPORTED
	if (sc->flags & (USING_MSI | USING_MSIX)) {
		device_printf(sc->dev, "releasing msi message(s)\n");
		pci_release_msi(sc->dev);
	} else {
		device_printf(sc->dev, "no msi message to release\n");
	}
#endif
	if (sc->msix_regs_res != NULL) {
		bus_release_resource(sc->dev, SYS_RES_MEMORY, sc->msix_regs_rid,
		    sc->msix_regs_res);
	}
	
	/*
	 * XXX need to drain the ifq by hand until
	 * it is taught about mbuf iovecs
	 */ 
	callout_drain(&sc->cxgb_tick_ch);

	t3_sge_deinit_sw(sc);

	if (sc->tq != NULL) {
		taskqueue_drain(sc->tq, &sc->ext_intr_task);
		taskqueue_free(sc->tq);
	}
	
	for (i = 0; i < (sc)->params.nports; ++i) {
		if (sc->portdev[i] != NULL)
			device_delete_child(sc->dev, sc->portdev[i]);
	}
		
	bus_generic_detach(sc->dev);

	if (is_offload(sc)) {
		cxgb_adapter_unofld(sc);
		if (isset(&sc->open_device_map,	OFFLOAD_DEVMAP_BIT))
			offload_close(&sc->tdev);
	}
	t3_free_sge_resources(sc);
	t3_sge_free(sc);

	if (sc->regs_res != NULL)
		bus_release_resource(sc->dev, SYS_RES_MEMORY, sc->regs_rid,
		    sc->regs_res);

	mtx_destroy(&sc->mdio_lock);
	mtx_destroy(&sc->sge.reg_lock);
	mtx_destroy(&sc->lock);
	
	return;
}

/**
 *	setup_sge_qsets - configure SGE Tx/Rx/response queues
 *	@sc: the controller softc
 *
 *	Determines how many sets of SGE queues to use and initializes them.
 *	We support multiple queue sets per port if we have MSI-X, otherwise
 *	just one queue set per port.
 */
static int
setup_sge_qsets(adapter_t *sc)
{
	int i, j, err, irq_idx, qset_idx;
	u_int ntxq = SGE_TXQ_PER_SET;

	if ((err = t3_sge_alloc(sc)) != 0) {
		device_printf(sc->dev, "t3_sge_alloc returned %d\n", err);
		return (err);
	}

	if (sc->params.rev > 0 && !(sc->flags & USING_MSI))
		irq_idx = -1;
	else
		irq_idx = 0;

	for (qset_idx = 0, i = 0; i < (sc)->params.nports; ++i) {
		struct port_info *pi = &sc->port[i];

		for (j = 0; j < pi->nqsets; ++j, ++qset_idx) {
			err = t3_sge_alloc_qset(sc, qset_idx, (sc)->params.nports,
			    (sc->flags & USING_MSIX) ? qset_idx + 1 : irq_idx,
			    &sc->params.sge.qset[qset_idx], ntxq, pi);
			if (err) {
				t3_free_sge_resources(sc);
				device_printf(sc->dev, "t3_sge_alloc_qset failed with %d\n", err);
				return (err);
			}
		}
	}

	return (0);
}

static int
cxgb_setup_msix(adapter_t *sc, int msix_count)
{
	int i, j, k, nqsets, rid;

	/* The first message indicates link changes and error conditions */
	sc->irq_rid = 1;
	if ((sc->irq_res = bus_alloc_resource_any(sc->dev, SYS_RES_IRQ,
	   &sc->irq_rid, RF_SHAREABLE | RF_ACTIVE)) == NULL) {
		device_printf(sc->dev, "Cannot allocate msix interrupt\n");
		return (EINVAL);
	}

	if (bus_setup_intr(sc->dev, sc->irq_res, INTR_MPSAFE|INTR_TYPE_NET,
#ifdef INTR_FILTERS
			NULL,
#endif
		cxgb_async_intr, sc, &sc->intr_tag)) {
		device_printf(sc->dev, "Cannot set up interrupt\n");
		return (EINVAL);
	}
	for (i = 0, k = 0; i < (sc)->params.nports; ++i) {
		nqsets = sc->port[i].nqsets;
		for (j = 0; j < nqsets; ++j, k++) {
			struct sge_qset *qs = &sc->sge.qs[k];
			    
			rid = k + 2;
			if (cxgb_debug)
				printf("rid=%d ", rid);
			if ((sc->msix_irq_res[k] = bus_alloc_resource_any(
			    sc->dev, SYS_RES_IRQ, &rid,
			    RF_SHAREABLE | RF_ACTIVE)) == NULL) {
				device_printf(sc->dev, "Cannot allocate "
				    "interrupt for message %d\n", rid);
				return (EINVAL);
			}
			sc->msix_irq_rid[k] = rid;
			if (bus_setup_intr(sc->dev, sc->msix_irq_res[j],
			    INTR_MPSAFE|INTR_TYPE_NET,
#ifdef INTR_FILTERS
			NULL,
#endif
				t3_intr_msix, qs, &sc->msix_intr_tag[k])) {
				device_printf(sc->dev, "Cannot set up "
				    "interrupt for message %d\n", rid);
				return (EINVAL);
			}
		}
	}


	return (0);
}

static int
cxgb_port_probe(device_t dev)
{
	struct port_info *p;
	char buf[80];

	p = device_get_softc(dev);

	snprintf(buf, sizeof(buf), "Port %d %s", p->port, p->port_type->desc);
	device_set_desc_copy(dev, buf);
	return (0);
}


static int
cxgb_makedev(struct port_info *pi)
{
	struct cdevsw *cxgb_cdevsw;

	if ((cxgb_cdevsw = malloc(sizeof(struct cdevsw), M_DEVBUF, M_NOWAIT|M_ZERO)) == NULL)
		return (ENOMEM);
	
	cxgb_cdevsw->d_version = D_VERSION;
	cxgb_cdevsw->d_name = strdup(pi->ifp->if_xname, M_DEVBUF);
	cxgb_cdevsw->d_ioctl = cxgb_extension_ioctl;	
	
	pi->port_cdev = make_dev(cxgb_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600,
	    pi->ifp->if_xname);
	
	if (pi->port_cdev == NULL)
		return (ENOMEM);

	pi->port_cdev->si_drv1 = (void *)pi;
	
	return (0);
}


#ifdef TSO_SUPPORTED
#define CXGB_CAP (IFCAP_VLAN_HWTAGGING | IFCAP_VLAN_MTU | IFCAP_HWCSUM | IFCAP_VLAN_HWCSUM | IFCAP_TSO | IFCAP_JUMBO_MTU)
/* Don't enable TSO6 yet */
#define CXGB_CAP_ENABLE (IFCAP_VLAN_HWTAGGING | IFCAP_VLAN_MTU | IFCAP_HWCSUM | IFCAP_VLAN_HWCSUM | IFCAP_TSO4 | IFCAP_JUMBO_MTU)
#else
#define CXGB_CAP (IFCAP_VLAN_HWTAGGING | IFCAP_VLAN_MTU | IFCAP_HWCSUM | IFCAP_JUMBO_MTU)
/* Don't enable TSO6 yet */
#define CXGB_CAP_ENABLE (IFCAP_VLAN_HWTAGGING | IFCAP_VLAN_MTU | IFCAP_HWCSUM |  IFCAP_JUMBO_MTU)
#define IFCAP_TSO4 0x0
#define CSUM_TSO   0x0
#endif


static int
cxgb_port_attach(device_t dev)
{
	struct port_info *p;
	struct ifnet *ifp;
	int media_flags;
	int err;
	char buf[64];
	
	p = device_get_softc(dev);

	snprintf(buf, sizeof(buf), "cxgb port %d", p->port);  	
	mtx_init(&p->lock, buf, 0, MTX_DEF);

	/* Allocate an ifnet object and set it up */
	ifp = p->ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(dev, "Cannot allocate ifnet\n");
		return (ENOMEM);
	}
	
	/*
	 * Note that there is currently no watchdog timer.
	 */
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_init = cxgb_init;
	ifp->if_softc = p;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = cxgb_ioctl;
	ifp->if_start = cxgb_start;
	ifp->if_timer = 0;	/* Disable ifnet watchdog */
	ifp->if_watchdog = NULL;

	ifp->if_snd.ifq_drv_maxlen = TX_ETH_Q_SIZE;
	IFQ_SET_MAXLEN(&ifp->if_snd, ifp->if_snd.ifq_drv_maxlen);
	IFQ_SET_READY(&ifp->if_snd);

	ifp->if_hwassist = ifp->if_capabilities = ifp->if_capenable = 0;
	ifp->if_capabilities |= CXGB_CAP;
	ifp->if_capenable |= CXGB_CAP_ENABLE;
	ifp->if_hwassist |= (CSUM_TCP | CSUM_UDP | CSUM_IP | CSUM_TSO);
	ifp->if_baudrate = 100000000;
	
	ether_ifattach(ifp, p->hw_addr);
#ifdef DEFAULT_JUMBO
	ifp->if_mtu = 9000;
#endif	
	if ((err = cxgb_makedev(p)) != 0) {
		printf("makedev failed %d\n", err);
		return (err);
	}
	ifmedia_init(&p->media, IFM_IMASK, cxgb_media_change,
	    cxgb_media_status);

	if (!strcmp(p->port_type->desc, "10GBASE-CX4"))
	        media_flags = IFM_ETHER | IFM_10G_CX4;
	else if (!strcmp(p->port_type->desc, "10GBASE-SR"))
	        media_flags = IFM_ETHER | IFM_10G_SR;
	else if (!strcmp(p->port_type->desc, "10GBASE-XR"))
	        media_flags = IFM_ETHER | IFM_10G_LR;
	else {
	        printf("unsupported media type %s\n", p->port_type->desc);
		return (ENXIO);
	}
		
	ifmedia_add(&p->media, media_flags, 0, NULL);
	ifmedia_add(&p->media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&p->media, media_flags);

	snprintf(buf, sizeof(buf), "cxgb_port_taskq%d", p->port);
#ifdef TASKQUEUE_CURRENT
	/* Create a port for handling TX without starvation */
	p->tq = taskqueue_create(buf, M_NOWAIT,
	    taskqueue_thread_enqueue, &p->tq);
#else
	/* Create a port for handling TX without starvation */
	p->tq = taskqueue_create_fast(buf, M_NOWAIT,
	    taskqueue_thread_enqueue, &p->tq);
#endif	
	
	
	if (p->tq == NULL) {
		device_printf(dev, "failed to allocate port task queue\n");
		return (ENOMEM);
	}	
	taskqueue_start_threads(&p->tq, 1, PI_NET, "%s taskq",
	    device_get_nameunit(dev));
	TASK_INIT(&p->start_task, 0, cxgb_start_proc, ifp);

	
	return (0);
}

static int
cxgb_port_detach(device_t dev)
{
	struct port_info *p;

	p = device_get_softc(dev);

	PORT_LOCK(p);
	cxgb_stop_locked(p);
	PORT_UNLOCK(p);
	
	mtx_destroy(&p->lock);
	if (p->tq != NULL) {
		taskqueue_drain(p->tq, &p->start_task);
		taskqueue_free(p->tq);
		p->tq = NULL;
	}
	
	ether_ifdetach(p->ifp);
	if_free(p->ifp);
	
	destroy_dev(p->port_cdev);


	return (0);
}

void
t3_fatal_err(struct adapter *sc)
{
	u_int fw_status[4];

	device_printf(sc->dev,"encountered fatal error, operation suspended\n");
	if (!t3_cim_ctl_blk_read(sc, 0xa0, 4, fw_status))
		device_printf(sc->dev, "FW_ status: 0x%x, 0x%x, 0x%x, 0x%x\n",
		    fw_status[0], fw_status[1], fw_status[2], fw_status[3]);
}

int
t3_os_find_pci_capability(adapter_t *sc, int cap)
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
t3_os_pci_save_state(struct adapter *sc)
{
	device_t dev;
	struct pci_devinfo *dinfo;

	dev = sc->dev;
	dinfo = device_get_ivars(dev);

	pci_cfg_save(dev, dinfo, 0);
	return (0);
}

int
t3_os_pci_restore_state(struct adapter *sc)
{
	device_t dev;
	struct pci_devinfo *dinfo;

	dev = sc->dev;
	dinfo = device_get_ivars(dev);

	pci_cfg_restore(dev, dinfo);
	return (0);
}

/**
 *	t3_os_link_changed - handle link status changes
 *	@adapter: the adapter associated with the link change
 *	@port_id: the port index whose limk status has changed
 *	@link_stat: the new status of the link
 *	@speed: the new speed setting
 *	@duplex: the new duplex setting
 *	@fc: the new flow-control setting
 *
 *	This is the OS-dependent handler for link status changes.  The OS
 *	neutral handler takes care of most of the processing for these events,
 *	then calls this handler for any OS-specific processing.
 */
void
t3_os_link_changed(adapter_t *adapter, int port_id, int link_status, int speed,
     int duplex, int fc)
{
	struct port_info *pi = &adapter->port[port_id];
	struct cmac *mac = &adapter->port[port_id].mac;

	if ((pi->ifp->if_flags & IFF_UP) == 0)
		return;

	if (link_status) {
		t3_mac_enable(mac, MAC_DIRECTION_RX);
		if_link_state_change(pi->ifp, LINK_STATE_UP);
	} else {
		if_link_state_change(pi->ifp, LINK_STATE_DOWN);
		pi->phy.ops->power_down(&pi->phy, 1);
		t3_mac_disable(mac, MAC_DIRECTION_RX);
		t3_link_start(&pi->phy, mac, &pi->link_config);
	}
}


/*
 * Interrupt-context handler for external (PHY) interrupts.
 */
void
t3_os_ext_intr_handler(adapter_t *sc)
{
	if (cxgb_debug)
		printf("t3_os_ext_intr_handler\n");
	/*
	 * Schedule a task to handle external interrupts as they may be slow
	 * and we use a mutex to protect MDIO registers.  We disable PHY
	 * interrupts in the meantime and let the task reenable them when
	 * it's done.
	 */
	ADAPTER_LOCK(sc);
	if (sc->slow_intr_mask) {
		sc->slow_intr_mask &= ~F_T3DBG;
		t3_write_reg(sc, A_PL_INT_ENABLE0, sc->slow_intr_mask);
		taskqueue_enqueue(sc->tq, &sc->ext_intr_task);
	}
	ADAPTER_UNLOCK(sc);
}

void
t3_os_set_hw_addr(adapter_t *adapter, int port_idx, u8 hw_addr[])
{

	/*
	 * The ifnet might not be allocated before this gets called,
	 * as this is called early on in attach by t3_prep_adapter
	 * save the address off in the port structure
	 */
	if (cxgb_debug)
		printf("set_hw_addr on idx %d addr %6D\n", port_idx, hw_addr, ":");
	bcopy(hw_addr, adapter->port[port_idx].hw_addr, ETHER_ADDR_LEN);
}

/**
 *	link_start - enable a port
 *	@p: the port to enable
 *
 *	Performs the MAC and PHY actions needed to enable a port.
 */
static void
cxgb_link_start(struct port_info *p)
{
	struct ifnet *ifp;
	struct t3_rx_mode rm;
	struct cmac *mac = &p->mac;

	ifp = p->ifp;

	t3_init_rx_mode(&rm, p);
	t3_mac_reset(mac);
	t3_mac_set_mtu(mac, ifp->if_mtu + ETHER_HDR_LEN);
	t3_mac_set_address(mac, 0, p->hw_addr);
	t3_mac_set_rx_mode(mac, &rm);
	t3_link_start(&p->phy, mac, &p->link_config);
	t3_mac_enable(mac, MAC_DIRECTION_RX | MAC_DIRECTION_TX);
}

/**
 *	setup_rss - configure Receive Side Steering (per-queue connection demux) 
 *	@adap: the adapter
 *
 *	Sets up RSS to distribute packets to multiple receive queues.  We
 *	configure the RSS CPU lookup table to distribute to the number of HW
 *	receive queues, and the response queue lookup table to narrow that
 *	down to the response queues actually configured for each port.
 *	We always configure the RSS mapping for two ports since the mapping
 *	table has plenty of entries.
 */
static void
setup_rss(adapter_t *adap)
{
	int i;
	u_int nq0 = adap->port[0].nqsets;
	u_int nq1 = max((u_int)adap->port[1].nqsets, 1U);
	uint8_t cpus[SGE_QSETS + 1];
	uint16_t rspq_map[RSS_TABLE_SIZE];

	for (i = 0; i < SGE_QSETS; ++i)
		cpus[i] = i;
	cpus[SGE_QSETS] = 0xff;

	for (i = 0; i < RSS_TABLE_SIZE / 2; ++i) {
		rspq_map[i] = i % nq0;
		rspq_map[i + RSS_TABLE_SIZE / 2] = (i % nq1) + nq0;
	}

	t3_config_rss(adap, F_RQFEEDBACKENABLE | F_TNLLKPEN | F_TNLMAPEN |
	    F_TNLPRTEN | F_TNL2TUPEN | F_TNL4TUPEN |
	    V_RRCPLCPUSIZE(6), cpus, rspq_map);
}

/*
 * Sends an mbuf to an offload queue driver
 * after dealing with any active network taps.
 */
static inline int
offload_tx(struct toedev *tdev, struct mbuf *m)
{
	int ret;

	critical_enter();
	ret = t3_offload_tx(tdev, m);
	critical_exit();
	return ret;
}

static int
write_smt_entry(struct adapter *adapter, int idx)
{
	struct port_info *pi = &adapter->port[idx];
	struct cpl_smt_write_req *req;
	struct mbuf *m;

	if ((m = m_gethdr(M_NOWAIT, MT_DATA)) == NULL)
		return (ENOMEM);

	req = mtod(m, struct cpl_smt_write_req *);
	req->wr.wr_hi = htonl(V_WR_OP(FW_WROPCODE_FORWARD));
	OPCODE_TID(req) = htonl(MK_OPCODE_TID(CPL_SMT_WRITE_REQ, idx));
	req->mtu_idx = NMTUS - 1;  /* should be 0 but there's a T3 bug */
	req->iff = idx;
	memset(req->src_mac1, 0, sizeof(req->src_mac1));
	memcpy(req->src_mac0, pi->hw_addr, ETHER_ADDR_LEN);

	m_set_priority(m, 1);

	offload_tx(&adapter->tdev, m);

	return (0);
}

static int
init_smt(struct adapter *adapter)
{
	int i;

	for_each_port(adapter, i)
		write_smt_entry(adapter, i);
	return 0;
}

static void
init_port_mtus(adapter_t *adapter)
{
	unsigned int mtus = adapter->port[0].ifp->if_mtu;

	if (adapter->port[1].ifp)
		mtus |= adapter->port[1].ifp->if_mtu << 16;
	t3_write_reg(adapter, A_TP_MTU_PORT_TABLE, mtus);
}

static void
send_pktsched_cmd(struct adapter *adap, int sched, int qidx, int lo,
			      int hi, int port)
{
	struct mbuf *m;
	struct mngt_pktsched_wr *req;

	m = m_gethdr(M_NOWAIT, MT_DATA);
	if (m) {	
		req = mtod(m, struct mngt_pktsched_wr *);
		req->wr_hi = htonl(V_WR_OP(FW_WROPCODE_MNGT));
		req->mngt_opcode = FW_MNGTOPCODE_PKTSCHED_SET;
		req->sched = sched;
		req->idx = qidx;
		req->min = lo;
		req->max = hi;
		req->binding = port;
		m->m_len = m->m_pkthdr.len = sizeof(*req);
		t3_mgmt_tx(adap, m);
	}
}

static void
bind_qsets(adapter_t *sc)
{
	int i, j;

	if (singleq)
		return;
	
	for (i = 0; i < (sc)->params.nports; ++i) {
		const struct port_info *pi = adap2pinfo(sc, i);

		for (j = 0; j < pi->nqsets; ++j)
			send_pktsched_cmd(sc, 1, pi->first_qset + j, -1,
					  -1, i);
	}
}

/**
 *	cxgb_up - enable the adapter
 *	@adap: adapter being enabled
 *
 *	Called when the first port is enabled, this function performs the
 *	actions necessary to make an adapter operational, such as completing
 *	the initialization of HW modules, and enabling interrupts.
 *
 */
static int
cxgb_up(struct adapter *sc)
{
	int err = 0;

	if ((sc->flags & FULL_INIT_DONE) == 0) {

		if ((sc->flags & FW_UPTODATE) == 0)
			err = upgrade_fw(sc);

		if (err)
			goto out;

		err = t3_init_hw(sc, 0);
		if (err)
			goto out;

		t3_write_reg(sc, A_ULPRX_TDDP_PSZ, V_HPZ0(PAGE_SHIFT - 12));

		err = setup_sge_qsets(sc);
		if (err)
			goto out;

		setup_rss(sc);
		sc->flags |= FULL_INIT_DONE;
	}

	t3_intr_clear(sc);

	/* If it's MSI or INTx, allocate a single interrupt for everything */
	if ((sc->flags & USING_MSIX) == 0) {
		if ((sc->irq_res = bus_alloc_resource_any(sc->dev, SYS_RES_IRQ,
		   &sc->irq_rid, RF_SHAREABLE | RF_ACTIVE)) == NULL) {
			device_printf(sc->dev, "Cannot allocate interrupt rid=%d\n", sc->irq_rid);
			err = EINVAL;
			goto out;
		}
		device_printf(sc->dev, "allocated irq_res=%p\n", sc->irq_res);

		if (bus_setup_intr(sc->dev, sc->irq_res, INTR_MPSAFE|INTR_TYPE_NET,
#ifdef INTR_FILTERS
			NULL,
#endif			
			sc->cxgb_intr, sc, &sc->intr_tag)) {
			device_printf(sc->dev, "Cannot set up interrupt\n");
			err = EINVAL;
			goto irq_err;
		}
	} else {
		cxgb_setup_msix(sc, sc->msi_count);
	}

	t3_sge_start(sc);
	t3_intr_enable(sc);

	if ((sc->flags & (USING_MSIX | QUEUES_BOUND)) == USING_MSIX)
		bind_qsets(sc);
	sc->flags |= QUEUES_BOUND;
out:
	return (err);
irq_err:
	CH_ERR(sc, "request_irq failed, err %d\n", err);
	goto out;
}


/*
 * Release resources when all the ports and offloading have been stopped.
 */
static void
cxgb_down(struct adapter *sc)
{
	int i;

	t3_sge_stop(sc);
	t3_intr_disable(sc);

	for (i = 0; i < SGE_QSETS; i++) {
		if (sc->msix_intr_tag[i] != NULL) {
			bus_teardown_intr(sc->dev, sc->msix_irq_res[i],
			    sc->msix_intr_tag[i]);
			sc->msix_intr_tag[i] = NULL;
		}
		if (sc->msix_irq_res[i] != NULL) {
			bus_release_resource(sc->dev, SYS_RES_IRQ,
			    sc->msix_irq_rid[i], sc->msix_irq_res[i]);
			sc->msix_irq_res[i] = NULL;
		}
	}

	if (sc->intr_tag != NULL) {
		bus_teardown_intr(sc->dev, sc->irq_res, sc->intr_tag);
		sc->intr_tag = NULL;
	}
	if (sc->irq_res != NULL) {
		device_printf(sc->dev, "de-allocating interrupt irq_rid=%d irq_res=%p\n",
		    sc->irq_rid, sc->irq_res);
		bus_release_resource(sc->dev, SYS_RES_IRQ, sc->irq_rid,
		    sc->irq_res);
		sc->irq_res = NULL;
	}

	callout_drain(&sc->sge_timer_ch);
	taskqueue_drain(sc->tq, &sc->slow_intr_task);
	taskqueue_drain(sc->tq, &sc->timer_reclaim_task);
}

static int
offload_open(struct port_info *pi)
{
	struct adapter *adapter = pi->adapter;
	struct toedev *tdev = TOEDEV(pi->ifp);
	int adap_up = adapter->open_device_map & PORT_MASK;
	int err = 0;

	if (atomic_cmpset_int(&adapter->open_device_map,
		(adapter->open_device_map & ~OFFLOAD_DEVMAP_BIT),
		(adapter->open_device_map | OFFLOAD_DEVMAP_BIT)) == 0)
		return (0);

	ADAPTER_LOCK(pi->adapter); 
	if (!adap_up)
		err = cxgb_up(adapter);
	ADAPTER_UNLOCK(pi->adapter);
	if (err < 0)
		return (err);

	t3_tp_set_offload_mode(adapter, 1);
	tdev->lldev = adapter->port[0].ifp;
	err = cxgb_offload_activate(adapter);
	if (err)
		goto out;

	init_port_mtus(adapter);
	t3_load_mtus(adapter, adapter->params.mtus, adapter->params.a_wnd,
		     adapter->params.b_wnd,
		     adapter->params.rev == 0 ?
		       adapter->port[0].ifp->if_mtu : 0xffff);
	init_smt(adapter);

	/* Call back all registered clients */
	cxgb_add_clients(tdev);

out:
	/* restore them in case the offload module has changed them */
	if (err) {
		t3_tp_set_offload_mode(adapter, 0);
		clrbit(&adapter->open_device_map, OFFLOAD_DEVMAP_BIT);
		cxgb_set_dummy_ops(tdev);
	}
	return (err);
}

static int
offload_close(struct toedev *tdev)
{
	struct adapter *adapter = tdev2adap(tdev);

	if (!isset(&adapter->open_device_map, OFFLOAD_DEVMAP_BIT))
		return 0;

	/* Call back all registered clients */
	cxgb_remove_clients(tdev);
	tdev->lldev = NULL;
	cxgb_set_dummy_ops(tdev);
	t3_tp_set_offload_mode(adapter, 0);
	clrbit(&adapter->open_device_map, OFFLOAD_DEVMAP_BIT);

	ADAPTER_LOCK(adapter);
	if (!adapter->open_device_map)
		cxgb_down(adapter);
	ADAPTER_UNLOCK(adapter);

	cxgb_offload_deactivate(adapter);
	return 0;
}

static void
cxgb_init(void *arg)
{
	struct port_info *p = arg;

	PORT_LOCK(p);
	cxgb_init_locked(p);
	PORT_UNLOCK(p);
}

static void
cxgb_init_locked(struct port_info *p)
{
	struct ifnet *ifp;
	adapter_t *sc = p->adapter;
	int err;

	mtx_assert(&p->lock, MA_OWNED);
	ifp = p->ifp;

	ADAPTER_LOCK(p->adapter);
	if ((sc->open_device_map == 0) && ((err = cxgb_up(sc)) < 0)) {
		ADAPTER_UNLOCK(p->adapter);
		cxgb_stop_locked(p);
		return;
	}
	if (p->adapter->open_device_map == 0)
		t3_intr_clear(sc);

	setbit(&p->adapter->open_device_map, p->port);

	ADAPTER_UNLOCK(p->adapter);
	if (is_offload(sc) && !ofld_disable) {
		err = offload_open(p);
		if (err)
			log(LOG_WARNING,
			    "Could not initialize offload capabilities\n");
	}
	cxgb_link_start(p);
	t3_port_intr_enable(sc, p->port);

	callout_reset(&sc->cxgb_tick_ch, sc->params.stats_update_period * hz,
	    cxgb_tick, sc);
	
	PORT_LOCK(p);
	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	PORT_UNLOCK(p);
}

static void
cxgb_set_rxmode(struct port_info *p)
{
	struct t3_rx_mode rm;
	struct cmac *mac = &p->mac;

	mtx_assert(&p->lock, MA_OWNED);
	
	t3_init_rx_mode(&rm, p);
	t3_mac_set_rx_mode(mac, &rm);
}

static void
cxgb_stop_locked(struct port_info *p)
{
	struct ifnet *ifp;

	mtx_assert(&p->lock, MA_OWNED);
	mtx_assert(&p->adapter->lock, MA_NOTOWNED);
		
	ifp = p->ifp;

	t3_port_intr_disable(p->adapter, p->port);
	PORT_LOCK(p);
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
	PORT_UNLOCK(p);
	p->phy.ops->power_down(&p->phy, 1);
	t3_mac_disable(&p->mac, MAC_DIRECTION_TX | MAC_DIRECTION_RX);

	ADAPTER_LOCK(p->adapter);
	clrbit(&p->adapter->open_device_map, p->port);
	/*
	 * XXX cancel check_task
	 */
	if (p->adapter->open_device_map == 0)
		cxgb_down(p->adapter);
	ADAPTER_UNLOCK(p->adapter);
}

static int
cxgb_ioctl(struct ifnet *ifp, unsigned long command, caddr_t data)
{
	struct port_info *p = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	int flags, error = 0;
	uint32_t mask;

	/* 
	 * XXX need to check that we aren't in the middle of an unload
	 */
	switch (command) {
	case SIOCSIFMTU:
		if ((ifr->ifr_mtu < ETHERMIN) ||
		    (ifr->ifr_mtu > ETHER_MAX_LEN_JUMBO))
			error = EINVAL;
		else if (ifp->if_mtu != ifr->ifr_mtu) {
			PORT_LOCK(p);
			ifp->if_mtu = ifr->ifr_mtu;
			t3_mac_set_mtu(&p->mac, ifp->if_mtu + ETHER_HDR_LEN);
			PORT_UNLOCK(p);
		}
		break;
	case SIOCSIFADDR:
	case SIOCGIFADDR:
		if (ifa->ifa_addr->sa_family == AF_INET) {
			ifp->if_flags |= IFF_UP;
			if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
				cxgb_init(p);
			}
			arp_ifinit(ifp, ifa);
		} else
			error = ether_ioctl(ifp, command, data);
		break;
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			PORT_LOCK(p);			
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				flags = p->if_flags;
				if (((ifp->if_flags ^ flags) & IFF_PROMISC) ||
				    ((ifp->if_flags ^ flags) & IFF_ALLMULTI))
					cxgb_set_rxmode(p);
			
			} else
				cxgb_init_locked(p);
			p->if_flags = ifp->if_flags;
			PORT_UNLOCK(p);
		} else {
			callout_drain(&p->adapter->cxgb_tick_ch);
			PORT_LOCK(p);
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				cxgb_stop_locked(p);
			} else {
				adapter_t *sc = p->adapter;
				callout_reset(&sc->cxgb_tick_ch,
				    sc->params.stats_update_period * hz,
				    cxgb_tick, sc);
			}
			PORT_UNLOCK(p);
		}


		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &p->media, command);
		break;
	case SIOCSIFCAP:
		PORT_LOCK(p);
		mask = ifr->ifr_reqcap ^ ifp->if_capenable;
		if (mask & IFCAP_TXCSUM) {
			if (IFCAP_TXCSUM & ifp->if_capenable) {
				ifp->if_capenable &= ~(IFCAP_TXCSUM|IFCAP_TSO4);
				ifp->if_hwassist &= ~(CSUM_TCP | CSUM_UDP
				    | CSUM_TSO);
			} else {
				ifp->if_capenable |= IFCAP_TXCSUM;
				ifp->if_hwassist |= (CSUM_TCP | CSUM_UDP);
			}
		} else if (mask & IFCAP_RXCSUM) {
			if (IFCAP_RXCSUM & ifp->if_capenable) {
				ifp->if_capenable &= ~IFCAP_RXCSUM;
			} else {
				ifp->if_capenable |= IFCAP_RXCSUM;
			}
		}
		if (mask & IFCAP_TSO4) {
			if (IFCAP_TSO4 & ifp->if_capenable) {
				ifp->if_capenable &= ~IFCAP_TSO4;
				ifp->if_hwassist &= ~CSUM_TSO;
			} else if (IFCAP_TXCSUM & ifp->if_capenable) {
				ifp->if_capenable |= IFCAP_TSO4;
				ifp->if_hwassist |= CSUM_TSO;
			} else {
				if (cxgb_debug)
					printf("cxgb requires tx checksum offload"
					    " be enabled to use TSO\n");
				error = EINVAL;
			}
		}
		PORT_UNLOCK(p);
		break;
	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}

	return (error);
}

static int
cxgb_start_tx(struct ifnet *ifp, uint32_t txmax)
{
	struct sge_qset *qs;
	struct sge_txq *txq;
	struct port_info *p = ifp->if_softc;
	struct mbuf *m0, *m = NULL;
	int err, in_use_init;
	
	if (!p->link_config.link_ok)
		return (ENXIO);

	if (IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		return (ENOBUFS);

	qs = &p->adapter->sge.qs[p->first_qset];
	txq = &qs->txq[TXQ_ETH];
	err = 0;

	mtx_lock(&txq->lock);
	in_use_init = txq->in_use;
	while ((txq->in_use - in_use_init < txmax) &&
	    (txq->size > txq->in_use + TX_MAX_DESC)) {
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;
		/*
		 * Convert chain to M_IOVEC
		 */
		KASSERT((m->m_flags & M_IOVEC) == 0, ("IOVEC set too early"));
		m0 = m;
#ifdef INVARIANTS
		/*
		 * Clean up after net stack sloppiness
		 * before calling m_sanity
		 */
		m0 = m->m_next;
		while (m0) {
			m0->m_flags &= ~M_PKTHDR;
			m0 = m0->m_next;
		}
		m_sanity(m0, 0);
		m0 = m;
#endif
		if (collapse_mbufs && m->m_pkthdr.len > MCLBYTES &&
		    m_collapse(m, TX_MAX_SEGS, &m0) == EFBIG) {
			if ((m0 = m_defrag(m, M_NOWAIT)) != NULL) {
				m = m0;
				m_collapse(m, TX_MAX_SEGS, &m0);
			} else
				break;	
		}
		m = m0;
		if ((err = t3_encap(p, &m)) != 0)
			break;
		BPF_MTAP(ifp, m);
	}
	mtx_unlock(&txq->lock);

	if (__predict_false(err)) {
		if (err == ENOMEM) {
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			IFQ_LOCK(&ifp->if_snd);
			IFQ_DRV_PREPEND(&ifp->if_snd, m);
			IFQ_UNLOCK(&ifp->if_snd);
		}
	}
	if (err == 0 && m == NULL) {
		return (ENOBUFS);
	}
	if ((err == 0) &&  (txq->size <= txq->in_use + TX_MAX_DESC) &&
	    (ifp->if_drv_flags & IFF_DRV_OACTIVE) == 0) {
		ifp->if_drv_flags |= IFF_DRV_OACTIVE;
		return (ENOSPC);
	}
	return (err);
}

static void
cxgb_start_proc(void *arg, int ncount)
{
	struct ifnet *ifp = arg;
	struct port_info *pi = ifp->if_softc;	
	struct sge_qset *qs;
	struct sge_txq *txq;
	int error = 0;

	qs = &pi->adapter->sge.qs[pi->first_qset];
	txq = &qs->txq[TXQ_ETH];

	while (error == 0) {
		if (desc_reclaimable(txq) > TX_CLEAN_MAX_DESC)
			taskqueue_enqueue(pi->adapter->tq,
			    &pi->adapter->timer_reclaim_task);

		error = cxgb_start_tx(ifp, TX_START_MAX_DESC);
	}
}

static void
cxgb_start(struct ifnet *ifp)
{
	struct port_info *pi = ifp->if_softc;	
	struct sge_qset *qs;
	struct sge_txq *txq;
	int err;

	qs = &pi->adapter->sge.qs[pi->first_qset];
	txq = &qs->txq[TXQ_ETH];
	
	if (desc_reclaimable(txq) > TX_CLEAN_MAX_DESC)
		taskqueue_enqueue(pi->adapter->tq,
		    &pi->adapter->timer_reclaim_task);
	
	err = cxgb_start_tx(ifp, TX_START_MAX_DESC);
	
	if (err == 0)
		taskqueue_enqueue(pi->tq, &pi->start_task);
}


static int
cxgb_media_change(struct ifnet *ifp)
{
	if_printf(ifp, "media change not supported\n");
	return (ENXIO);
}

static void
cxgb_media_status(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct port_info *p = ifp->if_softc;

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (!p->link_config.link_ok)
		return;

	ifmr->ifm_status |= IFM_ACTIVE;

	if (p->link_config.duplex)
		ifmr->ifm_active |= IFM_FDX;
	else
		ifmr->ifm_active |= IFM_HDX;
}

static void
cxgb_async_intr(void *data)
{
	adapter_t *sc = data;

	if (cxgb_debug)
		device_printf(sc->dev, "cxgb_async_intr\n");

	t3_slow_intr_handler(sc);

}

static void
cxgb_ext_intr_handler(void *arg, int count)
{
	adapter_t *sc = (adapter_t *)arg;

	if (cxgb_debug)
		printf("cxgb_ext_intr_handler\n");

	t3_phy_intr_handler(sc);

	/* Now reenable external interrupts */
	ADAPTER_LOCK(sc);
	if (sc->slow_intr_mask) {
		sc->slow_intr_mask |= F_T3DBG;
		t3_write_reg(sc, A_PL_INT_CAUSE0, F_T3DBG);
		t3_write_reg(sc, A_PL_INT_ENABLE0, sc->slow_intr_mask);
	}
	ADAPTER_UNLOCK(sc);
}

static void
check_link_status(adapter_t *sc)
{
	int i;

	for (i = 0; i < (sc)->params.nports; ++i) {
		struct port_info *p = &sc->port[i];

		if (!(p->port_type->caps & SUPPORTED_IRQ))
			t3_link_changed(sc, i);
	}
}

static void
check_t3b2_mac(struct adapter *adapter)
{
	int i;

	for_each_port(adapter, i) {
		struct port_info *p = &adapter->port[i];
		struct ifnet *ifp = p->ifp;
		int status;

		if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) 
			continue;
		
		status = 0;
		PORT_LOCK(p);
		if ((ifp->if_drv_flags & IFF_DRV_RUNNING)) 
			status = t3b2_mac_watchdog_task(&p->mac);
		if (status == 1)
			p->mac.stats.num_toggled++;
		else if (status == 2) {
			struct cmac *mac = &p->mac;

			t3_mac_set_mtu(mac, ifp->if_mtu + ETHER_HDR_LEN);
			t3_mac_set_address(mac, 0, p->hw_addr);
			cxgb_set_rxmode(p);
			t3_link_start(&p->phy, mac, &p->link_config);
			t3_mac_enable(mac, MAC_DIRECTION_RX | MAC_DIRECTION_TX);
			t3_port_intr_enable(adapter, p->port);
			p->mac.stats.num_resets++;
		}
		PORT_UNLOCK(p);
	}
}

static void
cxgb_tick(void *arg)
{
	adapter_t *sc = (adapter_t *)arg;
	const struct adapter_params *p = &sc->params;

	if (p->linkpoll_period)
		check_link_status(sc);
	callout_reset(&sc->cxgb_tick_ch, sc->params.stats_update_period * hz,
	    cxgb_tick, sc);

	/*
	 * adapter lock can currently only be acquire after the
	 * port lock
	 */
	ADAPTER_UNLOCK(sc);
	if (p->rev == T3_REV_B2)
		check_t3b2_mac(sc);

}

static int
in_range(int val, int lo, int hi)
{
	return val < 0 || (val <= hi && val >= lo);
}

static int
cxgb_extension_ioctl(struct cdev *dev, unsigned long cmd, caddr_t data,
    int fflag, struct thread *td)
{
	int mmd, error = 0;
	struct port_info *pi = dev->si_drv1;
	adapter_t *sc = pi->adapter;

#ifdef PRIV_SUPPORTED	
	if (priv_check(td, PRIV_DRIVER)) {
		if (cxgb_debug) 
			printf("user does not have access to privileged ioctls\n");
		return (EPERM);
	}
#else
	if (suser(td)) {
		if (cxgb_debug)
			printf("user does not have access to privileged ioctls\n");
		return (EPERM);
	}
#endif
	
	switch (cmd) {
	case SIOCGMIIREG: {
		uint32_t val;
		struct cphy *phy = &pi->phy;
		struct mii_data *mid = (struct mii_data *)data;
		
		if (!phy->mdio_read)
			return (EOPNOTSUPP);
		if (is_10G(sc)) {
			mmd = mid->phy_id >> 8;
			if (!mmd)
				mmd = MDIO_DEV_PCS;
			else if (mmd > MDIO_DEV_XGXS)
				return -EINVAL;

			error = phy->mdio_read(sc, mid->phy_id & 0x1f, mmd,
					     mid->reg_num, &val);
		} else
		        error = phy->mdio_read(sc, mid->phy_id & 0x1f, 0,
					     mid->reg_num & 0x1f, &val);
		if (error == 0)
			mid->val_out = val;
		break;
	}
	case SIOCSMIIREG: {
		struct cphy *phy = &pi->phy;
		struct mii_data *mid = (struct mii_data *)data;

		if (!phy->mdio_write)
			return (EOPNOTSUPP);
		if (is_10G(sc)) {
			mmd = mid->phy_id >> 8;
			if (!mmd)
				mmd = MDIO_DEV_PCS;
			else if (mmd > MDIO_DEV_XGXS)
				return (EINVAL);
			
			error = phy->mdio_write(sc, mid->phy_id & 0x1f,
					      mmd, mid->reg_num, mid->val_in);
		} else
			error = phy->mdio_write(sc, mid->phy_id & 0x1f, 0,
					      mid->reg_num & 0x1f,
					      mid->val_in);
		break;
	}
	case CHELSIO_SETREG: {
		struct ch_reg *edata = (struct ch_reg *)data;
		if ((edata->addr & 0x3) != 0 || edata->addr >= sc->mmio_len)
			return (EFAULT);
		t3_write_reg(sc, edata->addr, edata->val);
		break;
	}
	case CHELSIO_GETREG: {
		struct ch_reg *edata = (struct ch_reg *)data;
		if ((edata->addr & 0x3) != 0 || edata->addr >= sc->mmio_len)
			return (EFAULT);
		edata->val = t3_read_reg(sc, edata->addr);
		break;
	}
	case CHELSIO_GET_SGE_CONTEXT: {
		struct ch_cntxt *ecntxt = (struct ch_cntxt *)data;
		mtx_lock(&sc->sge.reg_lock);
		switch (ecntxt->cntxt_type) {
		case CNTXT_TYPE_EGRESS:
			error = t3_sge_read_ecntxt(sc, ecntxt->cntxt_id,
			    ecntxt->data);
			break;
		case CNTXT_TYPE_FL:
			error = t3_sge_read_fl(sc, ecntxt->cntxt_id,
			    ecntxt->data);
			break;
		case CNTXT_TYPE_RSP:
			error = t3_sge_read_rspq(sc, ecntxt->cntxt_id,
			    ecntxt->data);
			break;
		case CNTXT_TYPE_CQ:
			error = t3_sge_read_cq(sc, ecntxt->cntxt_id,
			    ecntxt->data);
			break;
		default:
			error = EINVAL;
			break;
		}
		mtx_unlock(&sc->sge.reg_lock);
		break;
	}
	case CHELSIO_GET_SGE_DESC: {
		struct ch_desc *edesc = (struct ch_desc *)data;
		int ret;
		if (edesc->queue_num >= SGE_QSETS * 6)
			return (EINVAL);
		ret = t3_get_desc(&sc->sge.qs[edesc->queue_num / 6],
		    edesc->queue_num % 6, edesc->idx, edesc->data);
		if (ret < 0)
			return (EINVAL);
		edesc->size = ret;
		break;
	}
	case CHELSIO_SET_QSET_PARAMS: {
		struct qset_params *q;
		struct ch_qset_params *t = (struct ch_qset_params *)data;
		
		if (t->qset_idx >= SGE_QSETS)
			return -EINVAL;
		if (!in_range(t->intr_lat, 0, M_NEWTIMER) ||
		    !in_range(t->cong_thres, 0, 255) ||
		    !in_range(t->txq_size[0], MIN_TXQ_ENTRIES,
			      MAX_TXQ_ENTRIES) ||
		    !in_range(t->txq_size[1], MIN_TXQ_ENTRIES,
			      MAX_TXQ_ENTRIES) ||
		    !in_range(t->txq_size[2], MIN_CTRL_TXQ_ENTRIES,
			      MAX_CTRL_TXQ_ENTRIES) ||
		    !in_range(t->fl_size[0], MIN_FL_ENTRIES, MAX_RX_BUFFERS) ||
		    !in_range(t->fl_size[1], MIN_FL_ENTRIES,
			      MAX_RX_JUMBO_BUFFERS) ||
		    !in_range(t->rspq_size, MIN_RSPQ_ENTRIES, MAX_RSPQ_ENTRIES))
		       return -EINVAL;
		if ((sc->flags & FULL_INIT_DONE) &&
		    (t->rspq_size >= 0 || t->fl_size[0] >= 0 ||
		     t->fl_size[1] >= 0 || t->txq_size[0] >= 0 ||
		     t->txq_size[1] >= 0 || t->txq_size[2] >= 0 ||
		     t->polling >= 0 || t->cong_thres >= 0))
			return -EBUSY;

		q = &sc->params.sge.qset[t->qset_idx];

		if (t->rspq_size >= 0)
			q->rspq_size = t->rspq_size;
		if (t->fl_size[0] >= 0)
			q->fl_size = t->fl_size[0];
		if (t->fl_size[1] >= 0)
			q->jumbo_size = t->fl_size[1];
		if (t->txq_size[0] >= 0)
			q->txq_size[0] = t->txq_size[0];
		if (t->txq_size[1] >= 0)
			q->txq_size[1] = t->txq_size[1];
		if (t->txq_size[2] >= 0)
			q->txq_size[2] = t->txq_size[2];
		if (t->cong_thres >= 0)
			q->cong_thres = t->cong_thres;
		if (t->intr_lat >= 0) {
			struct sge_qset *qs = &sc->sge.qs[t->qset_idx];

			q->coalesce_nsecs = t->intr_lat*1000;
			t3_update_qset_coalesce(qs, q);
		}
		break;
	}
	case CHELSIO_GET_QSET_PARAMS: {
		struct qset_params *q;
		struct ch_qset_params *t = (struct ch_qset_params *)data;

		if (t->qset_idx >= SGE_QSETS)
			return (EINVAL);

		q = &(sc)->params.sge.qset[t->qset_idx];
		t->rspq_size   = q->rspq_size;
		t->txq_size[0] = q->txq_size[0];
		t->txq_size[1] = q->txq_size[1];
		t->txq_size[2] = q->txq_size[2];
		t->fl_size[0]  = q->fl_size;
		t->fl_size[1]  = q->jumbo_size;
		t->polling     = q->polling;
		t->intr_lat    = q->coalesce_nsecs / 1000;
		t->cong_thres  = q->cong_thres;
		break;
	}
	case CHELSIO_SET_QSET_NUM: {
		struct ch_reg *edata = (struct ch_reg *)data;
		unsigned int port_idx = pi->port;
		
		if (sc->flags & FULL_INIT_DONE)
			return (EBUSY);
		if (edata->val < 1 ||
		    (edata->val > 1 && !(sc->flags & USING_MSIX)))
			return (EINVAL);
		if (edata->val + sc->port[!port_idx].nqsets > SGE_QSETS)
			return (EINVAL);
		sc->port[port_idx].nqsets = edata->val;
		sc->port[0].first_qset = 0;
		/*
		 * XXX hardcode ourselves to 2 ports just like LEEENUX
		 */
		sc->port[1].first_qset = sc->port[0].nqsets;
		break;
	}
	case CHELSIO_GET_QSET_NUM: {
		struct ch_reg *edata = (struct ch_reg *)data;
		edata->val = pi->nqsets;
		break;
	}
#ifdef notyet		
	case CHELSIO_LOAD_FW:
	case CHELSIO_GET_PM:
	case CHELSIO_SET_PM:
		return (EOPNOTSUPP);
		break;
#endif		
	case CHELSIO_SETMTUTAB: {
		struct ch_mtus *m = (struct ch_mtus *)data;
		int i;
		
		if (!is_offload(sc))
			return (EOPNOTSUPP);
		if (offload_running(sc))
			return (EBUSY);
		if (m->nmtus != NMTUS)
			return (EINVAL);
		if (m->mtus[0] < 81)         /* accommodate SACK */
			return (EINVAL);
		
		/*
		 * MTUs must be in ascending order
		 */
		for (i = 1; i < NMTUS; ++i)
			if (m->mtus[i] < m->mtus[i - 1])
				return (EINVAL);

		memcpy(sc->params.mtus, m->mtus,
		       sizeof(sc->params.mtus));
		break;
	}
	case CHELSIO_GETMTUTAB: {
		struct ch_mtus *m = (struct ch_mtus *)data;

		if (!is_offload(sc))
			return (EOPNOTSUPP);

		memcpy(m->mtus, sc->params.mtus, sizeof(m->mtus));
		m->nmtus = NMTUS;
		break;
	}		
	case CHELSIO_DEVUP:
		if (!is_offload(sc))
			return (EOPNOTSUPP);
		return offload_open(pi);
		break;
	case CHELSIO_GET_MEM: {
		struct ch_mem_range *t = (struct ch_mem_range *)data;
		struct mc7 *mem;
		uint8_t *useraddr;
		u64 buf[32];
		
		if (!is_offload(sc))
			return (EOPNOTSUPP);
		if (!(sc->flags & FULL_INIT_DONE))
			return (EIO);         /* need the memory controllers */
		if ((t->addr & 0x7) || (t->len & 0x7))
			return (EINVAL);
		if (t->mem_id == MEM_CM)
			mem = &sc->cm;
		else if (t->mem_id == MEM_PMRX)
			mem = &sc->pmrx;
		else if (t->mem_id == MEM_PMTX)
			mem = &sc->pmtx;
		else
			return (EINVAL);

		/*
		 * Version scheme:
		 * bits 0..9: chip version
		 * bits 10..15: chip revision
		 */
		t->version = 3 | (sc->params.rev << 10);
		
		/*
		 * Read 256 bytes at a time as len can be large and we don't
		 * want to use huge intermediate buffers.
		 */
		useraddr = (uint8_t *)(t + 1);   /* advance to start of buffer */
		while (t->len) {
			unsigned int chunk = min(t->len, sizeof(buf));

			error = t3_mc7_bd_read(mem, t->addr / 8, chunk / 8, buf);
			if (error)
				return (-error);
			if (copyout(buf, useraddr, chunk))
				return (EFAULT);
			useraddr += chunk;
			t->addr += chunk;
			t->len -= chunk;
		}
		break;
	}
	case CHELSIO_READ_TCAM_WORD: {
		struct ch_tcam_word *t = (struct ch_tcam_word *)data;

		if (!is_offload(sc))
			return (EOPNOTSUPP);
		return -t3_read_mc5_range(&sc->mc5, t->addr, 1, t->buf);
		break;
	}
	case CHELSIO_SET_TRACE_FILTER: {
		struct ch_trace *t = (struct ch_trace *)data;
		const struct trace_params *tp;

		tp = (const struct trace_params *)&t->sip;
		if (t->config_tx)
			t3_config_trace_filter(sc, tp, 0, t->invert_match,
					       t->trace_tx);
		if (t->config_rx)
			t3_config_trace_filter(sc, tp, 1, t->invert_match,
					       t->trace_rx);
		break;
	}
	case CHELSIO_SET_PKTSCHED: {
		struct ch_pktsched_params *p = (struct ch_pktsched_params *)data;
		if (sc->open_device_map == 0)
			return (EAGAIN);
		send_pktsched_cmd(sc, p->sched, p->idx, p->min, p->max,
		    p->binding);
		break;
	}
	case CHELSIO_IFCONF_GETREGS: {
		struct ifconf_regs *regs = (struct ifconf_regs *)data;
		int reglen = cxgb_get_regs_len();
		uint8_t *buf = malloc(REGDUMP_SIZE, M_DEVBUF, M_NOWAIT);
		if (buf == NULL) {
			return (ENOMEM);
		} if (regs->len > reglen)
			regs->len = reglen;
		else if (regs->len < reglen) {
			error = E2BIG;
			goto done;
		}
		cxgb_get_regs(sc, regs, buf);
		error = copyout(buf, regs->data, reglen);
		
		done:
		free(buf, M_DEVBUF);

		break;
	}
	case CHELSIO_SET_HW_SCHED: {
		struct ch_hw_sched *t = (struct ch_hw_sched *)data;
		unsigned int ticks_per_usec = core_ticks_per_usec(sc);

		if ((sc->flags & FULL_INIT_DONE) == 0)
			return (EAGAIN);       /* need TP to be initialized */
		if (t->sched >= NTX_SCHED || !in_range(t->mode, 0, 1) ||
		    !in_range(t->channel, 0, 1) ||
		    !in_range(t->kbps, 0, 10000000) ||
		    !in_range(t->class_ipg, 0, 10000 * 65535 / ticks_per_usec) ||
		    !in_range(t->flow_ipg, 0,
			      dack_ticks_to_usec(sc, 0x7ff)))
			return (EINVAL);

		if (t->kbps >= 0) {
			error = t3_config_sched(sc, t->kbps, t->sched);
			if (error < 0)
				return (-error);
		}
		if (t->class_ipg >= 0)
			t3_set_sched_ipg(sc, t->sched, t->class_ipg);
		if (t->flow_ipg >= 0) {
			t->flow_ipg *= 1000;     /* us -> ns */
			t3_set_pace_tbl(sc, &t->flow_ipg, t->sched, 1);
		}
		if (t->mode >= 0) {
			int bit = 1 << (S_TX_MOD_TIMER_MODE + t->sched);

			t3_set_reg_field(sc, A_TP_TX_MOD_QUEUE_REQ_MAP,
					 bit, t->mode ? bit : 0);
		}
		if (t->channel >= 0)
			t3_set_reg_field(sc, A_TP_TX_MOD_QUEUE_REQ_MAP,
					 1 << t->sched, t->channel << t->sched);
		break;
	}	
	default:
		return (EOPNOTSUPP);
		break;
	}

	return (error);
}

static __inline void
reg_block_dump(struct adapter *ap, uint8_t *buf, unsigned int start,
    unsigned int end)
{
	uint32_t *p = (uint32_t *)buf + start;

	for ( ; start <= end; start += sizeof(uint32_t))
		*p++ = t3_read_reg(ap, start);
}

#define T3_REGMAP_SIZE (3 * 1024)
static int
cxgb_get_regs_len(void)
{
	return T3_REGMAP_SIZE;
}
#undef T3_REGMAP_SIZE

static void
cxgb_get_regs(adapter_t *sc, struct ifconf_regs *regs, uint8_t *buf)
{	    
	
	/*
	 * Version scheme:
	 * bits 0..9: chip version
	 * bits 10..15: chip revision
	 * bit 31: set for PCIe cards
	 */
	regs->version = 3 | (sc->params.rev << 10) | (is_pcie(sc) << 31);

	/*
	 * We skip the MAC statistics registers because they are clear-on-read.
	 * Also reading multi-register stats would need to synchronize with the
	 * periodic mac stats accumulation.  Hard to justify the complexity.
	 */
	memset(buf, 0, REGDUMP_SIZE);
	reg_block_dump(sc, buf, 0, A_SG_RSPQ_CREDIT_RETURN);
	reg_block_dump(sc, buf, A_SG_HI_DRB_HI_THRSH, A_ULPRX_PBL_ULIMIT);
	reg_block_dump(sc, buf, A_ULPTX_CONFIG, A_MPS_INT_CAUSE);
	reg_block_dump(sc, buf, A_CPL_SWITCH_CNTRL, A_CPL_MAP_TBL_DATA);
	reg_block_dump(sc, buf, A_SMB_GLOBAL_TIME_CFG, A_XGM_SERDES_STAT3);
	reg_block_dump(sc, buf, A_XGM_SERDES_STATUS0,
		       XGM_REG(A_XGM_SERDES_STAT3, 1));
	reg_block_dump(sc, buf, XGM_REG(A_XGM_SERDES_STATUS0, 1),
		       XGM_REG(A_XGM_RX_SPI4_SOP_EOP_CNT, 1));
}
