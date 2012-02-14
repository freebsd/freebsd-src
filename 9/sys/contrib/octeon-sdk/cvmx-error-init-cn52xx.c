/***********************license start***************
 * Copyright (c) 2003-2010  Cavium Networks (support@cavium.com). All rights
 * reserved.
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.

 *   * Neither the name of Cavium Networks nor the names of
 *     its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written
 *     permission.

 * This Software, including technical data, may be subject to U.S. export  control
 * laws, including the U.S. Export Administration Act and its  associated
 * regulations, and may be subject to export or import  regulations in other
 * countries.

 * TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
 * AND WITH ALL FAULTS AND CAVIUM  NETWORKS MAKES NO PROMISES, REPRESENTATIONS OR
 * WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO
 * THE SOFTWARE, INCLUDING ITS CONDITION, ITS CONFORMITY TO ANY REPRESENTATION OR
 * DESCRIPTION, OR THE EXISTENCE OF ANY LATENT OR PATENT DEFECTS, AND CAVIUM
 * SPECIFICALLY DISCLAIMS ALL IMPLIED (IF ANY) WARRANTIES OF TITLE,
 * MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF
 * VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
 * CORRESPONDENCE TO DESCRIPTION. THE ENTIRE  RISK ARISING OUT OF USE OR
 * PERFORMANCE OF THE SOFTWARE LIES WITH YOU.
 ***********************license end**************************************/


/**
 * @file
 *
 * Automatically generated error messages for cn52xx.
 *
 * This file is auto generated. Do not edit.
 *
 * <hr>$Revision$<hr>
 *
 * <hr><h2>Error tree for CN52XX</h2>
 * @dot
 * digraph cn52xx
 * {
 *     rankdir=LR;
 *     node [shape=record, width=.1, height=.1, fontsize=8, font=helvitica];
 *     edge [fontsize=7, font=helvitica];
 *     cvmx_root [label="ROOT|<root>root"];
 *     cvmx_ciu_int0_sum0 [label="CIU_INTX_SUM0(0)|<mii>mii"];
 *     cvmx_mix0_isr [label="MIXX_ISR(0)|<odblovf>odblovf|<idblovf>idblovf|<data_drp>data_drp|<irun>irun|<orun>orun"];
 *     cvmx_ciu_int0_sum0:mii:e -> cvmx_mix0_isr [label="mii"];
 *     cvmx_root:root:e -> cvmx_ciu_int0_sum0 [label="root"];
 *     cvmx_ciu_int_sum1 [label="CIU_INT_SUM1|<mii1>mii1|<nand>nand"];
 *     cvmx_mix1_isr [label="MIXX_ISR(1)|<odblovf>odblovf|<idblovf>idblovf|<data_drp>data_drp|<irun>irun|<orun>orun"];
 *     cvmx_ciu_int_sum1:mii1:e -> cvmx_mix1_isr [label="mii1"];
 *     cvmx_ndf_int [label="NDF_INT|<wdog>wdog|<sm_bad>sm_bad|<ecc_1bit>ecc_1bit|<ecc_mult>ecc_mult|<ovrf>ovrf"];
 *     cvmx_ciu_int_sum1:nand:e -> cvmx_ndf_int [label="nand"];
 *     cvmx_root:root:e -> cvmx_ciu_int_sum1 [label="root"];
 *     cvmx_npei_rsl_int_blocks [label="PEXP_NPEI_RSL_INT_BLOCKS|<l2c>l2c|<agl>agl|<gmx0>gmx0|<mio>mio|<ipd>ipd|<tim>tim|<pow>pow|<usb1>usb1|<npei>npei|<rad>rad|<pko>pko|<asxpcs0>asxpcs0|<pip>pip|<fpa>fpa|<lmc0>lmc0|<iob>iob|<usb>usb"];
 *     cvmx_l2c_int_stat [label="L2C_INT_STAT|<l2tsec>l2tsec|<l2dsec>l2dsec|<oob1>oob1|<oob2>oob2|<oob3>oob3|<l2tded>l2tded|<l2dded>l2dded|<lck>lck|<lck2>lck2"];
 *     cvmx_npei_rsl_int_blocks:l2c:e -> cvmx_l2c_int_stat [label="l2c"];
 *     cvmx_l2d_err [label="L2D_ERR|<sec_err>sec_err|<ded_err>ded_err"];
 *     cvmx_npei_rsl_int_blocks:l2c:e -> cvmx_l2d_err [label="l2c"];
 *     cvmx_l2t_err [label="L2T_ERR|<sec_err>sec_err|<ded_err>ded_err|<lckerr>lckerr|<lckerr2>lckerr2"];
 *     cvmx_npei_rsl_int_blocks:l2c:e -> cvmx_l2t_err [label="l2c"];
 *     cvmx_agl_gmx_bad_reg [label="AGL_GMX_BAD_REG|<ovrflw>ovrflw|<txpop>txpop|<txpsh>txpsh|<ovrflw1>ovrflw1|<txpop1>txpop1|<txpsh1>txpsh1|<out_ovr>out_ovr|<loststat>loststat"];
 *     cvmx_npei_rsl_int_blocks:agl:e -> cvmx_agl_gmx_bad_reg [label="agl"];
 *     cvmx_agl_gmx_rx0_int_reg [label="AGL_GMX_RXX_INT_REG(0)|<skperr>skperr|<ovrerr>ovrerr"];
 *     cvmx_npei_rsl_int_blocks:agl:e -> cvmx_agl_gmx_rx0_int_reg [label="agl"];
 *     cvmx_agl_gmx_rx1_int_reg [label="AGL_GMX_RXX_INT_REG(1)|<skperr>skperr|<ovrerr>ovrerr"];
 *     cvmx_npei_rsl_int_blocks:agl:e -> cvmx_agl_gmx_rx1_int_reg [label="agl"];
 *     cvmx_agl_gmx_tx_int_reg [label="AGL_GMX_TX_INT_REG|<pko_nxa>pko_nxa|<undflw>undflw"];
 *     cvmx_npei_rsl_int_blocks:agl:e -> cvmx_agl_gmx_tx_int_reg [label="agl"];
 *     cvmx_gmx0_bad_reg [label="GMXX_BAD_REG(0)|<out_ovr>out_ovr|<loststat>loststat|<statovr>statovr|<inb_nxa>inb_nxa"];
 *     cvmx_npei_rsl_int_blocks:gmx0:e -> cvmx_gmx0_bad_reg [label="gmx0"];
 *     cvmx_gmx0_rx0_int_reg [label="GMXX_RXX_INT_REG(0,0)|<carext>carext|<skperr>skperr|<ovrerr>ovrerr|<loc_fault>loc_fault|<rem_fault>rem_fault|<bad_seq>bad_seq|<bad_term>bad_term|<unsop>unsop|<uneop>uneop|<undat>undat|<hg2fld>hg2fld|<hg2cc>hg2cc"];
 *     cvmx_npei_rsl_int_blocks:gmx0:e -> cvmx_gmx0_rx0_int_reg [label="gmx0"];
 *     cvmx_gmx0_rx1_int_reg [label="GMXX_RXX_INT_REG(1,0)|<carext>carext|<skperr>skperr|<ovrerr>ovrerr|<loc_fault>loc_fault|<rem_fault>rem_fault|<bad_seq>bad_seq|<bad_term>bad_term|<unsop>unsop|<uneop>uneop|<undat>undat|<hg2fld>hg2fld|<hg2cc>hg2cc"];
 *     cvmx_npei_rsl_int_blocks:gmx0:e -> cvmx_gmx0_rx1_int_reg [label="gmx0"];
 *     cvmx_gmx0_rx2_int_reg [label="GMXX_RXX_INT_REG(2,0)|<carext>carext|<skperr>skperr|<ovrerr>ovrerr|<loc_fault>loc_fault|<rem_fault>rem_fault|<bad_seq>bad_seq|<bad_term>bad_term|<unsop>unsop|<uneop>uneop|<undat>undat|<hg2fld>hg2fld|<hg2cc>hg2cc"];
 *     cvmx_npei_rsl_int_blocks:gmx0:e -> cvmx_gmx0_rx2_int_reg [label="gmx0"];
 *     cvmx_gmx0_rx3_int_reg [label="GMXX_RXX_INT_REG(3,0)|<carext>carext|<skperr>skperr|<ovrerr>ovrerr|<loc_fault>loc_fault|<rem_fault>rem_fault|<bad_seq>bad_seq|<bad_term>bad_term|<unsop>unsop|<uneop>uneop|<undat>undat|<hg2fld>hg2fld|<hg2cc>hg2cc"];
 *     cvmx_npei_rsl_int_blocks:gmx0:e -> cvmx_gmx0_rx3_int_reg [label="gmx0"];
 *     cvmx_gmx0_tx_int_reg [label="GMXX_TX_INT_REG(0)|<pko_nxa>pko_nxa|<undflw>undflw"];
 *     cvmx_npei_rsl_int_blocks:gmx0:e -> cvmx_gmx0_tx_int_reg [label="gmx0"];
 *     cvmx_mio_boot_err [label="MIO_BOOT_ERR|<adr_err>adr_err|<wait_err>wait_err"];
 *     cvmx_npei_rsl_int_blocks:mio:e -> cvmx_mio_boot_err [label="mio"];
 *     cvmx_ipd_int_sum [label="IPD_INT_SUM|<prc_par0>prc_par0|<prc_par1>prc_par1|<prc_par2>prc_par2|<prc_par3>prc_par3|<bp_sub>bp_sub|<dc_ovr>dc_ovr|<cc_ovr>cc_ovr|<c_coll>c_coll|<d_coll>d_coll|<bc_ovr>bc_ovr"];
 *     cvmx_npei_rsl_int_blocks:ipd:e -> cvmx_ipd_int_sum [label="ipd"];
 *     cvmx_tim_reg_error [label="TIM_REG_ERROR|<mask>mask"];
 *     cvmx_npei_rsl_int_blocks:tim:e -> cvmx_tim_reg_error [label="tim"];
 *     cvmx_pow_ecc_err [label="POW_ECC_ERR|<sbe>sbe|<dbe>dbe|<rpe>rpe|<iop>iop"];
 *     cvmx_npei_rsl_int_blocks:pow:e -> cvmx_pow_ecc_err [label="pow"];
 *     cvmx_usbn1_int_sum [label="USBNX_INT_SUM(1)|<pr_po_e>pr_po_e|<pr_pu_f>pr_pu_f|<nr_po_e>nr_po_e|<nr_pu_f>nr_pu_f|<lr_po_e>lr_po_e|<lr_pu_f>lr_pu_f|<pt_po_e>pt_po_e|<pt_pu_f>pt_pu_f|<nt_po_e>nt_po_e|<nt_pu_f>nt_pu_f|<lt_po_e>lt_po_e|<lt_pu_f>lt_pu_f|<dcred_e>dcred_e|<dcred_f>dcred_f|<l2c_s_e>l2c_s_e|<l2c_a_f>l2c_a_f|<lt_fi_e>lt_fi_e|<lt_fi_f>lt_fi_f|<rg_fi_e>rg_fi_e|<rg_fi_f>rg_fi_f|<rq_q2_f>rq_q2_f|<rq_q2_e>rq_q2_e|<rq_q3_f>rq_q3_f|<rq_q3_e>rq_q3_e|<uod_pe>uod_pe|<uod_pf>uod_pf|<ltl_f_pe>ltl_f_pe|<ltl_f_pf>ltl_f_pf|<nd4o_rpe>nd4o_rpe|<nd4o_rpf>nd4o_rpf|<nd4o_dpe>nd4o_dpe|<nd4o_dpf>nd4o_dpf"];
 *     cvmx_npei_rsl_int_blocks:usb1:e -> cvmx_usbn1_int_sum [label="usb1"];
 *     cvmx_npei_int_sum [label="PEXP_NPEI_INT_SUM|<c0_ldwn>c0_ldwn|<c0_se>c0_se|<c0_un_b0>c0_un_b0|<c0_un_b1>c0_un_b1|<c0_un_b2>c0_un_b2|<c0_un_bx>c0_un_bx|<c0_un_wf>c0_un_wf|<c0_un_wi>c0_un_wi|<c0_up_b0>c0_up_b0|<c0_up_b1>c0_up_b1|<c0_up_b2>c0_up_b2|<c0_up_bx>c0_up_bx|<c0_up_wf>c0_up_wf|<c0_up_wi>c0_up_wi|<c0_wake>c0_wake|<crs0_dr>crs0_dr|<crs0_er>crs0_er|<c1_ldwn>c1_ldwn|<c1_se>c1_se|<c1_un_b0>c1_un_b0|<c1_un_b1>c1_un_b1|<c1_un_b2>c1_un_b2|<c1_un_bx>c1_un_bx|<c1_un_wf>c1_un_wf|<c1_un_wi>c1_un_wi|<c1_up_b0>c1_up_b0|<c1_up_b1>c1_up_b1|<c1_up_b2>c1_up_b2|<c1_up_bx>c1_up_bx|<c1_up_wf>c1_up_wf|<c1_up_wi>c1_up_wi|<c1_wake>c1_wake|<crs1_dr>crs1_dr|<crs1_er>crs1_er|<bar0_to>bar0_to|<dma0dbo>dma0dbo|<dma1dbo>dma1dbo|<dma2dbo>dma2dbo|<dma3dbo>dma3dbo|<iob2big>iob2big|<rml_rto>rml_rto|<rml_wto>rml_wto|<dma4dbo>dma4dbo|<c0_exc>c0_exc|<c1_exc>c1_exc"];
 *     cvmx_pesc0_dbg_info [label="PESCX_DBG_INFO(0)|<spoison>spoison|<rtlplle>rtlplle|<recrce>recrce|<rpoison>rpoison|<rcemrc>rcemrc|<rnfemrc>rnfemrc|<rfemrc>rfemrc|<rpmerc>rpmerc|<rptamrc>rptamrc|<rumep>rumep|<rvdm>rvdm|<acto>acto|<rte>rte|<mre>mre|<rdwdle>rdwdle|<rtwdle>rtwdle|<dpeoosd>dpeoosd|<fcpvwt>fcpvwt|<rpe>rpe|<fcuv>fcuv|<rqo>rqo|<rauc>rauc|<racur>racur|<racca>racca|<caar>caar|<rarwdns>rarwdns|<ramtlp>ramtlp|<racpp>racpp|<rawwpp>rawwpp|<ecrc_e>ecrc_e"];
 *     cvmx_npei_int_sum:c0_exc:e -> cvmx_pesc0_dbg_info [label="c0_exc"];
 *     cvmx_pesc1_dbg_info [label="PESCX_DBG_INFO(1)|<spoison>spoison|<rtlplle>rtlplle|<recrce>recrce|<rpoison>rpoison|<rcemrc>rcemrc|<rnfemrc>rnfemrc|<rfemrc>rfemrc|<rpmerc>rpmerc|<rptamrc>rptamrc|<rumep>rumep|<rvdm>rvdm|<acto>acto|<rte>rte|<mre>mre|<rdwdle>rdwdle|<rtwdle>rtwdle|<dpeoosd>dpeoosd|<fcpvwt>fcpvwt|<rpe>rpe|<fcuv>fcuv|<rqo>rqo|<rauc>rauc|<racur>racur|<racca>racca|<caar>caar|<rarwdns>rarwdns|<ramtlp>ramtlp|<racpp>racpp|<rawwpp>rawwpp|<ecrc_e>ecrc_e"];
 *     cvmx_npei_int_sum:c1_exc:e -> cvmx_pesc1_dbg_info [label="c1_exc"];
 *     cvmx_npei_rsl_int_blocks:npei:e -> cvmx_npei_int_sum [label="npei"];
 *     cvmx_rad_reg_error [label="RAD_REG_ERROR|<doorbell>doorbell"];
 *     cvmx_npei_rsl_int_blocks:rad:e -> cvmx_rad_reg_error [label="rad"];
 *     cvmx_pko_reg_error [label="PKO_REG_ERROR|<parity>parity|<doorbell>doorbell|<currzero>currzero"];
 *     cvmx_npei_rsl_int_blocks:pko:e -> cvmx_pko_reg_error [label="pko"];
 *     cvmx_pcs0_int0_reg [label="PCSX_INTX_REG(0,0)|<an_err>an_err|<txfifu>txfifu|<txfifo>txfifo|<txbad>txbad|<rxbad>rxbad|<rxlock>rxlock|<an_bad>an_bad|<sync_bad>sync_bad"];
 *     cvmx_npei_rsl_int_blocks:asxpcs0:e -> cvmx_pcs0_int0_reg [label="asxpcs0"];
 *     cvmx_pcs0_int1_reg [label="PCSX_INTX_REG(1,0)|<an_err>an_err|<txfifu>txfifu|<txfifo>txfifo|<txbad>txbad|<rxbad>rxbad|<rxlock>rxlock|<an_bad>an_bad|<sync_bad>sync_bad"];
 *     cvmx_npei_rsl_int_blocks:asxpcs0:e -> cvmx_pcs0_int1_reg [label="asxpcs0"];
 *     cvmx_pcs0_int2_reg [label="PCSX_INTX_REG(2,0)|<an_err>an_err|<txfifu>txfifu|<txfifo>txfifo|<txbad>txbad|<rxbad>rxbad|<rxlock>rxlock|<an_bad>an_bad|<sync_bad>sync_bad"];
 *     cvmx_npei_rsl_int_blocks:asxpcs0:e -> cvmx_pcs0_int2_reg [label="asxpcs0"];
 *     cvmx_pcs0_int3_reg [label="PCSX_INTX_REG(3,0)|<an_err>an_err|<txfifu>txfifu|<txfifo>txfifo|<txbad>txbad|<rxbad>rxbad|<rxlock>rxlock|<an_bad>an_bad|<sync_bad>sync_bad"];
 *     cvmx_npei_rsl_int_blocks:asxpcs0:e -> cvmx_pcs0_int3_reg [label="asxpcs0"];
 *     cvmx_pcsx0_int_reg [label="PCSXX_INT_REG(0)|<txflt>txflt|<rxbad>rxbad|<rxsynbad>rxsynbad|<synlos>synlos|<algnlos>algnlos"];
 *     cvmx_npei_rsl_int_blocks:asxpcs0:e -> cvmx_pcsx0_int_reg [label="asxpcs0"];
 *     cvmx_pip_int_reg [label="PIP_INT_REG|<prtnxa>prtnxa|<badtag>badtag|<skprunt>skprunt|<todoovr>todoovr|<feperr>feperr|<beperr>beperr|<punyerr>punyerr"];
 *     cvmx_npei_rsl_int_blocks:pip:e -> cvmx_pip_int_reg [label="pip"];
 *     cvmx_fpa_int_sum [label="FPA_INT_SUM|<fed0_sbe>fed0_sbe|<fed0_dbe>fed0_dbe|<fed1_sbe>fed1_sbe|<fed1_dbe>fed1_dbe|<q0_und>q0_und|<q0_coff>q0_coff|<q0_perr>q0_perr|<q1_und>q1_und|<q1_coff>q1_coff|<q1_perr>q1_perr|<q2_und>q2_und|<q2_coff>q2_coff|<q2_perr>q2_perr|<q3_und>q3_und|<q3_coff>q3_coff|<q3_perr>q3_perr|<q4_und>q4_und|<q4_coff>q4_coff|<q4_perr>q4_perr|<q5_und>q5_und|<q5_coff>q5_coff|<q5_perr>q5_perr|<q6_und>q6_und|<q6_coff>q6_coff|<q6_perr>q6_perr|<q7_und>q7_und|<q7_coff>q7_coff|<q7_perr>q7_perr"];
 *     cvmx_npei_rsl_int_blocks:fpa:e -> cvmx_fpa_int_sum [label="fpa"];
 *     cvmx_lmc0_mem_cfg0 [label="LMCX_MEM_CFG0(0)|<sec_err>sec_err|<ded_err>ded_err"];
 *     cvmx_npei_rsl_int_blocks:lmc0:e -> cvmx_lmc0_mem_cfg0 [label="lmc0"];
 *     cvmx_iob_int_sum [label="IOB_INT_SUM|<np_sop>np_sop|<np_eop>np_eop|<p_sop>p_sop|<p_eop>p_eop|<np_dat>np_dat|<p_dat>p_dat"];
 *     cvmx_npei_rsl_int_blocks:iob:e -> cvmx_iob_int_sum [label="iob"];
 *     cvmx_usbn0_int_sum [label="USBNX_INT_SUM(0)|<pr_po_e>pr_po_e|<pr_pu_f>pr_pu_f|<nr_po_e>nr_po_e|<nr_pu_f>nr_pu_f|<lr_po_e>lr_po_e|<lr_pu_f>lr_pu_f|<pt_po_e>pt_po_e|<pt_pu_f>pt_pu_f|<nt_po_e>nt_po_e|<nt_pu_f>nt_pu_f|<lt_po_e>lt_po_e|<lt_pu_f>lt_pu_f|<dcred_e>dcred_e|<dcred_f>dcred_f|<l2c_s_e>l2c_s_e|<l2c_a_f>l2c_a_f|<lt_fi_e>lt_fi_e|<lt_fi_f>lt_fi_f|<rg_fi_e>rg_fi_e|<rg_fi_f>rg_fi_f|<rq_q2_f>rq_q2_f|<rq_q2_e>rq_q2_e|<rq_q3_f>rq_q3_f|<rq_q3_e>rq_q3_e|<uod_pe>uod_pe|<uod_pf>uod_pf|<ltl_f_pe>ltl_f_pe|<ltl_f_pf>ltl_f_pf|<nd4o_rpe>nd4o_rpe|<nd4o_rpf>nd4o_rpf|<nd4o_dpe>nd4o_dpe|<nd4o_dpf>nd4o_dpf"];
 *     cvmx_npei_rsl_int_blocks:usb:e -> cvmx_usbn0_int_sum [label="usb"];
 *     cvmx_agl_gmx_bad_reg -> cvmx_agl_gmx_rx0_int_reg [style=invis];
 *     cvmx_agl_gmx_rx0_int_reg -> cvmx_agl_gmx_rx1_int_reg [style=invis];
 *     cvmx_agl_gmx_rx1_int_reg -> cvmx_agl_gmx_tx_int_reg [style=invis];
 *     cvmx_gmx0_bad_reg -> cvmx_gmx0_rx0_int_reg [style=invis];
 *     cvmx_gmx0_rx0_int_reg -> cvmx_gmx0_rx1_int_reg [style=invis];
 *     cvmx_gmx0_rx1_int_reg -> cvmx_gmx0_rx2_int_reg [style=invis];
 *     cvmx_gmx0_rx2_int_reg -> cvmx_gmx0_rx3_int_reg [style=invis];
 *     cvmx_gmx0_rx3_int_reg -> cvmx_gmx0_tx_int_reg [style=invis];
 *     cvmx_pcs0_int0_reg -> cvmx_pcs0_int1_reg [style=invis];
 *     cvmx_pcs0_int1_reg -> cvmx_pcs0_int2_reg [style=invis];
 *     cvmx_pcs0_int2_reg -> cvmx_pcs0_int3_reg [style=invis];
 *     cvmx_pcs0_int3_reg -> cvmx_pcsx0_int_reg [style=invis];
 *     cvmx_root:root:e -> cvmx_npei_rsl_int_blocks [label="root"];
 * }
 * @enddot
 */
#ifdef CVMX_BUILD_FOR_LINUX_KERNEL
#include <asm/octeon/cvmx.h>
#include <asm/octeon/cvmx-error.h>
#include <asm/octeon/cvmx-error-custom.h>
#include <asm/octeon/cvmx-csr-typedefs.h>
#else
#include "cvmx.h"
#include "cvmx-error.h"
#include "cvmx-error-custom.h"
#endif

int cvmx_error_initialize_cn52xx(void);

int cvmx_error_initialize_cn52xx(void)
{
    cvmx_error_info_t info;
    int fail = 0;

    /* CVMX_CIU_INTX_SUM0(0) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_CIU_INTX_SUM0(0);
    info.status_mask        = 0;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = __CVMX_ERROR_REGISTER_NONE;
    info.parent.status_addr = 0;
    info.parent.status_mask = 0;
    info.func               = __cvmx_error_decode;
    info.user_info          = 0;
    fail |= cvmx_error_add(&info);

    /* CVMX_MIXX_ISR(0) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_MIXX_ISR(0);
    info.status_mask        = 1ull<<0 /* odblovf */;
    info.enable_addr        = CVMX_MIXX_INTENA(0);
    info.enable_mask        = 1ull<<0 /* ovfena */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_MGMT_PORT;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_INTX_SUM0(0);
    info.parent.status_mask = 1ull<<62 /* mii */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR MIXX_ISR(0)[ODBLOVF]: Outbound DoorBell(ODBELL) Overflow Detected\n"
        "    If SW attempts to write to the MIX_ORING2[ODBELL]\n"
        "    with a value greater than the remaining #of\n"
        "    O-Ring Buffer Entries (MIX_REMCNT[OREMCNT]), then\n"
        "    the following occurs:\n"
        "    1) The  MIX_ORING2[ODBELL] write is IGNORED\n"
        "    2) The ODBLOVF is set and the CIU_INTx_SUM0,4[MII]\n"
        "       bits are set if ((MIX_ISR & MIX_INTENA) != 0)).\n"
        "    If both the global interrupt mask bits (CIU_INTx_EN*[MII])\n"
        "    and the local interrupt mask bit(OVFENA) is set, than an\n"
        "    interrupt is reported for this event.\n"
        "    SW should keep track of the #I-Ring Entries in use\n"
        "    (ie: cumulative # of ODBELL writes),  and ensure that\n"
        "    future ODBELL writes don't exceed the size of the\n"
        "    O-Ring Buffer (MIX_ORING2[OSIZE]).\n"
        "    SW must reclaim O-Ring Entries by writing to the\n"
        "    MIX_ORCNT[ORCNT]. .\n"
        "    NOTE: There is no recovery from an ODBLOVF Interrupt.\n"
        "    If it occurs, it's an indication that SW has\n"
        "    overwritten the O-Ring buffer, and the only recourse\n"
        "    is a HW reset.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_MIXX_ISR(0);
    info.status_mask        = 1ull<<1 /* idblovf */;
    info.enable_addr        = CVMX_MIXX_INTENA(0);
    info.enable_mask        = 1ull<<1 /* ivfena */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_MGMT_PORT;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_INTX_SUM0(0);
    info.parent.status_mask = 1ull<<62 /* mii */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR MIXX_ISR(0)[IDBLOVF]: Inbound DoorBell(IDBELL) Overflow Detected\n"
        "    If SW attempts to write to the MIX_IRING2[IDBELL]\n"
        "    with a value greater than the remaining #of\n"
        "    I-Ring Buffer Entries (MIX_REMCNT[IREMCNT]), then\n"
        "    the following occurs:\n"
        "    1) The  MIX_IRING2[IDBELL] write is IGNORED\n"
        "    2) The ODBLOVF is set and the CIU_INTx_SUM0,4[MII]\n"
        "       bits are set if ((MIX_ISR & MIX_INTENA) != 0)).\n"
        "    If both the global interrupt mask bits (CIU_INTx_EN*[MII])\n"
        "    and the local interrupt mask bit(IVFENA) is set, than an\n"
        "    interrupt is reported for this event.\n"
        "    SW should keep track of the #I-Ring Entries in use\n"
        "    (ie: cumulative # of IDBELL writes),  and ensure that\n"
        "    future IDBELL writes don't exceed the size of the\n"
        "    I-Ring Buffer (MIX_IRING2[ISIZE]).\n"
        "    SW must reclaim I-Ring Entries by keeping track of the\n"
        "    #IRing-Entries, and writing to the MIX_IRCNT[IRCNT].\n"
        "    NOTE: The MIX_IRCNT[IRCNT] register represents the\n"
        "    total #packets(not IRing Entries) and SW must further\n"
        "    keep track of the # of I-Ring Entries associated with\n"
        "    each packet as they are processed.\n"
        "    NOTE: There is no recovery from an IDBLOVF Interrupt.\n"
        "    If it occurs, it's an indication that SW has\n"
        "    overwritten the I-Ring buffer, and the only recourse\n"
        "    is a HW reset.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_MIXX_ISR(0);
    info.status_mask        = 1ull<<4 /* data_drp */;
    info.enable_addr        = CVMX_MIXX_INTENA(0);
    info.enable_mask        = 1ull<<4 /* data_drpena */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_MGMT_PORT;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_INTX_SUM0(0);
    info.parent.status_mask = 1ull<<62 /* mii */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR MIXX_ISR(0)[DATA_DRP]: Data was dropped due to RX FIFO full\n"
        "    If this does occur, the DATA_DRP is set and the\n"
        "    CIU_INTx_SUM0,4[MII] bits are set.\n"
        "    If both the global interrupt mask bits (CIU_INTx_EN*[MII])\n"
        "    and the local interrupt mask bit(DATA_DRPENA) is set, than an\n"
        "    interrupt is reported for this event.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_MIXX_ISR(0);
    info.status_mask        = 1ull<<5 /* irun */;
    info.enable_addr        = CVMX_MIXX_INTENA(0);
    info.enable_mask        = 1ull<<5 /* irunena */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_MGMT_PORT;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_INTX_SUM0(0);
    info.parent.status_mask = 1ull<<62 /* mii */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR MIXX_ISR(0)[IRUN]: IRCNT UnderFlow Detected\n"
        "    If SW writes a larger value than what is currently\n"
        "    in the MIX_IRCNT[IRCNT], then HW will report the\n"
        "    underflow condition.\n"
        "    NOTE: The MIX_IRCNT[IRCNT] will clamp to to zero.\n"
        "    NOTE: If an IRUN underflow condition is detected,\n"
        "    the integrity of the MIX/AGL HW state has\n"
        "    been compromised. To recover, SW must issue a\n"
        "    software reset sequence (see: MIX_CTL[RESET]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_MIXX_ISR(0);
    info.status_mask        = 1ull<<6 /* orun */;
    info.enable_addr        = CVMX_MIXX_INTENA(0);
    info.enable_mask        = 1ull<<6 /* orunena */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_MGMT_PORT;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_INTX_SUM0(0);
    info.parent.status_mask = 1ull<<62 /* mii */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR MIXX_ISR(0)[ORUN]: ORCNT UnderFlow Detected\n"
        "    If SW writes a larger value than what is currently\n"
        "    in the MIX_ORCNT[ORCNT], then HW will report the\n"
        "    underflow condition.\n"
        "    NOTE: The MIX_ORCNT[IOCNT] will clamp to to zero.\n"
        "    NOTE: If an ORUN underflow condition is detected,\n"
        "    the integrity of the MIX/AGL HW state has\n"
        "    been compromised. To recover, SW must issue a\n"
        "    software reset sequence (see: MIX_CTL[RESET]\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_CIU_INT_SUM1 */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_CIU_INT_SUM1;
    info.status_mask        = 0;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = __CVMX_ERROR_REGISTER_NONE;
    info.parent.status_addr = 0;
    info.parent.status_mask = 0;
    info.func               = __cvmx_error_decode;
    info.user_info          = 0;
    fail |= cvmx_error_add(&info);

    /* CVMX_MIXX_ISR(1) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_MIXX_ISR(1);
    info.status_mask        = 1ull<<0 /* odblovf */;
    info.enable_addr        = CVMX_MIXX_INTENA(1);
    info.enable_mask        = 1ull<<0 /* ovfena */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_MGMT_PORT;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_INT_SUM1;
    info.parent.status_mask = 1ull<<18 /* mii1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR MIXX_ISR(1)[ODBLOVF]: Outbound DoorBell(ODBELL) Overflow Detected\n"
        "    If SW attempts to write to the MIX_ORING2[ODBELL]\n"
        "    with a value greater than the remaining #of\n"
        "    O-Ring Buffer Entries (MIX_REMCNT[OREMCNT]), then\n"
        "    the following occurs:\n"
        "    1) The  MIX_ORING2[ODBELL] write is IGNORED\n"
        "    2) The ODBLOVF is set and the CIU_INTx_SUM0,4[MII]\n"
        "       bits are set if ((MIX_ISR & MIX_INTENA) != 0)).\n"
        "    If both the global interrupt mask bits (CIU_INTx_EN*[MII])\n"
        "    and the local interrupt mask bit(OVFENA) is set, than an\n"
        "    interrupt is reported for this event.\n"
        "    SW should keep track of the #I-Ring Entries in use\n"
        "    (ie: cumulative # of ODBELL writes),  and ensure that\n"
        "    future ODBELL writes don't exceed the size of the\n"
        "    O-Ring Buffer (MIX_ORING2[OSIZE]).\n"
        "    SW must reclaim O-Ring Entries by writing to the\n"
        "    MIX_ORCNT[ORCNT]. .\n"
        "    NOTE: There is no recovery from an ODBLOVF Interrupt.\n"
        "    If it occurs, it's an indication that SW has\n"
        "    overwritten the O-Ring buffer, and the only recourse\n"
        "    is a HW reset.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_MIXX_ISR(1);
    info.status_mask        = 1ull<<1 /* idblovf */;
    info.enable_addr        = CVMX_MIXX_INTENA(1);
    info.enable_mask        = 1ull<<1 /* ivfena */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_MGMT_PORT;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_INT_SUM1;
    info.parent.status_mask = 1ull<<18 /* mii1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR MIXX_ISR(1)[IDBLOVF]: Inbound DoorBell(IDBELL) Overflow Detected\n"
        "    If SW attempts to write to the MIX_IRING2[IDBELL]\n"
        "    with a value greater than the remaining #of\n"
        "    I-Ring Buffer Entries (MIX_REMCNT[IREMCNT]), then\n"
        "    the following occurs:\n"
        "    1) The  MIX_IRING2[IDBELL] write is IGNORED\n"
        "    2) The ODBLOVF is set and the CIU_INTx_SUM0,4[MII]\n"
        "       bits are set if ((MIX_ISR & MIX_INTENA) != 0)).\n"
        "    If both the global interrupt mask bits (CIU_INTx_EN*[MII])\n"
        "    and the local interrupt mask bit(IVFENA) is set, than an\n"
        "    interrupt is reported for this event.\n"
        "    SW should keep track of the #I-Ring Entries in use\n"
        "    (ie: cumulative # of IDBELL writes),  and ensure that\n"
        "    future IDBELL writes don't exceed the size of the\n"
        "    I-Ring Buffer (MIX_IRING2[ISIZE]).\n"
        "    SW must reclaim I-Ring Entries by keeping track of the\n"
        "    #IRing-Entries, and writing to the MIX_IRCNT[IRCNT].\n"
        "    NOTE: The MIX_IRCNT[IRCNT] register represents the\n"
        "    total #packets(not IRing Entries) and SW must further\n"
        "    keep track of the # of I-Ring Entries associated with\n"
        "    each packet as they are processed.\n"
        "    NOTE: There is no recovery from an IDBLOVF Interrupt.\n"
        "    If it occurs, it's an indication that SW has\n"
        "    overwritten the I-Ring buffer, and the only recourse\n"
        "    is a HW reset.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_MIXX_ISR(1);
    info.status_mask        = 1ull<<4 /* data_drp */;
    info.enable_addr        = CVMX_MIXX_INTENA(1);
    info.enable_mask        = 1ull<<4 /* data_drpena */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_MGMT_PORT;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_INT_SUM1;
    info.parent.status_mask = 1ull<<18 /* mii1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR MIXX_ISR(1)[DATA_DRP]: Data was dropped due to RX FIFO full\n"
        "    If this does occur, the DATA_DRP is set and the\n"
        "    CIU_INTx_SUM0,4[MII] bits are set.\n"
        "    If both the global interrupt mask bits (CIU_INTx_EN*[MII])\n"
        "    and the local interrupt mask bit(DATA_DRPENA) is set, than an\n"
        "    interrupt is reported for this event.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_MIXX_ISR(1);
    info.status_mask        = 1ull<<5 /* irun */;
    info.enable_addr        = CVMX_MIXX_INTENA(1);
    info.enable_mask        = 1ull<<5 /* irunena */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_MGMT_PORT;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_INT_SUM1;
    info.parent.status_mask = 1ull<<18 /* mii1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR MIXX_ISR(1)[IRUN]: IRCNT UnderFlow Detected\n"
        "    If SW writes a larger value than what is currently\n"
        "    in the MIX_IRCNT[IRCNT], then HW will report the\n"
        "    underflow condition.\n"
        "    NOTE: The MIX_IRCNT[IRCNT] will clamp to to zero.\n"
        "    NOTE: If an IRUN underflow condition is detected,\n"
        "    the integrity of the MIX/AGL HW state has\n"
        "    been compromised. To recover, SW must issue a\n"
        "    software reset sequence (see: MIX_CTL[RESET]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_MIXX_ISR(1);
    info.status_mask        = 1ull<<6 /* orun */;
    info.enable_addr        = CVMX_MIXX_INTENA(1);
    info.enable_mask        = 1ull<<6 /* orunena */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_MGMT_PORT;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_INT_SUM1;
    info.parent.status_mask = 1ull<<18 /* mii1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR MIXX_ISR(1)[ORUN]: ORCNT UnderFlow Detected\n"
        "    If SW writes a larger value than what is currently\n"
        "    in the MIX_ORCNT[ORCNT], then HW will report the\n"
        "    underflow condition.\n"
        "    NOTE: The MIX_ORCNT[IOCNT] will clamp to to zero.\n"
        "    NOTE: If an ORUN underflow condition is detected,\n"
        "    the integrity of the MIX/AGL HW state has\n"
        "    been compromised. To recover, SW must issue a\n"
        "    software reset sequence (see: MIX_CTL[RESET]\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_NDF_INT */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_NDF_INT;
    info.status_mask        = 1ull<<2 /* wdog */;
    info.enable_addr        = CVMX_NDF_INT_EN;
    info.enable_mask        = 1ull<<2 /* wdog */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_INT_SUM1;
    info.parent.status_mask = 1ull<<19 /* nand */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR NDF_INT[WDOG]: Watch Dog timer expired during command execution\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_NDF_INT;
    info.status_mask        = 1ull<<3 /* sm_bad */;
    info.enable_addr        = CVMX_NDF_INT_EN;
    info.enable_mask        = 1ull<<3 /* sm_bad */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_INT_SUM1;
    info.parent.status_mask = 1ull<<19 /* nand */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR NDF_INT[SM_BAD]: One of the state machines in a bad state\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_NDF_INT;
    info.status_mask        = 1ull<<4 /* ecc_1bit */;
    info.enable_addr        = CVMX_NDF_INT_EN;
    info.enable_mask        = 1ull<<4 /* ecc_1bit */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_INT_SUM1;
    info.parent.status_mask = 1ull<<19 /* nand */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR NDF_INT[ECC_1BIT]: Single bit ECC error detected and fixed during boot\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_NDF_INT;
    info.status_mask        = 1ull<<5 /* ecc_mult */;
    info.enable_addr        = CVMX_NDF_INT_EN;
    info.enable_mask        = 1ull<<5 /* ecc_mult */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_INT_SUM1;
    info.parent.status_mask = 1ull<<19 /* nand */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR NDF_INT[ECC_MULT]: Multi bit ECC error detected during boot\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_NDF_INT;
    info.status_mask        = 1ull<<6 /* ovrf */;
    info.enable_addr        = CVMX_NDF_INT_EN;
    info.enable_mask        = 1ull<<6 /* ovrf */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_INT_SUM1;
    info.parent.status_mask = 1ull<<19 /* nand */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR NDF_INT[OVRF]: NDF_CMD write when fifo is full. Generally a\n"
        "    fatal error.\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_PEXP_NPEI_RSL_INT_BLOCKS */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.status_mask        = 0;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = __CVMX_ERROR_REGISTER_NONE;
    info.parent.status_addr = 0;
    info.parent.status_mask = 0;
    info.func               = __cvmx_error_decode;
    info.user_info          = 0;
    fail |= cvmx_error_add(&info);

    /* CVMX_L2C_INT_STAT */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_INT_STAT;
    info.status_mask        = 1ull<<3 /* l2tsec */;
    info.enable_addr        = CVMX_L2C_INT_EN;
    info.enable_mask        = 1ull<<3 /* l2tsecen */;
    info.flags              = CVMX_ERROR_FLAGS_ECC_SINGLE_BIT;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<16 /* l2c */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_INT_STAT[L2TSEC]: L2T Single Bit Error corrected (SEC) status\n"
        "    During every L2 Tag Probe, all 8 sets Tag's (at a\n"
        "    given index) are checked for single bit errors(SBEs).\n"
        "    This bit is set if ANY of the 8 sets contains an SBE.\n"
        "    SBEs are auto corrected in HW and generate an\n"
        "    interrupt(if enabled).\n"
        "    NOTE: This is the 'same' bit as L2T_ERR[SEC_ERR]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_INT_STAT;
    info.status_mask        = 1ull<<5 /* l2dsec */;
    info.enable_addr        = CVMX_L2C_INT_EN;
    info.enable_mask        = 1ull<<5 /* l2dsecen */;
    info.flags              = CVMX_ERROR_FLAGS_ECC_SINGLE_BIT;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<16 /* l2c */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_INT_STAT[L2DSEC]: L2D Single Error corrected (SEC)\n"
        "    NOTE: This is the 'same' bit as L2D_ERR[SEC_ERR]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_INT_STAT;
    info.status_mask        = 1ull<<0 /* oob1 */;
    info.enable_addr        = CVMX_L2C_INT_EN;
    info.enable_mask        = 1ull<<0 /* oob1en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<16 /* l2c */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_INT_STAT[OOB1]: DMA Out of Bounds Interrupt Status Range#1\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_INT_STAT;
    info.status_mask        = 1ull<<1 /* oob2 */;
    info.enable_addr        = CVMX_L2C_INT_EN;
    info.enable_mask        = 1ull<<1 /* oob2en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<16 /* l2c */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_INT_STAT[OOB2]: DMA Out of Bounds Interrupt Status Range#2\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_INT_STAT;
    info.status_mask        = 1ull<<2 /* oob3 */;
    info.enable_addr        = CVMX_L2C_INT_EN;
    info.enable_mask        = 1ull<<2 /* oob3en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<16 /* l2c */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_INT_STAT[OOB3]: DMA Out of Bounds Interrupt Status Range#3\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_INT_STAT;
    info.status_mask        = 1ull<<4 /* l2tded */;
    info.enable_addr        = CVMX_L2C_INT_EN;
    info.enable_mask        = 1ull<<4 /* l2tdeden */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<16 /* l2c */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_INT_STAT[L2TDED]: L2T Double Bit Error detected (DED)\n"
        "    During every L2 Tag Probe, all 8 sets Tag's (at a\n"
        "    given index) are checked for double bit errors(DBEs).\n"
        "    This bit is set if ANY of the 8 sets contains a DBE.\n"
        "    DBEs also generated an interrupt(if enabled).\n"
        "    NOTE: This is the 'same' bit as L2T_ERR[DED_ERR]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_INT_STAT;
    info.status_mask        = 1ull<<6 /* l2dded */;
    info.enable_addr        = CVMX_L2C_INT_EN;
    info.enable_mask        = 1ull<<6 /* l2ddeden */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<16 /* l2c */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_INT_STAT[L2DDED]: L2D Double Error detected (DED)\n"
        "    NOTE: This is the 'same' bit as L2D_ERR[DED_ERR]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_INT_STAT;
    info.status_mask        = 1ull<<7 /* lck */;
    info.enable_addr        = CVMX_L2C_INT_EN;
    info.enable_mask        = 1ull<<7 /* lckena */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<16 /* l2c */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_INT_STAT[LCK]: SW attempted to LOCK DOWN the last available set of\n"
        "    the INDEX (which is ignored by HW - but reported to SW).\n"
        "    The LDD(L1 load-miss) for the LOCK operation is completed\n"
        "    successfully, however the address is NOT locked.\n"
        "    NOTE: 'Available' sets takes the L2C_SPAR*[UMSK*]\n"
        "    into account. For example, if diagnostic PPx has\n"
        "    UMSKx defined to only use SETs [1:0], and SET1 had\n"
        "    been previously LOCKED, then an attempt to LOCK the\n"
        "    last available SET0 would result in a LCKERR. (This\n"
        "    is to ensure that at least 1 SET at each INDEX is\n"
        "    not LOCKED for general use by other PPs).\n"
        "    NOTE: This is the 'same' bit as L2T_ERR[LCKERR]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_INT_STAT;
    info.status_mask        = 1ull<<8 /* lck2 */;
    info.enable_addr        = CVMX_L2C_INT_EN;
    info.enable_mask        = 1ull<<8 /* lck2ena */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<16 /* l2c */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_INT_STAT[LCK2]: HW detected a case where a Rd/Wr Miss from PP#n\n"
        "    could not find an available/unlocked set (for\n"
        "    replacement).\n"
        "    Most likely, this is a result of SW mixing SET\n"
        "    PARTITIONING with ADDRESS LOCKING. If SW allows\n"
        "    another PP to LOCKDOWN all SETs available to PP#n,\n"
        "    then a Rd/Wr Miss from PP#n will be unable\n"
        "    to determine a 'valid' replacement set (since LOCKED\n"
        "    addresses should NEVER be replaced).\n"
        "    If such an event occurs, the HW will select the smallest\n"
        "    available SET(specified by UMSK'x)' as the replacement\n"
        "    set, and the address is unlocked.\n"
        "    NOTE: This is the 'same' bit as L2T_ERR[LCKERR2]\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_L2D_ERR */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2D_ERR;
    info.status_mask        = 1ull<<3 /* sec_err */;
    info.enable_addr        = CVMX_L2D_ERR;
    info.enable_mask        = 1ull<<1 /* sec_intena */;
    info.flags              = CVMX_ERROR_FLAGS_ECC_SINGLE_BIT;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<16 /* l2c */;
    info.func               = __cvmx_error_handle_l2d_err_sec_err;
    info.user_info          = (long)
        "ERROR L2D_ERR[SEC_ERR]: L2D Single Error corrected (SEC)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2D_ERR;
    info.status_mask        = 1ull<<4 /* ded_err */;
    info.enable_addr        = CVMX_L2D_ERR;
    info.enable_mask        = 1ull<<2 /* ded_intena */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<16 /* l2c */;
    info.func               = __cvmx_error_handle_l2d_err_ded_err;
    info.user_info          = (long)
        "ERROR L2D_ERR[DED_ERR]: L2D Double Error detected (DED)\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_L2T_ERR */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2T_ERR;
    info.status_mask        = 1ull<<3 /* sec_err */;
    info.enable_addr        = CVMX_L2T_ERR;
    info.enable_mask        = 1ull<<1 /* sec_intena */;
    info.flags              = CVMX_ERROR_FLAGS_ECC_SINGLE_BIT;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<16 /* l2c */;
    info.func               = __cvmx_error_handle_l2t_err_sec_err;
    info.user_info          = (long)
        "ERROR L2T_ERR[SEC_ERR]: L2T Single Bit Error corrected (SEC)\n"
        "    During every L2 Tag Probe, all 8 sets Tag's (at a\n"
        "    given index) are checked for single bit errors(SBEs).\n"
        "    This bit is set if ANY of the 8 sets contains an SBE.\n"
        "    SBEs are auto corrected in HW and generate an\n"
        "    interrupt(if enabled).\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2T_ERR;
    info.status_mask        = 1ull<<4 /* ded_err */;
    info.enable_addr        = CVMX_L2T_ERR;
    info.enable_mask        = 1ull<<2 /* ded_intena */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<16 /* l2c */;
    info.func               = __cvmx_error_handle_l2t_err_ded_err;
    info.user_info          = (long)
        "ERROR L2T_ERR[DED_ERR]: L2T Double Bit Error detected (DED)\n"
        "    During every L2 Tag Probe, all 8 sets Tag's (at a\n"
        "    given index) are checked for double bit errors(DBEs).\n"
        "    This bit is set if ANY of the 8 sets contains a DBE.\n"
        "    DBEs also generated an interrupt(if enabled).\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2T_ERR;
    info.status_mask        = 1ull<<24 /* lckerr */;
    info.enable_addr        = CVMX_L2T_ERR;
    info.enable_mask        = 1ull<<25 /* lck_intena */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<16 /* l2c */;
    info.func               = __cvmx_error_handle_l2t_err_lckerr;
    info.user_info          = (long)
        "ERROR L2T_ERR[LCKERR]: SW attempted to LOCK DOWN the last available set of\n"
        "    the INDEX (which is ignored by HW - but reported to SW).\n"
        "    The LDD(L1 load-miss) for the LOCK operation is completed\n"
        "    successfully, however the address is NOT locked.\n"
        "    NOTE: 'Available' sets takes the L2C_SPAR*[UMSK*]\n"
        "    into account. For example, if diagnostic PPx has\n"
        "    UMSKx defined to only use SETs [1:0], and SET1 had\n"
        "    been previously LOCKED, then an attempt to LOCK the\n"
        "    last available SET0 would result in a LCKERR. (This\n"
        "    is to ensure that at least 1 SET at each INDEX is\n"
        "    not LOCKED for general use by other PPs).\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2T_ERR;
    info.status_mask        = 1ull<<26 /* lckerr2 */;
    info.enable_addr        = CVMX_L2T_ERR;
    info.enable_mask        = 1ull<<27 /* lck_intena2 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<16 /* l2c */;
    info.func               = __cvmx_error_handle_l2t_err_lckerr2;
    info.user_info          = (long)
        "ERROR L2T_ERR[LCKERR2]: HW detected a case where a Rd/Wr Miss from PP#n\n"
        "    could not find an available/unlocked set (for\n"
        "    replacement).\n"
        "    Most likely, this is a result of SW mixing SET\n"
        "    PARTITIONING with ADDRESS LOCKING. If SW allows\n"
        "    another PP to LOCKDOWN all SETs available to PP#n,\n"
        "    then a Rd/Wr Miss from PP#n will be unable\n"
        "    to determine a 'valid' replacement set (since LOCKED\n"
        "    addresses should NEVER be replaced).\n"
        "    If such an event occurs, the HW will select the smallest\n"
        "    available SET(specified by UMSK'x)' as the replacement\n"
        "    set, and the address is unlocked.\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_AGL_GMX_BAD_REG */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_AGL_GMX_BAD_REG;
    info.status_mask        = 1ull<<32 /* ovrflw */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_MGMT_PORT;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<28 /* agl */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR AGL_GMX_BAD_REG[OVRFLW]: RX FIFO overflow (MII0)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_AGL_GMX_BAD_REG;
    info.status_mask        = 1ull<<33 /* txpop */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_MGMT_PORT;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<28 /* agl */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR AGL_GMX_BAD_REG[TXPOP]: TX FIFO underflow (MII0)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_AGL_GMX_BAD_REG;
    info.status_mask        = 1ull<<34 /* txpsh */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_MGMT_PORT;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<28 /* agl */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR AGL_GMX_BAD_REG[TXPSH]: TX FIFO overflow (MII0)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_AGL_GMX_BAD_REG;
    info.status_mask        = 1ull<<35 /* ovrflw1 */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_MGMT_PORT;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<28 /* agl */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR AGL_GMX_BAD_REG[OVRFLW1]: RX FIFO overflow (MII1)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_AGL_GMX_BAD_REG;
    info.status_mask        = 1ull<<36 /* txpop1 */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_MGMT_PORT;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<28 /* agl */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR AGL_GMX_BAD_REG[TXPOP1]: TX FIFO underflow (MII1)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_AGL_GMX_BAD_REG;
    info.status_mask        = 1ull<<37 /* txpsh1 */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_MGMT_PORT;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<28 /* agl */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR AGL_GMX_BAD_REG[TXPSH1]: TX FIFO overflow (MII1)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_AGL_GMX_BAD_REG;
    info.status_mask        = 0x3ull<<2 /* out_ovr */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_MGMT_PORT;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<28 /* agl */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR AGL_GMX_BAD_REG[OUT_OVR]: Outbound data FIFO overflow\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_AGL_GMX_BAD_REG;
    info.status_mask        = 1ull<<22 /* loststat */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_MGMT_PORT;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<28 /* agl */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR AGL_GMX_BAD_REG[LOSTSTAT]: TX Statistics data was over-written\n"
        "    TX Stats are corrupted\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_AGL_GMX_RXX_INT_REG(0) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_AGL_GMX_RXX_INT_REG(0);
    info.status_mask        = 1ull<<8 /* skperr */;
    info.enable_addr        = CVMX_AGL_GMX_RXX_INT_EN(0);
    info.enable_mask        = 1ull<<8 /* skperr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_MGMT_PORT;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<28 /* agl */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR AGL_GMX_RXX_INT_REG(0)[SKPERR]: Skipper error\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_AGL_GMX_RXX_INT_REG(0);
    info.status_mask        = 1ull<<10 /* ovrerr */;
    info.enable_addr        = CVMX_AGL_GMX_RXX_INT_EN(0);
    info.enable_mask        = 1ull<<10 /* ovrerr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_MGMT_PORT;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<28 /* agl */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR AGL_GMX_RXX_INT_REG(0)[OVRERR]: Internal Data Aggregation Overflow\n"
        "    This interrupt should never assert\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_AGL_GMX_RXX_INT_REG(1) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_AGL_GMX_RXX_INT_REG(1);
    info.status_mask        = 1ull<<8 /* skperr */;
    info.enable_addr        = CVMX_AGL_GMX_RXX_INT_EN(1);
    info.enable_mask        = 1ull<<8 /* skperr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_MGMT_PORT;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<28 /* agl */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR AGL_GMX_RXX_INT_REG(1)[SKPERR]: Skipper error\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_AGL_GMX_RXX_INT_REG(1);
    info.status_mask        = 1ull<<10 /* ovrerr */;
    info.enable_addr        = CVMX_AGL_GMX_RXX_INT_EN(1);
    info.enable_mask        = 1ull<<10 /* ovrerr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_MGMT_PORT;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<28 /* agl */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR AGL_GMX_RXX_INT_REG(1)[OVRERR]: Internal Data Aggregation Overflow\n"
        "    This interrupt should never assert\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_AGL_GMX_TX_INT_REG */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_AGL_GMX_TX_INT_REG;
    info.status_mask        = 1ull<<0 /* pko_nxa */;
    info.enable_addr        = CVMX_AGL_GMX_TX_INT_EN;
    info.enable_mask        = 1ull<<0 /* pko_nxa */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_MGMT_PORT;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<28 /* agl */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR AGL_GMX_TX_INT_REG[PKO_NXA]: Port address out-of-range from PKO Interface\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_AGL_GMX_TX_INT_REG;
    info.status_mask        = 0x3ull<<2 /* undflw */;
    info.enable_addr        = CVMX_AGL_GMX_TX_INT_EN;
    info.enable_mask        = 0x3ull<<2 /* undflw */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_MGMT_PORT;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<28 /* agl */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR AGL_GMX_TX_INT_REG[UNDFLW]: TX Underflow (MII mode only)\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_GMXX_BAD_REG(0) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_BAD_REG(0);
    info.status_mask        = 0xfull<<2 /* out_ovr */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_BAD_REG(0)[OUT_OVR]: Outbound data FIFO overflow (per port)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_BAD_REG(0);
    info.status_mask        = 0xfull<<22 /* loststat */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_BAD_REG(0)[LOSTSTAT]: TX Statistics data was over-written\n"
        "    In SGMII, one bit per port\n"
        "    In XAUI, only port0 is used\n"
        "    TX Stats are corrupted\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_BAD_REG(0);
    info.status_mask        = 1ull<<26 /* statovr */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_BAD_REG(0)[STATOVR]: TX Statistics overflow\n"
        "    The common FIFO to SGMII and XAUI had an overflow\n"
        "    TX Stats are corrupted\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_BAD_REG(0);
    info.status_mask        = 0xfull<<27 /* inb_nxa */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_BAD_REG(0)[INB_NXA]: Inbound port > GMX_RX_PRTS\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_GMXX_RXX_INT_REG(0,0) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,0);
    info.status_mask        = 1ull<<1 /* carext */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,0);
    info.enable_mask        = 1ull<<1 /* carext */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,0)[CAREXT]: Carrier extend error\n"
        "    (SGMII/1000Base-X only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,0);
    info.status_mask        = 1ull<<8 /* skperr */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,0);
    info.enable_mask        = 1ull<<8 /* skperr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,0)[SKPERR]: Skipper error\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,0);
    info.status_mask        = 1ull<<10 /* ovrerr */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,0);
    info.enable_mask        = 1ull<<10 /* ovrerr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,0)[OVRERR]: Internal Data Aggregation Overflow\n"
        "    This interrupt should never assert\n"
        "    (SGMII/1000Base-X only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,0);
    info.status_mask        = 1ull<<20 /* loc_fault */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,0);
    info.enable_mask        = 1ull<<20 /* loc_fault */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,0)[LOC_FAULT]: Local Fault Sequence Deteted\n"
        "    (XAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,0);
    info.status_mask        = 1ull<<21 /* rem_fault */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,0);
    info.enable_mask        = 1ull<<21 /* rem_fault */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,0)[REM_FAULT]: Remote Fault Sequence Deteted\n"
        "    (XAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,0);
    info.status_mask        = 1ull<<22 /* bad_seq */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,0);
    info.enable_mask        = 1ull<<22 /* bad_seq */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,0)[BAD_SEQ]: Reserved Sequence Deteted\n"
        "    (XAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,0);
    info.status_mask        = 1ull<<23 /* bad_term */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,0);
    info.enable_mask        = 1ull<<23 /* bad_term */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,0)[BAD_TERM]: Frame is terminated by control character other\n"
        "    than /T/.  The error propagation control\n"
        "    character /E/ will be included as part of the\n"
        "    frame and does not cause a frame termination.\n"
        "    (XAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,0);
    info.status_mask        = 1ull<<24 /* unsop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,0);
    info.enable_mask        = 1ull<<24 /* unsop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,0)[UNSOP]: Unexpected SOP\n"
        "    (XAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,0);
    info.status_mask        = 1ull<<25 /* uneop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,0);
    info.enable_mask        = 1ull<<25 /* uneop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,0)[UNEOP]: Unexpected EOP\n"
        "    (XAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,0);
    info.status_mask        = 1ull<<26 /* undat */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,0);
    info.enable_mask        = 1ull<<26 /* undat */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,0)[UNDAT]: Unexpected Data\n"
        "    (XAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,0);
    info.status_mask        = 1ull<<27 /* hg2fld */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,0);
    info.enable_mask        = 1ull<<27 /* hg2fld */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,0)[HG2FLD]: HiGig2 received message field error, as below\n"
        "    1) MSG_TYPE field not 6'b00_0000\n"
        "       i.e. it is not a FLOW CONTROL message, which\n"
        "       is the only defined type for HiGig2\n"
        "    2) FWD_TYPE field not 2'b00 i.e. Link Level msg\n"
        "       which is the only defined type for HiGig2\n"
        "    3) FC_OBJECT field is neither 4'b0000 for\n"
        "       Physical Link nor 4'b0010 for Logical Link.\n"
        "       Those are the only two defined types in HiGig2\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,0);
    info.status_mask        = 1ull<<28 /* hg2cc */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,0);
    info.enable_mask        = 1ull<<28 /* hg2cc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,0)[HG2CC]: HiGig2 received message CRC or Control char  error\n"
        "    Set when either CRC8 error detected or when\n"
        "    a Control Character is found in the message\n"
        "    bytes after the K.SOM\n"
        "    NOTE: HG2CC has higher priority than HG2FLD\n"
        "          i.e. a HiGig2 message that results in HG2CC\n"
        "          getting set, will never set HG2FLD.\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_GMXX_RXX_INT_REG(1,0) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,0);
    info.status_mask        = 1ull<<1 /* carext */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,0);
    info.enable_mask        = 1ull<<1 /* carext */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,0)[CAREXT]: Carrier extend error\n"
        "    (SGMII/1000Base-X only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,0);
    info.status_mask        = 1ull<<8 /* skperr */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,0);
    info.enable_mask        = 1ull<<8 /* skperr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,0)[SKPERR]: Skipper error\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,0);
    info.status_mask        = 1ull<<10 /* ovrerr */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,0);
    info.enable_mask        = 1ull<<10 /* ovrerr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,0)[OVRERR]: Internal Data Aggregation Overflow\n"
        "    This interrupt should never assert\n"
        "    (SGMII/1000Base-X only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,0);
    info.status_mask        = 1ull<<20 /* loc_fault */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,0);
    info.enable_mask        = 1ull<<20 /* loc_fault */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,0)[LOC_FAULT]: Local Fault Sequence Deteted\n"
        "    (XAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,0);
    info.status_mask        = 1ull<<21 /* rem_fault */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,0);
    info.enable_mask        = 1ull<<21 /* rem_fault */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,0)[REM_FAULT]: Remote Fault Sequence Deteted\n"
        "    (XAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,0);
    info.status_mask        = 1ull<<22 /* bad_seq */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,0);
    info.enable_mask        = 1ull<<22 /* bad_seq */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,0)[BAD_SEQ]: Reserved Sequence Deteted\n"
        "    (XAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,0);
    info.status_mask        = 1ull<<23 /* bad_term */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,0);
    info.enable_mask        = 1ull<<23 /* bad_term */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,0)[BAD_TERM]: Frame is terminated by control character other\n"
        "    than /T/.  The error propagation control\n"
        "    character /E/ will be included as part of the\n"
        "    frame and does not cause a frame termination.\n"
        "    (XAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,0);
    info.status_mask        = 1ull<<24 /* unsop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,0);
    info.enable_mask        = 1ull<<24 /* unsop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,0)[UNSOP]: Unexpected SOP\n"
        "    (XAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,0);
    info.status_mask        = 1ull<<25 /* uneop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,0);
    info.enable_mask        = 1ull<<25 /* uneop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,0)[UNEOP]: Unexpected EOP\n"
        "    (XAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,0);
    info.status_mask        = 1ull<<26 /* undat */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,0);
    info.enable_mask        = 1ull<<26 /* undat */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,0)[UNDAT]: Unexpected Data\n"
        "    (XAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,0);
    info.status_mask        = 1ull<<27 /* hg2fld */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,0);
    info.enable_mask        = 1ull<<27 /* hg2fld */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,0)[HG2FLD]: HiGig2 received message field error, as below\n"
        "    1) MSG_TYPE field not 6'b00_0000\n"
        "       i.e. it is not a FLOW CONTROL message, which\n"
        "       is the only defined type for HiGig2\n"
        "    2) FWD_TYPE field not 2'b00 i.e. Link Level msg\n"
        "       which is the only defined type for HiGig2\n"
        "    3) FC_OBJECT field is neither 4'b0000 for\n"
        "       Physical Link nor 4'b0010 for Logical Link.\n"
        "       Those are the only two defined types in HiGig2\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,0);
    info.status_mask        = 1ull<<28 /* hg2cc */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,0);
    info.enable_mask        = 1ull<<28 /* hg2cc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,0)[HG2CC]: HiGig2 received message CRC or Control char  error\n"
        "    Set when either CRC8 error detected or when\n"
        "    a Control Character is found in the message\n"
        "    bytes after the K.SOM\n"
        "    NOTE: HG2CC has higher priority than HG2FLD\n"
        "          i.e. a HiGig2 message that results in HG2CC\n"
        "          getting set, will never set HG2FLD.\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_GMXX_RXX_INT_REG(2,0) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,0);
    info.status_mask        = 1ull<<1 /* carext */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,0);
    info.enable_mask        = 1ull<<1 /* carext */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,0)[CAREXT]: Carrier extend error\n"
        "    (SGMII/1000Base-X only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,0);
    info.status_mask        = 1ull<<8 /* skperr */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,0);
    info.enable_mask        = 1ull<<8 /* skperr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,0)[SKPERR]: Skipper error\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,0);
    info.status_mask        = 1ull<<10 /* ovrerr */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,0);
    info.enable_mask        = 1ull<<10 /* ovrerr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,0)[OVRERR]: Internal Data Aggregation Overflow\n"
        "    This interrupt should never assert\n"
        "    (SGMII/1000Base-X only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,0);
    info.status_mask        = 1ull<<20 /* loc_fault */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,0);
    info.enable_mask        = 1ull<<20 /* loc_fault */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,0)[LOC_FAULT]: Local Fault Sequence Deteted\n"
        "    (XAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,0);
    info.status_mask        = 1ull<<21 /* rem_fault */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,0);
    info.enable_mask        = 1ull<<21 /* rem_fault */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,0)[REM_FAULT]: Remote Fault Sequence Deteted\n"
        "    (XAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,0);
    info.status_mask        = 1ull<<22 /* bad_seq */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,0);
    info.enable_mask        = 1ull<<22 /* bad_seq */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,0)[BAD_SEQ]: Reserved Sequence Deteted\n"
        "    (XAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,0);
    info.status_mask        = 1ull<<23 /* bad_term */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,0);
    info.enable_mask        = 1ull<<23 /* bad_term */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,0)[BAD_TERM]: Frame is terminated by control character other\n"
        "    than /T/.  The error propagation control\n"
        "    character /E/ will be included as part of the\n"
        "    frame and does not cause a frame termination.\n"
        "    (XAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,0);
    info.status_mask        = 1ull<<24 /* unsop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,0);
    info.enable_mask        = 1ull<<24 /* unsop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,0)[UNSOP]: Unexpected SOP\n"
        "    (XAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,0);
    info.status_mask        = 1ull<<25 /* uneop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,0);
    info.enable_mask        = 1ull<<25 /* uneop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,0)[UNEOP]: Unexpected EOP\n"
        "    (XAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,0);
    info.status_mask        = 1ull<<26 /* undat */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,0);
    info.enable_mask        = 1ull<<26 /* undat */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,0)[UNDAT]: Unexpected Data\n"
        "    (XAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,0);
    info.status_mask        = 1ull<<27 /* hg2fld */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,0);
    info.enable_mask        = 1ull<<27 /* hg2fld */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,0)[HG2FLD]: HiGig2 received message field error, as below\n"
        "    1) MSG_TYPE field not 6'b00_0000\n"
        "       i.e. it is not a FLOW CONTROL message, which\n"
        "       is the only defined type for HiGig2\n"
        "    2) FWD_TYPE field not 2'b00 i.e. Link Level msg\n"
        "       which is the only defined type for HiGig2\n"
        "    3) FC_OBJECT field is neither 4'b0000 for\n"
        "       Physical Link nor 4'b0010 for Logical Link.\n"
        "       Those are the only two defined types in HiGig2\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,0);
    info.status_mask        = 1ull<<28 /* hg2cc */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,0);
    info.enable_mask        = 1ull<<28 /* hg2cc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,0)[HG2CC]: HiGig2 received message CRC or Control char  error\n"
        "    Set when either CRC8 error detected or when\n"
        "    a Control Character is found in the message\n"
        "    bytes after the K.SOM\n"
        "    NOTE: HG2CC has higher priority than HG2FLD\n"
        "          i.e. a HiGig2 message that results in HG2CC\n"
        "          getting set, will never set HG2FLD.\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_GMXX_RXX_INT_REG(3,0) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,0);
    info.status_mask        = 1ull<<1 /* carext */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,0);
    info.enable_mask        = 1ull<<1 /* carext */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,0)[CAREXT]: Carrier extend error\n"
        "    (SGMII/1000Base-X only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,0);
    info.status_mask        = 1ull<<8 /* skperr */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,0);
    info.enable_mask        = 1ull<<8 /* skperr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,0)[SKPERR]: Skipper error\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,0);
    info.status_mask        = 1ull<<10 /* ovrerr */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,0);
    info.enable_mask        = 1ull<<10 /* ovrerr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,0)[OVRERR]: Internal Data Aggregation Overflow\n"
        "    This interrupt should never assert\n"
        "    (SGMII/1000Base-X only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,0);
    info.status_mask        = 1ull<<20 /* loc_fault */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,0);
    info.enable_mask        = 1ull<<20 /* loc_fault */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,0)[LOC_FAULT]: Local Fault Sequence Deteted\n"
        "    (XAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,0);
    info.status_mask        = 1ull<<21 /* rem_fault */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,0);
    info.enable_mask        = 1ull<<21 /* rem_fault */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,0)[REM_FAULT]: Remote Fault Sequence Deteted\n"
        "    (XAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,0);
    info.status_mask        = 1ull<<22 /* bad_seq */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,0);
    info.enable_mask        = 1ull<<22 /* bad_seq */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,0)[BAD_SEQ]: Reserved Sequence Deteted\n"
        "    (XAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,0);
    info.status_mask        = 1ull<<23 /* bad_term */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,0);
    info.enable_mask        = 1ull<<23 /* bad_term */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,0)[BAD_TERM]: Frame is terminated by control character other\n"
        "    than /T/.  The error propagation control\n"
        "    character /E/ will be included as part of the\n"
        "    frame and does not cause a frame termination.\n"
        "    (XAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,0);
    info.status_mask        = 1ull<<24 /* unsop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,0);
    info.enable_mask        = 1ull<<24 /* unsop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,0)[UNSOP]: Unexpected SOP\n"
        "    (XAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,0);
    info.status_mask        = 1ull<<25 /* uneop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,0);
    info.enable_mask        = 1ull<<25 /* uneop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,0)[UNEOP]: Unexpected EOP\n"
        "    (XAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,0);
    info.status_mask        = 1ull<<26 /* undat */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,0);
    info.enable_mask        = 1ull<<26 /* undat */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,0)[UNDAT]: Unexpected Data\n"
        "    (XAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,0);
    info.status_mask        = 1ull<<27 /* hg2fld */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,0);
    info.enable_mask        = 1ull<<27 /* hg2fld */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,0)[HG2FLD]: HiGig2 received message field error, as below\n"
        "    1) MSG_TYPE field not 6'b00_0000\n"
        "       i.e. it is not a FLOW CONTROL message, which\n"
        "       is the only defined type for HiGig2\n"
        "    2) FWD_TYPE field not 2'b00 i.e. Link Level msg\n"
        "       which is the only defined type for HiGig2\n"
        "    3) FC_OBJECT field is neither 4'b0000 for\n"
        "       Physical Link nor 4'b0010 for Logical Link.\n"
        "       Those are the only two defined types in HiGig2\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,0);
    info.status_mask        = 1ull<<28 /* hg2cc */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,0);
    info.enable_mask        = 1ull<<28 /* hg2cc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,0)[HG2CC]: HiGig2 received message CRC or Control char  error\n"
        "    Set when either CRC8 error detected or when\n"
        "    a Control Character is found in the message\n"
        "    bytes after the K.SOM\n"
        "    NOTE: HG2CC has higher priority than HG2FLD\n"
        "          i.e. a HiGig2 message that results in HG2CC\n"
        "          getting set, will never set HG2FLD.\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_GMXX_TX_INT_REG(0) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_TX_INT_REG(0);
    info.status_mask        = 1ull<<0 /* pko_nxa */;
    info.enable_addr        = CVMX_GMXX_TX_INT_EN(0);
    info.enable_mask        = 1ull<<0 /* pko_nxa */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_TX_INT_REG(0)[PKO_NXA]: Port address out-of-range from PKO Interface\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_TX_INT_REG(0);
    info.status_mask        = 0xfull<<2 /* undflw */;
    info.enable_addr        = CVMX_GMXX_TX_INT_EN(0);
    info.enable_mask        = 0xfull<<2 /* undflw */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_TX_INT_REG(0)[UNDFLW]: TX Underflow\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_MIO_BOOT_ERR */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_MIO_BOOT_ERR;
    info.status_mask        = 1ull<<0 /* adr_err */;
    info.enable_addr        = CVMX_MIO_BOOT_INT;
    info.enable_mask        = 1ull<<0 /* adr_int */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<0 /* mio */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR MIO_BOOT_ERR[ADR_ERR]: Address decode error\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_MIO_BOOT_ERR;
    info.status_mask        = 1ull<<1 /* wait_err */;
    info.enable_addr        = CVMX_MIO_BOOT_INT;
    info.enable_mask        = 1ull<<1 /* wait_int */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<0 /* mio */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR MIO_BOOT_ERR[WAIT_ERR]: Wait mode error\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_IPD_INT_SUM */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_IPD_INT_SUM;
    info.status_mask        = 1ull<<0 /* prc_par0 */;
    info.enable_addr        = CVMX_IPD_INT_ENB;
    info.enable_mask        = 1ull<<0 /* prc_par0 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<9 /* ipd */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR IPD_INT_SUM[PRC_PAR0]: Set when a parity error is dected for bits\n"
        "    [31:0] of the PBM memory.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_IPD_INT_SUM;
    info.status_mask        = 1ull<<1 /* prc_par1 */;
    info.enable_addr        = CVMX_IPD_INT_ENB;
    info.enable_mask        = 1ull<<1 /* prc_par1 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<9 /* ipd */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR IPD_INT_SUM[PRC_PAR1]: Set when a parity error is dected for bits\n"
        "    [63:32] of the PBM memory.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_IPD_INT_SUM;
    info.status_mask        = 1ull<<2 /* prc_par2 */;
    info.enable_addr        = CVMX_IPD_INT_ENB;
    info.enable_mask        = 1ull<<2 /* prc_par2 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<9 /* ipd */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR IPD_INT_SUM[PRC_PAR2]: Set when a parity error is dected for bits\n"
        "    [95:64] of the PBM memory.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_IPD_INT_SUM;
    info.status_mask        = 1ull<<3 /* prc_par3 */;
    info.enable_addr        = CVMX_IPD_INT_ENB;
    info.enable_mask        = 1ull<<3 /* prc_par3 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<9 /* ipd */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR IPD_INT_SUM[PRC_PAR3]: Set when a parity error is dected for bits\n"
        "    [127:96] of the PBM memory.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_IPD_INT_SUM;
    info.status_mask        = 1ull<<4 /* bp_sub */;
    info.enable_addr        = CVMX_IPD_INT_ENB;
    info.enable_mask        = 1ull<<4 /* bp_sub */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<9 /* ipd */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR IPD_INT_SUM[BP_SUB]: Set when a backpressure subtract is done with a\n"
        "    supplied illegal value.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_IPD_INT_SUM;
    info.status_mask        = 1ull<<5 /* dc_ovr */;
    info.enable_addr        = CVMX_IPD_INT_ENB;
    info.enable_mask        = 1ull<<5 /* dc_ovr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<9 /* ipd */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR IPD_INT_SUM[DC_OVR]: Set when the data credits to the IOB overflow.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_IPD_INT_SUM;
    info.status_mask        = 1ull<<6 /* cc_ovr */;
    info.enable_addr        = CVMX_IPD_INT_ENB;
    info.enable_mask        = 1ull<<6 /* cc_ovr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<9 /* ipd */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR IPD_INT_SUM[CC_OVR]: Set when the command credits to the IOB overflow.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_IPD_INT_SUM;
    info.status_mask        = 1ull<<7 /* c_coll */;
    info.enable_addr        = CVMX_IPD_INT_ENB;
    info.enable_mask        = 1ull<<7 /* c_coll */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<9 /* ipd */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR IPD_INT_SUM[C_COLL]: Set when the packet/WQE commands to be sent to IOB\n"
        "    collides.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_IPD_INT_SUM;
    info.status_mask        = 1ull<<8 /* d_coll */;
    info.enable_addr        = CVMX_IPD_INT_ENB;
    info.enable_mask        = 1ull<<8 /* d_coll */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<9 /* ipd */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR IPD_INT_SUM[D_COLL]: Set when the packet/WQE data to be sent to IOB\n"
        "    collides.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_IPD_INT_SUM;
    info.status_mask        = 1ull<<9 /* bc_ovr */;
    info.enable_addr        = CVMX_IPD_INT_ENB;
    info.enable_mask        = 1ull<<9 /* bc_ovr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<9 /* ipd */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR IPD_INT_SUM[BC_OVR]: Set when the byte-count to send to IOB overflows.\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_TIM_REG_ERROR */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_TIM_REG_ERROR;
    info.status_mask        = 0xffffull<<0 /* mask */;
    info.enable_addr        = CVMX_TIM_REG_INT_MASK;
    info.enable_mask        = 0xffffull<<0 /* mask */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<11 /* tim */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR TIM_REG_ERROR[MASK]: Bit mask indicating the rings in error\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_POW_ECC_ERR */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_POW_ECC_ERR;
    info.status_mask        = 1ull<<0 /* sbe */;
    info.enable_addr        = CVMX_POW_ECC_ERR;
    info.enable_mask        = 1ull<<2 /* sbe_ie */;
    info.flags              = CVMX_ERROR_FLAGS_ECC_SINGLE_BIT;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<12 /* pow */;
    info.func               = __cvmx_error_handle_pow_ecc_err_sbe;
    info.user_info          = (long)
        "ERROR POW_ECC_ERR[SBE]: Single bit error\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_POW_ECC_ERR;
    info.status_mask        = 1ull<<1 /* dbe */;
    info.enable_addr        = CVMX_POW_ECC_ERR;
    info.enable_mask        = 1ull<<3 /* dbe_ie */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<12 /* pow */;
    info.func               = __cvmx_error_handle_pow_ecc_err_dbe;
    info.user_info          = (long)
        "ERROR POW_ECC_ERR[DBE]: Double bit error\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_POW_ECC_ERR;
    info.status_mask        = 1ull<<12 /* rpe */;
    info.enable_addr        = CVMX_POW_ECC_ERR;
    info.enable_mask        = 1ull<<13 /* rpe_ie */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<12 /* pow */;
    info.func               = __cvmx_error_handle_pow_ecc_err_rpe;
    info.user_info          = (long)
        "ERROR POW_ECC_ERR[RPE]: Remote pointer error\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_POW_ECC_ERR;
    info.status_mask        = 0x1fffull<<16 /* iop */;
    info.enable_addr        = CVMX_POW_ECC_ERR;
    info.enable_mask        = 0x1fffull<<32 /* iop_ie */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<12 /* pow */;
    info.func               = __cvmx_error_handle_pow_ecc_err_iop;
    info.user_info          = (long)
        "ERROR POW_ECC_ERR[IOP]: Illegal operation errors\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_USBNX_INT_SUM(1) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(1);
    info.status_mask        = 1ull<<0 /* pr_po_e */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(1);
    info.enable_mask        = 1ull<<0 /* pr_po_e */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<15 /* usb1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(1)[PR_PO_E]: PP  Request Fifo Popped When Empty.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(1);
    info.status_mask        = 1ull<<1 /* pr_pu_f */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(1);
    info.enable_mask        = 1ull<<1 /* pr_pu_f */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<15 /* usb1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(1)[PR_PU_F]: PP  Request Fifo Pushed When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(1);
    info.status_mask        = 1ull<<2 /* nr_po_e */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(1);
    info.enable_mask        = 1ull<<2 /* nr_po_e */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<15 /* usb1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(1)[NR_PO_E]: NPI Request Fifo Popped When Empty.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(1);
    info.status_mask        = 1ull<<3 /* nr_pu_f */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(1);
    info.enable_mask        = 1ull<<3 /* nr_pu_f */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<15 /* usb1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(1)[NR_PU_F]: NPI Request Fifo Pushed When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(1);
    info.status_mask        = 1ull<<4 /* lr_po_e */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(1);
    info.enable_mask        = 1ull<<4 /* lr_po_e */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<15 /* usb1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(1)[LR_PO_E]: L2C Request Fifo Popped When Empty.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(1);
    info.status_mask        = 1ull<<5 /* lr_pu_f */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(1);
    info.enable_mask        = 1ull<<5 /* lr_pu_f */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<15 /* usb1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(1)[LR_PU_F]: L2C Request Fifo Pushed When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(1);
    info.status_mask        = 1ull<<6 /* pt_po_e */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(1);
    info.enable_mask        = 1ull<<6 /* pt_po_e */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<15 /* usb1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(1)[PT_PO_E]: PP  Trasaction Fifo Popped When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(1);
    info.status_mask        = 1ull<<7 /* pt_pu_f */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(1);
    info.enable_mask        = 1ull<<7 /* pt_pu_f */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<15 /* usb1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(1)[PT_PU_F]: PP  Trasaction Fifo Pushed When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(1);
    info.status_mask        = 1ull<<8 /* nt_po_e */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(1);
    info.enable_mask        = 1ull<<8 /* nt_po_e */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<15 /* usb1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(1)[NT_PO_E]: NPI Trasaction Fifo Popped When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(1);
    info.status_mask        = 1ull<<9 /* nt_pu_f */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(1);
    info.enable_mask        = 1ull<<9 /* nt_pu_f */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<15 /* usb1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(1)[NT_PU_F]: NPI Trasaction Fifo Pushed When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(1);
    info.status_mask        = 1ull<<10 /* lt_po_e */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(1);
    info.enable_mask        = 1ull<<10 /* lt_po_e */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<15 /* usb1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(1)[LT_PO_E]: L2C Trasaction Fifo Popped When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(1);
    info.status_mask        = 1ull<<11 /* lt_pu_f */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(1);
    info.enable_mask        = 1ull<<11 /* lt_pu_f */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<15 /* usb1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(1)[LT_PU_F]: L2C Trasaction Fifo Pushed When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(1);
    info.status_mask        = 1ull<<12 /* dcred_e */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(1);
    info.enable_mask        = 1ull<<12 /* dcred_e */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<15 /* usb1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(1)[DCRED_E]: Data Credit Fifo Pushed When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(1);
    info.status_mask        = 1ull<<13 /* dcred_f */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(1);
    info.enable_mask        = 1ull<<13 /* dcred_f */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<15 /* usb1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(1)[DCRED_F]: Data CreditFifo Pushed When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(1);
    info.status_mask        = 1ull<<14 /* l2c_s_e */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(1);
    info.enable_mask        = 1ull<<14 /* l2c_s_e */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<15 /* usb1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(1)[L2C_S_E]: L2C Credit Count Subtracted When Empty.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(1);
    info.status_mask        = 1ull<<15 /* l2c_a_f */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(1);
    info.enable_mask        = 1ull<<15 /* l2c_a_f */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<15 /* usb1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(1)[L2C_A_F]: L2C Credit Count Added When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(1);
    info.status_mask        = 1ull<<16 /* lt_fi_e */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(1);
    info.enable_mask        = 1ull<<16 /* l2_fi_e */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<15 /* usb1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(1)[LT_FI_E]: L2C Request Fifo Pushed When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(1);
    info.status_mask        = 1ull<<17 /* lt_fi_f */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(1);
    info.enable_mask        = 1ull<<17 /* l2_fi_f */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<15 /* usb1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(1)[LT_FI_F]: L2C Request Fifo Pushed When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(1);
    info.status_mask        = 1ull<<18 /* rg_fi_e */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(1);
    info.enable_mask        = 1ull<<18 /* rg_fi_e */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<15 /* usb1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(1)[RG_FI_E]: Register Request Fifo Pushed When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(1);
    info.status_mask        = 1ull<<19 /* rg_fi_f */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(1);
    info.enable_mask        = 1ull<<19 /* rg_fi_f */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<15 /* usb1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(1)[RG_FI_F]: Register Request Fifo Pushed When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(1);
    info.status_mask        = 1ull<<20 /* rq_q2_f */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(1);
    info.enable_mask        = 1ull<<20 /* rq_q2_f */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<15 /* usb1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(1)[RQ_Q2_F]: Request Queue-2 Fifo Pushed When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(1);
    info.status_mask        = 1ull<<21 /* rq_q2_e */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(1);
    info.enable_mask        = 1ull<<21 /* rq_q2_e */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<15 /* usb1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(1)[RQ_Q2_E]: Request Queue-2 Fifo Pushed When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(1);
    info.status_mask        = 1ull<<22 /* rq_q3_f */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(1);
    info.enable_mask        = 1ull<<22 /* rq_q3_f */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<15 /* usb1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(1)[RQ_Q3_F]: Request Queue-3 Fifo Pushed When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(1);
    info.status_mask        = 1ull<<23 /* rq_q3_e */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(1);
    info.enable_mask        = 1ull<<23 /* rq_q3_e */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<15 /* usb1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(1)[RQ_Q3_E]: Request Queue-3 Fifo Pushed When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(1);
    info.status_mask        = 1ull<<24 /* uod_pe */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(1);
    info.enable_mask        = 1ull<<24 /* uod_pe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<15 /* usb1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(1)[UOD_PE]: UOD Fifo Pop Empty.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(1);
    info.status_mask        = 1ull<<25 /* uod_pf */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(1);
    info.enable_mask        = 1ull<<25 /* uod_pf */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<15 /* usb1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(1)[UOD_PF]: UOD Fifo Push Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(1);
    info.status_mask        = 1ull<<32 /* ltl_f_pe */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(1);
    info.enable_mask        = 1ull<<32 /* ltl_f_pe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<15 /* usb1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(1)[LTL_F_PE]: L2C Transfer Length Fifo Pop Empty.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(1);
    info.status_mask        = 1ull<<33 /* ltl_f_pf */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(1);
    info.enable_mask        = 1ull<<33 /* ltl_f_pf */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<15 /* usb1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(1)[LTL_F_PF]: L2C Transfer Length Fifo Push Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(1);
    info.status_mask        = 1ull<<34 /* nd4o_rpe */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(1);
    info.enable_mask        = 1ull<<34 /* nd4o_rpe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<15 /* usb1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(1)[ND4O_RPE]: NCB DMA Out Request Fifo Pop Empty.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(1);
    info.status_mask        = 1ull<<35 /* nd4o_rpf */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(1);
    info.enable_mask        = 1ull<<35 /* nd4o_rpf */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<15 /* usb1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(1)[ND4O_RPF]: NCB DMA Out Request Fifo Push Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(1);
    info.status_mask        = 1ull<<36 /* nd4o_dpe */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(1);
    info.enable_mask        = 1ull<<36 /* nd4o_dpe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<15 /* usb1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(1)[ND4O_DPE]: NCB DMA Out Data Fifo Pop Empty.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(1);
    info.status_mask        = 1ull<<37 /* nd4o_dpf */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(1);
    info.enable_mask        = 1ull<<37 /* nd4o_dpf */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<15 /* usb1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(1)[ND4O_DPF]: NCB DMA Out Data Fifo Push Full.\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_PEXP_NPEI_INT_SUM */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<59 /* c0_ldwn */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<59 /* c0_ldwn */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[C0_LDWN]: Reset request due to link0 down status.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<21 /* c0_se */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<21 /* c0_se */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[C0_SE]: System Error, RC Mode Only.\n"
        "    Pcie Core 0. (cfg_sys_err_rc)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<38 /* c0_un_b0 */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<38 /* c0_un_b0 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[C0_UN_B0]: Received Unsupported N-TLP for Bar0.\n"
        "    Core 0.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<39 /* c0_un_b1 */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<39 /* c0_un_b1 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[C0_UN_B1]: Received Unsupported N-TLP for Bar1.\n"
        "    Core 0.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<40 /* c0_un_b2 */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<40 /* c0_un_b2 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[C0_UN_B2]: Received Unsupported N-TLP for Bar2.\n"
        "    Core 0.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<42 /* c0_un_bx */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<42 /* c0_un_bx */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[C0_UN_BX]: Received Unsupported N-TLP for unknown Bar.\n"
        "    Core 0.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<53 /* c0_un_wf */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<53 /* c0_un_wf */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[C0_UN_WF]: Received Unsupported N-TLP for filtered window\n"
        "    register. Core0.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<41 /* c0_un_wi */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<41 /* c0_un_wi */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[C0_UN_WI]: Received Unsupported N-TLP for Window Register.\n"
        "    Core 0.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<33 /* c0_up_b0 */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<33 /* c0_up_b0 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[C0_UP_B0]: Received Unsupported P-TLP for Bar0.\n"
        "    Core 0.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<34 /* c0_up_b1 */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<34 /* c0_up_b1 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[C0_UP_B1]: Received Unsupported P-TLP for Bar1.\n"
        "    Core 0.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<35 /* c0_up_b2 */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<35 /* c0_up_b2 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[C0_UP_B2]: Received Unsupported P-TLP for Bar2.\n"
        "    Core 0.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<37 /* c0_up_bx */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<37 /* c0_up_bx */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[C0_UP_BX]: Received Unsupported P-TLP for unknown Bar.\n"
        "    Core 0.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<55 /* c0_up_wf */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<55 /* c0_up_wf */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[C0_UP_WF]: Received Unsupported P-TLP for filtered window\n"
        "    register. Core0.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<36 /* c0_up_wi */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<36 /* c0_up_wi */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[C0_UP_WI]: Received Unsupported P-TLP for Window Register.\n"
        "    Core 0.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<23 /* c0_wake */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<23 /* c0_wake */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[C0_WAKE]: Wake up from Power Management Unit.\n"
        "    Pcie Core 0. (wake_n)\n"
        "    Octeon will never generate this interrupt.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<22 /* crs0_dr */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<22 /* crs0_dr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[CRS0_DR]: Had a CRS when Retries were disabled.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<20 /* crs0_er */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<20 /* crs0_er */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[CRS0_ER]: Had a CRS Timeout when Retries were enabled.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<60 /* c1_ldwn */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<60 /* c1_ldwn */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[C1_LDWN]: Reset request due to link1 down status.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<28 /* c1_se */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<28 /* c1_se */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[C1_SE]: System Error, RC Mode Only.\n"
        "    Pcie Core 1. (cfg_sys_err_rc)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<48 /* c1_un_b0 */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<48 /* c1_un_b0 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[C1_UN_B0]: Received Unsupported N-TLP for Bar0.\n"
        "    Core 1.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<49 /* c1_un_b1 */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<49 /* c1_un_b1 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[C1_UN_B1]: Received Unsupported N-TLP for Bar1.\n"
        "    Core 1.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<50 /* c1_un_b2 */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<50 /* c1_un_b2 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[C1_UN_B2]: Received Unsupported N-TLP for Bar2.\n"
        "    Core 1.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<52 /* c1_un_bx */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<52 /* c1_un_bx */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[C1_UN_BX]: Received Unsupported N-TLP for unknown Bar.\n"
        "    Core 1.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<54 /* c1_un_wf */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<54 /* c1_un_wf */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[C1_UN_WF]: Received Unsupported N-TLP for filtered window\n"
        "    register. Core1.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<51 /* c1_un_wi */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<51 /* c1_un_wi */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[C1_UN_WI]: Received Unsupported N-TLP for Window Register.\n"
        "    Core 1.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<43 /* c1_up_b0 */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<43 /* c1_up_b0 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[C1_UP_B0]: Received Unsupported P-TLP for Bar0.\n"
        "    Core 1.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<44 /* c1_up_b1 */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<44 /* c1_up_b1 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[C1_UP_B1]: Received Unsuppored P-TLP for Bar1.\n"
        "    Core 1.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<45 /* c1_up_b2 */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<45 /* c1_up_b2 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[C1_UP_B2]: Received Unsupported P-TLP for Bar2.\n"
        "    Core 1.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<47 /* c1_up_bx */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<47 /* c1_up_bx */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[C1_UP_BX]: Received Unsupported P-TLP for unknown Bar.\n"
        "    Core 1.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<56 /* c1_up_wf */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<56 /* c1_up_wf */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[C1_UP_WF]: Received Unsupported P-TLP for filtered window\n"
        "    register. Core1.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<46 /* c1_up_wi */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<46 /* c1_up_wi */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[C1_UP_WI]: Received Unsupported P-TLP for Window Register.\n"
        "    Core 1.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<30 /* c1_wake */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<30 /* c1_wake */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[C1_WAKE]: Wake up from Power Management Unit.\n"
        "    Pcie Core 1. (wake_n)\n"
        "    Octeon will never generate this interrupt.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<29 /* crs1_dr */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<29 /* crs1_dr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[CRS1_DR]: Had a CRS when Retries were disabled.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<27 /* crs1_er */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<27 /* crs1_er */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[CRS1_ER]: Had a CRS Timeout when Retries were enabled.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<2 /* bar0_to */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<2 /* bar0_to */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[BAR0_TO]: BAR0 R/W to a NCB device did not receive\n"
        "    read-data/commit in 0xffff core clocks.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<4 /* dma0dbo */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<4 /* dma0dbo */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[DMA0DBO]: DMA0 doorbell overflow.\n"
        "    Bit[32] of the doorbell count was set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<5 /* dma1dbo */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<5 /* dma1dbo */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[DMA1DBO]: DMA1 doorbell overflow.\n"
        "    Bit[32] of the doorbell count was set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<6 /* dma2dbo */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<6 /* dma2dbo */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[DMA2DBO]: DMA2 doorbell overflow.\n"
        "    Bit[32] of the doorbell count was set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<7 /* dma3dbo */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<7 /* dma3dbo */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[DMA3DBO]: DMA3 doorbell overflow.\n"
        "    Bit[32] of the doorbell count was set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<3 /* iob2big */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<3 /* iob2big */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[IOB2BIG]: A requested IOBDMA is to large.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<0 /* rml_rto */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<0 /* rml_rto */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[RML_RTO]: RML read did not return data in 0xffff core clocks.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<1 /* rml_wto */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<1 /* rml_wto */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[RML_WTO]: RML write did not get commit in 0xffff core clocks.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 1ull<<8 /* dma4dbo */;
    info.enable_addr        = CVMX_PEXP_NPEI_INT_ENB2;
    info.enable_mask        = 1ull<<8 /* dma4dbo */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_NPEI_INT_SUM[DMA4DBO]: DMA4 doorbell overflow.\n"
        "    Bit[32] of the doorbell count was set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_NPEI_INT_SUM;
    info.status_mask        = 0;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<3 /* npei */;
    info.func               = __cvmx_error_decode;
    info.user_info          = 0;
    fail |= cvmx_error_add(&info);

    /* CVMX_PESCX_DBG_INFO(0) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(0);
    info.status_mask        = 1ull<<0 /* spoison */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<0 /* spoison */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<57 /* c0_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(0)[SPOISON]: Poisoned TLP sent\n"
        "    peai__client0_tlp_ep & peai__client0_tlp_hv\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(0);
    info.status_mask        = 1ull<<2 /* rtlplle */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<2 /* rtlplle */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<57 /* c0_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(0)[RTLPLLE]: Received TLP has link layer error\n"
        "    pedc_radm_trgt1_dllp_abort & pedc__radm_trgt1_eot\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(0);
    info.status_mask        = 1ull<<3 /* recrce */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<3 /* recrce */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<57 /* c0_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(0)[RECRCE]: Received ECRC Error\n"
        "    pedc_radm_trgt1_ecrc_err & pedc__radm_trgt1_eot\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(0);
    info.status_mask        = 1ull<<4 /* rpoison */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<4 /* rpoison */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<57 /* c0_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(0)[RPOISON]: Received Poisoned TLP\n"
        "    pedc__radm_trgt1_poisoned & pedc__radm_trgt1_hv\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(0);
    info.status_mask        = 1ull<<5 /* rcemrc */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<5 /* rcemrc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<57 /* c0_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(0)[RCEMRC]: Received Correctable Error Message (RC Mode only)\n"
        "    pedc_radm_correctable_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(0);
    info.status_mask        = 1ull<<6 /* rnfemrc */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<6 /* rnfemrc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<57 /* c0_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(0)[RNFEMRC]: Received Non-Fatal Error Message (RC Mode only)\n"
        "    pedc_radm_nonfatal_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(0);
    info.status_mask        = 1ull<<7 /* rfemrc */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<7 /* rfemrc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<57 /* c0_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(0)[RFEMRC]: Received Fatal Error Message (RC Mode only)\n"
        "    pedc_radm_fatal_err\n"
        "    Bit set when a message with ERR_FATAL is set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(0);
    info.status_mask        = 1ull<<8 /* rpmerc */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<8 /* rpmerc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<57 /* c0_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(0)[RPMERC]: Received PME Message (RC Mode only)\n"
        "    pedc_radm_pm_pme\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(0);
    info.status_mask        = 1ull<<9 /* rptamrc */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<9 /* rptamrc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<57 /* c0_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(0)[RPTAMRC]: Received PME Turnoff Acknowledge Message\n"
        "    (RC Mode only)\n"
        "    pedc_radm_pm_to_ack\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(0);
    info.status_mask        = 1ull<<10 /* rumep */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<10 /* rumep */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<57 /* c0_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(0)[RUMEP]: Received Unlock Message (EP Mode Only)\n"
        "    pedc_radm_msg_unlock\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(0);
    info.status_mask        = 1ull<<11 /* rvdm */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<11 /* rvdm */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<57 /* c0_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(0)[RVDM]: Received Vendor-Defined Message\n"
        "    pedc_radm_vendor_msg\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(0);
    info.status_mask        = 1ull<<12 /* acto */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<12 /* acto */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<57 /* c0_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(0)[ACTO]: A Completion Timeout Occured\n"
        "    pedc_radm_cpl_timeout\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(0);
    info.status_mask        = 1ull<<13 /* rte */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<13 /* rte */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<57 /* c0_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(0)[RTE]: Replay Timer Expired\n"
        "    xdlh_replay_timeout_err\n"
        "    This bit is set when the REPLAY_TIMER expires in\n"
        "    the PCIE core. The probability of this bit being\n"
        "    set will increase with the traffic load.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(0);
    info.status_mask        = 1ull<<14 /* mre */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<14 /* mre */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<57 /* c0_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(0)[MRE]: Max Retries Exceeded\n"
        "    xdlh_replay_num_rlover_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(0);
    info.status_mask        = 1ull<<15 /* rdwdle */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<15 /* rdwdle */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<57 /* c0_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(0)[RDWDLE]: Received DLLP with DataLink Layer Error\n"
        "    rdlh_bad_dllp_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(0);
    info.status_mask        = 1ull<<16 /* rtwdle */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<16 /* rtwdle */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<57 /* c0_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(0)[RTWDLE]: Received TLP with DataLink Layer Error\n"
        "    rdlh_bad_tlp_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(0);
    info.status_mask        = 1ull<<17 /* dpeoosd */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<17 /* dpeoosd */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<57 /* c0_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(0)[DPEOOSD]: DLLP protocol error (out of sequence DLLP)\n"
        "    rdlh_prot_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(0);
    info.status_mask        = 1ull<<18 /* fcpvwt */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<18 /* fcpvwt */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<57 /* c0_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(0)[FCPVWT]: Flow Control Protocol Violation (Watchdog Timer)\n"
        "    rtlh_fc_prot_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(0);
    info.status_mask        = 1ull<<19 /* rpe */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<19 /* rpe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<57 /* c0_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(0)[RPE]: When the PHY reports 8B/10B decode error\n"
        "    (RxStatus = 3b100) or disparity error\n"
        "    (RxStatus = 3b111), the signal rmlh_rcvd_err will\n"
        "    be asserted.\n"
        "    rmlh_rcvd_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(0);
    info.status_mask        = 1ull<<20 /* fcuv */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<20 /* fcuv */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<57 /* c0_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(0)[FCUV]: Flow Control Update Violation (opt. checks)\n"
        "    int_xadm_fc_prot_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(0);
    info.status_mask        = 1ull<<21 /* rqo */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<21 /* rqo */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<57 /* c0_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(0)[RQO]: Receive queue overflow. Normally happens only when\n"
        "    flow control advertisements are ignored\n"
        "    radm_qoverflow\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(0);
    info.status_mask        = 1ull<<22 /* rauc */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<22 /* rauc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<57 /* c0_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(0)[RAUC]: Received an unexpected completion\n"
        "    radm_unexp_cpl_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(0);
    info.status_mask        = 1ull<<23 /* racur */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<23 /* racur */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<57 /* c0_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(0)[RACUR]: Received a completion with UR status\n"
        "    radm_rcvd_cpl_ur\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(0);
    info.status_mask        = 1ull<<24 /* racca */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<24 /* racca */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<57 /* c0_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(0)[RACCA]: Received a completion with CA status\n"
        "    radm_rcvd_cpl_ca\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(0);
    info.status_mask        = 1ull<<25 /* caar */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<25 /* caar */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<57 /* c0_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(0)[CAAR]: Completer aborted a request\n"
        "    radm_rcvd_ca_req\n"
        "    This bit will never be set because Octeon does\n"
        "    not generate Completer Aborts.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(0);
    info.status_mask        = 1ull<<26 /* rarwdns */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<26 /* rarwdns */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<57 /* c0_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(0)[RARWDNS]: Recieved a request which device does not support\n"
        "    radm_rcvd_ur_req\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(0);
    info.status_mask        = 1ull<<27 /* ramtlp */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<27 /* ramtlp */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<57 /* c0_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(0)[RAMTLP]: Received a malformed TLP\n"
        "    radm_mlf_tlp_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(0);
    info.status_mask        = 1ull<<28 /* racpp */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<28 /* racpp */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<57 /* c0_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(0)[RACPP]: Received a completion with poisoned payload\n"
        "    radm_rcvd_cpl_poisoned\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(0);
    info.status_mask        = 1ull<<29 /* rawwpp */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<29 /* rawwpp */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<57 /* c0_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(0)[RAWWPP]: Received a write with poisoned payload\n"
        "    radm_rcvd_wreq_poisoned\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(0);
    info.status_mask        = 1ull<<30 /* ecrc_e */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<30 /* ecrc_e */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<57 /* c0_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(0)[ECRC_E]: Received a ECRC error.\n"
        "    radm_ecrc_err\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_PESCX_DBG_INFO(1) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(1);
    info.status_mask        = 1ull<<0 /* spoison */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<0 /* spoison */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<58 /* c1_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(1)[SPOISON]: Poisoned TLP sent\n"
        "    peai__client0_tlp_ep & peai__client0_tlp_hv\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(1);
    info.status_mask        = 1ull<<2 /* rtlplle */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<2 /* rtlplle */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<58 /* c1_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(1)[RTLPLLE]: Received TLP has link layer error\n"
        "    pedc_radm_trgt1_dllp_abort & pedc__radm_trgt1_eot\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(1);
    info.status_mask        = 1ull<<3 /* recrce */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<3 /* recrce */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<58 /* c1_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(1)[RECRCE]: Received ECRC Error\n"
        "    pedc_radm_trgt1_ecrc_err & pedc__radm_trgt1_eot\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(1);
    info.status_mask        = 1ull<<4 /* rpoison */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<4 /* rpoison */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<58 /* c1_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(1)[RPOISON]: Received Poisoned TLP\n"
        "    pedc__radm_trgt1_poisoned & pedc__radm_trgt1_hv\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(1);
    info.status_mask        = 1ull<<5 /* rcemrc */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<5 /* rcemrc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<58 /* c1_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(1)[RCEMRC]: Received Correctable Error Message (RC Mode only)\n"
        "    pedc_radm_correctable_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(1);
    info.status_mask        = 1ull<<6 /* rnfemrc */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<6 /* rnfemrc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<58 /* c1_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(1)[RNFEMRC]: Received Non-Fatal Error Message (RC Mode only)\n"
        "    pedc_radm_nonfatal_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(1);
    info.status_mask        = 1ull<<7 /* rfemrc */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<7 /* rfemrc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<58 /* c1_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(1)[RFEMRC]: Received Fatal Error Message (RC Mode only)\n"
        "    pedc_radm_fatal_err\n"
        "    Bit set when a message with ERR_FATAL is set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(1);
    info.status_mask        = 1ull<<8 /* rpmerc */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<8 /* rpmerc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<58 /* c1_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(1)[RPMERC]: Received PME Message (RC Mode only)\n"
        "    pedc_radm_pm_pme\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(1);
    info.status_mask        = 1ull<<9 /* rptamrc */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<9 /* rptamrc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<58 /* c1_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(1)[RPTAMRC]: Received PME Turnoff Acknowledge Message\n"
        "    (RC Mode only)\n"
        "    pedc_radm_pm_to_ack\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(1);
    info.status_mask        = 1ull<<10 /* rumep */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<10 /* rumep */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<58 /* c1_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(1)[RUMEP]: Received Unlock Message (EP Mode Only)\n"
        "    pedc_radm_msg_unlock\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(1);
    info.status_mask        = 1ull<<11 /* rvdm */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<11 /* rvdm */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<58 /* c1_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(1)[RVDM]: Received Vendor-Defined Message\n"
        "    pedc_radm_vendor_msg\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(1);
    info.status_mask        = 1ull<<12 /* acto */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<12 /* acto */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<58 /* c1_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(1)[ACTO]: A Completion Timeout Occured\n"
        "    pedc_radm_cpl_timeout\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(1);
    info.status_mask        = 1ull<<13 /* rte */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<13 /* rte */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<58 /* c1_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(1)[RTE]: Replay Timer Expired\n"
        "    xdlh_replay_timeout_err\n"
        "    This bit is set when the REPLAY_TIMER expires in\n"
        "    the PCIE core. The probability of this bit being\n"
        "    set will increase with the traffic load.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(1);
    info.status_mask        = 1ull<<14 /* mre */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<14 /* mre */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<58 /* c1_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(1)[MRE]: Max Retries Exceeded\n"
        "    xdlh_replay_num_rlover_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(1);
    info.status_mask        = 1ull<<15 /* rdwdle */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<15 /* rdwdle */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<58 /* c1_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(1)[RDWDLE]: Received DLLP with DataLink Layer Error\n"
        "    rdlh_bad_dllp_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(1);
    info.status_mask        = 1ull<<16 /* rtwdle */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<16 /* rtwdle */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<58 /* c1_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(1)[RTWDLE]: Received TLP with DataLink Layer Error\n"
        "    rdlh_bad_tlp_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(1);
    info.status_mask        = 1ull<<17 /* dpeoosd */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<17 /* dpeoosd */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<58 /* c1_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(1)[DPEOOSD]: DLLP protocol error (out of sequence DLLP)\n"
        "    rdlh_prot_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(1);
    info.status_mask        = 1ull<<18 /* fcpvwt */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<18 /* fcpvwt */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<58 /* c1_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(1)[FCPVWT]: Flow Control Protocol Violation (Watchdog Timer)\n"
        "    rtlh_fc_prot_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(1);
    info.status_mask        = 1ull<<19 /* rpe */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<19 /* rpe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<58 /* c1_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(1)[RPE]: When the PHY reports 8B/10B decode error\n"
        "    (RxStatus = 3b100) or disparity error\n"
        "    (RxStatus = 3b111), the signal rmlh_rcvd_err will\n"
        "    be asserted.\n"
        "    rmlh_rcvd_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(1);
    info.status_mask        = 1ull<<20 /* fcuv */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<20 /* fcuv */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<58 /* c1_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(1)[FCUV]: Flow Control Update Violation (opt. checks)\n"
        "    int_xadm_fc_prot_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(1);
    info.status_mask        = 1ull<<21 /* rqo */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<21 /* rqo */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<58 /* c1_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(1)[RQO]: Receive queue overflow. Normally happens only when\n"
        "    flow control advertisements are ignored\n"
        "    radm_qoverflow\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(1);
    info.status_mask        = 1ull<<22 /* rauc */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<22 /* rauc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<58 /* c1_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(1)[RAUC]: Received an unexpected completion\n"
        "    radm_unexp_cpl_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(1);
    info.status_mask        = 1ull<<23 /* racur */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<23 /* racur */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<58 /* c1_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(1)[RACUR]: Received a completion with UR status\n"
        "    radm_rcvd_cpl_ur\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(1);
    info.status_mask        = 1ull<<24 /* racca */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<24 /* racca */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<58 /* c1_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(1)[RACCA]: Received a completion with CA status\n"
        "    radm_rcvd_cpl_ca\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(1);
    info.status_mask        = 1ull<<25 /* caar */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<25 /* caar */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<58 /* c1_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(1)[CAAR]: Completer aborted a request\n"
        "    radm_rcvd_ca_req\n"
        "    This bit will never be set because Octeon does\n"
        "    not generate Completer Aborts.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(1);
    info.status_mask        = 1ull<<26 /* rarwdns */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<26 /* rarwdns */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<58 /* c1_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(1)[RARWDNS]: Recieved a request which device does not support\n"
        "    radm_rcvd_ur_req\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(1);
    info.status_mask        = 1ull<<27 /* ramtlp */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<27 /* ramtlp */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<58 /* c1_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(1)[RAMTLP]: Received a malformed TLP\n"
        "    radm_mlf_tlp_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(1);
    info.status_mask        = 1ull<<28 /* racpp */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<28 /* racpp */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<58 /* c1_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(1)[RACPP]: Received a completion with poisoned payload\n"
        "    radm_rcvd_cpl_poisoned\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(1);
    info.status_mask        = 1ull<<29 /* rawwpp */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<29 /* rawwpp */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<58 /* c1_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(1)[RAWWPP]: Received a write with poisoned payload\n"
        "    radm_rcvd_wreq_poisoned\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PESCX_DBG_INFO(1);
    info.status_mask        = 1ull<<30 /* ecrc_e */;
    info.enable_addr        = CVMX_PESCX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<30 /* ecrc_e */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_INT_SUM;
    info.parent.status_mask = 1ull<<58 /* c1_exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PESCX_DBG_INFO(1)[ECRC_E]: Received a ECRC error.\n"
        "    radm_ecrc_err\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_RAD_REG_ERROR */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_RAD_REG_ERROR;
    info.status_mask        = 1ull<<0 /* doorbell */;
    info.enable_addr        = CVMX_RAD_REG_INT_MASK;
    info.enable_mask        = 1ull<<0 /* doorbell */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<14 /* rad */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR RAD_REG_ERROR[DOORBELL]: A doorbell count has overflowed\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_PKO_REG_ERROR */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PKO_REG_ERROR;
    info.status_mask        = 1ull<<0 /* parity */;
    info.enable_addr        = CVMX_PKO_REG_INT_MASK;
    info.enable_mask        = 1ull<<0 /* parity */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<10 /* pko */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PKO_REG_ERROR[PARITY]: Read parity error at port data buffer\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PKO_REG_ERROR;
    info.status_mask        = 1ull<<1 /* doorbell */;
    info.enable_addr        = CVMX_PKO_REG_INT_MASK;
    info.enable_mask        = 1ull<<1 /* doorbell */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<10 /* pko */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PKO_REG_ERROR[DOORBELL]: A doorbell count has overflowed\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PKO_REG_ERROR;
    info.status_mask        = 1ull<<2 /* currzero */;
    info.enable_addr        = CVMX_PKO_REG_INT_MASK;
    info.enable_mask        = 1ull<<2 /* currzero */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<10 /* pko */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PKO_REG_ERROR[CURRZERO]: A packet data pointer has size=0\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_PCSX_INTX_REG(0,0) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,0);
    info.status_mask        = 1ull<<2 /* an_err */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,0);
    info.enable_mask        = 1ull<<2 /* an_err_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,0)[AN_ERR]: AN Error, AN resolution function failed\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,0);
    info.status_mask        = 1ull<<3 /* txfifu */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,0);
    info.enable_mask        = 1ull<<3 /* txfifu_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,0)[TXFIFU]: Set whenever HW detects a TX fifo underflowflow\n"
        "    condition\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,0);
    info.status_mask        = 1ull<<4 /* txfifo */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,0);
    info.enable_mask        = 1ull<<4 /* txfifo_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,0)[TXFIFO]: Set whenever HW detects a TX fifo overflow\n"
        "    condition\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,0);
    info.status_mask        = 1ull<<5 /* txbad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,0);
    info.enable_mask        = 1ull<<5 /* txbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,0)[TXBAD]: Set by HW whenever tx st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,0);
    info.status_mask        = 1ull<<7 /* rxbad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,0);
    info.enable_mask        = 1ull<<7 /* rxbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,0)[RXBAD]: Set by HW whenever rx st machine reaches a  bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,0);
    info.status_mask        = 1ull<<8 /* rxlock */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,0);
    info.enable_mask        = 1ull<<8 /* rxlock_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,0)[RXLOCK]: Set by HW whenever code group Sync or bit lock\n"
        "    failure occurs\n"
        "    Cannot fire in loopback1 mode\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,0);
    info.status_mask        = 1ull<<9 /* an_bad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,0);
    info.enable_mask        = 1ull<<9 /* an_bad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,0)[AN_BAD]: Set by HW whenever AN st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,0);
    info.status_mask        = 1ull<<10 /* sync_bad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,0);
    info.enable_mask        = 1ull<<10 /* sync_bad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,0)[SYNC_BAD]: Set by HW whenever rx sync st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_PCSX_INTX_REG(1,0) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(1,0);
    info.status_mask        = 1ull<<2 /* an_err */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(1,0);
    info.enable_mask        = 1ull<<2 /* an_err_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(1,0)[AN_ERR]: AN Error, AN resolution function failed\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(1,0);
    info.status_mask        = 1ull<<3 /* txfifu */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(1,0);
    info.enable_mask        = 1ull<<3 /* txfifu_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(1,0)[TXFIFU]: Set whenever HW detects a TX fifo underflowflow\n"
        "    condition\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(1,0);
    info.status_mask        = 1ull<<4 /* txfifo */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(1,0);
    info.enable_mask        = 1ull<<4 /* txfifo_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(1,0)[TXFIFO]: Set whenever HW detects a TX fifo overflow\n"
        "    condition\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(1,0);
    info.status_mask        = 1ull<<5 /* txbad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(1,0);
    info.enable_mask        = 1ull<<5 /* txbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(1,0)[TXBAD]: Set by HW whenever tx st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(1,0);
    info.status_mask        = 1ull<<7 /* rxbad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(1,0);
    info.enable_mask        = 1ull<<7 /* rxbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(1,0)[RXBAD]: Set by HW whenever rx st machine reaches a  bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(1,0);
    info.status_mask        = 1ull<<8 /* rxlock */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(1,0);
    info.enable_mask        = 1ull<<8 /* rxlock_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(1,0)[RXLOCK]: Set by HW whenever code group Sync or bit lock\n"
        "    failure occurs\n"
        "    Cannot fire in loopback1 mode\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(1,0);
    info.status_mask        = 1ull<<9 /* an_bad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(1,0);
    info.enable_mask        = 1ull<<9 /* an_bad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(1,0)[AN_BAD]: Set by HW whenever AN st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(1,0);
    info.status_mask        = 1ull<<10 /* sync_bad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(1,0);
    info.enable_mask        = 1ull<<10 /* sync_bad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(1,0)[SYNC_BAD]: Set by HW whenever rx sync st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_PCSX_INTX_REG(2,0) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(2,0);
    info.status_mask        = 1ull<<2 /* an_err */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(2,0);
    info.enable_mask        = 1ull<<2 /* an_err_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(2,0)[AN_ERR]: AN Error, AN resolution function failed\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(2,0);
    info.status_mask        = 1ull<<3 /* txfifu */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(2,0);
    info.enable_mask        = 1ull<<3 /* txfifu_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(2,0)[TXFIFU]: Set whenever HW detects a TX fifo underflowflow\n"
        "    condition\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(2,0);
    info.status_mask        = 1ull<<4 /* txfifo */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(2,0);
    info.enable_mask        = 1ull<<4 /* txfifo_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(2,0)[TXFIFO]: Set whenever HW detects a TX fifo overflow\n"
        "    condition\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(2,0);
    info.status_mask        = 1ull<<5 /* txbad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(2,0);
    info.enable_mask        = 1ull<<5 /* txbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(2,0)[TXBAD]: Set by HW whenever tx st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(2,0);
    info.status_mask        = 1ull<<7 /* rxbad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(2,0);
    info.enable_mask        = 1ull<<7 /* rxbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(2,0)[RXBAD]: Set by HW whenever rx st machine reaches a  bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(2,0);
    info.status_mask        = 1ull<<8 /* rxlock */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(2,0);
    info.enable_mask        = 1ull<<8 /* rxlock_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(2,0)[RXLOCK]: Set by HW whenever code group Sync or bit lock\n"
        "    failure occurs\n"
        "    Cannot fire in loopback1 mode\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(2,0);
    info.status_mask        = 1ull<<9 /* an_bad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(2,0);
    info.enable_mask        = 1ull<<9 /* an_bad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(2,0)[AN_BAD]: Set by HW whenever AN st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(2,0);
    info.status_mask        = 1ull<<10 /* sync_bad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(2,0);
    info.enable_mask        = 1ull<<10 /* sync_bad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(2,0)[SYNC_BAD]: Set by HW whenever rx sync st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_PCSX_INTX_REG(3,0) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(3,0);
    info.status_mask        = 1ull<<2 /* an_err */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(3,0);
    info.enable_mask        = 1ull<<2 /* an_err_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(3,0)[AN_ERR]: AN Error, AN resolution function failed\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(3,0);
    info.status_mask        = 1ull<<3 /* txfifu */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(3,0);
    info.enable_mask        = 1ull<<3 /* txfifu_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(3,0)[TXFIFU]: Set whenever HW detects a TX fifo underflowflow\n"
        "    condition\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(3,0);
    info.status_mask        = 1ull<<4 /* txfifo */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(3,0);
    info.enable_mask        = 1ull<<4 /* txfifo_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(3,0)[TXFIFO]: Set whenever HW detects a TX fifo overflow\n"
        "    condition\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(3,0);
    info.status_mask        = 1ull<<5 /* txbad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(3,0);
    info.enable_mask        = 1ull<<5 /* txbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(3,0)[TXBAD]: Set by HW whenever tx st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(3,0);
    info.status_mask        = 1ull<<7 /* rxbad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(3,0);
    info.enable_mask        = 1ull<<7 /* rxbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(3,0)[RXBAD]: Set by HW whenever rx st machine reaches a  bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(3,0);
    info.status_mask        = 1ull<<8 /* rxlock */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(3,0);
    info.enable_mask        = 1ull<<8 /* rxlock_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(3,0)[RXLOCK]: Set by HW whenever code group Sync or bit lock\n"
        "    failure occurs\n"
        "    Cannot fire in loopback1 mode\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(3,0);
    info.status_mask        = 1ull<<9 /* an_bad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(3,0);
    info.enable_mask        = 1ull<<9 /* an_bad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(3,0)[AN_BAD]: Set by HW whenever AN st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(3,0);
    info.status_mask        = 1ull<<10 /* sync_bad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(3,0);
    info.enable_mask        = 1ull<<10 /* sync_bad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(3,0)[SYNC_BAD]: Set by HW whenever rx sync st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_PCSXX_INT_REG(0) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSXX_INT_REG(0);
    info.status_mask        = 1ull<<0 /* txflt */;
    info.enable_addr        = CVMX_PCSXX_INT_EN_REG(0);
    info.enable_mask        = 1ull<<0 /* txflt_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSXX_INT_REG(0)[TXFLT]: None defined at this time, always 0x0\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSXX_INT_REG(0);
    info.status_mask        = 1ull<<1 /* rxbad */;
    info.enable_addr        = CVMX_PCSXX_INT_EN_REG(0);
    info.enable_mask        = 1ull<<1 /* rxbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSXX_INT_REG(0)[RXBAD]: Set when RX state machine in bad state\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSXX_INT_REG(0);
    info.status_mask        = 1ull<<2 /* rxsynbad */;
    info.enable_addr        = CVMX_PCSXX_INT_EN_REG(0);
    info.enable_mask        = 1ull<<2 /* rxsynbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSXX_INT_REG(0)[RXSYNBAD]: Set when RX code grp sync st machine in bad state\n"
        "    in one of the 4 xaui lanes\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSXX_INT_REG(0);
    info.status_mask        = 1ull<<4 /* synlos */;
    info.enable_addr        = CVMX_PCSXX_INT_EN_REG(0);
    info.enable_mask        = 1ull<<4 /* synlos_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSXX_INT_REG(0)[SYNLOS]: Set when Code group sync lost on 1 or more  lanes\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSXX_INT_REG(0);
    info.status_mask        = 1ull<<5 /* algnlos */;
    info.enable_addr        = CVMX_PCSXX_INT_EN_REG(0);
    info.enable_mask        = 1ull<<5 /* algnlos_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSXX_INT_REG(0)[ALGNLOS]: Set when XAUI lanes lose alignment\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_PIP_INT_REG */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PIP_INT_REG;
    info.status_mask        = 1ull<<3 /* prtnxa */;
    info.enable_addr        = CVMX_PIP_INT_EN;
    info.enable_mask        = 1ull<<3 /* prtnxa */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<20 /* pip */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PIP_INT_REG[PRTNXA]: Non-existent port\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PIP_INT_REG;
    info.status_mask        = 1ull<<4 /* badtag */;
    info.enable_addr        = CVMX_PIP_INT_EN;
    info.enable_mask        = 1ull<<4 /* badtag */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<20 /* pip */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PIP_INT_REG[BADTAG]: A bad tag was sent from IPD\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PIP_INT_REG;
    info.status_mask        = 1ull<<5 /* skprunt */;
    info.enable_addr        = CVMX_PIP_INT_EN;
    info.enable_mask        = 1ull<<5 /* skprunt */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<20 /* pip */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PIP_INT_REG[SKPRUNT]: Packet was engulfed by skipper\n"
        "    This interrupt can occur with received PARTIAL\n"
        "    packets that are truncated to SKIP bytes or\n"
        "    smaller.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PIP_INT_REG;
    info.status_mask        = 1ull<<6 /* todoovr */;
    info.enable_addr        = CVMX_PIP_INT_EN;
    info.enable_mask        = 1ull<<6 /* todoovr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<20 /* pip */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PIP_INT_REG[TODOOVR]: Todo list overflow\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PIP_INT_REG;
    info.status_mask        = 1ull<<7 /* feperr */;
    info.enable_addr        = CVMX_PIP_INT_EN;
    info.enable_mask        = 1ull<<7 /* feperr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<20 /* pip */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PIP_INT_REG[FEPERR]: Parity Error in front end memory\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PIP_INT_REG;
    info.status_mask        = 1ull<<8 /* beperr */;
    info.enable_addr        = CVMX_PIP_INT_EN;
    info.enable_mask        = 1ull<<8 /* beperr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<20 /* pip */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PIP_INT_REG[BEPERR]: Parity Error in back end memory\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PIP_INT_REG;
    info.status_mask        = 1ull<<12 /* punyerr */;
    info.enable_addr        = CVMX_PIP_INT_EN;
    info.enable_mask        = 1ull<<12 /* punyerr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<20 /* pip */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PIP_INT_REG[PUNYERR]: Frame was received with length <=4B when CRC\n"
        "    stripping in IPD is enable\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_FPA_INT_SUM */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<0 /* fed0_sbe */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<0 /* fed0_sbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<5 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[FED0_SBE]: Set when a Single Bit Error is detected in FPF0.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<1 /* fed0_dbe */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<1 /* fed0_dbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<5 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[FED0_DBE]: Set when a Double Bit Error is detected in FPF0.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<2 /* fed1_sbe */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<2 /* fed1_sbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<5 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[FED1_SBE]: Set when a Single Bit Error is detected in FPF1.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<3 /* fed1_dbe */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<3 /* fed1_dbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<5 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[FED1_DBE]: Set when a Double Bit Error is detected in FPF1.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<4 /* q0_und */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<4 /* q0_und */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<5 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[Q0_UND]: Set when a Queue0 page count available goes\n"
        "    negative.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<5 /* q0_coff */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<5 /* q0_coff */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<5 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[Q0_COFF]: Set when a Queue0 stack end tag is present and\n"
        "    the count available is greater than pointers\n"
        "    present in the FPA.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<6 /* q0_perr */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<6 /* q0_perr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<5 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[Q0_PERR]: Set when a Queue0 pointer read from the stack in\n"
        "    the L2C does not have the FPA owner ship bit set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<7 /* q1_und */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<7 /* q1_und */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<5 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[Q1_UND]: Set when a Queue0 page count available goes\n"
        "    negative.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<8 /* q1_coff */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<8 /* q1_coff */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<5 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[Q1_COFF]: Set when a Queue0 stack end tag is present and\n"
        "    the count available is greater than pointers\n"
        "    present in the FPA.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<9 /* q1_perr */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<9 /* q1_perr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<5 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[Q1_PERR]: Set when a Queue0 pointer read from the stack in\n"
        "    the L2C does not have the FPA owner ship bit set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<10 /* q2_und */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<10 /* q2_und */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<5 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[Q2_UND]: Set when a Queue0 page count available goes\n"
        "    negative.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<11 /* q2_coff */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<11 /* q2_coff */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<5 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[Q2_COFF]: Set when a Queue0 stack end tag is present and\n"
        "    the count available is greater than than pointers\n"
        "    present in the FPA.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<12 /* q2_perr */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<12 /* q2_perr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<5 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[Q2_PERR]: Set when a Queue0 pointer read from the stack in\n"
        "    the L2C does not have the FPA owner ship bit set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<13 /* q3_und */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<13 /* q3_und */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<5 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[Q3_UND]: Set when a Queue0 page count available goes\n"
        "    negative.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<14 /* q3_coff */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<14 /* q3_coff */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<5 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[Q3_COFF]: Set when a Queue0 stack end tag is present and\n"
        "    the count available is greater than than pointers\n"
        "    present in the FPA.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<15 /* q3_perr */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<15 /* q3_perr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<5 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[Q3_PERR]: Set when a Queue0 pointer read from the stack in\n"
        "    the L2C does not have the FPA owner ship bit set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<16 /* q4_und */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<16 /* q4_und */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<5 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[Q4_UND]: Set when a Queue0 page count available goes\n"
        "    negative.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<17 /* q4_coff */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<17 /* q4_coff */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<5 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[Q4_COFF]: Set when a Queue0 stack end tag is present and\n"
        "    the count available is greater than than pointers\n"
        "    present in the FPA.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<18 /* q4_perr */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<18 /* q4_perr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<5 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[Q4_PERR]: Set when a Queue0 pointer read from the stack in\n"
        "    the L2C does not have the FPA owner ship bit set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<19 /* q5_und */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<19 /* q5_und */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<5 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[Q5_UND]: Set when a Queue0 page count available goes\n"
        "    negative.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<20 /* q5_coff */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<20 /* q5_coff */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<5 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[Q5_COFF]: Set when a Queue0 stack end tag is present and\n"
        "    the count available is greater than than pointers\n"
        "    present in the FPA.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<21 /* q5_perr */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<21 /* q5_perr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<5 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[Q5_PERR]: Set when a Queue0 pointer read from the stack in\n"
        "    the L2C does not have the FPA owner ship bit set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<22 /* q6_und */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<22 /* q6_und */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<5 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[Q6_UND]: Set when a Queue0 page count available goes\n"
        "    negative.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<23 /* q6_coff */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<23 /* q6_coff */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<5 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[Q6_COFF]: Set when a Queue0 stack end tag is present and\n"
        "    the count available is greater than than pointers\n"
        "    present in the FPA.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<24 /* q6_perr */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<24 /* q6_perr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<5 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[Q6_PERR]: Set when a Queue0 pointer read from the stack in\n"
        "    the L2C does not have the FPA owner ship bit set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<25 /* q7_und */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<25 /* q7_und */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<5 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[Q7_UND]: Set when a Queue0 page count available goes\n"
        "    negative.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<26 /* q7_coff */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<26 /* q7_coff */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<5 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[Q7_COFF]: Set when a Queue0 stack end tag is present and\n"
        "    the count available is greater than than pointers\n"
        "    present in the FPA.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<27 /* q7_perr */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<27 /* q7_perr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<5 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[Q7_PERR]: Set when a Queue0 pointer read from the stack in\n"
        "    the L2C does not have the FPA owner ship bit set.\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_LMCX_MEM_CFG0(0) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_LMCX_MEM_CFG0(0);
    info.status_mask        = 0xfull<<21 /* sec_err */;
    info.enable_addr        = CVMX_LMCX_MEM_CFG0(0);
    info.enable_mask        = 1ull<<19 /* intr_sec_ena */;
    info.flags              = CVMX_ERROR_FLAGS_ECC_SINGLE_BIT;
    info.group              = CVMX_ERROR_GROUP_LMC;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<17 /* lmc0 */;
    info.func               = __cvmx_error_handle_lmcx_mem_cfg0_sec_err;
    info.user_info          = (long)
        "ERROR LMCX_MEM_CFG0(0)[SEC_ERR]: Single Error (corrected) of Rd Data\n"
        "    In 64b mode, ecc is calculated on 2 cycle worth of data\n"
        "    [0] corresponds to DQ[63:0]_c0_p0\n"
        "    [1] corresponds to DQ[63:0]_c0_p1\n"
        "    [2] corresponds to DQ[63:0]_c1_p0\n"
        "    [3] corresponds to DQ[63:0]_c1_p1\n"
        "    In 32b mode, ecc is calculated on 4 cycle worth of data\n"
        "    [0] corresponds to [DQ[31:0]_c0_p1, DQ[31:0]_c0_p0]\n"
        "    [1] corresponds to [DQ[31:0]_c1_p1, DQ[31:0]_c1_p0]\n"
        "    [2] corresponds to [DQ[31:0]_c2_p1, DQ[31:0]_c2_p0]\n"
        "    [3] corresponds to [DQ[31:0]_c3_p1, DQ[31:0]_c3_p0]\n"
        "      where _cC_pP denotes cycle C and phase P\n"
        "    Write of 1 will clear the corresponding error bit\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_LMCX_MEM_CFG0(0);
    info.status_mask        = 0xfull<<25 /* ded_err */;
    info.enable_addr        = CVMX_LMCX_MEM_CFG0(0);
    info.enable_mask        = 1ull<<20 /* intr_ded_ena */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_LMC;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<17 /* lmc0 */;
    info.func               = __cvmx_error_handle_lmcx_mem_cfg0_ded_err;
    info.user_info          = (long)
        "ERROR LMCX_MEM_CFG0(0)[DED_ERR]: Double Error detected (DED) of Rd Data\n"
        "    In 64b mode, ecc is calculated on 2 cycle worth of data\n"
        "    [0] corresponds to DQ[63:0]_c0_p0\n"
        "    [1] corresponds to DQ[63:0]_c0_p1\n"
        "    [2] corresponds to DQ[63:0]_c1_p0\n"
        "    [3] corresponds to DQ[63:0]_c1_p1\n"
        "    In 32b mode, ecc is calculated on 4 cycle worth of data\n"
        "    [0] corresponds to [DQ[31:0]_c0_p1, DQ[31:0]_c0_p0]\n"
        "    [1] corresponds to [DQ[31:0]_c1_p1, DQ[31:0]_c1_p0]\n"
        "    [2] corresponds to [DQ[31:0]_c2_p1, DQ[31:0]_c2_p0]\n"
        "    [3] corresponds to [DQ[31:0]_c3_p1, DQ[31:0]_c3_p0]\n"
        "      where _cC_pP denotes cycle C and phase P\n"
        "    Write of 1 will clear the corresponding error bit\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_IOB_INT_SUM */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_IOB_INT_SUM;
    info.status_mask        = 1ull<<0 /* np_sop */;
    info.enable_addr        = CVMX_IOB_INT_ENB;
    info.enable_mask        = 1ull<<0 /* np_sop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<30 /* iob */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR IOB_INT_SUM[NP_SOP]: Set when a SOP is followed by an SOP for the same\n"
        "    port for a non-passthrough packet.\n"
        "    The first detected error associated with bits [5:0]\n"
        "    of this register will only be set here. A new bit\n"
        "    can be set when the previous reported bit is cleared.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_IOB_INT_SUM;
    info.status_mask        = 1ull<<1 /* np_eop */;
    info.enable_addr        = CVMX_IOB_INT_ENB;
    info.enable_mask        = 1ull<<1 /* np_eop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<30 /* iob */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR IOB_INT_SUM[NP_EOP]: Set when a EOP is followed by an EOP for the same\n"
        "    port for a non-passthrough packet.\n"
        "    The first detected error associated with bits [5:0]\n"
        "    of this register will only be set here. A new bit\n"
        "    can be set when the previous reported bit is cleared.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_IOB_INT_SUM;
    info.status_mask        = 1ull<<2 /* p_sop */;
    info.enable_addr        = CVMX_IOB_INT_ENB;
    info.enable_mask        = 1ull<<2 /* p_sop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<30 /* iob */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR IOB_INT_SUM[P_SOP]: Set when a SOP is followed by an SOP for the same\n"
        "    port for a passthrough packet.\n"
        "    The first detected error associated with bits [5:0]\n"
        "    of this register will only be set here. A new bit\n"
        "    can be set when the previous reported bit is cleared.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_IOB_INT_SUM;
    info.status_mask        = 1ull<<3 /* p_eop */;
    info.enable_addr        = CVMX_IOB_INT_ENB;
    info.enable_mask        = 1ull<<3 /* p_eop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<30 /* iob */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR IOB_INT_SUM[P_EOP]: Set when a EOP is followed by an EOP for the same\n"
        "    port for a passthrough packet.\n"
        "    The first detected error associated with bits [5:0]\n"
        "    of this register will only be set here. A new bit\n"
        "    can be set when the previous reported bit is cleared.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_IOB_INT_SUM;
    info.status_mask        = 1ull<<4 /* np_dat */;
    info.enable_addr        = CVMX_IOB_INT_ENB;
    info.enable_mask        = 1ull<<4 /* np_dat */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<30 /* iob */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR IOB_INT_SUM[NP_DAT]: Set when a data arrives before a SOP for the same\n"
        "    port for a non-passthrough packet.\n"
        "    The first detected error associated with bits [5:0]\n"
        "    of this register will only be set here. A new bit\n"
        "    can be set when the previous reported bit is cleared.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_IOB_INT_SUM;
    info.status_mask        = 1ull<<5 /* p_dat */;
    info.enable_addr        = CVMX_IOB_INT_ENB;
    info.enable_mask        = 1ull<<5 /* p_dat */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<30 /* iob */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR IOB_INT_SUM[P_DAT]: Set when a data arrives before a SOP for the same\n"
        "    port for a passthrough packet.\n"
        "    The first detected error associated with bits [5:0]\n"
        "    of this register will only be set here. A new bit\n"
        "    can be set when the previous reported bit is cleared.\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_USBNX_INT_SUM(0) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(0);
    info.status_mask        = 1ull<<0 /* pr_po_e */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(0);
    info.enable_mask        = 1ull<<0 /* pr_po_e */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(0)[PR_PO_E]: PP  Request Fifo Popped When Empty.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(0);
    info.status_mask        = 1ull<<1 /* pr_pu_f */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(0);
    info.enable_mask        = 1ull<<1 /* pr_pu_f */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(0)[PR_PU_F]: PP  Request Fifo Pushed When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(0);
    info.status_mask        = 1ull<<2 /* nr_po_e */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(0);
    info.enable_mask        = 1ull<<2 /* nr_po_e */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(0)[NR_PO_E]: NPI Request Fifo Popped When Empty.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(0);
    info.status_mask        = 1ull<<3 /* nr_pu_f */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(0);
    info.enable_mask        = 1ull<<3 /* nr_pu_f */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(0)[NR_PU_F]: NPI Request Fifo Pushed When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(0);
    info.status_mask        = 1ull<<4 /* lr_po_e */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(0);
    info.enable_mask        = 1ull<<4 /* lr_po_e */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(0)[LR_PO_E]: L2C Request Fifo Popped When Empty.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(0);
    info.status_mask        = 1ull<<5 /* lr_pu_f */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(0);
    info.enable_mask        = 1ull<<5 /* lr_pu_f */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(0)[LR_PU_F]: L2C Request Fifo Pushed When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(0);
    info.status_mask        = 1ull<<6 /* pt_po_e */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(0);
    info.enable_mask        = 1ull<<6 /* pt_po_e */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(0)[PT_PO_E]: PP  Trasaction Fifo Popped When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(0);
    info.status_mask        = 1ull<<7 /* pt_pu_f */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(0);
    info.enable_mask        = 1ull<<7 /* pt_pu_f */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(0)[PT_PU_F]: PP  Trasaction Fifo Pushed When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(0);
    info.status_mask        = 1ull<<8 /* nt_po_e */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(0);
    info.enable_mask        = 1ull<<8 /* nt_po_e */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(0)[NT_PO_E]: NPI Trasaction Fifo Popped When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(0);
    info.status_mask        = 1ull<<9 /* nt_pu_f */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(0);
    info.enable_mask        = 1ull<<9 /* nt_pu_f */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(0)[NT_PU_F]: NPI Trasaction Fifo Pushed When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(0);
    info.status_mask        = 1ull<<10 /* lt_po_e */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(0);
    info.enable_mask        = 1ull<<10 /* lt_po_e */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(0)[LT_PO_E]: L2C Trasaction Fifo Popped When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(0);
    info.status_mask        = 1ull<<11 /* lt_pu_f */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(0);
    info.enable_mask        = 1ull<<11 /* lt_pu_f */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(0)[LT_PU_F]: L2C Trasaction Fifo Pushed When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(0);
    info.status_mask        = 1ull<<12 /* dcred_e */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(0);
    info.enable_mask        = 1ull<<12 /* dcred_e */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(0)[DCRED_E]: Data Credit Fifo Pushed When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(0);
    info.status_mask        = 1ull<<13 /* dcred_f */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(0);
    info.enable_mask        = 1ull<<13 /* dcred_f */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(0)[DCRED_F]: Data CreditFifo Pushed When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(0);
    info.status_mask        = 1ull<<14 /* l2c_s_e */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(0);
    info.enable_mask        = 1ull<<14 /* l2c_s_e */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(0)[L2C_S_E]: L2C Credit Count Subtracted When Empty.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(0);
    info.status_mask        = 1ull<<15 /* l2c_a_f */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(0);
    info.enable_mask        = 1ull<<15 /* l2c_a_f */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(0)[L2C_A_F]: L2C Credit Count Added When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(0);
    info.status_mask        = 1ull<<16 /* lt_fi_e */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(0);
    info.enable_mask        = 1ull<<16 /* l2_fi_e */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(0)[LT_FI_E]: L2C Request Fifo Pushed When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(0);
    info.status_mask        = 1ull<<17 /* lt_fi_f */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(0);
    info.enable_mask        = 1ull<<17 /* l2_fi_f */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(0)[LT_FI_F]: L2C Request Fifo Pushed When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(0);
    info.status_mask        = 1ull<<18 /* rg_fi_e */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(0);
    info.enable_mask        = 1ull<<18 /* rg_fi_e */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(0)[RG_FI_E]: Register Request Fifo Pushed When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(0);
    info.status_mask        = 1ull<<19 /* rg_fi_f */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(0);
    info.enable_mask        = 1ull<<19 /* rg_fi_f */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(0)[RG_FI_F]: Register Request Fifo Pushed When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(0);
    info.status_mask        = 1ull<<20 /* rq_q2_f */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(0);
    info.enable_mask        = 1ull<<20 /* rq_q2_f */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(0)[RQ_Q2_F]: Request Queue-2 Fifo Pushed When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(0);
    info.status_mask        = 1ull<<21 /* rq_q2_e */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(0);
    info.enable_mask        = 1ull<<21 /* rq_q2_e */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(0)[RQ_Q2_E]: Request Queue-2 Fifo Pushed When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(0);
    info.status_mask        = 1ull<<22 /* rq_q3_f */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(0);
    info.enable_mask        = 1ull<<22 /* rq_q3_f */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(0)[RQ_Q3_F]: Request Queue-3 Fifo Pushed When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(0);
    info.status_mask        = 1ull<<23 /* rq_q3_e */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(0);
    info.enable_mask        = 1ull<<23 /* rq_q3_e */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(0)[RQ_Q3_E]: Request Queue-3 Fifo Pushed When Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(0);
    info.status_mask        = 1ull<<24 /* uod_pe */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(0);
    info.enable_mask        = 1ull<<24 /* uod_pe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(0)[UOD_PE]: UOD Fifo Pop Empty.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(0);
    info.status_mask        = 1ull<<25 /* uod_pf */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(0);
    info.enable_mask        = 1ull<<25 /* uod_pf */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(0)[UOD_PF]: UOD Fifo Push Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(0);
    info.status_mask        = 1ull<<32 /* ltl_f_pe */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(0);
    info.enable_mask        = 1ull<<32 /* ltl_f_pe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(0)[LTL_F_PE]: L2C Transfer Length Fifo Pop Empty.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(0);
    info.status_mask        = 1ull<<33 /* ltl_f_pf */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(0);
    info.enable_mask        = 1ull<<33 /* ltl_f_pf */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(0)[LTL_F_PF]: L2C Transfer Length Fifo Push Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(0);
    info.status_mask        = 1ull<<34 /* nd4o_rpe */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(0);
    info.enable_mask        = 1ull<<34 /* nd4o_rpe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(0)[ND4O_RPE]: NCB DMA Out Request Fifo Pop Empty.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(0);
    info.status_mask        = 1ull<<35 /* nd4o_rpf */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(0);
    info.enable_mask        = 1ull<<35 /* nd4o_rpf */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(0)[ND4O_RPF]: NCB DMA Out Request Fifo Push Full.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(0);
    info.status_mask        = 1ull<<36 /* nd4o_dpe */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(0);
    info.enable_mask        = 1ull<<36 /* nd4o_dpe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(0)[ND4O_DPE]: NCB DMA Out Data Fifo Pop Empty.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_USBNX_INT_SUM(0);
    info.status_mask        = 1ull<<37 /* nd4o_dpf */;
    info.enable_addr        = CVMX_USBNX_INT_ENB(0);
    info.enable_mask        = 1ull<<37 /* nd4o_dpf */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEXP_NPEI_RSL_INT_BLOCKS;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR USBNX_INT_SUM(0)[ND4O_DPF]: NCB DMA Out Data Fifo Push Full.\n";
    fail |= cvmx_error_add(&info);

    return fail;
}

