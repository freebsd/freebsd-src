
/**************************************************************************

Copyright (c) 2007, Chelsio Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Neither the name of the Chelsio Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.


$FreeBSD: src/sys/dev/cxgb/ulp/tom/cxgb_defs.h,v 1.4.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $

***************************************************************************/
#ifndef CXGB_DEFS_H_
#define CXGB_DEFS_H_

#define VALIDATE_TID 0

#define TOEPCB(so)  ((struct toepcb *)(sototcpcb((so))->t_toe))
#define TOE_DEV(so) (TOEPCB((so))->tp_toedev)
#define toeptoso(toep) ((toep)->tp_tp->t_inpcb->inp_socket)
#define sototoep(so) (sototcpcb((so))->t_toe)

#define TRACE_ENTER printf("%s:%s entered\n", __FUNCTION__, __FILE__)
#define TRACE_EXIT printf("%s:%s:%d exited\n", __FUNCTION__, __FILE__, __LINE__)
	
#define	KTR_TOM	KTR_SPARE2
#define	KTR_TCB	KTR_SPARE3

struct toepcb;
struct listen_ctx;

typedef void (*defer_handler_t)(struct toedev *dev, struct mbuf *m);

void t3tom_register_cpl_handler(unsigned int opcode, cxgb_cpl_handler_func h);
void t3_listen_start(struct toedev *dev, struct socket *so, struct t3cdev *cdev);
void t3_listen_stop(struct toedev *dev, struct socket *so, struct t3cdev *cdev);
int t3_push_frames(struct socket *so, int req_completion);
int t3_connect(struct toedev *tdev, struct socket *so, struct rtentry *rt,
	struct sockaddr *nam);
void t3_init_listen_cpl_handlers(void);
int t3_init_cpl_io(void);
void t3_init_wr_tab(unsigned int wr_len);
uint32_t t3_send_rx_credits(struct tcpcb *tp, uint32_t credits, uint32_t dack, int nofail);
void t3_send_rx_modulate(struct toepcb *toep);
void t3_cleanup_rbuf(struct tcpcb *tp, int copied);

void t3_init_socket_ops(void);
void t3_install_socket_ops(struct socket *so);


void t3_disconnect_acceptq(struct socket *listen_so);
void t3_reset_synq(struct listen_ctx *ctx);
void t3_defer_reply(struct mbuf *m, struct toedev *dev, defer_handler_t handler);

struct toepcb *toepcb_alloc(void);
void toepcb_hold(struct toepcb *);
void toepcb_release(struct toepcb *);
void toepcb_init(struct toepcb *);

void t3_set_rcv_coalesce_enable(struct toepcb *toep, int on_off);
void t3_set_dack_mss(struct toepcb *toep, int on);
void t3_set_keepalive(struct toepcb *toep, int on_off);
void t3_set_ddp_tag(struct toepcb *toep, int buf_idx, unsigned int tag);
void t3_set_ddp_buf(struct toepcb *toep, int buf_idx, unsigned int offset,
		    unsigned int len);
int t3_get_tcb(struct toepcb *toep);

int t3_ctloutput(struct socket *so, struct sockopt *sopt);

#endif
