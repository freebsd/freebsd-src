/*-
 * Copyright (c) 2001 Brian Somers <brian@Awfulhak.org>
 *   based on work by Slawa Olhovchenkov
 *                    John Prince <johnp@knight-trosoft.com>
 *                    Eric Hernes
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
 * $FreeBSD: src/sys/dev/digi/digireg.h,v 1.3 2003/08/07 15:04:24 jhb Exp $
 */

struct global_data {
	volatile u_short cin;
	volatile u_short cout;
	volatile u_short cstart;
	volatile u_short cmax;
	volatile u_short ein;
	volatile u_short eout;
	volatile u_short istart;
	volatile u_short imax;
};


struct board_chan {
	volatile u_short tpjmp;
	volatile u_short tcjmp;
	volatile u_short fil1;
	volatile u_short rpjmp;
	
	volatile u_short tseg;
	volatile u_short tin;
	volatile u_short tout;
	volatile u_short tmax;
	
	volatile u_short rseg;
	volatile u_short rin;
	volatile u_short rout;
	volatile u_short rmax;
	
	volatile u_short tlow;
	volatile u_short rlow;
	volatile u_short rhigh;
	volatile u_short incr;
	
	volatile u_short dev;
	volatile u_short edelay;
	volatile u_short blen;
	volatile u_short btime;
	
	volatile u_short iflag;
	volatile u_short oflag;
	volatile u_short cflag;
	volatile u_short gmask;
	
	volatile u_short col;
	volatile u_short delay;
	volatile u_short imask;
	volatile u_short tflush;

	volatile u_char _1[16];
	
	volatile u_char num;
	volatile u_char ract;
	volatile u_char bstat;
	volatile u_char tbusy;
	volatile u_char iempty;
	volatile u_char ilow;
	volatile u_char idata;
	volatile u_char eflag;
	
	volatile u_char tflag;
	volatile u_char rflag;
	volatile u_char xmask;
	volatile u_char xval;
	volatile u_char mstat;
	volatile u_char mchange;
	volatile u_char mint;
	volatile u_char lstat;

	volatile u_char mtran;
	volatile u_char orun;
	volatile u_char startca;
	volatile u_char stopca;
	volatile u_char startc;
	volatile u_char stopc;
	volatile u_char vnext;
	volatile u_char hflow;

	volatile u_char fillc;
	volatile u_char ochar;
	volatile u_char omask;
	volatile u_char _2;

	volatile u_char _3[28];
}; 

#define SRXLWATER      0xe0
#define SRXHWATER      0xe1
#define STPTR          0xe2
#define PAUSETX        0xe3
#define RESUMETX       0xe4
#define SAUXONOFFC     0xe6
#define SENDBREAK      0xe8
#define SETMODEM       0xe9
#define SETIFLAGS      0xeA
#define SONOFFC        0xeB
#define STXLWATER      0xeC
#define PAUSERX        0xeE
#define RESUMERX       0xeF
#define RESETCHAN      0xf0
#define SETBUFFER      0xf2
#define SETCOOKED      0xf3
#define SETHFLOW       0xf4
#define SETCFLAGS      0xf5
#define SETVNEXT       0xf6
#define SETBSLICE      0xf7
#define SETRSMODE      0xfd
#define SETCMDACK      0xfe
#define RESERV         0xff

#define BREAK_IND        0x01
#define LOWTX_IND        0x02
#define EMPTYTX_IND      0x04
#define DATA_IND         0x08
#define MODEMCHG_IND     0x20
#define RECV_OVR_IND	 0x40
#define CMD_ACK_IND	 0x40
#define UART_OVR_IND	 0x80

#define ALL_IND	(BREAK_IND|LOWTX_IND|EMPTYTX_IND|DATA_IND|MODEMCHG_IND)

#define FEPTIMEOUT 2000

#define FEPCLR	0x0
#define FEPMEM	0x2
#define FEPRST	0x4
#define FEPREQ	0x8
#define FEPWIN	0x80
#define FEPMASK 0xe
/* #define FEPMASK 0x4 */

#define	BOTWIN		0x100L
#define	TOPWIN		0xFF00L
#define	MISCGLOBAL	0x0C00L
#define	FEPCODESEG	0x0200L

/* #define BIOSCODE   0xff800 */	/* Window 15, offset 7800h */
#define FEPCODE    0x0d000

#define	FEP_CSTART	0x400L
#define	FEP_CMAX	0x800L
#define	FEP_ISTART	0x800L
#define	FEP_IMAX	0xC00L
#define	MBOX		0xC40L
#define	FEP_CIN		0xD10L
#define	FEP_GLOBAL	0xD10L
#define	FEP_EIN		0xD18L
#define	FEPSTAT		0xD20L
#define	CHANSTRUCT	0x1000L
#define	RXTXBUF		0x4000L

#define	BIOSOFFSET	0x1000L
#define	BIOSCODE	0xf800L
#define	FEPOFFSET	0x2000L

/* c_cflag bits */
#define	FEP_CSIZE	0x000030
#define	FEP_CS5		0x000000
#define	FEP_CS6		0x000010
#define	FEP_CS7		0x000020
#define	FEP_CS8		0x000030
#define	FEP_CSTOPB	0x000040
#define	FEP_CREAD	0x000080
#define	FEP_PARENB	0x000100
#define	FEP_PARODD	0x000200
#define	FEP_CLOCAL	0x000800
#define	FEP_FASTBAUD	0x000400

/* c_iflag bits */
#define	FEP_IGNBRK	0000001
#define	FEP_BRKINT	0000002
#define	FEP_IGNPAR	0000004
#define	FEP_PARMRK	0000010
#define	FEP_INPCK	0000020
#define	FEP_ISTRIP	0000040
#define	FEP_IXON	0002000
#define	FEP_IXANY	0004000
#define	FEP_IXOFF	0010000
