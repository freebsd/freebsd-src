/*-
 * Copyright (c) 2021,2022 NVIDIA CORPORATION & AFFILIATES. ALL RIGHTS RESERVED.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS `AS IS' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ck.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/pctrie.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/protosw.h>
#include <sys/taskqueue.h>

#include <machine/stdarg.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/vnet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/in_pcb.h>
#include <netinet/tcp_var.h>

#include <netipsec/key.h>
#include <netipsec/keydb.h>
#include <netipsec/key_debug.h>
#include <netipsec/xform.h>
#include <netipsec/ipsec.h>
#include <netipsec/ipsec_offload.h>
#include <netipsec/ah_var.h>
#include <netipsec/esp.h>
#include <netipsec/esp_var.h>
#include <netipsec/ipcomp_var.h>

#ifdef IPSEC_OFFLOAD

static struct mtx ipsec_accel_sav_tmp;
static struct unrhdr *drv_spi_unr;
static struct mtx ipsec_accel_cnt_lock;
static struct taskqueue *ipsec_accel_tq;

struct ipsec_accel_install_newkey_tq {
	struct secasvar *sav;
	struct vnet *install_vnet;
	struct task install_task;
};

struct ipsec_accel_forget_tq {
	struct vnet *forget_vnet;
	struct task forget_task;
	struct secasvar *sav;
};

struct ifp_handle_sav {
	CK_LIST_ENTRY(ifp_handle_sav) sav_link;
	CK_LIST_ENTRY(ifp_handle_sav) sav_allh_link;
	struct secasvar *sav;
	struct ifnet *ifp;
	void *ifdata;
	uint64_t drv_spi;
	uint32_t flags;
	size_t hdr_ext_size;
	uint64_t cnt_octets;
	uint64_t cnt_allocs;
};

#define	IFP_HS_HANDLED	0x00000001
#define	IFP_HS_REJECTED	0x00000002
#define	IFP_HS_MARKER	0x00000010

static CK_LIST_HEAD(, ifp_handle_sav) ipsec_accel_all_sav_handles;

struct ifp_handle_sp {
	CK_LIST_ENTRY(ifp_handle_sp) sp_link;
	CK_LIST_ENTRY(ifp_handle_sp) sp_allh_link;
	struct secpolicy *sp;
	struct ifnet *ifp;
	void *ifdata;
	uint32_t flags;
};

#define	IFP_HP_HANDLED	0x00000001
#define	IFP_HP_REJECTED	0x00000002
#define	IFP_HP_MARKER	0x00000004

static CK_LIST_HEAD(, ifp_handle_sp) ipsec_accel_all_sp_handles;

static void *
drvspi_sa_trie_alloc(struct pctrie *ptree)
{
	void *res;

	res = malloc(pctrie_node_size(), M_IPSEC_MISC, M_ZERO | M_NOWAIT);
	if (res != NULL)
		pctrie_zone_init(res, 0, 0);
	return (res);
}

static void
drvspi_sa_trie_free(struct pctrie *ptree, void *node)
{
	free(node, M_IPSEC_MISC);
}

PCTRIE_DEFINE(DRVSPI_SA, ifp_handle_sav, drv_spi,
    drvspi_sa_trie_alloc, drvspi_sa_trie_free);
static struct pctrie drv_spi_pctrie;

static eventhandler_tag ipsec_accel_ifdetach_event_tag;

static void ipsec_accel_sa_newkey_impl(struct secasvar *sav);
static int ipsec_accel_handle_sav(struct secasvar *sav, struct ifnet *ifp,
    u_int drv_spi, void *priv, uint32_t flags, struct ifp_handle_sav **ires);
static void ipsec_accel_forget_sav_clear(struct secasvar *sav);
static struct ifp_handle_sav *ipsec_accel_is_accel_sav_ptr(struct secasvar *sav,
    struct ifnet *ifp);
static int ipsec_accel_sa_lifetime_op_impl(struct secasvar *sav,
    struct seclifetime *lft_c, if_t ifp, enum IF_SA_CNT_WHICH op,
    struct rm_priotracker *sahtree_trackerp);
static void ipsec_accel_sa_recordxfer(struct secasvar *sav, struct mbuf *m);
static void ipsec_accel_sync_imp(void);
static bool ipsec_accel_is_accel_sav_impl(struct secasvar *sav);
static struct mbuf *ipsec_accel_key_setaccelif_impl(struct secasvar *sav);
static void ipsec_accel_on_ifdown_impl(struct ifnet *ifp);
static void ipsec_accel_drv_sa_lifetime_update_impl(struct secasvar *sav,
    if_t ifp, u_int drv_spi, uint64_t octets, uint64_t allocs);
static int ipsec_accel_drv_sa_lifetime_fetch_impl(struct secasvar *sav,
    if_t ifp, u_int drv_spi, uint64_t *octets, uint64_t *allocs);
static void ipsec_accel_ifdetach_event(void *arg, struct ifnet *ifp);

static void
ipsec_accel_init(void *arg)
{
	mtx_init(&ipsec_accel_sav_tmp, "ipasat", MTX_DEF, 0);
	mtx_init(&ipsec_accel_cnt_lock, "ipascn", MTX_DEF, 0);
	drv_spi_unr = new_unrhdr(IPSEC_ACCEL_DRV_SPI_MIN,
	    IPSEC_ACCEL_DRV_SPI_MAX, &ipsec_accel_sav_tmp);
	ipsec_accel_tq = taskqueue_create("ipsec_offload", M_WAITOK,
	    taskqueue_thread_enqueue, &ipsec_accel_tq);
	(void)taskqueue_start_threads(&ipsec_accel_tq,
	    1 /* Must be single-threaded */, PWAIT,
	    "ipsec_offload");
	ipsec_accel_sa_newkey_p = ipsec_accel_sa_newkey_impl;
	ipsec_accel_forget_sav_p = ipsec_accel_forget_sav_impl;
	ipsec_accel_spdadd_p = ipsec_accel_spdadd_impl;
	ipsec_accel_spddel_p = ipsec_accel_spddel_impl;
	ipsec_accel_sa_lifetime_op_p = ipsec_accel_sa_lifetime_op_impl;
	ipsec_accel_sync_p = ipsec_accel_sync_imp;
	ipsec_accel_is_accel_sav_p = ipsec_accel_is_accel_sav_impl;
	ipsec_accel_key_setaccelif_p = ipsec_accel_key_setaccelif_impl;
	ipsec_accel_on_ifdown_p = ipsec_accel_on_ifdown_impl;
	ipsec_accel_drv_sa_lifetime_update_p =
	    ipsec_accel_drv_sa_lifetime_update_impl;
	ipsec_accel_drv_sa_lifetime_fetch_p =
	    ipsec_accel_drv_sa_lifetime_fetch_impl;
	pctrie_init(&drv_spi_pctrie);
	ipsec_accel_ifdetach_event_tag = EVENTHANDLER_REGISTER(
	    ifnet_departure_event, ipsec_accel_ifdetach_event, NULL,
	    EVENTHANDLER_PRI_ANY);
}
SYSINIT(ipsec_accel_init, SI_SUB_VNET_DONE, SI_ORDER_ANY,
    ipsec_accel_init, NULL);

static void
ipsec_accel_fini(void *arg)
{
	EVENTHANDLER_DEREGISTER(ifnet_departure_event,
	    ipsec_accel_ifdetach_event_tag);
	ipsec_accel_sa_newkey_p = NULL;
	ipsec_accel_forget_sav_p = NULL;
	ipsec_accel_spdadd_p = NULL;
	ipsec_accel_spddel_p = NULL;
	ipsec_accel_sa_lifetime_op_p = NULL;
	ipsec_accel_sync_p = NULL;
	ipsec_accel_is_accel_sav_p = NULL;
	ipsec_accel_key_setaccelif_p = NULL;
	ipsec_accel_on_ifdown_p = NULL;
	ipsec_accel_drv_sa_lifetime_update_p = NULL;
	ipsec_accel_drv_sa_lifetime_fetch_p = NULL;
	ipsec_accel_sync_imp();
	clean_unrhdr(drv_spi_unr);	/* avoid panic, should go later */
	clear_unrhdr(drv_spi_unr);
	delete_unrhdr(drv_spi_unr);
	taskqueue_drain_all(ipsec_accel_tq);
	taskqueue_free(ipsec_accel_tq);
	mtx_destroy(&ipsec_accel_sav_tmp);
	mtx_destroy(&ipsec_accel_cnt_lock);
}
SYSUNINIT(ipsec_accel_fini, SI_SUB_VNET_DONE, SI_ORDER_ANY,
    ipsec_accel_fini, NULL);

SYSCTL_NODE(_net_inet_ipsec, OID_AUTO, offload, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "");

static bool ipsec_offload_verbose = false;
SYSCTL_BOOL(_net_inet_ipsec_offload, OID_AUTO, verbose, CTLFLAG_RW,
    &ipsec_offload_verbose, 0,
    "Verbose SA/SP offload install and deinstall");

static void
dprintf(const char *fmt, ...)
{
	va_list ap;

	if (!ipsec_offload_verbose)
		return;

	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

static void
ipsec_accel_alloc_forget_tq(struct secasvar *sav)
{
	void *ftq;

	if (sav->accel_forget_tq != 0)
		return;

	ftq = malloc(sizeof(struct ipsec_accel_forget_tq), M_TEMP, M_WAITOK);
	if (!atomic_cmpset_ptr(&sav->accel_forget_tq, 0, (uintptr_t)ftq))
		free(ftq, M_TEMP);
}

static bool
ipsec_accel_sa_install_match(if_t ifp, void *arg)
{
	if ((ifp->if_capenable2 & IFCAP2_BIT(IFCAP2_IPSEC_OFFLOAD)) == 0)
		return (false);
	if (ifp->if_ipsec_accel_m->if_sa_newkey == NULL) {
		dprintf("driver bug ifp %s if_sa_newkey NULL\n",
		    if_name(ifp));
		return (false);
	}
	return (true);
}

static int
ipsec_accel_sa_newkey_cb(if_t ifp, void *arg)
{
	struct ipsec_accel_install_newkey_tq *tq;
	void *priv;
	u_int drv_spi;
	int error;

	tq = arg;

	dprintf("ipsec_accel_sa_newkey_act: ifp %s h %p spi %#x "
	    "flags %#x seq %d\n",
	    if_name(ifp), ifp->if_ipsec_accel_m->if_sa_newkey,
	    be32toh(tq->sav->spi), tq->sav->flags, tq->sav->seq);
	priv = NULL;
	drv_spi = alloc_unr(drv_spi_unr);
	if (tq->sav->accel_ifname != NULL &&
	    strcmp(tq->sav->accel_ifname, if_name(ifp)) != 0) {
		error = ipsec_accel_handle_sav(tq->sav,
		    ifp, drv_spi, priv, IFP_HS_REJECTED, NULL);
		goto out;
	}
	if (drv_spi == -1) {
		/* XXXKIB */
		dprintf("ipsec_accel_sa_install_newkey: cannot alloc "
		    "drv_spi if %s spi %#x\n", if_name(ifp),
		    be32toh(tq->sav->spi));
		return (ENOMEM);
	}
	error = ifp->if_ipsec_accel_m->if_sa_newkey(ifp, tq->sav,
	    drv_spi, &priv);
	if (error != 0) {
		if (error == EOPNOTSUPP) {
			dprintf("ipsec_accel_sa_newkey: driver "
			    "refused sa if %s spi %#x\n",
			    if_name(ifp), be32toh(tq->sav->spi));
			error = ipsec_accel_handle_sav(tq->sav,
			    ifp, drv_spi, priv, IFP_HS_REJECTED, NULL);
			/* XXXKIB */
		} else {
			dprintf("ipsec_accel_sa_newkey: driver "
			    "error %d if %s spi %#x\n",
			    error, if_name(ifp), be32toh(tq->sav->spi));
			/* XXXKIB */
		}
	} else {
		error = ipsec_accel_handle_sav(tq->sav, ifp,
		    drv_spi, priv, IFP_HS_HANDLED, NULL);
		if (error != 0) {
			/* XXXKIB */
			dprintf("ipsec_accel_sa_newkey: handle_sav "
			    "err %d if %s spi %#x\n", error,
			    if_name(ifp), be32toh(tq->sav->spi));
		}
	}
out:
	return (error);
}

static void
ipsec_accel_sa_newkey_act(void *context, int pending)
{
	struct ipsec_accel_install_newkey_tq *tq;
	void *tqf;
	struct secasvar *sav;

	tq = context;
	tqf = NULL;
	sav = tq->sav;
	CURVNET_SET(tq->install_vnet);
	mtx_lock(&ipsec_accel_sav_tmp);
	if ((sav->accel_flags & (SADB_KEY_ACCEL_INST |
	    SADB_KEY_ACCEL_DEINST)) == 0 &&
	    sav->state == SADB_SASTATE_MATURE) {
		sav->accel_flags |= SADB_KEY_ACCEL_INST;
		mtx_unlock(&ipsec_accel_sav_tmp);
		if_foreach_sleep(ipsec_accel_sa_install_match, context,
		    ipsec_accel_sa_newkey_cb, context);
		ipsec_accel_alloc_forget_tq(sav);
		mtx_lock(&ipsec_accel_sav_tmp);

		/*
		 * If ipsec_accel_forget_sav() raced with us and set
		 * the flag, do its work.  Its task cannot execute in
		 * parallel since ipsec_accel taskqueue is single-threaded.
		 */
		if ((sav->accel_flags & SADB_KEY_ACCEL_DEINST) != 0) {
			tqf = (void *)sav->accel_forget_tq;
			sav->accel_forget_tq = 0;
			ipsec_accel_forget_sav_clear(sav);
		}
	}
	mtx_unlock(&ipsec_accel_sav_tmp);
	key_freesav(&tq->sav);
	CURVNET_RESTORE();
	free(tq, M_TEMP);
	free(tqf, M_TEMP);
}

static void
ipsec_accel_sa_newkey_impl(struct secasvar *sav)
{
	struct ipsec_accel_install_newkey_tq *tq;

	if ((sav->accel_flags & (SADB_KEY_ACCEL_INST |
	    SADB_KEY_ACCEL_DEINST)) != 0)
		return;

	dprintf(
	    "ipsec_accel_sa_install_newkey: spi %#x flags %#x seq %d\n",
	    be32toh(sav->spi), sav->flags, sav->seq);

	tq = malloc(sizeof(*tq), M_TEMP, M_NOWAIT);
	if (tq == NULL) {
		dprintf("ipsec_accel_sa_install_newkey: no memory for tq, "
		    "spi %#x\n", be32toh(sav->spi));
		/* XXXKIB */
		return;
	}

	refcount_acquire(&sav->refcnt);

	TASK_INIT(&tq->install_task, 0, ipsec_accel_sa_newkey_act, tq);
	tq->sav = sav;
	tq->install_vnet = curthread->td_vnet;
	taskqueue_enqueue(ipsec_accel_tq, &tq->install_task);
}

static int
ipsec_accel_handle_sav(struct secasvar *sav, struct ifnet *ifp,
    u_int drv_spi, void *priv, uint32_t flags, struct ifp_handle_sav **ires)
{
	struct ifp_handle_sav *ihs, *i;
	int error;

	MPASS(__bitcount(flags & (IFP_HS_HANDLED | IFP_HS_REJECTED)) == 1);

	ihs = malloc(sizeof(*ihs), M_IPSEC_MISC, M_WAITOK | M_ZERO);
	ihs->ifp = ifp;
	ihs->sav = sav;
	ihs->drv_spi = drv_spi;
	ihs->ifdata = priv;
	ihs->flags = flags;
	ihs->hdr_ext_size = esp_hdrsiz(sav);
	mtx_lock(&ipsec_accel_sav_tmp);
	CK_LIST_FOREACH(i, &sav->accel_ifps, sav_link) {
		if (i->ifp == ifp) {
			error = EALREADY;
			goto errout;
		}
	}
	error = DRVSPI_SA_PCTRIE_INSERT(&drv_spi_pctrie, ihs);
	if (error != 0)
		goto errout;
	if_ref(ihs->ifp);
	CK_LIST_INSERT_HEAD(&sav->accel_ifps, ihs, sav_link);
	CK_LIST_INSERT_HEAD(&ipsec_accel_all_sav_handles, ihs, sav_allh_link);
	mtx_unlock(&ipsec_accel_sav_tmp);
	if (ires != NULL)
		*ires = ihs;
	return (0);
errout:
	mtx_unlock(&ipsec_accel_sav_tmp);
	free(ihs, M_IPSEC_MISC);
	if (ires != NULL)
		*ires = NULL;
	return (error);
}

static void
ipsec_accel_forget_handle_sav(struct ifp_handle_sav *i, bool freesav)
{
	struct ifnet *ifp;
	struct secasvar *sav;

	mtx_assert(&ipsec_accel_sav_tmp, MA_OWNED);

	CK_LIST_REMOVE(i, sav_link);
	CK_LIST_REMOVE(i, sav_allh_link);
	DRVSPI_SA_PCTRIE_REMOVE(&drv_spi_pctrie, i->drv_spi);
	mtx_unlock(&ipsec_accel_sav_tmp);
	NET_EPOCH_WAIT();
	ifp = i->ifp;
	sav = i->sav;
	if ((i->flags & (IFP_HS_HANDLED | IFP_HS_REJECTED)) ==
	    IFP_HS_HANDLED) {
		dprintf("sa deinstall %s %p spi %#x ifl %#x\n",
		    if_name(ifp), sav, be32toh(sav->spi), i->flags);
		ifp->if_ipsec_accel_m->if_sa_deinstall(ifp,
		    i->drv_spi, i->ifdata);
	}
	if_rele(ifp);
	free_unr(drv_spi_unr, i->drv_spi);
	free(i, M_IPSEC_MISC);
	if (freesav)
		key_freesav(&sav);
	mtx_lock(&ipsec_accel_sav_tmp);
}

static void
ipsec_accel_forget_sav_clear(struct secasvar *sav)
{
	struct ifp_handle_sav *i;

	for (;;) {
		i = CK_LIST_FIRST(&sav->accel_ifps);
		if (i == NULL)
			break;
		ipsec_accel_forget_handle_sav(i, false);
	}
}

static void
ipsec_accel_forget_sav_act(void *arg, int pending)
{
	struct ipsec_accel_forget_tq *tq;
	struct secasvar *sav;

	tq = arg;
	sav = tq->sav;
	CURVNET_SET(tq->forget_vnet);
	mtx_lock(&ipsec_accel_sav_tmp);
	ipsec_accel_forget_sav_clear(sav);
	mtx_unlock(&ipsec_accel_sav_tmp);
	key_freesav(&sav);
	CURVNET_RESTORE();
	free(tq, M_TEMP);
}

void
ipsec_accel_forget_sav_impl(struct secasvar *sav)
{
	struct ipsec_accel_forget_tq *tq;

	mtx_lock(&ipsec_accel_sav_tmp);
	sav->accel_flags |= SADB_KEY_ACCEL_DEINST;
	tq = (void *)atomic_load_ptr(&sav->accel_forget_tq);
	if (tq == NULL || !atomic_cmpset_ptr(&sav->accel_forget_tq,
	    (uintptr_t)tq, 0)) {
		mtx_unlock(&ipsec_accel_sav_tmp);
		return;
	}
	mtx_unlock(&ipsec_accel_sav_tmp);

	refcount_acquire(&sav->refcnt);
	TASK_INIT(&tq->forget_task, 0, ipsec_accel_forget_sav_act, tq);
	tq->forget_vnet = curthread->td_vnet;
	tq->sav = sav;
	taskqueue_enqueue(ipsec_accel_tq, &tq->forget_task);
}

static void
ipsec_accel_on_ifdown_sav(struct ifnet *ifp)
{
	struct ifp_handle_sav *i, *marker;

	marker = malloc(sizeof(*marker), M_IPSEC_MISC, M_WAITOK | M_ZERO);
	marker->flags = IFP_HS_MARKER;

	mtx_lock(&ipsec_accel_sav_tmp);
	CK_LIST_INSERT_HEAD(&ipsec_accel_all_sav_handles, marker,
	    sav_allh_link);
	for (;;) {
		i = CK_LIST_NEXT(marker, sav_allh_link);
		if (i == NULL)
			break;
		CK_LIST_REMOVE(marker, sav_allh_link);
		CK_LIST_INSERT_AFTER(i, marker, sav_allh_link);
		if (i->ifp == ifp) {
			refcount_acquire(&i->sav->refcnt); /* XXXKIB wrap ? */
			ipsec_accel_forget_handle_sav(i, true);
		}
	}
	CK_LIST_REMOVE(marker, sav_allh_link);
	mtx_unlock(&ipsec_accel_sav_tmp);
	free(marker, M_IPSEC_MISC);
}

static struct ifp_handle_sav *
ipsec_accel_is_accel_sav_ptr_raw(struct secasvar *sav, struct ifnet *ifp)
{
	struct ifp_handle_sav *i;

	if ((ifp->if_capenable2 & IFCAP2_BIT(IFCAP2_IPSEC_OFFLOAD)) == 0)
		return (NULL);
	CK_LIST_FOREACH(i, &sav->accel_ifps, sav_link) {
		if (i->ifp == ifp)
			return (i);
	}
	return (NULL);
}

static struct ifp_handle_sav *
ipsec_accel_is_accel_sav_ptr(struct secasvar *sav, struct ifnet *ifp)
{
	NET_EPOCH_ASSERT();
	return (ipsec_accel_is_accel_sav_ptr_raw(sav, ifp));
}

static bool
ipsec_accel_is_accel_sav_impl(struct secasvar *sav)
{
	return (!CK_LIST_EMPTY(&sav->accel_ifps));
}

static struct secasvar *
ipsec_accel_drvspi_to_sa(u_int drv_spi)
{
	struct ifp_handle_sav *i;

	i = DRVSPI_SA_PCTRIE_LOOKUP(&drv_spi_pctrie, drv_spi);
	if (i == NULL)
		return (NULL);
	return (i->sav);
}

static struct ifp_handle_sp *
ipsec_accel_find_accel_sp(struct secpolicy *sp, if_t ifp)
{
	struct ifp_handle_sp *i;

	CK_LIST_FOREACH(i, &sp->accel_ifps, sp_link) {
		if (i->ifp == ifp)
			return (i);
	}
	return (NULL);
}

static bool
ipsec_accel_is_accel_sp(struct secpolicy *sp, if_t ifp)
{
	return (ipsec_accel_find_accel_sp(sp, ifp) != NULL);
}

static int
ipsec_accel_remember_sp(struct secpolicy *sp, if_t ifp,
    struct ifp_handle_sp **ip)
{
	struct ifp_handle_sp *i;

	i = malloc(sizeof(*i), M_IPSEC_MISC, M_WAITOK | M_ZERO);
	i->sp = sp;
	i->ifp = ifp;
	if_ref(ifp);
	i->flags = IFP_HP_HANDLED;
	mtx_lock(&ipsec_accel_sav_tmp);
	CK_LIST_INSERT_HEAD(&sp->accel_ifps, i, sp_link);
	CK_LIST_INSERT_HEAD(&ipsec_accel_all_sp_handles, i, sp_allh_link);
	mtx_unlock(&ipsec_accel_sav_tmp);
	*ip = i;
	return (0);
}

static bool
ipsec_accel_spdadd_match(if_t ifp, void *arg)
{
	struct secpolicy *sp;

	if ((ifp->if_capenable2 & IFCAP2_BIT(IFCAP2_IPSEC_OFFLOAD)) == 0 ||
	    ifp->if_ipsec_accel_m->if_spdadd == NULL)
		return (false);
	sp = arg;
	if (sp->accel_ifname != NULL &&
	    strcmp(sp->accel_ifname, if_name(ifp)) != 0)
		return (false);
	if (ipsec_accel_is_accel_sp(sp, ifp))
		return (false);
	return (true);
}

static int
ipsec_accel_spdadd_cb(if_t ifp, void *arg)
{
	struct secpolicy *sp;
	struct inpcb *inp;
	struct ifp_handle_sp *i;
	int error;

	sp = arg;
	inp = sp->ipsec_accel_add_sp_inp;
	dprintf("ipsec_accel_spdadd_cb: ifp %s m %p sp %p inp %p\n",
	    if_name(ifp), ifp->if_ipsec_accel_m->if_spdadd, sp, inp);
	error = ipsec_accel_remember_sp(sp, ifp, &i);
	if (error != 0) {
		dprintf("ipsec_accel_spdadd: %s if_spdadd %p remember res %d\n",
		    if_name(ifp), sp, error);
		return (error);
	}
	error = ifp->if_ipsec_accel_m->if_spdadd(ifp, sp, inp, &i->ifdata);
	if (error != 0) {
		i->flags |= IFP_HP_REJECTED;
		dprintf("ipsec_accel_spdadd: %s if_spdadd %p res %d\n",
		    if_name(ifp), sp, error);
	}
	return (error);
}

static void
ipsec_accel_spdadd_act(void *arg, int pending)
{
	struct secpolicy *sp;
	struct inpcb *inp;

	sp = arg;
	CURVNET_SET(sp->accel_add_tq.adddel_vnet);
	if_foreach_sleep(ipsec_accel_spdadd_match, arg,
	    ipsec_accel_spdadd_cb, arg);
	inp = sp->ipsec_accel_add_sp_inp;
	if (inp != NULL) {
		INP_WLOCK(inp);
		if (!in_pcbrele_wlocked(inp))
			INP_WUNLOCK(inp);
		sp->ipsec_accel_add_sp_inp = NULL;
	}
	CURVNET_RESTORE();
	key_freesp(&sp);
}

void
ipsec_accel_spdadd_impl(struct secpolicy *sp, struct inpcb *inp)
{
	struct ipsec_accel_adddel_sp_tq *tq;

	if (sp == NULL)
		return;
	if (sp->tcount == 0 && inp == NULL)
		return;
	tq = &sp->accel_add_tq;
	if (atomic_cmpset_int(&tq->adddel_scheduled, 0, 1) == 0)
		return;
	tq->adddel_vnet = curthread->td_vnet;
	sp->ipsec_accel_add_sp_inp = inp;
	if (inp != NULL)
		in_pcbref(inp);
	TASK_INIT(&tq->adddel_task, 0, ipsec_accel_spdadd_act, sp);
	key_addref(sp);
	taskqueue_enqueue(ipsec_accel_tq, &tq->adddel_task);
}

static void
ipsec_accel_spddel_act(void *arg, int pending)
{
	struct ifp_handle_sp *i;
	struct secpolicy *sp;
	int error;

	sp = arg;
	CURVNET_SET(sp->accel_del_tq.adddel_vnet);
	mtx_lock(&ipsec_accel_sav_tmp);
	for (;;) {
		i = CK_LIST_FIRST(&sp->accel_ifps);
		if (i == NULL)
			break;
		CK_LIST_REMOVE(i, sp_link);
		CK_LIST_REMOVE(i, sp_allh_link);
		mtx_unlock(&ipsec_accel_sav_tmp);
		NET_EPOCH_WAIT();
		if ((i->flags & (IFP_HP_HANDLED | IFP_HP_REJECTED)) ==
		    IFP_HP_HANDLED) {
			dprintf("spd deinstall %s %p\n", if_name(i->ifp), sp);
			error = i->ifp->if_ipsec_accel_m->if_spddel(i->ifp,
			    sp, i->ifdata);
			if (error != 0) {
				dprintf(
		    "ipsec_accel_spddel: %s if_spddel %p res %d\n",
				    if_name(i->ifp), sp, error);
			}
		}
		if_rele(i->ifp);
		free(i, M_IPSEC_MISC);
		mtx_lock(&ipsec_accel_sav_tmp);
	}
	mtx_unlock(&ipsec_accel_sav_tmp);
	key_freesp(&sp);
	CURVNET_RESTORE();
}

void
ipsec_accel_spddel_impl(struct secpolicy *sp)
{
	struct ipsec_accel_adddel_sp_tq *tq;

	if (sp == NULL)
		return;

	tq = &sp->accel_del_tq;
	if (atomic_cmpset_int(&tq->adddel_scheduled, 0, 1) == 0)
		return;
	tq->adddel_vnet = curthread->td_vnet;
	TASK_INIT(&tq->adddel_task, 0, ipsec_accel_spddel_act, sp);
	key_addref(sp);
	taskqueue_enqueue(ipsec_accel_tq, &tq->adddel_task);
}

static void
ipsec_accel_on_ifdown_sp(struct ifnet *ifp)
{
	struct ifp_handle_sp *i, *marker;
	struct secpolicy *sp;
	int error;

	marker = malloc(sizeof(*marker), M_IPSEC_MISC, M_WAITOK | M_ZERO);
	marker->flags = IFP_HS_MARKER;

	mtx_lock(&ipsec_accel_sav_tmp);
	CK_LIST_INSERT_HEAD(&ipsec_accel_all_sp_handles, marker,
	    sp_allh_link);
	for (;;) {
		i = CK_LIST_NEXT(marker, sp_allh_link);
		if (i == NULL)
			break;
		CK_LIST_REMOVE(marker, sp_allh_link);
		CK_LIST_INSERT_AFTER(i, marker, sp_allh_link);
		if (i->ifp != ifp)
			continue;

		sp = i->sp;
		key_addref(sp);
		CK_LIST_REMOVE(i, sp_link);
		CK_LIST_REMOVE(i, sp_allh_link);
		mtx_unlock(&ipsec_accel_sav_tmp);
		NET_EPOCH_WAIT();
		if ((i->flags & (IFP_HP_HANDLED | IFP_HP_REJECTED)) ==
		    IFP_HP_HANDLED) {
			dprintf("spd deinstall %s %p\n", if_name(ifp), sp);
			error = ifp->if_ipsec_accel_m->if_spddel(ifp,
			    sp, i->ifdata);
		}
		if (error != 0) {
			dprintf(
		    "ipsec_accel_on_ifdown_sp: %s if_spddel %p res %d\n",
			    if_name(ifp), sp, error);
		}
		key_freesp(&sp);
		if_rele(ifp);
		free(i, M_IPSEC_MISC);
		mtx_lock(&ipsec_accel_sav_tmp);
	}
	CK_LIST_REMOVE(marker, sp_allh_link);
	mtx_unlock(&ipsec_accel_sav_tmp);
	free(marker, M_IPSEC_MISC);
}

static void
ipsec_accel_on_ifdown_impl(struct ifnet *ifp)
{
	ipsec_accel_on_ifdown_sp(ifp);
	ipsec_accel_on_ifdown_sav(ifp);
}

static void
ipsec_accel_ifdetach_event(void *arg __unused, struct ifnet *ifp)
{
	if ((ifp->if_flags & IFF_RENAMING) != 0)
		return;
	ipsec_accel_on_ifdown_impl(ifp);
}

static bool
ipsec_accel_output_pad(struct mbuf *m, struct secasvar *sav, int skip, int mtu)
{
	int alen, blks, hlen, padding, rlen;

	rlen = m->m_pkthdr.len - skip;
	hlen = ((sav->flags & SADB_X_EXT_OLD) != 0 ? sizeof(struct esp) :
	    sizeof(struct newesp)) + sav->ivlen;
	blks = MAX(4, SAV_ISCTR(sav) && VNET(esp_ctr_compatibility) ?
	    sav->tdb_encalgxform->native_blocksize :
	    sav->tdb_encalgxform->blocksize);
	padding = ((blks - ((rlen + 2) % blks)) % blks) + 2;
	alen = xform_ah_authsize(sav->tdb_authalgxform);

	return (skip + hlen + rlen + padding + alen <= mtu);
}

static bool
ipsec_accel_output_tag(struct mbuf *m, u_int drv_spi)
{
	struct ipsec_accel_out_tag *tag;

	tag = (struct ipsec_accel_out_tag *)m_tag_get(
	    PACKET_TAG_IPSEC_ACCEL_OUT, sizeof(*tag), M_NOWAIT);
	if (tag == NULL)
		return (false);
	tag->drv_spi = drv_spi;
	m_tag_prepend(m, &tag->tag);
	return (true);
}

bool
ipsec_accel_output(struct ifnet *ifp, struct mbuf *m, struct inpcb *inp,
    struct secpolicy *sp, struct secasvar *sav, int af, int mtu, int *hwassist)
{
	struct ifp_handle_sav *i;
	struct ip *ip;
	struct tcpcb *tp;
	u_long ip_len, skip;
	bool res;

	*hwassist = 0;
	res = false;
	if (ifp == NULL)
		return (res);

	M_ASSERTPKTHDR(m);
	NET_EPOCH_ASSERT();

	if (sav == NULL) {
		res = ipsec_accel_output_tag(m, IPSEC_ACCEL_DRV_SPI_BYPASS);
		goto out;
	}

	i = ipsec_accel_is_accel_sav_ptr(sav, ifp);
	if (i == NULL || (i->flags & (IFP_HS_HANDLED | IFP_HS_REJECTED)) !=
	    IFP_HS_HANDLED)
		goto out;

	if ((m->m_pkthdr.csum_flags & CSUM_TSO) == 0) {
		ip_len = m->m_pkthdr.len;
		if (ip_len + i->hdr_ext_size > mtu)
			goto out;
		switch (af) {
		case AF_INET:
			ip = mtod(m, struct ip *);
			skip = ip->ip_hl << 2;
			break;
		case AF_INET6:
			skip = sizeof(struct ip6_hdr);
			break;
		default:
			__unreachable();
		}
		if (!ipsec_accel_output_pad(m, sav, skip, mtu))
			goto out;
	}

	if (!ipsec_accel_output_tag(m, i->drv_spi))
		goto out;

	ipsec_accel_sa_recordxfer(sav, m);
	key_freesav(&sav);
	if (sp != NULL)
		key_freesp(&sp);

	*hwassist = ifp->if_ipsec_accel_m->if_hwassist(ifp, sav,
	    i->drv_spi, i->ifdata);
	res = true;
out:
	if (inp != NULL && inp->inp_pcbinfo == &V_tcbinfo) {
		INP_WLOCK_ASSERT(inp);
		tp = (struct tcpcb *)inp;
		if (res && (*hwassist & (CSUM_TSO | CSUM_IP6_TSO)) != 0) {
			tp->t_flags2 |= TF2_IPSEC_TSO;
		} else {
			tp->t_flags2 &= ~TF2_IPSEC_TSO;
		}
	}
	return (res);
}

struct ipsec_accel_in_tag *
ipsec_accel_input_tag_lookup(const struct mbuf *m)
{
	struct ipsec_accel_in_tag *tag;
	struct m_tag *xtag;

	xtag = m_tag_find(__DECONST(struct mbuf *, m),
	    PACKET_TAG_IPSEC_ACCEL_IN, NULL);
	if (xtag == NULL)
		return (NULL);
	tag = __containerof(xtag, struct ipsec_accel_in_tag, tag);
	return (tag);
}

int
ipsec_accel_input(struct mbuf *m, int offset, int proto)
{
	struct secasvar *sav;
	struct ipsec_accel_in_tag *tag;

	tag = ipsec_accel_input_tag_lookup(m);
	if (tag == NULL)
		return (ENXIO);

	if (tag->drv_spi < IPSEC_ACCEL_DRV_SPI_MIN ||
	    tag->drv_spi > IPSEC_ACCEL_DRV_SPI_MAX) {
		dprintf("if %s mbuf %p drv_spi %d invalid, packet dropped\n",
		    (m->m_flags & M_PKTHDR) != 0 ? if_name(m->m_pkthdr.rcvif) :
		    "<unknwn>", m, tag->drv_spi);
		m_freem(m);
		return (EINPROGRESS);
	}

	sav = ipsec_accel_drvspi_to_sa(tag->drv_spi);
	if (sav != NULL)
		ipsec_accel_sa_recordxfer(sav, m);
	return (0);
}

static void
ipsec_accel_sa_recordxfer(struct secasvar *sav, struct mbuf *m)
{
	counter_u64_add(sav->accel_lft_sw, 1);
	counter_u64_add(sav->accel_lft_sw + 1, m->m_pkthdr.len);
	if (sav->accel_firstused == 0)
		sav->accel_firstused = time_second;
}

static void
ipsec_accel_sa_lifetime_update(struct seclifetime *lft_c,
    const struct seclifetime *lft_l)
{
	lft_c->allocations += lft_l->allocations;
	lft_c->bytes += lft_l->bytes;
	lft_c->usetime = min(lft_c->usetime, lft_l->usetime);
}

static void
ipsec_accel_drv_sa_lifetime_update_impl(struct secasvar *sav, if_t ifp,
    u_int drv_spi, uint64_t octets, uint64_t allocs)
{
	struct epoch_tracker et;
	struct ifp_handle_sav *i;
	uint64_t odiff, adiff;

	NET_EPOCH_ENTER(et);
	mtx_lock(&ipsec_accel_cnt_lock);

	if (allocs != 0) {
		if (sav->firstused == 0)
			sav->firstused = time_second;
		if (sav->accel_firstused == 0)
			sav->accel_firstused = time_second;
	}

	CK_LIST_FOREACH(i, &sav->accel_ifps, sav_link) {
		if (i->ifp == ifp && i->drv_spi == drv_spi)
			break;
	}
	if (i == NULL)
		goto out;

	odiff = octets - i->cnt_octets;
	adiff = allocs - i->cnt_allocs;

	if (sav->lft_c != NULL) {
		counter_u64_add(sav->lft_c_bytes, odiff);
		counter_u64_add(sav->lft_c_allocations, adiff);
	}

	i->cnt_octets = octets;
	i->cnt_allocs = allocs;
	sav->accel_hw_octets += odiff;
	sav->accel_hw_allocs += adiff;

out:
	mtx_unlock(&ipsec_accel_cnt_lock);
	NET_EPOCH_EXIT(et);
}

static int
ipsec_accel_drv_sa_lifetime_fetch_impl(struct secasvar *sav,
    if_t ifp, u_int drv_spi, uint64_t *octets, uint64_t *allocs)
{
	struct ifp_handle_sav *i;
	int error;

	NET_EPOCH_ASSERT();
	error = 0;

	mtx_lock(&ipsec_accel_cnt_lock);
	CK_LIST_FOREACH(i, &sav->accel_ifps, sav_link) {
		if (i->ifp == ifp && i->drv_spi == drv_spi) {
			*octets = i->cnt_octets;
			*allocs = i->cnt_allocs;
			break;
		}
	}
	if (i == NULL)
		error = ENOENT;
	mtx_unlock(&ipsec_accel_cnt_lock);
	return (error);
}

static void
ipsec_accel_sa_lifetime_hw(struct secasvar *sav, if_t ifp,
    struct seclifetime *lft)
{
	struct ifp_handle_sav *i;
	if_sa_cnt_fn_t p;

	IFNET_RLOCK_ASSERT();

	i = ipsec_accel_is_accel_sav_ptr(sav, ifp);
	if (i != NULL && (i->flags & (IFP_HS_HANDLED | IFP_HS_REJECTED)) ==
	    IFP_HS_HANDLED) {
		p = ifp->if_ipsec_accel_m->if_sa_cnt;
		if (p != NULL)
			p(ifp, sav, i->drv_spi, i->ifdata, lft);
	}
}

static int
ipsec_accel_sa_lifetime_op_impl(struct secasvar *sav,
    struct seclifetime *lft_c, if_t ifp, enum IF_SA_CNT_WHICH op,
    struct rm_priotracker *sahtree_trackerp)
{
	struct seclifetime lft_l, lft_s;
	struct ifp_handle_sav *i;
	if_t ifp1;
	if_sa_cnt_fn_t p;
	int error;

	error = 0;
	memset(&lft_l, 0, sizeof(lft_l));
	memset(&lft_s, 0, sizeof(lft_s));

	switch (op & ~IF_SA_CNT_UPD) {
	case IF_SA_CNT_IFP_HW_VAL:
		ipsec_accel_sa_lifetime_hw(sav, ifp, &lft_l);
		ipsec_accel_sa_lifetime_update(&lft_l, &lft_s);
		break;

	case IF_SA_CNT_TOTAL_SW_VAL:
		lft_l.allocations = (uint32_t)counter_u64_fetch(
		    sav->accel_lft_sw);
		lft_l.bytes = counter_u64_fetch(sav->accel_lft_sw + 1);
		lft_l.usetime = sav->accel_firstused;
		break;

	case IF_SA_CNT_TOTAL_HW_VAL:
		IFNET_RLOCK_ASSERT();
		CK_LIST_FOREACH(i, &sav->accel_ifps, sav_link) {
			if ((i->flags & (IFP_HS_HANDLED | IFP_HS_REJECTED)) !=
			    IFP_HS_HANDLED)
				continue;
			ifp1 = i->ifp;
			p = ifp1->if_ipsec_accel_m->if_sa_cnt;
			if (p == NULL)
				continue;
			memset(&lft_s, 0, sizeof(lft_s));
			if (sahtree_trackerp != NULL)
				ipsec_sahtree_runlock(sahtree_trackerp);
			error = p(ifp1, sav, i->drv_spi, i->ifdata, &lft_s);
			if (sahtree_trackerp != NULL)
				ipsec_sahtree_rlock(sahtree_trackerp);
			if (error == 0)
				ipsec_accel_sa_lifetime_update(&lft_l, &lft_s);
		}
		break;
	}

	if (error == 0) {
		if ((op & IF_SA_CNT_UPD) == 0)
			memset(lft_c, 0, sizeof(*lft_c));
		ipsec_accel_sa_lifetime_update(lft_c, &lft_l);
	}

	return (error);
}

static void
ipsec_accel_sync_imp(void)
{
	taskqueue_drain_all(ipsec_accel_tq);
}

static struct mbuf *
ipsec_accel_key_setaccelif_impl(struct secasvar *sav)
{
	struct mbuf *m, *m1;
	struct ifp_handle_sav *i;
	struct epoch_tracker et;

	if (sav->accel_ifname != NULL)
		return (key_setaccelif(sav->accel_ifname));

	m = m1 = NULL;

	NET_EPOCH_ENTER(et);
	CK_LIST_FOREACH(i, &sav->accel_ifps, sav_link) {
		if ((i->flags & (IFP_HS_HANDLED | IFP_HS_REJECTED)) ==
		    IFP_HS_HANDLED) {
			m1 = key_setaccelif(if_name(i->ifp));
			if (m == NULL)
				m = m1;
			else if (m1 != NULL)
				m_cat(m, m1);
		}
	}
	NET_EPOCH_EXIT(et);
	return (m);
}

#endif	/* IPSEC_OFFLOAD */
