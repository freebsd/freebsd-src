/*-
 * Copyright (c) 1999 Matthew N. Dodd <winter@jurai.net>
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
 * $FreeBSD: src/sys/dev/mca/mca_busreg.h,v 1.1 1999/09/03 03:40:00 mdodd Exp $
 */

/*
 * Standardized MCA configuration information
 */

#define MCA_MAX_SLOTS		8	/* max number of slots per bus */
#define MCA_MB_SCSI_SLOT	8
#define MCA_MB_VIDEO_SLOT	9
#define MCA_MAX_ADAPTERS	9

/*
 * When an ADF file talks about a POS register
 * its not talking about the same index we are
 * so provide this to convert ADF pos register
 * offsets to our register offsets. (Since 
 * to us, POS0 and POS1 are just 2 more registers
 */

#define MCA_ADP_POS(pos)	(pos + 2)

#define MCA_POS0		0	/* low byte of board ID		*/
#define MCA_POS1		1	/* high byte of board ID	*/
#define MCA_POS2		2
# define MCA_POS2_ENABLE	0x01    /* POS2, hi => adapter enabled */
#define MCA_POS3		3
#define MCA_POS4		4
#define MCA_POS5		5
# define MCA_POS5_CHCK_STAT	0x40	/* lo => CHCK status available */
# define MCA_POS5_CHCK		0x80	/* lo => adapter CHCK signal */
#define MCA_POS6		6	/* low byte of CHCK status */
#define MCA_POS7		7	/* high byte of CHCK status */

/*
 * MCA register addresses for IBM PS/2
 */

#define MCA_SYS_CTL_A_REG	0x92	/* PS/2 System Control Port A */
#define MCA_SYS_CTL_B_REG	0x60	/* PS/2 System Control Port B */
#define MCA_ARB_REG		0x90	/* MCA Arbitration port */
#define MCA_CSF_REG		0x91	/* MCA Card Select Feedback */

/*
 *	0x96, 0x97	POS Registers
 *	0x100 - 0x107	POS Registers
 */

#define MCA_MB_SETUP_REG	0x94	/* Motherboard setup register */
# define MCA_MB_SETUP_DIS	0xff	/* Disable motherboard setup */
# define MCA_MB_SETUP_VIDEO	0xdf
# define MCA_MB_SETUP_SCSI	0xf7	/* Pri. SCSI setup reg */
# define MCA_MB_SETUP_SCSI_ALT	0xfd	/* Alt. SCSI setup reg */

#define MCA_ADAP_SETUP_REG	0x96	/* Adapter setup register */
# define MCA_ADAP_SETUP_DIS	0x0	/* Disable adapter setup */
# define MCA_ADAP_SET		0x08	/* Adapter setup mode */
# define MCA_ADAP_CHR		0x80	/* Adapter channel reset */
#define MCA_POS_REG(n)		(0x100+(n))	/* POS registers 0-7 */
