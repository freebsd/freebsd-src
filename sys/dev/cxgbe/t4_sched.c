/*-
 * Copyright (c) 2017 Chelsio Communications, Inc.
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
#include "opt_inet6.h"
#include "opt_ratelimit.h"

#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/sbuf.h>
#include <sys/taskqueue.h>
#include <sys/sysctl.h>

#include "common/common.h"
#include "common/t4_regs.h"
#include "common/t4_regs_values.h"
#include "common/t4_msg.h"


static int
in_range(int val, int lo, int hi)
{

	return (val < 0 || (val <= hi && val >= lo));
}

static int
set_sched_class_config(struct adapter *sc, int minmax)
{
	int rc;

	if (minmax < 0)
		return (EINVAL);

	rc = begin_synchronized_op(sc, NULL, SLEEP_OK | INTR_OK, "t4sscc");
	if (rc)
		return (rc);
	rc = -t4_sched_config(sc, FW_SCHED_TYPE_PKTSCHED, minmax, 1);
	end_synchronized_op(sc, 0);

	return (rc);
}

static int
set_sched_class_params(struct adapter *sc, struct t4_sched_class_params *p,
    int sleep_ok)
{
	int rc, top_speed, fw_level, fw_mode, fw_rateunit, fw_ratemode;
	struct port_info *pi;
	struct tx_cl_rl_params *tc;
	bool check_pktsize = false;

	if (p->level == SCHED_CLASS_LEVEL_CL_RL)
		fw_level = FW_SCHED_PARAMS_LEVEL_CL_RL;
	else if (p->level == SCHED_CLASS_LEVEL_CL_WRR)
		fw_level = FW_SCHED_PARAMS_LEVEL_CL_WRR;
	else if (p->level == SCHED_CLASS_LEVEL_CH_RL)
		fw_level = FW_SCHED_PARAMS_LEVEL_CH_RL;
	else
		return (EINVAL);

	if (p->level == SCHED_CLASS_LEVEL_CL_RL) {
		if (p->mode == SCHED_CLASS_MODE_CLASS)
			fw_mode = FW_SCHED_PARAMS_MODE_CLASS;
		else if (p->mode == SCHED_CLASS_MODE_FLOW) {
			check_pktsize = true;
			fw_mode = FW_SCHED_PARAMS_MODE_FLOW;
		} else
			return (EINVAL);
	} else
		fw_mode = 0;

	/* Valid channel must always be provided. */
	if (p->channel < 0)
		return (EINVAL);
	if (!in_range(p->channel, 0, sc->chip_params->nchan - 1))
		return (ERANGE);

	pi = sc->port[sc->chan_map[p->channel]];
	if (pi == NULL)
		return (ENXIO);
	MPASS(pi->tx_chan == p->channel);
	top_speed = port_top_speed(pi) * 1000000; /* Gbps -> Kbps */

	if (p->level == SCHED_CLASS_LEVEL_CL_RL ||
	    p->level == SCHED_CLASS_LEVEL_CH_RL) {
		/*
		 * Valid rate (mode, unit and values) must be provided.
		 */

		if (p->minrate < 0)
			p->minrate = 0;
		if (p->maxrate < 0)
			return (EINVAL);

		if (p->rateunit == SCHED_CLASS_RATEUNIT_BITS) {
			fw_rateunit = FW_SCHED_PARAMS_UNIT_BITRATE;
			/* ratemode could be relative (%) or absolute. */
			if (p->ratemode == SCHED_CLASS_RATEMODE_REL) {
				fw_ratemode = FW_SCHED_PARAMS_RATE_REL;
				/* maxrate is % of port bandwidth. */
				if (!in_range(p->minrate, 0, 100) ||
				    !in_range(p->maxrate, 0, 100)) {
					return (ERANGE);
				}
			} else if (p->ratemode == SCHED_CLASS_RATEMODE_ABS) {
				fw_ratemode = FW_SCHED_PARAMS_RATE_ABS;
				/* maxrate is absolute value in kbps. */
				if (!in_range(p->minrate, 0, top_speed) ||
				    !in_range(p->maxrate, 0, top_speed)) {
					return (ERANGE);
				}
			} else
				return (EINVAL);
		} else if (p->rateunit == SCHED_CLASS_RATEUNIT_PKTS) {
			/* maxrate is the absolute value in pps. */
			check_pktsize = true;
			fw_rateunit = FW_SCHED_PARAMS_UNIT_PKTRATE;
		} else
			return (EINVAL);
	} else {
		MPASS(p->level == SCHED_CLASS_LEVEL_CL_WRR);

		/*
		 * Valid weight must be provided.
		 */
		if (p->weight < 0)
		       return (EINVAL);
		if (!in_range(p->weight, 1, 99))
			return (ERANGE);

		fw_rateunit = 0;
		fw_ratemode = 0;
	}

	if (p->level == SCHED_CLASS_LEVEL_CL_RL ||
	    p->level == SCHED_CLASS_LEVEL_CL_WRR) {
		/*
		 * Valid scheduling class must be provided.
		 */
		if (p->cl < 0)
			return (EINVAL);
		if (!in_range(p->cl, 0, sc->chip_params->nsched_cls - 1))
			return (ERANGE);
	}

	if (check_pktsize) {
		if (p->pktsize < 0)
			return (EINVAL);
		if (!in_range(p->pktsize, 64, pi->vi[0].ifp->if_mtu))
			return (ERANGE);
	}

	rc = begin_synchronized_op(sc, NULL,
	    sleep_ok ? (SLEEP_OK | INTR_OK) : HOLD_LOCK, "t4sscp");
	if (rc)
		return (rc);
	if (p->level == SCHED_CLASS_LEVEL_CL_RL) {
		tc = &pi->sched_params->cl_rl[p->cl];
		if (tc->refcount > 0) {
			rc = EBUSY;
			goto done;
		} else {
			tc->ratemode = fw_ratemode;
			tc->rateunit = fw_rateunit;
			tc->mode = fw_mode;
			tc->maxrate = p->maxrate;
			tc->pktsize = p->pktsize;
		}
	}
	rc = -t4_sched_params(sc, FW_SCHED_TYPE_PKTSCHED, fw_level, fw_mode,
	    fw_rateunit, fw_ratemode, p->channel, p->cl, p->minrate, p->maxrate,
	    p->weight, p->pktsize, sleep_ok);
	if (p->level == SCHED_CLASS_LEVEL_CL_RL && rc != 0) {
		/*
		 * Unknown state at this point, see parameters in tc for what
		 * was attempted.
		 */
		tc->flags |= TX_CLRL_ERROR;
	}
done:
	end_synchronized_op(sc, sleep_ok ? 0 : LOCK_HELD);

	return (rc);
}

static void
update_tx_sched(void *context, int pending)
{
	int i, j, mode, rateunit, ratemode, maxrate, pktsize, rc;
	struct port_info *pi;
	struct tx_cl_rl_params *tc;
	struct adapter *sc = context;
	const int n = sc->chip_params->nsched_cls;

	mtx_lock(&sc->tc_lock);
	for_each_port(sc, i) {
		pi = sc->port[i];
		tc = &pi->sched_params->cl_rl[0];
		for (j = 0; j < n; j++, tc++) {
			MPASS(mtx_owned(&sc->tc_lock));
			if ((tc->flags & TX_CLRL_REFRESH) == 0)
				continue;

			mode = tc->mode;
			rateunit = tc->rateunit;
			ratemode = tc->ratemode;
			maxrate = tc->maxrate;
			pktsize = tc->pktsize;
			mtx_unlock(&sc->tc_lock);

			if (begin_synchronized_op(sc, NULL, SLEEP_OK | INTR_OK,
			    "t4utxs") != 0) {
				mtx_lock(&sc->tc_lock);
				continue;
			}
			rc = t4_sched_params(sc, FW_SCHED_TYPE_PKTSCHED,
			    FW_SCHED_PARAMS_LEVEL_CL_RL, mode, rateunit,
			    ratemode, pi->tx_chan, j, 0, maxrate, 0, pktsize,
			    1);
			end_synchronized_op(sc, 0);

			mtx_lock(&sc->tc_lock);
			if (rc != 0) {
				tc->flags |= TX_CLRL_ERROR;
			} else if (tc->mode == mode &&
			    tc->rateunit == rateunit &&
			    tc->maxrate == maxrate &&
			    tc->pktsize == tc->pktsize) {
				tc->flags &= ~(TX_CLRL_REFRESH | TX_CLRL_ERROR);
			}
		}
	}
	mtx_unlock(&sc->tc_lock);
}

int
t4_set_sched_class(struct adapter *sc, struct t4_sched_params *p)
{

	if (p->type != SCHED_CLASS_TYPE_PACKET)
		return (EINVAL);

	if (p->subcmd == SCHED_CLASS_SUBCMD_CONFIG)
		return (set_sched_class_config(sc, p->u.config.minmax));

	if (p->subcmd == SCHED_CLASS_SUBCMD_PARAMS)
		return (set_sched_class_params(sc, &p->u.params, 1));

	return (EINVAL);
}

int
t4_set_sched_queue(struct adapter *sc, struct t4_sched_queue *p)
{
	struct port_info *pi = NULL;
	struct vi_info *vi;
	struct sge_txq *txq;
	uint32_t fw_mnem, fw_queue, fw_class;
	int i, rc;

	rc = begin_synchronized_op(sc, NULL, SLEEP_OK | INTR_OK, "t4setsq");
	if (rc)
		return (rc);

	if (p->port >= sc->params.nports) {
		rc = EINVAL;
		goto done;
	}

	/* XXX: Only supported for the main VI. */
	pi = sc->port[p->port];
	vi = &pi->vi[0];
	if (!(vi->flags & VI_INIT_DONE)) {
		/* tx queues not set up yet */
		rc = EAGAIN;
		goto done;
	}

	if (!in_range(p->queue, 0, vi->ntxq - 1) ||
	    !in_range(p->cl, 0, sc->chip_params->nsched_cls - 1)) {
		rc = EINVAL;
		goto done;
	}

	/*
	 * Create a template for the FW_PARAMS_CMD mnemonic and value (TX
	 * Scheduling Class in this case).
	 */
	fw_mnem = (V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_DMAQ) |
	    V_FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_DMAQ_EQ_SCHEDCLASS_ETH));
	fw_class = p->cl < 0 ? 0xffffffff : p->cl;

	/*
	 * If op.queue is non-negative, then we're only changing the scheduling
	 * on a single specified TX queue.
	 */
	if (p->queue >= 0) {
		txq = &sc->sge.txq[vi->first_txq + p->queue];
		fw_queue = (fw_mnem | V_FW_PARAMS_PARAM_YZ(txq->eq.cntxt_id));
		rc = -t4_set_params(sc, sc->mbox, sc->pf, 0, 1, &fw_queue,
		    &fw_class);
		goto done;
	}

	/*
	 * Change the scheduling on all the TX queues for the
	 * interface.
	 */
	for_each_txq(vi, i, txq) {
		fw_queue = (fw_mnem | V_FW_PARAMS_PARAM_YZ(txq->eq.cntxt_id));
		rc = -t4_set_params(sc, sc->mbox, sc->pf, 0, 1, &fw_queue,
		    &fw_class);
		if (rc)
			goto done;
	}

	rc = 0;
done:
	end_synchronized_op(sc, 0);
	return (rc);
}

int
t4_init_tx_sched(struct adapter *sc)
{
	int i, j;
	const int n = sc->chip_params->nsched_cls;
	struct port_info *pi;
	struct tx_cl_rl_params *tc;

	mtx_init(&sc->tc_lock, "tx_sched lock", NULL, MTX_DEF);
	TASK_INIT(&sc->tc_task, 0, update_tx_sched, sc);
	for_each_port(sc, i) {
		pi = sc->port[i];
		pi->sched_params = malloc(sizeof(*pi->sched_params) +
		    n * sizeof(*tc), M_CXGBE, M_ZERO | M_WAITOK);
		tc = &pi->sched_params->cl_rl[0];
		for (j = 0; j < n; j++, tc++) {
			tc->refcount = 0;
			tc->ratemode = FW_SCHED_PARAMS_RATE_ABS;
			tc->rateunit = FW_SCHED_PARAMS_UNIT_BITRATE;
			tc->mode = FW_SCHED_PARAMS_MODE_CLASS;
			tc->maxrate = 1000 * 1000;	/* 1 Gbps.  Arbitrary */

			if (t4_sched_params_cl_rl_kbps(sc, pi->tx_chan, j,
			    tc->mode, tc->maxrate, tc->pktsize, 1) == 0)
				tc->flags = 0;
			else
				tc->flags = TX_CLRL_ERROR;
		}
	}

	return (0);
}

int
t4_free_tx_sched(struct adapter *sc)
{
	int i;

	taskqueue_drain(taskqueue_thread, &sc->tc_task);

	for_each_port(sc, i) {
		if (sc->port[i] != NULL)
			free(sc->port[i]->sched_params, M_CXGBE);
	}

	if (mtx_initialized(&sc->tc_lock))
		mtx_destroy(&sc->tc_lock);

	return (0);
}

void
t4_update_tx_sched(struct adapter *sc)
{

	taskqueue_enqueue(taskqueue_thread, &sc->tc_task);
}

int
t4_reserve_cl_rl_kbps(struct adapter *sc, int port_id, u_int maxrate,
    int *tc_idx)
{
	int rc = 0, fa = -1, i;
	struct tx_cl_rl_params *tc;

	MPASS(port_id >= 0 && port_id < sc->params.nports);

	tc = &sc->port[port_id]->sched_params->cl_rl[0];
	mtx_lock(&sc->tc_lock);
	for (i = 0; i < sc->chip_params->nsched_cls; i++, tc++) {
		if (fa < 0 && tc->refcount == 0)
			fa = i;

		if (tc->ratemode == FW_SCHED_PARAMS_RATE_ABS &&
		    tc->rateunit == FW_SCHED_PARAMS_UNIT_BITRATE &&
		    tc->mode == FW_SCHED_PARAMS_MODE_FLOW &&
		    tc->maxrate == maxrate) {
			tc->refcount++;
			*tc_idx = i;
			goto done;
		}
	}
	/* Not found */
	MPASS(i == sc->chip_params->nsched_cls);
	if (fa != -1) {
		tc = &sc->port[port_id]->sched_params->cl_rl[fa];
		tc->flags = TX_CLRL_REFRESH;
		tc->refcount = 1;
		tc->ratemode = FW_SCHED_PARAMS_RATE_ABS;
		tc->rateunit = FW_SCHED_PARAMS_UNIT_BITRATE;
		tc->mode = FW_SCHED_PARAMS_MODE_FLOW;
		tc->maxrate = maxrate;
		tc->pktsize = ETHERMTU;	/* XXX */
		*tc_idx = fa;
		t4_update_tx_sched(sc);
	} else {
		*tc_idx = -1;
		rc = ENOSPC;
	}
done:
	mtx_unlock(&sc->tc_lock);
	return (rc);
}

void
t4_release_cl_rl_kbps(struct adapter *sc, int port_id, int tc_idx)
{
	struct tx_cl_rl_params *tc;

	MPASS(port_id >= 0 && port_id < sc->params.nports);
	MPASS(tc_idx >= 0 && tc_idx < sc->chip_params->nsched_cls);

	mtx_lock(&sc->tc_lock);
	tc = &sc->port[port_id]->sched_params->cl_rl[tc_idx];
	MPASS(tc->refcount > 0);
	MPASS(tc->ratemode == FW_SCHED_PARAMS_RATE_ABS);
	MPASS(tc->rateunit == FW_SCHED_PARAMS_UNIT_BITRATE);
	MPASS(tc->mode == FW_SCHED_PARAMS_MODE_FLOW);
	tc->refcount--;
	mtx_unlock(&sc->tc_lock);
}

#ifdef RATELIMIT
void
t4_init_etid_table(struct adapter *sc)
{
	int i;
	struct tid_info *t;

	if (!is_ethoffload(sc))
		return;

	t = &sc->tids;
	MPASS(t->netids > 0);

	mtx_init(&t->etid_lock, "etid lock", NULL, MTX_DEF);
	t->etid_tab = malloc(sizeof(*t->etid_tab) * t->netids, M_CXGBE,
			M_ZERO | M_WAITOK);
	t->efree = t->etid_tab;
	t->etids_in_use = 0;
	for (i = 1; i < t->netids; i++)
		t->etid_tab[i - 1].next = &t->etid_tab[i];
	t->etid_tab[t->netids - 1].next = NULL;
}

void
t4_free_etid_table(struct adapter *sc)
{
	struct tid_info *t;

	if (!is_ethoffload(sc))
		return;

	t = &sc->tids;
	MPASS(t->netids > 0);

	free(t->etid_tab, M_CXGBE);
	t->etid_tab = NULL;

	if (mtx_initialized(&t->etid_lock))
		mtx_destroy(&t->etid_lock);
}

/* etid services */
static int alloc_etid(struct adapter *, struct cxgbe_snd_tag *);
static void free_etid(struct adapter *, int);

static int
alloc_etid(struct adapter *sc, struct cxgbe_snd_tag *cst)
{
	struct tid_info *t = &sc->tids;
	int etid = -1;

	mtx_lock(&t->etid_lock);
	if (t->efree) {
		union etid_entry *p = t->efree;

		etid = p - t->etid_tab + t->etid_base;
		t->efree = p->next;
		p->cst = cst;
		t->etids_in_use++;
	}
	mtx_unlock(&t->etid_lock);
	return (etid);
}

struct cxgbe_snd_tag *
lookup_etid(struct adapter *sc, int etid)
{
	struct tid_info *t = &sc->tids;

	return (t->etid_tab[etid - t->etid_base].cst);
}

static void
free_etid(struct adapter *sc, int etid)
{
	struct tid_info *t = &sc->tids;
	union etid_entry *p = &t->etid_tab[etid - t->etid_base];

	mtx_lock(&t->etid_lock);
	p->next = t->efree;
	t->efree = p;
	t->etids_in_use--;
	mtx_unlock(&t->etid_lock);
}

int
cxgbe_snd_tag_alloc(struct ifnet *ifp, union if_snd_tag_alloc_params *params,
    struct m_snd_tag **pt)
{
	int rc, schedcl;
	struct vi_info *vi = ifp->if_softc;
	struct port_info *pi = vi->pi;
	struct adapter *sc = pi->adapter;
	struct cxgbe_snd_tag *cst;

	if (params->hdr.type != IF_SND_TAG_TYPE_RATE_LIMIT)
		return (ENOTSUP);

	rc = t4_reserve_cl_rl_kbps(sc, pi->port_id,
	    (params->rate_limit.max_rate * 8ULL / 1000), &schedcl);
	if (rc != 0)
		return (rc);
	MPASS(schedcl >= 0 && schedcl < sc->chip_params->nsched_cls);

	cst = malloc(sizeof(*cst), M_CXGBE, M_ZERO | M_NOWAIT);
	if (cst == NULL) {
failed:
		t4_release_cl_rl_kbps(sc, pi->port_id, schedcl);
		return (ENOMEM);
	}

	cst->etid = alloc_etid(sc, cst);
	if (cst->etid < 0) {
		free(cst, M_CXGBE);
		goto failed;
	}

	mtx_init(&cst->lock, "cst_lock", NULL, MTX_DEF);
	mbufq_init(&cst->pending_tx, INT_MAX);
	mbufq_init(&cst->pending_fwack, INT_MAX);
	cst->com.ifp = ifp;
	cst->flags |= EO_FLOWC_PENDING | EO_SND_TAG_REF;
	cst->adapter = sc;
	cst->port_id = pi->port_id;
	cst->schedcl = schedcl;
	cst->max_rate = params->rate_limit.max_rate;
	cst->tx_credits = sc->params.ofldq_wr_cred;
	cst->tx_total = cst->tx_credits;
	cst->plen = 0;
	cst->ctrl0 = htobe32(V_TXPKT_OPCODE(CPL_TX_PKT) |
	    V_TXPKT_INTF(pi->tx_chan) | V_TXPKT_PF(G_FW_VIID_PFN(vi->viid)) |
	    V_TXPKT_VF(G_FW_VIID_VIN(vi->viid)) |
	    V_TXPKT_VF_VLD(G_FW_VIID_VIVLD(vi->viid)));

	/*
	 * Queues will be selected later when the connection flowid is available.
	 */

	*pt = &cst->com;
	return (0);
}

/*
 * Change in parameters, no change in ifp.
 */
int
cxgbe_snd_tag_modify(struct m_snd_tag *mst,
    union if_snd_tag_modify_params *params)
{
	int rc, schedcl;
	struct cxgbe_snd_tag *cst = mst_to_cst(mst);
	struct adapter *sc = cst->adapter;

	/* XXX: is schedcl -1 ok here? */
	MPASS(cst->schedcl >= 0 && cst->schedcl < sc->chip_params->nsched_cls);

	mtx_lock(&cst->lock);
	MPASS(cst->flags & EO_SND_TAG_REF);
	rc = t4_reserve_cl_rl_kbps(sc, cst->port_id,
	    (params->rate_limit.max_rate * 8ULL / 1000), &schedcl);
	if (rc != 0)
		return (rc);
	MPASS(schedcl >= 0 && schedcl < sc->chip_params->nsched_cls);
	t4_release_cl_rl_kbps(sc, cst->port_id, cst->schedcl);
	cst->schedcl = schedcl;
	cst->max_rate = params->rate_limit.max_rate;
	mtx_unlock(&cst->lock);

	return (0);
}

int
cxgbe_snd_tag_query(struct m_snd_tag *mst,
    union if_snd_tag_query_params *params)
{
	struct cxgbe_snd_tag *cst = mst_to_cst(mst);

	params->rate_limit.max_rate = cst->max_rate;

#define CST_TO_MST_QLEVEL_SCALE (IF_SND_QUEUE_LEVEL_MAX / cst->tx_total)
	params->rate_limit.queue_level =
		(cst->tx_total - cst->tx_credits) * CST_TO_MST_QLEVEL_SCALE;

	return (0);
}

/*
 * Unlocks cst and frees it.
 */
void
cxgbe_snd_tag_free_locked(struct cxgbe_snd_tag *cst)
{
	struct adapter *sc = cst->adapter;

	mtx_assert(&cst->lock, MA_OWNED);
	MPASS((cst->flags & EO_SND_TAG_REF) == 0);
	MPASS(cst->tx_credits == cst->tx_total);
	MPASS(cst->plen == 0);
	MPASS(mbufq_first(&cst->pending_tx) == NULL);
	MPASS(mbufq_first(&cst->pending_fwack) == NULL);

	if (cst->etid >= 0)
		free_etid(sc, cst->etid);
	if (cst->schedcl != -1)
		t4_release_cl_rl_kbps(sc, cst->port_id, cst->schedcl);
	mtx_unlock(&cst->lock);
	mtx_destroy(&cst->lock);
	free(cst, M_CXGBE);
}

void
cxgbe_snd_tag_free(struct m_snd_tag *mst)
{
	struct cxgbe_snd_tag *cst = mst_to_cst(mst);

	mtx_lock(&cst->lock);

	/* The kernel is done with the snd_tag.  Remove its reference. */
	MPASS(cst->flags & EO_SND_TAG_REF);
	cst->flags &= ~EO_SND_TAG_REF;

	if (cst->ncompl == 0) {
		/*
		 * No fw4_ack in flight.  Free the tag right away if there are
		 * no outstanding credits.  Request the firmware to return all
		 * credits for the etid otherwise.
		 */
		if (cst->tx_credits == cst->tx_total) {
			cxgbe_snd_tag_free_locked(cst);
			return;	/* cst is gone. */
		}
		send_etid_flush_wr(cst);
	}
	mtx_unlock(&cst->lock);
}
#endif
