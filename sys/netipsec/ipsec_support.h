/*-
 * Copyright (c) 2016 Andrey V. Elsukov <ae@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _NETIPSEC_IPSEC_SUPPORT_H_
#define	_NETIPSEC_IPSEC_SUPPORT_H_

#ifdef _KERNEL
#if defined(IPSEC) || defined(IPSEC_SUPPORT)
struct mbuf;
struct inpcb;
struct tcphdr;
struct sockopt;
struct sockaddr;

size_t ipsec_hdrsiz_inpcb(struct inpcb *);
int ipsec_init_pcbpolicy(struct inpcb *);
int ipsec_delete_pcbpolicy(struct inpcb *);
int ipsec_copy_pcbpolicy(struct inpcb *, struct inpcb *);
int ipsec4_pcbctl(struct inpcb *, struct sockopt *);
int ipsec6_pcbctl(struct inpcb *, struct sockopt *);

struct ipsec_support {
	int	(*input)(struct mbuf *, int, int);
	int	(*check_policy)(const struct mbuf *, struct inpcb *);
	int	(*forward)(struct mbuf *);
	int	(*output)(struct mbuf *, struct inpcb *);
	int	(*pcbctl)(struct inpcb *, struct sockopt *);
	size_t	(*hdrsize)(struct inpcb *);
	int	(*capability)(struct mbuf *, u_int);
	int	(*ctlinput)(int, struct sockaddr *, void *);
};
#define	IPSEC_CAP_OPERABLE		1
#define	IPSEC_CAP_BYPASS_FILTER		2

#ifdef TCP_SIGNATURE
extern const int tcp_ipsec_support;

int tcp_ipsec_pcbctl(struct inpcb *, struct sockopt *);
int tcp_ipsec_input(struct mbuf *, struct tcphdr *, u_char *);
int tcp_ipsec_output(struct mbuf *, struct tcphdr *, u_char *);
#define	TCPMD5_INPUT(m, ...)		tcp_ipsec_input(m, __VA_ARGS__)
#define	TCPMD5_OUTPUT(m, ...)		tcp_ipsec_output(m, __VA_ARGS__)
#define	TCPMD5_PCBCTL(inp, sopt)	tcp_ipsec_pcbctl(inp, sopt)
#else
struct tcpmd5_support {
	int	(*input)(struct mbuf *, struct tcphdr *, u_char *);
	int	(*output)(struct mbuf *, struct tcphdr *, u_char *);
	int	(*pcbctl)(struct inpcb *, struct sockopt *);
};
extern volatile int tcp_ipsec_support;
extern const struct tcpmd5_support * volatile tcp_ipsec_methods;

int tcpmd5_kmod_pcbctl(struct inpcb *, struct sockopt *);
int tcpmd5_kmod_input(struct mbuf *, struct tcphdr *, u_char *);
int tcpmd5_kmod_output(struct mbuf *, struct tcphdr *, u_char *);
#define	TCPMD5_INPUT(m, ...)		tcpmd5_kmod_input(m, __VA_ARGS__)
#define	TCPMD5_OUTPUT(m, ...)		tcpmd5_kmod_output(m, __VA_ARGS__)
#define	TCPMD5_PCBCTL(inp, sopt)	tcpmd5_kmod_pcbctl(inp, sopt)
#endif

#define	IPSEC_ENABLED(proto)		((proto ## _ipsec_support) != 0)
#define	TCPMD5_ENABLED()		(tcp_ipsec_support != 0)
#endif /* IPSEC || IPSEC_SUPPORT */

#if defined(IPSEC)

extern const int ipv4_ipsec_support;
extern const struct ipsec_support * const ipv4_ipsec_methods;

int udp_ipsec_pcbctl(struct inpcb *, struct sockopt *);
int udp_ipsec_input(struct mbuf *, int, int);
#define	UDPENCAP_INPUT(m, ...)		udp_ipsec_input(m, __VA_ARGS__)
#define	UDPENCAP_PCBCTL(inp, sopt)	udp_ipsec_pcbctl(inp, sopt)

extern const int ipv6_ipsec_support;
extern const struct ipsec_support * const ipv6_ipsec_methods;

#define	IPSEC_INPUT(proto, m, ...)		\
    (*(proto ## _ipsec_methods)->input)(m, __VA_ARGS__)
#define	IPSEC_CHECK_POLICY(proto, m, ...)	\
    (*(proto ## _ipsec_methods)->check_policy)(m, __VA_ARGS__)
#define	IPSEC_FORWARD(proto, m)		\
    (*(proto ## _ipsec_methods)->forward)(m)
#define	IPSEC_OUTPUT(proto, m, ...)		\
    (*(proto ## _ipsec_methods)->output)(m, __VA_ARGS__)
#define	IPSEC_PCBCTL(proto, m, ...)		\
    (*(proto ## _ipsec_methods)->pcbctl)(m, __VA_ARGS__)
#define	IPSEC_CAPS(proto, m, ...)		\
    (*(proto ## _ipsec_methods)->capability)(m, __VA_ARGS__)
#define	IPSEC_HDRSIZE(proto, inp)		\
    (*(proto ## _ipsec_methods)->hdrsize)(inp)

#elif defined(IPSEC_SUPPORT)

struct udpencap_support {
	int	(*input)(struct mbuf *, int, int);
	int	(*pcbctl)(struct inpcb *, struct sockopt *);
};

extern volatile int ipv4_ipsec_support;
extern const struct ipsec_support * volatile ipv4_ipsec_methods;
extern const struct udpencap_support * volatile udp_ipsec_methods;

int udpencap_kmod_pcbctl(struct inpcb *, struct sockopt *);
int udpencap_kmod_input(struct mbuf *, int, int);
#define	UDPENCAP_INPUT(m, ...)		udpencap_kmod_input(m, __VA_ARGS__)
#define	UDPENCAP_PCBCTL(inp, sopt)	udpencap_kmod_pcbctl(inp, sopt)

extern volatile int ipv6_ipsec_support;
extern const struct ipsec_support * volatile ipv6_ipsec_methods;

extern struct rmlock ipsec_kmod_lock;
int ipsec_kmod_input(const struct ipsec_support *, struct mbuf *, int, int);
int ipsec_kmod_check_policy(const struct ipsec_support *, struct mbuf *,
    struct inpcb *);
int ipsec_kmod_forward(const struct ipsec_support *, struct mbuf *);
int ipsec_kmod_output(const struct ipsec_support *, struct mbuf *,
    struct inpcb *);
int ipsec_kmod_pcbctl(const struct ipsec_support *, struct inpcb *,
    struct sockopt *);
int ipsec_kmod_capability(const struct ipsec_support *, struct mbuf *, u_int);
size_t ipsec_kmod_hdrsize(const struct ipsec_support *, struct inpcb *);

#define	IPSEC_INPUT(proto, ...)		\
    ipsec_kmod_input(proto ## _ipsec_methods, __VA_ARGS__)
#define	IPSEC_CHECK_POLICY(proto, ...)	\
    ipsec_kmod_check_policy(proto ## _ipsec_methods, __VA_ARGS__)
#define	IPSEC_FORWARD(proto, ...)	\
    ipsec_kmod_forward(proto ## _ipsec_methods, __VA_ARGS__)
#define	IPSEC_OUTPUT(proto, ...)	\
    ipsec_kmod_output(proto ## _ipsec_methods, __VA_ARGS__)
#define	IPSEC_PCBCTL(proto, ...)	\
    ipsec_kmod_pcbctl(proto ## _ipsec_methods, __VA_ARGS__)
#define	IPSEC_CAPS(proto, ...)		\
    ipsec_kmod_capability(proto ## _ipsec_methods, __VA_ARGS__)
#define	IPSEC_HDRSIZE(proto, ...)	\
    ipsec_kmod_hdrsize(proto ## _ipsec_methods, __VA_ARGS__)
#endif /* IPSEC_SUPPORT */
#endif /* _KERNEL */
#endif /* _NETIPSEC_IPSEC_SUPPORT_H_ */
