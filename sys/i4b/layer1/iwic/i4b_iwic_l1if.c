/*
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
 *
 *---------------------------------------------------------------------------
 *
 *      i4b_iwic - isdn4bsd Winbond W6692 driver
 *      ----------------------------------------
 *
 * $FreeBSD$
 *
 *      last edit-date: [Sun Jan 21 11:09:33 2001]
 *
 *---------------------------------------------------------------------------*/

#include "iwic.h"
#include "opt_i4b.h"
#include "pci.h"

#if (NIWIC > 0) && (NPCI > 0)

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <net/if.h>




#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>
#include <machine/i4b_trace.h>

#include <i4b/include/i4b_global.h>
#include <i4b/include/i4b_l3l4.h>

#include <i4b/layer1/i4b_l1.h>

#include <i4b/layer1/iwic/i4b_iwic.h>
#include <i4b/layer1/iwic/i4b_iwic_ext.h>

/* jump table for multiplex routines */

struct i4b_l1mux_func iwic_l1mux_func = {
	iwic_ret_linktab,
	iwic_set_linktab,
	iwic_mph_command_req,
	iwic_ph_data_req,
	iwic_ph_activate_req,
};

/*---------------------------------------------------------------------------*
 *
 *---------------------------------------------------------------------------*/
int
iwic_ph_data_req(int unit, struct mbuf *m, int freeflag)
{
	struct iwic_softc *sc = iwic_find_sc(unit);

	return iwic_dchan_data_req(sc, m, freeflag);
}

/*---------------------------------------------------------------------------*
 *
 *---------------------------------------------------------------------------*/
int
iwic_ph_activate_req(int unit)
{
	struct iwic_softc *sc = iwic_find_sc(unit);

	iwic_next_state(sc, EV_PHAR);

	return 0;
}

/*---------------------------------------------------------------------------*
 *
 *---------------------------------------------------------------------------*/
int
iwic_mph_command_req(int unit, int command, void *parm)
{
	struct iwic_softc *sc = iwic_find_sc(unit);

	switch (command)
	{
		case CMR_DOPEN:	/* Daemon running */
			NDBGL1(L1_PRIM, "CMR_DOPEN");
			sc->enabled = TRUE;
			break;

		case CMR_DCLOSE:	/* Daemon not running */
			NDBGL1(L1_PRIM, "CMR_DCLOSE");
			sc->enabled = FALSE;
			break;

		case CMR_SETTRACE:
			NDBGL1(L1_PRIM, "CMR_SETTRACE, parm = %d", (unsigned int)parm);
			sc->sc_trace = (unsigned int)parm;
			break;

		default:
			NDBGL1(L1_PRIM, "unknown command = %d", command);
			break;
	}

	return 0;
}

/*---------------------------------------------------------------------------*
 *
 *---------------------------------------------------------------------------*/
isdn_link_t *
iwic_ret_linktab(int unit, int channel)
{
	struct iwic_softc *sc = iwic_find_sc(unit);
	struct iwic_bchan *bchan = &sc->sc_bchan[channel];

	return &bchan->iwic_isdn_linktab;
}

/*---------------------------------------------------------------------------*
 *
 *---------------------------------------------------------------------------*/
void
iwic_set_linktab (int unit, int channel, drvr_link_t *dlt)
{
	struct iwic_softc *sc = iwic_find_sc(unit);
	struct iwic_bchan *bchan = &sc->sc_bchan[channel];

	bchan->iwic_drvr_linktab = dlt;
}

#endif
