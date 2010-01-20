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
 *	i4b_q931.h - Q931 handling header file
 *	--------------------------------------
 *
 * $FreeBSD$
 *
 *      last edit-date: [Tue Mar 26 15:04:33 2002]
 *
 *---------------------------------------------------------------------------*/

#ifndef _I4B_Q931_H_
#define _I4B_Q931_H_

/* extension bit */

#define EXT_LAST		0x80	/* last octett */

/* reserve space in mbuf */

#define I_FRAME_HDRLEN		4	/* to be added by layer 2 */

/* SHIFT */

#define CODESET_MASK		0x07
#define UNSHIFTED		0
#define SHIFTED			1
#define CRLENGTH_MASK		0x0f

/* CONNECT */

#define MSG_CONNECT_LEN		4	/* length of a connect message */

/* DISCONNECT */

#define MSG_DISCONNECT_LEN	8	/* length of a disconnect message */

/* RELEASE COMPLETE */

#define MSG_RELEASE_COMPLETE_LEN 8	/* length of release complete msg */

/* for outgoing causes */

#define CAUSE_LEN		2
#define CAUSE_STD_LOC_OUT	0x80	/* std = CCITT, loc = user */

/* SETUP */

#define MSG_SETUP_LEN		12	/* without called party,	*/
					/*	calling party and	*/
					/*	keypad facility !	*/

#define IEI_BEARERCAP_LEN	2	/* 2 octetts length */

#define IT_CAP_SPEECH		0x80	/* BC: information xfer capability */
#define IT_CAP_UNR_DIG_INFO	0x88	/* BC: information xfer capability */

#define	IT_RATE_64K		0x90	/* BC: information xfer rate	*/
#define	IT_UL1_G711A		0xa3	/* layer1 proto G.711 A-law	*/

#define IEI_CHANNELID_LEN	0x01    /* length of channel id		*/
#define	CHANNELID_B1		0x81	/* channel = B1 (outgoing)	*/
#define	CHANNELID_B2		0x82	/* channel = B2 (outgoing) 	*/
#define	CHANNELID_ANY		0x83	/* channel = any channel (outgoing) */

#define IE_CHAN_ID_NO		0x00	/* no channel (incoming)	*/
#define IE_CHAN_ID_B1		0x01	/* B1 channel (incoming)	*/
#define IE_CHAN_ID_B2		0x02	/* B2 channel (incoming)	*/
#define IE_CHAN_ID_ANY		0x03	/* ANY channel (incoming)	*/

#define	NUMBER_TYPEPLAN		0x81    /* type of number/numbering plan */

#define IEI_CALLINGPN_LEN	1	/* without number string !	*/
#define IEI_CALLEDPN_LEN	1	/* without number string !	*/

#define IEI_CALLINGPS_LEN	1
#define IEI_CALLEDPS_LEN	1

#define	SUBADDR_TYPE_NSAP	0x80	/* subaddr: type=NSAP		*/

/* CONNECT_ACK */

#define MSG_CONNECT_ACK_LEN	4	/* length of a connect ack message */

/* STATUS */

#define MSG_STATUS_LEN		11
#define CALLSTATE_LEN		1

/* RELEASE */

#define MSG_RELEASE_LEN		8	/* length of release msg */

/* ALERT */

#define MSG_ALERT_LEN		4	/* length of an alert message */

#endif /* _I4B_Q931_H_ */

/* EOF */
