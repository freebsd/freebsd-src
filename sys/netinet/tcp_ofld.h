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

#ifndef _NETINET_TCP_OFLD_H_
#define	_NETINET_TCP_OFLD_H_

#ifndef _KERNEL
#error "no user-serviceable parts inside"
#endif

#define	SC_ENTRY_PRESENT	1
#define	SC_DROP			2

#define	tp_offload(tp)		((tp)->t_flags & TF_TOE)
#define	SO_OFFLOADABLE(so)	((so->so_options & SO_NOOFFLOAD) == 0)

int	ofld_connect(struct socket *so, struct sockaddr *nam);
int	ofld_can_offload(struct tcpcb *tp, struct sockaddr *nam);

int	ofld_abort(struct tcpcb *tp);
int	ofld_disconnect(struct tcpcb *tp);
int	ofld_send(struct tcpcb *tp);
int	ofld_rcvd(struct tcpcb *tp);
void	ofld_detach(struct tcpcb *tp);
void	ofld_listen_close(struct tcpcb *tp);
void	ofld_listen_open(struct tcpcb *tp);

#ifndef DISABLE_TCP_OFFLOAD
static __inline int
tcp_gen_connect(struct socket *so, struct sockaddr *nam)
{
	struct tcpcb *tp = sototcpcb(so);
	int error;

	if (!SO_OFFLOADABLE(so) || (error = ofld_connect(so, nam)) != 0)
		error = tcp_output(tp);
	return (error);
}

static __inline int
tcp_gen_disconnect(struct tcpcb *tp)
{
	int error;

	if (tp_offload(tp))
		error = ofld_disconnect(tp);
	else
		error = tcp_output(tp);
	return (error);
}

static __inline int
tcp_gen_abort(struct tcpcb *tp)
{
	int error;

	if (tp_offload(tp))
		error = ofld_abort(tp);
	else
		error = tcp_output(tp);
	return (error);
}

static __inline int
tcp_gen_send(struct tcpcb *tp)
{
	int error;

	if (tp_offload(tp))
		error = ofld_send(tp);
	else
		error = tcp_output(tp);
	return (error);
}

static __inline int
tcp_gen_rcvd(struct tcpcb *tp)
{
	int error;

	if (tp_offload(tp))
		error = ofld_rcvd(tp);
	else
		error = tcp_output(tp);
	return (error);
}

static __inline void
tcp_gen_listen_open(struct tcpcb *tp)
{

	if (SO_OFFLOADABLE(tp->t_inpcb->inp_socket))
	    ofld_listen_open(tp);
}

static __inline void
tcp_gen_listen_close(struct tcpcb *tp)
{
	ofld_listen_close(tp);
}

static __inline void
tcp_gen_detach(struct tcpcb *tp)
{
	if (tp_offload(tp))
		ofld_detach(tp);
}

#else

static __inline int
tcp_gen_connect(struct socket *so, struct sockaddr *nam)
{

	return (tcp_output(tp));
}

static __inline int
tcp_gen_disconnect(struct tcpcb *tp)
{

	return (tcp_output(tp));
}

static __inline int
tcp_gen_abort(struct tcpcb *tp)
{

	return (tcp_output(tp));
}

static __inline int
tcp_gen_send(struct tcpcb *tp)
{

	return (tcp_output(tp));
}

static __inline int
tcp_gen_rcvd(struct tcpcb *tp)
{

	return (tcp_output(tp));
}

static __inline void
tcp_gen_listen_open(struct tcpcb *tp)
{
}

static __inline void
tcp_gen_listen_close(struct tcpcb *tp)
{
}

static __inline void
tcp_gen_detach(struct tcpcb *tp)
{
}

#endif

struct toe_usrreqs {
	int (*tu_send)(struct tcpcb *tp);
	int (*tu_rcvd)(struct tcpcb *tp);
	int (*tu_disconnect)(struct tcpcb *tp);
	int (*tu_abort)(struct tcpcb *tp);
	void (*tu_detach)(struct tcpcb *tp);
	void (*tu_syncache_event)(int event, void *toep);
};

#define	OFLD_LISTEN_OPEN		1
#define	OFLD_LISTEN_CLOSE		2
typedef	void	(*ofld_listen_fn)(void *, int, struct tcpcb *);
EVENTHANDLER_DECLARE(ofld_listen, ofld_listen_fn);

#endif
