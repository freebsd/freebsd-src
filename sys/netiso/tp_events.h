/*
 *	from: unknown
 *	$Id: tp_events.h,v 1.3 1993/11/07 17:49:51 wollman Exp $
 */

#ifndef _NETISO_TP_EVENTS_H_
#define _NETISO_TP_EVENTS_H_ 1

struct tp_event {
  int ev_number;
  struct timeval e_time; 
#define TM_inact 0x0
#define TM_retrans 0x1
#define TM_sendack 0x2
#define TM_notused 0x3

  union{
    struct { SeqNum e_low; SeqNum e_high; int e_retrans; } EV_TM_reference;

#define TM_reference 0x4
    struct { SeqNum e_low; SeqNum e_high; int e_retrans; } EV_TM_data_retrans;

#define TM_data_retrans 0x5
    struct {
      u_char		e_reason;
    } EV_ER_TPDU;

#define ER_TPDU 0x6
    struct {
      struct mbuf 	*e_data;	/* first field */
      int 			e_datalen; /* 2nd field */
      u_int			e_cdt;
    } EV_CR_TPDU;

#define CR_TPDU 0x7
    struct {
      struct mbuf 	*e_data;	/* first field */
      int 			e_datalen; /* 2nd field */
      u_short		e_sref;
      u_char		e_reason;
    } EV_DR_TPDU;

#define DR_TPDU 0x8
#define DC_TPDU 0x9
    struct {
      struct mbuf 	*e_data;	/* first field */
      int 			e_datalen; /* 2nd field */
      u_short		e_sref;
      u_int			e_cdt;
    } EV_CC_TPDU;

#define CC_TPDU 0xa
    struct {
      u_int			e_cdt;	
      SeqNum 	 	e_seq;		
      SeqNum 	 	e_subseq;		
      u_char 	 	e_fcc_present;		
    } EV_AK_TPDU;

#define AK_TPDU 0xb
    struct {
      struct mbuf	*e_data; 	/* first field */
      int 			e_datalen; /* 2nd field */
      u_int 		e_eot;
      SeqNum		e_seq; 
    } EV_DT_TPDU;

#define DT_TPDU 0xc
    struct {
      struct mbuf 	*e_data;	/* first field */
      int 			e_datalen; 	/* 2nd field */
      SeqNum 		e_seq;	
    } EV_XPD_TPDU;

#define XPD_TPDU 0xd
    struct {
      SeqNum 		e_seq;		
    } EV_XAK_TPDU;

#define XAK_TPDU 0xe
#define T_CONN_req 0xf
    struct {
      u_char		e_reason; 	
    } EV_T_DISC_req;

#define T_DISC_req 0x10
#define T_LISTEN_req 0x11
#define T_DATA_req 0x12
#define T_XPD_req 0x13
#define T_USR_rcvd 0x14
#define T_USR_Xrcvd 0x15
#define T_DETACH 0x16
#define T_NETRESET 0x17
#define T_ACPT_req 0x18
  } ev_union;
};/* end struct event */

#define tp_NEVENTS 0x19

#define ATTR(X)ev_union.EV_/**/X/**/
#endif /* _NETISO_TP_EVENTS_H_ */
