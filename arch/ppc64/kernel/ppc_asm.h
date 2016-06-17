/*
 * arch/ppc64/kernel/ppc_asm.h
 *
 * Definitions used by various bits of low-level assembly code on PowerPC.
 *
 * Copyright (C) 1995-1999 Gary Thomas, Paul Mackerras, Cort Dougan.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 */

#include <linux/config.h>

#include <asm/ppc_asm.tmpl>
#include "ppc_defs.h"

/*
 * Macros for storing registers into and loading registers from
 * exception frames.
 */
#define SAVE_GPR(n, base)	std	n,GPR0+8*(n)(base)
#define SAVE_2GPRS(n, base)	SAVE_GPR(n, base); SAVE_GPR(n+1, base)
#define SAVE_4GPRS(n, base)	SAVE_2GPRS(n, base); SAVE_2GPRS(n+2, base)
#define SAVE_8GPRS(n, base)	SAVE_4GPRS(n, base); SAVE_4GPRS(n+4, base)
#define SAVE_10GPRS(n, base)	SAVE_8GPRS(n, base); SAVE_2GPRS(n+8, base)
#define REST_GPR(n, base)	ld	n,GPR0+8*(n)(base)
#define REST_2GPRS(n, base)	REST_GPR(n, base); REST_GPR(n+1, base)
#define REST_4GPRS(n, base)	REST_2GPRS(n, base); REST_2GPRS(n+2, base)
#define REST_8GPRS(n, base)	REST_4GPRS(n, base); REST_4GPRS(n+4, base)
#define REST_10GPRS(n, base)	REST_8GPRS(n, base); REST_2GPRS(n+8, base)

#define SAVE_FPR(n, base)	stfd	n,THREAD_FPR0+8*(n)(base)
#define SAVE_2FPRS(n, base)	SAVE_FPR(n, base); SAVE_FPR(n+1, base)
#define SAVE_4FPRS(n, base)	SAVE_2FPRS(n, base); SAVE_2FPRS(n+2, base)
#define SAVE_8FPRS(n, base)	SAVE_4FPRS(n, base); SAVE_4FPRS(n+4, base)
#define SAVE_16FPRS(n, base)	SAVE_8FPRS(n, base); SAVE_8FPRS(n+8, base)
#define SAVE_32FPRS(n, base)	SAVE_16FPRS(n, base); SAVE_16FPRS(n+16, base)
#define REST_FPR(n, base)	lfd	n,THREAD_FPR0+8*(n)(base)
#define REST_2FPRS(n, base)	REST_FPR(n, base); REST_FPR(n+1, base)
#define REST_4FPRS(n, base)	REST_2FPRS(n, base); REST_2FPRS(n+2, base)
#define REST_8FPRS(n, base)	REST_4FPRS(n, base); REST_4FPRS(n+4, base)
#define REST_16FPRS(n, base)	REST_8FPRS(n, base); REST_8FPRS(n+8, base)
#define REST_32FPRS(n, base)	REST_16FPRS(n, base); REST_16FPRS(n+16, base)

/*
 * Once a version of gas that understands the AltiVec instructions
 * is freely available, we can do this the normal way...  - paulus
 */
#define LVX(r,a,b)	.long	(31<<26)+((r)<<21)+((a)<<16)+((b)<<11)+(103<<1)
#define STVX(r,a,b)	.long	(31<<26)+((r)<<21)+((a)<<16)+((b)<<11)+(231<<1)
#define MFVSCR(r)	.long	(4<<26)+((r)<<21)+(770<<1)
#define MTVSCR(r)	.long	(4<<26)+((r)<<11)+(802<<1)
#define DSSALL		.long	(0x1f<<26)+(0x10<<21)+(0x336<<1)


#define SAVE_VR(n,b,base)	li b,THREAD_VR0+(16*(n)); STVX(n,b,base)
#define SAVE_2VR(n,b,base)	SAVE_VR(n,b,base); SAVE_VR(n+1,b,base)
#define SAVE_4VR(n,b,base)	SAVE_2VR(n,b,base); SAVE_2VR(n+2,b,base)
#define SAVE_8VR(n,b,base)	SAVE_4VR(n,b,base); SAVE_4VR(n+4,b,base)
#define SAVE_16VR(n,b,base)	SAVE_8VR(n,b,base); SAVE_8VR(n+8,b,base)
#define SAVE_32VR(n,b,base)	SAVE_16VR(n,b,base); SAVE_16VR(n+16,b,base)
#define REST_VR(n,b,base)	li b,THREAD_VR0+(16*(n)); LVX(n,b,base)
#define REST_2VR(n,b,base)	REST_VR(n,b,base); REST_VR(n+1,b,base)
#define REST_4VR(n,b,base)	REST_2VR(n,b,base); REST_2VR(n+2,b,base)
#define REST_8VR(n,b,base)	REST_4VR(n,b,base); REST_4VR(n+4,b,base)
#define REST_16VR(n,b,base)	REST_8VR(n,b,base); REST_8VR(n+8,b,base)
#define REST_32VR(n,b,base)	REST_16VR(n,b,base); REST_16VR(n+16,b,base)


#define CHECKANYINT(ra,rb)			\
	mfspr	rb,SPRG3;		/* Get Paca address */\
	ld	ra,PACALPPACA+LPPACAANYINT(rb); /* Get pending interrupt flags */\
	cmpldi	0,ra,0;

/* Macros to adjust thread priority for Iseries hardware multithreading */
#define HMT_LOW		or 1,1,1
#define HMT_MEDIUM	or 2,2,2
#define HMT_HIGH	or 3,3,3

/* Insert the high 32 bits of the MSR into what will be the new
   MSR (via SRR1 and rfid)  This preserves the MSR.SF and MSR.ISF
   bits. */

#define FIX_SRR1(ra, rb)	\
	mr	rb,ra;		\
	mfmsr	ra;		\
	rldimi	ra,rb,0,32

#define CLR_TOP32(r)	rlwinm	(r),(r),0,0,31	/* clear top 32 bits */

/* 
 * LOADADDR( rn, name )
 *   loads the address of 'name' into 'rn'
 *
 * LOADBASE( rn, name )
 *   loads the address (less the low 16 bits) of 'name' into 'rn'
 *   suitable for base+disp addressing
 */
#define LOADADDR(rn,name) \
	lis	rn,name##@highest;	\
	ori	rn,rn,name##@higher;	\
	rldicr	rn,rn,32,31;		\
	oris	rn,rn,name##@h;		\
	ori	rn,rn,name##@l

#define LOADBASE(rn,name) \
	lis	rn,name@highest;	\
	ori	rn,rn,name@higher;	\
	rldicr	rn,rn,32,31;		\
	oris	rn,rn,name@ha


#define SET_REG_TO_CONST(reg, value)	         	\
	lis     reg,(((value)>>48)&0xFFFF);             \
	ori     reg,reg,(((value)>>32)&0xFFFF);         \
	rldicr  reg,reg,32,31;                          \
	oris    reg,reg,(((value)>>16)&0xFFFF);         \
	ori     reg,reg,((value)&0xFFFF);

#define SET_REG_TO_LABEL(reg, label)	         	\
	lis     reg,(label)@highest;                    \
	ori     reg,reg,(label)@higher;                 \
	rldicr  reg,reg,32,31;                          \
	oris    reg,reg,(label)@h;                      \
	ori     reg,reg,(label)@l;


/* PPPBBB - DRENG  If KERNELBASE is always 0xC0...,
 * Then we can easily do this with one asm insn. -Peter
 */
#define tophys(rd,rs)                           \
        lis     rd,((KERNELBASE>>48)&0xFFFF);   \
        rldicr  rd,rd,32,31;                    \
        sub     rd,rs,rd

#define tovirt(rd,rs)                           \
        lis     rd,((KERNELBASE>>48)&0xFFFF);   \
        rldicr  rd,rd,32,31;                    \
        add     rd,rs,rd

