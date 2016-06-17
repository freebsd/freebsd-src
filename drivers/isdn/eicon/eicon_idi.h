/* $Id: eicon_idi.h,v 1.1.4.1 2001/11/20 14:19:35 kai Exp $
 *
 * ISDN lowlevel-module for the Eicon active cards.
 * IDI-Interface
 *
 * Copyright 1998-2000  by Armin Schindler (mac@melware.de)
 * Copyright 1999,2000  Cytronics & Melware (info@melware.de)
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#ifndef E_IDI_H
#define E_IDI_H

#include <linux/config.h>

#undef N_DATA
#undef ID_MASK

#include "pc.h"

#define AOC_IND  26		/* Advice of Charge                         */
#define PI  0x1e                /* Progress Indicator               */
#define NI  0x27                /* Notification Indicator           */

#define CALL_HOLD	0x22
#define CALL_HOLD_ACK	0x24

/* defines for statectrl */
#define WAITING_FOR_HANGUP	0x01
#define HAVE_CONN_REQ		0x02
#define IN_HOLD			0x04

typedef struct {
	char cpn[32];
	char oad[32];
	char dsa[32];
	char osa[32];
	__u8 plan;
	__u8 screen;
	__u8 sin[4];
	__u8 chi[4];
	__u8 e_chi[4];
	__u8 bc[12];
	__u8 e_bc[12];
 	__u8 llc[18];
	__u8 hlc[5];
	__u8 cau[4];
	__u8 e_cau[2];
	__u8 e_mt;
	__u8 dt[6];
	char display[83];
	char keypad[35];
	char rdn[32];
} idi_ind_message;

typedef struct { 
  __u16 next            __attribute__ ((packed));
  __u8  Req             __attribute__ ((packed));
  __u8  ReqId           __attribute__ ((packed));
  __u8  ReqCh           __attribute__ ((packed));
  __u8  Reserved1       __attribute__ ((packed));
  __u16 Reference       __attribute__ ((packed));
  __u8  Reserved[8]     __attribute__ ((packed));
  eicon_PBUFFER XBuffer; 
} eicon_REQ;

typedef struct {
  __u16 next            __attribute__ ((packed));
  __u8  Rc              __attribute__ ((packed));
  __u8  RcId            __attribute__ ((packed));
  __u8  RcCh            __attribute__ ((packed));
  __u8  Reserved1       __attribute__ ((packed));
  __u16 Reference       __attribute__ ((packed));
  __u8  Reserved2[8]    __attribute__ ((packed));
} eicon_RC;

typedef struct {
  __u16 next            __attribute__ ((packed));
  __u8  Ind             __attribute__ ((packed));
  __u8  IndId           __attribute__ ((packed));
  __u8  IndCh           __attribute__ ((packed));
  __u8  MInd            __attribute__ ((packed));
  __u16 MLength         __attribute__ ((packed));
  __u16 Reference       __attribute__ ((packed));
  __u8  RNR             __attribute__ ((packed));
  __u8  Reserved        __attribute__ ((packed));
  __u32 Ack             __attribute__ ((packed));
  eicon_PBUFFER RBuffer;
} eicon_IND;

typedef struct {
	__u8		*Data;
	unsigned int	Size;
	unsigned int	Len;
	__u8		*Next;
} eicon_OBJBUFFER;

extern int idi_do_req(eicon_card *card, eicon_chan *chan, int cmd, int layer);
extern int idi_hangup(eicon_card *card, eicon_chan *chan);
extern int idi_connect_res(eicon_card *card, eicon_chan *chan);
extern int eicon_idi_listen_req(eicon_card *card, eicon_chan *chan);
extern int idi_connect_req(eicon_card *card, eicon_chan *chan, char *phone,
	                    char *eazmsn, int si1, int si2);

extern void idi_handle_ack(eicon_card *card, struct sk_buff *skb);
extern void idi_handle_ind(eicon_card *card, struct sk_buff *skb);
extern int eicon_idi_manage(eicon_card *card, eicon_manifbuf *mb);
extern int idi_send_data(eicon_card *card, eicon_chan *chan, int ack, struct sk_buff *skb, int que, int chk);
extern void idi_audio_cmd(eicon_card *ccard, eicon_chan *chan, int cmd, u_char *value);
extern int capipmsg(eicon_card *card, eicon_chan *chan, capi_msg *cm);
#ifdef CONFIG_ISDN_TTY_FAX
extern void idi_fax_cmd(eicon_card *card, eicon_chan *chan);
extern int idi_faxdata_send(eicon_card *ccard, eicon_chan *chan, struct sk_buff *skb);
#endif

#endif
