/*
 *   Copyright (c) 1994, 1998 Hellmuth Michaelis. All rights reserved.
 *
 *   Based on code written by Stollmann GmbH, Hamburg. Many thanks to
 *   Christian Luehrs and Manfred Jung for docs, sources and answers!
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   
 *   THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 *   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 *   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *   SUCH DAMAGE.
 *
 *---------------------------------------------------------------------------
 *
 *	i4b_tina_ioctl.h - i4b Stollman Tina-dd ioctl header file
 *	---------------------------------------------------------
 *
 * $FreeBSD: src/sys/i4b/tina-dd/i4b_tina_ioctl.h,v 1.2 1999/08/28 00:45:58 peter Exp $
 *
 *	last edit-date: [Sat Dec  5 18:41:51 1998]
 *
 *---------------------------------------------------------------------------*/

#ifndef _I4B_TINA_IOCTL_H_
#define _I4B_TINA_IOCTL_H_

#define TINA_IOSIZE     8               /* 8 byte wide iospace occupied */

/*---------------------------------------------------------------------------*
 *	register offsets in i/o address space
 *---------------------------------------------------------------------------*/

#define CTRL_STAT	0		/* control & status	*/

#define ADDR_CNTL	1		/* address pointer low	*/
#define ADDR_CNTM	2		/* address pointer mid	*/
#define ADDR_CNTH	3		/* address pointer high	*/

#define DATA_LOW	4		/* data register low	*/
#define DATA_HIGH	5		/* data register high	*/

#define DATA_LOW_INC	6		/* data register low, post inc	*/
#define DATA_HIGH_INC	7		/* data register high, post inc	*/

/*---------------------------------------------------------------------------*
 *	status register (CTRL_STAT read access)
 *---------------------------------------------------------------------------*/

#define CR_INTC         0x80		/* irq FROM tina-dd TO pc active */
#define CR_INTP         0x40		/* irq FROM pc TO tina-dd active */
#define CR_INTPA        0x20		/* irq FROM pc TO tina-dd active */
#define CR_NMI          0x10		/* nmi FROM PC TO tina-dd active */
#define CR_FLASHLD      0x08		/* read of the FLASHLD-bit (n/c) */
#define CR_S2C          0x04		/* info bit */
#define CR_S1C          0x02		/* info bit */
#define CR_S0C          0x01		/* info bit */

/*---------------------------------------------------------------------------*
 *	control register (CTRL_STAT write access)
 *---------------------------------------------------------------------------*/

#define CR_CLR_INTC     0x80		/* clear irq on tina-dd */
#define CR_SET_INTP     0x40		/* trigger irq on tina-dd */
#define CR_RESET	0x20		/* reset tina-dd */
#define CR_SET_NMI      0x10		/* trigger nmi on tina-dd */
#define CR_SET_FLASHLD  0x08		/* activates pin FLASHLD (n/c) */
#define CR_S2P          0x04		/* info bit (not readable !) */
#define CR_S1P          0x02		/* info bit (not readable !) */
#define CR_S0P          0x01		/* info bit (not readable !) */

/*---------------------------------------------------------------------------*
 *	misc definitions in dual-ported mem on board of tina-dd
 *---------------------------------------------------------------------------*/

#define FW_SYSCB	0x200		/* address of FW SYSCB / MJ 300392 */
#define FW_SINFO_NAME	0x220		/* address of general info label   */

#define FW_HW_TYPE	0x224		/* address of hardware type byte:  */
#define  FW_HW_UNDEF	0x00		/* undefined ..                    */
#define  FW_HW_TINA_DD	0x10		/* TINA-dd			   */
#define  FW_HW_TINA_DS	0x20		/* TINA-ds 	(B channel/ser ?)  */
#define  FW_HW_TINA_D	0x30		/* TINA-d	(one B channel ?)  */
#define  FW_HW_TINA_DDM	0x40		/* TINA-dd with fax module	   */
#define  FW_HW_TINA_DDS	0x50		/* TINA-dd with fax/voice module   */
#define  FW_HW_SICCE	0x80		/* X.25 board			   */
#define  FW_HW_ASIC	0x01		/* ASIC version bit                */

#define FW_STAT		0x228		/* address of firmware status byte */
#define  FW_READY	0x20		/* firmware ready bit		   */
#define  FW_BOOTPRM_RDY	0x02		/* boot PROM ready		   */
#define  FW_UNDEF_0	0x00		/* undefined			   */
#define  FW_UNDEF_1	0xFF		/* undefined			   */

#define FW_SINFO_ID	"SYSI"		/* general info label for FW > 2.13*/
#define FW_SINFO_ID_LEN	4

#define FW_ADDR_PROFPTR 0x260		/* addr of ptr to board profile	   */

/*===========================================================================*
 *	Layer 0 - Hardware layer
 *===========================================================================*/

/* control and status register access */

#define ISDN_GETCSR	 _IOR('I', 1, unsigned char)	/* get csr */
#define ISDN_SETCSR	 _IOW('I', 2, unsigned char)	/* set csr */

/* dual ported ram access */

#define ISDN_GETBLK	_IOWR('I', 3, struct record)	/* get dpr record */
#define ISDN_SETBLK	 _IOW('I', 4, struct record)	/* set dpr record */

/*---------------------------------------------------------------------------*
 *	record structure for dual ported ram block rd/wr
 *---------------------------------------------------------------------------*/
struct record {
	unsigned int length;		/* length of data block */
	unsigned int addr;		/* address of mem on tina-dd board */
	unsigned char *data;		/* pointer to the datablock itself */
};

#endif /* _I4B_TINA_IOCTL_H_ */
