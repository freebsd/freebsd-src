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
 *	i4b_ifpnp_l1fsm.c - AVM Fritz PnP layer 1 I.430 state machine
 *	-------------------------------------------------------------
 *
 *	$Id: i4b_ifpnp_l1fsm.c,v 1.4 2000/05/29 15:41:41 hm Exp $ 
 *	$Ust: src/i4b/layer1-nb/ifpnp/i4b_ifpnp_l1fsm.c,v 1.4 2000/04/18 08:03:05 ust Exp $
 *
 * $FreeBSD$
 *
 *      last edit-date: [Mon May 29 15:25:04 2000]
 *
 *---------------------------------------------------------------------------*/

#include "ifpnp.h"

#if (NIFPNP > 0)

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/socket.h>

#include <machine/stdarg.h>
#include <machine/clock.h>

#include <net/if.h>

#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>
#include <machine/i4b_trace.h>

#include <i4b/layer1/isic/i4b_isic.h>
#include <i4b/layer1/isic/i4b_isac.h>
#include <i4b/layer1/isic/i4b_hscx.h>

#include <i4b/layer1/i4b_l1.h>

#include <i4b/include/i4b_global.h>

#include <i4b/include/i4b_mbuf.h>

#include <i4b/layer1/ifpnp/i4b_ifpnp_ext.h>

#if DO_I4B_DEBUG
static char *state_text[N_STATES] = {
	"F3 Deactivated",
	"F4 Awaiting Signal",
	"F5 Identifying Input",
	"F6 Synchronized",
	"F7 Activated",
	"F8 Lost Framing",
	"Illegal State"	
};

static char *event_text[N_EVENTS] = {
	"EV_PHAR PH_ACT_REQ",
	"EV_T3 Timer 3 expired",
	"EV_INFO0 INFO0 received",
	"EV_RSY Level Detected",
	"EV_INFO2 INFO2 received",
	"EV_INFO48 INFO4 received",
	"EV_INFO410 INFO4 received",
	"EV_DR Deactivate Req",
	"EV_PU Power UP",
	"EV_DIS Disconnected",
	"EV_EI Error Ind",
	"Illegal Event"
};
#endif

/* Function prototypes */

static void timer3_expired (struct l1_softc *sc);
static void T3_start (struct l1_softc *sc);
static void T3_stop (struct l1_softc *sc);
static void F_T3ex (struct l1_softc *sc);
static void timer4_expired (struct l1_softc *sc);
static void T4_start (struct l1_softc *sc);
static void T4_stop (struct l1_softc *sc);
static void F_AI8 (struct l1_softc *sc);
static void F_AI10 (struct l1_softc *sc);
static void F_I01 (struct l1_softc *sc);
static void F_I02 (struct l1_softc *sc);
static void F_I03 (struct l1_softc *sc);
static void F_I2 (struct l1_softc *sc);
static void F_ill (struct l1_softc *sc);
static void F_NULL (struct l1_softc *sc);

/*---------------------------------------------------------------------------*
 *	I.430 Timer T3 expire function
 *---------------------------------------------------------------------------*/	
static void
timer3_expired(struct l1_softc *sc)
{
	if(sc->sc_I430T3)
	{
		NDBGL1(L1_T_ERR, "state = %s", ifpnp_printstate(sc));
		sc->sc_I430T3 = 0;

		/* XXX try some recovery here XXX */

		ifpnp_recover(sc);

		sc->sc_init_tries++;	/* increment retry count */

/*XXX*/		if(sc->sc_init_tries > 4)
		{
			int s = SPLI4B();

			sc->sc_init_tries = 0;
			
			if(sc->sc_obuf2 != NULL)
			{
				i4b_Dfreembuf(sc->sc_obuf2);
				sc->sc_obuf2 = NULL;
			}
			if(sc->sc_obuf != NULL)
			{
				i4b_Dfreembuf(sc->sc_obuf);
				sc->sc_obuf = NULL;
				sc->sc_freeflag = 0;
				sc->sc_op = NULL;
				sc->sc_ol = 0;
			}

			splx(s);

			i4b_l1_mph_status_ind(L0IFPNPUNIT(sc->sc_unit), STI_NOL1ACC, 0, NULL);
		}
		
		ifpnp_next_state(sc, EV_T3);		
	}
	else
	{
		NDBGL1(L1_T_ERR, "expired without starting it ....");
	}
}

/*---------------------------------------------------------------------------*
 *	I.430 Timer T3 start
 *---------------------------------------------------------------------------*/	
static void
T3_start(struct l1_softc *sc)
{
	NDBGL1(L1_T_MSG, "state = %s", ifpnp_printstate(sc));
	sc->sc_I430T3 = 1;
	sc->sc_T3_callout = timeout((TIMEOUT_FUNC_T)timer3_expired,(struct l1_softc *)sc, 2*hz);
}

/*---------------------------------------------------------------------------*
 *	I.430 Timer T3 stop
 *---------------------------------------------------------------------------*/	
static void
T3_stop(struct l1_softc *sc)
{
	NDBGL1(L1_T_MSG, "state = %s", ifpnp_printstate(sc));

	sc->sc_init_tries = 0;	/* init connect retry count */
	
	if(sc->sc_I430T3)
	{
		sc->sc_I430T3 = 0;
		untimeout((TIMEOUT_FUNC_T)timer3_expired,(struct l1_softc *)sc, sc->sc_T3_callout);
	}
}

/*---------------------------------------------------------------------------*
 *	I.430 Timer T3 expiry
 *---------------------------------------------------------------------------*/	
static void
F_T3ex(struct l1_softc *sc)
{
	NDBGL1(L1_F_MSG, "FSM function F_T3ex executing");
	if(ctrl_desc[sc->sc_unit].protocol != PROTOCOL_D64S)
		i4b_l1_ph_deactivate_ind(L0IFPNPUNIT(sc->sc_unit));
}

/*---------------------------------------------------------------------------*
 *	Timer T4 expire function
 *---------------------------------------------------------------------------*/	
static void
timer4_expired(struct l1_softc *sc)
{
	if(sc->sc_I430T4)
	{
		NDBGL1(L1_T_MSG, "state = %s", ifpnp_printstate(sc));
		sc->sc_I430T4 = 0;
		i4b_l1_mph_status_ind(L0IFPNPUNIT(sc->sc_unit), STI_PDEACT, 0, NULL);
	}
	else
	{
		NDBGL1(L1_T_ERR, "expired without starting it ....");
	}
}

/*---------------------------------------------------------------------------*
 *	Timer T4 start
 *---------------------------------------------------------------------------*/	
static void
T4_start(struct l1_softc *sc)
{
	NDBGL1(L1_T_MSG, "state = %s", ifpnp_printstate(sc));
	sc->sc_I430T4 = 1;
	sc->sc_T4_callout = timeout((TIMEOUT_FUNC_T)timer4_expired,(struct l1_softc *)sc, hz);
}

/*---------------------------------------------------------------------------*
 *	Timer T4 stop
 *---------------------------------------------------------------------------*/	
static void
T4_stop(struct l1_softc *sc)
{
	NDBGL1(L1_T_MSG, "state = %s", ifpnp_printstate(sc));

	if(sc->sc_I430T4)
	{
		sc->sc_I430T4 = 0;
		untimeout((TIMEOUT_FUNC_T)timer4_expired,(struct l1_softc *)sc, sc->sc_T4_callout);
	}
}

/*---------------------------------------------------------------------------*
 *	FSM function: received AI8
 *---------------------------------------------------------------------------*/	
static void
F_AI8(struct l1_softc *sc)
{
	T4_stop(sc);

	NDBGL1(L1_F_MSG, "FSM function F_AI8 executing");

	if(ctrl_desc[sc->sc_unit].protocol != PROTOCOL_D64S)
		i4b_l1_ph_activate_ind(L0IFPNPUNIT(sc->sc_unit));

	T3_stop(sc);

	if(sc->sc_trace & TRACE_I)
	{
		i4b_trace_hdr_t hdr;
		char info = INFO4_8;
		
		hdr.unit = L0IFPNPUNIT(sc->sc_unit);
		hdr.type = TRC_CH_I;
		hdr.dir = FROM_NT;
		hdr.count = 0;
		MICROTIME(hdr.time);
		i4b_l1_trace_ind(&hdr, 1, &info);
	}
}

/*---------------------------------------------------------------------------*
 *	FSM function: received AI10
 *---------------------------------------------------------------------------*/	
static void
F_AI10(struct l1_softc *sc)
{
	T4_stop(sc);
	
	NDBGL1(L1_F_MSG, "FSM function F_AI10 executing");

	if(ctrl_desc[sc->sc_unit].protocol != PROTOCOL_D64S)
		i4b_l1_ph_activate_ind(L0IFPNPUNIT(sc->sc_unit));

	T3_stop(sc);

	if(sc->sc_trace & TRACE_I)
	{
		i4b_trace_hdr_t hdr;
		char info = INFO4_10;
		
		hdr.unit = L0IFPNPUNIT(sc->sc_unit);
		hdr.type = TRC_CH_I;
		hdr.dir = FROM_NT;
		hdr.count = 0;
		MICROTIME(hdr.time);
		i4b_l1_trace_ind(&hdr, 1, &info);
	}
}

/*---------------------------------------------------------------------------*
 *	FSM function: received INFO 0 in states F3 .. F5
 *---------------------------------------------------------------------------*/	
static void
F_I01(struct l1_softc *sc)
{
	NDBGL1(L1_F_MSG, "FSM function F_I01 executing");

	if(sc->sc_trace & TRACE_I)
	{
		i4b_trace_hdr_t hdr;
		char info = INFO0;
		
		hdr.unit = L0IFPNPUNIT(sc->sc_unit);
		hdr.type = TRC_CH_I;
		hdr.dir = FROM_NT;
		hdr.count = 0;
		MICROTIME(hdr.time);
		i4b_l1_trace_ind(&hdr, 1, &info);
	}
}

/*---------------------------------------------------------------------------*
 *	FSM function: received INFO 0 in state F6
 *---------------------------------------------------------------------------*/	
static void
F_I02(struct l1_softc *sc)
{
	NDBGL1(L1_F_MSG, "FSM function F_I02 executing");

	if(ctrl_desc[sc->sc_unit].protocol != PROTOCOL_D64S)
		i4b_l1_ph_deactivate_ind(L0IFPNPUNIT(sc->sc_unit));

	if(sc->sc_trace & TRACE_I)
	{
		i4b_trace_hdr_t hdr;
		char info = INFO0;
		
		hdr.unit = L0IFPNPUNIT(sc->sc_unit);
		hdr.type = TRC_CH_I;
		hdr.dir = FROM_NT;
		hdr.count = 0;
		MICROTIME(hdr.time);
		i4b_l1_trace_ind(&hdr, 1, &info);
	}
}

/*---------------------------------------------------------------------------*
 *	FSM function: received INFO 0 in state F7 or F8
 *---------------------------------------------------------------------------*/	
static void
F_I03(struct l1_softc *sc)
{
	NDBGL1(L1_F_MSG, "FSM function F_I03 executing");

	if(ctrl_desc[sc->sc_unit].protocol != PROTOCOL_D64S)
		i4b_l1_ph_deactivate_ind(L0IFPNPUNIT(sc->sc_unit));

	T4_start(sc);
	
	if(sc->sc_trace & TRACE_I)
	{
		i4b_trace_hdr_t hdr;
		char info = INFO0;
		
		hdr.unit = L0IFPNPUNIT(sc->sc_unit);
		hdr.type = TRC_CH_I;
		hdr.dir = FROM_NT;
		hdr.count = 0;
		MICROTIME(hdr.time);
		i4b_l1_trace_ind(&hdr, 1, &info);
	}
}

/*---------------------------------------------------------------------------*
 *	FSM function: activate request
 *---------------------------------------------------------------------------*/	
static void
F_AR(struct l1_softc *sc)
{
	NDBGL1(L1_F_MSG, "FSM function F_AR executing");

	if(sc->sc_trace & TRACE_I)
	{
		i4b_trace_hdr_t hdr;
		char info = INFO1_8;
		
		hdr.unit = L0IFPNPUNIT(sc->sc_unit);
		hdr.type = TRC_CH_I;
		hdr.dir = FROM_TE;
		hdr.count = 0;
		MICROTIME(hdr.time);
		i4b_l1_trace_ind(&hdr, 1, &info);
	}

	ifpnp_isac_l1_cmd(sc, CMD_AR8);

	T3_start(sc);
}

/*---------------------------------------------------------------------------*
 *	FSM function: received INFO2
 *---------------------------------------------------------------------------*/	
static void
F_I2(struct l1_softc *sc)
{
	NDBGL1(L1_F_MSG, "FSM function F_I2 executing");

	if(sc->sc_trace & TRACE_I)
	{
		i4b_trace_hdr_t hdr;
		char info = INFO2;
		
		hdr.unit = L0IFPNPUNIT(sc->sc_unit);
		hdr.type = TRC_CH_I;
		hdr.dir = FROM_NT;
		hdr.count = 0;
		MICROTIME(hdr.time);
		i4b_l1_trace_ind(&hdr, 1, &info);
	}		

}

/*---------------------------------------------------------------------------*
 *	illegal state default action
 *---------------------------------------------------------------------------*/	
static void
F_ill(struct l1_softc *sc)
{
	NDBGL1(L1_F_ERR, "FSM function F_ill executing");
}

/*---------------------------------------------------------------------------*
 *	No action
 *---------------------------------------------------------------------------*/	
static void
F_NULL(struct l1_softc *sc)
{
	NDBGL1(L1_F_MSG, "FSM function F_NULL executing");
}


/*---------------------------------------------------------------------------*
 *	layer 1 state transition table
 *---------------------------------------------------------------------------*/	
struct ifpnp_state_tab {
	void (*func) (struct l1_softc *sc);	/* function to execute */
	int newstate;				/* next state */
} ifpnp_state_tab[N_EVENTS][N_STATES] = {

/* STATE:	F3			F4			F5			F6			F7			F8			ILLEGAL STATE     */
/* -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EV_PHAR x*/	{{F_AR,   ST_F4},	{F_NULL, ST_F4},	{F_NULL, ST_F5},	{F_NULL, ST_F6},	{F_ill,  ST_ILL},	{F_NULL, ST_F8},	{F_ill, ST_ILL}},
/* EV_T3   x*/	{{F_NULL, ST_F3},	{F_T3ex, ST_F3},	{F_T3ex, ST_F3},	{F_T3ex, ST_F3},	{F_NULL, ST_F7},	{F_NULL, ST_F8},	{F_ill, ST_ILL}},
/* EV_INFO0 */	{{F_I01,  ST_F3},	{F_I01,  ST_F4},	{F_I01,  ST_F5},	{F_I02,  ST_F3},	{F_I03,  ST_F3},	{F_I03,  ST_F3},	{F_ill, ST_ILL}},
/* EV_RSY  x*/	{{F_NULL, ST_F3},	{F_NULL, ST_F5},	{F_NULL, ST_F5}, 	{F_NULL, ST_F8},	{F_NULL, ST_F8},	{F_NULL, ST_F8},	{F_ill, ST_ILL}},
/* EV_INFO2 */	{{F_I2,   ST_F6},	{F_I2,   ST_F6},	{F_I2,   ST_F6},	{F_I2,   ST_F6},	{F_I2,   ST_F6},	{F_I2,   ST_F6},	{F_ill, ST_ILL}},
/* EV_INFO48*/	{{F_AI8,  ST_F7},	{F_AI8,  ST_F7},	{F_AI8,  ST_F7},	{F_AI8,  ST_F7},	{F_NULL, ST_F7},	{F_AI8,  ST_F7},	{F_ill, ST_ILL}},
/* EV_INFO41*/	{{F_AI10, ST_F7},	{F_AI10, ST_F7},	{F_AI10, ST_F7},	{F_AI10, ST_F7},	{F_NULL, ST_F7},	{F_AI10, ST_F7},	{F_ill, ST_ILL}},
/* EV_DR    */	{{F_NULL, ST_F3},	{F_NULL, ST_F4},	{F_NULL, ST_F5},	{F_NULL, ST_F6},	{F_NULL, ST_F7},	{F_NULL, ST_F8},	{F_ill, ST_ILL}},
/* EV_PU    */	{{F_NULL, ST_F3},	{F_NULL, ST_F4},	{F_NULL, ST_F5},	{F_NULL, ST_F6},	{F_NULL, ST_F7},	{F_NULL, ST_F8},	{F_ill, ST_ILL}},
/* EV_DIS   */	{{F_ill,  ST_ILL},	{F_ill,  ST_ILL},	{F_ill,  ST_ILL},	{F_ill,  ST_ILL},	{F_ill,  ST_ILL},	{F_ill,  ST_ILL},	{F_ill, ST_ILL}},
/* EV_EI    */	{{F_NULL, ST_F3},	{F_NULL, ST_F3},	{F_NULL, ST_F3},	{F_NULL, ST_F3},	{F_NULL, ST_F3},	{F_NULL, ST_F3},	{F_ill, ST_ILL}},
/* EV_ILL   */	{{F_ill,  ST_ILL},	{F_ill,  ST_ILL},	{F_ill,  ST_ILL},	{F_ill,  ST_ILL},	{F_ill,  ST_ILL},	{F_ill,  ST_ILL},	{F_ill, ST_ILL}}
};

/*---------------------------------------------------------------------------*
 *	event handler
 *---------------------------------------------------------------------------*/	
void
ifpnp_next_state(struct l1_softc *sc, int event)
{
	int currstate, newstate;

	if(event >= N_EVENTS)
		panic("i4b_l1fsm.c: event >= N_EVENTS\n");

	currstate = sc->sc_I430state;

	if(currstate >= N_STATES)
		panic("i4b_l1fsm.c: currstate >= N_STATES\n");	

	newstate = ifpnp_state_tab[event][currstate].newstate;

	if(newstate >= N_STATES)
		panic("i4b_l1fsm.c: newstate >= N_STATES\n");	
	
	NDBGL1(L1_F_MSG, "FSM event [%s]: [%s => %s]", event_text[event],
                                           state_text[currstate],
                                           state_text[newstate]);

        (*ifpnp_state_tab[event][currstate].func)(sc);

	if(newstate == ST_ILL)
	{
		newstate = ST_F3;
		NDBGL1(L1_F_ERR, "FSM Illegal State ERROR, oldstate = %s, newstate = %s, event = %s!",
					state_text[currstate],
					state_text[newstate],
					event_text[event]);
	}

	sc->sc_I430state = newstate;
}

#if DO_I4B_DEBUG
/*---------------------------------------------------------------------------*
 *	return pointer to current state description
 *---------------------------------------------------------------------------*/	
char *
ifpnp_printstate(struct l1_softc *sc)
{
	return((char *) state_text[sc->sc_I430state]);
}
#endif
	
#endif /* NIFPNP > 0 */
