/*-
 * Copyright (c) 2007-2009 Robert N. M. Watson
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _NET_NETISR_H_
#define _NET_NETISR_H_
#ifdef _KERNEL

/*
 * The netisr (network interrupt service routine) provides a deferred
 * execution evironment in which (generally inbound) network processing can
 * take place.  Protocols register handlers which will be executed directly,
 * or via deferred dispatch, depending on the circumstances.
 *
 * Historically, this was implemented by the BSD software ISR facility; it is
 * now implemented via a software ithread (SWI).
 */
#define	NETISR_POLL	0		/* polling callback, must be first */
#define	NETISR_IP	2		/* same as AF_INET */
#define	NETISR_IGMP	3		/* IGMPv3 output queue */
#define	NETISR_ROUTE	14		/* routing socket */
#define	NETISR_AARP	15		/* Appletalk ARP */
#define	NETISR_ATALK2	16		/* Appletalk phase 2 */
#define	NETISR_ATALK1	17		/* Appletalk phase 1 */
#define	NETISR_ARP	18		/* same as AF_LINK */
#define	NETISR_IPX	23		/* same as AF_IPX */
#define	NETISR_ETHER	24		/* ethernet input */
#define	NETISR_IPV6	27
#define	NETISR_NATM	28
#define	NETISR_POLLMORE	31		/* polling callback, must be last */

/*-
 * Protocols express ordering constraints and affinity preferences by
 * implementing one or neither of nh_m2flow and nh_m2cpuid, which are used by
 * netisr to determine which per-CPU workstream to assign mbufs to.
 *
 * The following policies may be used by protocols:
 *
 * NETISR_POLICY_SOURCE - netisr should maintain source ordering without
 *                        advice from the protocol.  netisr will ignore any
 *                        flow IDs present on the mbuf for the purposes of
 *                        work placement.
 *
 * NETISR_POLICY_FLOW - netisr should maintain flow ordering as defined by
 *                      the mbuf header flow ID field.  If the protocol
 *                      implements nh_m2flow, then netisr will query the
 *                      protocol in the event that the mbuf doesn't have a
 *                      flow ID, falling back on source ordering.
 *
 * NETISR_POLICY_CPU - netisr will delegate all work placement decisions to
 *                     the protocol, querying nh_m2cpuid for each packet.
 *
 * Protocols might make decisions about work placement based on an existing
 * calculated flow ID on the mbuf, such as one provided in hardware, the
 * receive interface pointed to by the mbuf (if any), the optional source
 * identifier passed at some dispatch points, or even parse packet headers to
 * calculate a flow.  Both protocol handlers may return a new mbuf pointer
 * for the chain, or NULL if the packet proves invalid or m_pullup() fails.
 *
 * XXXRW: If we eventually support dynamic reconfiguration, there should be
 * protocol handlers to notify them of CPU configuration changes so that they
 * can rebalance work.
 */
struct mbuf;
typedef void		 netisr_handler_t (struct mbuf *m);
typedef struct mbuf	*netisr_m2cpuid_t(struct mbuf *m, uintptr_t source,
			 u_int *cpuid);
typedef	struct mbuf	*netisr_m2flow_t(struct mbuf *m, uintptr_t source);

#define	NETISR_POLICY_SOURCE	1	/* Maintain source ordering. */
#define	NETISR_POLICY_FLOW	2	/* Maintain flow ordering. */
#define	NETISR_POLICY_CPU	3	/* Protocol determines CPU placement. */

/*
 * Data structure describing a protocol handler.
 */
struct netisr_handler {
	const char	*nh_name;	/* Character string protocol name. */
	netisr_handler_t *nh_handler;	/* Protocol handler. */
	netisr_m2flow_t	*nh_m2flow;	/* Query flow for untagged packet. */
	netisr_m2cpuid_t *nh_m2cpuid;	/* Query CPU to process mbuf on. */
	u_int		 nh_proto;	/* Integer protocol ID. */
	u_int		 nh_qlimit;	/* Maximum per-CPU queue depth. */
	u_int		 nh_policy;	/* Work placement policy. */
	u_int		 nh_ispare[5];	/* For future use. */
	void		*nh_pspare[4];	/* For future use. */
};

/*
 * Register, unregister, and other netisr handler management functions.
 */
void	netisr_clearqdrops(const struct netisr_handler *nhp);
void	netisr_getqdrops(const struct netisr_handler *nhp,
	    u_int64_t *qdropsp);
void	netisr_getqlimit(const struct netisr_handler *nhp, u_int *qlimitp);
void	netisr_register(const struct netisr_handler *nhp);
int	netisr_setqlimit(const struct netisr_handler *nhp, u_int qlimit);
void	netisr_unregister(const struct netisr_handler *nhp);

/*
 * Process a packet destined for a protocol, and attempt direct dispatch.
 * Supplemental source ordering information can be passed using the _src
 * variant.
 */
int	netisr_dispatch(u_int proto, struct mbuf *m);
int	netisr_dispatch_src(u_int proto, uintptr_t source, struct mbuf *m);
int	netisr_queue(u_int proto, struct mbuf *m);
int	netisr_queue_src(u_int proto, uintptr_t source, struct mbuf *m);

/*
 * Provide a default implementation of "map an ID to a CPU ID".
 */
u_int	netisr_default_flow2cpu(u_int flowid);

/*
 * Utility routines to return the number of CPUs participting in netisr, and
 * to return a mapping from a number to a CPU ID that can be used with the
 * scheduler.
 */
u_int	netisr_get_cpucount(void);
u_int	netisr_get_cpuid(u_int cpunumber);

/*
 * Interfaces between DEVICE_POLLING and netisr.
 */
void	netisr_sched_poll(void);
void	netisr_poll(void);
void	netisr_pollmore(void);

#endif /* !_KERNEL */
#endif /* !_NET_NETISR_H_ */
