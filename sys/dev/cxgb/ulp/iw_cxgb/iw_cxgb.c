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

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/pciio.h>
#include <sys/conf.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus_dma.h>
#include <sys/rman.h>
#include <sys/ioccom.h>
#include <sys/mbuf.h>
#include <sys/rwlock.h>
#include <sys/linker.h>
#include <sys/firmware.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>
#include <sys/proc.h>
#include <sys/eventhandler.h>

#include <netinet/in.h>
#include <netinet/toecore.h>

#include <rdma/ib_verbs.h>
#include <linux/idr.h>
#include <ulp/iw_cxgb/iw_cxgb_ib_intfc.h>

#ifdef TCP_OFFLOAD
#include <cxgb_include.h>
#include <ulp/iw_cxgb/iw_cxgb_wr.h>
#include <ulp/iw_cxgb/iw_cxgb_hal.h>
#include <ulp/iw_cxgb/iw_cxgb_provider.h>
#include <ulp/iw_cxgb/iw_cxgb_cm.h>
#include <ulp/iw_cxgb/iw_cxgb.h>

static int iwch_mod_load(void);
static int iwch_mod_unload(void);
static int iwch_activate(struct adapter *);
static int iwch_deactivate(struct adapter *);

static struct uld_info iwch_uld_info = {
	.uld_id = ULD_IWARP,
	.activate = iwch_activate,
	.deactivate = iwch_deactivate,
};

static void
rnic_init(struct iwch_dev *rnicp)
{

	idr_init(&rnicp->cqidr);
	idr_init(&rnicp->qpidr);
	idr_init(&rnicp->mmidr);
	mtx_init(&rnicp->lock, "iwch rnic lock", NULL, MTX_DEF|MTX_DUPOK);

	rnicp->attr.vendor_id = 0x168;
	rnicp->attr.vendor_part_id = 7;
	rnicp->attr.max_qps = T3_MAX_NUM_QP - 32;
	rnicp->attr.max_wrs = T3_MAX_QP_DEPTH;
	rnicp->attr.max_sge_per_wr = T3_MAX_SGE;
	rnicp->attr.max_sge_per_rdma_write_wr = T3_MAX_SGE;
	rnicp->attr.max_cqs = T3_MAX_NUM_CQ - 1;
	rnicp->attr.max_cqes_per_cq = T3_MAX_CQ_DEPTH;
	rnicp->attr.max_mem_regs = cxio_num_stags(&rnicp->rdev);
	rnicp->attr.max_phys_buf_entries = T3_MAX_PBL_SIZE;
	rnicp->attr.max_pds = T3_MAX_NUM_PD - 1;
	rnicp->attr.mem_pgsizes_bitmask = T3_PAGESIZE_MASK;
	rnicp->attr.max_mr_size = T3_MAX_MR_SIZE;
	rnicp->attr.can_resize_wq = 0;
	rnicp->attr.max_rdma_reads_per_qp = 8;
	rnicp->attr.max_rdma_read_resources =
	    rnicp->attr.max_rdma_reads_per_qp * rnicp->attr.max_qps;
	rnicp->attr.max_rdma_read_qp_depth = 8;	/* IRD */
	rnicp->attr.max_rdma_read_depth =
	    rnicp->attr.max_rdma_read_qp_depth * rnicp->attr.max_qps;
	rnicp->attr.rq_overflow_handled = 0;
	rnicp->attr.can_modify_ird = 0;
	rnicp->attr.can_modify_ord = 0;
	rnicp->attr.max_mem_windows = rnicp->attr.max_mem_regs - 1;
	rnicp->attr.stag0_value = 1;
	rnicp->attr.zbva_support = 1;
	rnicp->attr.local_invalidate_fence = 1;
	rnicp->attr.cq_overflow_detection = 1;

	return;
}

static void
rnic_uninit(struct iwch_dev *rnicp)
{
	idr_destroy(&rnicp->cqidr);
	idr_destroy(&rnicp->qpidr);
	idr_destroy(&rnicp->mmidr);
	mtx_destroy(&rnicp->lock);
}

static int
iwch_activate(struct adapter *sc)
{
	struct iwch_dev *rnicp;
	int rc;

	KASSERT(!isset(&sc->offload_map, MAX_NPORTS),
	    ("%s: iWARP already activated on %s", __func__,
	    device_get_nameunit(sc->dev)));

	rnicp = (struct iwch_dev *)ib_alloc_device(sizeof(*rnicp));
	if (rnicp == NULL)
		return (ENOMEM);

	sc->iwarp_softc = rnicp;
	rnicp->rdev.adap = sc;

	cxio_hal_init(sc);
	iwch_cm_init_cpl(sc);

	rc = cxio_rdev_open(&rnicp->rdev);
	if (rc != 0) {
		printf("Unable to open CXIO rdev\n");
		goto err1;
	}

	rnic_init(rnicp);

	rc = iwch_register_device(rnicp);
	if (rc != 0) {
		printf("Unable to register device\n");
		goto err2;
	}

	return (0);

err2:
	rnic_uninit(rnicp);
	cxio_rdev_close(&rnicp->rdev);
err1:
	cxio_hal_uninit(sc);
	iwch_cm_term_cpl(sc);
	sc->iwarp_softc = NULL;

	return (rc);
}

static int
iwch_deactivate(struct adapter *sc)
{
	struct iwch_dev *rnicp;

	rnicp = sc->iwarp_softc;

	iwch_unregister_device(rnicp);
	rnic_uninit(rnicp);
	cxio_rdev_close(&rnicp->rdev);
	cxio_hal_uninit(sc);
	iwch_cm_term_cpl(sc);
	ib_dealloc_device(&rnicp->ibdev);

	sc->iwarp_softc = NULL;

	return (0);
}

static void
iwch_activate_all(struct adapter *sc, void *arg __unused)
{
	ADAPTER_LOCK(sc);
	if ((sc->open_device_map & sc->offload_map) != 0 &&
	    t3_activate_uld(sc, ULD_IWARP) == 0)
		setbit(&sc->offload_map, MAX_NPORTS);
	ADAPTER_UNLOCK(sc);
}

static void
iwch_deactivate_all(struct adapter *sc, void *arg __unused)
{
	ADAPTER_LOCK(sc);
	if (isset(&sc->offload_map, MAX_NPORTS) &&
	    t3_deactivate_uld(sc, ULD_IWARP) == 0)
		clrbit(&sc->offload_map, MAX_NPORTS);
	ADAPTER_UNLOCK(sc);
}

static int
iwch_mod_load(void)
{
	int rc;

	rc = iwch_cm_init();
	if (rc != 0)
		return (rc);

	rc = t3_register_uld(&iwch_uld_info);
	if (rc != 0) {
		iwch_cm_term();
		return (rc);
	}

	t3_iterate(iwch_activate_all, NULL);

	return (rc);
}

static int
iwch_mod_unload(void)
{
	t3_iterate(iwch_deactivate_all, NULL);

	iwch_cm_term();

	if (t3_unregister_uld(&iwch_uld_info) == EBUSY)
		return (EBUSY);

	return (0);
}
#endif	/* TCP_OFFLOAD */

#undef MODULE_VERSION
#include <sys/module.h>

static int
iwch_modevent(module_t mod, int cmd, void *arg)
{
	int rc = 0;

#ifdef TCP_OFFLOAD
	switch (cmd) {
	case MOD_LOAD:
		rc = iwch_mod_load();
		if(rc)
			printf("iw_cxgb: Chelsio T3 RDMA Driver failed to load\n");
		else
			printf("iw_cxgb: Chelsio T3 RDMA Driver loaded\n");
		break;

	case MOD_UNLOAD:
		rc = iwch_mod_unload();
		if(rc)
			printf("iw_cxgb: Chelsio T3 RDMA Driver failed to unload\n");
		else
			printf("iw_cxgb: Chelsio T3 RDMA Driver unloaded\n");
		break;

	default:
		rc = EINVAL;
	}
#else
	printf("iw_cxgb: compiled without TCP_OFFLOAD support.\n");
	rc = EOPNOTSUPP;
#endif
	return (rc);
}

static moduledata_t iwch_mod_data = {
	"iw_cxgb",
	iwch_modevent,
	0
};

MODULE_VERSION(iw_cxgb, 1);
DECLARE_MODULE(iw_cxgb, iwch_mod_data, SI_SUB_EXEC, SI_ORDER_ANY);
MODULE_DEPEND(t3_tom, cxgbc, 1, 1, 1);
MODULE_DEPEND(iw_cxgb, toecore, 1, 1, 1);
MODULE_DEPEND(iw_cxgb, t3_tom, 1, 1, 1);
