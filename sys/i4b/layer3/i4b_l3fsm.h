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
 *      i4b_l3fsm.c - layer 3 FSM
 *      -------------------------
 * 
 *	$Id: i4b_l3fsm.h,v 1.7 1999/12/13 21:25:27 hm Exp $ 
 *
 * $FreeBSD: src/sys/i4b/layer3/i4b_l3fsm.h,v 1.6 1999/12/14 20:48:31 hm Exp $
 *
 *      last edit-date: [Mon Dec 13 22:05:09 1999]
 *
 *---------------------------------------------------------------------------*/

#ifndef _I4B_L3FSM_H_
#define _I4B_L3FSM_H_

enum Q931_states {
	ST_U0,
	ST_U1,
	ST_U3,
	ST_U4,
	ST_U6,
	ST_U7,
	ST_U8,
	ST_U9,
	ST_U10,
	ST_U11,
	ST_U12,
	ST_U19,

	ST_IWA,		/* incoming call, wait establish, then accept */
	ST_IWR,		/* incoming call, wait establish, then reject */
	ST_OW,		/* outgoing call, wait establish */
	ST_IWL,		/* incoming call, wait establish, then alert */
	
	ST_SUSE,	/* SUBroutine SETs new state on exit */
	ST_ILL,		/* Illegal */
	
	N_STATES	/* number of states */
};

enum Q931_events {

	EV_SETUPRQ,	/* setup request from L4		*/
	EV_DISCRQ,	/* disconnect request from L4		*/
	EV_RELRQ,	/* release request from L4		*/
	EV_ALERTRQ,	/* alerting request from L4		*/
	EV_SETACRS,	/* setup response accept from l4	*/
	EV_SETRJRS,	/* setup response reject from l4	*/
	EV_SETDCRS,	/* setup response dontcare from l4	*/
	
	EV_SETUP,	/* incoming SETUP message from L2	*/
	EV_STATUS,	/* incoming STATUS message from L2	*/
	EV_RELEASE,	/* incoming RELEASE message from L2	*/
	EV_RELCOMP,	/* incoming RELEASE COMPLETE from L2	*/
	EV_SETUPAK,	/* incoming SETUP ACK message from L2	*/
	EV_CALLPRC,	/* incoming CALL PROCEEDING from L2	*/
	EV_ALERT,	/* incoming ALERT message from L2	*/
	EV_CONNECT,	/* incoming CONNECT message from L2	*/	
	EV_PROGIND,	/* incoming Progress IND from L2	*/
	EV_DISCONN,	/* incoming DISCONNECT message from L2	*/
	EV_CONACK,	/* incoming CONNECT ACK message from L2	*/
	EV_STATENQ,	/* incoming STATUS ENQ message from L2	*/
	EV_INFO,	/* incoming INFO message from L2	*/
	EV_FACILITY,	/* FACILITY message			*/
	
	EV_T303EXP,	/* Timer T303 expired			*/	
	EV_T305EXP,	/* Timer T305 expired			*/
	EV_T308EXP,	/* Timer T308 expired			*/	
	EV_T309EXP,	/* Timer T309 expired			*/	
	EV_T310EXP,	/* Timer T310 expired			*/	
	EV_T313EXP,	/* Timer T313 expired			*/	
	
	EV_DLESTIN,	/* dl establish indication from l2	*/
	EV_DLRELIN,	/* dl release indication from l2	*/
	EV_DLESTCF,	/* dl establish confirm from l2		*/
	EV_DLRELCF,	/* dl release indication from l2	*/
	
	EV_ILL,		/* Illegal */	
	N_EVENTS
};
	
#endif /* _I4B_L3FSM_H_ */
