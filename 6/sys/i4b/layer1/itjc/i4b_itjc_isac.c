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
 *	i4b_itjc_isac.c - i4b NetJet-S ISAC handler
 *	--------------------------------------------
 *      last edit-date: [Wed Jan 10 17:15:54 2001]
 *
 *---------------------------------------------------------------------------*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_i4b.h"

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

#include <i4b/layer1/i4b_l1.h>

#include <i4b/layer1/isic/i4b_isic.h>
#include <i4b/layer1/isic/i4b_isac.h>

#include <i4b/layer1/itjc/i4b_itjc_ext.h>

#include <i4b/include/i4b_global.h>
#include <i4b/include/i4b_mbuf.h>

static u_char itjc_isac_exir_hdlr(register struct l1_softc *sc, u_char exir);
static void itjc_isac_ind_hdlr(register struct l1_softc *sc, int ind);

/*---------------------------------------------------------------------------*
 *	ISAC interrupt service routine
 *---------------------------------------------------------------------------*/
void
itjc_isac_irq(struct l1_softc *sc, int ista)
{
	register u_char c = 0;
	NDBGL1(L1_F_MSG, "unit %d: ista = 0x%02x", sc->sc_unit, ista);

	if(ista & ISAC_ISTA_EXI)	/* extended interrupt */
	{
		c |= itjc_isac_exir_hdlr(sc, ISAC_READ(I_EXIR));
	}
	
	if(ista & ISAC_ISTA_RME)	/* receive message end */
	{
		register int rest;
		u_char rsta;

		/* get rx status register */
		
		rsta = ISAC_READ(I_RSTA);

		if((rsta & ISAC_RSTA_MASK) != 0x20)
		{
			int error = 0;
			
			if(!(rsta & ISAC_RSTA_CRC))	/* CRC error */
			{
				error++;
				NDBGL1(L1_I_ERR, "unit %d: CRC error", sc->sc_unit);
			}
	
			if(rsta & ISAC_RSTA_RDO)	/* ReceiveDataOverflow */
			{
				error++;
				NDBGL1(L1_I_ERR, "unit %d: Data Overrun error", sc->sc_unit);
			}
	
			if(rsta & ISAC_RSTA_RAB)	/* ReceiveABorted */
			{
				error++;
				NDBGL1(L1_I_ERR, "unit %d: Receive Aborted error", sc->sc_unit);
			}

			if(error == 0)			
				NDBGL1(L1_I_ERR, "unit %d: RME unknown error, RSTA = 0x%02x!", sc->sc_unit, rsta);

			i4b_Dfreembuf(sc->sc_ibuf);

			c |= ISAC_CMDR_RMC|ISAC_CMDR_RRES;

			sc->sc_ibuf = NULL;
			sc->sc_ib = NULL;
			sc->sc_ilen = 0;

			ISAC_WRITE(I_CMDR, ISAC_CMDR_RMC|ISAC_CMDR_RRES);
			ISACCMDRWRDELAY();

			return;
		}

		rest = (ISAC_READ(I_RBCL) & (ISAC_FIFO_LEN-1));

		if(rest == 0)
			rest = ISAC_FIFO_LEN;

		if(sc->sc_ibuf == NULL)
		{
			if((sc->sc_ibuf = i4b_Dgetmbuf(rest)) != NULL)
				sc->sc_ib = sc->sc_ibuf->m_data;
			else
				panic("itjc_isac_irq: RME, i4b_Dgetmbuf returns NULL!\n");
			sc->sc_ilen = 0;
		}

		if(sc->sc_ilen <= (MAX_DFRAME_LEN - rest))
		{
			ISAC_RDFIFO(sc->sc_ib, rest);
			sc->sc_ilen += rest;
			
			sc->sc_ibuf->m_pkthdr.len =
				sc->sc_ibuf->m_len = sc->sc_ilen;

			if(sc->sc_trace & TRACE_D_RX)
			{
				i4b_trace_hdr_t hdr;
				hdr.unit = L0ITJCUNIT(sc->sc_unit);
				hdr.type = TRC_CH_D;
				hdr.dir = FROM_NT;
				hdr.count = ++sc->sc_trace_dcount;
				MICROTIME(hdr.time);
				i4b_l1_trace_ind(&hdr, sc->sc_ibuf->m_len, sc->sc_ibuf->m_data);
			}

			c |= ISAC_CMDR_RMC;

			if(sc->sc_enabled &&
			   (ctrl_desc[sc->sc_unit].protocol != PROTOCOL_D64S))
			{
				i4b_l1_ph_data_ind(L0ITJCUNIT(sc->sc_unit), sc->sc_ibuf);
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
			c |= ISAC_CMDR_RMC|ISAC_CMDR_RRES;
		}

		sc->sc_ibuf = NULL;
		sc->sc_ib = NULL;
		sc->sc_ilen = 0;
	}

	if(ista & ISAC_ISTA_RPF)	/* receive fifo full */
	{
		if(sc->sc_ibuf == NULL)
		{
			if((sc->sc_ibuf = i4b_Dgetmbuf(MAX_DFRAME_LEN)) != NULL)
				sc->sc_ib= sc->sc_ibuf->m_data;
			else
				panic("itjc_isac_irq: RPF, i4b_Dgetmbuf returns NULL!\n");
			sc->sc_ilen = 0;
		}

		if(sc->sc_ilen <= (MAX_DFRAME_LEN - ISAC_FIFO_LEN))
		{
			ISAC_RDFIFO(sc->sc_ib, ISAC_FIFO_LEN);
			sc->sc_ilen += ISAC_FIFO_LEN;			
			sc->sc_ib += ISAC_FIFO_LEN;
			c |= ISAC_CMDR_RMC;
		}
		else
		{
			NDBGL1(L1_I_ERR, "RPF, input buffer overflow!");
			i4b_Dfreembuf(sc->sc_ibuf);
			sc->sc_ibuf = NULL;
			sc->sc_ib = NULL;
			sc->sc_ilen = 0;
			c |= ISAC_CMDR_RMC|ISAC_CMDR_RRES;			
		}
	}

	if(ista & ISAC_ISTA_XPR)	/* transmit fifo empty (XPR bit set) */
	{
		if((sc->sc_obuf2 != NULL) && (sc->sc_obuf == NULL))
		{
			sc->sc_freeflag = sc->sc_freeflag2;
			sc->sc_obuf = sc->sc_obuf2;
			sc->sc_op = sc->sc_obuf->m_data;
			sc->sc_ol = sc->sc_obuf->m_len;
			sc->sc_obuf2 = NULL;
		}
		
		if(sc->sc_obuf)
		{			
			ISAC_WRFIFO(sc->sc_op, min(sc->sc_ol, ISAC_FIFO_LEN));
	
			if(sc->sc_ol > ISAC_FIFO_LEN)	/* length > 32 ? */
			{
				sc->sc_op += ISAC_FIFO_LEN; /* bufferptr+32 */
				sc->sc_ol -= ISAC_FIFO_LEN; /* length - 32 */
				c |= ISAC_CMDR_XTF;	    /* set XTF bit */
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
	
				c |= ISAC_CMDR_XTF | ISAC_CMDR_XME;
			}
		}
		else
		{
			sc->sc_state &= ~ISAC_TX_ACTIVE;
		}
	}
	
	if(ista & ISAC_ISTA_CISQ)	/* channel status change CISQ */
	{
		register u_char ci;
	
		/* get command/indication rx register*/
	
		ci = ISAC_READ(I_CIRR);

		/* if S/Q IRQ, read SQC reg to clr SQC IRQ */
	
		if(ci & ISAC_CIRR_SQC)
			(void) ISAC_READ(I_SQRR);

		/* C/I code change IRQ (flag already cleared by CIRR read) */
	
		if(ci & ISAC_CIRR_CIC0)
			itjc_isac_ind_hdlr(sc, (ci >> 2) & 0xf);
	}
	
	if(c)
	{
		ISAC_WRITE(I_CMDR, c);
		ISACCMDRWRDELAY();
	}
}

/*---------------------------------------------------------------------------*
 *	ISAC L1 Extended IRQ handler
 *---------------------------------------------------------------------------*/
static u_char
itjc_isac_exir_hdlr(register struct l1_softc *sc, u_char exir)
{
	u_char c = 0;
	
	if(exir & ISAC_EXIR_XMR)
	{
		NDBGL1(L1_I_ERR, "EXIRQ Tx Message Repeat");

		c |= ISAC_CMDR_XRES;
	}
	
	if(exir & ISAC_EXIR_XDU)
	{
		NDBGL1(L1_I_ERR, "EXIRQ Tx Data Underrun");

		c |= ISAC_CMDR_XRES;
	}

	if(exir & ISAC_EXIR_PCE)
	{
		NDBGL1(L1_I_ERR, "EXIRQ Protocol Error");
	}

	if(exir & ISAC_EXIR_RFO)
	{
		NDBGL1(L1_I_ERR, "EXIRQ Rx Frame Overflow");

		c |= ISAC_CMDR_RMC|ISAC_CMDR_RRES;
	}

	if(exir & ISAC_EXIR_SOV)
	{
		NDBGL1(L1_I_ERR, "EXIRQ Sync Xfer Overflow");
	}

	if(exir & ISAC_EXIR_MOS)
	{
		NDBGL1(L1_I_ERR, "EXIRQ Monitor Status");
	}

	if(exir & ISAC_EXIR_SAW)
	{
		/* cannot happen, STCR:TSF is set to 0 */
		
		NDBGL1(L1_I_ERR, "EXIRQ Subscriber Awake");
	}

	if(exir & ISAC_EXIR_WOV)
	{
		/* cannot happen, STCR:TSF is set to 0 */

		NDBGL1(L1_I_ERR, "EXIRQ Watchdog Timer Overflow");
	}

	return(c);
}

/*---------------------------------------------------------------------------*
 *	ISAC L1 Indication handler
 *---------------------------------------------------------------------------*/
static void
itjc_isac_ind_hdlr(register struct l1_softc *sc, int ind)
{
	register int event;
	
	switch(ind)
	{
		case ISAC_CIRR_IAI8:
			NDBGL1(L1_I_CICO, "rx AI8 in state %s", itjc_printstate(sc));
			itjc_isac_l1_cmd(sc, CMD_AR8);
			event = EV_INFO48;
			i4b_l1_mph_status_ind(L0ITJCUNIT(sc->sc_unit), STI_L1STAT, LAYER_ACTIVE, NULL);
			break;
			
		case ISAC_CIRR_IAI10:
			NDBGL1(L1_I_CICO, "rx AI10 in state %s", itjc_printstate(sc));
			itjc_isac_l1_cmd(sc, CMD_AR10);
			event = EV_INFO410;
			i4b_l1_mph_status_ind(L0ITJCUNIT(sc->sc_unit), STI_L1STAT, LAYER_ACTIVE, NULL);
			break;

		case ISAC_CIRR_IRSY:
			NDBGL1(L1_I_CICO, "rx RSY in state %s", itjc_printstate(sc));
			event = EV_RSY;
			break;

		case ISAC_CIRR_IPU:
			NDBGL1(L1_I_CICO, "rx PU in state %s", itjc_printstate(sc));
			event = EV_PU;
			break;

		case ISAC_CIRR_IDR:
			NDBGL1(L1_I_CICO, "rx DR in state %s", itjc_printstate(sc));
			itjc_isac_l1_cmd(sc, CMD_DIU);
			event = EV_DR;			
			break;
			
		case ISAC_CIRR_IDID:
			NDBGL1(L1_I_CICO, "rx DID in state %s", itjc_printstate(sc));
			event = EV_INFO0;
			i4b_l1_mph_status_ind(L0ITJCUNIT(sc->sc_unit), STI_L1STAT, LAYER_IDLE, NULL);
			break;

		case ISAC_CIRR_IDIS:
			NDBGL1(L1_I_CICO, "rx DIS in state %s", itjc_printstate(sc));
			event = EV_DIS;
			break;

		case ISAC_CIRR_IEI:
			NDBGL1(L1_I_CICO, "rx EI in state %s", itjc_printstate(sc));
			itjc_isac_l1_cmd(sc, CMD_DIU);
			event = EV_EI;
			break;

		case ISAC_CIRR_IARD:
			NDBGL1(L1_I_CICO, "rx ARD in state %s", itjc_printstate(sc));
			event = EV_INFO2;
			break;

		case ISAC_CIRR_ITI:
			NDBGL1(L1_I_CICO, "rx TI in state %s", itjc_printstate(sc));
			event = EV_INFO0;
			break;

		case ISAC_CIRR_IATI:
			NDBGL1(L1_I_CICO, "rx ATI in state %s", itjc_printstate(sc));
			event = EV_INFO0;
			break;

		case ISAC_CIRR_ISD:
			NDBGL1(L1_I_CICO, "rx SD in state %s", itjc_printstate(sc));
			event = EV_INFO0;
			break;
		
		default:
			NDBGL1(L1_I_ERR, "UNKNOWN Indication 0x%x in state %s", ind, itjc_printstate(sc));
			event = EV_INFO0;
			break;
	}
	itjc_next_state(sc, event);
}

/*---------------------------------------------------------------------------*
 *	execute a layer 1 command
 *---------------------------------------------------------------------------*/	
void
itjc_isac_l1_cmd(struct l1_softc *sc, int command)
{
	u_char cmd;

	if(command < 0 || command > CMD_ILL)
	{
		NDBGL1(L1_I_ERR, "illegal cmd 0x%x in state %s", command, itjc_printstate(sc));
		return;
	}
                                           
	cmd = ISAC_CIX0_LOW;

	switch(command)
	{
		case CMD_TIM:
			NDBGL1(L1_I_CICO, "tx TIM in state %s", itjc_printstate(sc));
			cmd |= (ISAC_CIXR_CTIM << 2);
			break;

		case CMD_RS:
			NDBGL1(L1_I_CICO, "tx RS in state %s", itjc_printstate(sc));
			cmd |= (ISAC_CIXR_CRS << 2);
			break;

		case CMD_AR8:
			NDBGL1(L1_I_CICO, "tx AR8 in state %s", itjc_printstate(sc));
			cmd |= (ISAC_CIXR_CAR8 << 2);
			break;

		case CMD_AR10:
			NDBGL1(L1_I_CICO, "tx AR10 in state %s", itjc_printstate(sc));
			cmd |= (ISAC_CIXR_CAR10 << 2);
			break;

		case CMD_DIU:
			NDBGL1(L1_I_CICO, "tx DIU in state %s", itjc_printstate(sc));
			cmd |= (ISAC_CIXR_CDIU << 2);
			break;
	}
	ISAC_WRITE(I_CIXR, cmd);
}

/*---------------------------------------------------------------------------*
 *	L1 ISAC initialization
 *---------------------------------------------------------------------------*/
int
itjc_isac_init(struct l1_softc *sc)
{
	ISAC_IMASK = 0xff;		/* disable all irqs */

	ISAC_WRITE(I_MASK, ISAC_IMASK);

	NDBGL1(L1_I_SETUP, "configuring for IOM-2 mode");

	/* ADF2: Select mode IOM-2 */		
	ISAC_WRITE(I_ADF2, ISAC_ADF2_IMS);

	/* SPCR: serial port control register:
	 *	SPU - software power up = 0
	 *	SPM - timing mode 0
	 *	TLP - test loop = 0
	 *	C1C, C2C - B1 + C1 and B2 + IC2 monitoring
	 */
	ISAC_WRITE(I_SPCR, 0x00);

	/* SQXR: S/Q channel xmit register:
	 *	IDC  - IOM direction = 0 (master)
	 *	CFS  - Config Select = 0 (clock always active)
	 *	CI1E - C/I channel 1 IRQ enable = 0
                 *	SQIE - S/Q IRQ enable = 0
	 *	SQX1-4 - Fa bits = 1
	 */
	ISAC_WRITE(I_SQXR, ISAC_SQXR_SQX1|ISAC_SQXR_SQX2|ISAC_SQXR_SQX3|ISAC_SQXR_SQX4);

	/* ADF1: additional feature reg 1:
	 *	WTC - watchdog = 0
	 *	TEM - test mode = 0
	 *	PFS - pre-filter = 0
	 *	IOF - IOM i/f off = 0
	 *	ITF - interframe fill = idle
	 */	
	ISAC_WRITE(I_ADF1, 0x00);

	/* STCR: sync transfer control reg:
	 *	TSF - terminal secific functions = 0
	 *	TBA - TIC bus address = 7
	 *	STx/SCx = 0
	 */
	ISAC_WRITE(I_STCR, ISAC_STCR_TBA2|ISAC_STCR_TBA1|ISAC_STCR_TBA0);

	/* MODE: Mode Register:
	 *	MDSx - transparent mode 2
	 *	TMD  - timer mode = external
	 *	RAC  - Receiver enabled
	 *	DIMx - digital i/f mode
	 */
	ISAC_WRITE(I_MODE, ISAC_MODE_MDS2|ISAC_MODE_MDS1|ISAC_MODE_RAC|ISAC_MODE_DIM0);

	/* enabled interrupts:
	 * ===================
	 * RME  - receive message end
	 * RPF  - receive pool full
	 * XPR  - transmit pool ready
	 * CISQ - CI or S/Q channel change
	 * EXI  - extended interrupt
	 */

	ISAC_IMASK = ISAC_MASK_RSC |	/* auto mode only	*/
		     ISAC_MASK_TIN | 	/* timer irq		*/
		     ISAC_MASK_SIN;	/* sync xfer irq	*/

	ISAC_WRITE(I_MASK, ISAC_IMASK);

	return(0);
}
