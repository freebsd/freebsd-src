/* m68hc11.h -- Header file for Motorola 68HC11 & 68HC12 opcode table
   Copyright 1999, 2000, 2002, 2003 Free Software Foundation, Inc.
   Written by Stephane Carrez (stcarrez@nerim.fr)

This file is part of GDB, GAS, and the GNU binutils.

GDB, GAS, and the GNU binutils are free software; you can redistribute
them and/or modify them under the terms of the GNU General Public
License as published by the Free Software Foundation; either version
1, or (at your option) any later version.

GDB, GAS, and the GNU binutils are distributed in the hope that they
will be useful, but WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this file; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef _OPCODE_M68HC11_H
#define _OPCODE_M68HC11_H

/* Flags for the definition of the 68HC11 & 68HC12 CCR.  */
#define M6811_S_BIT     0x80	/* Stop disable */
#define M6811_X_BIT     0x40	/* X-interrupt mask */
#define M6811_H_BIT     0x20	/* Half carry flag */
#define M6811_I_BIT     0x10	/* I-interrupt mask */
#define M6811_N_BIT     0x08	/* Negative */
#define M6811_Z_BIT     0x04	/* Zero */
#define M6811_V_BIT     0x02	/* Overflow */
#define M6811_C_BIT     0x01	/* Carry */

/* 68HC11 register address offsets (range 0..0x3F or 0..64).
   The absolute address of the I/O register depends on the setting
   of the M6811_INIT register.  At init time, the I/O registers are
   mapped at 0x1000.  Address of registers is then:

   0x1000 + M6811_xxx
*/
#define M6811_PORTA     0x00	/* Port A register */
#define M6811__RES1	0x01	/* Unused/Reserved */
#define M6811_PIOC	0x02	/* Parallel I/O Control register */
#define M6811_PORTC	0x03	/* Port C register */
#define M6811_PORTB	0x04	/* Port B register */
#define M6811_PORTCL	0x05	/* Alternate latched port C */
#define M6811__RES6	0x06	/* Unused/Reserved */
#define M6811_DDRC	0x07	/* Data direction register for port C */
#define M6811_PORTD	0x08	/* Port D register */
#define M6811_DDRD	0x09	/* Data direction register for port D */
#define M6811_PORTE	0x0A	/* Port E input register */
#define M6811_CFORC	0x0B	/* Compare Force Register */
#define M6811_OC1M	0x0C	/* OC1 Action Mask register */
#define M6811_OC1D	0x0D	/* OC1 Action Data register */
#define M6811_TCTN	0x0E	/* Timer Counter Register */
#define M6811_TCTN_H	0x0E	/* "	 "	 " High part */
#define M6811_TCTN_L	0x0F	/* "	 "	 " Low part */
#define M6811_TIC1	0x10	/* Input capture 1 register */
#define M6811_TIC1_H	0x10	/* "	 "	 " High part */
#define M6811_TIC1_L	0x11	/* "	 "	 " Low part */
#define M6811_TIC2	0x12	/* Input capture 2 register */
#define M6811_TIC2_H	0x12	/* "	 "	 " High part */
#define M6811_TIC2_L	0x13	/* "	 "	 " Low part */
#define M6811_TIC3	0x14	/* Input capture 3 register */
#define M6811_TIC3_H	0x14	/* "	 "	 " High part */
#define M6811_TIC3_L	0x15	/* "	 "	 " Low part */
#define M6811_TOC1	0x16	/* Output Compare 1 register */
#define M6811_TOC1_H	0x16	/* "	 "	 " High part */
#define M6811_TOC1_L	0x17	/* "	 "	 " Low part */
#define M6811_TOC2	0x18	/* Output Compare 2 register */
#define M6811_TOC2_H	0x18	/* "	 "	 " High part */
#define M6811_TOC2_L	0x19	/* "	 "	 " Low part */
#define M6811_TOC3	0x1A	/* Output Compare 3 register */
#define M6811_TOC3_H	0x1A	/* "	 "	 " High part */
#define M6811_TOC3_L	0x1B	/* "	 "	 " Low part */
#define M6811_TOC4	0x1C	/* Output Compare 4 register */
#define M6811_TOC4_H	0x1C	/* "	 "	 " High part */
#define M6811_TOC4_L	0x1D	/* "	 "	 " Low part */
#define M6811_TOC5	0x1E	/* Output Compare 5 register */
#define M6811_TOC5_H	0x1E	/* "	 "	 " High part */
#define M6811_TOC5_L	0x1F	/* "	 "	 " Low part */
#define M6811_TCTL1	0x20	/* Timer Control register 1 */
#define M6811_TCTL2	0x21	/* Timer Control register 2 */
#define M6811_TMSK1	0x22	/* Timer Interrupt Mask Register 1 */
#define M6811_TFLG1	0x23	/* Timer Interrupt Flag Register 1 */
#define M6811_TMSK2	0x24	/* Timer Interrupt Mask Register 2 */
#define M6811_TFLG2	0x25	/* Timer Interrupt Flag Register 2 */
#define M6811_PACTL	0x26	/* Pulse Accumulator Control Register */
#define M6811_PACNT	0x27	/* Pulse Accumulator Count Register */
#define M6811_SPCR	0x28	/* SPI Control register */
#define M6811_SPSR	0x29	/* SPI Status register */
#define M6811_SPDR	0x2A	/* SPI Data register */
#define M6811_BAUD	0x2B	/* SCI Baud register */
#define M6811_SCCR1	0x2C	/* SCI Control register 1 */
#define M6811_SCCR2	0x2D	/* SCI Control register 2 */
#define M6811_SCSR	0x2E	/* SCI Status register */
#define M6811_SCDR	0x2F	/* SCI Data (Read => RDR, Write => TDR) */
#define M6811_ADCTL	0x30	/* A/D Control register */
#define M6811_ADR1	0x31	/* A/D, Analog Result register 1 */
#define M6811_ADR2	0x32	/* A/D, Analog Result register 2 */
#define M6811_ADR3	0x33	/* A/D, Analog Result register 3 */
#define M6811_ADR4	0x34	/* A/D, Analog Result register 4 */
#define M6811__RES35	0x35
#define M6811__RES36	0x36
#define M6811__RES37	0x37
#define M6811__RES38	0x38
#define M6811_OPTION	0x39	/* System Configuration Options */
#define M6811_COPRST	0x3A	/* Arm/Reset COP Timer Circuitry */
#define M6811_PPROG	0x3B	/* EEPROM Programming Control Register */
#define M6811_HPRIO	0x3C	/* Highest priority I-Bit int and misc */
#define M6811_INIT	0x3D	/* Ram and I/O mapping register */
#define M6811_TEST1	0x3E	/* Factory test control register */
#define M6811_CONFIG	0x3F	/* COP, ROM and EEPROM enables */


/* Flags of the CONFIG register (in EEPROM).  */
#define M6811_NOSEC     0x08    /* Security mode disable */
#define M6811_NOCOP     0x04    /* COP system disable */
#define M6811_ROMON     0x02    /* Enable on-chip rom */
#define M6811_EEON      0x01    /* Enable on-chip eeprom */

/* Flags of the PPROG register.  */
#define M6811_BYTE	0x10	/* Byte mode */
#define M6811_ROW       0x08	/* Row mode */
#define M6811_ERASE	0x04	/* Erase mode select (1 = erase, 0 = read) */
#define M6811_EELAT     0x02	/* EEPROM Latch Control */
#define M6811_EEPGM     0x01	/* EEPROM Programming Voltage Enable */

/* Flags of the PIOC register.  */
#define M6811_STAF	0x80	/* Strobe A Interrupt Status Flag */
#define M6811_STAI	0x40	/* Strobe A Interrupt Enable Mask */
#define M6811_CWOM	0x20	/* Port C Wire OR mode */
#define M6811_HNDS	0x10	/* Handshake mode */
#define M6811_OIN	0x08	/* Output or Input handshaking */
#define M6811_PLS	0x04	/* Pulse/Interlocked Handshake Operation */
#define M6811_EGA	0x02	/* Active Edge for Strobe A */
#define M6811_INVB	0x01	/* Invert Strobe B */

/* Flags of the SCCR1 register.  */
#define M6811_R8	0x80	/* Receive Data bit 8 */
#define M6811_T8	0x40	/* Transmit data bit 8 */
#define M6811__SCCR1_5  0x20	/* Unused */
#define M6811_M		0x10	/* SCI Character length */
#define M6811_WAKE	0x08	/* Wake up method select (0=idle, 1=addr mark) */

/* Flags of the SCCR2 register.  */
#define M6811_TIE	0x80	/* Transmit Interrupt enable */
#define M6811_TCIE	0x40	/* Transmit Complete Interrupt Enable */
#define M6811_RIE	0x20	/* Receive Interrupt Enable */
#define M6811_ILIE	0x10	/* Idle Line Interrupt Enable */
#define M6811_TE	0x08	/* Transmit Enable */
#define M6811_RE	0x04	/* Receive Enable */
#define M6811_RWU	0x02	/* Receiver Wake Up */
#define M6811_SBK	0x01	/* Send Break */

/* Flags of the SCSR register.  */
#define M6811_TDRE	0x80	/* Transmit Data Register Empty */
#define M6811_TC	0x40	/* Transmit Complete */
#define M6811_RDRF	0x20	/* Receive Data Register Full */
#define M6811_IDLE	0x10	/* Idle Line Detect */
#define M6811_OR	0x08	/* Overrun Error */
#define M6811_NF	0x04	/* Noise Flag */
#define M6811_FE	0x02	/* Framing Error */
#define M6811__SCSR_0	0x01	/* Unused */

/* Flags of the BAUD register.  */
#define M6811_TCLR	0x80	/* Clear Baud Rate (TEST mode) */
#define M6811__BAUD_6	0x40	/* Not used */
#define M6811_SCP1	0x20	/* SCI Baud rate prescaler select */
#define M6811_SCP0	0x10
#define M6811_RCKB	0x08	/* Baud Rate Clock Check (TEST mode) */
#define M6811_SCR2	0x04	/* SCI Baud rate select */
#define M6811_SCR1	0x02
#define M6811_SCR0	0x01

#define M6811_BAUD_DIV_1	(0)
#define M6811_BAUD_DIV_3	(M6811_SCP0)
#define M6811_BAUD_DIV_4	(M6811_SCP1)
#define M6811_BAUD_DIV_13	(M6811_SCP1|M6811_SCP0)

/* Flags of the SPCR register.  */
#define M6811_SPIE	0x80	/* Serial Peripheral Interrupt Enable */
#define M6811_SPE	0x40	/* Serial Peripheral System Enable */
#define M6811_DWOM	0x20	/* Port D Wire-OR mode option */
#define M6811_MSTR	0x10	/* Master Mode Select */
#define M6811_CPOL	0x08	/* Clock Polarity */
#define M6811_CPHA	0x04	/* Clock Phase */
#define M6811_SPR1	0x02	/* SPI Clock Rate Select */
#define M6811_SPR0	0x01

/* Flags of the SPSR register.  */
#define M6811_SPIF	0x80	/* SPI Transfer Complete flag */
#define M6811_WCOL	0x40	/* Write Collision */
#define M6811_MODF	0x10	/* Mode Fault */

/* Flags of the ADCTL register.  */
#define M6811_CCF	0x80	/* Conversions Complete Flag */
#define M6811_SCAN	0x20	/* Continuous Scan Control */
#define M6811_MULT	0x10	/* Multiple Channel/Single Channel Control */
#define M6811_CD	0x08	/* Channel Select D */
#define M6811_CC	0x04	/*                C */
#define M6811_CB	0x02	/*                B */
#define M6811_CA	0x01	/*                A */

/* Flags of the CFORC register.  */
#define M6811_FOC1	0x80	/* Force Output Compare 1 */
#define M6811_FOC2	0x40	/*			2 */
#define M6811_FOC3	0x20	/*			3 */
#define M6811_FOC4	0x10	/*			4 */
#define M6811_FOC5	0x08	/*			5 */

/* Flags of the OC1M register.  */
#define M6811_OC1M7	0x80	/* Output Compare 7 */
#define M6811_OC1M6	0x40	/*                6 */
#define M6811_OC1M5	0x20	/*                5 */
#define M6811_OC1M4	0x10	/*                4 */
#define M6811_OC1M3	0x08	/*                3 */

/* Flags of the OC1D register.  */
#define M6811_OC1D7	0x80
#define M6811_OC1D6	0x40
#define M6811_OC1D5	0x20
#define M6811_OC1D4	0x10
#define M6811_OC1D3	0x08

/* Flags of the TCTL1 register.  */
#define M6811_OM2	0x80	/* Output Mode 2 */
#define M6811_OL2	0x40	/* Output Level 2 */
#define M6811_OM3	0x20
#define M6811_OL3	0x10
#define M6811_OM4	0x08
#define M6811_OL4	0x04
#define M6811_OM5	0x02
#define M6811_OL5	0x01

/* Flags of the TCTL2 register.  */
#define M6811_EDG1B	0x20	/* Input Edge Capture Control 1 */
#define M6811_EDG1A	0x10
#define M6811_EDG2B	0x08	/* Input 2 */
#define M6811_EDG2A	0x04
#define M6811_EDG3B	0x02	/* Input 3 */
#define M6811_EDG3A	0x01

/* Flags of the TMSK1 register.  */
#define M6811_OC1I	0x80	/* Output Compare 1 Interrupt */
#define M6811_OC2I	0x40	/*		  2	      */
#define M6811_OC3I	0x20	/*		  3	      */
#define M6811_OC4I	0x10	/*		  4	      */
#define M6811_OC5I	0x08	/*		  5	      */
#define M6811_IC1I	0x04	/* Input Capture  1 Interrupt */
#define M6811_IC2I	0x02	/*		  2	      */
#define M6811_IC3I	0x01	/*		  3	      */

/* Flags of the TFLG1 register.  */
#define M6811_OC1F	0x80	/* Output Compare 1 Flag */
#define M6811_OC2F	0x40	/*		  2	 */
#define M6811_OC3F	0x20	/*		  3	 */
#define M6811_OC4F	0x10	/*		  4	 */
#define M6811_OC5F	0x08	/*		  5	 */
#define M6811_IC1F	0x04	/* Input Capture  1 Flag */
#define M6811_IC2F	0x02	/*		  2	 */
#define M6811_IC3F	0x01	/*		  3	 */

/* Flags of Timer Interrupt Mask Register 2 (TMSK2).  */
#define M6811_TOI       0x80    /* Timer Overflow Interrupt Enable */
#define M6811_RTII      0x40    /* RTI Interrupt Enable */
#define M6811_PAOVI     0x20    /* Pulse Accumulator Overflow Interrupt En. */
#define M6811_PAII      0x10    /* Pulse Accumulator Interrupt Enable */
#define M6811_PR1       0x02    /* Timer prescaler */
#define M6811_PR0       0x01    /* Timer prescaler */
#define M6811_TPR_1     0x00    /* " " prescale div 1 */
#define M6811_TPR_4     0x01    /* " " prescale div 4 */
#define M6811_TPR_8     0x02    /* " " prescale div 8 */
#define M6811_TPR_16    0x03    /* " " prescale div 16 */

/* Flags of Timer Interrupt Flag Register 2 (M6811_TFLG2).  */
#define M6811_TOF       0x80    /* Timer overflow bit */
#define M6811_RTIF      0x40    /* Read time interrupt flag */
#define M6811_PAOVF     0x20    /* Pulse accumulator overflow Interrupt flag */
#define M6811_PAIF      0x10    /* Pulse accumulator Input Edge " " " */

/* Flags of Pulse Accumulator Control Register (PACTL).  */
#define M6811_DDRA7     0x80    /* Data direction for port A bit 7 */
#define M6811_PAEN      0x40    /* Pulse accumulator system enable */
#define M6811_PAMOD     0x20    /* Pulse accumulator mode */
#define M6811_PEDGE     0x10    /* Pulse accumulator edge control */
#define M6811_RTR1      0x02    /* RTI Interrupt rates select */
#define M6811_RTR0      0x01    /* " " " " */

/* Flags of the Options register.  */
#define M6811_ADPU      0x80    /* A/D Powerup */
#define M6811_CSEL      0x40    /* A/D/EE Charge pump clock source select */
#define M6811_IRQE      0x20    /* IRQ Edge/Level sensitive */
#define M6811_DLY       0x10    /* Stop exit turn on delay */
#define M6811_CME       0x08    /* Clock Monitor enable */
#define M6811_CR1       0x02    /* COP timer rate select */
#define M6811_CR0       0x01    /* COP timer rate select */

/* Flags of the HPRIO register.  */
#define M6811_RBOOT	0x80	/* Read Bootstrap ROM */
#define M6811_SMOD	0x40	/* Special Mode */
#define M6811_MDA	0x20	/* Mode Select A */
#define M6811_IRV	0x10	/* Internal Read Visibility */
#define M6811_PSEL3	0x08	/* Priority Select */
#define M6811_PSEL2	0x04
#define M6811_PSEL1	0x02
#define M6811_PSEL0	0x01

/* Some insns used by gas to turn relative branches into absolute ones.  */
#define M6811_BRA	0x20
#define M6811_JMP	0x7e
#define M6811_BSR	0x8d
#define M6811_JSR	0xbd
#define M6812_JMP       0x06
#define M6812_BSR       0x07
#define M6812_JSR       0x16

/* Instruction code pages. Code page 1 is the default.  */
/*#define	M6811_OPCODE_PAGE1	0x00*/
#define	M6811_OPCODE_PAGE2	0x18
#define	M6811_OPCODE_PAGE3	0x1A
#define	M6811_OPCODE_PAGE4	0xCD


/* 68HC11 operands formats as stored in the m6811_opcode table.  These
   flags do not correspond to anything in the 68HC11 or 68HC12.
   They are only used by GAS to recognize operands.  */
#define M6811_OP_NONE         0        /* No operand */
#define M6811_OP_DIRECT       0x0001   /* Page 0 addressing:   *<val-8bits>  */
#define M6811_OP_IMM8         0x0002   /*  8 bits immediat:    #<val-8bits>  */
#define M6811_OP_IMM16        0x0004   /* 16 bits immediat:    #<val-16bits> */
#define M6811_OP_IND16        0x0008   /* Indirect abs:        <val-16>      */
#define M6812_OP_IND16_P2     0x0010   /* Second parameter indirect abs.     */
#define M6812_OP_REG          0x0020   /* Register operand 1                 */
#define M6812_OP_REG_2        0x0040   /* Register operand 2                 */

#define M6811_OP_IX           0x0080   /* Indirect IX:         <val-8>,x     */
#define M6811_OP_IY           0x0100   /* Indirect IY:         <val-8>,y     */
#define M6812_OP_IDX          0x0200   /* Indirect: N,r N,[+-]r[+-] N:5-bits */
#define M6812_OP_IDX_1        0x0400   /* N,r N:9-bits  */
#define M6812_OP_IDX_2        0x0800   /* N,r N:16-bits */
#define M6812_OP_D_IDX        0x1000   /* Indirect indexed: [D,r] */
#define M6812_OP_D_IDX_2      0x2000   /* [N,r] N:16-bits */
#define M6812_OP_PAGE         0x4000   /* Page number */
#define M6811_OP_MASK         0x07FFF
#define M6811_OP_BRANCH       0x00008000 /* Branch, jsr, call */
#define M6811_OP_BITMASK      0x00010000 /* Bitmask:             #<val-8>    */
#define M6811_OP_JUMP_REL     0x00020000 /* Pc-Relative:         <val-8>     */
#define M6812_OP_JUMP_REL16   0x00040000 /* Pc-relative:         <val-16>    */
#define M6811_OP_PAGE1        0x0000
#define M6811_OP_PAGE2        0x00080000 /* Need a page2 opcode before       */
#define M6811_OP_PAGE3        0x00100000 /* Need a page3 opcode before       */
#define M6811_OP_PAGE4        0x00200000 /* Need a page4 opcode before       */
#define M6811_MAX_OPERANDS    3     /* Max operands: brset <dst> <mask> <b> */

#define M6812_ACC_OFFSET      0x00400000 /* A,r B,r D,r			    */
#define M6812_ACC_IND         0x00800000 /* [D,r]			    */
#define M6812_PRE_INC         0x01000000 /* n,+r   n = -8..8		    */
#define M6812_PRE_DEC         0x02000000 /* n,-r			    */
#define M6812_POST_INC        0x04000000 /* n,r+			    */
#define M6812_POST_DEC        0x08000000 /* n,r-			    */
#define M6812_INDEXED_IND     0x10000000 /* [n,r]  n = 16-bits		    */
#define M6812_INDEXED         0x20000000 /* n,r    n = 5, 9 or 16-bits	    */
#define M6812_OP_IDX_P2       0x40000000

/* Markers to identify some instructions.  */
#define M6812_OP_EXG_MARKER   0x01000000 /* exg r1,r2 */
#define M6812_OP_TFR_MARKER   0x02000000 /* tfr r1,r2 */
#define M6812_OP_SEX_MARKER   0x04000000 /* sex r1,r2 */

#define M6812_OP_EQ_MARKER    0x80000000 /* dbeq/ibeq/tbeq */
#define M6812_OP_DBCC_MARKER  0x04000000 /* dbeq/dbne */
#define M6812_OP_IBCC_MARKER  0x02000000 /* ibeq/ibne */
#define M6812_OP_TBCC_MARKER  0x01000000

#define M6812_OP_TRAP_ID      0x80000000 /* trap #N */

#define M6811_OP_HIGH_ADDR    0x01000000 /* Used internally by gas.  */
#define M6811_OP_LOW_ADDR     0x02000000

#define M68HC12_BANK_VIRT 0x010000
#define M68HC12_BANK_MASK 0x00003fff
#define M68HC12_BANK_BASE 0x00008000
#define M68HC12_BANK_SHIFT 14
#define M68HC12_BANK_PAGE_MASK 0x0ff


/* CPU identification.  */
#define cpu6811 0x01
#define cpu6812 0x02
#define cpu6812s 0x04

/* The opcode table is an array of struct m68hc11_opcode.  */
struct m68hc11_opcode {
  const char*    name;     /* Op-code name */
  long           format;
  unsigned char  size;
  unsigned char  opcode;
  unsigned char  cycles_low;
  unsigned char  cycles_high;
  unsigned char  set_flags_mask;
  unsigned char  clr_flags_mask;
  unsigned char  chg_flags_mask;
  unsigned char  arch;
};

/* Alias definition for 68HC12.  */
struct m68hc12_opcode_alias 
{
  const char*   name;
  const char*   translation;
  unsigned char size;
  unsigned char code1;
  unsigned char code2;
};

/* The opcode table.  The table contains all the opcodes (all pages).
   You can't rely on the order.  */
extern const struct m68hc11_opcode m68hc11_opcodes[];
extern const int m68hc11_num_opcodes;

/* Alias table for 68HC12.  It translates some 68HC11 insn which are not
   implemented in 68HC12 but have equivalent translations.  */
extern const struct m68hc12_opcode_alias m68hc12_alias[];
extern const int m68hc12_num_alias;

#endif /* _OPCODE_M68HC11_H */
