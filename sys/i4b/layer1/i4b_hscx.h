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
 *	$Id: i4b_hscx.h,v 1.2 1999/12/13 21:25:26 hm Exp $ 
 *
 * $FreeBSD: src/sys/i4b/layer1/i4b_hscx.h,v 1.6 1999/12/14 20:48:20 hm Exp $
 *
 *      last edit-date: [Mon Dec 13 22:00:49 1999]
 *
 *---------------------------------------------------------------------------*/
 
#ifndef I4B_HSCX_H_
#define I4B_HSCX_H_

enum HSCX_VERSIONS {
	HSCX_VA1,	/* 82525 A1  */
	HSCX_UNKN1,	/* unknown 1 */	
	HSCX_VA2,	/* 82525 A2  */
	HSCX_UNKN3,	/* unknown 3 */
	HSCX_VA3,	/* 82525 A3  */
	HSCX_V21,	/* 82525 2.1 */	
	HSCX_UNKN	/* unknown version */
};

#define HSCX_CH_A	0	/* channel A */
#define HSCX_CH_B	1	/* channel B */

#define HSCX_FIFO_LEN	32	/* 32 bytes FIFO on chip */

/*
 * definitions of registers and bits for the HSCX ISDN chip.
 */
 
typedef struct hscx_reg {

	/* 32 byte deep FIFO always first */

	unsigned char hscx_fifo [HSCX_FIFO_LEN];

	/* most registers can be read/written, but have different names */
	/* so define a union with read/write names to make that clear */

	union {
		struct {
			unsigned char hscx_ista;
			unsigned char hscx_star;
			unsigned char hscx_mode;
			unsigned char hscx_timr;
			unsigned char hscx_exir;
			unsigned char hscx_rbcl;
			unsigned char dummy_26;
			unsigned char hscx_rsta;
			unsigned char hscx_ral1;
			unsigned char hscx_rhcr;
			unsigned char dummy_2a;
			unsigned char dummy_2b;
			unsigned char hscx_ccr2;
			unsigned char hscx_rbch;
			unsigned char hscx_vstr;
			unsigned char hscx_ccr;
			unsigned char dummy_30;
			unsigned char dummy_31;
			unsigned char dummy_32;
			unsigned char dummy_33;
		} hscx_r;
		struct {
			unsigned char hscx_mask;
			unsigned char hscx_cmdr;
			unsigned char hscx_mode;
			unsigned char hscx_timr;
			unsigned char hscx_xad1;
			unsigned char hscx_xad2;
			unsigned char hscx_rah1;
			unsigned char hscx_rah2;
			unsigned char hscx_ral1;
			unsigned char hscx_ral2;
			unsigned char hscx_xbcl;
			unsigned char hscx_bgr;
			unsigned char hscx_ccr2;
			unsigned char hscx_xbch;
			unsigned char hscx_rlcr;
			unsigned char hscx_ccr1;
			unsigned char hscx_tsax;
			unsigned char hscx_tsar;
			unsigned char hscx_xccr;
			unsigned char hscx_rccr;
		} hscx_w;
	} hscx_rw;
} hscx_reg_t;

#define REG_OFFSET(type, field) (int)(&(((type *)0)->field))

/* HSCX read registers */

#define h_ista hscx_rw.hscx_r.hscx_ista
#define H_ISTA REG_OFFSET(hscx_reg_t, hscx_rw.hscx_r.hscx_ista)
#define h_star hscx_rw.hscx_r.hscx_star
#define H_STAR REG_OFFSET(hscx_reg_t, hscx_rw.hscx_r.hscx_star)
#define h_mode hscx_rw.hscx_r.hscx_mode
#define H_MODE REG_OFFSET(hscx_reg_t, hscx_rw.hscx_r.hscx_mode)
#define h_timr hscx_rw.hscx_r.hscx_timr
#define H_TIMR REG_OFFSET(hscx_reg_t, hscx_rw.hscx_r.hscx_timr)
#define h_exir hscx_rw.hscx_r.hscx_exir
#define H_EXIR REG_OFFSET(hscx_reg_t, hscx_rw.hscx_r.hscx_exir)
#define h_rbcl hscx_rw.hscx_r.hscx_rbcl
#define H_RBCL REG_OFFSET(hscx_reg_t, hscx_rw.hscx_r.hscx_rbcl)
#define h_rsta hscx_rw.hscx_r.hscx_rsta
#define H_RSTA REG_OFFSET(hscx_reg_t, hscx_rw.hscx_r.hscx_rsta)
#define h_ral1 hscx_rw.hscx_r.hscx_ral1
#define H_RAL1 REG_OFFSET(hscx_reg_t, hscx_rw.hscx_r.hscx_ral1)
#define h_rhcr hscx_rw.hscx_r.hscx_rhcr
#define H_RHCR REG_OFFSET(hscx_reg_t, hscx_rw.hscx_r.hscx_rhcr)
#define h_ccr2 hscx_rw.hscx_r.hscx_ccr2
#define H_CCR2 REG_OFFSET(hscx_reg_t, hscx_rw.hscx_r.hscx_ccr2)
#define h_rbch hscx_rw.hscx_r.hscx_rbch
#define H_RBCH REG_OFFSET(hscx_reg_t, hscx_rw.hscx_r.hscx_rbch)
#define h_vstr hscx_rw.hscx_r.hscx_vstr
#define H_VSTR REG_OFFSET(hscx_reg_t, hscx_rw.hscx_r.hscx_vstr)
#define h_ccr hscx_rw.hscx_r.hscx_ccr
#define H_CCR REG_OFFSET(hscx_reg_t, hscx_rw.hscx_r.hscx_ccr)

/* HSCX write registers - for hscx_mode, hscx_timr, hscx_ral1, hscx_ccr2 */
/* see read registers */

#define h_mask hscx_rw.hscx_w.hscx_mask
#define H_MASK REG_OFFSET(hscx_reg_t, hscx_rw.hscx_w.hscx_mask)
#define h_cmdr hscx_rw.hscx_w.hscx_cmdr
#define H_CMDR REG_OFFSET(hscx_reg_t, hscx_rw.hscx_w.hscx_cmdr)
#define h_xad1 hscx_rw.hscx_w.hscx_xad1
#define H_XAD1 REG_OFFSET(hscx_reg_t, hscx_rw.hscx_w.hscx_xad1)
#define h_xad2 hscx_rw.hscx_w.hscx_xad2
#define H_XAD2 REG_OFFSET(hscx_reg_t, hscx_rw.hscx_w.hscx_xad2)
#define h_rah1 hscx_rw.hscx_w.hscx_rah1
#define H_RAH1 REG_OFFSET(hscx_reg_t, hscx_rw.hscx_w.hscx_rah1)
#define h_rah2 hscx_rw.hscx_w.hscx_rah2
#define H_RAH2 REG_OFFSET(hscx_reg_t, hscx_rw.hscx_w.hscx_rah2)
#define h_ral2 hscx_rw.hscx_w.hscx_ral2
#define H_RAL2 REG_OFFSET(hscx_reg_t, hscx_rw.hscx_w.hscx_ral2)
#define h_xbcl hscx_rw.hscx_w.hscx_xbcl
#define H_XBCL REG_OFFSET(hscx_reg_t, hscx_rw.hscx_w.hscx_xbcl)
#define h_bgr hscx_rw.hscx_w.hscx_bgr
#define H_BGR REG_OFFSET(hscx_reg_t, hscx_rw.hscx_w.hscx_bgr)
#define h_xbch hscx_rw.hscx_w.hscx_xbch
#define H_XBCH REG_OFFSET(hscx_reg_t, hscx_rw.hscx_w.hscx_xbch)
#define h_rlcr hscx_rw.hscx_w.hscx_rlcr
#define H_RLCR REG_OFFSET(hscx_reg_t, hscx_rw.hscx_w.hscx_rlcr)
#define h_ccr1 hscx_rw.hscx_w.hscx_ccr1
#define H_CCR1 REG_OFFSET(hscx_reg_t, hscx_rw.hscx_w.hscx_ccr1)
#define h_tsax hscx_rw.hscx_w.hscx_tsax
#define H_TSAX REG_OFFSET(hscx_reg_t, hscx_rw.hscx_w.hscx_tsax)
#define h_tsar hscx_rw.hscx_w.hscx_tsar
#define H_TSAR REG_OFFSET(hscx_reg_t, hscx_rw.hscx_w.hscx_tsar)
#define h_xccr hscx_rw.hscx_w.hscx_xccr
#define H_XCCR REG_OFFSET(hscx_reg_t, hscx_rw.hscx_w.hscx_xccr)
#define h_rccr hscx_rw.hscx_w.hscx_rccr
#define H_RCCR REG_OFFSET(hscx_reg_t, hscx_rw.hscx_w.hscx_rccr)

#define HSCX_ISTA_RME  0x80
#define HSCX_ISTA_RPF  0x40
#define HSCX_ISTA_RSC  0x20
#define HSCX_ISTA_XPR  0x10
#define HSCX_ISTA_TIN  0x08
#define HSCX_ISTA_ICA  0x04
#define HSCX_ISTA_EXA  0x02
#define HSCX_ISTA_EXB  0x01

#define HSCX_MASK_RME  0x80
#define HSCX_MASK_RPF  0x40
#define HSCX_MASK_RSC  0x20
#define HSCX_MASK_XPR  0x10
#define HSCX_MASK_TIN  0x08
#define HSCX_MASK_ICA  0x04
#define HSCX_MASK_EXA  0x02
#define HSCX_MASK_EXB  0x01

#define HSCX_EXIR_XMR  0x80
#define HSCX_EXIR_XDU  0x40
#define HSCX_EXIR_PCE  0x20
#define HSCX_EXIR_RFO  0x10
#define HSCX_EXIR_CSC  0x08
#define HSCX_EXIR_RFS  0x04

/* the other bits are always 0 */

#define HSCX_STAR_XDOV 0x80
#define HSCX_STAR_XFW  0x40
#define HSCX_STAR_XRNR 0x20
#define HSCX_STAR_RRNR 0x10
#define HSCX_STAR_RLI  0x08
#define HSCX_STAR_CEC  0x04
#define HSCX_STAR_CTS  0x02
#define HSCX_STAR_WFA  0x01

#define HSCX_CMDR_RMC  0x80
#define HSCX_CMDR_RHR  0x40
/* also known as XREP in transparent mode */
#define HSCX_CMDR_RNR  0x20
#define HSCX_CMDR_STI  0x10
#define HSCX_CMDR_XTF  0x08
#define HSCX_CMDR_XIF  0x04
#define HSCX_CMDR_XME  0x02
#define HSCX_CMDR_XRES 0x01

#define HSCX_MODE_MDS1 0x80
#define HSCX_MODE_MDS0 0x40
#define HSCX_MODE_ADM  0x20
#define HSCX_MODE_TMD  0x10
#define HSCX_MODE_RAC  0x08
#define HSCX_MODE_RTS  0x04
#define HSCX_MODE_TRS  0x02
#define HSCX_MODE_TLP  0x01

#define HSCX_RSTA_VFR  0x80
#define HSCX_RSTA_RDO  0x40
#define HSCX_RSTA_CRC  0x20
#define HSCX_RSTA_RAB  0x10
#define HSCX_RSTA_HA1  0x08
#define HSCX_RSTA_HA0  0x04
#define HSCX_RSTA_CR   0x02
#define HSCX_RSTA_LA   0x01

#define HSCX_RSTA_MASK 0xf0	/* the interesting ones */

/* only used in DMA mode */
#define HSCX_XBCH_DMA  0x80
#define HSCX_XBCH_NRM  0x40
#define HSCX_XBCH_CAS  0x20
#define HSCX_XBCH_XC   0x10
/* the rest are bits 11 thru 8 of the byte count */

#define HSCX_RBCH_DMA  0x80
#define HSCX_RBCH_NRM  0x40
#define HSCX_RBCH_CAS  0x20
#define HSCX_RBCH_OV   0x10
/* the rest are bits 11 thru 8 of the byte count */

#define HSCX_VSTR_CD   0x80
/* bits 6 thru 4 are 0 */
/* bits 3 thru 0 are the version number */

#define HSCX_RLCR_RC   0x80
/* the rest of the bits are used to set the received length */

#define HSCX_CCR1_PU   0x80
/* bits 6 and 5 are SC1 SC0 */
#define HSCX_CCR1_ODS  0x10
#define HSCX_CCR1_ITF  0x08
#define HSCX_CCR1_CM2  0x04
#define HSCX_CCR1_CM1  0x02
#define HSCX_CCR1_CM0  0x01

/* for clock mode 5 */
#define HSCX_CCR2_SOC2 0x80
#define HSCX_CCR2_SOC1 0x40
#define HSCX_CCR2_XCS0 0x20
#define HSCX_CCR2_RCS0 0x10
#define HSCX_CCR2_TIO  0x08
#define HSCX_CCR2_CIE  0x04
#define HSCX_CCR2_RIE  0x02
#define HSCX_CCR2_DIV  0x01

/* bits 7 thru 2 are TSNX */
#define HSCX_TSAX_XCS2 0x02
#define HSCX_TSAX_XCS1 0x01

/* bits 7 thru 2 are TSNR */
#define HSCX_TSAR_RCS2 0x02
#define HSCX_TSAR_RCS1 0x01

#endif /* I4B_HSCX_H_ */
