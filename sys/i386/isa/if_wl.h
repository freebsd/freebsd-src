/* 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain all copyright 
 *    notices, this list of conditions and the following disclaimer.
 * 2. The names of the authors may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */
/* Definitions for WaveLAN driver */

#ifndef	_IF_WL_H
#define _IF_WL_H

#define	STATUS_TRIES	15000

#define N_FD			100
#define N_RBD			100
#define N_TBD			72
#define RCVBUFSIZE		540
#define I82586NULL		0xffff

#define DSF_RUNNING	1

#define MOD_ENAL 1
#define MOD_PROM 2

typedef struct {
	rbd_t	r;
	char	rbd_pad[2];
	char	rbuffer[RCVBUFSIZE];
} ru_t;

/* Board 64k RAM layout. Offsets from 0x0000 */
	
#define OFFSET_RU		0x0000		/* 0x64 * fd_t = 0x898 */
#define OFFSET_RBD		0x0900		/* 0x64 * ru_t = 0xd7a0 */
#define OFFSET_CU		0xe0a0		/* 0x100 */
#define OFFSET_TBD		0xe1a0		/* 0x48 * tbd_t = 0x240 */
#define OFFSET_TBUF		0xe3e0		/* 0x1bfe */
#define OFFSET_SCB		0xffde		/* 0x1 * scb_t = 0x10 */
#define OFFSET_ISCP		0xffee		/* 0x1 * iscp_t = 0x8 */
#define OFFSET_SCP		0xfff6		/* 0x1 * scp_t = 0xa */
	
/* WaveLAN host interface definitions */

#define HACR(base)	(base)		/* Host Adapter Command Register */
#define HASR(base)	(base)		/* Host Adapter Status Register */
#define MMCR(base)	(base+0x2)	/* Modem Management Ctrl Register */
#define PIOR0(base)	(base+0x4)	/* Program I/O Address Register 0 */
#define PIOP0(base)	(base+0x6)	/* Program I/O Port 0 */
#define PIOR1(base)	(base+0x8)	/* Program I/O Address Register 1 */
#define PIOP1(base)	(base+0xa)	/* Program I/O Port 1 */
#define PIOR2(base)	(base+0xc)	/* Program I/O Address Register 2 */
#define PIOP2(base)	(base+0xe)	/* Program I/O Port 2 */

/* Program I/O Mode Register values */

#define STATIC_PIO		0	/* Mode 1: static mode */
#define AUTOINCR_PIO		1	/* Mode 2: auto increment mode */
#define AUTODECR_PIO		2	/* Mode 3: auto decrement mode */
#define PARAM_ACCESS_PIO	3	/* Mode 4: LAN parameter access mode */
#define PIO_MASK		3	/* register mask */
#define PIOM(cmd,piono)		((u_short)cmd << 10 << (piono * 2))

/* Host Adapter status register definitions */

#define HASR_INTR		0x0001	/* Interrupt request from 82586 */
#define HASR_MMC_INTR		0x0002	/* Interrupt request from MMC */
#define HASR_MMC_BUSY		0x0004	/* MMC busy indication */
#define HASR_PARA_BUSY		0x0008	/* LAN parameter storage area busy */

/* Host Adapter command register definitions */

#define HACR_RESET		0x0001	/* Reset board */
#define HACR_CA			0x0002	/* Set Channel Attention for 82586 */
#define HACR_16BITS		0x0004	/* 1==16 bits operation, 0==8 bits */
#define HACR_OUT1		0x0008	/* General purpose output pin */
#define HACR_OUT2		0x0010	/* General purpose output pin */
#define HACR_MASK_82586		0x0020	/* Mask 82586 interrupts, 1==unmask */
#define HACR_MASK_MMC		0x0040	/* Mask MMC interrupts, 1==unmask */
#define HACR_INTR_CLEN		0x0080	/* interrupt status clear enable */

#define HACR_DEFAULT	(HACR_OUT1 | HACR_OUT2 | HACR_16BITS | PIOM(STATIC_PIO, 0) | PIOM(AUTOINCR_PIO, 1) | PIOM(PARAM_ACCESS_PIO, 2))
#define HACR_INTRON	(HACR_MASK_82586 | HACR_MASK_MMC | HACR_INTR_CLEN)
#define CMD(unit)	\
		{ \
		   outw(HACR(WLSOFTC(unit)->base),WLSOFTC(unit)->hacr); \
		   /* delay for 50 us, might only be needed sometimes */ \
		   DELAY(DELAYCONST); \
	        }

/* macro for setting the channel attention bit.  No delays here since
 * it is used in critical sections
 */
#define SET_CHAN_ATTN(unit)   \
      { \
         outw(HACR(WLSOFTC(unit)->base),WLSOFTC(unit)->hacr | HACR_CA); \
      }


#define MMC_WRITE(cmd,val)	\
	while(inw(HASR(WLSOFTC(unit)->base)) & HASR_MMC_BUSY) ; \
	outw(MMCR(WLSOFTC(unit)->base), \
	     (u_short)(((u_short)(val) << 8) | ((cmd) << 1) | 1))

#endif	_IF_WL_H

