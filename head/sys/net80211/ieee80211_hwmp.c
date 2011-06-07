/*- 
 * Copyright (c) 2009 The FreeBSD Foundation 
 * All rights reserved. 
 * 
 * This software was developed by Rui Paulo under sponsorship from the
 * FreeBSD Foundation. 
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
#ifdef __FreeBSD__
__FBSDID("$FreeBSD$");
#endif

/*
 * IEEE 802.11s Hybrid Wireless Mesh Protocol, HWMP.
 * 
 * Based on March 2009, D3.0 802.11s draft spec.
 */
#include "opt_inet.h"
#include "opt_wlan.h"

#include <sys/param.h>
#include <sys/systm.h> 
#include <sys/mbuf.h>   
#include <sys/malloc.h>
#include <sys/kernel.h>

#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/endian.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/if_llc.h>
#include <net/ethernet.h>

#include <net/bpf.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_action.h>
#include <net80211/ieee80211_input.h>
#include <net80211/ieee80211_mesh.h>

static void	hwmp_vattach(struct ieee80211vap *);
static void	hwmp_vdetach(struct ieee80211vap *);
static int	hwmp_newstate(struct ieee80211vap *,
		    enum ieee80211_state, int);
static int	hwmp_send_action(struct ieee80211_node *,
		    const uint8_t [IEEE80211_ADDR_LEN],
		    const uint8_t [IEEE80211_ADDR_LEN],
		    uint8_t *, size_t);
static uint8_t * hwmp_add_meshpreq(uint8_t *,
		    const struct ieee80211_meshpreq_ie *);
static uint8_t * hwmp_add_meshprep(uint8_t *,
		    const struct ieee80211_meshprep_ie *);
static uint8_t * hwmp_add_meshperr(uint8_t *,
		    const struct ieee80211_meshperr_ie *);
static uint8_t * hwmp_add_meshrann(uint8_t *,
		    const struct ieee80211_meshrann_ie *);
static void	hwmp_rootmode_setup(struct ieee80211vap *);
static void	hwmp_rootmode_cb(void *);
static void	hwmp_rootmode_rann_cb(void *);
static void	hwmp_recv_preq(struct ieee80211vap *, struct ieee80211_node *,
		    const struct ieee80211_frame *,
		    const struct ieee80211_meshpreq_ie *);
static int	hwmp_send_preq(struct ieee80211_node *,
		    const uint8_t [IEEE80211_ADDR_LEN],
		    const uint8_t [IEEE80211_ADDR_LEN],
		    struct ieee80211_meshpreq_ie *);
static void	hwmp_recv_prep(struct ieee80211vap *, struct ieee80211_node *,
		    const struct ieee80211_frame *,
		    const struct ieee80211_meshprep_ie *);
static int	hwmp_send_prep(struct ieee80211_node *,
		    const uint8_t [IEEE80211_ADDR_LEN],
		    const uint8_t [IEEE80211_ADDR_LEN],
		    struct ieee80211_meshprep_ie *);
static void	hwmp_recv_perr(struct ieee80211vap *, struct ieee80211_node *,
		    const struct ieee80211_frame *,
		    const struct ieee80211_meshperr_ie *);
static int	hwmp_send_perr(struct ieee80211_node *,
		    const uint8_t [IEEE80211_ADDR_LEN],
		    const uint8_t [IEEE80211_ADDR_LEN],
		    struct ieee80211_meshperr_ie *);
static void	hwmp_recv_rann(struct ieee80211vap *, struct ieee80211_node *,
		   const struct ieee80211_frame *,
		   const struct ieee80211_meshrann_ie *);
static int	hwmp_send_rann(struct ieee80211_node *,
		    const uint8_t [IEEE80211_ADDR_LEN],
		    const uint8_t [IEEE80211_ADDR_LEN],
		    struct ieee80211_meshrann_ie *);
static struct ieee80211_node *
		hwmp_discover(struct ieee80211vap *,
		    const uint8_t [IEEE80211_ADDR_LEN], struct mbuf *);
static void	hwmp_peerdown(struct ieee80211_node *);

static struct timeval ieee80211_hwmp_preqminint = { 0, 100000 };
static struct timeval ieee80211_hwmp_perrminint = { 0, 100000 };

/* unalligned little endian access */
#define LE_WRITE_2(p, v) do {				\
	((uint8_t *)(p))[0] = (v) & 0xff;		\
	((uint8_t *)(p))[1] = ((v) >> 8) & 0xff;	\
} while (0)
#define LE_WRITE_4(p, v) do {				\
	((uint8_t *)(p))[0] = (v) & 0xff;		\
	((uint8_t *)(p))[1] = ((v) >> 8) & 0xff;	\
	((uint8_t *)(p))[2] = ((v) >> 16) & 0xff;	\
	((uint8_t *)(p))[3] = ((v) >> 24) & 0xff;	\
} while (0)


/* NB: the Target Address set in a Proactive PREQ is the broadcast address. */
static const uint8_t	broadcastaddr[IEEE80211_ADDR_LEN] =
	{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

typedef uint32_t ieee80211_hwmp_seq;
#define	HWMP_SEQ_LT(a, b)	((int32_t)((a)-(b)) < 0)
#define	HWMP_SEQ_LEQ(a, b)	((int32_t)((a)-(b)) <= 0)
#define	HWMP_SEQ_GT(a, b)	((int32_t)((a)-(b)) > 0)
#define	HWMP_SEQ_GEQ(a, b)	((int32_t)((a)-(b)) >= 0)

/*
 * Private extension of ieee80211_mesh_route.
 */
struct ieee80211_hwmp_route {
	ieee80211_hwmp_seq	hr_seq;		/* last HWMP seq seen from dst*/
	ieee80211_hwmp_seq	hr_preqid;	/* last PREQ ID seen from dst */
	ieee80211_hwmp_seq	hr_origseq;	/* seq. no. on our latest PREQ*/
	int			hr_preqretries;
};
struct ieee80211_hwmp_state {
	ieee80211_hwmp_seq	hs_seq;		/* next seq to be used */
	ieee80211_hwmp_seq	hs_preqid;	/* next PREQ ID to be used */
	struct timeval		hs_lastpreq;	/* last time we sent a PREQ */
	struct timeval		hs_lastperr;	/* last time we sent a PERR */
	int			hs_rootmode;	/* proactive HWMP */
	struct callout		hs_roottimer;
	uint8_t			hs_maxhops;	/* max hop count */
};

SYSCTL_NODE(_net_wlan, OID_AUTO, hwmp, CTLFLAG_RD, 0,
    "IEEE 802.11s HWMP parameters");
static int	ieee80211_hwmp_targetonly = 0;
SYSCTL_INT(_net_wlan_hwmp, OID_AUTO, targetonly, CTLTYPE_INT | CTLFLAG_RW,
    &ieee80211_hwmp_targetonly, 0, "Set TO bit on generated PREQs");
static int	ieee80211_hwmp_replyforward = 1;
SYSCTL_INT(_net_wlan_hwmp, OID_AUTO, replyforward, CTLTYPE_INT | CTLFLAG_RW,
    &ieee80211_hwmp_replyforward, 0, "Set RF bit on generated PREQs");
static int	ieee80211_hwmp_pathtimeout = -1;
SYSCTL_PROC(_net_wlan_hwmp, OID_AUTO, pathlifetime, CTLTYPE_INT | CTLFLAG_RW,
    &ieee80211_hwmp_pathtimeout, 0, ieee80211_sysctl_msecs_ticks, "I",
    "path entry lifetime (ms)");
static int	ieee80211_hwmp_roottimeout = -1;
SYSCTL_PROC(_net_wlan_hwmp, OID_AUTO, roottimeout, CTLTYPE_INT | CTLFLAG_RW,
    &ieee80211_hwmp_roottimeout, 0, ieee80211_sysctl_msecs_ticks, "I",
    "root PREQ timeout (ms)");
static int	ieee80211_hwmp_rootint = -1;
SYSCTL_PROC(_net_wlan_hwmp, OID_AUTO, rootint, CTLTYPE_INT | CTLFLAG_RW,
    &ieee80211_hwmp_rootint, 0, ieee80211_sysctl_msecs_ticks, "I",
    "root interval (ms)");
static int	ieee80211_hwmp_rannint = -1;
SYSCTL_PROC(_net_wlan_hwmp, OID_AUTO, rannint, CTLTYPE_INT | CTLFLAG_RW,
    &ieee80211_hwmp_rannint, 0, ieee80211_sysctl_msecs_ticks, "I",
    "root announcement interval (ms)");

#define	IEEE80211_HWMP_DEFAULT_MAXHOPS	31

static	ieee80211_recv_action_func hwmp_recv_action_meshpath;

static struct ieee80211_mesh_proto_path mesh_proto_hwmp = {
	.mpp_descr	= "HWMP",
	.mpp_ie		= IEEE80211_MESHCONF_PATH_HWMP,
	.mpp_discover	= hwmp_discover,
	.mpp_peerdown	= hwmp_peerdown,
	.mpp_vattach	= hwmp_vattach,
	.mpp_vdetach	= hwmp_vdetach,
	.mpp_newstate	= hwmp_newstate,
	.mpp_privlen	= sizeof(struct ieee80211_hwmp_route),
};
SYSCTL_PROC(_net_wlan_hwmp, OID_AUTO, inact, CTLTYPE_INT | CTLFLAG_RW,
	&mesh_proto_hwmp.mpp_inact, 0, ieee80211_sysctl_msecs_ticks, "I",
	"mesh route inactivity timeout (ms)");


static void
ieee80211_hwmp_init(void)
{
	ieee80211_hwmp_pathtimeout = msecs_to_ticks(5*1000);
	ieee80211_hwmp_roottimeout = msecs_to_ticks(5*1000);
	ieee80211_hwmp_rootint = msecs_to_ticks(2*1000);
	ieee80211_hwmp_rannint = msecs_to_ticks(1*1000);

	/*
	 * Register action frame handler.
	 */
	ieee80211_recv_action_register(IEEE80211_ACTION_CAT_MESHPATH,
	    IEEE80211_ACTION_MESHPATH_SEL, hwmp_recv_action_meshpath);

	/* NB: default is 5 secs per spec */
	mesh_proto_hwmp.mpp_inact = msecs_to_ticks(5*1000);

	/*
	 * Register HWMP.
	 */
	ieee80211_mesh_register_proto_path(&mesh_proto_hwmp);
}
SYSINIT(wlan_hwmp, SI_SUB_DRIVERS, SI_ORDER_SECOND, ieee80211_hwmp_init, NULL);

void
hwmp_vattach(struct ieee80211vap *vap)
{
	struct ieee80211_hwmp_state *hs;

	KASSERT(vap->iv_opmode == IEEE80211_M_MBSS,
	    ("not a mesh vap, opmode %d", vap->iv_opmode));

	hs = malloc(sizeof(struct ieee80211_hwmp_state), M_80211_VAP,
	    M_NOWAIT | M_ZERO);
	if (hs == NULL) {
		printf("%s: couldn't alloc HWMP state\n", __func__);
		return;
	}
	hs->hs_maxhops = IEEE80211_HWMP_DEFAULT_MAXHOPS;
	callout_init(&hs->hs_roottimer, CALLOUT_MPSAFE);
	vap->iv_hwmp = hs;
}

void
hwmp_vdetach(struct ieee80211vap *vap)
{
	struct ieee80211_hwmp_state *hs = vap->iv_hwmp;

	callout_drain(&hs->hs_roottimer);
	free(vap->iv_hwmp, M_80211_VAP);
	vap->iv_hwmp = NULL;
} 

int
hwmp_newstate(struct ieee80211vap *vap, enum ieee80211_state ostate, int arg)
{
	enum ieee80211_state nstate = vap->iv_state;
	struct ieee80211_hwmp_state *hs = vap->iv_hwmp;

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_STATE, "%s: %s -> %s (%d)\n",
	    __func__, ieee80211_state_name[ostate],
	    ieee80211_state_name[nstate], arg);

	if (nstate != IEEE80211_S_RUN && ostate == IEEE80211_S_RUN)
		callout_drain(&hs->hs_roottimer);
	if (nstate == IEEE80211_S_RUN)
		hwmp_rootmode_setup(vap);
	return 0;
}

static int
hwmp_recv_action_meshpath(struct ieee80211_node *ni,
	const struct ieee80211_frame *wh,
	const uint8_t *frm, const uint8_t *efrm)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211_meshpreq_ie preq;
	struct ieee80211_meshprep_ie prep;
	struct ieee80211_meshperr_ie perr;
	struct ieee80211_meshrann_ie rann;
	const uint8_t *iefrm = frm + 2; /* action + code */
	int found = 0;

	while (efrm - iefrm > 1) {
		IEEE80211_VERIFY_LENGTH(efrm - iefrm, iefrm[1] + 2, return 0);
		switch (*iefrm) {
		case IEEE80211_ELEMID_MESHPREQ:
		{
			const struct ieee80211_meshpreq_ie *mpreq =
			    (const struct ieee80211_meshpreq_ie *) iefrm;
			/* XXX > 1 target */
			if (mpreq->preq_len !=
			    sizeof(struct ieee80211_meshpreq_ie) - 2) {
				IEEE80211_DISCARD(vap,
				    IEEE80211_MSG_ACTION | IEEE80211_MSG_HWMP,
				    wh, NULL, "%s", "PREQ with wrong len");
				vap->iv_stats.is_rx_mgtdiscard++;
				break;
			}
			memcpy(&preq, mpreq, sizeof(preq));
			preq.preq_id = LE_READ_4(&mpreq->preq_id);
			preq.preq_origseq = LE_READ_4(&mpreq->preq_origseq);
			preq.preq_lifetime = LE_READ_4(&mpreq->preq_lifetime);
			preq.preq_metric = LE_READ_4(&mpreq->preq_metric);
			preq.preq_targets[0].target_seq =
			    LE_READ_4(&mpreq->preq_targets[0].target_seq);
			hwmp_recv_preq(vap, ni, wh, &preq);
			found++;
			break;	
		}
		case IEEE80211_ELEMID_MESHPREP:
		{
			const struct ieee80211_meshprep_ie *mprep =
			    (const struct ieee80211_meshprep_ie *) iefrm;
			if (mprep->prep_len !=
			    sizeof(struct ieee80211_meshprep_ie) - 2) {
				IEEE80211_DISCARD(vap,
				    IEEE80211_MSG_ACTION | IEEE80211_MSG_HWMP,
				    wh, NULL, "%s", "PREP with wrong len");
				vap->iv_stats.is_rx_mgtdiscard++;
				break;
			}
			memcpy(&prep, mprep, sizeof(prep));
			prep.prep_targetseq = LE_READ_4(&mprep->prep_targetseq);
			prep.prep_lifetime = LE_READ_4(&mprep->prep_lifetime);
			prep.prep_metric = LE_READ_4(&mprep->prep_metric);
			prep.prep_origseq = LE_READ_4(&mprep->prep_origseq);
			hwmp_recv_prep(vap, ni, wh, &prep);
			found++;
			break;
		}
		case IEEE80211_ELEMID_MESHPERR:
		{
			const struct ieee80211_meshperr_ie *mperr =
			    (const struct ieee80211_meshperr_ie *) iefrm;
			/* XXX > 1 target */
			if (mperr->perr_len !=
			    sizeof(struct ieee80211_meshperr_ie) - 2) {
				IEEE80211_DISCARD(vap,
				    IEEE80211_MSG_ACTION | IEEE80211_MSG_HWMP,
				    wh, NULL, "%s", "PERR with wrong len");
				vap->iv_stats.is_rx_mgtdiscard++;
				break;
			}
			memcpy(&perr, mperr, sizeof(perr));
			perr.perr_dests[0].dest_seq =
			    LE_READ_4(&mperr->perr_dests[0].dest_seq);
			hwmp_recv_perr(vap, ni, wh, &perr);
			found++;
			break;
		}
		case IEEE80211_ELEMID_MESHRANN:
		{
			const struct ieee80211_meshrann_ie *mrann =
			    (const struct ieee80211_meshrann_ie *) iefrm;
			if (mrann->rann_len !=
			    sizeof(struct ieee80211_meshrann_ie) - 2) {
				IEEE80211_DISCARD(vap,
				    IEEE80211_MSG_ACTION | IEEE80211_MSG_HWMP,
				    wh, NULL, "%s", "RAN with wrong len");
				vap->iv_stats.is_rx_mgtdiscard++;
				return 1;
			}
			memcpy(&rann, mrann, sizeof(rann));
			rann.rann_seq = LE_READ_4(&mrann->rann_seq);
			rann.rann_metric = LE_READ_4(&mrann->rann_metric);
			hwmp_recv_rann(vap, ni, wh, &rann);
			found++;
			break;
		}
		}
		iefrm += iefrm[1] + 2;
	}
	if (!found) {
		IEEE80211_DISCARD(vap,
		    IEEE80211_MSG_ACTION | IEEE80211_MSG_HWMP,
		    wh, NULL, "%s", "PATH SEL action without IE");
		vap->iv_stats.is_rx_mgtdiscard++;
	}
	return 0;
}

static int
hwmp_send_action(struct ieee80211_node *ni,
    const uint8_t sa[IEEE80211_ADDR_LEN],
    const uint8_t da[IEEE80211_ADDR_LEN],
    uint8_t *ie, size_t len)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = ni->ni_ic;
	struct ieee80211_bpf_params params;
	struct mbuf *m;
	uint8_t *frm;

	if (vap->iv_state == IEEE80211_S_CAC) {
		IEEE80211_NOTE(vap, IEEE80211_MSG_OUTPUT, ni,
		    "block %s frame in CAC state", "HWMP action");
		vap->iv_stats.is_tx_badstate++;
		return EIO;	/* XXX */
	}

	KASSERT(ni != NULL, ("null node"));
	/*
	 * Hold a reference on the node so it doesn't go away until after
	 * the xmit is complete all the way in the driver.  On error we
	 * will remove our reference.
	 */
#ifdef IEEE80211_DEBUG_REFCNT
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_NODE,
	    "ieee80211_ref_node (%s:%u) %p<%s> refcnt %d\n",
	    __func__, __LINE__,
	    ni, ether_sprintf(ni->ni_macaddr),
	    ieee80211_node_refcnt(ni)+1);
#endif
	ieee80211_ref_node(ni);

	m = ieee80211_getmgtframe(&frm,
	    ic->ic_headroom + sizeof(struct ieee80211_frame),
	    sizeof(struct ieee80211_action) + len
	);
	if (m == NULL) {
		ieee80211_free_node(ni);
		vap->iv_stats.is_tx_nobuf++;
		return ENOMEM;
	}
	*frm++ = IEEE80211_ACTION_CAT_MESHPATH;
	*frm++ = IEEE80211_ACTION_MESHPATH_SEL;
	switch (*ie) {
	case IEEE80211_ELEMID_MESHPREQ:
		frm = hwmp_add_meshpreq(frm,
		    (struct ieee80211_meshpreq_ie *)ie);
		break;
	case IEEE80211_ELEMID_MESHPREP:
		frm = hwmp_add_meshprep(frm,
		    (struct ieee80211_meshprep_ie *)ie);
		break;
	case IEEE80211_ELEMID_MESHPERR:
		frm = hwmp_add_meshperr(frm,
		    (struct ieee80211_meshperr_ie *)ie);
		break;
	case IEEE80211_ELEMID_MESHRANN:
		frm = hwmp_add_meshrann(frm,
		    (struct ieee80211_meshrann_ie *)ie);
		break;
	}

	m->m_pkthdr.len = m->m_len = frm - mtod(m, uint8_t *);
	M_PREPEND(m, sizeof(struct ieee80211_frame), M_DONTWAIT);
	if (m == NULL) {
		ieee80211_free_node(ni);
		vap->iv_stats.is_tx_nobuf++;
		return ENOMEM;
	}
	ieee80211_send_setup(ni, m,
	    IEEE80211_FC0_TYPE_MGT | IEEE80211_FC0_SUBTYPE_ACTION,
	    IEEE80211_NONQOS_TID, sa, da, sa);

	m->m_flags |= M_ENCAP;		/* mark encapsulated */
	IEEE80211_NODE_STAT(ni, tx_mgmt);

	memset(&params, 0, sizeof(params));
	params.ibp_pri = WME_AC_VO;
	params.ibp_rate0 = ni->ni_txparms->mgmtrate;
	if (IEEE80211_IS_MULTICAST(da))
		params.ibp_try0 = 1;
	else
		params.ibp_try0 = ni->ni_txparms->maxretry;
	params.ibp_power = ni->ni_txpower;
	return ic->ic_raw_xmit(ni, m, &params);
}

#define ADDSHORT(frm, v) do {		\
	frm[0] = (v) & 0xff;		\
	frm[1] = (v) >> 8;		\
	frm += 2;			\
} while (0)
#define ADDWORD(frm, v) do {		\
	LE_WRITE_4(frm, v);		\
	frm += 4;			\
} while (0)
/*
 * Add a Mesh Path Request IE to a frame.
 */
static uint8_t *
hwmp_add_meshpreq(uint8_t *frm, const struct ieee80211_meshpreq_ie *preq)
{
	int i;

	*frm++ = IEEE80211_ELEMID_MESHPREQ;
	*frm++ = sizeof(struct ieee80211_meshpreq_ie) - 2 +
	    (preq->preq_tcount - 1) * sizeof(*preq->preq_targets);
	*frm++ = preq->preq_flags;
	*frm++ = preq->preq_hopcount;
	*frm++ = preq->preq_ttl;
	ADDWORD(frm, preq->preq_id);
	IEEE80211_ADDR_COPY(frm, preq->preq_origaddr); frm += 6;
	ADDWORD(frm, preq->preq_origseq);
	ADDWORD(frm, preq->preq_lifetime);
	ADDWORD(frm, preq->preq_metric);
	*frm++ = preq->preq_tcount;
	for (i = 0; i < preq->preq_tcount; i++) {
		*frm++ = preq->preq_targets[i].target_flags;
		IEEE80211_ADDR_COPY(frm, preq->preq_targets[i].target_addr);
		frm += 6;
		ADDWORD(frm, preq->preq_targets[i].target_seq);
	}
	return frm;
}

/*
 * Add a Mesh Path Reply IE to a frame.
 */
static uint8_t *
hwmp_add_meshprep(uint8_t *frm, const struct ieee80211_meshprep_ie *prep)
{
	*frm++ = IEEE80211_ELEMID_MESHPREP;
	*frm++ = sizeof(struct ieee80211_meshprep_ie) - 2;
	*frm++ = prep->prep_flags;
	*frm++ = prep->prep_hopcount;
	*frm++ = prep->prep_ttl;
	IEEE80211_ADDR_COPY(frm, prep->prep_targetaddr); frm += 6;
	ADDWORD(frm, prep->prep_targetseq);
	ADDWORD(frm, prep->prep_lifetime);
	ADDWORD(frm, prep->prep_metric);
	IEEE80211_ADDR_COPY(frm, prep->prep_origaddr); frm += 6;
	ADDWORD(frm, prep->prep_origseq);
	return frm;
}

/*
 * Add a Mesh Path Error IE to a frame.
 */
static uint8_t *
hwmp_add_meshperr(uint8_t *frm, const struct ieee80211_meshperr_ie *perr)
{
	int i;

	*frm++ = IEEE80211_ELEMID_MESHPERR;
	*frm++ = sizeof(struct ieee80211_meshperr_ie) - 2 +
	    (perr->perr_ndests - 1) * sizeof(*perr->perr_dests);
	*frm++ = perr->perr_ttl;
	*frm++ = perr->perr_ndests;
	for (i = 0; i < perr->perr_ndests; i++) {
		*frm++ = perr->perr_dests[i].dest_flags;
		IEEE80211_ADDR_COPY(frm, perr->perr_dests[i].dest_addr);
		frm += 6;
		ADDWORD(frm, perr->perr_dests[i].dest_seq);
		ADDSHORT(frm, perr->perr_dests[i].dest_rcode);
	}
	return frm;
}

/*
 * Add a Root Annoucement IE to a frame.
 */
static uint8_t *
hwmp_add_meshrann(uint8_t *frm, const struct ieee80211_meshrann_ie *rann)
{
	*frm++ = IEEE80211_ELEMID_MESHRANN;
	*frm++ = sizeof(struct ieee80211_meshrann_ie) - 2;
	*frm++ = rann->rann_flags;
	*frm++ = rann->rann_hopcount;
	*frm++ = rann->rann_ttl;
	IEEE80211_ADDR_COPY(frm, rann->rann_addr); frm += 6;
	ADDWORD(frm, rann->rann_seq);
	ADDWORD(frm, rann->rann_metric);
	return frm;
}

static void
hwmp_rootmode_setup(struct ieee80211vap *vap)
{
	struct ieee80211_hwmp_state *hs = vap->iv_hwmp;

	switch (hs->hs_rootmode) {
	case IEEE80211_HWMP_ROOTMODE_DISABLED:
		callout_drain(&hs->hs_roottimer);
		break;
	case IEEE80211_HWMP_ROOTMODE_NORMAL:
	case IEEE80211_HWMP_ROOTMODE_PROACTIVE:
		callout_reset(&hs->hs_roottimer, ieee80211_hwmp_rootint,
		    hwmp_rootmode_cb, vap);
		break;
	case IEEE80211_HWMP_ROOTMODE_RANN:
		callout_reset(&hs->hs_roottimer, ieee80211_hwmp_rannint,
		    hwmp_rootmode_rann_cb, vap);
		break;
	}
}

/*
 * Send a broadcast Path Request to find all nodes on the mesh. We are
 * called when the vap is configured as a HWMP root node.
 */
#define	PREQ_TFLAGS(n)	preq.preq_targets[n].target_flags
#define	PREQ_TADDR(n)	preq.preq_targets[n].target_addr
#define	PREQ_TSEQ(n)	preq.preq_targets[n].target_seq
static void
hwmp_rootmode_cb(void *arg)
{
	struct ieee80211vap *vap = (struct ieee80211vap *)arg;
	struct ieee80211_hwmp_state *hs = vap->iv_hwmp;
	struct ieee80211_mesh_state *ms = vap->iv_mesh;
	struct ieee80211_meshpreq_ie preq;

	IEEE80211_NOTE(vap, IEEE80211_MSG_HWMP, vap->iv_bss,
	    "%s", "send broadcast PREQ");

	preq.preq_flags = IEEE80211_MESHPREQ_FLAGS_AM;
	if (ms->ms_flags & IEEE80211_MESHFLAGS_PORTAL)
		preq.preq_flags |= IEEE80211_MESHPREQ_FLAGS_PR;
	if (hs->hs_rootmode == IEEE80211_HWMP_ROOTMODE_PROACTIVE)
		preq.preq_flags |= IEEE80211_MESHPREQ_FLAGS_PP;
	preq.preq_hopcount = 0;
	preq.preq_ttl = ms->ms_ttl;
	preq.preq_id = ++hs->hs_preqid;
	IEEE80211_ADDR_COPY(preq.preq_origaddr, vap->iv_myaddr);
	preq.preq_origseq = ++hs->hs_seq;
	preq.preq_lifetime = ticks_to_msecs(ieee80211_hwmp_roottimeout);
	preq.preq_metric = IEEE80211_MESHLMETRIC_INITIALVAL;
	preq.preq_tcount = 1;
	IEEE80211_ADDR_COPY(PREQ_TADDR(0), broadcastaddr);
	PREQ_TFLAGS(0) = IEEE80211_MESHPREQ_TFLAGS_TO |
	    IEEE80211_MESHPREQ_TFLAGS_RF;
	PREQ_TSEQ(0) = 0;
	vap->iv_stats.is_hwmp_rootreqs++;
	hwmp_send_preq(vap->iv_bss, vap->iv_myaddr, broadcastaddr, &preq);
	hwmp_rootmode_setup(vap);
}
#undef	PREQ_TFLAGS
#undef	PREQ_TADDR
#undef	PREQ_TSEQ

/*
 * Send a Root Annoucement (RANN) to find all the nodes on the mesh. We are
 * called when the vap is configured as a HWMP RANN root node.
 */
static void
hwmp_rootmode_rann_cb(void *arg)
{
	struct ieee80211vap *vap = (struct ieee80211vap *)arg;
	struct ieee80211_hwmp_state *hs = vap->iv_hwmp;
	struct ieee80211_mesh_state *ms = vap->iv_mesh;
	struct ieee80211_meshrann_ie rann;

	IEEE80211_NOTE(vap, IEEE80211_MSG_HWMP, vap->iv_bss,
	    "%s", "send broadcast RANN");

	rann.rann_flags = 0;
	if (ms->ms_flags & IEEE80211_MESHFLAGS_PORTAL)
		rann.rann_flags |= IEEE80211_MESHRANN_FLAGS_PR;
	rann.rann_hopcount = 0;
	rann.rann_ttl = ms->ms_ttl;
	IEEE80211_ADDR_COPY(rann.rann_addr, vap->iv_myaddr);
	rann.rann_seq = ++hs->hs_seq;
	rann.rann_metric = IEEE80211_MESHLMETRIC_INITIALVAL;

	vap->iv_stats.is_hwmp_rootrann++;
	hwmp_send_rann(vap->iv_bss, vap->iv_myaddr, broadcastaddr, &rann);
	hwmp_rootmode_setup(vap);
}

#define	PREQ_TFLAGS(n)	preq->preq_targets[n].target_flags
#define	PREQ_TADDR(n)	preq->preq_targets[n].target_addr
#define	PREQ_TSEQ(n)	preq->preq_targets[n].target_seq
static void
hwmp_recv_preq(struct ieee80211vap *vap, struct ieee80211_node *ni,
    const struct ieee80211_frame *wh, const struct ieee80211_meshpreq_ie *preq)
{
	struct ieee80211_mesh_state *ms = vap->iv_mesh;
	struct ieee80211_mesh_route *rt = NULL;
	struct ieee80211_mesh_route *rtorig = NULL;
	struct ieee80211_hwmp_route *hrorig;
	struct ieee80211_hwmp_state *hs = vap->iv_hwmp;
	struct ieee80211_meshprep_ie prep;

	if (ni == vap->iv_bss ||
	    ni->ni_mlstate != IEEE80211_NODE_MESH_ESTABLISHED)
		return;
	/*
	 * Ignore PREQs from us. Could happen because someone forward it
	 * back to us.
	 */
	if (IEEE80211_ADDR_EQ(vap->iv_myaddr, preq->preq_origaddr))
		return;

	IEEE80211_NOTE(vap, IEEE80211_MSG_HWMP, ni,
	    "received PREQ, source %s", ether_sprintf(preq->preq_origaddr));

	/*
	 * Acceptance criteria: if the PREQ is not for us and
	 * forwarding is disabled, discard this PREQ.
	 */
	if (!IEEE80211_ADDR_EQ(vap->iv_myaddr, PREQ_TADDR(0)) &&
	    !(ms->ms_flags & IEEE80211_MESHFLAGS_FWD)) {
		IEEE80211_DISCARD_MAC(vap, IEEE80211_MSG_HWMP,
		    preq->preq_origaddr, NULL, "%s", "not accepting PREQ");
		return;
	}
	rtorig = ieee80211_mesh_rt_find(vap, preq->preq_origaddr);
	if (rtorig == NULL)
		rtorig = ieee80211_mesh_rt_add(vap, preq->preq_origaddr);
	if (rtorig == NULL) {
		/* XXX stat */
		return;
	}
	hrorig = IEEE80211_MESH_ROUTE_PRIV(rtorig, struct ieee80211_hwmp_route);
	/*
	 * Sequence number validation.
	 */
	if (HWMP_SEQ_LEQ(preq->preq_id, hrorig->hr_preqid) &&
	    HWMP_SEQ_LEQ(preq->preq_origseq, hrorig->hr_seq)) {
		IEEE80211_NOTE(vap, IEEE80211_MSG_HWMP, ni,
		    "discard PREQ from %s, old seq no %u <= %u",
		    ether_sprintf(preq->preq_origaddr),
		    preq->preq_origseq, hrorig->hr_seq);
		return;
	}
	hrorig->hr_preqid = preq->preq_id;
	hrorig->hr_seq = preq->preq_origseq;

	/*
	 * Check if the PREQ is addressed to us.
	 */
	if (IEEE80211_ADDR_EQ(vap->iv_myaddr, PREQ_TADDR(0))) {
		IEEE80211_NOTE(vap, IEEE80211_MSG_HWMP, ni,
		    "reply to %s", ether_sprintf(preq->preq_origaddr));
		/*
		 * Build and send a PREP frame.
		 */
		prep.prep_flags = 0;
		prep.prep_hopcount = 0;
		prep.prep_ttl = ms->ms_ttl;
		IEEE80211_ADDR_COPY(prep.prep_targetaddr, vap->iv_myaddr);
		prep.prep_targetseq = ++hs->hs_seq;
		prep.prep_lifetime = preq->preq_lifetime;
		prep.prep_metric = IEEE80211_MESHLMETRIC_INITIALVAL;
		IEEE80211_ADDR_COPY(prep.prep_origaddr, preq->preq_origaddr);
		prep.prep_origseq = preq->preq_origseq;
		hwmp_send_prep(ni, vap->iv_myaddr, wh->i_addr2, &prep);
		/*
		 * Build the reverse path, if we don't have it already.
		 */
		rt = ieee80211_mesh_rt_find(vap, preq->preq_origaddr);
		if (rt == NULL)
			hwmp_discover(vap, preq->preq_origaddr, NULL);
		else if ((rt->rt_flags & IEEE80211_MESHRT_FLAGS_VALID) == 0)
			hwmp_discover(vap, rt->rt_dest, NULL);
		return;
	}
	/*
	 * Proactive PREQ: reply with a proactive PREP to the
	 * root STA if requested.
	 */
	if (IEEE80211_ADDR_EQ(PREQ_TADDR(0), broadcastaddr) &&
	    (PREQ_TFLAGS(0) &
	    ((IEEE80211_MESHPREQ_TFLAGS_TO|IEEE80211_MESHPREQ_TFLAGS_RF) ==
	    (IEEE80211_MESHPREQ_TFLAGS_TO|IEEE80211_MESHPREQ_TFLAGS_RF)))) {
		uint8_t rootmac[IEEE80211_ADDR_LEN];

		IEEE80211_ADDR_COPY(rootmac, preq->preq_origaddr);
		rt = ieee80211_mesh_rt_find(vap, rootmac);
		if (rt == NULL) {
			rt = ieee80211_mesh_rt_add(vap, rootmac);
			if (rt == NULL) {
				IEEE80211_NOTE(vap, IEEE80211_MSG_HWMP, ni,
				    "unable to add root mesh path to %s",
				    ether_sprintf(rootmac));
				vap->iv_stats.is_mesh_rtaddfailed++;
				return;
			}
		}
		IEEE80211_NOTE(vap, IEEE80211_MSG_HWMP, ni,
		    "root mesh station @ %s", ether_sprintf(rootmac));

		/*
		 * Reply with a PREP if we don't have a path to the root
		 * or if the root sent us a proactive PREQ.
		 */
		if ((rt->rt_flags & IEEE80211_MESHRT_FLAGS_VALID) == 0 ||
		    (preq->preq_flags & IEEE80211_MESHPREQ_FLAGS_PP)) {
			prep.prep_flags = 0;
			prep.prep_hopcount = 0;
			prep.prep_ttl = ms->ms_ttl;
			IEEE80211_ADDR_COPY(prep.prep_origaddr, rootmac);
			prep.prep_origseq = preq->preq_origseq;
			prep.prep_lifetime = preq->preq_lifetime;
			prep.prep_metric = IEEE80211_MESHLMETRIC_INITIALVAL;
			IEEE80211_ADDR_COPY(prep.prep_targetaddr,
			    vap->iv_myaddr);
			prep.prep_targetseq = ++hs->hs_seq;
			hwmp_send_prep(vap->iv_bss, vap->iv_myaddr,
			    broadcastaddr, &prep);
		}
		hwmp_discover(vap, rootmac, NULL);
		return;
	}
	rt = ieee80211_mesh_rt_find(vap, PREQ_TADDR(0));

	/*
	 * Forwarding and Intermediate reply for PREQs with 1 target.
	 */
	if (preq->preq_tcount == 1) {
		struct ieee80211_meshpreq_ie ppreq; /* propagated PREQ */

		memcpy(&ppreq, preq, sizeof(ppreq));
		/*
		 * We have a valid route to this node.
		 */
		if (rt != NULL &&
		    (rt->rt_flags & IEEE80211_MESHRT_FLAGS_VALID)) {
			if (preq->preq_ttl > 1 &&
			    preq->preq_hopcount < hs->hs_maxhops) {
				IEEE80211_NOTE(vap, IEEE80211_MSG_HWMP, ni,
				    "forward PREQ from %s",
				    ether_sprintf(preq->preq_origaddr));
				/*
				 * Propagate the original PREQ.
				 */
				ppreq.preq_hopcount += 1;
				ppreq.preq_ttl -= 1;
				ppreq.preq_metric +=
				    ms->ms_pmetric->mpm_metric(ni);
				/*
				 * Set TO and unset RF bits because we are going
				 * to send a PREP next.
				 */
				ppreq.preq_targets[0].target_flags |=
				    IEEE80211_MESHPREQ_TFLAGS_TO;
				ppreq.preq_targets[0].target_flags &=
				    ~IEEE80211_MESHPREQ_TFLAGS_RF;
				hwmp_send_preq(ni, vap->iv_myaddr,
				    broadcastaddr, &ppreq);
			}
			/*
			 * Check if we can send an intermediate Path Reply,
			 * i.e., Target Only bit is not set.
			 */
	    		if (!(PREQ_TFLAGS(0) & IEEE80211_MESHPREQ_TFLAGS_TO)) {
				struct ieee80211_meshprep_ie prep;

				IEEE80211_NOTE(vap, IEEE80211_MSG_HWMP, ni,
				    "intermediate reply for PREQ from %s",
				    ether_sprintf(preq->preq_origaddr));
				prep.prep_flags = 0;
				prep.prep_hopcount = rt->rt_nhops + 1;
				prep.prep_ttl = ms->ms_ttl;
				IEEE80211_ADDR_COPY(&prep.prep_targetaddr,
				    PREQ_TADDR(0));
				prep.prep_targetseq = hrorig->hr_seq;
				prep.prep_lifetime = preq->preq_lifetime;
				prep.prep_metric = rt->rt_metric +
				    ms->ms_pmetric->mpm_metric(ni);
				IEEE80211_ADDR_COPY(&prep.prep_origaddr,
				    preq->preq_origaddr);
				prep.prep_origseq = hrorig->hr_seq;
				hwmp_send_prep(ni, vap->iv_myaddr,
				    broadcastaddr, &prep);
			}
		/*
		 * We have no information about this path,
		 * propagate the PREQ.
		 */
		} else if (preq->preq_ttl > 1 &&
		    preq->preq_hopcount < hs->hs_maxhops) {
			if (rt == NULL) {
				rt = ieee80211_mesh_rt_add(vap, PREQ_TADDR(0));
				if (rt == NULL) {
					IEEE80211_NOTE(vap, IEEE80211_MSG_HWMP,
					    ni, "unable to add PREQ path to %s",
					    ether_sprintf(PREQ_TADDR(0)));
					vap->iv_stats.is_mesh_rtaddfailed++;
					return;
				}
			}
			rt->rt_metric = preq->preq_metric;
			rt->rt_lifetime = preq->preq_lifetime;
			hrorig = IEEE80211_MESH_ROUTE_PRIV(rt,
			    struct ieee80211_hwmp_route);
			hrorig->hr_seq = preq->preq_origseq;
			hrorig->hr_preqid = preq->preq_id;

			IEEE80211_NOTE(vap, IEEE80211_MSG_HWMP, ni,
			    "forward PREQ from %s",
			    ether_sprintf(preq->preq_origaddr));
			ppreq.preq_hopcount += 1;
			ppreq.preq_ttl -= 1;
			ppreq.preq_metric += ms->ms_pmetric->mpm_metric(ni);
			hwmp_send_preq(ni, vap->iv_myaddr, broadcastaddr,
			    &ppreq);
		}
	}

}
#undef	PREQ_TFLAGS
#undef	PREQ_TADDR
#undef	PREQ_TSEQ

static int
hwmp_send_preq(struct ieee80211_node *ni,
    const uint8_t sa[IEEE80211_ADDR_LEN],
    const uint8_t da[IEEE80211_ADDR_LEN],
    struct ieee80211_meshpreq_ie *preq)
{
	struct ieee80211_hwmp_state *hs = ni->ni_vap->iv_hwmp;

	/*
	 * Enforce PREQ interval.
	 */
	if (ratecheck(&hs->hs_lastpreq, &ieee80211_hwmp_preqminint) == 0)
		return EALREADY;
	getmicrouptime(&hs->hs_lastpreq);

	/*
	 * mesh preq action frame format
	 *     [6] da
	 *     [6] sa 
	 *     [6] addr3 = sa
	 *     [1] action
	 *     [1] category
	 *     [tlv] mesh path request
	 */
	preq->preq_ie = IEEE80211_ELEMID_MESHPREQ;
	return hwmp_send_action(ni, sa, da, (uint8_t *)preq,
	    sizeof(struct ieee80211_meshpreq_ie));
}

static void
hwmp_recv_prep(struct ieee80211vap *vap, struct ieee80211_node *ni,
    const struct ieee80211_frame *wh, const struct ieee80211_meshprep_ie *prep)
{
	struct ieee80211_mesh_state *ms = vap->iv_mesh;
	struct ieee80211_hwmp_state *hs = vap->iv_hwmp;
	struct ieee80211_mesh_route *rt = NULL;
	struct ieee80211_hwmp_route *hr;
	struct ieee80211com *ic = vap->iv_ic;
	struct ifnet *ifp = vap->iv_ifp;
	struct mbuf *m, *next;

	/*
	 * Acceptance criteria: if the corresponding PREQ was not generated
	 * by us and forwarding is disabled, discard this PREP.
	 */
	if (ni == vap->iv_bss ||
	    ni->ni_mlstate != IEEE80211_NODE_MESH_ESTABLISHED)
		return;
	if (!IEEE80211_ADDR_EQ(vap->iv_myaddr, prep->prep_origaddr) &&
	    !(ms->ms_flags & IEEE80211_MESHFLAGS_FWD))
		return;

	IEEE80211_NOTE(vap, IEEE80211_MSG_HWMP, ni,
	    "received PREP from %s", ether_sprintf(prep->prep_targetaddr));

	rt = ieee80211_mesh_rt_find(vap, prep->prep_targetaddr);
	if (rt == NULL) {
		/*
		 * If we have no entry this could be a reply to a root PREQ.
		 */
		if (hs->hs_rootmode != IEEE80211_HWMP_ROOTMODE_DISABLED) {
			rt = ieee80211_mesh_rt_add(vap, prep->prep_targetaddr);
			if (rt == NULL) {
				IEEE80211_NOTE(vap, IEEE80211_MSG_HWMP,
				    ni, "unable to add PREP path to %s",
				    ether_sprintf(prep->prep_targetaddr));
				vap->iv_stats.is_mesh_rtaddfailed++;
				return;
			}
			IEEE80211_ADDR_COPY(rt->rt_nexthop, wh->i_addr2);
			rt->rt_nhops = prep->prep_hopcount;
			rt->rt_lifetime = prep->prep_lifetime;
			rt->rt_metric = prep->prep_metric;
			rt->rt_flags |= IEEE80211_MESHRT_FLAGS_VALID;
			IEEE80211_NOTE(vap, IEEE80211_MSG_HWMP, ni,
			    "add root path to %s nhops %d metric %d (PREP)",
			    ether_sprintf(prep->prep_targetaddr),
			    rt->rt_nhops, rt->rt_metric);
			return;
		} 
		return;
	}
	/*
	 * Sequence number validation.
	 */
	hr = IEEE80211_MESH_ROUTE_PRIV(rt, struct ieee80211_hwmp_route);
	if (HWMP_SEQ_LEQ(prep->prep_targetseq, hr->hr_seq)) {
		IEEE80211_NOTE(vap, IEEE80211_MSG_HWMP, ni,
		    "discard PREP from %s, old seq no %u <= %u",
		    ether_sprintf(prep->prep_targetaddr),
		    prep->prep_targetseq, hr->hr_seq);
		return;
	}
	hr->hr_seq = prep->prep_targetseq;
	/*
	 * If it's NOT for us, propagate the PREP.
	 */
	if (!IEEE80211_ADDR_EQ(vap->iv_myaddr, prep->prep_origaddr) &&
	    prep->prep_ttl > 1 && prep->prep_hopcount < hs->hs_maxhops) {
		struct ieee80211_meshprep_ie pprep; /* propagated PREP */

		IEEE80211_NOTE(vap, IEEE80211_MSG_HWMP, ni,
		    "propagate PREP from %s",
		    ether_sprintf(prep->prep_targetaddr));

		memcpy(&pprep, prep, sizeof(pprep));
		pprep.prep_hopcount += 1;
		pprep.prep_ttl -= 1;
		pprep.prep_metric += ms->ms_pmetric->mpm_metric(ni);
		IEEE80211_ADDR_COPY(pprep.prep_targetaddr, vap->iv_myaddr);
		hwmp_send_prep(ni, vap->iv_myaddr, broadcastaddr, &pprep);
	}
	hr = IEEE80211_MESH_ROUTE_PRIV(rt, struct ieee80211_hwmp_route);
	if (rt->rt_flags & IEEE80211_MESHRT_FLAGS_PROXY) {
		/* NB: never clobber a proxy entry */;
		IEEE80211_NOTE(vap, IEEE80211_MSG_HWMP, ni,
		    "discard PREP for %s, route is marked PROXY",
		    ether_sprintf(prep->prep_targetaddr));
		vap->iv_stats.is_hwmp_proxy++;
	} else if (prep->prep_origseq == hr->hr_origseq) {
		/*
		 * Check if we already have a path to this node.
		 * If we do, check if this path reply contains a
		 * better route.
		 */
		if ((rt->rt_flags & IEEE80211_MESHRT_FLAGS_VALID) == 0 ||
		    (prep->prep_hopcount < rt->rt_nhops ||
		     prep->prep_metric < rt->rt_metric)) {
			IEEE80211_NOTE(vap, IEEE80211_MSG_HWMP, ni,
			    "%s path to %s, hopcount %d:%d metric %d:%d",
			    rt->rt_flags & IEEE80211_MESHRT_FLAGS_VALID ?
				"prefer" : "update",
			    ether_sprintf(prep->prep_origaddr),
			    rt->rt_nhops, prep->prep_hopcount,
			    rt->rt_metric, prep->prep_metric);
			IEEE80211_ADDR_COPY(rt->rt_nexthop, wh->i_addr2);
			rt->rt_nhops = prep->prep_hopcount;
			rt->rt_lifetime = prep->prep_lifetime;
			rt->rt_metric = prep->prep_metric;
			rt->rt_flags |= IEEE80211_MESHRT_FLAGS_VALID;
		} else {
			IEEE80211_NOTE(vap, IEEE80211_MSG_HWMP, ni,
			    "ignore PREP for %s, hopcount %d:%d metric %d:%d",
			    ether_sprintf(prep->prep_targetaddr),
			    rt->rt_nhops, prep->prep_hopcount,
			    rt->rt_metric, prep->prep_metric);
		}
	} else {
		IEEE80211_NOTE(vap, IEEE80211_MSG_HWMP, ni,
		    "discard PREP for %s, wrong seqno %u != %u",
		    ether_sprintf(prep->prep_targetaddr), prep->prep_origseq,
		    hr->hr_seq);
		vap->iv_stats.is_hwmp_wrongseq++;
	} 
	/*
	 * Check for frames queued awaiting path discovery.
	 * XXX probably can tell exactly and avoid remove call
	 * NB: hash may have false matches, if so they will get
	 *     stuck back on the stageq because there won't be
	 *     a path.
	 */
	m = ieee80211_ageq_remove(&ic->ic_stageq, 
	    (struct ieee80211_node *)(uintptr_t)
		ieee80211_mac_hash(ic, rt->rt_dest));
	for (; m != NULL; m = next) {
		next = m->m_nextpkt;
		m->m_nextpkt = NULL;
		IEEE80211_NOTE(vap, IEEE80211_MSG_HWMP, ni,
		    "flush queued frame %p len %d", m, m->m_pkthdr.len);
		ifp->if_transmit(ifp, m);
	}
}

static int
hwmp_send_prep(struct ieee80211_node *ni,
    const uint8_t sa[IEEE80211_ADDR_LEN],
    const uint8_t da[IEEE80211_ADDR_LEN],
    struct ieee80211_meshprep_ie *prep)
{
	/* NB: there's no PREP minimum interval. */

	/*
	 * mesh prep action frame format
	 *     [6] da
	 *     [6] sa 
	 *     [6] addr3 = sa
	 *     [1] action
	 *     [1] category
	 *     [tlv] mesh path reply
	 */
	prep->prep_ie = IEEE80211_ELEMID_MESHPREP;
	return hwmp_send_action(ni, sa, da, (uint8_t *)prep,
	    sizeof(struct ieee80211_meshprep_ie));
}

#define	PERR_DFLAGS(n)	perr.perr_dests[n].dest_flags
#define	PERR_DADDR(n)	perr.perr_dests[n].dest_addr
#define	PERR_DSEQ(n)	perr.perr_dests[n].dest_seq
#define	PERR_DRCODE(n)	perr.perr_dests[n].dest_rcode
static void
hwmp_peerdown(struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211_mesh_state *ms = vap->iv_mesh;
	struct ieee80211_meshperr_ie perr;
	struct ieee80211_mesh_route *rt;
	struct ieee80211_hwmp_route *hr;

	rt = ieee80211_mesh_rt_find(vap, ni->ni_macaddr);
	if (rt == NULL)
		return;
	hr = IEEE80211_MESH_ROUTE_PRIV(rt, struct ieee80211_hwmp_route);
	IEEE80211_NOTE(vap, IEEE80211_MSG_HWMP, ni,
	    "%s", "delete route entry");
	perr.perr_ttl = ms->ms_ttl;
	perr.perr_ndests = 1;
	PERR_DFLAGS(0) = 0;
	if (hr->hr_seq == 0)
		PERR_DFLAGS(0) |= IEEE80211_MESHPERR_DFLAGS_USN;
	PERR_DFLAGS(0) |= IEEE80211_MESHPERR_DFLAGS_RC;
	IEEE80211_ADDR_COPY(PERR_DADDR(0), rt->rt_dest);
	PERR_DSEQ(0) = hr->hr_seq;
	PERR_DRCODE(0) = IEEE80211_REASON_MESH_PERR_DEST_UNREACH;
	/* NB: flush everything passing through peer */
	ieee80211_mesh_rt_flush_peer(vap, ni->ni_macaddr);
	hwmp_send_perr(vap->iv_bss, vap->iv_myaddr, broadcastaddr, &perr);
}
#undef	PERR_DFLAGS
#undef	PERR_DADDR
#undef	PERR_DSEQ
#undef	PERR_DRCODE

#define	PERR_DFLAGS(n)	perr->perr_dests[n].dest_flags
#define	PERR_DADDR(n)	perr->perr_dests[n].dest_addr
#define	PERR_DSEQ(n)	perr->perr_dests[n].dest_seq
#define	PERR_DRCODE(n)	perr->perr_dests[n].dest_rcode
static void
hwmp_recv_perr(struct ieee80211vap *vap, struct ieee80211_node *ni,
    const struct ieee80211_frame *wh, const struct ieee80211_meshperr_ie *perr)
{
	struct ieee80211_mesh_state *ms = vap->iv_mesh;
	struct ieee80211_mesh_route *rt = NULL;
	struct ieee80211_hwmp_route *hr;
 	struct ieee80211_meshperr_ie pperr;
	int i, forward = 0;

	/*
	 * Acceptance criteria: check if we received a PERR from a
	 * neighbor and forwarding is enabled.
	 */
	if (ni == vap->iv_bss ||
	    ni->ni_mlstate != IEEE80211_NODE_MESH_ESTABLISHED ||
	    !(ms->ms_flags & IEEE80211_MESHFLAGS_FWD))
		return;
	/*
	 * Find all routing entries that match and delete them.
	 */
	for (i = 0; i < perr->perr_ndests; i++) {
		rt = ieee80211_mesh_rt_find(vap, PERR_DADDR(i));
		if (rt == NULL)
			continue;
		hr = IEEE80211_MESH_ROUTE_PRIV(rt, struct ieee80211_hwmp_route);
		if (!(PERR_DFLAGS(0) & IEEE80211_MESHPERR_DFLAGS_USN) && 
		    HWMP_SEQ_GEQ(PERR_DSEQ(i), hr->hr_seq)) {
			ieee80211_mesh_rt_del(vap, rt->rt_dest);
			ieee80211_mesh_rt_flush_peer(vap, rt->rt_dest);
			rt = NULL;
			forward = 1;
		}
	}
	/*
	 * Propagate the PERR if we previously found it on our routing table.
	 * XXX handle ndest > 1
	 */
	if (forward && perr->perr_ttl > 1) {
		IEEE80211_NOTE(vap, IEEE80211_MSG_HWMP, ni,
		    "propagate PERR from %s", ether_sprintf(wh->i_addr2));
		memcpy(&pperr, perr, sizeof(*perr));
		pperr.perr_ttl--;
		hwmp_send_perr(vap->iv_bss, vap->iv_myaddr, broadcastaddr,
		    &pperr);
	}
}
#undef	PEER_DADDR
#undef	PERR_DSEQ

static int
hwmp_send_perr(struct ieee80211_node *ni,
    const uint8_t sa[IEEE80211_ADDR_LEN],
    const uint8_t da[IEEE80211_ADDR_LEN],
    struct ieee80211_meshperr_ie *perr)
{
	struct ieee80211_hwmp_state *hs = ni->ni_vap->iv_hwmp;

	/*
	 * Enforce PERR interval.
	 */
	if (ratecheck(&hs->hs_lastperr, &ieee80211_hwmp_perrminint) == 0)
		return EALREADY;
	getmicrouptime(&hs->hs_lastperr);

	/*
	 * mesh perr action frame format
	 *     [6] da
	 *     [6] sa
	 *     [6] addr3 = sa
	 *     [1] action
	 *     [1] category
	 *     [tlv] mesh path error
	 */
	perr->perr_ie = IEEE80211_ELEMID_MESHPERR;
	return hwmp_send_action(ni, sa, da, (uint8_t *)perr,
	    sizeof(struct ieee80211_meshperr_ie));
}

static void
hwmp_recv_rann(struct ieee80211vap *vap, struct ieee80211_node *ni,
    const struct ieee80211_frame *wh, const struct ieee80211_meshrann_ie *rann)
{
	struct ieee80211_mesh_state *ms = vap->iv_mesh;
	struct ieee80211_hwmp_state *hs = vap->iv_hwmp;
	struct ieee80211_mesh_route *rt = NULL;
	struct ieee80211_hwmp_route *hr;
	struct ieee80211_meshrann_ie prann;

	if (ni == vap->iv_bss ||
	    ni->ni_mlstate != IEEE80211_NODE_MESH_ESTABLISHED ||
	    IEEE80211_ADDR_EQ(rann->rann_addr, vap->iv_myaddr))
		return;

	rt = ieee80211_mesh_rt_find(vap, rann->rann_addr);
	/*
	 * Discover the path to the root mesh STA.
	 * If we already know it, propagate the RANN element.
	 */
	if (rt == NULL) {
		hwmp_discover(vap, rann->rann_addr, NULL);
		return;
	}
	hr = IEEE80211_MESH_ROUTE_PRIV(rt, struct ieee80211_hwmp_route);
	if (HWMP_SEQ_GT(rann->rann_seq, hr->hr_seq)) {
		hr->hr_seq = rann->rann_seq;
		if (rann->rann_ttl > 1 &&
		    rann->rann_hopcount < hs->hs_maxhops &&
		    (ms->ms_flags & IEEE80211_MESHFLAGS_FWD)) {
			memcpy(&prann, rann, sizeof(prann));
			prann.rann_hopcount += 1;
			prann.rann_ttl -= 1;
			prann.rann_metric += ms->ms_pmetric->mpm_metric(ni);
			hwmp_send_rann(vap->iv_bss, vap->iv_myaddr,
			    broadcastaddr, &prann);
		}
	}
}

static int
hwmp_send_rann(struct ieee80211_node *ni,
    const uint8_t sa[IEEE80211_ADDR_LEN],
    const uint8_t da[IEEE80211_ADDR_LEN],
    struct ieee80211_meshrann_ie *rann)
{
	/*
	 * mesh rann action frame format
	 *     [6] da
	 *     [6] sa 
	 *     [6] addr3 = sa
	 *     [1] action
	 *     [1] category
	 *     [tlv] root annoucement
	 */
	rann->rann_ie = IEEE80211_ELEMID_MESHRANN;
	return hwmp_send_action(ni, sa, da, (uint8_t *)rann,
	    sizeof(struct ieee80211_meshrann_ie));
}

#define	PREQ_TFLAGS(n)	preq.preq_targets[n].target_flags
#define	PREQ_TADDR(n)	preq.preq_targets[n].target_addr
#define	PREQ_TSEQ(n)	preq.preq_targets[n].target_seq
static struct ieee80211_node *
hwmp_discover(struct ieee80211vap *vap,
    const uint8_t dest[IEEE80211_ADDR_LEN], struct mbuf *m)
{
	struct ieee80211_hwmp_state *hs = vap->iv_hwmp;
	struct ieee80211_mesh_state *ms = vap->iv_mesh;
	struct ieee80211_mesh_route *rt = NULL;
	struct ieee80211_hwmp_route *hr;
	struct ieee80211_meshpreq_ie preq;
	struct ieee80211_node *ni;
	int sendpreq = 0;

	KASSERT(vap->iv_opmode == IEEE80211_M_MBSS,
	    ("not a mesh vap, opmode %d", vap->iv_opmode));

	KASSERT(!IEEE80211_ADDR_EQ(vap->iv_myaddr, dest),
	    ("%s: discovering self!", __func__));

	ni = NULL;
	if (!IEEE80211_IS_MULTICAST(dest)) {
		rt = ieee80211_mesh_rt_find(vap, dest);
		if (rt == NULL) {
			rt = ieee80211_mesh_rt_add(vap, dest);
			if (rt == NULL) {
				IEEE80211_NOTE(vap, IEEE80211_MSG_HWMP,
				    ni, "unable to add discovery path to %s",
				    ether_sprintf(dest));
				vap->iv_stats.is_mesh_rtaddfailed++;
				goto done;
			}
		}
		hr = IEEE80211_MESH_ROUTE_PRIV(rt,
		    struct ieee80211_hwmp_route);
		if ((rt->rt_flags & IEEE80211_MESHRT_FLAGS_VALID) == 0) {
			if (hr->hr_origseq == 0)
				hr->hr_origseq = ++hs->hs_seq;
			rt->rt_metric = IEEE80211_MESHLMETRIC_INITIALVAL;
			rt->rt_lifetime =
			    ticks_to_msecs(ieee80211_hwmp_pathtimeout);
			/* XXX check preq retries */
			sendpreq = 1;
			IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_HWMP, dest,
			    "start path discovery (src %s)",
			    m == NULL ? "<none>" : ether_sprintf(
				mtod(m, struct ether_header *)->ether_shost));
			/*
			 * Try to discover the path for this node.
			 */
			preq.preq_flags = 0;
			preq.preq_hopcount = 0;
			preq.preq_ttl = ms->ms_ttl;
			preq.preq_id = ++hs->hs_preqid;
			IEEE80211_ADDR_COPY(preq.preq_origaddr, vap->iv_myaddr);
			preq.preq_origseq = hr->hr_origseq;
			preq.preq_lifetime = rt->rt_lifetime;
			preq.preq_metric = rt->rt_metric;
			preq.preq_tcount = 1;
			IEEE80211_ADDR_COPY(PREQ_TADDR(0), dest);
			PREQ_TFLAGS(0) = 0;
			if (ieee80211_hwmp_targetonly)
				PREQ_TFLAGS(0) |= IEEE80211_MESHPREQ_TFLAGS_TO;
			if (ieee80211_hwmp_replyforward)
				PREQ_TFLAGS(0) |= IEEE80211_MESHPREQ_TFLAGS_RF;
			PREQ_TFLAGS(0) |= IEEE80211_MESHPREQ_TFLAGS_USN;
			PREQ_TSEQ(0) = 0;
			/* XXX check return value */
			hwmp_send_preq(vap->iv_bss, vap->iv_myaddr,
			    broadcastaddr, &preq);
		}
		if (rt->rt_flags & IEEE80211_MESHRT_FLAGS_VALID)
			ni = ieee80211_find_txnode(vap, rt->rt_nexthop);
	} else {
		ni = ieee80211_find_txnode(vap, dest);
		/* NB: if null then we leak mbuf */
		KASSERT(ni != NULL, ("leak mcast frame"));
		return ni;
	}
done:
	if (ni == NULL && m != NULL) {
		if (sendpreq) {
			struct ieee80211com *ic = vap->iv_ic;
			/*
			 * Queue packet for transmit when path discovery
			 * completes.  If discovery never completes the
			 * frame will be flushed by way of the aging timer.
			 */
			IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_HWMP, dest,
			    "%s", "queue frame until path found");
			m->m_pkthdr.rcvif = (void *)(uintptr_t)
			    ieee80211_mac_hash(ic, dest);
			/* XXX age chosen randomly */
			ieee80211_ageq_append(&ic->ic_stageq, m,
			    IEEE80211_INACT_WAIT);
		} else {
			IEEE80211_DISCARD_MAC(vap, IEEE80211_MSG_HWMP,
			    dest, NULL, "%s", "no valid path to this node");
			m_freem(m);
		}
	}
	return ni;
}
#undef	PREQ_TFLAGS
#undef	PREQ_TADDR
#undef	PREQ_TSEQ

static int
hwmp_ioctl_get80211(struct ieee80211vap *vap, struct ieee80211req *ireq)
{
	struct ieee80211_hwmp_state *hs = vap->iv_hwmp;
	int error;
 
	if (vap->iv_opmode != IEEE80211_M_MBSS)
		return ENOSYS;
	error = 0;
	switch (ireq->i_type) {
	case IEEE80211_IOC_HWMP_ROOTMODE:
		ireq->i_val = hs->hs_rootmode;
		break;
	case IEEE80211_IOC_HWMP_MAXHOPS:
		ireq->i_val = hs->hs_maxhops;
		break;
	default:
		return ENOSYS;
	}
	return error;
}
IEEE80211_IOCTL_GET(hwmp, hwmp_ioctl_get80211);

static int
hwmp_ioctl_set80211(struct ieee80211vap *vap, struct ieee80211req *ireq)
{
	struct ieee80211_hwmp_state *hs = vap->iv_hwmp;
	int error;

	if (vap->iv_opmode != IEEE80211_M_MBSS)
		return ENOSYS;
	error = 0;
	switch (ireq->i_type) {
	case IEEE80211_IOC_HWMP_ROOTMODE:
		if (ireq->i_val < 0 || ireq->i_val > 3)
			return EINVAL;
		hs->hs_rootmode = ireq->i_val;
		hwmp_rootmode_setup(vap);
		break;
	case IEEE80211_IOC_HWMP_MAXHOPS:
		if (ireq->i_val <= 0 || ireq->i_val > 255)
			return EINVAL;
		hs->hs_maxhops = ireq->i_val;
		break;
	default:
		return ENOSYS;
	}
	return error;
}
IEEE80211_IOCTL_SET(hwmp, hwmp_ioctl_set80211);
