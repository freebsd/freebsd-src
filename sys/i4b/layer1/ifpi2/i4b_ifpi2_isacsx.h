/*-
 *   Copyright (c) 2001 Gary Jennejohn. All rights reserved. 
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
 */

/*---------------------------------------------------------------------------
 *
 * $FreeBSD: src/sys/i4b/layer1/ifpi2/i4b_ifpi2_isacsx.h,v 1.2 2005/01/06 22:18:19 imp Exp $
 *
 *
 *---------------------------------------------------------------------------*/
 
#ifndef I4B_ISACSX_H_
#define I4B_ISACSX_H_

#define ISACSX_FIFO_LEN	32	/* 32 bytes FIFO on chip */

#define ISACSX_V13 0x01

/*
 * definitions of registers and bits for the ISAC-SX ISDN chip.
 */
 
typedef struct isacsx_reg {

	/* 32 byte deep FIFO always first */

	unsigned char isacsx_fifo [ISACSX_FIFO_LEN];

	/* most registers can be read/written, but have different names */
	/* so define a union with read/write names to make that clear */

	union {
		struct {
			unsigned char isacsx_istad;
			unsigned char isacsx_stard;
			unsigned char isacsx_moded;
			unsigned char isacsx_exmd1;
			unsigned char isacsx_timr1;
			unsigned char dummy_25;
			unsigned char isacsx_rbcld;
			unsigned char isacsx_rbchd;
			unsigned char isacsx_rstad;
			unsigned char isacsx_tmd;
			unsigned char dummy_2a;
			unsigned char dummy_2b;
			unsigned char dummy_2c;
			unsigned char dummy_2d;
			unsigned char isacsx_cir0;
			unsigned char isacsx_codr1;
			unsigned char isacsx_tr_conf0;
			unsigned char isacsx_tr_conf1;
			unsigned char isacsx_tr_conf2;
			unsigned char isacsx_tr_sta;
			unsigned char dummy_34;
			unsigned char isacsx_sqrr1;
			unsigned char isacsx_sqrr2;
			unsigned char isacsx_sqrr3;
			unsigned char isacsx_istatr;
			unsigned char isacsx_masktr;
			unsigned char dummy_3a;
			unsigned char dummy_3b;
			unsigned char isacsx_acgf2;
			unsigned char dummy_3d;
			unsigned char dummy_3e;
			unsigned char dummy_3f;
			unsigned char isacsx_cda10;
			unsigned char isacsx_cda11;
			unsigned char isacsx_cda20;
			unsigned char isacsx_cda21;
			unsigned char isacsx_cda_tsdp10;
			unsigned char isacsx_cda_tsdp11;
			unsigned char isacsx_cda_tsdp20;
			unsigned char isacsx_cda_tsdp21;
			unsigned char dummy_48;
			unsigned char dummy_49;
			unsigned char dummy_4a;
			unsigned char dummy_4b;
			unsigned char isacsx_tr_tsdp_bc1;
			unsigned char isacsx_tr_tsdp_bc2;
			unsigned char isacsx_cda1_cr;
			unsigned char isacsx_cda2_cr;
			unsigned char isacsx_tr_cr;
			unsigned char dummy_51;
			unsigned char dummy_52;
			unsigned char isacsx_dci_cr;
			unsigned char isacsx_mon_cr;
			unsigned char isacsx_sds_cr;
			unsigned char dummy_56;
			unsigned char isacsx_iom_cr;
			unsigned char isacsx_sti;
			unsigned char isacsx_msti;
			unsigned char isacsx_sds_conf;
			unsigned char isacsx_mcda;
			unsigned char isacsx_mor;
			unsigned char isacsx_mosr;
			unsigned char isacsx_mocr;
			unsigned char isacsx_msta;
			unsigned char isacsx_ista;
			unsigned char isacsx_auxi;
			unsigned char isacsx_mode1;
			unsigned char isacsx_mode2;
			unsigned char isacsx_id;
			unsigned char isacsx_timr2;
			unsigned char dummy_66;
			unsigned char dummy_67;
			unsigned char dummy_68;
			unsigned char dummy_69;
			unsigned char dummy_6a;
			unsigned char dummy_6b;
			unsigned char dummy_6c;
			unsigned char dummy_6d;
			unsigned char dummy_6e;
			unsigned char dummy_6f;
		} isacsx_r;
		struct {
			unsigned char isacsx_maskd;
			unsigned char isacsx_cmdrd;
			unsigned char isacsx_moded;
			unsigned char isacsx_exmd1;
			unsigned char isacsx_timr1;
			unsigned char isacsx_sap1;
			unsigned char isacsx_sap2;
			unsigned char isacsx_tei1;
			unsigned char isacsx_tei2;
			unsigned char isacsx_tmd;
			unsigned char dummy_2a;
			unsigned char dummy_2b;
			unsigned char dummy_2c;
			unsigned char dummy_2d;
			unsigned char isacsx_cix0;
			unsigned char isacsx_codx1;
			unsigned char isacsx_tr_conf0;
			unsigned char isacsx_tr_conf1;
			unsigned char isacsx_tr_conf2;
			unsigned char dummy_33;
			unsigned char dummy_34;
			unsigned char isacsx_sqrx1;
			unsigned char dummy_36;
			unsigned char dummy_37;
			unsigned char dummy_38;
			unsigned char isacsx_masktr;
			unsigned char dummy_3a;
			unsigned char dummy_3b;
			unsigned char isacsx_acgf2;
			unsigned char dummy_3d;
			unsigned char dummy_3e;
			unsigned char dummy_3f;
			unsigned char isacsx_cda10;
			unsigned char isacsx_cda11;
			unsigned char isacsx_cda20;
			unsigned char isacsx_cda21;
			unsigned char isacsx_cda_tsdp10;
			unsigned char isacsx_cda_tsdp11;
			unsigned char isacsx_cda_tsdp20;
			unsigned char isacsx_cda_tsdp21;
			unsigned char dummy_48;
			unsigned char dummy_49;
			unsigned char dummy_4a;
			unsigned char dummy_4b;
			unsigned char isacsx_tr_tsdp_bc1;
			unsigned char isacsx_tr_tsdp_bc2;
			unsigned char isacsx_cda1_cr;
			unsigned char isacsx_cda2_cr;
			unsigned char isacsx_tr_cr;
			unsigned char dummy_51;
			unsigned char dummy_52;
			unsigned char isacsx_dci_cr;
			unsigned char isacsx_mon_cr;
			unsigned char isacsx_sds_cr;
			unsigned char dummy_56;
			unsigned char isacsx_iom_cr;
			unsigned char isacsx_asti;
			unsigned char isacsx_msti;
			unsigned char isacsx_sds_conf;
			unsigned char dummy_5b;
			unsigned char isacsx_mox;
			unsigned char dummy_5d;
			unsigned char isacsx_mocr;
			unsigned char isacsx_mconf;
			unsigned char isacsx_mask;
			unsigned char isacsx_auxm;
			unsigned char isacsx_mode1;
			unsigned char isacsx_mode2;
			unsigned char isacsx_sres;
			unsigned char isacsx_timr2;
			unsigned char dummy_66;
			unsigned char dummy_67;
			unsigned char dummy_68;
			unsigned char dummy_69;
			unsigned char dummy_6a;
			unsigned char dummy_6b;
			unsigned char dummy_6c;
			unsigned char dummy_6d;
			unsigned char dummy_6e;
			unsigned char dummy_6f;
		} isacsx_w;
	} isacsx_rw;
} isacsx_reg_t;

#define REG_OFFSET(type, field) (int)(&(((type *)0)->field))

/* ISACSX read registers */

#define i_istad isacsx_rw.isacsx_r.isacsx_istad
#define I_ISTAD REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_istad)
#define i_stard isacsx_rw.isacsx_r.isacsx_stard
#define I_STARD REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_stard)
#define i_rmoded isacsx_rw.isacsx_r.isacsx_moded
#define I_RMODED REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_moded)
#define i_rexmd1 isacsx_rw.isacsx_r.isacsx_exmd1
#define I_REXMD1 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_exmd1)
#define i_rtimr1 isacsx_rw.isacsx_r.isacsx_timr1
#define I_RTIMR1 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_timr1)
#define i_rbcld isacsx_rw.isacsx_r.isacsx_rbcld
#define I_RBCLD REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_rbcld)
#define i_rbchd isacsx_rw.isacsx_r.isacsx_rbchd
#define I_RBCHD REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_rbchd)
#define i_rstad isacsx_rw.isacsx_r.isacsx_rstad
#define I_RSTAD REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_rstad)
#define i_rtmd isacsx_rw.isacsx_r.isacsx_tmd
#define I_RTMD REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_tmd)
#define i_cir0 isacsx_rw.isacsx_r.isacsx_cir0
#define I_CIR0 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_cir0)
#define i_codr1 isacsx_rw.isacsx_r.isacsx_codr1
#define I_CODR1 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_codr1)
#define i_rtr_conf0 isacsx_rw.isacsx_r.isacsx_tr_conf0
#define I_RTR_CONF0 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_tr_conf0)
#define i_rtr_conf1 isacsx_rw.isacsx_r.isacsx_tr_conf1
#define I_RTR_CONF1 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_tr_conf1)
#define i_rtr_conf2 isacsx_rw.isacsx_r.isacsx_tr_conf2
#define I_RTR_CONF2 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_tr_conf2)
#define i_sta isacsx_rw.isacsx_r.isacsx_sta
#define I_STA REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_sta)
#define i_sqrr1 isacsx_rw.isacsx_r.isacsx_sqrr1
#define I_SQRR1 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_sqrr1)
#define i_sqrr2 isacsx_rw.isacsx_r.isacsx_sqrr2
#define I_SQRR2 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_sqrr2)
#define i_sqrr3 isacsx_rw.isacsx_r.isacsx_sqrr3
#define I_SQRR3 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_sqrr3)
#define i_istatr isacsx_rw.isacsx_r.isacsx_istatr
#define I_ISTATR REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_istatr)
#define i_rmasktr isacsx_rw.isacsx_r.isacsx_masktr
#define I_RMASKTR REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_masktr)
#define i_racgf2 isacsx_rw.isacsx_r.isacsx_acgf2
#define I_RACGF2 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_acgf2)
#define i_rcda10 isacsx_rw.isacsx_r.isacsx_cda10
#define I_RCDA10 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_cda10)
#define i_rcda11 isacsx_rw.isacsx_r.isacsx_cda11
#define I_RCDA11 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_cda11)
#define i_rcda20 isacsx_rw.isacsx_r.isacsx_cda20
#define I_RCDA20 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_cda20)
#define i_rcda21 isacsx_rw.isacsx_r.isacsx_cda21
#define I_RCDA21 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_cda21)
#define i_cda_tsdp10 isacsx_rw.isacsx_r.isacsx_cda_tsdp10
#define I_CDA_TSDP10 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_cda_tsdp10)
#define i_cda_tsdp11 isacsx_rw.isacsx_r.isacsx_cda_tsdp11
#define I_CDA_TSDP11 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_cda_tsdp11)
#define i_cda_tsdp20 isacsx_rw.isacsx_r.isacsx_cda_tsdp20
#define I_CDA_TSDP20 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_cda_tsdp20)
#define i_cda_tsdp21 isacsx_rw.isacsx_r.isacsx_cda_tsdp21
#define I_CDA_TSDP21 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_cda_tsdp21)
#define i_tr_tsdp_bc1 isacsx_rw.isacsx_r.isacsx_tr_tsdp_bc1
#define I_TR_TSDP_BC1 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_tr_tsdp_bc1)
#define i_tr_tsdp_bc2 isacsx_rw.isacsx_r.isacsx_tr_tsdp_bc2
#define I_TR_TSDP_BC2 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_tr_tsdp_bc2)
#define i_cda1_cr isacsx_rw.isacsx_r.isacsx_cda1_cr
#define I_CDA1_CR REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_cda1_cr)
#define i_cda2_cr isacsx_rw.isacsx_r.isacsx_cda2_cr
#define I_CDA2_CR REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_cda2_cr)
#define i_tr_cr isacsx_rw.isacsx_r.isacsx_tr_cr
#define I_TR_CR REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_tr_cr)
#define i_dci_cr isacsx_rw.isacsx_r.isacsx_dci_cr
#define I_DCI_CR REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_dci_cr)
#define i_mon_cr isacsx_rw.isacsx_r.isacsx_mon_cr
#define I_MON_CR REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_mon_cr)
#define i_sds_cr isacsx_rw.isacsx_r.isacsx_sds_cr
#define I_SDS_CR REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_sds_cr)
#define i_iom_cr isacsx_rw.isacsx_r.isacsx_iom_cr
#define I_IOM_CR REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_iom_cr)
#define i_sti isacsx_rw.isacsx_r.isacsx_sti
#define I_STI REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_sti)
#define i_msti isacsx_rw.isacsx_r.isacsx_msti
#define I_MSTI REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_msti)
#define i_sds_conf isacsx_rw.isacsx_r.isacsx_sds_conf
#define I_SDS_CONF REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_sds_conf)
#define i_mcda isacsx_rw.isacsx_r.isacsx_mcda
#define I_MCDA REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_mcda)
#define i_mor isacsx_rw.isacsx_r.isacsx_mor
#define I_MOR REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_mor)
#define i_mosr isacsx_rw.isacsx_r.isacsx_mosr
#define I_MOSR REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_mosr)
#define i_rmocr isacsx_rw.isacsx_r.isacsx_mocr
#define I_RMOCR REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_mocr)
#define i_msta isacsx_rw.isacsx_r.isacsx_msta
#define I_MSTA REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_msta)
#define i_ista isacsx_rw.isacsx_r.isacsx_ista
#define I_ISTA REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_ista)
#define i_auxi isacsx_rw.isacsx_r.isacsx_auxi
#define I_AUXI REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_auxi)
#define i_rmode1 isacsx_rw.isacsx_r.isacsx_mode1
#define I_RMODE1 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_mode1)
#define i_rmode2 isacsx_rw.isacsx_r.isacsx_mode2
#define I_RMODE2 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_mode2)
#define i_id isacsx_rw.isacsx_r.isacsx_id
#define I_ID REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_id)
#define i_rtimr2 isacsx_rw.isacsx_r.isacsx_timr2
#define I_RTIMR2 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_timr2)

/* ISAC write registers - isacsx_mode, isacsx_timr, isacsx_star2, isacsx_spcr, */
/* isacsx_c1r, isacsx_c2r, isacsx_adf2 see read registers */

#define i_maskd isacsx_rw.isacsx_w.isacsx_maskd
#define I_MASKD REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_w.isacsx_maskd)
#define i_cmdrd isacsx_rw.isacsx_w.isacsx_cmdrd
#define I_CMDRD REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_w.isacsx_cmdrd)
#define i_wmoded isacsx_rw.isacsx_w.isacsx_moded
#define I_WMODED REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_w.isacsx_moded)
#define i_wexmd1 isacsx_rw.isacsx_w.isacsx_exmd1
#define I_WEXMD1 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_w.isacsx_exmd1)
#define i_wtimr1 isacsx_rw.isacsx_w.isacsx_timr1
#define I_WTIMR1 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_w.isacsx_timr1)
#define i_sap1 isacsx_rw.isacsx_w.isacsx_sap1
#define I_SAP1 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_w.isacsx_sap1)
#define i_sap2 isacsx_rw.isacsx_w.isacsx_sap2
#define I_SAP2 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_w.isacsx_sap2)
#define i_tei1 isacsx_rw.isacsx_w.isacsx_tei1
#define i_tei2 isacsx_rw.isacsx_w.isacsx_tei2
#define i_wtmd isacsx_rw.isacsx_w.isacsx_tmd
#define I_WTMD REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_w.isacsx_tmd)
#define i_cix0 isacsx_rw.isacsx_w.isacsx_cix0
#define I_CIX0 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_w.isacsx_cix0)
#define i_codx1 isacsx_rw.isacsx_w.isacsx_codx1
#define I_CODX1 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_w.isacsx_codx1)
#define i_wtr_conf0 isacsx_rw.isacsx_w.isacsx_tr_conf0
#define I_WTR_CONF0 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_w.isacsx_tr_conf0)
#define i_wtr_conf1 isacsx_rw.isacsx_w.isacsx_tr_conf1
#define I_WTR_CONF1 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_w.isacsx_tr_conf1)
#define i_wtr_conf2 isacsx_rw.isacsx_w.isacsx_tr_conf2
#define I_WTR_CONF2 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_w.isacsx_tr_conf2)
#define i_sqrx1 isacsx_rw.isacsx_w.isacsx_sqrx1
#define I_SQRX1 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_w.isacsx_sqrx1)
#define i_wmasktr isacsx_rw.isacsx_w.isacsx_masktr
#define I_WMASKTR REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_w.isacsx_masktr)
#define i_wacgf2 isacsx_rw.isacsx_w.isacsx_acgf2
#define I_WACGF2 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_w.isacsx_acgf2)
#define i_wcda10 isacsx_rw.isacsx_w.isacsx_cda10
#define I_WCDA10 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_w.isacsx_cda10)
#define i_wcda11 isacsx_rw.isacsx_r.isacsx_cda11
#define I_WCDA11 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_cda11)
#define i_wcda20 isacsx_rw.isacsx_r.isacsx_cda20
#define I_WCDA20 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_cda20)
#define i_wcda21 isacsx_rw.isacsx_r.isacsx_cda21
#define I_WCDA21 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_cda21)
#define i_cda_tsdp10 isacsx_rw.isacsx_r.isacsx_cda_tsdp10
#define I_CDA_TSDP10 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_cda_tsdp10)
#define i_cda_tsdp11 isacsx_rw.isacsx_r.isacsx_cda_tsdp11
#define I_CDA_TSDP11 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_cda_tsdp11)
#define i_cda_tsdp20 isacsx_rw.isacsx_r.isacsx_cda_tsdp20
#define I_CDA_TSDP20 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_cda_tsdp20)
#define i_cda_tsdp21 isacsx_rw.isacsx_r.isacsx_cda_tsdp21
#define I_CDA_TSDP21 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_cda_tsdp21)
#define i_tr_tsdp_bc1 isacsx_rw.isacsx_r.isacsx_tr_tsdp_bc1
#define I_TR_TSDP_BC1 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_tr_tsdp_bc1)
#define i_tr_tsdp_bc2 isacsx_rw.isacsx_r.isacsx_tr_tsdp_bc2
#define I_TR_TSDP_BC2 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_tr_tsdp_bc2)
#define i_cda1_cr isacsx_rw.isacsx_r.isacsx_cda1_cr
#define I_CDA1_CR REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_cda1_cr)
#define i_cda2_cr isacsx_rw.isacsx_r.isacsx_cda2_cr
#define I_CDA2_CR REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_cda2_cr)
#define i_tr_cr isacsx_rw.isacsx_r.isacsx_tr_cr
#define I_TR_CR REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_tr_cr)
#define i_dci_cr isacsx_rw.isacsx_r.isacsx_dci_cr
#define I_DCI_CR REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_dci_cr)
#define i_mon_cr isacsx_rw.isacsx_r.isacsx_mon_cr
#define I_MON_CR REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_mon_cr)
#define i_sds_cr isacsx_rw.isacsx_r.isacsx_sds_cr
#define I_SDS_CR REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_sds_cr)
#define i_iom_cr isacsx_rw.isacsx_r.isacsx_iom_cr
#define I_IOM_CR REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_iom_cr)
#define i_asti isacsx_rw.isacsx_r.isacsx_asti
#define I_ASTI REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_asti)
#define i_msti isacsx_rw.isacsx_r.isacsx_msti
#define I_MSTI REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_msti)
#define i_sds_conf isacsx_rw.isacsx_r.isacsx_sds_conf
#define I_SDS_CONF REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_r.isacsx_sds_conf)
#define i_mox isacsx_rw.isacsx_w.isacsx_mox
#define I_MOX REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_w.isacsx_mox)
#define i_wmocr isacsx_rw.isacsx_w.isacsx_mocr
#define I_WMOCR REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_w.isacsx_mocr)
#define i_mconf isacsx_rw.isacsx_w.isacsx_mconf
#define I_MCONF REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_w.isacsx_mconf)
#define i_mask isacsx_rw.isacsx_w.isacsx_mask
#define I_MASK REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_w.isacsx_mask)
#define i_auxm isacsx_rw.isacsx_w.isacsx_auxm
#define I_AUXM REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_w.isacsx_auxm)
#define i_wmode1 isacsx_rw.isacsx_w.isacsx_mode1
#define I_WMODE1 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_w.isacsx_mode1)
#define i_wmode2 isacsx_rw.isacsx_w.isacsx_mode2
#define I_WMODE2 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_w.isacsx_mode2)
#define i_sres isacsx_rw.isacsx_w.isacsx_sres
#define I_SRES REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_w.isacsx_sres)
#define i_wtimr2 isacsx_rw.isacsx_w.isacsx_timr2
#define I_WTIMR2 REG_OFFSET(isacsx_reg_t, isacsx_rw.isacsx_w.isacsx_timr2)

#define ISACSX_ISTAD_RME  0x80
#define ISACSX_ISTAD_RPF  0x40
#define ISACSX_ISTAD_RFO  0x20
#define ISACSX_ISTAD_XPR  0x10
#define ISACSX_ISTAD_XMR  0x08
#define ISACSX_ISTAD_XDU  0x04

#define ISACSX_MASKD_RME  0x80
#define ISACSX_MASKD_RPF  0x40
#define ISACSX_MASKD_RFO  0x20
#define ISACSX_MASKD_XPR  0x10
#define ISACSX_MASKD_XMR  0x08
#define ISACSX_MASKD_XDU  0x04
/* these must always be set */
#define ISACSX_MASKD_LOW  0x03
#define ISACSX_MASKD_ALL  0xff

#define ISACSX_STARD_XDOV 0x80
#define ISACSX_STARD_XFW  0x40
#define ISACSX_STARD_RAC1 0x08
#define ISACSX_STARD_XAC1 0x02

#define ISACSX_CMDRD_RMC  0x80
#define ISACSX_CMDRD_RRES 0x40
#define ISACSX_CMDRD_STI  0x10
#define ISACSX_CMDRD_XTF  0x08
#define ISACSX_CMDRD_XME  0x02
#define ISACSX_CMDRD_XRES 0x01

#define ISACSX_MODED_MDS2 0x80
#define ISACSX_MODED_MDS1 0x40
#define ISACSX_MODED_MDS0 0x20
#define ISACSX_MODED_RAC  0x08
#define ISACSX_MODED_DIM2 0x04
#define ISACSX_MODED_DIM1 0x02
#define ISACSX_MODED_DIM0 0x01

/* default */
#define ISACSX_EXMD1_XFBS_32  0x00 /* XFIFO is 32 bytes */
#define ISACSX_EXMD1_XFBS_16  0x80 /* XFIFO is 16 bytes */
/* default */
#define ISACSX_EXMD1_RFBS_32  0x00 /* XFIFO is 32 bytes */
#define ISACSX_EXMD1_RFBS_16  0x20 /* XFIFO is 16 bytes */
#define ISACSX_EXMD1_RFBS_08  0x40 /* XFIFO is 8 bytes */
#define ISACSX_EXMD1_RFBS_04  0x60 /* XFIFO is 4 bytes */
#define ISACSX_EXMD1_SRA      0x10
#define ISACSX_EXMD1_XCRC     0x08
#define ISACSX_EXMD1_RCRC     0x04
#define ISACSX_EXMD1_ITF      0x01

#define ISACSX_RSTAD_VFR  0x80
#define ISACSX_RSTAD_RDO  0x40
#define ISACSX_RSTAD_CRC  0x20
#define ISACSX_RSTAD_RAB  0x10
#define ISACSX_RSTAD_SA1  0x08
#define ISACSX_RSTAD_SA0  0x04
#define ISACSX_RSTAD_CR   0x02
#define ISACSX_RSTAD_TA   0x01

#define ISACSX_RSTAD_MASK 0xf0	/* the interesting bits */

#define ISACSX_RBCHD_OV   0x10
/* the other 4 bits are the high bits of the receive byte count */

#define ISACSX_CIR0_CIC0  0x08
/* CODR0 >> 4 */
#define ISACSX_CIR0_IPU   0x07
#define ISACSX_CIR0_IDR   0x00
#define ISACSX_CIR0_ISD   0x02
#define ISACSX_CIR0_IDIS  0x03
#define ISACSX_CIR0_IEI   0x06
#define ISACSX_CIR0_IRSY  0x04
#define ISACSX_CIR0_IARD  0x08
#define ISACSX_CIR0_ITI   0x0a
#define ISACSX_CIR0_IATI  0x0b
#define ISACSX_CIR0_IAI8  0x0c
#define ISACSX_CIR0_IAI10 0x0d
#define ISACSX_CIR0_IDID  0x0f

#define ISACSX_IOM_CR_SPU      0x80
#define ISACSX_IOM_CR_CI_CS    0x20
#define ISACSX_IOM_CR_TIC_DIS  0x10
#define ISACSX_IOM_CR_EN_BCL   0x08
#define ISACSX_IOM_CR_CLKM     0x04
#define ISACSX_IOM_CR_DIS_OD   0x02
#define ISACSX_IOM_CR_DIS_IOM  0x01

#define ISACSX_CI_MASK	0x0f

#define ISACSX_CIX0_BAC  0x01
/* in IOM-2 mode the low bits are always 1 */
#define ISACSX_CIX0_LOW  0x0e
/* C/I codes from bits 7-4 (>> 4 & 0xf) */
/* the commands */
#define ISACSX_CIX0_CTIM  0
#define ISACSX_CIX0_CRS   0x01
/* test mode only */
#define ISACSX_CIX0_CSSSP  0x02
/* test mode only */
#define ISACSX_CIX0_CSSCP  0x03
#define ISACSX_CIX0_CAR8  0x08
#define ISACSX_CIX0_CAR10 0x09
#define ISACSX_CIX0_CARL  0x0a
#define ISACSX_CIX0_CDIU  0x0f

/* Interrupt, General Configuration Registers */

#define ISACSX_ISTA_ST    0x20
#define ISACSX_ISTA_CIC   0x10
#define ISACSX_ISTA_AUX   0x08
#define ISACSX_ISTA_TRAN  0x04
#define ISACSX_ISTA_MOS   0x02
#define ISACSX_ISTA_ICD   0x01

#define ISACSX_MASK_ST    0x20
#define ISACSX_MASK_CIC   0x10
#define ISACSX_MASK_AUX   0x08
#define ISACSX_MASK_TRAN  0x04
#define ISACSX_MASK_MOS   0x02
#define ISACSX_MASK_ICD   0x01

#define ISACSX_AUXI_EAW   0x20
#define ISACSX_AUXI_WOV   0x10
#define ISACSX_AUXI_TIN2  0x08
#define ISACSX_AUXI_TIN1  0x04

#define ISACSX_AUXM_EAW   0x20
#define ISACSX_AUXM_WOV   0x10
#define ISACSX_AUXM_TIN2  0x08
#define ISACSX_AUXM_TIN1  0x04

#define ISACSX_MODE1_WTC1 0x10
#define ISACSX_MODE1_WTC2 0x08
#define ISACSX_MODE1_CFS  0x04
#define ISACSX_MODE1_RSS2 0x02
#define ISACSX_MODE1_RSS1 0x01

#define ISACSX_MODE2_INT_POL 0x08
#define ISACSX_MODE2_PPSDX   0x01

#define ISACSX_ID_MASK 0x2F /* 0x01 = Version 1.3 */

#endif /* I4B_ISACSX_H_ */
