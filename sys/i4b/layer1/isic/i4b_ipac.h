/*-
 * Copyright (c) 1997, 2001 Hellmuth Michaelis. All rights reserved.
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
 */

/*---------------------------------------------------------------------------
 *
 *	i4b_ipac.h - definitions for the Siemens IPAC PSB2115 chip
 *	==========================================================
 *
 * $FreeBSD: src/sys/i4b/layer1/isic/i4b_ipac.h,v 1.3.18.1 2008/11/25 02:59:29 kensmith Exp $
 *
 *      last edit-date: [Wed Jan 24 09:10:09 2001]
 *
 *---------------------------------------------------------------------------
 */
 
#ifndef _I4B_IPAC_H_
#define _I4B_IPAC_H_

#define IPAC_BFIFO_LEN	64	/* 64 bytes B-channel FIFO on chip	*/

#define IPAC_HSCXA_OFF	0x00
#define IPAC_HSCXB_OFF	0x40
#define IPAC_ISAC_OFF	0x80
#define IPAC_IPAC_OFF	0xc0

/* chip version */

#define	IPAC_V11	0x01	/* IPAC Version 1.1 */
#define	IPAC_V12	0x02	/* IPAC Version 1.2 */

/*
 * definitions of registers and bits for the IPAC ISDN chip.
 */
 
typedef struct ipac_reg {

	/* most registers can be read/written, but have different names */
	/* so define a union with read/write names to make that clear */

	union {
		struct {
			unsigned char ipac_conf;
			unsigned char ipac_ista;
			unsigned char ipac_id;
			unsigned char ipac_acfg;
			unsigned char ipac_aoe;
			unsigned char ipac_arx;
			unsigned char ipac_pita1;
			unsigned char ipac_pita2;
			unsigned char ipac_pota1;
			unsigned char ipac_pota2;
			unsigned char ipac_pcfg;
			unsigned char ipac_scfg;
			unsigned char ipac_timr2;
		} ipac_r;
		struct {
			unsigned char ipac_conf;
			unsigned char ipac_mask;
			unsigned char ipac_dummy;
			unsigned char ipac_acfg;
			unsigned char ipac_aoe;
			unsigned char ipac_atx;
			unsigned char ipac_pita1;
			unsigned char ipac_pita2;
			unsigned char ipac_pota1;
			unsigned char ipac_pota2;
			unsigned char ipac_pcfg;
			unsigned char ipac_scfg;
			unsigned char ipac_timr2;
		} ipac_w;
	} ipac_rw;
} ipac_reg_t;

#define REG_OFFSET(type, field) (int)(&(((type *)0)->field))

/* IPAC read registers */

#define IPAC_CONF  REG_OFFSET(ipac_reg_t, ipac_rw.ipac_r.ipac_conf)
#define IPAC_ISTA  REG_OFFSET(ipac_reg_t, ipac_rw.ipac_r.ipac_ista)
#define IPAC_ID    REG_OFFSET(ipac_reg_t, ipac_rw.ipac_r.ipac_id)
#define IPAC_ACFG  REG_OFFSET(ipac_reg_t, ipac_rw.ipac_r.ipac_acfg)
#define IPAC_AOE   REG_OFFSET(ipac_reg_t, ipac_rw.ipac_r.ipac_aoe)
#define IPAC_ARX   REG_OFFSET(ipac_reg_t, ipac_rw.ipac_r.ipac_arx)
#define IPAC_PITA1 REG_OFFSET(ipac_reg_t, ipac_rw.ipac_r.ipac_pita1)
#define IPAC_PITA2 REG_OFFSET(ipac_reg_t, ipac_rw.ipac_r.ipac_pita2)
#define IPAC_POTA1 REG_OFFSET(ipac_reg_t, ipac_rw.ipac_r.ipac_pota1)
#define IPAC_POTA2 REG_OFFSET(ipac_reg_t, ipac_rw.ipac_r.ipac_pota2)
#define IPAC_PCFG  REG_OFFSET(ipac_reg_t, ipac_rw.ipac_r.ipac_pcfg)
#define IPAC_SCFG  REG_OFFSET(ipac_reg_t, ipac_rw.ipac_r.ipac_scfg)
#define IPAC_TIMR2 REG_OFFSET(ipac_reg_t, ipac_rw.ipac_r.ipac_timr2)

/* IPAC write registers */

#define IPAC_MASK  REG_OFFSET(ipac_reg_t, ipac_rw.ipac_w.ipac_mask)
#define IPAC_ATX   REG_OFFSET(ipac_reg_t, ipac_rw.ipac_w.ipac_atx)

/* register bits */

#define IPAC_CONF_AMP  0x80
#define IPAC_CONF_CFS  0x40
#define IPAC_CONF_TEM  0x20
#define IPAC_CONF_PDS  0x10
#define IPAC_CONF_IDH  0x08
#define IPAC_CONF_SGO  0x04
#define IPAC_CONF_ODS  0x02
#define IPAC_CONF_IOF  0x01

#define IPAC_ISTA_INT1 0x80
#define IPAC_ISTA_INT0 0x40
#define IPAC_ISTA_ICD  0x20
#define IPAC_ISTA_EXD  0x10
#define IPAC_ISTA_ICA  0x08
#define IPAC_ISTA_EXA  0x04
#define IPAC_ISTA_ICB  0x02
#define IPAC_ISTA_EXB  0x01

#define IPAC_MASK_INT1 0x80
#define IPAC_MASK_INT0 0x40
#define IPAC_MASK_ICD  0x20
#define IPAC_MASK_EXD  0x10
#define IPAC_MASK_ICA  0x08
#define IPAC_MASK_EXA  0x04
#define IPAC_MASK_ICB  0x02
#define IPAC_MASK_EXB  0x01

#define IPAC_ACFG_OD7  0x80
#define IPAC_ACFG_OD6  0x40
#define IPAC_ACFG_OD5  0x20
#define IPAC_ACFG_OD4  0x10
#define IPAC_ACFG_OD3  0x08
#define IPAC_ACFG_OD2  0x04
#define IPAC_ACFG_EL1  0x02
#define IPAC_ACFG_EL2  0x01

#define IPAC_AOE_OE7   0x80
#define IPAC_AOE_OE6   0x40
#define IPAC_AOE_OE5   0x20
#define IPAC_AOE_OE4   0x10
#define IPAC_AOE_OE3   0x08
#define IPAC_AOE_OE2   0x04

#define IPAC_ARX_AR7   0x80
#define IPAC_ARX_AR6   0x40
#define IPAC_ARX_AR5   0x20
#define IPAC_ARX_AR4   0x10
#define IPAC_ARX_AR3   0x08
#define IPAC_ARX_AR2   0x04

#define IPAC_ATX_AT7   0x80
#define IPAC_ATX_AT6   0x40
#define IPAC_ATX_AT5   0x20
#define IPAC_ATX_AT4   0x10
#define IPAC_ATX_AT3   0x08
#define IPAC_ATX_AT2   0x04

#define IPAC_PITA1_ENA  0x80
#define IPAC_PITA1_DUDD 0x40

#define IPAC_PITA2_ENA  0x80
#define IPAC_PITA2_DUDD 0x40

#define IPAC_POTA1_ENA  0x80
#define IPAC_POTA1_DUDD 0x40

#define IPAC_POTA2_ENA  0x80
#define IPAC_POTA2_DUDD 0x40

#define IPAC_PCFG_DPS  0x80
#define IPAC_PCFG_ACL  0x40
#define IPAC_PCFG_LED  0x20
#define IPAC_PCFG_PLD  0x10
#define IPAC_PCFG_FBS  0x08
#define IPAC_PCFG_CSL2 0x04
#define IPAC_PCFG_CSL1 0x02
#define IPAC_PCFG_CSL0 0x01

#define IPAC_SCFG_PRI  0x80
#define IPAC_SCFG_TXD  0x40
#define IPAC_SCFG_TLEN 0x20

#define IPAC_TIMR2_TMD 0x80

#endif /* _I4B_IPAC_H_ */
