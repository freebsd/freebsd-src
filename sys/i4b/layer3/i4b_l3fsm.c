/*
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
 *
 *---------------------------------------------------------------------------
 *
 *	i4b_l3fsm.c - layer 3 FSM
 *	-------------------------
 *      last edit-date: [Sat Mar  9 19:34:39 2002]
 *
 *---------------------------------------------------------------------------*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>

#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>
#include <machine/i4b_cause.h>

#include <i4b/include/i4b_l2l3.h>
#include <i4b/include/i4b_l3l4.h>
#include <i4b/include/i4b_global.h>

#include <i4b/layer3/i4b_l3.h>
#include <i4b/layer3/i4b_l3fsm.h>

#include <i4b/layer4/i4b_l4.h>

static void F_00A(call_desc_t *cd), F_00H(call_desc_t *cd), F_00I(call_desc_t *cd);
static void F_00J(call_desc_t *cd);

static void F_01B(call_desc_t *cd), F_01K(call_desc_t *cd), F_01L(call_desc_t *cd);
static void F_01M(call_desc_t *cd), F_01N(call_desc_t *cd), F_01U(call_desc_t *cd);
static void F_01O(call_desc_t *cd);

static void F_03C(call_desc_t *cd), F_03N(call_desc_t *cd), F_03O(call_desc_t *cd);
static void F_03P(call_desc_t *cd), F_03Y(call_desc_t *cd);

static void F_04O(call_desc_t *cd);

static void F_06D(call_desc_t *cd), F_06E(call_desc_t *cd), F_06F(call_desc_t *cd);
static void F_06G(call_desc_t *cd), F_06J(call_desc_t *cd), F_06Q(call_desc_t *cd);

static void F_07E(call_desc_t *cd), F_07F(call_desc_t *cd), F_07G(call_desc_t *cd);

static void F_08R(call_desc_t *cd), F_08Z(call_desc_t *cd);

static void F_09D(call_desc_t *cd), F_09E(call_desc_t *cd), F_09F(call_desc_t *cd);
static void F_09G(call_desc_t *cd);

static void F_11J(call_desc_t *cd), F_11Q(call_desc_t *cd), F_11V(call_desc_t *cd);

static void F_12C(call_desc_t *cd), F_12J(call_desc_t *cd);

static void F_19I(call_desc_t *cd), F_19J(call_desc_t *cd), F_19K(call_desc_t *cd);
static void F_19W(call_desc_t *cd);

static void F_NCNA(call_desc_t *cd), F_STENQ(call_desc_t *cd), F_STAT(call_desc_t *cd);
static void F_INFO(call_desc_t *cd), F_RELCP(call_desc_t *cd), F_REL(call_desc_t *cd);
static void F_DISC(call_desc_t *cd), F_DCRQ(call_desc_t *cd), F_UEM(call_desc_t *cd);
static void F_SIGN(call_desc_t *cd), F_DLEI(call_desc_t *cd), F_ILL(call_desc_t *cd);
static void F_309TO(call_desc_t *cd), F_DECF(call_desc_t *cd), F_FCTY(call_desc_t *cd);
static void F_DECF1(call_desc_t *cd), F_DECF2(call_desc_t *cd), F_DECF3(call_desc_t *cd);
static void F_DLRI(call_desc_t *cd), F_DLRIA(call_desc_t *cd), F_DECF4(call_desc_t *cd);

static void F_308TO(call_desc_t *cd);

#if DO_I4B_DEBUG
static char *l3state_text[N_STATES] = {
	 "ST_U0 - Null",
	 "ST_U1 - Out Init",
	 "ST_U3 - Out Proc",
	 "ST_U4 - Out Delv",
	 "ST_U6 - In Pres",
	 "ST_U7 - In Rxd",
	 "ST_U8 - In ConReq",
	 "ST_U9 - In Proc",
	"ST_U10 - Active",
	"ST_U11 - Disc Req",
	"ST_U12 - Disc Ind",
	"ST_U19 - Rel Req",

	"ST_IWA - In Wait EST-Accept",
	"ST_IWR - In Wait EST-Reject",
	"ST_OW - Out Wait EST",
	"ST_IWL - In Wait EST-Alert",	

	"ST_SUSE - Subroutine sets state",	

	"Illegal State"
};

static char *l3event_text[N_EVENTS] = {
	"EV_SETUPRQ - L4 SETUP REQ",	/* setup request from L4		*/
	"EV_DISCRQ - L4 DISC REQ",	/* disconnect request from L4		*/
	"EV_RELRQ - L4 REL REQ",	/* release request from L4		*/
	"EV_ALERTRQ - L4 ALERT REQ",	/* alerting request from L4		*/
	"EV_SETACRS - L4 accept RSP",	/* setup response accept from l4	*/
	"EV_SETRJRS - L4 reject RSP",	/* setup response reject from l4	*/
	"EV_SETDCRS - L4 ignore RSP",	/* setup response dontcare from l4	*/
	
	"EV_SETUP - rxd SETUP",		/* incoming SETUP message from L2	*/
	"EV_STATUS - rxd STATUS",	/* incoming STATUS message from L2	*/
	"EV_RELEASE - rxd REL",		/* incoming RELEASE message from L2	*/
	"EV_RELCOMP - rxd REL COMPL",	/* incoming RELEASE COMPLETE from L2	*/
	"EV_SETUPAK - rxd SETUP ACK",	/* incoming SETUP ACK message from L2	*/
	"EV_CALLPRC - rxd CALL PROC",	/* incoming CALL PROCEEDING from L2	*/
	"EV_ALERT - rxd ALERT",		/* incoming ALERT message from L2	*/
	"EV_CONNECT - rxd CONNECT",	/* incoming CONNECT message from L2	*/	
	"EV_PROGIND - rxd PROG IND",	/* incoming Progress IND from L2	*/
	"EV_DISCONN - rxd DISC",	/* incoming DISCONNECT message from L2	*/
	"EV_CONACK - rxd CONN ACK",	/* incoming CONNECT ACK message from L2	*/
	"EV_STATENQ - rxd STAT ENQ",	/* incoming STATUS ENQ message from L2	*/
	"EV_INFO - rxd INFO",		/* incoming INFO message from L2	*/
	"EV_FACILITY - rxd FACILITY",	/* incoming FACILITY message 		*/
	
	"EV_T303EXP - T303 timeout",	/* Timer T303 expired			*/	
	"EV_T305EXP - T305 timeout",	/* Timer T305 expired			*/
	"EV_T308EXP - T308 timeout",	/* Timer T308 expired			*/	
	"EV_T309EXP - T309 timeout",	/* Timer T309 expired			*/	
	"EV_T310EXP - T310 timeout",	/* Timer T310 expired			*/	
	"EV_T313EXP - T313 timeout",	/* Timer T313 expired			*/	
	
	"EV_DLESTIN - L2 DL_Est_Ind",	/* dl establish indication from l2	*/
	"EV_DLRELIN - L2 DL_Rel_Ind",	/* dl release indication from l2	*/
	"EV_DLESTCF - L2 DL_Est_Cnf",	/* dl establish confirm from l2		*/
	"EV_DLRELCF - L2 DL_Rel_Cnf",	/* dl release confirm from l2		*/	

	"EV_ILL - Illegal event!!" 	/* Illegal */
};
#endif

/*---------------------------------------------------------------------------*
 *	layer 3 state transition table
 *---------------------------------------------------------------------------*/	
struct l3state_tab {
	void (*func) (call_desc_t *);	/* function to execute */
	int newstate;				/* next state */
} l3state_tab[N_EVENTS][N_STATES] = {

/* STATE:	ST_U0			ST_U1			ST_U3			ST_U4			ST_U6			ST_U7			ST_U8			ST_U9			ST_U10			ST_U11			ST_U12			ST_U19			ST_IWA			ST_IWR			ST_OW			ST_IWL			ST_SUBSET		ST_ILL	      */
/* ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*EV_SETUPRQ*/	{{F_00A,  ST_SUSE},	{F_ILL,	 ST_ILL},	{F_ILL,  ST_ILL},	{F_ILL,  ST_ILL},	{F_ILL,  ST_ILL},	{F_ILL,  ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,  ST_ILL},	{F_ILL,  ST_ILL},	{F_ILL,  ST_ILL},	{F_ILL,  ST_ILL},	{F_ILL, ST_ILL},        {F_ILL, ST_ILL}},
/*EV_DISCRQ */	{{F_ILL,  ST_ILL},	{F_01B,	 ST_U11},	{F_DCRQ, ST_U11},	{F_DCRQ, ST_U11},	{F_ILL,  ST_ILL},	{F_DCRQ, ST_U11},	{F_DCRQ, ST_U11},	{F_DCRQ, ST_U11},	{F_DCRQ, ST_U11},	{F_ILL,	 ST_ILL},	{F_NCNA, ST_U12},	{F_ILL,	 ST_ILL},	{F_DCRQ, ST_U11},	{F_DCRQ, ST_U11},	{F_DCRQ, ST_U11},	{F_DCRQ, ST_U11},	{F_ILL,	ST_ILL},        {F_ILL, ST_ILL}},
/*EV_RELRQ  */	{{F_ILL,  ST_ILL},	{F_ILL,	 ST_ILL},	{F_03C,  ST_U19},	{F_ILL,  ST_ILL},	{F_ILL,  ST_ILL},	{F_ILL,  ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_12C,	 ST_U19},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	ST_ILL},        {F_ILL, ST_ILL}},
/*EV_ALERTRQ*/	{{F_ILL,  ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,  ST_ILL},	{F_ILL,  ST_ILL},	{F_06D,  ST_SUSE},	{F_ILL,  ST_ILL},	{F_ILL,	 ST_ILL},	{F_09D,	 ST_U7},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	ST_ILL},        {F_ILL, ST_ILL}},
/*EV_SETACRS*/	{{F_ILL,  ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,  ST_ILL},	{F_ILL,  ST_ILL},	{F_06E,  ST_SUSE},	{F_07E,	 ST_U8},	{F_ILL,	 ST_ILL},	{F_09E,	 ST_U8},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	ST_ILL},        {F_ILL, ST_ILL}},
/*EV_SETRJRS*/	{{F_ILL,  ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,  ST_ILL},	{F_ILL,  ST_ILL},	{F_06F,  ST_SUSE},	{F_07F,	 ST_U0},	{F_ILL,	 ST_ILL},	{F_09F,	 ST_U0},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	ST_ILL},        {F_ILL, ST_ILL}},
/*EV_SETDCRS*/	{{F_ILL,  ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,  ST_ILL},	{F_ILL,  ST_ILL},	{F_06G,  ST_U0},	{F_07G,	 ST_U0},	{F_ILL,	 ST_ILL},	{F_09G,	 ST_U0},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	ST_ILL},        {F_ILL, ST_ILL}},
/* STATE:	ST_U0			ST_U1			ST_U3			ST_U4			ST_U6			ST_U7			ST_U8			ST_U9			ST_U10			ST_U11			ST_U12			ST_U19			ST_IWA			ST_IWR			ST_OW			ST_IWL			ST_SUBSET		ST_ILL	      */
/* ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*EV_SETUP  */	{{F_00H,  ST_U6},	{F_SIGN, ST_U1},	{F_SIGN, ST_U3},	{F_SIGN, ST_U4},	{F_SIGN, ST_U6},	{F_SIGN, ST_U7},	{F_SIGN, ST_U8},	{F_SIGN, ST_U9},	{F_SIGN, ST_U10},	{F_SIGN, ST_U11},	{F_SIGN, ST_U12},	{F_SIGN, ST_U19},	{F_SIGN, ST_IWA},	{F_SIGN, ST_IWR},	{F_SIGN, ST_OW},	{F_SIGN, ST_IWL},	{F_ILL,	ST_ILL},        {F_ILL, ST_ILL}},
/*EV_STATUS */	{{F_00I,  ST_SUSE},	{F_STAT, ST_SUSE},	{F_STAT, ST_SUSE},	{F_STAT, ST_SUSE},	{F_STAT, ST_SUSE},	{F_STAT, ST_SUSE},	{F_STAT, ST_SUSE},	{F_STAT, ST_SUSE},	{F_STAT, ST_SUSE},	{F_STAT, ST_SUSE},	{F_STAT, ST_SUSE},	{F_19I,	 ST_SUSE},	{F_STAT, ST_SUSE},	{F_STAT, ST_SUSE},	{F_STAT, ST_SUSE},	{F_STAT, ST_SUSE},	{F_ILL,	ST_ILL},        {F_ILL, ST_ILL}},
/*EV_RELEASE*/	{{F_00J,  ST_U0},	{F_UEM,	 ST_SUSE},	{F_REL,  ST_U0},	{F_REL,  ST_U0},	{F_06J,	 ST_U0},	{F_REL,	 ST_U0},	{F_REL,	 ST_U0},	{F_REL,	 ST_U0},	{F_REL,	 ST_U0},	{F_11J,	 ST_U0},	{F_12J,	 ST_U0},	{F_19J,	 ST_U0},	{F_REL,	 ST_U0},	{F_REL,	 ST_U0},	{F_REL,	 ST_U0},	{F_REL,  ST_U0},	{F_ILL,	ST_ILL},        {F_ILL, ST_ILL}},
/*EV_RELCOMP*/	{{F_NCNA, ST_U0},	{F_01K,	 ST_U0},	{F_RELCP,ST_U0},	{F_RELCP,ST_U0},	{F_RELCP,ST_U0},	{F_RELCP,ST_U0},	{F_RELCP,ST_U0},	{F_RELCP,ST_U0},	{F_RELCP,ST_U0},	{F_RELCP,ST_U0},	{F_RELCP,ST_U0},	{F_19K,	 ST_U0},	{F_RELCP,ST_U0},	{F_RELCP,ST_U0},	{F_RELCP,ST_U0},	{F_RELCP,ST_U0},	{F_ILL,	ST_ILL},        {F_ILL, ST_ILL}},
/*EV_SETUPAK*/	{{F_UEM,  ST_SUSE},	{F_01L,	 ST_U3},	{F_UEM,  ST_SUSE},	{F_UEM,  ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,  ST_SUSE},	{F_ILL,	ST_ILL},        {F_ILL, ST_ILL}},
/*EV_CALLPRC*/	{{F_UEM,  ST_SUSE},	{F_01M,	 ST_U3},	{F_NCNA, ST_U3},	{F_UEM,  ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,  ST_SUSE},	{F_ILL,	ST_ILL},        {F_ILL, ST_ILL}},
/*EV_ALERT  */	{{F_UEM,  ST_SUSE},	{F_01N,	 ST_U4},	{F_03N,  ST_U4},	{F_UEM,  ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,  ST_SUSE},	{F_ILL,	ST_ILL},        {F_ILL, ST_ILL}},
/*EV_CONNECT*/	{{F_UEM,  ST_SUSE},	{F_01O,	 ST_U10},	{F_03O,  ST_U10},	{F_04O,  ST_U10},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,  ST_SUSE},	{F_ILL,	ST_ILL},        {F_ILL, ST_ILL}},
/*EV_PROGIND*/	{{F_UEM,  ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_03P,  ST_U3},	{F_UEM,  ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,  ST_SUSE},	{F_ILL,	ST_ILL},        {F_ILL, ST_ILL}},
/*EV_DISCONN*/	{{F_UEM,  ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_DISC, ST_U12},	{F_DISC, ST_U12},	{F_06Q,	 ST_U12},	{F_DISC, ST_U12},	{F_DISC, ST_U12},	{F_DISC, ST_U12},	{F_DISC, ST_U12},	{F_11Q,	 ST_U19},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_DISC, ST_U12},	{F_DISC, ST_U12},	{F_DISC, ST_U12},	{F_DISC, ST_U12},	{F_ILL,	ST_ILL},        {F_ILL, ST_ILL}},
/*EV_CONACK */	{{F_UEM,  ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,  ST_SUSE},	{F_UEM,  ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_08R,	 ST_U10},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,	 ST_SUSE},	{F_UEM,  ST_SUSE},	{F_ILL,	ST_ILL},        {F_ILL, ST_ILL}},
/*EV_STATENQ*/	{{F_STENQ,ST_U0},	{F_STENQ,ST_U1},	{F_STENQ,ST_U3},	{F_STENQ,ST_U4},	{F_STENQ,ST_U6},	{F_STENQ,ST_U7},	{F_STENQ,ST_U8},	{F_STENQ,ST_U9},	{F_STENQ,ST_U10},	{F_STENQ,ST_U11},	{F_STENQ,ST_U12},	{F_STENQ,ST_U19},	{F_STENQ,ST_IWA},	{F_STENQ,ST_IWR},	{F_STENQ,ST_OW},	{F_STENQ,ST_OW},	{F_ILL,	ST_ILL},        {F_ILL, ST_ILL}},
/*EV_INFO   */	{{F_UEM,  ST_SUSE},	{F_UEM,  ST_SUSE},	{F_INFO, ST_U3},	{F_INFO, ST_U4},	{F_UEM,	 ST_SUSE},	{F_INFO, ST_U7},	{F_INFO, ST_U8},	{F_INFO, ST_U9},	{F_INFO, ST_U10},	{F_INFO, ST_U11},	{F_INFO, ST_U12},	{F_UEM,  ST_SUSE},	{F_INFO, ST_IWA},	{F_INFO, ST_IWR},	{F_INFO, ST_OW},	{F_INFO, ST_OW},	{F_ILL,	ST_ILL},        {F_ILL, ST_ILL}},
/*EV_FACILITY*/	{{F_FCTY, ST_SUSE},	{F_FCTY, ST_SUSE},	{F_FCTY, ST_SUSE},	{F_FCTY, ST_SUSE},	{F_FCTY, ST_SUSE},	{F_FCTY, ST_SUSE},	{F_FCTY, ST_SUSE},	{F_FCTY, ST_SUSE},	{F_FCTY, ST_SUSE},	{F_FCTY, ST_SUSE},	{F_FCTY, ST_SUSE},	{F_FCTY, ST_SUSE},	{F_FCTY, ST_SUSE},	{F_FCTY, ST_SUSE},	{F_FCTY, ST_SUSE},	{F_FCTY, ST_SUSE},	{F_ILL,	ST_ILL},        {F_ILL, ST_ILL}},
/* STATE:	ST_U0			ST_U1			ST_U3			ST_U4			ST_U6			ST_U7			ST_U8			ST_U9			ST_U10			ST_U11			ST_U12			ST_U19			ST_IWA			ST_IWR			ST_OW			ST_IWL			ST_SUBSET		ST_ILL	      */
/* ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*EV_T303EXP*/	{{F_ILL,  ST_ILL},	{F_01U,	 ST_SUSE},	{F_ILL,  ST_ILL},	{F_ILL,  ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	ST_ILL},        {F_ILL, ST_ILL}},
/*EV_T305EXP*/	{{F_ILL,  ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,  ST_ILL},	{F_ILL,  ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_11V,	 ST_U19},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	ST_ILL},        {F_ILL, ST_ILL}},
/*EV_T308EXP*/	{{F_ILL,  ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,  ST_ILL},	{F_ILL,  ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_308TO,ST_ILL},	{F_19W,	 ST_SUSE},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	ST_ILL},        {F_ILL, ST_ILL}},
/*EV_T309EXP*/	{{F_309TO,ST_U0},	{F_309TO,ST_U0},	{F_309TO,ST_U0},	{F_309TO,ST_U0},	{F_309TO,ST_U0},	{F_309TO,ST_U0},	{F_309TO,ST_U0},	{F_309TO,ST_U0},	{F_309TO,ST_U0},	{F_309TO,ST_U0},	{F_309TO,ST_U0},	{F_309TO,ST_U0},	{F_309TO,ST_U0},	{F_309TO,ST_U0},	{F_309TO,ST_U0},	{F_309TO,ST_U0},	{F_ILL,	ST_ILL},        {F_ILL, ST_ILL}},
/*EV_T310EXP*/	{{F_ILL,  ST_ILL},	{F_ILL,	 ST_ILL},	{F_03Y,  ST_U11},	{F_ILL,  ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	ST_ILL},        {F_ILL, ST_ILL}},
/*EV_T313EXP*/	{{F_ILL,  ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,  ST_ILL},	{F_ILL,  ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_08Z,	 ST_U11},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	ST_ILL},        {F_ILL, ST_ILL}},
/* STATE:	ST_U0			ST_U1			ST_U3			ST_U4			ST_U6			ST_U7			ST_U8			ST_U9			ST_U10			ST_U11			ST_U12			ST_U19			ST_IWA			ST_IWR			ST_OW			ST_IWL			ST_SUBSET		ST_ILL	      */
/* ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*EV_DLESTIN*/	{{F_ILL,  ST_ILL},	{F_DLEI, ST_U1},	{F_DLEI, ST_U3},	{F_DLEI, ST_U4},	{F_DLEI, ST_U1},	{F_DLEI, ST_U1},	{F_DLEI, ST_U1},	{F_DLEI, ST_U1},	{F_DLEI, ST_U1},	{F_DLEI, ST_U1},	{F_DLEI, ST_U1},	{F_DLEI, ST_U1},	{F_DLEI, ST_U1},	{F_DLEI, ST_U1},	{F_DLEI, ST_U1},	{F_DLEI, ST_U1},	{F_ILL,	ST_ILL},        {F_ILL, ST_ILL}},
/*EV_DLRELIN*/	{{F_NCNA, ST_U0},	{F_DLRIA, ST_OW},	{F_DLRI, ST_U0},	{F_DLRI, ST_U0},	{F_DLRI, ST_U0},	{F_DLRI, ST_U0},	{F_DLRI, ST_U0},	{F_DLRI, ST_U0},	{F_DLRIA,ST_U10},	{F_DLRI, ST_U0},	{F_DLRI, ST_U0},	{F_DLRI, ST_U0},	{F_DLRI, ST_U0},	{F_DLRI, ST_U0},	{F_DLRI, ST_U0},	{F_DLRI, ST_U0},	{F_ILL,	ST_ILL},        {F_ILL, ST_ILL}},
/*EV_DLESTCF*/	{{F_DECF, ST_SUSE},	{F_DECF, ST_SUSE},	{F_DECF, ST_SUSE},	{F_DECF, ST_SUSE},	{F_DECF, ST_SUSE},	{F_DECF, ST_SUSE},	{F_DECF, ST_SUSE},	{F_DECF, ST_SUSE},	{F_DECF, ST_SUSE},	{F_DECF, ST_SUSE},	{F_DECF, ST_SUSE},	{F_DECF, ST_SUSE},	{F_DECF2,ST_U8},	{F_DECF3,ST_U0},	{F_DECF1,ST_U1},	{F_DECF4,ST_U7},	{F_ILL,	ST_ILL},        {F_ILL, ST_ILL}},
/*EV_DLRELCF*/	{{F_ILL,  ST_ILL},	{F_ILL,  ST_ILL},	{F_ILL,  ST_ILL},	{F_ILL,  ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	ST_ILL},        {F_ILL, ST_ILL}},
/*EV_ILL    */	{{F_ILL,  ST_ILL},	{F_ILL,  ST_ILL},	{F_ILL,  ST_ILL},	{F_ILL,  ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	 ST_ILL},	{F_ILL,	ST_ILL},        {F_ILL, ST_ILL}}
};

/*---------------------------------------------------------------------------*
 *	event handler
 *---------------------------------------------------------------------------*/	
void next_l3state(call_desc_t *cd, int event)
{
	int currstate, newstate;

	if(event >= N_EVENTS)
		panic("i4b_l3fsm.c: event >= N_EVENTS\n");

	currstate = cd->Q931state;

	if(currstate >= N_STATES)
		panic("i4b_l3fsm.c: currstate >= N_STATES\n");	

	newstate = l3state_tab[event][currstate].newstate;

	if(newstate > N_STATES)
		panic("i4b_l3fsm.c: newstate > N_STATES\n");	
	
	NDBGL3(L3_F_MSG, "L3 FSM event [%s]: [%s => %s]",
				l3event_text[event],
                                l3state_text[currstate],
                                l3state_text[newstate]);

	/* execute function */
	
        (*l3state_tab[event][currstate].func)(cd);

	if(newstate == ST_ILL)
	{
		newstate = currstate;
		NDBGL3(L3_F_ERR, "FSM illegal state, state = %s, event = %s!",
				l3state_text[newstate],
				l3event_text[event]);
	}

	if(newstate != ST_SUSE)	
		cd->Q931state = newstate;        
}

#if DO_I4B_DEBUG
/*---------------------------------------------------------------------------*
 *	return pointer to current state description
 *---------------------------------------------------------------------------*/	
char *print_l3state(call_desc_t *cd)
{
	return((char *) l3state_text[cd->Q931state]);
}
#endif

/*---------------------------------------------------------------------------*
 *	L3 FSM state U0 event L4 setup req
 *---------------------------------------------------------------------------*/	
static void F_00A(call_desc_t *cd)
{
	int s;
	NDBGL3(L3_F_MSG, "FSM function F_00A executing");

	cd->T303_first_to = 1;
	T303_start(cd);

	s = SPLI4B();
	if(i4b_get_dl_stat(cd) == DL_DOWN)
	{
		splx(s);
		DL_Est_Req(ctrl_desc[cd->controller].unit);
		cd->Q931state = ST_OW;
	}
	else
	{
		cd->Q931state = ST_U1;
		splx(s);
		i4b_l3_tx_setup(cd);
	}		
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U0 event SETUP from L2
 *---------------------------------------------------------------------------*/	
static void F_00H(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_00H executing");
	i4b_l4_connect_ind(cd);	/* tell l4 we have an incoming setup */	
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U0 event STATUS from L2
 *---------------------------------------------------------------------------*/	
static void F_00I(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_00I executing");

	if(cd->call_state != 0)
	{
		cd->cause_out = 101;
		i4b_l3_tx_release_complete(cd, 1);	/* 1 = send cause */
	}
	cd->Q931state = ST_U0;
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U0 event RELEASE from L2
 *---------------------------------------------------------------------------*/	
static void F_00J(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_00J executing");
	i4b_l3_tx_release_complete(cd, 0);	/* 0 = don't send cause */	
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U1 event disconnect req from L4
 *---------------------------------------------------------------------------*/	
static void F_01B(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_01B executing");
	/* cause from L4 */
	i4b_l3_tx_disconnect(cd);
	T303_stop(cd);
	T305_start(cd);	
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U1 event RELEASE COMPLETE from L2
 *---------------------------------------------------------------------------*/	
static void F_01K(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_01K executing");
	T303_stop(cd);
	i4b_l4_disconnect_ind(cd);	/* tell l4 we were rejected */
	freecd_by_cd(cd);	
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U1 event SETUP ACK from L2
 *---------------------------------------------------------------------------*/	
static void F_01L(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_01L executing");
	T303_stop(cd);

	/*
	 * since this implementation does NOT support overlap sending,
	 * we react here as if we received a CALL PROCEEDING because
	 * several PBX's react with a SETUP ACK even if the called
	 * number is complete AND we sent a SENDING COMPLETE in the
	 * preceding SETUP message. (-hm)
	 */

	T310_start(cd);
	i4b_l4_proceeding_ind(cd);
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U1 event CALL PROCEEDING from L2
 *---------------------------------------------------------------------------*/	
static void F_01M(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_01M executing");
	T303_stop(cd);
	T310_start(cd);
	i4b_l4_proceeding_ind(cd);
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U1 event ALERT from L2  (XXX !)
 *---------------------------------------------------------------------------*/	
static void F_01N(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_01N executing");
	T303_stop(cd);
	i4b_l4_alert_ind(cd);
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U1 event CONNECT from L2 (XXX !)
 *---------------------------------------------------------------------------*/	
static void F_01O(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_01O executing");
	T303_stop(cd);
	i4b_l3_tx_connect_ack(cd);
	i4b_l4_connect_active_ind(cd);
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U1 event T303 timeout
 *---------------------------------------------------------------------------*/	
static void F_01U(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_01U executing");
	if(cd->T303_first_to == 1)
	{
		cd->T303_first_to = 0;
		i4b_l3_tx_setup(cd);
		T303_start(cd);
		cd->Q931state = ST_U1;
	}
	else
	{
		i4b_l4_disconnect_ind(cd);
		freecd_by_cd(cd);
		cd->Q931state = ST_U0;
	}
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U3 event release req from L4
 *---------------------------------------------------------------------------*/	
static void F_03C(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_03C executing");
	T310_stop(cd);
	cd->cause_out = 6;
	i4b_l3_tx_release(cd, 1);	/* 0 = don't send cause */	
	cd->T308_first_to = 1;
	T308_start(cd);
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U3 event ALERT from L2
 *---------------------------------------------------------------------------*/	
static void F_03N(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_03N executing");
	T310_stop(cd);
	i4b_l4_alert_ind(cd);
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U3 event CONNECT from L2
 *---------------------------------------------------------------------------*/	
static void F_03O(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_03O executing");
	T310_stop(cd);
	i4b_l3_tx_connect_ack(cd);	/* CONNECT ACK to network */
	i4b_l4_connect_active_ind(cd);
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U3 event PROGESS IND from L2
 *---------------------------------------------------------------------------*/	
static void F_03P(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_03P executing");
	T310_stop(cd);
#ifdef NOTDEF
	i4b_l4_progress_ind(cd);
#endif	
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U3 event T310 timeout
 *---------------------------------------------------------------------------*/	
static void F_03Y(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_03Y executing");
	cd->cause_out = 102;	/* recovery on timer expiry */
	i4b_l3_tx_disconnect(cd);
	T305_start(cd);
	i4b_l4_disconnect_ind(cd);
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U4 event CONNECT from L2
 *---------------------------------------------------------------------------*/	
static void F_04O(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_04O executing");
	i4b_l3_tx_connect_ack(cd);	/* CONNECT ACK to network */		
	i4b_l4_connect_active_ind(cd);
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U6 event alert req from L4
 *---------------------------------------------------------------------------*/	
static void F_06D(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_06D executing");

	if(i4b_get_dl_stat(cd) == DL_DOWN)
	{	
		DL_Est_Req(ctrl_desc[cd->controller].unit);
		cd->Q931state = ST_IWL;
	}
	else
	{
		i4b_l3_tx_alert(cd);
		cd->Q931state = ST_U7;
	}
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U6 event incoming setup accept from L4
 *---------------------------------------------------------------------------*/	
static void F_06E(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_06E executing");

	if(i4b_get_dl_stat(cd) == DL_DOWN)
	{	
		DL_Est_Req(ctrl_desc[cd->controller].unit);
		cd->Q931state = ST_IWA;		
	}
	else
	{
		i4b_l3_tx_connect(cd);
		cd->Q931state = ST_U8;
	}
	T313_start(cd);		
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U6 event incoming setup reject from L4
 *---------------------------------------------------------------------------*/	
static void F_06F(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_06F executing");

	if(i4b_get_dl_stat(cd) == DL_DOWN)
	{	
		DL_Est_Req(ctrl_desc[cd->controller].unit);
		cd->Q931state = ST_IWR;		
	}
	else
	{
		int s = SPLI4B();
		i4b_l3_tx_release_complete(cd, 1);
		cd->Q931state = ST_U0;
		freecd_by_cd(cd);
		splx(s);
	}
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U6 event incoming setup ignore from L4
 *---------------------------------------------------------------------------*/	
static void F_06G(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_06G executing");
	freecd_by_cd(cd);
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U6 event RELEASE from L2
 *---------------------------------------------------------------------------*/	
static void F_06J(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_06J executing");
	i4b_l3_tx_release_complete(cd, 0);
	i4b_l4_disconnect_ind(cd);	
	freecd_by_cd(cd);	
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U6 event DISCONNECT from L2
 *---------------------------------------------------------------------------*/	
static void F_06Q(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_06Q executing");
	i4b_l4_disconnect_ind(cd);
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U7 event setup response accept from L4
 *---------------------------------------------------------------------------*/	
static void F_07E(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_07E executing");
	i4b_l3_tx_connect(cd);
	T313_start(cd);
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U7 event setup response reject from L4
 *---------------------------------------------------------------------------*/	
static void F_07F(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_07F executing");
	i4b_l3_tx_release_complete(cd, 1);
	freecd_by_cd(cd);
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U7 event setup response ignore from L4
 *---------------------------------------------------------------------------*/	
static void F_07G(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_07G executing");
	freecd_by_cd(cd);
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U8 event CONNECT ACK from L2
 *---------------------------------------------------------------------------*/	
static void F_08R(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_08R executing");
	T313_stop(cd);
	i4b_l4_connect_active_ind(cd);
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U8 event T313 timeout
 *---------------------------------------------------------------------------*/	
static void F_08Z(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_08Z executing");
	cd->cause_out = 102;	/* recovery on timer expiry */
	i4b_l3_tx_disconnect(cd);
	T305_start(cd);
	i4b_l4_disconnect_ind(cd);
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U9 event alert req from L4
 *---------------------------------------------------------------------------*/	
static void F_09D(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_09D executing");
	i4b_l3_tx_alert(cd);
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U9 event setup response accept from L4
 *---------------------------------------------------------------------------*/	
static void F_09E(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_09E executing");
	i4b_l3_tx_connect(cd);
	T313_start(cd);
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U9 event setup response reject from L4
 *---------------------------------------------------------------------------*/	
static void F_09F(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_09F executing");
	i4b_l3_tx_release_complete(cd, 1);
	freecd_by_cd(cd);
}
/*---------------------------------------------------------------------------*
 *	L3 FSM state U9 event setup response ignore from L4
 *---------------------------------------------------------------------------*/	
static void F_09G(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_09G executing");
	freecd_by_cd(cd);	
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U11 event RELEASE from L2
 *---------------------------------------------------------------------------*/	
static void F_11J(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_11J executing");
	T305_stop(cd);
	i4b_l3_tx_release_complete(cd, 0);
	i4b_l4_disconnect_ind(cd);
	freecd_by_cd(cd);
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U11 event DISCONNECT from L2
 *---------------------------------------------------------------------------*/	
static void F_11Q(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_11Q executing");
	T305_stop(cd);
	i4b_l3_tx_release(cd, 0);
	cd->T308_first_to = 1;
	T308_start(cd);
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U11 event T305 timeout
 *---------------------------------------------------------------------------*/	
static void F_11V(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_11V executing");
	cd->cause_out = 102;
	i4b_l3_tx_release(cd, 1);
	cd->T308_first_to = 1;
	T308_start(cd);
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U12 event release req from L4
 *---------------------------------------------------------------------------*/	
static void F_12C(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_12C executing");
	i4b_l3_tx_release(cd, 1);
	cd->T308_first_to = 1;
	T308_start(cd);
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U12 event RELEASE from L2
 *---------------------------------------------------------------------------*/	
static void F_12J(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_12J executing");
	i4b_l3_tx_release_complete(cd, 0);
	i4b_l4_disconnect_ind(cd);	
	freecd_by_cd(cd);
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U19 event STATUS from L2
 *---------------------------------------------------------------------------*/	
static void F_19I(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_19I executing");

	if(cd->call_state == 0)
	{
		i4b_l4_status_ind(cd);
		freecd_by_cd(cd);
		cd->Q931state = ST_U0;
	}
	else
	{
		cd->Q931state = ST_U19;
	}
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U19 event RELEASE from L2
 *---------------------------------------------------------------------------*/	
static void F_19J(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_19J executing");
	T308_stop(cd);
	i4b_l4_disconnect_ind(cd);
	freecd_by_cd(cd);
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U19 event RELEASE COMPLETE from L2
 *---------------------------------------------------------------------------*/	
static void F_19K(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_19K executing");
	T308_stop(cd);
	i4b_l4_disconnect_ind(cd);
	freecd_by_cd(cd);
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U19 event T308 timeout
 *---------------------------------------------------------------------------*/	
static void F_19W(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_19W executing");
	if(cd->T308_first_to == 0)
	{
		cd->T308_first_to = 1;
		i4b_l3_tx_release(cd, 0);
		T308_start(cd);
		cd->Q931state = ST_U19;
	}
	else
	{
		cd->T308_first_to = 0;
		i4b_l4_disconnect_ind(cd);
		freecd_by_cd(cd);
		cd->Q931state = ST_U0;
	}
}

/*---------------------------------------------------------------------------*
 *	L3 FSM routine no change no action
 *---------------------------------------------------------------------------*/	
static void F_NCNA(call_desc_t *cd)
{
}

/*---------------------------------------------------------------------------*
 *	L3 FSM any state event STATUS ENQ from L2
 *---------------------------------------------------------------------------*/	
static void F_STENQ(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_STENQ executing");
	i4b_l3_tx_status(cd, CAUSE_Q850_STENQRSP); /* 30, resonse to stat enq */
}

/*---------------------------------------------------------------------------*
 *	L3 FSM any state except 0 & 19 event STATUS from L2
 *---------------------------------------------------------------------------*/	
static void F_STAT(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_STAT executing");
	if(cd->call_state == 0)
	{
		i4b_l4_status_ind(cd);
		cd->Q931state = ST_U0;		
		freecd_by_cd(cd);
	}
	else
	{
		/* XXX !!!!!!!!!!!!!!!!!! */
		
		i4b_l4_status_ind(cd);
		cd->cause_out = 101;	/* message not compatible with call state */
		i4b_l3_tx_disconnect(cd);
		T305_start(cd);
		cd->Q931state = ST_U11;
	}
}

/*---------------------------------------------------------------------------*
 *	L3 FSM some states event INFORMATION from L2
 *---------------------------------------------------------------------------*/	
static void F_INFO(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_INFO executing");
	i4b_l4_info_ind(cd);
	/* remain in current state */
}

/*---------------------------------------------------------------------------*
 *	L3 FSM T308 timeout while expecting RELEASE COMPLETE
 *---------------------------------------------------------------------------*/	
static void F_308TO(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_308TO executing");
	i4b_l3_stop_all_timers(cd);
	i4b_l4_disconnect_ind(cd);
	freecd_by_cd(cd);
}

/*---------------------------------------------------------------------------*
 *	L3 FSM some states event RELEASE COMPLETE from L2
 *---------------------------------------------------------------------------*/	
static void F_RELCP(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_RELCP executing");
	i4b_l3_stop_all_timers(cd);
	i4b_l4_disconnect_ind(cd);
	freecd_by_cd(cd);
}

/*---------------------------------------------------------------------------*
 *	L3 FSM some states event RELEASE from L2
 *---------------------------------------------------------------------------*/	
static void F_REL(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_REL executing");
	i4b_l3_stop_all_timers(cd);
	i4b_l3_tx_release_complete(cd, 0);
	i4b_l4_disconnect_ind(cd);	
	freecd_by_cd(cd);
}

/*---------------------------------------------------------------------------*
 *	L3 FSM some states event DISCONNECT from L2
 *---------------------------------------------------------------------------*/	
static void F_DISC(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_DISC executing");
	i4b_l3_stop_all_timers(cd);

	/*
	 * no disconnect ind to L4, no jump to state U12
	 * instead we issue a RELEASE and jump to U19
	 */

	i4b_l3_tx_release(cd, 0);
	cd->T308_first_to = 1;	
	T308_start(cd);
	cd->Q931state = ST_U19;
}

/*---------------------------------------------------------------------------*
 *	L3 FSM some states event disconnect request from L4
 *---------------------------------------------------------------------------*/	
static void F_DCRQ(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_DCRQ executing");

	/* stop T310 in case this is the result of an incoming call for a */
	/* calledback connection */

	if(cd->T310 == TIMER_ACTIVE)
		T310_stop(cd);
		
	/* cause from L4 */
	i4b_l3_tx_disconnect(cd);
	T305_start(cd);
	cd->Q931state = ST_U11;	
}

/*---------------------------------------------------------------------------*
 *	L3 FSM any state except 0 event unexpected message from L2
 *---------------------------------------------------------------------------*/	
static void F_UEM(call_desc_t *cd)
{
	NDBGL3(L3_F_ERR, "FSM function F_UEM executing, state = %s", print_l3state(cd));
	i4b_l3_tx_status(cd, CAUSE_Q850_MSGNCWCS); /* 101, message not compatible with call state */
}

/*---------------------------------------------------------------------------*
 *	L3 FSM any state except 0 event SETUP from L2
 *---------------------------------------------------------------------------*/	
static void F_SIGN(call_desc_t *cd)
{
	NDBGL3(L3_F_ERR, "FSM function F_SIGN executing");

/* XXX */ /* freecd_by_cd(cd); ?????????? XXX */
}

/*---------------------------------------------------------------------------*
 *	L3 FSM relevant states event DL ESTABLISH IND from L2
 *---------------------------------------------------------------------------*/	
static void F_DLEI(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_DLEI executing");

/* XXX */

	/* remain in current state */
}

/*---------------------------------------------------------------------------*
 *	L3 FSM any state event illegal event occured
 *---------------------------------------------------------------------------*/	
static void F_ILL(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_ILL executing");
}

/*---------------------------------------------------------------------------*
 *	L3 FSM any state event T309 timeout
 *---------------------------------------------------------------------------*/	
static void F_309TO(call_desc_t *cd)
{
	NDBGL3(L3_F_ERR, "FSM function F_309TO executing");

/* XXX */

#ifdef NOTDEF
	i4b_l4_dl_fail_ind(cd);
#endif	

	freecd_by_cd(cd);
}

/*---------------------------------------------------------------------------*
 *	L3 FSM any state event FACILITY message received
 *---------------------------------------------------------------------------*/	
static void F_FCTY(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_FCTY executing");
	/* ST_SUSE, no change in state ! */
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state ST_OW event DL ESTABLISH CONF from L2
 *---------------------------------------------------------------------------*/	
static void F_DECF1(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_DECF1 executing");
	i4b_l3_tx_setup(cd);
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state ST_IWA event DL ESTABLISH CONF from L2
 *---------------------------------------------------------------------------*/	
static void F_DECF2(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_DECF2 executing");
	i4b_l3_tx_connect(cd);
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state ST_IWR event DL ESTABLISH CONF from L2
 *---------------------------------------------------------------------------*/	
static void F_DECF3(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_DECF3 executing");
	i4b_l3_tx_release_complete(cd, 1);
	freecd_by_cd(cd);
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state ST_IWL event DL ESTABLISH CONF from L2
 *---------------------------------------------------------------------------*/	
static void F_DECF4(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_DECF4 executing");
	i4b_l3_tx_alert(cd);
}


/*---------------------------------------------------------------------------*
 *	L3 FSM any state event DL ESTABLISH CONF from L2
 *---------------------------------------------------------------------------*/	
static void F_DECF(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_DECF executing");
	T309_stop(cd);
	i4b_l3_tx_status(cd, CAUSE_Q850_NORMUNSP); /* 31, normal unspecified */
}

/*---------------------------------------------------------------------------*
 *	L3 FSM any state except U10 event DL RELEASE IND from L2
 *---------------------------------------------------------------------------*/	
static void F_DLRI(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_DLRI executing");
	i4b_l3_stop_all_timers(cd);
	i4b_l4_disconnect_ind(cd);
	freecd_by_cd(cd);
}

/*---------------------------------------------------------------------------*
 *	L3 FSM state U10 event DL RELEASE IND from L2
 *---------------------------------------------------------------------------*/	
static void F_DLRIA(call_desc_t *cd)
{
	NDBGL3(L3_F_MSG, "FSM function F_DLRIA executing");

	if(cd->T309 == TIMER_IDLE)
		T309_start(cd);

	DL_Est_Req(ctrl_desc[cd->controller].unit);
}
