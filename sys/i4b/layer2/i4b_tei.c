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
 *	i4b_tei.c - tei handling procedures
 *	-----------------------------------
 *
 * $FreeBSD$
 *
 *      last edit-date: [Sat Mar  9 17:53:27 2002]
 *
 *---------------------------------------------------------------------------*/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/syslog.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <net/if.h>

#include <machine/i4b_debug.h>

#include <i4b/include/i4b_global.h>
#include <i4b/include/i4b_l1l2.h>
#include <i4b/include/i4b_l2l3.h>
#include <i4b/include/i4b_mbuf.h>

#include <i4b/layer2/i4b_l2.h>
#include <i4b/layer2/i4b_l2fsm.h>

/*---------------------------------------------------------------------------*
 *	handle a received TEI management frame
 *---------------------------------------------------------------------------*/
void
i4b_tei_rxframe(int unit, struct mbuf *m)
{
	l2_softc_t *l2sc = &l2_softc[unit];
	u_char *ptr = m->m_data;
	
	switch(*(ptr + OFF_MT))
	{
		case MT_ID_ASSIGN:
			if( (*(ptr + OFF_RIL) == l2sc->last_ril) &&
			    (*(ptr + OFF_RIH) == l2sc->last_rih))
			{
				l2sc->tei = GET_TEIFROMAI(*(ptr+OFF_AI));
				l2sc->tei_valid = TEI_VALID;

				if(l2sc->T202 == TIMER_ACTIVE)
					i4b_T202_stop(l2sc);

				MDL_Status_Ind(l2sc->unit, STI_TEIASG, l2sc->tei);

				log(LOG_INFO, "i4b: unit %d, assigned TEI = %d = 0x%02x\n", l2sc->unit, l2sc->tei, l2sc->tei);

				NDBGL2(L2_TEI_MSG, "TEI ID Assign - TEI = %d", l2sc->tei);

				i4b_next_l2state(l2sc, EV_MDASGRQ);
			}
			break;
			
		case MT_ID_DENY:
			if( (*(ptr + OFF_RIL) == l2sc->last_ril) &&
			    (*(ptr + OFF_RIH) == l2sc->last_rih))
			{
				l2sc->tei_valid = TEI_INVALID;
				l2sc->tei = GET_TEIFROMAI(*(ptr+OFF_AI));

				if(l2sc->tei == GROUP_TEI)
				{
					log(LOG_WARNING, "i4b: unit %d, denied TEI, no TEI values available from exchange!\n", l2sc->unit);
					NDBGL2(L2_TEI_ERR, "TEI ID Denied, No TEI values available from exchange!");
				}
				else
				{
					log(LOG_WARNING, "i4b: unit %d, denied TEI = %d = 0x%02x\n", l2sc->unit, l2sc->tei, l2sc->tei);
					NDBGL2(L2_TEI_ERR, "TEI ID Denied - TEI = %d", l2sc->tei);
				}					
				MDL_Status_Ind(l2sc->unit, STI_TEIASG, -1);
				i4b_next_l2state(l2sc, EV_MDERRRS);
			}
			break;
			
		case MT_ID_CHK_REQ:
			if( (l2sc->tei_valid == TEI_VALID) &&
			    ( (l2sc->tei == GET_TEIFROMAI(*(ptr+OFF_AI))) ||
			      (GROUP_TEI == GET_TEIFROMAI(*(ptr+OFF_AI))) ))
			{
				static int lasttei = -1;

				if(l2sc->tei != lasttei)
				{
					NDBGL2(L2_TEI_MSG, "TEI ID Check Req - TEI = %d", l2sc->tei);
					lasttei = l2sc->tei;
				}
				
				if(l2sc->T202 == TIMER_ACTIVE)
					i4b_T202_stop(l2sc);
				i4b_tei_chkresp(l2sc);
			}
			break;
			
		case MT_ID_REMOVE:
			if( (l2sc->tei_valid == TEI_VALID) &&
			    ( (l2sc->tei == GET_TEIFROMAI(*(ptr+OFF_AI))) ||
			      (l2sc->tei == GET_TEIFROMAI(*(ptr+OFF_AI)))))
			{
				l2sc->tei_valid = TEI_INVALID;
				l2sc->tei = GET_TEIFROMAI(*(ptr+OFF_AI));

				log(LOG_INFO, "i4b: unit %d, removed TEI = %d = 0x%02x\n", l2sc->unit, l2sc->tei, l2sc->tei);
				NDBGL2(L2_TEI_MSG, "TEI ID Remove - TEI = %d", l2sc->tei);
				MDL_Status_Ind(l2sc->unit, STI_TEIASG, -1);
				i4b_next_l2state(l2sc, EV_MDREMRQ);
			}
			break;
			
		default:
			NDBGL2(L2_TEI_ERR, "UNKNOWN TEI MGMT Frame, type = 0x%x", *(ptr + OFF_MT));
			i4b_print_frame(m->m_len, m->m_data);
			break;
	}
	i4b_Dfreembuf(m);
}

/*---------------------------------------------------------------------------*
 *	allocate and fill up a TEI management frame for sending
 *---------------------------------------------------------------------------*/
static struct mbuf *
build_tei_mgmt_frame(l2_softc_t *l2sc, unsigned char type)
{
	struct mbuf *m;
	
	if((m = i4b_Dgetmbuf(TEI_MGMT_FRM_LEN)) == NULL)
		return(NULL);

	m->m_data[TEIM_SAPIO] = 0xfc;	/* SAPI = 63, CR = 0, EA = 0 */
	m->m_data[TEIM_TEIO]  = 0xff;	/* TEI = 127, EA = 1 */
	m->m_data[TEIM_UIO]   = UI;	/* UI */
	m->m_data[TEIM_MEIO]  = MEI;	/* MEI */
	m->m_data[TEIM_MTO]   = type;	/* message type */
	
	switch(type)
	{
		case MT_ID_REQEST:
			i4b_make_rand_ri(l2sc);
			m->m_data[TEIM_RILO] = l2sc->last_ril;
			m->m_data[TEIM_RIHO] = l2sc->last_rih;
			m->m_data[TEIM_AIO] = (GROUP_TEI << 1) | 0x01;
			break;

		case MT_ID_CHK_RSP:
			i4b_make_rand_ri(l2sc);
			m->m_data[TEIM_RILO] = l2sc->last_ril;
			m->m_data[TEIM_RIHO] = l2sc->last_rih;
			m->m_data[TEIM_AIO] = (l2sc->tei << 1) | 0x01;
			break;
			
		case MT_ID_VERIFY:
			m->m_data[TEIM_RILO] = 0;
			m->m_data[TEIM_RIHO] = 0;
			m->m_data[TEIM_AIO] = (l2sc->tei << 1) | 0x01;
			break;

		default:
			i4b_Dfreembuf(m);
			panic("build_tei_mgmt_frame: invalid type");
			break;
	}
	l2sc->stat.tx_tei++;
	return(m);
}

/*---------------------------------------------------------------------------*
 *	i4b_tei_assign - TEI assignment procedure (Q.921, 5.3.2, pp 24)
 *	T202func and N202 _MUST_ be set prior to calling this function !
 *---------------------------------------------------------------------------*/
void
i4b_tei_assign(l2_softc_t *l2sc)
{
	struct mbuf *m;

	NDBGL2(L2_TEI_MSG, "tx TEI ID_Request");
	
	m = build_tei_mgmt_frame(l2sc, MT_ID_REQEST);

	if(m == NULL)
		panic("i4b_tei_assign: no mbuf");		

	i4b_T202_start(l2sc);
	
	PH_Data_Req(l2sc->unit, m, MBUF_FREE);
}

/*---------------------------------------------------------------------------*
 *	i4b_tei_assign - TEI verify procedure (Q.921, 5.3.5, pp 29)
 *	T202func and N202 _MUST_ be set prior to calling this function !
 *---------------------------------------------------------------------------*/
void
i4b_tei_verify(l2_softc_t *l2sc)
{
	struct mbuf *m;

	NDBGL2(L2_TEI_MSG, "tx TEI ID_Verify");

	m = build_tei_mgmt_frame(l2sc, MT_ID_VERIFY);

	if(m == NULL)
		panic("i4b_tei_verify: no mbuf");		

	i4b_T202_start(l2sc);
	
	PH_Data_Req(l2sc->unit, m, MBUF_FREE);
}

/*---------------------------------------------------------------------------*
 *	i4b_tei_chkresp - TEI check response procedure (Q.921, 5.3.5, pp 29)
 *---------------------------------------------------------------------------*/
void
i4b_tei_chkresp(l2_softc_t *l2sc)
{
	struct mbuf *m;
	static int lasttei = 0;

	if(l2sc->tei != lasttei)
	{
		lasttei = l2sc->tei;
		NDBGL2(L2_TEI_MSG, "tx TEI ID_Check_Response");
	}

	m = build_tei_mgmt_frame(l2sc, MT_ID_CHK_RSP);

	if(m == NULL)
		panic("i4b_tei_chkresp: no mbuf");		

	PH_Data_Req(l2sc->unit, m, MBUF_FREE);
}

/*---------------------------------------------------------------------------*
 *	generate some 16 bit "random" number used for TEI mgmt Ri field
 *---------------------------------------------------------------------------*/
void
i4b_make_rand_ri(l2_softc_t *l2sc)
{
	u_short val;

#ifdef RANDOMDEV
        read_random((char *)&val, sizeof(val));
#else
	val = (u_short)random();
#endif /* RANDOMDEV */ 

	l2sc->last_rih = (val >> 8) & 0x00ff;
	l2sc->last_ril = val & 0x00ff;
}
