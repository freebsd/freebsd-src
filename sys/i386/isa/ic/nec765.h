/*-
 * Copyright (c) 1991 The Regents of the University of California.
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
 *	from: @(#)nec765.h	7.1 (Berkeley) 5/9/91
 *	$Id: nec765.h,v 1.2 1993/10/16 13:48:50 rgrimes Exp $
 */

/*
 * Nec 765 floppy disc controller definitions
 */

/* Main status register */
#define NE7_DAB	0x01	/* Diskette drive A is seeking, thus busy */
#define NE7_DBB	0x02	/* Diskette drive B is seeking, thus busy */
#define NE7_CB	0x10	/* Diskette Controller Busy */
#define NE7_NDM	0x20	/* Diskette Controller in Non Dma Mode */
#define NE7_DIO	0x40	/* Diskette Controller Data register I/O */
#define NE7_RQM	0x80	/* Diskette Controller ReQuest for Master */

/* Status register ST0 */
#define NE7_ST0BITS	"\020\010invld\007abnrml\006seek_cmplt\005drv_chck\004drive_rdy\003top_head"

/* Status register ST1 */
#define NE7_ST1BITS	"\020\010end_of_cyl\006bad_crc\005data_overrun\003sec_not_fnd\002write_protect\001no_am"

/* Status register ST2 */
#define NE7_ST2BITS	"\020\007ctrl_mrk\006bad_crc\005wrong_cyl\004scn_eq\003scn_not_fnd\002bad_cyl\001no_dam"

/* Status register ST3 */
#define NE7_ST3BITS	"\020\010fault\007write_protect\006drdy\005tk0\004two_side\003side_sel\002"

/* Commands */
#define NE7CMD_SPECIFY	3	/*  specify drive parameters - requires unit
					parameters byte */
#define NE7CMD_SENSED	4	/*  sense drive - requires unit select byte */
#define NE7CMD_WRITE	0xc5	/*  write - requires eight additional bytes */
#define NE7CMD_READ	0xe6	/*  read - requires eight additional bytes */
#define NE7CMD_FORMAT	0x4c	/*  format - requires five additional bytes */
#define NE7CMD_RECAL	7	/*  recalibrate drive - requires
					unit select byte */
#define NE7CMD_SENSEI	8	/*  sense controller interrupt status */
#define NE7CMD_SEEK	15	/*  seek drive - requires unit select byte
					and new cyl byte */
