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
 *	i4b_uframe.c - routines for handling U-frames
 *	-----------------------------------------------
 *
 *	$Id: i4b_uframe.c,v 1.13 2000/08/24 11:48:58 hm Exp $ 
 *
 * $FreeBSD$
 *
 *      last edit-date: [Mon May 29 16:55:30 2000]
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
#include <sys/mbuf.h>
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

#include <i4b/include/i4b_l1l2.h>
#include <i4b/include/i4b_l2l3.h>
#include <i4b/include/i4b_mbuf.h>

#include <i4b/layer2/i4b_l2.h>
#include <i4b/layer2/i4b_l2fsm.h>

/*---------------------------------------------------------------------------*
 *	process a received U-frame
 *---------------------------------------------------------------------------*/
void
i4b_rxd_u_frame(int unit, struct mbuf *m)
{
	l2_softc_t *l2sc = &l2_softc[unit];
	u_char *ptr = m->m_data;

	int sapi = GETSAPI(*(ptr + OFF_SAPI));
	int tei = GETTEI(*(ptr + OFF_TEI));	
	int pfbit = GETUPF(*(ptr + OFF_CNTL));
	
	switch(*(ptr + OFF_CNTL) & ~UPFBIT)
	{
		/* commands */

		case SABME:
			if((l2sc->tei_valid == TEI_VALID) &&
			   (l2sc->tei == GETTEI(*(ptr+OFF_TEI))))
			{
				l2sc->stat.rx_sabme++;
				NDBGL2(L2_U_MSG, "SABME, sapi = %d, tei = %d", sapi, tei);
				l2sc->rxd_PF = pfbit;
				i4b_next_l2state(l2sc, EV_RXSABME);
			}
			i4b_Dfreembuf(m);
			break;

		case UI:
			if(sapi == SAPI_L2M &&
			   tei == GROUP_TEI &&
			   *(ptr + OFF_MEI) == MEI)
			{
				/* layer 2 management (SAPI = 63) */
				l2sc->stat.rx_tei++;
				i4b_tei_rxframe(unit, m);
			}
			else if(sapi == SAPI_CCP && tei == GROUP_TEI)
			{
				/* call control (SAPI = 0) */
				l2sc->stat.rx_ui++;
				/* strip ui header */
				m_adj(m, UI_HDR_LEN);
				/* to upper layer */
				DL_Unit_Data_Ind(unit, m);
			}
			else
			{
				l2sc->stat.err_rx_badui++;
				NDBGL2(L2_U_ERR, "unknown UI frame!");
				i4b_Dfreembuf(m);				
			}
			break;
			
		case DISC:
			if((l2sc->tei_valid == TEI_VALID) &&
			   (l2sc->tei == GETTEI(*(ptr+OFF_TEI))))
			{		
				l2sc->stat.rx_disc++;
				NDBGL2(L2_U_MSG, "DISC, sapi = %d, tei = %d", sapi, tei);
				l2sc->rxd_PF = pfbit;
				i4b_next_l2state(l2sc, EV_RXDISC);
			}
			i4b_Dfreembuf(m);
			break;

		case XID:
			if((l2sc->tei_valid == TEI_VALID) &&
			   (l2sc->tei == GETTEI(*(ptr+OFF_TEI))))
			{		
				l2sc->stat.rx_xid++;
				NDBGL2(L2_U_MSG, "XID, sapi = %d, tei = %d", sapi, tei);
			}
			i4b_Dfreembuf(m);			
			break;
			
		/* responses */

		case DM:
			if((l2sc->tei_valid == TEI_VALID) &&
			   (l2sc->tei == GETTEI(*(ptr+OFF_TEI))))
			{		
				l2sc->stat.rx_dm++;
				NDBGL2(L2_U_MSG, "DM, sapi = %d, tei = %d", sapi, tei);
				i4b_print_frame(m->m_len, m->m_data);
				l2sc->rxd_PF = pfbit;
				i4b_next_l2state(l2sc, EV_RXDM);
			}
			i4b_Dfreembuf(m);
			break;
			
		case UA:
			if((l2sc->tei_valid == TEI_VALID) &&
			   (l2sc->tei == GETTEI(*(ptr+OFF_TEI))))
			{		
				l2sc->stat.rx_ua++;
				NDBGL2(L2_U_MSG, "UA, sapi = %d, tei = %d", sapi, tei);
				l2sc->rxd_PF = pfbit;
				i4b_next_l2state(l2sc, EV_RXUA);
			}
			i4b_Dfreembuf(m);			
			break;			

		case FRMR:
			if((l2sc->tei_valid == TEI_VALID) &&
			   (l2sc->tei == GETTEI(*(ptr+OFF_TEI))))
			{
				l2sc->stat.rx_frmr++;
				NDBGL2(L2_U_MSG, "FRMR, sapi = %d, tei = %d", sapi, tei);
				l2sc->rxd_PF = pfbit;
				i4b_next_l2state(l2sc, EV_RXFRMR);
			}
			i4b_Dfreembuf(m);			
			break;

		default:
			if((l2sc->tei_valid == TEI_VALID) &&
			   (l2sc->tei == GETTEI(*(ptr+OFF_TEI))))
			{		
				NDBGL2(L2_U_ERR, "UNKNOWN TYPE ERROR, sapi = %d, tei = %d, frame = ", sapi, tei);
				i4b_print_frame(m->m_len, m->m_data);
			}
			else
			{		
				NDBGL2(L2_U_ERR, "not mine -  UNKNOWN TYPE ERROR, sapi = %d, tei = %d, frame = ", sapi, tei);
				i4b_print_frame(m->m_len, m->m_data);
			}
			l2sc->stat.err_rx_badui++;			
			i4b_Dfreembuf(m);			
			break;
	}
}

/*---------------------------------------------------------------------------*
 *	build U-frame for sending
 *---------------------------------------------------------------------------*/
struct mbuf *
i4b_build_u_frame(l2_softc_t *l2sc, crbit_to_nt_t crbit, pbit_t pbit, u_char type)
{
	struct mbuf *m;
	
	if((m = i4b_Dgetmbuf(U_FRAME_LEN)) == NULL)
		return(NULL);

	PUTSAPI(SAPI_CCP, crbit, m->m_data[OFF_SAPI]);
		
	PUTTEI(l2sc->tei, m->m_data[OFF_TEI]);

	if(pbit)
		m->m_data[OFF_CNTL] = type | UPBITSET;
	else
		m->m_data[OFF_CNTL] = type & ~UPBITSET;

	return(m);
}

/*---------------------------------------------------------------------------*
 *	transmit SABME command
 *---------------------------------------------------------------------------*/
void
i4b_tx_sabme(l2_softc_t *l2sc, pbit_t pbit)
{
	struct mbuf *m;

	l2sc->stat.tx_sabme++;
	NDBGL2(L2_U_MSG, "tx SABME, tei = %d", l2sc->tei);
	m = i4b_build_u_frame(l2sc, CR_CMD_TO_NT, pbit, SABME);
	PH_Data_Req(l2sc->unit, m, MBUF_FREE);
}

/*---------------------------------------------------------------------------*
 *	transmit DM response
 *---------------------------------------------------------------------------*/
void
i4b_tx_dm(l2_softc_t *l2sc, fbit_t fbit)
{
	struct mbuf *m;

	l2sc->stat.tx_dm++;	
	NDBGL2(L2_U_MSG, "tx DM, tei = %d", l2sc->tei);
	m = i4b_build_u_frame(l2sc, CR_RSP_TO_NT, fbit, DM);
	PH_Data_Req(l2sc->unit, m, MBUF_FREE);
}

/*---------------------------------------------------------------------------*
 *	transmit DISC command
 *---------------------------------------------------------------------------*/
void
i4b_tx_disc(l2_softc_t *l2sc, pbit_t pbit)
{
	struct mbuf *m;
	
	l2sc->stat.tx_disc++;
	NDBGL2(L2_U_MSG, "tx DISC, tei = %d", l2sc->tei);
	m = i4b_build_u_frame(l2sc, CR_CMD_TO_NT, pbit, DISC);
	PH_Data_Req(l2sc->unit, m, MBUF_FREE);
}

/*---------------------------------------------------------------------------*
 *	transmit UA response
 *---------------------------------------------------------------------------*/
void
i4b_tx_ua(l2_softc_t *l2sc, fbit_t fbit)
{
	struct mbuf *m;
	
	l2sc->stat.tx_ua++;
	NDBGL2(L2_U_MSG, "tx UA, tei = %d", l2sc->tei);
	m = i4b_build_u_frame(l2sc, CR_RSP_TO_NT, fbit, UA);
	PH_Data_Req(l2sc->unit, m, MBUF_FREE);
}

/*---------------------------------------------------------------------------*
 *	transmit FRMR response
 *---------------------------------------------------------------------------*/
void
i4b_tx_frmr(l2_softc_t *l2sc, fbit_t fbit)
{
	struct mbuf *m;
	
	l2sc->stat.tx_frmr++;
	NDBGL2(L2_U_MSG, "tx FRMR, tei = %d", l2sc->tei);
	m = i4b_build_u_frame(l2sc, CR_RSP_TO_NT, fbit, FRMR);
	PH_Data_Req(l2sc->unit, m, MBUF_FREE);
}

#endif /* NI4BQ921 > 0 */
