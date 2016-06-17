/*
 * DECnet       An implementation of the DECnet protocol suite for the LINUX
 *              operating system.  DECnet is implemented using the  BSD Socket
 *              interface as the means of communication with the user level.
 *
 *              DECnet Device Layer
 *
 * Authors:     Steve Whitehouse <SteveW@ACM.org>
 *              Eduardo Marcelo Serrat <emserrat@geocities.com>
 *
 * Changes:
 *          Steve Whitehouse : Devices now see incoming frames so they
 *                             can mark on who it came from.
 *          Steve Whitehouse : Fixed bug in creating neighbours. Each neighbour
 *                             can now have a device specific setup func.
 *          Steve Whitehouse : Added /proc/sys/net/decnet/conf/<dev>/
 *          Steve Whitehouse : Fixed bug which sometimes killed timer
 *          Steve Whitehouse : Multiple ifaddr support
 *          Steve Whitehouse : SIOCGIFCONF is now a compile time option
 *          Steve Whitehouse : /proc/sys/net/decnet/conf/<sys>/forwarding
 *          Steve Whitehouse : Removed timer1 - its a user space issue now
 *         Patrick Caulfield : Fixed router hello message format
 */

#include <linux/config.h>
#include <linux/net.h>
#include <linux/netdevice.h>
#include <linux/proc_fs.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/if_arp.h>
#include <linux/if_ether.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/rtnetlink.h>
#include <linux/sysctl.h>
#include <asm/uaccess.h>
#include <net/neighbour.h>
#include <net/dst.h>
#include <net/dn.h>
#include <net/dn_dev.h>
#include <net/dn_route.h>
#include <net/dn_neigh.h>
#include <net/dn_fib.h>

#define DN_IFREQ_SIZE (sizeof(struct ifreq) - sizeof(struct sockaddr) + sizeof(struct sockaddr_dn))

static char dn_rt_all_end_mcast[ETH_ALEN] = {0xAB,0x00,0x00,0x04,0x00,0x00};
static char dn_rt_all_rt_mcast[ETH_ALEN]  = {0xAB,0x00,0x00,0x03,0x00,0x00};
static char dn_hiord[ETH_ALEN]            = {0xAA,0x00,0x04,0x00,0x00,0x00};
static unsigned char dn_eco_version[3]    = {0x02,0x00,0x00};

extern struct neigh_table dn_neigh_table;

struct net_device *decnet_default_device;

static struct dn_dev *dn_dev_create(struct net_device *dev, int *err);
static void dn_dev_delete(struct net_device *dev);
static void rtmsg_ifa(int event, struct dn_ifaddr *ifa);

static int dn_eth_up(struct net_device *);
static void dn_send_brd_hello(struct net_device *dev);
#if 0
static void dn_send_ptp_hello(struct net_device *dev);
#endif

static struct dn_dev_parms dn_dev_list[] =  {
{
	type:		ARPHRD_ETHER, /* Ethernet */
	mode:		DN_DEV_BCAST,
	state:		DN_DEV_S_RU,
	blksize:	1498,
	t2:		1,
	t3:		10,
	name:		"ethernet",
	ctl_name:	NET_DECNET_CONF_ETHER,
	up:		dn_eth_up,
	timer3:		dn_send_brd_hello,
},
{
	type:		ARPHRD_IPGRE, /* DECnet tunneled over GRE in IP */
	mode:		DN_DEV_BCAST,
	state:		DN_DEV_S_RU,
	blksize:	1400,
	t2:		1,
	t3:		10,
	name:		"ipgre",
	ctl_name:	NET_DECNET_CONF_GRE,
	timer3:		dn_send_brd_hello,
},
#if 0
{
	type:		ARPHRD_X25, /* Bog standard X.25 */
	mode:		DN_DEV_UCAST,
	state:		DN_DEV_S_DS,
	blksize:	230,
	t2:		1,
	t3:		120,
	name:		"x25",
	ctl_name:	NET_DECNET_CONF_X25,
	timer3:		dn_send_ptp_hello,
},
#endif
#if 0
{
	type:		ARPHRD_PPP, /* DECnet over PPP */
	mode:		DN_DEV_BCAST,
	state:		DN_DEV_S_RU,
	blksize:	230,
	t2:		1,
	t3:		10,
	name:		"ppp",
	ctl_name:	NET_DECNET_CONF_PPP,
	timer3:		dn_send_brd_hello,
},
#endif
#if 0
{
	type:		ARPHRD_DDCMP, /* DECnet over DDCMP */
	mode:		DN_DEV_UCAST,
	state:		DN_DEV_S_DS,
	blksize:	230,
	t2:		1,
	t3:		120,
	name:		"ddcmp",
	ctl_name:	NET_DECNET_CONF_DDCMP,
	timer3:		dn_send_ptp_hello,
},
#endif
{
	type:		ARPHRD_LOOPBACK, /* Loopback interface - always last */
	mode:		DN_DEV_BCAST,
	state:		DN_DEV_S_RU,
	blksize:	1498,
	t2:		1,
	t3:		10,
	name:		"loopback",
	ctl_name:	NET_DECNET_CONF_LOOPBACK,
	timer3:		dn_send_brd_hello,
}
};

#define DN_DEV_LIST_SIZE (sizeof(dn_dev_list)/sizeof(struct dn_dev_parms))

#define DN_DEV_PARMS_OFFSET(x) ((int) ((char *) &((struct dn_dev_parms *)0)->x))

#ifdef CONFIG_SYSCTL

static int min_t2[] = { 1 };
static int max_t2[] = { 60 }; /* No max specified, but this seems sensible */
static int min_t3[] = { 1 };
static int max_t3[] = { 8191 }; /* Must fit in 16 bits when multiplied by BCT3MULT or T3MULT */

static int min_priority[1];
static int max_priority[] = { 127 }; /* From DECnet spec */

static int dn_forwarding_proc(ctl_table *, int, struct file *,
			void *, size_t *);
static int dn_forwarding_sysctl(ctl_table *table, int *name, int nlen,
			void *oldval, size_t *oldlenp,
			void *newval, size_t newlen,
			void **context);

static struct dn_dev_sysctl_table {
	struct ctl_table_header *sysctl_header;
	ctl_table dn_dev_vars[5];
	ctl_table dn_dev_dev[2];
	ctl_table dn_dev_conf_dir[2];
	ctl_table dn_dev_proto_dir[2];
	ctl_table dn_dev_root_dir[2];
} dn_dev_sysctl = {
	NULL,
	{
	{NET_DECNET_CONF_DEV_FORWARDING, "forwarding",
	(void *)DN_DEV_PARMS_OFFSET(forwarding),
	sizeof(int), 0644, NULL,
	dn_forwarding_proc, dn_forwarding_sysctl,
	NULL, NULL, NULL},
	{NET_DECNET_CONF_DEV_PRIORITY, "priority",
	(void *)DN_DEV_PARMS_OFFSET(priority),
	sizeof(int), 0644, NULL,
	proc_dointvec_minmax, sysctl_intvec,
	NULL, &min_priority, &max_priority},
	{NET_DECNET_CONF_DEV_T2, "t2", (void *)DN_DEV_PARMS_OFFSET(t2),
	sizeof(int), 0644, NULL,
	proc_dointvec_minmax, sysctl_intvec,
	NULL, &min_t2, &max_t2},
	{NET_DECNET_CONF_DEV_T3, "t3", (void *)DN_DEV_PARMS_OFFSET(t3),
	sizeof(int), 0644, NULL,
	proc_dointvec_minmax, sysctl_intvec,
	NULL, &min_t3, &max_t3},
	{0}
	},
	{{0, "", NULL, 0, 0555, dn_dev_sysctl.dn_dev_vars}, {0}},
	{{NET_DECNET_CONF, "conf", NULL, 0, 0555, dn_dev_sysctl.dn_dev_dev}, {0}},
	{{NET_DECNET, "decnet", NULL, 0, 0555, dn_dev_sysctl.dn_dev_conf_dir}, {0}},
	{{CTL_NET, "net", NULL, 0, 0555, dn_dev_sysctl.dn_dev_proto_dir}, {0}}
};

static void dn_dev_sysctl_register(struct net_device *dev, struct dn_dev_parms *parms)
{
	struct dn_dev_sysctl_table *t;
	int i;

	t = kmalloc(sizeof(*t), GFP_KERNEL);
	if (t == NULL)
		return;

	memcpy(t, &dn_dev_sysctl, sizeof(*t));

	for(i = 0; i < (sizeof(t->dn_dev_vars)/sizeof(t->dn_dev_vars[0]) - 1); i++) {
		long offset = (long)t->dn_dev_vars[i].data;
		t->dn_dev_vars[i].data = ((char *)parms) + offset;
		t->dn_dev_vars[i].de = NULL;
	}

	if (dev) {
		t->dn_dev_dev[0].procname = dev->name;
		t->dn_dev_dev[0].ctl_name = dev->ifindex;
	} else {
		t->dn_dev_dev[0].procname = parms->name;
		t->dn_dev_dev[0].ctl_name = parms->ctl_name;
	}

	t->dn_dev_dev[0].child = t->dn_dev_vars;
	t->dn_dev_dev[0].de = NULL;
	t->dn_dev_conf_dir[0].child = t->dn_dev_dev;
	t->dn_dev_conf_dir[0].de = NULL;
	t->dn_dev_proto_dir[0].child = t->dn_dev_conf_dir;
	t->dn_dev_proto_dir[0].de = NULL;
	t->dn_dev_root_dir[0].child = t->dn_dev_proto_dir;
	t->dn_dev_root_dir[0].de = NULL;
	t->dn_dev_vars[0].extra1 = (void *)dev;

	t->sysctl_header = register_sysctl_table(t->dn_dev_root_dir, 0);
	if (t->sysctl_header == NULL)
		kfree(t);
	else
		parms->sysctl = t;
}

static void dn_dev_sysctl_unregister(struct dn_dev_parms *parms)
{
	if (parms->sysctl) {
		struct dn_dev_sysctl_table *t = parms->sysctl;
		parms->sysctl = NULL;
		unregister_sysctl_table(t->sysctl_header);
		kfree(t);
	}
}


static int dn_forwarding_proc(ctl_table *table, int write, 
				struct file *filep,
				void *buffer, size_t *lenp)
{
#ifdef CONFIG_DECNET_ROUTER
	struct net_device *dev = table->extra1;
	struct dn_dev *dn_db;
	int err;
	int tmp, old;

	if (table->extra1 == NULL)
		return -EINVAL;

	dn_db = dev->dn_ptr;
	old = dn_db->parms.forwarding;

	err = proc_dointvec(table, write, filep, buffer, lenp);

	if ((err >= 0) && write) {
		if (dn_db->parms.forwarding < 0)
			dn_db->parms.forwarding = 0;
		if (dn_db->parms.forwarding > 2)
			dn_db->parms.forwarding = 2;
		/*
		 * What an ugly hack this is... its works, just. It
		 * would be nice if sysctl/proc were just that little
		 * bit more flexible so I don't have to write a special
		 * routine, or suffer hacks like this - SJW
		 */
		tmp = dn_db->parms.forwarding;
		dn_db->parms.forwarding = old;
		if (dn_db->parms.down)
			dn_db->parms.down(dev);
		dn_db->parms.forwarding = tmp;
		if (dn_db->parms.up)
			dn_db->parms.up(dev);
	}

	return err;
#else
	return -EINVAL;
#endif
}

static int dn_forwarding_sysctl(ctl_table *table, int *name, int nlen,
			void *oldval, size_t *oldlenp,
			void *newval, size_t newlen,
			void **context)
{
#ifdef CONFIG_DECNET_ROUTER
	struct net_device *dev = table->extra1;
	struct dn_dev *dn_db;
	int value;

	if (table->extra1 == NULL)
		return -EINVAL;

	dn_db = dev->dn_ptr;

	if (newval && newlen) {
		if (newlen != sizeof(int))
			return -EINVAL;

		if (get_user(value, (int *)newval))
			return -EFAULT;
		if (value < 0)
			return -EINVAL;
		if (value > 2)
			return -EINVAL;

		if (dn_db->parms.down)
			dn_db->parms.down(dev);
		dn_db->parms.forwarding = value;
		if (dn_db->parms.up)
			dn_db->parms.up(dev);
	}

	return 0;
#else
	return -EINVAL;
#endif
}

#else /* CONFIG_SYSCTL */
static void dn_dev_sysctl_unregister(struct dn_dev_parms *parms)
{
}
static void dn_dev_sysctl_register(struct net_device *dev, struct dn_dev_parms *parms)
{
}

#endif /* CONFIG_SYSCTL */

static struct dn_ifaddr *dn_dev_alloc_ifa(void)
{
	struct dn_ifaddr *ifa;

	ifa = kmalloc(sizeof(*ifa), GFP_KERNEL);

	if (ifa) {
		memset(ifa, 0, sizeof(*ifa));
	}

	return ifa;
}

static __inline__ void dn_dev_free_ifa(struct dn_ifaddr *ifa)
{
	kfree(ifa);
}

static void dn_dev_del_ifa(struct dn_dev *dn_db, struct dn_ifaddr **ifap, int destroy)
{
	struct dn_ifaddr *ifa1 = *ifap;

	*ifap = ifa1->ifa_next;

	rtmsg_ifa(RTM_DELADDR, ifa1);

	if (destroy) {
		dn_dev_free_ifa(ifa1);

		if (dn_db->ifa_list == NULL)
			dn_dev_delete(dn_db->dev);
	}
}

static int dn_dev_insert_ifa(struct dn_dev *dn_db, struct dn_ifaddr *ifa)
{
	/*
	 * FIXME: Duplicate check here.
	 */

	ifa->ifa_next = dn_db->ifa_list;
	dn_db->ifa_list = ifa;

	rtmsg_ifa(RTM_NEWADDR, ifa);

	return 0;
}

static int dn_dev_set_ifa(struct net_device *dev, struct dn_ifaddr *ifa)
{
	struct dn_dev *dn_db = dev->dn_ptr;

	if (dn_db == NULL) {
		int err;
		dn_db = dn_dev_create(dev, &err);
		if (dn_db == NULL)
			return err;
	}

	ifa->ifa_dev = dn_db;

	if (dev->flags & IFF_LOOPBACK)
		ifa->ifa_scope = RT_SCOPE_HOST;

	return dn_dev_insert_ifa(dn_db, ifa);
}


int dn_dev_ioctl(unsigned int cmd, void *arg)
{
	char buffer[DN_IFREQ_SIZE];
	struct ifreq *ifr = (struct ifreq *)buffer;
	struct sockaddr_dn *sdn = (struct sockaddr_dn *)&ifr->ifr_addr;
	struct dn_dev *dn_db;
	struct net_device *dev;
	struct dn_ifaddr *ifa = NULL, **ifap = NULL;
	int exclusive = 0;
	int ret = 0;

	if (copy_from_user(ifr, arg, DN_IFREQ_SIZE))
		return -EFAULT;
	ifr->ifr_name[IFNAMSIZ-1] = 0;

#ifdef CONFIG_KMOD
	dev_load(ifr->ifr_name);
#endif

	switch(cmd) {
		case SIOCGIFADDR:
			break;
		case SIOCSIFADDR:
			if (!capable(CAP_NET_ADMIN))
				return -EACCES;
			if (sdn->sdn_family != AF_DECnet)
				return -EINVAL;
			rtnl_lock();
			exclusive = 1;
			break;
		default:
			return -EINVAL;
	}

	if ((dev = __dev_get_by_name(ifr->ifr_name)) == NULL) {
		ret = -ENODEV;
		goto done;
	}

	if ((dn_db = dev->dn_ptr) != NULL) {
		for (ifap = &dn_db->ifa_list; (ifa=*ifap) != NULL; ifap = &ifa->ifa_next)
			if (strcmp(ifr->ifr_name, ifa->ifa_label) == 0)
				break;
	}

	if (ifa == NULL && cmd != SIOCSIFADDR) {
		ret = -EADDRNOTAVAIL;
		goto done;
	}

	switch(cmd) {
		case SIOCGIFADDR:
			*((dn_address *)sdn->sdn_nodeaddr) = ifa->ifa_local;
			goto rarok;

		case SIOCSIFADDR:
			if (!ifa) {
				if ((ifa = dn_dev_alloc_ifa()) == NULL) {
					ret = -ENOBUFS;
					break;
				}
				memcpy(ifa->ifa_label, dev->name, IFNAMSIZ);
			} else {
				if (ifa->ifa_local == dn_saddr2dn(sdn))
					break;
				dn_dev_del_ifa(dn_db, ifap, 0);
			}

			ifa->ifa_local = dn_saddr2dn(sdn);

			ret = dn_dev_set_ifa(dev, ifa);
	}
done:
	if (exclusive)
		rtnl_unlock();

	return ret;
rarok:
	if (copy_to_user(arg, ifr, DN_IFREQ_SIZE))
		return -EFAULT;

	return 0;
}

static struct dn_dev *dn_dev_by_index(int ifindex)
{
	struct net_device *dev;
	struct dn_dev *dn_dev = NULL;
	dev = dev_get_by_index(ifindex);
	if (dev) {
		dn_dev = dev->dn_ptr;
		dev_put(dev);
	}

	return dn_dev;
}

static int dn_dev_rtm_deladdr(struct sk_buff *skb, struct nlmsghdr *nlh, void *arg)
{
	struct rtattr **rta = arg;
	struct dn_dev *dn_db;
	struct ifaddrmsg *ifm = NLMSG_DATA(nlh);
	struct dn_ifaddr *ifa, **ifap;

	if ((dn_db = dn_dev_by_index(ifm->ifa_index)) == NULL)
		return -EADDRNOTAVAIL;

	for(ifap = &dn_db->ifa_list; (ifa=*ifap) != NULL; ifap = &ifa->ifa_next) {
		void *tmp = rta[IFA_LOCAL-1];
		if ((tmp && memcmp(RTA_DATA(tmp), &ifa->ifa_local, 2)) ||
				(rta[IFA_LABEL-1] && strcmp(RTA_DATA(rta[IFA_LABEL-1]), ifa->ifa_label)))
			continue;

		dn_dev_del_ifa(dn_db, ifap, 1);
		return 0;
	}

	return -EADDRNOTAVAIL;
}

static int dn_dev_rtm_newaddr(struct sk_buff *skb, struct nlmsghdr *nlh, void *arg)
{
	struct rtattr **rta = arg;
	struct net_device *dev;
	struct dn_dev *dn_db;
	struct ifaddrmsg *ifm = NLMSG_DATA(nlh);
	struct dn_ifaddr *ifa;

	if (rta[IFA_LOCAL-1] == NULL)
		return -EINVAL;

	if ((dev = __dev_get_by_index(ifm->ifa_index)) == NULL)
		return -ENODEV;

	if ((dn_db = dev->dn_ptr) == NULL) {
		int err;
		dn_db = dn_dev_create(dev, &err);
		if (!dn_db)
			return err;
	}
	
	if ((ifa = dn_dev_alloc_ifa()) == NULL)
		return -ENOBUFS;

	memcpy(&ifa->ifa_local, RTA_DATA(rta[IFA_LOCAL-1]), 2);
	ifa->ifa_flags = ifm->ifa_flags;
	ifa->ifa_scope = ifm->ifa_scope;
	ifa->ifa_dev = dn_db;
	if (rta[IFA_LABEL-1])
		memcpy(ifa->ifa_label, RTA_DATA(rta[IFA_LABEL-1]), IFNAMSIZ);
	else
		memcpy(ifa->ifa_label, dev->name, IFNAMSIZ);

	return dn_dev_insert_ifa(dn_db, ifa);
}

static int dn_dev_fill_ifaddr(struct sk_buff *skb, struct dn_ifaddr *ifa,
				u32 pid, u32 seq, int event)
{
	struct ifaddrmsg *ifm;
	struct nlmsghdr *nlh;
	unsigned char *b = skb->tail;

	nlh = NLMSG_PUT(skb, pid, seq, event, sizeof(*ifm));
	ifm = NLMSG_DATA(nlh);

	ifm->ifa_family = AF_DECnet;
	ifm->ifa_prefixlen = 16;
	ifm->ifa_flags = ifa->ifa_flags | IFA_F_PERMANENT;
	ifm->ifa_scope = ifa->ifa_scope;
	ifm->ifa_index = ifa->ifa_dev->dev->ifindex;
	RTA_PUT(skb, IFA_LOCAL, 2, &ifa->ifa_local);
	if (ifa->ifa_label[0])
		RTA_PUT(skb, IFA_LABEL, IFNAMSIZ, &ifa->ifa_label);
	nlh->nlmsg_len = skb->tail - b;
	return skb->len;

nlmsg_failure:
rtattr_failure:
        skb_trim(skb, b - skb->data);
        return -1;
}

static void rtmsg_ifa(int event, struct dn_ifaddr *ifa)
{
	struct sk_buff *skb;
	int size = NLMSG_SPACE(sizeof(struct ifaddrmsg)+128);

	skb = alloc_skb(size, GFP_KERNEL);
	if (!skb) {
		netlink_set_err(rtnl, 0, RTMGRP_DECnet_IFADDR, ENOBUFS);
		return;
	}
	if (dn_dev_fill_ifaddr(skb, ifa, 0, 0, event) < 0) {
		kfree_skb(skb);
		netlink_set_err(rtnl, 0, RTMGRP_DECnet_IFADDR, EINVAL);
		return;
	}
	NETLINK_CB(skb).dst_groups = RTMGRP_DECnet_IFADDR;
	netlink_broadcast(rtnl, skb, 0, RTMGRP_DECnet_IFADDR, GFP_KERNEL);
}

static int dn_dev_dump_ifaddr(struct sk_buff *skb, struct netlink_callback *cb)
{
	int idx, dn_idx;
	int s_idx, s_dn_idx;
	struct net_device *dev;
	struct dn_dev *dn_db;
	struct dn_ifaddr *ifa;

	s_idx = cb->args[0];
	s_dn_idx = dn_idx = cb->args[1];
	read_lock(&dev_base_lock);
	for(dev = dev_base, idx = 0; dev; dev = dev->next) {
		if ((dn_db = dev->dn_ptr) == NULL)
			continue;
		idx++;
		if (idx < s_idx)
			continue;
		if (idx > s_idx)
			s_dn_idx = 0;
		if ((dn_db = dev->dn_ptr) == NULL)
			continue;

		for(ifa = dn_db->ifa_list, dn_idx = 0; ifa; ifa = ifa->ifa_next, dn_idx++) {
			if (dn_idx < s_dn_idx)
				continue;

			if (dn_dev_fill_ifaddr(skb, ifa, NETLINK_CB(cb->skb).pid, cb->nlh->nlmsg_seq, RTM_NEWADDR) <= 0)
				goto done;
		}
	}
done:
	read_unlock(&dev_base_lock);
	cb->args[0] = idx;
	cb->args[1] = dn_idx;

	return skb->len;
}

static void dn_send_endnode_hello(struct net_device *dev)
{
        struct endnode_hello_message *msg;
        struct sk_buff *skb = NULL;
        unsigned short int *pktlen;
	struct dn_dev *dn_db = (struct dn_dev *)dev->dn_ptr;

        if ((skb = dn_alloc_skb(NULL, sizeof(*msg), GFP_ATOMIC)) == NULL)
		return;

        skb->dev = dev;

        msg = (struct endnode_hello_message *)skb_put(skb,sizeof(*msg));

        msg->msgflg  = 0x0D;
        memcpy(msg->tiver, dn_eco_version, 3);
        memcpy(msg->id, decnet_ether_address, 6);
        msg->iinfo   = DN_RT_INFO_ENDN;
        msg->blksize = dn_htons(dn_db->parms.blksize);
        msg->area    = 0x00;
        memset(msg->seed, 0, 8);
        memcpy(msg->neighbor, dn_hiord, ETH_ALEN);

	if (dn_db->router) {
		struct dn_neigh *dn = (struct dn_neigh *)dn_db->router;
		dn_dn2eth(msg->neighbor, dn->addr);
	}

        msg->timer   = dn_htons((unsigned short)dn_db->parms.t3);
        msg->mpd     = 0x00;
        msg->datalen = 0x02;
        memset(msg->data, 0xAA, 2);
        
        pktlen = (unsigned short *)skb_push(skb,2);
        *pktlen = dn_htons(skb->len - 2);

	skb->nh.raw = skb->data;

	dn_rt_finish_output(skb, dn_rt_all_rt_mcast);
}


#ifdef CONFIG_DECNET_ROUTER

#define DRDELAY (5 * HZ)

static int dn_am_i_a_router(struct dn_neigh *dn, struct dn_dev *dn_db)
{
	/* First check time since device went up */
	if ((jiffies - dn_db->uptime) < DRDELAY)
		return 0;

	/* If there is no router, then yes... */
	if (!dn_db->router)
		return 1;

	/* otherwise only if we have a higher priority or.. */
	if (dn->priority < dn_db->parms.priority)
		return 1;

	/* if we have equal priority and a higher node number */
	if (dn->priority != dn_db->parms.priority)
		return 0;

	if (dn_ntohs(dn->addr) < dn_ntohs(decnet_address))
		return 1;

	return 0;
}

static void dn_send_router_hello(struct net_device *dev)
{
	int n;
	struct dn_dev *dn_db = dev->dn_ptr;
	struct dn_neigh *dn = (struct dn_neigh *)dn_db->router;
	struct sk_buff *skb;
	size_t size;
	unsigned char *ptr;
	unsigned char *i1, *i2;
	unsigned short *pktlen;

	if (dn_db->parms.blksize < (26 + 7))
		return;

	n = dn_db->parms.blksize - 26;
	n /= 7;

	if (n > 32)
		n = 32;

	size = 2 + 26 + 7 * n;

	if ((skb = dn_alloc_skb(NULL, size, GFP_ATOMIC)) == NULL)
		return;

	skb->dev = dev;
	ptr = skb_put(skb, size);

	*ptr++ = DN_RT_PKT_CNTL | DN_RT_PKT_ERTH;
	*ptr++ = 2; /* ECO */
	*ptr++ = 0;
	*ptr++ = 0;
	memcpy(ptr, decnet_ether_address, ETH_ALEN);
	ptr += ETH_ALEN;
	*ptr++ = dn_db->parms.forwarding == 1 ? 
			DN_RT_INFO_L1RT : DN_RT_INFO_L2RT;
	*((unsigned short *)ptr) = dn_htons(dn_db->parms.blksize);
	ptr += 2;
	*ptr++ = dn_db->parms.priority; /* Priority */ 
	*ptr++ = 0; /* Area: Reserved */
	*((unsigned short *)ptr) = dn_htons((unsigned short)dn_db->parms.t3);
	ptr += 2;
	*ptr++ = 0; /* MPD: Reserved */
	i1 = ptr++;
	memset(ptr, 0, 7); /* Name: Reserved */
	ptr += 7;
	i2 = ptr++;

	n = dn_neigh_elist(dev, ptr, n);

	*i2 = 7 * n;
	*i1 = 8 + *i2;

	skb_trim(skb, (27 + *i2));

	pktlen = (unsigned short *)skb_push(skb, 2);
	*pktlen = dn_htons(skb->len - 2);

	skb->nh.raw = skb->data;

	if (dn_am_i_a_router(dn, dn_db)) {
		struct sk_buff *skb2 = skb_copy(skb, GFP_ATOMIC);
		if (skb2) {
			dn_rt_finish_output(skb2, dn_rt_all_end_mcast);
		}
	}

	dn_rt_finish_output(skb, dn_rt_all_rt_mcast);
}

static void dn_send_brd_hello(struct net_device *dev)
{
	struct dn_dev *dn_db = (struct dn_dev *)dev->dn_ptr;

	if (dn_db->parms.forwarding == 0)
		dn_send_endnode_hello(dev);
	else
		dn_send_router_hello(dev);
}
#else
static void dn_send_brd_hello(struct net_device *dev)
{
	dn_send_endnode_hello(dev);
}
#endif

#if 0
static void dn_send_ptp_hello(struct net_device *dev)
{
	int tdlen = 16;
	int size = dev->hard_header_len + 2 + 4 + tdlen;
	struct sk_buff *skb = dn_alloc_skb(NULL, size, GFP_ATOMIC);
	struct dn_dev *dn_db = dev->dn_ptr;
	int i;
	unsigned char *ptr;
	struct dn_neigh *dn = (struct dn_neigh *)dn_db->router;

	if (skb == NULL)
		return ;

	skb->dev = dev;
	skb_push(skb, dev->hard_header_len);
	ptr = skb_put(skb, 2 + 4 + tdlen);

	*ptr++ = DN_RT_PKT_HELO;
	*((dn_address *)ptr) = decnet_address;
	ptr += 2;
	*ptr++ = tdlen;

	for(i = 0; i < tdlen; i++)
		*ptr++ = 0252;

	if (dn_am_i_a_router(dn, dn_db)) {
		struct sk_buff *skb2 = skb_copy(skb, GFP_ATOMIC);
		if (skb2) {
			dn_rt_finish_output(skb2, dn_rt_all_end_mcast);
		}
	}

	dn_rt_finish_output(skb, dn_rt_all_rt_mcast);
}
#endif

static int dn_eth_up(struct net_device *dev)
{
	struct dn_dev *dn_db = dev->dn_ptr;

	if (dn_db->parms.forwarding == 0)
		dev_mc_add(dev, dn_rt_all_end_mcast, ETH_ALEN, 0);
	else
		dev_mc_add(dev, dn_rt_all_rt_mcast, ETH_ALEN, 0);

	dev_mc_upload(dev);

	dn_db->use_long = 1;

	return 0;
}

static void dn_dev_set_timer(struct net_device *dev);

static void dn_dev_timer_func(unsigned long arg)
{
	struct net_device *dev = (struct net_device *)arg;
	struct dn_dev *dn_db = dev->dn_ptr;

	if (dn_db->t3 <= dn_db->parms.t2) {
		if (dn_db->parms.timer3)
			dn_db->parms.timer3(dev);
		dn_db->t3 = dn_db->parms.t3;
	} else {
		dn_db->t3 -= dn_db->parms.t2;
	}

	dn_dev_set_timer(dev);
}

static void dn_dev_set_timer(struct net_device *dev)
{
	struct dn_dev *dn_db = dev->dn_ptr;

	if (dn_db->parms.t2 > dn_db->parms.t3)
		dn_db->parms.t2 = dn_db->parms.t3;

	dn_db->timer.data = (unsigned long)dev;
	dn_db->timer.function = dn_dev_timer_func;
	dn_db->timer.expires = jiffies + (dn_db->parms.t2 * HZ);

	add_timer(&dn_db->timer);
}

struct dn_dev *dn_dev_create(struct net_device *dev, int *err)
{
	int i;
	struct dn_dev_parms *p = dn_dev_list;
	struct dn_dev *dn_db;

	for(i = 0; i < DN_DEV_LIST_SIZE; i++, p++) {
		if (p->type == dev->type)
			break;
	}

	*err = -ENODEV;
	if (i == DN_DEV_LIST_SIZE)
		return NULL;

	*err = -ENOBUFS;
	if ((dn_db = kmalloc(sizeof(struct dn_dev), GFP_ATOMIC)) == NULL)
		return NULL;

	memset(dn_db, 0, sizeof(struct dn_dev));
	memcpy(&dn_db->parms, p, sizeof(struct dn_dev_parms));
	dev->dn_ptr = dn_db;
	dn_db->dev = dev;
	init_timer(&dn_db->timer);

	memcpy(dn_db->addr, decnet_ether_address, ETH_ALEN); /* To go... */

	dn_db->uptime = jiffies;
	if (dn_db->parms.up) {
		if (dn_db->parms.up(dev) < 0) {
			dev->dn_ptr = NULL;
			kfree(dn_db);
			return NULL;
		}
	}

	dn_db->neigh_parms = neigh_parms_alloc(dev, &dn_neigh_table);
	/* dn_db->neigh_parms->neigh_setup = dn_db->parms.neigh_setup; */

	dn_dev_sysctl_register(dev, &dn_db->parms);

	dn_dev_set_timer(dev);

	*err = 0;
	return dn_db;
}


/*
 * This processes a device up event. We only start up
 * the loopback device & ethernet devices with correct
 * MAC addreses automatically. Others must be started
 * specifically.
 */
void dn_dev_up(struct net_device *dev)
{
	struct dn_ifaddr *ifa;

	if ((dev->type != ARPHRD_ETHER) && (dev->type != ARPHRD_LOOPBACK))
		return;

	if (dev->type == ARPHRD_ETHER)
		if (memcmp(dev->dev_addr, decnet_ether_address, ETH_ALEN) != 0)
			return;

	if ((ifa = dn_dev_alloc_ifa()) == NULL)
		return;

	ifa->ifa_local = decnet_address;
	ifa->ifa_flags = 0;
	ifa->ifa_scope = RT_SCOPE_UNIVERSE;
	strcpy(ifa->ifa_label, dev->name);

	dn_dev_set_ifa(dev, ifa);
}

static void dn_dev_delete(struct net_device *dev)
{
	struct dn_dev *dn_db = dev->dn_ptr;

	if (dn_db == NULL)
		return;

	del_timer_sync(&dn_db->timer);

	dn_dev_sysctl_unregister(&dn_db->parms);

	neigh_ifdown(&dn_neigh_table, dev);

	if (dev == decnet_default_device)
		decnet_default_device = NULL;

	if (dn_db->parms.down)
		dn_db->parms.down(dev);

	dev->dn_ptr = NULL;

	neigh_parms_release(&dn_neigh_table, dn_db->neigh_parms);

	if (dn_db->router)
		neigh_release(dn_db->router);
	if (dn_db->peer)
		neigh_release(dn_db->peer);

	kfree(dn_db);
}

void dn_dev_down(struct net_device *dev)
{
	struct dn_dev *dn_db = dev->dn_ptr;
	struct dn_ifaddr *ifa;

	if (dn_db == NULL)
		return;

	while((ifa = dn_db->ifa_list) != NULL) {
		dn_dev_del_ifa(dn_db, &dn_db->ifa_list, 0);
		dn_dev_free_ifa(ifa);
	}

	dn_dev_delete(dev);
}

void dn_dev_init_pkt(struct sk_buff *skb)
{
	return;
}

void dn_dev_veri_pkt(struct sk_buff *skb)
{
	return;
}

void dn_dev_hello(struct sk_buff *skb)
{
	return;
}

void dn_dev_devices_off(void)
{
	struct net_device *dev;

	for(dev = dev_base; dev; dev = dev->next)
		dn_dev_down(dev);

}

void dn_dev_devices_on(void)
{
	struct net_device *dev;

	for(dev = dev_base; dev; dev = dev->next) {
		if (dev->flags & IFF_UP)
			dn_dev_up(dev);
	}
}


#ifdef CONFIG_DECNET_SIOCGIFCONF
/*
 * Now we support multiple addresses per interface.
 * Since we don't want to break existing code, you have to enable
 * it as a compile time option. Probably you should use the
 * rtnetlink interface instead.
 */
int dnet_gifconf(struct net_device *dev, char *buf, int len)
{
	struct dn_dev *dn_db = (struct dn_dev *)dev->dn_ptr;
	struct dn_ifaddr *ifa;
	struct ifreq *ifr = (struct ifreq *)buf;
	int done = 0;

	if ((dn_db == NULL) || ((ifa = dn_db->ifa_list) == NULL))
		return 0;

	for(; ifa; ifa = ifa->ifa_next) {
		if (!ifr) {
			done += sizeof(DN_IFREQ_SIZE);
			continue;
		}
		if (len < DN_IFREQ_SIZE)
			return done;
		memset(ifr, 0, DN_IFREQ_SIZE);

		if (ifa->ifa_label)
			strcpy(ifr->ifr_name, ifa->ifa_label);
		else
			strcpy(ifr->ifr_name, dev->name);

		(*(struct sockaddr_dn *) &ifr->ifr_addr).sdn_family = AF_DECnet;
		(*(struct sockaddr_dn *) &ifr->ifr_addr).sdn_add.a_len = 2;
		(*(dn_address *)(*(struct sockaddr_dn *) &ifr->ifr_addr).sdn_add.a_addr) = ifa->ifa_local;

		ifr = (struct ifreq *)((char *)ifr + DN_IFREQ_SIZE);
		len  -= DN_IFREQ_SIZE;
		done += DN_IFREQ_SIZE;
	}

	return done;
}
#endif /* CONFIG_DECNET_SIOCGIFCONF */


#ifdef CONFIG_PROC_FS

static char *dn_type2asc(char type)
{
	switch(type) {
		case DN_DEV_BCAST:
			return "B";
		case DN_DEV_UCAST:
			return "U";
		case DN_DEV_MPOINT:
			return "M";
	}

	return "?";
}

static int decnet_dev_get_info(char *buffer, char **start, off_t offset, int length)
{
        struct dn_dev *dn_db;
	struct net_device *dev;
        int len = 0;
        off_t pos = 0;
        off_t begin = 0;
	char peer_buf[DN_ASCBUF_LEN];
	char router_buf[DN_ASCBUF_LEN];


        len += sprintf(buffer, "Name     Flags T1   Timer1 T3   Timer3 BlkSize Pri State DevType    Router Peer\n");

	read_lock(&dev_base_lock);
        for (dev = dev_base; dev; dev = dev->next) {
		if ((dn_db = (struct dn_dev *)dev->dn_ptr) == NULL)
			continue;

                len += sprintf(buffer + len, "%-8s %1s     %04u %04u   %04lu %04lu   %04hu    %03d %02x    %-10s %-7s %-7s\n",
                             	dev->name ? dev->name : "???",
                             	dn_type2asc(dn_db->parms.mode),
                             	0, 0,
				dn_db->t3, dn_db->parms.t3,
				dn_db->parms.blksize,
				dn_db->parms.priority,
				dn_db->parms.state, dn_db->parms.name,
				dn_db->router ? dn_addr2asc(dn_ntohs(*(dn_address *)dn_db->router->primary_key), router_buf) : "",
				dn_db->peer ? dn_addr2asc(dn_ntohs(*(dn_address *)dn_db->peer->primary_key), peer_buf) : "");


                pos = begin + len;

                if (pos < offset) {
                        len   = 0;
                        begin = pos;
                }
                if (pos > offset + length)
                        break;
        }

	read_unlock(&dev_base_lock);

        *start = buffer + (offset - begin);
        len   -= (offset - begin);

        if (len > length) len = length;

        return(len);
}

#endif /* CONFIG_PROC_FS */

static struct rtnetlink_link dnet_rtnetlink_table[RTM_MAX-RTM_BASE+1] = 
{
	{ NULL,			NULL,			},
	{ NULL,			NULL,			},
	{ NULL,			NULL,			},
	{ NULL,			NULL,			},

	{ dn_dev_rtm_newaddr,	NULL,			},
	{ dn_dev_rtm_deladdr,	NULL,			},
	{ NULL,			dn_dev_dump_ifaddr,	},
	{ NULL,			NULL,			},

#ifdef CONFIG_DECNET_ROUTER
	{ dn_fib_rtm_newroute,	NULL,			},
	{ dn_fib_rtm_delroute,	NULL,			},
	{ dn_cache_getroute,	dn_fib_dump,		},
	{ NULL,			NULL,			},
#else
	{ NULL,			NULL,			},
	{ NULL,			NULL,			},
	{ dn_cache_getroute,	dn_cache_dump,		},
	{ NULL,			NULL,			},
#endif
	{ NULL,			NULL,			},
	{ NULL,			NULL,			},
	{ NULL,			NULL,			},
	{ NULL,			NULL,			},

#ifdef CONFIG_DECNET_ROUTER
	{ dn_fib_rtm_newrule,	NULL,			},
	{ dn_fib_rtm_delrule,	NULL,			},
	{ NULL,			dn_fib_dump_rules,	},
	{ NULL,			NULL,			}
#else
	{ NULL,			NULL,			},
	{ NULL,			NULL,			},
	{ NULL,			NULL,			},
	{ NULL,			NULL,			}
#endif
};

void __init dn_dev_init(void)
{

	dn_dev_devices_on();
#ifdef CONFIG_DECNET_SIOCGIFCONF
	register_gifconf(PF_DECnet, dnet_gifconf);
#endif /* CONFIG_DECNET_SIOCGIFCONF */

	rtnetlink_links[PF_DECnet] = dnet_rtnetlink_table;

#ifdef CONFIG_PROC_FS
	proc_net_create("decnet_dev", 0, decnet_dev_get_info);
#endif /* CONFIG_PROC_FS */

#ifdef CONFIG_SYSCTL
	{
		int i;
		for(i = 0; i < DN_DEV_LIST_SIZE; i++)
			dn_dev_sysctl_register(NULL, &dn_dev_list[i]);
	}
#endif /* CONFIG_SYSCTL */
}

void __exit dn_dev_cleanup(void)
{
	rtnetlink_links[PF_DECnet] = NULL;

#ifdef CONFIG_DECNET_SIOCGIFCONF
	unregister_gifconf(PF_DECnet);
#endif /* CONFIG_DECNET_SIOCGIFCONF */

#ifdef CONFIG_SYSCTL
	{
		int i;
		for(i = 0; i < DN_DEV_LIST_SIZE; i++)
			dn_dev_sysctl_unregister(&dn_dev_list[i]);
	}
#endif /* CONFIG_SYSCTL */

	proc_net_remove("decnet_dev");

	dn_dev_devices_off();
}
