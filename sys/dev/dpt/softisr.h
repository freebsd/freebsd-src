/*
 *       Copyright (c) 1997 by Simon Shapiro
 *       All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: softisr.h,v 1.1.2.1 1998/03/06 23:44:18 julian Exp $
 */

#ifndef	_SOFTISR_H_
#define	_SOFTISR_H_

/*
 *	The following manifest constants are named w/o
 *	regard to imagination
 */
#define	DPTISR_DPT	 0
#define	DPTISR_1	 1
#define	DPTISR_2	 2
#define	DPTISR_3	 3
#define	DPTISR_4	 4
#define	DPTISR_5	 5
#define	DPTISR_6	 6
#define	DPTISR_7	 7
#define	DPTISR_8	 8
#define	DPTISR_9	 9
#define	DPTISR_10	10
#define	DPTISR_11	11
#define	DPTISR_12	12
#define	DPTISR_13	13
#define	DPTISR_14	14
#define	DPTISR_15	15
#define	DPTISR_16	16
#define	DPTISR_17	17
#define	DPTISR_18	18
#define	DPTISR_19	19
#define	DPTISR_20	20
#define	DPTISR_21	21
#define	DPTISR_22	22
#define	DPTISR_23	23
#define	DPTISR_24	24
#define	DPTISR_25	25
#define	DPTISR_26	26
#define	DPTISR_27	27
#define	DPTISR_28	28
#define	DPTISR_29	29
#define	DPTISR_30	30
#define	DPTISR_31	31

/*
 *	equivalent to schednetisr() for the DPT driver
 */

#ifndef setsoftdpt
extern void setsoftdpt(void);
#endif

#define	scheddptisr(anisr)	{ dptisr |= 1<<(anisr); setsoftdpt(); }

#ifndef	LOCORE
#ifdef	KERNEL
extern	volatile unsigned int dptisr; /* Scheduling bits for DPT driver */

typedef void dptisr_t(void);

struct dptisrtab {
	int sint_num;
	dptisr_t *sint_isr;
};

#define DPTISR_SET(num, isr) \
	static struct dptisrtab mod_sint = { num, isr}; \
	DATA_SET(dptisr_set, mod_sint);

int register_dptisr __P((int, dptisr_t *));

#endif	/* #ifdef  KERNEL */
#endif	/* #ifndef LOCORE */

#endif	/* #ifndef _SOFTISR_H_ */
