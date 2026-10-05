/*-
 * Copyright (c) 2026 Ruslan Bukin <br@bsdpad.com>
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

#ifndef _DEV_DWC_DWC4_MTL_H_
#define _DEV_DWC_DWC4_MTL_H_

#define	ETH_MTLOMR		0x0C00
#define	ETH_MTLISR		0x0C20
#define	ETH_MTLRXQDMAMR		0x0C30
#define	ETH_MTLTBSCR		0x0C40
#define	ETH_MTLESTCR		0x0C50
#define	ETH_MTLESTECR		0x0C54
#define	ETH_MTLESTSR		0x0C58
#define	ETH_MTLESTSCHER		0x0C60
#define	ETH_MTLESTFSER		0x0C64
#define	ETH_MTLESTFSCR		0x0C68
#define	ETH_MTLESTIER		0x0C70
#define	ETH_MTLESTGCLCR		0x0C80
#define	ETH_MTLESTGCLDR		0x0C84
#define	ETH_MTLFPECSR		0x0C90
#define	ETH_MTLFPEAR		0x0C94

#define	ETH_MTLTXQOMR(x)	(0x0D00 + 0x40 * (x))
#define	 MTLTXQOMR_TQS_S	16 /* Transmit queue size */
#define	 MTLTXQOMR_TQS_M	(0x1f << MTLTXQOMR_TQS_S)
#define	 MTLTXQOMR_TQS_8K	(0x1f << MTLTXQOMR_TQS_S)
#define	 MTLTXQOMR_TXQEN_S	2 /* Transmit Queue Enable */
#define	 MTLTXQOMR_TXQEN_M	(0x3 << MTLTXQOMR_TXQEN_S)
#define	 MTLTXQOMR_EN_AV_MODE	(0x1 << MTLTXQOMR_TXQEN_S)
#define	 MTLTXQOMR_EN		(0x2 << MTLTXQOMR_TXQEN_S)
#define	 MTLTXQOMR_TSF		(1 << 1) /* Transmit Store and Forward */
#define	 MTLTXQOMR_FTQ		(1 << 0) /* Flush Transmit Queue */
#define	ETH_MTLTXQUR(x)		(0x0D04 + 0x40 * (x))
#define	ETH_MTLTXQDR(x)		(0x0D08 + 0x40 * (x))
#define	ETH_MTLTXQESR(x)	(0x0D14 + 0x40 * (x))
#define	ETH_MTLTXQQWR(x)	(0x0D18 + 0x40 * (x))
#define	ETH_MTLQICSR(x)		(0x0D2C + 0x40 * (x))
#define	ETH_MTLRXQOMR(x)	(0x0D30 + 0x40 * (x))
#define	 MTLRXQOMR_RQS_S	20 /* Receive Queue Size */
#define	 MTLRXQOMR_RQS_M	(0xf << MTLRXQOMR_RQS_S)
#define	 MTLRXQOMR_RQS_4K	(0xf << MTLRXQOMR_RQS_S)
#define	 MTLRXQOMR_EHFC		(1 << 7) /* Enable Hardware Flow Control */
#define	 MTLRXQOMR_RSF		(1 << 5) /* Receive Queue Store and Forward */
#define	 MTLRXQOMR_RTC_S	0 /* Receive Queue Threshold Control */
#define	 MTLRXQOMR_RTC_M	(0x3 << MTLRXQOMR_RTC_S)
#define	 MTLRXQOMR_RTC_64	(0x0 << MTLRXQOMR_RTC_S)
#define	 MTLRXQOMR_RTC_32	(0x1 << MTLRXQOMR_RTC_S)
#define	 MTLRXQOMR_RTC_96	(0x2 << MTLRXQOMR_RTC_S)
#define	 MTLRXQOMR_RTC_128	(0x3 << MTLRXQOMR_RTC_S)
#define	ETH_MTLRXQMPOCR(x)	(0x0D34 + 0x40 * (x))
#define	ETH_MTLRXQDR(x)		(0x0D38 + 0x40 * (x))
#define	ETH_MTLRXQCR(x)		(0x0D3C + 0x40 * (x))

#define	ETH_MTLTXQ0OMR		0x0D00
#define	ETH_MTLTXQ0UR		0x0D04
#define	ETH_MTLTXQ0DR		0x0D08
#define	ETH_MTLTXQ0ESR		0x0D14
#define	ETH_MTLTXQ0QWR		0x0D18
#define	ETH_MTLQ0ICSR		0x0D2C
#define	ETH_MTLRXQ0OMR		0x0D30
#define	ETH_MTLRXQ0MPOCR	0x0D34
#define	ETH_MTLRXQ0DR		0x0D38
#define	ETH_MTLRXQ0CR		0x0D3C

#define	ETH_MTLTXQ1OMR		0x0D40
#define	ETH_MTLTXQ1UR		0x0D44
#define	ETH_MTLTXQ1DR		0x0D48
#define	ETH_MTLTXQ1ECR		0x0D50
#define	ETH_MTLTXQ1ESR		0x0D54
#define	ETH_MTLTXQ1QWR		0x0D58
#define	ETH_MTLTXQ1SSCR		0x0D5C
#define	ETH_MTLTXQ1HCR		0x0D60
#define	ETH_MTLTXQ1LCR		0x0D64
#define	ETH_MTLQ1ICSR		0x0D6C
#define	ETH_MTLRXQ1OMR		0x0D70
#define	ETH_MTLRXQ1MPOCR	0x0D74
#define	ETH_MTLRXQ1DR		0x0D78
#define	ETH_MTLRXQ1CR		0x0D7C

#define	ETH_MTLTXQ2OMR		0x0D80
#define	ETH_MTLTXQ2UR		0x0D84
#define	ETH_MTLTXQ2DR		0x0D88
#define	ETH_MTLTXQ2ECR		0x0D90
#define	ETH_MTLTXQ2ESR		0x0D94
#define	ETH_MTLTXQ2QWR		0x0D98
#define	ETH_MTLTXQ2SSCR		0x0D9C
#define	ETH_MTLTXQ2HCR		0x0DA0
#define	ETH_MTLTXQ2LCR		0x0DA4
#define	ETH_MTLQ2ICSR		0x0DAC

#define	ETH_MTLTXQ3OMR		0x0DC0
#define	ETH_MTLTXQ3UR		0x0DC4
#define	ETH_MTLTXQ3DR		0x0DC8
#define	ETH_MTLTXQ3ECR		0x0DD0
#define	ETH_MTLTXQ3ESR		0x0DD4
#define	ETH_MTLTXQ3QWR		0x0DD8
#define	ETH_MTLTXQ3SSCR		0x0DDC
#define	ETH_MTLTXQ3HCR		0x0DE0
#define	ETH_MTLTXQ3LCR		0x0DE4
#define	ETH_MTLQ3ICSR		0x0DEC

#endif	/* _DEV_DWC_DWC4_MTL_H_ */
