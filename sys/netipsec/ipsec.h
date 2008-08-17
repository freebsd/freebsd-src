/*	$FreeBSD$	*/
/*	$KAME: ipsec.h,v 1.53 2001/11/20 08:32:38 itojun Exp $	*/

/*-
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * IPsec controller part.
 */

#ifndef _NETIPSEC_IPSEC_H_
#define _NETIPSEC_IPSEC_H_

#if defined(_KERNEL) && !defined(_LKM) && !defined(KLD_MODULE)
#include "opt_inet.h"
#include "opt_ipsec.h"
#endif

#include <net/pfkeyv2.h>
#include <netipsec/keydb.h>

#ifdef _KERNEL

#define	IPSEC_ASSERT(_c,_m) KASSERT(_c, _m)

#define	IPSEC_IS_PRIVILEGED_SO(_so) \
	((_so)->so_cred != NULL && \
	 priv_check_cred((_so)->so_cred, PRIV_NETINET_IPSEC, 0) \
	 == 0)

/*
 * Security Policy Index
 * Ensure that both address families in the "src" and "dst" are same.
 * When the value of the ul_proto is ICMPv6, the port field in "src"
 * specifies ICMPv6 type, and the port field in "dst" specifies ICMPv6 code.
 */
struct secpolicyindex {
	u_int8_t dir;			/* direction of packet flow, see blow */
	union sockaddr_union src;	/* IP src address for SP */
	union sockaddr_union dst;	/* IP dst address for SP */
	u_int8_t prefs;			/* prefix length in bits for src */
	u_int8_t prefd;			/* prefix length in bits for dst */
	u_int16_t ul_proto;		/* upper layer Protocol */
#ifdef notyet
	uid_t uids;
	uid_t uidd;
	gid_t gids;
	gid_t gidd;
#endif
};

/* Security Policy Data Base */
struct secpolicy {
	LIST_ENTRY(secpolicy) chain;
	struct mtx lock;

	u_int refcnt;			/* reference count */
	struct secpolicyindex spidx;	/* selector */
	u_int32_t id;			/* It's unique number on the system. */
	u_int state;			/* 0: dead, others: alive */
#define IPSEC_SPSTATE_DEAD	0
#define IPSEC_SPSTATE_ALIVE	1
	u_int16_t policy;		/* policy_type per pfkeyv2.h */
	u_int16_t scangen;		/* scan generation # */
	struct ipsecrequest *req;
				/* pointer to the ipsec request tree, */
				/* if policy == IPSEC else this value == NULL.*/

	/*
	 * lifetime handler.
	 * the policy can be used without limitiation if both lifetime and
	 * validtime are zero.
	 * "lifetime" is passed by sadb_lifetime.sadb_lifetime_addtime.
	 * "validtime" is passed by sadb_lifetime.sadb_lifetime_usetime.
	 */
	time_t created;		/* time created the policy */
	time_t lastused;	/* updated every when kernel sends a packet */
	long lifetime;		/* duration of the lifetime of this policy */
	long validtime;		/* duration this policy is valid without use */
};

#define	SECPOLICY_LOCK_INIT(_sp) \
	mtx_init(&(_sp)->lock, "ipsec policy", NULL, MTX_DEF)
#define	SECPOLICY_LOCK(_sp)		mtx_lock(&(_sp)->lock)
#define	SECPOLICY_UNLOCK(_sp)		mtx_unlock(&(_sp)->lock)
#define	SECPOLICY_LOCK_DESTROY(_sp)	mtx_destroy(&(_sp)->lock)
#define	SECPOLICY_LOCK_ASSERT(_sp)	mtx_assert(&(_sp)->lock, MA_OWNED)

/* Request for IPsec */
struct ipsecrequest {
	struct ipsecrequest *next;
				/* pointer to next structure */
				/* If NULL, it means the end of chain. */
	struct secasindex saidx;/* hint for search proper SA */
				/* if __ss_len == 0 then no address specified.*/
	u_int level;		/* IPsec level defined below. */

	struct secasvar *sav;	/* place holder of SA for use */
	struct secpolicy *sp;	/* back pointer to SP */
	struct mtx lock;	/* to interlock updates */
};

/*
 * Need recursion for when crypto callbacks happen directly,
 * as in the case of software crypto.  Need to look at how
 * hard it is to remove this...
 */
#define	IPSECREQUEST_LOCK_INIT(_isr) \
	mtx_init(&(_isr)->lock, "ipsec request", NULL, MTX_DEF | MTX_RECURSE)
#define	IPSECREQUEST_LOCK(_isr)		mtx_lock(&(_isr)->lock)
#define	IPSECREQUEST_UNLOCK(_isr)	mtx_unlock(&(_isr)->lock)
#define	IPSECREQUEST_LOCK_DESTROY(_isr)	mtx_destroy(&(_isr)->lock)
#define	IPSECREQUEST_LOCK_ASSERT(_isr)	mtx_assert(&(_isr)->lock, MA_OWNED)

/* security policy in PCB */
struct inpcbpolicy {
	struct secpolicy *sp_in;
	struct secpolicy *sp_out;
	int priv;			/* privileged socket ? */
};

/* SP acquiring list table. */
struct secspacq {
	LIST_ENTRY(secspacq) chain;

	struct secpolicyindex spidx;

	time_t created;		/* for lifetime */
	int count;		/* for lifetime */
	/* XXX: here is mbuf place holder to be sent ? */
};
#endif /* _KERNEL */

/* according to IANA assignment, port 0x0000 and proto 0xff are reserved. */
#define IPSEC_PORT_ANY		0
#define IPSEC_ULPROTO_ANY	255
#define IPSEC_PROTO_ANY		255

/* mode of security protocol */
/* NOTE: DON'T use IPSEC_MODE_ANY at SPD.  It's only use in SAD */
#define	IPSEC_MODE_ANY		0	/* i.e. wildcard. */
#define	IPSEC_MODE_TRANSPORT	1
#define	IPSEC_MODE_TUNNEL	2
#define	IPSEC_MODE_TCPMD5	3	/* TCP MD5 mode */

/*
 * Direction of security policy.
 * NOTE: Since INVALID is used just as flag.
 * The other are used for loop counter too.
 */
#define IPSEC_DIR_ANY		0
#define IPSEC_DIR_INBOUND	1
#define IPSEC_DIR_OUTBOUND	2
#define IPSEC_DIR_MAX		3
#define IPSEC_DIR_INVALID	4

/* Policy level */
/*
 * IPSEC, ENTRUST and BYPASS are allowed for setsockopt() in PCB,
 * DISCARD, IPSEC and NONE are allowed for setkey() in SPD.
 * DISCARD and NONE are allowed for system default.
 */
#define IPSEC_POLICY_DISCARD	0	/* discarding packet */
#define IPSEC_POLICY_NONE	1	/* through IPsec engine */
#define IPSEC_POLICY_IPSEC	2	/* do IPsec */
#define IPSEC_POLICY_ENTRUST	3	/* consulting SPD if present. */
#define IPSEC_POLICY_BYPASS	4	/* only for privileged socket. */

/* Security protocol level */
#define	IPSEC_LEVEL_DEFAULT	0	/* reference to system default */
#define	IPSEC_LEVEL_USE		1	/* use SA if present. */
#define	IPSEC_LEVEL_REQUIRE	2	/* require SA. */
#define	IPSEC_LEVEL_UNIQUE	3	/* unique SA. */

#define IPSEC_MANUAL_REQID_MAX	0x3fff
				/*
				 * if security policy level == unique, this id
				 * indicate to a relative SA for use, else is
				 * zero.
				 * 1 - 0x3fff are reserved for manual keying.
				 * 0 are reserved for above reason.  Others is
				 * for kernel use.
				 * Note that this id doesn't identify SA
				 * by only itself.
				 */
#define IPSEC_REPLAYWSIZE  32

/* statistics for ipsec processing */
struct ipsecstat {
	u_quad_t in_success;  /* succeeded inbound process */
	u_quad_t in_polvio;
			/* security policy violation for inbound process */
	u_quad_t in_nosa;     /* inbound SA is unavailable */
	u_quad_t in_inval;    /* inbound processing failed due to EINVAL */
	u_quad_t in_nomem;    /* inbound processing failed due to ENOBUFS */
	u_quad_t in_badspi;   /* failed getting a SPI */
	u_quad_t in_ahreplay; /* AH replay check failed */
	u_quad_t in_espreplay; /* ESP replay check failed */
	u_quad_t in_ahauthsucc; /* AH authentication success */
	u_quad_t in_ahauthfail; /* AH authentication failure */
	u_quad_t in_espauthsucc; /* ESP authentication success */
	u_quad_t in_espauthfail; /* ESP authentication failure */
	u_quad_t in_esphist[256];
	u_quad_t in_ahhist[256];
	u_quad_t in_comphist[256];
	u_quad_t out_success; /* succeeded outbound process */
	u_quad_t out_polvio;
			/* security policy violation for outbound process */
	u_quad_t out_nosa;    /* outbound SA is unavailable */
	u_quad_t out_inval;   /* outbound process failed due to EINVAL */
	u_quad_t out_nomem;    /* inbound processing failed due to ENOBUFS */
	u_quad_t out_noroute; /* there is no route */
	u_quad_t out_esphist[256];
	u_quad_t out_ahhist[256];
	u_quad_t out_comphist[256];

	u_quad_t spdcachelookup;
	u_quad_t spdcachemiss;

	u_int32_t ips_in_polvio;	/* input: sec policy violation */
	u_int32_t ips_out_polvio;	/* output: sec policy violation */
	u_int32_t ips_out_nosa;		/* output: SA unavailable  */
	u_int32_t ips_out_nomem;	/* output: no memory available */
	u_int32_t ips_out_noroute;	/* output: no route available */
	u_int32_t ips_out_inval;	/* output: generic error */
	u_int32_t ips_out_bundlesa;	/* output: bundled SA processed */
	u_int32_t ips_mbcoalesced;	/* mbufs coalesced during clone */
	u_int32_t ips_clcoalesced;	/* clusters coalesced during clone */
	u_int32_t ips_clcopied;		/* clusters copied during clone */
	u_int32_t ips_mbinserted;	/* mbufs inserted during makespace */
	/* 
	 * Temporary statistics for performance analysis.
	 */
	/* See where ESP/AH/IPCOMP header land in mbuf on input */
	u_int32_t ips_input_front;
	u_int32_t ips_input_middle;
	u_int32_t ips_input_end;
};

/*
 * Definitions for IPsec & Key sysctl operations.
 */
/*
 * Names for IPsec & Key sysctl objects
 */
#define IPSECCTL_STATS			1	/* stats */
#define IPSECCTL_DEF_POLICY		2
#define IPSECCTL_DEF_ESP_TRANSLEV	3	/* int; ESP transport mode */
#define IPSECCTL_DEF_ESP_NETLEV		4	/* int; ESP tunnel mode */
#define IPSECCTL_DEF_AH_TRANSLEV	5	/* int; AH transport mode */
#define IPSECCTL_DEF_AH_NETLEV		6	/* int; AH tunnel mode */
#if 0	/* obsolete, do not reuse */
#define IPSECCTL_INBOUND_CALL_IKE	7
#endif
#define	IPSECCTL_AH_CLEARTOS		8
#define	IPSECCTL_AH_OFFSETMASK		9
#define	IPSECCTL_DFBIT			10
#define	IPSECCTL_ECN			11
#define	IPSECCTL_DEBUG			12
#define	IPSECCTL_ESP_RANDPAD		13
#define IPSECCTL_MAXID			14

#define IPSECCTL_NAMES { \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ "def_policy", CTLTYPE_INT }, \
	{ "esp_trans_deflev", CTLTYPE_INT }, \
	{ "esp_net_deflev", CTLTYPE_INT }, \
	{ "ah_trans_deflev", CTLTYPE_INT }, \
	{ "ah_net_deflev", CTLTYPE_INT }, \
	{ 0, 0 }, \
	{ "ah_cleartos", CTLTYPE_INT }, \
	{ "ah_offsetmask", CTLTYPE_INT }, \
	{ "dfbit", CTLTYPE_INT }, \
	{ "ecn", CTLTYPE_INT }, \
	{ "debug", CTLTYPE_INT }, \
	{ "esp_randpad", CTLTYPE_INT }, \
}

#define IPSEC6CTL_NAMES { \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ "def_policy", CTLTYPE_INT }, \
	{ "esp_trans_deflev", CTLTYPE_INT }, \
	{ "esp_net_deflev", CTLTYPE_INT }, \
	{ "ah_trans_deflev", CTLTYPE_INT }, \
	{ "ah_net_deflev", CTLTYPE_INT }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ "ecn", CTLTYPE_INT }, \
	{ "debug", CTLTYPE_INT }, \
	{ "esp_randpad", CTLTYPE_INT }, \
}

#ifdef _KERNEL
struct ipsec_output_state {
	struct mbuf *m;
	struct route *ro;
	struct sockaddr *dst;
};

struct ipsec_history {
	int ih_proto;
	u_int32_t ih_spi;
};

extern int ipsec_debug;
#ifdef REGRESSION
extern int ipsec_replay;
extern int ipsec_integrity;
#endif

extern struct ipsecstat ipsec4stat;
extern struct secpolicy ip4_def_policy;
extern int ip4_esp_trans_deflev;
extern int ip4_esp_net_deflev;
extern int ip4_ah_trans_deflev;
extern int ip4_ah_net_deflev;
extern int ip4_ah_cleartos;
extern int ip4_ah_offsetmask;
extern int ip4_ipsec_dfbit;
extern int ip4_ipsec_ecn;
extern int ip4_esp_randpad;
extern int crypto_support;

#define ipseclog(x)	do { if (V_ipsec_debug) log x; } while (0)
/* for openbsd compatibility */
#define	DPRINTF(x)	do { if (V_ipsec_debug) printf x; } while (0)

extern	struct ipsecrequest *ipsec_newisr(void);
extern	void ipsec_delisr(struct ipsecrequest *);

struct tdb_ident;
extern struct secpolicy *ipsec_getpolicy __P((struct tdb_ident*, u_int));
struct inpcb;
extern struct secpolicy *ipsec4_checkpolicy __P((struct mbuf *, u_int, u_int,
	int *, struct inpcb *));
extern struct secpolicy *ipsec_getpolicybysock(struct mbuf *, u_int,
	struct inpcb *, int *);
extern struct secpolicy * ipsec_getpolicybyaddr(struct mbuf *, u_int,
	int, int *);

struct inpcb;
extern int ipsec_init_policy __P((struct socket *so, struct inpcbpolicy **));
extern int ipsec_copy_policy
	__P((struct inpcbpolicy *, struct inpcbpolicy *));
extern u_int ipsec_get_reqlevel __P((struct ipsecrequest *));
extern int ipsec_in_reject __P((struct secpolicy *, struct mbuf *));

extern int ipsec4_set_policy __P((struct inpcb *inp, int optname,
	caddr_t request, size_t len, struct ucred *cred));
extern int ipsec4_get_policy __P((struct inpcb *inpcb, caddr_t request,
	size_t len, struct mbuf **mp));
extern int ipsec4_delete_pcbpolicy __P((struct inpcb *));
extern int ipsec4_in_reject __P((struct mbuf *, struct inpcb *));

struct secas;
struct tcpcb;
extern int ipsec_chkreplay __P((u_int32_t, struct secasvar *));
extern int ipsec_updatereplay __P((u_int32_t, struct secasvar *));

extern size_t ipsec4_hdrsiz __P((struct mbuf *, u_int, struct inpcb *));
extern size_t ipsec_hdrsiz_tcp __P((struct tcpcb *));

union sockaddr_union;
extern char * ipsec_address(union sockaddr_union* sa);
extern const char *ipsec_logsastr __P((struct secasvar *));

extern void ipsec_dumpmbuf __P((struct mbuf *));

struct m_tag;
extern void ah4_input(struct mbuf *m, int off);
extern void ah4_ctlinput(int cmd, struct sockaddr *sa, void *);
extern void esp4_input(struct mbuf *m, int off);
extern void esp4_ctlinput(int cmd, struct sockaddr *sa, void *);
extern void ipcomp4_input(struct mbuf *m, int off);
extern int ipsec4_common_input(struct mbuf *m, ...);
extern int ipsec4_common_input_cb(struct mbuf *m, struct secasvar *sav,
			int skip, int protoff, struct m_tag *mt);
extern int ipsec4_process_packet __P((struct mbuf *, struct ipsecrequest *,
			int, int));
extern int ipsec_process_done __P((struct mbuf *, struct ipsecrequest *));

extern struct mbuf *ipsec_copypkt __P((struct mbuf *));

extern	void m_checkalignment(const char* where, struct mbuf *m0,
		int off, int len);
extern	struct mbuf *m_makespace(struct mbuf *m0, int skip, int hlen, int *off);
extern	caddr_t m_pad(struct mbuf *m, int n);
extern	int m_striphdr(struct mbuf *m, int skip, int hlen);

#ifdef DEV_ENC
#define	ENC_BEFORE	0x0001
#define	ENC_AFTER	0x0002
#define	ENC_IN		0x0100
#define	ENC_OUT		0x0200
extern	int ipsec_filter(struct mbuf **, int, int);
extern	void ipsec_bpf(struct mbuf *, struct secasvar *, int, int);
#endif
#endif /* _KERNEL */

#ifndef _KERNEL
extern caddr_t ipsec_set_policy __P((char *, int));
extern int ipsec_get_policylen __P((caddr_t));
extern char *ipsec_dump_policy __P((caddr_t, char *));

extern const char *ipsec_strerror __P((void));
#endif /* !_KERNEL */

#endif /* _NETIPSEC_IPSEC_H_ */
