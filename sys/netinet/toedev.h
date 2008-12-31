/*-
 * Copyright (c) 2007-2008, Chelsio Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Neither the name of the Chelsio Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/netinet/toedev.h,v 1.5.2.3.2.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef _NETINET_TOEDEV_H_
#define	_NETINET_TOEDEV_H_

#ifndef _KERNEL
#error "no user-serviceable parts inside"
#endif

extern uint32_t toedev_registration_count;

/* Parameter values for offload_get_phys_egress(). */
enum {
	TOE_OPEN,
	TOE_FAILOVER,
};

/* Parameter values for toe_failover(). */
enum {
	TOE_ACTIVE_SLAVE,
	TOE_LINK_DOWN,
	TOE_LINK_UP,
	TOE_RELEASE,
	TOE_RELEASE_ALL,
};

#define	TOENAMSIZ	16

/* Get the toedev associated with a ifnet. */
#define	TOEDEV(ifp)	((ifp)->if_llsoftc)

struct offload_id {
	unsigned int	id;
	unsigned long	data;
};

struct ifnet;
struct rt_entry;
struct tom_info;
struct sysctl_oid;
struct socket;
struct mbuf;

struct toedev {
	TAILQ_ENTRY(toedev) entry;  
	char 		tod_name[TOENAMSIZ];	/* TOE device name */
	unsigned int 	tod_ttid;		/* TOE type id */
	unsigned long 	tod_flags;		/* device flags */
	unsigned int	tod_mtu;		/* max TX offloaded data */
	unsigned int	tod_nconn;		/* max # of offloaded
						 * connections
						 */
	struct ifnet 	*tod_lldev;   		/* first interface */
	const struct tom_info *tod_offload_mod; /* TCP offload module */

	/*
	 * This TOE device is capable of offloading the connection for socket so
	 */
	int	(*tod_can_offload)(struct toedev *dev, struct socket *so);

	/*
	 * Establish a connection to nam using the TOE device dev
	 */
	int	(*tod_connect)(struct toedev *dev, struct socket *so,
	        struct rtentry *rt, struct sockaddr *nam);
	/*
	 * Send an mbuf down to the toe device 
	 */
	int	(*tod_send)(struct toedev *dev, struct mbuf *m);
	/*
	 * Receive an array of mbufs from the TOE device dev 
	 */
	int	(*tod_recv)(struct toedev *dev, struct mbuf **m, int n);
	/*
	 * Device specific ioctl interface
	 */
	int	(*tod_ctl)(struct toedev *dev, unsigned int req, void *data);
	/*
	 * Update L2 entry in toedev 
	 */
	void	(*tod_arp_update)(struct toedev *dev, struct rtentry *neigh);
	/*
	 * Failover from one toe device to another
	 */
	void	(*tod_failover)(struct toedev *dev, struct ifnet *bond_ifp,
			 struct ifnet *ndev, int event);
	void	*tod_priv;			/* driver private data */
	void 	*tod_l2opt;			/* optional layer 2 data */
	void	*tod_l3opt; 			/* optional layer 3 data */
	void 	*tod_l4opt;			/* optional layer 4 data */
	void 	*tod_ulp;			/* upper lever protocol */
};

struct tom_info {
	TAILQ_ENTRY(tom_info)	entry;
	int		(*ti_attach)(struct toedev *dev,
	                             const struct offload_id *entry);
	int		(*ti_detach)(struct toedev *dev);
	const char	*ti_name;
	const struct offload_id	*ti_id_table;
};

static __inline void
init_offload_dev(struct toedev *dev)
{
}

int	register_tom(struct tom_info *t);
int	unregister_tom(struct tom_info *t);
int	register_toedev(struct toedev *dev, const char *name);
int	unregister_toedev(struct toedev *dev);
int	activate_offload(struct toedev *dev);
int	toe_send(struct toedev *dev, struct mbuf *m);
void	toe_arp_update(struct rtentry *rt);
struct ifnet	*offload_get_phys_egress(struct ifnet *ifp,
        struct socket *so, int context);
int 	toe_receive_mbuf(struct toedev *dev, struct mbuf **m, int n);

static __inline void
toe_neigh_update(struct ifnet *ifp)
{
}

static __inline void
toe_failover(struct ifnet *bond_ifp, struct ifnet *fail_ifp, int event)
{
}

static __inline int
toe_enslave(struct ifnet *bond_ifp, struct ifnet *slave_ifp)
{
	return (0);
}

#endif /* _NETINET_TOEDEV_H_ */
