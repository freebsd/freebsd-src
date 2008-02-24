/*-
 * Copyright (c) 1997, 2002 Hellmuth Michaelis. All rights reserved.
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
 */

/*---------------------------------------------------------------------------
 *
 *	i4b_cause.h - causes and cause handling for i4b
 *	-----------------------------------------------
 *
 * $FreeBSD: src/sys/i4b/include/i4b_cause.h,v 1.10 2007/07/05 21:52:43 bz Exp $
 *
 *      last edit-date: [Sun Mar 17 10:08:21 2002]
 *
 *---------------------------------------------------------------------------*/

#ifndef _I4B_CAUSE_H_
#define _I4B_CAUSE_H_

/*---------------------------------------------------------------------------*
 *	ISDN4BSD internal causes specification
 *---------------------------------------------------------------------------*/

#define CAUSE_VAL	0x000000ff	/* cause value			*/
#define CAUSE_TYPE	0x0000ff00	/* cause type			*/
#define  CAUSET_Q850	0		/* value coded according to Q.850 */
#define  CAUSET_I4B	1		/* i4b protocol independent causes*/

#define GET_CAUSE_VAL(cause)	  ((cause) & 0xff)
#define SET_CAUSE_VAL(dest, val)  ((dest) = ((dest & 0xffffff00) | \
						(val & 0x000000ff)))

#define GET_CAUSE_TYPE(cause)	   (((cause) >> 8) & 0xff)
#define SET_CAUSE_TYPE(dest, type) ((dest) = ((dest & 0xffff00ff) | \
						((type << 8) & 0x0000ff00)))

#define SET_CAUSE_TV(dest, type, val) ((dest) = ((val & 0x000000ff) | \
						 ((type << 8) & 0x0000ff00)))

/* CAUSET_I4B - protocol independent cause values */

#define	CAUSE_I4B_NORMAL	0	/* normal call clearing 	*/
#define CAUSE_I4B_BUSY		1	/* user busy			*/
#define CAUSE_I4B_NOCHAN	2	/* circuit/channel not available*/
#define	CAUSE_I4B_INCOMP	3	/* incompatible source/dest	*/
#define CAUSE_I4B_REJECT	4	/* call rejected		*/
#define CAUSE_I4B_OOO		5	/* destination out of order	*/
#define CAUSE_I4B_TMPFAIL	6	/* temporary failure		*/
#define CAUSE_I4B_L1ERROR	7	/* L1 error / persistent deact	*/
#define CAUSE_I4B_LLDIAL	8	/* no dialout on leased line	*/
#define CAUSE_I4B_MAX		9
	/* NOTE: update isdnd/pcause.c when adding causes !!!!!		*/
	/* NOTE: update layer3/i4b_q931.c when adding causes !!!!!	*/
	
/* CAUSET_Q850 - causes defined in Q.850 */

#define	CAUSE_Q850_SHUTDN	0x00	/* normal D-channel shutdown */
#define CAUSE_Q850_NUNALLC	0x01	/* Unallocated (unassigned) number */
#define CAUSE_Q850_NRTTN	0x02	/* No route to specified transit network */
#define CAUSE_Q850_NRTDST	0x03	/* No route to destination */
#define CAUSE_Q850_SSINFTN	0x04	/* Send special information tone */
#define CAUSE_Q850_MDIALTP	0x05	/* Misdialled trunk prefix */
#define CAUSE_Q850_CHUNACC	0x06	/* Channel unacceptable */
#define CAUSE_Q850_CALLAWD	0x07	/* Call awarded and being delivered in an established channel */
#define CAUSE_Q850_PREEMPT	0x08	/* Preemption */
#define CAUSE_Q850_PREECRR	0x09	/* Preemption - circuit reserved for reuse */
#define CAUSE_Q850_NCCLR	0x10	/* Normal call clearing */
#define CAUSE_Q850_USRBSY	0x11	/* User busy */
#define CAUSE_Q850_NOUSRRSP	0x12	/* No user responding */
#define CAUSE_Q850_NOANSWR	0x13	/* No answer from user (user alerted) */
#define CAUSE_Q850_SUBSABS	0x14	/* Subscriber absent */
#define CAUSE_Q850_CALLREJ	0x15	/* Call rejected */
#define CAUSE_Q850_NUCHNG	0x16	/* Number changed */
#define CAUSE_Q850_NONSELUC	0x1A	/* Non-selected user clearing */
#define CAUSE_Q850_DSTOOORDR	0x1B	/* Destination out of order */
#define CAUSE_Q850_INVNUFMT	0x1C	/* Invalid number format */
#define CAUSE_Q850_FACREJ	0x1D	/* Facility rejected */
#define CAUSE_Q850_STENQRSP	0x1E	/* Response to STATUS ENQUIRY */
#define CAUSE_Q850_NORMUNSP	0x1F	/* Normal, unspecified */
#define CAUSE_Q850_NOCAVAIL	0x22	/* No circuit / channel available */
#define CAUSE_Q850_NETOOORDR	0x26	/* Network out of order */
#define CAUSE_Q850_PFMCDOOSERV	0x27	/* Permanent frame mode connection out of service */
#define CAUSE_Q850_PFMCOPER	0x28	/* Permanent frame mode connection operational */
#define CAUSE_Q850_TMPFAIL	0x29	/* Temporary failure */
#define CAUSE_Q850_SWEQCONG	0x2A	/* Switching equipment congestion */
#define CAUSE_Q850_ACCINFDIS	0x2B	/* Access information discarded */
#define CAUSE_Q850_REQCNOTAV	0x2C	/* Requested circuit/channel not available */
#define CAUSE_Q850_PRECALBLK	0x2E	/* Precedence call blocked */
#define CAUSE_Q850_RESUNAVAIL	0x2F	/* Resources unavailable, unspecified */
#define CAUSE_Q850_QOSUNAVAIL	0x31	/* Quality of service unavailable */
#define CAUSE_Q850_REQSERVNS	0x32	/* Requested facility not subscribed */
#define CAUSE_Q850_OCBARRCUG	0x35	/* Outgoing calls barred within CUG */
#define CAUSE_Q850_ICBARRCUG	0x36	/* Incoming calls barred within CUG */
#define CAUSE_Q850_BCAPNAUTH	0x39	/* Bearer capability not authorized */
#define CAUSE_Q850_BCAPNAVAIL	0x3A	/* Bearer capability not presently available */
#define CAUSE_Q850_INCSTOACISC	0x3E	/* Inconsistenciy in designated outgoing access information and subscriber class */
#define CAUSE_Q850_SOONOTAVAIL	0x3F	/* Service or option not available, unspecified */
#define CAUSE_Q850_BCAPNOTIMPL	0x41	/* Bearer capability not implemented */
#define CAUSE_Q850_CHTYPNIMPL	0x42	/* Channel type not implemented */
#define CAUSE_Q850_REQFACNIMPL	0x45	/* Requested facility not implemented */
#define CAUSE_Q850_ORDINBCAVL	0x46	/* Only restricted digital information bearer capability is available */
#define CAUSE_Q850_SOONOTIMPL	0x4F	/* Service or option not implemented, unspecified */
#define CAUSE_Q850_INVCLRFVAL	0x51	/* Invalid call reference value */
#define CAUSE_Q850_IDCHDNOEX	0x52	/* Identified channel does not exist */
#define CAUSE_Q850_SUSCAEXIN	0x53	/* A suspended call exists, but this call identity does not */
#define CAUSE_Q850_CLIDINUSE	0x54	/* Call identity in use */
#define CAUSE_Q850_NOCLSUSP	0x55	/* No call suspended */
#define CAUSE_Q850_CLIDCLRD	0x56	/* Call having the requested call identity has been cleared */
#define CAUSE_Q850_UNOTMEMCUG	0x57	/* User not member of CUG */
#define CAUSE_Q850_INCDEST	0x58	/* Incompatible destination */
#define CAUSE_Q850_NONEXCUG	0x5A	/* Non-existent CUG */
#define CAUSE_Q850_INVNTWSEL	0x5B	/* Invalid transit network selection */
#define CAUSE_Q850_INVMSG	0x5F	/* Invalid message, unspecified */
#define CAUSE_Q850_MIEMISS	0x60	/* Mandatory information element is missing */
#define CAUSE_Q850_MSGTNI	0x61	/* Message type non-existent or not implemented */
#define CAUSE_Q850_MSGNCMPT	0x62	/* Message not compatible with call state or message type non-existent or not implemented */
#define CAUSE_Q850_IENENI	0x63	/* Information element/parameter non-existent or not implemented */
#define CAUSE_Q850_INVIEC	0x64	/* Invalid information element contents */
#define CAUSE_Q850_MSGNCWCS	0x65	/* Message not compatible with call state */
#define CAUSE_Q850_RECOTIMEXP	0x66	/* Recovery on timer expiry */
#define CAUSE_Q850_PARMNENIPO	0x67	/* Parameter non-existent or not implemented, passed on */
#define CAUSE_Q850_MSGUNRDPRM	0x6E	/* Message with unrecognized parameter, discarded */
#define CAUSE_Q850_PROTERR	0x6F	/* Protocol error, unspecified */
#define CAUSE_Q850_INTWRKU	0x7F	/* Interworking, unspecified */

#define CAUSE_Q850_MAX		128

#endif /* _I4B_CAUSE_H_ */
