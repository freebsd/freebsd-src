/*
 * Copyright (c) 1993 Daniel Boulet
 * Copyright (c) 1994 Ugen J.S.Antsilevich
 *
 * Redistribution and use in source forms, with and without modification,
 * are permitted provided that this entire comment appears intact.
 *
 * Redistribution in binary form may occur without any restrictions.
 * Obviously, it would be nice if you gave credit where credit is due
 * but requiring it would be too onerous.
 *
 * This software is provided ``AS IS'' without any warranties of any kind.
 *
 * $FreeBSD$
 */

#ifndef _IP_FW_H
#define _IP_FW_H

#include <sys/queue.h>

/*
 * This union structure identifies an interface, either explicitly
 * by name or implicitly by IP address. The flags IP_FW_F_IIFNAME
 * and IP_FW_F_OIFNAME say how to interpret this structure. An
 * interface unit number of -1 matches any unit number, while an
 * IP address of 0.0.0.0 indicates matches any interface.
 *
 * The receive and transmit interfaces are only compared against the
 * the packet if the corresponding bit (IP_FW_F_IIFACE or IP_FW_F_OIFACE)
 * is set. Note some packets lack a receive or transmit interface
 * (in which case the missing "interface" never matches).
 */

union ip_fw_if {
	struct in_addr	fu_via_ip;	/* Specified by IP address */
	struct {			/* Specified by interface name */
#define FW_IFNLEN	10		/* need room ! was IFNAMSIZ */
		char	name[FW_IFNLEN];
		short	unit;		/* -1 means match any unit */
	} fu_via_if;
};

/*
 * Format of an IP firewall descriptor
 *
 * fw_src, fw_dst, fw_smsk, fw_dmsk are always stored in network byte order.
 * fw_flg and fw_n*p are stored in host byte order (of course).
 * Port numbers are stored in HOST byte order.
 */

struct ip_fw {
	LIST_ENTRY(ip_fw) next;		/* bidirectional list of rules */
	u_int		fw_flg;		/* Operational Flags word */
	u_int64_t	fw_pcnt;	/* Packet counters */
	u_int64_t	fw_bcnt;	/* Byte counters */
	struct in_addr	fw_src;		/* Source IP address */
	struct in_addr	fw_dst;		/* Destination IP address */
	struct in_addr	fw_smsk;	/* Mask for source IP address */
	struct in_addr	fw_dmsk;	/* Mask for destination address */
	u_short		fw_number;	/* Rule number */
	u_char		fw_prot;	/* IP protocol */
#if 1
	u_char		fw_nports;	/* # of src/dst port in array */
#define	IP_FW_GETNSRCP(rule)		((rule)->fw_nports & 0x0f)
#define	IP_FW_SETNSRCP(rule, n)		do {				\
					    (rule)->fw_nports &= ~0x0f;	\
					    (rule)->fw_nports |= (n);	\
					} while (0)
#define	IP_FW_GETNDSTP(rule)		((rule)->fw_nports >> 4)
#define	IP_FW_SETNDSTP(rule, n)		do {				  \
					    (rule)->fw_nports &= ~0xf0;	  \
					    (rule)->fw_nports |= (n) << 4;\
					} while (0)
#define	IP_FW_HAVEPORTS(rule)		((rule)->fw_nports != 0)
#else
	u_char		__pad[1];
	u_int		_nsrcp;
	u_int		_ndstp;
#define	IP_FW_GETNSRCP(rule)		(rule)->_nsrcp
#define	IP_FW_SETNSRCP(rule,n)		(rule)->_nsrcp = n
#define	IP_FW_GETNDSTP(rule)		(rule)->_ndstp
#define	IP_FW_SETNDSTP(rule,n)		(rule)->_ndstp = n
#define	IP_FW_HAVEPORTS(rule)		((rule)->_ndstp + (rule)->_nsrcp != 0)
#endif
#define	IP_FW_MAX_PORTS	10		/* A reasonable maximum */
    union {
	u_short		fw_pts[IP_FW_MAX_PORTS]; /* port numbers to match */
#define	IP_FW_ICMPTYPES_MAX	128
#define	IP_FW_ICMPTYPES_DIM	(IP_FW_ICMPTYPES_MAX / (sizeof(unsigned) * 8))
	unsigned	fw_icmptypes[IP_FW_ICMPTYPES_DIM]; /*ICMP types bitmap*/
    } fw_uar;

	u_int		fw_ipflg;	/* IP flags word */
	u_short		fw_iplen;	/* IP length */
	u_short		fw_ipid;	/* Identification */
	u_char		fw_ipopt;	/* IP options set */
	u_char		fw_ipnopt;	/* IP options unset */
	u_char		fw_iptos;	/* IP type of service set */
	u_char		fw_ipntos;	/* IP type of service unset */
	u_char		fw_ipttl;	/* IP time to live */
	u_int		fw_ipver:4;	/* IP version */
	u_char		fw_tcpopt;	/* TCP options set */
	u_char		fw_tcpnopt;	/* TCP options unset */
	u_char		fw_tcpf;	/* TCP flags set */
	u_char		fw_tcpnf;	/* TCP flags unset */
	u_short		fw_tcpwin;	/* TCP window size */
	u_int32_t	fw_tcpseq;	/* TCP sequence */
	u_int32_t	fw_tcpack;	/* TCP acknowledgement */
	long		timestamp;	/* timestamp (tv_sec) of last match */
	union ip_fw_if	fw_in_if;	/* Incoming interfaces */
	union ip_fw_if	fw_out_if;	/* Outgoing interfaces */
    union {
	u_short		fu_divert_port;	/* Divert/tee port (options IPDIVERT) */
	u_short		fu_pipe_nr;	/* queue number (option DUMMYNET) */
	u_short		fu_skipto_rule;	/* SKIPTO command rule number */
	u_short		fu_reject_code;	/* REJECT response code */
	struct sockaddr_in fu_fwd_ip;
    } fw_un;
	void		*pipe_ptr;	/* flow_set ptr for dummynet pipe */
	void		*next_rule_ptr;	/* next rule in case of match */
	uid_t		fw_uid;		/* uid to match */
	gid_t		fw_gid;		/* gid to match */
	int		fw_logamount;	/* amount to log */
	u_int64_t	fw_loghighest;	/* highest number packet to log */

	long		dont_match_prob; /* 0x7fffffff means 1.0, always fail */
	u_char		dyn_type;	/* type for dynamic rule */

#define	DYN_KEEP_STATE	0	/* type for keep-state rules	*/
#define	DYN_LIMIT	1	/* type for limit connection rules */
#define	DYN_LIMIT_PARENT 2	/* parent entry for limit connection rules */

	/* following two fields are used to limit number of connections
	 * basing on either src, srcport, dst, dstport.
	 */
	u_char		limit_mask;	/* mask type for limit rule, can
					 * have many.
					 */
#define	DYN_SRC_ADDR	0x1
#define	DYN_SRC_PORT	0x2
#define	DYN_DST_ADDR	0x4
#define	DYN_DST_PORT	0x8

	u_short		conn_limit;	/* # of connections for limit rule */
};

#define	fw_divert_port	fw_un.fu_divert_port
#define	fw_skipto_rule	fw_un.fu_skipto_rule
#define	fw_reject_code	fw_un.fu_reject_code
#define	fw_pipe_nr	fw_un.fu_pipe_nr
#define	fw_fwd_ip	fw_un.fu_fwd_ip

/**
 *
 *   rule_ptr  -------------+
 *                          V
 *     [ next.le_next ]---->[ next.le_next ]---- [ next.le_next ]--->
 *     [ next.le_prev ]<----[ next.le_prev ]<----[ next.le_prev ]<---
 *     [ <ip_fw> body ]     [ <ip_fw> body ]     [ <ip_fw> body ]
 *
 */

/*
 * Flow mask/flow id for each queue.
 */
struct ipfw_flow_id {
	u_int32_t	dst_ip;
	u_int32_t	src_ip;
	u_int16_t	dst_port;
	u_int16_t	src_port;
	u_int8_t	proto;
	u_int8_t	flags;	/* protocol-specific flags */
};

/*
 * dynamic ipfw rule
 */
struct ipfw_dyn_rule {
	struct ipfw_dyn_rule *next;
	struct ipfw_flow_id id;		/* (masked) flow id */
	struct ip_fw	*rule;		/* pointer to rule */
	struct ipfw_dyn_rule *parent;	/* pointer to parent rule */
	u_int32_t	expire;		/* expire time */
	u_int64_t	pcnt;		/* packet match counters */
	u_int64_t	bcnt;		/* byte match counters */
	u_int32_t	bucket;		/* which bucket in hash table */
	u_int32_t	state;		/* state of this rule (typically a
					 * combination of TCP flags)
					 */
	u_int16_t	dyn_type;	 /* rule type */
	u_int16_t	count;		/* refcount */
};

/*
 * Values for "flags" field .
 */
#define	IP_FW_F_COMMAND	0x000000ff	/* Mask for type of chain entry: */
#define	IP_FW_F_DENY	0x00000000	/* This is a deny rule */
#define	IP_FW_F_REJECT	0x00000001	/* Deny and send a response packet */
#define	IP_FW_F_ACCEPT	0x00000002	/* This is an accept rule */
#define	IP_FW_F_COUNT	0x00000003	/* This is a count rule */
#define	IP_FW_F_DIVERT	0x00000004	/* This is a divert rul */
#define	IP_FW_F_TEE	0x00000005	/* This is a tee rule */
#define	IP_FW_F_SKIPTO	0x00000006	/* This is a skipto rule */
#define	IP_FW_F_FWD	0x00000007	/* This is a "change forwarding
					 * address" rule */
#define	IP_FW_F_PIPE	0x00000008	/* This is a dummynet rule */
#define	IP_FW_F_QUEUE	0x00000009	/* This is a dummynet queue */

#define	IP_FW_F_IN	0x00000100	/* Check inbound packets */
#define	IP_FW_F_OUT	0x00000200	/* Check outbound packets */
#define	IP_FW_F_IIFACE	0x00000400	/* Apply inbound interface test */
#define	IP_FW_F_OIFACE	0x00000800	/* Apply outbound interface test */
#define	IP_FW_F_PRN	0x00001000	/* Print if this rule matches */
#define	IP_FW_F_SRNG	0x00002000	/* The first two src ports are a min
					 * and max range (stored in host byte
					 * order).
					 */
#define	IP_FW_F_DRNG	0x00004000	/* The first two dst ports are a min
					 * and max range (stored in host byte
					 * order).
					 */
#define	IP_FW_F_FRAG	0x00008000	/* Fragment */
#define	IP_FW_F_IIFNAME	0x00010000	/* In interface by name/unit (not IP) */
#define	IP_FW_F_OIFNAME	0x00020000	/* Out interface by name/unit (not IP)*/
#define	IP_FW_F_INVSRC	0x00040000	/* Invert sense of src check */
#define	IP_FW_F_INVDST	0x00080000	/* Invert sense of dst check */
#define	IP_FW_F_ICMPBIT	0x00100000	/* ICMP type bitmap is valid */
#define	IP_FW_F_UID	0x00200000	/* filter by uid */
#define	IP_FW_F_GID	0x00400000	/* filter by gid */
#define	IP_FW_F_RND_MATCH 0x00800000	/* probabilistic rule match */
#define	IP_FW_F_SMSK	0x01000000	/* src-port + mask */
#define	IP_FW_F_DMSK	0x02000000	/* dst-port + mask */
#define	IP_FW_BRIDGED	0x04000000	/* only match bridged packets */
#define	IP_FW_F_KEEP_S	0x08000000	/* keep state */
#define	IP_FW_F_CHECK_S	0x10000000	/* check state */
#define	IP_FW_F_SME	0x20000000	/* source = me */
#define	IP_FW_F_DME	0x40000000	/* destination = me */

#define	IP_FW_F_MASK	0x7FFFFFFF	/* All possible flag bits mask */

/* 
 * Flags for the 'fw_ipflg' field, for comparing values
 * of ip and its protocols.
 */
#define	IP_FW_IF_TCPOPT	0x00000001	/* tcp options */
#define	IP_FW_IF_TCPFLG	0x00000002	/* tcp flags */
#define	IP_FW_IF_TCPSEQ	0x00000004	/* tcp sequence number */
#define	IP_FW_IF_TCPACK	0x00000008	/* tcp acknowledgement number */
#define	IP_FW_IF_TCPWIN	0x00000010	/* tcp window size */
#define	IP_FW_IF_TCPEST	0x00000020	/* established TCP connection */
#define	IP_FW_IF_TCPMSK	0x0000003f	/* mask of all tcp values */
#define	IP_FW_IF_IPOPT	0x00000100	/* ip options */
#define	IP_FW_IF_IPLEN	0x00000200	/* ip length */
#define	IP_FW_IF_IPID	0x00000400	/* ip identification */
#define	IP_FW_IF_IPTOS	0x00000800	/* ip type of service */
#define	IP_FW_IF_IPTTL	0x00001000	/* ip time to live */
#define	IP_FW_IF_IPVER	0x00002000	/* ip version */
#define	IP_FW_IF_IPMSK	0x00003f00	/* mask of all ip values */
#define	IP_FW_IF_MSK	0x0000ffff	/* All possible bits mask */

/*
 * For backwards compatibility with rules specifying "via iface" but
 * not restricted to only "in" or "out" packets, we define this combination
 * of bits to represent this configuration.
 */

#define	IF_FW_F_VIAHACK	(IP_FW_F_IN|IP_FW_F_OUT|IP_FW_F_IIFACE|IP_FW_F_OIFACE)

/*
 * Definitions for REJECT response codes.
 * Values less than 256 correspond to ICMP unreachable codes.
 */
#define	IP_FW_REJECT_RST	0x0100	/* TCP packets: send RST */

/*
 * Definitions for IP option names.
 */
#define	IP_FW_IPOPT_LSRR	0x01
#define	IP_FW_IPOPT_SSRR	0x02
#define	IP_FW_IPOPT_RR		0x04
#define	IP_FW_IPOPT_TS		0x08

/*
 * Definitions for TCP option names.
 */
#define	IP_FW_TCPOPT_MSS	0x01
#define	IP_FW_TCPOPT_WINDOW	0x02
#define	IP_FW_TCPOPT_SACK	0x04
#define	IP_FW_TCPOPT_TS		0x08
#define	IP_FW_TCPOPT_CC		0x10

/*
 * Definitions for TCP flags.
 */
#define	IP_FW_TCPF_FIN		TH_FIN
#define	IP_FW_TCPF_SYN		TH_SYN
#define	IP_FW_TCPF_RST		TH_RST
#define	IP_FW_TCPF_PSH		TH_PUSH
#define	IP_FW_TCPF_ACK		TH_ACK
#define	IP_FW_TCPF_URG		TH_URG

/*
 * Main firewall chains definitions and global var's definitions.
 */
#ifdef _KERNEL

#define	IP_FW_PORT_DYNT_FLAG	0x10000
#define	IP_FW_PORT_TEE_FLAG	0x20000
#define	IP_FW_PORT_DENY_FLAG	0x40000

/*
 * Function definitions.
 */
void ip_fw_init __P((void));

/* Firewall hooks */
struct ip;
struct sockopt;
typedef int ip_fw_chk_t (struct ip **, int, struct ifnet *, u_int16_t *,
    struct mbuf **, struct ip_fw **, struct sockaddr_in **);
typedef int ip_fw_ctl_t (struct sockopt *);
extern ip_fw_chk_t *ip_fw_chk_ptr;
extern ip_fw_ctl_t *ip_fw_ctl_ptr;
extern int fw_one_pass;
extern int fw_enable;
extern struct ipfw_flow_id last_pkt;
#endif /* _KERNEL */

#endif /* _IP_FW_H */
