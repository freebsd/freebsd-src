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
 * IEEE 802.11s Mesh Point (MBSS) support.
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

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_action.h>
#include <net80211/ieee80211_input.h>
#include <net80211/ieee80211_mesh.h>

static void	mesh_rt_flush_invalid(struct ieee80211vap *);
static int	mesh_select_proto_path(struct ieee80211vap *, const char *);
static int	mesh_select_proto_metric(struct ieee80211vap *, const char *);
static void	mesh_vattach(struct ieee80211vap *);
static int	mesh_newstate(struct ieee80211vap *, enum ieee80211_state, int);
static void	mesh_rt_cleanup_cb(void *);
static void	mesh_linkchange(struct ieee80211_node *,
		    enum ieee80211_mesh_mlstate);
static void	mesh_checkid(void *, struct ieee80211_node *);
static uint32_t	mesh_generateid(struct ieee80211vap *);
static int	mesh_checkpseq(struct ieee80211vap *,
		    const uint8_t [IEEE80211_ADDR_LEN], uint32_t);
static struct ieee80211_node *
		mesh_find_txnode(struct ieee80211vap *,
		    const uint8_t [IEEE80211_ADDR_LEN]);
static void	mesh_forward(struct ieee80211vap *, struct mbuf *,
		    const struct ieee80211_meshcntl *);
static int	mesh_input(struct ieee80211_node *, struct mbuf *, int, int);
static void	mesh_recv_mgmt(struct ieee80211_node *, struct mbuf *, int,
		    int, int);
static void	mesh_peer_timeout_setup(struct ieee80211_node *);
static void	mesh_peer_timeout_backoff(struct ieee80211_node *);
static void	mesh_peer_timeout_cb(void *);
static __inline void
		mesh_peer_timeout_stop(struct ieee80211_node *);
static int	mesh_verify_meshid(struct ieee80211vap *, const uint8_t *);
static int	mesh_verify_meshconf(struct ieee80211vap *, const uint8_t *);
static int	mesh_verify_meshpeer(struct ieee80211vap *, uint8_t,
    		    const uint8_t *);
uint32_t	mesh_airtime_calc(struct ieee80211_node *);

/*
 * Timeout values come from the specification and are in milliseconds.
 */
SYSCTL_NODE(_net_wlan, OID_AUTO, mesh, CTLFLAG_RD, 0,
    "IEEE 802.11s parameters");
static int ieee80211_mesh_retrytimeout = -1;
SYSCTL_PROC(_net_wlan_mesh, OID_AUTO, retrytimeout, CTLTYPE_INT | CTLFLAG_RW,
    &ieee80211_mesh_retrytimeout, 0, ieee80211_sysctl_msecs_ticks, "I",
    "Retry timeout (msec)");
static int ieee80211_mesh_holdingtimeout = -1;
SYSCTL_PROC(_net_wlan_mesh, OID_AUTO, holdingtimeout, CTLTYPE_INT | CTLFLAG_RW,
    &ieee80211_mesh_holdingtimeout, 0, ieee80211_sysctl_msecs_ticks, "I",
    "Holding state timeout (msec)");
static int ieee80211_mesh_confirmtimeout = -1;
SYSCTL_PROC(_net_wlan_mesh, OID_AUTO, confirmtimeout, CTLTYPE_INT | CTLFLAG_RW,
    &ieee80211_mesh_confirmtimeout, 0, ieee80211_sysctl_msecs_ticks, "I",
    "Confirm state timeout (msec)");
static int ieee80211_mesh_maxretries = 2;
SYSCTL_INT(_net_wlan_mesh, OID_AUTO, maxretries, CTLTYPE_INT | CTLFLAG_RW,
    &ieee80211_mesh_maxretries, 0,
    "Maximum retries during peer link establishment");

static const uint8_t broadcastaddr[IEEE80211_ADDR_LEN] =
	{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

static	ieee80211_recv_action_func mesh_recv_action_meshpeering_open;
static	ieee80211_recv_action_func mesh_recv_action_meshpeering_confirm;
static	ieee80211_recv_action_func mesh_recv_action_meshpeering_close;
static	ieee80211_recv_action_func mesh_recv_action_meshlmetric_req;
static	ieee80211_recv_action_func mesh_recv_action_meshlmetric_rep;

static	ieee80211_send_action_func mesh_send_action_meshpeering_open;
static	ieee80211_send_action_func mesh_send_action_meshpeering_confirm;
static	ieee80211_send_action_func mesh_send_action_meshpeering_close;
static	ieee80211_send_action_func mesh_send_action_meshlink_request;
static	ieee80211_send_action_func mesh_send_action_meshlink_reply;

static const struct ieee80211_mesh_proto_metric mesh_metric_airtime = {
	.mpm_descr	= "AIRTIME",
	.mpm_ie		= IEEE80211_MESHCONF_METRIC_AIRTIME,
	.mpm_metric	= mesh_airtime_calc,
};

static struct ieee80211_mesh_proto_path		mesh_proto_paths[4];
static struct ieee80211_mesh_proto_metric	mesh_proto_metrics[4];

#define	MESH_RT_LOCK(ms)	mtx_lock(&(ms)->ms_rt_lock)
#define	MESH_RT_LOCK_ASSERT(ms)	mtx_assert(&(ms)->ms_rt_lock, MA_OWNED)
#define	MESH_RT_UNLOCK(ms)	mtx_unlock(&(ms)->ms_rt_lock)

MALLOC_DEFINE(M_80211_MESH_RT, "80211mesh", "802.11s routing table");

/*
 * Helper functions to manipulate the Mesh routing table.
 */

static struct ieee80211_mesh_route *
mesh_rt_find_locked(struct ieee80211_mesh_state *ms,
    const uint8_t dest[IEEE80211_ADDR_LEN])
{
	struct ieee80211_mesh_route *rt;

	MESH_RT_LOCK_ASSERT(ms);

	TAILQ_FOREACH(rt, &ms->ms_routes, rt_next) {
		if (IEEE80211_ADDR_EQ(dest, rt->rt_dest))
			return rt;
	}
	return NULL;
}

static struct ieee80211_mesh_route *
mesh_rt_add_locked(struct ieee80211_mesh_state *ms,
    const uint8_t dest[IEEE80211_ADDR_LEN])
{
	struct ieee80211_mesh_route *rt;

	KASSERT(!IEEE80211_ADDR_EQ(broadcastaddr, dest),
	    ("%s: adding broadcast to the routing table", __func__));

	MESH_RT_LOCK_ASSERT(ms);

	rt = malloc(ALIGN(sizeof(struct ieee80211_mesh_route)) +
	    ms->ms_ppath->mpp_privlen, M_80211_MESH_RT, M_NOWAIT | M_ZERO);
	if (rt != NULL) {
		IEEE80211_ADDR_COPY(rt->rt_dest, dest);
		rt->rt_priv = (void *)ALIGN(&rt[1]);
		rt->rt_crtime = ticks;
		TAILQ_INSERT_TAIL(&ms->ms_routes, rt, rt_next);
	}
	return rt;
}

struct ieee80211_mesh_route *
ieee80211_mesh_rt_find(struct ieee80211vap *vap,
    const uint8_t dest[IEEE80211_ADDR_LEN])
{
	struct ieee80211_mesh_state *ms = vap->iv_mesh;
	struct ieee80211_mesh_route *rt;

	MESH_RT_LOCK(ms);
	rt = mesh_rt_find_locked(ms, dest);
	MESH_RT_UNLOCK(ms);
	return rt;
}

struct ieee80211_mesh_route *
ieee80211_mesh_rt_add(struct ieee80211vap *vap,
    const uint8_t dest[IEEE80211_ADDR_LEN])
{
	struct ieee80211_mesh_state *ms = vap->iv_mesh;
	struct ieee80211_mesh_route *rt;

	KASSERT(ieee80211_mesh_rt_find(vap, dest) == NULL,
	    ("%s: duplicate entry in the routing table", __func__));
	KASSERT(!IEEE80211_ADDR_EQ(vap->iv_myaddr, dest),
	    ("%s: adding self to the routing table", __func__));

	MESH_RT_LOCK(ms);
	rt = mesh_rt_add_locked(ms, dest);
	MESH_RT_UNLOCK(ms);
	return rt;
}

/*
 * Add a proxy route (as needed) for the specified destination.
 */
void
ieee80211_mesh_proxy_check(struct ieee80211vap *vap,
    const uint8_t dest[IEEE80211_ADDR_LEN])
{
	struct ieee80211_mesh_state *ms = vap->iv_mesh;
	struct ieee80211_mesh_route *rt;

	MESH_RT_LOCK(ms);
	rt = mesh_rt_find_locked(ms, dest);
	if (rt == NULL) {
		rt = mesh_rt_add_locked(ms, dest);
		if (rt == NULL) {
			IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_MESH, dest,
			    "%s", "unable to add proxy entry");
			vap->iv_stats.is_mesh_rtaddfailed++;
		} else {
			IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_MESH, dest,
			    "%s", "add proxy entry");
			IEEE80211_ADDR_COPY(rt->rt_nexthop, vap->iv_myaddr);
			rt->rt_flags |= IEEE80211_MESHRT_FLAGS_VALID
				     |  IEEE80211_MESHRT_FLAGS_PROXY;
		}
	/* XXX assert PROXY? */
	} else if ((rt->rt_flags & IEEE80211_MESHRT_FLAGS_VALID) == 0) {
		struct ieee80211com *ic = vap->iv_ic;
		/*
		 * Fix existing entry created by received frames from
		 * stations that have some memory of dest.  We also
		 * flush any frames held on the staging queue; delivering
		 * them is too much trouble right now.
		 */
		IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_MESH, dest,
		    "%s", "fix proxy entry");
		IEEE80211_ADDR_COPY(rt->rt_nexthop, vap->iv_myaddr);
		rt->rt_flags |= IEEE80211_MESHRT_FLAGS_VALID
			     |  IEEE80211_MESHRT_FLAGS_PROXY;
		/* XXX belongs in hwmp */
		ieee80211_ageq_drain_node(&ic->ic_stageq,
		   (void *)(uintptr_t) ieee80211_mac_hash(ic, dest));
		/* XXX stat? */
	}
	MESH_RT_UNLOCK(ms);
}

static __inline void
mesh_rt_del(struct ieee80211_mesh_state *ms, struct ieee80211_mesh_route *rt)
{
	TAILQ_REMOVE(&ms->ms_routes, rt, rt_next);
	free(rt, M_80211_MESH_RT);
}

void
ieee80211_mesh_rt_del(struct ieee80211vap *vap,
    const uint8_t dest[IEEE80211_ADDR_LEN])
{
	struct ieee80211_mesh_state *ms = vap->iv_mesh;
	struct ieee80211_mesh_route *rt, *next;

	MESH_RT_LOCK(ms);
	TAILQ_FOREACH_SAFE(rt, &ms->ms_routes, rt_next, next) {
		if (IEEE80211_ADDR_EQ(rt->rt_dest, dest)) {
			mesh_rt_del(ms, rt);
			MESH_RT_UNLOCK(ms);
			return;
		}
	}
	MESH_RT_UNLOCK(ms);
}

void
ieee80211_mesh_rt_flush(struct ieee80211vap *vap)
{
	struct ieee80211_mesh_state *ms = vap->iv_mesh;
	struct ieee80211_mesh_route *rt, *next;

	if (ms == NULL)
		return;
	MESH_RT_LOCK(ms);
	TAILQ_FOREACH_SAFE(rt, &ms->ms_routes, rt_next, next)
		mesh_rt_del(ms, rt);
	MESH_RT_UNLOCK(ms);
}

void
ieee80211_mesh_rt_flush_peer(struct ieee80211vap *vap,
    const uint8_t peer[IEEE80211_ADDR_LEN])
{
	struct ieee80211_mesh_state *ms = vap->iv_mesh;
	struct ieee80211_mesh_route *rt, *next;

	MESH_RT_LOCK(ms);
	TAILQ_FOREACH_SAFE(rt, &ms->ms_routes, rt_next, next) {
		if (IEEE80211_ADDR_EQ(rt->rt_nexthop, peer))
			mesh_rt_del(ms, rt);
	}
	MESH_RT_UNLOCK(ms);
}

/*
 * Flush expired routing entries, i.e. those in invalid state for
 * some time.
 */
static void
mesh_rt_flush_invalid(struct ieee80211vap *vap)
{
	struct ieee80211_mesh_state *ms = vap->iv_mesh;
	struct ieee80211_mesh_route *rt, *next;

	if (ms == NULL)
		return;
	MESH_RT_LOCK(ms);
	TAILQ_FOREACH_SAFE(rt, &ms->ms_routes, rt_next, next) {
		if ((rt->rt_flags & IEEE80211_MESHRT_FLAGS_VALID) == 0 &&
		    ticks - rt->rt_crtime >= ms->ms_ppath->mpp_inact)
			mesh_rt_del(ms, rt);
	}
	MESH_RT_UNLOCK(ms);
}

#define	N(a)	(sizeof(a) / sizeof(a[0]))
int
ieee80211_mesh_register_proto_path(const struct ieee80211_mesh_proto_path *mpp)
{
	int i, firstempty = -1;

	for (i = 0; i < N(mesh_proto_paths); i++) {
		if (strncmp(mpp->mpp_descr, mesh_proto_paths[i].mpp_descr,
		    IEEE80211_MESH_PROTO_DSZ) == 0)
			return EEXIST;
		if (!mesh_proto_paths[i].mpp_active && firstempty == -1)
			firstempty = i;
	}
	if (firstempty < 0)
		return ENOSPC;
	memcpy(&mesh_proto_paths[firstempty], mpp, sizeof(*mpp));
	mesh_proto_paths[firstempty].mpp_active = 1;
	return 0;
}

int
ieee80211_mesh_register_proto_metric(const struct
    ieee80211_mesh_proto_metric *mpm)
{
	int i, firstempty = -1;

	for (i = 0; i < N(mesh_proto_metrics); i++) {
		if (strncmp(mpm->mpm_descr, mesh_proto_metrics[i].mpm_descr,
		    IEEE80211_MESH_PROTO_DSZ) == 0)
			return EEXIST;
		if (!mesh_proto_metrics[i].mpm_active && firstempty == -1)
			firstempty = i;
	}
	if (firstempty < 0)
		return ENOSPC;
	memcpy(&mesh_proto_metrics[firstempty], mpm, sizeof(*mpm));
	mesh_proto_metrics[firstempty].mpm_active = 1;
	return 0;
}

static int
mesh_select_proto_path(struct ieee80211vap *vap, const char *name)
{
	struct ieee80211_mesh_state *ms = vap->iv_mesh;
	int i;

	for (i = 0; i < N(mesh_proto_paths); i++) {
		if (strcasecmp(mesh_proto_paths[i].mpp_descr, name) == 0) {
			ms->ms_ppath = &mesh_proto_paths[i];
			return 0;
		}
	}
	return ENOENT;
}

static int
mesh_select_proto_metric(struct ieee80211vap *vap, const char *name)
{
	struct ieee80211_mesh_state *ms = vap->iv_mesh;
	int i;

	for (i = 0; i < N(mesh_proto_metrics); i++) {
		if (strcasecmp(mesh_proto_metrics[i].mpm_descr, name) == 0) {
			ms->ms_pmetric = &mesh_proto_metrics[i];
			return 0;
		}
	}
	return ENOENT;
}
#undef	N

static void
ieee80211_mesh_init(void)
{

	memset(mesh_proto_paths, 0, sizeof(mesh_proto_paths));
	memset(mesh_proto_metrics, 0, sizeof(mesh_proto_metrics));

	/*
	 * Setup mesh parameters that depends on the clock frequency.
	 */
	ieee80211_mesh_retrytimeout = msecs_to_ticks(40);
	ieee80211_mesh_holdingtimeout = msecs_to_ticks(40);
	ieee80211_mesh_confirmtimeout = msecs_to_ticks(40);

	/*
	 * Register action frame handlers.
	 */
	ieee80211_recv_action_register(IEEE80211_ACTION_CAT_MESHPEERING,
	    IEEE80211_ACTION_MESHPEERING_OPEN,
	    mesh_recv_action_meshpeering_open);
	ieee80211_recv_action_register(IEEE80211_ACTION_CAT_MESHPEERING,
	    IEEE80211_ACTION_MESHPEERING_CONFIRM,
	    mesh_recv_action_meshpeering_confirm);
	ieee80211_recv_action_register(IEEE80211_ACTION_CAT_MESHPEERING,
	    IEEE80211_ACTION_MESHPEERING_CLOSE,
	    mesh_recv_action_meshpeering_close);
	ieee80211_recv_action_register(IEEE80211_ACTION_CAT_MESHLMETRIC,
	    IEEE80211_ACTION_MESHLMETRIC_REQ, mesh_recv_action_meshlmetric_req);
	ieee80211_recv_action_register(IEEE80211_ACTION_CAT_MESHLMETRIC,
	    IEEE80211_ACTION_MESHLMETRIC_REP, mesh_recv_action_meshlmetric_rep);

	ieee80211_send_action_register(IEEE80211_ACTION_CAT_MESHPEERING, 
	    IEEE80211_ACTION_MESHPEERING_OPEN,
	    mesh_send_action_meshpeering_open);
	ieee80211_send_action_register(IEEE80211_ACTION_CAT_MESHPEERING, 
	    IEEE80211_ACTION_MESHPEERING_CONFIRM,
	    mesh_send_action_meshpeering_confirm);
	ieee80211_send_action_register(IEEE80211_ACTION_CAT_MESHPEERING, 
	    IEEE80211_ACTION_MESHPEERING_CLOSE,
	    mesh_send_action_meshpeering_close);
	ieee80211_send_action_register(IEEE80211_ACTION_CAT_MESHLMETRIC, 
	    IEEE80211_ACTION_MESHLMETRIC_REQ,
	    mesh_send_action_meshlink_request);
	ieee80211_send_action_register(IEEE80211_ACTION_CAT_MESHLMETRIC, 
	    IEEE80211_ACTION_MESHLMETRIC_REP,
	    mesh_send_action_meshlink_reply);

	/*
	 * Register Airtime Link Metric.
	 */
	ieee80211_mesh_register_proto_metric(&mesh_metric_airtime);

}
SYSINIT(wlan_mesh, SI_SUB_DRIVERS, SI_ORDER_FIRST, ieee80211_mesh_init, NULL);

void
ieee80211_mesh_attach(struct ieee80211com *ic)
{
	ic->ic_vattach[IEEE80211_M_MBSS] = mesh_vattach;
}

void
ieee80211_mesh_detach(struct ieee80211com *ic)
{
}

static void
mesh_vdetach_peers(void *arg, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;
	uint16_t args[3];

	if (ni->ni_mlstate == IEEE80211_NODE_MESH_ESTABLISHED) {
		args[0] = ni->ni_mlpid;
		args[1] = ni->ni_mllid;
		args[2] = IEEE80211_REASON_PEER_LINK_CANCELED;
		ieee80211_send_action(ni,
		    IEEE80211_ACTION_CAT_MESHPEERING,
		    IEEE80211_ACTION_MESHPEERING_CLOSE,
		    args);
	}
	callout_drain(&ni->ni_mltimer);
	/* XXX belongs in hwmp */
	ieee80211_ageq_drain_node(&ic->ic_stageq,
	   (void *)(uintptr_t) ieee80211_mac_hash(ic, ni->ni_macaddr));
}

static void
mesh_vdetach(struct ieee80211vap *vap)
{
	struct ieee80211_mesh_state *ms = vap->iv_mesh;

	callout_drain(&ms->ms_cleantimer);
	ieee80211_iterate_nodes(&vap->iv_ic->ic_sta, mesh_vdetach_peers,
	    NULL);
	ieee80211_mesh_rt_flush(vap);
	mtx_destroy(&ms->ms_rt_lock);
	ms->ms_ppath->mpp_vdetach(vap);
	free(vap->iv_mesh, M_80211_VAP);
	vap->iv_mesh = NULL;
}

static void
mesh_vattach(struct ieee80211vap *vap)
{
	struct ieee80211_mesh_state *ms;
	vap->iv_newstate = mesh_newstate;
	vap->iv_input = mesh_input;
	vap->iv_opdetach = mesh_vdetach;
	vap->iv_recv_mgmt = mesh_recv_mgmt;
	ms = malloc(sizeof(struct ieee80211_mesh_state), M_80211_VAP,
	    M_NOWAIT | M_ZERO);
	if (ms == NULL) {
		printf("%s: couldn't alloc MBSS state\n", __func__);
		return;
	}
	vap->iv_mesh = ms;
	ms->ms_seq = 0;
	ms->ms_flags = (IEEE80211_MESHFLAGS_AP | IEEE80211_MESHFLAGS_FWD);
	ms->ms_ttl = IEEE80211_MESH_DEFAULT_TTL;
	TAILQ_INIT(&ms->ms_routes);
	mtx_init(&ms->ms_rt_lock, "MBSS", "802.11s routing table", MTX_DEF);
	callout_init(&ms->ms_cleantimer, CALLOUT_MPSAFE);
	mesh_select_proto_metric(vap, "AIRTIME");
	KASSERT(ms->ms_pmetric, ("ms_pmetric == NULL"));
	mesh_select_proto_path(vap, "HWMP");
	KASSERT(ms->ms_ppath, ("ms_ppath == NULL"));
	ms->ms_ppath->mpp_vattach(vap);
}

/*
 * IEEE80211_M_MBSS vap state machine handler.
 */
static int
mesh_newstate(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	struct ieee80211_mesh_state *ms = vap->iv_mesh;
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node *ni;
	enum ieee80211_state ostate;

	IEEE80211_LOCK_ASSERT(ic);

	ostate = vap->iv_state;
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_STATE, "%s: %s -> %s (%d)\n",
	    __func__, ieee80211_state_name[ostate],
	    ieee80211_state_name[nstate], arg);
	vap->iv_state = nstate;		/* state transition */
	if (ostate != IEEE80211_S_SCAN)
		ieee80211_cancel_scan(vap);	/* background scan */
	ni = vap->iv_bss;			/* NB: no reference held */
	if (nstate != IEEE80211_S_RUN && ostate == IEEE80211_S_RUN)
		callout_drain(&ms->ms_cleantimer);
	switch (nstate) {
	case IEEE80211_S_INIT:
		switch (ostate) {
		case IEEE80211_S_SCAN:
			ieee80211_cancel_scan(vap);
			break;
		case IEEE80211_S_CAC:
			ieee80211_dfs_cac_stop(vap);
			break;
		case IEEE80211_S_RUN:
			ieee80211_iterate_nodes(&ic->ic_sta,
			    mesh_vdetach_peers, NULL);
			break;
		default:
			break;
		}
		if (ostate != IEEE80211_S_INIT) {
			/* NB: optimize INIT -> INIT case */
			ieee80211_reset_bss(vap);
			ieee80211_mesh_rt_flush(vap);
		}
		break;
	case IEEE80211_S_SCAN:
		switch (ostate) {
		case IEEE80211_S_INIT:
			if (vap->iv_des_chan != IEEE80211_CHAN_ANYC &&
			    !IEEE80211_IS_CHAN_RADAR(vap->iv_des_chan) &&
			    ms->ms_idlen != 0) {
				/*
				 * Already have a channel and a mesh ID; bypass
				 * the scan and startup immediately.
				 */
				ieee80211_create_ibss(vap, vap->iv_des_chan);
				break;
			}
			/*
			 * Initiate a scan.  We can come here as a result
			 * of an IEEE80211_IOC_SCAN_REQ too in which case
			 * the vap will be marked with IEEE80211_FEXT_SCANREQ
			 * and the scan request parameters will be present
			 * in iv_scanreq.  Otherwise we do the default.
			*/
			if (vap->iv_flags_ext & IEEE80211_FEXT_SCANREQ) {
				ieee80211_check_scan(vap,
				    vap->iv_scanreq_flags,
				    vap->iv_scanreq_duration,
				    vap->iv_scanreq_mindwell,
				    vap->iv_scanreq_maxdwell,
				    vap->iv_scanreq_nssid, vap->iv_scanreq_ssid);
				vap->iv_flags_ext &= ~IEEE80211_FEXT_SCANREQ;
			} else
				ieee80211_check_scan_current(vap);
			break;
		default:
			break;
		}
		break;
	case IEEE80211_S_CAC:
		/*
		 * Start CAC on a DFS channel.  We come here when starting
		 * a bss on a DFS channel (see ieee80211_create_ibss).
		 */
		ieee80211_dfs_cac_start(vap);
		break;
	case IEEE80211_S_RUN:
		switch (ostate) {
		case IEEE80211_S_INIT:
			/*
			 * Already have a channel; bypass the
			 * scan and startup immediately.
			 * Note that ieee80211_create_ibss will call
			 * back to do a RUN->RUN state change.
			 */
			ieee80211_create_ibss(vap,
			    ieee80211_ht_adjust_channel(ic,
				ic->ic_curchan, vap->iv_flags_ht));
			/* NB: iv_bss is changed on return */
			break;
		case IEEE80211_S_CAC:
			/*
			 * NB: This is the normal state change when CAC
			 * expires and no radar was detected; no need to
			 * clear the CAC timer as it's already expired.
			 */
			/* fall thru... */
		case IEEE80211_S_CSA:
#if 0
			/*
			 * Shorten inactivity timer of associated stations
			 * to weed out sta's that don't follow a CSA.
			 */
			ieee80211_iterate_nodes(&ic->ic_sta, sta_csa, vap);
#endif
			/*
			 * Update bss node channel to reflect where
			 * we landed after CSA.
			 */
			ieee80211_node_set_chan(vap->iv_bss,
			    ieee80211_ht_adjust_channel(ic, ic->ic_curchan,
				ieee80211_htchanflags(vap->iv_bss->ni_chan)));
			/* XXX bypass debug msgs */
			break;
		case IEEE80211_S_SCAN:
		case IEEE80211_S_RUN:
#ifdef IEEE80211_DEBUG
			if (ieee80211_msg_debug(vap)) {
				struct ieee80211_node *ni = vap->iv_bss;
				ieee80211_note(vap,
				    "synchronized with %s meshid ",
				    ether_sprintf(ni->ni_meshid));
				ieee80211_print_essid(ni->ni_meshid,
				    ni->ni_meshidlen);
				/* XXX MCS/HT */
				printf(" channel %d\n",
				    ieee80211_chan2ieee(ic, ic->ic_curchan));
			}
#endif
			break;
		default:
			break;
		}
		ieee80211_node_authorize(vap->iv_bss);
		callout_reset(&ms->ms_cleantimer, ms->ms_ppath->mpp_inact,
                    mesh_rt_cleanup_cb, vap);
		break;
	default:
		break;
	}
	/* NB: ostate not nstate */
	ms->ms_ppath->mpp_newstate(vap, ostate, arg);
	return 0;
}

static void
mesh_rt_cleanup_cb(void *arg)
{
	struct ieee80211vap *vap = arg;
	struct ieee80211_mesh_state *ms = vap->iv_mesh;

	mesh_rt_flush_invalid(vap);
	callout_reset(&ms->ms_cleantimer, ms->ms_ppath->mpp_inact,
	    mesh_rt_cleanup_cb, vap);
}


/*
 * Helper function to note the Mesh Peer Link FSM change.
 */
static void
mesh_linkchange(struct ieee80211_node *ni, enum ieee80211_mesh_mlstate state)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211_mesh_state *ms = vap->iv_mesh;
#ifdef IEEE80211_DEBUG
	static const char *meshlinkstates[] = {
		[IEEE80211_NODE_MESH_IDLE]		= "IDLE",
		[IEEE80211_NODE_MESH_OPENSNT]		= "OPEN SENT",
		[IEEE80211_NODE_MESH_OPENRCV]		= "OPEN RECEIVED",
		[IEEE80211_NODE_MESH_CONFIRMRCV]	= "CONFIRM RECEIVED",
		[IEEE80211_NODE_MESH_ESTABLISHED]	= "ESTABLISHED",
		[IEEE80211_NODE_MESH_HOLDING]		= "HOLDING"
	};
#endif
	IEEE80211_NOTE(vap, IEEE80211_MSG_MESH,
	    ni, "peer link: %s -> %s",
	    meshlinkstates[ni->ni_mlstate], meshlinkstates[state]);

	/* track neighbor count */
	if (state == IEEE80211_NODE_MESH_ESTABLISHED &&
	    ni->ni_mlstate != IEEE80211_NODE_MESH_ESTABLISHED) {
		KASSERT(ms->ms_neighbors < 65535, ("neighbor count overflow"));
		ms->ms_neighbors++;
		ieee80211_beacon_notify(vap, IEEE80211_BEACON_MESHCONF);
	} else if (ni->ni_mlstate == IEEE80211_NODE_MESH_ESTABLISHED &&
	    state != IEEE80211_NODE_MESH_ESTABLISHED) {
		KASSERT(ms->ms_neighbors > 0, ("neighbor count 0"));
		ms->ms_neighbors--;
		ieee80211_beacon_notify(vap, IEEE80211_BEACON_MESHCONF);
	}
	ni->ni_mlstate = state;
	switch (state) {
	case IEEE80211_NODE_MESH_HOLDING:
		ms->ms_ppath->mpp_peerdown(ni);
		break;
	case IEEE80211_NODE_MESH_ESTABLISHED:
		ieee80211_mesh_discover(vap, ni->ni_macaddr, NULL);
		break;
	default:
		break;
	}
}

/*
 * Helper function to generate a unique local ID required for mesh
 * peer establishment.
 */
static void
mesh_checkid(void *arg, struct ieee80211_node *ni)
{
	uint16_t *r = arg;
	
	if (*r == ni->ni_mllid)
		*(uint16_t *)arg = 0;
}

static uint32_t
mesh_generateid(struct ieee80211vap *vap)
{
	int maxiter = 4;
	uint16_t r;

	do {
		get_random_bytes(&r, 2);
		ieee80211_iterate_nodes(&vap->iv_ic->ic_sta, mesh_checkid, &r);
		maxiter--;
	} while (r == 0 && maxiter > 0);
	return r;
}

/*
 * Verifies if we already received this packet by checking its
 * sequence number.
 * Returns 0 if the frame is to be accepted, 1 otherwise.
 */
static int
mesh_checkpseq(struct ieee80211vap *vap,
    const uint8_t source[IEEE80211_ADDR_LEN], uint32_t seq)
{
	struct ieee80211_mesh_route *rt;

	rt = ieee80211_mesh_rt_find(vap, source);
	if (rt == NULL) {
		rt = ieee80211_mesh_rt_add(vap, source);
		if (rt == NULL) {
			IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_MESH, source,
			    "%s", "add mcast route failed");
			vap->iv_stats.is_mesh_rtaddfailed++;
			return 1;
		}
		IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_MESH, source,
		    "add mcast route, mesh seqno %d", seq);
		rt->rt_lastmseq = seq;
		return 0;
	}
	if (IEEE80211_MESH_SEQ_GEQ(rt->rt_lastmseq, seq)) {
		return 1;
	} else {
		rt->rt_lastmseq = seq;
		return 0;
	}
}

/*
 * Iterate the routing table and locate the next hop.
 */
static struct ieee80211_node *
mesh_find_txnode(struct ieee80211vap *vap,
    const uint8_t dest[IEEE80211_ADDR_LEN])
{
	struct ieee80211_mesh_route *rt;

	rt = ieee80211_mesh_rt_find(vap, dest);
	if (rt == NULL)
		return NULL;
	if ((rt->rt_flags & IEEE80211_MESHRT_FLAGS_VALID) == 0 ||
	    (rt->rt_flags & IEEE80211_MESHRT_FLAGS_PROXY)) {
		IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_MESH, dest,
		    "%s: !valid or proxy, flags 0x%x", __func__, rt->rt_flags);
		/* XXX stat */
		return NULL;
	}
	return ieee80211_find_txnode(vap, rt->rt_nexthop);
}

/*
 * Forward the specified frame.
 * Decrement the TTL and set TA to our MAC address.
 */
static void
mesh_forward(struct ieee80211vap *vap, struct mbuf *m,
    const struct ieee80211_meshcntl *mc)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_mesh_state *ms = vap->iv_mesh;
	struct ifnet *ifp = vap->iv_ifp;
	struct ifnet *parent = ic->ic_ifp;
	const struct ieee80211_frame *wh =
	    mtod(m, const struct ieee80211_frame *);
	struct mbuf *mcopy;
	struct ieee80211_meshcntl *mccopy;
	struct ieee80211_frame *whcopy;
	struct ieee80211_node *ni;
	int err;

	if (mc->mc_ttl == 0) {
		IEEE80211_NOTE_FRAME(vap, IEEE80211_MSG_MESH, wh,
		    "%s", "frame not fwd'd, ttl 0");
		vap->iv_stats.is_mesh_fwd_ttl++;
		return;
	}
	if (!(ms->ms_flags & IEEE80211_MESHFLAGS_FWD)) {
		IEEE80211_NOTE_FRAME(vap, IEEE80211_MSG_MESH, wh,
		    "%s", "frame not fwd'd, fwding disabled");
		vap->iv_stats.is_mesh_fwd_disabled++;
		return;
	}
	mcopy = m_dup(m, M_DONTWAIT);
	if (mcopy == NULL) {
		IEEE80211_NOTE_FRAME(vap, IEEE80211_MSG_MESH, wh,
		    "%s", "frame not fwd'd, cannot dup");
		vap->iv_stats.is_mesh_fwd_nobuf++;
		ifp->if_oerrors++;
		return;
	}
	mcopy = m_pullup(mcopy, ieee80211_hdrspace(ic, wh) +
	    sizeof(struct ieee80211_meshcntl));
	if (mcopy == NULL) {
		IEEE80211_NOTE_FRAME(vap, IEEE80211_MSG_MESH, wh,
		    "%s", "frame not fwd'd, too short");
		vap->iv_stats.is_mesh_fwd_tooshort++;
		ifp->if_oerrors++;
		m_freem(mcopy);
		return;
	}
	whcopy = mtod(mcopy, struct ieee80211_frame *);
	mccopy = (struct ieee80211_meshcntl *)
	    (mtod(mcopy, uint8_t *) + ieee80211_hdrspace(ic, wh));
	/* XXX clear other bits? */
	whcopy->i_fc[1] &= ~IEEE80211_FC1_RETRY;
	IEEE80211_ADDR_COPY(whcopy->i_addr2, vap->iv_myaddr);
	if (IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		ni = ieee80211_ref_node(vap->iv_bss);
		mcopy->m_flags |= M_MCAST;
	} else {
		ni = mesh_find_txnode(vap, whcopy->i_addr3);
		if (ni == NULL) {
			IEEE80211_NOTE_FRAME(vap, IEEE80211_MSG_MESH, wh,
			    "%s", "frame not fwd'd, no path");
			vap->iv_stats.is_mesh_fwd_nopath++;
			m_freem(mcopy);
			return;
		}
		IEEE80211_ADDR_COPY(whcopy->i_addr1, ni->ni_macaddr);
	}
	KASSERT(mccopy->mc_ttl > 0, ("%s called with wrong ttl", __func__));
	mccopy->mc_ttl--;

	/* XXX calculate priority so drivers can find the tx queue */
	M_WME_SETAC(mcopy, WME_AC_BE);

	/* XXX do we know m_nextpkt is NULL? */
	mcopy->m_pkthdr.rcvif = (void *) ni;
	err = parent->if_transmit(parent, mcopy);
	if (err != 0) {
		/* NB: IFQ_HANDOFF reclaims mbuf */
		ieee80211_free_node(ni);
	} else {
		ifp->if_opackets++;
	}
}

static struct mbuf *
mesh_decap(struct ieee80211vap *vap, struct mbuf *m, int hdrlen, int meshdrlen)
{
#define	WHDIR(wh) ((wh)->i_fc[1] & IEEE80211_FC1_DIR_MASK)
	uint8_t b[sizeof(struct ieee80211_qosframe_addr4) +
		  sizeof(struct ieee80211_meshcntl_ae11)];
	const struct ieee80211_qosframe_addr4 *wh;
	const struct ieee80211_meshcntl_ae10 *mc;
	struct ether_header *eh;
	struct llc *llc;
	int ae;

	if (m->m_len < hdrlen + sizeof(*llc) &&
	    (m = m_pullup(m, hdrlen + sizeof(*llc))) == NULL) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
		    "discard data frame: %s", "m_pullup failed");
		vap->iv_stats.is_rx_tooshort++;
		return NULL;
	}
	memcpy(b, mtod(m, caddr_t), hdrlen);
	wh = (const struct ieee80211_qosframe_addr4 *)&b[0];
	mc = (const struct ieee80211_meshcntl_ae10 *)&b[hdrlen - meshdrlen];
	KASSERT(WHDIR(wh) == IEEE80211_FC1_DIR_FROMDS ||
		WHDIR(wh) == IEEE80211_FC1_DIR_DSTODS,
	    ("bogus dir, fc 0x%x:0x%x", wh->i_fc[0], wh->i_fc[1]));

	llc = (struct llc *)(mtod(m, caddr_t) + hdrlen);
	if (llc->llc_dsap == LLC_SNAP_LSAP && llc->llc_ssap == LLC_SNAP_LSAP &&
	    llc->llc_control == LLC_UI && llc->llc_snap.org_code[0] == 0 &&
	    llc->llc_snap.org_code[1] == 0 && llc->llc_snap.org_code[2] == 0 &&
	    /* NB: preserve AppleTalk frames that have a native SNAP hdr */
	    !(llc->llc_snap.ether_type == htons(ETHERTYPE_AARP) ||
	      llc->llc_snap.ether_type == htons(ETHERTYPE_IPX))) {
		m_adj(m, hdrlen + sizeof(struct llc) - sizeof(*eh));
		llc = NULL;
	} else {
		m_adj(m, hdrlen - sizeof(*eh));
	}
	eh = mtod(m, struct ether_header *);
	ae = mc->mc_flags & 3;
	if (WHDIR(wh) == IEEE80211_FC1_DIR_FROMDS) {
		IEEE80211_ADDR_COPY(eh->ether_dhost, wh->i_addr1);
		if (ae == 0) {
			IEEE80211_ADDR_COPY(eh->ether_shost, wh->i_addr3);
		} else if (ae == 1) {
			IEEE80211_ADDR_COPY(eh->ether_shost, mc->mc_addr4);
		} else {
			IEEE80211_DISCARD(vap, IEEE80211_MSG_ANY,
			    (const struct ieee80211_frame *)wh, NULL,
			    "bad AE %d", ae);
			vap->iv_stats.is_mesh_badae++;
			m_freem(m);
			return NULL;
		}
	} else {
		if (ae == 0) {
			IEEE80211_ADDR_COPY(eh->ether_dhost, wh->i_addr3);
			IEEE80211_ADDR_COPY(eh->ether_shost, wh->i_addr4);
		} else if (ae == 2) {
			IEEE80211_ADDR_COPY(eh->ether_dhost, mc->mc_addr4);
			IEEE80211_ADDR_COPY(eh->ether_shost, mc->mc_addr5);
		} else {
			IEEE80211_DISCARD(vap, IEEE80211_MSG_ANY,
			    (const struct ieee80211_frame *)wh, NULL,
			    "bad AE %d", ae);
			vap->iv_stats.is_mesh_badae++;
			m_freem(m);
			return NULL;
		}
	}
#ifdef ALIGNED_POINTER
	if (!ALIGNED_POINTER(mtod(m, caddr_t) + sizeof(*eh), uint32_t)) {
		m = ieee80211_realign(vap, m, sizeof(*eh));
		if (m == NULL)
			return NULL;
	}
#endif /* ALIGNED_POINTER */
	if (llc != NULL) {
		eh = mtod(m, struct ether_header *);
		eh->ether_type = htons(m->m_pkthdr.len - sizeof(*eh));
	}
	return m;
#undef WDIR
}

/*
 * Return non-zero if the unicast mesh data frame should be processed
 * locally.  Frames that are not proxy'd have our address, otherwise
 * we need to consult the routing table to look for a proxy entry.
 */
static __inline int
mesh_isucastforme(struct ieee80211vap *vap, const struct ieee80211_frame *wh,
    const struct ieee80211_meshcntl *mc)
{
	int ae = mc->mc_flags & 3;

	KASSERT((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) == IEEE80211_FC1_DIR_DSTODS,
	    ("bad dir 0x%x:0x%x", wh->i_fc[0], wh->i_fc[1]));
	KASSERT(ae == 0 || ae == 2, ("bad AE %d", ae));
	if (ae == 2) {				/* ucast w/ proxy */
		const struct ieee80211_meshcntl_ae10 *mc10 =
		    (const struct ieee80211_meshcntl_ae10 *) mc;
		struct ieee80211_mesh_route *rt =
		    ieee80211_mesh_rt_find(vap, mc10->mc_addr4);
		/* check for proxy route to ourself */
		return (rt != NULL &&
		    (rt->rt_flags & IEEE80211_MESHRT_FLAGS_PROXY));
	} else					/* ucast w/o proxy */
		return IEEE80211_ADDR_EQ(wh->i_addr3, vap->iv_myaddr);
}

static int
mesh_input(struct ieee80211_node *ni, struct mbuf *m, int rssi, int nf)
{
#define	SEQ_LEQ(a,b)	((int)((a)-(b)) <= 0)
#define	HAS_SEQ(type)	((type & 0x4) == 0)
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = ni->ni_ic;
	struct ifnet *ifp = vap->iv_ifp;
	struct ieee80211_frame *wh;
	const struct ieee80211_meshcntl *mc;
	int hdrspace, meshdrlen, need_tap;
	uint8_t dir, type, subtype, qos;
	uint32_t seq;
	uint8_t *addr;
	ieee80211_seq rxseq;

	KASSERT(ni != NULL, ("null node"));
	ni->ni_inact = ni->ni_inact_reload;

	need_tap = 1;			/* mbuf need to be tapped. */
	type = -1;			/* undefined */

	if (m->m_pkthdr.len < sizeof(struct ieee80211_frame_min)) {
		IEEE80211_DISCARD_MAC(vap, IEEE80211_MSG_ANY,
		    ni->ni_macaddr, NULL,
		    "too short (1): len %u", m->m_pkthdr.len);
		vap->iv_stats.is_rx_tooshort++;
		goto out;
	}
	/*
	 * Bit of a cheat here, we use a pointer for a 3-address
	 * frame format but don't reference fields past outside
	 * ieee80211_frame_min w/o first validating the data is
	 * present.
	*/
	wh = mtod(m, struct ieee80211_frame *);

	if ((wh->i_fc[0] & IEEE80211_FC0_VERSION_MASK) !=
	    IEEE80211_FC0_VERSION_0) {
		IEEE80211_DISCARD_MAC(vap, IEEE80211_MSG_ANY,
		    ni->ni_macaddr, NULL, "wrong version %x", wh->i_fc[0]);
		vap->iv_stats.is_rx_badversion++;
		goto err;
	}
	dir = wh->i_fc[1] & IEEE80211_FC1_DIR_MASK;
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
	if ((ic->ic_flags & IEEE80211_F_SCAN) == 0) {
		IEEE80211_RSSI_LPF(ni->ni_avgrssi, rssi);
		ni->ni_noise = nf;
		if (HAS_SEQ(type)) {
			uint8_t tid = ieee80211_gettid(wh);

			if (IEEE80211_QOS_HAS_SEQ(wh) &&
			    TID_TO_WME_AC(tid) >= WME_AC_VI)
				ic->ic_wme.wme_hipri_traffic++;
			rxseq = le16toh(*(uint16_t *)wh->i_seq);
			if ((ni->ni_flags & IEEE80211_NODE_HT) == 0 &&
			    (wh->i_fc[1] & IEEE80211_FC1_RETRY) &&
			    SEQ_LEQ(rxseq, ni->ni_rxseqs[tid])) {
				/* duplicate, discard */
				IEEE80211_DISCARD_MAC(vap, IEEE80211_MSG_INPUT,
				    wh->i_addr1, "duplicate",
				    "seqno <%u,%u> fragno <%u,%u> tid %u",
				    rxseq >> IEEE80211_SEQ_SEQ_SHIFT,
				    ni->ni_rxseqs[tid] >>
				    IEEE80211_SEQ_SEQ_SHIFT,
				    rxseq & IEEE80211_SEQ_FRAG_MASK,
				    ni->ni_rxseqs[tid] &
				    IEEE80211_SEQ_FRAG_MASK,
				    tid);
				vap->iv_stats.is_rx_dup++;
				IEEE80211_NODE_STAT(ni, rx_dup);
				goto out;
			}
			ni->ni_rxseqs[tid] = rxseq;
		}
	}
#ifdef IEEE80211_DEBUG
	/*
	 * It's easier, but too expensive, to simulate different mesh
	 * topologies by consulting the ACL policy very early, so do this
	 * only under DEBUG.
	 *
	 * NB: this check is also done upon peering link initiation.
	 */
	if (vap->iv_acl != NULL && !vap->iv_acl->iac_check(vap, wh->i_addr2)) {
		IEEE80211_DISCARD(vap, IEEE80211_MSG_ACL,
		    wh, NULL, "%s", "disallowed by ACL");
		vap->iv_stats.is_rx_acl++;
		goto out;
	}
#endif
	switch (type) {
	case IEEE80211_FC0_TYPE_DATA:
		if (ni == vap->iv_bss)
			goto out;
		if (ni->ni_mlstate != IEEE80211_NODE_MESH_ESTABLISHED) {
			IEEE80211_DISCARD_MAC(vap, IEEE80211_MSG_MESH,
			    ni->ni_macaddr, NULL,
			    "peer link not yet established (%d)",
			    ni->ni_mlstate);
			vap->iv_stats.is_mesh_nolink++;
			goto out;
		}	
		if (dir != IEEE80211_FC1_DIR_FROMDS &&
		    dir != IEEE80211_FC1_DIR_DSTODS) {
			IEEE80211_DISCARD(vap, IEEE80211_MSG_INPUT,
			    wh, "data", "incorrect dir 0x%x", dir);
			vap->iv_stats.is_rx_wrongdir++;
			goto err;
		}
		/* pull up enough to get to the mesh control */
		hdrspace = ieee80211_hdrspace(ic, wh);
		if (m->m_len < hdrspace + sizeof(struct ieee80211_meshcntl) &&
		    (m = m_pullup(m, hdrspace +
		        sizeof(struct ieee80211_meshcntl))) == NULL) {
			IEEE80211_DISCARD_MAC(vap, IEEE80211_MSG_ANY,
			    ni->ni_macaddr, NULL,
			    "data too short: expecting %u", hdrspace);
			vap->iv_stats.is_rx_tooshort++;
			goto out;		/* XXX */
		}
		/*
		 * Now calculate the full extent of the headers. Note
		 * mesh_decap will pull up anything we didn't get
		 * above when it strips the 802.11 headers.
		 */
		mc = (const struct ieee80211_meshcntl *)
		    (mtod(m, const uint8_t *) + hdrspace);
		meshdrlen = sizeof(struct ieee80211_meshcntl) +
		    (mc->mc_flags & 3) * IEEE80211_ADDR_LEN;
		hdrspace += meshdrlen;
		seq = LE_READ_4(mc->mc_seq);
		if (IEEE80211_IS_MULTICAST(wh->i_addr1))
			addr = wh->i_addr3;
		else
			addr = ((struct ieee80211_qosframe_addr4 *)wh)->i_addr4;
		if (IEEE80211_ADDR_EQ(vap->iv_myaddr, addr)) {
			IEEE80211_DISCARD_MAC(vap, IEEE80211_MSG_INPUT,
			    addr, "data", "%s", "not to me");
			vap->iv_stats.is_rx_wrongbss++;	/* XXX kinda */
			goto out;
		}
		if (mesh_checkpseq(vap, addr, seq) != 0) {
			vap->iv_stats.is_rx_dup++;
			goto out;
		}

		/*
		 * Potentially forward packet.  See table s36 (p140)
		 * for the rules.  XXX tap fwd'd packets not for us?
		 */
		if (dir == IEEE80211_FC1_DIR_FROMDS ||
		    !mesh_isucastforme(vap, wh, mc)) {
			mesh_forward(vap, m, mc);
			if (dir == IEEE80211_FC1_DIR_DSTODS)
				goto out;
			/* NB: fall thru to deliver mcast frames locally */
		}

		/*
		 * Save QoS bits for use below--before we strip the header.
		 */
		if (subtype == IEEE80211_FC0_SUBTYPE_QOS) {
			qos = (dir == IEEE80211_FC1_DIR_DSTODS) ?
			    ((struct ieee80211_qosframe_addr4 *)wh)->i_qos[0] :
			    ((struct ieee80211_qosframe *)wh)->i_qos[0];
		} else
			qos = 0;
		/*
		 * Next up, any fragmentation.
		 */
		if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
			m = ieee80211_defrag(ni, m, hdrspace);
			if (m == NULL) {
				/* Fragment dropped or frame not complete yet */
				goto out;
			}
		}
		wh = NULL;		/* no longer valid, catch any uses */

		if (ieee80211_radiotap_active_vap(vap))
			ieee80211_radiotap_rx(vap, m);
		need_tap = 0;

		/*
		 * Finally, strip the 802.11 header.
		 */
		m = mesh_decap(vap, m, hdrspace, meshdrlen);
		if (m == NULL) {
			/* XXX mask bit to check for both */
			/* don't count Null data frames as errors */
			if (subtype == IEEE80211_FC0_SUBTYPE_NODATA ||
			    subtype == IEEE80211_FC0_SUBTYPE_QOS_NULL)
				goto out;
			IEEE80211_DISCARD_MAC(vap, IEEE80211_MSG_INPUT,
			    ni->ni_macaddr, "data", "%s", "decap error");
			vap->iv_stats.is_rx_decap++;
			IEEE80211_NODE_STAT(ni, rx_decap);
			goto err;
		}
		if (qos & IEEE80211_QOS_AMSDU) {
			m = ieee80211_decap_amsdu(ni, m);
			if (m == NULL)
				return IEEE80211_FC0_TYPE_DATA;
		}
		ieee80211_deliver_data(vap, ni, m);
		return type;
	case IEEE80211_FC0_TYPE_MGT:
		vap->iv_stats.is_rx_mgmt++;
		IEEE80211_NODE_STAT(ni, rx_mgmt);
		if (dir != IEEE80211_FC1_DIR_NODS) {
			IEEE80211_DISCARD(vap, IEEE80211_MSG_INPUT,
			    wh, "mgt", "incorrect dir 0x%x", dir);
			vap->iv_stats.is_rx_wrongdir++;
			goto err;
		}
		if (m->m_pkthdr.len < sizeof(struct ieee80211_frame)) {
			IEEE80211_DISCARD_MAC(vap, IEEE80211_MSG_ANY,
			    ni->ni_macaddr, "mgt", "too short: len %u",
			    m->m_pkthdr.len);
			vap->iv_stats.is_rx_tooshort++;
			goto out;
		}
#ifdef IEEE80211_DEBUG
		if ((ieee80211_msg_debug(vap) && 
		    (vap->iv_ic->ic_flags & IEEE80211_F_SCAN)) ||
		    ieee80211_msg_dumppkts(vap)) {
			if_printf(ifp, "received %s from %s rssi %d\n",
			    ieee80211_mgt_subtype_name[subtype >>
			    IEEE80211_FC0_SUBTYPE_SHIFT],
			    ether_sprintf(wh->i_addr2), rssi);
		}
#endif
		if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
			IEEE80211_DISCARD(vap, IEEE80211_MSG_INPUT,
			    wh, NULL, "%s", "WEP set but not permitted");
			vap->iv_stats.is_rx_mgtdiscard++; /* XXX */
			goto out;
		}
		vap->iv_recv_mgmt(ni, m, subtype, rssi, nf);
		goto out;
	case IEEE80211_FC0_TYPE_CTL:
		vap->iv_stats.is_rx_ctl++;
		IEEE80211_NODE_STAT(ni, rx_ctrl);
		goto out;
	default:
		IEEE80211_DISCARD(vap, IEEE80211_MSG_ANY,
		    wh, "bad", "frame type 0x%x", type);
		/* should not come here */
		break;
	}
err:
	ifp->if_ierrors++;
out:
	if (m != NULL) {
		if (need_tap && ieee80211_radiotap_active_vap(vap))
			ieee80211_radiotap_rx(vap, m);
		m_freem(m);
	}
	return type;
}

static void
mesh_recv_mgmt(struct ieee80211_node *ni, struct mbuf *m0, int subtype,
    int rssi, int nf)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211_mesh_state *ms = vap->iv_mesh;
	struct ieee80211com *ic = ni->ni_ic;
	struct ieee80211_frame *wh;
	uint8_t *frm, *efrm;

	wh = mtod(m0, struct ieee80211_frame *);
	frm = (uint8_t *)&wh[1];
	efrm = mtod(m0, uint8_t *) + m0->m_len;
	switch (subtype) {
	case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
	case IEEE80211_FC0_SUBTYPE_BEACON:
	{
		struct ieee80211_scanparams scan;
		/*
		 * We process beacon/probe response
		 * frames to discover neighbors.
		 */
		if (ieee80211_parse_beacon(ni, m0, &scan) != 0)
			return;
		/*
		 * Count frame now that we know it's to be processed.
		 */
		if (subtype == IEEE80211_FC0_SUBTYPE_BEACON) {
			vap->iv_stats.is_rx_beacon++;	/* XXX remove */
			IEEE80211_NODE_STAT(ni, rx_beacons);
		} else
			IEEE80211_NODE_STAT(ni, rx_proberesp);
		/*
		 * If scanning, just pass information to the scan module.
		 */
		if (ic->ic_flags & IEEE80211_F_SCAN) {
			if (ic->ic_flags_ext & IEEE80211_FEXT_PROBECHAN) {
				/*
				 * Actively scanning a channel marked passive;
				 * send a probe request now that we know there
				 * is 802.11 traffic present.
				 *
				 * XXX check if the beacon we recv'd gives
				 * us what we need and suppress the probe req
				 */
				ieee80211_probe_curchan(vap, 1);
				ic->ic_flags_ext &= ~IEEE80211_FEXT_PROBECHAN;
			}
			ieee80211_add_scan(vap, &scan, wh,
			    subtype, rssi, nf);
			return;
		}

		/* The rest of this code assumes we are running */
		if (vap->iv_state != IEEE80211_S_RUN)
			return;
		/*
		 * Ignore non-mesh STAs.
		 */
		if ((scan.capinfo &
		     (IEEE80211_CAPINFO_ESS|IEEE80211_CAPINFO_IBSS)) ||
		    scan.meshid == NULL || scan.meshconf == NULL) {
			IEEE80211_DISCARD(vap, IEEE80211_MSG_INPUT,
			    wh, "beacon", "%s", "not a mesh sta");
			vap->iv_stats.is_mesh_wrongmesh++;
			return;
		}
		/*
		 * Ignore STAs for other mesh networks.
		 */
		if (memcmp(scan.meshid+2, ms->ms_id, ms->ms_idlen) != 0 ||
		    mesh_verify_meshconf(vap, scan.meshconf)) {
			IEEE80211_DISCARD(vap, IEEE80211_MSG_INPUT,
			    wh, "beacon", "%s", "not for our mesh");
			vap->iv_stats.is_mesh_wrongmesh++;
			return;
		}
		/*
		 * Peer only based on the current ACL policy.
		 */
		if (vap->iv_acl != NULL &&
		    !vap->iv_acl->iac_check(vap, wh->i_addr2)) {
			IEEE80211_DISCARD(vap, IEEE80211_MSG_ACL,
			    wh, NULL, "%s", "disallowed by ACL");
			vap->iv_stats.is_rx_acl++;
			return;
		}
		/*
		 * Do neighbor discovery.
		 */
		if (!IEEE80211_ADDR_EQ(wh->i_addr2, ni->ni_macaddr)) {
			/*
			 * Create a new entry in the neighbor table.
			 */
			ni = ieee80211_add_neighbor(vap, wh, &scan);
		}
		/*
		 * Automatically peer with discovered nodes if possible.
		 * XXX backoff on repeated failure
		 */
		if (ni != vap->iv_bss &&
		    (ms->ms_flags & IEEE80211_MESHFLAGS_AP) &&
		    ni->ni_mlstate == IEEE80211_NODE_MESH_IDLE) {
			uint16_t args[1];

			ni->ni_mlpid = mesh_generateid(vap);
			if (ni->ni_mlpid == 0)
				return;
			mesh_linkchange(ni, IEEE80211_NODE_MESH_OPENSNT);
			args[0] = ni->ni_mlpid;
			ieee80211_send_action(ni,
			    IEEE80211_ACTION_CAT_MESHPEERING,
			    IEEE80211_ACTION_MESHPEERING_OPEN, args);
			ni->ni_mlrcnt = 0;
			mesh_peer_timeout_setup(ni);
		}
		break;
	}
	case IEEE80211_FC0_SUBTYPE_PROBE_REQ:
	{
		uint8_t *ssid, *meshid, *rates, *xrates;
		uint8_t *sfrm;

		if (vap->iv_state != IEEE80211_S_RUN) {
			IEEE80211_DISCARD(vap, IEEE80211_MSG_INPUT,
			    wh, NULL, "wrong state %s",
			    ieee80211_state_name[vap->iv_state]);
			vap->iv_stats.is_rx_mgtdiscard++;
			return;
		}
		if (IEEE80211_IS_MULTICAST(wh->i_addr2)) {
			/* frame must be directed */
			IEEE80211_DISCARD(vap, IEEE80211_MSG_INPUT,
			    wh, NULL, "%s", "not unicast");
			vap->iv_stats.is_rx_mgtdiscard++;	/* XXX stat */
			return;
		}
		/*
		 * prreq frame format
		 *      [tlv] ssid
		 *      [tlv] supported rates
		 *      [tlv] extended supported rates
		 *	[tlv] mesh id
		 */
		ssid = meshid = rates = xrates = NULL;
		sfrm = frm;
		while (efrm - frm > 1) {
			IEEE80211_VERIFY_LENGTH(efrm - frm, frm[1] + 2, return);
			switch (*frm) {
			case IEEE80211_ELEMID_SSID:
				ssid = frm;
				break;
			case IEEE80211_ELEMID_RATES:
				rates = frm;
				break;
			case IEEE80211_ELEMID_XRATES:
				xrates = frm;
				break;
			case IEEE80211_ELEMID_MESHID:
				meshid = frm;
				break;
			}
			frm += frm[1] + 2;
		}
		IEEE80211_VERIFY_ELEMENT(ssid, IEEE80211_NWID_LEN, return);
		IEEE80211_VERIFY_ELEMENT(rates, IEEE80211_RATE_MAXSIZE, return);
		if (xrates != NULL)
			IEEE80211_VERIFY_ELEMENT(xrates,
			    IEEE80211_RATE_MAXSIZE - rates[1], return);
		if (meshid != NULL)
			IEEE80211_VERIFY_ELEMENT(meshid,
			    IEEE80211_MESHID_LEN, return);
		/* NB: meshid, not ssid */
		IEEE80211_VERIFY_SSID(vap->iv_bss, meshid, return);

		/* XXX find a better class or define it's own */
		IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_INPUT, wh->i_addr2,
		    "%s", "recv probe req");
		/*
		 * Some legacy 11b clients cannot hack a complete
		 * probe response frame.  When the request includes
		 * only a bare-bones rate set, communicate this to
		 * the transmit side.
		 */
		ieee80211_send_proberesp(vap, wh->i_addr2, 0);
		break;
	}
	case IEEE80211_FC0_SUBTYPE_ACTION:
		if (vap->iv_state != IEEE80211_S_RUN) {
			vap->iv_stats.is_rx_mgtdiscard++;
			break;
		}
		/*
		 * We received an action for an unknown neighbor.
		 * XXX: wait for it to beacon or create ieee80211_node?
		 */
		if (ni == vap->iv_bss) {
			IEEE80211_DISCARD(vap, IEEE80211_MSG_MESH,
			    wh, NULL, "%s", "unknown node");
			vap->iv_stats.is_rx_mgtdiscard++;
			break;
		}
		/*
		 * Discard if not for us.
		 */
		if (!IEEE80211_ADDR_EQ(vap->iv_myaddr, wh->i_addr1) &&
		    !IEEE80211_IS_MULTICAST(wh->i_addr1)) {
			IEEE80211_DISCARD(vap, IEEE80211_MSG_MESH,
			    wh, NULL, "%s", "not for me");
			vap->iv_stats.is_rx_mgtdiscard++;
			break;
		}
		/* XXX parse_action is a bit useless now */
		if (ieee80211_parse_action(ni, m0) == 0)
			ic->ic_recv_action(ni, wh, frm, efrm);
		break;
	case IEEE80211_FC0_SUBTYPE_AUTH:
	case IEEE80211_FC0_SUBTYPE_ASSOC_REQ:
	case IEEE80211_FC0_SUBTYPE_REASSOC_REQ:
	case IEEE80211_FC0_SUBTYPE_ASSOC_RESP:
	case IEEE80211_FC0_SUBTYPE_REASSOC_RESP:
	case IEEE80211_FC0_SUBTYPE_DEAUTH:
	case IEEE80211_FC0_SUBTYPE_DISASSOC:
		IEEE80211_DISCARD(vap, IEEE80211_MSG_INPUT,
		    wh, NULL, "%s", "not handled");
		vap->iv_stats.is_rx_mgtdiscard++;
		return;
	default:
		IEEE80211_DISCARD(vap, IEEE80211_MSG_ANY,
		    wh, "mgt", "subtype 0x%x not handled", subtype);
		vap->iv_stats.is_rx_badsubtype++;
		break;
	}
}

/*
 * Parse meshpeering action ie's for open+confirm frames; the
 * important bits are returned in the supplied structure.
 */
static const struct ieee80211_meshpeer_ie *
mesh_parse_meshpeering_action(struct ieee80211_node *ni,
	const struct ieee80211_frame *wh,	/* XXX for VERIFY_LENGTH */
	const uint8_t *frm, const uint8_t *efrm,
	struct ieee80211_meshpeer_ie *mp, uint8_t subtype)
{
	struct ieee80211vap *vap = ni->ni_vap;
	const struct ieee80211_meshpeer_ie *mpie;
	const uint8_t *meshid, *meshconf, *meshpeer;

	meshid = meshconf = meshpeer = NULL;
	while (efrm - frm > 1) {
		IEEE80211_VERIFY_LENGTH(efrm - frm, frm[1] + 2, return NULL);
		switch (*frm) {
		case IEEE80211_ELEMID_MESHID:
			meshid = frm;
			break;
		case IEEE80211_ELEMID_MESHCONF:
			meshconf = frm;
			break;
		case IEEE80211_ELEMID_MESHPEER:
			meshpeer = frm;
			mpie = (const struct ieee80211_meshpeer_ie *) frm;
			memset(mp, 0, sizeof(*mp));
			mp->peer_llinkid = LE_READ_2(&mpie->peer_llinkid);
			/* NB: peer link ID is optional on these frames */
			if (subtype == IEEE80211_MESH_PEER_LINK_CLOSE &&
			    mpie->peer_len == 8) {
				mp->peer_linkid = 0;
				mp->peer_rcode = LE_READ_2(&mpie->peer_linkid);
			} else {
				mp->peer_linkid = LE_READ_2(&mpie->peer_linkid);
				mp->peer_rcode = LE_READ_2(&mpie->peer_rcode);
			}
			break;
		}
		frm += frm[1] + 2;
	}

	/*
	 * Verify the contents of the frame. Action frames with
	 * close subtype don't have a Mesh Configuration IE.
	 * If if fails validation, close the peer link.
	 */
	KASSERT(meshpeer != NULL &&
	    subtype != IEEE80211_ACTION_MESHPEERING_CLOSE,
	    ("parsing close action"));

	if (mesh_verify_meshid(vap, meshid) ||
	    mesh_verify_meshpeer(vap, subtype, meshpeer) ||
	    mesh_verify_meshconf(vap, meshconf)) {
		uint16_t args[3];

		IEEE80211_DISCARD(vap,
		    IEEE80211_MSG_ACTION | IEEE80211_MSG_MESH,
		    wh, NULL, "%s", "not for our mesh");
		vap->iv_stats.is_rx_mgtdiscard++;
		switch (ni->ni_mlstate) {
		case IEEE80211_NODE_MESH_IDLE:
		case IEEE80211_NODE_MESH_ESTABLISHED:
		case IEEE80211_NODE_MESH_HOLDING:
			/* ignore */
			break;
		case IEEE80211_NODE_MESH_OPENSNT:
		case IEEE80211_NODE_MESH_OPENRCV:
		case IEEE80211_NODE_MESH_CONFIRMRCV:
			args[0] = ni->ni_mlpid;
			args[1] = ni->ni_mllid;
			args[2] = IEEE80211_REASON_PEER_LINK_CANCELED;
			ieee80211_send_action(ni,
			    IEEE80211_ACTION_CAT_MESHPEERING,
			    IEEE80211_ACTION_MESHPEERING_CLOSE,
			    args);
			mesh_linkchange(ni, IEEE80211_NODE_MESH_HOLDING);
			mesh_peer_timeout_setup(ni);
			break;
		}
		return NULL;
	}
	return (const struct ieee80211_meshpeer_ie *) mp;
}

static int
mesh_recv_action_meshpeering_open(struct ieee80211_node *ni,
	const struct ieee80211_frame *wh,
	const uint8_t *frm, const uint8_t *efrm)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211_meshpeer_ie ie;
	const struct ieee80211_meshpeer_ie *meshpeer;
	uint16_t args[3];

	/* +2+2 for action + code + capabilites */
	meshpeer = mesh_parse_meshpeering_action(ni, wh, frm+2+2, efrm, &ie,
	    IEEE80211_ACTION_MESHPEERING_OPEN);
	if (meshpeer == NULL) {
		return 0;
	}

	/* XXX move up */
	IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION | IEEE80211_MSG_MESH, ni,
	    "recv PEER OPEN, lid 0x%x", meshpeer->peer_llinkid);

	switch (ni->ni_mlstate) {
	case IEEE80211_NODE_MESH_IDLE:
		mesh_linkchange(ni, IEEE80211_NODE_MESH_OPENRCV);
		ni->ni_mllid = meshpeer->peer_llinkid;
		ni->ni_mlpid = mesh_generateid(vap);
		if (ni->ni_mlpid == 0)
			return 0;		/* XXX */
		args[0] = ni->ni_mlpid;
		/* Announce we're open too... */
		ieee80211_send_action(ni,
		    IEEE80211_ACTION_CAT_MESHPEERING,
		    IEEE80211_ACTION_MESHPEERING_OPEN, args);
		/* ...and confirm the link. */
		args[0] = ni->ni_mlpid;
		args[1] = ni->ni_mllid;
		ieee80211_send_action(ni,
		    IEEE80211_ACTION_CAT_MESHPEERING,
		    IEEE80211_ACTION_MESHPEERING_CONFIRM,
		    args);
		mesh_peer_timeout_setup(ni);
		break;
	case IEEE80211_NODE_MESH_OPENRCV:
		/* Wrong Link ID */
		if (ni->ni_mllid != meshpeer->peer_llinkid) {
			args[0] = ni->ni_mllid;
			args[1] = ni->ni_mlpid;
			args[2] = IEEE80211_REASON_PEER_LINK_CANCELED;
			ieee80211_send_action(ni,
			    IEEE80211_ACTION_CAT_MESHPEERING,
			    IEEE80211_ACTION_MESHPEERING_CLOSE,
			    args);
			mesh_linkchange(ni, IEEE80211_NODE_MESH_HOLDING);
			mesh_peer_timeout_setup(ni);
			break;
		}
		/* Duplicate open, confirm again. */
		args[0] = ni->ni_mlpid;
		args[1] = ni->ni_mllid;
		ieee80211_send_action(ni,
		    IEEE80211_ACTION_CAT_MESHPEERING,
		    IEEE80211_ACTION_MESHPEERING_CONFIRM,
		    args);
		break;
	case IEEE80211_NODE_MESH_OPENSNT:
		ni->ni_mllid = meshpeer->peer_llinkid;
		mesh_linkchange(ni, IEEE80211_NODE_MESH_OPENRCV);
		args[0] = ni->ni_mlpid;
		args[1] = ni->ni_mllid;
		ieee80211_send_action(ni,
		    IEEE80211_ACTION_CAT_MESHPEERING,
		    IEEE80211_ACTION_MESHPEERING_CONFIRM,
		    args);
		/* NB: don't setup/clear any timeout */
		break;
	case IEEE80211_NODE_MESH_CONFIRMRCV:
		if (ni->ni_mlpid != meshpeer->peer_linkid ||
		    ni->ni_mllid != meshpeer->peer_llinkid) {
			args[0] = ni->ni_mlpid;
			args[1] = ni->ni_mllid;
			args[2] = IEEE80211_REASON_PEER_LINK_CANCELED;
			ieee80211_send_action(ni,
			    IEEE80211_ACTION_CAT_MESHPEERING,
			    IEEE80211_ACTION_MESHPEERING_CLOSE,
			    args);
			mesh_linkchange(ni,
			    IEEE80211_NODE_MESH_HOLDING);
			mesh_peer_timeout_setup(ni);
			break;
		}
		mesh_linkchange(ni, IEEE80211_NODE_MESH_ESTABLISHED);
		ni->ni_mllid = meshpeer->peer_llinkid;
		args[0] = ni->ni_mlpid;
		args[1] = ni->ni_mllid;
		ieee80211_send_action(ni,
		    IEEE80211_ACTION_CAT_MESHPEERING,
		    IEEE80211_ACTION_MESHPEERING_CONFIRM,
		    args);
		mesh_peer_timeout_stop(ni);
		break;
	case IEEE80211_NODE_MESH_ESTABLISHED:
		if (ni->ni_mllid != meshpeer->peer_llinkid) {
			args[0] = ni->ni_mllid;
			args[1] = ni->ni_mlpid;
			args[2] = IEEE80211_REASON_PEER_LINK_CANCELED;
			ieee80211_send_action(ni,
			    IEEE80211_ACTION_CAT_MESHPEERING,
			    IEEE80211_ACTION_MESHPEERING_CLOSE,
			    args);
			mesh_linkchange(ni, IEEE80211_NODE_MESH_HOLDING);
			mesh_peer_timeout_setup(ni);
			break;
		}
		args[0] = ni->ni_mlpid;
		args[1] = ni->ni_mllid;
		ieee80211_send_action(ni,
		    IEEE80211_ACTION_CAT_MESHPEERING,
		    IEEE80211_ACTION_MESHPEERING_CONFIRM,
		    args);
		break;
	case IEEE80211_NODE_MESH_HOLDING:
		args[0] = ni->ni_mlpid;
		args[1] = meshpeer->peer_llinkid;
		args[2] = IEEE80211_REASON_MESH_MAX_RETRIES;
		ieee80211_send_action(ni,
		    IEEE80211_ACTION_CAT_MESHPEERING,
		    IEEE80211_ACTION_MESHPEERING_CLOSE,
		    args);
		break;
	}
	return 0;
}

static int
mesh_recv_action_meshpeering_confirm(struct ieee80211_node *ni,
	const struct ieee80211_frame *wh,
	const uint8_t *frm, const uint8_t *efrm)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211_meshpeer_ie ie;
	const struct ieee80211_meshpeer_ie *meshpeer;
	uint16_t args[3];

	/* +2+2+2+2 for action + code + capabilites + status code + AID */
	meshpeer = mesh_parse_meshpeering_action(ni, wh, frm+2+2+2+2, efrm, &ie,
	    IEEE80211_ACTION_MESHPEERING_CONFIRM);
	if (meshpeer == NULL) {
		return 0;
	}

	IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION | IEEE80211_MSG_MESH, ni,
	    "recv PEER CONFIRM, local id 0x%x, peer id 0x%x",
	    meshpeer->peer_llinkid, meshpeer->peer_linkid);

	switch (ni->ni_mlstate) {
	case IEEE80211_NODE_MESH_OPENRCV:
		mesh_linkchange(ni, IEEE80211_NODE_MESH_ESTABLISHED);
		mesh_peer_timeout_stop(ni);
		break;
	case IEEE80211_NODE_MESH_OPENSNT:
		mesh_linkchange(ni, IEEE80211_NODE_MESH_CONFIRMRCV);
		break;
	case IEEE80211_NODE_MESH_HOLDING:
		args[0] = ni->ni_mlpid;
		args[1] = meshpeer->peer_llinkid;
		args[2] = IEEE80211_REASON_MESH_MAX_RETRIES;
		ieee80211_send_action(ni,
		    IEEE80211_ACTION_CAT_MESHPEERING,
		    IEEE80211_ACTION_MESHPEERING_CLOSE,
		    args);
		break;
	case IEEE80211_NODE_MESH_CONFIRMRCV:
		if (ni->ni_mllid != meshpeer->peer_llinkid) {
			args[0] = ni->ni_mlpid;
			args[1] = ni->ni_mllid;
			args[2] = IEEE80211_REASON_PEER_LINK_CANCELED;
			ieee80211_send_action(ni,
			    IEEE80211_ACTION_CAT_MESHPEERING,
			    IEEE80211_ACTION_MESHPEERING_CLOSE,
			    args);
			mesh_linkchange(ni, IEEE80211_NODE_MESH_HOLDING);
			mesh_peer_timeout_setup(ni);
		}
		break;
	default:
		IEEE80211_DISCARD(vap,
		    IEEE80211_MSG_ACTION | IEEE80211_MSG_MESH,
		    wh, NULL, "received confirm in invalid state %d",
		    ni->ni_mlstate);
		vap->iv_stats.is_rx_mgtdiscard++;
		break;
	}
	return 0;
}

static int
mesh_recv_action_meshpeering_close(struct ieee80211_node *ni,
	const struct ieee80211_frame *wh,
	const uint8_t *frm, const uint8_t *efrm)
{
	uint16_t args[3];

	IEEE80211_NOTE(ni->ni_vap, IEEE80211_MSG_ACTION | IEEE80211_MSG_MESH,
	    ni, "%s", "recv PEER CLOSE");

	switch (ni->ni_mlstate) {
	case IEEE80211_NODE_MESH_IDLE:
		/* ignore */
		break;
	case IEEE80211_NODE_MESH_OPENRCV:
	case IEEE80211_NODE_MESH_OPENSNT:
	case IEEE80211_NODE_MESH_CONFIRMRCV:
	case IEEE80211_NODE_MESH_ESTABLISHED:
		args[0] = ni->ni_mlpid;
		args[1] = ni->ni_mllid;
		args[2] = IEEE80211_REASON_MESH_CLOSE_RCVD;
		ieee80211_send_action(ni,
		    IEEE80211_ACTION_CAT_MESHPEERING,
		    IEEE80211_ACTION_MESHPEERING_CLOSE,
		    args);
		mesh_linkchange(ni, IEEE80211_NODE_MESH_HOLDING);
		mesh_peer_timeout_setup(ni);
		break;
	case IEEE80211_NODE_MESH_HOLDING:
		mesh_linkchange(ni, IEEE80211_NODE_MESH_IDLE);
		mesh_peer_timeout_setup(ni);
		break;
	}
	return 0;
}

/*
 * Link Metric handling.
 */
static int
mesh_recv_action_meshlmetric_req(struct ieee80211_node *ni,
	const struct ieee80211_frame *wh,
	const uint8_t *frm, const uint8_t *efrm)
{
	uint32_t metric;

	metric = mesh_airtime_calc(ni);
	ieee80211_send_action(ni,
	    IEEE80211_ACTION_CAT_MESHLMETRIC,
	    IEEE80211_ACTION_MESHLMETRIC_REP,
	    &metric);
	return 0;
}

static int
mesh_recv_action_meshlmetric_rep(struct ieee80211_node *ni,
	const struct ieee80211_frame *wh,
	const uint8_t *frm, const uint8_t *efrm)
{
	return 0;
}

static int
mesh_send_action(struct ieee80211_node *ni, struct mbuf *m)
{
	struct ieee80211_bpf_params params;

	memset(&params, 0, sizeof(params));
	params.ibp_pri = WME_AC_VO;
	params.ibp_rate0 = ni->ni_txparms->mgmtrate;
	/* XXX ucast/mcast */
	params.ibp_try0 = ni->ni_txparms->maxretry;
	params.ibp_power = ni->ni_txpower;
	return ieee80211_mgmt_output(ni, m, IEEE80211_FC0_SUBTYPE_ACTION,
	     &params);
}

#define	ADDSHORT(frm, v) do {			\
	frm[0] = (v) & 0xff;			\
	frm[1] = (v) >> 8;			\
	frm += 2;				\
} while (0)
#define	ADDWORD(frm, v) do {			\
	frm[0] = (v) & 0xff;			\
	frm[1] = ((v) >> 8) & 0xff;		\
	frm[2] = ((v) >> 16) & 0xff;		\
	frm[3] = ((v) >> 24) & 0xff;		\
	frm += 4;				\
} while (0)

static int
mesh_send_action_meshpeering_open(struct ieee80211_node *ni,
	int category, int action, void *args0)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = ni->ni_ic;
	uint16_t *args = args0;
	const struct ieee80211_rateset *rs;
	struct mbuf *m;
	uint8_t *frm;

	IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION | IEEE80211_MSG_MESH, ni,
	    "send PEER OPEN action: localid 0x%x", args[0]);

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_NODE,
	    "ieee80211_ref_node (%s:%u) %p<%s> refcnt %d\n", __func__, __LINE__,
	    ni, ether_sprintf(ni->ni_macaddr), ieee80211_node_refcnt(ni)+1);
	ieee80211_ref_node(ni);

	m = ieee80211_getmgtframe(&frm,
	    ic->ic_headroom + sizeof(struct ieee80211_frame),
	    sizeof(uint16_t)	/* action+category */
	    + sizeof(uint16_t)	/* capabilites */
	    + 2 + IEEE80211_RATE_SIZE	 
	    + 2 + (IEEE80211_RATE_MAXSIZE - IEEE80211_RATE_SIZE)	 
	    + 2 + IEEE80211_MESHID_LEN
	    + sizeof(struct ieee80211_meshconf_ie)	 
	    + sizeof(struct ieee80211_meshpeer_ie)
	);
	if (m != NULL) {
		/*
		 * mesh peer open action frame format:
		 *   [1] category
		 *   [1] action
		 *   [2] capabilities
		 *   [tlv] rates
		 *   [tlv] xrates
		 *   [tlv] mesh id
		 *   [tlv] mesh conf
		 *   [tlv] mesh peer link mgmt
		 */
		*frm++ = category;
		*frm++ = action;
		ADDSHORT(frm, ieee80211_getcapinfo(vap, ni->ni_chan));
		rs = ieee80211_get_suprates(ic, ic->ic_curchan);
		frm = ieee80211_add_rates(frm, rs);
		frm = ieee80211_add_xrates(frm, rs);
		frm = ieee80211_add_meshid(frm, vap);
		frm = ieee80211_add_meshconf(frm, vap);
		frm = ieee80211_add_meshpeer(frm, IEEE80211_MESH_PEER_LINK_OPEN,
		    args[0], 0, 0);
		m->m_pkthdr.len = m->m_len = frm - mtod(m, uint8_t *);
		return mesh_send_action(ni, m);
	} else {
		vap->iv_stats.is_tx_nobuf++;
		ieee80211_free_node(ni);
		return ENOMEM;
	}
}

static int
mesh_send_action_meshpeering_confirm(struct ieee80211_node *ni,
	int category, int action, void *args0)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = ni->ni_ic;
	uint16_t *args = args0;
	const struct ieee80211_rateset *rs;
	struct mbuf *m;
	uint8_t *frm;

	IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION | IEEE80211_MSG_MESH, ni,
	    "send PEER CONFIRM action: localid 0x%x, peerid 0x%x",
	    args[0], args[1]);

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_NODE,
	    "ieee80211_ref_node (%s:%u) %p<%s> refcnt %d\n", __func__, __LINE__,
	    ni, ether_sprintf(ni->ni_macaddr), ieee80211_node_refcnt(ni)+1);
	ieee80211_ref_node(ni);

	m = ieee80211_getmgtframe(&frm,
	    ic->ic_headroom + sizeof(struct ieee80211_frame),
	    sizeof(uint16_t)	/* action+category */
	    + sizeof(uint16_t)	/* capabilites */
	    + sizeof(uint16_t)	/* status code */
	    + sizeof(uint16_t)	/* AID */
	    + 2 + IEEE80211_RATE_SIZE	 
	    + 2 + (IEEE80211_RATE_MAXSIZE - IEEE80211_RATE_SIZE)	 
	    + 2 + IEEE80211_MESHID_LEN
	    + sizeof(struct ieee80211_meshconf_ie)	 
	    + sizeof(struct ieee80211_meshpeer_ie)
	);
	if (m != NULL) {
		/*
		 * mesh peer confirm action frame format:
		 *   [1] category
		 *   [1] action
		 *   [2] capabilities
		 *   [2] status code
		 *   [2] association id (peer ID)
		 *   [tlv] rates
		 *   [tlv] xrates
		 *   [tlv] mesh id
		 *   [tlv] mesh conf
		 *   [tlv] mesh peer link mgmt
		 */
		*frm++ = category;
		*frm++ = action;
		ADDSHORT(frm, ieee80211_getcapinfo(vap, ni->ni_chan));
		ADDSHORT(frm, 0);		/* status code */
		ADDSHORT(frm, args[1]);		/* AID */
		rs = ieee80211_get_suprates(ic, ic->ic_curchan);
		frm = ieee80211_add_rates(frm, rs);
		frm = ieee80211_add_xrates(frm, rs);
		frm = ieee80211_add_meshid(frm, vap);
		frm = ieee80211_add_meshconf(frm, vap);
		frm = ieee80211_add_meshpeer(frm,
		    IEEE80211_MESH_PEER_LINK_CONFIRM,
		    args[0], args[1], 0);
		m->m_pkthdr.len = m->m_len = frm - mtod(m, uint8_t *);
		return mesh_send_action(ni, m);
	} else {
		vap->iv_stats.is_tx_nobuf++;
		ieee80211_free_node(ni);
		return ENOMEM;
	}
}

static int
mesh_send_action_meshpeering_close(struct ieee80211_node *ni,
	int category, int action, void *args0)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = ni->ni_ic;
	uint16_t *args = args0;
	struct mbuf *m;
	uint8_t *frm;

	IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION | IEEE80211_MSG_MESH, ni,
	    "send PEER CLOSE action: localid 0x%x, peerid 0x%x reason %d",
	    args[0], args[1], args[2]);

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_NODE,
	    "ieee80211_ref_node (%s:%u) %p<%s> refcnt %d\n", __func__, __LINE__,
	    ni, ether_sprintf(ni->ni_macaddr), ieee80211_node_refcnt(ni)+1);
	ieee80211_ref_node(ni);

	m = ieee80211_getmgtframe(&frm,
	    ic->ic_headroom + sizeof(struct ieee80211_frame),
	    sizeof(uint16_t)	/* action+category */
	    + sizeof(uint16_t)	/* reason code */
	    + 2 + IEEE80211_MESHID_LEN
	    + sizeof(struct ieee80211_meshpeer_ie) 
	);
	if (m != NULL) {
		/*
		 * mesh peer close action frame format:
		 *   [1] category
		 *   [1] action
		 *   [2] reason code
		 *   [tlv] mesh id
		 *   [tlv] mesh peer link mgmt
		 */
		*frm++ = category;
		*frm++ = action;
		ADDSHORT(frm, args[2]);		/* reason code */
		frm = ieee80211_add_meshid(frm, vap);
		frm = ieee80211_add_meshpeer(frm,
		    IEEE80211_MESH_PEER_LINK_CLOSE,
		    args[0], args[1], args[2]);
		m->m_pkthdr.len = m->m_len = frm - mtod(m, uint8_t *);
		return mesh_send_action(ni, m);
	} else {
		vap->iv_stats.is_tx_nobuf++;
		ieee80211_free_node(ni);
		return ENOMEM;
	}
}

static int
mesh_send_action_meshlink_request(struct ieee80211_node *ni,
	int category, int action, void *arg0)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = ni->ni_ic;
	struct mbuf *m;
	uint8_t *frm;

	IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION | IEEE80211_MSG_MESH, ni,
	    "%s", "send LINK METRIC REQUEST action");

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_NODE,
	    "ieee80211_ref_node (%s:%u) %p<%s> refcnt %d\n", __func__, __LINE__,
	    ni, ether_sprintf(ni->ni_macaddr), ieee80211_node_refcnt(ni)+1);
	ieee80211_ref_node(ni);

	m = ieee80211_getmgtframe(&frm,
	    ic->ic_headroom + sizeof(struct ieee80211_frame),
	    sizeof(uint16_t)	/* action+category */
	);
	if (m != NULL) {
		/*
		 * mesh link metric request
		 *   [1] category
		 *   [1] action
		 */
		*frm++ = category;
		*frm++ = action;
		m->m_pkthdr.len = m->m_len = frm - mtod(m, uint8_t *);
		return mesh_send_action(ni, m);
	} else {
		vap->iv_stats.is_tx_nobuf++;
		ieee80211_free_node(ni);
		return ENOMEM;
	}
}

static int
mesh_send_action_meshlink_reply(struct ieee80211_node *ni,
	int category, int action, void *args0)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = ni->ni_ic;
	uint32_t *metric = args0;
	struct mbuf *m;
	uint8_t *frm;

	IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION | IEEE80211_MSG_MESH, ni,
	    "send LINK METRIC REPLY action: metric 0x%x", *metric);

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_NODE,
	    "ieee80211_ref_node (%s:%u) %p<%s> refcnt %d\n", __func__, __LINE__,
	    ni, ether_sprintf(ni->ni_macaddr), ieee80211_node_refcnt(ni)+1);
	ieee80211_ref_node(ni);

	m = ieee80211_getmgtframe(&frm,
	    ic->ic_headroom + sizeof(struct ieee80211_frame),
	    sizeof(uint16_t)	/* action+category */
	    + sizeof(struct ieee80211_meshlmetric_ie)
	);
	if (m != NULL) {
		/*
		 * mesh link metric reply
		 *   [1] category
		 *   [1] action
		 *   [tlv] mesh link metric
		 */
		*frm++ = category;
		*frm++ = action;
		frm = ieee80211_add_meshlmetric(frm, *metric);
		m->m_pkthdr.len = m->m_len = frm - mtod(m, uint8_t *);
		return mesh_send_action(ni, m);
	} else {
		vap->iv_stats.is_tx_nobuf++;
		ieee80211_free_node(ni);
		return ENOMEM;
	}
}

static void
mesh_peer_timeout_setup(struct ieee80211_node *ni)
{
	switch (ni->ni_mlstate) {
	case IEEE80211_NODE_MESH_HOLDING:
		ni->ni_mltval = ieee80211_mesh_holdingtimeout;
		break;
	case IEEE80211_NODE_MESH_CONFIRMRCV:
		ni->ni_mltval = ieee80211_mesh_confirmtimeout;
		break;
	case IEEE80211_NODE_MESH_IDLE:
		ni->ni_mltval = 0;
		break;
	default:
		ni->ni_mltval = ieee80211_mesh_retrytimeout;
		break;
	}
	if (ni->ni_mltval)
		callout_reset(&ni->ni_mltimer, ni->ni_mltval,
		    mesh_peer_timeout_cb, ni);
}

/*
 * Same as above but backoffs timer statisically 50%.
 */
static void
mesh_peer_timeout_backoff(struct ieee80211_node *ni)
{
	uint32_t r;
	
	r = arc4random();
	ni->ni_mltval += r % ni->ni_mltval;
	callout_reset(&ni->ni_mltimer, ni->ni_mltval, mesh_peer_timeout_cb,
	    ni);
}

static __inline void
mesh_peer_timeout_stop(struct ieee80211_node *ni)
{
	callout_drain(&ni->ni_mltimer);
}

/*
 * Mesh Peer Link Management FSM timeout handling.
 */
static void
mesh_peer_timeout_cb(void *arg)
{
	struct ieee80211_node *ni = (struct ieee80211_node *)arg;
	uint16_t args[3];

	IEEE80211_NOTE(ni->ni_vap, IEEE80211_MSG_MESH,
	    ni, "mesh link timeout, state %d, retry counter %d",
	    ni->ni_mlstate, ni->ni_mlrcnt);
	
	switch (ni->ni_mlstate) {
	case IEEE80211_NODE_MESH_IDLE:
	case IEEE80211_NODE_MESH_ESTABLISHED:
		break;
	case IEEE80211_NODE_MESH_OPENSNT:
	case IEEE80211_NODE_MESH_OPENRCV:
		if (ni->ni_mlrcnt == ieee80211_mesh_maxretries) {
			args[0] = ni->ni_mlpid;
			args[2] = IEEE80211_REASON_MESH_MAX_RETRIES;
			ieee80211_send_action(ni,
			    IEEE80211_ACTION_CAT_MESHPEERING,
			    IEEE80211_ACTION_MESHPEERING_CLOSE, args);
			ni->ni_mlrcnt = 0;
			mesh_linkchange(ni, IEEE80211_NODE_MESH_HOLDING);
			mesh_peer_timeout_setup(ni);
		} else {
			args[0] = ni->ni_mlpid;
			ieee80211_send_action(ni,
			    IEEE80211_ACTION_CAT_MESHPEERING,
			    IEEE80211_ACTION_MESHPEERING_OPEN, args);
			ni->ni_mlrcnt++;
			mesh_peer_timeout_backoff(ni);
		}
		break;
	case IEEE80211_NODE_MESH_CONFIRMRCV:
		if (ni->ni_mlrcnt == ieee80211_mesh_maxretries) {
			args[0] = ni->ni_mlpid;
			args[2] = IEEE80211_REASON_MESH_CONFIRM_TIMEOUT;
			ieee80211_send_action(ni,
			    IEEE80211_ACTION_CAT_MESHPEERING,
			    IEEE80211_ACTION_MESHPEERING_CLOSE, args);
			ni->ni_mlrcnt = 0;
			mesh_linkchange(ni, IEEE80211_NODE_MESH_HOLDING);
			mesh_peer_timeout_setup(ni);
		} else {
			ni->ni_mlrcnt++;
			mesh_peer_timeout_setup(ni);
		}
		break;
	case IEEE80211_NODE_MESH_HOLDING:
		mesh_linkchange(ni, IEEE80211_NODE_MESH_IDLE);
		break;
	}
}

static int
mesh_verify_meshid(struct ieee80211vap *vap, const uint8_t *ie)
{
	struct ieee80211_mesh_state *ms = vap->iv_mesh;

	if (ie == NULL || ie[1] != ms->ms_idlen)
		return 1;
	return memcmp(ms->ms_id, ie + 2, ms->ms_idlen);
}

/*
 * Check if we are using the same algorithms for this mesh.
 */
static int
mesh_verify_meshconf(struct ieee80211vap *vap, const uint8_t *ie)
{
	const struct ieee80211_meshconf_ie *meshconf =
	    (const struct ieee80211_meshconf_ie *) ie;
	const struct ieee80211_mesh_state *ms = vap->iv_mesh;

	if (meshconf == NULL)
		return 1;
	if (meshconf->conf_pselid != ms->ms_ppath->mpp_ie) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_MESH,
		    "unknown path selection algorithm: 0x%x\n",
		    meshconf->conf_pselid);
		return 1;
	}
	if (meshconf->conf_pmetid != ms->ms_pmetric->mpm_ie) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_MESH,
		    "unknown path metric algorithm: 0x%x\n",
		    meshconf->conf_pmetid);
		return 1;
	}
	if (meshconf->conf_ccid != 0) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_MESH,
		    "unknown congestion control algorithm: 0x%x\n",
		    meshconf->conf_ccid);
		return 1;
	}
	if (meshconf->conf_syncid != IEEE80211_MESHCONF_SYNC_NEIGHOFF) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_MESH,
		    "unknown sync algorithm: 0x%x\n",
		    meshconf->conf_syncid);
		return 1;
	}
	if (meshconf->conf_authid != 0) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_MESH,
		    "unknown auth auth algorithm: 0x%x\n",
		    meshconf->conf_pselid);
		return 1;
	}
	/* Not accepting peers */
	if (!(meshconf->conf_cap & IEEE80211_MESHCONF_CAP_AP)) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_MESH,
		    "not accepting peers: 0x%x\n", meshconf->conf_cap);
		return 1;
	}
	return 0;
}

static int
mesh_verify_meshpeer(struct ieee80211vap *vap, uint8_t subtype,
    const uint8_t *ie)
{
	const struct ieee80211_meshpeer_ie *meshpeer =
	    (const struct ieee80211_meshpeer_ie *) ie;

	if (meshpeer == NULL || meshpeer->peer_len < 6 ||
	    meshpeer->peer_len > 10)
		return 1;
	switch (subtype) {
	case IEEE80211_MESH_PEER_LINK_OPEN:
		if (meshpeer->peer_len != 6)
			return 1;
		break;
	case IEEE80211_MESH_PEER_LINK_CONFIRM:
		if (meshpeer->peer_len != 8)
			return 1;
		break;
	case IEEE80211_MESH_PEER_LINK_CLOSE:
		if (meshpeer->peer_len < 8)
			return 1;
		if (meshpeer->peer_len == 8 && meshpeer->peer_linkid != 0)
			return 1;
		if (meshpeer->peer_rcode == 0)
			return 1;
		break;
	}
	return 0;
}

/*
 * Add a Mesh ID IE to a frame.
 */
uint8_t *
ieee80211_add_meshid(uint8_t *frm, struct ieee80211vap *vap)
{
	struct ieee80211_mesh_state *ms = vap->iv_mesh;

	KASSERT(vap->iv_opmode == IEEE80211_M_MBSS, ("not a mbss vap"));

	*frm++ = IEEE80211_ELEMID_MESHID;
	*frm++ = ms->ms_idlen;
	memcpy(frm, ms->ms_id, ms->ms_idlen);
	return frm + ms->ms_idlen;
}

/*
 * Add a Mesh Configuration IE to a frame.
 * For now just use HWMP routing, Airtime link metric, Null Congestion
 * Signaling, Null Sync Protocol and Null Authentication.
 */
uint8_t *
ieee80211_add_meshconf(uint8_t *frm, struct ieee80211vap *vap)
{
	const struct ieee80211_mesh_state *ms = vap->iv_mesh;

	KASSERT(vap->iv_opmode == IEEE80211_M_MBSS, ("not a MBSS vap"));

	*frm++ = IEEE80211_ELEMID_MESHCONF;
	*frm++ = sizeof(struct ieee80211_meshconf_ie) - 2;
	*frm++ = ms->ms_ppath->mpp_ie;		/* path selection */
	*frm++ = ms->ms_pmetric->mpm_ie;	/* link metric */
	*frm++ = IEEE80211_MESHCONF_CC_DISABLED;
	*frm++ = IEEE80211_MESHCONF_SYNC_NEIGHOFF;
	*frm++ = IEEE80211_MESHCONF_AUTH_DISABLED;
	/* NB: set the number of neighbors before the rest */
	*frm = (ms->ms_neighbors > 15 ? 15 : ms->ms_neighbors) << 1;
	if (ms->ms_flags & IEEE80211_MESHFLAGS_PORTAL)
		*frm |= IEEE80211_MESHCONF_FORM_MP;
	frm += 1;
	if (ms->ms_flags & IEEE80211_MESHFLAGS_AP)
		*frm |= IEEE80211_MESHCONF_CAP_AP;
	if (ms->ms_flags & IEEE80211_MESHFLAGS_FWD)
		*frm |= IEEE80211_MESHCONF_CAP_FWRD;
	frm += 1;
	return frm;
}

/*
 * Add a Mesh Peer Management IE to a frame.
 */
uint8_t *
ieee80211_add_meshpeer(uint8_t *frm, uint8_t subtype, uint16_t localid,
    uint16_t peerid, uint16_t reason)
{
	/* XXX change for AH */
	static const uint8_t meshpeerproto[4] = IEEE80211_MESH_PEER_PROTO;

	KASSERT(localid != 0, ("localid == 0"));

	*frm++ = IEEE80211_ELEMID_MESHPEER;
	switch (subtype) {
	case IEEE80211_MESH_PEER_LINK_OPEN:
		*frm++ = 6;		/* length */
		memcpy(frm, meshpeerproto, 4);
		frm += 4;
		ADDSHORT(frm, localid);	/* local ID */
		break;
	case IEEE80211_MESH_PEER_LINK_CONFIRM:
		KASSERT(peerid != 0, ("sending peer confirm without peer id"));
		*frm++ = 8;		/* length */
		memcpy(frm, meshpeerproto, 4);
		frm += 4;
		ADDSHORT(frm, localid);	/* local ID */
		ADDSHORT(frm, peerid);	/* peer ID */
		break;
	case IEEE80211_MESH_PEER_LINK_CLOSE:
		if (peerid)
			*frm++ = 10;	/* length */
		else
			*frm++ = 8;	/* length */
		memcpy(frm, meshpeerproto, 4);
		frm += 4;
		ADDSHORT(frm, localid);	/* local ID */
		if (peerid)
			ADDSHORT(frm, peerid);	/* peer ID */
		ADDSHORT(frm, reason);
		break;
	}
	return frm;
}

/*
 * Compute an Airtime Link Metric for the link with this node.
 *
 * Based on Draft 3.0 spec (11B.10, p.149).
 */
/*
 * Max 802.11s overhead.
 */
#define IEEE80211_MESH_MAXOVERHEAD \
	(sizeof(struct ieee80211_qosframe_addr4) \
	 + sizeof(struct ieee80211_meshcntl_ae11) \
	+ sizeof(struct llc) \
	+ IEEE80211_ADDR_LEN \
	+ IEEE80211_WEP_IVLEN \
	+ IEEE80211_WEP_KIDLEN \
	+ IEEE80211_WEP_CRCLEN \
	+ IEEE80211_WEP_MICLEN \
	+ IEEE80211_CRC_LEN)
uint32_t
mesh_airtime_calc(struct ieee80211_node *ni)
{
#define M_BITS 8
#define S_FACTOR (2 * M_BITS)
	struct ieee80211com *ic = ni->ni_ic;
	struct ifnet *ifp = ni->ni_vap->iv_ifp;
	const static int nbits = 8192 << M_BITS;
	uint32_t overhead, rate, errrate;
	uint64_t res;

	/* Time to transmit a frame */
	rate = ni->ni_txrate;
	overhead = ieee80211_compute_duration(ic->ic_rt,
	    ifp->if_mtu + IEEE80211_MESH_MAXOVERHEAD, rate, 0) << M_BITS;
	/* Error rate in percentage */
	/* XXX assuming small failures are ok */
	errrate = (((ifp->if_oerrors +
	    ifp->if_ierrors) / 100) << M_BITS) / 100;
	res = (overhead + (nbits / rate)) *
	    ((1 << S_FACTOR) / ((1 << M_BITS) - errrate));

	return (uint32_t)(res >> S_FACTOR);
#undef M_BITS
#undef S_FACTOR
}

/*
 * Add a Mesh Link Metric report IE to a frame.
 */
uint8_t *
ieee80211_add_meshlmetric(uint8_t *frm, uint32_t metric)
{
	*frm++ = IEEE80211_ELEMID_MESHLINK;
	*frm++ = 4;
	ADDWORD(frm, metric);
	return frm;
}
#undef ADDSHORT
#undef ADDWORD

/*
 * Initialize any mesh-specific node state.
 */
void
ieee80211_mesh_node_init(struct ieee80211vap *vap, struct ieee80211_node *ni)
{
	ni->ni_flags |= IEEE80211_NODE_QOS;
	callout_init(&ni->ni_mltimer, CALLOUT_MPSAFE);
}

/*
 * Cleanup any mesh-specific node state.
 */
void
ieee80211_mesh_node_cleanup(struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211_mesh_state *ms = vap->iv_mesh;

	callout_drain(&ni->ni_mltimer);
	/* NB: short-circuit callbacks after mesh_vdetach */
	if (vap->iv_mesh != NULL)
		ms->ms_ppath->mpp_peerdown(ni);
}

void
ieee80211_parse_meshid(struct ieee80211_node *ni, const uint8_t *ie)
{
	ni->ni_meshidlen = ie[1];
	memcpy(ni->ni_meshid, ie + 2, ie[1]);
}

/*
 * Setup mesh-specific node state on neighbor discovery.
 */
void
ieee80211_mesh_init_neighbor(struct ieee80211_node *ni,
	const struct ieee80211_frame *wh,
	const struct ieee80211_scanparams *sp)
{
	ieee80211_parse_meshid(ni, sp->meshid);
}

void
ieee80211_mesh_update_beacon(struct ieee80211vap *vap,
	struct ieee80211_beacon_offsets *bo)
{
	KASSERT(vap->iv_opmode == IEEE80211_M_MBSS, ("not a MBSS vap"));

	if (isset(bo->bo_flags, IEEE80211_BEACON_MESHCONF)) {
		(void)ieee80211_add_meshconf(bo->bo_meshconf, vap);
		clrbit(bo->bo_flags, IEEE80211_BEACON_MESHCONF);
	}
}

static int
mesh_ioctl_get80211(struct ieee80211vap *vap, struct ieee80211req *ireq)
{
	struct ieee80211_mesh_state *ms = vap->iv_mesh;
	uint8_t tmpmeshid[IEEE80211_NWID_LEN];
	struct ieee80211_mesh_route *rt;
	struct ieee80211req_mesh_route *imr;
	size_t len, off;
	uint8_t *p;
	int error;

	if (vap->iv_opmode != IEEE80211_M_MBSS)
		return ENOSYS;

	error = 0;
	switch (ireq->i_type) {
	case IEEE80211_IOC_MESH_ID:
		ireq->i_len = ms->ms_idlen;
		memcpy(tmpmeshid, ms->ms_id, ireq->i_len);
		error = copyout(tmpmeshid, ireq->i_data, ireq->i_len);
		break;
	case IEEE80211_IOC_MESH_AP:
		ireq->i_val = (ms->ms_flags & IEEE80211_MESHFLAGS_AP) != 0;
		break;
	case IEEE80211_IOC_MESH_FWRD:
		ireq->i_val = (ms->ms_flags & IEEE80211_MESHFLAGS_FWD) != 0;
		break;
	case IEEE80211_IOC_MESH_TTL:
		ireq->i_val = ms->ms_ttl;
		break;
	case IEEE80211_IOC_MESH_RTCMD:
		switch (ireq->i_val) {
		case IEEE80211_MESH_RTCMD_LIST:
			len = 0;
			MESH_RT_LOCK(ms);
			TAILQ_FOREACH(rt, &ms->ms_routes, rt_next) {
				len += sizeof(*imr);
			}
			MESH_RT_UNLOCK(ms);
			if (len > ireq->i_len || ireq->i_len < sizeof(*imr)) {
				ireq->i_len = len;
				return ENOMEM;
			}
			ireq->i_len = len;
			/* XXX M_WAIT? */
			p = malloc(len, M_TEMP, M_NOWAIT | M_ZERO);
			if (p == NULL)
				return ENOMEM;
			off = 0;
			MESH_RT_LOCK(ms);
			TAILQ_FOREACH(rt, &ms->ms_routes, rt_next) {
				if (off >= len)
					break;
				imr = (struct ieee80211req_mesh_route *)
				    (p + off);
				imr->imr_flags = rt->rt_flags;
				IEEE80211_ADDR_COPY(imr->imr_dest,
				    rt->rt_dest);
				IEEE80211_ADDR_COPY(imr->imr_nexthop,
				    rt->rt_nexthop);
				imr->imr_metric = rt->rt_metric;
				imr->imr_nhops = rt->rt_nhops;
				imr->imr_lifetime = rt->rt_lifetime;
				imr->imr_lastmseq = rt->rt_lastmseq;
				off += sizeof(*imr);
			}
			MESH_RT_UNLOCK(ms);
			error = copyout(p, (uint8_t *)ireq->i_data,
			    ireq->i_len);
			free(p, M_TEMP);
			break;
		case IEEE80211_MESH_RTCMD_FLUSH:
		case IEEE80211_MESH_RTCMD_ADD:
		case IEEE80211_MESH_RTCMD_DELETE:
			return EINVAL;
		default:
			return ENOSYS;
		}
		break;
	case IEEE80211_IOC_MESH_PR_METRIC:
		len = strlen(ms->ms_pmetric->mpm_descr);
		if (ireq->i_len < len)
			return EINVAL;
		ireq->i_len = len;
		error = copyout(ms->ms_pmetric->mpm_descr,
		    (uint8_t *)ireq->i_data, len);
		break;
	case IEEE80211_IOC_MESH_PR_PATH:
		len = strlen(ms->ms_ppath->mpp_descr);
		if (ireq->i_len < len)
			return EINVAL;
		ireq->i_len = len;
		error = copyout(ms->ms_ppath->mpp_descr,
		    (uint8_t *)ireq->i_data, len);
		break;
	default:
		return ENOSYS;
	}

	return error;
}
IEEE80211_IOCTL_GET(mesh, mesh_ioctl_get80211);

static int
mesh_ioctl_set80211(struct ieee80211vap *vap, struct ieee80211req *ireq)
{
	struct ieee80211_mesh_state *ms = vap->iv_mesh;
	uint8_t tmpmeshid[IEEE80211_NWID_LEN];
	uint8_t tmpaddr[IEEE80211_ADDR_LEN];
	char tmpproto[IEEE80211_MESH_PROTO_DSZ];
	int error;

	if (vap->iv_opmode != IEEE80211_M_MBSS)
		return ENOSYS;

	error = 0;
	switch (ireq->i_type) {
	case IEEE80211_IOC_MESH_ID:
		if (ireq->i_val != 0 || ireq->i_len > IEEE80211_MESHID_LEN)
			return EINVAL;
		error = copyin(ireq->i_data, tmpmeshid, ireq->i_len);
		if (error != 0)
			break;
		memset(ms->ms_id, 0, IEEE80211_NWID_LEN);
		ms->ms_idlen = ireq->i_len;
		memcpy(ms->ms_id, tmpmeshid, ireq->i_len);
		error = ENETRESET;
		break;
	case IEEE80211_IOC_MESH_AP:
		if (ireq->i_val)
			ms->ms_flags |= IEEE80211_MESHFLAGS_AP;
		else
			ms->ms_flags &= ~IEEE80211_MESHFLAGS_AP;
		error = ENETRESET;
		break;
	case IEEE80211_IOC_MESH_FWRD:
		if (ireq->i_val)
			ms->ms_flags |= IEEE80211_MESHFLAGS_FWD;
		else
			ms->ms_flags &= ~IEEE80211_MESHFLAGS_FWD;
		break;
	case IEEE80211_IOC_MESH_TTL:
		ms->ms_ttl = (uint8_t) ireq->i_val;
		break;
	case IEEE80211_IOC_MESH_RTCMD:
		switch (ireq->i_val) {
		case IEEE80211_MESH_RTCMD_LIST:
			return EINVAL;
		case IEEE80211_MESH_RTCMD_FLUSH:
			ieee80211_mesh_rt_flush(vap);
			break;
		case IEEE80211_MESH_RTCMD_ADD:
			if (IEEE80211_ADDR_EQ(vap->iv_myaddr, ireq->i_data) ||
			    IEEE80211_ADDR_EQ(broadcastaddr, ireq->i_data))
				return EINVAL;
			error = copyin(ireq->i_data, &tmpaddr,
			    IEEE80211_ADDR_LEN);
			if (error == 0)
				ieee80211_mesh_discover(vap, tmpaddr, NULL);
			break;
		case IEEE80211_MESH_RTCMD_DELETE:
			ieee80211_mesh_rt_del(vap, ireq->i_data);
			break;
		default:
			return ENOSYS;
		}
		break;
	case IEEE80211_IOC_MESH_PR_METRIC:
		error = copyin(ireq->i_data, tmpproto, sizeof(tmpproto));
		if (error == 0) {
			error = mesh_select_proto_metric(vap, tmpproto);
			if (error == 0)
				error = ENETRESET;
		}
		break;
	case IEEE80211_IOC_MESH_PR_PATH:
		error = copyin(ireq->i_data, tmpproto, sizeof(tmpproto));
		if (error == 0) {
			error = mesh_select_proto_path(vap, tmpproto);
			if (error == 0)
				error = ENETRESET;
		}
		break;
	default:
		return ENOSYS;
	}
	return error;
}
IEEE80211_IOCTL_SET(mesh, mesh_ioctl_set80211);
