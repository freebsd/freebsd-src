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
 *	i4b_l1.c - isdn4bsd layer 1 handler
 *	-----------------------------------
 *
 *	$Id: i4b_l1.c,v 1.2 1999/12/13 21:25:26 hm Exp $ 
 *
 * $FreeBSD: src/sys/i4b/layer1/i4b_l1.c,v 1.6 1999/12/14 20:48:22 hm Exp $
 *
 *      last edit-date: [Mon Dec 13 22:01:55 1999]
 *
 *---------------------------------------------------------------------------*/

#include "isic.h"

#if NISIC > 0

#include <sys/param.h>
#include <sys/ioccom.h>
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

#include <i4b/layer1/i4b_l1.h>
#include <i4b/layer1/i4b_isac.h>
#include <i4b/layer1/i4b_hscx.h>

#include <i4b/include/i4b_l1l2.h>
#include <i4b/include/i4b_mbuf.h>
#include <i4b/include/i4b_global.h>

unsigned int i4b_l1_debug = L1_DEBUG_DEFAULT;

static int ph_data_req(int, struct mbuf *, int);
static int ph_activate_req(int);

/* from i4btrc driver i4b_trace.c */
extern int get_trace_data_from_l1(int unit, int what, int len, char *buf);

/* from layer 2 */
extern int i4b_ph_data_ind(int unit, struct mbuf *m);
extern int i4b_ph_activate_ind(int unit);
extern int i4b_ph_deactivate_ind(int unit);
extern int i4b_mph_attach_ind(int unit);
extern int i4b_mph_status_ind(int, int, int);

/* layer 1 lme */
static int i4b_mph_command_req(int, int, int);

/* jump table */
struct i4b_l1l2_func i4b_l1l2_func = {

	/* Layer 1 --> Layer 2 */
	
	(int (*)(int, struct mbuf *))		i4b_ph_data_ind,
	(int (*)(int)) 				i4b_ph_activate_ind,
	(int (*)(int))				i4b_ph_deactivate_ind,

	/* Layer 2 --> Layer 1 */

	(int (*)(int, struct mbuf *, int))	ph_data_req,
	(int (*)(int))				ph_activate_req,

	/* Layer 1 --> upstream, ISDN trace data */

	(int (*)(i4b_trace_hdr_t *, int, u_char *))	get_trace_data_from_l1,

	/* Driver control and status information */

	(int (*)(int, int, int))		i4b_mph_status_ind,
	(int (*)(int, int, int))		i4b_mph_command_req,
};
 
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
static int
ph_data_req(int unit, struct mbuf *m, int freeflag)
{
	u_char cmd;
	int s;
	struct l1_softc *sc = &l1_sc[unit];

#ifdef NOTDEF
	DBGL1(L1_PRIM, "PH-DATA-REQ", ("unit %d, freeflag=%d\n", unit, freeflag));
#endif

	if(m == NULL)			/* failsafe */
		return (0);

	s = SPLI4B();

	if(sc->sc_I430state == ST_F3)	/* layer 1 not running ? */
	{
		DBGL1(L1_I_ERR, "ph_data_req", ("still in state F3!\n"));
		ph_activate_req(unit);
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

			DBGL1(L1_I_MSG, "ph_data_req", ("using 2nd ISAC TX buffer, state = %s\n", isic_printstate(sc)));

			if(sc->sc_trace & TRACE_D_TX)
			{
				i4b_trace_hdr_t hdr;
				hdr.unit = unit;
				hdr.type = TRC_CH_D;
				hdr.dir = FROM_TE;
				hdr.count = ++sc->sc_trace_dcount;
				MICROTIME(hdr.time);
				MPH_Trace_Ind(&hdr, m->m_len, m->m_data);
			}
			splx(s);
			return(1);
		}

		DBGL1(L1_I_ERR, "ph_data_req", ("No Space in TX FIFO, state = %s\n", isic_printstate(sc)));
	
		if(freeflag == MBUF_FREE)
			i4b_Dfreembuf(m);			
	
		splx(s);
		return (0);
	}

	if(sc->sc_trace & TRACE_D_TX)
	{
		i4b_trace_hdr_t hdr;
		hdr.unit = unit;
		hdr.type = TRC_CH_D;
		hdr.dir = FROM_TE;
		hdr.count = ++sc->sc_trace_dcount;
		MICROTIME(hdr.time);
		MPH_Trace_Ind(&hdr, m->m_len, m->m_data);
	}
	
	sc->sc_state |= ISAC_TX_ACTIVE;	/* set transmitter busy flag */

	DBGL1(L1_I_MSG, "ph_data_req", ("ISAC_TX_ACTIVE set\n"));

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
static int
ph_activate_req(int unit)
{
	struct l1_softc *sc = &l1_sc[unit];
	DBGL1(L1_PRIM, "PH-ACTIVATE-REQ", ("unit %d\n", unit));
	isic_next_state(sc, EV_PHAR);
	return(0);
}

/*---------------------------------------------------------------------------*
 *	command from the upper layers
 *---------------------------------------------------------------------------*/
static int
i4b_mph_command_req(int unit, int command, int parm)
{
	struct l1_softc *sc = &l1_sc[unit];

	switch(command)
	{
		case CMR_DOPEN:		/* daemon running */
			DBGL1(L1_PRIM, "MPH-COMMAND-REQ", ("unit %d, command = CMR_DOPEN\n", unit));
			sc->sc_enabled = 1;			
			break;
			
		case CMR_DCLOSE:	/* daemon not running */
			DBGL1(L1_PRIM, "MPH-COMMAND-REQ", ("unit %d, command = CMR_DCLOSE\n", unit));
			sc->sc_enabled = 0;
			break;

		default:
			DBGL1(L1_ERROR, "i4b_mph_command_req", ("ERROR, unknown command = %d, unit = %d, parm = %d\n", command, unit, parm));
			break;
	}

	return(0);
}

#endif /* NISIC > 0 */
