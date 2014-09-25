/* $NetBSD: s3c2410reg.h,v 1.6 2004/02/12 03:52:46 bsh Exp $ */

/*-
 * Copyright (c) 2003, 2004  Genetec corporation.  All rights reserved.
 * Written by Hiroyuki Bessho for Genetec corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Genetec corporation may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GENETEC CORP. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GENETEC CORP.
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */


/*
 * Samsung S3C2410X processor is ARM920T based integrated CPU
 *
 * Reference:
 *  S3C2410X User's Manual
 */
#ifndef _ARM_S3C2XX0_S3C2410REG_H_
#define	_ARM_S3C2XX0_S3C2410REG_H_

/* common definitions for S3C2410 and S3C2440 */
#include <arm/samsung/s3c2xx0/s3c24x0reg.h>

/*
 * Memory Map
 */
#define	S3C2410_BANK_SIZE 	0x08000000
#define	S3C2410_BANK_START(n)	(S3C2410_BANK_SIZE*(n))
#define	S3C2410_SDRAM_START	S3C2410_BANK_START(6)


/* interrupt control */
#define	S3C2410_SUBIRQ_MAX	(S3C24X0_SUBIRQ_MIN+10)

/* Clock control */
/* CLKMAN_CLKCON */
#define	 S3C2410_CLKCON_SM	(1<<0)	/* 1=transition to SPECIAL mode */
/* CLKMAN_CLKDIVN */
#define	 S3C2410_CLKDIVN_HDIVN	(1<<1)	/* hclk=fclk/2 */

/* NAND Flash controller */
#define	S3C2410_NANDFC_SIZE	0x18
/* NANDFC_NFCONF */
#define	 S3C2410_NFCONF_ENABLE	(1<<15)	/* NAND controller enabled */
#define	 S3C2410_NFCONF_ECC	(1<<12)	/* Initialize ECC decoder/encoder */
#define	 S3C2410_NFCONF_FCE	(1<<11)	/* Flash chip enabled */
#define	 S3C2410_NFCONF_TACLS	(7<<8)	/* CLE and ALE duration */
#define	 S3C2410_NFCONF_TWRPH0	(7<<4)	/* TWRPH0 duration */
#define	 S3C2410_NFCONF_TWRPH1	(7<<0)	/* TWRPH1 duration */
#define	S3C2410_NANDFC_NFCMD 	0x04	/* command */
#define	S3C2410_NANDFC_NFADDR 	0x08	/* address */
#define	S3C2410_NANDFC_NFDATA 	0x0c	/* data */
#define	S3C2410_NANDFC_NFSTAT 	0x10	/* operation status */
#define	S3C2410_NANDFC_NFECC	0x14	/* ecc */

/* MMC/SD */
/* SDI_CON */
#define  S3C2410_CON_FIFO_RESET		(1<<1)

/* GPIO */
#define	S3C2410_GPIO_SIZE	0xb4

/* SD interface */
#define	S3C2410_SDI_SIZE 	0x44
#define  DCON_STOP		(1<<14) /* Force the transfer to stop */
#define S3C2410_SDI_DAT		0x3c
#define S3C2410_SDI_IMSK	0x40 /* Interrupt mask */
#define  S3C2410_SDI_IMASK_ALL	0x3ffdf

/* ADC */
#define	S3C2410_ADC_SIZE 	0x14

#endif /* _ARM_S3C2XX0_S3C2410REG_H_ */
