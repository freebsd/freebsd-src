/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET  is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Definitions for the Forwarding Information Base.
 *
 * Authors:	A.N.Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */

#ifndef _NET_IP_FIB_H
#define _NET_IP_FIB_H

#include <linux/config.h>

struct kern_rta
{
	void		*rta_dst;
	void		*rta_src;
	int		*rta_iif;
	int		*rta_oif;
	void		*rta_gw;
	u32		*rta_priority;
	void		*rta_prefsrc;
	struct rtattr	*rta_mx;
	struct rtattr	*rta_mp;
	unsigned char	*rta_protoinfo;
	unsigned char	*rta_flow;
	struct rta_cacheinfo *rta_ci;
};

struct fib_nh
{
	struct net_device		*nh_dev;
	unsigned		nh_flags;
	unsigned char		nh_scope;
#ifdef CONFIG_IP_ROUTE_MULTIPATH
	int			nh_weight;
	int			nh_power;
#endif
#ifdef CONFIG_NET_CLS_ROUTE
	__u32			nh_tclassid;
#endif
	int			nh_oif;
	u32			nh_gw;
};

/*
 * This structure contains data shared by many of routes.
 */

struct fib_info
{
	struct fib_info		*fib_next;
	struct fib_info		*fib_prev;
	int			fib_treeref;
	atomic_t		fib_clntref;
	int			fib_dead;
	unsigned		fib_flags;
	int			fib_protocol;
	u32			fib_prefsrc;
	u32			fib_priority;
	unsigned		fib_metrics[RTAX_MAX];
#define fib_mtu fib_metrics[RTAX_MTU-1]
#define fib_window fib_metrics[RTAX_WINDOW-1]
#define fib_rtt fib_metrics[RTAX_RTT-1]
#define fib_advmss fib_metrics[RTAX_ADVMSS-1]
	int			fib_nhs;
#ifdef CONFIG_IP_ROUTE_MULTIPATH
	int			fib_power;
#endif
	struct fib_nh		fib_nh[0];
#define fib_dev		fib_nh[0].nh_dev
};


#ifdef CONFIG_IP_MULTIPLE_TABLES
struct fib_rule;
#endif

struct fib_result
{
	unsigned char	prefixlen;
	unsigned char	nh_sel;
	unsigned char	type;
	unsigned char	scope;
	struct fib_info *fi;
#ifdef CONFIG_IP_MULTIPLE_TABLES
	struct fib_rule	*r;
#endif
};


#ifdef CONFIG_IP_ROUTE_MULTIPATH

#define FIB_RES_NH(res)		((res).fi->fib_nh[(res).nh_sel])
#define FIB_RES_RESET(res)	((res).nh_sel = 0)

#else /* CONFIG_IP_ROUTE_MULTIPATH */

#define FIB_RES_NH(res)		((res).fi->fib_nh[0])
#define FIB_RES_RESET(res)

#endif /* CONFIG_IP_ROUTE_MULTIPATH */

#define FIB_RES_PREFSRC(res)		((res).fi->fib_prefsrc ? : __fib_res_prefsrc(&res))
#define FIB_RES_GW(res)			(FIB_RES_NH(res).nh_gw)
#define FIB_RES_DEV(res)		(FIB_RES_NH(res).nh_dev)
#define FIB_RES_OIF(res)		(FIB_RES_NH(res).nh_oif)

struct fib_table
{
	unsigned char	tb_id;
	unsigned	tb_stamp;
	int		(*tb_lookup)(struct fib_table *tb, const struct rt_key *key, struct fib_result *res);
	int		(*tb_insert)(struct fib_table *table, struct rtmsg *r,
				     struct kern_rta *rta, struct nlmsghdr *n,
				     struct netlink_skb_parms *req);
	int		(*tb_delete)(struct fib_table *table, struct rtmsg *r,
				     struct kern_rta *rta, struct nlmsghdr *n,
				     struct netlink_skb_parms *req);
	int		(*tb_dump)(struct fib_table *table, struct sk_buff *skb,
				     struct netlink_callback *cb);
	int		(*tb_flush)(struct fib_table *table);
	int		(*tb_get_info)(struct fib_table *table, char *buf,
				       int first, int count);
	void		(*tb_select_default)(struct fib_table *table,
					     const struct rt_key *key, struct fib_result *res);

	unsigned char	tb_data[0];
};

#ifndef CONFIG_IP_MULTIPLE_TABLES

extern struct fib_table *local_table;
extern struct fib_table *main_table;

static inline struct fib_table *fib_get_table(int id)
{
	if (id != RT_TABLE_LOCAL)
		return main_table;
	return local_table;
}

static inline struct fib_table *fib_new_table(int id)
{
	return fib_get_table(id);
}

static inline int fib_lookup(const struct rt_key *key, struct fib_result *res)
{
	if (local_table->tb_lookup(local_table, key, res) &&
	    main_table->tb_lookup(main_table, key, res))
		return -ENETUNREACH;
	return 0;
}

static inline void fib_select_default(const struct rt_key *key, struct fib_result *res)
{
	if (FIB_RES_GW(*res) && FIB_RES_NH(*res).nh_scope == RT_SCOPE_LINK)
		main_table->tb_select_default(main_table, key, res);
}

#else /* CONFIG_IP_MULTIPLE_TABLES */
#define local_table (fib_tables[RT_TABLE_LOCAL])
#define main_table (fib_tables[RT_TABLE_MAIN])

extern struct fib_table * fib_tables[RT_TABLE_MAX+1];
extern int fib_lookup(const struct rt_key *key, struct fib_result *res);
extern struct fib_table *__fib_new_table(int id);
extern void fib_rule_put(struct fib_rule *r);

static inline struct fib_table *fib_get_table(int id)
{
	if (id == 0)
		id = RT_TABLE_MAIN;

	return fib_tables[id];
}

static inline struct fib_table *fib_new_table(int id)
{
	if (id == 0)
		id = RT_TABLE_MAIN;

	return fib_tables[id] ? : __fib_new_table(id);
}

extern void fib_select_default(const struct rt_key *key, struct fib_result *res);

#endif /* CONFIG_IP_MULTIPLE_TABLES */

/* Exported by fib_frontend.c */
extern void		ip_fib_init(void);
extern void		fib_flush(void);
extern int inet_rtm_delroute(struct sk_buff *skb, struct nlmsghdr* nlh, void *arg);
extern int inet_rtm_newroute(struct sk_buff *skb, struct nlmsghdr* nlh, void *arg);
extern int inet_rtm_getroute(struct sk_buff *skb, struct nlmsghdr* nlh, void *arg);
extern int inet_dump_fib(struct sk_buff *skb, struct netlink_callback *cb);
extern int fib_validate_source(u32 src, u32 dst, u8 tos, int oif,
			       struct net_device *dev, u32 *spec_dst, u32 *itag);
extern void fib_select_multipath(const struct rt_key *key, struct fib_result *res);

/* Exported by fib_semantics.c */
extern int 		ip_fib_check_default(u32 gw, struct net_device *dev);
extern void		fib_release_info(struct fib_info *);
extern int		fib_semantic_match(int type, struct fib_info *,
					   const struct rt_key *, struct fib_result*);
extern struct fib_info	*fib_create_info(const struct rtmsg *r, struct kern_rta *rta,
					 const struct nlmsghdr *, int *err);
extern int fib_nh_match(struct rtmsg *r, struct nlmsghdr *, struct kern_rta *rta, struct fib_info *fi);
extern int fib_dump_info(struct sk_buff *skb, u32 pid, u32 seq, int event,
			 u8 tb_id, u8 type, u8 scope, void *dst, int dst_len, u8 tos,
			 struct fib_info *fi);
extern int fib_sync_down(u32 local, struct net_device *dev, int force);
extern int fib_sync_up(struct net_device *dev);
extern int fib_convert_rtentry(int cmd, struct nlmsghdr *nl, struct rtmsg *rtm,
			       struct kern_rta *rta, struct rtentry *r);
extern void fib_node_get_info(int type, int dead, struct fib_info *fi, u32 prefix, u32 mask, char *buffer);
extern u32  __fib_res_prefsrc(struct fib_result *res);

/* Exported by fib_hash.c */
extern struct fib_table *fib_hash_init(int id);

#ifdef CONFIG_IP_MULTIPLE_TABLES
/* Exported by fib_rules.c */

extern int inet_rtm_delrule(struct sk_buff *skb, struct nlmsghdr* nlh, void *arg);
extern int inet_rtm_newrule(struct sk_buff *skb, struct nlmsghdr* nlh, void *arg);
extern int inet_dump_rules(struct sk_buff *skb, struct netlink_callback *cb);
extern u32 fib_rules_map_destination(u32 daddr, struct fib_result *res);
#ifdef CONFIG_NET_CLS_ROUTE
extern u32 fib_rules_tclass(struct fib_result *res);
#endif
extern u32 fib_rules_policy(u32 saddr, struct fib_result *res, unsigned *flags);
extern void fib_rules_init(void);
#endif

static inline void fib_combine_itag(u32 *itag, struct fib_result *res)
{
#ifdef CONFIG_NET_CLS_ROUTE
#ifdef CONFIG_IP_MULTIPLE_TABLES
	u32 rtag;
#endif
	*itag = FIB_RES_NH(*res).nh_tclassid<<16;
#ifdef CONFIG_IP_MULTIPLE_TABLES
	rtag = fib_rules_tclass(res);
	if (*itag == 0)
		*itag = (rtag<<16);
	*itag |= (rtag>>16);
#endif
#endif
}

extern void free_fib_info(struct fib_info *fi);

static inline void fib_info_put(struct fib_info *fi)
{
	if (atomic_dec_and_test(&fi->fib_clntref))
		free_fib_info(fi);
}

static inline void fib_res_put(struct fib_result *res)
{
	if (res->fi)
		fib_info_put(res->fi);
#ifdef CONFIG_IP_MULTIPLE_TABLES
	if (res->r)
		fib_rule_put(res->r);
#endif
}


#endif  /* _NET_FIB_H */
