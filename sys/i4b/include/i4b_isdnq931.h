/*
 * Copyright (c) 1997, 1999 Hellmuth Michaelis. All rights reserved.
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
 *---------------------------------------------------------------------------
 *
 *	i4b_isdnq931.h - DSS1 layer 3 message types
 *	-------------------------------------------
 *
 *	$Id: i4b_isdnq931.h,v 1.6 1999/12/13 21:25:24 hm Exp $
 *
 * $FreeBSD: src/sys/i4b/include/i4b_isdnq931.h,v 1.6 1999/12/14 20:48:15 hm Exp $
 *
 *	last edit-date: [Mon Dec 13 21:44:28 1999]
 *
 *---------------------------------------------------------------------------*/

/* protocol discriminators */

#define PD_Q931		0x08	/* Q.931/I.451				*/

/* Q.931 single octett information element identifiers */

#define IEI_SENDCOMPL	0xa1	/* sending complete			*/

/* Q.931 variable length information element identifiers */

#define IEI_SEGMMSG	0x00	/* segmented message			*/
#define	IEI_BEARERCAP	0x04	/* bearer capabilities			*/
#define	IEI_CAUSE	0x08	/* cause 				*/
#define IEI_CALLID	0x10	/* call identity			*/
#define IEI_CALLSTATE	0x14	/* call state				*/
#define IEI_CHANNELID	0x18	/* channel identification 		*/
#define IEI_PROGRESSI	0x1e	/* progress indicator			*/
#define IEI_NETSPCFAC	0x20	/* network specific facilities		*/
#define IEI_NOTIFIND	0x27	/* notification indicator		*/
#define IEI_DISPLAY	0x28	/* display				*/
#define IEI_DATETIME	0x29	/* date/time				*/
#define IEI_KEYPAD	0x2c	/* keypad facility			*/
#define IEI_SIGNAL	0x34	/* signal				*/
#define IEI_INFRATE	0x40	/* information rate			*/
#define IEI_ETETDEL	0x42	/* end to end transit delay		*/
#define IEI_TDELSELIND	0x43	/* transit delay selection and indication */
#define IEI_PLBPARMS	0x44	/* packet layer binary parameters	*/
#define IEI_PLWSIZE	0x45	/* packet layer window size		*/
#define IEI_PSIZE	0x46	/* packet size				*/
#define IEI_CUG		0x47	/* closed user group			*/
#define IEI_REVCHRGI	0x4a	/* reverse charge indication		*/
#define IEI_CALLINGPN	0x6c	/* calling party number			*/
#define IEI_CALLINGPS	0x6d	/* calling party subaddress		*/
#define IEI_CALLEDPN	0x70	/* called party number			*/
#define IEI_CALLEDPS	0x71	/* called party subaddress		*/
#define IEI_REDIRNO	0x74	/* redirecting number			*/
#define IEI_TRNSEL	0x78	/* transit network selection		*/
#define IEI_RESTARTI	0x79	/* restart indicator			*/
#define IEI_LLCOMPAT	0x7c	/* low layer compatibility		*/
#define IEI_HLCOMPAT	0x7d	/* high layer compatibility		*/
#define IEI_USERUSER	0x7e	/* user-user				*/
#define IEI_ESACPE	0x7f	/* escape for extension			*/

/* Q.932 variable length information element identifiers */

#define IEI_EXTFAC	0x0d	/* extended facility			*/
#define IEI_FACILITY	0x1c	/* facility				*/
#define IEI_INFOREQ	0x32	/* information request 			*/
#define IEI_FEATACT	0x38	/* feature activation 			*/
#define IEI_FEATIND	0x39	/* feature indication			*/
#define IEI_SERVPID	0x3a	/* service profile identification	*/
#define IEI_ENDPTID	0x3b	/* endpoint identifier			*/

/* Q.933 variable length information element identifiers */

#define IEI_DATALCID	0x19	/* data link connection identifier	*/
#define IEI_LLCOREP	0x48	/* link layer core parameters		*/
#define IEI_LLPROTP	0x49	/* link layer protocol parameters	*/
#define IEI_X213PRI	0x50	/* X.213 priority 			*/
#define IEI_REPORTT	0x51	/* report type				*/
#define IEI_LNKITYVERF	0x53	/* link integrity verification		*/
#define IEI_PVCSTAT	0x57	/* PVC status				*/

/* Q.95x variable length information element identifiers */

#define IEI_PRECLEV	0x41	/* precedence level			*/
#define IEI_CONCTDNO	0x4c	/* connected number			*/
#define IEI_CONCTDSA	0x4d	/* connected subaddress			*/
#define IEI_REDICNNO	0x76	/* redirection number			*/

/* single octett information elements */

#define SOIE_SHIFT	0x90	/* shift codeset			*/
#define	 SHIFT_LOCK	0x08	/* shift codeset, locking shift bit	*/
#define SOIE_MDSC	0xa0	/* more data AND/OR sending complete	*/
#define SOIE_SENDC	0xa1	/* sending complete			*/
#define SOIE_CONGL	0xb0	/* congestion level			*/
#define SOIE_REPTI	0xd0	/* repeat indicator			*/

/* codesets */

#define	CODESET_0	0	/* codeset 0, normal DSS1 codeset	*/

/* Q.931/Q.932 message types (see Q.931 03/93 p10 and p311) */

/* call establishment messages */

#define ALERT			0x01
#define CALL_PROCEEDING		0x02
#define PROGRESS		0x03
#define SETUP			0x05
#define CONNECT			0x07
#define SETUP_ACKNOWLEDGE	0x0d
#define CONNECT_ACKNOWLEDGE	0x0f

/* call information phase messages */

#define USER_INFORMATION	0x20
#define SUSPEND_REJECT		0x21
#define RESUME_REJECT		0x22
#define HOLD			0x24
#define SUSPEND			0x25
#define RESUME			0x26
#define HOLD_ACKNOWLEDGE	0x28
#define SUSPEND_ACKNOWLEDGE	0x2d
#define RESUME_ACKNOWLEDGE	0x2e
#define HOLD_REJECT		0x30
#define RETRIEVE		0x31
#define RETRIEVE_ACKNOWLEDGE	0x32
#define RETRIEVE_REJECT		0x37

/* call clearing */

#define DISCONNECT		0x45
#define RESTART			0x46
#define RELEASE			0x4d
#define RESTART_ACKNOWLEDGE	0x4e
#define RELEASE_COMPLETE	0x5a

/* misc messages */

#define SEGMENT			0x60
#define FACILITY		0x62
#define REGISTER		0x64
#define NOTIFY			0x6e
#define STATUS_ENQUIRY		0x75
#define CONGESTION_CONTROL	0x79
#define INFORMATION		0x7b
#define STATUS			0x7d

/* EOF */
