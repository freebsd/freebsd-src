/*
 * Copyright (c) 2000 Hans Petter Selasky. All rights reserved.
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
 *	i4b_ihfc_l1.c - hfc layer 1 handler
 *	-----------------------------------
 *
 *	The idea of this file is to seperate hfcs/sp/pci data/signal
 *	handling and the I4B data/signal handling.
 *
 *	Everything which has got anything to do with I4B has been put here!
 *
 *      last edit-date: [Wed Jul 19 09:41:03 2000]
 *
 *      $Id: i4b_ihfc_l1if.c,v 1.10 2000/09/19 13:50:36 hm Exp $
 *
 * $FreeBSD$
 *
 *---------------------------------------------------------------------------*/

#include "ihfc.h"

#if (NIHFC > 0)

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>


#include <net/if.h>

#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>
#include <machine/i4b_trace.h>

#include <i4b/include/i4b_mbuf.h>
#include <i4b/include/i4b_global.h>

#include <i4b/layer1/i4b_l1.h>
#include <i4b/layer1/ihfc/i4b_ihfc.h>
#include <i4b/layer1/ihfc/i4b_ihfc_ext.h>

/*---------------------------------------------------------------------------*
 *	Local prototypes
 *
 *	NOTE: The prototypes for get/putmbuf and B_linkinit 
 *		have been put in i4b_hfc_ext.h for global hfc use.
 *
 *	NOTE: channel != chan
 *---------------------------------------------------------------------------*/
static
isdn_link_t   * ihfc_B_ret_linktab   (int unit, int channel);
static 	void	ihfc_B_set_linktab   (int unit, int channel, drvr_link_t *B_linktab);

static 	void	ihfc_B_start         (int unit, int chan);
static 	void	ihfc_B_stat          (int unit, int chan, bchan_statistics_t *bsp);
	void	ihfc_B_setup         (int unit, int chan, int bprot, int activate);

static	int	ihfc_mph_command_req (int unit, int command, void *parm);

static	int	ihfc_ph_activate_req (int unit);
static	int	ihfc_ph_data_req     (int unit, struct mbuf *m, int freeflag);

static  void	ihfc_T3_expired      (ihfc_sc_t *sc);

/*---------------------------------------------------------------------------*
 *	Our I4B L1 mulitplexer link
 *---------------------------------------------------------------------------*/
struct i4b_l1mux_func ihfc_l1mux_func = {
	ihfc_B_ret_linktab,
	ihfc_B_set_linktab,
	ihfc_mph_command_req,
	ihfc_ph_data_req,
	ihfc_ph_activate_req,
};

/*---------------------------------------------------------------------------*
 *	L2 -> L1: PH-DATA-REQUEST (D-Channel)
 *
 *	NOTE: We may get called here from ihfc_hdlc_Dread or isac_hdlc_Dread
 *	via the upper layers.
 *---------------------------------------------------------------------------*/
int
ihfc_ph_data_req(int unit, struct mbuf *m, int freeflag)
{
	ihfc_sc_t *sc = &ihfc_softc[unit];
	u_char chan = 0;
	HFC_VAR;

	if (!m) return 0;

	HFC_BEG;

	if(S_PHSTATE != 3)
	{
		NDBGL1(L1_PRIM, "L1 was not running: "
			"ihfc_ph_activate_req(unit = %d)!", unit);

			ihfc_ph_activate_req(unit);
	}

	/* "Allow" I-frames (-hp) */

	if (freeflag == MBUF_DONTFREE)	m = m_copypacket(m, M_DONTWAIT);

	if (!IF_QFULL(&S_IFQUEUE) && m)
	{
		IF_ENQUEUE(&S_IFQUEUE, m);

		ihfc_B_start(unit, chan);	/* (recycling) */
	}
	else
	{
		NDBGL1(L1_ERROR, "No frame out (unit = %d)", unit);
		i4b_Dfreembuf(m);
	}

	if (S_INTR_ACTIVE) S_INT_S1 |= 0x04;

	HFC_END;

	return 1;
}

/*---------------------------------------------------------------------------*
 *	L2 -> L1: PH-ACTIVATE-REQUEST (B-channel and D-channel)
 *---------------------------------------------------------------------------*/
int
ihfc_ph_activate_req(int unit)
{
	ihfc_sc_t *sc = &ihfc_softc[unit];
	HFC_VAR;

	HFC_BEG;

	if ((!S_STM_T3) && (S_PHSTATE != 3))
	{
		HFC_FSM(sc, 1);

		S_STM_T3 = 1;
		S_STM_T3CALLOUT = timeout((TIMEOUT_FUNC_T)
					ihfc_T3_expired, (ihfc_sc_t *)sc,
					IHFC_ACTIVATION_TIMEOUT);
	}

	HFC_END;
	return 0;
}
/*---------------------------------------------------------------------------*
 *	T3 timeout - persistant deactivation
 *---------------------------------------------------------------------------*/
void
ihfc_T3_expired(ihfc_sc_t *sc)
{
	u_char chan = 0;
	HFC_VAR;

	HFC_BEG;

	S_STM_T3 = 0;

	if (S_PHSTATE != 3)	/* line was not activated */
	{
		i4b_Dcleanifq(&S_IFQUEUE);
		i4b_l1_ph_deactivate_ind(S_I4BUNIT);

		i4b_l1_mph_status_ind(S_I4BUNIT, STI_PDEACT, 0, 0);

		HFC_FSM(sc, 2);		/* L1 deactivate */
	}

	HFC_END;
}

/*---------------------------------------------------------------------------*
 *	Command from the upper layers (B-channel and D-channel)
 *---------------------------------------------------------------------------*/
int
ihfc_mph_command_req(int unit, int command, void *parm)
{
	ihfc_sc_t *sc = &ihfc_softc[unit];

	switch(command)
	{
		case CMR_DOPEN:		/* daemon running */
			NDBGL1(L1_PRIM,
				"unit %d, command = CMR_DOPEN", unit);
			S_ENABLED = 1;
			break;
			
		case CMR_DCLOSE:	/* daemon not running */
			NDBGL1(L1_PRIM,
				"unit %d, command = CMR_DCLOSE", unit);
			S_ENABLED = 0;
			break;

		case CMR_SETTRACE:	/* set new trace mask */
			NDBGL1(L1_PRIM,
				"unit %d, command = CMR_SETTRACE, parm = %d",
				unit, (unsigned int)parm);
			S_TRACE = (unsigned int)parm;
			break;

		case CMR_GCST:		/* get chip statistic */
			NDBGL1(L1_PRIM,
				"unit %d, command = CMR_GCST, parm = %d",
				unit, (unsigned int)parm);

			#define CST ((struct chipstat *)parm)

			CST->driver_type = L1DRVR_IHFC;

			/* XXX CST->xxxx_stat = xxx; */

			#undef CST
			break;

		default:
			NDBGL1(L1_ERROR, 
				"ERROR, unknown command = %d, unit = %d, parm = %d",
				command, unit, (unsigned int)parm);
			break;
	}

	return 0;
}

/*---------------------------------------------------------------------------*
 *	Data source switch for Read channels - 1, 3 and 5 (B and D-Channel)
 *---------------------------------------------------------------------------*/
void
ihfc_putmbuf (ihfc_sc_t *sc, u_char chan, struct mbuf *m)
{
	i4b_trace_hdr_t hdr;

	if (chan < 2)
	{
		if(S_TRACE & TRACE_D_RX)
		{
			hdr.count = ++S_DTRACECOUNT;
			hdr.dir   = FROM_NT;
			hdr.type  = TRC_CH_D;
			hdr.unit  = S_I4BUNIT;

			MICROTIME(hdr.time);

			i4b_l1_trace_ind(&hdr, m->m_len, m->m_data);
		}

		if (!S_ENABLED) { i4b_Dfreembuf(m); return; }

		m->m_pkthdr.len = m->m_len;
	
		i4b_l1_ph_data_ind(S_I4BUNIT, m);
	}
	else
	{
		if(S_TRACE & TRACE_B_RX)
		{
			hdr.count = ++S_BTRACECOUNT;
			hdr.dir   = FROM_NT;
			hdr.type  = (chan < 4) ? TRC_CH_B1 : TRC_CH_B2;
			hdr.unit  = S_I4BUNIT;

			MICROTIME(hdr.time);

			i4b_l1_trace_ind(&hdr, m->m_len, m->m_data);
		}

		if (!S_ENABLED) { i4b_Bfreembuf(m); return; }

		if (S_PROT == BPROT_NONE)
		{
			if(!i4b_l1_bchan_tel_silence(m->m_data, m->m_len))
			{
				S_BDRVLINK->bch_activity(S_BDRVLINK->unit, ACT_RX);
			}

			if (!IF_QFULL(&S_IFQUEUE))
			{
				S_BYTES += m->m_len;
				IF_ENQUEUE(&S_IFQUEUE, m);
				S_BDRVLINK->bch_rx_data_ready(S_BDRVLINK->unit);
			}

			return;
		}

		if (S_PROT == BPROT_RHDLC)
		{
			S_MBUFDUMMY = m;
			S_BYTES    += m->m_pkthdr.len = m->m_len;
			S_BDRVLINK->bch_rx_data_ready(S_BDRVLINK->unit);
			S_MBUFDUMMY = NULL;

			return;
		}

		NDBGL1(L1_ERROR, "Unknown protocol: %d", S_PROT);
	}
}

/*---------------------------------------------------------------------------*
 *	Data destinator switch for write channels - 0, 2 and 4
 *---------------------------------------------------------------------------*/
struct mbuf *
ihfc_getmbuf (ihfc_sc_t *sc, u_char chan)
{
	register struct mbuf  *m;
	i4b_trace_hdr_t hdr;

	if (chan < 2)
	{
		IF_DEQUEUE(&S_IFQUEUE, m);

		if((S_TRACE & TRACE_D_TX) && m)
		{
			hdr.count = ++S_DTRACECOUNT;
			hdr.dir   = FROM_TE;
			hdr.type  = TRC_CH_D;
			hdr.unit  = S_I4BUNIT;

			MICROTIME(hdr.time);

			i4b_l1_trace_ind(&hdr, m->m_len, m->m_data);
		}
	}
	else
	{
		IF_DEQUEUE(&S_IFQUEUE, m);

		if (!m)
		{
			S_BDRVLINK->bch_tx_queue_empty(S_BDRVLINK->unit);

			IF_DEQUEUE(&S_IFQUEUE, m);
		}
		if (m)
		{
		 	if(!i4b_l1_bchan_tel_silence(m->m_data, m->m_len))
			{
				S_BDRVLINK->bch_activity(S_BDRVLINK->unit, ACT_TX);
			}

			S_BYTES += m->m_len;

			if(S_TRACE & TRACE_B_TX)
			{
				hdr.count = ++S_BTRACECOUNT;
				hdr.dir   = FROM_TE;
				hdr.type  = (chan < 4) ? TRC_CH_B1 : TRC_CH_B2;
				hdr.unit  = S_I4BUNIT;

				MICROTIME(hdr.time);

				i4b_l1_trace_ind(&hdr, m->m_len, m->m_data);
			}
		}
	}

	return(m);
}

/*---------------------------------------------------------------------------*
 *	Initialize rx/tx data structures (B-channel)
 *---------------------------------------------------------------------------*/
void
ihfc_B_setup(int unit, int chan, int bprot, int activate)
{
	ihfc_sc_t *sc = &ihfc_softc[unit];
	HFC_VAR;

	if (((u_int)chan > 5) || ((u_int)chan < 2)) return;

	HFC_BEG;

	HFC_INIT(sc, chan, bprot, activate);

	HFC_END;
}

/*---------------------------------------------------------------------------*
 *	Start transmission (B-channel or D-channel tx)
 *	NOTE: if "chan" variable is corrupted, it will not cause any harm,
 *	but data may be lost and there may be software sync. errors.
 *---------------------------------------------------------------------------*/
void
ihfc_B_start(int unit, int chan)
{
	ihfc_sc_t *sc = &ihfc_softc[unit];
	HFC_VAR;

	if ((u_int)chan > 5) return;

	HFC_BEG;

	if (S_FILTER && !S_MBUF && !S_INTR_ACTIVE)
	{
		S_INTR_ACTIVE |= 2;	/* never know what *
                                         * they put in the *
                                         * L2 code         */

		S_FILTER(sc, chan);	/* quick tx */

		S_INTR_ACTIVE &= ~2;
	}

	HFC_END;
}

/*---------------------------------------------------------------------------*
 *	Fill statistics struct (B-channel)
 *---------------------------------------------------------------------------*/
void
ihfc_B_stat(int unit, int chan, bchan_statistics_t *bsp)
{
	ihfc_sc_t *sc = &ihfc_softc[unit];
	HFC_VAR;

	if ((u_int)chan > 5) return;

	chan &= ~1;

	HFC_BEG;

	bsp->inbytes  = S_BYTES; S_BYTES = 0;

	chan++;

	bsp->outbytes = S_BYTES; S_BYTES = 0;

	HFC_END;
}

/*---------------------------------------------------------------------------*
 *	Return the address of IHFC linktab to I4B (B-channel)
 *---------------------------------------------------------------------------*/
isdn_link_t *
ihfc_B_ret_linktab(int unit, int channel)
{
	ihfc_sc_t *sc = &ihfc_softc[unit];

	if (channel < 2)
		return(&sc->sc_blinktab[channel]);
	else
		return 0;
}
 
/*---------------------------------------------------------------------------*
 *	Set the I4B driver linktab for IHFC use (B-channel)
 *---------------------------------------------------------------------------*/
void
ihfc_B_set_linktab(int unit, int channel, drvr_link_t *B_linktab)
{
	ihfc_sc_t *sc = &ihfc_softc[unit];

	if (channel < 2)
		sc->sc_bdrvlinktab[channel] = B_linktab;
}

/*---------------------------------------------------------------------------*
 *	Initialize linktab for I4B use (B-channel)
 *---------------------------------------------------------------------------*/
void
ihfc_B_linkinit(ihfc_sc_t *sc)
{
	u_char chan;

	/* make sure the hardware driver is known to layer 4 */
	ctrl_types[CTRL_PASSIVE].set_linktab = i4b_l1_set_linktab;
	ctrl_types[CTRL_PASSIVE].get_linktab = i4b_l1_ret_linktab;

	for (chan = 2; chan < 6; chan++)
	{
		S_BLINK.unit          = S_UNIT;
		S_BLINK.channel       = chan;		/* point to tx-chan */
		S_BLINK.bch_config    = ihfc_B_setup;
		S_BLINK.bch_tx_start  = ihfc_B_start;
		S_BLINK.bch_stat      = ihfc_B_stat;
		
		/* This is a transmit channel (even) */
		S_BLINK.tx_queue   = &S_IFQUEUE;
		chan++;
		/* This is a receive channel (odd) */
		S_BLINK.rx_queue   = &S_IFQUEUE;
		S_BLINK.rx_mbuf    = &S_MBUFDUMMY;
	}
}

#endif /* NIHFC > 0 */
