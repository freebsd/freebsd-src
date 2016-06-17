/* -*- linux-c -*- */
/* 
 * Copyright (C) 2001 By Joachim Martillo, Telford Tools, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 **/

#ifndef _SABIOCTL_H_
#define _SABIOCTL_H_

#include <linux/tty.h>
#include <asm/ioctl.h>
#include <linux/sockios.h>
#include "ring.h"
#include "Reg9050.h"

/* Channel Configuration Register 0 (CCR0) */
#define SAB82532_CCR0_PU		0x80
#define SAB82532_CCR0_MCE		0x40
#define SAB82532_CCR0_SC_NRZ		0x00
#define SAB82532_CCR0_SC_NRZI		0x08
#define SAB82532_CCR0_SC_FM0		0x10
#define SAB82532_CCR0_SC_FM1		0x14
#define SAB82532_CCR0_SC_MANCH		0x18
#define SAB82532_CCR0_SM_HDLC		0x00
#define SAB82532_CCR0_SM_SDLC_LOOP	0x01
#define SAB82532_CCR0_SM_BISYNC		0x02
#define SAB82532_CCR0_SM_ASYNC		0x03

/* Channel Configuration Register 1 (CCR1) */
#define SAB82532_CCR1_SFLG		0x80
#define SAB82532_CCR1_ODS		0x10
#define SAB82532_CCR1_BCR		0x08
#define SAB82532_CCR1_IFF		0x08
#define SAB82532_CCR1_ITF		0x00
#define SAB82532_CCR1_CM_MASK		0x07

/* Channel Configuration Register 2 (CCR2) */
#define SAB82532_CCR2_SOC1		0x80
#define SAB82532_CCR2_SOC0		0x40
#define SAB82532_CCR2_BR9		0x80
#define SAB82532_CCR2_BR8		0x40
#define SAB82532_CCR2_BDF		0x20
#define SAB82532_CCR2_SSEL		0x10
#define SAB82532_CCR2_XCS0		0x20
#define SAB82532_CCR2_RCS0		0x10
#define SAB82532_CCR2_TOE		0x08
#define SAB82532_CCR2_RWX		0x04
#define SAB82532_CCR2_C32		0x02
#define SAB82532_CCR2_DIV		0x01

/* Channel Configuration Register 3 (CCR3) */
#define SAB82532_CCR3_PSD		0x01
#define SAB82532_CCR3_RCRC		0x04

/* Channel Configuration Register 4 (CCR4) */
#define SAB82532_CCR4_MCK4		0x80
#define SAB82532_CCR4_EBRG		0x40
#define SAB82532_CCR4_TST1		0x20
#define SAB82532_CCR4_ICD		0x10
#define SAB82532_CCR4_RF32		0x00
#define SAB82532_CCR4_RF16		0x01
#define SAB82532_CCR4_RF04		0x02
#define SAB82532_CCR4_RF02		0x03

/* Mode Register (MODE) */
#define SAB82532_MODE_TM0		0x80
#define SAB82532_MODE_FRTS		0x40
#define SAB82532_MODE_FCTS		0x20
#define SAB82532_MODE_FLON		0x10
#define SAB82532_MODE_TCPU		0x10
#define SAB82532_MODE_RAC		0x08
#define SAB82532_MODE_RTS		0x04
#define SAB82532_MODE_TRS		0x02
#define SAB82532_MODE_TLP		0x01

struct channelcontrol
{
	unsigned char ccr0;
	unsigned char ccr1;
	unsigned char ccr2;
	unsigned char ccr3;
	unsigned char ccr4;
	unsigned char mode;
	unsigned char rlcr;
};

struct sep9050
{
	unsigned short values[EPROM9050_SIZE];
};

				/* EXTERNAL-CLOCKING */
#define DEFAULT_CCR0 (SAB82532_CCR0_MCE | SAB82532_CCR0_SC_NRZ | SAB82532_CCR0_SM_HDLC)
#define DEFAULT_CCR1 (SAB82532_CCR1_SFLG | SAB82532_CCR1_ODS | SAB82532_CCR1_IFF) /* clock mode 0 */
#define DEFAULT_CCR2 0 /*SAB82532_CCR2_SOC1*/		/* 0a -- RTS high*/
#define DEFAULT_CCR3 SAB82532_CCR3_RCRC
#define DEFAULT_CCR4 0
#define DEFAULT_MODE (SAB82532_MODE_TM0 | SAB82532_MODE_RTS | SAB82532_MODE_RAC)
#define DEFAULT_RLCR ((RXSIZE/32) - 1)

#define DEFAULT_RLCR_NET ((RXSIZE/32) - 1)

				/* Internal-Clocking */

#define DCE_CCR0 (SAB82532_CCR0_MCE | SAB82532_CCR0_SC_NRZ | SAB82532_CCR0_SM_HDLC)
#define DCE_CCR1 (SAB82532_CCR1_SFLG | SAB82532_CCR1_ODS | SAB82532_CCR1_IFF | 6) /* clock mode 6 */
#define DCE_CCR2 (SAB82532_CCR2_BDF | SAB82532_CCR2_SSEL | SAB82532_CCR2_TOE) /* 6b */
#define DCE_CCR3 (SAB82532_CCR3_RCRC)
#define DCE_CCR4 (SAB82532_CCR4_MCK4|SAB82532_CCR4_EBRG)
#define DCE_MODE SAB82532_MODE_TM0 | SAB82532_MODE_RTS | SAB82532_MODE_RAC
#define DCE_RLCR ((RXSIZE/32) - 1)

#define ATIS_MAGIC_IOC	'A'
#define ATIS_IOCSPARAMS		_IOW(ATIS_MAGIC_IOC,0,struct channelcontrol)
#define ATIS_IOCGPARAMS		_IOR(ATIS_MAGIC_IOC,1,struct channelcontrol)
#define ATIS_IOCSSPEED		_IOW(ATIS_MAGIC_IOC,2,unsigned long)
#define ATIS_IOCGSPEED		_IOR(ATIS_MAGIC_IOC,3,unsigned long)
#define ATIS_IOCSSEP9050	_IOW(ATIS_MAGIC_IOC,4,struct sep9050)
#define ATIS_IOCGSEP9050	_IOR(ATIS_MAGIC_IOC,5,struct sep9050)
#define ATIS_IOCSSIGMODE	_IOW(ATIS_MAGIC_IOC,6,unsigned int)
#define ATIS_IOCGSIGMODE	_IOW(ATIS_MAGIC_IOC,7,unsigned int)

/* same order as the bytes in sp502.h and as the names in 8253xtty.c */

#define SP502_OFF_MODE		0
#define SP502_RS232_MODE	1
#define SP502_V28_MODE		SP502_RS232_MODE
#define SP502_RS422_MODE	2
#define SP502_V11_MODE		SP502_RS422_MODE
#define SP502_X27_MODE		SP502_RS422_MODE
#define SP502_RS485_MODE	3
#define SP502_RS449_MODE	4
#define SP502_EIA530_MODE	5
#define SP502_V35_MODE		6

#define SAB8253XCLEARCOUNTERS 	(SIOCDEVPRIVATE + 5 + 1)

#endif
