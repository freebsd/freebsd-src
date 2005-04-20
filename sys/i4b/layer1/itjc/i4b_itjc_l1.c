/*-
 * Copyright (c) 1997, 2001 Hellmuth Michaelis. All rights reserved.
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
 *	i4b_itjc_l1.c - NetJet-S layer 1 handler
 *	---------------------------------------------
 *      last edit-date: [Wed Jan 10 17:16:19 2001]
 *
 *---------------------------------------------------------------------------*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>

#include <machine/stdarg.h>
#include <machine/clock.h>

#include <net/if.h>

#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>
#include <machine/i4b_trace.h>

#include <i4b/layer1/isic/i4b_isic.h>
#include <i4b/layer1/isic/i4b_isac.h>

#include <i4b/layer1/itjc/i4b_itjc_ext.h>

#include <i4b/layer1/i4b_l1.h>

#include <i4b/include/i4b_mbuf.h>
#include <i4b/include/i4b_global.h>

/*---------------------------------------------------------------------------*
 *
 *	L2 -> L1: PH-DATA-REQUEST
 *	=========================
 *
 *	parms:
 *		unit		physical interface unit number
 *		m		mbuf containing L2 frame to be sent out
 *		freeflag	MBUF_FREE: free mbuf here after having sent
 *						it out
 *				MBUF_DONTFREE: mbuf is freed by Layer 2
 *	returns:
 *		==0	fail, nothing sent out
 *		!=0	ok, frame sent out
 *
 *---------------------------------------------------------------------------*/
int
itjc_ph_data_req(int unit, struct mbuf *m, int freeflag)
{
	u_char cmd;
	int s;
	struct l1_softc *sc = itjc_scp[unit];

	NDBGL1(L1_PRIM, "PH-DATA-REQ, unit %d, freeflag=%d", unit, freeflag);

	if(m == NULL)			/* failsafe */
		return (0);

	s = SPLI4B();

	if(sc->sc_I430state == ST_F3)	/* layer 1 not running ? */
	{
		NDBGL1(L1_I_ERR, "still in state F3!");
		itjc_ph_activate_req(unit);
	}

	if(sc->sc_state & ISAC_TX_ACTIVE)
	{
		if(sc->sc_obuf2 == NULL)
		{
			sc->sc_obuf2 = m;		/* save mbuf ptr */

			if(freeflag)
				sc->sc_freeflag2 = 1;	/* IRQ must mfree */
			else
				sc->sc_freeflag2 = 0;	/* IRQ must not mfree */

			NDBGL1(L1_I_MSG, "using 2nd ISAC TX buffer, state = %s", itjc_printstate(sc));

			if(sc->sc_trace & TRACE_D_TX)
			{
				i4b_trace_hdr_t hdr;
				hdr.unit = L0ITJCUNIT(unit);
				hdr.type = TRC_CH_D;
				hdr.dir = FROM_TE;
				hdr.count = ++sc->sc_trace_dcount;
				MICROTIME(hdr.time);
				i4b_l1_trace_ind(&hdr, m->m_len, m->m_data);
			}
			splx(s);
			return(1);
		}

		NDBGL1(L1_I_ERR, "No Space in TX FIFO, state = %s", itjc_printstate(sc));
	
		if(freeflag == MBUF_FREE)
			i4b_Dfreembuf(m);			
	
		splx(s);
		return (0);
	}

	if(sc->sc_trace & TRACE_D_TX)
	{
		i4b_trace_hdr_t hdr;
		hdr.unit = L0ITJCUNIT(unit);
		hdr.type = TRC_CH_D;
		hdr.dir = FROM_TE;
		hdr.count = ++sc->sc_trace_dcount;
		MICROTIME(hdr.time);
		i4b_l1_trace_ind(&hdr, m->m_len, m->m_data);
	}
	
	sc->sc_state |= ISAC_TX_ACTIVE;	/* set transmitter busy flag */

	NDBGL1(L1_I_MSG, "ISAC_TX_ACTIVE set");

	sc->sc_freeflag = 0;		/* IRQ must NOT mfree */
	
	ISAC_WRFIFO(m->m_data, min(m->m_len, ISAC_FIFO_LEN)); /* output to TX fifo */

	if(m->m_len > ISAC_FIFO_LEN)	/* message > 32 bytes ? */
	{
		sc->sc_obuf = m;	/* save mbuf ptr */
		sc->sc_op = m->m_data + ISAC_FIFO_LEN; 	/* ptr for irq hdl */
		sc->sc_ol = m->m_len - ISAC_FIFO_LEN;	/* length for irq hdl */

		if(freeflag)
			sc->sc_freeflag = 1;	/* IRQ must mfree */
		
		cmd = ISAC_CMDR_XTF;
	}
	else
	{
		sc->sc_obuf = NULL;
		sc->sc_op = NULL;
		sc->sc_ol = 0;

		if(freeflag)
			i4b_Dfreembuf(m);

		cmd = ISAC_CMDR_XTF | ISAC_CMDR_XME;
  	}

	ISAC_WRITE(I_CMDR, cmd);
	ISACCMDRWRDELAY();

	splx(s);
	
	return(1);
}

/*---------------------------------------------------------------------------*
 *
 *	L2 -> L1: PH-ACTIVATE-REQUEST
 *	=============================
 *
 *	parms:
 *		unit	physical interface unit number
 *
 *	returns:
 *		==0	
 *		!=0	
 *
 *---------------------------------------------------------------------------*/
int
itjc_ph_activate_req(int unit)
{
	struct l1_softc *sc = itjc_scp[unit];
	NDBGL1(L1_PRIM, "PH-ACTIVATE-REQ, unit %d", unit);
	itjc_next_state(sc, EV_PHAR);
	return(0);
}

/*---------------------------------------------------------------------------*
 *	command from the upper layers
 *---------------------------------------------------------------------------*/
int
itjc_mph_command_req(int unit, int command, void *parm)
{
	struct l1_softc *sc = itjc_scp[unit];

	switch(command)
	{
		case CMR_DOPEN:		/* daemon running */
			NDBGL1(L1_PRIM, "unit %d, command = CMR_DOPEN", unit);
			sc->sc_enabled = 1;			
			break;
			
		case CMR_DCLOSE:	/* daemon not running */
			NDBGL1(L1_PRIM, "unit %d, command = CMR_DCLOSE", unit);
			sc->sc_enabled = 0;
			break;

		case CMR_SETTRACE:
			NDBGL1(L1_PRIM, "unit %d, command = CMR_SETTRACE, parm = %d", unit, (unsigned int)parm);
			sc->sc_trace = (unsigned int)parm;
			break;
		
		default:
			NDBGL1(L1_ERROR, "ERROR, unknown command = %d, unit = %d, parm = %d", command, unit, (unsigned int)parm);
			break;
	}

	return(0);
}
