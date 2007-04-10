/*	$OpenBSD: if_trunk.h,v 1.11 2007/01/31 06:20:19 reyk Exp $	*/

/*
 * Copyright (c) 2005, 2006 Reyk Floeter <reyk@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $FreeBSD$
 */

#ifndef _NET_TRUNK_H
#define _NET_TRUNK_H

/*
 * Global definitions
 */

#define TRUNK_MAX_PORTS		32	/* logically */
#define TRUNK_MAX_NAMESIZE	32	/* name of a protocol */
#define TRUNK_MAX_STACKING	4	/* maximum number of stacked trunks */

/* Port flags */
#define TRUNK_PORT_SLAVE	0x00000000	/* normal enslaved port */
#define TRUNK_PORT_MASTER	0x00000001	/* primary port */
#define TRUNK_PORT_STACK	0x00000002	/* stacked trunk port */
#define TRUNK_PORT_ACTIVE	0x00000004	/* port is active */
#define TRUNK_PORT_COLLECTING	0x00000008	/* port is active */
#define TRUNK_PORT_DISTRIBUTING	0x00000010	/* port is active */
#define TRUNK_PORT_GLOBAL	0x80000000	/* IOCTL: global flag */
#define TRUNK_PORT_BITS		"\20\01MASTER\02STACK\03ACTIVE\04COLLECTING" \
				  "\05DISTRIBUTING"

/* Supported trunk PROTOs */
#define	TRUNK_PROTO_NONE		0	/* no trunk protocol defined */
#define	TRUNK_PROTO_ROUNDROBIN		1	/* simple round robin */
#define	TRUNK_PROTO_FAILOVER		2	/* active failover */
#define	TRUNK_PROTO_LOADBALANCE		3	/* loadbalance */
#define	TRUNK_PROTO_LACP		4	/* 802.3ad lacp */
#define	TRUNK_PROTO_ETHERCHANNEL	5	/* Cisco FEC */
#define	TRUNK_PROTO_MAX			6

struct trunk_protos {
	const char		*tpr_name;
	int			tpr_proto;
};

#define	TRUNK_PROTO_DEFAULT	TRUNK_PROTO_FAILOVER
#define TRUNK_PROTOS	{						\
	{ "failover",		TRUNK_PROTO_FAILOVER },			\
	{ "fec",		TRUNK_PROTO_ETHERCHANNEL },		\
	{ "lacp",		TRUNK_PROTO_LACP },			\
	{ "loadbalance",	TRUNK_PROTO_LOADBALANCE },		\
	{ "roundrobin",		TRUNK_PROTO_ROUNDROBIN },		\
	{ "none",		TRUNK_PROTO_NONE },			\
	{ "default",		TRUNK_PROTO_DEFAULT }			\
}

/*
 * Trunk ioctls.
 */

/* Trunk port settings */
struct trunk_reqport {
	char			rp_ifname[IFNAMSIZ];	/* name of the trunk */
	char			rp_portname[IFNAMSIZ];	/* name of the port */
	u_int32_t		rp_prio;		/* port priority */
	u_int32_t		rp_flags;		/* port flags */
};

#define SIOCGTRUNKPORT		_IOWR('i', 140, struct trunk_reqport)
#define SIOCSTRUNKPORT		 _IOW('i', 141, struct trunk_reqport)
#define SIOCSTRUNKDELPORT	 _IOW('i', 142, struct trunk_reqport)

/* Trunk, ports and options */
struct trunk_reqall {
	char			ra_ifname[IFNAMSIZ];	/* name of the trunk */
	u_int			ra_proto;		/* trunk protocol */

	size_t			ra_size;		/* size of buffer */
	struct trunk_reqport	*ra_port;		/* allocated buffer */
	int			ra_ports;		/* total port count */
};

#define SIOCGTRUNK		_IOWR('i', 143, struct trunk_reqall)
#define SIOCSTRUNK		 _IOW('i', 144, struct trunk_reqall)

#ifdef _KERNEL
/*
 * Internal kernel part
 */

#define tp_ifname		tp_ifp->if_xname	/* interface name */
#define tp_link_state		tp_ifp->if_link_state	/* link state */
#define tp_capabilities		tp_ifp->if_capabilities	/* capabilities */

#define TRUNK_PORTACTIVE(_tp)	(					\
	((_tp)->tp_link_state == LINK_STATE_UP) &&			\
	((_tp)->tp_ifp->if_flags & IFF_UP)					\
)

#define mc_enm	mc_u.mcu_enm

struct trunk_ifreq {
	union {
		struct ifreq ifreq;
		struct {
			char ifr_name[IFNAMSIZ];
			struct sockaddr_storage ifr_ss;
		} ifreq_storage;
	} ifreq;
};

#define tr_ifflags		tr_ifp->if_flags		/* flags */
#define tr_ifname		tr_ifp->if_xname		/* name */
#define tr_capabilities		tr_ifp->if_capabilities	/* capabilities */

#define IFCAP_TRUNK_MASK	0xffff0000	/* private capabilities */
#define IFCAP_TRUNK_FULLDUPLEX	0x00010000	/* full duplex with >1 ports */

/* Private data used by the loadbalancing protocol */
#define TRUNK_LB_MAXKEYS	8
struct trunk_lb {
	u_int32_t		lb_key;
	struct trunk_port	*lb_ports[TRUNK_MAX_PORTS];
};

struct trunk_mc {
	union {
		struct ether_multi	*mcu_enm;
	} mc_u;
	struct sockaddr_storage		mc_addr;

	SLIST_ENTRY(trunk_mc)		mc_entries;
};

struct trunk_softc {
	struct ifnet			*tr_ifp;	/* virtual interface */
	struct mtx			tr_mtx;
	int				tr_proto;	/* trunk protocol */
	u_int				tr_count;	/* number of ports */
	struct trunk_port		*tr_primary;	/* primary port */
	struct ifmedia			tr_media;	/* media config */
	caddr_t				tr_psc;		/* protocol data */

	SLIST_HEAD(__tplhd, trunk_port)	tr_ports;	/* list of interfaces */
	SLIST_ENTRY(trunk_softc)	tr_entries;

	SLIST_HEAD(__mclhd, trunk_mc)	tr_mc_head;	/* multicast addresses */

	/* Trunk protocol callbacks */
	int	(*tr_detach)(struct trunk_softc *);
	int	(*tr_start)(struct trunk_softc *, struct mbuf *);
	struct mbuf *(*tr_input)(struct trunk_softc *, struct trunk_port *,
		    struct mbuf *);
	int	(*tr_port_create)(struct trunk_port *);
	void	(*tr_port_destroy)(struct trunk_port *);
	void	(*tr_linkstate)(struct trunk_port *);
	void	(*tr_init)(struct trunk_softc *);
	void	(*tr_stop)(struct trunk_softc *);
	void	(*tr_lladdr)(struct trunk_softc *);
};

struct trunk_port {
	struct ifnet			*tp_ifp;	/* physical interface */
	struct trunk_softc		*tp_trunk;	/* parent trunk */
	uint8_t				tp_lladdr[ETHER_ADDR_LEN];

	u_char				tp_iftype;	/* interface type */
	uint32_t			tp_prio;	/* port priority */
	uint32_t			tp_flags;	/* port flags */
	int				tp_ifflags;	/* saved ifp flags */
	void				*lh_cookie;	/* if state hook */
	caddr_t				tp_psc;		/* protocol data */

	/* Redirected callbacks */
	int	(*tp_ioctl)(struct ifnet *, u_long, caddr_t);
	int	(*tp_output)(struct ifnet *, struct mbuf *, struct sockaddr *,
		     struct rtentry *);

	SLIST_ENTRY(trunk_port)		tp_entries;
};

#define TRUNK_LOCK_INIT(_tr)	mtx_init(&(_tr)->tr_mtx, "if_trunk", NULL, \
				    MTX_DEF)
#define TRUNK_LOCK_DESTROY(_tr)	mtx_destroy(&(_tr)->tr_mtx)
#define TRUNK_LOCK(_tr)		mtx_lock(&(_tr)->tr_mtx)
#define TRUNK_UNLOCK(_tr)	mtx_unlock(&(_tr)->tr_mtx)
#define TRUNK_LOCKED(_tr)	mtx_owned(&(_tr)->tr_mtx)
#define TRUNK_LOCK_ASSERT(_tr)	mtx_assert(&(_tr)->tr_mtx, MA_OWNED)

extern struct mbuf *(*trunk_input_p)(struct ifnet *, struct mbuf *);
extern void	(*trunk_linkstate_p)(struct ifnet *, int );

int		trunk_enqueue(struct ifnet *, struct mbuf *);
uint32_t	trunk_hashmbuf(struct mbuf *, uint32_t);

#endif /* _KERNEL */

#endif /* _NET_TRUNK_H */
