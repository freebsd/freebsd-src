/*-
 * Copyright (c) 2007, Chelsio Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Neither the name of the Chelsio Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _NETINET_TCP_OFFLOAD_H_
#define	_NETINET_TCP_OFFLOAD_H_

#ifndef _KERNEL
#error "no user-serviceable parts inside"
#endif

/*
 * A driver publishes that it provides offload services
 * by setting IFCAP_TOE in the ifnet. The offload connect
 * will bypass any further work if the interface that a
 * connection would use does not support TCP offload.
 *
 * The TOE API assumes that the tcp offload engine can offload the 
 * the entire connection from set up to teardown, with some provision 
 * being made to allowing the software stack to handle time wait. If
 * the device does not meet these criteria, it is the driver's responsibility
 * to overload the functions that it needs to in tcp_usrreqs and make
 * its own calls to tcp_output if it needs to do so.
 *
 * There is currently no provision for the device advertising the congestion
 * control algorithms it supports as there is currently no API for querying 
 * an operating system for the protocols that it has loaded. This is a desirable
 * future extension.
 *
 *
 *
 * It is assumed that individuals deploying TOE will want connections
 * to be offloaded without software changes so all connections on an
 * interface providing TOE are offloaded unless the the SO_NO_OFFLOAD 
 * flag is set on the socket.
 *
 *
 * The toe_usrreqs structure constitutes the TOE driver's 
 * interface to the TCP stack for functionality that doesn't
 * interact directly with userspace. If one wants to provide
 * (optional) functionality to do zero-copy to/from
 * userspace one still needs to override soreceive/sosend 
 * with functions that fault in and pin the user buffers.
 *
 * + tu_send
 *   - tells the driver that new data may have been added to the 
 *     socket's send buffer - the driver should not fail if the
 *     buffer is in fact unchanged
 *   - the driver is responsible for providing credits (bytes in the send window)
 *     back to the socket by calling sbdrop() as segments are acknowledged.
 *   - The driver expects the inpcb lock to be held - the driver is expected
 *     not to drop the lock. Hence the driver is not allowed to acquire the
 *     pcbinfo lock during this call.
 *
 * + tu_rcvd
 *   - returns credits to the driver and triggers window updates
 *     to the peer (a credit as used here is a byte in the peer's receive window)
 *   - the driver is expected to determine how many bytes have been 
 *     consumed and credit that back to the card so that it can grow
 *     the window again by maintaining its own state between invocations.
 *   - In principle this could be used to shrink the window as well as
 *     grow the window, although it is not used for that now.
 *   - this function needs to correctly handle being called any number of
 *     times without any bytes being consumed from the receive buffer.
 *   - The driver expects the inpcb lock to be held - the driver is expected
 *     not to drop the lock. Hence the driver is not allowed to acquire the
 *     pcbinfo lock during this call.
 *
 * + tu_disconnect
 *   - tells the driver to send FIN to peer
 *   - driver is expected to send the remaining data and then do a clean half close
 *   - disconnect implies at least half-close so only send, reset, and detach
 *     are legal
 *   - the driver is expected to handle transition through the shutdown
 *     state machine and allow the stack to support SO_LINGER.
 *   - The driver expects the inpcb lock to be held - the driver is expected
 *     not to drop the lock. Hence the driver is not allowed to acquire the
 *     pcbinfo lock during this call.
 *
 * + tu_reset
 *   - closes the connection and sends a RST to peer
 *   - driver is expectd to trigger an RST and detach the toepcb
 *   - no further calls are legal after reset
 *   - The driver expects the inpcb lock to be held - the driver is expected
 *     not to drop the lock. Hence the driver is not allowed to acquire the
 *     pcbinfo lock during this call.
 *
 *   The following fields in the tcpcb are expected to be referenced by the driver:
 *	+ iss
 *	+ rcv_nxt
 *	+ rcv_wnd
 *	+ snd_isn
 *	+ snd_max
 *	+ snd_nxt
 *	+ snd_una
 *	+ t_flags
 *	+ t_inpcb
 *	+ t_maxseg
 *	+ t_toe
 *
 *   The following fields in the inpcb are expected to be referenced by the driver:
 *	+ inp_lport
 *	+ inp_fport
 *	+ inp_laddr
 *	+ inp_fport
 *	+ inp_socket
 *	+ inp_ip_tos
 *
 *   The following fields in the socket are expected to be referenced by the
 *   driver:
 *	+ so_comp
 *	+ so_error
 *	+ so_linger
 *	+ so_options
 *	+ so_rcv
 *	+ so_snd
 *	+ so_state
 *	+ so_timeo
 *
 *   These functions all return 0 on success and can return the following errors
 *   as appropriate:
 *	+ EPERM:
 *	+ ENOBUFS: memory allocation failed
 *	+ EMSGSIZE: MTU changed during the call
 *	+ EHOSTDOWN:
 *	+ EHOSTUNREACH:
 *	+ ENETDOWN:
 *	* ENETUNREACH: the peer is no longer reachable
 *
 * + tu_detach
 *   - tells driver that the socket is going away so disconnect
 *     the toepcb and free appropriate resources
 *   - allows the driver to cleanly handle the case of connection state
 *     outliving the socket
 *   - no further calls are legal after detach
 *   - the driver is expected to provide its own synchronization between
 *     detach and receiving new data.
 * 
 * + tu_syncache_event
 *   - even if it is not actually needed, the driver is expected to
 *     call syncache_add for the initial SYN and then syncache_expand
 *     for the SYN,ACK
 *   - tells driver that a connection either has not been added or has 
 *     been dropped from the syncache
 *   - the driver is expected to maintain state that lives outside the 
 *     software stack so the syncache needs to be able to notify the
 *     toe driver that the software stack is not going to create a connection
 *     for a received SYN
 *   - The driver is responsible for any synchronization required between
 *     the syncache dropping an entry and the driver processing the SYN,ACK.
 * 
 */
struct toe_usrreqs {
	int (*tu_send)(struct tcpcb *tp);
	int (*tu_rcvd)(struct tcpcb *tp);
	int (*tu_disconnect)(struct tcpcb *tp);
	int (*tu_reset)(struct tcpcb *tp);
	void (*tu_detach)(struct tcpcb *tp);
	void (*tu_syncache_event)(int event, void *toep);
};

/*
 * Proxy for struct tcpopt between TOE drivers and TCP functions.
 */
struct toeopt {
	u_int64_t	to_flags;	/* see tcpopt in tcp_var.h */
	u_int16_t	to_mss;		/* maximum segment size */
	u_int8_t	to_wscale;	/* window scaling */

	u_int8_t	_pad1;		/* explicit pad for 64bit alignment */
	u_int32_t	_pad2;		/* explicit pad for 64bit alignment */
	u_int64_t	_pad3[4];	/* TBD */
};

#define	TOE_SC_ENTRY_PRESENT		1	/* 4-tuple already present */
#define	TOE_SC_DROP			2	/* connection was timed out */

/*
 * Because listen is a one-to-many relationship (a socket can be listening 
 * on all interfaces on a machine some of which may be using different TCP
 * offload devices), listen uses a publish/subscribe mechanism. The TCP
 * offload driver registers a listen notification function with the stack.
 * When a listen socket is created all TCP offload devices are notified
 * so that they can do the appropriate set up to offload connections on the
 * port to which the socket is bound. When the listen socket is closed,
 * the offload devices are notified so that they will stop listening on that
 * port and free any associated resources as well as sending RSTs on any
 * connections in the SYN_RCVD state.
 *
 */

typedef	void	(*tcp_offload_listen_start_fn)(void *, struct tcpcb *);
typedef	void	(*tcp_offload_listen_stop_fn)(void *, struct tcpcb *);

EVENTHANDLER_DECLARE(tcp_offload_listen_start, tcp_offload_listen_start_fn);
EVENTHANDLER_DECLARE(tcp_offload_listen_stop, tcp_offload_listen_stop_fn);

/*
 * Check if the socket can be offloaded by the following steps:
 * - determine the egress interface
 * - check the interface for TOE capability and TOE is enabled
 * - check if the device has resources to offload the connection
 */
int	tcp_offload_connect(struct socket *so, struct sockaddr *nam);

/*
 * The tcp_output_* routines are wrappers around the toe_usrreqs calls
 * which trigger packet transmission. In the non-offloaded case they
 * translate to tcp_output. The tcp_offload_* routines notify TOE
 * of specific events. I the non-offloaded case they are no-ops.
 *
 * Listen is a special case because it is a 1 to many relationship
 * and there can be more than one offload driver in the system.
 */

/*
 * Connection is offloaded
 */
#define	tp_offload(tp)		((tp)->t_flags & TF_TOE)

/*
 * hackish way of allowing this file to also be included by TOE
 * which needs to be kept ignorant of socket implementation details
 */
#ifdef _SYS_SOCKETVAR_H_
/*
 * The socket has not been marked as "do not offload"
 */
#define	SO_OFFLOADABLE(so)	((so->so_options & SO_NO_OFFLOAD) == 0)

static __inline int
tcp_output_connect(struct socket *so, struct sockaddr *nam)
{
	struct tcpcb *tp = sototcpcb(so);
	int error;

	/*
	 * If offload has been disabled for this socket or the 
	 * connection cannot be offloaded just call tcp_output
	 * to start the TCP state machine.
	 */
#ifndef TCP_OFFLOAD_DISABLE	
	if (!SO_OFFLOADABLE(so) || (error = tcp_offload_connect(so, nam)) != 0)
#endif		
		error = tcp_output(tp);
	return (error);
}

static __inline int
tcp_output_send(struct tcpcb *tp)
{

#ifndef TCP_OFFLOAD_DISABLE
	if (tp_offload(tp))
		return (tp->t_tu->tu_send(tp));
#endif
	return (tcp_output(tp));
}

static __inline int
tcp_output_rcvd(struct tcpcb *tp)
{

#ifndef TCP_OFFLOAD_DISABLE
	if (tp_offload(tp))
		return (tp->t_tu->tu_rcvd(tp));
#endif
	return (tcp_output(tp));
}

static __inline int
tcp_output_disconnect(struct tcpcb *tp)
{

#ifndef TCP_OFFLOAD_DISABLE
	if (tp_offload(tp))
		return (tp->t_tu->tu_disconnect(tp));
#endif
	return (tcp_output(tp));
}

static __inline int
tcp_output_reset(struct tcpcb *tp)
{

#ifndef TCP_OFFLOAD_DISABLE
	if (tp_offload(tp))
		return (tp->t_tu->tu_reset(tp));
#endif
	return (tcp_output(tp));
}

static __inline void
tcp_offload_detach(struct tcpcb *tp)
{

#ifndef TCP_OFFLOAD_DISABLE
	if (tp_offload(tp))
		tp->t_tu->tu_detach(tp);
#endif	
}

static __inline void
tcp_offload_listen_open(struct tcpcb *tp)
{

#ifndef TCP_OFFLOAD_DISABLE
	if (SO_OFFLOADABLE(tp->t_inpcb->inp_socket))
		EVENTHANDLER_INVOKE(tcp_offload_listen_start, tp);
#endif	
}

static __inline void
tcp_offload_listen_close(struct tcpcb *tp)
{

#ifndef TCP_OFFLOAD_DISABLE
	EVENTHANDLER_INVOKE(tcp_offload_listen_stop, tp);
#endif	
}
#undef SO_OFFLOADABLE
#endif /* _SYS_SOCKETVAR_H_ */
#undef tp_offload

void tcp_offload_twstart(struct tcpcb *tp);
struct tcpcb *tcp_offload_close(struct tcpcb *tp);
struct tcpcb *tcp_offload_drop(struct tcpcb *tp, int error);

#endif /* _NETINET_TCP_OFFLOAD_H_ */
