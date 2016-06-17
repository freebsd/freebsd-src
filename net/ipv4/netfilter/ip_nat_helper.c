/* ip_nat_mangle.c - generic support functions for NAT helpers 
 *
 * (C) 2000-2002 by Harald Welte <laforge@gnumonks.org>
 *
 * distributed under the terms of GNU GPL
 *
 * 	14 Jan 2002 Harald Welte <laforge@gnumonks.org>:
 *		- add support for SACK adjustment 
 *	14 Mar 2002 Harald Welte <laforge@gnumonks.org>:
 *		- merge SACK support into newnat API
 *	16 Aug 2002 Brian J. Murrell <netfilter@interlinx.bc.ca>:
 *		- make ip_nat_resize_packet more generic (TCP and UDP)
 *		- add ip_nat_mangle_udp_packet
 */
#include <linux/version.h>
#include <linux/config.h>
#include <linux/module.h>
#include <linux/kmod.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/skbuff.h>
#include <linux/netfilter_ipv4.h>
#include <linux/brlock.h>
#include <net/checksum.h>
#include <net/icmp.h>
#include <net/ip.h>
#include <net/tcp.h>
#include <net/udp.h>

#define ASSERT_READ_LOCK(x) MUST_BE_READ_LOCKED(&ip_nat_lock)
#define ASSERT_WRITE_LOCK(x) MUST_BE_WRITE_LOCKED(&ip_nat_lock)

#include <linux/netfilter_ipv4/ip_conntrack.h>
#include <linux/netfilter_ipv4/ip_conntrack_helper.h>
#include <linux/netfilter_ipv4/ip_nat.h>
#include <linux/netfilter_ipv4/ip_nat_protocol.h>
#include <linux/netfilter_ipv4/ip_nat_core.h>
#include <linux/netfilter_ipv4/ip_nat_helper.h>
#include <linux/netfilter_ipv4/listhelp.h>

#if 0
#define DEBUGP printk
#define DUMP_OFFSET(x)	printk("offset_before=%d, offset_after=%d, correction_pos=%u\n", x->offset_before, x->offset_after, x->correction_pos);
#else
#define DEBUGP(format, args...)
#define DUMP_OFFSET(x)
#endif

DECLARE_LOCK(ip_nat_seqofs_lock);
			 
static inline int 
ip_nat_resize_packet(struct sk_buff **skb,
		     struct ip_conntrack *ct, 
		     enum ip_conntrack_info ctinfo,
		     int new_size)
{
	struct iphdr *iph;
	int dir;
	struct ip_nat_seq *this_way, *other_way;

	DEBUGP("ip_nat_resize_packet: old_size = %u, new_size = %u\n",
		(*skb)->len, new_size);

	dir = CTINFO2DIR(ctinfo);

	this_way = &ct->nat.info.seq[dir];
	other_way = &ct->nat.info.seq[!dir];

	if (new_size > (*skb)->len + skb_tailroom(*skb)) {
		struct sk_buff *newskb;
		newskb = skb_copy_expand(*skb, skb_headroom(*skb),
					 new_size - (*skb)->len,
					 GFP_ATOMIC);

		if (!newskb) {
			printk("ip_nat_resize_packet: oom\n");
			return 0;
		} else {
			kfree_skb(*skb);
			*skb = newskb;
		}
	}

	iph = (*skb)->nh.iph;
	if (iph->protocol == IPPROTO_TCP) {
		struct tcphdr *tcph = (void *)iph + iph->ihl*4;

		DEBUGP("ip_nat_resize_packet: Seq_offset before: ");
		DUMP_OFFSET(this_way);

		LOCK_BH(&ip_nat_seqofs_lock);

		/* SYN adjust. If it's uninitialized, of this is after last 
		 * correction, record it: we don't handle more than one 
		 * adjustment in the window, but do deal with common case of a 
		 * retransmit */
		if (this_way->offset_before == this_way->offset_after
		    || before(this_way->correction_pos, ntohl(tcph->seq))) {
			this_way->correction_pos = ntohl(tcph->seq);
			this_way->offset_before = this_way->offset_after;
			this_way->offset_after = (int32_t)
				this_way->offset_before + new_size -
				(*skb)->len;
		}

		UNLOCK_BH(&ip_nat_seqofs_lock);

		DEBUGP("ip_nat_resize_packet: Seq_offset after: ");
		DUMP_OFFSET(this_way);
	}
	
	return 1;
}


/* Generic function for mangling variable-length address changes inside
 * NATed TCP connections (like the PORT XXX,XXX,XXX,XXX,XXX,XXX
 * command in FTP).
 *
 * Takes care about all the nasty sequence number changes, checksumming,
 * skb enlargement, ...
 *
 * */
int 
ip_nat_mangle_tcp_packet(struct sk_buff **skb,
			 struct ip_conntrack *ct,
			 enum ip_conntrack_info ctinfo,
			 unsigned int match_offset,
			 unsigned int match_len,
			 char *rep_buffer,
			 unsigned int rep_len)
{
	struct iphdr *iph = (*skb)->nh.iph;
	struct tcphdr *tcph;
	unsigned char *data;
	u_int32_t tcplen, newlen, newtcplen;

	tcplen = (*skb)->len - iph->ihl*4;
	newtcplen = tcplen - match_len + rep_len;
	newlen = iph->ihl*4 + newtcplen;

	if (newlen > 65535) {
		if (net_ratelimit())
			printk("ip_nat_mangle_tcp_packet: nat'ed packet "
				"exceeds maximum packet size\n");
		return 0;
	}

	if ((*skb)->len != newlen) {
		if (!ip_nat_resize_packet(skb, ct, ctinfo, newlen)) {
			printk("resize_packet failed!!\n");
			return 0;
		}
	}

	/* Alexey says: if a hook changes _data_ ... it can break
	   original packet sitting in tcp queue and this is fatal */
	if (skb_cloned(*skb)) {
		struct sk_buff *nskb = skb_copy(*skb, GFP_ATOMIC);
		if (!nskb) {
			if (net_ratelimit())
				printk("Out of memory cloning TCP packet\n");
			return 0;
		}
		/* Rest of kernel will get very unhappy if we pass it
		   a suddenly-orphaned skbuff */
		if ((*skb)->sk)
			skb_set_owner_w(nskb, (*skb)->sk);
		kfree_skb(*skb);
		*skb = nskb;
	}

	/* skb may be copied !! */
	iph = (*skb)->nh.iph;
	tcph = (void *)iph + iph->ihl*4;
	data = (void *)tcph + tcph->doff*4;

	if (rep_len != match_len)
		/* move post-replacement */
		memmove(data + match_offset + rep_len,
			data + match_offset + match_len,
			(*skb)->tail - (data + match_offset + match_len));

	/* insert data from buffer */
	memcpy(data + match_offset, rep_buffer, rep_len);

	/* update skb info */
	if (newlen > (*skb)->len) {
		DEBUGP("ip_nat_mangle_tcp_packet: Extending packet by "
			"%u to %u bytes\n", newlen - (*skb)->len, newlen);
		skb_put(*skb, newlen - (*skb)->len);
	} else {
		DEBUGP("ip_nat_mangle_tcp_packet: Shrinking packet from "
			"%u to %u bytes\n", (*skb)->len, newlen);
		skb_trim(*skb, newlen);
	}

	iph->tot_len = htons(newlen);
	/* fix checksum information */
	tcph->check = 0;
	tcph->check = tcp_v4_check(tcph, newtcplen, iph->saddr, iph->daddr,
				   csum_partial((char *)tcph, newtcplen, 0));
	ip_send_check(iph);

	return 1;
}
			
/* Generic function for mangling variable-length address changes inside
 * NATed UDP connections (like the CONNECT DATA XXXXX MESG XXXXX INDEX XXXXX
 * command in the Amanda protocol)
 *
 * Takes care about all the nasty sequence number changes, checksumming,
 * skb enlargement, ...
 *
 * XXX - This function could be merged with ip_nat_mangle_tcp_packet which
 *       should be fairly easy to do.
 */
int 
ip_nat_mangle_udp_packet(struct sk_buff **skb,
			 struct ip_conntrack *ct,
			 enum ip_conntrack_info ctinfo,
			 unsigned int match_offset,
			 unsigned int match_len,
			 char *rep_buffer,
			 unsigned int rep_len)
{
	struct iphdr *iph = (*skb)->nh.iph;
	struct udphdr *udph = (void *)iph + iph->ihl * 4;
	unsigned char *data;
	u_int32_t udplen, newlen, newudplen;

	udplen = (*skb)->len - iph->ihl*4;
	newudplen = udplen - match_len + rep_len;
	newlen = iph->ihl*4 + newudplen;

	/* UDP helpers might accidentally mangle the wrong packet */
	if (udplen < sizeof(*udph) + match_offset + match_len) {
		if (net_ratelimit())
			printk("ip_nat_mangle_udp_packet: undersized packet\n");
		return 0;
	}

	if (newlen > 65535) {
		if (net_ratelimit())
			printk("ip_nat_mangle_udp_packet: nat'ed packet "
				"exceeds maximum packet size\n");
		return 0;
	}

	if ((*skb)->len != newlen) {
		if (!ip_nat_resize_packet(skb, ct, ctinfo, newlen)) {
			printk("resize_packet failed!!\n");
			return 0;
		}
	}

	/* Alexey says: if a hook changes _data_ ... it can break
	   original packet sitting in tcp queue and this is fatal */
	if (skb_cloned(*skb)) {
		struct sk_buff *nskb = skb_copy(*skb, GFP_ATOMIC);
		if (!nskb) {
			if (net_ratelimit())
				printk("Out of memory cloning TCP packet\n");
			return 0;
		}
		/* Rest of kernel will get very unhappy if we pass it
		   a suddenly-orphaned skbuff */
		if ((*skb)->sk)
			skb_set_owner_w(nskb, (*skb)->sk);
		kfree_skb(*skb);
		*skb = nskb;
	}

	/* skb may be copied !! */
	iph = (*skb)->nh.iph;
	udph = (void *)iph + iph->ihl*4;
	data = (void *)udph + sizeof(struct udphdr);

	if (rep_len != match_len)
		/* move post-replacement */
		memmove(data + match_offset + rep_len,
			data + match_offset + match_len,
			(*skb)->tail - (data + match_offset + match_len));

	/* insert data from buffer */
	memcpy(data + match_offset, rep_buffer, rep_len);

	/* update skb info */
	if (newlen > (*skb)->len) {
		DEBUGP("ip_nat_mangle_udp_packet: Extending packet by "
			"%u to %u bytes\n", newlen - (*skb)->len, newlen);
		skb_put(*skb, newlen - (*skb)->len);
	} else {
		DEBUGP("ip_nat_mangle_udp_packet: Shrinking packet from "
			"%u to %u bytes\n", (*skb)->len, newlen);
		skb_trim(*skb, newlen);
	}

	/* update the length of the UDP and IP packets to the new values*/
	udph->len = htons((*skb)->len - iph->ihl*4);
	iph->tot_len = htons(newlen);

	/* fix udp checksum if udp checksum was previously calculated */
	if (udph->check != 0) {
		udph->check = 0;
		udph->check = csum_tcpudp_magic(iph->saddr, iph->daddr,
						newudplen, IPPROTO_UDP,
						csum_partial((char *)udph,
						             newudplen, 0));
	}

	ip_send_check(iph);

	return 1;
}

/* Adjust one found SACK option including checksum correction */
static void
sack_adjust(struct tcphdr *tcph, 
	    unsigned char *ptr, 
	    struct ip_nat_seq *natseq)
{
	struct tcp_sack_block *sp = (struct tcp_sack_block *)(ptr+2);
	int num_sacks = (ptr[1] - TCPOLEN_SACK_BASE)>>3;
	int i;

	for (i = 0; i < num_sacks; i++, sp++) {
		u_int32_t new_start_seq, new_end_seq;

		if (after(ntohl(sp->start_seq) - natseq->offset_before,
			  natseq->correction_pos))
			new_start_seq = ntohl(sp->start_seq) 
					- natseq->offset_after;
		else
			new_start_seq = ntohl(sp->start_seq) 
					- natseq->offset_before;
		new_start_seq = htonl(new_start_seq);

		if (after(ntohl(sp->end_seq) - natseq->offset_before,
			  natseq->correction_pos))
			new_end_seq = ntohl(sp->end_seq)
				      - natseq->offset_after;
		else
			new_end_seq = ntohl(sp->end_seq)
				      - natseq->offset_before;
		new_end_seq = htonl(new_end_seq);

		DEBUGP("sack_adjust: start_seq: %d->%d, end_seq: %d->%d\n",
			ntohl(sp->start_seq), new_start_seq,
			ntohl(sp->end_seq), new_end_seq);

		tcph->check = 
			ip_nat_cheat_check(~sp->start_seq, new_start_seq,
					   ip_nat_cheat_check(~sp->end_seq, 
						   	      new_end_seq,
							      tcph->check));

		sp->start_seq = new_start_seq;
		sp->end_seq = new_end_seq;
	}
}
			

/* TCP SACK sequence number adjustment. */
static inline void
ip_nat_sack_adjust(struct sk_buff *skb,
		   struct ip_conntrack *ct,
		   enum ip_conntrack_info ctinfo)
{
	struct tcphdr *tcph;
	unsigned char *ptr, *optend;
	unsigned int dir;

	tcph = (void *)skb->nh.iph + skb->nh.iph->ihl*4;
	optend = (unsigned char *)tcph + tcph->doff*4;
	ptr = (unsigned char *)(tcph+1);

	dir = CTINFO2DIR(ctinfo);

	while (ptr < optend) {
		int opcode = ptr[0];
		int opsize;

		switch (opcode) {
		case TCPOPT_EOL:
			return;
		case TCPOPT_NOP:
			ptr++;
			continue;
		default:
			opsize = ptr[1];
			 /* no partial opts */
			if (ptr + opsize > optend || opsize < 2)
				return;
			if (opcode == TCPOPT_SACK) {
				/* found SACK */
				if((opsize >= (TCPOLEN_SACK_BASE
					       +TCPOLEN_SACK_PERBLOCK)) &&
				   !((opsize - TCPOLEN_SACK_BASE)
				     % TCPOLEN_SACK_PERBLOCK))
					sack_adjust(tcph, ptr,
						    &ct->nat.info.seq[!dir]);
			}
			ptr += opsize;
		}
	}
}

/* TCP sequence number adjustment */
int 
ip_nat_seq_adjust(struct sk_buff *skb, 
		  struct ip_conntrack *ct, 
		  enum ip_conntrack_info ctinfo)
{
	struct iphdr *iph;
	struct tcphdr *tcph;
	int dir, newseq, newack;
	struct ip_nat_seq *this_way, *other_way;	
	
	iph = skb->nh.iph;
	tcph = (void *)iph + iph->ihl*4;

	dir = CTINFO2DIR(ctinfo);

	this_way = &ct->nat.info.seq[dir];
	other_way = &ct->nat.info.seq[!dir];
	
	if (after(ntohl(tcph->seq), this_way->correction_pos))
		newseq = ntohl(tcph->seq) + this_way->offset_after;
	else
		newseq = ntohl(tcph->seq) + this_way->offset_before;
	newseq = htonl(newseq);

	if (after(ntohl(tcph->ack_seq) - other_way->offset_before,
		  other_way->correction_pos))
		newack = ntohl(tcph->ack_seq) - other_way->offset_after;
	else
		newack = ntohl(tcph->ack_seq) - other_way->offset_before;
	newack = htonl(newack);

	tcph->check = ip_nat_cheat_check(~tcph->seq, newseq,
					 ip_nat_cheat_check(~tcph->ack_seq, 
					 		    newack, 
							    tcph->check));

	DEBUGP("Adjusting sequence number from %u->%u, ack from %u->%u\n",
		ntohl(tcph->seq), ntohl(newseq), ntohl(tcph->ack_seq),
		ntohl(newack));

	tcph->seq = newseq;
	tcph->ack_seq = newack;

	ip_nat_sack_adjust(skb, ct, ctinfo);

	return 0;
}

static inline int
helper_cmp(const struct ip_nat_helper *helper,
	   const struct ip_conntrack_tuple *tuple)
{
	return ip_ct_tuple_mask_cmp(tuple, &helper->tuple, &helper->mask);
}

#define MODULE_MAX_NAMELEN		32

int ip_nat_helper_register(struct ip_nat_helper *me)
{
	int ret = 0;

	if (me->me && !(me->flags & IP_NAT_HELPER_F_STANDALONE)) {
		struct ip_conntrack_helper *ct_helper;
		
		if ((ct_helper = ip_ct_find_helper(&me->tuple))
		    && ct_helper->me) {
			__MOD_INC_USE_COUNT(ct_helper->me);
		} else {

			/* We are a NAT helper for protocol X.  If we need
			 * respective conntrack helper for protoccol X, compute
			 * conntrack helper name and try to load module */
			char name[MODULE_MAX_NAMELEN];
			const char *tmp = me->me->name;
			
			if (strlen(tmp) + 6 > MODULE_MAX_NAMELEN) {
				printk("%s: unable to "
				       "compute conntrack helper name "
				       "from %s\n", __FUNCTION__, tmp);
				return -EBUSY;
			}
			tmp += 6;
			sprintf(name, "ip_conntrack%s", tmp);
#ifdef CONFIG_KMOD
			if (!request_module(name)
			    && (ct_helper = ip_ct_find_helper(&me->tuple))
			    && ct_helper->me) {
				__MOD_INC_USE_COUNT(ct_helper->me);
			} else {
				printk("unable to load module %s\n", name);
				return -EBUSY;
			}
#else
			printk("unable to load module %s automatically "
			       "because kernel was compiled without kernel "
			       "module loader support\n", name);
			return -EBUSY;
#endif
		}
	}
	WRITE_LOCK(&ip_nat_lock);
	if (LIST_FIND(&helpers, helper_cmp, struct ip_nat_helper *,&me->tuple))
		ret = -EBUSY;
	else {
		list_prepend(&helpers, me);
		MOD_INC_USE_COUNT;
	}
	WRITE_UNLOCK(&ip_nat_lock);

	return ret;
}

static int
kill_helper(const struct ip_conntrack *i, void *helper)
{
	int ret;

	READ_LOCK(&ip_nat_lock);
	ret = (i->nat.info.helper == helper);
	READ_UNLOCK(&ip_nat_lock);

	return ret;
}

void ip_nat_helper_unregister(struct ip_nat_helper *me)
{
	int found = 0;
	
	WRITE_LOCK(&ip_nat_lock);
	/* Autoloading conntrack helper might have failed */
	if (LIST_FIND(&helpers, helper_cmp, struct ip_nat_helper *,&me->tuple)) {
		LIST_DELETE(&helpers, me);
		found = 1;
	}
	WRITE_UNLOCK(&ip_nat_lock);

	/* Someone could be still looking at the helper in a bh. */
	br_write_lock_bh(BR_NETPROTO_LOCK);
	br_write_unlock_bh(BR_NETPROTO_LOCK);

	/* Find anything using it, and umm, kill them.  We can't turn
	   them into normal connections: if we've adjusted SYNs, then
	   they'll ackstorm.  So we just drop it.  We used to just
	   bump module count when a connection existed, but that
	   forces admins to gen fake RSTs or bounce box, either of
	   which is just a long-winded way of making things
	   worse. --RR */
	ip_ct_selective_cleanup(kill_helper, me);

	if (found)
		MOD_DEC_USE_COUNT;

	/* If we are no standalone NAT helper, we need to decrement usage count
	 * on our conntrack helper */
	if (me->me && !(me->flags & IP_NAT_HELPER_F_STANDALONE)) {
		struct ip_conntrack_helper *ct_helper;
		
		if ((ct_helper = ip_ct_find_helper(&me->tuple))
		    && ct_helper->me) {
			__MOD_DEC_USE_COUNT(ct_helper->me);
		} else 
			printk("%s: unable to decrement usage count"
			       " of conntrack helper %s\n",
			       __FUNCTION__, me->me->name);
	}
}
