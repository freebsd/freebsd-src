/*
 * Copyright (c) 1997, 2000 Hellmuth Michaelis. All rights reserved.
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
 *	i4b_l2fsm.c - layer 2 FSM
 *	-------------------------
 *
 *	$Id: i4b_l2fsm.c,v 1.22 2000/08/24 11:48:58 hm Exp $ 
 *
 * $FreeBSD$
 *
 *      last edit-date: [Tue May 30 15:48:20 2000]
 *
 *---------------------------------------------------------------------------*/

#ifdef __FreeBSD__
#include "i4bq921.h"
#else
#define	NI4BQ921	1
#endif
#if NI4BQ921 > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <net/if.h>

#if defined(__NetBSD__) && __NetBSD_Version__ >= 104230000
#include <sys/callout.h>
#endif

#ifdef __FreeBSD__
#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>
#else
#include <i4b/i4b_debug.h>
#include <i4b/i4b_ioctl.h>
#endif

#include <i4b/include/i4b_global.h>
#include <i4b/include/i4b_l2l3.h>
#include <i4b/include/i4b_mbuf.h>

#include <i4b/layer2/i4b_l2.h>
#include <i4b/layer2/i4b_l2fsm.h>

l2_softc_t l2_softc[MAXL1UNITS];

#if DO_I4B_DEBUG
static char *l2state_text[N_STATES] = {
	"ST_TEI_UNAS",
	"ST_ASG_AW_TEI",
	"ST_EST_AW_TEI",
	"ST_TEI_ASGD",

	"ST_AW_EST",
	"ST_AW_REL",
	"ST_MULTIFR",
	"ST_TIMREC",

	"ST_SUBSET",
	"Illegal State"
};

static char *l2event_text[N_EVENTS] = {
	"EV_DLESTRQ",
	"EV_DLUDTRQ",
	"EV_MDASGRQ",
	"EV_MDERRRS",
	"EV_PSDEACT",
	"EV_MDREMRQ",
	"EV_RXSABME",
	"EV_RXDISC",
	"EV_RXUA",
	"EV_RXDM",
	"EV_T200EXP",
	"EV_DLDATRQ",
	"EV_DLRELRQ",
	"EV_T203EXP",
	"EV_OWNBUSY",
	"EV_OWNRDY", 
	"EV_RXRR",
	"EV_RXREJ",
	"EV_RXRNR",
	"EV_RXFRMR",
	"Illegal Event"
};
#endif

static void F_TU01 __P((l2_softc_t *));
static void F_TU03 __P((l2_softc_t *));

static void F_TA03 __P((l2_softc_t *));
static void F_TA04 __P((l2_softc_t *));
static void F_TA05 __P((l2_softc_t *));

static void F_TE03 __P((l2_softc_t *));
static void F_TE04 __P((l2_softc_t *));
static void F_TE05 __P((l2_softc_t *));

static void F_T01 __P((l2_softc_t *));
static void F_T05 __P((l2_softc_t *));
static void F_T06 __P((l2_softc_t *));
static void F_T07 __P((l2_softc_t *));
static void F_T08 __P((l2_softc_t *));
static void F_T09 __P((l2_softc_t *));
static void F_T10 __P((l2_softc_t *));
static void F_T13 __P((l2_softc_t *));

static void F_AE01 __P((l2_softc_t *));
static void F_AE05 __P((l2_softc_t *));
static void F_AE06 __P((l2_softc_t *));
static void F_AE07 __P((l2_softc_t *));
static void F_AE08 __P((l2_softc_t *));
static void F_AE09 __P((l2_softc_t *));
static void F_AE10 __P((l2_softc_t *));
static void F_AE11 __P((l2_softc_t *));
static void F_AE12 __P((l2_softc_t *));

static void F_AR05 __P((l2_softc_t *));
static void F_AR06 __P((l2_softc_t *));
static void F_AR07 __P((l2_softc_t *));
static void F_AR08 __P((l2_softc_t *));
static void F_AR09 __P((l2_softc_t *));
static void F_AR10 __P((l2_softc_t *));
static void F_AR11 __P((l2_softc_t *));

static void F_MF01 __P((l2_softc_t *));
static void F_MF05 __P((l2_softc_t *));
static void F_MF06 __P((l2_softc_t *));
static void F_MF07 __P((l2_softc_t *));
static void F_MF08 __P((l2_softc_t *));
static void F_MF09 __P((l2_softc_t *));
static void F_MF10 __P((l2_softc_t *));
static void F_MF11 __P((l2_softc_t *));
static void F_MF12 __P((l2_softc_t *));
static void F_MF13 __P((l2_softc_t *));
static void F_MF14 __P((l2_softc_t *));
static void F_MF15 __P((l2_softc_t *));
static void F_MF16 __P((l2_softc_t *));
static void F_MF17 __P((l2_softc_t *));
static void F_MF18 __P((l2_softc_t *));
static void F_MF19 __P((l2_softc_t *));
static void F_MF20 __P((l2_softc_t *));

static void F_TR01 __P((l2_softc_t *));
static void F_TR05 __P((l2_softc_t *));
static void F_TR06 __P((l2_softc_t *));
static void F_TR07 __P((l2_softc_t *));
static void F_TR08 __P((l2_softc_t *));
static void F_TR09 __P((l2_softc_t *));
static void F_TR10 __P((l2_softc_t *));
static void F_TR11 __P((l2_softc_t *));
static void F_TR12 __P((l2_softc_t *));
static void F_TR13 __P((l2_softc_t *));
static void F_TR15 __P((l2_softc_t *));
static void F_TR16 __P((l2_softc_t *));
static void F_TR17 __P((l2_softc_t *));
static void F_TR18 __P((l2_softc_t *));
static void F_TR19 __P((l2_softc_t *));
static void F_TR20 __P((l2_softc_t *));
static void F_ILL __P((l2_softc_t *));
static void F_NCNA __P((l2_softc_t *));

/*---------------------------------------------------------------------------*
 *	FSM illegal state default action
 *---------------------------------------------------------------------------*/	
static void
F_ILL(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_ERR, "FSM function F_ILL executing");
}

/*---------------------------------------------------------------------------*
 *	FSM No change, No action
 *---------------------------------------------------------------------------*/	
static void
F_NCNA(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_NCNA executing");
}

/*---------------------------------------------------------------------------*
 *	layer 2 state transition table
 *---------------------------------------------------------------------------*/	
struct l2state_tab {
	void (*func) __P((l2_softc_t *));	/* function to execute */
	int newstate;				/* next state */
} l2state_tab[N_EVENTS][N_STATES] = {

/* STATE:	ST_TEI_UNAS,			ST_ASG_AW_TEI,			ST_EST_AW_TEI,			ST_TEI_ASGD,		ST_AW_EST,		ST_AW_REL,		ST_MULTIFR,		ST_TIMREC,		ST_SUBSET,		ILLEGAL STATE	*/
/* -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*EV_DLESTRQ*/{	{F_TU01, ST_EST_AW_TEI},	{F_NCNA, ST_EST_AW_TEI},	{F_ILL,	ST_ILL},		{F_T01,	ST_AW_EST},     {F_AE01, ST_AW_EST},	{F_ILL,	ST_ILL},	{F_MF01, ST_AW_EST},	{F_TR01, ST_AW_EST},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL} },
/*EV_DLUDTRQ*/{	{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL} },
/*EV_MDASGRQ*/{	{F_TU03, ST_TEI_ASGD},		{F_TA03, ST_TEI_ASGD},		{F_TE03, ST_AW_EST},		{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL} },
/*EV_MDERRRS*/{	{F_ILL,	ST_ILL},		{F_TA04, ST_TEI_UNAS},		{F_TE04, ST_TEI_UNAS},		{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL} },
/*EV_PSDEACT*/{	{F_ILL,	ST_ILL},		{F_TA05, ST_TEI_UNAS},		{F_TE05, ST_TEI_UNAS},		{F_T05,	ST_TEI_ASGD},	{F_AE05, ST_TEI_ASGD},	{F_AR05, ST_TEI_ASGD},	{F_MF05, ST_TEI_ASGD},	{F_TR05, ST_TEI_ASGD},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL} },
/*EV_MDREMRQ*/{	{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},		{F_T06,	ST_TEI_UNAS},	{F_AE06, ST_TEI_UNAS},	{F_AR06, ST_TEI_UNAS},	{F_MF06, ST_TEI_UNAS},	{F_TR06, ST_TEI_UNAS},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL} },
/*EV_RXSABME*/{	{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},		{F_T07,	ST_SUBSET},	{F_AE07, ST_AW_EST},	{F_AR07, ST_AW_REL},	{F_MF07, ST_MULTIFR},	{F_TR07, ST_MULTIFR},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL} },
/*EV_RXDISC */{	{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},		{F_T08,	ST_TEI_ASGD},	{F_AE08, ST_AW_EST},	{F_AR08, ST_AW_REL},	{F_MF08, ST_TEI_ASGD},	{F_TR08, ST_TEI_ASGD},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL} },
/*EV_RXUA   */{	{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},		{F_T09,	ST_TEI_ASGD},	{F_AE09, ST_SUBSET},	{F_AR09, ST_SUBSET},	{F_MF09, ST_MULTIFR},	{F_TR09, ST_TIMREC},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL} },
/*EV_RXDM   */{	{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},		{F_T10,	ST_SUBSET},	{F_AE10, ST_SUBSET},	{F_AR10, ST_SUBSET},	{F_MF10, ST_SUBSET},	{F_TR10, ST_AW_EST},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL} },
/*EV_T200EXP*/{	{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},	{F_AE11, ST_SUBSET},	{F_AR11, ST_SUBSET},	{F_MF11, ST_TIMREC},	{F_TR11, ST_SUBSET},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL} },
/*EV_DLDATRQ*/{	{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},	{F_AE12, ST_AW_EST},	{F_ILL,	ST_ILL},	{F_MF12, ST_MULTIFR},	{F_TR12, ST_TIMREC},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL} },
/*EV_DLRELRQ*/{	{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},		{F_T13,	ST_TEI_ASGD},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL},	{F_MF13, ST_AW_REL},	{F_TR13, ST_AW_REL},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL} },
/*EV_T203EXP*/{	{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL},	{F_MF14, ST_TIMREC},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL} },
/*EV_OWNBUSY*/{	{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL},	{F_MF15, ST_MULTIFR},	{F_TR15, ST_TIMREC},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL} },
/*EV_OWNRDY */{	{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL},	{F_MF16, ST_MULTIFR},	{F_TR16, ST_TIMREC},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL} },
/*EV_RXRR   */{	{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL},	{F_MF17, ST_SUBSET},	{F_TR17, ST_SUBSET},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL} },
/*EV_RXREJ  */{	{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL},	{F_MF18, ST_SUBSET},	{F_TR18, ST_SUBSET},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL} },
/*EV_RXRNR  */{	{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL},	{F_MF19, ST_SUBSET},	{F_TR19, ST_SUBSET},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL} },
/*EV_RXFRMR */{	{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL},	{F_MF20, ST_AW_EST},	{F_TR20, ST_AW_EST},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL} },
/*EV_ILL    */{	{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},		{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL},	{F_ILL,	ST_ILL} }
};

/*---------------------------------------------------------------------------*
 *	event handler, executes function and sets new state
 *---------------------------------------------------------------------------*/	
void i4b_next_l2state(l2_softc_t *l2sc, int event)
{
	int currstate, newstate;
	int (*savpostfsmfunc)(int) = NULL;

	/* check event number */
	if(event > N_EVENTS)
		panic("i4b_l2fsm.c: event > N_EVENTS\n");

	/* get current state and check it */
	if((currstate = l2sc->Q921_state) > N_STATES) 	/* failsafe */
		panic("i4b_l2fsm.c: currstate > N_STATES\n");	

	/* get new state and check it */
	if((newstate = l2state_tab[event][currstate].newstate) > N_STATES)
		panic("i4b_l2fsm.c: newstate > N_STATES\n");	
	
	
	if(newstate != ST_SUBSET)
	{	/* state function does NOT set new state */
		NDBGL2(L2_F_MSG, "FSM event [%s]: [%s/%d => %s/%d]",
				l2event_text[event],
                                l2state_text[currstate], currstate,
                                l2state_text[newstate], newstate);
        }

	/* execute state transition function */
        (*l2state_tab[event][currstate].func)(l2sc);

	if(newstate == ST_SUBSET)
	{	/* state function DOES set new state */
		NDBGL2(L2_F_MSG, "FSM S-event [%s]: [%s => %s]", l2event_text[event],
                                           l2state_text[currstate],
                                           l2state_text[l2sc->Q921_state]);
        }
        
	/* check for illegal new state */

	if(newstate == ST_ILL)
	{
		newstate = currstate;
		NDBGL2(L2_F_ERR, "FSM illegal state, state = %s, event = %s!",
                                l2state_text[currstate],
				l2event_text[event]);
	}

	/* check if state machine function has to set new state */

	if(newstate != ST_SUBSET)
		l2sc->Q921_state = newstate;        /* no, we set new state */

	if(l2sc->postfsmfunc != NULL)
	{
		NDBGL2(L2_F_MSG, "FSM executing postfsmfunc!");
		/* try to avoid an endless loop */
		savpostfsmfunc = l2sc->postfsmfunc;
		l2sc->postfsmfunc = NULL;
        	(*savpostfsmfunc)(l2sc->postfsmarg);
        }
}

#if DO_I4B_DEBUG
/*---------------------------------------------------------------------------*
 *	return pointer to current state description
 *---------------------------------------------------------------------------*/	
char *i4b_print_l2state(l2_softc_t *l2sc)
{
	return((char *) l2state_text[l2sc->Q921_state]);
}
#endif

/*---------------------------------------------------------------------------*
 *	FSM state ST_TEI_UNAS event dl establish request
 *---------------------------------------------------------------------------*/	
static void
F_TU01(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_TU01 executing");
	i4b_mdl_assign_ind(l2sc);
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_TEI_UNAS event mdl assign request
 *---------------------------------------------------------------------------*/	
static void
F_TU03(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_TU03 executing");
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_ASG_AW_TEI event mdl assign request
 *---------------------------------------------------------------------------*/	
static void
F_TA03(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_TA03 executing");
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_ASG_AW_TEI event mdl error response
 *---------------------------------------------------------------------------*/	
static void
F_TA04(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_TA04 executing");
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_ASG_AW_TEI event persistent deactivation
 *---------------------------------------------------------------------------*/	
static void
F_TA05(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_TA05 executing");
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_EST_AW_TEI event mdl assign request
 *---------------------------------------------------------------------------*/	
static void
F_TE03(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_TE03 executing");
	i4b_establish_data_link(l2sc);
	l2sc->l3initiated = 1;
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_EST_AW_TEI event mdl error response
 *---------------------------------------------------------------------------*/	
static void
F_TE04(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_TE04 executing");
	l2sc->postfsmarg = l2sc->unit;
	l2sc->postfsmfunc = DL_Rel_Ind_A;
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_EST_AW_TEI event persistent deactivation
 *---------------------------------------------------------------------------*/	
static void
F_TE05(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_TE05 executing");
	l2sc->postfsmarg = l2sc->unit;
	l2sc->postfsmfunc = DL_Rel_Ind_A;
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_TEI_ASGD event dl establish request
 *---------------------------------------------------------------------------*/	
static void
F_T01(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_T01 executing");
	i4b_establish_data_link(l2sc);
	l2sc->l3initiated = 1;
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_TEI_ASGD event persistent deactivation
 *---------------------------------------------------------------------------*/	
static void
F_T05(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_T05 executing");
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_TEI_ASGD event mdl remove request
 *---------------------------------------------------------------------------*/	
static void
F_T06(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_T06 executing");
/*XXX*/	i4b_mdl_assign_ind(l2sc);
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_TEI_ASGD event rx'd SABME
 *---------------------------------------------------------------------------*/	
static void
F_T07(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_T07 executing");

/* XXX */
#ifdef NOTDEF
	if(NOT able to establish)
	{
		i4b_tx_dm(l2sc, l2sc->rxd_PF);
		l2sc->Q921_state = ST_TEI_ASGD;
		return;
	}
#endif

	i4b_clear_exception_conditions(l2sc);

	MDL_Status_Ind(l2sc->unit, STI_L2STAT, LAYER_ACTIVE);
	
	i4b_tx_ua(l2sc, l2sc->rxd_PF);

	l2sc->vs = 0;
	l2sc->va = 0;
	l2sc->vr = 0;	

	l2sc->postfsmarg = l2sc->unit;
	l2sc->postfsmfunc = DL_Est_Ind_A;

	i4b_T203_start(l2sc);	

	l2sc->Q921_state = ST_MULTIFR;
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_TEI_ASGD event rx'd DISC
 *---------------------------------------------------------------------------*/	
static void
F_T08(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_T08 executing");
	MDL_Status_Ind(l2sc->unit, STI_L2STAT, LAYER_IDLE);
	i4b_tx_ua(l2sc, l2sc->rxd_PF);
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_TEI_ASGD event rx'd UA
 *---------------------------------------------------------------------------*/	
static void
F_T09(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_T09 executing");
	i4b_mdl_error_ind(l2sc, "F_T09", MDL_ERR_C);
	i4b_mdl_error_ind(l2sc, "F_T09", MDL_ERR_D);	
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_TEI_ASGD event rx'd DM
 *---------------------------------------------------------------------------*/	
static void
F_T10(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_T10 executing");

	if(l2sc->rxd_PF)
	{
		l2sc->Q921_state = ST_TEI_ASGD;
	}
	else
	{
#ifdef NOTDEF
		if(NOT able_to_etablish)
		{
			l2sc->Q921_state = ST_TEI_ASGD;
			return;
		}
#endif
		i4b_establish_data_link(l2sc);

		l2sc->l3initiated = 1;

		l2sc->Q921_state = ST_AW_EST;
	}
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_TEI_ASGD event dl release request
 *---------------------------------------------------------------------------*/	
static void
F_T13(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_T13 executing");
	l2sc->postfsmarg = l2sc->unit;
	l2sc->postfsmfunc = DL_Rel_Cnf_A;
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_AW_EST event dl establish request
 *---------------------------------------------------------------------------*/	
static void
F_AE01(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_AE01 executing");

	i4b_Dcleanifq(&l2sc->i_queue);
	
	l2sc->l3initiated = 1;
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_AW_EST event persistent deactivation
 *---------------------------------------------------------------------------*/	
static void
F_AE05(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_AE05 executing");

	i4b_Dcleanifq(&l2sc->i_queue);	

	l2sc->postfsmarg = l2sc->unit;
	l2sc->postfsmfunc = DL_Rel_Ind_A;

	i4b_T200_stop(l2sc);
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_AW_EST event mdl remove request
 *---------------------------------------------------------------------------*/	
static void
F_AE06(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_AE06 executing");

	i4b_Dcleanifq(&l2sc->i_queue);	

	l2sc->postfsmarg = l2sc->unit;
	l2sc->postfsmfunc = DL_Rel_Ind_A;

	i4b_T200_stop(l2sc);

/*XXX*/	i4b_mdl_assign_ind(l2sc);
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_AW_EST event rx'd SABME
 *---------------------------------------------------------------------------*/	
static void
F_AE07(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_AE07 executing");
	MDL_Status_Ind(l2sc->unit, STI_L2STAT, LAYER_ACTIVE);
	i4b_tx_ua(l2sc, l2sc->rxd_PF);
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_AW_EST event rx'd DISC
 *---------------------------------------------------------------------------*/	
static void
F_AE08(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_AE08 executing");
	i4b_tx_dm(l2sc, l2sc->rxd_PF);
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_AW_EST event rx'd UA
 *---------------------------------------------------------------------------*/	
static void
F_AE09(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_AE09 executing");

	if(l2sc->rxd_PF == 0)
	{
		i4b_mdl_error_ind(l2sc, "F_AE09", MDL_ERR_D);
		l2sc->Q921_state = ST_AW_EST;
	}
	else
	{
		if(l2sc->l3initiated)
		{
			l2sc->l3initiated = 0;
			l2sc->vr = 0;
			l2sc->postfsmarg = l2sc->unit;
			l2sc->postfsmfunc = DL_Est_Cnf_A;
		}
		else
		{
			if(l2sc->vs != l2sc->va)
			{
				i4b_Dcleanifq(&l2sc->i_queue);
				l2sc->postfsmarg = l2sc->unit;
				l2sc->postfsmfunc = DL_Est_Ind_A;
			}
		}

		MDL_Status_Ind(l2sc->unit, STI_L2STAT, LAYER_ACTIVE);
		
		i4b_T200_stop(l2sc);
		i4b_T203_start(l2sc);

		l2sc->vs = 0;
		l2sc->va = 0;

		l2sc->Q921_state = ST_MULTIFR;
	}
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_AW_EST event rx'd DM
 *---------------------------------------------------------------------------*/	
static void
F_AE10(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_AE10 executing");

	if(l2sc->rxd_PF == 0)
	{
		l2sc->Q921_state = ST_AW_EST;
	}
	else
	{
		i4b_Dcleanifq(&l2sc->i_queue);

		l2sc->postfsmarg = l2sc->unit;
		l2sc->postfsmfunc = DL_Rel_Ind_A;

		i4b_T200_stop(l2sc);

		l2sc->Q921_state = ST_TEI_ASGD;		
	}
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_AW_EST event T200 expiry
 *---------------------------------------------------------------------------*/	
static void
F_AE11(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_AE11 executing");

	if(l2sc->RC >= N200)
	{
		i4b_Dcleanifq(&l2sc->i_queue);

		i4b_mdl_error_ind(l2sc, "F_AE11", MDL_ERR_G);

		l2sc->postfsmarg = l2sc->unit;
		l2sc->postfsmfunc = DL_Rel_Ind_A;

		l2sc->Q921_state = ST_TEI_ASGD;
	}
	else
	{
		l2sc->RC++;

		i4b_tx_sabme(l2sc, P1);

		i4b_T200_start(l2sc);

		l2sc->Q921_state = ST_AW_EST;
	}
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_AW_EST event dl data request
 *---------------------------------------------------------------------------*/	
static void
F_AE12(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_AE12 executing");

	if(l2sc->l3initiated == 0)
	{
		i4b_i_frame_queued_up(l2sc);
	}
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_AW_REL event persistent deactivation
 *---------------------------------------------------------------------------*/	
static void
F_AR05(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_AR05 executing");

	l2sc->postfsmarg = l2sc->unit;
	l2sc->postfsmfunc = DL_Rel_Cnf_A;

	i4b_T200_stop(l2sc);
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_AW_REL event mdl remove request
 *---------------------------------------------------------------------------*/	
static void
F_AR06(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_AR06 executing");

	l2sc->postfsmarg = l2sc->unit;
	l2sc->postfsmfunc = DL_Rel_Cnf_A;

	i4b_T200_stop(l2sc);

/*XXX*/	i4b_mdl_assign_ind(l2sc);	
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_AW_REL event rx'd SABME
 *---------------------------------------------------------------------------*/	
static void
F_AR07(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_AR07 executing");	
	i4b_tx_dm(l2sc, l2sc->rxd_PF);
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_AW_REL event rx'd DISC
 *---------------------------------------------------------------------------*/	
static void
F_AR08(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_AR08 executing");
	MDL_Status_Ind(l2sc->unit, STI_L2STAT, LAYER_IDLE);
	i4b_tx_ua(l2sc, l2sc->rxd_PF);	
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_AW_REL event rx'd UA
 *---------------------------------------------------------------------------*/	
static void
F_AR09(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_AR09 executing");

	if(l2sc->rxd_PF)
	{
		l2sc->postfsmarg = l2sc->unit;
		l2sc->postfsmfunc = DL_Rel_Cnf_A;

		i4b_T200_stop(l2sc);

		l2sc->Q921_state = ST_TEI_ASGD;
	}
	else
	{
		i4b_mdl_error_ind(l2sc, "F_AR09", MDL_ERR_D);
		
		l2sc->Q921_state = ST_AW_REL;
	}
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_AW_REL event rx'd DM
 *---------------------------------------------------------------------------*/	
static void
F_AR10(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_AR10 executing");

	if(l2sc->rxd_PF)
	{
		l2sc->postfsmarg = l2sc->unit;
		l2sc->postfsmfunc = DL_Rel_Cnf_A;

		i4b_T200_stop(l2sc);

		l2sc->Q921_state = ST_TEI_ASGD;
	}
	else
	{
		l2sc->Q921_state = ST_AW_REL;
	}
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_AW_REL event T200 expiry
 *---------------------------------------------------------------------------*/	
static void
F_AR11(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_AR11 executing");

	if(l2sc->RC >= N200)
	{
		i4b_mdl_error_ind(l2sc, "F_AR11", MDL_ERR_H);

		l2sc->postfsmarg = l2sc->unit;
		l2sc->postfsmfunc = DL_Rel_Cnf_A;

		l2sc->Q921_state = ST_TEI_ASGD;
	}
	else
	{
		l2sc->RC++;

		i4b_tx_disc(l2sc, P1);

		i4b_T200_start(l2sc);

		l2sc->Q921_state = ST_AW_REL;
	}
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_MULTIFR event dl establish request
 *---------------------------------------------------------------------------*/	
static void
F_MF01(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_MF01 executing");

	i4b_Dcleanifq(&l2sc->i_queue);

	i4b_establish_data_link(l2sc);
	
	l2sc->l3initiated = 1;
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_MULTIFR event persistent deactivation
 *---------------------------------------------------------------------------*/	
static void
F_MF05(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_MF05 executing");

	i4b_Dcleanifq(&l2sc->i_queue);
	
	l2sc->postfsmarg = l2sc->unit;
	l2sc->postfsmfunc = DL_Rel_Ind_A;
	
	i4b_T200_stop(l2sc);
	i4b_T203_stop(l2sc);
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_MULTIFR event mdl remove request
 *---------------------------------------------------------------------------*/	
static void
F_MF06(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_MF06 executing");

	i4b_Dcleanifq(&l2sc->i_queue);
	
	l2sc->postfsmarg = l2sc->unit;
	l2sc->postfsmfunc = DL_Rel_Ind_A;
	
	i4b_T200_stop(l2sc);
	i4b_T203_stop(l2sc);

/*XXX*/	i4b_mdl_assign_ind(l2sc);	
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_MULTIFR event rx'd SABME
 *---------------------------------------------------------------------------*/	
static void
F_MF07(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_MF07 executing");

	i4b_clear_exception_conditions(l2sc);

	MDL_Status_Ind(l2sc->unit, STI_L2STAT, LAYER_ACTIVE);	

	i4b_tx_ua(l2sc, l2sc->rxd_PF);

	i4b_mdl_error_ind(l2sc, "F_MF07", MDL_ERR_F);

	if(l2sc->vs != l2sc->va)
	{
		i4b_Dcleanifq(&l2sc->i_queue);
	
		l2sc->postfsmarg = l2sc->unit;
		l2sc->postfsmfunc = DL_Est_Ind_A;
	}

	i4b_T200_stop(l2sc);
	i4b_T203_start(l2sc);

	l2sc->vs = 0;
	l2sc->va = 0;
	l2sc->vr = 0;	
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_MULTIFR event rx'd DISC
 *---------------------------------------------------------------------------*/	
static void
F_MF08(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_MF08 executing");

	i4b_Dcleanifq(&l2sc->i_queue);
	MDL_Status_Ind(l2sc->unit, STI_L2STAT, LAYER_IDLE);
	i4b_tx_ua(l2sc, l2sc->rxd_PF);
	
	l2sc->postfsmarg = l2sc->unit;
	l2sc->postfsmfunc = DL_Rel_Ind_A;

	i4b_T200_stop(l2sc);
	i4b_T203_stop(l2sc);
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_MULTIFR event rx'd UA
 *---------------------------------------------------------------------------*/	
static void
F_MF09(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_MF09 executing");
	if(l2sc->rxd_PF)
		i4b_mdl_error_ind(l2sc, "F_MF09", MDL_ERR_C);
	else
		i4b_mdl_error_ind(l2sc, "F_MF09", MDL_ERR_D);
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_MULTIFR event rx'd DM
 *---------------------------------------------------------------------------*/	
static void
F_MF10(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_MF10 executing");

	if(l2sc->rxd_PF)
	{
		i4b_mdl_error_ind(l2sc, "F_MF10", MDL_ERR_B);
		
		l2sc->Q921_state = ST_MULTIFR;
	}
	else
	{
		i4b_mdl_error_ind(l2sc, "F_MF10", MDL_ERR_E);
		
		i4b_establish_data_link(l2sc);

		l2sc->l3initiated = 0;
		
		l2sc->Q921_state = ST_AW_EST;
	}
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_MULTIFR event T200 expiry
 *---------------------------------------------------------------------------*/	
static void
F_MF11(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_MF11 executing");

	l2sc->RC = 0;

	i4b_transmit_enquire(l2sc);

	l2sc->RC++;
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_MULTIFR event dl data request
 *---------------------------------------------------------------------------*/	
static void
F_MF12(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_MF12 executing");

	i4b_i_frame_queued_up(l2sc);
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_MULTIFR event dl release request
 *---------------------------------------------------------------------------*/	
static void
F_MF13(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_MF13 executing");

	i4b_Dcleanifq(&l2sc->i_queue);

	l2sc->RC = 0;

	i4b_tx_disc(l2sc, P1);
	
	i4b_T203_stop(l2sc);
	i4b_T200_restart(l2sc);	
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_MULTIFR event T203 expiry
 *---------------------------------------------------------------------------*/	
static void
F_MF14(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_MF14 executing");

	i4b_transmit_enquire(l2sc);

	l2sc->RC = 0;
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_MULTIFR event set own rx busy
 *---------------------------------------------------------------------------*/	
static void
F_MF15(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_MF15 executing");

	if(l2sc->own_busy == 0)
	{
		l2sc->own_busy = 1;

		i4b_tx_rnr_response(l2sc, F0); /* wrong in Q.921 03/93 p 64 */

		l2sc->ack_pend = 0;
	}
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_MULTIFR event clear own rx busy
 *---------------------------------------------------------------------------*/	
static void
F_MF16(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_MF16 executing");

	if(l2sc->own_busy != 0)
	{
		l2sc->own_busy = 0;

		i4b_tx_rr_response(l2sc, F0); /* wrong in Q.921 03/93 p 64 */

		l2sc->ack_pend = 0;
	}
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_MULTIFR event rx'd RR
 *---------------------------------------------------------------------------*/	
static void
F_MF17(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_MF17 executing");

	l2sc->peer_busy = 0;

	if(l2sc->rxd_CR == CR_CMD_FROM_NT)
	{
		if(l2sc->rxd_PF == 1)
		{
			i4b_enquiry_response(l2sc);
		}
	}
	else
	{
		if(l2sc->rxd_PF == 1)
		{
			i4b_mdl_error_ind(l2sc, "F_MF17", MDL_ERR_A);
		}
	}

	if(i4b_l2_nr_ok(l2sc->rxd_NR, l2sc->va, l2sc->vs))
	{
		if(l2sc->rxd_NR == l2sc->vs)
		{
			l2sc->va = l2sc->rxd_NR;
			i4b_T200_stop(l2sc);
			i4b_T203_restart(l2sc);
		}
		else if(l2sc->rxd_NR != l2sc->va)
		{
			l2sc->va = l2sc->rxd_NR;
			i4b_T200_restart(l2sc);
		}
		l2sc->Q921_state = ST_MULTIFR;
	}
	else
	{
		i4b_nr_error_recovery(l2sc);
		l2sc->Q921_state = ST_AW_EST;
	}
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_MULTIFR event rx'd REJ
 *---------------------------------------------------------------------------*/	
static void
F_MF18(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_MF18 executing");

	l2sc->peer_busy = 0;

	if(l2sc->rxd_CR == CR_CMD_FROM_NT)
	{
		if(l2sc->rxd_PF == 1)
		{
			i4b_enquiry_response(l2sc);
		}
	}
	else
	{
		if(l2sc->rxd_PF == 1)
		{
			i4b_mdl_error_ind(l2sc, "F_MF18", MDL_ERR_A);
		}
	}

	if(i4b_l2_nr_ok(l2sc->rxd_NR, l2sc->va, l2sc->vs))
	{
		l2sc->va = l2sc->rxd_NR;
		i4b_T200_stop(l2sc);
		i4b_T203_start(l2sc);
		i4b_invoke_retransmission(l2sc, l2sc->rxd_NR);
		l2sc->Q921_state = ST_MULTIFR;
	}
	else
	{
		i4b_nr_error_recovery(l2sc);
		l2sc->Q921_state = ST_AW_EST;		
	}
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_MULTIFR event rx'd RNR
 *---------------------------------------------------------------------------*/	
static void
F_MF19(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_MF19 executing");

	l2sc->peer_busy = 1;

	if(l2sc->rxd_CR == CR_CMD_FROM_NT)
	{
		if(l2sc->rxd_PF == 1)
		{
			i4b_enquiry_response(l2sc);
		}
	}
	else
	{
		if(l2sc->rxd_PF == 1)
		{
			i4b_mdl_error_ind(l2sc, "F_MF19", MDL_ERR_A);
                }
        }

	if(i4b_l2_nr_ok(l2sc->rxd_NR, l2sc->va, l2sc->vs))
        {
                l2sc->va = l2sc->rxd_NR;
                i4b_T203_stop(l2sc);
                i4b_T200_restart(l2sc);
		l2sc->Q921_state = ST_MULTIFR;
        }
        else
        {
                i4b_nr_error_recovery(l2sc);
		l2sc->Q921_state = ST_AW_EST;                
        }
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_MULTIFR event rx'd FRMR
 *---------------------------------------------------------------------------*/	
static void
F_MF20(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_MF20 executing");

	i4b_mdl_error_ind(l2sc, "F_MF20", MDL_ERR_K);

	i4b_establish_data_link(l2sc);

	l2sc->l3initiated = 0;
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_TIMREC event dl establish request
 *---------------------------------------------------------------------------*/	
static void
F_TR01(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_TR01 executing");

	i4b_Dcleanifq(&l2sc->i_queue);

	i4b_establish_data_link(l2sc);

	l2sc->l3initiated = 1;
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_TIMREC event persistent deactivation
 *---------------------------------------------------------------------------*/	
static void
F_TR05(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_TR05 executing");

	i4b_Dcleanifq(&l2sc->i_queue);	

	l2sc->postfsmarg = l2sc->unit;
	l2sc->postfsmfunc = DL_Rel_Ind_A;

	i4b_T200_stop(l2sc);
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_TIMREC event mdl remove request
 *---------------------------------------------------------------------------*/	
static void
F_TR06(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_TR06 executing");

	i4b_Dcleanifq(&l2sc->i_queue);

	l2sc->postfsmarg = l2sc->unit;
	l2sc->postfsmfunc = DL_Rel_Ind_A;

	i4b_T200_stop(l2sc);

/*XXX*/	i4b_mdl_assign_ind(l2sc);	
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_TIMREC event rx'd SABME
 *---------------------------------------------------------------------------*/	
static void
F_TR07(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_TR07 executing");

	i4b_clear_exception_conditions(l2sc);

	MDL_Status_Ind(l2sc->unit, STI_L2STAT, LAYER_ACTIVE);
	
	i4b_tx_ua(l2sc, l2sc->rxd_PF);

	i4b_mdl_error_ind(l2sc, "F_TR07", MDL_ERR_F);

	if(l2sc->vs != l2sc->va)
	{
		i4b_Dcleanifq(&l2sc->i_queue);		

		l2sc->postfsmarg = l2sc->unit;
		l2sc->postfsmfunc = DL_Est_Ind_A;
	}

	i4b_T200_stop(l2sc);
	i4b_T203_start(l2sc);
	
	l2sc->vs = 0;
	l2sc->va = 0;
	l2sc->vr = 0;
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_TIMREC event rx'd DISC
 *---------------------------------------------------------------------------*/	
static void
F_TR08(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_TR08 executing");

	i4b_Dcleanifq(&l2sc->i_queue);		
	MDL_Status_Ind(l2sc->unit, STI_L2STAT, LAYER_IDLE);
	i4b_tx_ua(l2sc, l2sc->rxd_PF);

	l2sc->postfsmarg = l2sc->unit;
	l2sc->postfsmfunc = DL_Rel_Ind_A;

	i4b_T200_stop(l2sc);
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_TIMREC event rx'd UA
 *---------------------------------------------------------------------------*/	
static void
F_TR09(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_TR09 executing");
	if(l2sc->rxd_PF)
		i4b_mdl_error_ind(l2sc, "F_TR09", MDL_ERR_C);
	else
		i4b_mdl_error_ind(l2sc, "F_TR09", MDL_ERR_D);	
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_TIMREC event rx'd DM
 *---------------------------------------------------------------------------*/	
static void
F_TR10(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_TR10 executing");

	if(l2sc->rxd_PF)
	{
		i4b_mdl_error_ind(l2sc, "F_TR10", MDL_ERR_B);
	}
	else
	{
		i4b_mdl_error_ind(l2sc, "F_TR10", MDL_ERR_E);
	}

	i4b_establish_data_link(l2sc);

	l2sc->l3initiated = 0;
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_TIMREC event T200 expiry
 *---------------------------------------------------------------------------*/	
static void
F_TR11(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_TR11 executing");

	if(l2sc->RC >= N200)
	{
		i4b_mdl_error_ind(l2sc, "F_TR11", MDL_ERR_I);

		i4b_establish_data_link(l2sc);

		l2sc->l3initiated = 0;

		l2sc->Q921_state = ST_AW_EST;
	}
	else
	{
		i4b_transmit_enquire(l2sc);

		l2sc->RC++;

		l2sc->Q921_state = ST_TIMREC;
	}
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_TIMREC event dl data request
 *---------------------------------------------------------------------------*/	
static void
F_TR12(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_TR12 executing");

	i4b_i_frame_queued_up(l2sc);
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_TIMREC event dl release request
 *---------------------------------------------------------------------------*/	
static void
F_TR13(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_TR13 executing");

	i4b_Dcleanifq(&l2sc->i_queue);			

	l2sc->RC = 0;

	i4b_tx_disc(l2sc, P1);

	i4b_T200_restart(l2sc);
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_TIMREC event set own rx busy
 *---------------------------------------------------------------------------*/	
static void
F_TR15(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_TR15 executing");

	if(l2sc->own_busy == 0)
	{
		l2sc->own_busy = 1;

		i4b_tx_rnr_response(l2sc, F0);

		l2sc->ack_pend = 0;
	}
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_TIMREC event clear own rx busy
 *---------------------------------------------------------------------------*/	
static void
F_TR16(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_TR16 executing");

	if(l2sc->own_busy != 0)
	{
		l2sc->own_busy = 0;

		i4b_tx_rr_response(l2sc, F0);	/* this is wrong	 */
						/* in Q.921 03/93 p 74 ! */
		l2sc->ack_pend = 0;
	}
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_TIMREC event rx'd RR
 *---------------------------------------------------------------------------*/	
static void
F_TR17(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_TR17 executing");

	l2sc->peer_busy = 0;

	if(l2sc->rxd_CR == CR_CMD_FROM_NT)
	{
		if(l2sc->rxd_PF == 1)
		{
			i4b_enquiry_response(l2sc);
		}
	}
	else
	{
		if(l2sc->rxd_PF == 1)
		{
			if(i4b_l2_nr_ok(l2sc->rxd_NR, l2sc->va, l2sc->vs))
			{
				l2sc->va = l2sc->rxd_NR;
				i4b_T200_stop(l2sc);
				i4b_T203_start(l2sc);
				i4b_invoke_retransmission(l2sc, l2sc->rxd_NR);
				l2sc->Q921_state = ST_MULTIFR;
				return;
			}
			else
			{
				i4b_nr_error_recovery(l2sc);
				l2sc->Q921_state = ST_AW_EST;
				return;
			}
		}
	}

	if(i4b_l2_nr_ok(l2sc->rxd_NR, l2sc->va, l2sc->vs))
	{
		l2sc->va = l2sc->rxd_NR;
		l2sc->Q921_state = ST_TIMREC;
	}
	else
	{
		i4b_nr_error_recovery(l2sc);
		l2sc->Q921_state = ST_AW_EST;
	}
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_TIMREC event 
 *---------------------------------------------------------------------------*/	
static void
F_TR18(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_TR18 executing");

	l2sc->peer_busy = 0;

	if(l2sc->rxd_CR == CR_CMD_FROM_NT)
	{
		if(l2sc->rxd_PF == 1)
		{
			i4b_enquiry_response(l2sc);
		}
	}
	else
	{
		if(l2sc->rxd_PF == 1)
		{
			if(i4b_l2_nr_ok(l2sc->rxd_NR, l2sc->va, l2sc->vs))
			{
				l2sc->va = l2sc->rxd_NR;
				i4b_T200_stop(l2sc);
				i4b_T203_start(l2sc);
				i4b_invoke_retransmission(l2sc, l2sc->rxd_NR);
				l2sc->Q921_state = ST_MULTIFR;
				return;
			}
			else
			{
				i4b_nr_error_recovery(l2sc);
				l2sc->Q921_state = ST_AW_EST;
				return;
			}
		}
	}

	if(i4b_l2_nr_ok(l2sc->rxd_NR, l2sc->va, l2sc->vs))
	{
		l2sc->va = l2sc->rxd_NR;
		l2sc->Q921_state = ST_TIMREC;
	}
	else
	{
		i4b_nr_error_recovery(l2sc);
		l2sc->Q921_state = ST_AW_EST;
	}
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_TIMREC event rx'd RNR
 *---------------------------------------------------------------------------*/	
static void
F_TR19(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_TR19 executing");

	l2sc->peer_busy = 0;

	if(l2sc->rxd_CR == CR_CMD_FROM_NT)
	{
		if(l2sc->rxd_PF == 1)
		{
			i4b_enquiry_response(l2sc);
		}
	}
	else
	{
		if(l2sc->rxd_PF == 1)
		{
			if(i4b_l2_nr_ok(l2sc->rxd_NR, l2sc->va, l2sc->vs))
			{
				l2sc->va = l2sc->rxd_NR;
				i4b_T200_restart(l2sc);
				i4b_invoke_retransmission(l2sc, l2sc->rxd_NR);
				l2sc->Q921_state = ST_MULTIFR;
				return;
			}
			else
			{
				i4b_nr_error_recovery(l2sc);
				l2sc->Q921_state = ST_AW_EST;
				return;
			}
		}
	}

	if(i4b_l2_nr_ok(l2sc->rxd_NR, l2sc->va, l2sc->vs))
	{
		l2sc->va = l2sc->rxd_NR;
		l2sc->Q921_state = ST_TIMREC;
	}
	else
	{
		i4b_nr_error_recovery(l2sc);
		l2sc->Q921_state = ST_AW_EST;
	}
}

/*---------------------------------------------------------------------------*
 *	FSM state ST_TIMREC event rx'd FRMR
 *---------------------------------------------------------------------------*/	
static void
F_TR20(l2_softc_t *l2sc)
{
	NDBGL2(L2_F_MSG, "FSM function F_TR20 executing");

	i4b_mdl_error_ind(l2sc, "F_TR20", MDL_ERR_K);

	i4b_establish_data_link(l2sc);

	l2sc->l3initiated = 0;
}
	
#endif /* NI4BQ921 > 0 */
