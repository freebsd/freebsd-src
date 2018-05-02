/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2018 Chelsio Communications, Inc.
 * All rights reserved.
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

#include <sys/param.h>
#include <sys/eventhandler.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>
#include <sys/socket.h>
#include <sys/sbuf.h>
#include <netinet/in.h>

#include "common/common.h"
#include "common/t4_msg.h"
#include "common/t4_regs.h"
#include "t4_l2t.h"

struct filter_entry {
        uint32_t valid:1;	/* filter allocated and valid */
        uint32_t locked:1;	/* filter is administratively locked */
        uint32_t pending:1;	/* filter action is pending firmware reply */
	uint32_t smtidx:8;	/* Source MAC Table index for smac */
	struct l2t_entry *l2t;	/* Layer Two Table entry for dmac */

        struct t4_filter_specification fs;
};

static uint32_t
fconf_iconf_to_mode(uint32_t fconf, uint32_t iconf)
{
	uint32_t mode;

	mode = T4_FILTER_IPv4 | T4_FILTER_IPv6 | T4_FILTER_IP_SADDR |
	    T4_FILTER_IP_DADDR | T4_FILTER_IP_SPORT | T4_FILTER_IP_DPORT;

	if (fconf & F_FRAGMENTATION)
		mode |= T4_FILTER_IP_FRAGMENT;

	if (fconf & F_MPSHITTYPE)
		mode |= T4_FILTER_MPS_HIT_TYPE;

	if (fconf & F_MACMATCH)
		mode |= T4_FILTER_MAC_IDX;

	if (fconf & F_ETHERTYPE)
		mode |= T4_FILTER_ETH_TYPE;

	if (fconf & F_PROTOCOL)
		mode |= T4_FILTER_IP_PROTO;

	if (fconf & F_TOS)
		mode |= T4_FILTER_IP_TOS;

	if (fconf & F_VLAN)
		mode |= T4_FILTER_VLAN;

	if (fconf & F_VNIC_ID) {
		mode |= T4_FILTER_VNIC;
		if (iconf & F_VNIC)
			mode |= T4_FILTER_IC_VNIC;
	}

	if (fconf & F_PORT)
		mode |= T4_FILTER_PORT;

	if (fconf & F_FCOE)
		mode |= T4_FILTER_FCoE;

	return (mode);
}

static uint32_t
mode_to_fconf(uint32_t mode)
{
	uint32_t fconf = 0;

	if (mode & T4_FILTER_IP_FRAGMENT)
		fconf |= F_FRAGMENTATION;

	if (mode & T4_FILTER_MPS_HIT_TYPE)
		fconf |= F_MPSHITTYPE;

	if (mode & T4_FILTER_MAC_IDX)
		fconf |= F_MACMATCH;

	if (mode & T4_FILTER_ETH_TYPE)
		fconf |= F_ETHERTYPE;

	if (mode & T4_FILTER_IP_PROTO)
		fconf |= F_PROTOCOL;

	if (mode & T4_FILTER_IP_TOS)
		fconf |= F_TOS;

	if (mode & T4_FILTER_VLAN)
		fconf |= F_VLAN;

	if (mode & T4_FILTER_VNIC)
		fconf |= F_VNIC_ID;

	if (mode & T4_FILTER_PORT)
		fconf |= F_PORT;

	if (mode & T4_FILTER_FCoE)
		fconf |= F_FCOE;

	return (fconf);
}

static uint32_t
mode_to_iconf(uint32_t mode)
{

	if (mode & T4_FILTER_IC_VNIC)
		return (F_VNIC);
	return (0);
}

static int check_fspec_against_fconf_iconf(struct adapter *sc,
    struct t4_filter_specification *fs)
{
	struct tp_params *tpp = &sc->params.tp;
	uint32_t fconf = 0;

	if (fs->val.frag || fs->mask.frag)
		fconf |= F_FRAGMENTATION;

	if (fs->val.matchtype || fs->mask.matchtype)
		fconf |= F_MPSHITTYPE;

	if (fs->val.macidx || fs->mask.macidx)
		fconf |= F_MACMATCH;

	if (fs->val.ethtype || fs->mask.ethtype)
		fconf |= F_ETHERTYPE;

	if (fs->val.proto || fs->mask.proto)
		fconf |= F_PROTOCOL;

	if (fs->val.tos || fs->mask.tos)
		fconf |= F_TOS;

	if (fs->val.vlan_vld || fs->mask.vlan_vld)
		fconf |= F_VLAN;

	if (fs->val.ovlan_vld || fs->mask.ovlan_vld) {
		fconf |= F_VNIC_ID;
		if (tpp->ingress_config & F_VNIC)
			return (EINVAL);
	}

	if (fs->val.pfvf_vld || fs->mask.pfvf_vld) {
		fconf |= F_VNIC_ID;
		if ((tpp->ingress_config & F_VNIC) == 0)
			return (EINVAL);
	}

	if (fs->val.iport || fs->mask.iport)
		fconf |= F_PORT;

	if (fs->val.fcoe || fs->mask.fcoe)
		fconf |= F_FCOE;

	if ((tpp->vlan_pri_map | fconf) != tpp->vlan_pri_map)
		return (E2BIG);

	return (0);
}

int
get_filter_mode(struct adapter *sc, uint32_t *mode)
{
	struct tp_params *tpp = &sc->params.tp;

	/*
	 * We trust the cached values of the relevant TP registers.  This means
	 * things work reliably only if writes to those registers are always via
	 * t4_set_filter_mode_.
	 */
	*mode = fconf_iconf_to_mode(tpp->vlan_pri_map, tpp->ingress_config);

	return (0);
}

int
set_filter_mode(struct adapter *sc, uint32_t mode)
{
	struct tp_params *tpp = &sc->params.tp;
	uint32_t fconf, iconf;
	int rc;

	iconf = mode_to_iconf(mode);
	if ((iconf ^ tpp->ingress_config) & F_VNIC) {
		/*
		 * For now we just complain if A_TP_INGRESS_CONFIG is not
		 * already set to the correct value for the requested filter
		 * mode.  It's not clear if it's safe to write to this register
		 * on the fly.  (And we trust the cached value of the register).
		 */
		return (EBUSY);
	}

	fconf = mode_to_fconf(mode);

	rc = begin_synchronized_op(sc, NULL, HOLD_LOCK | SLEEP_OK | INTR_OK,
	    "t4setfm");
	if (rc)
		return (rc);

	if (sc->tids.ftids_in_use > 0) {
		rc = EBUSY;
		goto done;
	}

#ifdef TCP_OFFLOAD
	if (uld_active(sc, ULD_TOM)) {
		rc = EBUSY;
		goto done;
	}
#endif

	rc = -t4_set_filter_mode(sc, fconf, true);
done:
	end_synchronized_op(sc, LOCK_HELD);
	return (rc);
}

static inline uint64_t
get_filter_hits(struct adapter *sc, uint32_t fid)
{
	uint32_t tcb_addr;

	tcb_addr = t4_read_reg(sc, A_TP_CMM_TCB_BASE) +
	    (fid + sc->tids.ftid_base) * TCB_SIZE;

	if (is_t4(sc)) {
		uint64_t hits;

		read_via_memwin(sc, 0, tcb_addr + 16, (uint32_t *)&hits, 8);
		return (be64toh(hits));
	} else {
		uint32_t hits;

		read_via_memwin(sc, 0, tcb_addr + 24, &hits, 4);
		return (be32toh(hits));
	}
}

int
get_filter(struct adapter *sc, struct t4_filter *t)
{
	int i, rc, nfilters = sc->tids.nftids;
	struct filter_entry *f;

	rc = begin_synchronized_op(sc, NULL, HOLD_LOCK | SLEEP_OK | INTR_OK,
	    "t4getf");
	if (rc)
		return (rc);

	if (sc->tids.ftids_in_use == 0 || sc->tids.ftid_tab == NULL ||
	    t->idx >= nfilters) {
		t->idx = 0xffffffff;
		goto done;
	}

	f = &sc->tids.ftid_tab[t->idx];
	for (i = t->idx; i < nfilters; i++, f++) {
		if (f->valid) {
			t->idx = i;
			t->l2tidx = f->l2t ? f->l2t->idx : 0;
			t->smtidx = f->smtidx;
			if (f->fs.hitcnts)
				t->hits = get_filter_hits(sc, t->idx);
			else
				t->hits = UINT64_MAX;
			t->fs = f->fs;

			goto done;
		}
	}

	t->idx = 0xffffffff;
done:
	end_synchronized_op(sc, LOCK_HELD);
	return (0);
}

static int
set_filter_wr(struct adapter *sc, int fidx)
{
	struct filter_entry *f = &sc->tids.ftid_tab[fidx];
	struct fw_filter_wr *fwr;
	unsigned int ftid, vnic_vld, vnic_vld_mask;
	struct wrq_cookie cookie;

	ASSERT_SYNCHRONIZED_OP(sc);

	if (f->fs.newdmac || f->fs.newvlan) {
		/* This filter needs an L2T entry; allocate one. */
		f->l2t = t4_l2t_alloc_switching(sc->l2t);
		if (f->l2t == NULL)
			return (EAGAIN);
		if (t4_l2t_set_switching(sc, f->l2t, f->fs.vlan, f->fs.eport,
		    f->fs.dmac)) {
			t4_l2t_release(f->l2t);
			f->l2t = NULL;
			return (ENOMEM);
		}
	}

	/* Already validated against fconf, iconf */
	MPASS((f->fs.val.pfvf_vld & f->fs.val.ovlan_vld) == 0);
	MPASS((f->fs.mask.pfvf_vld & f->fs.mask.ovlan_vld) == 0);
	if (f->fs.val.pfvf_vld || f->fs.val.ovlan_vld)
		vnic_vld = 1;
	else
		vnic_vld = 0;
	if (f->fs.mask.pfvf_vld || f->fs.mask.ovlan_vld)
		vnic_vld_mask = 1;
	else
		vnic_vld_mask = 0;

	ftid = sc->tids.ftid_base + fidx;

	fwr = start_wrq_wr(&sc->sge.mgmtq, howmany(sizeof(*fwr), 16), &cookie);
	if (fwr == NULL)
		return (ENOMEM);
	bzero(fwr, sizeof(*fwr));

	fwr->op_pkd = htobe32(V_FW_WR_OP(FW_FILTER_WR));
	fwr->len16_pkd = htobe32(FW_LEN16(*fwr));
	fwr->tid_to_iq =
	    htobe32(V_FW_FILTER_WR_TID(ftid) |
		V_FW_FILTER_WR_RQTYPE(f->fs.type) |
		V_FW_FILTER_WR_NOREPLY(0) |
		V_FW_FILTER_WR_IQ(f->fs.iq));
	fwr->del_filter_to_l2tix =
	    htobe32(V_FW_FILTER_WR_RPTTID(f->fs.rpttid) |
		V_FW_FILTER_WR_DROP(f->fs.action == FILTER_DROP) |
		V_FW_FILTER_WR_DIRSTEER(f->fs.dirsteer) |
		V_FW_FILTER_WR_MASKHASH(f->fs.maskhash) |
		V_FW_FILTER_WR_DIRSTEERHASH(f->fs.dirsteerhash) |
		V_FW_FILTER_WR_LPBK(f->fs.action == FILTER_SWITCH) |
		V_FW_FILTER_WR_DMAC(f->fs.newdmac) |
		V_FW_FILTER_WR_SMAC(f->fs.newsmac) |
		V_FW_FILTER_WR_INSVLAN(f->fs.newvlan == VLAN_INSERT ||
		    f->fs.newvlan == VLAN_REWRITE) |
		V_FW_FILTER_WR_RMVLAN(f->fs.newvlan == VLAN_REMOVE ||
		    f->fs.newvlan == VLAN_REWRITE) |
		V_FW_FILTER_WR_HITCNTS(f->fs.hitcnts) |
		V_FW_FILTER_WR_TXCHAN(f->fs.eport) |
		V_FW_FILTER_WR_PRIO(f->fs.prio) |
		V_FW_FILTER_WR_L2TIX(f->l2t ? f->l2t->idx : 0));
	fwr->ethtype = htobe16(f->fs.val.ethtype);
	fwr->ethtypem = htobe16(f->fs.mask.ethtype);
	fwr->frag_to_ovlan_vldm =
	    (V_FW_FILTER_WR_FRAG(f->fs.val.frag) |
		V_FW_FILTER_WR_FRAGM(f->fs.mask.frag) |
		V_FW_FILTER_WR_IVLAN_VLD(f->fs.val.vlan_vld) |
		V_FW_FILTER_WR_OVLAN_VLD(vnic_vld) |
		V_FW_FILTER_WR_IVLAN_VLDM(f->fs.mask.vlan_vld) |
		V_FW_FILTER_WR_OVLAN_VLDM(vnic_vld_mask));
	fwr->smac_sel = 0;
	fwr->rx_chan_rx_rpl_iq = htobe16(V_FW_FILTER_WR_RX_CHAN(0) |
	    V_FW_FILTER_WR_RX_RPL_IQ(sc->sge.fwq.abs_id));
	fwr->maci_to_matchtypem =
	    htobe32(V_FW_FILTER_WR_MACI(f->fs.val.macidx) |
		V_FW_FILTER_WR_MACIM(f->fs.mask.macidx) |
		V_FW_FILTER_WR_FCOE(f->fs.val.fcoe) |
		V_FW_FILTER_WR_FCOEM(f->fs.mask.fcoe) |
		V_FW_FILTER_WR_PORT(f->fs.val.iport) |
		V_FW_FILTER_WR_PORTM(f->fs.mask.iport) |
		V_FW_FILTER_WR_MATCHTYPE(f->fs.val.matchtype) |
		V_FW_FILTER_WR_MATCHTYPEM(f->fs.mask.matchtype));
	fwr->ptcl = f->fs.val.proto;
	fwr->ptclm = f->fs.mask.proto;
	fwr->ttyp = f->fs.val.tos;
	fwr->ttypm = f->fs.mask.tos;
	fwr->ivlan = htobe16(f->fs.val.vlan);
	fwr->ivlanm = htobe16(f->fs.mask.vlan);
	fwr->ovlan = htobe16(f->fs.val.vnic);
	fwr->ovlanm = htobe16(f->fs.mask.vnic);
	bcopy(f->fs.val.dip, fwr->lip, sizeof (fwr->lip));
	bcopy(f->fs.mask.dip, fwr->lipm, sizeof (fwr->lipm));
	bcopy(f->fs.val.sip, fwr->fip, sizeof (fwr->fip));
	bcopy(f->fs.mask.sip, fwr->fipm, sizeof (fwr->fipm));
	fwr->lp = htobe16(f->fs.val.dport);
	fwr->lpm = htobe16(f->fs.mask.dport);
	fwr->fp = htobe16(f->fs.val.sport);
	fwr->fpm = htobe16(f->fs.mask.sport);
	if (f->fs.newsmac)
		bcopy(f->fs.smac, fwr->sma, sizeof (fwr->sma));

	f->pending = 1;
	sc->tids.ftids_in_use++;

	commit_wrq_wr(&sc->sge.mgmtq, fwr, &cookie);
	return (0);
}

int
set_filter(struct adapter *sc, struct t4_filter *t)
{
	unsigned int nfilters, nports;
	struct filter_entry *f;
	int i, rc;

	rc = begin_synchronized_op(sc, NULL, SLEEP_OK | INTR_OK, "t4setf");
	if (rc)
		return (rc);

	nfilters = sc->tids.nftids;
	nports = sc->params.nports;

	if (nfilters == 0) {
		rc = ENOTSUP;
		goto done;
	}

	if (t->idx >= nfilters) {
		rc = EINVAL;
		goto done;
	}

	/* Validate against the global filter mode and ingress config */
	rc = check_fspec_against_fconf_iconf(sc, &t->fs);
	if (rc != 0)
		goto done;

	if (t->fs.action == FILTER_SWITCH && t->fs.eport >= nports) {
		rc = EINVAL;
		goto done;
	}

	if (t->fs.val.iport >= nports) {
		rc = EINVAL;
		goto done;
	}

	/* Can't specify an iq if not steering to it */
	if (!t->fs.dirsteer && t->fs.iq) {
		rc = EINVAL;
		goto done;
	}

	/* IPv6 filter idx must be 4 aligned */
	if (t->fs.type == 1 &&
	    ((t->idx & 0x3) || t->idx + 4 >= nfilters)) {
		rc = EINVAL;
		goto done;
	}

	if (!(sc->flags & FULL_INIT_DONE) &&
	    ((rc = adapter_full_init(sc)) != 0))
		goto done;

	if (sc->tids.ftid_tab == NULL) {
		KASSERT(sc->tids.ftids_in_use == 0,
		    ("%s: no memory allocated but filters_in_use > 0",
		    __func__));

		sc->tids.ftid_tab = malloc(sizeof (struct filter_entry) *
		    nfilters, M_CXGBE, M_NOWAIT | M_ZERO);
		if (sc->tids.ftid_tab == NULL) {
			rc = ENOMEM;
			goto done;
		}
		mtx_init(&sc->tids.ftid_lock, "T4 filters", 0, MTX_DEF);
	}

	for (i = 0; i < 4; i++) {
		f = &sc->tids.ftid_tab[t->idx + i];

		if (f->pending || f->valid) {
			rc = EBUSY;
			goto done;
		}
		if (f->locked) {
			rc = EPERM;
			goto done;
		}

		if (t->fs.type == 0)
			break;
	}

	f = &sc->tids.ftid_tab[t->idx];
	f->fs = t->fs;

	rc = set_filter_wr(sc, t->idx);
done:
	end_synchronized_op(sc, 0);

	if (rc == 0) {
		mtx_lock(&sc->tids.ftid_lock);
		for (;;) {
			if (f->pending == 0) {
				rc = f->valid ? 0 : EIO;
				break;
			}

			if (mtx_sleep(&sc->tids.ftid_tab, &sc->tids.ftid_lock,
			    PCATCH, "t4setfw", 0)) {
				rc = EINPROGRESS;
				break;
			}
		}
		mtx_unlock(&sc->tids.ftid_lock);
	}
	return (rc);
}

static int
del_filter_wr(struct adapter *sc, int fidx)
{
	struct filter_entry *f = &sc->tids.ftid_tab[fidx];
	struct fw_filter_wr *fwr;
	unsigned int ftid;
	struct wrq_cookie cookie;

	ftid = sc->tids.ftid_base + fidx;

	fwr = start_wrq_wr(&sc->sge.mgmtq, howmany(sizeof(*fwr), 16), &cookie);
	if (fwr == NULL)
		return (ENOMEM);
	bzero(fwr, sizeof (*fwr));

	t4_mk_filtdelwr(ftid, fwr, sc->sge.fwq.abs_id);

	f->pending = 1;
	commit_wrq_wr(&sc->sge.mgmtq, fwr, &cookie);
	return (0);
}

int
del_filter(struct adapter *sc, struct t4_filter *t)
{
	unsigned int nfilters;
	struct filter_entry *f;
	int rc;

	rc = begin_synchronized_op(sc, NULL, SLEEP_OK | INTR_OK, "t4delf");
	if (rc)
		return (rc);

	nfilters = sc->tids.nftids;

	if (nfilters == 0) {
		rc = ENOTSUP;
		goto done;
	}

	if (sc->tids.ftid_tab == NULL || sc->tids.ftids_in_use == 0 ||
	    t->idx >= nfilters) {
		rc = EINVAL;
		goto done;
	}

	if (!(sc->flags & FULL_INIT_DONE)) {
		rc = EAGAIN;
		goto done;
	}

	f = &sc->tids.ftid_tab[t->idx];

	if (f->pending) {
		rc = EBUSY;
		goto done;
	}
	if (f->locked) {
		rc = EPERM;
		goto done;
	}

	if (f->valid) {
		t->fs = f->fs;	/* extra info for the caller */
		rc = del_filter_wr(sc, t->idx);
	}

done:
	end_synchronized_op(sc, 0);

	if (rc == 0) {
		mtx_lock(&sc->tids.ftid_lock);
		for (;;) {
			if (f->pending == 0) {
				rc = f->valid ? EIO : 0;
				break;
			}

			if (mtx_sleep(&sc->tids.ftid_tab, &sc->tids.ftid_lock,
			    PCATCH, "t4delfw", 0)) {
				rc = EINPROGRESS;
				break;
			}
		}
		mtx_unlock(&sc->tids.ftid_lock);
	}

	return (rc);
}

static void
clear_filter(struct filter_entry *f)
{
	if (f->l2t)
		t4_l2t_release(f->l2t);

	bzero(f, sizeof (*f));
}

int
t4_filter_rpl(struct sge_iq *iq, const struct rss_header *rss, struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_set_tcb_rpl *rpl = (const void *)(rss + 1);
	unsigned int idx = GET_TID(rpl);
	unsigned int rc;
	struct filter_entry *f;

	KASSERT(m == NULL, ("%s: payload with opcode %02x", __func__,
	    rss->opcode));
	MPASS(iq == &sc->sge.fwq);
	MPASS(is_ftid(sc, idx));

	idx -= sc->tids.ftid_base;
	f = &sc->tids.ftid_tab[idx];
	rc = G_COOKIE(rpl->cookie);

	mtx_lock(&sc->tids.ftid_lock);
	if (rc == FW_FILTER_WR_FLT_ADDED) {
		KASSERT(f->pending, ("%s: filter[%u] isn't pending.",
		    __func__, idx));
		f->smtidx = (be64toh(rpl->oldval) >> 24) & 0xff;
		f->pending = 0;  /* asynchronous setup completed */
		f->valid = 1;
	} else {
		if (rc != FW_FILTER_WR_FLT_DELETED) {
			/* Add or delete failed, display an error */
			log(LOG_ERR,
			    "filter %u setup failed with error %u\n",
			    idx, rc);
		}

		clear_filter(f);
		sc->tids.ftids_in_use--;
	}
	wakeup(&sc->tids.ftid_tab);
	mtx_unlock(&sc->tids.ftid_lock);

	return (0);
}
