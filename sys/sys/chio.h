/*
 * Copyright (c) 1982, 1986 The Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)chio.h	7.6 (Berkeley) 2/5/91
 *
 * PATCHES MAGIC                LEVEL   PATCH THAT GOT US HERE
 * --------------------         -----   ----------------------
 * CURRENT PATCH LEVEL:         1       00098
 * --------------------         -----   ----------------------
 *
 * 16 Feb 93	Julian Elischer		ADDED for SCSI system
 */

/* This is a "convertet" mtio.h from 386BSD 
   Stefan Grefen grefen@goofy.zdv.uni-mainz.de 
 */

/*
 * Structures and definitions for changer io control commands
 */
#ifndef _CHIO_H_
#define _CHIO_H_

#define CH_INVERT		0x10000
#define CH_ADDR_MASK		0xffff
struct chop {
	short	ch_op;		/* operations defined below */
	short	result;		/* The result		    */
	union {
	   struct {
		int chm;		/* Transport element */
		int from;
		int to;
	   } move;
	   struct {
		int chm;		/* Transport element */
		int to;
	   } position; 
	   struct {
	        short   chmo;                   /* Offset of first CHM */
	        short   chms;                   /* No. of CHM */
	        short   slots;                  /* No. of Storage Elements */
                short   sloto;                  /* Offset of first SE */
                short   imexs;                  /* No. of Import/Export Slots */
                short   imexo;                  /* Offset of first IM/EX */
                short   drives;                 /* No. of CTS */
                short   driveo;                 /* Offset of first CTS */
                short   rot;                    /* CHM can rotate */
	   } getparam;
	   struct {
		int type;
#define CH_CHM	1
#define CH_STOR	2
#define CH_IMEX	3
#define CH_CTS	4
		int from;
		struct {
			u_char elema_1;
			u_char elema_0;
			u_char full:1;
			u_char rsvd:1;
			u_char except:1;
			u_char :5;
			u_char rsvd2;
			union {
				struct {
				u_char add_sense_code;
				u_char add_sense_code_qualifier;
				} specs;
				short add_sense;
/* WARINING LSB only */
#define CH_CHOLDER	0x0290	/* Cartridge holder is missing */
#define CH_STATUSQ	0x0390	/* Status is questionable */
#define CH_CTS_CLOSED	0x0490	/* CTS door is closed */

			} ch_add_sense;
			u_char rsvd3[3];
			u_char :6;
			u_char invert:1;
			u_char svalid:1;
			u_char source_1;
			u_char source_0;
			u_char rsvd4[4];
			} elem_data;
		} get_elem_stat;
	} u;
};

/* operations */
#define CHMOVE				1
#define CHPOSITION			2
#define CHGETPARAM			3
#define CHGETELEM			4


/* Changer IO control command */
#define	CHIOOP	_IOWR('c', 1, struct chop)	/* do a mag tape op */
#endif
