/*
 * cyclades cyclom-y serial driver
 *	Andrew Herbert <andrew@werple.apana.org.au>, 17 August 1993
 *
 * Copyright (c) 1993 Andrew Herbert.
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
 * 3. The name Andrew Herbert may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL I BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define CD1400_NO_OF_CHANNELS	4	/* four serial channels per chip */
#define CD1400_FIFOSIZE		12	/* 12 chars */

/* register definitions */

#define CD1400_CCR		2*0x05	/* channel control */
#define CD1400_CMD_RESET		0x81	/* full reset */

#define CD1400_SRER		2*0x06	/* service request enable */

#define CD1400_GFRCR		2*0x40	/* global firmware revision code */

#define CD1400_LIVR		2*0x18	/* local intr vector */
#define CD1400_MIVR		2*0x41	/* modem intr vector */
#define CD1400_TIVR		2*0x42	/* transmit intr vector */
#define CD1400_RIVR		2*0x43	/* receive intr vector */
#define CD1400_RIVR_EXCEPTION		(1<<2)	/* receive exception bit */

#define CD1400_RICR		2*0x44	/* receive intr channel */
#define CD1400_TICR		2*0x45	/* transmit intr channel */
#define CD1400_MICR		2*0x46	/* modem intr channel */

#define CD1400_RDCR		2*0x0e	/* rx data count */

#define CD1400_EOSRR		2*0x60	/* end of service request */
#define CD1400_RDSR		2*0x62	/* rx data/status */
#define CD1400_RDSR_OVERRUN		(1<<0)	/* rx overrun error */
#define CD1400_RDSR_FRAMING		(1<<1)	/* rx framing error */
#define CD1400_RDSR_PARITY		(1<<2)	/* rx parity error */
#define CD1400_RDSR_BREAK		(1<<3)	/* rx break */
#define CD1400_RDSR_SPECIAL		(7<<4)	/* rx special char */
#define CD1400_RDSR_SPECIAL_SHIFT	4	/* rx special char shift */
#define CD1400_RDSR_TIMEOUT		(1<<7)	/* rx timeout */

#define CD1400_TDR		2*0x63	/* tx data */

#define CD1400_MISR		2*0x4c	/* modem intr status */
#define CD1400_MISR_DSRd		(1<<7)	/* DSR delta */
#define CD1400_MISR_CTSd		(1<<6)	/* CTS delta */
#define CD1400_MISR_RId			(1<<5)	/* RI delta */
#define CD1400_MISR_CDd			(1<<4)	/* CD delta */

#define CD1400_MSVR		2*0x6d	/* modem signals */
#define CD1400_MSVR_DSR			(1<<7)	/* !DSR line */
#define CD1400_MSVR_CTS			(1<<6)	/* !CTS line */
#define CD1400_MSVR_RI			(1<<5)	/* !RI line */
#define CD1400_MSVR_CD			(1<<4)	/* !CD line */
#define CD1400_MSVR_DTR			(1<<1)	/* DTR line */

#define CD1400_DTR		2*0x6d	/* dtr control */
#define CD1400_DTR_CLEAR		0
#define CD1400_DTR_SET			(1<<1)

#define CD1400_PPR		2*0x7e
#define CD1400_CLOCK_25_1MS		0x31

#define CD1400_CAR		2*0x68	/* channel access */

#define CD1400_RIR		2*0x6B	/* receive interrupt status */
#define CD1400_TIR		2*0x6A	/* transmit interrupt status */
#define CD1400_MIR		2*0x69	/* modem interrupt status */

#define CD1400_RBPR		2*0x78	/* receive baud rate period */
#define CD1400_RCOR		2*0x7C	/* receive clock option */
#define CD1400_TBPR		2*0x72	/* transmit baud rate period */
#define CD1400_TCOR		2*0x76	/* transmit clock option */

#define CD1400_COR1		2*0x08	/* channel option 1 */
#define CD1400_COR2		2*0x09	/* channel option 2 */
#define CD1400_COR3		2*0x0A	/* channel option 3 */
#define CD1400_COR4		2*0x1E	/* channel option 4 */
#define CD1400_COR5		2*0x1F	/* channel option 5 */

#define CD1400_SCHR1		2*0x1A	/* special character 1 */
#define CD1400_SCHR2		2*0x1B	/* special character 2 */
#define CD1400_SCHR3		2*0x1C	/* special character 3 */
#define CD1400_SCHR4		2*0x1D	/* special character 4 */

#define CD1400_MCOR1		2*0x15	/* modem change 1 */
#define CD1400_MCOR2		2*0x16	/* modem change 2 */
#define CD1400_RTPR		2*0x21	/* receive timeout period */

#define CD1400_SVRR		2*0x67	/* service request */
#define CD1400_SVRR_RX			(1<<0)
#define CD1400_SVRR_TX			(1<<1)
#define CD1400_SVRR_MDM			(1<<2)

/* hardware SVCACK addresses, for use in interrupt handlers */
#define CD1400_SVCACKR		0x100
#define CD1400_SVCACKT		0x200
#define CD1400_SVCACKM		0x300
