/*
 * Copyright (c) 1996, by Peter Wemm and Steve Passe
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the developer may NOT be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *	$Id: apic.h,v 1.4 1997/06/22 16:03:46 peter Exp $
 */

#ifndef _MACHINE_APIC_H_
#define _MACHINE_APIC_H_

/*
 * Local && I/O APIC definitions.
 */

/*
 * Pentium P54C+ Build-in APIC
 * (Advanced programmable Interrupt Controller)
 * 
 * Base Address of Build-in APIC in memory location
 * is 0xfee00000.
 * 
 * Map of APIC REgisters:
 * 
 * Offset (hex)    Description                     Read/Write state
 * 000             Reserved
 * 010             Reserved
 * 020 ID          Local APIC ID                   R/W
 * 030 VER         Local APIC Version              R
 * 040             Reserved
 * 050             Reserved
 * 060             Reserved
 * 070             Reserved
 * 080             Task Priority Register          R/W
 * 090             Arbitration Priority Register   R
 * 0A0             Processor Priority Register     R
 * 0B0             EOI Register                    W
 * 0C0 RRR         Remote read                     R
 * 0D0             Logical Destination             R/W
 * 0E0             Destination Format Register     0..27 R;  28..31 R/W
 * 0F0 SVR         Spurious Interrupt Vector Reg.  0..3  R;  4..9   R/W
 * 100             ISR  000-031                    R
 * 110             ISR  032-063                    R
 * 120             ISR  064-095                    R
 * 130             ISR  095-128                    R
 * 140             ISR  128-159                    R
 * 150             ISR  160-191                    R
 * 160             ISR  192-223                    R
 * 170             ISR  224-255                    R
 * 180             TMR  000-031                    R
 * 190             TMR  032-063                    R
 * 1A0             TMR  064-095                    R
 * 1B0             TMR  095-128                    R
 * 1C0             TMR  128-159                    R
 * 1D0             TMR  160-191                    R
 * 1E0             TMR  192-223                    R
 * 1F0             TMR  224-255                    R
 * 200             IRR  000-031                    R
 * 210             IRR  032-063                    R
 * 220             IRR  064-095                    R
 * 230             IRR  095-128                    R
 * 240             IRR  128-159                    R
 * 250             IRR  160-191                    R
 * 260             IRR  192-223                    R
 * 270             IRR  224-255                    R
 * 280             Error Status Register           R
 * 290             Reserved
 * 2A0             Reserved
 * 2B0             Reserved
 * 2C0             Reserved
 * 2D0             Reserved
 * 2E0             Reserved
 * 2F0             Reserved
 * 300 ICR_LOW     Interrupt Command Reg. (0-31)   R/W
 * 310 ICR_HI      Interrupt Command Reg. (32-63)  R/W
 * 320             Local Vector Table (Timer)      R/W
 * 330             Reserved
 * 340             Reserved
 * 350 LVT1        Local Vector Table (LINT0)      R/W
 * 360 LVT2        Local Vector Table (LINT1)      R/W
 * 370 LVT3        Local Vector Table (ERROR)      R/W
 * 380             Initial Count Reg. for Timer    R/W
 * 390             Current Count of Timer          R
 * 3A0             Reserved
 * 3B0             Reserved
 * 3C0             Reserved
 * 3D0             Reserved
 * 3E0             Timer Divide Configuration Reg. R/W
 * 3F0             Reserved
 */


/******************************************************************************
 * global defines, etc.
 */

/**
 * this enables code concerned with handling more than one IO APIC.
 * Note: this is NOT READY for use!
 *
#define MULTIPLE_IOAPICS
 */

/******************************************************************************
 * LOCAL APIC structure
 */

#ifndef LOCORE
#include <sys/types.h>

#define PAD3	int : 32; int : 32; int : 32
#define PAD4	int : 32; int : 32; int : 32; int : 32

struct LAPIC {
	/* reserved */		PAD4;
	/* reserved */		PAD4;
	u_int32_t id;		PAD3;
	u_int32_t version;	PAD3;
	/* reserved */		PAD4;
	/* reserved */		PAD4;
	/* reserved */		PAD4;
	/* reserved */		PAD4;
	u_int32_t tpr;		PAD3;
	u_int32_t apr;		PAD3;
	u_int32_t ppr;		PAD3;
	u_int32_t eoi;		PAD3;
	/* reserved */		PAD4;
	u_int32_t ldr;		PAD3;
	u_int32_t dfr;		PAD3;
	u_int32_t svr;		PAD3;
	u_int32_t isr0;		PAD3;
	u_int32_t isr1;		PAD3;
	u_int32_t isr2;		PAD3;
	u_int32_t isr3;		PAD3;
	u_int32_t isr4;		PAD3;
	u_int32_t isr5;		PAD3;
	u_int32_t isr6;		PAD3;
	u_int32_t isr7;		PAD3;
	u_int32_t tmr0;		PAD3;
	u_int32_t tmr1;		PAD3;
	u_int32_t tmr2;		PAD3;
	u_int32_t tmr3;		PAD3;
	u_int32_t tmr4;		PAD3;
	u_int32_t tmr5;		PAD3;
	u_int32_t tmr6;		PAD3;
	u_int32_t tmr7;		PAD3;
	u_int32_t irr0;		PAD3;
	u_int32_t irr1;		PAD3;
	u_int32_t irr2;		PAD3;
	u_int32_t irr3;		PAD3;
	u_int32_t irr4;		PAD3;
	u_int32_t irr5;		PAD3;
	u_int32_t irr6;		PAD3;
	u_int32_t irr7;		PAD3;
	u_int32_t esr;		PAD3;
	/* reserved */		PAD4;
	/* reserved */		PAD4;
	/* reserved */		PAD4;
	/* reserved */		PAD4;
	/* reserved */		PAD4;
	/* reserved */		PAD4;
	/* reserved */		PAD4;
	u_int32_t icr_lo;	PAD3;
	u_int32_t icr_hi;	PAD3;
	u_int32_t lvt_timer;	PAD3;
	/* reserved */		PAD4;
	u_int32_t lvt_pcint;	PAD3;
	u_int32_t lvt_lint0;	PAD3;
	u_int32_t lvt_lint1;	PAD3;
	u_int32_t lvt_error;	PAD3;
	u_int32_t icr_timer;	PAD3;
	u_int32_t ccr_timer;	PAD3;
	/* reserved */		PAD4;
	/* reserved */		PAD4;
	/* reserved */		PAD4;
	/* reserved */		PAD4;
	u_int32_t dcr_timer;	PAD3;
	/* reserved */		PAD4;
};

typedef struct LAPIC lapic_t;

/******************************************************************************
 * I/O APIC structure
 */

struct IOAPIC {
	u_int32_t ioregsel;	PAD3;
	u_int32_t iowin;	PAD3;
};

typedef struct IOAPIC ioapic_t;

#undef PAD4
#undef PAD3

#endif  /* LOCORE */


/******************************************************************************
 * LOCAL APIC defines
 */

/* default physical locations of LOCAL (CPU) APICs */
#define DEFAULT_APIC_BASE	0xfee00000

/* fields in VER */
#define APIC_VER_VERSION	0x000000ff
#define APIC_VER_MAXLVT		0x00ff0000
#define MAXLVTSHIFT		16

/* fields in SVR */
#define APIC_SVR_ENABLE		0x00000100
# define APIC_SVR_SWDIS		0x00000000
# define APIC_SVR_SWEN		0x00000100
#define APIC_SVR_FOCUS		0x00000200
# define APIC_SVR_FEN		0x00000000
# define APIC_SVR_FDIS		0x00000200
#define APIC_TPR_PRIO		0x000000ff
# define APIC_TPR_INT		0x000000f0
# define APIC_TPR_SUB		0x0000000f


/* fields in ICR_LOW */
#define APIC_VECTOR_MASK	0x000000ff

#define APIC_DELMODE_MASK	0x00000700
# define APIC_DELMODE_FIXED	0x00000000
# define APIC_DELMODE_LOWPRIO	0x00000100
# define APIC_DELMODE_SMI	0x00000200
# define APIC_DELMODE_RR	0x00000300
# define APIC_DELMODE_NMI	0x00000400
# define APIC_DELMODE_INIT	0x00000500
# define APIC_DELMODE_STARTUP	0x00000600
# define APIC_DELMODE_RESV	0x00000700

#define APIC_DESTMODE_MASK	0x00000800
# define APIC_DESTMODE_PHY	0x00000000
# define APIC_DESTMODE_LOG	0x00000800

#define APIC_DELSTAT_MASK	0x00001000
# define APIC_DELSTAT_IDLE	0x00000000
# define APIC_DELSTAT_PEND	0x00001000

#define APIC_RESV1_MASK		0x00002000

#define APIC_LEVEL_MASK		0x00004000
# define APIC_LEVEL_DEASSERT	0x00000000
# define APIC_LEVEL_ASSERT	0x00004000

#define APIC_TRIGMOD_MASK	0x00008000
# define APIC_TRIGMOD_EDGE	0x00000000
# define APIC_TRIGMOD_LEVEL	0x00008000

#define APIC_RRSTAT_MASK	0x00030000
# define APIC_RRSTAT_INVALID	0x00000000
# define APIC_RRSTAT_INPROG	0x00010000
# define APIC_RRSTAT_VALID	0x00020000
# define APIC_RRSTAT_RESV	0x00030000

#define APIC_DEST_MASK		0x000c0000
# define APIC_DEST_DESTFLD	0x00000000
# define APIC_DEST_SELF		0x00040000
# define APIC_DEST_ALLISELF	0x00080000
# define APIC_DEST_ALLESELF	0x000c0000

#define APIC_RESV2_MASK		0xfff00000


/* fields in ICR_HIGH */
#define APIC_ID_MASK		0x0f000000


/* fields in LVT1/2 */
#define APIC_LVT_VECTOR		0x000000ff
#define APIC_LVT_DM		0x00000700
# define APIC_LVT_DM_FIXED	0x00000000
# define APIC_LVT_DM_NMI	0x00000400
# define APIC_LVT_DM_EXTINT	0x00000700
#define APIC_LVT_DS		0x00001000
#define APIC_LVT_IIPP		0x00002000
#define APIC_LVT_IIPP_INTALO	0x00002000
#define APIC_LVT_IIPP_INTAHI	0x00000000
#define APIC_LVT_RIRR		0x00004000
#define APIC_LVT_TM		0x00008000
#define APIC_LVT_M		0x00010000


/* fields in LVT Timer */
#define APIC_LVTT_VECTOR	0x000000ff
#define APIC_LVTT_DS		0x00001000
#define APIC_LVTT_M		0x00010000
#define APIC_LVTT_TM		0x00020000


/* fields in TDCR */
#define APIC_TDCR_2		0x00
#define APIC_TDCR_4		0x01
#define APIC_TDCR_8		0x02
#define APIC_TDCR_16		0x03
#define APIC_TDCR_32		0x08
#define APIC_TDCR_64		0x09
#define APIC_TDCR_128		0x0a
#define APIC_TDCR_1		0x0b


/*
 * fields in IRR
 * ISA INTerrupts are in bits 16-31 of the 1st IRR register.
 * these masks DON'T EQUAL the isa IRQs of the same name.
 * FIXME: how do we make this portable for MP table configurations???
 *        look for "HARD_VECTORXXX" marking places with this problem.
 */
#define APIC_IRQ0		0x00000001
#define APIC_IRQ1		0x00000002
#define APIC_IRQ2		0x00000004
#define APIC_IRQ3		0x00000008
#define APIC_IRQ4		0x00000010
#define APIC_IRQ5		0x00000020
#define APIC_IRQ6		0x00000040
#define APIC_IRQ7		0x00000080
#define APIC_IRQ8		0x00000100
#define APIC_IRQ9		0x00000200
#define APIC_IRQ10		0x00000400
#define APIC_IRQ11		0x00000800
#define APIC_IRQ12		0x00001000
#define APIC_IRQ13		0x00002000
#define APIC_IRQ14		0x00004000
#define APIC_IRQ15		0x00008000
#define APIC_IRQ16		0x00010000
#define APIC_IRQ17		0x00020000
#define APIC_IRQ18		0x00040000
#define APIC_IRQ19		0x00080000
#define APIC_IRQ20		0x00100000
#define APIC_IRQ21		0x00200000
#define APIC_IRQ22		0x00400000
#define APIC_IRQ23		0x00800000


/******************************************************************************
 * I/O APIC defines
 */

/* default physical locations of an IO APIC */
#define DEFAULT_IO_APIC_BASE	0xfec00000

/* window register offset */
#define IOAPIC_WINDOW		0x10

/* indexes into IO APIC */
#define IOAPIC_ID		0x00
#define IOAPIC_VER		0x01
#define IOAPIC_ARB		0x02
#define IOAPIC_REDTBL		0x10
#define IOAPIC_REDTBL0		IOAPIC_REDTBL
#define IOAPIC_REDTBL1		(IOAPIC_REDTBL+0x02)
#define IOAPIC_REDTBL2		(IOAPIC_REDTBL+0x04)
#define IOAPIC_REDTBL3		(IOAPIC_REDTBL+0x06)
#define IOAPIC_REDTBL4		(IOAPIC_REDTBL+0x08)
#define IOAPIC_REDTBL5		(IOAPIC_REDTBL+0x0a)
#define IOAPIC_REDTBL6		(IOAPIC_REDTBL+0x0c)
#define IOAPIC_REDTBL7		(IOAPIC_REDTBL+0x0e)
#define IOAPIC_REDTBL8		(IOAPIC_REDTBL+0x10)
#define IOAPIC_REDTBL9		(IOAPIC_REDTBL+0x12)
#define IOAPIC_REDTBL10		(IOAPIC_REDTBL+0x14)
#define IOAPIC_REDTBL11		(IOAPIC_REDTBL+0x16)
#define IOAPIC_REDTBL12		(IOAPIC_REDTBL+0x18)
#define IOAPIC_REDTBL13		(IOAPIC_REDTBL+0x1a)
#define IOAPIC_REDTBL14		(IOAPIC_REDTBL+0x1c)
#define IOAPIC_REDTBL15		(IOAPIC_REDTBL+0x1e)
#define IOAPIC_REDTBL16		(IOAPIC_REDTBL+0x20)
#define IOAPIC_REDTBL17		(IOAPIC_REDTBL+0x22)
#define IOAPIC_REDTBL18		(IOAPIC_REDTBL+0x24)
#define IOAPIC_REDTBL19		(IOAPIC_REDTBL+0x26)
#define IOAPIC_REDTBL20		(IOAPIC_REDTBL+0x28)
#define IOAPIC_REDTBL21		(IOAPIC_REDTBL+0x2a)
#define IOAPIC_REDTBL22		(IOAPIC_REDTBL+0x2c)
#define IOAPIC_REDTBL23		(IOAPIC_REDTBL+0x2e)

/* fields in VER */
#define IOART_VER_VERSION	0x000000ff
#define IOART_VER_MAXREDIR	0x00ff0000
#define MAXREDIRSHIFT		16

/*
 * fields in the IO APIC's redirection table entries
 */
#define IOART_DEST	APIC_ID_MASK	/* broadcast addr: all APICs */

#define IOART_RESV	0x00fe0000	/* reserved */

#define IOART_INTMASK	0x00010000	/* R/W: INTerrupt mask */
# define IOART_INTMCLR	0x00000000	/*       clear, allow INTs */
# define IOART_INTMSET	0x00010000	/*       set, inhibit INTs */

#define IOART_TRGRMOD	0x00008000	/* R/W: trigger mode */
# define IOART_TRGREDG	0x00000000	/*       edge */
# define IOART_TRGRLVL	0x00008000	/*       level */

#define IOART_REM_IRR	0x00004000	/* RO: remote IRR */

#define IOART_INTPOL	0x00002000	/* R/W: INT input pin polarity */
# define IOART_INTAHI	0x00000000	/*      active high */
# define IOART_INTALO	0x00002000	/*      active low */

#define IOART_DELIVS	0x00001000	/* RO: delivery status */

#define IOART_DESTMOD	0x00000800	/* R/W: destination mode */
# define IOART_DESTPHY	0x00000000	/*      physical */
# define IOART_DESTLOG	0x00000800	/*      logical */

#define IOART_DELMOD	0x00000700	/* R/W: delivery mode */
# define IOART_DELFIXED	0x00000000	/*       fixed */
# define IOART_DELLOPRI	0x00000100	/*       lowest priority */
# define IOART_DELSMI	0x00000200	/*       System Management INT */
# define IOART_DELRSV1	0x00000300	/*       reserved */
# define IOART_DELNMI	0x00000400	/*       NMI signal */
# define IOART_DELINIT	0x00000500	/*       INIT signal */
# define IOART_DELRSV2	0x00000600	/*       reserved */
# define IOART_DELEXINT	0x00000700	/*       External INTerrupt */

#define IOART_INTVEC	0x000000ff	/* R/W: INTerrupt vector field */

#endif /* _MACHINE_APIC_H_ */
