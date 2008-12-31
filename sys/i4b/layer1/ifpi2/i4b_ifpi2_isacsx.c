/*-
 * Copyright (c) 1997, 2000 Hellmuth Michaelis. All rights reserved.
 * Copyright (c) 2001 Gary Jennejohn. All rights reserved.
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
 *	i4b_ifpi2_isac.c - i4b Fritz PCI Version 2 ISACSX handler
 *	--------------------------------------------
 *	$Id$ 
 *
 *---------------------------------------------------------------------------*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/i4b/layer1/ifpi2/i4b_ifpi2_isacsx.c,v 1.6.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include "opt_i4b.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>


#include <net/if.h>

#include <i4b/include/i4b_debug.h>
#include <i4b/include/i4b_ioctl.h>
#include <i4b/include/i4b_trace.h>

#include <i4b/layer1/i4b_l1.h>

#include <i4b/layer1/isic/i4b_isic.h>
#include <i4b/layer1/isic/i4b_hscx.h>

#include <i4b/layer1/ifpi2/i4b_ifpi2_ext.h>
#include <i4b/layer1/ifpi2/i4b_ifpi2_isacsx.h>

#include <i4b/include/i4b_global.h>
#include <i4b/include/i4b_mbuf.h>

static u_char ifpi2_isacsx_exir_hdlr(register struct l1_softc *sc, u_char exir);
static void ifpi2_isacsx_ind_hdlr(register struct l1_softc *sc, int ind);

/* the ISACSX has 2 mask registers of interest - cannot use ISAC_IMASK */
unsigned char isacsx_imaskd;
unsigned char isacsx_imask;

/*---------------------------------------------------------------------------*
 *	ISACSX interrupt service routine
 *---------------------------------------------------------------------------*/
void
ifpi2_isacsx_irq(struct l1_softc *sc, int ista)
{
	register u_char c = 0;
	register u_char istad = 0;

	NDBGL1(L1_F_MSG, "unit %d: ista = 0x%02x", sc->sc_unit, ista);

	/* was it an HDLC interrupt ? */
	if (ista & ISACSX_ISTA_ICD)
	{
		istad = ISAC_READ(I_ISTAD);
		NDBGL1(L1_F_MSG, "unit %d: istad = 0x%02x", sc->sc_unit, istad);

		if(istad & (ISACSX_ISTAD_RFO|ISACSX_ISTAD_XMR|ISACSX_ISTAD_XDU))
		{
			/* not really EXIR, but very similar */
			c |= ifpi2_isacsx_exir_hdlr(sc, istad);
		}
	}
	
	if(istad & ISACSX_ISTAD_RME)	/* receive message end */
	{
		register int rest;
		u_char rsta;

		/* get rx status register */
		
		rsta = ISAC_READ(I_RSTAD);

		/* Check for Frame and CRC valid */
		if((rsta & ISACSX_RSTAD_MASK) != (ISACSX_RSTAD_VFR|ISACSX_RSTAD_CRC))
		{
			int error = 0;
			
			if(!(rsta & ISACSX_RSTAD_VFR))	/* VFR error */
			{
				error++;
				NDBGL1(L1_I_ERR, "unit %d: Frame not valid error", sc->sc_unit);
			}
	
			if(!(rsta & ISACSX_RSTAD_CRC))	/* CRC error */
			{
				error++;
				NDBGL1(L1_I_ERR, "unit %d: CRC error", sc->sc_unit);
			}
	
			if(rsta & ISACSX_RSTAD_RDO)	/* ReceiveDataOverflow */
			{
				error++;
				NDBGL1(L1_I_ERR, "unit %d: Data Overrun error", sc->sc_unit);
			}
	
			if(rsta & ISACSX_RSTAD_RAB)	/* ReceiveABorted */
			{
				error++;
				NDBGL1(L1_I_ERR, "unit %d: Receive Aborted error", sc->sc_unit);
			}

			if(error == 0)			
				NDBGL1(L1_I_ERR, "unit %d: RME unknown error, RSTAD = 0x%02x!", sc->sc_unit, rsta);

			i4b_Dfreembuf(sc->sc_ibuf);

			c |= ISACSX_CMDRD_RMC|ISACSX_CMDRD_RRES;

			sc->sc_ibuf = NULL;
			sc->sc_ib = NULL;
			sc->sc_ilen = 0;

			ISAC_WRITE(I_CMDRD, ISACSX_CMDRD_RMC|ISACSX_CMDRD_RRES);

			return;
		}

		rest = (ISAC_READ(I_RBCLD) & (ISACSX_FIFO_LEN-1));

		if(rest == 0)
			rest = ISACSX_FIFO_LEN;

		if(sc->sc_ibuf == NULL)
		{
			if((sc->sc_ibuf = i4b_Dgetmbuf(rest)) != NULL)
				sc->sc_ib = sc->sc_ibuf->m_data;
			else
				panic("ifpi2_isacsx_irq: RME, i4b_Dgetmbuf returns NULL!\n");
			sc->sc_ilen = 0;
		}

		if(sc->sc_ilen <= (MAX_DFRAME_LEN - rest))
		{
			ISAC_RDFIFO(sc->sc_ib, rest);
			 /* the  last byte contains status, strip it */
			sc->sc_ilen += rest - 1;
			
			sc->sc_ibuf->m_pkthdr.len =
				sc->sc_ibuf->m_len = sc->sc_ilen;

			if(sc->sc_trace & TRACE_D_RX)
			{
				i4b_trace_hdr_t hdr;
				hdr.unit = L0IFPI2UNIT(sc->sc_unit);
				hdr.type = TRC_CH_D;
				hdr.dir = FROM_NT;
				hdr.count = ++sc->sc_trace_dcount;
				MICROTIME(hdr.time);
				i4b_l1_trace_ind(&hdr, sc->sc_ibuf->m_len, sc->sc_ibuf->m_data);
			}

			c |= ISACSX_CMDRD_RMC;

			if(sc->sc_enabled &&
			   (ctrl_desc[sc->sc_unit].protocol != PROTOCOL_D64S))
			{
				i4b_l1_ph_data_ind(L0IFPI2UNIT(sc->sc_unit), sc->sc_ibuf);
			}
			else
			{
				i4b_Dfreembuf(sc->sc_ibuf);
			}
		}
		else
		{
			NDBGL1(L1_I_ERR, "RME, input buffer overflow!");
			i4b_Dfreembuf(sc->sc_ibuf);
			c |= ISACSX_CMDRD_RMC|ISACSX_CMDRD_RRES;
		}

		sc->sc_ibuf = NULL;
		sc->sc_ib = NULL;
		sc->sc_ilen = 0;
	}

	if(istad & ISACSX_ISTAD_RPF)	/* receive fifo full */
	{
		if(sc->sc_ibuf == NULL)
		{
			if((sc->sc_ibuf = i4b_Dgetmbuf(MAX_DFRAME_LEN)) != NULL)
				sc->sc_ib= sc->sc_ibuf->m_data;
			else
				panic("ifpi2_isacsx_irq: RPF, i4b_Dgetmbuf returns NULL!\n");
			sc->sc_ilen = 0;
		}

		if(sc->sc_ilen <= (MAX_DFRAME_LEN - ISACSX_FIFO_LEN))
		{
			ISAC_RDFIFO(sc->sc_ib, ISACSX_FIFO_LEN);
			sc->sc_ilen += ISACSX_FIFO_LEN;			
			sc->sc_ib += ISACSX_FIFO_LEN;
			c |= ISACSX_CMDRD_RMC;
		}
		else
		{
			NDBGL1(L1_I_ERR, "RPF, input buffer overflow!");
			i4b_Dfreembuf(sc->sc_ibuf);
			sc->sc_ibuf = NULL;
			sc->sc_ib = NULL;
			sc->sc_ilen = 0;
			c |= ISACSX_CMDRD_RMC|ISACSX_CMDRD_RRES;			
		}
	}

	if(istad & ISACSX_ISTAD_XPR)	/* transmit fifo empty (XPR bit set) */
	{
		if((sc->sc_obuf2 != NULL) && (sc->sc_obuf == NULL))
		{
			sc->sc_freeflag = sc->sc_freeflag2;
			sc->sc_obuf = sc->sc_obuf2;
			sc->sc_op = sc->sc_obuf->m_data;
			sc->sc_ol = sc->sc_obuf->m_len;
			sc->sc_obuf2 = NULL;
#ifdef NOTDEF			
			printf("ob2=%x, op=%x, ol=%d, f=%d #",
				sc->sc_obuf,
				sc->sc_op,
				sc->sc_ol,
				sc->sc_state);
#endif				
		}
		else
		{
#ifdef NOTDEF
			printf("ob=%x, op=%x, ol=%d, f=%d #",
				sc->sc_obuf,
				sc->sc_op,
				sc->sc_ol,
				sc->sc_state);
#endif
		}			
		
		if(sc->sc_obuf)
		{			
			ISAC_WRFIFO(sc->sc_op, min(sc->sc_ol, ISACSX_FIFO_LEN));
	
			if(sc->sc_ol > ISACSX_FIFO_LEN)	/* length > 32 ? */
			{
				sc->sc_op += ISACSX_FIFO_LEN; /* bufferptr+32 */
				sc->sc_ol -= ISACSX_FIFO_LEN; /* length - 32 */
				c |= ISACSX_CMDRD_XTF;	    /* set XTF bit */
			}
			else
			{
				if(sc->sc_freeflag)
				{
					i4b_Dfreembuf(sc->sc_obuf);
					sc->sc_freeflag = 0;
				}
				sc->sc_obuf = NULL;
				sc->sc_op = NULL;
				sc->sc_ol = 0;
	
				c |= ISACSX_CMDRD_XTF | ISACSX_CMDRD_XME;
			}
		}
		else
		{
			sc->sc_state &= ~ISAC_TX_ACTIVE;
		}
	}
	
	if(ista & ISACSX_ISTA_CIC)	/* channel status change CISQ */
	{
		register u_char ci;
	
		/* get command/indication rx register*/
	
		ci = ISAC_READ(I_CIR0);

		/* C/I code change IRQ (flag already cleared by CIR0 read) */
	
		if(ci & ISACSX_CIR0_CIC0)
			ifpi2_isacsx_ind_hdlr(sc, (ci >> 4) & 0xf);
	}
	
	if(c)
	{
		ISAC_WRITE(I_CMDRD, c);
	}
}

/*---------------------------------------------------------------------------*
 *	ISACSX L1 Extended IRQ handler
 *---------------------------------------------------------------------------*/
static u_char
ifpi2_isacsx_exir_hdlr(register struct l1_softc *sc, u_char exir)
{
	u_char c = 0;
	
	if(exir & ISACSX_ISTAD_XMR)
	{
		NDBGL1(L1_I_ERR, "EXIRQ Tx Message Repeat");

		c |= ISACSX_CMDRD_XRES;
	}
	
	if(exir & ISACSX_ISTAD_XDU)
	{
		NDBGL1(L1_I_ERR, "EXIRQ Tx Data Underrun");

		c |= ISACSX_CMDRD_XRES;
	}

	if(exir & ISACSX_ISTAD_RFO)
	{
		NDBGL1(L1_I_ERR, "EXIRQ Rx Frame Overflow");

		c |= ISACSX_CMDRD_RMC;
	}

#if 0 /* all blocked per default */
	if(exir & ISACSX_EXIR_SOV)
	{
		NDBGL1(L1_I_ERR, "EXIRQ Sync Xfer Overflow");
	}

	if(exir & ISACSX_EXIR_MOS)
	{
		NDBGL1(L1_I_ERR, "EXIRQ Monitor Status");
	}

	if(exir & ISACSX_EXIR_SAW)
	{
		/* cannot happen, STCR:TSF is set to 0 */
		
		NDBGL1(L1_I_ERR, "EXIRQ Subscriber Awake");
	}

	if(exir & ISACSX_EXIR_WOV)
	{
		/* cannot happen, STCR:TSF is set to 0 */

		NDBGL1(L1_I_ERR, "EXIRQ Watchdog Timer Overflow");
	}
#endif

	return(c);
}

/*---------------------------------------------------------------------------*
 *	ISACSX L1 Indication handler
 *---------------------------------------------------------------------------*/
static void
ifpi2_isacsx_ind_hdlr(register struct l1_softc *sc, int ind)
{
	register int event;
	
	switch(ind)
	{
		case ISACSX_CIR0_IAI8:
			NDBGL1(L1_I_CICO, "rx AI8 in state %s", ifpi2_printstate(sc));
			if(sc->sc_bustyp == BUS_TYPE_IOM2)
				ifpi2_isacsx_l1_cmd(sc, CMD_AR8);
			event = EV_INFO48;
			i4b_l1_mph_status_ind(L0IFPI2UNIT(sc->sc_unit), STI_L1STAT, LAYER_ACTIVE, NULL);
			break;
			
		case ISACSX_CIR0_IAI10:
			NDBGL1(L1_I_CICO, "rx AI10 in state %s", ifpi2_printstate(sc));
			if(sc->sc_bustyp == BUS_TYPE_IOM2)
				ifpi2_isacsx_l1_cmd(sc, CMD_AR10);
			event = EV_INFO410;
			i4b_l1_mph_status_ind(L0IFPI2UNIT(sc->sc_unit), STI_L1STAT, LAYER_ACTIVE, NULL);
			break;

		case ISACSX_CIR0_IRSY:
			NDBGL1(L1_I_CICO, "rx RSY in state %s", ifpi2_printstate(sc));
			event = EV_RSY;
			break;

		case ISACSX_CIR0_IPU:
			NDBGL1(L1_I_CICO, "rx PU in state %s", ifpi2_printstate(sc));
			event = EV_PU;
			break;

		case ISACSX_CIR0_IDR:
			NDBGL1(L1_I_CICO, "rx DR in state %s", ifpi2_printstate(sc));
			ifpi2_isacsx_l1_cmd(sc, CMD_DIU);
			event = EV_DR;			
			break;
			
		case ISACSX_CIR0_IDID:
			NDBGL1(L1_I_CICO, "rx DID in state %s", ifpi2_printstate(sc));
			event = EV_INFO0;
			i4b_l1_mph_status_ind(L0IFPI2UNIT(sc->sc_unit), STI_L1STAT, LAYER_IDLE, NULL);
			break;

		case ISACSX_CIR0_IDIS:
			NDBGL1(L1_I_CICO, "rx DIS in state %s", ifpi2_printstate(sc));
			event = EV_DIS;
			break;

		case ISACSX_CIR0_IEI:
			NDBGL1(L1_I_CICO, "rx EI in state %s", ifpi2_printstate(sc));
			ifpi2_isacsx_l1_cmd(sc, CMD_DIU);
			event = EV_EI;
			break;

		case ISACSX_CIR0_IARD:
			NDBGL1(L1_I_CICO, "rx ARD in state %s", ifpi2_printstate(sc));
			event = EV_INFO2;
			break;

		case ISACSX_CIR0_ITI:
			NDBGL1(L1_I_CICO, "rx TI in state %s", ifpi2_printstate(sc));
			event = EV_INFO0;
			break;

		case ISACSX_CIR0_IATI:
			NDBGL1(L1_I_CICO, "rx ATI in state %s", ifpi2_printstate(sc));
			event = EV_INFO0;
			break;

		case ISACSX_CIR0_ISD:
			NDBGL1(L1_I_CICO, "rx SD in state %s", ifpi2_printstate(sc));
			event = EV_INFO0;
			break;
		
		default:
			NDBGL1(L1_I_ERR, "UNKNOWN Indication 0x%x in state %s", ind, ifpi2_printstate(sc));
			event = EV_INFO0;
			break;
	}
	ifpi2_next_state(sc, event);
}

/*---------------------------------------------------------------------------*
 *	execute a layer 1 command
 *---------------------------------------------------------------------------*/	
void
ifpi2_isacsx_l1_cmd(struct l1_softc *sc, int command)
{
	u_char cmd;

#ifdef I4B_SMP_WORKAROUND

	/* XXXXXXXXXXXXXXXXXXX */
	
	/*
	 * patch from Wolfgang Helbig:
	 *
	 * Here is a patch that makes i4b work on an SMP:
	 * The card (TELES 16.3) didn't interrupt on an SMP machine.
	 * This is a gross workaround, but anyway it works *and* provides
	 * some information as how to finally fix this problem.
	 */
	
	HSCX_WRITE(0, H_MASK, 0xff);
	HSCX_WRITE(1, H_MASK, 0xff);
	ISAC_WRITE(I_MASKD, 0xff);
	ISAC_WRITE(I_MASK, 0xff);
	DELAY(100);
	HSCX_WRITE(0, H_MASK, HSCX_A_IMASK);
	HSCX_WRITE(1, H_MASK, HSCX_B_IMASK);
	ISAC_WRITE(I_MASKD, isacsx_imaskd);
	ISAC_WRITE(I_MASK, isacsx_imask);

	/* XXXXXXXXXXXXXXXXXXX */
	
#endif /* I4B_SMP_WORKAROUND */

	if(command < 0 || command > CMD_ILL)
	{
		NDBGL1(L1_I_ERR, "illegal cmd 0x%x in state %s", command, ifpi2_printstate(sc));
		return;
	}
                                           
	cmd = ISACSX_CIX0_LOW;

	switch(command)
	{
		case CMD_TIM:
			NDBGL1(L1_I_CICO, "tx TIM in state %s", ifpi2_printstate(sc));
			cmd |= (ISACSX_CIX0_CTIM << 4);
			break;

		case CMD_RS:
			NDBGL1(L1_I_CICO, "tx RS in state %s", ifpi2_printstate(sc));
			cmd |= (ISACSX_CIX0_CRS << 4);
			break;

		case CMD_AR8:
			NDBGL1(L1_I_CICO, "tx AR8 in state %s", ifpi2_printstate(sc));
			cmd |= (ISACSX_CIX0_CAR8 << 4);
			break;

		case CMD_AR10:
			NDBGL1(L1_I_CICO, "tx AR10 in state %s", ifpi2_printstate(sc));
			cmd |= (ISACSX_CIX0_CAR10 << 4);
			break;

		case CMD_DIU:
			NDBGL1(L1_I_CICO, "tx DIU in state %s", ifpi2_printstate(sc));
			cmd |= (ISACSX_CIX0_CDIU << 4);
			break;
	}
	ISAC_WRITE(I_CIX0, cmd);
}

/*---------------------------------------------------------------------------*
 *	L1 ISACSX initialization
 *---------------------------------------------------------------------------*/
int
ifpi2_isacsx_init(struct l1_softc *sc)
{
	isacsx_imaskd = 0xff;		/* disable all irqs */
	isacsx_imask = 0xff;		/* disable all irqs */

	ISAC_WRITE(I_MASKD, isacsx_imaskd);
	ISAC_WRITE(I_MASK, isacsx_imask);

	/* the ISACSX only runs in IOM-2 mode */
	NDBGL1(L1_I_SETUP, "configuring for IOM-2 mode");

	/* TR_CONF0: Transceiver Configuration Register 0:
	 *	DIS_TR - transceiver enabled
	 *	EN_ICV - normal operation
	 *	EXLP - no external loop
	 *	LDD - automatic clock generation
	 */
	ISAC_WRITE(I_WTR_CONF0, 0);

	/* TR_CONF2: Transceiver Configuration Register 1:
	 *	DIS_TX - transmitter enabled
	 *	PDS - phase deviation 2 S-bits
	 *	RLP - remote line loop open
	 */
	ISAC_WRITE(I_WTR_CONF2, 0);

	/* MODED: Mode Register:
	 *	MDSx - transparent mode 0
	 *	TMD  - timer mode = external
	 *	RAC  - Receiver enabled
	 *	DIMx - digital i/f mode
	 */
	ISAC_WRITE(I_WMODED, ISACSX_MODED_MDS2|ISACSX_MODED_MDS1|ISACSX_MODED_RAC|ISACSX_MODED_DIM0);

	/* enabled interrupts:
	 * ===================
	 * RME  - receive message end
	 * RPF  - receive pool full
	 * RPO  - receive pool overflow
	 * XPR  - transmit pool ready
	 * XMR  - transmit message repeat
	 * XDU  - transmit data underrun
	 */

	isacsx_imaskd = ISACSX_MASKD_LOW;
	ISAC_WRITE(I_MASKD, isacsx_imaskd);

	/* enabled interrupts:
	 * ===================
	 * ICD - HDLC interrupt from D-channel
	 * CIC - C/I channel change
	 */

	isacsx_imask = ~(ISACSX_MASK_ICD | ISACSX_MASK_CIC);

	ISAC_WRITE(I_MASK, isacsx_imask);

	return(0);
}
