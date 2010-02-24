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

#if __FreeBSD_version < 800044
#define V_ifnet ifnet
#endif

#include <net/if.h>
#include <net/if_var.h>
#if __FreeBSD_version >= 800056
#include <net/vnet.h>
#endif

#include <netinet/in.h>

#include <contrib/rdma/ib_verbs.h>

#include <cxgb_include.h>
#include <ulp/iw_cxgb/iw_cxgb_wr.h>
#include <ulp/iw_cxgb/iw_cxgb_hal.h>
#include <ulp/iw_cxgb/iw_cxgb_provider.h>
#include <ulp/iw_cxgb/iw_cxgb_cm.h>
#include <ulp/iw_cxgb/iw_cxgb.h>

/*
 * XXX :-/
 * 
 */

#define idr_init(x)

cxgb_cpl_handler_func t3c_handlers[NUM_CPL_CMDS];

static void open_rnic_dev(struct t3cdev *);
static void close_rnic_dev(struct t3cdev *);

static TAILQ_HEAD( ,iwch_dev) dev_list;
static struct mtx dev_mutex;
static eventhandler_tag event_tag;

static void
rnic_init(struct iwch_dev *rnicp)
{
	CTR2(KTR_IW_CXGB, "%s iwch_dev %p", __FUNCTION__,  rnicp);
	idr_init(&rnicp->cqidr);
	idr_init(&rnicp->qpidr);
	idr_init(&rnicp->mmidr);
	mtx_init(&rnicp->lock, "iwch rnic lock", NULL, MTX_DEF|MTX_DUPOK);

	rnicp->attr.vendor_id = 0x168;
	rnicp->attr.vendor_part_id = 7;
	rnicp->attr.max_qps = T3_MAX_NUM_QP - 32;
	rnicp->attr.max_wrs = (1UL << 24) - 1;
	rnicp->attr.max_sge_per_wr = T3_MAX_SGE;
	rnicp->attr.max_sge_per_rdma_write_wr = T3_MAX_SGE;
	rnicp->attr.max_cqs = T3_MAX_NUM_CQ - 1;
	rnicp->attr.max_cqes_per_cq = (1UL << 24) - 1;
	rnicp->attr.max_mem_regs = cxio_num_stags(&rnicp->rdev);
	rnicp->attr.max_phys_buf_entries = T3_MAX_PBL_SIZE;
	rnicp->attr.max_pds = T3_MAX_NUM_PD - 1;
	rnicp->attr.mem_pgsizes_bitmask = 0x7FFF;	/* 4KB-128MB */
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
open_rnic_dev(struct t3cdev *tdev)
{
	struct iwch_dev *rnicp;
	static int vers_printed;

	CTR2(KTR_IW_CXGB, "%s t3cdev %p", __FUNCTION__,  tdev);
	if (!vers_printed++)
		printf("Chelsio T3 RDMA Driver - version x.xx\n");
	rnicp = (struct iwch_dev *)ib_alloc_device(sizeof(*rnicp));
	if (!rnicp) {
		printf("Cannot allocate ib device\n");
		return;
	}
	rnicp->rdev.ulp = rnicp;
	rnicp->rdev.t3cdev_p = tdev;

	mtx_lock(&dev_mutex);

	if (cxio_rdev_open(&rnicp->rdev)) {
		mtx_unlock(&dev_mutex);
		printf("Unable to open CXIO rdev\n");
		ib_dealloc_device(&rnicp->ibdev);
		return;
	}

	rnic_init(rnicp);

	TAILQ_INSERT_TAIL(&dev_list, rnicp, entry);
	mtx_unlock(&dev_mutex);

	if (iwch_register_device(rnicp)) {
		printf("Unable to register device\n");
		close_rnic_dev(tdev);
	}
#ifdef notyet	
	printf("Initialized device %s\n",
	       pci_name(rnicp->rdev.rnic_info.pdev));
#endif	
	return;
}

static void
close_rnic_dev(struct t3cdev *tdev)
{
	struct iwch_dev *dev, *tmp;
	CTR2(KTR_IW_CXGB, "%s t3cdev %p", __FUNCTION__,  tdev);
	mtx_lock(&dev_mutex);

	TAILQ_FOREACH_SAFE(dev, &dev_list, entry, tmp) {
		if (dev->rdev.t3cdev_p == tdev) {
#ifdef notyet			
			list_del(&dev->entry);
			iwch_unregister_device(dev);
			cxio_rdev_close(&dev->rdev);
			idr_destroy(&dev->cqidr);
			idr_destroy(&dev->qpidr);
			idr_destroy(&dev->mmidr);
			ib_dealloc_device(&dev->ibdev);
#endif			
			break;
		}
	}
	mtx_unlock(&dev_mutex);
}

static ifaddr_event_handler_t
ifaddr_event_handler(void *arg, struct ifnet *ifp)
{
	printf("%s if name %s \n", __FUNCTION__, ifp->if_xname);
	if (ifp->if_capabilities & IFCAP_TOE4) {
		KASSERT(T3CDEV(ifp) != NULL, ("null t3cdev ptr!"));
		if (cxio_hal_find_rdev_by_t3cdev(T3CDEV(ifp)) == NULL)
			open_rnic_dev(T3CDEV(ifp));
	}
	return 0;
}


static int
iwch_init_module(void)
{
	VNET_ITERATOR_DECL(vnet_iter);
	int err;
	struct ifnet *ifp;

	printf("%s enter\n", __FUNCTION__);
	TAILQ_INIT(&dev_list);
	mtx_init(&dev_mutex, "iwch dev_list lock", NULL, MTX_DEF);
	
	err = cxio_hal_init();
	if (err)
		return err;
	err = iwch_cm_init();
	if (err)
		return err;
	cxio_register_ev_cb(iwch_ev_dispatch);

	/* Register for ifaddr events to dynamically add TOE devs */
	event_tag = EVENTHANDLER_REGISTER(ifaddr_event, ifaddr_event_handler,
			NULL, EVENTHANDLER_PRI_ANY);

	/* Register existing TOE interfaces by walking the ifnet chain */
	IFNET_RLOCK();
	VNET_LIST_RLOCK();
	VNET_FOREACH(vnet_iter) {
		CURVNET_SET(vnet_iter); /* XXX CURVNET_SET_QUIET() ? */
		TAILQ_FOREACH(ifp, &V_ifnet, if_link)
			(void)ifaddr_event_handler(NULL, ifp);
		CURVNET_RESTORE();
	}
	VNET_LIST_RUNLOCK();
	IFNET_RUNLOCK();
	return 0;
}

static void
iwch_exit_module(void)
{
	EVENTHANDLER_DEREGISTER(ifaddr_event, event_tag);
	cxio_unregister_ev_cb(iwch_ev_dispatch);
	iwch_cm_term();
	cxio_hal_exit();
}

static int 
iwch_load(module_t mod, int cmd, void *arg)
{
        int err = 0;

        switch (cmd) {
        case MOD_LOAD:
                printf("Loading iw_cxgb.\n");

                iwch_init_module();
                break;
        case MOD_QUIESCE:
                break;
        case MOD_UNLOAD:
                printf("Unloading iw_cxgb.\n");
		iwch_exit_module();
                break;
        case MOD_SHUTDOWN:
                break;
        default:
                err = EOPNOTSUPP;
                break;
        }

        return (err);
}

static moduledata_t mod_data = {
	"iw_cxgb",
	iwch_load,
	0
};

MODULE_VERSION(iw_cxgb, 1);
DECLARE_MODULE(iw_cxgb, mod_data, SI_SUB_EXEC, SI_ORDER_ANY);
MODULE_DEPEND(iw_cxgb, rdma_core, 1, 1, 1);
MODULE_DEPEND(iw_cxgb, if_cxgb, 1, 1, 1);
MODULE_DEPEND(iw_cxgb, t3_tom, 1, 1, 1);

