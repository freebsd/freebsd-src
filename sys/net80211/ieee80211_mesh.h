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
 * 
 * $FreeBSD$
 */
#ifndef _NET80211_IEEE80211_MESH_H_
#define _NET80211_IEEE80211_MESH_H_

#define	IEEE80211_MESH_DEFAULT_TTL	31

/*
 * NB: all structures are __packed  so sizeof works on arm, et. al.
 */
/*
 * 802.11s Information Elements.
*/
/* Mesh Configuration */
struct ieee80211_meshconf_ie {
	uint8_t		conf_ie;	/* IEEE80211_ELEMID_MESHCONF */
	uint8_t		conf_len;
	uint8_t		conf_ver;
	uint8_t		conf_pselid[4];	/* Active Path Sel. Proto. ID */
	uint8_t		conf_pmetid[4];	/* APS Metric Identifier */
	uint8_t		conf_ccid[4];	/* Congestion Control Mode ID  */
	uint8_t		conf_syncid[4];	/* Sync. Protocol ID */
	uint8_t		conf_authid[4];	/* Auth. Protocol ID */
	uint8_t		conf_form;	/* Formation Information */
	uint8_t		conf_cap;
} __packed;

#define	IEEE80211_MESHCONF_VERSION		1
/* Null Protocol */
#define	IEEE80211_MESHCONF_NULL_OUI		0x00, 0x0f, 0xac
#define	IEEE80211_MESHCONF_NULL_VALUE		0xff
#define	IEEE80211_MESHCONF_NULL		{ IEEE80211_MESHCONF_NULL_OUI, \
					  IEEE80211_MESHCONF_NULL_VALUE }
/* Hybrid Wireless Mesh Protocol */
#define	IEEE80211_MESHCONF_HWMP_OUI		0x00, 0x0f, 0xac
#define	IEEE80211_MESHCONF_HWMP_VALUE		0x00
#define	IEEE80211_MESHCONF_HWMP		{ IEEE80211_MESHCONF_HWMP_OUI, \
					  IEEE80211_MESHCONF_HWMP_VALUE }
/* Airtime Link Metric */
#define	IEEE80211_MESHCONF_AIRTIME_OUI		0x00, 0x0f, 0xac
#define	IEEE80211_MESHCONF_AIRTIME_VALUE	0x00
#define	IEEE80211_MESHCONF_AIRTIME	{ IEEE80211_MESHCONF_AIRTIME_OUI, \
					  IEEE80211_MESHCONF_AIRTIME_VALUE }
/* Congestion Control Signaling */
#define	IEEE80211_MESHCONF_CCSIG_OUI		0x00, 0x0f, 0xac
#define	IEEE80211_MESHCONF_CCSIG_VALUE		0x00
#define	IEEE80211_MESHCONF_CCSIG	{ IEEE80211_MESHCONF_CCSIG_OUI,\
					  IEEE80211_MESHCONF_CCSIG_VALUE }
/* Neighbour Offset */
#define	IEEE80211_MESHCONF_NEIGHOFF_OUI		0x00, 0x0f, 0xac
#define	IEEE80211_MESHCONF_NEIGHOFF_VALUE	0x00
#define	IEEE80211_MESHCONF_NEIGHOFF	{ IEEE80211_MESHCONF_NEIGHOFF_OUI, \
					  IEEE80211_MESHCONF_NEIGHOFF_VALUE }
/* Simultaneous Authenticaction of Equals */
#define	IEEE80211_MESHCONF_SAE_OUI		0x00, 0x0f, 0xac
#define	IEEE80211_MESHCONF_SAE_VALUE		0x01
#define	IEEE80211_MESHCONF_SAE		{ IEEE80211_MESHCONF_SAE_OUI, \
					  IEEE80211_MESHCONF_SAE_VALUE }
#define	IEEE80211_MESHCONF_FORM_MP		0x01 /* Connected to Portal */
#define	IEEE80211_MESHCONF_FORM_NNEIGH_MASK	0x04 /* Number of Neighbours */
#define	IEEE80211_MESHCONF_CAP_AP	0x01	/* Accepting Peers */
#define	IEEE80211_MESHCONF_CAP_MCCAS	0x02	/* MCCA supported */
#define	IEEE80211_MESHCONF_CAP_MCCAE	0x04	/* MCCA enabled */
#define	IEEE80211_MESHCONF_CAP_FWRD 	0x08	/* forwarding enabled */
#define	IEEE80211_MESHCONF_CAP_BTR	0x10	/* Beacon Timing Report Enab */
#define	IEEE80211_MESHCONF_CAP_TBTTA	0x20	/* TBTT Adj. Enabled */
#define	IEEE80211_MESHCONF_CAP_PSL	0x40	/* Power Save Level */

/* Mesh Identifier */
struct ieee80211_meshid_ie {
	uint8_t		id_ie;		/* IEEE80211_ELEMID_MESHID */
	uint8_t		id_len;
} __packed;

/* Link Metric Report */
struct ieee80211_meshlmetric_ie {
	uint8_t		lm_ie;	/* IEEE80211_ELEMID_MESHLINK */
	uint8_t		lm_len;
	uint32_t	lm_metric;
#define	IEEE80211_MESHLMETRIC_INITIALVAL	0
} __packed;

/* Congestion Notification */
struct ieee80211_meshcngst_ie {
	uint8_t		cngst_ie;	/* IEEE80211_ELEMID_MESHCNGST */
	uint8_t		cngst_len;
	uint16_t	cngst_timer[4];	/* Expiration Timers: AC_BK,
					   AC_BE, AC_VI, AC_VO */
} __packed;

/* Peer Link Management */
struct ieee80211_meshpeer_ie {
	uint8_t		peer_ie;	/* IEEE80211_ELEMID_MESHPEER */
	uint8_t		peer_len;
	uint8_t		peer_proto[4];	/* Peer Management Protocol */
	uint16_t	peer_llinkid;	/* Local Link ID */
	uint16_t	peer_linkid;	/* Peer Link ID */
	uint16_t	peer_rcode;
} __packed;

enum {
	IEEE80211_MESH_PEER_LINK_OPEN		= 0,
	IEEE80211_MESH_PEER_LINK_CONFIRM	= 1,
	IEEE80211_MESH_PEER_LINK_CLOSE		= 2,
	/* values 3-255 are reserved */
};

/* Mesh Peering Management Protocol */
#define	IEEE80211_MESH_PEER_PROTO_OUI		0x00, 0x0f, 0xac
#define	IEEE80211_MESH_PEER_PROTO_VALUE		0x2a
#define	IEEE80211_MESH_PEER_PROTO	{ IEEE80211_MESH_PEER_PROTO_OUI, \
					  IEEE80211_MESH_PEER_PROTO_VALUE }
/* Abbreviated Handshake Protocol */
#define	IEEE80211_MESH_PEER_PROTO_AH_OUI	0x00, 0x0f, 0xac
#define	IEEE80211_MESH_PEER_PROTO_AH_VALUE	0x2b
#define	IEEE80211_MESH_PEER_PROTO_AH	{ IEEE80211_MESH_PEER_PROTO_AH_OUI, \
					  IEEE80211_MESH_PEER_PROTO_AH_VALUE }
#ifdef notyet
/* Mesh Channel Switch Annoucement */
struct ieee80211_meshcsa_ie {
	uint8_t		csa_ie;		/* IEEE80211_ELEMID_MESHCSA */
	uint8_t		csa_len;
	uint8_t		csa_mode;
	uint8_t		csa_newclass;	/* New Regulatory Class */
	uint8_t		csa_newchan;
	uint8_t		csa_precvalue;	/* Precedence Value */
	uint8_t		csa_count;
} __packed;

/* Mesh TIM */
/* Equal to the non Mesh version */

/* Mesh Awake Window */
struct ieee80211_meshawakew_ie {
	uint8_t		awakew_ie;		/* IEEE80211_ELEMID_MESHAWAKEW */
	uint8_t		awakew_len;
	uint8_t		awakew_windowlen;	/* in TUs */
} __packed;

/* Mesh Beacon Timing */
struct ieee80211_meshbeacont_ie {
	uint8_t		beacont_ie;		/* IEEE80211_ELEMID_MESHBEACONT */
	uint8_t		beacont_len;
	struct {
		uint8_t		mp_aid;		/* Least Octet of AID */
		uint16_t	mp_btime;	/* Beacon Time */
		uint16_t	mp_bint;	/* Beacon Interval */
	} __packed mp[1];			/* NB: variable size */
} __packed;
#endif

/* Portal (MP) Annoucement */
struct ieee80211_meshpann_ie {
	uint8_t		pann_ie;		/* IEEE80211_ELEMID_MESHPANN */
	uint8_t		pann_len;
	uint8_t		pann_flags;
	uint8_t		pann_hopcount;
	uint8_t		pann_ttl;
	uint8_t		pann_addr[IEEE80211_ADDR_LEN];
	uint8_t		pann_seq;		/* PANN Sequence Number */
} __packed;

/* Root (MP) Annoucement */
struct ieee80211_meshrann_ie {
	uint8_t		rann_ie;		/* IEEE80211_ELEMID_MESHRANN */
	uint8_t		rann_len;
	uint8_t		rann_flags;
#define	IEEE80211_MESHRANN_FLAGS_PR	0x01	/* Portal Role */
	uint8_t		rann_hopcount;
	uint8_t		rann_ttl;
	uint8_t		rann_addr[IEEE80211_ADDR_LEN];
	uint32_t	rann_seq;		/* HWMP Sequence Number */
	uint32_t	rann_metric;
} __packed;

/* Mesh Path Request */
struct ieee80211_meshpreq_ie {
	uint8_t		preq_ie;	/* IEEE80211_ELEMID_MESHPREQ */
	uint8_t		preq_len;
	uint8_t		preq_flags;
#define	IEEE80211_MESHPREQ_FLAGS_PR	0x01	/* Portal Role */
#define	IEEE80211_MESHPREQ_FLAGS_AM	0x02	/* 0 = ucast / 1 = bcast */
#define	IEEE80211_MESHPREQ_FLAGS_PP	0x04	/* Proactive PREP */
#define	IEEE80211_MESHPREQ_FLAGS_AE	0x40	/* Address Extension */
	uint8_t		preq_hopcount;
	uint8_t		preq_ttl;
	uint32_t	preq_id;
	uint8_t		preq_origaddr[IEEE80211_ADDR_LEN];
	uint32_t	preq_origseq;	/* HWMP Sequence Number */
	/* NB: may have Originator Proxied Address */
	uint32_t	preq_lifetime;
	uint32_t	preq_metric;
	uint8_t		preq_tcount;	/* target count */
	struct {
		uint8_t		target_flags;
#define	IEEE80211_MESHPREQ_TFLAGS_TO	0x01	/* Target Only */
#define	IEEE80211_MESHPREQ_TFLAGS_RF	0x02	/* Reply and Forward */
#define	IEEE80211_MESHPREQ_TFLAGS_USN	0x04	/* Unknown HWMP seq number */
		uint8_t		target_addr[IEEE80211_ADDR_LEN];
		uint32_t	target_seq;	/* HWMP Sequence Number */
	} __packed preq_targets[1];		/* NB: variable size */
} __packed;

/* Mesh Path Reply */
struct ieee80211_meshprep_ie {
	uint8_t		prep_ie;	/* IEEE80211_ELEMID_MESHPREP */
	uint8_t		prep_len;
	uint8_t		prep_flags;
	uint8_t		prep_hopcount;
	uint8_t		prep_ttl;
	uint8_t		prep_targetaddr[IEEE80211_ADDR_LEN];
	uint32_t	prep_targetseq;
	/* NB: May have Target Proxied Address */
	uint32_t	prep_lifetime;
	uint32_t	prep_metric;
	uint8_t		prep_origaddr[IEEE80211_ADDR_LEN];
	uint32_t	prep_origseq;	/* HWMP Sequence Number */
} __packed;

/* Mesh Path Error */
struct ieee80211_meshperr_ie {
	uint8_t		perr_ie;	/* IEEE80211_ELEMID_MESHPERR */
	uint8_t		perr_len;
	uint8_t		perr_ttl;
	uint8_t		perr_ndests;	/* Number of Destinations */
	struct {
		uint8_t		dest_flags;
#define	IEEE80211_MESHPERR_DFLAGS_USN	0x01
#define	IEEE80211_MESHPERR_DFLAGS_RC	0x02
		uint8_t		dest_addr[IEEE80211_ADDR_LEN];
		uint32_t	dest_seq;	/* HWMP Sequence Number */
		uint16_t	dest_rcode;
	} __packed perr_dests[1];		/* NB: variable size */
} __packed;

#ifdef notyet
/* Mesh Proxy Update */
struct ieee80211_meshpu_ie {
	uint8_t		pu_ie;		/* IEEE80211_ELEMID_MESHPU */
	uint8_t		pu_len;
	uint8_t		pu_flags;
#define	IEEE80211_MESHPU_FLAGS_MASK		0x1
#define	IEEE80211_MESHPU_FLAGS_DEL		0x0
#define	IEEE80211_MESHPU_FLAGS_ADD		0x1
	uint8_t		pu_seq;		/* PU Sequence Number */
	uint8_t		pu_addr[IEEE80211_ADDR_LEN];
	uint8_t		pu_naddr;	/* Number of Proxied Addresses */
	/* NB: proxied address follows */
} __packed;

/* Mesh Proxy Update Confirmation */
struct ieee80211_meshpuc_ie {
	uint8_t		puc_ie;		/* IEEE80211_ELEMID_MESHPUC */
	uint8_t		puc_len;
	uint8_t		puc_flags;
	uint8_t		puc_seq;	/* PU Sequence Number */
	uint8_t		puc_daddr[IEEE80211_ADDR_LEN];
} __packed;
#endif

/*
 * 802.11s Action Frames
 */
#define	IEEE80211_ACTION_CAT_MESHPEERING	30	/* XXX Linux */
#define	IEEE80211_ACTION_CAT_MESHLMETRIC	13
#define	IEEE80211_ACTION_CAT_MESHPATH		32	/* XXX Linux */
#define	IEEE80211_ACTION_CAT_INTERWORK		15
#define	IEEE80211_ACTION_CAT_RESOURCE		16
#define	IEEE80211_ACTION_CAT_PROXY		17

/*
 * Mesh Peering Action codes.
 */
enum {
	IEEE80211_ACTION_MESHPEERING_OPEN	= 0,
	IEEE80211_ACTION_MESHPEERING_CONFIRM	= 1,
	IEEE80211_ACTION_MESHPEERING_CLOSE	= 2,
	/* 3-255 reserved */
};

/*
 * Mesh Path Selection Action code.
 */
enum {
	IEEE80211_ACTION_MESHPATH_SEL	= 0,
	/* 1-255 reserved */
};

/*
 * Mesh Link Metric Action codes.
 */
enum {
	IEEE80211_ACTION_MESHLMETRIC_REQ = 0,	/* Link Metric Request */
	IEEE80211_ACTION_MESHLMETRIC_REP = 1,	/* Link Metric Report */
	/* 2-255 reserved */
};

/*
 * Mesh Portal Annoucement Action codes.
 */
enum {
	IEEE80211_ACTION_MESHPANN	= 0,
	/* 1-255 reserved */
};

/*
 * Different mesh control structures based on the AE
 * (Address Extension) bits.
 */
struct ieee80211_meshcntl {
	uint8_t		mc_flags;	/* Address Extension 00 */
	uint8_t		mc_ttl;		/* TTL */
	uint8_t		mc_seq[4];	/* Sequence No. */
	/* NB: more addresses may follow */
} __packed;

struct ieee80211_meshcntl_ae01 {
	uint8_t		mc_flags;	/* Address Extension 01 */
	uint8_t		mc_ttl;		/* TTL */
	uint8_t		mc_seq[4];	/* Sequence No. */
	uint8_t		mc_addr4[IEEE80211_ADDR_LEN];
} __packed;

struct ieee80211_meshcntl_ae10 {
	uint8_t		mc_flags;	/* Address Extension 10 */
	uint8_t		mc_ttl;		/* TTL */
	uint8_t		mc_seq[4];	/* Sequence No. */
	uint8_t		mc_addr4[IEEE80211_ADDR_LEN];
	uint8_t		mc_addr5[IEEE80211_ADDR_LEN];
} __packed;

struct ieee80211_meshcntl_ae11 {
	uint8_t		mc_flags;	/* Address Extension 11 */
	uint8_t		mc_ttl;		/* TTL */
	uint8_t		mc_seq[4];	/* Sequence No. */
	uint8_t		mc_addr4[IEEE80211_ADDR_LEN];
	uint8_t		mc_addr5[IEEE80211_ADDR_LEN];
	uint8_t		mc_addr6[IEEE80211_ADDR_LEN];
} __packed;

#ifdef _KERNEL
MALLOC_DECLARE(M_80211_MESH_RT);
struct ieee80211_mesh_route {
	TAILQ_ENTRY(ieee80211_mesh_route)	rt_next;
	int			rt_crtime;	/* creation time */
	uint8_t			rt_dest[IEEE80211_ADDR_LEN];
	uint8_t			rt_nexthop[IEEE80211_ADDR_LEN];
	uint32_t		rt_metric;	/* path metric */
	uint16_t		rt_nhops;	/* number of hops */
	uint16_t		rt_flags;
#define	IEEE80211_MESHRT_FLAGS_VALID	0x01	/* patch discovery complete */
#define	IEEE80211_MESHRT_FLAGS_PROXY	0x02	/* proxy entry */
	uint32_t		rt_lifetime;
	uint32_t		rt_lastmseq;	/* last seq# seen dest */
	void			*rt_priv;	/* private data */
};
#define	IEEE80211_MESH_ROUTE_PRIV(rt, cast)	((cast *)rt->rt_priv)

#define	IEEE80211_MESH_PROTO_DSZ	12	/* description size */
/*
 * Mesh Path Selection Protocol.
 */
enum ieee80211_state;
struct ieee80211_mesh_proto_path {
	char 		mpp_descr[IEEE80211_MESH_PROTO_DSZ];
	uint8_t		mpp_ie[4];
	struct ieee80211_node *
	    		(*mpp_discover)(struct ieee80211vap *,
				const uint8_t [IEEE80211_ADDR_LEN],
				struct mbuf *);
	void		(*mpp_peerdown)(struct ieee80211_node *);
	void		(*mpp_vattach)(struct ieee80211vap *);
	void		(*mpp_vdetach)(struct ieee80211vap *);
	int		(*mpp_newstate)(struct ieee80211vap *,
			    enum ieee80211_state, int);
	const size_t	mpp_privlen;	/* size required in the routing table
					   for private data */
	int		mpp_inact;	/* inact. timeout for invalid routes
					   (ticks) */
};

/*
 * Mesh Link Metric Report Protocol.
 */
struct ieee80211_mesh_proto_metric {
	char		mpm_descr[IEEE80211_MESH_PROTO_DSZ];
	uint8_t		mpm_ie[4];
	uint32_t	(*mpm_metric)(struct ieee80211_node *);
};

#ifdef notyet
/*
 * Mesh Authentication Protocol.
 */
struct ieee80211_mesh_proto_auth {
	uint8_t		mpa_ie[4];
};

struct ieee80211_mesh_proto_congestion {
};

struct ieee80211_mesh_proto_sync {
};
#endif

typedef uint32_t ieee80211_mesh_seq;
#define	IEEE80211_MESH_SEQ_LEQ(a, b)	((int32_t)((a)-(b)) <= 0)
#define	IEEE80211_MESH_SEQ_GEQ(a, b)	((int32_t)((a)-(b)) >= 0)

struct ieee80211_mesh_state {
	int				ms_idlen;
	uint8_t				ms_id[IEEE80211_MESHID_LEN];
	ieee80211_mesh_seq		ms_seq;	/* seq no for meshcntl */
	uint16_t			ms_neighbors;
	uint8_t				ms_ttl;	/* mesh ttl set in packets */
#define IEEE80211_MESHFLAGS_AP		0x01	/* accept peers */
#define IEEE80211_MESHFLAGS_PORTAL	0x02	/* mesh portal role */
#define IEEE80211_MESHFLAGS_FWD		0x04	/* forward packets */
	uint8_t				ms_flags;
	struct mtx			ms_rt_lock;
	struct callout			ms_cleantimer;
	TAILQ_HEAD(, ieee80211_mesh_route)  ms_routes;
	struct ieee80211_mesh_proto_metric *ms_pmetric;
	struct ieee80211_mesh_proto_path   *ms_ppath;
};
void		ieee80211_mesh_attach(struct ieee80211com *);
void		ieee80211_mesh_detach(struct ieee80211com *);

struct ieee80211_mesh_route *
		ieee80211_mesh_rt_find(struct ieee80211vap *,
		    const uint8_t [IEEE80211_ADDR_LEN]);
struct ieee80211_mesh_route *
                ieee80211_mesh_rt_add(struct ieee80211vap *,
		    const uint8_t [IEEE80211_ADDR_LEN]);
void		ieee80211_mesh_rt_del(struct ieee80211vap *,
		    const uint8_t [IEEE80211_ADDR_LEN]);
void		ieee80211_mesh_rt_flush(struct ieee80211vap *);
void		ieee80211_mesh_rt_flush_peer(struct ieee80211vap *,
		    const uint8_t [IEEE80211_ADDR_LEN]);
void		ieee80211_mesh_proxy_check(struct ieee80211vap *,
		    const uint8_t [IEEE80211_ADDR_LEN]);

int		ieee80211_mesh_register_proto_path(const
		    struct ieee80211_mesh_proto_path *);
int		ieee80211_mesh_register_proto_metric(const
		    struct ieee80211_mesh_proto_metric *);

uint8_t *	ieee80211_add_meshid(uint8_t *, struct ieee80211vap *);
uint8_t *	ieee80211_add_meshconf(uint8_t *, struct ieee80211vap *);
uint8_t *	ieee80211_add_meshpeer(uint8_t *, uint8_t, uint16_t, uint16_t,
		    uint16_t);
uint8_t *	ieee80211_add_meshlmetric(uint8_t *, uint32_t);

void		ieee80211_mesh_node_init(struct ieee80211vap *,
		    struct ieee80211_node *);
void		ieee80211_mesh_node_cleanup(struct ieee80211_node *);
void		ieee80211_parse_meshid(struct ieee80211_node *,
		    const uint8_t *);
struct ieee80211_scanparams;
void		ieee80211_mesh_init_neighbor(struct ieee80211_node *,
		   const struct ieee80211_frame *,
		   const struct ieee80211_scanparams *);

/*
 * Return non-zero if proxy operation is enabled.
 */
static __inline int
ieee80211_mesh_isproxyena(struct ieee80211vap *vap)
{
	struct ieee80211_mesh_state *ms = vap->iv_mesh;
	return (ms->ms_flags &
	    (IEEE80211_MESHFLAGS_AP | IEEE80211_MESHFLAGS_PORTAL)) != 0;
}

/*
 * Process an outbound frame: if a path is known to the
 * destination then return a reference to the next hop
 * for immediate transmission.  Otherwise initiate path
 * discovery and, if possible queue the packet to be
 * sent when path discovery completes.
 */
static __inline struct ieee80211_node *
ieee80211_mesh_discover(struct ieee80211vap *vap,
    const uint8_t dest[IEEE80211_ADDR_LEN], struct mbuf *m)
{
	struct ieee80211_mesh_state *ms = vap->iv_mesh;
	return ms->ms_ppath->mpp_discover(vap, dest, m);
}

#endif /* _KERNEL */
#endif /* !_NET80211_IEEE80211_MESH_H_ */
