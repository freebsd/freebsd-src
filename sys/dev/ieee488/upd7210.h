/*-
 * Copyright (c) 2005 Poul-Henning Kamp <phk@FreeBSD.org>
 * Copyright (c) 2010 Joerg Wunsch <joerg@FreeBSD.org>
 * All rights reserved.
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
 * $FreeBSD$
 *
 * Locating an actual µPD7210 data book has proven quite impossible for me.
 * There are a fair number of newer chips which are supersets of the µPD7210
 * but they are particular eager to comprehensively mark what the extensions
 * are and what is in the base set.  Some even give the registers and their
 * bits new names.
 *
 * The following information is based on a description of the µPD7210 found
 * in an old manual for a VME board which used the chip.
 */

#ifndef _DEV_IEEE488_UPD7210_H_
#define _DEV_IEEE488_UPD7210_H_
#ifdef _KERNEL

struct upd7210;
struct ibfoo;

/* upd7210 interface definitions for HW drivers */

typedef int upd7210_irq_t(struct upd7210 *, int);

struct upd7210 {
	struct resource		*reg_res[8];
	struct resource		*irq_clear_res;
	int			dmachan;
	int			unit;

	/* private stuff */
	struct mtx		mutex;
	uint8_t			rreg[8];
	uint8_t			wreg[8 + 8];

	upd7210_irq_t		*irq;

	int			busy;
	u_char			*buf;
	size_t			bufsize;
	u_int			buf_wp;
	u_int			buf_rp;
	struct cdev		*cdev;

	struct ibfoo		*ibfoo;
};

#ifdef UPD7210_HW_DRIVER
void upd7210intr(void *);
void upd7210attach(struct upd7210 *);
void upd7210detach(struct upd7210 *);
#endif

#ifdef UPD7210_SW_DRIVER

/* upd7210 hardware definitions. */

/* Write registers */
enum upd7210_wreg {
	CDOR	= 0,			/* Command/Data Out Register	*/
	IMR1	= 1,			/* Interrupt Mask Register 1	*/
	IMR2	= 2,			/* Interrupt Mask Register 2	*/
	SPMR	= 3,			/* Serial Poll Mode Register	*/
	ADMR	= 4,			/* ADdress Mode Register	*/
	AUXMR	= 5,			/* AUXilliary Mode Register	*/
	ICR	= 5,			/* Internal Counter Register	*/
	PPR	= 5,			/* Parallel Poll Register	*/
	AUXRA	= 5,			/* AUXilliary Register A	*/
	AUXRB	= 5,			/* AUXilliary Register B	*/
	AUXRE	= 5,			/* AUXilliary Register E	*/
	ADR	= 6,			/* ADdress Register		*/
	EOSR	= 7,			/* End-Of-String Register	*/
};

/* Read registers */
enum upd7210_rreg {
	DIR	= 0,			/* Data In Register		*/
	ISR1	= 1,			/* Interrupt Status Register 1	*/
	ISR2	= 2,			/* Interrupt Status Register 2	*/
	SPSR	= 3,			/* Serial Poll Status Register	*/
	ADSR	= 4,			/* ADdress Status Register	*/
	CPTR	= 5,			/* Command Pass Though Register	*/
	ADR0	= 6,			/* ADdress Register 0		*/
	ADR1	= 7,			/* ADdress Register 1		*/
};

/* Bits for ISR1 and IMR1 */
#define IXR1_DI		(1 << 0)	/* Data In			*/
#define IXR1_DO		(1 << 1)	/* Data Out			*/
#define IXR1_ERR	(1 << 2)	/* Error			*/
#define IXR1_DEC	(1 << 3)	/* Device Clear			*/
#define IXR1_ENDRX	(1 << 4)	/* End Received			*/
#define IXR1_DET	(1 << 5)	/* Device Execute Trigger	*/
#define IXR1_APT	(1 << 6)	/* Address Pass-Through		*/
#define IXR1_CPT	(1 << 7)	/* Command Pass-Through		*/

/* Bits for ISR2 and IMR2 */
#define IXR2_ADSC	(1 << 0)	/* Addressed Status Change	*/
#define IXR2_REMC	(1 << 1)	/* Remote Change		*/
#define IXR2_LOKC	(1 << 2)	/* Lockout Change		*/
#define IXR2_CO		(1 << 3)	/* Command Out			*/
#define ISR2_REM	(1 << 4)	/* Remove			*/
#define IMR2_DMAI	(1 << 4)	/* DMA In Enable		*/
#define ISR2_LOK	(1 << 5)	/* Lockout			*/
#define IMR2_DMAO	(1 << 5)	/* DMA Out Enable		*/
#define IXR2_SRQI	(1 << 6)	/* Service Request Input	*/
#define ISR2_INT	(1 << 7)	/* Interrupt			*/

#define SPSR_PEND	(1 << 6)	/* Pending			*/
#define SPMR_RSV	(1 << 6)	/* Request SerVice		*/

#define ADSR_MJMN	(1 << 0)	/* MaJor MiNor			*/
#define ADSR_TA		(1 << 1)	/* Talker Active		*/
#define ADSR_LA		(1 << 2)	/* Listener Active		*/
#define ADSR_TPAS	(1 << 3)	/* Talker Primary Addr. State	*/
#define ADSR_LPAS	(1 << 4)	/* Listener Primary Addr. State	*/
#define ADSR_SPMS	(1 << 5)	/* Serial Poll Mode State	*/
#define ADSR_ATN	(1 << 6)	/* Attention			*/
#define ADSR_CIC	(1 << 7)	/* Controller In Charge		*/

#define ADMR_ADM0	(1 << 0)	/* Address Mode 0		*/
#define ADMR_ADM1	(1 << 1)	/* Address Mode 1		*/
#define ADMR_TRM0	(1 << 4)	/* Transmit/Receive Mode 0	*/
#define ADMR_TRM1	(1 << 5)	/* Transmit/Receive Mode 1	*/
#define ADMR_LON	(1 << 6)	/* Listen Only			*/
#define ADMR_TON	(1 << 7)	/* Talk Only			*/

/* Constant part of overloaded write registers */
#define	C_ICR		0x20
#define	C_PPR		0x60
#define	C_AUXA		0x80
#define	C_AUXB		0xa0
#define	C_AUXE		0xc0

#define AUXMR_PON	0x00		/* Immediate Execute pon	*/
#define AUXMR_CPP	0x01		/* Clear Parallel Poll		*/
#define AUXMR_CRST	0x02		/* Chip Reset			*/
#define AUXMR_RFD	0x03		/* Finish Handshake		*/
#define AUXMR_TRIG	0x04		/* Trigger			*/
#define AUXMR_RTL	0x05		/* Return to local		*/
#define AUXMR_SEOI	0x06		/* Send EOI			*/
#define AUXMR_NVSA	0x07		/* Non-Valid Secondary cmd/addr	*/
					/* 0x08 undefined/unknown	*/
#define AUXMR_SPP	0x09		/* Set Parallel Poll		*/
					/* 0x0a undefined/unknown	*/
					/* 0x0b undefined/unknown	*/
					/* 0x0c undefined/unknown	*/
					/* 0x0d undefined/unknown	*/
					/* 0x0e undefined/unknown	*/
#define AUXMR_VSA	0x0f		/* Valid Secondary cmd/addr	*/
#define AUXMR_GTS	0x10		/* Go to Standby		*/
#define AUXMR_TCA	0x11		/* Take Control Async (pulsed)	*/
#define AUXMR_TCS	0x12		/* Take Control Synchronously	*/
#define AUXMR_LISTEN	0x13		/* Listen			*/
#define AUXMR_DSC	0x14		/* Disable System Control	*/
					/* 0x15 undefined/unknown	*/
#define AUXMR_SIFC	0x16		/* Set IFC			*/
#define AUXMR_CREN	0x17		/* Clear REN			*/
					/* 0x18 undefined/unknown	*/
					/* 0x19 undefined/unknown	*/
#define AUXMR_TCSE	0x1a		/* Take Control Sync on End	*/
#define AUXMR_LCM	0x1b		/* Listen Continuously Mode	*/
#define AUXMR_LUNL	0x1c		/* Local Unlisten		*/
#define AUXMR_EPP	0x1d		/* Execute Parallel Poll	*/
#define AUXMR_CIFC	0x1e		/* Clear IFC			*/
#define AUXMR_SREN	0x1f		/* Set REN			*/

#define PPR_U		(1 << 4)	/* Unconfigure			*/
#define PPR_S		(1 << 3)	/* Status Polarity		*/

#define AUXA_HLDA	(1 << 0)	/* Holdoff on All		*/
#define AUXA_HLDE	(1 << 1)	/* Holdoff on END		*/
#define AUXA_REOS	(1 << 2)	/* End on EOS received		*/
#define AUXA_XEOS	(1 << 3)	/* Transmit END with EOS	*/
#define AUXA_BIN	(1 << 4)	/* Binary			*/

#define AUXB_CPTE	(1 << 0)	/* Cmd Pass Through Enable	*/
#define AUXB_SPEOI	(1 << 1)	/* Send Serial Poll EOI		*/
#define AUXB_TRI	(1 << 2)	/* Three-State Timing		*/
#define AUXB_INV	(1 << 3)	/* Invert			*/
#define AUXB_ISS	(1 << 4)	/* Individual Status Select	*/

#define AUXE_DHDT	(1 << 0)	/* DAC Holdoff on DTAS		*/
#define AUXE_DHDC	(1 << 1)	/* DAC Holdoff on DCAS		*/

#define ADR0_DL0	(1 << 5)	/* Disable Listener 0		*/
#define ADR0_DT0	(1 << 6)	/* Disable Talker 0		*/

#define ADR_DL		(1 << 5)	/* Disable Listener		*/
#define ADR_DT		(1 << 6)	/* Disable Talker		*/
#define ADR_ARS		(1 << 7)	/* Address Register Select	*/

#define ADR1_DL1	(1 << 5)	/* Disable Listener 1		*/
#define ADR1_DT1	(1 << 6)	/* Disable Talker 1		*/
#define ADR1_EOI	(1 << 7)	/* End or Identify		*/

/* Stuff from software drivers */
extern struct cdevsw gpib_ib_cdevsw;

/* Stuff from upd7210.c */
void upd7210_print_isr(u_int isr1, u_int isr2);
u_int upd7210_rd(struct upd7210 *u, enum upd7210_rreg reg);
void upd7210_wr(struct upd7210 *u, enum upd7210_wreg reg, u_int val);
int upd7210_take_ctrl_async(struct upd7210 *u);
int upd7210_goto_standby(struct upd7210 *u);

#endif /* UPD7210_SW_DRIVER */

#endif /* _KERNEL */
#endif /* _DEV_IEEE488_UPD7210_H_ */
