/*-
 * Copyright (c) 1991 The Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)tp_pcb.c	7.11 (Berkeley) 5/6/91
 *	$Id: tp_pcb.c,v 1.2 1993/10/16 21:05:55 rgrimes Exp $
 */

/***********************************************************
				Copyright IBM Corporation 1987

                      All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of IBM not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
IBM BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

/*
 * ARGO Project, Computer Sciences Dept., University of Wisconsin - Madison
 */

/* 
 * ARGO TP
 *
 * This is the initialization and cleanup stuff - 
 * for the tp machine in general as well as  for the individual pcbs.
 * tp_init() is called at system startup.  tp_attach() and tp_getref() are
 * called when a socket is created.  tp_detach() and tp_freeref()
 * are called during the closing stage and/or when the reference timer 
 * goes off. 
 * tp_soisdisconnecting() and tp_soisdisconnected() are tp-specific 
 * versions of soisconnect*
 * and are called (obviously) during the closing phase.
 *
 */

#include "types.h"
#include "param.h"
#include "mbuf.h"
#include "socket.h"
#include "socketvar.h"
#include "protosw.h"
#include "errno.h"
#include "time.h"
#include "argo_debug.h"
#include "tp_param.h"
#include "tp_timer.h"
#include "tp_ip.h"
#include "tp_stat.h"
#include "tp_pcb.h"
#include "tp_tpdu.h"
#include "tp_trace.h"
#include "tp_meas.h"
#include "tp_seq.h"
#include "tp_clnp.h"

struct tp_param tp_param = {
	1,				/*  configured 		*/
};

/* ticks are in units of: 
 * 500 nano-fortnights ;-) or
 * 500 ms or 
 * 1/2 second 
 */

struct tp_conn_param tp_conn_param[] = {
	/* ISO_CLNS: TP4 CONNECTION LESS */
	{
		TP_NRETRANS, 	/* short p_Nretrans;  */
		20,		/* 10 sec */ 	/* short p_dr_ticks;  */

		20,		/* 10 sec */ 	/* short p_cc_ticks; */
		20,		/* 10 sec */ 	/* short p_dt_ticks; */

		40,		/* 20 sec */ 	/* short p_x_ticks;	 */
		80,		/* 40 sec */ 	/* short p_cr_ticks;*/

		240,	/* 2 min */ 	/* short p_keepalive_ticks;*/
		10,		/* 5 sec */ 	/* short p_sendack_ticks;  */

		600,	/* 5 min */ 	/* short p_ref_ticks;	*/
		360,	/* 3 min */ 	/* short p_inact_ticks;	*/

		(short) 100, 			/* short p_lcdtfract */
		(short) TP_SOCKBUFSIZE,	/* short p_winsize */
		TP_TPDUSIZE, 			/* u_char p_tpdusize */

		TPACK_WINDOW, 			/* 4 bits p_ack_strat */
		TPRX_USE_CW | TPRX_FASTSTART, 
								/* 4 bits p_rx_strat*/
		TP_CLASS_4 | TP_CLASS_0,/* 5 bits p_class */
		1,						/* 1 bit xtd format */
		1,						/* 1 bit xpd service */
		1,						/* 1 bit use_checksum */
		0,						/* 1 bit use net xpd */
		0,						/* 1 bit use rcc */
		0,						/* 1 bit use efc */
		1,						/* no disc indications */
		0,						/* don't change params */
		ISO_CLNS,				/* p_netservice */
	},
	/* IN_CLNS: TP4 CONNECTION LESS */
	{
		TP_NRETRANS, 	/* short p_Nretrans;  */
		20,		/* 10 sec */ 	/* short p_dr_ticks;  */

		20,		/* 10 sec */ 	/* short p_cc_ticks; */
		20,		/* 10 sec */ 	/* short p_dt_ticks; */

		40,		/* 20 sec */ 	/* short p_x_ticks;	 */
		80,		/* 40 sec */ 	/* short p_cr_ticks;*/

		240,	/* 2 min */ 	/* short p_keepalive_ticks;*/
		10,		/* 5 sec */ 	/* short p_sendack_ticks;  */

		600,	/* 5 min */ 	/* short p_ref_ticks;	*/
		360,	/* 3 min */ 	/* short p_inact_ticks;	*/

		(short) 100, 			/* short p_lcdtfract */
		(short) TP_SOCKBUFSIZE,	/* short p_winsize */
		TP_TPDUSIZE, 			/* u_char p_tpdusize */

		TPACK_WINDOW, 			/* 4 bits p_ack_strat */
		TPRX_USE_CW | TPRX_FASTSTART, 
								/* 4 bits p_rx_strat*/
		TP_CLASS_4,				/* 5 bits p_class */
		1,						/* 1 bit xtd format */
		1,						/* 1 bit xpd service */
		1,						/* 1 bit use_checksum */
		0,						/* 1 bit use net xpd */
		0,						/* 1 bit use rcc */
		0,						/* 1 bit use efc */
		1,						/* no disc indications */
		0,						/* don't change params */
		IN_CLNS,				/* p_netservice */
	},
	/* ISO_CONS: TP0 CONNECTION MODE */
	{
		TP_NRETRANS, 			/* short p_Nretrans;  */
		0,		/* n/a */		/* short p_dr_ticks; */

		40,		/* 20 sec */	/* short p_cc_ticks; */
		0,		/* n/a */		/* short p_dt_ticks; */

		0,		/* n/a */		/* short p_x_ticks;	*/
		360,	/* 3  min */	/* short p_cr_ticks;*/

		0,		/* n/a */		/* short p_keepalive_ticks;*/
		0,		/* n/a */		/* short p_sendack_ticks; */

		600,	/* for cr/cc to clear *//* short p_ref_ticks;	*/
		0,		/* n/a */		/* short p_inact_ticks;	*/

		/* Use tp4 defaults just in case the user changes ONLY
		 * the class 
		 */
		(short) 100, 			/* short p_lcdtfract */
		(short) TP0_SOCKBUFSIZE,	/* short p_winsize */
		TP0_TPDUSIZE, 			/* 8 bits p_tpdusize */

		0, 						/* 4 bits p_ack_strat */
		0, 						/* 4 bits p_rx_strat*/
		TP_CLASS_0,				/* 5 bits p_class */
		0,						/* 1 bit xtd format */
		0,						/* 1 bit xpd service */
		0,						/* 1 bit use_checksum */
		0,						/* 1 bit use net xpd */
		0,						/* 1 bit use rcc */
		0,						/* 1 bit use efc */
		0,						/* no disc indications */
		0,						/* don't change params */
		ISO_CONS,				/* p_netservice */
	},
	/* ISO_COSNS: TP4 CONNECTION LESS SERVICE over CONSNS */
	{
		TP_NRETRANS, 	/* short p_Nretrans;  */
		40,		/* 20 sec */ 	/* short p_dr_ticks;  */

		40,		/* 20 sec */ 	/* short p_cc_ticks; */
		80,		/* 40 sec */ 	/* short p_dt_ticks; */

		120,		/* 1 min */ 	/* short p_x_ticks;	 */
		360,		/* 3 min */ 	/* short p_cr_ticks;*/

		360,	/* 3 min */ 	/* short p_keepalive_ticks;*/
		20,		/* 10 sec */ 	/* short p_sendack_ticks;  */

		600,	/* 5 min */ 	/* short p_ref_ticks;	*/
		480,	/* 4 min */ 	/* short p_inact_ticks;	*/

		(short) 100, 			/* short p_lcdtfract */
		(short) TP0_SOCKBUFSIZE,	/* short p_winsize */
		TP0_TPDUSIZE, 			/* u_char p_tpdusize */

		TPACK_WINDOW, 			/* 4 bits p_ack_strat */
		TPRX_USE_CW ,			/* No fast start */ 
								/* 4 bits p_rx_strat*/
		TP_CLASS_4 | TP_CLASS_0,/* 5 bits p_class */
		0,						/* 1 bit xtd format */
		1,						/* 1 bit xpd service */
		1,						/* 1 bit use_checksum */
		0,						/* 1 bit use net xpd */
		0,						/* 1 bit use rcc */
		0,						/* 1 bit use efc */
		0,						/* no disc indications */
		0,						/* don't change params */
		ISO_COSNS,				/* p_netservice */
	},
};

#ifdef INET
int		in_putnetaddr();
int		in_getnetaddr();
int		in_cmpnetaddr();
int 	in_putsufx(); 
int 	in_getsufx(); 
int 	in_recycle_tsuffix(); 
int 	tpip_mtu(); 
int 	in_pcbbind(); 
int 	in_pcbconnect(); 
int 	in_pcbdisconnect(); 
int 	in_pcbdetach(); 
int 	in_pcballoc(); 
int 	tpip_output(); 
int 	tpip_output_dg(); 
struct inpcb	tp_inpcb;
#endif INET
#ifdef ISO
int		iso_putnetaddr();
int		iso_getnetaddr();
int		iso_cmpnetaddr();
int 	iso_putsufx(); 
int 	iso_getsufx(); 
int 	iso_recycle_tsuffix(); 
int		tpclnp_mtu(); 
int		iso_pcbbind(); 
int		iso_pcbconnect(); 
int		iso_pcbdisconnect(); 
int 	iso_pcbdetach(); 
int 	iso_pcballoc(); 
int 	tpclnp_output(); 
int 	tpclnp_output_dg(); 
int		iso_nlctloutput();
struct isopcb	tp_isopcb;
#endif ISO
#ifdef TPCONS
int		iso_putnetaddr();
int		iso_getnetaddr();
int		iso_cmpnetaddr();
int 	iso_putsufx(); 
int 	iso_getsufx(); 
int 	iso_recycle_tsuffix(); 
int		iso_pcbbind(); 
int		tpcons_pcbconnect(); 
int		tpclnp_mtu();
int		iso_pcbdisconnect(); 
int 	iso_pcbdetach(); 
int 	iso_pcballoc(); 
int 	tpcons_output(); 
struct isopcb	tp_isopcb;
#endif TPCONS


struct nl_protosw nl_protosw[] = {
	/* ISO_CLNS */
#ifdef ISO
	{ AF_ISO, iso_putnetaddr, iso_getnetaddr, iso_cmpnetaddr,
		iso_putsufx, iso_getsufx,
		iso_recycle_tsuffix,
		tpclnp_mtu, iso_pcbbind, iso_pcbconnect,
		iso_pcbdisconnect,	iso_pcbdetach,
		iso_pcballoc,
		tpclnp_output, tpclnp_output_dg, iso_nlctloutput,
		(caddr_t) &tp_isopcb,
		},
#else
	{ 0 },
#endif ISO
	/* IN_CLNS */
#ifdef INET
	{ AF_INET, in_putnetaddr, in_getnetaddr, in_cmpnetaddr,
		in_putsufx, in_getsufx,
		in_recycle_tsuffix,
		tpip_mtu, in_pcbbind, in_pcbconnect,
		in_pcbdisconnect,	in_pcbdetach,
		in_pcballoc,
		tpip_output, tpip_output_dg, /* nl_ctloutput */ NULL,
		(caddr_t) &tp_inpcb,
		},
#else
	{ 0 },
#endif INET
	/* ISO_CONS */
#if defined(ISO) && defined(TPCONS)
	{ AF_ISO, iso_putnetaddr, iso_getnetaddr, iso_cmpnetaddr,
		iso_putsufx, iso_getsufx,
		iso_recycle_tsuffix,
		tpclnp_mtu, iso_pcbbind, tpcons_pcbconnect,
		iso_pcbdisconnect,	iso_pcbdetach,
		iso_pcballoc,
		tpcons_output, tpcons_output, iso_nlctloutput,
		(caddr_t) &tp_isopcb,
		},
#else
	{ 0 },
#endif ISO_CONS
	/* End of protosw marker */
	{ 0 }
};

/*
 * NAME:  tp_init()
 *
 * CALLED FROM:
 *  autoconf through the protosw structure
 *
 * FUNCTION:
 *  initialize tp machine
 *
 * RETURNS:  Nada
 *
 * SIDE EFFECTS:
 * 
 * NOTES:
 */
int
tp_init()
{
	static int 	init_done=0;
	void	 	tp_timerinit();

	if (init_done++)
		return 0;


	/* FOR INET */
	tp_inpcb.inp_next = tp_inpcb.inp_prev = &tp_inpcb;
	/* FOR ISO */
	tp_isopcb.isop_next = tp_isopcb.isop_prev = &tp_isopcb;

    tp_start_win = 2;

	tp_timerinit();
	bzero((caddr_t)&tp_stat, sizeof(struct tp_stat));
	return 0;
}

/*
 * NAME: 	tp_soisdisconnecting()
 *
 * CALLED FROM:
 *  tp.trans
 *
 * FUNCTION and ARGUMENTS:
 *  Set state of the socket (so) to reflect that fact that we're disconnectING
 *
 * RETURNS: 	Nada
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 *  This differs from the regular soisdisconnecting() in that the latter
 *  also sets the SS_CANTRECVMORE and SS_CANTSENDMORE flags.
 *  We don't want to set those flags because those flags will cause
 *  a SIGPIPE to be delivered in sosend() and we don't like that.
 *  If anyone else is sleeping on this socket, wake 'em up.
 */
void
tp_soisdisconnecting(so)
	register struct socket *so;
{
	soisdisconnecting(so);
	so->so_state &= ~SS_CANTSENDMORE;
	IFPERF(sototpcb(so))
		register struct tp_pcb *tpcb = sototpcb(so);
		u_int 	fsufx, lsufx;

		bcopy ((caddr_t)tpcb->tp_fsuffix, (caddr_t)&fsufx, sizeof(u_int) );
		bcopy ((caddr_t)tpcb->tp_lsuffix, (caddr_t)&lsufx, sizeof(u_int) );

		tpmeas(tpcb->tp_lref, TPtime_close, &time, fsufx, lsufx, tpcb->tp_fref);
		tpcb->tp_perf_on = 0; /* turn perf off */
	ENDPERF
}


/*
 * NAME: tp_soisdisconnected()
 *
 * CALLED FROM:
 *	tp.trans	
 *
 * FUNCTION and ARGUMENTS:
 *  Set state of the socket (so) to reflect that fact that we're disconnectED
 *  Set the state of the reference structure to closed, and
 *  recycle the suffix.
 *  Start a reference timer.
 *
 * RETURNS:	Nada
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 *  This differs from the regular soisdisconnected() in that the latter
 *  also sets the SS_CANTRECVMORE and SS_CANTSENDMORE flags.
 *  We don't want to set those flags because those flags will cause
 *  a SIGPIPE to be delivered in sosend() and we don't like that.
 *  If anyone else is sleeping on this socket, wake 'em up.
 */
void
tp_soisdisconnected(tpcb)
	register struct tp_pcb	*tpcb;
{
	register struct socket	*so = tpcb->tp_sock;

	soisdisconnecting(so);
	so->so_state &= ~SS_CANTSENDMORE;
	IFPERF(sototpcb(so))
		register struct tp_pcb *ttpcb = sototpcb(so);
		u_int 	fsufx, lsufx;

		/* CHOKE */
		bcopy ((caddr_t)ttpcb->tp_fsuffix, (caddr_t)&fsufx, sizeof(u_int) );
		bcopy ((caddr_t)ttpcb->tp_lsuffix, (caddr_t)&lsufx, sizeof(u_int) );

		tpmeas(ttpcb->tp_lref, TPtime_close, 
		   &time, &lsufx, &fsufx, ttpcb->tp_fref);
		tpcb->tp_perf_on = 0; /* turn perf off */
	ENDPERF

	tpcb->tp_refp->tpr_state = REF_FROZEN;
	tp_recycle_tsuffix( tpcb );
	tp_etimeout(tpcb->tp_refp, TM_reference, 0,0,0, (int)tpcb->tp_refer_ticks);
}

int tp_maxrefopen;  /* highest reference # of the set of open tp connections */

/*
 * NAME:	tp_freeref()
 *
 * CALLED FROM:
 *  tp.trans when the reference timer goes off, and
 *  from tp_attach() and tp_detach() when a tpcb is partially set up but not
 *  set up enough to have a ref timer set for it, and it's discarded
 *  due to some sort of error or an early close()
 *
 * FUNCTION and ARGUMENTS:
 *  Frees the reference represented by (r) for re-use.
 *
 * RETURNS: Nothing
 * 
 * SIDE EFFECTS:
 *
 * NOTES:	better be called at clock priority !!!!!
 */
void
tp_freeref(r)
	register struct tp_ref *r;
{
	IFDEBUG(D_TIMER)
		printf("tp_freeref called for ref %d maxrefopen %d\n", 
		r - tp_ref, tp_maxrefopen);
	ENDDEBUG
	IFTRACE(D_TIMER)
		tptrace(TPPTmisc, "tp_freeref ref tp_maxrefopen",
		r - tp_ref, tp_maxrefopen, 0, 0);
	ENDTRACE
	r->tpr_state = REF_FREE;
	IFDEBUG(D_CONN)
		printf("tp_freeref: CLEARING tpr_pcb 0x%x\n", r->tpr_pcb);
	ENDDEBUG
	r->tpr_pcb = (struct tp_pcb *)0;

	r = &tp_ref[tp_maxrefopen];

	while( tp_maxrefopen > 0 ) {
		if(r->tpr_state )
			break;
		tp_maxrefopen--;
		r--;
	}
	IFDEBUG(D_TIMER)
		printf("tp_freeref ends w/ maxrefopen %d\n", tp_maxrefopen);
	ENDDEBUG
}

/*
 * NAME:  tp_getref()
 *
 * CALLED FROM:
 *  tp_attach()
 *
 * FUNCTION and ARGUMENTS:
 *  obtains the next free reference and allocates the appropriate
 *  ref structure, links that structure to (tpcb) 
 *
 * RETURN VALUE:
 *	a reference number
 *  or TP_ENOREF
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 */
static RefNum
tp_getref(tpcb) 
	register struct tp_pcb *tpcb;
{
	register struct tp_ref	*r = tp_ref; /* tp_ref[0] is never used */
	register int 			i=1;


	while ((++r)->tpr_state != REF_FREE) {
		if (++i == N_TPREF)
			return TP_ENOREF;
	}
	r->tpr_state = REF_OPENING;
	if (tp_maxrefopen < i) 
		tp_maxrefopen = i;
	r->tpr_pcb = tpcb;
	tpcb->tp_refp = r;

	return i;
}

/*
 * NAME: tp_attach()
 *
 * CALLED FROM:
 *	tp_usrreq, PRU_ATTACH
 *
 * FUNCTION and ARGUMENTS:
 *  given a socket (so) and a protocol family (dom), allocate a tpcb
 *  and ref structure, initialize everything in the structures that
 *  needs to be initialized.
 *
 * RETURN VALUE:
 *  0 ok
 *  EINVAL if DEBUG(X) in is on and a disaster has occurred
 *  ENOPROTOOPT if TP hasn't been configured or if the
 *   socket wasn't created with tp as its protocol
 *  EISCONN if this socket is already part of a connection
 *  ETOOMANYREFS if ran out of tp reference numbers.
 *  E* whatever error is returned from soreserve()
 *    for from the network-layer pcb allocation routine
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 */
tp_attach(so, dom)
	struct socket 	*so;
	int 			dom;
{
	register struct tp_pcb	*tpcb;
	int 					error;
	int 					protocol = so->so_proto->pr_protocol;
	extern struct tp_conn_param tp_conn_param[];

	IFDEBUG(D_CONN)
		printf("tp_attach:dom 0x%x so 0x%x ", dom, so);
	ENDDEBUG
	IFTRACE(D_CONN)
		tptrace(TPPTmisc, "tp_attach:dom so", dom, so, 0, 0);
	ENDTRACE
	if ( ! tp_param.tpp_configed ) {
		error = ENOPROTOOPT; /* protocol not available */
		goto bad2;
	}

	if (so->so_pcb != NULL) { 
		return EISCONN;	/* socket already part of a connection*/
	}

	error = soreserve(so, TP_SOCKBUFSIZE, TP_SOCKBUFSIZE);
		/* later an ioctl will allow reallocation IF still in closed state */

	if (error)
		goto bad2;

	MALLOC(tpcb, struct tp_pcb *, sizeof(*tpcb), M_PCB, M_NOWAIT);
	if (tpcb == NULL) {
		error = ENOBUFS;
		goto bad2;
	}
	bzero( (caddr_t)tpcb, sizeof (struct tp_pcb) );

	if ( ((tpcb->tp_lref = tp_getref(tpcb)) &  TP_ENOREF) != 0 ) { 
		error = ETOOMANYREFS; 
		goto bad3;
	}
	tpcb->tp_sock =  so;
	tpcb->tp_domain = dom;
	if (protocol<ISOPROTO_TP4) {
		tpcb->tp_netservice = ISO_CONS;
		tpcb->tp_snduna = (SeqNum) -1;/* kludge so the pseudo-ack from the CR/CC
								 * will generate correct fake-ack values
								 */
	} else {
		tpcb->tp_netservice = (dom== AF_INET)?IN_CLNS:ISO_CLNS;
		/* the default */
	}
	tpcb->_tp_param = tp_conn_param[tpcb->tp_netservice];

	tpcb->tp_cong_win = 1;	
	tpcb->tp_state = TP_CLOSED;
	tpcb->tp_vers  = TP_VERSION;

		   /* Spec says default is 128 octets,
			* that is, if the tpdusize argument never appears, use 128.
			* As the initiator, we will always "propose" the 2048
			* size, that is, we will put this argument in the CR 
			* always, but accept what the other side sends on the CC.
			* If the initiator sends us something larger on a CR,
			* we'll respond w/ this.
			* Our maximum is 4096.  See tp_chksum.c comments.
			*/
	tpcb->tp_l_tpdusize = 1 << tpcb->tp_tpdusize;

	tpcb->tp_seqmask  = TP_NML_FMT_MASK;
	tpcb->tp_seqbit  =  TP_NML_FMT_BIT;
	tpcb->tp_seqhalf  =  tpcb->tp_seqbit >> 1;
	tpcb->tp_sndhiwat = (SeqNum) - 1; /* a kludge but it works */
	tpcb->tp_s_subseq = 0;

	/* attach to a network-layer protoswitch */
	/* new way */
	tpcb->tp_nlproto = & nl_protosw[tpcb->tp_netservice];
	ASSERT( tpcb->tp_nlproto->nlp_afamily == tpcb->tp_domain);
#ifdef notdef
	/* OLD WAY */
	/* TODO: properly, this search would be on the basis of 
	* domain,netservice or just netservice only (if you have
	* IN_CLNS, ISO_CLNS, and ISO_CONS)
	*/
	tpcb->tp_nlproto = nl_protosw;
	while(tpcb->tp_nlproto->nlp_afamily != tpcb->tp_domain )  {
		if( tpcb->tp_nlproto->nlp_afamily == 0 ) {
			error = EAFNOSUPPORT;
			goto bad4;
		}
		tpcb->tp_nlproto ++;
	}
#endif notdef

	/* xx_pcballoc sets so_pcb */
	if ( error =  (tpcb->tp_nlproto->nlp_pcballoc) ( 
							so, tpcb->tp_nlproto->nlp_pcblist ) ) {
		goto bad4;
	}

	if( dom == AF_INET )
		sotoinpcb(so)->inp_ppcb = (caddr_t) tpcb;
		/* nothing to do for iso case */

	tpcb->tp_npcb = (caddr_t) so->so_pcb;
	so->so_tpcb = (caddr_t) tpcb;

	return 0;

bad4:
	IFDEBUG(D_CONN)
		printf("BAD4 in tp_attach, so 0x%x\n", so);
	ENDDEBUG
	tp_freeref(tpcb->tp_refp);

bad3:
	IFDEBUG(D_CONN)
		printf("BAD3 in tp_attach, so 0x%x\n", so);
	ENDDEBUG

	free((caddr_t)tpcb, M_PCB); /* never a cluster  */

bad2:
	IFDEBUG(D_CONN)
		printf("BAD2 in tp_attach, so 0x%x\n", so);
	ENDDEBUG
	so->so_pcb = 0;
	so->so_tpcb = 0;

/*bad:*/
	IFDEBUG(D_CONN)
		printf("BAD in tp_attach, so 0x%x\n", so);
	ENDDEBUG
	return error;
}

/*
 * NAME:  tp_detach()
 *
 * CALLED FROM:
 *	tp.trans, on behalf of a user close request
 *  and when the reference timer goes off
 * (if the disconnect  was initiated by the protocol entity 
 * rather than by the user)
 *
 * FUNCTION and ARGUMENTS:
 *  remove the tpcb structure from the list of active or
 *  partially active connections, recycle all the mbufs
 *  associated with the pcb, ref structure, sockbufs, etc.
 *  Only free the ref structure if you know that a ref timer
 *  wasn't set for this tpcb.
 *
 * RETURNS:  Nada
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 *  tp_soisdisconnected() was already when this is called
 */
void
tp_detach(tpcb)
	register struct tp_pcb 	*tpcb;
{
	void					tp_freeref();
	register struct socket	 *so = tpcb->tp_sock;

	IFDEBUG(D_CONN)
		printf("tp_detach(tpcb 0x%x, so 0x%x)\n",
			tpcb,so);
	ENDDEBUG
	IFTRACE(D_CONN)
		tptraceTPCB(TPPTmisc, "tp_detach tpcb so lsufx", 
			tpcb, so, *(u_short *)(tpcb->tp_lsuffix), 0);
	ENDTRACE

	if (so->so_head) {
		if (!soqremque(so, 0) && !soqremque(so, 1))
			panic("sofree dq");
		so->so_head = 0;
	}

	IFDEBUG(D_CONN)
		printf("tp_detach(freeing RTC list snduna 0x%x rcvnxt 0x%x)\n",
		tpcb->tp_snduna_rtc,
		tpcb->tp_rcvnxt_rtc);
	ENDDEBUG

#define FREE_RTC_LIST(XXX)\
	{ register struct tp_rtc *xxr = XXX, *xxs; while (xxr) {\
		xxs = xxr->tprt_next;\
		m_freem( xxr->tprt_data );\
		m_free( dtom(xxr) ); xxr = xxs; }\
		XXX = (struct tp_rtc *)0;\
	}

	FREE_RTC_LIST( tpcb->tp_snduna_rtc );
	tpcb->tp_sndhiwat_rtc = (struct tp_rtc *)0;

	FREE_RTC_LIST( tpcb->tp_rcvnxt_rtc );

#undef FREE_RTC_LIST

	IFDEBUG(D_CONN)
		printf("so_snd at 0x%x so_rcv at 0x%x\n", &so->so_snd, &so->so_rcv);
		dump_mbuf(so->so_snd.sb_mb, "so_snd at detach ");
		printf("about to call LL detach, nlproto 0x%x, nl_detach 0x%x\n",
				tpcb->tp_nlproto, tpcb->tp_nlproto->nlp_pcbdetach);
	ENDDEBUG

	if (so->so_snd.sb_cc != 0)
		sbflush(&so->so_snd);
	if (tpcb->tp_Xrcv.sb_cc != 0)
		sbdrop(&tpcb->tp_Xrcv, (int)tpcb->tp_Xrcv.sb_cc);
	if (tpcb->tp_ucddata)
		m_freem(tpcb->tp_ucddata);

	IFDEBUG(D_CONN)
		printf("calling (...nlproto->...)(0x%x, so 0x%x)\n", 
			so->so_pcb, so);
		printf("so 0x%x so_head 0x%x,  qlen %d q0len %d qlimit %d\n", 
		so,  so->so_head,
		so->so_q0len, so->so_qlen, so->so_qlimit);
	ENDDEBUG


	(tpcb->tp_nlproto->nlp_pcbdetach)(so->so_pcb);
				/* does an sofree(so) */

	IFDEBUG(D_CONN)
		printf("after xxx_pcbdetach\n");
	ENDDEBUG

	if( tpcb->tp_refp->tpr_state == REF_OPENING ) {
		/* no connection existed here so no reference timer will be called */
		IFDEBUG(D_CONN)
			printf("SETTING ref %d, 0x%x to REF_FREE\n", tpcb->tp_lref,
			tpcb->tp_refp - &tp_ref[0]);
		ENDDEBUG

		tp_freeref(tpcb->tp_refp);
	}

	if (tpcb->tp_Xsnd.sb_mb) {
		printf("Unsent Xdata on detach; would panic");
		sbflush(&tpcb->tp_Xsnd);
	}
	so->so_tpcb = (caddr_t)0;

	/* 
	 * Get rid of the cluster mbuf allocated for performance measurements, if
	 * there is one.  Note that tpcb->tp_perf_on says nothing about whether or 
	 * not a cluster mbuf was allocated, so you have to check for a pointer 
	 * to one (that is, we need the TP_PERF_MEASs around the following section 
	 * of code, not the IFPERFs)
	 */
#ifdef TP_PERF_MEAS
	if (tpcb->tp_p_mbuf) {
		register struct mbuf *m = tpcb->tp_p_mbuf;
		struct mbuf *n;
		IFDEBUG(D_PERF_MEAS)
			printf("freeing tp_p_meas 0x%x  ", tpcb->tp_p_meas);
		ENDDEBUG
		do {
		    MFREE(m, n);
		    m = n;
		} while (n);
		tpcb->tp_p_meas = 0;
		tpcb->tp_p_mbuf = 0;
	}
#endif TP_PERF_MEAS

	IFDEBUG(D_CONN)
		printf( "end of detach, NOT single, tpcb 0x%x\n", tpcb);
	ENDDEBUG
	/* free((caddr_t)tpcb, M_PCB); WHere to put this ? */
}
