/* $NetBSD: aureg.h,v 1.18 2006/10/02 06:44:00 gdamore Exp $ */

/*
 * Copyright 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Simon Burge for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_MIPS_ALCHEMY_AUREG_H
#define	_MIPS_ALCHEMY_AUREG_H

/************************************************************************/
/********************   AC97 Controller registers   *********************/
/************************************************************************/
#define	AC97_BASE		0x10000000

/************************************************************************/
/***********************   USB Host registers   *************************/
/************************************************************************/
#define	USBH_BASE		0x10100000
#define	AU1550_USBH_BASE	0x14020000

#define	USBH_ENABLE		0x7fffc
#define	USBH_SIZE		0x100000

#define	AU1550_USBH_ENABLE	0x7ffc
#define AU1550_USBH_SIZE	0x60000

/************************************************************************/
/**********************   USB Device registers   ************************/
/************************************************************************/
#define	USBD_BASE		0x10200000

/************************************************************************/
/*************************   IRDA registers   ***************************/
/************************************************************************/
#define	IRDA_BASE		0x10300000

/************************************************************************/
/******************   Interrupt Controller registers   ******************/
/************************************************************************/

#define	IC0_BASE		0x10400000
#define	IC1_BASE		0x11800000

/*
 * The *_READ registers read the current value of the register
 * The *_SET registers set to 1 all bits that are written 1
 * The *_CLEAR registers clear to zero all bits that are written as 1
 */
#define	IC_CONFIG0_READ			0x40	/* See table below */
#define	IC_CONFIG0_SET			0x40
#define	IC_CONFIG0_CLEAR		0x44

#define	IC_CONFIG1_READ			0x48	/* See table below */
#define	IC_CONFIG1_SET			0x48
#define	IC_CONFIG1_CLEAR		0x4c

#define	IC_CONFIG2_READ			0x50	/* See table below */
#define	IC_CONFIG2_SET			0x50
#define	IC_CONFIG2_CLEAR		0x54

#define	IC_REQUEST0_INT			0x54	/* Show active interrupts on request 0 */

#define	IC_SOURCE_READ			0x58	/* Interrupt source */
#define	IC_SOURCE_SET			0x58	/*  0 - test bit used as source */
#define	IC_SOURCE_CLEAR			0x5c	/*  1 - peripheral/GPIO used as source */

#define	IC_REQUEST1_INT			0x5c	/* Show active interrupts on request 1 */

#define	IC_ASSIGN_REQUEST_READ		0x60	/* Assigns the interrupt to one of the */
#define	IC_ASSIGN_REQUEST_SET		0x60	/* CPU requests (0 - assign to request 1, */
#define	IC_ASSIGN_REQUEST_CLEAR		0x64	/* 1 - assign to request 0) */

#define	IC_WAKEUP_READ			0x68	/* Controls whether the interrupt can */
#define	IC_WAKEUP_SET			0x68	/* cause a wakeup from IDLE */
#define	IC_WAKEUP_CLEAR			0x6c

#define	IC_MASK_READ			0x70	/* Enables/Disables the interrupt */
#define	IC_MASK_SET			0x70
#define	IC_MASK_CLEAR			0x74

#define	IC_RISING_EDGE			0x78	/* Check/clear rising edge */

#define	IC_FALLING_EDGE			0x7c	/* Check/clear falling edge */

#define	IC_TEST_BIT			0x80	/* single bit source select */

/*
 *	Interrupt Configuration Register Functions
 *
 *	Cfg2[n]	Cfg1[n]	Cfg0[n]		Function
 *	   0	   0	   0		Interrupts Disabled
 *	   0	   0	   1		Rising Edge Enabled
 *	   0	   1	   0		Falling Edge Enabled
 *	   0	   1	   1		Rising and Falling Edge Enabled
 *	   1	   0	   0		Interrupts Disabled
 *	   1	   0	   1		High Level Enabled
 *	   1	   1	   0		Low Level Enabled
 *	   1	   1	   1		Both Levels and Both Edges Enabled
 */

/************************************************************************/
/*************   Programable Serial Controller registers   **************/
/************************************************************************/

#define	PSC0_BASE		0x11A00000
#define	PSC1_BASE		0x11B00000
#define	PSC2_BASE		0x10A00000
#define	PSC3_BASE		0x10B00000


/************************************************************************/
/**********************   Ethernet MAC registers   **********************/
/************************************************************************/

#define	MAC0_BASE		0x10500000
#define	MAC1_BASE		0x10510000
#define	MACx_SIZE		0x28

#define	AU1500_MAC0_BASE	0x11500000	/* Grr, different on Au1500 */
#define	AU1500_MAC1_BASE	0x11510000	/* Grr, different on Au1500 */

#define	MAC0_ENABLE		0x10520000
#define	MAC1_ENABLE		0x10520004
#define	MACENx_SIZE		0x04

#define	AU1500_MAC0_ENABLE	0x11520000	/* Grr, different on Au1500 */
#define	AU1500_MAC1_ENABLE	0x11520004	/* Grr, different on Au1500 */

#define	MAC0_DMA_BASE		0x14004000
#define	MAC1_DMA_BASE		0x14004200
#define	MACx_DMA_SIZE		0x140

/************************************************************************/
/**********************   Static Bus registers   ************************/
/************************************************************************/
#define	STATIC_BUS_BASE		0x14001000

/************************************************************************/
/********************   Secure Digital registers   **********************/
/************************************************************************/
#define	SD0_BASE		0x10600000
#define	SD1_BASE		0x10680000

/************************************************************************/
/*************************   I^2S registers   ***************************/
/************************************************************************/
#define	I2S_BASE		0x11000000

/************************************************************************/
/**************************   UART registers   **************************/
/************************************************************************/

#define	UART0_BASE		0x11100000
#define	UART1_BASE		0x11200000
#define	UART2_BASE		0x11300000
#define	UART3_BASE		0x11400000

/************************************************************************/
/*************************   SSI registers   ****************************/
/************************************************************************/
#define	SSI0_BASE		0x11600000
#define	SSI1_BASE		0x11680000

/************************************************************************/
/************************   GPIO2 registers   ***************************/
/************************************************************************/
#define	GPIO_BASE		0x11900100

/************************************************************************/
/************************   GPIO2 registers   ***************************/
/************************************************************************/
#define	GPIO2_BASE		0x11700000

/************************************************************************/
/*************************   PCI registers   ****************************/
/************************************************************************/
#define	PCI_BASE		0x14005000
#define	PCI_HEADER		0x14005100
#define	PCI_MEM_BASE		0x400000000ULL
#define	PCI_IO_BASE		0x500000000ULL
#define	PCI_CONFIG_BASE		0x600000000ULL

/************************************************************************/
/***********************   PCMCIA registers   ***************************/
/************************************************************************/
#define	PCMCIA_BASE		0xF00000000ULL

/************************************************************************/
/******************   Programmable Counter registers   ******************/
/************************************************************************/

#define	SYS_BASE		0x11900000

#define	PC_BASE			SYS_BASE

#define	PC_TRIM0		0x00		/* PC0 Divide (16 bits) */
#define	PC_COUNTER_WRITE0	0x04		/* set PC0 */
#define	PC_MATCH0_0		0x08		/* match counter & interrupt */
#define	PC_MATCH1_0		0x0c		/* match counter & interrupt */
#define	PC_MATCH2_0		0x10		/* match counter & interrupt */
#define	PC_COUNTER_CONTROL	0x14		/* Programmable Counter Control */
#define	  CC_E1S		  0x00800000	/* Enable PC1 write status */
#define	  CC_T1S		  0x00100000	/* Trim PC1 write status */
#define	  CC_M21		  0x00080000	/* Match 2 of PC1 write status */
#define	  CC_M11		  0x00040000	/* Match 1 of PC1 write status */
#define	  CC_M01		  0x00020000	/* Match 0 of PC1 write status */
#define	  CC_C1S		  0x00010000	/* PC1 write status */
#define	  CC_BP			  0x00004000	/* Bypass OSC (use GPIO1) */
#define	  CC_EN1		  0x00002000	/* Enable PC1 */
#define	  CC_BT1		  0x00001000	/* Bypass Trim on PC1 */
#define	  CC_EN0		  0x00000800	/* Enable PC0 */
#define	  CC_BT0		  0x00000400	/* Bypass Trim on PC0 */
#define	  CC_EO			  0x00000100	/* Enable Oscillator */
#define	  CC_E0S		  0x00000080	/* Enable PC0 write status */
#define	  CC_32S		  0x00000020	/* 32.768kHz OSC status */
#define	  CC_T0S		  0x00000010	/* Trim PC0 write status */
#define	  CC_M20		  0x00000008	/* Match 2 of PC0 write status */
#define	  CC_M10		  0x00000004	/* Match 1 of PC0 write status */
#define	  CC_M00		  0x00000002	/* Match 0 of PC0 write status */
#define	  CC_C0S		  0x00000001	/* PC0 write status */
#define	PC_COUNTER_READ_0	0x40		/* get PC0 */
#define	PC_TRIM1		0x44		/* PC1 Divide (16 bits) */
#define	PC_COUNTER_WRITE1	0x48		/* set PC1 */
#define	PC_MATCH0_1		0x4c		/* match counter & interrupt */
#define	PC_MATCH1_1		0x50		/* match counter & interrupt */
#define	PC_MATCH2_1		0x54		/* match counter & interrupt */
#define	PC_COUNTER_READ_1	0x58		/* get PC1 */

#define	PC_SIZE			0x5c		/* size of register set */
#define	PC_RATE			32768		/* counter rate is 32.768kHz */

/************************************************************************/
/*******************   Frequency Generator Registers   ******************/
/************************************************************************/

#define SYS_FREQCTRL0		(SYS_BASE + 0x20)
#define SFC_FRDIV2(f)		(f<<22)		/* 29:22. Freq Divider 2 */
#define SFC_FE2			(1<<21)		/* Freq generator output enable 2 */
#define SFC_FS2			(1<<20)		/* Freq generator source 2 */
#define SFC_FRDIV1(f)		(f<<12)		/* 19:12. Freq Divider 1 */
#define SFC_FE1			(1<<11)		/* Freq generator output enable 1 */
#define SFC_FS1			(1<<10)		/* Freq generator source 1 */
#define SFC_FRDIV0(f)		(f<<2)		/* 9:2. Freq Divider 0 */
#define SFC_FE0			2		/* Freq generator output enable 0 */
#define SFC_FS0			1		/* Freq generator source 0 */

#define SYS_FREQCTRL1		(SYS_BASE + 0x24)
#define SFC_FRDIV5(f)		(f<<22)		/* 29:22. Freq Divider 5 */
#define SFC_FE5			(1<<21)		/* Freq generator output enable 5 */
#define SFC_FS5			(1<<20)		/* Freq generator source 5 */
#define SFC_FRDIV4(f)		(f<<12)		/* 19:12. Freq Divider 4 */
#define SFC_FE4			(1<<11)		/* Freq generator output enable 4 */
#define SFC_FS4			(1<<10)		/* Freq generator source 4 */
#define SFC_FRDIV3(f)		(f<<2)		/* 9:2. Freq Divider 3 */
#define SFC_FE3			2		/* Freq generator output enable 3 */
#define SFC_FS3			1		/* Freq generator source 3 */

/************************************************************************/
/******************   Clock Source Control Registers   ******************/
/************************************************************************/

#define SYS_CLKSRC		(SYS_BASE + 0x28)
#define  SCS_ME1(n)		(n<<27)		/* EXTCLK1 Clock Mux input select */
#define  SCS_ME0(n)		(n<<22)		/* EXTCLK0 Clock Mux input select */
#define  SCS_MPC(n)		(n<<17)		/* PCI clock mux input select */
#define  SCS_MUH(n)		(n<<12)		/* USB Host clock mux input select */
#define  SCS_MUD(n)		(n<<7)		/* USB Device clock mux input select */
#define   SCS_MEx_AUX		0x1		/* Aux clock */
#define   SCS_MEx_FREQ0		0x2		/* FREQ0 */
#define   SCS_MEx_FREQ1		0x3		/* FREQ1 */
#define   SCS_MEx_FREQ2		0x4		/* FREQ2 */
#define   SCS_MEx_FREQ3		0x5		/* FREQ3 */
#define   SCS_MEx_FREQ4		0x6		/* FREQ4 */
#define   SCS_MEx_FREQ5		0x7		/* FREQ5 */
#define  SCS_DE1		(1<<26)		/* EXTCLK1 clock divider select */
#define  SCS_CE1		(1<<25)		/* EXTCLK1 clock select */
#define  SCS_DE0		(1<<21)		/* EXTCLK0 clock divider select */
#define  SCS_CE0		(1<<20)		/* EXTCLK0 clock select */
#define  SCS_DPC		(1<<16)		/* PCI clock divider select */
#define  SCS_CPC		(1<<15)		/* PCI clock select */
#define  SCS_DUH		(1<<11)		/* USB Host clock divider select */
#define  SCS_CUH		(1<<10)		/* USB Host clock select */
#define  SCS_DUD		(1<<6)		/* USB Device clock divider select */
#define  SCS_CUD		(1<<5)		/* USB Device clock select */
/*
 * Au1550 bits, needed for PSCs. Note that some bits collide with
 * earlier parts.  On Au1550, USB clocks (both device and host) are
 * shared with PSC2, and must be configured for 48MHz.  DBAU1550 YAMON
 * does this by default.  Also, EXTCLK0 is shared with PSC3.  DBAU1550
 * YAMON does not configure any clocks besides PSC2.
 */
#define  SCS_MP3(n)		(n<<22)		/* psc3_intclock mux */
#define	 SCS_DP3		(1<<21)		/* psc3_intclock divider */
#define	 SCS_CP3		(1<<20)		/* psc3_intclock select */
#define  SCS_MP1(n)		(n<<12)		/* psc1_intclock mux */
#define	 SCS_DP1		(1<<11)		/* psc1_intclock divider */
#define	 SCS_CP1		(1<<10)		/* psc1_intclock select */
#define	 SCS_MP0(n)		(n<<7)		/* psc0_intclock mux */
#define  SCS_DP0		(1<<6)		/* psc0_intclock divider */
#define	 SCS_CP0		(1<<5)		/* psc0_intclock seelct */
#define	 SCS_MP2(n)		(n<<2)		/* psc2_intclock mux */
#define	 SCS_DP2		(1<<1)		/* psc2_intclock divider */
#define	 SCS_CP2		(1<<0)		/* psc2_intclock select */

/************************************************************************/
/***************************  PIN Function  *****************************/
/************************************************************************/

#define	SYS_PINFUNC		(SYS_BASE + 0x2c)
#define	 SPF_PSC3_MASK		(7<<20)
#define	 SPF_PSC3_AC97		(0<<17)		/* select AC97/SPI */
#define	 SPF_PSC3_I2S		(1<<17)		/* select I2S */
#define	 SPF_PSC3_SMBUS		(3<<17)		/* select SMbus */
#define	 SPF_PSC3_GPIO		(7<<17)		/* select gpio215:211 */
#define  SPF_PSC2_MASK		(7<<17)
#define	 SPF_PSC2_AC97		(0<<17)		/* select AC97/SPI */
#define	 SPF_PSC2_I2S		(1<<17)		/* select I2S */
#define	 SPF_PSC2_SMBUS		(3<<17)		/* select SMbus */
#define	 SPF_PSC2_GPIO		(7<<17)		/* select gpio210:206*/
#define	 SPF_CS			(1<<16)		/* extclk0 or 32kHz osc */
#define	 SPF_USB		(1<<15)		/* host or device usb otg */
#define	 SPF_U3T		(1<<14)		/* uart3 tx or gpio23 */
#define	 SPF_U1R		(1<<13)		/* uart1 rx or gpio22 */
#define	 SPF_U1T		(1<<12)		/* uart1 tx or gpio21 */
#define	 SPF_EX1		(1<<10)		/* gpio3 or extclk1 */
#define	 SPF_EX0		(1<<9)		/* gpio2 or extclk0/32kHz osc*/
#define	 SPF_U3			(1<<7)		/* gpio14:9 or uart3 */
#define	 SPF_MBSa		(1<<5)		/* must be set */
#define	 SPF_NI2		(1<<4)		/* enet1 or gpio28:24 */
#define	 SPF_U0			(1<<3)		/* uart0 or gpio20 */
#define	 SPF_MBSb		(1<<2)		/* must be set */
#define	 SPF_S1			(1<<1)		/* gpio17 or psc1_sync1 */
#define	 SPF_S0			(1<<0)		/* gpio16 or psc0_sync1 */

/************************************************************************/
/***************************   PLL Control  *****************************/
/************************************************************************/

#define SYS_CPUPLL		(SYS_BASE + 0x60)
#define SYS_AUXPLL              (SYS_BASE + 0x64)

#endif	/* _MIPS_ALCHEMY_AUREG_H */
