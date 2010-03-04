/*
 * $FreeBSD$
 *
 * Copyright (c) 2002-2004 David Boggs. (boggs@boggs.palo-alto.ca.us)
 * All rights reserved.
 *
 * BSD License:
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
 * GNU General Public License:
 *
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU General Public License as published by the Free 
 * Software Foundation; either version 2 of the License, or (at your option) 
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT 
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 
 * Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef IF_LMC_H
#define IF_LMC_H

#define DEVICE_NAME		"lmc"

/* Linux RPM-style version information */
#define DRIVER_MAJOR_VERSION	2005	/* year */
#define DRIVER_MINOR_VERSION	9	/* month */
#define DRIVER_SUB_VERSION	29	/* day */

/* netgraph stuff */
#define NG_LMC_NODE_TYPE	DEVICE_NAME
#define NGM_LMC_COOKIE		1128054761	/* date -u +'%s' */

/* Tulip PCI configuration registers */
#define	TLP_CFID		0x00		/*  0: CFg ID register     */
#define	TLP_CFCS		0x04		/*  1: CFg Command/Status  */
#define	TLP_CFRV		0x08		/*  2: CFg ReVision        */
#define	TLP_CFLT		0x0C		/*  3: CFg Latency Timer   */
#define	TLP_CBIO		0x10		/*  4: Cfg Base IO address */
#define	TLP_CBMA		0x14		/*  5: Cfg Base Mem Addr   */
#define TLP_CSID		0x2C		/* 11: Cfg Subsys ID reg   */
#define	TLP_CFIT		0x3C		/* 15: CFg InTerrupt       */
#define	TLP_CFDD		0x40		/* 16: CFg Driver Data     */

#define TLP_CFID_TULIP		0x00091011	/* DEC 21140A Ethernet chip */

#define TLP_CFCS_MSTR_ABORT	0x20000000
#define TLP_CFCS_TARG_ABORT	0x10000000
#define TLP_CFCS_SYS_ERROR	0x00000100
#define TLP_CFCS_PAR_ERROR	0x00000040
#define TLP_CFCS_MWI_ENABLE	0x00000010
#define TLP_CFCS_BUS_MASTER	0x00000004
#define TLP_CFCS_MEM_ENABLE	0x00000002
#define TLP_CFCS_IO_ENABLE	0x00000001

#define TLP_CFLT_LATENCY	0x0000FF00
#define TLP_CFLT_CACHE		0x000000FF

#define TLP_CSID_HSSI		0x00031376	/* LMC 5200 HSSI card */
#define TLP_CSID_T3		0x00041376	/* LMC 5245 T3   card */
#define TLP_CSID_SSI		0x00051376	/* LMC 1000 SSI  card */
#define TLP_CSID_T1E1		0x00061376	/* LMC 1200 T1E1 card */
#define TLP_CSID_HSSIc		0x00071376	/* LMC 5200 HSSI cPCI */
#define TLP_CSID_SDSL		0x00081376	/* LMC 1168 SDSL card */

#define TLP_CFIT_MAX_LAT	0xFF000000

#define TLP_CFDD_SLEEP		0x80000000
#define TLP_CFDD_SNOOZE		0x40000000

/* Tulip Control and Status Registers */
#define TLP_CSR_STRIDE		 8		/* 64 bits */
#define TLP_BUS_MODE		 0 * TLP_CSR_STRIDE
#define TLP_TX_POLL		 1 * TLP_CSR_STRIDE
#define TLP_RX_POLL		 2 * TLP_CSR_STRIDE
#define TLP_RX_LIST		 3 * TLP_CSR_STRIDE
#define TLP_TX_LIST		 4 * TLP_CSR_STRIDE
#define TLP_STATUS		 5 * TLP_CSR_STRIDE
#define TLP_OP_MODE		 6 * TLP_CSR_STRIDE
#define TLP_INT_ENBL		 7 * TLP_CSR_STRIDE
#define TLP_MISSED		 8 * TLP_CSR_STRIDE
#define TLP_SROM_MII		 9 * TLP_CSR_STRIDE
#define TLP_BIOS_ROM		10 * TLP_CSR_STRIDE
#define TLP_TIMER		11 * TLP_CSR_STRIDE
#define TLP_GPIO		12 * TLP_CSR_STRIDE
#define TLP_CSR13		13 * TLP_CSR_STRIDE
#define TLP_CSR14		14 * TLP_CSR_STRIDE
#define TLP_WDOG		15 * TLP_CSR_STRIDE
#define TLP_CSR_SIZE		128		/* IO bus space size */

/* CSR 0 - PCI Bus Mode Register */
#define TLP_BUS_WRITE_INVAL	0x01000000 /* DONT USE! */
#define TLP_BUS_READ_LINE	0x00800000
#define TLP_BUS_READ_MULT	0x00200000
#define TLP_BUS_DESC_BIGEND	0x00100000
#define TLP_BUS_TAP		0x000E0000
#define TLP_BUS_CAL		0x0000C000
#define TLP_BUS_PBL		0x00003F00
#define TLP_BUS_DATA_BIGEND	0x00000080
#define TLP_BUS_DSL		0x0000007C
#define TLP_BUS_ARB		0x00000002
#define TLP_BUS_RESET		0x00000001
#define TLP_BUS_CAL_SHIFT	14
#define TLP_BUS_PBL_SHIFT	 8

/* CSR 5 - Status Register */
#define TLP_STAT_FATAL_BITS	0x03800000
#define TLP_STAT_TX_FSM		0x00700000
#define TLP_STAT_RX_FSM		0x000E0000
#define TLP_STAT_FATAL_ERROR	0x00002000
#define TLP_STAT_TX_UNDERRUN	0x00000020
#define TLP_STAT_FATAL_SHIFT	23

/* CSR 6 - Operating Mode Register */
#define TLP_OP_RECEIVE_ALL	0x40000000
#define TLP_OP_MUST_BE_ONE	0x02000000
#define TLP_OP_NO_HEART_BEAT	0x00080000
#define TLP_OP_PORT_SELECT	0x00040000
#define TLP_OP_TX_THRESH	0x0000C000
#define TLP_OP_TX_RUN		0x00002000
#define TLP_OP_LOOP_MODE	0x00000C00
#define TLP_OP_EXT_LOOP		0x00000800
#define TLP_OP_INT_LOOP		0x00000400
#define TLP_OP_FULL_DUPLEX	0x00000200
#define TLP_OP_PROMISCUOUS	0x00000040
#define TLP_OP_PASS_BAD_PKT	0x00000008
#define TLP_OP_RX_RUN		0x00000002
#define TLP_OP_TR_SHIFT		14
#define TLP_OP_INIT		(TLP_OP_PORT_SELECT   | \
				 TLP_OP_FULL_DUPLEX   | \
				 TLP_OP_MUST_BE_ONE   | \
				 TLP_OP_NO_HEART_BEAT | \
				 TLP_OP_RECEIVE_ALL   | \
				 TLP_OP_PROMISCUOUS   | \
				 TLP_OP_PASS_BAD_PKT  | \
				 TLP_OP_RX_RUN        | \
				 TLP_OP_TX_RUN)

/* CSR 7 - Interrupt Enable Register */
#define TLP_INT_NORMAL_INTR	0x00010000
#define TLP_INT_ABNRML_INTR	0x00008000
#define TLP_INT_FATAL_ERROR	0x00002000
#define TLP_INT_RX_NO_BUFS	0x00000080
#define TLP_INT_RX_INTR		0x00000040
#define TLP_INT_TX_UNDERRUN	0x00000020
#define TLP_INT_TX_INTR		0x00000001
#define TLP_INT_DISABLE		0
#define TLP_INT_TX		(TLP_INT_NORMAL_INTR | \
				 TLP_INT_ABNRML_INTR | \
				 TLP_INT_FATAL_ERROR | \
				 TLP_INT_TX_UNDERRUN | \
				 TLP_INT_TX_INTR)
#define TLP_INT_RX		(TLP_INT_NORMAL_INTR | \
				 TLP_INT_ABNRML_INTR | \
				 TLP_INT_FATAL_ERROR | \
				 TLP_INT_RX_NO_BUFS  | \
				 TLP_INT_RX_INTR)
#define TLP_INT_TXRX		(TLP_INT_TX | TLP_INT_RX)

/* CSR 8 - RX Missed Frames & Overrun Register */
#define TLP_MISS_OCO		0x10000000
#define TLP_MISS_OVERRUN	0x0FFE0000
#define TLP_MISS_MFO		0x00010000
#define TLP_MISS_MISSED		0x0000FFFF
#define TLP_OVERRUN_SHIFT	17

/* CSR 9 - SROM & MII & Boot ROM Register */
#define TLP_MII_MDIN		0x00080000
#define TLP_MII_MDOE		0x00040000
#define TLP_MII_MDOUT		0x00020000
#define TLP_MII_MDC		0x00010000

#define TLP_BIOS_RD		0x00004000
#define TLP_BIOS_WR		0x00002000
#define TLP_BIOS_SEL		0x00001000

#define TLP_SROM_RD		0x00004000
#define TLP_SROM_SEL		0x00000800
#define TLP_SROM_DOUT		0x00000008
#define TLP_SROM_DIN		0x00000004
#define TLP_SROM_CLK		0x00000002
#define TLP_SROM_CS		0x00000001

/* CSR 12 - General Purpose IO register */
#define TLP_GPIO_DIR		0x00000100

/* CSR 15 - Watchdog Timer Register */
#define TLP_WDOG_RX_OFF		0x00000010
#define TLP_WDOG_TX_OFF		0x00000001
#define TLP_WDOG_INIT		(TLP_WDOG_TX_OFF | \
				 TLP_WDOG_RX_OFF)

/* GPIO bits common to all cards */
#define GPIO_INIT		0x01	/*    from Xilinx                  */
#define GPIO_RESET		0x02	/* to      Xilinx                  */
/* bits 2 and 3 vary with board type -- see below */
#define GPIO_MODE		0x10	/* to      Xilinx                  */
#define GPIO_DP			0x20	/* to/from Xilinx                  */
#define GPIO_DATA		0x40	/* serial data                     */
#define GPIO_CLK		0x80	/* serial clock                    */

/* HSSI GPIO bits */
#define GPIO_HSSI_ST		0x04	/* send timing sense (deprecated)  */
#define GPIO_HSSI_TXCLK		0x08	/* clock source                    */

/* HSSIc GPIO bits */
#define GPIO_HSSI_SYNTH		0x04	/* Synth osc chip select           */
#define GPIO_HSSI_DCE		0x08	/* provide clock on TXCLOCK output */

/* T3   GPIO bits */
#define GPIO_T3_DAC		0x04	/* DAC chip select                 */
#define GPIO_T3_INTEN		0x08	/* Framer Interupt enable          */

/* SSI  GPIO bits */
#define GPIO_SSI_SYNTH		0x04	/* Synth osc chip select           */
#define GPIO_SSI_DCE		0x08	/* provide clock on TXCLOCK output */

/* T1E1 GPIO bits */
#define GPIO_T1_INTEN		0x08	/* Framer Interupt enable          */

/* MII register 16 bits common to all cards */
/* NB: LEDs  for HSSI & SSI are in DIFFERENT bits than for T1E1 & T3; oops */
/* NB: CRC32 for HSSI & SSI is  in DIFFERENT bit  than for T1E1 & T3; oops */
#define MII16_LED_ALL		0x0780	/* RW: LED bit mask                */
#define MII16_FIFO		0x0800	/* RW: 1=reset, 0=not reset        */

/* MII register 16 bits for HSSI */
#define MII16_HSSI_TA		0x0001	/* RW: host ready;  host->modem    */
#define MII16_HSSI_CA		0x0002	/* RO: modem ready; modem->host    */
#define MII16_HSSI_LA		0x0004	/* RW: loopback A;  host->modem    */
#define MII16_HSSI_LB		0x0008	/* RW: loopback B;  host->modem    */
#define MII16_HSSI_LC		0x0010	/* RO: loopback C;  modem->host    */
#define MII16_HSSI_TM		0x0020	/* RO: test mode;   modem->host    */
#define MII16_HSSI_CRC32	0x0040	/* RW: CRC length 16/32            */
#define MII16_HSSI_LED_LL	0x0080	/* RW: lower left  - green         */
#define MII16_HSSI_LED_LR	0x0100	/* RW: lower right - green         */
#define MII16_HSSI_LED_UL	0x0200	/* RW: upper left  - green         */
#define MII16_HSSI_LED_UR	0x0400	/* RW: upper right - red           */
#define MII16_HSSI_FIFO		0x0800	/* RW: reset fifos                 */
#define MII16_HSSI_FORCECA	0x1000	/* RW: [cPCI] force CA on          */
#define MII16_HSSI_CLKMUX	0x6000  /* RW: [cPCI] TX clock selection   */
#define MII16_HSSI_LOOP		0x8000	/* RW: [cPCI] LOOP TX into RX      */
#define MII16_HSSI_MODEM	0x003F	/* TA+CA+LA+LB+LC+TM               */

/* MII register 16 bits for DS3 */
#define MII16_DS3_ZERO		0x0001	/* RW: short/long cables           */
#define MII16_DS3_TRLBK		0x0002	/* RW: loop towards host           */
#define MII16_DS3_LNLBK		0x0004	/* RW: loop towards net            */
#define MII16_DS3_RAIS		0x0008	/* RO: LIU receive AIS      (depr) */
#define MII16_DS3_TAIS		0x0010	/* RW: LIU transmit AIS     (depr) */
#define MII16_DS3_BIST		0x0020	/* RO: LIU QRSS patt match  (depr) */
#define MII16_DS3_DLOS		0x0040	/* RO: LIU Digital LOS      (depr) */
#define MII16_DS3_LED_BLU	0x0080	/* RW: lower right - blue          */
#define MII16_DS3_LED_YEL	0x0100	/* RW: lower left  - yellow        */
#define MII16_DS3_LED_RED	0x0200	/* RW: upper right - red           */
#define MII16_DS3_LED_GRN	0x0400	/* RW: upper left  - green         */
#define MII16_DS3_FIFO		0x0800	/* RW: reset fifos                 */
#define MII16_DS3_CRC32		0x1000	/* RW: CRC length 16/32            */
#define MII16_DS3_SCRAM		0x2000	/* RW: payload scrambler           */
#define MII16_DS3_POLY		0x4000	/* RW: 1=Larse, 0=DigLink|Kentrox  */
#define MII16_DS3_FRAME		0x8000	/* RW: 1=stop txframe pulses       */

/* MII register 16 bits for SSI */
#define MII16_SSI_DTR		0x0001	/* RW: DTR host->modem             */
#define MII16_SSI_DSR		0x0002	/* RO: DSR modem->host             */
#define MII16_SSI_RTS		0x0004	/* RW: RTS host->modem             */
#define MII16_SSI_CTS		0x0008	/* RO: CTS modem->host             */
#define MII16_SSI_DCD		0x0010	/* RW: DCD modem<->host            */
#define MII16_SSI_RI		0x0020	/* RO: RI  modem->host             */
#define MII16_SSI_CRC32		0x0040	/* RW: CRC length 16/32            */
#define MII16_SSI_LED_LL	0x0080	/* RW: lower left  - green         */
#define MII16_SSI_LED_LR	0x0100	/* RW: lower right - green         */
#define MII16_SSI_LED_UL	0x0200	/* RW: upper left  - green         */
#define MII16_SSI_LED_UR	0x0400	/* RW: upper right - red           */
#define MII16_SSI_FIFO		0x0800	/* RW: reset fifos                 */
#define MII16_SSI_LL		0x1000	/* RW: LL: host->modem             */
#define MII16_SSI_RL		0x2000	/* RW: RL: host->modem             */
#define MII16_SSI_TM		0x4000	/* RO: TM: modem->host             */
#define MII16_SSI_LOOP		0x8000	/* RW: Loop at ext conn            */
#define MII16_SSI_MODEM		0x703F	/* DTR+DSR+RTS+CTS+DCD+RI+LL+RL+TM */

/* Mii register 17 has the SSI cable bits */
#define MII17_SSI_CABLE_SHIFT	3	/* shift to get cable type         */
#define MII17_SSI_CABLE_MASK	0x0038	/* RO: mask  to get cable type     */
#define MII17_SSI_PRESCALE	0x0040	/* RW: divide by: 0=16; 1=512      */
#define MII17_SSI_ITF		0x0100	/* RW: fill with: 0=flags; 1=ones  */
#define MII17_SSI_NRZI		0x0400	/* RW: coding: NRZ= 0; NRZI=1      */

/* MII register 16 bits for T1/E1 */
#define MII16_T1_UNUSED1	0x0001
#define MII16_T1_INVERT		0x0002	/* RW: invert data (for SF/AMI)    */
#define MII16_T1_XOE		0x0004	/* RW: TX Output Enable; 0=disable */
#define MII16_T1_RST		0x0008	/* RW: Bt8370 chip reset           */
#define MII16_T1_Z		0x0010	/* RW: output impedance T1=1 E1=0  */
#define MII16_T1_INTR		0x0020	/* RO: interrupt from Bt8370       */
#define MII16_T1_ONESEC		0x0040	/* RO: one second square wave      */
#define MII16_T1_LED_BLU	0x0080	/* RW: lower right - blue          */
#define MII16_T1_LED_YEL	0x0100	/* RW: lower left  - yellow        */
#define MII16_T1_LED_RED	0x0200	/* RW: upper right - red           */
#define MII16_T1_LED_GRN	0x0400	/* RW: upper left  - green         */
#define MII16_T1_FIFO		0x0800	/* RW: reset fifos                 */
#define MII16_T1_CRC32		0x1000	/* RW: CRC length 16/32            */
#define MII16_T1_UNUSED2	0xE000

/* T3 framer:  RW=Read/Write;  RO=Read-Only;  RC=Read/Clr;  WO=Write-Only  */
#define T3CSR_STAT0		0x00	/* RO: real-time status            */
#define T3CSR_CTL1		0x01	/* RW: global control bits         */
#define T3CSR_FEBE		0x02	/* RC: Far End Block Error Counter */
#define T3CSR_CERR		0x03	/* RC: C-bit Parity Error Counter  */
#define T3CSR_PERR		0x04	/* RC: P-bit Parity Error Counter  */
#define T3CSR_TX_FEAC		0x05	/* RW: Far End Alarm & Control     */
#define T3CSR_RX_FEAC		0x06	/* RO: Far End Alarm & Control     */
#define T3CSR_STAT7		0x07	/* RL: latched real-time status    */
#define T3CSR_CTL8		0x08	/* RW: extended global ctl bits    */
#define T3CSR_STAT9		0x09	/* RL: extended status bits        */
#define T3CSR_FERR		0x0A	/* RC: F-bit Error Counter         */
#define T3CSR_MERR		0x0B	/* RC: M-bit Error Counter         */
#define T3CSR_CTL12		0x0C	/* RW: more extended ctl bits      */
#define T3CSR_DBL_FEAC		0x0D	/* RW: TX double FEAC              */
#define T3CSR_CTL14		0x0E	/* RW: even more extended ctl bits */
#define T3CSR_FEAC_STK		0x0F	/* RO: RX FEAC stack               */
#define T3CSR_STAT16		0x10	/* RL: extended latched status     */
#define T3CSR_INTEN		0x11	/* RW: interrupt enable            */
#define T3CSR_CVLO		0x12	/* RC: coding violation cntr LSB   */
#define T3CSR_CVHI		0x13	/* RC: coding violation cntr MSB   */
#define T3CSR_CTL20		0x14	/* RW: yet more extended ctl bits  */

#define CTL1_XTX		0x01	/* Transmit X-bit value            */
#define CTL1_3LOOP		0x02	/* framer loop back                */
#define CTL1_SER		0x04	/* SERial interface selected       */
#define CTL1_M13MODE		0x08	/* M13 frame format                */
#define CTL1_TXIDL		0x10	/* Transmit Idle signal            */
#define CTL1_ENAIS		0x20	/* Enable AIS upon LOS             */
#define CTL1_TXAIS		0x40	/* Transmit Alarm Indication Sig   */
#define CTL1_NOFEBE		0x80	/* No Far End Block Errors         */

#define CTL5_EMODE		0x80	/* rev B Extended features enabled */
#define CTL5_START		0x40	/* transmit the FEAC msg now       */

#define CTL8_FBEC		0x80	/* F-Bit Error Count control       */
#define CTL8_TBLU		0x20	/* Transmit Blue signal            */

#define STAT9_SEF		0x80	/* Severely Errored Frame          */
#define STAT9_RBLU		0x20	/* Receive Blue signal             */

#define CTL12_RTPLLEN		0x80	/* Rx-to-Tx Payload Lpbk Lock ENbl */
#define CTL12_RTPLOOP		0x40	/* Rx-to-Tx Payload Loopback       */
#define CTL12_DLCB1		0x08	/* Data Link C-Bits forced to 1    */
#define CTL12_C21		0x04	/* C2 forced to 1                  */
#define CTL12_MCB1		0x02	/* Most C-Bits forced to 1         */

#define CTL13_DFEXEC		0x40	/* Execute Double FEAC             */

#define CTL14_FEAC10		0x80	/* Transmit FEAC word 10 times     */
#define CTL14_RGCEN		0x20	/* Receive Gapped Clock Out Enbl   */
#define CTL14_TGCEN		0x10	/* Timing Gen Gapped Clk Out Enbl  */

#define FEAC_STK_MORE		0x80	/* FEAC stack has more FEACs       */
#define FEAC_STK_VALID		0x40	/* FEAC stack is valid             */
#define FEAC_STK_FEAC		0x3F	/* FEAC stack FEAC data            */

#define STAT16_XERR		0x01	/* X-bit Error                     */
#define STAT16_SEF		0x02    /* Severely Errored Frame          */
#define STAT16_RTLOC		0x04	/* Rx/Tx Loss Of Clock             */
#define STAT16_FEAC		0x08	/* new FEAC msg                    */
#define STAT16_RIDL		0x10	/* channel IDLe signal             */
#define STAT16_RAIS		0x20	/* Alarm Indication Signal         */
#define STAT16_ROOF		0x40	/* Out Of Frame sync               */
#define STAT16_RLOS		0x80	/* Loss Of Signal                  */

#define CTL20_CVEN		0x01	/* Coding Violation Counter Enbl   */

/* T1.107 Bit Oriented C-Bit Parity Far End Alarm Control and Status codes */
#define T3BOP_OOF		0x00	/* Yellow alarm status             */
#define T3BOP_LINE_UP		0x07	/* line loopback activate          */
#define T3BOP_LINE_DOWN		0x1C	/* line loopback deactivate        */
#define T3BOP_LOOP_DS3		0x1B	/* loopback full DS3               */
#define T3BOP_IDLE		0x1A	/* IDLE alarm status               */
#define T3BOP_AIS		0x16	/* AIS  alarm status               */
#define T3BOP_LOS		0x0E	/* LOS  alarm status               */

/* T1E1 regs;  RW=Read/Write;  RO=Read-Only;  RC=Read/Clr;  WO=Write-Only  */
#define Bt8370_DID		0x000	/* RO: Device ID                   */
#define Bt8370_CR0		0x001	/* RW; Primary Control Register    */
#define Bt8370_JAT_CR		0x002	/* RW: Jitter Attenuator CR        */
#define Bt8370_IRR		0x003	/* RO: Interrupt Request Reg       */
#define Bt8370_ISR7		0x004	/* RC: Alarm 1 Interrupt Status    */
#define Bt8370_ISR6		0x005	/* RC: Alarm 2 Interrupt Status    */
#define Bt8370_ISR5		0x006	/* RC: Error Interrupt Status      */
#define Bt8370_ISR4		0x007	/* RC; Cntr Ovfl Interrupt Status  */
#define Bt8370_ISR3		0x008	/* RC: Timer Interrupt Status      */
#define Bt8370_ISR2		0x009	/* RC: Data Link 1 Int Status      */
#define Bt8370_ISR1		0x00A	/* RC: Data Link 2 Int Status      */
#define Bt8370_ISR0		0x00B	/* RC: Pattrn Interrupt Status     */
#define Bt8370_IER7		0x00C	/* RW: Alarm 1 Interrupt Enable    */
#define Bt8370_IER6		0x00D	/* RW: Alarm 2 Interrupt Enable    */
#define Bt8370_IER5		0x00E	/* RW: Error Interrupt Enable      */
#define Bt8370_IER4		0x00F	/* RW: Cntr Ovfl Interrupt Enable  */

#define Bt8370_IER3		0x010	/* RW: Timer Interrupt Enable      */
#define Bt8370_IER2		0x011	/* RW: Data Link 1 Int Enable      */
#define Bt8370_IER1		0x012	/* RW: Data Link 2 Int Enable      */
#define Bt8370_IER0		0x013	/* RW: Pattern Interrupt Enable    */
#define Bt8370_LOOP		0x014	/* RW: Loopback Config Reg         */
#define Bt8370_DL3_TS		0x015	/* RW: External Data Link Channel  */
#define Bt8370_DL3_BIT		0x016	/* RW: External Data Link Bit      */
#define Bt8370_FSTAT		0x017	/* RO: Offline Framer Status       */
#define Bt8370_PIO		0x018	/* RW: Programmable Input/Output   */
#define Bt8370_POE		0x019	/* RW: Programmable Output Enable  */
#define Bt8370_CMUX		0x01A	/* RW: Clock Input Mux             */
#define Bt8370_TMUX		0x01B	/* RW: Test Mux Config             */
#define Bt8370_TEST		0x01C	/* RW: Test Config                 */

#define Bt8370_LIU_CR		0x020	/* RW: Line Intf Unit Config Reg   */
#define Bt8370_RSTAT		0x021	/* RO; Receive LIU Status          */
#define Bt8370_RLIU_CR		0x022	/* RW: Receive LIU Config          */
#define Bt8370_LPF		0x023	/* RW: RPLL Low Pass Filter        */
#define Bt8370_VGA_MAX		0x024	/* RW: Variable Gain Amplifier Max */
#define Bt8370_EQ_DAT		0x025	/* RW: Equalizer Coeff Data Reg    */
#define Bt8370_EQ_PTR		0x026	/* RW: Equzlizer Coeff Table Ptr   */
#define Bt8370_DSLICE		0x027	/* RW: Data Slicer Threshold       */
#define Bt8370_EQ_OUT		0x028	/* RW: Equalizer Output Levels     */
#define Bt8370_VGA		0x029	/* RO: Variable Gain Ampl Status   */
#define Bt8370_PRE_EQ		0x02A	/* RW: Pre-Equalizer               */

#define Bt8370_COEFF0		0x030	/* RO: LMS Adj Eq Coeff Status     */
#define Bt8370_GAIN0		0x038	/* RW: Equalizer Gain Thresh       */
#define Bt8370_GAIN1		0x039	/* RW: Equalizer Gain Thresh       */
#define Bt8370_GAIN2		0x03A	/* RW: Equalizer Gain Thresh       */
#define Bt8370_GAIN3		0x03B	/* RW: Equalizer Gain Thresh       */
#define Bt8370_GAIN4		0x03C	/* RW: Equalizer Gain Thresh       */

#define Bt8370_RCR0		0x040	/* RW: Rx Configuration            */
#define Bt8370_RPATT		0x041	/* RW: Rx Test Pattern Config      */
#define Bt8370_RLB		0x042	/* RW: Rx Loopback Code Detr Conf  */
#define Bt8370_LBA		0x043	/* RW: Loopback Activate Code Patt */
#define Bt8370_LBD		0x044	/* RW: Loopback Deact Code Patt    */
#define Bt8370_RALM		0x045	/* RW: Rx Alarm Signal Config      */
#define Bt8370_LATCH		0x046	/* RW: Alarm/Err/Cntr Latch Config */
#define Bt8370_ALM1		0x047	/* RO: Alarm 1 Status              */
#define Bt8370_ALM2		0x048	/* RO: Alarm 2 Status              */
#define Bt8370_ALM3		0x049	/* RO: Alarm 3 Status              */

#define Bt8370_FERR_LO		0x050	/* RC: Framing Bit Error Cntr LSB  */
#define Bt8370_FERR_HI		0x051	/* RC: Framing Bit Error Cntr MSB  */
#define Bt8370_CRC_LO		0x052	/* RC: CRC    Error   Counter LSB  */
#define Bt8370_CRC_HI		0x053	/* RC: CRC    Error   Counter MSB  */
#define Bt8370_LCV_LO		0x054	/* RC: Line Code Viol Counter LSB  */
#define Bt8370_LCV_HI		0x055	/* RC: Line Code Viol Counter MSB  */
#define Bt8370_FEBE_LO		0x056	/* RC: Far End Block Err Cntr LSB  */
#define Bt8370_FEBE_HI		0x057	/* RC: Far End Block Err Cntr MSB  */
#define Bt8370_BERR_LO		0x058	/* RC: PRBS Bit Error Counter LSB  */
#define Bt8370_BERR_HI		0x059	/* RC: PRBS Bit Error Counter MSB  */
#define Bt8370_AERR		0x05A	/* RC: SEF/LOF/COFA counter        */
#define Bt8370_RSA4		0x05B	/* RO: Rx Sa4 Byte Buffer          */
#define Bt8370_RSA5		0x05C	/* RO: Rx Sa5 Byte Buffer          */
#define Bt8370_RSA6		0x05D	/* RO: Rx Sa6 Byte Buffer          */
#define Bt8370_RSA7		0x05E	/* RO: Rx Sa7 Byte Buffer          */
#define Bt8370_RSA8		0x05F	/* RO: Rx Sa8 Byte Buffer          */

#define Bt8370_SHAPE0		0x060	/* RW: Tx Pulse Shape Config       */
#define Bt8370_TLIU_CR		0x068	/* RW: Tx LIU Config Reg           */

#define Bt8370_TCR0		0x070	/* RW: Tx Framer Config            */
#define Bt8370_TCR1		0x071	/* RW: Txter Configuration         */
#define Bt8370_TFRM		0x072	/* RW: Tx Frame Format             */
#define Bt8370_TERROR		0x073	/* RW: Tx Error Insert             */
#define Bt8370_TMAN		0x074	/* RW: Tx Manual Sa/FEBE Config    */
#define Bt8370_TALM		0x075	/* RW: Tx Alarm Signal Config      */
#define Bt8370_TPATT		0x076	/* RW: Tx Test Pattern Config      */
#define Bt8370_TLB		0x077	/* RW: Tx Inband Loopback Config   */
#define Bt8370_LBP		0x078	/* RW: Tx Inband Loopback Patt     */
#define Bt8370_TSA4		0x07B	/* RW: Tx Sa4 Byte Buffer          */
#define Bt8370_TSA5		0x07C	/* RW: Tx Sa5 Byte Buffer          */
#define Bt8370_TSA6		0x07D	/* RW: Tx Sa6 Byte Buffer          */
#define Bt8370_TSA7		0x07E	/* RW: Tx Sa7 Byte Buffer          */
#define Bt8370_TSA8		0x07F	/* RW: Tx Sa8 Byte Buffer          */

#define Bt8370_CLAD_CR		0x090	/* RW: Clock Rate Adapter Config   */
#define Bt8370_CSEL		0x091	/* RW: CLAD Frequency Select       */
#define Bt8370_CPHASE		0x092	/* RW: CLAD Phase Det Scale Factor */
#define Bt8370_CTEST		0x093	/* RW: CLAD Test                   */

#define Bt8370_BOP		0x0A0	/* RW: Bit Oriented Protocol Xcvr  */
#define Bt8370_TBOP		0x0A1	/* RW: Tx BOP Codeword             */
#define Bt8370_RBOP		0x0A2	/* RO; Rx BOP Codeword             */
#define Bt8370_BOP_STAT		0x0A3	/* RO: BOP Status                  */
#define Bt8370_DL1_TS		0x0A4	/* RW: DL1 Time Slot Enable        */
#define Bt8370_DL1_BIT		0x0A5	/* RW: DL1 Bit Enable              */
#define Bt8370_DL1_CTL		0x0A6	/* RW: DL1 Control                 */
#define Bt8370_RDL1_FFC		0x0A7	/* RW: RDL1 FIFO Fill Control      */
#define Bt8370_RDL1		0x0A8	/* RO: RDL1 FIFO                   */
#define Bt8370_RDL1_STAT	0x0A9	/* RO: RDL1 Status                 */
#define Bt8370_PRM		0x0AA	/* RW: Performance Report Message  */
#define Bt8370_TDL1_FEC		0x0AB	/* RW: TDL1 FIFO Empty Control     */
#define Bt8370_TDL1_EOM		0x0AC	/* WO: TDL1 End Of Message Control */
#define Bt8370_TDL1		0x0AD	/* RW: TDL1 FIFO                   */
#define Bt8370_TDL1_STAT	0x0AE	/* RO: TDL1 Status                 */
#define Bt8370_DL2_TS		0x0AF	/* RW: DL2 Time Slot Enable        */

#define Bt8370_DL2_BIT		0x0B0	/* RW: DL2 Bit Enable              */
#define Bt8370_DL2_CTL		0x0B1	/* RW: DL2 Control                 */
#define Bt8370_RDL2_FFC		0x0B2	/* RW: RDL2 FIFO Fill Control      */
#define Bt8370_RDL2		0x0B3	/* RO: RDL2 FIFO                   */
#define Bt8370_RDL2_STAT	0x0B4	/* RO: RDL2 Status                 */
#define Bt8370_TDL2_FEC		0x0B6	/* RW: TDL2 FIFO Empty Control     */
#define Bt8370_TDL2_EOM		0x0B7	/* WO; TDL2 End Of Message Control */
#define Bt8370_TDL2		0x0B8	/* RW: TDL2 FIFO                   */
#define Bt8370_TDL2_STAT	0x0B9	/* RO: TDL2 Status                 */
#define Bt8370_DL_TEST1		0x0BA	/* RW: DLINK Test Config           */
#define Bt8370_DL_TEST2		0x0BB	/* RW: DLINK Test Status           */
#define Bt8370_DL_TEST3		0x0BC	/* RW: DLINK Test Status           */
#define Bt8370_DL_TEST4		0x0BD	/* RW: DLINK Test Control          */
#define Bt8370_DL_TEST5		0x0BE	/* RW: DLINK Test Control          */

#define Bt8370_SBI_CR		0x0D0	/* RW: System Bus Interface Config */
#define Bt8370_RSB_CR		0x0D1	/* RW: Rx System Bus Config        */
#define Bt8370_RSYNC_BIT	0x0D2	/* RW: Rx System Bus Sync Bit Offs */
#define Bt8370_RSYNC_TS		0x0D3	/* RW: Rx System Bus Sync TS Offs  */
#define Bt8370_TSB_CR		0x0D4	/* RW: Tx System Bus Config        */
#define Bt8370_TSYNC_BIT	0x0D5	/* RW: Tx System Bus Sync Bit OFfs */
#define Bt8370_TSYNC_TS		0x0D6	/* RW: Tx System Bus Sync TS Offs  */
#define Bt8370_RSIG_CR		0x0D7	/* RW: Rx Siganalling Config       */
#define Bt8370_RSYNC_FRM	0x0D8	/* RW: Sig Reinsertion Frame Offs  */
#define Bt8370_SSTAT		0x0D9	/* RO: Slip Buffer Status          */
#define Bt8370_STACK		0x0DA	/* RO: Rx Signalling Stack         */
#define Bt8370_RPHASE		0x0DB	/* RO: RSLIP Phase Status          */
#define Bt8370_TPHASE		0x0DC	/* RO: TSLIP Phase Status          */
#define Bt8370_PERR		0x0DD	/* RO: RAM Parity Status           */

#define Bt8370_SBCn		0x0E0	/* RW: System Bus Per-Channel Ctl  */
#define Bt8370_TPCn		0x100	/* RW: Tx Per-Channel Control      */
#define Bt8370_TSIGn		0x120	/* RW: Tx Signalling Buffer        */
#define Bt8370_TSLIP_LOn	0x140	/* RW: Tx PCM Slip Buffer Lo       */
#define Bt8370_TSLIP_HIn	0x160	/* RW: Tx PCM Slip Buffer Hi       */
#define Bt8370_RPCn		0x180	/* RW: Rx Per-Channel Control      */
#define Bt8370_RSIGn		0x1A0	/* RW: Rx Signalling Buffer        */
#define Bt8370_RSLIP_LOn	0x1C0	/* RW: Rx PCM Slip Buffer Lo       */
#define Bt8370_RSLIP_HIn	0x1E0	/* RW: Rx PCM Slip Buffer Hi       */

/* Bt8370_LOOP (0x14) framer loopback control register bits */
#define LOOP_ANALOG		0x01	/* inward  loop thru LIU           */
#define LOOP_FRAMER		0x02	/* inward  loop thru framer        */
#define LOOP_LINE		0x04	/* outward loop thru LIU           */
#define LOOP_PAYLOAD		0x08	/* outward loop of payload         */
#define LOOP_DUAL		0x06	/* inward framer + outward line    */

/* Bt8370_ALM1 (0x47) receiver alarm status register bits */
#define ALM1_SIGFRZ		0x01	/* Rx Signalling Freeze            */
#define ALM1_RLOF		0x02	/* Rx loss of frame alignment      */
#define ALM1_RLOS		0x04	/* Rx digital loss of signal       */
#define ALM1_RALOS		0x08	/* Rx analog  loss of signal       */
#define ALM1_RAIS		0x10	/* Rx Alarm Indication Signal      */
#define ALM1_RYEL		0x40	/* Rx Yellow alarm indication      */
#define ALM1_RMYEL		0x80	/* Rx multiframe YELLOW alarm      */

/* Bt8370_ALM3 (0x49) receive framer status register bits */
#define ALM3_FRED		0x04	/* Rx Out Of T1/FAS alignment      */
#define ALM3_MRED		0x08	/* Rx Out Of MFAS alignment        */
#define ALM3_SRED		0x10	/* Rx Out Of CAS alignment         */
#define ALM3_SEF		0x20	/* Rx Severely Errored Frame       */
#define ALM3_RMAIS		0x40	/* Rx TS16 AIS (CAS)               */

/* Bt8370_TALM (0x75) transmit alarm control register bits */
#define TALM_TAIS		0x01	/* Tx Alarm Indication Signal      */
#define TALM_TYEL		0x02	/* Tx Yellow alarm                 */
#define TALM_TMYEL		0x04	/* Tx Multiframe Yellow alarm      */
#define TALM_AUTO_AIS		0x08	/* auto send AIS on LOS            */
#define TALM_AUTO_YEL		0x10	/* auto send YEL on LOF            */
#define TALM_AUTO_MYEL		0x20	/* auto send E1-Y16 on loss-of-CAS */

/* 8370 BOP (Bit Oriented Protocol) command fragments */
#define RBOP_OFF		0x00	/* BOP Rx disabled                 */
#define RBOP_25			0xE0	/* BOP Rx requires 25 BOPs         */
#define TBOP_OFF		0x00	/* BOP Tx disabled                 */
#define TBOP_25			0x0B	/* BOP Tx sends 25 BOPs            */
#define TBOP_CONT		0x0F	/* BOP Tx sends continuously       */

/* T1.403 Bit-Oriented ESF Data-Link Message codes */
#define T1BOP_OOF		0x00	/* Yellow alarm status             */
#define T1BOP_LINE_UP		0x07	/* line loopback activate          */
#define T1BOP_LINE_DOWN		0x1C	/* line loopback deactivate        */
#define T1BOP_PAY_UP		0x0A	/* payload loopback activate       */
#define T1BOP_PAY_DOWN		0x19	/* payload loopback deactivate     */
#define T1BOP_NET_UP		0x09	/* network loopback activate       */
#define T1BOP_NET_DOWN		0x12	/* network loopback deactivate     */

/* Unix & Linux reserve 16 device-private IOCTLs */
#if BSD
# define LMCIOCGSTAT		_IOWR('i', 240, struct status)
# define LMCIOCGCFG		_IOWR('i', 241, struct config)
# define LMCIOCSCFG		 _IOW('i', 242, struct config)
# define LMCIOCREAD		_IOWR('i', 243, struct ioctl)
# define LMCIOCWRITE		 _IOW('i', 244, struct ioctl)
# define LMCIOCTL		_IOWR('i', 245, struct ioctl)
#elif defined(__linux__)  /* sigh */
# define LMCIOCGSTAT		SIOCDEVPRIVATE+0
# define LMCIOCGCFG		SIOCDEVPRIVATE+1
# define LMCIOCSCFG		SIOCDEVPRIVATE+2
# define LMCIOCREAD		SIOCDEVPRIVATE+3
# define LMCIOCWRITE		SIOCDEVPRIVATE+4
# define LMCIOCTL		SIOCDEVPRIVATE+5
#endif

struct iohdr				/* all LMCIOCs begin with this     */
  {
  char ifname[IFNAMSIZ];		/* interface name, e.g. "lmc0"     */
  u_int32_t cookie;			/* interface version number        */
  u_int16_t direction;			/* missing in Linux IOCTL          */
  u_int16_t length;			/* missing in Linux IOCTL          */
  struct iohdr *iohdr;			/* missing in Linux IOCTL          */
  u_int32_t spare;			/* pad this struct to **32 bytes** */
  };

#define DIR_IO   0
#define DIR_IOW  1			/* copy data user->kernel          */
#define DIR_IOR  2			/* copy data kernel->user          */
#define DIR_IOWR 3			/* copy data kernel<->user         */

struct hssi_snmp
  {
  u_int16_t sigs;			/* MII16_HSSI & MII16_HSSI_MODEM   */
  };

struct ssi_snmp
  {
  u_int16_t sigs;			/* MII16_SSI & MII16_SSI_MODEM     */
  };

struct t3_snmp
  {
  u_int16_t febe;			/*  8 bits - Far End Block err cnt */
  u_int16_t lcv;			/* 16 bits - BPV           err cnt */
  u_int16_t pcv;			/*  8 bits - P-bit         err cnt */
  u_int16_t ccv;			/*  8 bits - C-bit         err cnt */
  u_int16_t line;			/* line status bit vector          */
  u_int16_t loop;			/* loop status bit vector          */
  };

struct t1_snmp
  {
  u_int16_t prm[4];			/* T1.403 Performance Report Msg   */
  u_int16_t febe;			/* 10 bits - E1 FAR CRC    err cnt */
  u_int16_t lcv;			/* 16 bits - BPV + EXZ     err cnt */
  u_int16_t fe;				/* 12 bits - Ft/Fs/FPS/FAS err cnt */
  u_int16_t crc;			/* 10 bits - CRC6/CRC4     err cnt */
  u_int16_t line;			/* line status bit vector          */
  u_int16_t loop;			/* loop status bit vector          */
  };

/* SNMP trunk MIB Send codes */
#define TSEND_NORMAL		   1	/* Send data (normal or looped)    */
#define TSEND_LINE		   2	/* Send 'line loopback activate'   */
#define TSEND_PAYLOAD		   3	/* Send 'payload loop activate'    */
#define TSEND_RESET		   4	/* Send 'loopback deactivate'      */
#define TSEND_QRS		   5	/* Send Quasi Random Signal        */

/* ANSI T1.403 Performance Report Msg -- once a second from the far end    */
#define T1PRM_FE		0x8000	/* Frame Sync Bit Error Event >= 1 */
#define T1PRM_SE		0x4000	/* Severely Err Framing Event >= 1 */
#define T1PRM_LB		0x2000	/* Payload Loopback Activated      */
#define T1PRM_G1		0x1000	/* CRC Error Event = 1             */
#define T1PRM_R			0x0800  /* Reserved                        */
#define T1PRM_G2		0x0400	/* 1 < CRC Error Event <= 5        */
#define T1PRM_SEQ		0x0300	/* modulo 4 counter                */
#define T1PRM_G3		0x0080	/* 5 < CRC Error Event <= 10       */
#define T1PRM_LV		0x0040	/* Line Code Violation Event >= 1  */
#define T1PRM_G4		0x0020	/* 10 < CRC Error Event <= 100     */
#define T1PRM_U			0x0018	/* Under study for synchronization */
#define T1PRM_G5		0x0004	/* 100 < CRC Error Event <= 319    */
#define T1PRM_SL		0x0002  /* Slip Event >= 1                 */
#define T1PRM_G6		0x0001	/* CRC Error Event >= 320          */

/* SNMP Line Status */
#define TLINE_NORM		0x0001	/* no alarm present                */
#define TLINE_RX_RAI		0x0002	/* receiving RAI = Yellow alarm    */
#define TLINE_TX_RAI		0x0004	/* sending   RAI = Yellow alarm    */
#define TLINE_RX_AIS		0x0008  /* receiving AIS =  blue  alarm    */
#define TLINE_TX_AIS		0x0010	/* sending   AIS =  blue  alarm    */
#define TLINE_LOF		0x0020	/* near end  LOF =   red  alarm    */
#define TLINE_LOS		0x0040	/* near end loss of Signal         */
#define TLINE_LOOP		0x0080	/* near end is looped              */
#define T1LINE_RX_TS16_AIS	0x0100	/* near end receiving TS16 AIS     */
#define T1LINE_RX_TS16_LOMF	0x0200	/* near end sending   TS16 LOMF    */
#define T1LINE_TX_TS16_LOMF	0x0400	/* near end receiving TS16 LOMF    */
#define T1LINE_RX_TEST		0x0800	/* near end receiving QRS Signal   */
#define T1LINE_SEF		0x1000	/* near end severely errored frame */
#define T3LINE_RX_IDLE		0x0100	/* near end receiving IDLE signal  */
#define T3LINE_SEF		0x0200	/* near end severely errored frame */

/* SNMP Loopback Status */
#define TLOOP_NONE		0x01    /* no loopback                     */
#define TLOOP_NEAR_PAYLOAD	0x02	/* near end payload loopback       */
#define TLOOP_NEAR_LINE		0x04	/* near end line loopback          */
#define TLOOP_NEAR_OTHER	0x08	/* near end looped somehow         */
#define TLOOP_NEAR_INWARD	0x10	/* near end looped inward          */
#define TLOOP_FAR_PAYLOAD	0x20	/* far  end payload loopback       */
#define TLOOP_FAR_LINE		0x40	/* far  end line loopback          */

/* event counters record interesting statistics */
struct event_cntrs
  {
  struct timeval reset_time;		/* time when cntrs were reset      */
  u_int64_t ibytes;			/* Rx bytes   with good status     */
  u_int64_t obytes;			/* Tx bytes                        */
  u_int64_t ipackets;			/* Rx packets with good status     */
  u_int64_t opackets;			/* Tx packets                      */
  u_int32_t ierrors;			/* Rx packets with bad status      */
  u_int32_t oerrors;			/* Tx packets with bad status      */
  u_int32_t idiscards;			/* Rx packets discarded            */
  u_int32_t odiscards;			/* Tx packets discarded            */
  u_int32_t fifo_over;			/* Rx fifo overruns                */
  u_int32_t fifo_under;			/* Tx fifo underruns               */
  u_int32_t missed;			/* Rx pkts missed: no DMA descs    */
  u_int32_t overruns;			/* Rx pkts missed: fifo overrun    */
  u_int32_t fdl_pkts;			/* Rx T1 Facility Data Link pkts   */
  u_int32_t crc_errs;			/* Rx T1 frame CRC errors          */
  u_int32_t lcv_errs;			/* Rx T1 T3 Line Coding Violation  */
  u_int32_t frm_errs;			/* Rx T1 T3 Frame bit errors       */
  u_int32_t febe_errs;			/* Rx T1 T3 Far End Bit Errors     */
  u_int32_t par_errs;			/* Rx T3 P-bit parity errors       */
  u_int32_t cpar_errs;			/* Rx T3 C-bit parity errors       */
  u_int32_t mfrm_errs;			/* Rx T3 Multi-frame bit errors    */
  u_int32_t rxdma;			/* Rx out of kernel buffers        */
  u_int32_t txdma;			/* Tx out of DMA desciptors        */
  u_int32_t lck_watch;			/* try_lock conflict in watchdog   */
  u_int32_t lck_ioctl;			/* try_lock conflict in ioctl      */
  u_int32_t lck_intr;			/* try_lock conflict in interrupt  */
  };

/* sc->status is the READ ONLY status of the card.                         */
/* Accessed using socket IO control calls or netgraph control messages.    */
struct status
  {
  struct iohdr iohdr;			/* common ioctl header             */
  u_int32_t card_type;			/* PCI device number               */
  u_int16_t ieee[3];			/* IEEE MAC-addr from Tulip SROM   */
  u_int16_t oper_status;		/* actual state:  up, down, test   */
  u_int32_t tx_speed;			/* measured TX bits/sec            */
  u_int32_t cable_type;			/* SSI only: cable type            */
  u_int32_t line_pkg;			/* actual line pkg in use          */
  u_int32_t line_prot;			/* actual line proto in use        */
  u_int32_t ticks;			/* incremented by watchdog @ 1 Hz  */
  struct event_cntrs cntrs;		/* event counters                  */
  union
    {
    struct hssi_snmp hssi;		/* data for RFC-???? HSSI MIB      */
    struct t3_snmp t3;			/* data for RFC-2496 T3 MIB        */
    struct ssi_snmp ssi;		/* data for RFC-1659 RS232 MIB     */
    struct t1_snmp t1;			/* data for RFC-2495 T1 MIB        */
    } snmp;
  };

/* line protocol package codes                                       fnobl */
#define PKG_RAWIP		   1	/* driver                    yyyyy */
#define PKG_SPPP		   2	/* fbsd, nbsd, obsd          yyynn */
#define PKG_P2P			   3	/* bsd/os                    nnnyn */
#define PKG_NG			   4	/* fbsd                      ynnnn */
#define PKG_GEN_HDLC		   5	/* linux                     nnnny */

/* line protocol codes                                               fnobl */
#define PROT_PPP		   1	/* Point-to-Point Protocol   yyyyy */
#define PROT_C_HDLC		   2	/* Cisco HDLC Protocol       yyyyy */
#define PROT_FRM_RLY		   3	/* Frame Relay Protocol      ynnyy */
#define PROT_X25		   4	/* X.25/LAPB Protocol        nnnny */
#define PROT_ETH_HDLC		   5	/* raw Ether pkts in HDLC    nnnny */
#define PROT_IP_HDLC		   6	/* raw IP4/6 pkts in HDLC    yyyyy */

/* oper_status codes (same as SNMP status codes) */
#define STATUS_UP		   1	/* may/will    tx/rx pkts          */
#define STATUS_DOWN		   2	/* can't/won't tx/rx pkts          */
#define STATUS_TEST		   3	/* currently not used              */

struct synth				/* programmable oscillator params  */
  {
  unsigned n :7;			/*   numerator (3..127)            */
  unsigned m :7;			/* denominator (3..127)            */
  unsigned v :1;			/* mul by 1|8                      */
  unsigned x :2;			/* div by 1|2|4|8                  */
  unsigned r :2;			/* div by 1|2|4|8                  */
  unsigned prescale :13;		/* log(final divisor): 2, 4 or 9   */
  } __attribute__ ((packed));

#define SYNTH_FREF	        20e6	/* reference xtal =  20 MHz        */
#define SYNTH_FMIN	        50e6	/* internal VCO min  50 MHz        */
#define SYNTH_FMAX	       250e6	/* internal VCO max 250 MHz        */

/* sc->config is the READ/WRITE configuration of the card.                 */
/* Accessed using socket IO control calls or netgraph control messages.    */
struct config
  {
  struct iohdr iohdr;			/* common ioctl header             */
  u_int32_t crc_len;			/* ALL: CRC-16 or CRC-32 or none   */
  u_int32_t loop_back;			/* ALL: many kinds of loopbacks    */
  u_int32_t tx_clk_src;			/* T1, HSSI: ST, RT, int, ext      */
  u_int32_t format;			/* T3, T1: ckt framing format      */
  u_int32_t time_slots;			/* T1: 64Kb time slot config       */
  u_int32_t cable_len;			/* T3, T1: cable length in meters  */
  u_int32_t scrambler;			/* T3: payload scrambler config    */
  u_int32_t dte_dce;			/* SSI, HSSIc: drive TXCLK         */
  struct synth synth;			/* SSI, HSSIc: synth oscil params  */
  u_int32_t rx_gain;			/* T1: receiver gain limit 0-50 dB */
  u_int32_t tx_pulse;			/* T1: transmitter pulse shape     */
  u_int32_t tx_lbo;			/* T1: transmitter atten 0-22.5 dB */
  u_int32_t debug;			/* ALL: extra printout             */
  u_int32_t line_pkg;			/* ALL:  use this line pkg         */
  u_int32_t line_prot;			/* SPPP: use this line proto       */
  u_int32_t keep_alive;			/* SPPP: use keep-alive packets    */
  };

#define CFG_CRC_0		   0	/* no CRC                          */
#define CFG_CRC_16		   2    /* X^16+X^12+X^5+1 (default)       */
#define CFG_CRC_32		   4	/* X^32+X^26+X^23+X^22+X^16+X^12+  */
					/* X^11+X^10+X^8+X^7+X^5+X^4+X^2+X+1 */
#define CFG_LOOP_NONE		   1	/* SNMP don't loop back anything   */
#define CFG_LOOP_PAYLOAD	   2    /* SNMP loop outward thru framer   */
#define CFG_LOOP_LINE		   3	/* SNMP loop outward thru LIU      */
#define CFG_LOOP_OTHER		   4	/* SNMP loop  inward thru LIU      */
#define CFG_LOOP_INWARD		   5	/* SNMP loop  inward thru framer   */
#define CFG_LOOP_DUAL		   6	/* SNMP loop  inward & outward     */
#define CFG_LOOP_TULIP		  16	/* ALL: loop  inward thru Tulip    */
#define CFG_LOOP_PINS		  17	/* HSSIc, SSI: loop inward-pins    */
#define CFG_LOOP_LL		  18	/* HSSI, SSI: assert LA/LL mdm pin */
#define CFG_LOOP_RL		  19	/* HSSI, SSI: assert LB/RL mdm pin */

#define CFG_CLKMUX_ST		   1	/* TX clk <- Send timing           */
#define CFG_CLKMUX_INT		   2	/* TX clk <- internal source       */
#define CFG_CLKMUX_RT		   3	/* TX clk <- Receive (loop) timing */
#define CFG_CLKMUX_EXT		   4    /* TX clk <- ext connector         */

/* values 0-31 are Bt8370 CR0 register values (LSB is zero if E1).         */
/* values 32-99 are reserved for other T1E1 formats, (even number if E1)   */
/* values 100 and up are used for T3 frame formats.                        */
#define CFG_FORMAT_T1SF		   9	/* T1-SF          AMI              */
#define CFG_FORMAT_T1ESF	  27	/* T1-ESF+CRC     B8ZS     X^6+X+1 */
#define CFG_FORMAT_E1FAS	   0	/* E1-FAS         HDB3 TS0         */
#define CFG_FORMAT_E1FASCRC	   8	/* E1-FAS+CRC     HDB3 TS0 X^4+X+1 */
#define CFG_FORMAT_E1FASCAS	  16	/* E1-FAS    +CAS HDB3 TS0 & TS16  */
#define CFG_FORMAT_E1FASCRCCAS	  24	/* E1-FAS+CRC+CAS HDB3 TS0 & TS16  */
#define CFG_FORMAT_E1NONE	  32	/* E1-NO framing  HDB3             */
#define CFG_FORMAT_T3CPAR	 100	/* T3-C-Bit par   B3ZS             */
#define CFG_FORMAT_T3M13	 101	/* T3-M13 format  B3ZS             */

/* format aliases that improve code readability */
#define FORMAT_T1ANY		((sc->config.format & 1)==1)
#define FORMAT_E1ANY		((sc->config.format & 1)==0)
#define FORMAT_E1CAS		((sc->config.format & 0x11)==0x10)
#define FORMAT_E1CRC		((sc->config.format & 0x09)==0x08)
#define FORMAT_E1NONE		 (sc->config.format == CFG_FORMAT_E1NONE)
#define FORMAT_T1ESF		 (sc->config.format == CFG_FORMAT_T1ESF)
#define FORMAT_T1SF		 (sc->config.format == CFG_FORMAT_T1SF)
#define FORMAT_T3CPAR		 (sc->config.format == CFG_FORMAT_T3CPAR)

#define CFG_SCRAM_OFF		   1	/* DS3 payload scrambler off       */
#define CFG_SCRAM_DL_KEN	   2	/* DS3 DigitalLink/Kentrox X^43+1  */
#define CFG_SCRAM_LARS		   3	/* DS3 Larscom X^20+X^17+1 w/28ZS  */

#define CFG_DTE			   1	/* HSSIc, SSI: rcv TXCLK; rcv DCD  */
#define CFG_DCE			   2	/* HSSIc, SSI: drv TXCLK; drv DCD  */

#define CFG_GAIN_SHORT		0x24	/* 0-20 dB of equalized gain       */
#define CFG_GAIN_MEDIUM		0x2C	/* 0-30 dB of equalized gain       */
#define CFG_GAIN_LONG		0x34	/* 0-40 dB of equalized gain       */
#define CFG_GAIN_EXTEND		0x3F	/* 0-64 dB of equalized gain       */
#define CFG_GAIN_AUTO		0xFF	/* auto-set based on cable length  */

#define CFG_PULSE_T1DSX0	   0	/* T1 DSX   0- 40 meters           */
#define CFG_PULSE_T1DSX1	   2	/* T1 DSX  40- 80 meters           */
#define CFG_PULSE_T1DSX2	   4	/* T1 DSX  80-120 meters           */
#define CFG_PULSE_T1DSX3	   6	/* T1 DSX 120-160 meters           */
#define CFG_PULSE_T1DSX4	   8	/* T1 DSX 160-200 meters           */
#define CFG_PULSE_E1COAX	  10	/* E1  75 ohm coax pair            */
#define CFG_PULSE_E1TWIST	  12	/* E1 120 ohm twisted pairs        */
#define CFG_PULSE_T1CSU		  14	/* T1 CSU 200-2000 meters; set LBO */
#define CFG_PULSE_AUTO		0xFF	/* auto-set based on cable length  */

#define CFG_LBO_0DB		   0	/* T1CSU LBO =  0.0 dB; FCC opt A  */
#define CFG_LBO_7DB		  16	/* T1CSU LBO =  7.5 dB; FCC opt B  */
#define CFG_LBO_15DB		  32	/* T1CSU LBO = 15.0 dB; FCC opt C  */
#define CFG_LBO_22DB		  48	/* T1CSU LBO = 22.5 dB; final span */
#define CFG_LBO_AUTO		0xFF	/* auto-set based on cable length  */

struct ioctl
  {
  struct iohdr iohdr;			/* common ioctl header             */
  u_int32_t cmd;			/* command			   */
  u_int32_t address;			/* command address                 */
  u_int32_t data;			/* command data                    */
  char *ucode;				/* user-land address of ucode      */
  };

#define IOCTL_RW_PCI		   1	/* RW: Tulip PCI config registers  */
#define IOCTL_RW_CSR		   2	/* RW: Tulip Control & Status Regs */
#define IOCTL_RW_SROM		   3	/* RW: Tulip Serial Rom            */
#define IOCTL_RW_BIOS		   4	/* RW: Tulip Boot rom              */
#define IOCTL_RW_MII		   5	/* RW: MII registers               */
#define IOCTL_RW_FRAME		   6	/* RW: Framer registers            */
#define IOCTL_WO_SYNTH		   7	/* WO: Synthesized oscillator      */
#define IOCTL_WO_DAC		   8	/* WO: Digital/Analog Converter    */

#define IOCTL_XILINX_RESET	  16	/* reset Xilinx: all FFs set to 0  */
#define IOCTL_XILINX_ROM	  17	/* load  Xilinx program from ROM   */
#define IOCTL_XILINX_FILE	  18	/* load  Xilinx program from file  */

#define IOCTL_SET_STATUS	  50	/* set mdm ctrl bits (internal use)*/
#define IOCTL_SNMP_SEND		  51	/* trunk MIB send code             */
#define IOCTL_SNMP_LOOP		  52	/* trunk MIB loop configuration    */
#define IOCTL_SNMP_SIGS		  53	/* RS232-like modem control sigs   */
#define IOCTL_RESET_CNTRS	  54	/* reset event counters            */

/* storage for these strings is allocated here! */
const char *ssi_cables[] =
  {
  "V.10/EIA423",
  "V.11/EIA530A",
  "RESERVED",
  "X.21",
  "V.35",
  "V.36/EIA449",
  "V.28/EIA232",
  "NO CABLE",
  NULL,
  };

/***************************************************************************/
/*    Declarations above here are shared with the user lmcconfig program.  */
/*    Declarations below here are private to the kernel device driver.     */
/***************************************************************************/

#if (_KERNEL || KERNEL || __KERNEL__)

#define SNDQ_MAXLEN	32	/* packets awaiting transmission */
#define DESCS_PER_PKT	 4	/* DMA descriptors per TX pkt */
#define	NUM_TX_DESCS	(DESCS_PER_PKT * SNDQ_MAXLEN)
/* Increase DESCS_PER_PKT if status.cntrs.txdma increments. */

/* A Tulip DMA descriptor can point to two chunks of memory.
 * Each chunk has a max length of 2047 bytes (ask the VMS guys...).
 * 2047 isn't a multiple of a cache line size (32 bytes typically).
 * So back off to 2048-32 = 2016 bytes per chunk (2 chunks per descr).
 */
#define MAX_CHUNK_LEN	2016
#define MAX_DESC_LEN	(2 * MAX_CHUNK_LEN)

/* Tulip DMA descriptor; THIS STRUCT MUST MATCH THE HARDWARE */
struct dma_desc
  {
  u_int32_t status;		/* hardware->to->software */
#if (BYTE_ORDER == LITTLE_ENDIAN) /* left-to-right packing by compiler */
  unsigned  length1:11;		/* buffer1 length */
  unsigned  length2:11;		/* buffer2 length */
  unsigned  control:10;		/* software->to->hardware */
#else /* right-to-left packing by compiler */
  unsigned  control:10;		/* software->to->hardware */
  unsigned  length2:11;		/* buffer2 length */
  unsigned  length1:11;		/* buffer1 length */
#endif
  u_int32_t address1;		/* buffer1 bus address */
  u_int32_t address2;		/* buffer2 bus address */
#if (defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__))
  bus_dmamap_t map;		/* bus dmamap for this descriptor */
# define TLP_BUS_DSL_VAL	(sizeof(bus_dmamap_t) & TLP_BUS_DSL)
#else
# define TLP_BUS_DSL_VAL	0
#endif
  } __attribute__ ((packed));

/* Tulip DMA descriptor status bits */
#define TLP_DSTS_OWNER		0x80000000
#define TLP_DSTS_RX_DESC_ERR	0x00004000
#define TLP_DSTS_RX_FIRST_DESC	0x00000200
#define TLP_DSTS_RX_LAST_DESC	0x00000100
#define TLP_DSTS_RX_MII_ERR	0x00000008
#define TLP_DSTS_RX_DRIBBLE	0x00000004
#define TLP_DSTS_TX_UNDERRUN	0x00000002
#define TLP_DSTS_RX_OVERRUN	0x00000001  /* not documented in rev AF */
#define TLP_DSTS_RX_BAD		(TLP_DSTS_RX_MII_ERR  | \
				 TLP_DSTS_RX_DRIBBLE  | \
				 TLP_DSTS_RX_DESC_ERR | \
				 TLP_DSTS_RX_OVERRUN)

/* Tulip DMA descriptor control bits */
#define TLP_DCTL_TX_INTERRUPT	0x0200
#define TLP_DCTL_TX_LAST_SEG	0x0100
#define TLP_DCTL_TX_FIRST_SEG	0x0080
#define TLP_DCTL_TX_NO_CRC	0x0010
#define TLP_DCTL_END_RING	0x0008
#define TLP_DCTL_TX_NO_PAD	0x0002

/* DMA descriptors are kept in a ring.
 * Ring is empty when (read == write).
 * Ring is full  when (read == wrap(write+1)),
 * The ring also contains a tailq of data buffers.
 */
struct desc_ring
  {
  struct dma_desc *read;	/* next  descriptor to be read */
  struct dma_desc *write;	/* next  descriptor to be written */
  struct dma_desc *first;	/* first descriptor in ring */
  struct dma_desc *last;	/* last  descriptor in ring */
  struct dma_desc *temp;	/* temporary write pointer for tx */
  u_int32_t dma_addr;		/* bus address for desc array */
  int size_descs;		/* bus_dmamap_sync needs this */
  int num_descs;		/* used to set rx quota */
#ifdef __linux__
  struct sk_buff *head;		/* tail-queue of skbuffs */
  struct sk_buff *tail;
#elif BSD
  struct mbuf *head;		/* tail-queue of mbufs */
  struct mbuf *tail;
# if (defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__))
  bus_dma_tag_t tag;		/* bus_dma tag for desc array */
  bus_dmamap_t map;		/* bus_dma map for desc array */
  bus_dma_segment_t segs[2];	/* bus_dmamap_load() or bus_dmamem_alloc() */
  int nsegs;			/* bus_dmamap_load() or bus_dmamem_alloc() */
# endif
#endif
  };

/* break circular definition */
typedef struct softc softc_t;

/* card-dependent methods */
struct card
  {
  void (* config)(softc_t *);
  void (* ident)(softc_t *);
  int  (* watchdog)(softc_t *);		/* must not sleep */
  int  (* ioctl)(softc_t *, struct ioctl *); /* can sleep */
  };

/* flag bits in sc->flags */
#define FLAG_IFNET		0x00000002  /* IFNET is attached           */
#define FLAG_NETDEV		0x00000004  /* NETDEV is registered        */
#define FLAG_NETGRAPH		0x00000008  /* NETGRAPH is attached        */

/* Accessing Tulip CSRs:
 * There are two ways: IO instruction (default) and memory reference.
 *  IO refs are used if IOREF_CSR is defined; otherwise memory refs are used.
 *  MEMORY REFERENCES DO NOT WORK in BSD/OS: page faults happen.
 */
#define IOREF_CSR 1  /* access Tulip CSRs with IO cycles if 1 */

#if (defined(__FreeBSD__) && defined(DEVICE_POLLING))
# define DEV_POLL 1
#else
# define DEV_POLL 0
#endif

#if defined(ALTQ) && ALTQ
# define ALTQ_PRESENT 1
#else
# define ALTQ_PRESENT 0
#endif

/* This is the instance data, or "software context" for the device driver. */
/* NetBSD, OpenBSD and BSD/OS want struct device first in the softc. */
/* FreeBSD wants struct ifnet first in the softc. */
struct softc
  {
#if (defined(__NetBSD__) || defined(__OpenBSD__))
  struct device	dev;		/* base device -- must be first in softc   */
  pcitag_t	pa_tag;		/* pci_conf_read/write need this           */
  pci_chipset_tag_t pa_pc;	/* pci_conf_read/write need this           */
  bus_dma_tag_t	pa_dmat;	/* bus_dma needs this                      */
  bus_space_tag_t csr_tag;	/* bus_space needs this                    */
  bus_space_handle_t csr_handle;/* bus_space needs this                    */
  pci_intr_handle_t intr_handle;/* interrupt handle                        */
  void		*irq_cookie;	/* pci_intr_disestablish needs this        */
  void		*sdh_cookie;	/* shutdownhook_disestablish needs this    */
  struct simplelock top_lock;	/* lock card->watchdog vs core_ioctl       */
  struct simplelock bottom_lock;/* lock for buf queues & descriptor rings  */
  struct mbuf	*tx_mbuf;	/* hang mbuf here while building dma descs */
#endif  /* __NetBSD__ || __OpenBSD__ */

#ifdef __bsdi__
  struct device	dev;		/* base device -- must be first in softc   */
  struct isadev	id;		/* bus resource                            */
  struct intrhand ih;		/* interrupt vectoring                     */
  struct atshutdown ats;	/* shutdown hook                           */
  pci_devaddr_t	cfgbase;	/* base address of PCI config regs         */
  u_int16_t	 csr_iobase;	/*     io base address of Tulip CSRs       */
  u_int32_t	*csr_membase;	/* kv mem base address of Tulip CSRs       */
  struct simplelock top_lock;	/* lock card->watchdog vs core_ioctl       */
  struct simplelock bottom_lock;/* lock for buf queues & descriptor rings  */
  struct mbuf	*tx_mbuf;	/* hang mbuf here while building dma descs */
#endif /* __bsdi__ */

  /* State for kernel-resident Line Protocols */
#if IFNET
  struct ifnet *ifp;
  struct ifmedia ifm;		/* hooks for ifconfig(8) */
# if NSPPP
#  if (__FreeBSD_version < 600000)
  struct sppp spppcom;		/* must be first in sc for fbsd < 6 */
#  endif
  struct sppp *sppp;
# elif P2P
  struct p2pcom p2pcom;
  struct p2pcom *p2p;
# elif (__FreeBSD_version < 600000)
  struct ifnet ifnet;		/* must be first in sc for fbsd < 6 */
# endif
#endif

#ifdef __linux__
# if GEN_HDLC
  hdlc_device	*hdlc_dev;	/* state for HDLC code                     */
  sync_serial_settings hdlc_settings; /* state set by sethdlc program      */
# else
  struct net_device_stats net_stats; /* linux_stats storage                */
# endif
#endif

#if NETGRAPH
  node_p	ng_node;	/* pointer to our node struct              */
  hook_p	ng_hook;	/* non-zero means NETGRAPH owns device     */
# if (__FreeBSD_version >= 503000)
  struct ifaltq	ng_sndq;
  struct ifaltq ng_fastq;
# else
  struct ifqueue ng_sndq;
  struct ifqueue ng_fastq;
# endif
#endif

#ifdef __FreeBSD__
  struct callout callout;	/* watchdog needs this                  */
  struct device	*dev;		/* base device pointer                     */
  bus_space_tag_t csr_tag;	/* bus_space needs this                    */
  bus_space_handle_t csr_handle;/* bus_space_needs this                    */
  void		*irq_cookie;	/* bus_teardown_intr needs this            */
  struct resource *irq_res;	/* bus_release_resource needs this         */
  int		irq_res_id;	/* bus_release_resource needs this         */
  struct resource *csr_res;	/* bus_release_resource needs this         */
  int		csr_res_id;	/* bus_release resource needs this         */
  int		csr_res_type;	/* bus_release resource needs this         */
  struct mbuf	*tx_mbuf;	/* hang mbuf here while building dma descs */
# ifdef DEVICE_POLLING
  int		quota;		/* used for incoming packet flow control   */
# endif
# if (__FreeBSD_version >= 500000)
  struct mtx	top_mtx;	/* lock card->watchdog vs core_ioctl       */
  struct mtx	bottom_mtx;	/* lock for buf queues & descriptor rings  */
# else /* FreeBSD-4 */
  int		top_spl;	/* lock card->watchdog vs core_ioctl       */
  int		bottom_spl;	/* lock for buf queues & descriptor rings  */
# endif
#endif /* __FreeBSD__ */

#ifdef __linux__
  struct pci_dev    *pci_dev;	/* READ/WRITE_PCI_CFG macros need this     */
  struct net_device *net_dev;	/* NAME_UNIT macro needs this              */
  struct timer_list wd_timer;	/* timer calls watchdog() once a second    */
  u_int32_t	 csr_iobase;	/*     io base address of Tulip CSRs       */
  void		*csr_membase;	/* kv mem base address of Tulip CSRs       */
  struct sk_buff *tx_skb;	/* hang skb here while building dma descs  */
  int		quota;		/* used for incoming packet flow control   */
  struct semaphore top_lock;	/* lock card->watchdog vs core_ioctl       */
  spinlock_t	bottom_lock;	/* lock for buf queues & descriptor rings  */
#endif  /* __linux__ */

  /* Top-half state used by all card types; lock with top_lock,            */
  const char	*dev_desc;	/* string describing type of board         */
  struct status status;		/* driver status lmcconfig can read        */
  struct config	config;		/* driver config lmcconfig can read/write  */
  struct card	*card;		/* card methods: config, ioctl, watchdog   */
  u_int32_t	gpio_dir;	/* s/w copy of GPIO direction register     */
  u_int16_t	led_state;	/* last value written to mii16             */
  u_int32_t	flags;		/* driver-global flags                     */

  /* Top-half state used by card-specific watchdogs; lock with top_lock.   */
  u_int32_t	last_mii16;	/* SSI, HSSI: MII reg 16 one second ago    */
  u_int32_t	last_stat16;	/* T3:     framer reg 16 one second ago    */
  u_int32_t	last_alm1;	/* T1E1:   framer reg 47 one second ago    */
  u_int32_t	last_FEAC;	/* last FEAC msg code received             */
  u_int32_t	loop_timer;	/* seconds until loopback expires          */

  /* Bottom-half state used by the interrupt code; lock with bottom_lock.  */
  struct desc_ring txring;	/* tx descriptor ring state                */
  struct desc_ring rxring;	/* rx descriptor ring state                */
  };  /* end of softc */

/* Hide the minor differences between OS versions */

#ifdef __FreeBSD__
  typedef void intr_return_t;
# define  READ_PCI_CFG(sc, addr)       pci_read_config ((sc)->dev, addr, 4)
# define WRITE_PCI_CFG(sc, addr, data) pci_write_config((sc)->dev, addr, data, 4)
# define  READ_CSR(csr)		bus_space_read_4 (sc->csr_tag, sc->csr_handle, csr)
# define WRITE_CSR(csr, val)	bus_space_write_4(sc->csr_tag, sc->csr_handle, csr, val)
# define NAME_UNIT		device_get_nameunit(sc->dev)
# define DRIVER_DEBUG		((sc->config.debug) || (sc->ifp->if_flags & IFF_DEBUG))
# if (__FreeBSD_version >= 500000)
#  define TOP_TRYLOCK		mtx_trylock(&sc->top_mtx)
#  define TOP_UNLOCK		mtx_unlock (&sc->top_mtx)
#  define BOTTOM_TRYLOCK	mtx_trylock(&sc->bottom_mtx)
#  define BOTTOM_UNLOCK		mtx_unlock (&sc->bottom_mtx)
#  if (__FreeBSD_version >= 700000)
#   define CHECK_CAP		priv_check(curthread, PRIV_DRIVER)
#  else
#   define CHECK_CAP		suser(curthread)
#  endif
# else /* FreeBSD-4 */
#  define TOP_TRYLOCK		(sc->top_spl = splimp())
#  define TOP_UNLOCK		splx(sc->top_spl)
#  define BOTTOM_TRYLOCK	1 /* giant_lock protects */
#  define BOTTOM_UNLOCK		/* nothing */
#  define CHECK_CAP		suser(curproc)
# endif
# define DISABLE_INTR		/* nothing */
# define ENABLE_INTR		/* nothing */
# define IRQ_NONE		/* nothing */
# define IRQ_HANDLED		/* nothing */
# define IFP2SC(ifp)		(ifp)->if_softc
# define COPY_BREAK		MHLEN
# define SLEEP(usecs)		tsleep(sc, PCATCH | PZERO, DEVICE_NAME, 1+(usecs/tick))
# define DMA_SYNC(map, size, flags) bus_dmamap_sync(ring->tag, map, flags)
# define DMA_LOAD(map, addr, size)  bus_dmamap_load(ring->tag, map, addr, size, fbsd_dmamap_load, ring, 0)
# if (NBPFILTER != 0)
#  if (__FreeBSD_version >= 500000)
#   define LMC_BPF_MTAP(mbuf)	BPF_MTAP(sc->ifp, mbuf)
#  else  /* FreeBSD-4 */
#   define LMC_BPF_MTAP(mbuf)	if (sc->ifp->if_bpf) bpf_mtap(sc->ifp, mbuf)
#  endif
#  define LMC_BPF_ATTACH(dlt, len) bpfattach(sc->ifp, dlt, len)
#  define LMC_BPF_DETACH	   bpfdetach(sc->ifp)
# endif
# if (__FreeBSD_version >= 500000)
#  define IF_DROP(ifq)		_IF_DROP(ifq)
#  define IF_QFULL(ifq)		_IF_QFULL(ifq)
# endif
# if (__FreeBSD_version < 500000)
#  define INTR_MPSAFE		0
#  define BUS_DMA_COHERENT	0
# endif
# if (__FreeBSD_version >= 600000)
#  define IFF_RUNNING		IFF_DRV_RUNNING
# endif
#endif  /* __FreeBSD__ */

#ifdef __NetBSD__
  typedef int intr_return_t;
# define  READ_PCI_CFG(sc, addr)       pci_conf_read ((sc)->pa_pc, (sc)->pa_tag, addr)
# define WRITE_PCI_CFG(sc, addr, data) pci_conf_write((sc)->pa_pc, (sc)->pa_tag, addr, data)
# define  READ_CSR(csr)		bus_space_read_4 (sc->csr_tag, sc->csr_handle, csr)
# define WRITE_CSR(csr, val)	bus_space_write_4(sc->csr_tag, sc->csr_handle, csr, val)
# define NAME_UNIT		sc->dev.dv_xname
# define DRIVER_DEBUG		((sc->config.debug) || (sc->ifp->if_flags & IFF_DEBUG))
# define TOP_TRYLOCK		simple_lock_try(&sc->top_lock)
# define TOP_UNLOCK		simple_unlock  (&sc->top_lock)
# define BOTTOM_TRYLOCK		simple_lock_try(&sc->bottom_lock)
# define BOTTOM_UNLOCK		simple_unlock  (&sc->bottom_lock)
# define CHECK_CAP		suser(curproc->p_ucred, &curproc->p_acflag)
# define DISABLE_INTR		int spl = splnet()
# define ENABLE_INTR		splx(spl)
# define IRQ_NONE		0
# define IRQ_HANDLED		1
# define IFP2SC(ifp)		(ifp)->if_softc
# define COPY_BREAK		MHLEN
# define SLEEP(usecs)		tsleep(sc, PCATCH | PZERO, DEVICE_NAME, 1+(usecs/tick))
# define DMA_SYNC(map, size, flags) bus_dmamap_sync(ring->tag, map, 0, size, flags)
# define DMA_LOAD(map, addr, size)  bus_dmamap_load(ring->tag, map, addr, size, 0, BUS_DMA_NOWAIT)
# if (NBPFILTER != 0)
#  define LMC_BPF_MTAP(mbuf)	if (sc->ifp->if_bpf) bpf_mtap(sc->ifp->if_bpf, mbuf)
#  define LMC_BPF_ATTACH(dlt, len) bpfattach(sc->ifp, dlt, len)
#  define LMC_BPF_DETACH	   bpfdetach(sc->ifp)
# endif
#endif /* __NetBSD__ */

#ifdef __OpenBSD__
  typedef int intr_return_t;
# define  READ_PCI_CFG(sc, addr)       pci_conf_read ((sc)->pa_pc, (sc)->pa_tag, addr)
# define WRITE_PCI_CFG(sc, addr, data) pci_conf_write((sc)->pa_pc, (sc)->pa_tag, addr, data)
# define  READ_CSR(csr)		bus_space_read_4 (sc->csr_tag, sc->csr_handle, csr)
# define WRITE_CSR(csr, val)	bus_space_write_4(sc->csr_tag, sc->csr_handle, csr, val)
# define NAME_UNIT		sc->dev.dv_xname
# define DRIVER_DEBUG		((sc->config.debug) || (sc->ifp->if_flags & IFF_DEBUG))
# define TOP_TRYLOCK		simple_lock_try(&sc->top_lock)
# define TOP_UNLOCK		simple_unlock  (&sc->top_lock)
# define BOTTOM_TRYLOCK		simple_lock_try(&sc->bottom_lock)
# define BOTTOM_UNLOCK		simple_unlock  (&sc->bottom_lock)
# define CHECK_CAP		suser(curproc, 0)
# define DISABLE_INTR		int spl = splnet()
# define ENABLE_INTR		splx(spl)
# define IRQ_NONE		0
# define IRQ_HANDLED		1
# define IFP2SC(ifp)		(ifp)->if_softc
# define COPY_BREAK		MHLEN
# define SLEEP(usecs)		tsleep(sc, PCATCH | PZERO, DEVICE_NAME, 1+(usecs/tick))
# define DMA_SYNC(map, size, flags) bus_dmamap_sync(ring->tag, map, 0, size, flags)
# define DMA_LOAD(map, addr, size)  bus_dmamap_load(ring->tag, map, addr, size, 0, BUS_DMA_NOWAIT)
# if (NBPFILTER != 0)
#  define LMC_BPF_MTAP(mbuf)	if (sc->ifp->if_bpf) bpf_mtap(sc->ifp->if_bpf, mbuf)
#  define LMC_BPF_ATTACH(dlt, len) bpfattach(&sc->ifp->if_bpf, sc->ifp, dlt, len)
#  define LMC_BPF_DETACH	   bpfdetach(sc->ifp)
# endif
#endif /* __OpenBSD__ */

#ifdef __bsdi__
  typedef int intr_return_t;
# define  READ_PCI_CFG(sc, addr)        pci_inl(&(sc)->cfgbase, addr)
# define WRITE_PCI_CFG(sc, addr, data) pci_outl(&(sc)->cfgbase, addr, data)
# if IOREF_CSR
#  define  READ_CSR(csr)	 inl(sc->csr_iobase+(csr))
#  define WRITE_CSR(csr, val)	outl(sc->csr_iobase+(csr), (val))
# else
# error Memory refs to Tulip CSRs cause page faults in BSD/OS
#  define  READ_CSR(csr)	   (0 + *(sc->csr_membase+(csr)))
#  define WRITE_CSR(csr, val)	((void)(*(sc->csr_membase+(csr)) = (val)))
# endif
# define NAME_UNIT		sc->dev.dv_xname
# define DRIVER_DEBUG		((sc->config.debug) || (sc->ifp->if_flags & IFF_DEBUG))
# define TOP_TRYLOCK		simple_lock_try(&sc->top_lock)
# define TOP_UNLOCK		simple_unlock  (&sc->top_lock)
# define BOTTOM_TRYLOCK		simple_lock_try(&sc->bottom_lock)
# define BOTTOM_UNLOCK		simple_unlock  (&sc->bottom_lock)
# define CHECK_CAP		suser(PCPU(curproc)->p_ucred, &PCPU(curproc)->p_acflag)
# define DISABLE_INTR		int spl = splimp()
# define ENABLE_INTR		splx(spl)
# define IRQ_NONE		1 /* XXX 0 */
# define IRQ_HANDLED		1
# define IFP2SC(ifp)		(ifp)->if_softc
# define COPY_BREAK		MHLEN
# define SLEEP(usecs)		tsleep(sc, PCATCH | PZERO, DEVICE_NAME, 1+(usecs/tick))
# define DMA_SYNC(map, size, flags)   /* nothing */
# define DMA_LOAD(map, addr, size)    0
# define bus_dmamap_unload(tag, map)  /* nothing */
# define bus_dmamap_destroy(tag, map) /* nothing */
# if (NBPFILTER != 0)
#  define LMC_BPF_MTAP(mbuf)	if (sc->ifp->if_bpf) bpf_mtap(sc->ifp->if_bpf, mbuf)
#  define LMC_BPF_ATTACH(dlt, len) bpfattach(&sc->ifp->if_bpf, sc->ifp, dlt, len)
#  define LMC_BPF_DETACH	/* bpfdetach(sc->ifp) */
# endif
# define memcpy(dst, src, len)  bcopy(src, dst, len)
# define if_detach(ifp)		/* nothing */

/*  BSD/OS-4.1 doesn't have a back pointer to softc in struct ifnet, */
/*  and it passes a unit number not a struct ifnet* to watchdog. */
# if (_BSDI_VERSION <= 199910)
   extern struct cfdriver	lmccd;
#  undef  IFP2SC
#  define UNIT2SC(unit)		((softc_t *)lmccd.cd_devs[unit])
#  define IFP2SC(ifp)		(UNIT2SC((ifp)->if_unit))
# endif
#endif /* __bsdi__ */

#ifdef __linux__
static u_int32_t /* inline? so rare it doesn't matter */
READ_PCI_CFG(softc_t *sc, u_int32_t addr)
  {
  u_int32_t data;
  pci_read_config_dword(sc->pci_dev, addr, &data);
  return data;
  }
# define WRITE_PCI_CFG(sc, addr, data) pci_write_config_dword(sc->pci_dev, addr, data)
# if IOREF_CSR
#  define  READ_CSR(csr)	       inl((sc->csr_iobase+(csr)))
#  define WRITE_CSR(csr, val)	outl((val),(sc->csr_iobase+(csr)))
# else
#  define  READ_CSR(csr)	       readl((sc->csr_membase+(csr)))
#  define WRITE_CSR(csr, val)	writel((val),(sc->csr_membase+(csr)))
# endif
# define NAME_UNIT		sc->net_dev->name
# define DRIVER_DEBUG		((sc->config.debug) || (sc->net_dev->flags & IFF_DEBUG))
# define TOP_TRYLOCK		((down_trylock(&sc->top_lock)==0) ? 1:0)
# define TOP_UNLOCK		up(&sc->top_lock)
# define BOTTOM_TRYLOCK		spin_trylock_bh(&sc->bottom_lock)
# define BOTTOM_UNLOCK		spin_unlock_bh(&sc->bottom_lock)
# define CHECK_CAP		capable(CAP_NET_ADMIN)? 0 : -EPERM
# define DISABLE_INTR		/* nothing */
# define ENABLE_INTR		/* nothing */
# define COPY_BREAK		200
# define DELAY(usecs)		udelay(usecs)
# define SLEEP(usecs)		do { set_current_state(TASK_INTERRUPTIBLE);\
				schedule_timeout(1+(usecs*HZ)/1000000UL); } while (0)
# define printf			printk
# define copyin(u, k, len)	copy_from_user(k, u, len)
# define microtime(time)	do_gettimeofday(time)
# define malloc(len, t, f)	kmalloc(len, GFP_KERNEL)
# define free(addr, t)		kfree(addr)
# define LITTLE_ENDIAN		4321
# define BIG_ENDIAN		1234
# if defined(__LITTLE_ENDIAN)
#  define BYTE_ORDER LITTLE_ENDIAN
# elif defined(__BIG_ENDIAN)
#  define BYTE_ORDER BIG_ENDIAN
# else
#  error "asm/byteorder.h is wrong"
# endif
# if (GEN_HDLC == 0)
#  define dev_to_hdlc(net_dev) net_dev
#  define hdlc_set_carrier(val, net_dev) /* nothing */
# endif
#endif /* __linux__ */

#if (NBPFILTER == 0)
# define LMC_BPF_MTAP(mbuf)		/* nothing */
# define LMC_BPF_ATTACH(dlt, len)	/* nothing */
# define LMC_BPF_DETACH			/* nothing */
#endif

#if (defined(__bsdi__) || /* unconditionally */ \
    (defined(__FreeBSD__) && (__FreeBSD_version < 503000)) || \
    (defined(__NetBSD__)  && (__NetBSD_Version__ < 106000000)) || \
    (defined(__OpenBSD__) && (  OpenBSD < 200111)))
# define IFQ_ENQUEUE(ifq, m, pa, err)   \
do {					\
  if (pa==0); /* suppress warning */	\
  if (IF_QFULL(ifq))			\
    {					\
    IF_DROP(ifq);			\
    m_freem(m);				\
    err = ENOBUFS;			\
    }					\
  else					\
    {					\
    IF_ENQUEUE(ifq, m);			\
    err = 0;				\
    }					\
   } while (0)
# define IFQ_DEQUEUE(ifq, m)		do { IF_DEQUEUE((ifq), m) } while (0)
# define IFQ_IS_EMPTY(ifq)		((ifq)->ifq_head == NULL)
# define IFQ_SET_MAXLEN(ifq, len)	(ifq)->ifq_maxlen = len
# define IFQ_SET_READY(ifq)		/* nothing */
# define IFQ_PURGE(ifq)			\
do {					\
  while ((ifq)->ifq_head != NULL)	\
    {					\
    struct mbuf *m;			\
    IF_DEQUEUE(ifq, m);			\
    m_freem(m);				\
    }					\
   } while (0)
#endif

#define HSSI_DESC "SBE/LMC HSSI Card"
#define T3_DESC   "SBE/LMC T3 Card"
#define SSI_DESC  "SBE/LMC SSI Card"
#define T1E1_DESC "SBE/LMC T1E1 Card"

/* procedure prototypes */

static void shift_srom_bits(softc_t *, u_int32_t, u_int32_t);
static u_int16_t read_srom(softc_t *, u_int8_t);
static void write_srom(softc_t *, u_int8_t, u_int16_t);

static u_int8_t read_bios(softc_t *, u_int32_t);
static void write_bios_phys(softc_t *, u_int32_t, u_int8_t);
static void write_bios(softc_t *, u_int32_t, u_int8_t);
static void erase_bios(softc_t *);

static void shift_mii_bits(softc_t *, u_int32_t, u_int32_t);
static u_int16_t read_mii(softc_t *, u_int8_t);
static void write_mii(softc_t *, u_int8_t, u_int16_t);

static void set_mii16_bits(softc_t *, u_int16_t);
static void clr_mii16_bits(softc_t *, u_int16_t);
static void set_mii17_bits(softc_t *, u_int16_t);
static void clr_mii17_bits(softc_t *, u_int16_t);

static void led_off(softc_t *, u_int16_t);
static void led_on(softc_t *, u_int16_t);
static void led_inv(softc_t *, u_int16_t);

static void write_framer(softc_t *, u_int16_t, u_int8_t);
static u_int8_t read_framer(softc_t *, u_int16_t);

static void make_gpio_input(softc_t *, u_int32_t);
static void make_gpio_output(softc_t *, u_int32_t);
static u_int32_t read_gpio(softc_t *);
static void set_gpio_bits(softc_t *, u_int32_t);
static void clr_gpio_bits(softc_t *, u_int32_t);

static void reset_xilinx(softc_t *);
static void  load_xilinx_from_rom(softc_t *);
static int   load_xilinx_from_file(softc_t *, char *, u_int32_t);

static void shift_synth_bits(softc_t *, u_int32_t, u_int32_t);
static void write_synth(softc_t *, struct synth *);

static void write_dac(softc_t *, u_int16_t);

static void hssi_config(softc_t *);
static void hssi_ident(softc_t *);
static int  hssi_watchdog(softc_t *);
static int  hssi_ioctl(softc_t *, struct ioctl *);

static void t3_config(softc_t *);
static void t3_ident(softc_t *);
static int  t3_watchdog(softc_t *);
static void t3_send_dbl_feac(softc_t *, int, int);
static int  t3_ioctl(softc_t *, struct ioctl *);

static void ssi_config(softc_t *);
static void ssi_ident(softc_t *);
static int  ssi_watchdog(softc_t *);
static int  ssi_ioctl(softc_t *, struct ioctl *);

static void t1_config(softc_t *);
static void t1_ident(softc_t *);
static int  t1_watchdog(softc_t *);
static void t1_send_bop(softc_t *, int);
static int  t1_ioctl(softc_t *, struct ioctl *);

#if IFNET
# if ((defined(__FreeBSD__) && (__FreeBSD_version < 500000)) ||\
        defined(__NetBSD__) || defined(__OpenBSD__) || defined(__bsdi__))
static void netisr_dispatch(int, struct mbuf *);
# endif
static void lmc_raw_input(struct ifnet *, struct mbuf *);
#endif /* IFNET */

#if BSD
static void mbuf_enqueue(struct desc_ring *, struct mbuf *);
static struct mbuf* mbuf_dequeue(struct desc_ring *);
# ifdef __FreeBSD__
static void fbsd_dmamap_load(void *, bus_dma_segment_t *, int, int);
# endif
static int create_ring(softc_t *, struct desc_ring *, int);
static void destroy_ring(softc_t *, struct desc_ring *);
static int rxintr_cleanup(softc_t *);
static int rxintr_setup(softc_t *);
static int txintr_cleanup(softc_t *);
static int txintr_setup_mbuf(softc_t *, struct mbuf *);
static int txintr_setup(softc_t *);
#endif /* BSD */

#ifdef __linux__
static void skbuff_enqueue(struct desc_ring *, struct sk_buff *);
static struct sk_buff* skbuff_dequeue(struct desc_ring *);
static int create_ring(softc_t *, struct desc_ring *, int);
static void destroy_ring(softc_t *, struct desc_ring *);
static int rxintr_cleanup(softc_t *);
static int rxintr_setup(softc_t *);
static int txintr_cleanup(softc_t *sc);
static int txintr_setup_frag(softc_t *, char *, int);
static int txintr_setup_skb(softc_t *, struct sk_buff *);
static int txintr_setup(softc_t *);
#endif /* __linux__ */

static void check_intr_status(softc_t *);
static void core_interrupt(void *, int);
static void user_interrupt(softc_t *, int);
#if BSD
# if (defined(__FreeBSD__) && defined(DEVICE_POLLING))
static int fbsd_poll(struct ifnet *, enum poll_cmd, int);
# endif
static intr_return_t bsd_interrupt(void *);
#endif /* BSD */

static void set_status(softc_t *, int);
#if P2P
static int p2p_getmdm(struct p2pcom *, caddr_t);
static int p2p_mdmctl(struct p2pcom *, int);
#endif
#if NSPPP
static void sppp_tls(struct sppp *);
static void sppp_tlf(struct sppp *);
#endif

static void config_proto(softc_t *, struct config *);
static int core_ioctl(softc_t *, u_long, caddr_t);
static void core_watchdog(softc_t *);

#if IFNET
static int lmc_raw_ioctl(struct ifnet *, u_long, caddr_t);
static int lmc_ifnet_ioctl(struct ifnet *, u_long, caddr_t);
static void lmc_ifnet_start(struct ifnet *);
static int lmc_raw_output(struct ifnet *, struct mbuf *,
 struct sockaddr *, struct route *);
# ifdef __OpenBSD__
static int ifmedia_change(struct ifnet *);
static void ifmedia_status(struct ifnet *, struct ifmediareq *);
# endif /* __OpenBSD__ */
static void setup_ifnet(struct ifnet *);
static int lmc_ifnet_attach(softc_t *);
static void lmc_ifnet_detach(softc_t *);
#endif /* IFNET */

#if NETGRAPH
# if (__FreeBSD_version >= 500000)
static int ng_constructor(node_p);
# else /* FreeBSD-4 */
static int ng_constructor(node_p *);
# endif
# if (__FreeBSD_version >= 500000)
static int ng_rcvmsg(node_p, item_p, hook_p);
# else /* FreeBSD-4 */
static int ng_rcvmsg(node_p, struct ng_mesg *,
 const char *,  struct ng_mesg **);
# endif
static int ng_shutdown(node_p);
static int ng_newhook(node_p, hook_p, const char *);
static int ng_connect(hook_p);
# if (__FreeBSD_version >= 500000)
static int ng_rcvdata(hook_p, item_p);
# else /* FreeBSD-4 */
static int ng_rcvdata(hook_p, struct mbuf *, meta_p);
# endif
static int ng_disconnect(hook_p);
# if (IFNET == 0)
static void ng_watchdog(void *);
# endif
static int ng_attach(softc_t *);
static void ng_detach(softc_t *);
#endif /* NETGRAPH */

static int startup_card(softc_t *);
static void shutdown_card(void *);
static int attach_card(softc_t *, const char *);
static void detach_card(softc_t *);

#ifdef __FreeBSD__
static int fbsd_probe(device_t);
static int fbsd_detach(device_t);
static int fbsd_shutdown(device_t);
static int fbsd_attach(device_t);
#endif /* __FreeBSD__ */

#ifdef __NetBSD__
static int nbsd_match(struct device *t, struct cfdata *, void *);
static int nbsd_detach(struct device *, int);
static void nbsd_attach(struct device *, struct device *, void *);
static int lkm_nbsd_match(struct pci_attach_args *);
int if_lmc_lkmentry(struct lkm_table *, int, int);
#endif  /* __NetBSD__ */

#ifdef __OpenBSD__
static int obsd_match(struct device *, void *, void *);
static int obsd_detach(struct device *, int);
static void obsd_attach(struct device *, struct device *, void *);
int if_lmc_lkmentry(struct lkm_table *, int, int);
#endif  /* __OpenBSD__ */

#ifdef __bsdi__
static int bsdi_match(pci_devaddr_t *);
static int bsdi_probe(struct device *, struct cfdata *, void *);
static void bsdi_attach(struct device *, struct device *, void *);
#endif  /* __bsdi__ */

#ifdef __linux__
static irqreturn_t linux_interrupt(int, void *, struct pt_regs *);
static int linux_poll(struct net_device *, int *);
static int linux_start(struct sk_buff *, struct net_device *);
static void linux_timeout(struct net_device *);
static int linux_ioctl(struct net_device *, struct ifreq *, int);
static struct net_device_stats * linux_stats(struct net_device *);
static void linux_watchdog(unsigned long);
static int linux_stop(struct net_device *);
static int linux_open(struct net_device *);
# if GEN_HDLC
static int hdlc_attach(struct net_device *,
 unsigned short, unsigned short);
# endif
static void __exit linux_remove(struct pci_dev *);
static void setup_netdev(struct net_device *);
static int __init linux_probe(struct pci_dev *, const struct pci_device_id *);
#endif /* __linux__ */

#endif /* KERNEL */

#endif /* IF_LMC_H */
