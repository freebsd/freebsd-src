/*
 *   Copyright (c) 1996, 1998 Gary Jennejohn. All rights reserved. 
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
 *   3. Neither the name of the author nor the names of any co-contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *   4. Altered versions must be plainly marked as such, and must not be
 *      misrepresented as being the original software and/or documentation.
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
 *	$Id: i4b_isac.h,v 1.2 1999/12/13 21:25:26 hm Exp $ 
 *
 * $FreeBSD: src/sys/i4b/layer1/i4b_isac.h,v 1.6 1999/12/14 20:48:20 hm Exp $
 *
 *      last edit-date: [Mon Dec 13 22:01:25 1999]
 *
 *---------------------------------------------------------------------------*/
 
#ifndef I4B_ISAC_H_
#define I4B_ISAC_H_

/*
 * The ISAC databook specifies a delay of 2.5 DCL clock cycles between
 * writes to the ISAC command register CMDR. This is the delay used to
 * satisfy this requirement.
 */

#define I4B_ISAC_CMDRWRDELAY	30

#if (I4B_ISAC_CMDRWRDELAY > 0)
#define ISACCMDRWRDELAY() DELAY(I4B_ISAC_CMDRWRDELAY)
#else
#warning "I4B_ISAC_CMDRWRDELAY set to 0!"
#define ISACCMDRWRDELAY()
#endif
 
enum ISAC_VERSIONS {
	ISAC_VA,	/* 2085 A1 or A2, 2086/2186 V1.1	*/
	ISAC_VB1,	/* 2085 B1				*/
	ISAC_VB2,	/* 2085 B2				*/
	ISAC_VB3,	/* 2085 B3/V2.3				*/
	ISAC_UNKN	/* unknown version			*/
};

#define ISAC_FIFO_LEN	32	/* 32 bytes FIFO on chip */

/*
 * definitions of registers and bits for the ISAC ISDN chip.
 */
 
typedef struct isac_reg {

	/* 32 byte deep FIFO always first */

	unsigned char isac_fifo [ISAC_FIFO_LEN];

	/* most registers can be read/written, but have different names */
	/* so define a union with read/write names to make that clear */

	union {
		struct {
			unsigned char isac_ista;
			unsigned char isac_star;
			unsigned char isac_mode;
			unsigned char isac_timr;
			unsigned char isac_exir;
			unsigned char isac_rbcl;
			unsigned char isac_sapr;
			unsigned char isac_rsta;
			unsigned char dummy_28;
			unsigned char isac_rhcr;
			unsigned char isac_rbch;
			unsigned char isac_star2;
			unsigned char dummy_2c;
			unsigned char dummy_2d;
			unsigned char dummy_2e;
			unsigned char dummt_2f;
			unsigned char isac_spcr;
			unsigned char isac_cirr;
			unsigned char isac_mor;
			unsigned char isac_sscr;
			unsigned char isac_sfcr;
			unsigned char isac_c1r;
			unsigned char isac_c2r;
			unsigned char isac_b1cr;
			unsigned char isac_b2cr;
			unsigned char isac_adf2;
			unsigned char isac_mosr;
			unsigned char isac_sqrr;
		} isac_r;
		struct {
			unsigned char isac_mask;
			unsigned char isac_cmdr;
			unsigned char isac_mode;
			unsigned char isac_timr;
			unsigned char isac_xad1;
			unsigned char isac_xad2;
			unsigned char isac_sap1;
			unsigned char isac_sap2;
			unsigned char isac_tei1;
			unsigned char isac_tei2;
			unsigned char dummy_2a;
			unsigned char isac_star2;
			unsigned char dummy_2c;
			unsigned char dummy_2d;
			unsigned char dummy_2e;
			unsigned char dummt_2f;
			unsigned char isac_spcr;
			unsigned char isac_cixr;
			unsigned char isac_mox;
			unsigned char isac_sscx;
			unsigned char isac_sfcw;
			unsigned char isac_c1r;
			unsigned char isac_c2r;
			unsigned char isac_stcr;
			unsigned char isac_adf1;
			unsigned char isac_adf2;
			unsigned char isac_mocr;
			unsigned char isac_sqxr;
		} isac_w;
	} isac_rw;
} isac_reg_t;

#define REG_OFFSET(type, field) (int)(&(((type *)0)->field))

/* ISAC read registers */

#define i_ista isac_rw.isac_r.isac_ista
#define I_ISTA REG_OFFSET(isac_reg_t, isac_rw.isac_r.isac_ista)
#define i_star isac_rw.isac_r.isac_star
#define I_STAR REG_OFFSET(isac_reg_t, isac_rw.isac_r.isac_star)
#define i_mode isac_rw.isac_r.isac_mode
#define I_MODE REG_OFFSET(isac_reg_t, isac_rw.isac_r.isac_mode)
#define i_timr isac_rw.isac_r.isac_timr
#define I_TIMR REG_OFFSET(isac_reg_t, isac_rw.isac_r.isac_timr)
#define i_exir isac_rw.isac_r.isac_exir
#define I_EXIR REG_OFFSET(isac_reg_t, isac_rw.isac_r.isac_exir)
#define i_rbcl isac_rw.isac_r.isac_rbcl
#define I_RBCL REG_OFFSET(isac_reg_t, isac_rw.isac_r.isac_rbcl)
#define i_sapr isac_rw.isac_r.isac_sapr
#define I_SAPR REG_OFFSET(isac_reg_t, isac_rw.isac_r.isac_sapr)
#define i_rsta isac_rw.isac_r.isac_rsta
#define I_RSTA REG_OFFSET(isac_reg_t, isac_rw.isac_r.isac_rsta)
#define i_rhcr isac_rw.isac_r.isac_rhcr
#define I_RHCR REG_OFFSET(isac_reg_t, isac_rw.isac_r.isac_rhcr)
#define i_rbch isac_rw.isac_r.isac_rbch
#define I_RBCH REG_OFFSET(isac_reg_t, isac_rw.isac_r.isac_rbch)
#define i_star2 isac_rw.isac_r.isac_star2
#define I_STAR2 REG_OFFSET(isac_reg_t, isac_rw.isac_r.isac_star2)
#define i_spcr isac_rw.isac_r.isac_spcr
#define I_SPCR REG_OFFSET(isac_reg_t, isac_rw.isac_r.isac_spcr)
#define i_cirr isac_rw.isac_r.isac_cirr
#define I_CIRR REG_OFFSET(isac_reg_t, isac_rw.isac_r.isac_cirr)
#define i_mor isac_rw.isac_r.isac_mor
#define I_MOR REG_OFFSET(isac_reg_t, isac_rw.isac_r.isac_mor)
#define i_sscr isac_rw.isac_r.isac_sscr
#define I_SSCR REG_OFFSET(isac_reg_t, isac_rw.isac_r.isac_sscr)
#define i_sfcr isac_rw.isac_r.isac_sfcr
#define I_SFCR REG_OFFSET(isac_reg_t, isac_rw.isac_r.isac_sfcr)
#define i_c1r isac_rw.isac_r.isac_c1r
#define I_C1R REG_OFFSET(isac_reg_t, isac_rw.isac_r.isac_c1r)
#define i_c2r isac_rw.isac_r.isac_c2r
#define I_C2R REG_OFFSET(isac_reg_t, isac_rw.isac_r.isac_c2r)
#define i_b1cr isac_rw.isac_r.isac_b1cr
#define I_B1CR REG_OFFSET(isac_reg_t, isac_rw.isac_r.isac_b1cr)
#define i_b2cr isac_rw.isac_r.isac_b2cr
#define I_B2CR REG_OFFSET(isac_reg_t, isac_rw.isac_r.isac_b2cr)
#define i_adf2 isac_rw.isac_r.isac_adf2
#define I_ADF2 REG_OFFSET(isac_reg_t, isac_rw.isac_r.isac_adf2)
#define i_mosr isac_rw.isac_r.isac_mosr
#define I_MOSR REG_OFFSET(isac_reg_t, isac_rw.isac_r.isac_mosr)
#define i_sqrr isac_rw.isac_r.isac_sqrr
#define I_SQRR REG_OFFSET(isac_reg_t, isac_rw.isac_r.isac_sqrr)

/* ISAC write registers - isac_mode, isac_timr, isac_star2, isac_spcr, */
/* isac_c1r, isac_c2r, isac_adf2 see read registers */

#define i_mask isac_rw.isac_w.isac_mask
#define I_MASK REG_OFFSET(isac_reg_t, isac_rw.isac_w.isac_mask)
#define i_cmdr isac_rw.isac_w.isac_cmdr
#define I_CMDR REG_OFFSET(isac_reg_t, isac_rw.isac_w.isac_cmdr)
#define i_xad1 isac_rw.isac_w.isac_xad1
#define I_XAD1 REG_OFFSET(isac_reg_t, isac_rw.isac_w.isac_xad1)
#define i_xad2 isac_rw.isac_w.isac_xad2
#define I_XAD2 REG_OFFSET(isac_reg_t, isac_rw.isac_w.isac_xad2)
#define i_sap1 isac_rw.isac_w.isac_sap1
#define I_SAP1 REG_OFFSET(isac_reg_t, isac_rw.isac_w.isac_sap1)
#define i_sap2 isac_rw.isac_w.isac_sap2
#define I_SAP2 REG_OFFSET(isac_reg_t, isac_rw.isac_w.isac_sap2)
#define i_tei1 isac_rw.isac_w.isac_tei1
#define i_tei2 isac_rw.isac_w.isac_tei2
#define i_cixr isac_rw.isac_w.isac_cixr
#define I_CIXR REG_OFFSET(isac_reg_t, isac_rw.isac_w.isac_cixr)
#define I_CIX0 I_CIXR
#define i_mox isac_rw.isac_w.isac_mox
#define I_MOX REG_OFFSET(isac_reg_t, isac_rw.isac_w.isac_mox)
#define i_sscx isac_rw.isac_w.isac_sscx
#define I_SSCX REG_OFFSET(isac_reg_t, isac_rw.isac_w.isac_sscx)
#define i_sfcw isac_rw.isac_w.isac_sfcw
#define I_SFCW REG_OFFSET(isac_reg_t, isac_rw.isac_w.isac_sfcw)
#define i_stcr isac_rw.isac_w.isac_stcr
#define I_STCR REG_OFFSET(isac_reg_t, isac_rw.isac_w.isac_stcr)
#define i_adf1 isac_rw.isac_w.isac_adf1
#define I_ADF1 REG_OFFSET(isac_reg_t, isac_rw.isac_w.isac_adf1)
#define i_mocr isac_rw.isac_w.isac_mocr
#define I_MOCR REG_OFFSET(isac_reg_t, isac_rw.isac_w.isac_mocr)
#define i_sqxr isac_rw.isac_w.isac_sqxr
#define I_SQXR REG_OFFSET(isac_reg_t, isac_rw.isac_w.isac_sqxr)

#define ISAC_ISTA_RME  0x80
#define ISAC_ISTA_RPF  0x40
#define ISAC_ISTA_RSC  0x20
#define ISAC_ISTA_XPR  0x10
#define ISAC_ISTA_TIN  0x08
#define ISAC_ISTA_CISQ 0x04
#define ISAC_ISTA_SIN  0x02
#define ISAC_ISTA_EXI  0x01

#define ISAC_MASK_RME  0x80
#define ISAC_MASL_RPF  0x40
#define ISAC_MASK_RSC  0x20
#define ISAC_MASK_XPR  0x10
#define ISAC_MASK_TIN  0x08
#define ISAC_MASK_CISQ 0x04
#define ISAC_MASK_SIN  0x02
#define ISAC_MASK_EXI  0x01
#define ISAC_MASK_ALL  0xff

#define ISAC_STAR_XDOV 0x80
#define ISAC_STAR_XFW  0x40
#define ISAC_STAR_XRNR 0x20
#define ISAC_STAR_RRNR 0x10
#define ISAC_STAR_MBR  0x08
#define ISAC_STAR_MAC1 0x04
#define ISAC_STAR_BVS  0x02
#define ISAC_STAR_MAC0 0x01

#define ISAC_CMDR_RMC  0x80
#define ISAC_CMDR_RRES 0x40
#define ISAC_CMDR_RNR  0x20
#define ISAC_CMDR_STI  0x10
#define ISAC_CMDR_XTF  0x08
#define ISAC_CMDR_XIF  0x04
#define ISAC_CMDR_XME  0x02
#define ISAC_CMDR_XRES 0x01

#define ISAC_MODE_MDS2 0x80
#define ISAC_MODE_MDS1 0x40
#define ISAC_MODE_MDS0 0x20
#define ISAC_MODE_TMD  0x10
#define ISAC_MODE_RAC  0x08
#define ISAC_MODE_DIM2 0x04
#define ISAC_MODE_DIM1 0x02
#define ISAC_MODE_DIM0 0x01

#define ISAC_EXIR_XMR  0x80
#define ISAC_EXIR_XDU  0x40
#define ISAC_EXIR_PCE  0x20
#define ISAC_EXIR_RFO  0x10
#define ISAC_EXIR_SOV  0x08
#define ISAC_EXIR_MOS  0x04
#define ISAC_EXIR_SAW  0x02
#define ISAC_EXIR_WOV  0x01

#define ISAC_RSTA_RDA  0x80
#define ISAC_RSTA_RDO  0x40
#define ISAC_RSTA_CRC  0x20
#define ISAC_RSTA_RAB  0x10
#define ISAC_RSTA_SA1  0x08
#define ISAC_RSTA_SA0  0x04
#define ISAC_RSTA_CR   0x02
#define ISAC_RSTA_TA   0x01

#define ISAC_RSTA_MASK 0x70	/* the interesting bits */

#define ISAC_RBCH_XAC  0x80
#define ISAC_RBCH_VN1  0x40
#define ISAC_RBCH_VN0  0x20
#define ISAC_RBCH_OV   0x10
/* the other 4 bits are the high bits of the receive byte count */

#define ISAC_SPCR_SPU  0x80
#define ISAC_SPCR_SAC  0x40
#define ISAC_SPCR_SPM  0x20
#define ISAC_SPCR_TLP  0x10
#define ISAC_SPCR_C1C1 0x08
#define ISAC_SPCR_C1C0 0x04
#define ISAC_SPCR_C2C1 0x02
#define ISAC_SPCR_C2C0 0x01

#define ISAC_CIRR_SQC  0x80
#define ISAC_CIRR_BAS  0x40
/* bits 5-2 CODR */
#define ISAC_CIRR_CIC0 0x02
/* bit 0 is always 0 */
/* C/I codes from bits 5-2 (>> 2 & 0xf) */
/* the indications */
#define ISAC_CIRR_IPU   0x07
#define ISAC_CIRR_IDR   0x00
#define ISAC_CIRR_ISD   0x02
#define ISAC_CIRR_IDIS  0x03
#define ISAC_CIRR_IEI   0x06
#define ISAC_CIRR_IRSY  0x04
#define ISAC_CIRR_IARD  0x08
#define ISAC_CIRR_ITI   0x0a
#define ISAC_CIRR_IATI  0x0b
#define ISAC_CIRR_IAI8  0x0c
#define ISAC_CIRR_IAI10 0x0d
#define ISAC_CIRR_IDID  0x0f

#define ISAC_CI_MASK	0x0f

#define ISAC_CIXR_RSS  0x80
#define ISAC_CIXR_BAC  0x40
/* bits 5-2 CODX */
#define ISAC_CIXR_TCX  0x02
#define ISAC_CIXR_ECX  0x01
/* in IOM-2 mode the low bits are always 1 */
#define ISAC_CIX0_LOW  0x03
/* C/I codes from bits 5-2 (>> 2 & 0xf) */
/* the commands */
#define ISAC_CIXR_CTIM  0
#define ISAC_CIXR_CRS   0x01
#define ISAC_CIXR_CSCZ  0x04
#define ISAC_CIXR_CSSZ  0x02
#define ISAC_CIXR_CAR8  0x08
#define ISAC_CIXR_CAR10 0x09
#define ISAC_CIXR_CARL  0x0a
#define ISAC_CIXR_CDIU  0x0f

#define ISAC_STCR_TSF  0x80
#define ISAC_STCR_TBA2 0x40
#define ISAC_STCR_TBA1 0x20
#define ISAC_STCR_TBA0 0x10
#define ISAC_STCR_ST1  0x08
#define ISAC_STCR_ST0  0x04
#define ISAC_STCR_SC1  0x02
#define ISAC_STCR_SC0  0x01

#define ISAC_ADF1_WTC1 0x80
#define ISAC_ADF1_WTC2 0x40
#define ISAC_ADF1_TEM  0x20
#define ISAC_ADF1_PFS  0x10
#define ISAC_ADF1_CFS  0x08
#define ISAC_ADF1_FC2  0x04
#define ISAC_ADF1_FC1  0x02
#define ISAC_ADF1_ITF  0x01

#define ISAC_ADF2_IMS  0x80
/* all other bits are 0 */

/* bits 7-5 are always 0 */
#define ISAC_SQRR_SYN  0x10
#define ISAC_SQRR_SQR1 0x08
#define ISAC_SQRR_SQR2 0x04
#define ISAC_SQRR_SQR3 0x02
#define ISAC_SQRR_SQR4 0x01

#define ISAC_SQXR_IDC  0x80
#define ISAC_SQXR_CFS  0x40
#define ISAC_SQXR_CI1E 0x20
#define ISAC_SQXR_SQIE 0x10
#define ISAC_SQXR_SQX1 0x08
#define ISAC_SQXR_SQX2 0x04
#define ISAC_SQXR_SQX3 0x02
#define ISAC_SQXR_SQX4 0x01

#endif /* I4B_ISAC_H_ */
