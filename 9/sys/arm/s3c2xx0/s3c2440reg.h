/*-
 * Copyright (C) 2009 Andrew Turner
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
 * Samsung S3C2440X processor is ARM920T based integrated CPU
 *
 * Reference:
 *  S3C2440A/S3C2442B User's Manual 
 */
#ifndef _ARM_S3C2XX0_S3C2440REG_H_
#define	_ARM_S3C2XX0_S3C2440REG_H_

/* common definitions for S3C2410 and S3C2440 */
#include <arm/s3c2xx0/s3c24x0reg.h>

/*
 * Memory Map
 */
#define	S3C2440_BANK_SIZE 	0x08000000
#define	S3C2440_BANK_START(n)	(S3C2410_BANK_SIZE*(n))
#define	S3C2440_SDRAM_START	S3C2410_BANK_START(6)


/* interrupt control */
#define	S3C2440_SUBIRQ_MAX	(S3C24X0_SUBIRQ_MIN+10)

/* Clock control */
/* CLKMAN_CLKCON */
#define	 S3C2440_CLKCON_STOP	(1<<0)	/* 1=transition to STOP mode */
/* CLKMAN_CLKDIVN */
#define	 S3C2440_CLKDIVN_HDIVN	(3<<1)	/* hclk */
#define S3C2440_CLKMAN_CAMDIVN	0x18
#define  S3C2440_CAMDIVN_HCLK4_HALF	(1<<9)
#define  S3C2440_CAMDIVN_HCLK3_HALF	(1<<8)

/* NAND Flash controller */
#define S3C2440_NANDFC_SIZE	0x40

#define S3C2440_NANDFC_NFCONT	0x04
#define  S3C2440_NFCONT_LOCK_TIGHT	(1<<13) /* Lock part of the NAND */
#define  S3C2440_NFCONT_SOFT_LOCK	(1<<12) /* Soft lock part of the NAND */
#define  S3C2440_NFCONT_ILLEGAL_ACC_INT	(1<<10) /* Illegal access interrupt */
#define  S3C2440_NFCONT_RNB_INT		(1<<9) /* RnB transition interrupt */
#define  S3C2440_NFCONT_RNB_TRANS_MODE	(1<<8) /* RnB transition mode */
#define  S3C2440_NFCONT_SPARE_ECC_LOCK	(1<<6) /* Lock spare ECC generation */
#define  S3C2440_NFCONT_MAIN_ECC_LOCK	(1<<5) /* Lock main ECC generation */
#define  S3C2440_NFCONT_INIT_ECC	(1<<4) /* Init ECC encoder/decoder */
#define  S3C2440_NFCONT_NCE		(1<<1) /* NAND Chip select */
#define  S3C2440_NFCONT_ENABLE		(1<<0) /* Enable the controller */
#define S3C2440_NANDFC_NFCMMD	0x08
#define S3C2440_NANDFC_NFADDR	0x0c
#define S3C2440_NANDFC_NFDATA	0x10
#define S3C2440_NANDFC_NFSTAT	0x20

/* MMC/SD */
/* SDI_CON */
#define  S3C2440_CON_RESET		(1<<8)
#define  S3C2440_CON_CLOCK_TYPE		(1<<5)
/* SDI_FSTA */
#define  S3c2440_FSTA_RESET		(1<<16)
#define  S3C2440_FSTA_FAIL_ERROR_MSK	(3<<14)
#define  S3C2440_FSTA_FAIL_NONE		(0<<14)
#define  S3C2440_FSTA_FAIL_FIFO		(1<<14)
#define  S3C2440_FSTA_FAIL_LAST_TRANS	(2<<14)

/* GPIO */
#define	S3C2440_GPIO_SIZE	0xd0

/* SD interface */
#define	S3C2410_SDI_SIZE 	0x44
#define  DCON_START		(1<<14) /* Start the data transfer */
#define S3C2440_SDI_IMSK	0x3c /* Interrupt mask */
#define  S3C2440_SDI_IMASK_ALL	0x3C7C0
#define S3C2440_SDI_DAT		0x40

/* ADC */
#define	 ADCTSC_UD_SEN		(1<<8)
#define	S3C2440_ADC_SIZE 	0x18

/* UART */
#define  S3C2440_UFSTAT_TXCOUNT	(0x3f << 8)
#define  S3C2440_UFSTAT_RXCOUNT	(0x3f << 0)

#endif /* _ARM_S3C2XX0_S3C2440REG_H_ */
