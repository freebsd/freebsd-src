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
 *	i4b - Siemens HSCX chip (B-channel) handling
 *	--------------------------------------------
 *
 *	$Id: i4b_hscx.c,v 1.7 2000/05/29 15:41:41 hm Exp $ 
 *
 * $FreeBSD$
 *
 *      last edit-date: [Mon May 29 16:44:50 2000]
 *
 *---------------------------------------------------------------------------*/

#include "isic.h"

#if NISIC > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>


#include <net/if.h>

#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>
#include <machine/i4b_trace.h>

#include <i4b/layer1/isic/i4b_isic.h>
#include <i4b/layer1/isic/i4b_hscx.h>

#include <i4b/layer1/i4b_l1.h>

#include <i4b/include/i4b_global.h>
#include <i4b/include/i4b_mbuf.h>

/*---------------------------------------------------------------------------*
 *	HSCX IRQ Handler
 *---------------------------------------------------------------------------*/
void
isic_hscx_irq(register struct l1_softc *sc, u_char ista, int h_chan, u_char ex_irq)
{
	register l1_bchan_state_t *chan = &sc->sc_chan[h_chan];
	u_char exir = 0;
	int activity = -1;
	u_char cmd = 0;

	NDBGL1(L1_H_IRQ, "%#x", ista);

	if(ex_irq)
	{
		/* get channel extended irq reg */

		exir = HSCX_READ(h_chan, H_EXIR);

		if(exir & HSCX_EXIR_RFO)
		{
			chan->stat_RFO++;
			NDBGL1(L1_H_XFRERR, "ex_irq: receive data overflow");
		}

		if((exir & HSCX_EXIR_XDU) && (chan->bprot != BPROT_NONE))/* xmit data underrun */
		{
			chan->stat_XDU++;			
			NDBGL1(L1_H_XFRERR, "ex_irq: xmit data underrun");
			isic_hscx_cmd(sc, h_chan, HSCX_CMDR_XRES);

			if (chan->out_mbuf_head != NULL)  /* don't continue to transmit this buffer */
			{
				i4b_Bfreembuf(chan->out_mbuf_head);
				chan->out_mbuf_cur = chan->out_mbuf_head = NULL;
			}
		}

	}

	/* rx message end, end of frame */
	
	if(ista & HSCX_ISTA_RME)
	{
		register int fifo_data_len;
		u_char rsta;		
		int error = 0;

		rsta = HSCX_READ(h_chan, H_RSTA);

		if((rsta & 0xf0) != 0xa0)
		{
			if((rsta & HSCX_RSTA_VFR) == 0)
			{
				chan->stat_VFR++;
				cmd |= (HSCX_CMDR_RHR);
				NDBGL1(L1_H_XFRERR, "received invalid Frame");
				error++;
			}
	
			if(rsta & HSCX_RSTA_RDO)
			{
				chan->stat_RDO++;
				NDBGL1(L1_H_XFRERR, "receive data overflow");
				error++;				
			}
			
			if((rsta & HSCX_RSTA_CRC) == 0)
			{
				chan->stat_CRC++;
				cmd |= (HSCX_CMDR_RHR);
				NDBGL1(L1_H_XFRERR, "CRC check failed");
				error++;
			}
			
			if(rsta & HSCX_RSTA_RAB)
			{
				chan->stat_RAB++;				
				NDBGL1(L1_H_XFRERR, "Receive message aborted");
				error++;
			}
		}

		fifo_data_len = ((HSCX_READ(h_chan, H_RBCL)) &
						((sc->sc_bfifolen)-1));
		
		if(fifo_data_len == 0)
			fifo_data_len = sc->sc_bfifolen;

		/* all error conditions checked, now decide and take action */
		
		if(error == 0)
		{
			if(chan->in_mbuf == NULL)
			{
				if((chan->in_mbuf = i4b_Bgetmbuf(BCH_MAX_DATALEN)) == NULL)
					panic("L1 isic_hscx_irq: RME, cannot allocate mbuf!\n");
				chan->in_cbptr = chan->in_mbuf->m_data;
				chan->in_len = 0;
			}

			fifo_data_len -= 1; /* last byte in fifo is RSTA ! */
			
			if((chan->in_len + fifo_data_len) <= BCH_MAX_DATALEN)
			{
				/* read data from HSCX fifo */
	
				HSCX_RDFIFO(h_chan, chan->in_cbptr, fifo_data_len);

				cmd |= (HSCX_CMDR_RMC);
				isic_hscx_cmd(sc, h_chan, cmd);
				cmd = 0;
				
		                chan->in_len += fifo_data_len;
				chan->rxcount += fifo_data_len;

				/* setup mbuf data length */
					
				chan->in_mbuf->m_len = chan->in_len;
				chan->in_mbuf->m_pkthdr.len = chan->in_len;
		
				if(sc->sc_trace & TRACE_B_RX)
				{
					i4b_trace_hdr_t hdr;
					hdr.unit = L0ISICUNIT(sc->sc_unit);
					hdr.type = (h_chan == HSCX_CH_A ? TRC_CH_B1 : TRC_CH_B2);
					hdr.dir = FROM_NT;
					hdr.count = ++sc->sc_trace_bcount;
					MICROTIME(hdr.time);
					i4b_l1_trace_ind(&hdr, chan->in_mbuf->m_len, chan->in_mbuf->m_data);
				}

				(*chan->isic_drvr_linktab->bch_rx_data_ready)(chan->isic_drvr_linktab->unit);

				activity = ACT_RX;
				
				/* mark buffer ptr as unused */
					
				chan->in_mbuf = NULL;
				chan->in_cbptr = NULL;
				chan->in_len = 0;
			}
			else
			{
				NDBGL1(L1_H_XFRERR, "RAWHDLC rx buffer overflow in RME, in_len=%d, fifolen=%d", chan->in_len, fifo_data_len);
				chan->in_cbptr = chan->in_mbuf->m_data;
				chan->in_len = 0;
				cmd |= (HSCX_CMDR_RHR | HSCX_CMDR_RMC);	
			}
		}
		else
		{
			if (chan->in_mbuf != NULL)
			{
				i4b_Bfreembuf(chan->in_mbuf);
				chan->in_mbuf = NULL;
				chan->in_cbptr = NULL;
				chan->in_len = 0;
			}
			cmd |= (HSCX_CMDR_RMC);
		}
	}

	/* rx fifo full */

	if(ista & HSCX_ISTA_RPF)
	{
		if(chan->in_mbuf == NULL)
		{
			if((chan->in_mbuf = i4b_Bgetmbuf(BCH_MAX_DATALEN)) == NULL)
				panic("L1 isic_hscx_irq: RPF, cannot allocate mbuf!\n");
			chan->in_cbptr = chan->in_mbuf->m_data;
			chan->in_len = 0;
		}

		chan->rxcount += sc->sc_bfifolen;
		
		if((chan->in_len + sc->sc_bfifolen) <= BCH_MAX_DATALEN)
		{
			/* read data from HSCX fifo */

			HSCX_RDFIFO(h_chan, chan->in_cbptr, sc->sc_bfifolen);

			chan->in_cbptr += sc->sc_bfifolen;
	                chan->in_len += sc->sc_bfifolen;
		}
		else
		{
			if(chan->bprot == BPROT_NONE)
			{
				/* setup mbuf data length */
				
				chan->in_mbuf->m_len = chan->in_len;
				chan->in_mbuf->m_pkthdr.len = chan->in_len;

				if(sc->sc_trace & TRACE_B_RX)
				{
					i4b_trace_hdr_t hdr;
					hdr.unit = L0ISICUNIT(sc->sc_unit);
					hdr.type = (h_chan == HSCX_CH_A ? TRC_CH_B1 : TRC_CH_B2);
					hdr.dir = FROM_NT;
					hdr.count = ++sc->sc_trace_bcount;
					MICROTIME(hdr.time);
					i4b_l1_trace_ind(&hdr, chan->in_mbuf->m_len, chan->in_mbuf->m_data);
				}

				/* silence detection */
				
				if(!(i4b_l1_bchan_tel_silence(chan->in_mbuf->m_data, chan->in_mbuf->m_len)))
					activity = ACT_RX;

				(void) IF_HANDOFF(&chan->rx_queue, chan->in_mbuf, NULL);

				/* signal upper driver that data is available */

				(*chan->isic_drvr_linktab->bch_rx_data_ready)(chan->isic_drvr_linktab->unit);
				
				/* alloc new buffer */
				
				if((chan->in_mbuf = i4b_Bgetmbuf(BCH_MAX_DATALEN)) == NULL)
					panic("L1 isic_hscx_irq: RPF, cannot allocate new mbuf!\n");
	
				/* setup new data ptr */
				
				chan->in_cbptr = chan->in_mbuf->m_data;
	
				/* read data from HSCX fifo */
	
				HSCX_RDFIFO(h_chan, chan->in_cbptr, sc->sc_bfifolen);

				chan->in_cbptr += sc->sc_bfifolen;
				chan->in_len = sc->sc_bfifolen;

				chan->rxcount += sc->sc_bfifolen;
			}
			else
			{
				NDBGL1(L1_H_XFRERR, "RAWHDLC rx buffer overflow in RPF, in_len=%d", chan->in_len);
				chan->in_cbptr = chan->in_mbuf->m_data;
				chan->in_len = 0;
				cmd |= (HSCX_CMDR_RHR);
			}
		}
		
		/* command to release fifo space */
		
		cmd |= HSCX_CMDR_RMC;
	}

	/* transmit fifo empty, new data can be written to fifo */
	
	if(ista & HSCX_ISTA_XPR)
	{
		/*
		 * for a description what is going on here, please have
		 * a look at isic_bchannel_start() in i4b_bchan.c !
		 */
		 
		int activity = -1;
		int len;
		int nextlen;

		NDBGL1(L1_H_IRQ, "unit %d, chan %d - XPR, Tx Fifo Empty!", sc->sc_unit, h_chan);

		if(chan->out_mbuf_cur == NULL) 	/* last frame is transmitted */
		{
			IF_DEQUEUE(&chan->tx_queue, chan->out_mbuf_head);

			if(chan->out_mbuf_head == NULL)
			{
				chan->state &= ~HSCX_TX_ACTIVE;
				(*chan->isic_drvr_linktab->bch_tx_queue_empty)(chan->isic_drvr_linktab->unit);
			}
			else
			{
				chan->state |= HSCX_TX_ACTIVE;
				chan->out_mbuf_cur = chan->out_mbuf_head;
				chan->out_mbuf_cur_ptr = chan->out_mbuf_cur->m_data;
				chan->out_mbuf_cur_len = chan->out_mbuf_cur->m_len;

				if(sc->sc_trace & TRACE_B_TX)
				{
					i4b_trace_hdr_t hdr;
					hdr.unit = L0ISICUNIT(sc->sc_unit);
					hdr.type = (h_chan == HSCX_CH_A ? TRC_CH_B1 : TRC_CH_B2);
					hdr.dir = FROM_TE;
					hdr.count = ++sc->sc_trace_bcount;
					MICROTIME(hdr.time);
					i4b_l1_trace_ind(&hdr, chan->out_mbuf_cur->m_len, chan->out_mbuf_cur->m_data);
				}
				
				if(chan->bprot == BPROT_NONE)
				{
					if(!(i4b_l1_bchan_tel_silence(chan->out_mbuf_cur->m_data, chan->out_mbuf_cur->m_len)))
						activity = ACT_TX;
				}
				else
				{
					activity = ACT_TX;
				}
			}
		}
			
		len = 0;

		while(chan->out_mbuf_cur && len != sc->sc_bfifolen)
		{
			nextlen = min(chan->out_mbuf_cur_len, sc->sc_bfifolen - len);

#ifdef NOTDEF			
			printf("i:mh=%x, mc=%x, mcp=%x, mcl=%d l=%d nl=%d # ",
				chan->out_mbuf_head,
				chan->out_mbuf_cur,			
				chan->out_mbuf_cur_ptr,
				chan->out_mbuf_cur_len,
				len,
				next_len);
#endif

			isic_hscx_waitxfw(sc, h_chan);	/* necessary !!! */
			
			HSCX_WRFIFO(h_chan, chan->out_mbuf_cur_ptr, nextlen);
			cmd |= HSCX_CMDR_XTF;
	
			len += nextlen;
			chan->txcount += nextlen;
	
			chan->out_mbuf_cur_ptr += nextlen;
			chan->out_mbuf_cur_len -= nextlen;
			
			if(chan->out_mbuf_cur_len == 0) 
			{
				if((chan->out_mbuf_cur = chan->out_mbuf_cur->m_next) != NULL)
				{
					chan->out_mbuf_cur_ptr = chan->out_mbuf_cur->m_data;
					chan->out_mbuf_cur_len = chan->out_mbuf_cur->m_len;
	
					if(sc->sc_trace & TRACE_B_TX)
					{
						i4b_trace_hdr_t hdr;
						hdr.unit = L0ISICUNIT(sc->sc_unit);
						hdr.type = (h_chan == HSCX_CH_A ? TRC_CH_B1 : TRC_CH_B2);
						hdr.dir = FROM_TE;
						hdr.count = ++sc->sc_trace_bcount;
						MICROTIME(hdr.time);
						i4b_l1_trace_ind(&hdr, chan->out_mbuf_cur->m_len, chan->out_mbuf_cur->m_data);
					}
				}
				else
				{
					if (chan->bprot != BPROT_NONE)
						cmd |= HSCX_CMDR_XME;
					i4b_Bfreembuf(chan->out_mbuf_head);
					chan->out_mbuf_head = NULL;
				}

			}
		}
	}

	if(cmd)		/* is there a command for the HSCX ? */
	{
		isic_hscx_cmd(sc, h_chan, cmd);	/* yes, to HSCX */
	}

	/* call timeout handling routine */
	
	if(activity == ACT_RX || activity == ACT_TX)
		(*chan->isic_drvr_linktab->bch_activity)(chan->isic_drvr_linktab->unit, activity);
}

/*---------------------------------------------------------------------------*
 *	HSCX initialization
 *
 *	for telephony: extended transparent mode 1
 *	for raw hdlc:  transparent mode 0
 *---------------------------------------------------------------------------*/
void
isic_hscx_init(struct l1_softc *sc, int h_chan, int activate)
{	
	l1_bchan_state_t *chan = &sc->sc_chan[h_chan];

	HSCX_WRITE(h_chan, H_MASK, 0xff);		/* mask irq's */

	if(sc->sc_ipac)
	{
		/* CCR1: Power Up, Clock Mode 5 */
		HSCX_WRITE(h_chan, H_CCR1, HSCX_CCR1_PU  |	/* power up */
			      HSCX_CCR1_CM1);	/* IPAC clock mode 5 */
	}
	else
	{
		/* CCR1: Power Up, Clock Mode 5 */
		HSCX_WRITE(h_chan, H_CCR1, HSCX_CCR1_PU  |	/* power up */
			      HSCX_CCR1_CM2 |	/* HSCX clock mode 5 */
			      HSCX_CCR1_CM0);
	}
		
	/* XAD1: Transmit Address Byte 1 */
	HSCX_WRITE(h_chan, H_XAD1, 0xff);
	
	/* XAD2: Transmit Address Byte 2 */
	HSCX_WRITE(h_chan, H_XAD2, 0xff);

	/* RAH2: Receive Address Byte High Reg. 2 */
	HSCX_WRITE(h_chan, H_RAH2, 0xff);
	
	/* XBCH: reset Transmit Byte Count High */
	HSCX_WRITE(h_chan, H_XBCH, 0x00);
	
	/* RLCR: reset Receive Length Check Register */
	HSCX_WRITE(h_chan, H_RLCR, 0x00);
	
	/* CCR2: set tx/rx clock shift bit 0	*/
	/*       disable CTS irq, disable RIE irq*/
	HSCX_WRITE(h_chan, H_CCR2, HSCX_CCR2_XCS0|HSCX_CCR2_RCS0);

	/* XCCR: tx bit count per time slot */
	HSCX_WRITE(h_chan, H_XCCR, 0x07);

	/* RCCR: rx bit count per time slot */
	HSCX_WRITE(h_chan, H_RCCR, 0x07);
	
	if(sc->sc_bustyp == BUS_TYPE_IOM2)
	{
		switch(h_chan) 
		{
			case HSCX_CH_A:	/* Prepare HSCX channel A */
				/* TSAX: tx clock shift bits 1 & 2	*/
				/*       tx time slot number		*/
		        	HSCX_WRITE(h_chan, H_TSAX, 0x2f);

				/* TSAR: rx clock shift bits 1 & 2	*/
				/*       rx time slot number		*/
				HSCX_WRITE(h_chan, H_TSAR, 0x2f);
				break;

			case HSCX_CH_B: /* Prepare HSCX channel B */
				/* TSAX: tx clock shift bits 1 & 2	*/
				/*       tx time slot number		*/
				HSCX_WRITE(h_chan, H_TSAX, 0x03);

				/* TSAR: rx clock shift bits 1 & 2	*/
				/*       rx time slot number		*/
				HSCX_WRITE(h_chan, H_TSAR, 0x03);
				break;
		}
	}
	else	/* IOM 1 setup */
	{
		/* TSAX: tx clock shift bits 1 & 2	*/
		/*       tx time slot number		*/
		HSCX_WRITE(h_chan, H_TSAX, 0x07);

		/* TSAR: rx clock shift bits 1 & 2	*/
		/*       rx time slot number		*/
		HSCX_WRITE(h_chan, H_TSAR, 0x07);
	}

	if(activate)
	{
		if(chan->bprot == BPROT_RHDLC)
		{
		  /* HDLC Frames, transparent mode 0 */
		  HSCX_WRITE(h_chan, H_MODE,
		     HSCX_MODE_MDS1|HSCX_MODE_RAC|HSCX_MODE_RTS);
		}
		else
		{
		  /* Raw Telephony, extended transparent mode 1 */
		  HSCX_WRITE(h_chan, H_MODE,
		     HSCX_MODE_MDS1|HSCX_MODE_MDS0|HSCX_MODE_ADM|HSCX_MODE_RTS);
		}
#if 0
		isic_hscx_cmd(sc, h_chan, HSCX_CMDR_RHR|HSCX_CMDR_XRES);
#else
		isic_hscx_cmd(sc, h_chan, HSCX_CMDR_RHR);
#endif
	}
	else
	{
		/* TSAX: tx time slot */
		HSCX_WRITE(h_chan, H_TSAX, 0xff);

		/* TSAR: rx time slot */
		HSCX_WRITE(h_chan, H_TSAR, 0xff);

		/* Raw Telephony, extended transparent mode 1 */
		HSCX_WRITE(h_chan, H_MODE,
		   HSCX_MODE_MDS1|HSCX_MODE_MDS0|HSCX_MODE_ADM|HSCX_MODE_RTS);
	}

 	/* don't touch ICA, EXA and EXB bits, this could be HSCX_CH_B */	
	/* always disable RSC and TIN */

	chan->hscx_mask |= HSCX_MASK_RSC | HSCX_MASK_TIN;

	if(activate)
	{
		/* enable */
		chan->hscx_mask &= ~(HSCX_MASK_RME | HSCX_MASK_RPF | HSCX_MASK_XPR);
	}
	else
	{
		/* disable */
		chan->hscx_mask |= HSCX_MASK_RME | HSCX_MASK_RPF | HSCX_MASK_XPR;
	}

	/* handle ICA, EXA, and EXB via interrupt mask of channel b */

	if (h_chan == HSCX_CH_A)
	{
		if (activate) 
			HSCX_B_IMASK &= ~(HSCX_MASK_EXA | HSCX_MASK_ICA);
		else
			HSCX_B_IMASK |= HSCX_MASK_EXA | HSCX_MASK_ICA;
		HSCX_WRITE(HSCX_CH_A, H_MASK, HSCX_A_IMASK);
		HSCX_WRITE(HSCX_CH_B, H_MASK, HSCX_B_IMASK);
	}
	else
	{
		if (activate)
			HSCX_B_IMASK &= ~HSCX_MASK_EXB;
		else
			HSCX_B_IMASK |= HSCX_MASK_EXB;
		HSCX_WRITE(HSCX_CH_B, H_MASK, HSCX_B_IMASK);
	}

	/* clear spurious interrupts left over */

	if(h_chan == HSCX_CH_A)
	{
		HSCX_READ(h_chan, H_EXIR);
		HSCX_READ(h_chan, H_ISTA);
	}
	else  /* mask ICA, because it must not be cleared by reading ISTA */
	{
		HSCX_WRITE(HSCX_CH_B, H_MASK, HSCX_B_IMASK | HSCX_MASK_ICA);
		HSCX_READ(h_chan, H_EXIR);
		HSCX_READ(h_chan, H_ISTA);
		HSCX_WRITE(HSCX_CH_B, H_MASK, HSCX_B_IMASK);
	}
}

/*---------------------------------------------------------------------------*
 *	write command to HSCX command register
 *---------------------------------------------------------------------------*/
void
isic_hscx_cmd(struct l1_softc *sc, int h_chan, unsigned char cmd)
{	
	int timeout = 20;

	while(((HSCX_READ(h_chan, H_STAR)) & HSCX_STAR_CEC) && timeout)
	{
		DELAY(10);
		timeout--;
	}

	if(timeout == 0)
	{
		NDBGL1(L1_H_ERR, "HSCX wait for CEC timeout!");
	}

	HSCX_WRITE(h_chan, H_CMDR, cmd);	
}

/*---------------------------------------------------------------------------*
 *	wait for HSCX transmit FIFO write enable
 *---------------------------------------------------------------------------*/
void
isic_hscx_waitxfw(struct l1_softc *sc, int h_chan)
{	
#define WAITVAL 50
#define WAITTO	200

	int timeout = WAITTO;

	while((!(((HSCX_READ(h_chan, H_STAR)) &
		(HSCX_STAR_CEC | HSCX_STAR_XFW)) == HSCX_STAR_XFW)) && timeout)
	{
		DELAY(WAITVAL);
		timeout--;
	}

	if(timeout == 0)
	{
		NDBGL1(L1_H_ERR, "HSCX wait for XFW timeout!");
	}
	else if (timeout != WAITTO)
	{
		NDBGL1(L1_H_XFRERR, "HSCX wait for XFW time: %d uS", (WAITTO-timeout)*50);
	}
}
		
#endif /* NISIC > 0 */
