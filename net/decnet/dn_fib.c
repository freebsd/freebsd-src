/*
 * DECnet       An implementation of the DECnet protocol suite for the LINUX
 *              operating system.  DECnet is implemented using the  BSD Socket
 *              interface as the means of communication with the user level.
 *
 *              DECnet Routing Forwarding Information Base (Glue/Info List)
 *
 * Author:      Steve Whitehouse <SteveW@ACM.org>
 *
 *
 * Changes:
 *              Alexey Kuznetsov : SMP locking changes
 *              Steve Whitehouse : Rewrote it... Well to be more correct, I
 *                                 copied most of it from the ipv4 fib code.
 *
 */
#include <linux/config.h>
#include <linux/string.h>
#include <linux/net.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/proc_fs.h>
#include <linux/netdevice.h>
#include <linux/timer.h>
#include <linux/spinlock.h>
#include <asm/atomic.h>
#include <asm/uaccess.h>
#include <net/neighbour.h>
#include <net/dst.h>
#include <net/dn.h>
#include <net/dn_route.h>
#include <net/dn_fib.h>
#include <net/dn_neigh.h>
#include <net/dn_dev.h>


#define for_fib_info() { struct dn_fib_info *fi;\
	for(fi = dn_fib_info_list; fi; fi = fi->fib_next)
#define endfor_fib_info() }

#define for_nexthops(fi) { int nhsel; const struct dn_fib_nh *nh;\
	for(nhsel = 0, nh = (fi)->fib_nh; nhsel < (fi)->fib_nhs; nh++, nhsel++)

#define change_nexthops(fi) { int nhsel; struct dn_fib_nh *nh;\
	for(nhsel = 0, nh = (struct dn_fib_nh *)((fi)->fib_nh); nhsel < (fi)->fib_nhs; nh++, nhsel++)

#define endfor_nexthops(fi) }

extern int dn_cache_dump(struct sk_buff *skb, struct netlink_callback *cb);


static struct dn_fib_info *dn_fib_info_list;
static rwlock_t dn_fib_info_lock = RW_LOCK_UNLOCKED;
int dn_fib_info_cnt;

static struct
{
	int error;
	u8 scope;
} dn_fib_props[RTA_MAX+1] = {
	{ 0, RT_SCOPE_NOWHERE },		/* RTN_UNSPEC */
	{ 0, RT_SCOPE_UNIVERSE },		/* RTN_UNICAST */
	{ 0, RT_SCOPE_HOST },			/* RTN_LOCAL */
	{ -EINVAL, RT_SCOPE_NOWHERE },		/* RTN_BROADCAST */
	{ -EINVAL, RT_SCOPE_NOWHERE },		/* RTN_ANYCAST */
	{ -EINVAL, RT_SCOPE_NOWHERE },		/* RTN_MULTICAST */
	{ -EINVAL, RT_SCOPE_UNIVERSE },		/* RTN_BLACKHOLE */
	{ -EHOSTUNREACH, RT_SCOPE_UNIVERSE },	/* RTN_UNREACHABLE */
	{ -EACCES, RT_SCOPE_UNIVERSE },		/* RTN_PROHIBIT */
	{ -EAGAIN, RT_SCOPE_UNIVERSE },		/* RTN_THROW */
	{ -EINVAL, RT_SCOPE_NOWHERE },		/* RTN_NAT */
	{ -EINVAL, RT_SCOPE_NOWHERE }		/* RTN_XRESOLVE */
};

void dn_fib_free_info(struct dn_fib_info *fi)
{
	if (fi->fib_dead == 0) {
		printk(KERN_DEBUG "DECnet: BUG! Attempt to free alive dn_fib_info\n");
		return;
	}

	change_nexthops(fi) {
		if (nh->nh_dev)
			dev_put(nh->nh_dev);
		nh->nh_dev = NULL;
	} endfor_nexthops(fi);
	dn_fib_info_cnt--;
	kfree(fi);
}

void dn_fib_release_info(struct dn_fib_info *fi)
{
	write_lock(&dn_fib_info_lock);
	if (fi && --fi->fib_treeref == 0) {
		if (fi->fib_next)
			fi->fib_next->fib_prev = fi->fib_prev;
		if (fi->fib_prev)
			fi->fib_prev->fib_next = fi->fib_next;
		if (fi == dn_fib_info_list)
			dn_fib_info_list = fi->fib_next;
		fi->fib_dead = 1;
		dn_fib_info_put(fi);
	}
	write_unlock(&dn_fib_info_lock);
}

static __inline__ int dn_fib_nh_comp(const struct dn_fib_info *fi, const struct dn_fib_info *ofi)
{
	const struct dn_fib_nh *onh = ofi->fib_nh;

	for_nexthops(fi) {
		if (nh->nh_oif != onh->nh_oif ||
			nh->nh_gw != onh->nh_gw ||
			nh->nh_scope != onh->nh_scope ||
			nh->nh_weight != onh->nh_weight ||
			((nh->nh_flags^onh->nh_flags)&~RTNH_F_DEAD))
				return -1;
		onh++;
	} endfor_nexthops(fi);
	return 0;
}

static __inline__ struct dn_fib_info *dn_fib_find_info(const struct dn_fib_info *nfi)
{
	for_fib_info() {
		if (fi->fib_nhs != nfi->fib_nhs)
			continue;
		if (nfi->fib_protocol == fi->fib_protocol &&
			nfi->fib_prefsrc == fi->fib_prefsrc &&
			nfi->fib_priority == fi->fib_priority &&
			((nfi->fib_flags^fi->fib_flags)&~RTNH_F_DEAD) == 0 &&
			(nfi->fib_nhs == 0 || dn_fib_nh_comp(fi, nfi) == 0))
				return fi;
	} endfor_fib_info();
	return NULL;
}

u16 dn_fib_get_attr16(struct rtattr *attr, int attrlen, int type)
{
	while(RTA_OK(attr,attrlen)) {
		if (attr->rta_type == type)
			return *(u16*)RTA_DATA(attr);
		attr = RTA_NEXT(attr, attrlen);
	}

	return 0;
}

static int dn_fib_count_nhs(struct rtattr *rta)
{
	int nhs = 0;
	struct rtnexthop *nhp = RTA_DATA(rta);
	int nhlen = RTA_PAYLOAD(rta);

	while(nhlen >= (int)sizeof(struct rtnexthop)) {
		if ((nhlen -= nhp->rtnh_len) < 0)
			return 0;
		nhs++;
		nhp = RTNH_NEXT(nhp);
	}

	return nhs;
}

static int dn_fib_get_nhs(struct dn_fib_info *fi, const struct rtattr *rta, const struct rtmsg *r)
{
	struct rtnexthop *nhp = RTA_DATA(rta);
	int nhlen = RTA_PAYLOAD(rta);

	change_nexthops(fi) {
		int attrlen = nhlen - sizeof(struct rtnexthop);
		if (attrlen < 0 || (nhlen -= nhp->rtnh_len) < 0)
			return -EINVAL;

		nh->nh_flags  = (r->rtm_flags&~0xFF) | nhp->rtnh_flags;
		nh->nh_oif    = nhp->rtnh_ifindex;
		nh->nh_weight = nhp->rtnh_hops + 1;

		if (attrlen) {
			nh->nh_gw = dn_fib_get_attr16(RTNH_DATA(nhp), attrlen, RTA_GATEWAY);
		}
		nhp = RTNH_NEXT(nhp);
	} endfor_nexthops(fi);

	return 0;
}


static int dn_fib_check_nh(const struct rtmsg *r, struct dn_fib_info *fi, struct dn_fib_nh *nh)
{
	int err;

	if (nh->nh_gw) {
		struct dn_fib_key key;
		struct dn_fib_res res;

		if (nh->nh_flags&RTNH_F_ONLINK) {
			struct net_device *dev;

			if (r->rtm_scope >= RT_SCOPE_LINK)
				return -EINVAL;
			if ((dev = __dev_get_by_index(nh->nh_oif)) == NULL)
				return -ENODEV;
			if (!(dev->flags&IFF_UP))
				return -ENETDOWN;
			nh->nh_dev = dev;
			atomic_inc(&dev->refcnt);
			nh->nh_scope = RT_SCOPE_LINK;
			return 0;
		}

		memset(&key, 0, sizeof(key));
		key.dst = nh->nh_gw;
		key.oif = nh->nh_oif;
		key.scope = r->rtm_scope + 1;

		if (key.scope < RT_SCOPE_LINK)
			key.scope = RT_SCOPE_LINK;

		if ((err = dn_fib_lookup(&key, &res)) != 0)
			return err;

		nh->nh_scope = res.scope;
		nh->nh_oif = DN_FIB_RES_OIF(res);
		nh->nh_dev = DN_FIB_RES_DEV(res);
		if (nh->nh_dev)
			atomic_inc(&nh->nh_dev->refcnt);
		dn_fib_res_put(&res);
	} else {
		struct net_device *dev;

		if (nh->nh_flags&(RTNH_F_PERVASIVE|RTNH_F_ONLINK))
			return -EINVAL;

		dev = __dev_get_by_index(nh->nh_oif);
		if (dev == NULL || dev->dn_ptr == NULL)
			return -ENODEV;
		if (!(dev->flags&IFF_UP))
			return -ENETDOWN;
		nh->nh_dev = dev;
		atomic_inc(&nh->nh_dev->refcnt);
		nh->nh_scope = RT_SCOPE_HOST;
	}

	return 0;
}


struct dn_fib_info *dn_fib_create_info(const struct rtmsg *r, struct dn_kern_rta *rta, const struct nlmsghdr *nlh, int *errp)
{
	int err;
	struct dn_fib_info *fi = NULL;
	struct dn_fib_info *ofi;
	int nhs = 1;

	if (dn_fib_props[r->rtm_type].scope > r->rtm_scope)
		goto err_inval;

	if (rta->rta_mp) {
		nhs = dn_fib_count_nhs(rta->rta_mp);
		if (nhs == 0)
			goto err_inval;
	}

	fi = kmalloc(sizeof(*fi)+nhs*sizeof(struct dn_fib_nh), GFP_KERNEL);
	err = -ENOBUFS;
	if (fi == NULL)
		goto failure;
	memset(fi, 0, sizeof(*fi)+nhs*sizeof(struct dn_fib_nh));

	fi->fib_protocol = r->rtm_protocol;
	fi->fib_nhs = nhs;
	fi->fib_flags = r->rtm_flags;
	if (rta->rta_priority)
		fi->fib_priority = *rta->rta_priority;
	if (rta->rta_prefsrc)
		memcpy(&fi->fib_prefsrc, rta->rta_prefsrc, 2);

	if (rta->rta_mp) {
		if ((err = dn_fib_get_nhs(fi, rta->rta_mp, r)) != 0)
			goto failure;
		if (rta->rta_oif && fi->fib_nh->nh_oif != *rta->rta_oif)
			goto err_inval;
		if (rta->rta_gw && memcmp(&fi->fib_nh->nh_gw, rta->rta_gw, 2))
			goto err_inval;
	} else {
		struct dn_fib_nh *nh = fi->fib_nh;
		if (rta->rta_oif)
			nh->nh_oif = *rta->rta_oif;
		if (rta->rta_gw)
			memcpy(&nh->nh_gw, rta->rta_gw, 2);
		nh->nh_flags = r->rtm_flags;
		nh->nh_weight = 1;
	}

	if (dn_fib_props[r->rtm_type].error) {
		if (rta->rta_gw || rta->rta_oif || rta->rta_mp)
			goto err_inval;
		goto link_it;
	}

	if (r->rtm_scope > RT_SCOPE_HOST)
		goto err_inval;

	if (r->rtm_scope == RT_SCOPE_HOST) {
		struct dn_fib_nh *nh = fi->fib_nh;

		/* Local address is added */
		if (nhs != 1 || nh->nh_gw)
			goto err_inval;
		nh->nh_scope = RT_SCOPE_NOWHERE;
		nh->nh_dev = dev_get_by_index(fi->fib_nh->nh_oif);
		err = -ENODEV;
		if (nh->nh_dev == NULL)
			goto failure;
	} else {
		change_nexthops(fi) {
			if ((err = dn_fib_check_nh(r, fi, nh)) != 0)
				goto failure;
		} endfor_nexthops(fi)
	}

#if I_GET_AROUND_TO_FIXING_PREFSRC
	if (fi->fib_prefsrc) {
		if (r->rtm_type != RTN_LOCAL || rta->rta_dst == NULL ||
		    memcmp(&fi->fib_prefsrc, rta->rta_dst, 2))
			if (dn_addr_type(fi->fib_prefsrc) != RTN_LOCAL)
				goto err_inval;
	}
#endif

link_it:
	if ((ofi = dn_fib_find_info(fi)) != NULL) {
		fi->fib_dead = 1;
		dn_fib_free_info(fi);
		ofi->fib_treeref++;
		return ofi;
	}

	fi->fib_treeref++;
	atomic_inc(&fi->fib_clntref);
	write_lock(&dn_fib_info_lock);
	fi->fib_next = dn_fib_info_list;
	fi->fib_prev = NULL;
	if (dn_fib_info_list)
		dn_fib_info_list->fib_prev = fi;
	dn_fib_info_list = fi;
	dn_fib_info_cnt++;
	write_unlock(&dn_fib_info_lock);
	return fi;

err_inval:
	err = -EINVAL;

failure:
	*errp = err;
	if (fi) {
		fi->fib_dead = 1;
		dn_fib_free_info(fi);
	}

	return NULL;
}


void dn_fib_select_multipath(const struct dn_fib_key *key, struct dn_fib_res *res)
{
	struct dn_fib_info *fi = res->fi;
	int w;

	if (fi->fib_power <= 0) {
		int power = 0;
		change_nexthops(fi) {
			if (!(nh->nh_flags&RTNH_F_DEAD)) {
				power += nh->nh_weight;
				nh->nh_power = nh->nh_weight;
			}
		} endfor_nexthops(fi);
		fi->fib_power = power;
	}

	w = jiffies % fi->fib_power;

	change_nexthops(fi) {
		if (!(nh->nh_flags&RTNH_F_DEAD) && nh->nh_power) {
			if ((w -= nh->nh_power) <= 0) {
				nh->nh_power--;
				fi->fib_power--;
				res->nh_sel = nhsel;
				return;
			}
		}
	} endfor_nexthops(fi);

	printk(KERN_DEBUG "DECnet: BUG! dn_fib_select_multipath\n");
}



/*
 * Punt to user via netlink for example, but for now
 * we just drop it.
 */
int dn_fib_rt_message(struct sk_buff *skb)
{
	kfree_skb(skb);

	return 0;
}


static int dn_fib_check_attr(struct rtmsg *r, struct rtattr **rta)
{
	int i;

	for(i = 1; i <= RTA_MAX; i++) {
		struct rtattr *attr = rta[i-1];
		if (attr) {
			if (RTA_PAYLOAD(attr) < 4 && RTA_PAYLOAD(attr) != 2)
				return -EINVAL;
			if (i != RTA_MULTIPATH && i != RTA_METRICS)
				rta[i-1] = (struct rtattr *)RTA_DATA(attr);
		}
	}

	return 0;
}

int dn_fib_rtm_delroute(struct sk_buff *skb, struct nlmsghdr *nlh, void *arg)
{
	struct dn_fib_table *tb;
	struct rtattr **rta = arg;
	struct rtmsg *r = NLMSG_DATA(nlh);

	if (dn_fib_check_attr(r, rta))
		return -EINVAL;

	tb = dn_fib_get_table(r->rtm_table, 0);
	if (tb)
		return tb->delete(tb, r, (struct dn_kern_rta *)rta, nlh, &NETLINK_CB(skb));

	return -ESRCH;
}

int dn_fib_rtm_newroute(struct sk_buff *skb, struct nlmsghdr *nlh, void *arg)
{
	struct dn_fib_table *tb;
	struct rtattr **rta = arg;
	struct rtmsg *r = NLMSG_DATA(nlh);

	if (dn_fib_check_attr(r, rta))
		return -EINVAL;

	tb = dn_fib_get_table(r->rtm_table, 1);
	if (tb) 
		return tb->insert(tb, r, (struct dn_kern_rta *)rta, nlh, &NETLINK_CB(skb));

	return -ENOBUFS;
}


int dn_fib_dump(struct sk_buff *skb, struct netlink_callback *cb)
{
	int t;
	int s_t;
	struct dn_fib_table *tb;

	if (NLMSG_PAYLOAD(cb->nlh, 0) >= sizeof(struct rtmsg) &&
		((struct rtmsg *)NLMSG_DATA(cb->nlh))->rtm_flags&RTM_F_CLONED)
			return dn_cache_dump(skb, cb);

	s_t = cb->args[0];
	if (s_t == 0)
		s_t = cb->args[0] = DN_MIN_TABLE;

	for(t = s_t; t < DN_NUM_TABLES; t++) {
		if (t < s_t)
			continue;
		if (t > s_t)
			memset(&cb->args[1], 0, sizeof(cb->args)-sizeof(int));
		tb = dn_fib_get_table(t, 0);
		if (tb == NULL)
			continue;
		if (tb->dump(tb, skb, cb) < 0)
			break;
	}

	cb->args[0] = t;

	return skb->len;
}

int dn_fib_sync_down(dn_address local, struct net_device *dev, int force)
{
        int ret = 0;
        int scope = RT_SCOPE_NOWHERE;

        if (force)
                scope = -1;

        for_fib_info() {
                /* 
                 * This makes no sense for DECnet.... we will almost
                 * certainly have more than one local address the same
                 * over all our interfaces. It needs thinking about
                 * some more.
                 */
                if (local && fi->fib_prefsrc == local) {
                        fi->fib_flags |= RTNH_F_DEAD;
                        ret++;
                } else if (dev && fi->fib_nhs) {
                        int dead = 0;

                        change_nexthops(fi) {
                                if (nh->nh_flags&RTNH_F_DEAD)
                                        dead++;
                                else if (nh->nh_dev == dev &&
                                                nh->nh_scope != scope) {
                                        nh->nh_flags |= RTNH_F_DEAD;
                                        fi->fib_power -= nh->nh_power;
                                        nh->nh_power = 0;
                                        dead++;
                                }
                        } endfor_nexthops(fi)
                        if (dead == fi->fib_nhs) {
                                fi->fib_flags |= RTNH_F_DEAD;
                                ret++;
                        }
                }
        } endfor_fib_info();
        return ret;
}


int dn_fib_sync_up(struct net_device *dev)
{
        int ret = 0;

        if (!(dev->flags&IFF_UP))
                return 0;

        for_fib_info() {
                int alive = 0;

                change_nexthops(fi) {
                        if (!(nh->nh_flags&RTNH_F_DEAD)) {
                                alive++;
                                continue;
                        }
                        if (nh->nh_dev == NULL || !(nh->nh_dev->flags&IFF_UP))
                                continue;
                        if (nh->nh_dev != dev || dev->dn_ptr == NULL)
                                continue;
                        alive++;
                        nh->nh_power = 0;
                        nh->nh_flags &= ~RTNH_F_DEAD;
                } endfor_nexthops(fi);

                if (alive == fi->fib_nhs) {
                        fi->fib_flags &= ~RTNH_F_DEAD;
                        ret++;
                }
        } endfor_fib_info();
        return ret;
}

void dn_fib_flush(void)
{
        int flushed = 0;
        struct dn_fib_table *tb;
        int id;

        for(id = DN_NUM_TABLES; id > 0; id--) {
                if ((tb = dn_fib_get_table(id, 0)) == NULL)
                        continue;
                flushed += tb->flush(tb);
        }

        if (flushed)
                dn_rt_cache_flush(-1);
}

int dn_fib_ioctl(struct socket *sock, unsigned int cmd, unsigned long arg)
{

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	switch(cmd) {
		case SIOCADDRT:
		case SIOCDELRT:
			return 0;
	}

	return -EINVAL;
}

#ifdef CONFIG_PROC_FS

static int decnet_rt_get_info(char *buffer, char **start, off_t offset, int length)
{
        int first = offset / 128;
        char *ptr = buffer;
        int count = (length + 127) / 128;
        int len;
        int i;
        struct dn_fib_table *tb;

        *start = buffer + (offset % 128);

        if (--first < 0) {
                sprintf(buffer, "%-127s\n", "Iface\tDest\tGW  \tFlags\tRefCnt\tUse\tMetric\tMask\t\tMTU\tWindow\tIRTT");
                --count;
                ptr += 128;
                first = 0;
        }


        for(i = DN_MIN_TABLE; (i <= DN_NUM_TABLES) && (count > 0); i++) {
                if ((tb = dn_fib_get_table(i, 0)) != NULL) {
                        int n = tb->get_info(tb, ptr, first, count);
                        count -= n;
                        ptr += n * 128;
                }
        }

        len = ptr - *start;
        if (len >= length)
                return length;
        if (len >= 0)
                return len;

        return 0;
}
#endif /* CONFIG_PROC_FS */

void __exit dn_fib_cleanup(void)
{
	proc_net_remove("decnet_route");

	dn_fib_table_cleanup();
	dn_fib_rules_cleanup();
}


void __init dn_fib_init(void)
{

#ifdef CONFIG_PROC_FS
	proc_net_create("decnet_route", 0, decnet_rt_get_info);
#endif

	dn_fib_table_init();
	dn_fib_rules_init();
}


