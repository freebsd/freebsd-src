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
 *      i4b_l2fsm.h - layer 2 FSM
 *      -------------------------
 *
 * $FreeBSD$
 *
 *      last edit-date: [Sat Mar  9 17:47:53 2002]
 *
 *---------------------------------------------------------------------------*/

#ifndef _I4B_L2FSM_H_
#define _I4B_L2FSM_H_

enum Q921_states {
	ST_TEI_UNAS,	/* TEI unassigned */
	ST_ASG_AW_TEI,	/* assign awaiting TEI */
	ST_EST_AW_TEI,	/* establish awaiting TEI */
	ST_TEI_ASGD,	/* TEI assigned */

	ST_AW_EST,	/* awaiting establishment */
	ST_AW_REL,	/* awaiting release */
	ST_MULTIFR,	/* multiple frame established */
	ST_TIMREC,	/* timer recovery */

	ST_SUBSET,	/* SUBroutine SETs new state */
	ST_ILL,		/* illegal state */
	N_STATES	/* number of states */
};

enum Q921_events {
	EV_DLESTRQ,	/* dl establish req */
	EV_DLUDTRQ,	/* dl unit data req */
	EV_MDASGRQ,	/* mdl assign req */
	EV_MDERRRS,	/* mdl error response */
	EV_PSDEACT,	/* persistent deactivation */
	EV_MDREMRQ,	/* mdl remove req */
	EV_RXSABME,	/* rx'd SABME */
	EV_RXDISC,	/* rx'd DISC */
	EV_RXUA,	/* rx'd UA */
	EV_RXDM,	/* rx'd DM */
	EV_T200EXP,	/* T200 expired */
	EV_DLDATRQ,	/* dl data req */
	EV_DLRELRQ,	/* dl release req */
	EV_T203EXP,	/* T203 expired */
	EV_OWNBUSY,	/* set own rx busy */
	EV_OWNRDY,	/* clear own rx busy */
	EV_RXRR,	/* rx'd RR */
	EV_RXREJ,	/* rx'd REJ */
	EV_RXRNR,	/* rx'd RNR */
	EV_RXFRMR,	/* rx'd FRMR */

	EV_ILL,		/* Illegal */
	N_EVENTS
};
	
#endif /* _I4B_L2FSM_H_ */
