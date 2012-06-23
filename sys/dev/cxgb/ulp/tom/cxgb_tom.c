/*-
 * Copyright (c) 2012 Chelsio Communications, Inc.
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
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/taskqueue.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/toecore.h>

#ifdef TCP_OFFLOAD
#include "cxgb_include.h"
#include "ulp/tom/cxgb_tom.h"
#include "ulp/tom/cxgb_l2t.h"
#include "ulp/tom/cxgb_toepcb.h"

MALLOC_DEFINE(M_CXGB, "cxgb", "Chelsio T3 Offload services");

/* Module ops */
static int t3_tom_mod_load(void);
static int t3_tom_mod_unload(void);
static int t3_tom_modevent(module_t, int, void *);

/* ULD ops and helpers */
static int t3_tom_activate(struct adapter *);
static int t3_tom_deactivate(struct adapter *);

static int alloc_tid_tabs(struct tid_info *, u_int, u_int, u_int, u_int, u_int);
static void free_tid_tabs(struct tid_info *);
static int write_smt_entry(struct adapter *, int);
static void free_tom_data(struct tom_data *);

static struct uld_info tom_uld_info = {
	.uld_id = ULD_TOM,
	.activate = t3_tom_activate,
	.deactivate = t3_tom_deactivate,
};

struct toepcb *
toepcb_alloc(struct toedev *tod)
{
	struct toepcb *toep;

	toep = malloc(sizeof(struct toepcb), M_CXGB, M_NOWAIT | M_ZERO);
	if (toep == NULL)
		return (NULL);

	toep->tp_tod = tod;
	toep->tp_wr_max = toep->tp_wr_avail = 15;
	toep->tp_wr_unacked = 0;
	toep->tp_delack_mode = 0;

	return (toep);
}

void
toepcb_free(struct toepcb *toep)
{
	free(toep, M_CXGB);
}

static int
alloc_tid_tabs(struct tid_info *t, u_int ntids, u_int natids, u_int nstids,
    u_int atid_base, u_int stid_base)
{
	unsigned long size = ntids * sizeof(*t->tid_tab) +
	    natids * sizeof(*t->atid_tab) + nstids * sizeof(*t->stid_tab);

	t->tid_tab = malloc(size, M_CXGB, M_NOWAIT | M_ZERO);
	if (!t->tid_tab)
		return (ENOMEM);

	t->stid_tab = (union listen_entry *)&t->tid_tab[ntids];
	t->atid_tab = (union active_open_entry *)&t->stid_tab[nstids];
	t->ntids = ntids;
	t->nstids = nstids;
	t->stid_base = stid_base;
	t->sfree = NULL;
	t->natids = natids;
	t->atid_base = atid_base;
	t->afree = NULL;
	t->stids_in_use = t->atids_in_use = 0;
	t->tids_in_use = 0;
	mtx_init(&t->stid_lock, "stid", NULL, MTX_DEF);
	mtx_init(&t->atid_lock, "atid", NULL, MTX_DEF);

	/*
	 * Setup the free lists for stid_tab and atid_tab.
	 */
	if (nstids) {
		while (--nstids)
			t->stid_tab[nstids - 1].next = &t->stid_tab[nstids];
		t->sfree = t->stid_tab;
	}
	if (natids) {
		while (--natids)
			t->atid_tab[natids - 1].next = &t->atid_tab[natids];
		t->afree = t->atid_tab;
	}
	return (0);
}

static void
free_tid_tabs(struct tid_info *t)
{
	if (mtx_initialized(&t->stid_lock))
		mtx_destroy(&t->stid_lock);
	if (mtx_initialized(&t->atid_lock))
		mtx_destroy(&t->atid_lock);
	free(t->tid_tab, M_CXGB);
}

static int
write_smt_entry(struct adapter *sc, int idx)
{
	struct port_info *pi = &sc->port[idx];
	struct cpl_smt_write_req *req;
	struct mbuf *m;

	m = M_GETHDR_OFLD(0, CPL_PRIORITY_CONTROL, req);
	if (m == NULL) {
		log(LOG_ERR, "%s: no mbuf, can't write SMT entry for %d\n",
		    __func__, idx);
		return (ENOMEM);
	}

	req->wr.wrh_hi = htonl(V_WR_OP(FW_WROPCODE_FORWARD));
	OPCODE_TID(req) = htonl(MK_OPCODE_TID(CPL_SMT_WRITE_REQ, idx));
	req->mtu_idx = NMTUS - 1;  /* should be 0 but there's a T3 bug */
	req->iff = idx;
	memset(req->src_mac1, 0, sizeof(req->src_mac1));
	memcpy(req->src_mac0, pi->hw_addr, ETHER_ADDR_LEN);

	t3_offload_tx(sc, m);

	return (0);
}

static void
free_tom_data(struct tom_data *td)
{
	KASSERT(TAILQ_EMPTY(&td->toep_list),
	    ("%s: toep_list not empty", __func__));

	if (td->listen_mask != 0)
		hashdestroy(td->listen_hash, M_CXGB, td->listen_mask);

	if (mtx_initialized(&td->toep_list_lock))
		mtx_destroy(&td->toep_list_lock);
	if (mtx_initialized(&td->lctx_hash_lock))
		mtx_destroy(&td->lctx_hash_lock);
	if (mtx_initialized(&td->tid_release_lock))
		mtx_destroy(&td->tid_release_lock);
	if (td->l2t)
		t3_free_l2t(td->l2t);
	free_tid_tabs(&td->tid_maps);
	free(td, M_CXGB);
}

/*
 * Ground control to Major TOM
 * Commencing countdown, engines on
 */
static int
t3_tom_activate(struct adapter *sc)
{
	struct tom_data *td;
	struct toedev *tod;
	int i, rc = 0;
	struct mc5_params *mc5 = &sc->params.mc5;
	u_int ntids, natids, mtus;

	ADAPTER_LOCK_ASSERT_OWNED(sc);	/* for sc->flags */

	/* per-adapter softc for TOM */
	td = malloc(sizeof(*td), M_CXGB, M_ZERO | M_NOWAIT);
	if (td == NULL)
		return (ENOMEM);

	/* List of TOE PCBs and associated lock */
	mtx_init(&td->toep_list_lock, "PCB list lock", NULL, MTX_DEF);
	TAILQ_INIT(&td->toep_list);

	/* Listen context */
	mtx_init(&td->lctx_hash_lock, "lctx hash lock", NULL, MTX_DEF);
	td->listen_hash = hashinit_flags(LISTEN_HASH_SIZE, M_CXGB,
	    &td->listen_mask, HASH_NOWAIT);

	/* TID release task */
	TASK_INIT(&td->tid_release_task, 0 , t3_process_tid_release_list, td);
	mtx_init(&td->tid_release_lock, "tid release", NULL, MTX_DEF);

	/* L2 table */
	td->l2t = t3_init_l2t(L2T_SIZE);
	if (td->l2t == NULL) {
		rc = ENOMEM;
		goto done;
	}

	/* TID tables */
	ntids = t3_mc5_size(&sc->mc5) - mc5->nroutes - mc5->nfilters -
	    mc5->nservers;
	natids = min(ntids / 2, 64 * 1024);
	rc = alloc_tid_tabs(&td->tid_maps, ntids, natids, mc5->nservers,
	    0x100000 /* ATID_BASE */, ntids);
	if (rc != 0)
		goto done;

	/* CPL handlers */
	t3_init_listen_cpl_handlers(sc);
	t3_init_l2t_cpl_handlers(sc);
	t3_init_cpl_io(sc);

	/* toedev ops */
	tod = &td->tod;
	init_toedev(tod);
	tod->tod_softc = sc;
	tod->tod_connect = t3_connect;
	tod->tod_listen_start = t3_listen_start;
	tod->tod_listen_stop = t3_listen_stop;
	tod->tod_rcvd = t3_rcvd;
	tod->tod_output = t3_tod_output;
	tod->tod_send_rst = t3_send_rst;
	tod->tod_send_fin = t3_send_fin;
	tod->tod_pcb_detach = t3_pcb_detach;
	tod->tod_l2_update = t3_l2_update;
	tod->tod_syncache_added = t3_syncache_added;
	tod->tod_syncache_removed = t3_syncache_removed;
	tod->tod_syncache_respond = t3_syncache_respond;
	tod->tod_offload_socket = t3_offload_socket;

	/* port MTUs */
	mtus = sc->port[0].ifp->if_mtu;
	if (sc->params.nports > 1)
		mtus |= sc->port[1].ifp->if_mtu << 16;
	t3_write_reg(sc, A_TP_MTU_PORT_TABLE, mtus);
	t3_load_mtus(sc, sc->params.mtus, sc->params.a_wnd, sc->params.b_wnd,
	    sc->params.rev == 0 ? sc->port[0].ifp->if_mtu : 0xffff);

	/* SMT entry for each port */
	for_each_port(sc, i) {
		write_smt_entry(sc, i);
		TOEDEV(sc->port[i].ifp) = &td->tod;
	}

	/* Switch TP to offload mode */
	t3_tp_set_offload_mode(sc, 1);

	sc->tom_softc = td;
	sc->flags |= TOM_INIT_DONE;
	register_toedev(tod);

done:
	if (rc != 0)
		free_tom_data(td);

	return (rc);
}

static int
t3_tom_deactivate(struct adapter *sc)
{
	int rc = 0;
	struct tom_data *td = sc->tom_softc;

	ADAPTER_LOCK_ASSERT_OWNED(sc);	/* for sc->flags */

	if (td == NULL)
		return (0);	/* XXX. KASSERT? */

	if (sc->offload_map != 0)
		return (EBUSY);	/* at least one port has IFCAP_TOE enabled */

	mtx_lock(&td->toep_list_lock);
	if (!TAILQ_EMPTY(&td->toep_list))
		rc = EBUSY;
	mtx_unlock(&td->toep_list_lock);

	mtx_lock(&td->lctx_hash_lock);
	if (td->lctx_count > 0)
		rc = EBUSY;
	mtx_unlock(&td->lctx_hash_lock);

	if (rc == 0) {
		unregister_toedev(&td->tod);
		t3_tp_set_offload_mode(sc, 0);
		free_tom_data(td);
		sc->tom_softc = NULL;
		sc->flags &= ~TOM_INIT_DONE;
	}

	return (rc);
}

static int
t3_tom_mod_load(void)
{
	int rc;

	rc = t3_register_uld(&tom_uld_info);
	if (rc != 0)
		t3_tom_mod_unload();

	return (rc);
}

static void
tom_uninit(struct adapter *sc, void *arg __unused)
{
	/* Try to free resources (works only if no port has IFCAP_TOE) */
	ADAPTER_LOCK(sc);
	if (sc->flags & TOM_INIT_DONE)
		t3_deactivate_uld(sc, ULD_TOM);
	ADAPTER_UNLOCK(sc);
}

static int
t3_tom_mod_unload(void)
{
	t3_iterate(tom_uninit, NULL);

	if (t3_unregister_uld(&tom_uld_info) == EBUSY)
		return (EBUSY);

	return (0);
}
#endif	/* ifdef TCP_OFFLOAD */

static int
t3_tom_modevent(module_t mod, int cmd, void *arg)
{
	int rc = 0;

#ifdef TCP_OFFLOAD
	switch (cmd) {
	case MOD_LOAD:
		rc = t3_tom_mod_load();
		break;

	case MOD_UNLOAD:
		rc = t3_tom_mod_unload();
		break;

	default:
		rc = EINVAL;
	}
#else
	rc = EOPNOTSUPP;
#endif
	return (rc);
}

static moduledata_t t3_tom_moddata= {
	"t3_tom",
	t3_tom_modevent,
	0
};

MODULE_VERSION(t3_tom, 1);
MODULE_DEPEND(t3_tom, toecore, 1, 1, 1);
MODULE_DEPEND(t3_tom, cxgbc, 1, 1, 1);
DECLARE_MODULE(t3_tom, t3_tom_moddata, SI_SUB_EXEC, SI_ORDER_ANY);
