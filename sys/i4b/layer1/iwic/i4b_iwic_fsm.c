/*-
 * Copyright (c) 1999, 2000 Dave Boyce. All rights reserved.
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
 *      i4b_iwic - isdn4bsd Winbond W6692 driver
 *      ----------------------------------------
 *      last edit-date: [Sun Jan 21 11:09:24 2001]
 *
 *---------------------------------------------------------------------------*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/i4b/layer1/iwic/i4b_iwic_fsm.c,v 1.9 2007/07/06 07:17:21 bz Exp $");

#include "opt_i4b.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <net/if.h>


#include <i4b/include/i4b_debug.h>
#include <i4b/include/i4b_ioctl.h>
#include <i4b/include/i4b_trace.h>

#include <i4b/layer1/i4b_l1.h>

#include <i4b/include/i4b_l3l4.h>

#include <i4b/layer1/iwic/i4b_iwic.h>
#include <i4b/layer1/iwic/i4b_w6692.h>

#if DO_I4B_DEBUG
static char *state_names[] = {
	"F3N",
	"F3",
	"F4",
	"F5",
	"F6",
	"F7",
	"F8",
	"ILLEGAL",
};

static char *event_names[] = {
	"PHAR",
	"CE",
	"T3",
	"INFO0",
	"RSY",
	"INFO2",
	"INFO48",
	"INFO410",
	"DR",
	"PU",
	"DIS",
	"EI",
	"ILLEGAL"
};
#endif

/*---------------------------------------------------------------------------*
 *
 *---------------------------------------------------------------------------*/
static void
F_NULL(struct iwic_softc *sc)
{
	NDBGL1(L1_F_MSG, "FSM function F_NULL executing");
}

/*---------------------------------------------------------------------------*
 *
 *---------------------------------------------------------------------------*/
static void
F_AR(struct iwic_softc *sc)
{
	NDBGL1(L1_F_MSG, "FSM function F_AR executing");
	IWIC_WRITE(sc, CIX, CIX_ECK);
}

/*---------------------------------------------------------------------------*
 *
 *---------------------------------------------------------------------------*/
static void
F_AR3(struct iwic_softc *sc)
{
	NDBGL1(L1_F_MSG, "FSM function F_AR3 executing");
	IWIC_WRITE(sc, CIX, CIX_AR8);
}

/*---------------------------------------------------------------------------*
 *
 *---------------------------------------------------------------------------*/
static void
F_I0I(struct iwic_softc *sc)
{
	NDBGL1(L1_F_MSG, "FSM function F_IOI executing");
}

/*---------------------------------------------------------------------------*
 *
 *---------------------------------------------------------------------------*/
static void
F_I0A(struct iwic_softc *sc)
{
	NDBGL1(L1_F_MSG, "FSM function F_IOA executing");
	iwic_dchan_disable(sc);
	i4b_l1_ph_deactivate_ind(L0IWICUNIT(sc->sc_unit));
}

/*---------------------------------------------------------------------------*
 *
 *---------------------------------------------------------------------------*/
static void
F_AI8(struct iwic_softc *sc)
{
	NDBGL1(L1_F_MSG, "FSM function F_AI8 executing");
	iwic_dchan_transmit(sc);
	i4b_l1_ph_activate_ind(L0IWICUNIT(sc->sc_unit));
}

/*---------------------------------------------------------------------------*
 *
 *---------------------------------------------------------------------------*/
static void
F_AI10(struct iwic_softc *sc)
{
	NDBGL1(L1_F_MSG, "FSM function F_AI10 executing");
	iwic_dchan_transmit(sc);
	i4b_l1_ph_activate_ind(L0IWICUNIT(sc->sc_unit));
}

/*---------------------------------------------------------------------------*
 *
 *---------------------------------------------------------------------------*/
struct iwic_state_tab {
	void (*func) (struct iwic_softc *sc);	/* function to execute */
	int newstate;				/* next state */
} iwic_state_tab[N_EVENTS][N_STATES] = {

/* STATE:       F3N                  F3                  F4                  F5                  F6                  F7                  F8                  ILLEGAL STATE        */
/* ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ */
/* EV_PHAR   */ {{F_AR,   ST_F3  },  {F_AR3,  ST_F4  },  {F_NULL, ST_F4  },  {F_NULL, ST_F5  },  {F_NULL, ST_F6  },  {F_NULL, ST_F7  },  {F_NULL, ST_F8  },  {F_NULL, ST_ILL }},
/* EV_CE     */ {{F_NULL, ST_F3  },  {F_AR3,  ST_F4  },  {F_NULL, ST_F4  },  {F_NULL, ST_F4  },  {F_NULL, ST_F4  },  {F_NULL, ST_F4  },  {F_NULL, ST_F4  },  {F_NULL, ST_ILL }},
/* EV_T3     */ {{F_NULL, ST_F3N },  {F_NULL, ST_F3  },  {F_NULL, ST_F3  },  {F_NULL, ST_F3  },  {F_NULL, ST_F3  },  {F_NULL, ST_F7  },  {F_NULL, ST_F8  },  {F_NULL, ST_ILL }},
/* EV_INFO0  */ {{F_I0I,  ST_F3  },  {F_I0I,  ST_F3  },  {F_I0I,  ST_F3  },  {F_I0I,  ST_F3  },  {F_I0A,  ST_F3  },  {F_I0A,  ST_F3  },  {F_I0A,  ST_F3  },  {F_NULL, ST_ILL }},
/* EV_RSY    */ {{F_NULL, ST_F3  },  {F_NULL, ST_F5  },  {F_NULL, ST_F5  },  {F_NULL, ST_F5  },  {F_NULL, ST_F8  },  {F_NULL, ST_F8  },  {F_NULL, ST_F8  },  {F_NULL, ST_ILL }},
/* EV_INFO2  */ {{F_NULL, ST_F6  },  {F_NULL, ST_F6  },  {F_NULL, ST_F6  },  {F_NULL, ST_F6  },  {F_NULL, ST_F6  },  {F_NULL, ST_F6  },  {F_NULL, ST_F6  },  {F_NULL, ST_ILL }},
/* EV_INFO48 */ {{F_AI8 , ST_F7  },  {F_AI8,  ST_F7  },  {F_AI8,  ST_F7  },  {F_AI8,  ST_F7  },  {F_AI8,  ST_F7  },  {F_AI8,  ST_F7  },  {F_AI8,  ST_F7  },  {F_NULL, ST_ILL }},
/* EV_INFO410*/ {{F_AI10, ST_F7  },  {F_AI10, ST_F7  },  {F_AI10, ST_F7  },  {F_AI10, ST_F7  },  {F_AI10, ST_F7  },  {F_AI10, ST_F7  },  {F_AI10, ST_F7  },  {F_NULL, ST_ILL }},
/* EV_DR     */ {{F_NULL, ST_F3  },  {F_NULL, ST_F3  },  {F_NULL, ST_F4  },  {F_NULL, ST_F5  },  {F_NULL, ST_F6  },  {F_NULL, ST_F7  },  {F_NULL, ST_F8  },  {F_NULL, ST_ILL }},
/* EV_PU     */ {{F_NULL, ST_F3  },  {F_NULL, ST_F3  },  {F_NULL, ST_F4  },  {F_NULL, ST_F5  },  {F_NULL, ST_F6  },  {F_NULL, ST_F7  },  {F_NULL, ST_F8  },  {F_NULL, ST_ILL }},
/* EV_DIS    */ {{F_NULL, ST_F3N },  {F_NULL, ST_F3N },  {F_NULL, ST_F3N },  {F_NULL, ST_F3N },  {F_NULL, ST_F3N },  {F_I0A,  ST_F3N },  {F_I0A,  ST_F3N },  {F_NULL, ST_ILL }},
/* EV_EI     */ {{F_NULL, ST_F3  },  {F_NULL, ST_F3  },  {F_NULL, ST_F3  },  {F_NULL, ST_F3  },  {F_NULL, ST_F3  },  {F_NULL, ST_F3  },  {F_NULL, ST_F3  },  {F_NULL, ST_ILL }},
/* EV_ILL    */ {{F_NULL, ST_ILL },  {F_NULL, ST_ILL },  {F_NULL, ST_ILL },  {F_NULL, ST_ILL },  {F_NULL, ST_ILL },  {F_NULL, ST_ILL },  {F_NULL, ST_ILL },  {F_NULL, ST_ILL }},
};

/*---------------------------------------------------------------------------*
 *
 *---------------------------------------------------------------------------*/
void
iwic_next_state(struct iwic_softc *sc, int event)
{
	int currstate, newstate;

	NDBGL1(L1_F_MSG, "event %s", event_names[event]);

	if (event >= N_EVENTS)
	{
		printf("iwic_next_state: event >= N_EVENTS\n");
		return;
	}

	currstate = sc->sc_I430state;

	newstate = iwic_state_tab[event][currstate].newstate;
	if (newstate >= N_STATES)
	{
		printf("iwic_next_state: newstate >= N_STATES\n");
		return;
	}

	NDBGL1(L1_F_MSG, "state %s -> %s",
		state_names[currstate], state_names[newstate]);

	sc->sc_I430state = newstate;

	(*iwic_state_tab[event][currstate].func) (sc);
}

#if DO_I4B_DEBUG
/*---------------------------------------------------------------------------*
 *      return pointer to current state description
 *---------------------------------------------------------------------------*/
char *
iwic_printstate(struct iwic_softc *sc)
{
	return((char *)state_names[sc->sc_I430state]);
}
#endif
