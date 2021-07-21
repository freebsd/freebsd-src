#ifndef _NET_NETLINK_H
#define _NET_NETLINK_H

#include <sys/param.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/domain.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/rwlock.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/vnet.h>
#include <net/raw_cb.h>
#include <sys/types.h>
#include <sys/systm.h>

#include <linux/netlink.h>


/* Modified from: https://elixir.bootlin.com/linux/latest/source/include/net/netlink.h
 * ========================================================================
 *         Netlink Messages and Attributes Interface 
 * ------------------------------------------------------------------------
 *                          Messages Interface
 * ------------------------------------------------------------------------
 *
 * Message Format:
 *    <--- nlmsg_total_size(payload)  --->
 *    <-- nlmsg_msg_size(payload) ->
 *   +----------+- - -+-------------+- - -+-------- - -
 *   | nlmsghdr | Pad |   Payload   | Pad | nlmsghdr
 *   +----------+- - -+-------------+- - -+-------- - -
 *   nlmsg_data(nlh)---^            ^
 *   nl_data_end_ptr(m)-------------+
 *   ^------nl_nlmsghdr(m)       
 *   <-nl_message_length(offset, m)-> 
 * Payload Format:
 *    <---------------------- nlmsg_len(nlh) --------------------->
 *    <------ hdrlen ------>       <- nlmsg_attrlen(nlh, hdrlen) ->
 *   +----------------------+- - -+--------------------------------+
 *   |     Family Header    | Pad |           Attributes           |
 *   +----------------------+- - -+--------------------------------+
 *   nlmsg_attrdata(nlh, hdrlen)---^
 */
/*
 *  <------- NLA_HDRLEN ------> <-- NLA_ALIGN(payload)-->
 * +---------------------+- - -+- - - - - - - - - -+- - -+
 * |        Header       | Pad |     Payload       | Pad |
 * |   (struct nlattr)   | ing |                   | ing |
 * +---------------------+- - -+- - - - - - - - - -+- - -+
 *  <-------------- nlattr->nla_len -------------->
 */

//TODO: Change to max netlink number
#define NL_MAX_HANDLERS 100
typedef int (*nl_handler)(void *data, struct socket *so);

int 
nl_register_or_replace_handler(int proto, nl_handler handle);

/*---- nlmsg helpers ----*/
static inline int
nlmsg_msg_size(int payload) {
	return NLMSG_HDRLEN + payload;
}

static inline int
nlmsg_aligned_msg_size(int payload) {
	return NLMSG_ALIGN(nlmsg_msg_size(payload));
}
static inline void *
nlmsg_data(struct nlmsghdr *nlh)
{
	return (unsigned char *) nlh + NLMSG_HDRLEN;
}


static inline int
nlmsg_len(const struct nlmsghdr *nlh)
{
	return nlh->nlmsg_len - NLMSG_HDRLEN;
}

void *
nl_data_end_ptr(struct mbuf * m);

static inline struct mbuf *
nlmsg_new(int payload, int flags)
{
	int size = nlmsg_aligned_msg_size(payload);
	struct mbuf * m = m_getm(NULL, size, flags, MT_DATA);
	//flags specify M_WAITOK or M_WAITNOTOK
	bzero(mtod(m, caddr_t), size);
	return m;
}

static inline int
nlmsg_end(struct mbuf *m, struct nlmsghdr *nlh) {
	nlh->nlmsg_len = (char*)nl_data_end_ptr(m) - (char*) nlh;
	return nlh->nlmsg_len;
}



/*TODO: Put inline back*/

// Places fields in nlmsghdr at the start of buffer 
static struct nlmsghdr *
nlmsg_put(struct mbuf* m, int portid, int seq, int type, int payload, int flags)
{
	struct nlmsghdr *nlh;
	int size = nlmsg_msg_size(payload);
	nlh = mtod(m, struct nlmsghdr *);
	if (nlh == NULL) {
		printf("Error at mtod");
		return NULL;
	}
	nlh->nlmsg_type = type;
	nlh->nlmsg_len = size;
	nlh->nlmsg_pid = portid;
	nlh->nlmsg_seq = seq;
	
	m->m_len += NLMSG_ALIGN(size);
	m->m_pkthdr.len += NLMSG_ALIGN(size);

	if (NLMSG_ALIGN(size) - size != 0)
		memset((char*)nlmsg_data(nlh) + payload, 0, NLMSG_ALIGN(size) - size);
	return nlh;
}




/*---- end nlmsg helpers ----*/
struct nlpcb {
	struct rawcb rp; /*rawcb*/
	uint32_t			portid;
	uint32_t			dst_portid;
	uint32_t			dst_group;
	uint32_t			flags;
};
#define sotonlpcb(so)       ((struct nlpcb *)(so)->so_pcb)

#define _M_NLPROTO(m)  ((m)->m_pkthdr.rsstype)  /* netlink proto, 8 bit */
#define NETISR_NETLINK  15  // XXX hack, must be unused and < 16

 /**
  * Standard attribute types to specify validation policy
  */
enum {
	NLA_UNSPEC,
	NLA_U8,
	NLA_U16,
	NLA_U32,
	NLA_U64,
	NLA_S8,
	NLA_S16,
	NLA_S32,
	NLA_S64,
	NLA_STRING,
	NLA_FLAG,
	NLA_REJECT,
	NLA_NESTED,
	NLA_NESTED_ARRAY,
	NLA_NUL_STRING,
	__NLA_TYPE_MAX,
};
#define NLA_TYPE_MAX (__NLA_TYPE_MAX - 1)
struct nla_policy {
    uint16_t        type;
    uint16_t        len;
    struct nla_policy *nested_policy;

};

static const uint8_t nla_attr_len[NLA_TYPE_MAX+1] = {
	[NLA_U8]	= sizeof(uint8_t),
	[NLA_U16]	= sizeof(uint16_t),
	[NLA_U32]	= sizeof(uint32_t),
	[NLA_U64]	= sizeof(uint64_t),
	[NLA_S8]	= sizeof(int8_t),
	[NLA_S16]	= sizeof(int16_t),
	[NLA_S32]	= sizeof(int32_t),
	[NLA_S64]	= sizeof(int64_t),
};

static const uint8_t nla_attr_minlen[NLA_TYPE_MAX+1] = {
	[NLA_U8]	= sizeof(uint8_t),
	[NLA_U16]	= sizeof(uint16_t),
	[NLA_U32]	= sizeof(uint32_t),
	[NLA_U64]	= sizeof(uint64_t),
	//[NLA_MSECS]	= sizeof(uint64_t),
	[NLA_S8]	= sizeof(int8_t),
	[NLA_S16]	= sizeof(int16_t),
	[NLA_S32]	= sizeof(int32_t),
	[NLA_S64]	= sizeof(int64_t),
};
#define NLA_ALIGNTO		4
#define NLA_ALIGN(len)		(((len) + NLA_ALIGNTO - 1) & ~(NLA_ALIGNTO - 1))
#define NLA_HDRLEN		((int) NLA_ALIGN(sizeof(struct nlattr)))

/**
 * nla_for_each_attr - iterate over a stream of attributes
 * @pos: loop counter, set to current attribute
 * @head: head of attribute stream
 * @len: length of attribute stream
 * @rem: initialized to len, holds bytes currently remaining in stream
 */
#define nla_for_each_attribute(pos, head, len, rem) \
	for (pos = head, rem = len; \
	     nla_ok(pos, rem); \
	     pos = nla_next(pos, &(rem)))

/**
 * nla_for_each_nested - iterate over nested attributes
 * @pos: loop counter, set to current attribute
 * @nla: attribute containing the nested attributes
 * @rem: initialized to len, holds bytes currently remaining in stream
 */
#define nla_for_each_nested(pos, nla, rem) \
	nla_for_each_attr(pos, nla_data(nla), nla_len(nla), rem)


#define MAX_POLICY_RECURSION_DEPTH 10

	int nl_send_msg(struct mbuf *m);
#endif
