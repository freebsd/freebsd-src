/* This is a file to handle the "simple" NAT cases (redirect and
   masquerade) required for the compatibility layer.

   `bind to foreign address' and `getpeername' hacks are not
   supported.

   FIXME: Timing is overly simplistic.  If anyone complains, make it
   use conntrack.
*/
#include <linux/config.h>
#include <linux/netfilter.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/tcp.h>
#include <net/checksum.h>
#include <linux/timer.h>
#include <linux/netdevice.h>
#include <linux/if.h>
#include <linux/in.h>

#include <linux/netfilter_ipv4/lockhelp.h>

/* Very simple timeout pushed back by each packet */
#define REDIR_TIMEOUT (240*HZ)

static DECLARE_LOCK(redir_lock);
#define ASSERT_READ_LOCK(x) MUST_BE_LOCKED(&redir_lock)
#define ASSERT_WRITE_LOCK(x) MUST_BE_LOCKED(&redir_lock)

#include <linux/netfilter_ipv4/listhelp.h>

#if 0
#define DEBUGP printk
#else
#define DEBUGP(format, args...)
#endif

#ifdef CONFIG_NETFILTER_DEBUG
#define IP_NF_ASSERT(x)							 \
do {									 \
	if (!(x))							 \
		/* Wooah!  I'm tripping my conntrack in a frenzy of	 \
		   netplay... */					 \
		printk("ASSERT: %s:%i(%s)\n",				 \
		       __FILE__, __LINE__, __FUNCTION__);		 \
} while(0)
#else
#define IP_NF_ASSERT(x)
#endif

static u_int16_t
cheat_check(u_int32_t oldvalinv, u_int32_t newval, u_int16_t oldcheck)
{
	u_int32_t diffs[] = { oldvalinv, newval };
	return csum_fold(csum_partial((char *)diffs, sizeof(diffs),
				      oldcheck^0xFFFF));
}

struct redir_core {
	u_int32_t orig_srcip, orig_dstip;
	u_int16_t orig_sport, orig_dport;

	u_int32_t new_dstip;
	u_int16_t new_dport;
};

struct redir
{
	struct list_head list;
	struct redir_core core;
	struct timer_list destroyme;
};

static LIST_HEAD(redirs);

static int
redir_cmp(const struct redir *i,
	  u_int32_t orig_srcip, u_int32_t orig_dstip,
	  u_int16_t orig_sport, u_int16_t orig_dport)
{
	return (i->core.orig_srcip == orig_srcip
		&& i->core.orig_dstip == orig_dstip
		&& i->core.orig_sport == orig_sport
		&& i->core.orig_dport == orig_dport);
}

/* Search for an existing redirection of the TCP packet. */
static struct redir *
find_redir(u_int32_t orig_srcip, u_int32_t orig_dstip,
	   u_int16_t orig_sport, u_int16_t orig_dport)
{
	return LIST_FIND(&redirs, redir_cmp, struct redir *,
			 orig_srcip, orig_dstip, orig_sport, orig_dport);
}

static void do_tcp_redir(struct sk_buff *skb, struct redir *redir)
{
	struct iphdr *iph = skb->nh.iph;
	struct tcphdr *tcph = (struct tcphdr *)((u_int32_t *)iph
						+ iph->ihl);

	tcph->check = cheat_check(~redir->core.orig_dstip,
				  redir->core.new_dstip,
				  cheat_check(redir->core.orig_dport ^ 0xFFFF,
					      redir->core.new_dport,
					      tcph->check));
	iph->check = cheat_check(~redir->core.orig_dstip,
				 redir->core.new_dstip, iph->check);
	tcph->dest = redir->core.new_dport;
	iph->daddr = redir->core.new_dstip;

	skb->nfcache |= NFC_ALTERED;
}

static int
unredir_cmp(const struct redir *i,
	    u_int32_t new_dstip, u_int32_t orig_srcip,
	    u_int16_t new_dport, u_int16_t orig_sport)
{
	return (i->core.orig_srcip == orig_srcip
		&& i->core.new_dstip == new_dstip
		&& i->core.orig_sport == orig_sport
		&& i->core.new_dport == new_dport);
}

/* Match reply packet against redir */
static struct redir *
find_unredir(u_int32_t new_dstip, u_int32_t orig_srcip,
	     u_int16_t new_dport, u_int16_t orig_sport)
{
	return LIST_FIND(&redirs, unredir_cmp, struct redir *,
			 new_dstip, orig_srcip, new_dport, orig_sport);
}

/* `unredir' a reply packet. */
static void do_tcp_unredir(struct sk_buff *skb, struct redir *redir)
{
	struct iphdr *iph = skb->nh.iph;
	struct tcphdr *tcph = (struct tcphdr *)((u_int32_t *)iph
						+ iph->ihl);

	tcph->check = cheat_check(~redir->core.new_dstip,
				  redir->core.orig_dstip,
				  cheat_check(redir->core.new_dport ^ 0xFFFF,
					      redir->core.orig_dport,
					      tcph->check));
	iph->check = cheat_check(~redir->core.new_dstip,
				 redir->core.orig_dstip,
				 iph->check);
	tcph->source = redir->core.orig_dport;
	iph->saddr = redir->core.orig_dstip;

	skb->nfcache |= NFC_ALTERED;
}

static void destroyme(unsigned long me)
{
	LOCK_BH(&redir_lock);
	LIST_DELETE(&redirs, (struct redir *)me);
	UNLOCK_BH(&redir_lock);
	kfree((struct redir *)me);
}

/* REDIRECT a packet. */
unsigned int
do_redirect(struct sk_buff *skb,
	    const struct net_device *dev,
	    u_int16_t redirpt)
{
	struct iphdr *iph = skb->nh.iph;
	u_int32_t newdst;

	/* Figure out address: not loopback. */
	if (!dev)
		return NF_DROP;

	/* Grab first address on interface. */
	newdst = ((struct in_device *)dev->ip_ptr)->ifa_list->ifa_local;

	switch (iph->protocol) {
	case IPPROTO_UDP: {
		/* Simple mangle. */
		struct udphdr *udph = (struct udphdr *)((u_int32_t *)iph
							+ iph->ihl);

		/* Must have whole header */
		if (skb->len < iph->ihl*4 + sizeof(*udph))
			return NF_DROP;

		if (udph->check) /* 0 is a special case meaning no checksum */
			udph->check = cheat_check(~iph->daddr, newdst,
					  cheat_check(udph->dest ^ 0xFFFF,
						      redirpt,
						      udph->check));
		iph->check = cheat_check(~iph->daddr, newdst, iph->check);
		udph->dest = redirpt;
		iph->daddr = newdst;

		skb->nfcache |= NFC_ALTERED;
		return NF_ACCEPT;
	}
	case IPPROTO_TCP: {
		/* Mangle, maybe record. */
		struct tcphdr *tcph = (struct tcphdr *)((u_int32_t *)iph
							+ iph->ihl);
		struct redir *redir;
		int ret;

		/* Must have whole header */
		if (skb->len < iph->ihl*4 + sizeof(*tcph))
			return NF_DROP;

		DEBUGP("Doing tcp redirect. %08X:%u %08X:%u -> %08X:%u\n",
		       iph->saddr, tcph->source, iph->daddr, tcph->dest,
		       newdst, redirpt);
		LOCK_BH(&redir_lock);
		redir = find_redir(iph->saddr, iph->daddr,
				   tcph->source, tcph->dest);

		if (!redir) {
			redir = kmalloc(sizeof(struct redir), GFP_ATOMIC);
			if (!redir) {
				ret = NF_DROP;
				goto out;
			}
			list_prepend(&redirs, redir);
			init_timer(&redir->destroyme);
			redir->destroyme.function = destroyme;
			redir->destroyme.data = (unsigned long)redir;
			redir->destroyme.expires = jiffies + REDIR_TIMEOUT;
			add_timer(&redir->destroyme);
		}
		/* In case mangling has changed, rewrite this part. */
		redir->core = ((struct redir_core)
			       { iph->saddr, iph->daddr,
				 tcph->source, tcph->dest,
				 newdst, redirpt });
		do_tcp_redir(skb, redir);
		ret = NF_ACCEPT;

	out:
		UNLOCK_BH(&redir_lock);
		return ret;
	}

	default: /* give up if not TCP or UDP. */
		return NF_DROP;
	}
}

/* Incoming packet: is it a reply to a masqueraded connection, or
   part of an already-redirected TCP connection? */
void
check_for_redirect(struct sk_buff *skb)
{
	struct iphdr *iph = skb->nh.iph;
	struct tcphdr *tcph = (struct tcphdr *)((u_int32_t *)iph
						+ iph->ihl);
	struct redir *redir;

	if (iph->protocol != IPPROTO_TCP)
		return;

	/* Must have whole header */
	if (skb->len < iph->ihl*4 + sizeof(*tcph))
		return;

	LOCK_BH(&redir_lock);
	redir = find_redir(iph->saddr, iph->daddr, tcph->source, tcph->dest);
	if (redir) {
		DEBUGP("Doing tcp redirect again.\n");
		do_tcp_redir(skb, redir);
		if (del_timer(&redir->destroyme)) {
			redir->destroyme.expires = jiffies + REDIR_TIMEOUT;
			add_timer(&redir->destroyme);
		}
	}
	UNLOCK_BH(&redir_lock);
}

void
check_for_unredirect(struct sk_buff *skb)
{
	struct iphdr *iph = skb->nh.iph;
	struct tcphdr *tcph = (struct tcphdr *)((u_int32_t *)iph
						+ iph->ihl);
	struct redir *redir;

	if (iph->protocol != IPPROTO_TCP)
		return;

	/* Must have whole header */
	if (skb->len < iph->ihl*4 + sizeof(*tcph))
		return;

	LOCK_BH(&redir_lock);
	redir = find_unredir(iph->saddr, iph->daddr, tcph->source, tcph->dest);
	if (redir) {
		DEBUGP("Doing tcp unredirect.\n");
		do_tcp_unredir(skb, redir);
		if (del_timer(&redir->destroyme)) {
			redir->destroyme.expires = jiffies + REDIR_TIMEOUT;
			add_timer(&redir->destroyme);
		}
	}
	UNLOCK_BH(&redir_lock);
}
