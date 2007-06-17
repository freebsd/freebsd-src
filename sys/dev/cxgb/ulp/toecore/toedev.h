
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

$FreeBSD$

***************************************************************************/

#ifndef _OFFLOAD_DEV_H_
#define _OFFLOAD_DEV_H_


/* Parameter values for offload_get_phys_egress() */
enum {
	TOE_OPEN,
	TOE_FAILOVER,
};

/* Parameter values for toe_failover() */
enum {
	TOE_ACTIVE_SLAVE,
	TOE_LINK_DOWN,
	TOE_LINK_UP,
	TOE_RELEASE,
	TOE_RELEASE_ALL,
};


#define TOENAMSIZ 16

/* belongs in linux/netdevice.h */
#define NETIF_F_TCPIP_OFFLOAD (1 << 15)

/* Get the toedev associated with a ifnet */
#define TOEDEV(netdev) (*(struct toedev **)&(netdev)->if_softc)

/* offload type ids */
enum {
	TOE_ID_CHELSIO_T1 = 1,
	TOE_ID_CHELSIO_T1C,
	TOE_ID_CHELSIO_T2,
	TOE_ID_CHELSIO_T3,
	TOE_ID_CHELSIO_T3B,
};

struct offload_id {
	unsigned int id;
	unsigned long data;
};

struct rtentry;
struct ifnet;
struct tom_info;
struct sysctl_oid;
struct socket;
struct mbuf;

enum toetype {
        T3A = 0,
        T3B
};

struct toedev {
	char name[TOENAMSIZ];       /* TOE device name */
	enum toetype type;
	struct adapter *adapter;
	unsigned int ttid;          /* TOE type id */
	unsigned long flags;        /* device flags */
	unsigned int mtu;           /* max size of TX offloaded data */
	unsigned int nconn;         /* max # of offloaded connections */
	struct ifnet *lldev;   /* LL device associated with TOE messages */
	const struct tom_info *offload_mod; /* attached TCP offload module */
	struct sysctl_oid *sysctl_root;    /* root of proc dir for this TOE */
	TAILQ_ENTRY(toedev) ofld_entry;  /* for list linking */
	int (*open)(struct toedev *dev);
	int (*close)(struct toedev *dev);
	int (*can_offload)(struct toedev *dev, struct socket *so);
	int (*connect)(struct toedev *dev, struct socket *so,
		       struct ifnet *egress_ifp);
	int (*send)(struct toedev *dev, struct mbuf *m);
	int (*recv)(struct toedev *dev, struct mbuf **m, int n);
	int (*ctl)(struct toedev *dev, unsigned int req, void *data);
	void (*neigh_update)(struct toedev *dev, struct rtentry *neigh);
	void (*failover)(struct toedev *dev, struct ifnet *bond_ifp,
			 struct ifnet *ndev, int event);
	void *priv;                 /* driver private data */
	void *l2opt;                /* optional layer 2 data */
	void *l3opt;                /* optional layer 3 data */
	void *l4opt;                /* optional layer 4 data */
	void *ulp;                  /* ulp stuff */
};

struct tom_info {
	int (*attach)(struct toedev *dev, const struct offload_id *entry);
	int (*detach)(struct toedev *dev);
	const char *name;
	const struct offload_id *id_table;
	TAILQ_ENTRY(tom_info) entry;
};

static inline void init_offload_dev(struct toedev *dev)
{

}

extern int register_tom(struct tom_info *t);
extern int unregister_tom(struct tom_info *t);
extern int register_toedev(struct toedev *dev, const char *name);
extern int unregister_toedev(struct toedev *dev);
extern int activate_offload(struct toedev *dev);
extern int toe_send(struct toedev *dev, struct mbuf *m);
extern struct ifnet *offload_get_phys_egress(struct ifnet *dev,
						  struct socket *so,
						  int context);

#if defined(CONFIG_TCP_OFFLOAD_MODULE)
static inline int toe_receive_mbuf(struct toedev *dev, struct mbuf **m,
				  int n)
{
	return dev->recv(dev, m, n);
}

extern int  prepare_tcp_for_offload(void);
extern void restore_tcp_to_nonoffload(void);
#elif defined(CONFIG_TCP_OFFLOAD)
extern int toe_receive_mbuf(struct toedev *dev, struct mbuf **m, int n);
#endif

#if defined(CONFIG_TCP_OFFLOAD) || \
    (defined(CONFIG_TCP_OFFLOAD_MODULE) && defined(MODULE))
extern void toe_neigh_update(struct rtentry *neigh);
extern void toe_failover(struct ifnet *bond_ifp,
			 struct ifnet *fail_ifp, int event);
extern int toe_enslave(struct ifnet *bond_ifp,
		       struct ifnet *slave_ifp);
#else
static inline void toe_neigh_update(struct ifnet *neigh) {}
static inline void toe_failover(struct ifnet *bond_ifp,
				struct ifnet *fail_ifp, int event)
{}
static inline int toe_enslave(struct ifnet *bond_ifp,
			      struct ifnet *slave_ifp)
{
	return 0;
}
#endif /* CONFIG_TCP_OFFLOAD */

#endif /* _OFFLOAD_DEV_H_ */
