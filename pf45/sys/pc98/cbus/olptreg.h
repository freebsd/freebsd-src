/*-
 * Copyright (c) 2005 TAKAHASHI Yoshihiro
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
 * $FreeBSD$
 */

/*
 * modified for PC9801 by A.Kojima
 *			Kyoto University Microcomputer Club (KMC)
 */

#define lpt_pstb_ctrl	(-9)	/* PSTB enable control */
#define	LPC_EN_PSTB	0xc	/* PSTB enable */
#define	LPC_DIS_PSTB	0xd	/* PSTB disable */

#define lpt_data	0	/* Data to/from printer (R/W) */

#define lpt_status	2	/* Status of printer (R) */
#define	LPS_NBSY	0x4	/* printer no ack of data */

#define lpt_control	6	/* Control printer (W) */
#define	LPC_MODE8255	0x82	/* 8255 mode */
#define	LPC_IRQ8	0x6	/* IRQ8 active */
#define	LPC_NIRQ8	0x7	/* IRQ8 inactive */
#define	LPC_PSTB	0xe	/* PSTB active */
#define	LPC_NPSTB	0xf	/* PSTB inactive */
