#ifndef __NET_PKT_CLS_H
#define __NET_PKT_CLS_H


#include <linux/pkt_cls.h>

struct rtattr;
struct tcmsg;

/* Basic packet classifier frontend definitions. */

struct tcf_result
{
	unsigned long	class;
	u32		classid;
};

struct tcf_proto
{
	/* Fast access part */
	struct tcf_proto	*next;
	void			*root;
	int			(*classify)(struct sk_buff*, struct tcf_proto*, struct tcf_result *);
	u32			protocol;

	/* All the rest */
	u32			prio;
	u32			classid;
	struct Qdisc		*q;
	void			*data;
	struct tcf_proto_ops	*ops;
};

struct tcf_walker
{
	int	stop;
	int	skip;
	int	count;
	int	(*fn)(struct tcf_proto *, unsigned long node, struct tcf_walker *);
};

struct tcf_proto_ops
{
	struct tcf_proto_ops	*next;
	char			kind[IFNAMSIZ];

	int			(*classify)(struct sk_buff*, struct tcf_proto*, struct tcf_result *);
	int			(*init)(struct tcf_proto*);
	void			(*destroy)(struct tcf_proto*);

	unsigned long		(*get)(struct tcf_proto*, u32 handle);
	void			(*put)(struct tcf_proto*, unsigned long);
	int			(*change)(struct tcf_proto*, unsigned long, u32 handle, struct rtattr **, unsigned long *);
	int			(*delete)(struct tcf_proto*, unsigned long);
	void			(*walk)(struct tcf_proto*, struct tcf_walker *arg);

	/* rtnetlink specific */
	int			(*dump)(struct tcf_proto*, unsigned long, struct sk_buff *skb, struct tcmsg*);
};

/* Main classifier routine: scans classifier chain attached
   to this qdisc, (optionally) tests for protocol and asks
   specific classifiers.
 */

static inline int tc_classify(struct sk_buff *skb, struct tcf_proto *tp, struct tcf_result *res)
{
	int err = 0;
	u32 protocol = skb->protocol;

	for ( ; tp; tp = tp->next) {
		if ((tp->protocol == protocol ||
		     tp->protocol == __constant_htons(ETH_P_ALL)) &&
		    (err = tp->classify(skb, tp, res)) >= 0)
			return err;
	}
	return -1;
}

static inline void tcf_destroy(struct tcf_proto *tp)
{
	tp->ops->destroy(tp);
	kfree(tp);
}

extern int register_tcf_proto_ops(struct tcf_proto_ops *ops);
extern int unregister_tcf_proto_ops(struct tcf_proto_ops *ops);



#endif
