/*
 *	from: unknown
 *	$Id: tp_driver.c,v 1.3 1993/10/16 21:05:37 rgrimes Exp $
 */

#define _XEBEC_PG static

#include "tp_states.h"

static struct act_ent {
	int a_newstate;
	int a_action;
} statetable[] = { {0,0},
#include "tp_states.init"
};

/*	%W% (Berkeley) %G% */
#include "param.h"
#include "systm.h"
#include "socket.h"
#include "socketvar.h"
#include "protosw.h"
#include "mbuf.h"
#include "time.h"
#include "errno.h"
#include "../netiso/tp_param.h"
#include "../netiso/tp_stat.h"
#include "../netiso/tp_pcb.h"
#include "../netiso/tp_tpdu.h"
#include "../netiso/argo_debug.h"
#include "../netiso/tp_trace.h"
#include "../netiso/iso_errno.h"
#include "../netiso/tp_seq.h"
#include "../netiso/cons.h"

#define DRIVERTRACE TPPTdriver
#define sbwakeup(sb)	sowakeup(p->tp_sock, sb);
#define MCPY(d, w) (d ? m_copym(d, 0, (int)M_COPYALL, w): 0)

static 	trick_hc = 1;

int 	tp_emit(),
		tp_goodack(),				tp_goodXack(),
		tp_stash()
;
void	tp_indicate(),				tp_getoptions(),	
		tp_soisdisconnecting(), 	tp_soisdisconnected(),
		tp_recycle_tsuffix(),		
		tp_etimeout(),				tp_euntimeout(),
		tp_euntimeout_lss(),		tp_ctimeout(),
		tp_cuntimeout(),			tp_ctimeout_MIN(),
		tp_freeref(),				tp_detach(),
		tp0_stash(), 				tp0_send(),
		tp_netcmd(),				tp_send()
;

typedef  struct tp_pcb tpcb_struct;



typedef tpcb_struct tp_PCB_;

#include "tp_events.h"

_XEBEC_PG int _Xebec_action(a,e,p)
int a;
struct tp_event *e;
tp_PCB_ *p;
{
switch(a) {
case -1:  return tp_protocol_error(e,p);
case 0x1: 
		{
		(void) tp_emit(DC_TPDU_type, p, 0, 0, MNULL);
	}
		 break;
case 0x2: 
		{
#		ifdef TP_DEBUG
		if( e->ev_number != AK_TPDU )
			printf("TPDU 0x%x in REFWAIT!!!!\n", e->ev_number);
#		endif TP_DEBUG
	}
		 break;
case 0x3: 
		{
		/* oh, man is this grotesque or what? */
		(void) tp_goodack(p, e->ev_union.EV_AK_TPDU.e_cdt, e->ev_union.EV_AK_TPDU.e_seq,  e->ev_union.EV_AK_TPDU.e_subseq);
		/* but it's necessary because this pseudo-ack may happen
		 * before the CC arrives, but we HAVE to adjust the
		 * snduna as a result of the ack, WHENEVER it arrives
		 */
	}
		 break;
case 0x4: 
		{
		tp_detach(p);
	}
		 break;
case 0x5: 
		{
		p->tp_refp->tpr_state = REF_OPEN; /* has timers ??? */
	}
		 break;
case 0x6: 
		{
		IFTRACE(D_CONN)
			tptrace(TPPTmisc, "CR datalen data", e->ev_union.EV_CR_TPDU.e_datalen, e->ev_union.EV_CR_TPDU.e_data,0,0);
		ENDTRACE
		IFDEBUG(D_CONN)
			printf("CR datalen 0x%x data 0x%x", e->ev_union.EV_CR_TPDU.e_datalen, e->ev_union.EV_CR_TPDU.e_data);
		ENDDEBUG
		p->tp_refp->tpr_state = REF_OPEN; /* has timers */
		p->tp_fcredit = e->ev_union.EV_CR_TPDU.e_cdt;

		if (e->ev_union.EV_CR_TPDU.e_datalen > 0) {
			/* n/a for class 0 */
			ASSERT(p->tp_Xrcv.sb_cc == 0); 
			sbappendrecord(&p->tp_Xrcv, e->ev_union.EV_CR_TPDU.e_data);
			e->ev_union.EV_CR_TPDU.e_data = MNULL; 
		} 
	}
		 break;
case 0x7: 
		{
		IncStat(ts_tp0_conn);
		IFTRACE(D_CONN)
			tptrace(TPPTmisc, "Confiming", p, 0,0,0);
		ENDTRACE
		IFDEBUG(D_CONN)
			printf("Confirming connection: p" );
		ENDDEBUG
		soisconnected(p->tp_sock);
		(void) tp_emit(CC_TPDU_type, p, 0,0, MNULL) ;
		p->tp_fcredit = 1;
	}
		 break;
case 0x8: 
		{
		IncStat(ts_tp4_conn); /* even though not quite open */
		IFTRACE(D_CONN)
			tptrace(TPPTmisc, "Confiming", p, 0,0,0);
		ENDTRACE
		IFDEBUG(D_CONN)
			printf("Confirming connection: p" );
		ENDDEBUG
		soisconnecting(p->tp_sock);
		if ((p->tp_rx_strat & TPRX_FASTSTART) && (p->tp_fcredit > 0))
			p->tp_cong_win = p->tp_fcredit;
		p->tp_retrans = p->tp_Nretrans;
		tp_ctimeout(p->tp_refp, TM_retrans, (int)p->tp_cc_ticks);
	}
		 break;
case 0x9: 
		{
		register struct tp_ref *r = p->tp_refp;

		IFDEBUG(D_CONN)
			printf("event: CR_TPDU emit CC failed done " );
		ENDDEBUG
		soisdisconnected(p->tp_sock);
		tp_recycle_tsuffix( p );
		tp_freeref(r);
		tp_detach(p);
	}
		 break;
case 0xa: 
		{
		int error;
		struct mbuf *data = MNULL;

		IFTRACE(D_CONN)
			tptrace(TPPTmisc, "T_CONN_req flags ucddata", (int)p->tp_flags,
			p->tp_ucddata, 0, 0);
		ENDTRACE
		data =  MCPY(p->tp_ucddata, M_WAIT);
		if (data) {
			IFDEBUG(D_CONN)
				printf("T_CONN_req.trans m_copy cc 0x%x\n", 
					p->tp_ucddata);
				dump_mbuf(data, "sosnd @ T_CONN_req");
			ENDDEBUG
		}

		if (error = tp_emit(CR_TPDU_type, p, 0, 0, data) )
			return error; /* driver WON'T change state; will return error */
		
		p->tp_refp->tpr_state = REF_OPEN; /* has timers */
		if(p->tp_class != TP_CLASS_0) {
			p->tp_retrans = p->tp_Nretrans;
			tp_ctimeout(p->tp_refp, TM_retrans, (int)p->tp_cr_ticks);
		}
	}
		 break;
case 0xb: 
		{
		sbflush(&p->tp_Xrcv); /* purge non-delivered data data */
		if (e->ev_union.EV_DR_TPDU.e_datalen > 0) {
			sbappendrecord(&p->tp_Xrcv, e->ev_union.EV_DR_TPDU.e_data);
			e->ev_union.EV_DR_TPDU.e_data = MNULL;
		} 
		/* must return no error, or disc data and reason can't be read */
		tp_indicate(T_DISCONNECT, p, 0);
		tp_soisdisconnected(p);
		if (p->tp_class != TP_CLASS_0) {
			if (p->tp_state == TP_OPEN ) {
				tp_euntimeout(p->tp_refp, TM_data_retrans); /* all */
				tp_cuntimeout(p->tp_refp, TM_retrans);
				tp_cuntimeout(p->tp_refp, TM_inact);
				tp_cuntimeout(p->tp_refp, TM_sendack);
			}
			tp_cuntimeout(p->tp_refp, TM_retrans);
			if( e->ev_union.EV_DR_TPDU.e_sref !=  0 ) 
				(void) tp_emit(DC_TPDU_type, p, 0, 0, MNULL);
		}
	}
		 break;
case 0xc: 
		{
		if( e->ev_union.EV_DR_TPDU.e_sref != 0 )
			(void) tp_emit(DC_TPDU_type, p, 0, 0, MNULL); 
		/* reference timer already set - reset it to be safe (???) */
		tp_euntimeout(p->tp_refp, TM_reference); /* all */
		tp_etimeout(p->tp_refp, TM_reference, 0, 0, 0, (int)p->tp_refer_ticks);
	}
		 break;
case 0xd: 
		{	
		tp_cuntimeout(p->tp_refp, TM_retrans);
		tp_indicate(ER_TPDU, p, e->ev_union.EV_ER_TPDU.e_reason);
		tp_soisdisconnected(p);
	}
		 break;
case 0xe: 
		{	 
		tp_cuntimeout(p->tp_refp, TM_retrans);
		tp_soisdisconnected(p);
	}
		 break;
case 0xf: 
		{	 
		tp_indicate(ER_TPDU, p, e->ev_union.EV_ER_TPDU.e_reason);
		tp_cuntimeout(p->tp_refp, TM_retrans);
		tp_soisdisconnected(p);
	}
		 break;
case 0x10: 
		{	 
		tp_cuntimeout(p->tp_refp, TM_retrans);
		tp_soisdisconnected(p);
	}
		 break;
case 0x11: 
		{	/* don't ask me why we have to do this - spec says so */
		(void) tp_emit(DR_TPDU_type, p, 0, E_TP_NO_SESSION, MNULL);
		/* don't bother with retransmissions of the DR */
	}
		 break;
case 0x12: 
		{
		tp_soisdisconnecting(p->tp_sock);
		tp_indicate(ER_TPDU, p, e->ev_union.EV_ER_TPDU.e_reason);
		tp_soisdisconnected(p);
		tp_netcmd( p, CONN_CLOSE );
	}
		 break;
case 0x13: 
		{
		if (p->tp_state == TP_OPEN) {
			tp_euntimeout(p->tp_refp, TM_data_retrans); /* all */
			tp_cuntimeout(p->tp_refp, TM_inact);
			tp_cuntimeout(p->tp_refp, TM_sendack);
		}
		tp_soisdisconnecting(p->tp_sock);
		tp_indicate(ER_TPDU, p, e->ev_union.EV_ER_TPDU.e_reason);
		p->tp_retrans = p->tp_Nretrans;
		tp_ctimeout(p->tp_refp, TM_retrans, (int)p->tp_dr_ticks);
		(void) tp_emit(DR_TPDU_type, p, 0, E_TP_PROTO_ERR, MNULL);
	}
		 break;
case 0x14: 
		{	
		tp_cuntimeout(p->tp_refp, TM_retrans);
		IncStat(ts_tp0_conn);
		p->tp_fcredit = 1;
		soisconnected(p->tp_sock);
	}
		 break;
case 0x15: 
		{	
		IFDEBUG(D_CONN)
			printf("trans: CC_TPDU in CRSENT state flags 0x%x\n", 
				(int)p->tp_flags);
		ENDDEBUG
		IncStat(ts_tp4_conn);
		p->tp_fref = e->ev_union.EV_CC_TPDU.e_sref;
		p->tp_fcredit = e->ev_union.EV_CC_TPDU.e_cdt;
		p->tp_ackrcvd = 0;
		if ((p->tp_rx_strat & TPRX_FASTSTART) && (e->ev_union.EV_CC_TPDU.e_cdt > 0))
			p->tp_cong_win = e->ev_union.EV_CC_TPDU.e_cdt;
		tp_getoptions(p);
		tp_cuntimeout(p->tp_refp, TM_retrans);
		if (p->tp_ucddata) {
			IFDEBUG(D_CONN)
				printf("dropping user connect data cc 0x%x\n",
					p->tp_ucddata->m_len);
			ENDDEBUG
			m_freem(p->tp_ucddata);
			p->tp_ucddata = 0;
		}
		soisconnected(p->tp_sock);
		if (e->ev_union.EV_CC_TPDU.e_datalen > 0) {
			ASSERT(p->tp_Xrcv.sb_cc == 0); /* should be empty */
			sbappendrecord(&p->tp_Xrcv, e->ev_union.EV_CC_TPDU.e_data);
			e->ev_union.EV_CC_TPDU.e_data = MNULL;
		}

		(void) tp_emit(AK_TPDU_type, p, p->tp_rcvnxt, 0, MNULL);
		tp_ctimeout(p->tp_refp, TM_inact, (int)p->tp_inact_ticks);
	}
		 break;
case 0x16: 
		{
		struct mbuf *data = MNULL;
		int error;

		IncStat(ts_retrans_cr);
		p->tp_cong_win = 1;
		p->tp_ackrcvd = 0;
		data = MCPY(p->tp_ucddata, M_NOWAIT);
		if(p->tp_ucddata) {
			IFDEBUG(D_CONN)
				printf("TM_retrans.trans m_copy cc 0x%x\n", data);
				dump_mbuf(p->tp_ucddata, "sosnd @ TM_retrans");
			ENDDEBUG
			if( data == MNULL )
				return ENOBUFS;
		}

		p->tp_retrans --;
		if( error = tp_emit(CR_TPDU_type, p, 0, 0, data) ) {
			p->tp_sock->so_error = error;
		}
		tp_ctimeout(p->tp_refp, TM_retrans, (int)p->tp_cr_ticks);
	}
		 break;
case 0x17: 
		{ 	
		IncStat(ts_conn_gaveup);
		p->tp_sock->so_error = ETIMEDOUT;
		tp_indicate(T_DISCONNECT, p, ETIMEDOUT);
		tp_soisdisconnected(p);
	}
		 break;
case 0x18: 
		{	
		int error;
		struct mbuf *data = MCPY(p->tp_ucddata, M_WAIT);

		if( error = tp_emit(CC_TPDU_type, p, 0, 0, data) ) {
			p->tp_sock->so_error = error;
		}
		p->tp_retrans = p->tp_Nretrans;
		tp_ctimeout(p->tp_refp, TM_retrans, (int)p->tp_cc_ticks);
	}
		 break;
case 0x19: 
		{
		int doack;

		/*
		 * Get rid of any confirm or connect data, so that if we
		 * crash or close, it isn't thought of as disconnect data.
		 */
		if (p->tp_ucddata) {
			m_freem(p->tp_ucddata);
			p->tp_ucddata = 0;
		}
		tp_ctimeout(p->tp_refp, TM_inact, (int)p->tp_inact_ticks);
		tp_cuntimeout(p->tp_refp, TM_retrans);
		soisconnected(p->tp_sock);
		tp_getoptions(p);
		tp_ctimeout(p->tp_refp, TM_inact, (int)p->tp_inact_ticks);

		/* see also next 2 transitions, if you make any changes */

		doack = tp_stash(p, e);
		IFDEBUG(D_DATA)
			printf("tp_stash returns %d\n",doack);
		ENDDEBUG

		if(doack) {
			(void) tp_emit(AK_TPDU_type, p, p->tp_rcvnxt, 0, MNULL ); 
			tp_ctimeout(p->tp_refp, TM_sendack, (int)p->tp_keepalive_ticks);
		} else
			tp_ctimeout( p->tp_refp, TM_sendack, (int)p->tp_sendack_ticks);
		
		IFDEBUG(D_DATA)
			printf("after stash calling sbwakeup\n");
		ENDDEBUG
	}
		 break;
case 0x1a: 
		{
		tp0_stash(p, e);
		sbwakeup( &p->tp_sock->so_rcv );

		IFDEBUG(D_DATA)
			printf("after stash calling sbwakeup\n");
		ENDDEBUG
	}
		 break;
case 0x1b: 
		{
		int doack; /* tells if we must ack immediately */

		tp_ctimeout(p->tp_refp, TM_inact, (int)p->tp_inact_ticks);
		sbwakeup( &p->tp_sock->so_rcv );

		doack = tp_stash(p, e);
		IFDEBUG(D_DATA)
			printf("tp_stash returns %d\n",doack);
		ENDDEBUG

		if(doack)
			(void) tp_emit(AK_TPDU_type, p, p->tp_rcvnxt, 0, MNULL ); 
		else
			tp_ctimeout_MIN( p->tp_refp, TM_sendack, (int)p->tp_sendack_ticks);
		
		IFDEBUG(D_DATA)
			printf("after stash calling sbwakeup\n");
		ENDDEBUG
	}
		 break;
case 0x1c: 
		{ 	
		IFTRACE(D_DATA)
			tptrace(TPPTmisc, "NIW seq rcvnxt lcredit ",
				e->ev_union.EV_DT_TPDU.e_seq, p->tp_rcvnxt, p->tp_lcredit, 0);
		ENDTRACE
		IncStat(ts_dt_niw);
		m_freem(e->ev_union.EV_DT_TPDU.e_data);
		tp_ctimeout(p->tp_refp, TM_inact, (int)p->tp_inact_ticks);
		(void) tp_emit(AK_TPDU_type, p, p->tp_rcvnxt, 0, MNULL ); 
	}
		 break;
case 0x1d: 
		{
		if (p->tp_ucddata) {
			m_freem(p->tp_ucddata);
			p->tp_ucddata = 0;
		}
		(void) tp_goodack(p, e->ev_union.EV_AK_TPDU.e_cdt, e->ev_union.EV_AK_TPDU.e_seq, e->ev_union.EV_AK_TPDU.e_subseq);
		tp_cuntimeout(p->tp_refp, TM_retrans);

		tp_getoptions(p);
		soisconnected(p->tp_sock);
		IFTRACE(D_CONN)
			struct socket *so = p->tp_sock;
			tptrace(TPPTmisc, 
			"called sosiconn: so so_state rcv.sb_sel rcv.sb_flags",
				so, so->so_state, so->so_rcv.sb_sel, so->so_rcv.sb_flags);
			tptrace(TPPTmisc, 
			"called sosiconn 2: so_qlen so_error so_rcv.sb_cc so_head",
				so->so_qlen, so->so_error, so->so_rcv.sb_cc, so->so_head);
		ENDTRACE

		tp_ctimeout(p->tp_refp, TM_sendack, (int)p->tp_keepalive_ticks);
		tp_ctimeout(p->tp_refp, TM_inact, (int)p->tp_inact_ticks);
	}
		 break;
case 0x1e: 
		{
		if( p->tp_state == TP_AKWAIT ) {
			if (p->tp_ucddata) {
				m_freem(p->tp_ucddata);
				p->tp_ucddata = 0;
			}
			tp_cuntimeout(p->tp_refp, TM_retrans);
			tp_getoptions(p);
			soisconnected(p->tp_sock);
			tp_ctimeout(p->tp_refp, TM_sendack, (int)p->tp_keepalive_ticks);
			tp_ctimeout(p->tp_refp, TM_inact, (int)p->tp_inact_ticks);
		} 
		IFTRACE(D_XPD)
		tptrace(TPPTmisc, "XPD tpdu accepted Xrcvnxt, e_seq datalen m_len\n",
				p->tp_Xrcvnxt,e->ev_union.EV_XPD_TPDU.e_seq,  e->ev_union.EV_XPD_TPDU.e_datalen, e->ev_union.EV_XPD_TPDU.e_data->m_len);
		ENDTRACE

		p->tp_sock->so_state |= SS_RCVATMARK;
		e->ev_union.EV_XPD_TPDU.e_data->m_flags |= M_EOR;
		sbinsertoob(&p->tp_Xrcv, e->ev_union.EV_XPD_TPDU.e_data);
		IFDEBUG(D_XPD)
			dump_mbuf(e->ev_union.EV_XPD_TPDU.e_data, "XPD TPDU: tp_Xrcv");
		ENDDEBUG
		tp_indicate(T_XDATA, p, 0);
		sbwakeup( &p->tp_Xrcv );

		(void) tp_emit(XAK_TPDU_type, p, p->tp_Xrcvnxt, 0, MNULL);
		SEQ_INC(p, p->tp_Xrcvnxt);
	}
		 break;
case 0x1f: 
		{
		if( p->tp_Xrcv.sb_cc == 0 ) {
			/* kludge for select(): */ 
			/* p->tp_sock->so_state &= ~SS_OOBAVAIL; */
		}
	}
		 break;
case 0x20: 
		{
		IFTRACE(D_XPD)
			tptrace(TPPTmisc, "XPD tpdu niw (Xrcvnxt, e_seq) or not cdt (cc)\n",
				p->tp_Xrcvnxt, e->ev_union.EV_XPD_TPDU.e_seq,  p->tp_Xrcv.sb_cc , 0);
		ENDTRACE
		if( p->tp_Xrcvnxt != e->ev_union.EV_XPD_TPDU.e_seq )
			IncStat(ts_xpd_niw);
		if( p->tp_Xrcv.sb_cc ) {
			/* might as well kick 'em again */
			tp_indicate(T_XDATA, p, 0);
			IncStat(ts_xpd_dup);
		}
		m_freem(e->ev_union.EV_XPD_TPDU.e_data);
		tp_ctimeout(p->tp_refp, TM_inact, (int)p->tp_inact_ticks);
		/* don't send an xack because the xak gives "last one received", not
		 * "next one i expect" (dumb)
		 */
	}
		 break;
case 0x21: 
		{
		struct socket *so = p->tp_sock;

		/* detach from parent socket so it can finish closing */
		if (so->so_head) {
			if (!soqremque(so, 0) && !soqremque(so, 1))
				panic("tp: T_DETACH");
			so->so_head = 0;
		}
		tp_soisdisconnecting(p->tp_sock);
		tp_netcmd( p, CONN_CLOSE);
		tp_soisdisconnected(p);
	}
		 break;
case 0x22: 
		{
		struct socket *so = p->tp_sock;
		struct mbuf *data = MNULL;

		/* detach from parent socket so it can finish closing */
		if (so->so_head) {
			if (!soqremque(so, 0) && !soqremque(so, 1))
				panic("tp: T_DETACH");
			so->so_head = 0;
		}
		if (p->tp_state != TP_CLOSING) {
			tp_soisdisconnecting(p->tp_sock);
			data = MCPY(p->tp_ucddata, M_NOWAIT);
			(void) tp_emit(DR_TPDU_type, p, 0, E_TP_NORMAL_DISC, data);
			p->tp_retrans = p->tp_Nretrans;
			tp_ctimeout(p->tp_refp, TM_retrans, (int)p->tp_dr_ticks);
		}
	}
		 break;
case 0x23: 
		{
		tp_soisdisconnecting(p->tp_sock);
		tp_netcmd( p, CONN_CLOSE);
		tp_soisdisconnected(p);
	}
		 break;
case 0x24: 
		{
		struct mbuf *data = MCPY(p->tp_ucddata, M_WAIT);

		if(p->tp_state == TP_OPEN) {
			tp_euntimeout(p->tp_refp, TM_data_retrans); /* all */
			tp_cuntimeout(p->tp_refp, TM_inact);
			tp_cuntimeout(p->tp_refp, TM_sendack);
		}
		if (data) {
			IFDEBUG(D_CONN)
				printf("T_DISC_req.trans tp_ucddata 0x%x\n", 
					p->tp_ucddata);
				dump_mbuf(data, "ucddata @ T_DISC_req");
			ENDDEBUG
		}
		tp_soisdisconnecting(p->tp_sock);
		p->tp_retrans = p->tp_Nretrans;
		tp_ctimeout(p->tp_refp, TM_retrans, (int)p->tp_dr_ticks);

		if( trick_hc )
			return tp_emit(DR_TPDU_type, p, 0, e->ev_union.EV_T_DISC_req.e_reason, data);
	}
		 break;
case 0x25: 
		{
		int error;
		struct mbuf *data = MCPY(p->tp_ucddata, M_WAIT);

		IncStat(ts_retrans_cc);
		p->tp_retrans --;
		p->tp_cong_win = 1;
		p->tp_ackrcvd = 0;

		if( error = tp_emit(CC_TPDU_type, p, 0, 0, data) ) 
			p->tp_sock->so_error = error;
		tp_ctimeout(p->tp_refp, TM_retrans, (int)p->tp_cc_ticks);
	}
		 break;
case 0x26: 
		{
		IncStat(ts_conn_gaveup);
		tp_soisdisconnecting(p->tp_sock);
		p->tp_sock->so_error = ETIMEDOUT;
		tp_indicate(T_DISCONNECT, p, ETIMEDOUT);
		(void) tp_emit(DR_TPDU_type, p, 0, E_TP_CONGEST, MNULL);
		p->tp_retrans = p->tp_Nretrans;
		tp_ctimeout(p->tp_refp, TM_retrans, (int)p->tp_dr_ticks);
	}
		 break;
case 0x27: 
		{
		tp_euntimeout(p->tp_refp, TM_data_retrans); /* all */
		tp_cuntimeout(p->tp_refp, TM_inact); 
		tp_cuntimeout(p->tp_refp, TM_sendack);

		IncStat(ts_conn_gaveup);
		tp_soisdisconnecting(p->tp_sock);
		p->tp_sock->so_error = ETIMEDOUT;
		tp_indicate(T_DISCONNECT, p, ETIMEDOUT);
		(void) tp_emit(DR_TPDU_type, p, 0, E_TP_CONGEST_2, MNULL);
		p->tp_retrans = p->tp_Nretrans;
		tp_ctimeout(p->tp_refp, TM_retrans, (int)p->tp_dr_ticks);
	}
		 break;
case 0x28: 
		{
		p->tp_cong_win = 1;
		p->tp_ackrcvd = 0;
		/* resume XPD */
		if	( p->tp_Xsnd.sb_mb )  {
			struct mbuf *m = m_copy(p->tp_Xsnd.sb_mb, 0, (int)p->tp_Xsnd.sb_cc);
			/* m_copy doesn't preserve the m_xlink field, but at this pt.
			 * that doesn't matter
			 */

			IFTRACE(D_XPD)
				tptrace(TPPTmisc, "XPD retrans: Xuna Xsndnxt sndhiwat snduna",
					p->tp_Xuna, p->tp_Xsndnxt, p->tp_sndhiwat, 
					p->tp_snduna); 
			ENDTRACE
			IFDEBUG(D_XPD)
				dump_mbuf(m, "XPD retrans emitting M");
			ENDDEBUG
			IncStat(ts_retrans_xpd);
			p->tp_retrans --;
			(void) tp_emit(XPD_TPDU_type, p, p->tp_Xuna, 1, m);
			tp_ctimeout(p->tp_refp, TM_retrans, (int)p->tp_xpd_ticks);
		}
	}
		 break;
case 0x29: 
		{	
		register 	SeqNum			low, lowsave = 0;
		register	struct tp_rtc 	*r = p->tp_snduna_rtc;
		register	struct mbuf 	*m;
		register	SeqNum			high = e->ev_union.EV_TM_data_retrans.e_high;

		low = p->tp_snduna;
		lowsave = high = low;

		tp_euntimeout_lss(p->tp_refp, TM_data_retrans,
			SEQ_ADD(p, p->tp_sndhiwat, 1));
		p->tp_retrans_hiwat = p->tp_sndhiwat;

		if ((p->tp_rx_strat & TPRX_EACH) == 0)
			high = (high>low)?low:high;

		if( p->tp_rx_strat & TPRX_USE_CW ) {
			register int i;

			p->tp_cong_win = 1;
			p->tp_ackrcvd = 0;
			i = SEQ_ADD(p, low, p->tp_cong_win);

			high = SEQ_MIN(p, high, p->tp_sndhiwat);

		}

		while( SEQ_LEQ(p, low, high) ){
			if ( r == (struct tp_rtc *)0 ){
				IFDEBUG(D_RTC)
					printf( "tp: retrans rtc list is GONE!\n");
				ENDDEBUG
				break;
			}
			if ( r->tprt_seq == low ){
				if(( m = m_copy(r->tprt_data, 0, r->tprt_octets ))== MNULL)
					break;
				(void) tp_emit(DT_TPDU_type, p, low, r->tprt_eot, m);
				IncStat(ts_retrans_dt);
				SEQ_INC(p, low );
			}
			r = r->tprt_next;
		}
/* CE_BIT
		if ( SEQ_LEQ(p, lowsave, high) ){
*/
			e->ev_union.EV_TM_data_retrans.e_retrans --;
			tp_etimeout(p->tp_refp, TM_data_retrans, (caddr_t)lowsave,
					(caddr_t)high, e->ev_union.EV_TM_data_retrans.e_retrans,
					(p->tp_Nretrans - e->ev_union.EV_TM_data_retrans.e_retrans) * (int)p->tp_dt_ticks);
/* CE_BIT
		}
*/
	}
		 break;
case 0x2a: 
		{	
		p->tp_retrans --;
		(void) tp_emit(DR_TPDU_type, p, 0, E_TP_DR_NO_REAS, MNULL);
		IncStat(ts_retrans_dr);
		tp_ctimeout(p->tp_refp, TM_retrans, (int)p->tp_dr_ticks);
	}
		 break;
case 0x2b: 
		{	
		p->tp_sock->so_error = ETIMEDOUT;
		p->tp_refp->tpr_state = REF_FROZEN;
		tp_recycle_tsuffix( p );
		tp_etimeout(p->tp_refp, TM_reference, 0,0,0, (int)p->tp_refer_ticks);
	}
		 break;
case 0x2c: 
		{
		tp_freeref(p->tp_refp);
		tp_detach(p);
	}
		 break;
case 0x2d: 
		{	
		if( p->tp_class != TP_CLASS_0) {
			tp_ctimeout(p->tp_refp, TM_inact, (int)p->tp_inact_ticks);
			if ( e->ev_number == CC_TPDU )
				(void) tp_emit(AK_TPDU_type, p, p->tp_rcvnxt, 0, MNULL); 
		}
		/* ignore it if class 0 - state tables are blank for this */
	}
		 break;
case 0x2e: 
		{
		IFTRACE(D_DATA)
			tptrace(TPPTmisc, "T_DATA_req sndhiwat snduna fcredit, tpcb",
				p->tp_sndhiwat, p->tp_snduna, p->tp_fcredit, p);
		ENDTRACE

		tp_send(p);
	}
		 break;
case 0x2f: 
		{
		int error = 0;

		/* resume XPD */
		if	( p->tp_Xsnd.sb_mb )  {
			struct mbuf *m = m_copy(p->tp_Xsnd.sb_mb, 0, (int)p->tp_Xsnd.sb_cc);
			/* m_copy doesn't preserve the m_xlink field, but at this pt.
			 * that doesn't matter
			 */

			IFTRACE(D_XPD)
				tptrace(TPPTmisc, "XPD req: Xuna Xsndnxt sndhiwat snduna",
					p->tp_Xuna, p->tp_Xsndnxt, p->tp_sndhiwat, 
					p->tp_snduna); 
			ENDTRACE
			IFDEBUG(D_XPD)
				printf("T_XPD_req: sb_cc 0x%x\n", p->tp_Xsnd.sb_cc);
				dump_mbuf(m, "XPD req emitting M");
			ENDDEBUG
			error = 
				tp_emit(XPD_TPDU_type, p, p->tp_Xuna, 1, m);
			p->tp_retrans = p->tp_Nretrans;
			tp_ctimeout(p->tp_refp, TM_retrans, (int)p->tp_xpd_ticks);
			SEQ_INC(p, p->tp_Xsndnxt);
		} 
		if(trick_hc)
			return error;
	}
		 break;
case 0x30: 
		{
		IFDEBUG(D_ACKRECV)
			printf("GOOD ACK seq 0x%x cdt 0x%x\n", e->ev_union.EV_AK_TPDU.e_seq, e->ev_union.EV_AK_TPDU.e_cdt);
		ENDDEBUG
		if( p->tp_class != TP_CLASS_0) {
			tp_ctimeout(p->tp_refp, TM_inact, (int)p->tp_inact_ticks);
			tp_euntimeout_lss(p->tp_refp, TM_data_retrans, e->ev_union.EV_AK_TPDU.e_seq);
		}
		sbwakeup( &p->tp_sock->so_snd );

		if (p->tp_sndhiwat <= p->tp_retrans_hiwat &&
			p->tp_snduna <= p->tp_retrans_hiwat) {

			register    struct mbuf     *m;
			/* extern      struct mbuf     *m_copy(); */
			register    struct tp_rtc   *r;
			SeqNum      high, retrans, low_save;

			high = SEQ_MIN(p, SEQ_ADD(p, p->tp_snduna,
					MIN(p->tp_cong_win, p->tp_fcredit)) - 1,
					p->tp_sndhiwat);
			low_save = retrans = SEQ_MAX(p, SEQ_ADD(p, p->tp_last_retrans, 1),
					p->tp_snduna);
			for (; SEQ_LEQ(p, retrans, high); SEQ_INC(p, retrans)) {

				for (r = p->tp_snduna_rtc; r; r = r->tprt_next){
					if ( r->tprt_seq == retrans ){
						if(( m = m_copy(r->tprt_data, 0, r->tprt_octets ))
								== MNULL)
							break;
						(void) tp_emit(DT_TPDU_type, p, retrans,
							r->tprt_eot, m);
						p->tp_last_retrans = retrans;
						IncStat(ts_retrans_dt);
						break;
					}
				}
				if ( r == (struct tp_rtc *)0 ){
					IFDEBUG(D_RTC)
						printf( "tp: retrans rtc list is GONE!\n");
					ENDDEBUG
					break;
				}
			}
			tp_etimeout(p->tp_refp, TM_data_retrans, (caddr_t)low_save,
					(caddr_t)high, p->tp_retrans, (int)p->tp_dt_ticks);
			if (SEQ_DEC(p, retrans) == p->tp_retrans_hiwat)
				tp_send(p);
		}
		else {
			tp_send(p);
		}
		IFDEBUG(D_ACKRECV)
			printf("GOOD ACK new sndhiwat 0x%x\n", p->tp_sndhiwat);
		ENDDEBUG
	}
		 break;
case 0x31: 
		{
		IFTRACE(D_ACKRECV)
			tptrace(TPPTmisc, "BOGUS ACK fcc_present, tp_r_subseq e_subseq", 
				e->ev_union.EV_AK_TPDU.e_fcc_present, p->tp_r_subseq, e->ev_union.EV_AK_TPDU.e_subseq, 0);
		ENDTRACE
		if( p->tp_class != TP_CLASS_0 ) {

			if ( !e->ev_union.EV_AK_TPDU.e_fcc_present ) {
				/* send ACK with FCC */
				IncStat( ts_ackreason[_ACK_FCC_] );
				(void) tp_emit(AK_TPDU_type, p, p->tp_rcvnxt, 1, MNULL);
			}
			tp_ctimeout(p->tp_refp, TM_inact, (int)p->tp_inact_ticks);
		} 
	}
		 break;
case 0x32: 
		{	
		tp_ctimeout(p->tp_refp, TM_inact, (int)p->tp_inact_ticks);
		tp_cuntimeout(p->tp_refp, TM_retrans);

		sbwakeup( &p->tp_sock->so_snd );

		/* resume normal data */
		tp_send(p);
	}
		 break;
case 0x33: 
		{
		IFTRACE(D_ACKRECV)
			tptrace(TPPTmisc, "BOGUS XACK eventtype ", e->ev_number, 0, 0,0);
		ENDTRACE
		if( p->tp_class != TP_CLASS_0 ) {
			tp_ctimeout(p->tp_refp, TM_inact, (int)p->tp_inact_ticks);
		} 
	}
		 break;
case 0x34: 
		{	
		IFTRACE(D_TIMER)
			tptrace(TPPTsendack, -1, p->tp_lcredit, p->tp_sent_uwe, 
			p->tp_sent_lcdt, 0);
		ENDTRACE
		IncPStat(p, tps_n_TMsendack);
		(void) tp_emit(AK_TPDU_type, p, p->tp_rcvnxt, 0, MNULL);
	}
		 break;
case 0x35: 
		{
		if (sbspace(&p->tp_sock->so_rcv) > 0)
			tp0_openflow(p);
	}
		 break;
case 0x36: 
		{	
		if( trick_hc ) {
			IncStat(ts_ackreason[_ACK_USRRCV_]);

			/* send an ACK only if there's new information */
			LOCAL_CREDIT( p );
			if ((p->tp_rcvnxt != p->tp_sent_rcvnxt) ||
				(p->tp_lcredit != p->tp_sent_lcdt))

				return tp_emit(AK_TPDU_type, p, p->tp_rcvnxt, 0, MNULL);
		}
	}
		 break;
case 0x37: 
		{
		if(trick_hc)
		return ECONNABORTED;
	}
		 break;
case 0x38: 
		{
		ASSERT( p->tp_state != TP_LISTENING );
		tp_indicate(T_DISCONNECT, p, ECONNRESET);
		tp_soisdisconnected(p);
	}
		 break;
	}
return 0;
}

_XEBEC_PG int
_Xebec_index( e,p )
	struct tp_event *e;
	tp_PCB_ *p;
{
switch( (e->ev_number<<4)+(p->tp_state) ) {
case 0x12:
	if (	p->tp_retrans > 0 ) return 0x1e;
	 else return 0x1f;
case 0x13:
	if ( p->tp_retrans > 0 ) return 0x2f;
	 else return 0x30;
case 0x14:
	if ( p->tp_retrans > 0 ) return 0x32;
	 else return 0x31;
case 0x15:
	if (	p->tp_retrans > 0 ) return 0x34;
	 else return 0x35;
case 0x54:
	if ( e->ev_union.EV_TM_data_retrans.e_retrans > 0 ) return 0x33;
	 else return 0x31;
case 0x64:
	if (p->tp_class == TP_CLASS_0) return 0x1a;
	 else return 0x1b;
case 0x77:
	if ( p->tp_class == TP_CLASS_0) return 0xd;
	 else return 0xe;
case 0x86:
	if ( e->ev_union.EV_DR_TPDU.e_sref !=  0 ) return 0x2;
	 else return 0x3;
case 0xa2:
	if (p->tp_class == TP_CLASS_0) return 0x1c;
	 else return 0x1d;
case 0xb2:
	if (p->tp_class == TP_CLASS_0) return 0x5;
	 else return 0x0;
case 0xb4:
	if ( tp_goodack(p, e->ev_union.EV_AK_TPDU.e_cdt, e->ev_union.EV_AK_TPDU.e_seq, e->ev_union.EV_AK_TPDU.e_subseq)  ) return 0x3a;
	 else return 0x3b;
case 0xc3:
	if ( IN_RWINDOW( p, e->ev_union.EV_DT_TPDU.e_seq,
					p->tp_rcvnxt, SEQ(p, p->tp_rcvnxt + p->tp_lcredit)) ) return 0x21;
	 else return 0x24;
case 0xc4:
	if ( p->tp_class == TP_CLASS_0 ) return 0x22;
	 else if ( IN_RWINDOW( p, e->ev_union.EV_DT_TPDU.e_seq,
					p->tp_rcvnxt, SEQ(p, p->tp_rcvnxt + p->tp_lcredit)) ) return 0x23;
	 else return 0x25;
case 0xd3:
	if (p->tp_Xrcvnxt == e->ev_union.EV_XPD_TPDU.e_seq) return 0x27;
	 else return 0x2a;
case 0xd4:
	if (p->tp_Xrcvnxt == e->ev_union.EV_XPD_TPDU.e_seq) return 0x27;
	 else return 0x29;
case 0xe4:
	if ( tp_goodXack(p, e->ev_union.EV_XAK_TPDU.e_seq) ) return 0x3c;
	 else return 0x3d;
case 0x102:
	if ( p->tp_class == TP_CLASS_0 ) return 0x2d;
	 else return 0x2e;
case 0x104:
	if ( p->tp_class == TP_CLASS_0 ) return 0x2d;
	 else return 0x2e;
case 0x144:
	if (p->tp_class == TP_CLASS_0) return 0x3f;
	 else return 0x40;
case 0x162:
	if (p->tp_class == TP_CLASS_0) return 0x2b;
	 else return 0x2c;
case 0x172:
	if ( p->tp_class != TP_CLASS_4 ) return 0x42;
	 else return 0x46;
case 0x174:
	if ( p->tp_class != TP_CLASS_4 ) return 0x42;
	 else return 0x47;
case 0x177:
	if ( p->tp_class != TP_CLASS_4 ) return 0x42;
	 else return 0x43;
case 0x188:
	if ( p->tp_class == TP_CLASS_0 ) return 0xf;
	 else if (tp_emit(CC_TPDU_type, p, 0,0, MCPY(p->tp_ucddata, M_NOWAIT)) == 0) return 0x10;
	 else return 0x11;
default: return 0;
} /* end switch */
} /* _Xebec_index() */
static int inx[26][9] = { {0,0,0,0,0,0,0,0,0,},
 {0x0,0x0,0x0,0x0,0x31,0x0,0x0,0x0,0x0, },
 {0x0,0x0,-1,-1,-1,-1,0x0,0x0,0x0, },
 {0x0,0x0,0x0,0x0,0x3e,0x0,0x0,0x0,0x0, },
 {0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0, },
 {0x0,0x0,0x0,0x0,0x0,0x0,0x36,0x0,0x0, },
 {0x0,0x0,0x0,0x0,-1,0x0,0x0,0x0,0x0, },
 {0x0,0x7,0x15,0x1b,-1,0x17,0x3,0xa,0x0, },
 {0x0,0x19,0x6,0x20,0x37,0x8,0x3,-1,0x0, },
 {0x0,0x14,0x13,0x13,0x13,0x16,-1,0xa,0x0, },
 {0x0,0x7,0x6,0x1,0x9,0x18,0x3,0xa,0x0, },
 {0x0,0x19,-1,0x1,0x37,0x8,0x3,0xa,0x0, },
 {0x0,0x7,-1,0x26,-1,0x8,0x3,0xa,0x0, },
 {0x0,0x7,0x6,-1,-1,0x8,0x3,0xa,0x0, },
 {0x0,0x7,0x6,-1,-1,0x8,0x3,0xa,0x0, },
 {0x0,0x7,0x6,0x1,-1,0x8,0x3,0xa,0x0, },
 {0x0,0x12,0x0,0x0,0x0,0x0,0x0,0x0,0x0, },
 {0x0,0x0,-1,0x2e,-1,0x0,0x4,0x0,0x2e, },
 {0x0,0xb,0x0,0x0,0x0,0x0,0x0,0x0,0x0, },
 {0x0,0x0,0x0,0x0,0x38,0x0,0x0,0x0,0x0, },
 {0x0,0x0,0x0,0x0,0x39,0x0,0x0,0x0,0x0, },
 {0x0,0x0,0x0,0x0,-1,0x0,0x41,0x0,0x0, },
 {0x0,0x0,0x0,0x0,0x28,0x0,0x41,0x0,0x0, },
 {0x0,0xc,-1,0x2c,0x0,0x2c,0x4,0xc,0x2c, },
 {0x0,0x49,-1,0x45,-1,0x44,0x48,-1,0x0, },
 {0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,-1, },
};
tp_driver(p, e)
register tp_PCB_ *p;
register struct tp_event *e;
{
	register int index, error=0;
	struct act_ent *a;
	static struct act_ent erroraction = {0,-1};

	index = inx[1 + e->ev_number][p->tp_state];
	if(index<0) index=_Xebec_index(e, p);
	if (index==0) {
		a = &erroraction;
	} else
		a = &statetable[index];

	if(a->a_action)
		error = _Xebec_action( a->a_action, e, p );
	IFTRACE(D_DRIVER)
	tptrace(DRIVERTRACE,		a->a_newstate, p->tp_state, e->ev_number, a->a_action, 0);

	ENDTRACE
	if(error==0)
	p->tp_state = a->a_newstate;
	return error;
}
