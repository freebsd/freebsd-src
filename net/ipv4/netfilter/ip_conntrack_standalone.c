/* This file contains all the functions required for the standalone
   ip_conntrack module.

   These are not required by the compatibility layer.
*/

/* (c) 1999 Paul `Rusty' Russell.  Licenced under the GNU General
   Public Licence. */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/ip.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/proc_fs.h>
#include <linux/version.h>
#include <linux/brlock.h>
#ifdef CONFIG_SYSCTL
#include <linux/sysctl.h>
#endif
#include <net/checksum.h>

#define ASSERT_READ_LOCK(x) MUST_BE_READ_LOCKED(&ip_conntrack_lock)
#define ASSERT_WRITE_LOCK(x) MUST_BE_WRITE_LOCKED(&ip_conntrack_lock)

#include <linux/netfilter_ipv4/ip_conntrack.h>
#include <linux/netfilter_ipv4/ip_conntrack_protocol.h>
#include <linux/netfilter_ipv4/ip_conntrack_core.h>
#include <linux/netfilter_ipv4/ip_conntrack_helper.h>
#include <linux/netfilter_ipv4/listhelp.h>

#if 0
#define DEBUGP printk
#else
#define DEBUGP(format, args...)
#endif

MODULE_LICENSE("GPL");

static int kill_proto(const struct ip_conntrack *i, void *data)
{
	return (i->tuplehash[IP_CT_DIR_ORIGINAL].tuple.dst.protonum == 
			*((u_int8_t *) data));
}

static unsigned int
print_tuple(char *buffer, const struct ip_conntrack_tuple *tuple,
	    struct ip_conntrack_protocol *proto)
{
	int len;

	len = sprintf(buffer, "src=%u.%u.%u.%u dst=%u.%u.%u.%u ",
		      NIPQUAD(tuple->src.ip), NIPQUAD(tuple->dst.ip));

	len += proto->print_tuple(buffer + len, tuple);

	return len;
}

/* FIXME: Don't print source proto part. --RR */
static unsigned int
print_expect(char *buffer, const struct ip_conntrack_expect *expect)
{
	unsigned int len;

	if (expect->expectant->helper->timeout)
		len = sprintf(buffer, "EXPECTING: %lu ",
			      timer_pending(&expect->timeout)
			      ? (expect->timeout.expires - jiffies)/HZ : 0);
	else
		len = sprintf(buffer, "EXPECTING: - ");
	len += sprintf(buffer + len, "use=%u proto=%u ",
		      atomic_read(&expect->use), expect->tuple.dst.protonum);
	len += print_tuple(buffer + len, &expect->tuple,
			   __ip_ct_find_proto(expect->tuple.dst.protonum));
	len += sprintf(buffer + len, "\n");
	return len;
}

static unsigned int
print_conntrack(char *buffer, struct ip_conntrack *conntrack)
{
	unsigned int len;
	struct ip_conntrack_protocol *proto
		= __ip_ct_find_proto(conntrack->tuplehash[IP_CT_DIR_ORIGINAL]
			       .tuple.dst.protonum);

	len = sprintf(buffer, "%-8s %u %lu ",
		      proto->name,
		      conntrack->tuplehash[IP_CT_DIR_ORIGINAL]
		      .tuple.dst.protonum,
		      timer_pending(&conntrack->timeout)
		      ? (conntrack->timeout.expires - jiffies)/HZ : 0);

	len += proto->print_conntrack(buffer + len, conntrack);
	len += print_tuple(buffer + len,
			   &conntrack->tuplehash[IP_CT_DIR_ORIGINAL].tuple,
			   proto);
	if (!(test_bit(IPS_SEEN_REPLY_BIT, &conntrack->status)))
		len += sprintf(buffer + len, "[UNREPLIED] ");
	len += print_tuple(buffer + len,
			   &conntrack->tuplehash[IP_CT_DIR_REPLY].tuple,
			   proto);
	if (test_bit(IPS_ASSURED_BIT, &conntrack->status))
		len += sprintf(buffer + len, "[ASSURED] ");
	len += sprintf(buffer + len, "use=%u ",
		       atomic_read(&conntrack->ct_general.use));
	len += sprintf(buffer + len, "\n");

	return len;
}

/* Returns true when finished. */
static inline int
conntrack_iterate(const struct ip_conntrack_tuple_hash *hash,
		  char *buffer, off_t offset, off_t *upto,
		  unsigned int *len, unsigned int maxlen)
{
	unsigned int newlen;
	IP_NF_ASSERT(hash->ctrack);

	MUST_BE_READ_LOCKED(&ip_conntrack_lock);

	/* Only count originals */
	if (DIRECTION(hash))
		return 0;

	if ((*upto)++ < offset)
		return 0;

	newlen = print_conntrack(buffer + *len, hash->ctrack);
	if (*len + newlen > maxlen)
		return 1;
	else *len += newlen;

	return 0;
}

static int
list_conntracks(char *buffer, char **start, off_t offset, int length)
{
	unsigned int i;
	unsigned int len = 0;
	off_t upto = 0;
	struct list_head *e;

	READ_LOCK(&ip_conntrack_lock);
	/* Traverse hash; print originals then reply. */
	for (i = 0; i < ip_conntrack_htable_size; i++) {
		if (LIST_FIND(&ip_conntrack_hash[i], conntrack_iterate,
			      struct ip_conntrack_tuple_hash *,
			      buffer, offset, &upto, &len, length))
			goto finished;
	}

	/* Now iterate through expecteds. */
	for (e = ip_conntrack_expect_list.next; 
	     e != &ip_conntrack_expect_list; e = e->next) {
		unsigned int last_len;
		struct ip_conntrack_expect *expect
			= (struct ip_conntrack_expect *)e;
		if (upto++ < offset) continue;

		last_len = len;
		len += print_expect(buffer + len, expect);
		if (len > length) {
			len = last_len;
			goto finished;
		}
	}

 finished:
	READ_UNLOCK(&ip_conntrack_lock);

	/* `start' hack - see fs/proc/generic.c line ~165 */
	*start = (char *)((unsigned int)upto - offset);
	return len;
}

static unsigned int ip_confirm(unsigned int hooknum,
			       struct sk_buff **pskb,
			       const struct net_device *in,
			       const struct net_device *out,
			       int (*okfn)(struct sk_buff *))
{
	/* We've seen it coming out the other side: confirm it */
	return ip_conntrack_confirm(*pskb);
}

static unsigned int ip_refrag(unsigned int hooknum,
			      struct sk_buff **pskb,
			      const struct net_device *in,
			      const struct net_device *out,
			      int (*okfn)(struct sk_buff *))
{
	struct rtable *rt = (struct rtable *)(*pskb)->dst;

	/* We've seen it coming out the other side: confirm */
	if (ip_confirm(hooknum, pskb, in, out, okfn) != NF_ACCEPT)
		return NF_DROP;

	/* Local packets are never produced too large for their
	   interface.  We degfragment them at LOCAL_OUT, however,
	   so we have to refragment them here. */
	if ((*pskb)->len > rt->u.dst.pmtu) {
		/* No hook can be after us, so this should be OK. */
		ip_fragment(*pskb, okfn);
		return NF_STOLEN;
	}
	return NF_ACCEPT;
}

static unsigned int ip_conntrack_local(unsigned int hooknum,
				       struct sk_buff **pskb,
				       const struct net_device *in,
				       const struct net_device *out,
				       int (*okfn)(struct sk_buff *))
{
	/* root is playing with raw sockets. */
	if ((*pskb)->len < sizeof(struct iphdr)
	    || (*pskb)->nh.iph->ihl * 4 < sizeof(struct iphdr)) {
		if (net_ratelimit())
			printk("ipt_hook: happy cracking.\n");
		return NF_ACCEPT;
	}
	return ip_conntrack_in(hooknum, pskb, in, out, okfn);
}

/* Connection tracking may drop packets, but never alters them, so
   make it the first hook. */
static struct nf_hook_ops ip_conntrack_in_ops
= { { NULL, NULL }, ip_conntrack_in, PF_INET, NF_IP_PRE_ROUTING,
	NF_IP_PRI_CONNTRACK };
static struct nf_hook_ops ip_conntrack_local_out_ops
= { { NULL, NULL }, ip_conntrack_local, PF_INET, NF_IP_LOCAL_OUT,
	NF_IP_PRI_CONNTRACK };
/* Refragmenter; last chance. */
static struct nf_hook_ops ip_conntrack_out_ops
= { { NULL, NULL }, ip_refrag, PF_INET, NF_IP_POST_ROUTING, NF_IP_PRI_LAST };
static struct nf_hook_ops ip_conntrack_local_in_ops
= { { NULL, NULL }, ip_confirm, PF_INET, NF_IP_LOCAL_IN, NF_IP_PRI_LAST-1 };

/* Sysctl support */

#ifdef CONFIG_SYSCTL

/* From ip_conntrack_core.c */
extern int ip_conntrack_max;
extern unsigned int ip_conntrack_htable_size;

/* From ip_conntrack_proto_tcp.c */
extern unsigned long ip_ct_tcp_timeout_syn_sent;
extern unsigned long ip_ct_tcp_timeout_syn_recv;
extern unsigned long ip_ct_tcp_timeout_established;
extern unsigned long ip_ct_tcp_timeout_fin_wait;
extern unsigned long ip_ct_tcp_timeout_close_wait;
extern unsigned long ip_ct_tcp_timeout_last_ack;
extern unsigned long ip_ct_tcp_timeout_time_wait;
extern unsigned long ip_ct_tcp_timeout_close;

/* From ip_conntrack_proto_udp.c */
extern unsigned long ip_ct_udp_timeout;
extern unsigned long ip_ct_udp_timeout_stream;

/* From ip_conntrack_proto_icmp.c */
extern unsigned long ip_ct_icmp_timeout;

/* From ip_conntrack_proto_icmp.c */
extern unsigned long ip_ct_generic_timeout;

static struct ctl_table_header *ip_ct_sysctl_header;

static ctl_table ip_ct_sysctl_table[] = {
	{NET_IPV4_NF_CONNTRACK_MAX, "ip_conntrack_max",
	 &ip_conntrack_max, sizeof(int), 0644, NULL,
	 &proc_dointvec},
	{NET_IPV4_NF_CONNTRACK_BUCKETS, "ip_conntrack_buckets",
	 &ip_conntrack_htable_size, sizeof(unsigned int), 0444, NULL,
	 &proc_dointvec},
	{NET_IPV4_NF_CONNTRACK_TCP_TIMEOUT_SYN_SENT, "ip_conntrack_tcp_timeout_syn_sent",
	 &ip_ct_tcp_timeout_syn_sent, sizeof(unsigned int), 0644, NULL,
	 &proc_dointvec_jiffies},
	{NET_IPV4_NF_CONNTRACK_TCP_TIMEOUT_SYN_RECV, "ip_conntrack_tcp_timeout_syn_recv",
	 &ip_ct_tcp_timeout_syn_recv, sizeof(unsigned int), 0644, NULL,
	 &proc_dointvec_jiffies},
	{NET_IPV4_NF_CONNTRACK_TCP_TIMEOUT_ESTABLISHED, "ip_conntrack_tcp_timeout_established",
	 &ip_ct_tcp_timeout_established, sizeof(unsigned int), 0644, NULL,
	 &proc_dointvec_jiffies},
	{NET_IPV4_NF_CONNTRACK_TCP_TIMEOUT_FIN_WAIT, "ip_conntrack_tcp_timeout_fin_wait",
	 &ip_ct_tcp_timeout_fin_wait, sizeof(unsigned int), 0644, NULL,
	 &proc_dointvec_jiffies},
	{NET_IPV4_NF_CONNTRACK_TCP_TIMEOUT_CLOSE_WAIT, "ip_conntrack_tcp_timeout_close_wait",
	 &ip_ct_tcp_timeout_close_wait, sizeof(unsigned int), 0644, NULL,
	 &proc_dointvec_jiffies},
	{NET_IPV4_NF_CONNTRACK_TCP_TIMEOUT_LAST_ACK, "ip_conntrack_tcp_timeout_last_ack",
	 &ip_ct_tcp_timeout_last_ack, sizeof(unsigned int), 0644, NULL,
	 &proc_dointvec_jiffies},
	{NET_IPV4_NF_CONNTRACK_TCP_TIMEOUT_TIME_WAIT, "ip_conntrack_tcp_timeout_time_wait",
	 &ip_ct_tcp_timeout_time_wait, sizeof(unsigned int), 0644, NULL,
	 &proc_dointvec_jiffies},
	{NET_IPV4_NF_CONNTRACK_TCP_TIMEOUT_CLOSE, "ip_conntrack_tcp_timeout_close",
	 &ip_ct_tcp_timeout_close, sizeof(unsigned int), 0644, NULL,
	 &proc_dointvec_jiffies},
	{NET_IPV4_NF_CONNTRACK_UDP_TIMEOUT, "ip_conntrack_udp_timeout",
	 &ip_ct_udp_timeout, sizeof(unsigned int), 0644, NULL,
	 &proc_dointvec_jiffies},
	{NET_IPV4_NF_CONNTRACK_UDP_TIMEOUT_STREAM, "ip_conntrack_udp_timeout_stream",
	 &ip_ct_udp_timeout_stream, sizeof(unsigned int), 0644, NULL,
	 &proc_dointvec_jiffies},
	{NET_IPV4_NF_CONNTRACK_ICMP_TIMEOUT, "ip_conntrack_icmp_timeout",
	 &ip_ct_icmp_timeout, sizeof(unsigned int), 0644, NULL,
	 &proc_dointvec_jiffies},
	{NET_IPV4_NF_CONNTRACK_GENERIC_TIMEOUT, "ip_conntrack_generic_timeout",
	 &ip_ct_generic_timeout, sizeof(unsigned int), 0644, NULL,
	 &proc_dointvec_jiffies},
	{0}
};

#define NET_IP_CONNTRACK_MAX 2089

static ctl_table ip_ct_netfilter_table[] = {
	{NET_IPV4_NETFILTER, "netfilter", NULL, 0, 0555, ip_ct_sysctl_table, 0, 0, 0, 0, 0},
	{NET_IP_CONNTRACK_MAX, "ip_conntrack_max",
	 &ip_conntrack_max, sizeof(int), 0644, NULL,
	 &proc_dointvec},
	{0}
};

static ctl_table ip_ct_ipv4_table[] = {
	{NET_IPV4, "ipv4", NULL, 0, 0555, ip_ct_netfilter_table, 0, 0, 0, 0, 0},
	{0}
};

static ctl_table ip_ct_net_table[] = {
	{CTL_NET, "net", NULL, 0, 0555, ip_ct_ipv4_table, 0, 0, 0, 0, 0},
	{0}
};
#endif
static int init_or_cleanup(int init)
{
	struct proc_dir_entry *proc;
	int ret = 0;

	if (!init) goto cleanup;

	ret = ip_conntrack_init();
	if (ret < 0)
		goto cleanup_nothing;

	proc = proc_net_create("ip_conntrack",0,list_conntracks);
	if (!proc) goto cleanup_init;
	proc->owner = THIS_MODULE;

	ret = nf_register_hook(&ip_conntrack_in_ops);
	if (ret < 0) {
		printk("ip_conntrack: can't register pre-routing hook.\n");
		goto cleanup_proc;
	}
	ret = nf_register_hook(&ip_conntrack_local_out_ops);
	if (ret < 0) {
		printk("ip_conntrack: can't register local out hook.\n");
		goto cleanup_inops;
	}
	ret = nf_register_hook(&ip_conntrack_out_ops);
	if (ret < 0) {
		printk("ip_conntrack: can't register post-routing hook.\n");
		goto cleanup_inandlocalops;
	}
	ret = nf_register_hook(&ip_conntrack_local_in_ops);
	if (ret < 0) {
		printk("ip_conntrack: can't register local in hook.\n");
		goto cleanup_inoutandlocalops;
	}
#ifdef CONFIG_SYSCTL
	ip_ct_sysctl_header = register_sysctl_table(ip_ct_net_table, 0);
	if (ip_ct_sysctl_header == NULL) {
		printk("ip_conntrack: can't register to sysctl.\n");
		goto cleanup;
	}
#endif

	return ret;

 cleanup:
#ifdef CONFIG_SYSCTL
 	unregister_sysctl_table(ip_ct_sysctl_header);
#endif
	nf_unregister_hook(&ip_conntrack_local_in_ops);
 cleanup_inoutandlocalops:
	nf_unregister_hook(&ip_conntrack_out_ops);
 cleanup_inandlocalops:
	nf_unregister_hook(&ip_conntrack_local_out_ops);
 cleanup_inops:
	nf_unregister_hook(&ip_conntrack_in_ops);
 cleanup_proc:
	proc_net_remove("ip_conntrack");
 cleanup_init:
	ip_conntrack_cleanup();
 cleanup_nothing:
	return ret;
}

/* FIXME: Allow NULL functions and sub in pointers to generic for
   them. --RR */
int ip_conntrack_protocol_register(struct ip_conntrack_protocol *proto)
{
	int ret = 0;
	struct list_head *i;

	WRITE_LOCK(&ip_conntrack_lock);
	for (i = protocol_list.next; i != &protocol_list; i = i->next) {
		if (((struct ip_conntrack_protocol *)i)->proto
		    == proto->proto) {
			ret = -EBUSY;
			goto out;
		}
	}

	list_prepend(&protocol_list, proto);
	MOD_INC_USE_COUNT;

 out:
	WRITE_UNLOCK(&ip_conntrack_lock);
	return ret;
}

void ip_conntrack_protocol_unregister(struct ip_conntrack_protocol *proto)
{
	WRITE_LOCK(&ip_conntrack_lock);

	/* ip_ct_find_proto() returns proto_generic in case there is no protocol 
	 * helper. So this should be enough - HW */
	LIST_DELETE(&protocol_list, proto);
	WRITE_UNLOCK(&ip_conntrack_lock);
	
	/* Somebody could be still looking at the proto in bh. */
	br_write_lock_bh(BR_NETPROTO_LOCK);
	br_write_unlock_bh(BR_NETPROTO_LOCK);

	/* Remove all contrack entries for this protocol */
	ip_ct_selective_cleanup(kill_proto, &proto->proto);

	MOD_DEC_USE_COUNT;
}

static int __init init(void)
{
	return init_or_cleanup(1);
}

static void __exit fini(void)
{
	init_or_cleanup(0);
}

module_init(init);
module_exit(fini);

EXPORT_SYMBOL(ip_conntrack_protocol_register);
EXPORT_SYMBOL(ip_conntrack_protocol_unregister);
EXPORT_SYMBOL(invert_tuplepr);
EXPORT_SYMBOL(ip_conntrack_alter_reply);
EXPORT_SYMBOL(ip_conntrack_destroyed);
EXPORT_SYMBOL(ip_conntrack_get);
EXPORT_SYMBOL(ip_conntrack_helper_register);
EXPORT_SYMBOL(ip_conntrack_helper_unregister);
EXPORT_SYMBOL(ip_ct_selective_cleanup);
EXPORT_SYMBOL(ip_ct_refresh);
EXPORT_SYMBOL(ip_ct_find_proto);
EXPORT_SYMBOL(__ip_ct_find_proto);
EXPORT_SYMBOL(ip_ct_find_helper);
EXPORT_SYMBOL(ip_conntrack_expect_related);
EXPORT_SYMBOL(ip_conntrack_change_expect);
EXPORT_SYMBOL(ip_conntrack_unexpect_related);
EXPORT_SYMBOL_GPL(ip_conntrack_expect_find_get);
EXPORT_SYMBOL_GPL(ip_conntrack_expect_put);
EXPORT_SYMBOL(ip_conntrack_tuple_taken);
EXPORT_SYMBOL(ip_ct_gather_frags);
EXPORT_SYMBOL(ip_conntrack_htable_size);
EXPORT_SYMBOL(ip_conntrack_expect_list);
EXPORT_SYMBOL(ip_conntrack_lock);
EXPORT_SYMBOL(ip_conntrack_hash);
EXPORT_SYMBOL_GPL(ip_conntrack_find_get);
EXPORT_SYMBOL_GPL(ip_conntrack_put);
