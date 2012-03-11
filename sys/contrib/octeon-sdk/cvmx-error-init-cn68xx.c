/***********************license start***************
 * Copyright (c) 2003-2012  Cavium Inc. (support@cavium.com). All rights
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

 *   * Neither the name of Cavium Inc. nor the names of
 *     its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written
 *     permission.

 * This Software, including technical data, may be subject to U.S. export  control
 * laws, including the U.S. Export Administration Act and its  associated
 * regulations, and may be subject to export or import  regulations in other
 * countries.

 * TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
 * AND WITH ALL FAULTS AND CAVIUM INC. MAKES NO PROMISES, REPRESENTATIONS OR
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
 * Automatically generated error messages for cn68xx.
 *
 * This file is auto generated. Do not edit.
 *
 * <hr>$Revision$<hr>
 *
 * <hr><h2>Error tree for CN68XX</h2>
 * @dot
 * digraph cn68xx
 * {
 *     rankdir=LR;
 *     node [shape=record, width=.1, height=.1, fontsize=8, font=helvitica];
 *     edge [fontsize=7, font=helvitica];
 *     cvmx_root [label="ROOT|<root>root"];
 *     cvmx_ciu2_src_pp0_ip2_pkt [label="CIU2_SRC_PPX_IP2_PKT(0)|<mii>mii|<agl>agl|<ilk>ilk"];
 *     cvmx_mix0_isr [label="MIXX_ISR(0)|<odblovf>odblovf|<idblovf>idblovf|<data_drp>data_drp|<irun>irun|<orun>orun"];
 *     cvmx_ciu2_src_pp0_ip2_pkt:mii:e -> cvmx_mix0_isr [label="mii"];
 *     cvmx_agl_gmx_bad_reg [label="AGL_GMX_BAD_REG|<ovrflw>ovrflw|<txpop>txpop|<txpsh>txpsh|<ovrflw1>ovrflw1|<txpop1>txpop1|<txpsh1>txpsh1|<out_ovr>out_ovr|<loststat>loststat"];
 *     cvmx_ciu2_src_pp0_ip2_pkt:agl:e -> cvmx_agl_gmx_bad_reg [label="agl"];
 *     cvmx_agl_gmx_rx0_int_reg [label="AGL_GMX_RXX_INT_REG(0)|<skperr>skperr|<ovrerr>ovrerr"];
 *     cvmx_ciu2_src_pp0_ip2_pkt:agl:e -> cvmx_agl_gmx_rx0_int_reg [label="agl"];
 *     cvmx_agl_gmx_tx_int_reg [label="AGL_GMX_TX_INT_REG|<pko_nxa>pko_nxa|<undflw>undflw"];
 *     cvmx_ciu2_src_pp0_ip2_pkt:agl:e -> cvmx_agl_gmx_tx_int_reg [label="agl"];
 *     cvmx_ilk_gbl_int [label="ILK_GBL_INT|<rxf_lnk0_perr>rxf_lnk0_perr|<rxf_lnk1_perr>rxf_lnk1_perr|<rxf_ctl_perr>rxf_ctl_perr|<rxf_pop_empty>rxf_pop_empty|<rxf_push_full>rxf_push_full"];
 *     cvmx_ciu2_src_pp0_ip2_pkt:ilk:e -> cvmx_ilk_gbl_int [label="ilk"];
 *     cvmx_ilk_tx0_int [label="ILK_TXX_INT(0)|<txf_err>txf_err|<bad_seq>bad_seq|<bad_pipe>bad_pipe|<stat_cnt_ovfl>stat_cnt_ovfl"];
 *     cvmx_ciu2_src_pp0_ip2_pkt:ilk:e -> cvmx_ilk_tx0_int [label="ilk"];
 *     cvmx_ilk_tx1_int [label="ILK_TXX_INT(1)|<txf_err>txf_err|<bad_seq>bad_seq|<bad_pipe>bad_pipe|<stat_cnt_ovfl>stat_cnt_ovfl"];
 *     cvmx_ciu2_src_pp0_ip2_pkt:ilk:e -> cvmx_ilk_tx1_int [label="ilk"];
 *     cvmx_ilk_rx0_int [label="ILK_RXX_INT(0)|<lane_align_fail>lane_align_fail|<crc24_err>crc24_err|<word_sync_done>word_sync_done|<lane_align_done>lane_align_done|<stat_cnt_ovfl>stat_cnt_ovfl|<lane_bad_word>lane_bad_word|<pkt_drop_rxf>pkt_drop_rxf|<pkt_drop_rid>pkt_drop_rid|<pkt_drop_sop>pkt_drop_sop"];
 *     cvmx_ciu2_src_pp0_ip2_pkt:ilk:e -> cvmx_ilk_rx0_int [label="ilk"];
 *     cvmx_ilk_rx1_int [label="ILK_RXX_INT(1)|<lane_align_fail>lane_align_fail|<crc24_err>crc24_err|<word_sync_done>word_sync_done|<lane_align_done>lane_align_done|<stat_cnt_ovfl>stat_cnt_ovfl|<lane_bad_word>lane_bad_word|<pkt_drop_rxf>pkt_drop_rxf|<pkt_drop_rid>pkt_drop_rid|<pkt_drop_sop>pkt_drop_sop"];
 *     cvmx_ciu2_src_pp0_ip2_pkt:ilk:e -> cvmx_ilk_rx1_int [label="ilk"];
 *     cvmx_ilk_rx_lne0_int [label="ILK_RX_LNEX_INT(0)|<serdes_lock_loss>serdes_lock_loss|<bdry_sync_loss>bdry_sync_loss|<crc32_err>crc32_err|<ukwn_cntl_word>ukwn_cntl_word|<scrm_sync_loss>scrm_sync_loss|<dskew_fifo_ovfl>dskew_fifo_ovfl|<stat_msg>stat_msg|<stat_cnt_ovfl>stat_cnt_ovfl|<bad_64b67b>bad_64b67b"];
 *     cvmx_ciu2_src_pp0_ip2_pkt:ilk:e -> cvmx_ilk_rx_lne0_int [label="ilk"];
 *     cvmx_ilk_rx_lne1_int [label="ILK_RX_LNEX_INT(1)|<serdes_lock_loss>serdes_lock_loss|<bdry_sync_loss>bdry_sync_loss|<crc32_err>crc32_err|<ukwn_cntl_word>ukwn_cntl_word|<scrm_sync_loss>scrm_sync_loss|<dskew_fifo_ovfl>dskew_fifo_ovfl|<stat_msg>stat_msg|<stat_cnt_ovfl>stat_cnt_ovfl|<bad_64b67b>bad_64b67b"];
 *     cvmx_ciu2_src_pp0_ip2_pkt:ilk:e -> cvmx_ilk_rx_lne1_int [label="ilk"];
 *     cvmx_ilk_rx_lne2_int [label="ILK_RX_LNEX_INT(2)|<serdes_lock_loss>serdes_lock_loss|<bdry_sync_loss>bdry_sync_loss|<crc32_err>crc32_err|<ukwn_cntl_word>ukwn_cntl_word|<scrm_sync_loss>scrm_sync_loss|<dskew_fifo_ovfl>dskew_fifo_ovfl|<stat_msg>stat_msg|<stat_cnt_ovfl>stat_cnt_ovfl|<bad_64b67b>bad_64b67b"];
 *     cvmx_ciu2_src_pp0_ip2_pkt:ilk:e -> cvmx_ilk_rx_lne2_int [label="ilk"];
 *     cvmx_ilk_rx_lne3_int [label="ILK_RX_LNEX_INT(3)|<serdes_lock_loss>serdes_lock_loss|<bdry_sync_loss>bdry_sync_loss|<crc32_err>crc32_err|<ukwn_cntl_word>ukwn_cntl_word|<scrm_sync_loss>scrm_sync_loss|<dskew_fifo_ovfl>dskew_fifo_ovfl|<stat_msg>stat_msg|<stat_cnt_ovfl>stat_cnt_ovfl|<bad_64b67b>bad_64b67b"];
 *     cvmx_ciu2_src_pp0_ip2_pkt:ilk:e -> cvmx_ilk_rx_lne3_int [label="ilk"];
 *     cvmx_ilk_rx_lne4_int [label="ILK_RX_LNEX_INT(4)|<serdes_lock_loss>serdes_lock_loss|<bdry_sync_loss>bdry_sync_loss|<crc32_err>crc32_err|<ukwn_cntl_word>ukwn_cntl_word|<scrm_sync_loss>scrm_sync_loss|<dskew_fifo_ovfl>dskew_fifo_ovfl|<stat_msg>stat_msg|<stat_cnt_ovfl>stat_cnt_ovfl|<bad_64b67b>bad_64b67b"];
 *     cvmx_ciu2_src_pp0_ip2_pkt:ilk:e -> cvmx_ilk_rx_lne4_int [label="ilk"];
 *     cvmx_ilk_rx_lne5_int [label="ILK_RX_LNEX_INT(5)|<serdes_lock_loss>serdes_lock_loss|<bdry_sync_loss>bdry_sync_loss|<crc32_err>crc32_err|<ukwn_cntl_word>ukwn_cntl_word|<scrm_sync_loss>scrm_sync_loss|<dskew_fifo_ovfl>dskew_fifo_ovfl|<stat_msg>stat_msg|<stat_cnt_ovfl>stat_cnt_ovfl|<bad_64b67b>bad_64b67b"];
 *     cvmx_ciu2_src_pp0_ip2_pkt:ilk:e -> cvmx_ilk_rx_lne5_int [label="ilk"];
 *     cvmx_ilk_rx_lne6_int [label="ILK_RX_LNEX_INT(6)|<serdes_lock_loss>serdes_lock_loss|<bdry_sync_loss>bdry_sync_loss|<crc32_err>crc32_err|<ukwn_cntl_word>ukwn_cntl_word|<scrm_sync_loss>scrm_sync_loss|<dskew_fifo_ovfl>dskew_fifo_ovfl|<stat_msg>stat_msg|<stat_cnt_ovfl>stat_cnt_ovfl|<bad_64b67b>bad_64b67b"];
 *     cvmx_ciu2_src_pp0_ip2_pkt:ilk:e -> cvmx_ilk_rx_lne6_int [label="ilk"];
 *     cvmx_ilk_rx_lne7_int [label="ILK_RX_LNEX_INT(7)|<serdes_lock_loss>serdes_lock_loss|<bdry_sync_loss>bdry_sync_loss|<crc32_err>crc32_err|<ukwn_cntl_word>ukwn_cntl_word|<scrm_sync_loss>scrm_sync_loss|<dskew_fifo_ovfl>dskew_fifo_ovfl|<stat_msg>stat_msg|<stat_cnt_ovfl>stat_cnt_ovfl|<bad_64b67b>bad_64b67b"];
 *     cvmx_ciu2_src_pp0_ip2_pkt:ilk:e -> cvmx_ilk_rx_lne7_int [label="ilk"];
 *     cvmx_agl_gmx_bad_reg -> cvmx_agl_gmx_rx0_int_reg [style=invis];
 *     cvmx_agl_gmx_rx0_int_reg -> cvmx_agl_gmx_tx_int_reg [style=invis];
 *     cvmx_ilk_gbl_int -> cvmx_ilk_tx0_int [style=invis];
 *     cvmx_ilk_tx0_int -> cvmx_ilk_tx1_int [style=invis];
 *     cvmx_ilk_tx1_int -> cvmx_ilk_rx0_int [style=invis];
 *     cvmx_ilk_rx0_int -> cvmx_ilk_rx1_int [style=invis];
 *     cvmx_ilk_rx1_int -> cvmx_ilk_rx_lne0_int [style=invis];
 *     cvmx_ilk_rx_lne0_int -> cvmx_ilk_rx_lne1_int [style=invis];
 *     cvmx_ilk_rx_lne1_int -> cvmx_ilk_rx_lne2_int [style=invis];
 *     cvmx_ilk_rx_lne2_int -> cvmx_ilk_rx_lne3_int [style=invis];
 *     cvmx_ilk_rx_lne3_int -> cvmx_ilk_rx_lne4_int [style=invis];
 *     cvmx_ilk_rx_lne4_int -> cvmx_ilk_rx_lne5_int [style=invis];
 *     cvmx_ilk_rx_lne5_int -> cvmx_ilk_rx_lne6_int [style=invis];
 *     cvmx_ilk_rx_lne6_int -> cvmx_ilk_rx_lne7_int [style=invis];
 *     cvmx_root:root:e -> cvmx_ciu2_src_pp0_ip2_pkt [label="root"];
 *     cvmx_ciu2_src_pp0_ip2_rml [label="CIU2_SRC_PPX_IP2_RML(0)|<l2c>l2c|<fpa>fpa|<zip>zip|<ipd>ipd|<rad>rad|<sso>sso|<sli>sli|<key>key|<pip>pip|<dfa>dfa|<pko>pko|<dpi>dpi"];
 *     cvmx_l2c_int_reg [label="L2C_INT_REG|<holerd>holerd|<holewr>holewr|<vrtwr>vrtwr|<vrtidrng>vrtidrng|<vrtadrng>vrtadrng|<vrtpe>vrtpe|<bigwr>bigwr|<bigrd>bigrd|<tad1>tad1|<tad0>tad0|<tad3>tad3|<tad2>tad2"];
 *     cvmx_l2c_tad1_int [label="L2C_TADX_INT(1)|<l2dsbe>l2dsbe|<l2ddbe>l2ddbe|<tagsbe>tagsbe|<tagdbe>tagdbe|<vbfsbe>vbfsbe|<vbfdbe>vbfdbe|<noway>noway|<rddislmc>rddislmc|<wrdislmc>wrdislmc"];
 *     cvmx_l2c_int_reg:tad1:e -> cvmx_l2c_tad1_int [label="tad1"];
 *     cvmx_l2c_err_tdt1 [label="L2C_ERR_TDTX(1)|<vsbe>vsbe|<vdbe>vdbe|<sbe>sbe|<dbe>dbe"];
 *     cvmx_l2c_int_reg:tad1:e -> cvmx_l2c_err_tdt1 [label="tad1"];
 *     cvmx_l2c_err_ttg1 [label="L2C_ERR_TTGX(1)|<noway>noway|<sbe>sbe|<dbe>dbe"];
 *     cvmx_l2c_int_reg:tad1:e -> cvmx_l2c_err_ttg1 [label="tad1"];
 *     cvmx_l2c_tad0_int [label="L2C_TADX_INT(0)|<l2dsbe>l2dsbe|<l2ddbe>l2ddbe|<tagsbe>tagsbe|<tagdbe>tagdbe|<vbfsbe>vbfsbe|<vbfdbe>vbfdbe|<noway>noway|<rddislmc>rddislmc|<wrdislmc>wrdislmc"];
 *     cvmx_l2c_int_reg:tad0:e -> cvmx_l2c_tad0_int [label="tad0"];
 *     cvmx_l2c_err_tdt0 [label="L2C_ERR_TDTX(0)|<vsbe>vsbe|<vdbe>vdbe|<sbe>sbe|<dbe>dbe"];
 *     cvmx_l2c_int_reg:tad0:e -> cvmx_l2c_err_tdt0 [label="tad0"];
 *     cvmx_l2c_err_ttg0 [label="L2C_ERR_TTGX(0)|<noway>noway|<sbe>sbe|<dbe>dbe"];
 *     cvmx_l2c_int_reg:tad0:e -> cvmx_l2c_err_ttg0 [label="tad0"];
 *     cvmx_l2c_tad3_int [label="L2C_TADX_INT(3)|<l2dsbe>l2dsbe|<l2ddbe>l2ddbe|<tagsbe>tagsbe|<tagdbe>tagdbe|<vbfsbe>vbfsbe|<vbfdbe>vbfdbe|<noway>noway|<rddislmc>rddislmc|<wrdislmc>wrdislmc"];
 *     cvmx_l2c_int_reg:tad3:e -> cvmx_l2c_tad3_int [label="tad3"];
 *     cvmx_l2c_err_tdt3 [label="L2C_ERR_TDTX(3)|<vsbe>vsbe|<vdbe>vdbe|<sbe>sbe|<dbe>dbe"];
 *     cvmx_l2c_int_reg:tad3:e -> cvmx_l2c_err_tdt3 [label="tad3"];
 *     cvmx_l2c_err_ttg3 [label="L2C_ERR_TTGX(3)|<noway>noway|<sbe>sbe|<dbe>dbe"];
 *     cvmx_l2c_int_reg:tad3:e -> cvmx_l2c_err_ttg3 [label="tad3"];
 *     cvmx_l2c_tad2_int [label="L2C_TADX_INT(2)|<l2dsbe>l2dsbe|<l2ddbe>l2ddbe|<tagsbe>tagsbe|<tagdbe>tagdbe|<vbfsbe>vbfsbe|<vbfdbe>vbfdbe|<noway>noway|<rddislmc>rddislmc|<wrdislmc>wrdislmc"];
 *     cvmx_l2c_int_reg:tad2:e -> cvmx_l2c_tad2_int [label="tad2"];
 *     cvmx_l2c_err_tdt2 [label="L2C_ERR_TDTX(2)|<vsbe>vsbe|<vdbe>vdbe|<sbe>sbe|<dbe>dbe"];
 *     cvmx_l2c_int_reg:tad2:e -> cvmx_l2c_err_tdt2 [label="tad2"];
 *     cvmx_l2c_err_ttg2 [label="L2C_ERR_TTGX(2)|<noway>noway|<sbe>sbe|<dbe>dbe"];
 *     cvmx_l2c_int_reg:tad2:e -> cvmx_l2c_err_ttg2 [label="tad2"];
 *     cvmx_l2c_tad0_int -> cvmx_l2c_err_tdt0 [style=invis];
 *     cvmx_l2c_err_tdt0 -> cvmx_l2c_err_ttg0 [style=invis];
 *     cvmx_l2c_tad3_int -> cvmx_l2c_err_tdt3 [style=invis];
 *     cvmx_l2c_err_tdt3 -> cvmx_l2c_err_ttg3 [style=invis];
 *     cvmx_l2c_tad2_int -> cvmx_l2c_err_tdt2 [style=invis];
 *     cvmx_l2c_err_tdt2 -> cvmx_l2c_err_ttg2 [style=invis];
 *     cvmx_ciu2_src_pp0_ip2_rml:l2c:e -> cvmx_l2c_int_reg [label="l2c"];
 *     cvmx_fpa_int_sum [label="FPA_INT_SUM|<fed0_sbe>fed0_sbe|<fed0_dbe>fed0_dbe|<fed1_sbe>fed1_sbe|<fed1_dbe>fed1_dbe|<q0_und>q0_und|<q0_coff>q0_coff|<q0_perr>q0_perr|<q1_und>q1_und|<q1_coff>q1_coff|<q1_perr>q1_perr|<q2_und>q2_und|<q2_coff>q2_coff|<q2_perr>q2_perr|<q3_und>q3_und|<q3_coff>q3_coff|<q3_perr>q3_perr|<q4_und>q4_und|<q4_coff>q4_coff|<q4_perr>q4_perr|<q5_und>q5_und|<q5_coff>q5_coff|<q5_perr>q5_perr|<q6_und>q6_und|<q6_coff>q6_coff|<q6_perr>q6_perr|<q7_und>q7_und|<q7_coff>q7_coff|<q7_perr>q7_perr|<pool0th>pool0th|<pool1th>pool1th|<pool2th>pool2th|<pool3th>pool3th|<pool4th>pool4th|<pool5th>pool5th|<pool6th>pool6th|<pool7th>pool7th|<free0>free0|<free1>free1|<free2>free2|<free3>free3|<free4>free4|<free5>free5|<free6>free6|<free7>free7|<free8>free8|<q8_und>q8_und|<q8_coff>q8_coff|<q8_perr>q8_perr|<pool8th>pool8th|<paddr_e>paddr_e"];
 *     cvmx_ciu2_src_pp0_ip2_rml:fpa:e -> cvmx_fpa_int_sum [label="fpa"];
 *     cvmx_zip_error [label="ZIP_ERROR|<doorbell>doorbell"];
 *     cvmx_ciu2_src_pp0_ip2_rml:zip:e -> cvmx_zip_error [label="zip"];
 *     cvmx_ipd_int_sum [label="IPD_INT_SUM|<prc_par0>prc_par0|<prc_par1>prc_par1|<prc_par2>prc_par2|<prc_par3>prc_par3|<bp_sub>bp_sub|<dc_ovr>dc_ovr|<cc_ovr>cc_ovr|<c_coll>c_coll|<d_coll>d_coll|<bc_ovr>bc_ovr|<sop>sop|<eop>eop|<dat>dat|<pw0_sbe>pw0_sbe|<pw0_dbe>pw0_dbe|<pw1_sbe>pw1_sbe|<pw1_dbe>pw1_dbe|<pw2_sbe>pw2_sbe|<pw2_dbe>pw2_dbe|<pw3_sbe>pw3_sbe|<pw3_dbe>pw3_dbe"];
 *     cvmx_ciu2_src_pp0_ip2_rml:ipd:e -> cvmx_ipd_int_sum [label="ipd"];
 *     cvmx_rad_reg_error [label="RAD_REG_ERROR|<doorbell>doorbell"];
 *     cvmx_ciu2_src_pp0_ip2_rml:rad:e -> cvmx_rad_reg_error [label="rad"];
 *     cvmx_sso_err [label="SSO_ERR|<iop>iop|<fidx_dbe>fidx_dbe|<idx_sbe>idx_sbe|<pnd_dbe0>pnd_dbe0|<oth_sbe1>oth_sbe1|<oth_dbe1>oth_dbe1|<oth_sbe0>oth_sbe0|<oth_dbe0>oth_dbe0|<pnd_sbe1>pnd_sbe1|<pnd_dbe1>pnd_dbe1|<pnd_sbe0>pnd_sbe0|<fpe>fpe|<awe>awe|<bfp>bfp|<idx_dbe>idx_dbe|<fidx_sbe>fidx_sbe"];
 *     cvmx_ciu2_src_pp0_ip2_rml:sso:e -> cvmx_sso_err [label="sso"];
 *     cvmx_sli_int_sum [label="PEXP_SLI_INT_SUM|<rml_to>rml_to|<reserved_1_1>reserved_1_1|<bar0_to>bar0_to|<iob2big>iob2big|<reserved_6_7>reserved_6_7|<m0_up_b0>m0_up_b0|<m0_up_wi>m0_up_wi|<m0_un_b0>m0_un_b0|<m0_un_wi>m0_un_wi|<m1_up_b0>m1_up_b0|<m1_up_wi>m1_up_wi|<m1_un_b0>m1_un_b0|<m1_un_wi>m1_un_wi|<pidbof>pidbof|<psldbof>psldbof|<pout_err>pout_err|<pgl_err>pgl_err|<pdi_err>pdi_err|<pop_err>pop_err|<pins_err>pins_err|<sprt0_err>sprt0_err|<sprt1_err>sprt1_err|<ill_pad>ill_pad|<pipe_err>pipe_err"];
 *     cvmx_ciu2_src_pp0_ip2_rml:sli:e -> cvmx_sli_int_sum [label="sli"];
 *     cvmx_key_int_sum [label="KEY_INT_SUM|<ked0_sbe>ked0_sbe|<ked0_dbe>ked0_dbe|<ked1_sbe>ked1_sbe|<ked1_dbe>ked1_dbe"];
 *     cvmx_ciu2_src_pp0_ip2_rml:key:e -> cvmx_key_int_sum [label="key"];
 *     cvmx_pip_int_reg [label="PIP_INT_REG|<prtnxa>prtnxa|<badtag>badtag|<skprunt>skprunt|<todoovr>todoovr|<feperr>feperr|<beperr>beperr|<punyerr>punyerr"];
 *     cvmx_ciu2_src_pp0_ip2_rml:pip:e -> cvmx_pip_int_reg [label="pip"];
 *     cvmx_dfa_error [label="DFA_ERROR|<dblovf>dblovf|<dc0perr>dc0perr|<dc1perr>dc1perr|<dc2perr>dc2perr|<dlc0_ovferr>dlc0_ovferr|<dlc1_ovferr>dlc1_ovferr|<dfanxm>dfanxm|<replerr>replerr"];
 *     cvmx_ciu2_src_pp0_ip2_rml:dfa:e -> cvmx_dfa_error [label="dfa"];
 *     cvmx_pko_reg_error [label="PKO_REG_ERROR|<parity>parity|<doorbell>doorbell|<currzero>currzero|<loopback>loopback"];
 *     cvmx_ciu2_src_pp0_ip2_rml:pko:e -> cvmx_pko_reg_error [label="pko"];
 *     cvmx_dpi_int_reg [label="DPI_INT_REG|<nderr>nderr|<nfovr>nfovr|<dmadbo>dmadbo|<req_badadr>req_badadr|<req_badlen>req_badlen|<req_ovrflw>req_ovrflw|<req_undflw>req_undflw|<req_anull>req_anull|<req_inull>req_inull|<req_badfil>req_badfil|<sprt0_rst>sprt0_rst|<sprt1_rst>sprt1_rst"];
 *     cvmx_ciu2_src_pp0_ip2_rml:dpi:e -> cvmx_dpi_int_reg [label="dpi"];
 *     cvmx_dpi_pkt_err_rsp [label="DPI_PKT_ERR_RSP|<pkterr>pkterr"];
 *     cvmx_ciu2_src_pp0_ip2_rml:dpi:e -> cvmx_dpi_pkt_err_rsp [label="dpi"];
 *     cvmx_dpi_req_err_rsp [label="DPI_REQ_ERR_RSP|<qerr>qerr"];
 *     cvmx_ciu2_src_pp0_ip2_rml:dpi:e -> cvmx_dpi_req_err_rsp [label="dpi"];
 *     cvmx_dpi_req_err_rst [label="DPI_REQ_ERR_RST|<qerr>qerr"];
 *     cvmx_ciu2_src_pp0_ip2_rml:dpi:e -> cvmx_dpi_req_err_rst [label="dpi"];
 *     cvmx_dpi_int_reg -> cvmx_dpi_pkt_err_rsp [style=invis];
 *     cvmx_dpi_pkt_err_rsp -> cvmx_dpi_req_err_rsp [style=invis];
 *     cvmx_dpi_req_err_rsp -> cvmx_dpi_req_err_rst [style=invis];
 *     cvmx_root:root:e -> cvmx_ciu2_src_pp0_ip2_rml [label="root"];
 *     cvmx_ciu2_src_pp0_ip2_mio [label="CIU2_SRC_PPX_IP2_MIO(0)|<rst>rst|<nand>nand|<mio>mio"];
 *     cvmx_mio_rst_int [label="MIO_RST_INT|<rst_link0>rst_link0|<rst_link1>rst_link1|<perst0>perst0|<perst1>perst1"];
 *     cvmx_ciu2_src_pp0_ip2_mio:rst:e -> cvmx_mio_rst_int [label="rst"];
 *     cvmx_ndf_int [label="NDF_INT|<wdog>wdog|<sm_bad>sm_bad|<ecc_1bit>ecc_1bit|<ecc_mult>ecc_mult|<ovrf>ovrf"];
 *     cvmx_ciu2_src_pp0_ip2_mio:nand:e -> cvmx_ndf_int [label="nand"];
 *     cvmx_mio_boot_err [label="MIO_BOOT_ERR|<adr_err>adr_err|<wait_err>wait_err"];
 *     cvmx_ciu2_src_pp0_ip2_mio:mio:e -> cvmx_mio_boot_err [label="mio"];
 *     cvmx_root:root:e -> cvmx_ciu2_src_pp0_ip2_mio [label="root"];
 *     cvmx_ciu2_sum_pp0_ip2 [label="CIU2_SUM_PPX_IP2(0)|<mem>mem|<pkt>pkt"];
 *     cvmx_lmc0_int [label="LMCX_INT(0)|<sec_err>sec_err|<nxm_wr_err>nxm_wr_err|<ded_err>ded_err"];
 *     cvmx_ciu2_sum_pp0_ip2:mem:e -> cvmx_lmc0_int [label="mem"];
 *     cvmx_lmc1_int [label="LMCX_INT(1)|<sec_err>sec_err|<nxm_wr_err>nxm_wr_err|<ded_err>ded_err"];
 *     cvmx_ciu2_sum_pp0_ip2:mem:e -> cvmx_lmc1_int [label="mem"];
 *     cvmx_lmc2_int [label="LMCX_INT(2)|<sec_err>sec_err|<nxm_wr_err>nxm_wr_err|<ded_err>ded_err"];
 *     cvmx_ciu2_sum_pp0_ip2:mem:e -> cvmx_lmc2_int [label="mem"];
 *     cvmx_lmc3_int [label="LMCX_INT(3)|<sec_err>sec_err|<nxm_wr_err>nxm_wr_err|<ded_err>ded_err"];
 *     cvmx_ciu2_sum_pp0_ip2:mem:e -> cvmx_lmc3_int [label="mem"];
 *     cvmx_gmx0_rx0_int_reg [label="GMXX_RXX_INT_REG(0,0)|<carext>carext|<skperr>skperr|<ovrerr>ovrerr|<loc_fault>loc_fault|<rem_fault>rem_fault|<bad_seq>bad_seq|<bad_term>bad_term|<unsop>unsop|<uneop>uneop|<undat>undat|<hg2fld>hg2fld|<hg2cc>hg2cc"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_gmx0_rx0_int_reg [label="pkt"];
 *     cvmx_gmx0_rx1_int_reg [label="GMXX_RXX_INT_REG(1,0)|<carext>carext|<skperr>skperr|<ovrerr>ovrerr|<loc_fault>loc_fault|<rem_fault>rem_fault|<bad_seq>bad_seq|<bad_term>bad_term|<unsop>unsop|<uneop>uneop|<undat>undat|<hg2fld>hg2fld|<hg2cc>hg2cc"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_gmx0_rx1_int_reg [label="pkt"];
 *     cvmx_gmx0_rx2_int_reg [label="GMXX_RXX_INT_REG(2,0)|<carext>carext|<skperr>skperr|<ovrerr>ovrerr|<loc_fault>loc_fault|<rem_fault>rem_fault|<bad_seq>bad_seq|<bad_term>bad_term|<unsop>unsop|<uneop>uneop|<undat>undat|<hg2fld>hg2fld|<hg2cc>hg2cc"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_gmx0_rx2_int_reg [label="pkt"];
 *     cvmx_gmx0_rx3_int_reg [label="GMXX_RXX_INT_REG(3,0)|<carext>carext|<skperr>skperr|<ovrerr>ovrerr|<loc_fault>loc_fault|<rem_fault>rem_fault|<bad_seq>bad_seq|<bad_term>bad_term|<unsop>unsop|<uneop>uneop|<undat>undat|<hg2fld>hg2fld|<hg2cc>hg2cc"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_gmx0_rx3_int_reg [label="pkt"];
 *     cvmx_gmx1_rx0_int_reg [label="GMXX_RXX_INT_REG(0,1)|<carext>carext|<skperr>skperr|<ovrerr>ovrerr|<loc_fault>loc_fault|<rem_fault>rem_fault|<bad_seq>bad_seq|<bad_term>bad_term|<unsop>unsop|<uneop>uneop|<undat>undat|<hg2fld>hg2fld|<hg2cc>hg2cc"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_gmx1_rx0_int_reg [label="pkt"];
 *     cvmx_gmx2_rx0_int_reg [label="GMXX_RXX_INT_REG(0,2)|<carext>carext|<skperr>skperr|<ovrerr>ovrerr|<loc_fault>loc_fault|<rem_fault>rem_fault|<bad_seq>bad_seq|<bad_term>bad_term|<unsop>unsop|<uneop>uneop|<undat>undat|<hg2fld>hg2fld|<hg2cc>hg2cc"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_gmx2_rx0_int_reg [label="pkt"];
 *     cvmx_gmx2_rx1_int_reg [label="GMXX_RXX_INT_REG(1,2)|<carext>carext|<skperr>skperr|<ovrerr>ovrerr|<loc_fault>loc_fault|<rem_fault>rem_fault|<bad_seq>bad_seq|<bad_term>bad_term|<unsop>unsop|<uneop>uneop|<undat>undat|<hg2fld>hg2fld|<hg2cc>hg2cc"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_gmx2_rx1_int_reg [label="pkt"];
 *     cvmx_gmx2_rx2_int_reg [label="GMXX_RXX_INT_REG(2,2)|<carext>carext|<skperr>skperr|<ovrerr>ovrerr|<loc_fault>loc_fault|<rem_fault>rem_fault|<bad_seq>bad_seq|<bad_term>bad_term|<unsop>unsop|<uneop>uneop|<undat>undat|<hg2fld>hg2fld|<hg2cc>hg2cc"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_gmx2_rx2_int_reg [label="pkt"];
 *     cvmx_gmx2_rx3_int_reg [label="GMXX_RXX_INT_REG(3,2)|<carext>carext|<skperr>skperr|<ovrerr>ovrerr|<loc_fault>loc_fault|<rem_fault>rem_fault|<bad_seq>bad_seq|<bad_term>bad_term|<unsop>unsop|<uneop>uneop|<undat>undat|<hg2fld>hg2fld|<hg2cc>hg2cc"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_gmx2_rx3_int_reg [label="pkt"];
 *     cvmx_gmx3_rx0_int_reg [label="GMXX_RXX_INT_REG(0,3)|<carext>carext|<skperr>skperr|<ovrerr>ovrerr|<loc_fault>loc_fault|<rem_fault>rem_fault|<bad_seq>bad_seq|<bad_term>bad_term|<unsop>unsop|<uneop>uneop|<undat>undat|<hg2fld>hg2fld|<hg2cc>hg2cc"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_gmx3_rx0_int_reg [label="pkt"];
 *     cvmx_gmx3_rx1_int_reg [label="GMXX_RXX_INT_REG(1,3)|<carext>carext|<skperr>skperr|<ovrerr>ovrerr|<loc_fault>loc_fault|<rem_fault>rem_fault|<bad_seq>bad_seq|<bad_term>bad_term|<unsop>unsop|<uneop>uneop|<undat>undat|<hg2fld>hg2fld|<hg2cc>hg2cc"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_gmx3_rx1_int_reg [label="pkt"];
 *     cvmx_gmx3_rx2_int_reg [label="GMXX_RXX_INT_REG(2,3)|<carext>carext|<skperr>skperr|<ovrerr>ovrerr|<loc_fault>loc_fault|<rem_fault>rem_fault|<bad_seq>bad_seq|<bad_term>bad_term|<unsop>unsop|<uneop>uneop|<undat>undat|<hg2fld>hg2fld|<hg2cc>hg2cc"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_gmx3_rx2_int_reg [label="pkt"];
 *     cvmx_gmx3_rx3_int_reg [label="GMXX_RXX_INT_REG(3,3)|<carext>carext|<skperr>skperr|<ovrerr>ovrerr|<loc_fault>loc_fault|<rem_fault>rem_fault|<bad_seq>bad_seq|<bad_term>bad_term|<unsop>unsop|<uneop>uneop|<undat>undat|<hg2fld>hg2fld|<hg2cc>hg2cc"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_gmx3_rx3_int_reg [label="pkt"];
 *     cvmx_gmx4_rx0_int_reg [label="GMXX_RXX_INT_REG(0,4)|<carext>carext|<skperr>skperr|<ovrerr>ovrerr|<loc_fault>loc_fault|<rem_fault>rem_fault|<bad_seq>bad_seq|<bad_term>bad_term|<unsop>unsop|<uneop>uneop|<undat>undat|<hg2fld>hg2fld|<hg2cc>hg2cc"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_gmx4_rx0_int_reg [label="pkt"];
 *     cvmx_gmx4_rx1_int_reg [label="GMXX_RXX_INT_REG(1,4)|<carext>carext|<skperr>skperr|<ovrerr>ovrerr|<loc_fault>loc_fault|<rem_fault>rem_fault|<bad_seq>bad_seq|<bad_term>bad_term|<unsop>unsop|<uneop>uneop|<undat>undat|<hg2fld>hg2fld|<hg2cc>hg2cc"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_gmx4_rx1_int_reg [label="pkt"];
 *     cvmx_gmx4_rx2_int_reg [label="GMXX_RXX_INT_REG(2,4)|<carext>carext|<skperr>skperr|<ovrerr>ovrerr|<loc_fault>loc_fault|<rem_fault>rem_fault|<bad_seq>bad_seq|<bad_term>bad_term|<unsop>unsop|<uneop>uneop|<undat>undat|<hg2fld>hg2fld|<hg2cc>hg2cc"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_gmx4_rx2_int_reg [label="pkt"];
 *     cvmx_gmx4_rx3_int_reg [label="GMXX_RXX_INT_REG(3,4)|<carext>carext|<skperr>skperr|<ovrerr>ovrerr|<loc_fault>loc_fault|<rem_fault>rem_fault|<bad_seq>bad_seq|<bad_term>bad_term|<unsop>unsop|<uneop>uneop|<undat>undat|<hg2fld>hg2fld|<hg2cc>hg2cc"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_gmx4_rx3_int_reg [label="pkt"];
 *     cvmx_gmx0_tx_int_reg [label="GMXX_TX_INT_REG(0)|<pko_nxa>pko_nxa|<undflw>undflw|<ptp_lost>ptp_lost"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_gmx0_tx_int_reg [label="pkt"];
 *     cvmx_gmx1_tx_int_reg [label="GMXX_TX_INT_REG(1)|<pko_nxa>pko_nxa|<undflw>undflw|<ptp_lost>ptp_lost"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_gmx1_tx_int_reg [label="pkt"];
 *     cvmx_gmx2_tx_int_reg [label="GMXX_TX_INT_REG(2)|<pko_nxa>pko_nxa|<undflw>undflw|<ptp_lost>ptp_lost"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_gmx2_tx_int_reg [label="pkt"];
 *     cvmx_gmx3_tx_int_reg [label="GMXX_TX_INT_REG(3)|<pko_nxa>pko_nxa|<undflw>undflw|<ptp_lost>ptp_lost"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_gmx3_tx_int_reg [label="pkt"];
 *     cvmx_gmx4_tx_int_reg [label="GMXX_TX_INT_REG(4)|<pko_nxa>pko_nxa|<undflw>undflw|<ptp_lost>ptp_lost"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_gmx4_tx_int_reg [label="pkt"];
 *     cvmx_pcs0_int0_reg [label="PCSX_INTX_REG(0,0)|<an_err>an_err|<txfifu>txfifu|<txfifo>txfifo|<txbad>txbad|<rxbad>rxbad|<rxlock>rxlock|<an_bad>an_bad|<sync_bad>sync_bad|<dbg_sync>dbg_sync"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_pcs0_int0_reg [label="pkt"];
 *     cvmx_pcs0_int1_reg [label="PCSX_INTX_REG(1,0)|<an_err>an_err|<txfifu>txfifu|<txfifo>txfifo|<txbad>txbad|<rxbad>rxbad|<rxlock>rxlock|<an_bad>an_bad|<sync_bad>sync_bad|<dbg_sync>dbg_sync"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_pcs0_int1_reg [label="pkt"];
 *     cvmx_pcs0_int2_reg [label="PCSX_INTX_REG(2,0)|<an_err>an_err|<txfifu>txfifu|<txfifo>txfifo|<txbad>txbad|<rxbad>rxbad|<rxlock>rxlock|<an_bad>an_bad|<sync_bad>sync_bad|<dbg_sync>dbg_sync"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_pcs0_int2_reg [label="pkt"];
 *     cvmx_pcs0_int3_reg [label="PCSX_INTX_REG(3,0)|<an_err>an_err|<txfifu>txfifu|<txfifo>txfifo|<txbad>txbad|<rxbad>rxbad|<rxlock>rxlock|<an_bad>an_bad|<sync_bad>sync_bad|<dbg_sync>dbg_sync"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_pcs0_int3_reg [label="pkt"];
 *     cvmx_pcs1_int0_reg [label="PCSX_INTX_REG(0,1)|<an_err>an_err|<txfifu>txfifu|<txfifo>txfifo|<txbad>txbad|<rxbad>rxbad|<rxlock>rxlock|<an_bad>an_bad|<sync_bad>sync_bad|<dbg_sync>dbg_sync"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_pcs1_int0_reg [label="pkt"];
 *     cvmx_pcs2_int0_reg [label="PCSX_INTX_REG(0,2)|<an_err>an_err|<txfifu>txfifu|<txfifo>txfifo|<txbad>txbad|<rxbad>rxbad|<rxlock>rxlock|<an_bad>an_bad|<sync_bad>sync_bad|<dbg_sync>dbg_sync"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_pcs2_int0_reg [label="pkt"];
 *     cvmx_pcs2_int1_reg [label="PCSX_INTX_REG(1,2)|<an_err>an_err|<txfifu>txfifu|<txfifo>txfifo|<txbad>txbad|<rxbad>rxbad|<rxlock>rxlock|<an_bad>an_bad|<sync_bad>sync_bad|<dbg_sync>dbg_sync"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_pcs2_int1_reg [label="pkt"];
 *     cvmx_pcs2_int2_reg [label="PCSX_INTX_REG(2,2)|<an_err>an_err|<txfifu>txfifu|<txfifo>txfifo|<txbad>txbad|<rxbad>rxbad|<rxlock>rxlock|<an_bad>an_bad|<sync_bad>sync_bad|<dbg_sync>dbg_sync"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_pcs2_int2_reg [label="pkt"];
 *     cvmx_pcs2_int3_reg [label="PCSX_INTX_REG(3,2)|<an_err>an_err|<txfifu>txfifu|<txfifo>txfifo|<txbad>txbad|<rxbad>rxbad|<rxlock>rxlock|<an_bad>an_bad|<sync_bad>sync_bad|<dbg_sync>dbg_sync"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_pcs2_int3_reg [label="pkt"];
 *     cvmx_pcs3_int0_reg [label="PCSX_INTX_REG(0,3)|<an_err>an_err|<txfifu>txfifu|<txfifo>txfifo|<txbad>txbad|<rxbad>rxbad|<rxlock>rxlock|<an_bad>an_bad|<sync_bad>sync_bad|<dbg_sync>dbg_sync"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_pcs3_int0_reg [label="pkt"];
 *     cvmx_pcs3_int1_reg [label="PCSX_INTX_REG(1,3)|<an_err>an_err|<txfifu>txfifu|<txfifo>txfifo|<txbad>txbad|<rxbad>rxbad|<rxlock>rxlock|<an_bad>an_bad|<sync_bad>sync_bad|<dbg_sync>dbg_sync"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_pcs3_int1_reg [label="pkt"];
 *     cvmx_pcs3_int2_reg [label="PCSX_INTX_REG(2,3)|<an_err>an_err|<txfifu>txfifu|<txfifo>txfifo|<txbad>txbad|<rxbad>rxbad|<rxlock>rxlock|<an_bad>an_bad|<sync_bad>sync_bad|<dbg_sync>dbg_sync"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_pcs3_int2_reg [label="pkt"];
 *     cvmx_pcs3_int3_reg [label="PCSX_INTX_REG(3,3)|<an_err>an_err|<txfifu>txfifu|<txfifo>txfifo|<txbad>txbad|<rxbad>rxbad|<rxlock>rxlock|<an_bad>an_bad|<sync_bad>sync_bad|<dbg_sync>dbg_sync"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_pcs3_int3_reg [label="pkt"];
 *     cvmx_pcs4_int0_reg [label="PCSX_INTX_REG(0,4)|<an_err>an_err|<txfifu>txfifu|<txfifo>txfifo|<txbad>txbad|<rxbad>rxbad|<rxlock>rxlock|<an_bad>an_bad|<sync_bad>sync_bad|<dbg_sync>dbg_sync"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_pcs4_int0_reg [label="pkt"];
 *     cvmx_pcs4_int1_reg [label="PCSX_INTX_REG(1,4)|<an_err>an_err|<txfifu>txfifu|<txfifo>txfifo|<txbad>txbad|<rxbad>rxbad|<rxlock>rxlock|<an_bad>an_bad|<sync_bad>sync_bad|<dbg_sync>dbg_sync"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_pcs4_int1_reg [label="pkt"];
 *     cvmx_pcs4_int2_reg [label="PCSX_INTX_REG(2,4)|<an_err>an_err|<txfifu>txfifu|<txfifo>txfifo|<txbad>txbad|<rxbad>rxbad|<rxlock>rxlock|<an_bad>an_bad|<sync_bad>sync_bad|<dbg_sync>dbg_sync"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_pcs4_int2_reg [label="pkt"];
 *     cvmx_pcs4_int3_reg [label="PCSX_INTX_REG(3,4)|<an_err>an_err|<txfifu>txfifu|<txfifo>txfifo|<txbad>txbad|<rxbad>rxbad|<rxlock>rxlock|<an_bad>an_bad|<sync_bad>sync_bad|<dbg_sync>dbg_sync"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_pcs4_int3_reg [label="pkt"];
 *     cvmx_pcsx0_int_reg [label="PCSXX_INT_REG(0)|<txflt>txflt|<rxbad>rxbad|<rxsynbad>rxsynbad|<bitlckls>bitlckls|<synlos>synlos|<algnlos>algnlos|<dbg_sync>dbg_sync"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_pcsx0_int_reg [label="pkt"];
 *     cvmx_pcsx1_int_reg [label="PCSXX_INT_REG(1)|<txflt>txflt|<rxbad>rxbad|<rxsynbad>rxsynbad|<bitlckls>bitlckls|<synlos>synlos|<algnlos>algnlos|<dbg_sync>dbg_sync"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_pcsx1_int_reg [label="pkt"];
 *     cvmx_pcsx2_int_reg [label="PCSXX_INT_REG(2)|<txflt>txflt|<rxbad>rxbad|<rxsynbad>rxsynbad|<bitlckls>bitlckls|<synlos>synlos|<algnlos>algnlos|<dbg_sync>dbg_sync"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_pcsx2_int_reg [label="pkt"];
 *     cvmx_pcsx3_int_reg [label="PCSXX_INT_REG(3)|<txflt>txflt|<rxbad>rxbad|<rxsynbad>rxsynbad|<bitlckls>bitlckls|<synlos>synlos|<algnlos>algnlos|<dbg_sync>dbg_sync"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_pcsx3_int_reg [label="pkt"];
 *     cvmx_pcsx4_int_reg [label="PCSXX_INT_REG(4)|<txflt>txflt|<rxbad>rxbad|<rxsynbad>rxsynbad|<bitlckls>bitlckls|<synlos>synlos|<algnlos>algnlos|<dbg_sync>dbg_sync"];
 *     cvmx_ciu2_sum_pp0_ip2:pkt:e -> cvmx_pcsx4_int_reg [label="pkt"];
 *     cvmx_gmx0_rx0_int_reg -> cvmx_gmx0_rx1_int_reg [style=invis];
 *     cvmx_gmx0_rx1_int_reg -> cvmx_gmx0_rx2_int_reg [style=invis];
 *     cvmx_gmx0_rx2_int_reg -> cvmx_gmx0_rx3_int_reg [style=invis];
 *     cvmx_gmx0_rx3_int_reg -> cvmx_gmx1_rx0_int_reg [style=invis];
 *     cvmx_gmx1_rx0_int_reg -> cvmx_gmx2_rx0_int_reg [style=invis];
 *     cvmx_gmx2_rx0_int_reg -> cvmx_gmx2_rx1_int_reg [style=invis];
 *     cvmx_gmx2_rx1_int_reg -> cvmx_gmx2_rx2_int_reg [style=invis];
 *     cvmx_gmx2_rx2_int_reg -> cvmx_gmx2_rx3_int_reg [style=invis];
 *     cvmx_gmx2_rx3_int_reg -> cvmx_gmx3_rx0_int_reg [style=invis];
 *     cvmx_gmx3_rx0_int_reg -> cvmx_gmx3_rx1_int_reg [style=invis];
 *     cvmx_gmx3_rx1_int_reg -> cvmx_gmx3_rx2_int_reg [style=invis];
 *     cvmx_gmx3_rx2_int_reg -> cvmx_gmx3_rx3_int_reg [style=invis];
 *     cvmx_gmx3_rx3_int_reg -> cvmx_gmx4_rx0_int_reg [style=invis];
 *     cvmx_gmx4_rx0_int_reg -> cvmx_gmx4_rx1_int_reg [style=invis];
 *     cvmx_gmx4_rx1_int_reg -> cvmx_gmx4_rx2_int_reg [style=invis];
 *     cvmx_gmx4_rx2_int_reg -> cvmx_gmx4_rx3_int_reg [style=invis];
 *     cvmx_gmx4_rx3_int_reg -> cvmx_gmx0_tx_int_reg [style=invis];
 *     cvmx_gmx0_tx_int_reg -> cvmx_gmx1_tx_int_reg [style=invis];
 *     cvmx_gmx1_tx_int_reg -> cvmx_gmx2_tx_int_reg [style=invis];
 *     cvmx_gmx2_tx_int_reg -> cvmx_gmx3_tx_int_reg [style=invis];
 *     cvmx_gmx3_tx_int_reg -> cvmx_gmx4_tx_int_reg [style=invis];
 *     cvmx_gmx4_tx_int_reg -> cvmx_pcs0_int0_reg [style=invis];
 *     cvmx_pcs0_int0_reg -> cvmx_pcs0_int1_reg [style=invis];
 *     cvmx_pcs0_int1_reg -> cvmx_pcs0_int2_reg [style=invis];
 *     cvmx_pcs0_int2_reg -> cvmx_pcs0_int3_reg [style=invis];
 *     cvmx_pcs0_int3_reg -> cvmx_pcs1_int0_reg [style=invis];
 *     cvmx_pcs1_int0_reg -> cvmx_pcs2_int0_reg [style=invis];
 *     cvmx_pcs2_int0_reg -> cvmx_pcs2_int1_reg [style=invis];
 *     cvmx_pcs2_int1_reg -> cvmx_pcs2_int2_reg [style=invis];
 *     cvmx_pcs2_int2_reg -> cvmx_pcs2_int3_reg [style=invis];
 *     cvmx_pcs2_int3_reg -> cvmx_pcs3_int0_reg [style=invis];
 *     cvmx_pcs3_int0_reg -> cvmx_pcs3_int1_reg [style=invis];
 *     cvmx_pcs3_int1_reg -> cvmx_pcs3_int2_reg [style=invis];
 *     cvmx_pcs3_int2_reg -> cvmx_pcs3_int3_reg [style=invis];
 *     cvmx_pcs3_int3_reg -> cvmx_pcs4_int0_reg [style=invis];
 *     cvmx_pcs4_int0_reg -> cvmx_pcs4_int1_reg [style=invis];
 *     cvmx_pcs4_int1_reg -> cvmx_pcs4_int2_reg [style=invis];
 *     cvmx_pcs4_int2_reg -> cvmx_pcs4_int3_reg [style=invis];
 *     cvmx_pcs4_int3_reg -> cvmx_pcsx0_int_reg [style=invis];
 *     cvmx_pcsx0_int_reg -> cvmx_pcsx1_int_reg [style=invis];
 *     cvmx_pcsx1_int_reg -> cvmx_pcsx2_int_reg [style=invis];
 *     cvmx_pcsx2_int_reg -> cvmx_pcsx3_int_reg [style=invis];
 *     cvmx_pcsx3_int_reg -> cvmx_pcsx4_int_reg [style=invis];
 *     cvmx_root:root:e -> cvmx_ciu2_sum_pp0_ip2 [label="root"];
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

int cvmx_error_initialize_cn68xx(void);

int cvmx_error_initialize_cn68xx(void)
{
    cvmx_error_info_t info;
    int fail = 0;

    /* CVMX_CIU2_SRC_PPX_IP2_PKT(0) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<40 /* mii */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR MIXX_ISR(0)[ODBLOVF]: Outbound DoorBell(ODBELL) Overflow Detected\n"
        "    If SW attempts to write to the MIX_ORING2[ODBELL]\n"
        "    with a value greater than the remaining #of\n"
        "    O-Ring Buffer Entries (MIX_REMCNT[OREMCNT]), then\n"
        "    the following occurs:\n"
        "    1) The  MIX_ORING2[ODBELL] write is IGNORED\n"
        "    2) The ODBLOVF is set and the CIU2_RAW_PKT[MII]\n"
        "       bit is set if ((MIX_ISR & MIX_INTENA) != 0)).\n"
        "    If both the global interrupt mask bits (CIU2_EN_xx_yy_PKT[MII])\n"
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<40 /* mii */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR MIXX_ISR(0)[IDBLOVF]: Inbound DoorBell(IDBELL) Overflow Detected\n"
        "    If SW attempts to write to the MIX_IRING2[IDBELL]\n"
        "    with a value greater than the remaining #of\n"
        "    I-Ring Buffer Entries (MIX_REMCNT[IREMCNT]), then\n"
        "    the following occurs:\n"
        "    1) The  MIX_IRING2[IDBELL] write is IGNORED\n"
        "    2) The ODBLOVF is set and the CIU2_RAW_PKT[MII]\n"
        "       bit is set if ((MIX_ISR & MIX_INTENA) != 0)).\n"
        "    If both the global interrupt mask bits (CIU2_EN_xx_yy_PKT[MII])\n"
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<40 /* mii */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR MIXX_ISR(0)[DATA_DRP]: Data was dropped due to RX FIFO full\n"
        "    If this does occur, the DATA_DRP is set and the\n"
        "    CIU2_RAW_PKT[MII] bit is set.\n"
        "    If both the global interrupt mask bits (CIU2_EN_xx_yy_PKT[MII])\n"
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<40 /* mii */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<40 /* mii */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<32 /* agl */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<32 /* agl */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<32 /* agl */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<32 /* agl */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<32 /* agl */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<32 /* agl */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<32 /* agl */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR AGL_GMX_BAD_REG[OUT_OVR]: Outbound data FIFO overflow\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_AGL_GMX_BAD_REG;
    info.status_mask        = 0x3ull<<22 /* loststat */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_MGMT_PORT;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<32 /* agl */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR AGL_GMX_BAD_REG[LOSTSTAT]: TX Statistics data was over-written\n"
        "    In MII/RGMII, one bit per port\n"
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<32 /* agl */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<32 /* agl */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR AGL_GMX_RXX_INT_REG(0)[OVRERR]: Internal Data Aggregation Overflow\n"
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<32 /* agl */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<32 /* agl */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR AGL_GMX_TX_INT_REG[UNDFLW]: TX Underflow\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_ILK_GBL_INT */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_GBL_INT;
    info.status_mask        = 1ull<<0 /* rxf_lnk0_perr */;
    info.enable_addr        = CVMX_ILK_GBL_INT_EN;
    info.enable_mask        = 1ull<<0 /* rxf_lnk0_perr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_GBL_INT[RXF_LNK0_PERR]: RXF parity error occurred on RxLink0 packet data.  Packet will\n"
        "    be marked with error at eop\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_GBL_INT;
    info.status_mask        = 1ull<<1 /* rxf_lnk1_perr */;
    info.enable_addr        = CVMX_ILK_GBL_INT_EN;
    info.enable_mask        = 1ull<<1 /* rxf_lnk1_perr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_GBL_INT[RXF_LNK1_PERR]: RXF parity error occurred on RxLink1 packet data\n"
        "    Packet will be marked with error at eop\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_GBL_INT;
    info.status_mask        = 1ull<<2 /* rxf_ctl_perr */;
    info.enable_addr        = CVMX_ILK_GBL_INT_EN;
    info.enable_mask        = 1ull<<2 /* rxf_ctl_perr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_GBL_INT[RXF_CTL_PERR]: RXF parity error occurred on sideband control signals.  Data\n"
        "    cycle will be dropped.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_GBL_INT;
    info.status_mask        = 1ull<<3 /* rxf_pop_empty */;
    info.enable_addr        = CVMX_ILK_GBL_INT_EN;
    info.enable_mask        = 1ull<<3 /* rxf_pop_empty */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_GBL_INT[RXF_POP_EMPTY]: RXF underflow\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_GBL_INT;
    info.status_mask        = 1ull<<4 /* rxf_push_full */;
    info.enable_addr        = CVMX_ILK_GBL_INT_EN;
    info.enable_mask        = 1ull<<4 /* rxf_push_full */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_GBL_INT[RXF_PUSH_FULL]: RXF overflow\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_ILK_TXX_INT(0) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_TXX_INT(0);
    info.status_mask        = 1ull<<0 /* txf_err */;
    info.enable_addr        = CVMX_ILK_TXX_INT_EN(0);
    info.enable_mask        = 1ull<<0 /* txf_err */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_TXX_INT(0)[TXF_ERR]: TX fifo parity error occurred.  At EOP time, EOP_Format will\n"
        "    reflect the error.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_TXX_INT(0);
    info.status_mask        = 1ull<<1 /* bad_seq */;
    info.enable_addr        = CVMX_ILK_TXX_INT_EN(0);
    info.enable_mask        = 1ull<<1 /* bad_seq */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_TXX_INT(0)[BAD_SEQ]: Received sequence is not SOP followed by 0 or more data cycles\n"
        "    followed by EOP.  PKO config assigned multiple engines to the\n"
        "    same ILK Tx Link.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_TXX_INT(0);
    info.status_mask        = 1ull<<2 /* bad_pipe */;
    info.enable_addr        = CVMX_ILK_TXX_INT_EN(0);
    info.enable_mask        = 1ull<<2 /* bad_pipe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_TXX_INT(0)[BAD_PIPE]: Received a PKO port-pipe out of the range specified by\n"
        "    ILK_TXX_PIPE\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_TXX_INT(0);
    info.status_mask        = 1ull<<3 /* stat_cnt_ovfl */;
    info.enable_addr        = CVMX_ILK_TXX_INT_EN(0);
    info.enable_mask        = 1ull<<3 /* stat_cnt_ovfl */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_TXX_INT(0)[STAT_CNT_OVFL]: Statistics counter overflow\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_ILK_TXX_INT(1) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_TXX_INT(1);
    info.status_mask        = 1ull<<0 /* txf_err */;
    info.enable_addr        = CVMX_ILK_TXX_INT_EN(1);
    info.enable_mask        = 1ull<<0 /* txf_err */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_TXX_INT(1)[TXF_ERR]: TX fifo parity error occurred.  At EOP time, EOP_Format will\n"
        "    reflect the error.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_TXX_INT(1);
    info.status_mask        = 1ull<<1 /* bad_seq */;
    info.enable_addr        = CVMX_ILK_TXX_INT_EN(1);
    info.enable_mask        = 1ull<<1 /* bad_seq */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_TXX_INT(1)[BAD_SEQ]: Received sequence is not SOP followed by 0 or more data cycles\n"
        "    followed by EOP.  PKO config assigned multiple engines to the\n"
        "    same ILK Tx Link.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_TXX_INT(1);
    info.status_mask        = 1ull<<2 /* bad_pipe */;
    info.enable_addr        = CVMX_ILK_TXX_INT_EN(1);
    info.enable_mask        = 1ull<<2 /* bad_pipe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_TXX_INT(1)[BAD_PIPE]: Received a PKO port-pipe out of the range specified by\n"
        "    ILK_TXX_PIPE\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_TXX_INT(1);
    info.status_mask        = 1ull<<3 /* stat_cnt_ovfl */;
    info.enable_addr        = CVMX_ILK_TXX_INT_EN(1);
    info.enable_mask        = 1ull<<3 /* stat_cnt_ovfl */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_TXX_INT(1)[STAT_CNT_OVFL]: Statistics counter overflow\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_ILK_RXX_INT(0) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RXX_INT(0);
    info.status_mask        = 1ull<<0 /* lane_align_fail */;
    info.enable_addr        = CVMX_ILK_RXX_INT_EN(0);
    info.enable_mask        = 1ull<<0 /* lane_align_fail */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RXX_INT(0)[LANE_ALIGN_FAIL]: Lane Alignment fails (4 tries).  Hardware will repeat lane\n"
        "    alignment until is succeeds or until ILK_RXx_CFG1[RX_ALIGN_ENA]\n"
        "    is cleared.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RXX_INT(0);
    info.status_mask        = 1ull<<1 /* crc24_err */;
    info.enable_addr        = CVMX_ILK_RXX_INT_EN(0);
    info.enable_mask        = 1ull<<1 /* crc24_err */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RXX_INT(0)[CRC24_ERR]: Burst CRC24 error.  All open packets will be receive an error.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RXX_INT(0);
    info.status_mask        = 1ull<<2 /* word_sync_done */;
    info.enable_addr        = CVMX_ILK_RXX_INT_EN(0);
    info.enable_mask        = 1ull<<2 /* word_sync_done */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RXX_INT(0)[WORD_SYNC_DONE]: All enabled lanes have achieved word boundary lock and\n"
        "    scrambler synchronization.  Lane alignment may now be enabled.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RXX_INT(0);
    info.status_mask        = 1ull<<3 /* lane_align_done */;
    info.enable_addr        = CVMX_ILK_RXX_INT_EN(0);
    info.enable_mask        = 1ull<<3 /* lane_align_done */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RXX_INT(0)[LANE_ALIGN_DONE]: Lane alignment successful\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RXX_INT(0);
    info.status_mask        = 1ull<<4 /* stat_cnt_ovfl */;
    info.enable_addr        = CVMX_ILK_RXX_INT_EN(0);
    info.enable_mask        = 1ull<<4 /* stat_cnt_ovfl */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RXX_INT(0)[STAT_CNT_OVFL]: Statistics counter overflow\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RXX_INT(0);
    info.status_mask        = 1ull<<5 /* lane_bad_word */;
    info.enable_addr        = CVMX_ILK_RXX_INT_EN(0);
    info.enable_mask        = 1ull<<5 /* lane_bad_word */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RXX_INT(0)[LANE_BAD_WORD]: A lane encountered either a bad 64B/67B codeword or an unknown\n"
        "    control word type.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RXX_INT(0);
    info.status_mask        = 1ull<<6 /* pkt_drop_rxf */;
    info.enable_addr        = CVMX_ILK_RXX_INT_EN(0);
    info.enable_mask        = 1ull<<6 /* pkt_drop_rxf */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RXX_INT(0)[PKT_DROP_RXF]: Some/all of a packet dropped due to RX_FIFO_CNT == RX_FIFO_MAX\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RXX_INT(0);
    info.status_mask        = 1ull<<7 /* pkt_drop_rid */;
    info.enable_addr        = CVMX_ILK_RXX_INT_EN(0);
    info.enable_mask        = 1ull<<7 /* pkt_drop_rid */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RXX_INT(0)[PKT_DROP_RID]: Entire packet dropped due to the lack of reassembly-ids or\n"
        "    because ILK_RXX_CFG1[PKT_ENA]=0\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RXX_INT(0);
    info.status_mask        = 1ull<<8 /* pkt_drop_sop */;
    info.enable_addr        = CVMX_ILK_RXX_INT_EN(0);
    info.enable_mask        = 1ull<<8 /* pkt_drop_sop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RXX_INT(0)[PKT_DROP_SOP]: Entire packet dropped due to RX_FIFO_CNT == RX_FIFO_MAX,\n"
        "    lack of reassembly-ids or because ILK_RXX_CFG1[PKT_ENA]=0      | $RW\n"
        "    because ILK_RXX_CFG1[PKT_ENA]=0\n"
        "    ***NOTE: Added in pass 2.0\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_ILK_RXX_INT(1) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RXX_INT(1);
    info.status_mask        = 1ull<<0 /* lane_align_fail */;
    info.enable_addr        = CVMX_ILK_RXX_INT_EN(1);
    info.enable_mask        = 1ull<<0 /* lane_align_fail */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RXX_INT(1)[LANE_ALIGN_FAIL]: Lane Alignment fails (4 tries).  Hardware will repeat lane\n"
        "    alignment until is succeeds or until ILK_RXx_CFG1[RX_ALIGN_ENA]\n"
        "    is cleared.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RXX_INT(1);
    info.status_mask        = 1ull<<1 /* crc24_err */;
    info.enable_addr        = CVMX_ILK_RXX_INT_EN(1);
    info.enable_mask        = 1ull<<1 /* crc24_err */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RXX_INT(1)[CRC24_ERR]: Burst CRC24 error.  All open packets will be receive an error.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RXX_INT(1);
    info.status_mask        = 1ull<<2 /* word_sync_done */;
    info.enable_addr        = CVMX_ILK_RXX_INT_EN(1);
    info.enable_mask        = 1ull<<2 /* word_sync_done */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RXX_INT(1)[WORD_SYNC_DONE]: All enabled lanes have achieved word boundary lock and\n"
        "    scrambler synchronization.  Lane alignment may now be enabled.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RXX_INT(1);
    info.status_mask        = 1ull<<3 /* lane_align_done */;
    info.enable_addr        = CVMX_ILK_RXX_INT_EN(1);
    info.enable_mask        = 1ull<<3 /* lane_align_done */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RXX_INT(1)[LANE_ALIGN_DONE]: Lane alignment successful\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RXX_INT(1);
    info.status_mask        = 1ull<<4 /* stat_cnt_ovfl */;
    info.enable_addr        = CVMX_ILK_RXX_INT_EN(1);
    info.enable_mask        = 1ull<<4 /* stat_cnt_ovfl */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RXX_INT(1)[STAT_CNT_OVFL]: Statistics counter overflow\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RXX_INT(1);
    info.status_mask        = 1ull<<5 /* lane_bad_word */;
    info.enable_addr        = CVMX_ILK_RXX_INT_EN(1);
    info.enable_mask        = 1ull<<5 /* lane_bad_word */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RXX_INT(1)[LANE_BAD_WORD]: A lane encountered either a bad 64B/67B codeword or an unknown\n"
        "    control word type.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RXX_INT(1);
    info.status_mask        = 1ull<<6 /* pkt_drop_rxf */;
    info.enable_addr        = CVMX_ILK_RXX_INT_EN(1);
    info.enable_mask        = 1ull<<6 /* pkt_drop_rxf */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RXX_INT(1)[PKT_DROP_RXF]: Some/all of a packet dropped due to RX_FIFO_CNT == RX_FIFO_MAX\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RXX_INT(1);
    info.status_mask        = 1ull<<7 /* pkt_drop_rid */;
    info.enable_addr        = CVMX_ILK_RXX_INT_EN(1);
    info.enable_mask        = 1ull<<7 /* pkt_drop_rid */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RXX_INT(1)[PKT_DROP_RID]: Entire packet dropped due to the lack of reassembly-ids or\n"
        "    because ILK_RXX_CFG1[PKT_ENA]=0\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RXX_INT(1);
    info.status_mask        = 1ull<<8 /* pkt_drop_sop */;
    info.enable_addr        = CVMX_ILK_RXX_INT_EN(1);
    info.enable_mask        = 1ull<<8 /* pkt_drop_sop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RXX_INT(1)[PKT_DROP_SOP]: Entire packet dropped due to RX_FIFO_CNT == RX_FIFO_MAX,\n"
        "    lack of reassembly-ids or because ILK_RXX_CFG1[PKT_ENA]=0      | $RW\n"
        "    because ILK_RXX_CFG1[PKT_ENA]=0\n"
        "    ***NOTE: Added in pass 2.0\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_ILK_RX_LNEX_INT(0) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(0);
    info.status_mask        = 1ull<<0 /* serdes_lock_loss */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(0);
    info.enable_mask        = 1ull<<0 /* serdes_lock_loss */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(0)[SERDES_LOCK_LOSS]: Rx SERDES loses lock\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(0);
    info.status_mask        = 1ull<<1 /* bdry_sync_loss */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(0);
    info.enable_mask        = 1ull<<1 /* bdry_sync_loss */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(0)[BDRY_SYNC_LOSS]: Rx logic loses word boundary sync (16 tries).  Hardware will\n"
        "    automatically attempt to regain word boundary sync\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(0);
    info.status_mask        = 1ull<<2 /* crc32_err */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(0);
    info.enable_mask        = 1ull<<2 /* crc32_err */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(0)[CRC32_ERR]: Diagnostic CRC32 errors\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(0);
    info.status_mask        = 1ull<<3 /* ukwn_cntl_word */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(0);
    info.enable_mask        = 1ull<<3 /* ukwn_cntl_word */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(0)[UKWN_CNTL_WORD]: Unknown framing control word. Block type does not match any of\n"
        "    (SYNC,SCRAM,SKIP,DIAG)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(0);
    info.status_mask        = 1ull<<4 /* scrm_sync_loss */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(0);
    info.enable_mask        = 1ull<<4 /* scrm_sync_loss */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(0)[SCRM_SYNC_LOSS]: 4 consecutive bad sync words or 3 consecutive scramble state\n"
        "    mismatches\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(0);
    info.status_mask        = 1ull<<5 /* dskew_fifo_ovfl */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(0);
    info.enable_mask        = 1ull<<5 /* dskew_fifo_ovfl */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(0)[DSKEW_FIFO_OVFL]: Rx deskew fifo overflow occurred.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(0);
    info.status_mask        = 1ull<<6 /* stat_msg */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(0);
    info.enable_mask        = 1ull<<6 /* stat_msg */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(0)[STAT_MSG]: Status bits for the link or a lane transitioned from a '1'\n"
        "    (healthy) to a '0' (problem)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(0);
    info.status_mask        = 1ull<<7 /* stat_cnt_ovfl */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(0);
    info.enable_mask        = 1ull<<7 /* stat_cnt_ovfl */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(0)[STAT_CNT_OVFL]: Rx lane statistic counter overflow\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(0);
    info.status_mask        = 1ull<<8 /* bad_64b67b */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(0);
    info.enable_mask        = 1ull<<8 /* bad_64b67b */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(0)[BAD_64B67B]: Bad 64B/67B codeword encountered.  Once the bad word reaches\n"
        "    the burst control unit (as deonted by\n"
        "    ILK_RXx_INT[LANE_BAD_WORD]) it will be tossed and all open\n"
        "    packets will receive an error.\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_ILK_RX_LNEX_INT(1) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(1);
    info.status_mask        = 1ull<<0 /* serdes_lock_loss */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(1);
    info.enable_mask        = 1ull<<0 /* serdes_lock_loss */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(1)[SERDES_LOCK_LOSS]: Rx SERDES loses lock\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(1);
    info.status_mask        = 1ull<<1 /* bdry_sync_loss */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(1);
    info.enable_mask        = 1ull<<1 /* bdry_sync_loss */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(1)[BDRY_SYNC_LOSS]: Rx logic loses word boundary sync (16 tries).  Hardware will\n"
        "    automatically attempt to regain word boundary sync\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(1);
    info.status_mask        = 1ull<<2 /* crc32_err */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(1);
    info.enable_mask        = 1ull<<2 /* crc32_err */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(1)[CRC32_ERR]: Diagnostic CRC32 errors\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(1);
    info.status_mask        = 1ull<<3 /* ukwn_cntl_word */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(1);
    info.enable_mask        = 1ull<<3 /* ukwn_cntl_word */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(1)[UKWN_CNTL_WORD]: Unknown framing control word. Block type does not match any of\n"
        "    (SYNC,SCRAM,SKIP,DIAG)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(1);
    info.status_mask        = 1ull<<4 /* scrm_sync_loss */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(1);
    info.enable_mask        = 1ull<<4 /* scrm_sync_loss */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(1)[SCRM_SYNC_LOSS]: 4 consecutive bad sync words or 3 consecutive scramble state\n"
        "    mismatches\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(1);
    info.status_mask        = 1ull<<5 /* dskew_fifo_ovfl */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(1);
    info.enable_mask        = 1ull<<5 /* dskew_fifo_ovfl */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(1)[DSKEW_FIFO_OVFL]: Rx deskew fifo overflow occurred.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(1);
    info.status_mask        = 1ull<<6 /* stat_msg */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(1);
    info.enable_mask        = 1ull<<6 /* stat_msg */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(1)[STAT_MSG]: Status bits for the link or a lane transitioned from a '1'\n"
        "    (healthy) to a '0' (problem)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(1);
    info.status_mask        = 1ull<<7 /* stat_cnt_ovfl */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(1);
    info.enable_mask        = 1ull<<7 /* stat_cnt_ovfl */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(1)[STAT_CNT_OVFL]: Rx lane statistic counter overflow\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(1);
    info.status_mask        = 1ull<<8 /* bad_64b67b */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(1);
    info.enable_mask        = 1ull<<8 /* bad_64b67b */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(1)[BAD_64B67B]: Bad 64B/67B codeword encountered.  Once the bad word reaches\n"
        "    the burst control unit (as deonted by\n"
        "    ILK_RXx_INT[LANE_BAD_WORD]) it will be tossed and all open\n"
        "    packets will receive an error.\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_ILK_RX_LNEX_INT(2) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(2);
    info.status_mask        = 1ull<<0 /* serdes_lock_loss */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(2);
    info.enable_mask        = 1ull<<0 /* serdes_lock_loss */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(2)[SERDES_LOCK_LOSS]: Rx SERDES loses lock\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(2);
    info.status_mask        = 1ull<<1 /* bdry_sync_loss */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(2);
    info.enable_mask        = 1ull<<1 /* bdry_sync_loss */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(2)[BDRY_SYNC_LOSS]: Rx logic loses word boundary sync (16 tries).  Hardware will\n"
        "    automatically attempt to regain word boundary sync\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(2);
    info.status_mask        = 1ull<<2 /* crc32_err */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(2);
    info.enable_mask        = 1ull<<2 /* crc32_err */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(2)[CRC32_ERR]: Diagnostic CRC32 errors\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(2);
    info.status_mask        = 1ull<<3 /* ukwn_cntl_word */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(2);
    info.enable_mask        = 1ull<<3 /* ukwn_cntl_word */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(2)[UKWN_CNTL_WORD]: Unknown framing control word. Block type does not match any of\n"
        "    (SYNC,SCRAM,SKIP,DIAG)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(2);
    info.status_mask        = 1ull<<4 /* scrm_sync_loss */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(2);
    info.enable_mask        = 1ull<<4 /* scrm_sync_loss */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(2)[SCRM_SYNC_LOSS]: 4 consecutive bad sync words or 3 consecutive scramble state\n"
        "    mismatches\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(2);
    info.status_mask        = 1ull<<5 /* dskew_fifo_ovfl */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(2);
    info.enable_mask        = 1ull<<5 /* dskew_fifo_ovfl */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(2)[DSKEW_FIFO_OVFL]: Rx deskew fifo overflow occurred.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(2);
    info.status_mask        = 1ull<<6 /* stat_msg */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(2);
    info.enable_mask        = 1ull<<6 /* stat_msg */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(2)[STAT_MSG]: Status bits for the link or a lane transitioned from a '1'\n"
        "    (healthy) to a '0' (problem)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(2);
    info.status_mask        = 1ull<<7 /* stat_cnt_ovfl */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(2);
    info.enable_mask        = 1ull<<7 /* stat_cnt_ovfl */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(2)[STAT_CNT_OVFL]: Rx lane statistic counter overflow\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(2);
    info.status_mask        = 1ull<<8 /* bad_64b67b */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(2);
    info.enable_mask        = 1ull<<8 /* bad_64b67b */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(2)[BAD_64B67B]: Bad 64B/67B codeword encountered.  Once the bad word reaches\n"
        "    the burst control unit (as deonted by\n"
        "    ILK_RXx_INT[LANE_BAD_WORD]) it will be tossed and all open\n"
        "    packets will receive an error.\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_ILK_RX_LNEX_INT(3) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(3);
    info.status_mask        = 1ull<<0 /* serdes_lock_loss */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(3);
    info.enable_mask        = 1ull<<0 /* serdes_lock_loss */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(3)[SERDES_LOCK_LOSS]: Rx SERDES loses lock\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(3);
    info.status_mask        = 1ull<<1 /* bdry_sync_loss */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(3);
    info.enable_mask        = 1ull<<1 /* bdry_sync_loss */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(3)[BDRY_SYNC_LOSS]: Rx logic loses word boundary sync (16 tries).  Hardware will\n"
        "    automatically attempt to regain word boundary sync\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(3);
    info.status_mask        = 1ull<<2 /* crc32_err */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(3);
    info.enable_mask        = 1ull<<2 /* crc32_err */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(3)[CRC32_ERR]: Diagnostic CRC32 errors\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(3);
    info.status_mask        = 1ull<<3 /* ukwn_cntl_word */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(3);
    info.enable_mask        = 1ull<<3 /* ukwn_cntl_word */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(3)[UKWN_CNTL_WORD]: Unknown framing control word. Block type does not match any of\n"
        "    (SYNC,SCRAM,SKIP,DIAG)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(3);
    info.status_mask        = 1ull<<4 /* scrm_sync_loss */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(3);
    info.enable_mask        = 1ull<<4 /* scrm_sync_loss */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(3)[SCRM_SYNC_LOSS]: 4 consecutive bad sync words or 3 consecutive scramble state\n"
        "    mismatches\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(3);
    info.status_mask        = 1ull<<5 /* dskew_fifo_ovfl */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(3);
    info.enable_mask        = 1ull<<5 /* dskew_fifo_ovfl */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(3)[DSKEW_FIFO_OVFL]: Rx deskew fifo overflow occurred.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(3);
    info.status_mask        = 1ull<<6 /* stat_msg */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(3);
    info.enable_mask        = 1ull<<6 /* stat_msg */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(3)[STAT_MSG]: Status bits for the link or a lane transitioned from a '1'\n"
        "    (healthy) to a '0' (problem)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(3);
    info.status_mask        = 1ull<<7 /* stat_cnt_ovfl */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(3);
    info.enable_mask        = 1ull<<7 /* stat_cnt_ovfl */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(3)[STAT_CNT_OVFL]: Rx lane statistic counter overflow\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(3);
    info.status_mask        = 1ull<<8 /* bad_64b67b */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(3);
    info.enable_mask        = 1ull<<8 /* bad_64b67b */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(3)[BAD_64B67B]: Bad 64B/67B codeword encountered.  Once the bad word reaches\n"
        "    the burst control unit (as deonted by\n"
        "    ILK_RXx_INT[LANE_BAD_WORD]) it will be tossed and all open\n"
        "    packets will receive an error.\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_ILK_RX_LNEX_INT(4) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(4);
    info.status_mask        = 1ull<<0 /* serdes_lock_loss */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(4);
    info.enable_mask        = 1ull<<0 /* serdes_lock_loss */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 4;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(4)[SERDES_LOCK_LOSS]: Rx SERDES loses lock\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(4);
    info.status_mask        = 1ull<<1 /* bdry_sync_loss */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(4);
    info.enable_mask        = 1ull<<1 /* bdry_sync_loss */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 4;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(4)[BDRY_SYNC_LOSS]: Rx logic loses word boundary sync (16 tries).  Hardware will\n"
        "    automatically attempt to regain word boundary sync\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(4);
    info.status_mask        = 1ull<<2 /* crc32_err */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(4);
    info.enable_mask        = 1ull<<2 /* crc32_err */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 4;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(4)[CRC32_ERR]: Diagnostic CRC32 errors\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(4);
    info.status_mask        = 1ull<<3 /* ukwn_cntl_word */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(4);
    info.enable_mask        = 1ull<<3 /* ukwn_cntl_word */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 4;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(4)[UKWN_CNTL_WORD]: Unknown framing control word. Block type does not match any of\n"
        "    (SYNC,SCRAM,SKIP,DIAG)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(4);
    info.status_mask        = 1ull<<4 /* scrm_sync_loss */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(4);
    info.enable_mask        = 1ull<<4 /* scrm_sync_loss */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 4;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(4)[SCRM_SYNC_LOSS]: 4 consecutive bad sync words or 3 consecutive scramble state\n"
        "    mismatches\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(4);
    info.status_mask        = 1ull<<5 /* dskew_fifo_ovfl */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(4);
    info.enable_mask        = 1ull<<5 /* dskew_fifo_ovfl */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 4;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(4)[DSKEW_FIFO_OVFL]: Rx deskew fifo overflow occurred.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(4);
    info.status_mask        = 1ull<<6 /* stat_msg */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(4);
    info.enable_mask        = 1ull<<6 /* stat_msg */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 4;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(4)[STAT_MSG]: Status bits for the link or a lane transitioned from a '1'\n"
        "    (healthy) to a '0' (problem)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(4);
    info.status_mask        = 1ull<<7 /* stat_cnt_ovfl */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(4);
    info.enable_mask        = 1ull<<7 /* stat_cnt_ovfl */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 4;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(4)[STAT_CNT_OVFL]: Rx lane statistic counter overflow\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(4);
    info.status_mask        = 1ull<<8 /* bad_64b67b */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(4);
    info.enable_mask        = 1ull<<8 /* bad_64b67b */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 4;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(4)[BAD_64B67B]: Bad 64B/67B codeword encountered.  Once the bad word reaches\n"
        "    the burst control unit (as deonted by\n"
        "    ILK_RXx_INT[LANE_BAD_WORD]) it will be tossed and all open\n"
        "    packets will receive an error.\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_ILK_RX_LNEX_INT(5) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(5);
    info.status_mask        = 1ull<<0 /* serdes_lock_loss */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(5);
    info.enable_mask        = 1ull<<0 /* serdes_lock_loss */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 5;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(5)[SERDES_LOCK_LOSS]: Rx SERDES loses lock\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(5);
    info.status_mask        = 1ull<<1 /* bdry_sync_loss */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(5);
    info.enable_mask        = 1ull<<1 /* bdry_sync_loss */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 5;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(5)[BDRY_SYNC_LOSS]: Rx logic loses word boundary sync (16 tries).  Hardware will\n"
        "    automatically attempt to regain word boundary sync\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(5);
    info.status_mask        = 1ull<<2 /* crc32_err */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(5);
    info.enable_mask        = 1ull<<2 /* crc32_err */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 5;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(5)[CRC32_ERR]: Diagnostic CRC32 errors\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(5);
    info.status_mask        = 1ull<<3 /* ukwn_cntl_word */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(5);
    info.enable_mask        = 1ull<<3 /* ukwn_cntl_word */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 5;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(5)[UKWN_CNTL_WORD]: Unknown framing control word. Block type does not match any of\n"
        "    (SYNC,SCRAM,SKIP,DIAG)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(5);
    info.status_mask        = 1ull<<4 /* scrm_sync_loss */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(5);
    info.enable_mask        = 1ull<<4 /* scrm_sync_loss */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 5;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(5)[SCRM_SYNC_LOSS]: 4 consecutive bad sync words or 3 consecutive scramble state\n"
        "    mismatches\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(5);
    info.status_mask        = 1ull<<5 /* dskew_fifo_ovfl */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(5);
    info.enable_mask        = 1ull<<5 /* dskew_fifo_ovfl */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 5;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(5)[DSKEW_FIFO_OVFL]: Rx deskew fifo overflow occurred.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(5);
    info.status_mask        = 1ull<<6 /* stat_msg */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(5);
    info.enable_mask        = 1ull<<6 /* stat_msg */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 5;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(5)[STAT_MSG]: Status bits for the link or a lane transitioned from a '1'\n"
        "    (healthy) to a '0' (problem)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(5);
    info.status_mask        = 1ull<<7 /* stat_cnt_ovfl */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(5);
    info.enable_mask        = 1ull<<7 /* stat_cnt_ovfl */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 5;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(5)[STAT_CNT_OVFL]: Rx lane statistic counter overflow\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(5);
    info.status_mask        = 1ull<<8 /* bad_64b67b */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(5);
    info.enable_mask        = 1ull<<8 /* bad_64b67b */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 5;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(5)[BAD_64B67B]: Bad 64B/67B codeword encountered.  Once the bad word reaches\n"
        "    the burst control unit (as deonted by\n"
        "    ILK_RXx_INT[LANE_BAD_WORD]) it will be tossed and all open\n"
        "    packets will receive an error.\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_ILK_RX_LNEX_INT(6) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(6);
    info.status_mask        = 1ull<<0 /* serdes_lock_loss */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(6);
    info.enable_mask        = 1ull<<0 /* serdes_lock_loss */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 6;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(6)[SERDES_LOCK_LOSS]: Rx SERDES loses lock\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(6);
    info.status_mask        = 1ull<<1 /* bdry_sync_loss */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(6);
    info.enable_mask        = 1ull<<1 /* bdry_sync_loss */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 6;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(6)[BDRY_SYNC_LOSS]: Rx logic loses word boundary sync (16 tries).  Hardware will\n"
        "    automatically attempt to regain word boundary sync\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(6);
    info.status_mask        = 1ull<<2 /* crc32_err */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(6);
    info.enable_mask        = 1ull<<2 /* crc32_err */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 6;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(6)[CRC32_ERR]: Diagnostic CRC32 errors\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(6);
    info.status_mask        = 1ull<<3 /* ukwn_cntl_word */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(6);
    info.enable_mask        = 1ull<<3 /* ukwn_cntl_word */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 6;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(6)[UKWN_CNTL_WORD]: Unknown framing control word. Block type does not match any of\n"
        "    (SYNC,SCRAM,SKIP,DIAG)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(6);
    info.status_mask        = 1ull<<4 /* scrm_sync_loss */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(6);
    info.enable_mask        = 1ull<<4 /* scrm_sync_loss */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 6;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(6)[SCRM_SYNC_LOSS]: 4 consecutive bad sync words or 3 consecutive scramble state\n"
        "    mismatches\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(6);
    info.status_mask        = 1ull<<5 /* dskew_fifo_ovfl */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(6);
    info.enable_mask        = 1ull<<5 /* dskew_fifo_ovfl */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 6;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(6)[DSKEW_FIFO_OVFL]: Rx deskew fifo overflow occurred.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(6);
    info.status_mask        = 1ull<<6 /* stat_msg */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(6);
    info.enable_mask        = 1ull<<6 /* stat_msg */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 6;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(6)[STAT_MSG]: Status bits for the link or a lane transitioned from a '1'\n"
        "    (healthy) to a '0' (problem)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(6);
    info.status_mask        = 1ull<<7 /* stat_cnt_ovfl */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(6);
    info.enable_mask        = 1ull<<7 /* stat_cnt_ovfl */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 6;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(6)[STAT_CNT_OVFL]: Rx lane statistic counter overflow\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(6);
    info.status_mask        = 1ull<<8 /* bad_64b67b */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(6);
    info.enable_mask        = 1ull<<8 /* bad_64b67b */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 6;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(6)[BAD_64B67B]: Bad 64B/67B codeword encountered.  Once the bad word reaches\n"
        "    the burst control unit (as deonted by\n"
        "    ILK_RXx_INT[LANE_BAD_WORD]) it will be tossed and all open\n"
        "    packets will receive an error.\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_ILK_RX_LNEX_INT(7) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(7);
    info.status_mask        = 1ull<<0 /* serdes_lock_loss */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(7);
    info.enable_mask        = 1ull<<0 /* serdes_lock_loss */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 7;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(7)[SERDES_LOCK_LOSS]: Rx SERDES loses lock\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(7);
    info.status_mask        = 1ull<<1 /* bdry_sync_loss */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(7);
    info.enable_mask        = 1ull<<1 /* bdry_sync_loss */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 7;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(7)[BDRY_SYNC_LOSS]: Rx logic loses word boundary sync (16 tries).  Hardware will\n"
        "    automatically attempt to regain word boundary sync\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(7);
    info.status_mask        = 1ull<<2 /* crc32_err */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(7);
    info.enable_mask        = 1ull<<2 /* crc32_err */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 7;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(7)[CRC32_ERR]: Diagnostic CRC32 errors\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(7);
    info.status_mask        = 1ull<<3 /* ukwn_cntl_word */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(7);
    info.enable_mask        = 1ull<<3 /* ukwn_cntl_word */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 7;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(7)[UKWN_CNTL_WORD]: Unknown framing control word. Block type does not match any of\n"
        "    (SYNC,SCRAM,SKIP,DIAG)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(7);
    info.status_mask        = 1ull<<4 /* scrm_sync_loss */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(7);
    info.enable_mask        = 1ull<<4 /* scrm_sync_loss */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 7;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(7)[SCRM_SYNC_LOSS]: 4 consecutive bad sync words or 3 consecutive scramble state\n"
        "    mismatches\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(7);
    info.status_mask        = 1ull<<5 /* dskew_fifo_ovfl */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(7);
    info.enable_mask        = 1ull<<5 /* dskew_fifo_ovfl */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 7;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(7)[DSKEW_FIFO_OVFL]: Rx deskew fifo overflow occurred.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(7);
    info.status_mask        = 1ull<<6 /* stat_msg */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(7);
    info.enable_mask        = 1ull<<6 /* stat_msg */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 7;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(7)[STAT_MSG]: Status bits for the link or a lane transitioned from a '1'\n"
        "    (healthy) to a '0' (problem)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(7);
    info.status_mask        = 1ull<<7 /* stat_cnt_ovfl */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(7);
    info.enable_mask        = 1ull<<7 /* stat_cnt_ovfl */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 7;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(7)[STAT_CNT_OVFL]: Rx lane statistic counter overflow\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ILK_RX_LNEX_INT(7);
    info.status_mask        = 1ull<<8 /* bad_64b67b */;
    info.enable_addr        = CVMX_ILK_RX_LNEX_INT_EN(7);
    info.enable_mask        = 1ull<<8 /* bad_64b67b */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ILK;
    info.group_index        = 7;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_PKT(0);
    info.parent.status_mask = 1ull<<48 /* ilk */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ILK_RX_LNEX_INT(7)[BAD_64B67B]: Bad 64B/67B codeword encountered.  Once the bad word reaches\n"
        "    the burst control unit (as deonted by\n"
        "    ILK_RXx_INT[LANE_BAD_WORD]) it will be tossed and all open\n"
        "    packets will receive an error.\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_CIU2_SRC_PPX_IP2_RML(0) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_CIU2_SRC_PPX_IP2_RML(0);
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

    /* CVMX_L2C_INT_REG */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_INT_REG;
    info.status_mask        = 1ull<<0 /* holerd */;
    info.enable_addr        = CVMX_L2C_INT_ENA;
    info.enable_mask        = 1ull<<0 /* holerd */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<48 /* l2c */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_INT_REG[HOLERD]: Read reference to 256MB hole occurred\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_INT_REG;
    info.status_mask        = 1ull<<1 /* holewr */;
    info.enable_addr        = CVMX_L2C_INT_ENA;
    info.enable_mask        = 1ull<<1 /* holewr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<48 /* l2c */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_INT_REG[HOLEWR]: Write reference to 256MB hole occurred\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_INT_REG;
    info.status_mask        = 1ull<<2 /* vrtwr */;
    info.enable_addr        = CVMX_L2C_INT_ENA;
    info.enable_mask        = 1ull<<2 /* vrtwr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<48 /* l2c */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_INT_REG[VRTWR]: Virtualization ID prevented a write\n"
        "    Set when L2C_VRT_MEM blocked a store.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_INT_REG;
    info.status_mask        = 1ull<<3 /* vrtidrng */;
    info.enable_addr        = CVMX_L2C_INT_ENA;
    info.enable_mask        = 1ull<<3 /* vrtidrng */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<48 /* l2c */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_INT_REG[VRTIDRNG]: Virtualization ID out of range\n"
        "    Set when a L2C_VRT_CTL[NUMID] violation blocked a\n"
        "    store.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_INT_REG;
    info.status_mask        = 1ull<<4 /* vrtadrng */;
    info.enable_addr        = CVMX_L2C_INT_ENA;
    info.enable_mask        = 1ull<<4 /* vrtadrng */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<48 /* l2c */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_INT_REG[VRTADRNG]: Address outside of virtualization range\n"
        "    Set when a L2C_VRT_CTL[MEMSZ] violation blocked a\n"
        "    store.\n"
        "    L2C_VRT_CTL[OOBERR] must be set for L2C to set this.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_INT_REG;
    info.status_mask        = 1ull<<5 /* vrtpe */;
    info.enable_addr        = CVMX_L2C_INT_ENA;
    info.enable_mask        = 1ull<<5 /* vrtpe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<48 /* l2c */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_INT_REG[VRTPE]: L2C_VRT_MEM read found a parity error\n"
        "    Whenever an L2C_VRT_MEM read finds a parity error,\n"
        "    that L2C_VRT_MEM cannot cause stores to be blocked.\n"
        "    Software should correct the error.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_INT_REG;
    info.status_mask        = 1ull<<6 /* bigwr */;
    info.enable_addr        = CVMX_L2C_INT_ENA;
    info.enable_mask        = 1ull<<6 /* bigwr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<48 /* l2c */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_INT_REG[BIGWR]: Write reference past L2C_BIG_CTL[MAXDRAM] occurred\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_INT_REG;
    info.status_mask        = 1ull<<7 /* bigrd */;
    info.enable_addr        = CVMX_L2C_INT_ENA;
    info.enable_mask        = 1ull<<7 /* bigrd */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<48 /* l2c */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_INT_REG[BIGRD]: Read reference past L2C_BIG_CTL[MAXDRAM] occurred\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_INT_REG;
    info.status_mask        = 0;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<48 /* l2c */;
    info.func               = __cvmx_error_decode;
    info.user_info          = 0;
    fail |= cvmx_error_add(&info);

    /* CVMX_L2C_TADX_INT(1) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_TADX_INT(1);
    info.status_mask        = 1ull<<0 /* l2dsbe */;
    info.enable_addr        = CVMX_L2C_TADX_IEN(1);
    info.enable_mask        = 1ull<<0 /* l2dsbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<17 /* tad1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_TADX_INT(1)[L2DSBE]: L2D Single-Bit Error\n"
        "    Shadow copy of L2C_ERR_TDTX[SBE]\n"
        "    Writes of 1 also clear L2C_ERR_TDTX[SBE]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_TADX_INT(1);
    info.status_mask        = 1ull<<1 /* l2ddbe */;
    info.enable_addr        = CVMX_L2C_TADX_IEN(1);
    info.enable_mask        = 1ull<<1 /* l2ddbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<17 /* tad1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_TADX_INT(1)[L2DDBE]: L2D Double-Bit Error\n"
        "    Shadow copy of L2C_ERR_TDTX[DBE]\n"
        "    Writes of 1 also clear L2C_ERR_TDTX[DBE]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_TADX_INT(1);
    info.status_mask        = 1ull<<2 /* tagsbe */;
    info.enable_addr        = CVMX_L2C_TADX_IEN(1);
    info.enable_mask        = 1ull<<2 /* tagsbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<17 /* tad1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_TADX_INT(1)[TAGSBE]: TAG Single-Bit Error\n"
        "    Shadow copy of L2C_ERR_TTGX[SBE]\n"
        "    Writes of 1 also clear L2C_ERR_TTGX[SBE]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_TADX_INT(1);
    info.status_mask        = 1ull<<3 /* tagdbe */;
    info.enable_addr        = CVMX_L2C_TADX_IEN(1);
    info.enable_mask        = 1ull<<3 /* tagdbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<17 /* tad1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_TADX_INT(1)[TAGDBE]: TAG Double-Bit Error\n"
        "    Shadow copy of L2C_ERR_TTGX[DBE]\n"
        "    Writes of 1 also clear L2C_ERR_TTGX[DBE]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_TADX_INT(1);
    info.status_mask        = 1ull<<4 /* vbfsbe */;
    info.enable_addr        = CVMX_L2C_TADX_IEN(1);
    info.enable_mask        = 1ull<<4 /* vbfsbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<17 /* tad1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_TADX_INT(1)[VBFSBE]: VBF Single-Bit Error\n"
        "    Shadow copy of L2C_ERR_TDTX[VSBE]\n"
        "    Writes of 1 also clear L2C_ERR_TDTX[VSBE]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_TADX_INT(1);
    info.status_mask        = 1ull<<5 /* vbfdbe */;
    info.enable_addr        = CVMX_L2C_TADX_IEN(1);
    info.enable_mask        = 1ull<<5 /* vbfdbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<17 /* tad1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_TADX_INT(1)[VBFDBE]: VBF Double-Bit Error\n"
        "    Shadow copy of L2C_ERR_TDTX[VDBE]\n"
        "    Writes of 1 also clear L2C_ERR_TDTX[VDBE]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_TADX_INT(1);
    info.status_mask        = 1ull<<6 /* noway */;
    info.enable_addr        = CVMX_L2C_TADX_IEN(1);
    info.enable_mask        = 1ull<<6 /* noway */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<17 /* tad1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_TADX_INT(1)[NOWAY]: No way available interrupt\n"
        "    Shadow copy of L2C_ERR_TTGX[NOWAY]\n"
        "    Writes of 1 also clear L2C_ERR_TTGX[NOWAY]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_TADX_INT(1);
    info.status_mask        = 1ull<<7 /* rddislmc */;
    info.enable_addr        = CVMX_L2C_TADX_IEN(1);
    info.enable_mask        = 1ull<<7 /* rddislmc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<17 /* tad1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_TADX_INT(1)[RDDISLMC]: Illegal Read  to Disabled LMC Error\n"
        "    A DRAM read  arrived before the LMC(s) were enabled\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_TADX_INT(1);
    info.status_mask        = 1ull<<8 /* wrdislmc */;
    info.enable_addr        = CVMX_L2C_TADX_IEN(1);
    info.enable_mask        = 1ull<<8 /* wrdislmc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<17 /* tad1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_TADX_INT(1)[WRDISLMC]: Illegal Write to Disabled LMC Error\n"
        "    A DRAM write arrived before the LMC(s) were enabled\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_L2C_ERR_TDTX(1) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_ERR_TDTX(1);
    info.status_mask        = 1ull<<60 /* vsbe */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<17 /* tad1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_ERR_TDTX(1)[VSBE]: VBF Single-Bit error has occurred\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_ERR_TDTX(1);
    info.status_mask        = 1ull<<61 /* vdbe */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<17 /* tad1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_ERR_TDTX(1)[VDBE]: VBF Double-Bit error has occurred\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_ERR_TDTX(1);
    info.status_mask        = 1ull<<62 /* sbe */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<17 /* tad1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_ERR_TDTX(1)[SBE]: L2D Single-Bit error has occurred\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_ERR_TDTX(1);
    info.status_mask        = 1ull<<63 /* dbe */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<17 /* tad1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_ERR_TDTX(1)[DBE]: L2D Double-Bit error has occurred\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_L2C_ERR_TTGX(1) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_ERR_TTGX(1);
    info.status_mask        = 1ull<<61 /* noway */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<17 /* tad1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_ERR_TTGX(1)[NOWAY]: No way was available for allocation.\n"
        "    L2C sets NOWAY during its processing of a\n"
        "    transaction whenever it needed/wanted to allocate\n"
        "    a WAY in the L2 cache, but was unable to. NOWAY==1\n"
        "    is (generally) not an indication that L2C failed to\n"
        "    complete transactions. Rather, it is a hint of\n"
        "    possible performance degradation. (For example, L2C\n"
        "    must read-modify-write DRAM for every transaction\n"
        "    that updates some, but not all, of the bytes in a\n"
        "    cache block, misses in the L2 cache, and cannot\n"
        "    allocate a WAY.) There is one \"failure\" case where\n"
        "    L2C will set NOWAY: when it cannot leave a block\n"
        "    locked in the L2 cache as part of a LCKL2\n"
        "    transaction.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_ERR_TTGX(1);
    info.status_mask        = 1ull<<62 /* sbe */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<17 /* tad1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_ERR_TTGX(1)[SBE]: Single-Bit ECC error\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_ERR_TTGX(1);
    info.status_mask        = 1ull<<63 /* dbe */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<17 /* tad1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_ERR_TTGX(1)[DBE]: Double-Bit ECC error\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_L2C_TADX_INT(0) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_TADX_INT(0);
    info.status_mask        = 1ull<<0 /* l2dsbe */;
    info.enable_addr        = CVMX_L2C_TADX_IEN(0);
    info.enable_mask        = 1ull<<0 /* l2dsbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<16 /* tad0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_TADX_INT(0)[L2DSBE]: L2D Single-Bit Error\n"
        "    Shadow copy of L2C_ERR_TDTX[SBE]\n"
        "    Writes of 1 also clear L2C_ERR_TDTX[SBE]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_TADX_INT(0);
    info.status_mask        = 1ull<<1 /* l2ddbe */;
    info.enable_addr        = CVMX_L2C_TADX_IEN(0);
    info.enable_mask        = 1ull<<1 /* l2ddbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<16 /* tad0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_TADX_INT(0)[L2DDBE]: L2D Double-Bit Error\n"
        "    Shadow copy of L2C_ERR_TDTX[DBE]\n"
        "    Writes of 1 also clear L2C_ERR_TDTX[DBE]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_TADX_INT(0);
    info.status_mask        = 1ull<<2 /* tagsbe */;
    info.enable_addr        = CVMX_L2C_TADX_IEN(0);
    info.enable_mask        = 1ull<<2 /* tagsbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<16 /* tad0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_TADX_INT(0)[TAGSBE]: TAG Single-Bit Error\n"
        "    Shadow copy of L2C_ERR_TTGX[SBE]\n"
        "    Writes of 1 also clear L2C_ERR_TTGX[SBE]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_TADX_INT(0);
    info.status_mask        = 1ull<<3 /* tagdbe */;
    info.enable_addr        = CVMX_L2C_TADX_IEN(0);
    info.enable_mask        = 1ull<<3 /* tagdbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<16 /* tad0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_TADX_INT(0)[TAGDBE]: TAG Double-Bit Error\n"
        "    Shadow copy of L2C_ERR_TTGX[DBE]\n"
        "    Writes of 1 also clear L2C_ERR_TTGX[DBE]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_TADX_INT(0);
    info.status_mask        = 1ull<<4 /* vbfsbe */;
    info.enable_addr        = CVMX_L2C_TADX_IEN(0);
    info.enable_mask        = 1ull<<4 /* vbfsbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<16 /* tad0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_TADX_INT(0)[VBFSBE]: VBF Single-Bit Error\n"
        "    Shadow copy of L2C_ERR_TDTX[VSBE]\n"
        "    Writes of 1 also clear L2C_ERR_TDTX[VSBE]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_TADX_INT(0);
    info.status_mask        = 1ull<<5 /* vbfdbe */;
    info.enable_addr        = CVMX_L2C_TADX_IEN(0);
    info.enable_mask        = 1ull<<5 /* vbfdbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<16 /* tad0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_TADX_INT(0)[VBFDBE]: VBF Double-Bit Error\n"
        "    Shadow copy of L2C_ERR_TDTX[VDBE]\n"
        "    Writes of 1 also clear L2C_ERR_TDTX[VDBE]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_TADX_INT(0);
    info.status_mask        = 1ull<<6 /* noway */;
    info.enable_addr        = CVMX_L2C_TADX_IEN(0);
    info.enable_mask        = 1ull<<6 /* noway */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<16 /* tad0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_TADX_INT(0)[NOWAY]: No way available interrupt\n"
        "    Shadow copy of L2C_ERR_TTGX[NOWAY]\n"
        "    Writes of 1 also clear L2C_ERR_TTGX[NOWAY]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_TADX_INT(0);
    info.status_mask        = 1ull<<7 /* rddislmc */;
    info.enable_addr        = CVMX_L2C_TADX_IEN(0);
    info.enable_mask        = 1ull<<7 /* rddislmc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<16 /* tad0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_TADX_INT(0)[RDDISLMC]: Illegal Read  to Disabled LMC Error\n"
        "    A DRAM read  arrived before the LMC(s) were enabled\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_TADX_INT(0);
    info.status_mask        = 1ull<<8 /* wrdislmc */;
    info.enable_addr        = CVMX_L2C_TADX_IEN(0);
    info.enable_mask        = 1ull<<8 /* wrdislmc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<16 /* tad0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_TADX_INT(0)[WRDISLMC]: Illegal Write to Disabled LMC Error\n"
        "    A DRAM write arrived before the LMC(s) were enabled\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_L2C_ERR_TDTX(0) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_ERR_TDTX(0);
    info.status_mask        = 1ull<<60 /* vsbe */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<16 /* tad0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_ERR_TDTX(0)[VSBE]: VBF Single-Bit error has occurred\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_ERR_TDTX(0);
    info.status_mask        = 1ull<<61 /* vdbe */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<16 /* tad0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_ERR_TDTX(0)[VDBE]: VBF Double-Bit error has occurred\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_ERR_TDTX(0);
    info.status_mask        = 1ull<<62 /* sbe */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<16 /* tad0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_ERR_TDTX(0)[SBE]: L2D Single-Bit error has occurred\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_ERR_TDTX(0);
    info.status_mask        = 1ull<<63 /* dbe */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<16 /* tad0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_ERR_TDTX(0)[DBE]: L2D Double-Bit error has occurred\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_L2C_ERR_TTGX(0) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_ERR_TTGX(0);
    info.status_mask        = 1ull<<61 /* noway */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<16 /* tad0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_ERR_TTGX(0)[NOWAY]: No way was available for allocation.\n"
        "    L2C sets NOWAY during its processing of a\n"
        "    transaction whenever it needed/wanted to allocate\n"
        "    a WAY in the L2 cache, but was unable to. NOWAY==1\n"
        "    is (generally) not an indication that L2C failed to\n"
        "    complete transactions. Rather, it is a hint of\n"
        "    possible performance degradation. (For example, L2C\n"
        "    must read-modify-write DRAM for every transaction\n"
        "    that updates some, but not all, of the bytes in a\n"
        "    cache block, misses in the L2 cache, and cannot\n"
        "    allocate a WAY.) There is one \"failure\" case where\n"
        "    L2C will set NOWAY: when it cannot leave a block\n"
        "    locked in the L2 cache as part of a LCKL2\n"
        "    transaction.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_ERR_TTGX(0);
    info.status_mask        = 1ull<<62 /* sbe */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<16 /* tad0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_ERR_TTGX(0)[SBE]: Single-Bit ECC error\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_ERR_TTGX(0);
    info.status_mask        = 1ull<<63 /* dbe */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<16 /* tad0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_ERR_TTGX(0)[DBE]: Double-Bit ECC error\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_L2C_TADX_INT(3) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_TADX_INT(3);
    info.status_mask        = 1ull<<0 /* l2dsbe */;
    info.enable_addr        = CVMX_L2C_TADX_IEN(3);
    info.enable_mask        = 1ull<<0 /* l2dsbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<19 /* tad3 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_TADX_INT(3)[L2DSBE]: L2D Single-Bit Error\n"
        "    Shadow copy of L2C_ERR_TDTX[SBE]\n"
        "    Writes of 1 also clear L2C_ERR_TDTX[SBE]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_TADX_INT(3);
    info.status_mask        = 1ull<<1 /* l2ddbe */;
    info.enable_addr        = CVMX_L2C_TADX_IEN(3);
    info.enable_mask        = 1ull<<1 /* l2ddbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<19 /* tad3 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_TADX_INT(3)[L2DDBE]: L2D Double-Bit Error\n"
        "    Shadow copy of L2C_ERR_TDTX[DBE]\n"
        "    Writes of 1 also clear L2C_ERR_TDTX[DBE]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_TADX_INT(3);
    info.status_mask        = 1ull<<2 /* tagsbe */;
    info.enable_addr        = CVMX_L2C_TADX_IEN(3);
    info.enable_mask        = 1ull<<2 /* tagsbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<19 /* tad3 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_TADX_INT(3)[TAGSBE]: TAG Single-Bit Error\n"
        "    Shadow copy of L2C_ERR_TTGX[SBE]\n"
        "    Writes of 1 also clear L2C_ERR_TTGX[SBE]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_TADX_INT(3);
    info.status_mask        = 1ull<<3 /* tagdbe */;
    info.enable_addr        = CVMX_L2C_TADX_IEN(3);
    info.enable_mask        = 1ull<<3 /* tagdbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<19 /* tad3 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_TADX_INT(3)[TAGDBE]: TAG Double-Bit Error\n"
        "    Shadow copy of L2C_ERR_TTGX[DBE]\n"
        "    Writes of 1 also clear L2C_ERR_TTGX[DBE]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_TADX_INT(3);
    info.status_mask        = 1ull<<4 /* vbfsbe */;
    info.enable_addr        = CVMX_L2C_TADX_IEN(3);
    info.enable_mask        = 1ull<<4 /* vbfsbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<19 /* tad3 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_TADX_INT(3)[VBFSBE]: VBF Single-Bit Error\n"
        "    Shadow copy of L2C_ERR_TDTX[VSBE]\n"
        "    Writes of 1 also clear L2C_ERR_TDTX[VSBE]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_TADX_INT(3);
    info.status_mask        = 1ull<<5 /* vbfdbe */;
    info.enable_addr        = CVMX_L2C_TADX_IEN(3);
    info.enable_mask        = 1ull<<5 /* vbfdbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<19 /* tad3 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_TADX_INT(3)[VBFDBE]: VBF Double-Bit Error\n"
        "    Shadow copy of L2C_ERR_TDTX[VDBE]\n"
        "    Writes of 1 also clear L2C_ERR_TDTX[VDBE]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_TADX_INT(3);
    info.status_mask        = 1ull<<6 /* noway */;
    info.enable_addr        = CVMX_L2C_TADX_IEN(3);
    info.enable_mask        = 1ull<<6 /* noway */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<19 /* tad3 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_TADX_INT(3)[NOWAY]: No way available interrupt\n"
        "    Shadow copy of L2C_ERR_TTGX[NOWAY]\n"
        "    Writes of 1 also clear L2C_ERR_TTGX[NOWAY]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_TADX_INT(3);
    info.status_mask        = 1ull<<7 /* rddislmc */;
    info.enable_addr        = CVMX_L2C_TADX_IEN(3);
    info.enable_mask        = 1ull<<7 /* rddislmc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<19 /* tad3 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_TADX_INT(3)[RDDISLMC]: Illegal Read  to Disabled LMC Error\n"
        "    A DRAM read  arrived before the LMC(s) were enabled\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_TADX_INT(3);
    info.status_mask        = 1ull<<8 /* wrdislmc */;
    info.enable_addr        = CVMX_L2C_TADX_IEN(3);
    info.enable_mask        = 1ull<<8 /* wrdislmc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<19 /* tad3 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_TADX_INT(3)[WRDISLMC]: Illegal Write to Disabled LMC Error\n"
        "    A DRAM write arrived before the LMC(s) were enabled\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_L2C_ERR_TDTX(3) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_ERR_TDTX(3);
    info.status_mask        = 1ull<<60 /* vsbe */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<19 /* tad3 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_ERR_TDTX(3)[VSBE]: VBF Single-Bit error has occurred\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_ERR_TDTX(3);
    info.status_mask        = 1ull<<61 /* vdbe */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<19 /* tad3 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_ERR_TDTX(3)[VDBE]: VBF Double-Bit error has occurred\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_ERR_TDTX(3);
    info.status_mask        = 1ull<<62 /* sbe */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<19 /* tad3 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_ERR_TDTX(3)[SBE]: L2D Single-Bit error has occurred\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_ERR_TDTX(3);
    info.status_mask        = 1ull<<63 /* dbe */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<19 /* tad3 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_ERR_TDTX(3)[DBE]: L2D Double-Bit error has occurred\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_L2C_ERR_TTGX(3) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_ERR_TTGX(3);
    info.status_mask        = 1ull<<61 /* noway */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<19 /* tad3 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_ERR_TTGX(3)[NOWAY]: No way was available for allocation.\n"
        "    L2C sets NOWAY during its processing of a\n"
        "    transaction whenever it needed/wanted to allocate\n"
        "    a WAY in the L2 cache, but was unable to. NOWAY==1\n"
        "    is (generally) not an indication that L2C failed to\n"
        "    complete transactions. Rather, it is a hint of\n"
        "    possible performance degradation. (For example, L2C\n"
        "    must read-modify-write DRAM for every transaction\n"
        "    that updates some, but not all, of the bytes in a\n"
        "    cache block, misses in the L2 cache, and cannot\n"
        "    allocate a WAY.) There is one \"failure\" case where\n"
        "    L2C will set NOWAY: when it cannot leave a block\n"
        "    locked in the L2 cache as part of a LCKL2\n"
        "    transaction.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_ERR_TTGX(3);
    info.status_mask        = 1ull<<62 /* sbe */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<19 /* tad3 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_ERR_TTGX(3)[SBE]: Single-Bit ECC error\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_ERR_TTGX(3);
    info.status_mask        = 1ull<<63 /* dbe */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<19 /* tad3 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_ERR_TTGX(3)[DBE]: Double-Bit ECC error\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_L2C_TADX_INT(2) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_TADX_INT(2);
    info.status_mask        = 1ull<<0 /* l2dsbe */;
    info.enable_addr        = CVMX_L2C_TADX_IEN(2);
    info.enable_mask        = 1ull<<0 /* l2dsbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<18 /* tad2 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_TADX_INT(2)[L2DSBE]: L2D Single-Bit Error\n"
        "    Shadow copy of L2C_ERR_TDTX[SBE]\n"
        "    Writes of 1 also clear L2C_ERR_TDTX[SBE]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_TADX_INT(2);
    info.status_mask        = 1ull<<1 /* l2ddbe */;
    info.enable_addr        = CVMX_L2C_TADX_IEN(2);
    info.enable_mask        = 1ull<<1 /* l2ddbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<18 /* tad2 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_TADX_INT(2)[L2DDBE]: L2D Double-Bit Error\n"
        "    Shadow copy of L2C_ERR_TDTX[DBE]\n"
        "    Writes of 1 also clear L2C_ERR_TDTX[DBE]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_TADX_INT(2);
    info.status_mask        = 1ull<<2 /* tagsbe */;
    info.enable_addr        = CVMX_L2C_TADX_IEN(2);
    info.enable_mask        = 1ull<<2 /* tagsbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<18 /* tad2 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_TADX_INT(2)[TAGSBE]: TAG Single-Bit Error\n"
        "    Shadow copy of L2C_ERR_TTGX[SBE]\n"
        "    Writes of 1 also clear L2C_ERR_TTGX[SBE]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_TADX_INT(2);
    info.status_mask        = 1ull<<3 /* tagdbe */;
    info.enable_addr        = CVMX_L2C_TADX_IEN(2);
    info.enable_mask        = 1ull<<3 /* tagdbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<18 /* tad2 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_TADX_INT(2)[TAGDBE]: TAG Double-Bit Error\n"
        "    Shadow copy of L2C_ERR_TTGX[DBE]\n"
        "    Writes of 1 also clear L2C_ERR_TTGX[DBE]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_TADX_INT(2);
    info.status_mask        = 1ull<<4 /* vbfsbe */;
    info.enable_addr        = CVMX_L2C_TADX_IEN(2);
    info.enable_mask        = 1ull<<4 /* vbfsbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<18 /* tad2 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_TADX_INT(2)[VBFSBE]: VBF Single-Bit Error\n"
        "    Shadow copy of L2C_ERR_TDTX[VSBE]\n"
        "    Writes of 1 also clear L2C_ERR_TDTX[VSBE]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_TADX_INT(2);
    info.status_mask        = 1ull<<5 /* vbfdbe */;
    info.enable_addr        = CVMX_L2C_TADX_IEN(2);
    info.enable_mask        = 1ull<<5 /* vbfdbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<18 /* tad2 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_TADX_INT(2)[VBFDBE]: VBF Double-Bit Error\n"
        "    Shadow copy of L2C_ERR_TDTX[VDBE]\n"
        "    Writes of 1 also clear L2C_ERR_TDTX[VDBE]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_TADX_INT(2);
    info.status_mask        = 1ull<<6 /* noway */;
    info.enable_addr        = CVMX_L2C_TADX_IEN(2);
    info.enable_mask        = 1ull<<6 /* noway */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<18 /* tad2 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_TADX_INT(2)[NOWAY]: No way available interrupt\n"
        "    Shadow copy of L2C_ERR_TTGX[NOWAY]\n"
        "    Writes of 1 also clear L2C_ERR_TTGX[NOWAY]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_TADX_INT(2);
    info.status_mask        = 1ull<<7 /* rddislmc */;
    info.enable_addr        = CVMX_L2C_TADX_IEN(2);
    info.enable_mask        = 1ull<<7 /* rddislmc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<18 /* tad2 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_TADX_INT(2)[RDDISLMC]: Illegal Read  to Disabled LMC Error\n"
        "    A DRAM read  arrived before the LMC(s) were enabled\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_TADX_INT(2);
    info.status_mask        = 1ull<<8 /* wrdislmc */;
    info.enable_addr        = CVMX_L2C_TADX_IEN(2);
    info.enable_mask        = 1ull<<8 /* wrdislmc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<18 /* tad2 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_TADX_INT(2)[WRDISLMC]: Illegal Write to Disabled LMC Error\n"
        "    A DRAM write arrived before the LMC(s) were enabled\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_L2C_ERR_TDTX(2) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_ERR_TDTX(2);
    info.status_mask        = 1ull<<60 /* vsbe */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<18 /* tad2 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_ERR_TDTX(2)[VSBE]: VBF Single-Bit error has occurred\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_ERR_TDTX(2);
    info.status_mask        = 1ull<<61 /* vdbe */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<18 /* tad2 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_ERR_TDTX(2)[VDBE]: VBF Double-Bit error has occurred\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_ERR_TDTX(2);
    info.status_mask        = 1ull<<62 /* sbe */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<18 /* tad2 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_ERR_TDTX(2)[SBE]: L2D Single-Bit error has occurred\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_ERR_TDTX(2);
    info.status_mask        = 1ull<<63 /* dbe */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<18 /* tad2 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_ERR_TDTX(2)[DBE]: L2D Double-Bit error has occurred\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_L2C_ERR_TTGX(2) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_ERR_TTGX(2);
    info.status_mask        = 1ull<<61 /* noway */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<18 /* tad2 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_ERR_TTGX(2)[NOWAY]: No way was available for allocation.\n"
        "    L2C sets NOWAY during its processing of a\n"
        "    transaction whenever it needed/wanted to allocate\n"
        "    a WAY in the L2 cache, but was unable to. NOWAY==1\n"
        "    is (generally) not an indication that L2C failed to\n"
        "    complete transactions. Rather, it is a hint of\n"
        "    possible performance degradation. (For example, L2C\n"
        "    must read-modify-write DRAM for every transaction\n"
        "    that updates some, but not all, of the bytes in a\n"
        "    cache block, misses in the L2 cache, and cannot\n"
        "    allocate a WAY.) There is one \"failure\" case where\n"
        "    L2C will set NOWAY: when it cannot leave a block\n"
        "    locked in the L2 cache as part of a LCKL2\n"
        "    transaction.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_ERR_TTGX(2);
    info.status_mask        = 1ull<<62 /* sbe */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<18 /* tad2 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_ERR_TTGX(2)[SBE]: Single-Bit ECC error\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_L2C_ERR_TTGX(2);
    info.status_mask        = 1ull<<63 /* dbe */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_L2C_INT_REG;
    info.parent.status_mask = 1ull<<18 /* tad2 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR L2C_ERR_TTGX(2)[DBE]: Double-Bit ECC error\n";
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[Q7_PERR]: Set when a Queue0 pointer read from the stack in\n"
        "    the L2C does not have the FPA owner ship bit set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<28 /* pool0th */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<28 /* pool0th */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[POOL0TH]: Set when FPA_QUE0_AVAILABLE is equal to\n"
        "    FPA_POOL0_THRESHOLD[THRESH] and a pointer is\n"
        "    allocated or de-allocated.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<29 /* pool1th */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<29 /* pool1th */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[POOL1TH]: Set when FPA_QUE1_AVAILABLE is equal to\n"
        "    FPA_POOL1_THRESHOLD[THRESH] and a pointer is\n"
        "    allocated or de-allocated.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<30 /* pool2th */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<30 /* pool2th */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[POOL2TH]: Set when FPA_QUE2_AVAILABLE is equal to\n"
        "    FPA_POOL2_THRESHOLD[THRESH] and a pointer is\n"
        "    allocated or de-allocated.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<31 /* pool3th */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<31 /* pool3th */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[POOL3TH]: Set when FPA_QUE3_AVAILABLE is equal to\n"
        "    FPA_POOL3_THRESHOLD[THRESH] and a pointer is\n"
        "    allocated or de-allocated.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<32 /* pool4th */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<32 /* pool4th */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[POOL4TH]: Set when FPA_QUE4_AVAILABLE is equal to\n"
        "    FPA_POOL4_THRESHOLD[THRESH] and a pointer is\n"
        "    allocated or de-allocated.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<33 /* pool5th */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<33 /* pool5th */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[POOL5TH]: Set when FPA_QUE5_AVAILABLE is equal to\n"
        "    FPA_POOL5_THRESHOLD[THRESH] and a pointer is\n"
        "    allocated or de-allocated.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<34 /* pool6th */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<34 /* pool6th */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[POOL6TH]: Set when FPA_QUE6_AVAILABLE is equal to\n"
        "    FPA_POOL6_THRESHOLD[THRESH] and a pointer is\n"
        "    allocated or de-allocated.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<35 /* pool7th */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<35 /* pool7th */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[POOL7TH]: Set when FPA_QUE7_AVAILABLE is equal to\n"
        "    FPA_POOL7_THRESHOLD[THRESH] and a pointer is\n"
        "    allocated or de-allocated.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<36 /* free0 */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<36 /* free0 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[FREE0]: When a pointer for POOL0 is freed bit is set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<37 /* free1 */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<37 /* free1 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[FREE1]: When a pointer for POOL1 is freed bit is set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<38 /* free2 */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<38 /* free2 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[FREE2]: When a pointer for POOL2 is freed bit is set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<39 /* free3 */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<39 /* free3 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[FREE3]: When a pointer for POOL3 is freed bit is set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<40 /* free4 */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<40 /* free4 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[FREE4]: When a pointer for POOL4 is freed bit is set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<41 /* free5 */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<41 /* free5 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[FREE5]: When a pointer for POOL5 is freed bit is set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<42 /* free6 */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<42 /* free6 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[FREE6]: When a pointer for POOL6 is freed bit is set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<43 /* free7 */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<43 /* free7 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[FREE7]: When a pointer for POOL7 is freed bit is set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<44 /* free8 */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<44 /* free8 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[FREE8]: When a pointer for POOL8 is freed bit is set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<45 /* q8_und */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<45 /* q8_und */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[Q8_UND]: Set when a Queue8 page count available goes\n"
        "    negative.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<46 /* q8_coff */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<46 /* q8_coff */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[Q8_COFF]: Set when a Queue8 stack end tag is present and\n"
        "    the count available is greater than than pointers\n"
        "    present in the FPA.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<47 /* q8_perr */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<47 /* q8_perr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[Q8_PERR]: Set when a Queue8 pointer read from the stack in\n"
        "    the L2C does not have the FPA owner ship bit set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<48 /* pool8th */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<48 /* pool8th */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[POOL8TH]: Set when FPA_QUE8_AVAILABLE is equal to\n"
        "    FPA_POOL8_THRESHOLD[THRESH] and a pointer is\n"
        "    allocated or de-allocated.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_FPA_INT_SUM;
    info.status_mask        = 1ull<<49 /* paddr_e */;
    info.enable_addr        = CVMX_FPA_INT_ENB;
    info.enable_mask        = 1ull<<49 /* paddr_e */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<4 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[PADDR_E]: Set when a pointer address does not fall in the\n"
        "    address range for a pool specified by\n"
        "    FPA_POOLX_START_ADDR and FPA_POOLX_END_ADDR.\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_ZIP_ERROR */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_ZIP_ERROR;
    info.status_mask        = 1ull<<0 /* doorbell */;
    info.enable_addr        = CVMX_ZIP_INT_MASK;
    info.enable_mask        = 1ull<<0 /* doorbell */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<24 /* zip */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ZIP_ERROR[DOORBELL]: A doorbell count has overflowed\n";
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<5 /* ipd */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR IPD_INT_SUM[PRC_PAR0]: Set when a parity error is dected for bits\n"
        "    [31:0] of the PBM memory.\n"
        "    NOT USED ON o68.\n";
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<5 /* ipd */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR IPD_INT_SUM[PRC_PAR1]: Set when a parity error is dected for bits\n"
        "    [63:32] of the PBM memory.\n"
        "    NOT USED ON o68.\n";
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<5 /* ipd */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR IPD_INT_SUM[PRC_PAR2]: Set when a parity error is dected for bits\n"
        "    [95:64] of the PBM memory.\n"
        "    NOT USED ON o68.\n";
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<5 /* ipd */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR IPD_INT_SUM[PRC_PAR3]: Set when a parity error is dected for bits\n"
        "    [127:96] of the PBM memory.\n"
        "    NOT USED ON o68.\n";
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<5 /* ipd */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<5 /* ipd */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<5 /* ipd */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<5 /* ipd */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<5 /* ipd */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<5 /* ipd */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR IPD_INT_SUM[BC_OVR]: Set when the byte-count to send to IOB overflows.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_IPD_INT_SUM;
    info.status_mask        = 1ull<<12 /* sop */;
    info.enable_addr        = CVMX_IPD_INT_ENB;
    info.enable_mask        = 1ull<<12 /* sop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<5 /* ipd */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR IPD_INT_SUM[SOP]: Set when a SOP is followed by an SOP for the same\n"
        "    reasm-id for a packet.\n"
        "    The first detected error associated with bits [14:12]\n"
        "    of this register will only be set here. A new bit\n"
        "    can be set when the previous reported bit is cleared.\n"
        "    Also see IPD_PKT_ERR.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_IPD_INT_SUM;
    info.status_mask        = 1ull<<13 /* eop */;
    info.enable_addr        = CVMX_IPD_INT_ENB;
    info.enable_mask        = 1ull<<13 /* eop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<5 /* ipd */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR IPD_INT_SUM[EOP]: Set when a EOP is followed by an EOP for the same\n"
        "    reasm-id for a packet.\n"
        "    The first detected error associated with bits [14:12]\n"
        "    of this register will only be set here. A new bit\n"
        "    can be set when the previous reported bit is cleared.\n"
        "    Also see IPD_PKT_ERR.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_IPD_INT_SUM;
    info.status_mask        = 1ull<<14 /* dat */;
    info.enable_addr        = CVMX_IPD_INT_ENB;
    info.enable_mask        = 1ull<<14 /* dat */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<5 /* ipd */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR IPD_INT_SUM[DAT]: Set when a data arrives before a SOP for the same\n"
        "    reasm-id for a packet.\n"
        "    The first detected error associated with bits [14:12]\n"
        "    of this register will only be set here. A new bit\n"
        "    can be set when the previous reported bit is cleared.\n"
        "    Also see IPD_PKT_ERR.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_IPD_INT_SUM;
    info.status_mask        = 1ull<<15 /* pw0_sbe */;
    info.enable_addr        = CVMX_IPD_INT_ENB;
    info.enable_mask        = 1ull<<15 /* pw0_sbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<5 /* ipd */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR IPD_INT_SUM[PW0_SBE]: Packet memory 0 had ECC SBE.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_IPD_INT_SUM;
    info.status_mask        = 1ull<<16 /* pw0_dbe */;
    info.enable_addr        = CVMX_IPD_INT_ENB;
    info.enable_mask        = 1ull<<16 /* pw0_dbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<5 /* ipd */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR IPD_INT_SUM[PW0_DBE]: Packet memory 0 had ECC DBE.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_IPD_INT_SUM;
    info.status_mask        = 1ull<<17 /* pw1_sbe */;
    info.enable_addr        = CVMX_IPD_INT_ENB;
    info.enable_mask        = 1ull<<17 /* pw1_sbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<5 /* ipd */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR IPD_INT_SUM[PW1_SBE]: Packet memory 1 had ECC SBE.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_IPD_INT_SUM;
    info.status_mask        = 1ull<<18 /* pw1_dbe */;
    info.enable_addr        = CVMX_IPD_INT_ENB;
    info.enable_mask        = 1ull<<18 /* pw1_dbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<5 /* ipd */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR IPD_INT_SUM[PW1_DBE]: Packet memory 1 had ECC DBE.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_IPD_INT_SUM;
    info.status_mask        = 1ull<<19 /* pw2_sbe */;
    info.enable_addr        = CVMX_IPD_INT_ENB;
    info.enable_mask        = 1ull<<19 /* pw2_sbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<5 /* ipd */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR IPD_INT_SUM[PW2_SBE]: Packet memory 2 had ECC SBE.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_IPD_INT_SUM;
    info.status_mask        = 1ull<<20 /* pw2_dbe */;
    info.enable_addr        = CVMX_IPD_INT_ENB;
    info.enable_mask        = 1ull<<20 /* pw2_dbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<5 /* ipd */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR IPD_INT_SUM[PW2_DBE]: Packet memory 2 had ECC DBE.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_IPD_INT_SUM;
    info.status_mask        = 1ull<<21 /* pw3_sbe */;
    info.enable_addr        = CVMX_IPD_INT_ENB;
    info.enable_mask        = 1ull<<21 /* pw3_sbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<5 /* ipd */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR IPD_INT_SUM[PW3_SBE]: Packet memory 3 had ECC SBE.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_IPD_INT_SUM;
    info.status_mask        = 1ull<<22 /* pw3_dbe */;
    info.enable_addr        = CVMX_IPD_INT_ENB;
    info.enable_mask        = 1ull<<22 /* pw3_dbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<5 /* ipd */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR IPD_INT_SUM[PW3_DBE]: Packet memory 3 had ECC DBE.\n";
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<29 /* rad */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR RAD_REG_ERROR[DOORBELL]: A doorbell count has overflowed\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_SSO_ERR */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SSO_ERR;
    info.status_mask        = 0x7ffull<<32 /* iop */;
    info.enable_addr        = CVMX_SSO_ERR_ENB;
    info.enable_mask        = 0x7ffull<<32 /* iop_ie */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<16 /* sso */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SSO_ERR[IOP]: Illegal operation errors\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SSO_ERR;
    info.status_mask        = 1ull<<1 /* fidx_dbe */;
    info.enable_addr        = CVMX_SSO_ERR_ENB;
    info.enable_mask        = 1ull<<1 /* fidx_dbe_ie */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<16 /* sso */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SSO_ERR[FIDX_DBE]: Double bit error for FIDX RAM\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SSO_ERR;
    info.status_mask        = 1ull<<2 /* idx_sbe */;
    info.enable_addr        = CVMX_SSO_ERR_ENB;
    info.enable_mask        = 1ull<<2 /* idx_sbe_ie */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<16 /* sso */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SSO_ERR[IDX_SBE]: Single bit error for IDX RAM\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SSO_ERR;
    info.status_mask        = 1ull<<11 /* pnd_dbe0 */;
    info.enable_addr        = CVMX_SSO_ERR_ENB;
    info.enable_mask        = 1ull<<11 /* pnd_dbe0_ie */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<16 /* sso */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SSO_ERR[PND_DBE0]: Double bit error for even PND RAM\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SSO_ERR;
    info.status_mask        = 1ull<<4 /* oth_sbe1 */;
    info.enable_addr        = CVMX_SSO_ERR_ENB;
    info.enable_mask        = 1ull<<4 /* oth_sbe1_ie */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<16 /* sso */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SSO_ERR[OTH_SBE1]: Single bit error for odd OTH RAM\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SSO_ERR;
    info.status_mask        = 1ull<<5 /* oth_dbe1 */;
    info.enable_addr        = CVMX_SSO_ERR_ENB;
    info.enable_mask        = 1ull<<5 /* oth_dbe1_ie */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<16 /* sso */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SSO_ERR[OTH_DBE1]: Double bit error for odd OTH RAM\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SSO_ERR;
    info.status_mask        = 1ull<<6 /* oth_sbe0 */;
    info.enable_addr        = CVMX_SSO_ERR_ENB;
    info.enable_mask        = 1ull<<6 /* oth_sbe0_ie */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<16 /* sso */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SSO_ERR[OTH_SBE0]: Single bit error for even OTH RAM\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SSO_ERR;
    info.status_mask        = 1ull<<7 /* oth_dbe0 */;
    info.enable_addr        = CVMX_SSO_ERR_ENB;
    info.enable_mask        = 1ull<<7 /* oth_dbe0_ie */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<16 /* sso */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SSO_ERR[OTH_DBE0]: Double bit error for even OTH RAM\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SSO_ERR;
    info.status_mask        = 1ull<<8 /* pnd_sbe1 */;
    info.enable_addr        = CVMX_SSO_ERR_ENB;
    info.enable_mask        = 1ull<<8 /* pnd_sbe1_ie */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<16 /* sso */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SSO_ERR[PND_SBE1]: Single bit error for odd PND RAM\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SSO_ERR;
    info.status_mask        = 1ull<<9 /* pnd_dbe1 */;
    info.enable_addr        = CVMX_SSO_ERR_ENB;
    info.enable_mask        = 1ull<<9 /* pnd_dbe1_ie */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<16 /* sso */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SSO_ERR[PND_DBE1]: Double bit error for odd PND RAM\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SSO_ERR;
    info.status_mask        = 1ull<<10 /* pnd_sbe0 */;
    info.enable_addr        = CVMX_SSO_ERR_ENB;
    info.enable_mask        = 1ull<<10 /* pnd_sbe0_ie */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<16 /* sso */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SSO_ERR[PND_SBE0]: Single bit error for even PND RAM\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SSO_ERR;
    info.status_mask        = 1ull<<45 /* fpe */;
    info.enable_addr        = CVMX_SSO_ERR_ENB;
    info.enable_mask        = 1ull<<45 /* fpe_ie */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<16 /* sso */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SSO_ERR[FPE]: Free page error\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SSO_ERR;
    info.status_mask        = 1ull<<46 /* awe */;
    info.enable_addr        = CVMX_SSO_ERR_ENB;
    info.enable_mask        = 1ull<<46 /* awe_ie */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<16 /* sso */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SSO_ERR[AWE]: Out-of-memory error (ADDWQ Request is dropped)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SSO_ERR;
    info.status_mask        = 1ull<<47 /* bfp */;
    info.enable_addr        = CVMX_SSO_ERR_ENB;
    info.enable_mask        = 1ull<<47 /* bfp_ie */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<16 /* sso */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SSO_ERR[BFP]: Bad Fill Packet error\n"
        "    Last byte of the fill packet did not match 8'h1a\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SSO_ERR;
    info.status_mask        = 1ull<<3 /* idx_dbe */;
    info.enable_addr        = CVMX_SSO_ERR_ENB;
    info.enable_mask        = 1ull<<3 /* idx_dbe_ie */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<16 /* sso */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SSO_ERR[IDX_DBE]: Double bit error for IDX RAM\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SSO_ERR;
    info.status_mask        = 1ull<<0 /* fidx_sbe */;
    info.enable_addr        = CVMX_SSO_ERR_ENB;
    info.enable_mask        = 1ull<<0 /* fidx_sbe_ie */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<16 /* sso */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SSO_ERR[FIDX_SBE]: Single bit error for FIDX RAM\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_PEXP_SLI_INT_SUM */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_SLI_INT_SUM;
    info.status_mask        = 1ull<<0 /* rml_to */;
    info.enable_addr        = CVMX_PEXP_SLI_INT_ENB_CIU;
    info.enable_mask        = 1ull<<0 /* rml_to */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<32 /* sli */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_SLI_INT_SUM[RML_TO]: A read or write transfer did not complete\n"
        "    within 0xffff core clocks.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_SLI_INT_SUM;
    info.status_mask        = 1ull<<1 /* reserved_1_1 */;
    info.enable_addr        = CVMX_PEXP_SLI_INT_ENB_CIU;
    info.enable_mask        = 1ull<<1 /* reserved_1_1 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<32 /* sli */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_SLI_INT_SUM[RESERVED_1_1]: Error Bit\n"
;
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_SLI_INT_SUM;
    info.status_mask        = 1ull<<2 /* bar0_to */;
    info.enable_addr        = CVMX_PEXP_SLI_INT_ENB_CIU;
    info.enable_mask        = 1ull<<2 /* bar0_to */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<32 /* sli */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_SLI_INT_SUM[BAR0_TO]: BAR0 R/W to a NCB device did not receive\n"
        "    read-data/commit in 0xffff core clocks.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_SLI_INT_SUM;
    info.status_mask        = 1ull<<3 /* iob2big */;
    info.enable_addr        = CVMX_PEXP_SLI_INT_ENB_CIU;
    info.enable_mask        = 1ull<<3 /* iob2big */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<32 /* sli */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_SLI_INT_SUM[IOB2BIG]: A requested IOBDMA is to large.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_SLI_INT_SUM;
    info.status_mask        = 0x3ull<<6 /* reserved_6_7 */;
    info.enable_addr        = CVMX_PEXP_SLI_INT_ENB_CIU;
    info.enable_mask        = 0x3ull<<6 /* reserved_6_7 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<32 /* sli */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_SLI_INT_SUM[RESERVED_6_7]: Error Bit\n"
;
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_SLI_INT_SUM;
    info.status_mask        = 1ull<<8 /* m0_up_b0 */;
    info.enable_addr        = CVMX_PEXP_SLI_INT_ENB_CIU;
    info.enable_mask        = 1ull<<8 /* m0_up_b0 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<32 /* sli */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_SLI_INT_SUM[M0_UP_B0]: Received Unsupported P-TLP for Bar0 from MAC 0.\n"
        "    This occurs when the BAR 0 address space is\n"
        "    disabeled.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_SLI_INT_SUM;
    info.status_mask        = 1ull<<9 /* m0_up_wi */;
    info.enable_addr        = CVMX_PEXP_SLI_INT_ENB_CIU;
    info.enable_mask        = 1ull<<9 /* m0_up_wi */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<32 /* sli */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_SLI_INT_SUM[M0_UP_WI]: Received Unsupported P-TLP for Window Register\n"
        "    from MAC 0. This occurs when the window registers\n"
        "    are disabeld and a window register access occurs.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_SLI_INT_SUM;
    info.status_mask        = 1ull<<10 /* m0_un_b0 */;
    info.enable_addr        = CVMX_PEXP_SLI_INT_ENB_CIU;
    info.enable_mask        = 1ull<<10 /* m0_un_b0 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<32 /* sli */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_SLI_INT_SUM[M0_UN_B0]: Received Unsupported N-TLP for Bar0 from MAC 0.\n"
        "    This occurs when the BAR 0 address space is\n"
        "    disabeled.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_SLI_INT_SUM;
    info.status_mask        = 1ull<<11 /* m0_un_wi */;
    info.enable_addr        = CVMX_PEXP_SLI_INT_ENB_CIU;
    info.enable_mask        = 1ull<<11 /* m0_un_wi */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<32 /* sli */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_SLI_INT_SUM[M0_UN_WI]: Received Unsupported N-TLP for Window Register\n"
        "    from MAC 0. This occurs when the window registers\n"
        "    are disabeld and a window register access occurs.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_SLI_INT_SUM;
    info.status_mask        = 1ull<<12 /* m1_up_b0 */;
    info.enable_addr        = CVMX_PEXP_SLI_INT_ENB_CIU;
    info.enable_mask        = 1ull<<12 /* m1_up_b0 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<32 /* sli */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_SLI_INT_SUM[M1_UP_B0]: Received Unsupported P-TLP for Bar0 from MAC 1.\n"
        "    This occurs when the BAR 0 address space is\n"
        "    disabeled.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_SLI_INT_SUM;
    info.status_mask        = 1ull<<13 /* m1_up_wi */;
    info.enable_addr        = CVMX_PEXP_SLI_INT_ENB_CIU;
    info.enable_mask        = 1ull<<13 /* m1_up_wi */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<32 /* sli */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_SLI_INT_SUM[M1_UP_WI]: Received Unsupported P-TLP for Window Register\n"
        "    from MAC 1. This occurs when the window registers\n"
        "    are disabeld and a window register access occurs.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_SLI_INT_SUM;
    info.status_mask        = 1ull<<14 /* m1_un_b0 */;
    info.enable_addr        = CVMX_PEXP_SLI_INT_ENB_CIU;
    info.enable_mask        = 1ull<<14 /* m1_un_b0 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<32 /* sli */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_SLI_INT_SUM[M1_UN_B0]: Received Unsupported N-TLP for Bar0 from MAC 1.\n"
        "    This occurs when the BAR 0 address space is\n"
        "    disabeled.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_SLI_INT_SUM;
    info.status_mask        = 1ull<<15 /* m1_un_wi */;
    info.enable_addr        = CVMX_PEXP_SLI_INT_ENB_CIU;
    info.enable_mask        = 1ull<<15 /* m1_un_wi */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<32 /* sli */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_SLI_INT_SUM[M1_UN_WI]: Received Unsupported N-TLP for Window Register\n"
        "    from MAC 1. This occurs when the window registers\n"
        "    are disabeld and a window register access occurs.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_SLI_INT_SUM;
    info.status_mask        = 1ull<<48 /* pidbof */;
    info.enable_addr        = CVMX_PEXP_SLI_INT_ENB_CIU;
    info.enable_mask        = 1ull<<48 /* pidbof */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<32 /* sli */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_SLI_INT_SUM[PIDBOF]: Packet Instruction Doorbell count overflowed. Which\n"
        "    doorbell can be found in DPI_PINT_INFO[PIDBOF]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_SLI_INT_SUM;
    info.status_mask        = 1ull<<49 /* psldbof */;
    info.enable_addr        = CVMX_PEXP_SLI_INT_ENB_CIU;
    info.enable_mask        = 1ull<<49 /* psldbof */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<32 /* sli */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_SLI_INT_SUM[PSLDBOF]: Packet Scatterlist Doorbell count overflowed. Which\n"
        "    doorbell can be found in DPI_PINT_INFO[PSLDBOF]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_SLI_INT_SUM;
    info.status_mask        = 1ull<<50 /* pout_err */;
    info.enable_addr        = CVMX_PEXP_SLI_INT_ENB_CIU;
    info.enable_mask        = 1ull<<50 /* pout_err */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<32 /* sli */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_SLI_INT_SUM[POUT_ERR]: Set when PKO sends packet data with the error bit\n"
        "    set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_SLI_INT_SUM;
    info.status_mask        = 1ull<<52 /* pgl_err */;
    info.enable_addr        = CVMX_PEXP_SLI_INT_ENB_CIU;
    info.enable_mask        = 1ull<<52 /* pgl_err */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<32 /* sli */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_SLI_INT_SUM[PGL_ERR]: When a read error occurs on a packet gather list\n"
        "    read this bit is set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_SLI_INT_SUM;
    info.status_mask        = 1ull<<53 /* pdi_err */;
    info.enable_addr        = CVMX_PEXP_SLI_INT_ENB_CIU;
    info.enable_mask        = 1ull<<53 /* pdi_err */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<32 /* sli */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_SLI_INT_SUM[PDI_ERR]: When a read error occurs on a packet data read\n"
        "    this bit is set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_SLI_INT_SUM;
    info.status_mask        = 1ull<<54 /* pop_err */;
    info.enable_addr        = CVMX_PEXP_SLI_INT_ENB_CIU;
    info.enable_mask        = 1ull<<54 /* pop_err */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<32 /* sli */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_SLI_INT_SUM[POP_ERR]: When a read error occurs on a packet scatter\n"
        "    pointer pair this bit is set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_SLI_INT_SUM;
    info.status_mask        = 1ull<<55 /* pins_err */;
    info.enable_addr        = CVMX_PEXP_SLI_INT_ENB_CIU;
    info.enable_mask        = 1ull<<55 /* pins_err */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<32 /* sli */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_SLI_INT_SUM[PINS_ERR]: When a read error occurs on a packet instruction\n"
        "    this bit is set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_SLI_INT_SUM;
    info.status_mask        = 1ull<<56 /* sprt0_err */;
    info.enable_addr        = CVMX_PEXP_SLI_INT_ENB_CIU;
    info.enable_mask        = 1ull<<56 /* sprt0_err */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<32 /* sli */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_SLI_INT_SUM[SPRT0_ERR]: When an error response received on SLI port 0\n"
        "    this bit is set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_SLI_INT_SUM;
    info.status_mask        = 1ull<<57 /* sprt1_err */;
    info.enable_addr        = CVMX_PEXP_SLI_INT_ENB_CIU;
    info.enable_mask        = 1ull<<57 /* sprt1_err */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<32 /* sli */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_SLI_INT_SUM[SPRT1_ERR]: When an error response received on SLI port 1\n"
        "    this bit is set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_SLI_INT_SUM;
    info.status_mask        = 1ull<<60 /* ill_pad */;
    info.enable_addr        = CVMX_PEXP_SLI_INT_ENB_CIU;
    info.enable_mask        = 1ull<<60 /* ill_pad */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<32 /* sli */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_SLI_INT_SUM[ILL_PAD]: Set when a BAR0 address R/W falls into theaddress\n"
        "    range of the Packet-CSR, but for an unused\n"
        "    address.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_SLI_INT_SUM;
    info.status_mask        = 1ull<<61 /* pipe_err */;
    info.enable_addr        = CVMX_PEXP_SLI_INT_ENB_CIU;
    info.enable_mask        = 1ull<<61 /* pipe_err */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<32 /* sli */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_SLI_INT_SUM[PIPE_ERR]: Set when a PIPE value outside range is received.\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_KEY_INT_SUM */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_KEY_INT_SUM;
    info.status_mask        = 1ull<<0 /* ked0_sbe */;
    info.enable_addr        = CVMX_KEY_INT_ENB;
    info.enable_mask        = 1ull<<0 /* ked0_sbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<30 /* key */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR KEY_INT_SUM[KED0_SBE]: Error Bit\n"
;
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_KEY_INT_SUM;
    info.status_mask        = 1ull<<1 /* ked0_dbe */;
    info.enable_addr        = CVMX_KEY_INT_ENB;
    info.enable_mask        = 1ull<<1 /* ked0_dbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<30 /* key */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR KEY_INT_SUM[KED0_DBE]: Error Bit\n"
;
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_KEY_INT_SUM;
    info.status_mask        = 1ull<<2 /* ked1_sbe */;
    info.enable_addr        = CVMX_KEY_INT_ENB;
    info.enable_mask        = 1ull<<2 /* ked1_sbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<30 /* key */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR KEY_INT_SUM[KED1_SBE]: Error Bit\n"
;
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_KEY_INT_SUM;
    info.status_mask        = 1ull<<3 /* ked1_dbe */;
    info.enable_addr        = CVMX_KEY_INT_ENB;
    info.enable_mask        = 1ull<<3 /* ked1_dbe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<30 /* key */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR KEY_INT_SUM[KED1_DBE]: Error Bit\n"
;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<6 /* pip */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<6 /* pip */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<6 /* pip */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<6 /* pip */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PIP_INT_REG[TODOOVR]: Todo list overflow (see PIP_BCK_PRS[HIWATER])\n";
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<6 /* pip */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<6 /* pip */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<6 /* pip */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PIP_INT_REG[PUNYERR]: Frame was received with length <=4B when CRC\n"
        "    stripping in IPD is enable\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_DFA_ERROR */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_DFA_ERROR;
    info.status_mask        = 1ull<<0 /* dblovf */;
    info.enable_addr        = CVMX_DFA_INTMSK;
    info.enable_mask        = 1ull<<0 /* dblina */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<40 /* dfa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR DFA_ERROR[DBLOVF]: Doorbell Overflow detected - Status bit\n"
        "    When set, the 20b accumulated doorbell register\n"
        "    had overflowed (SW wrote too many doorbell requests).\n"
        "    If the DBLINA had previously been enabled(set),\n"
        "    an interrupt will be posted. Software can clear\n"
        "    the interrupt by writing a 1 to this register bit.\n"
        "    NOTE: Detection of a Doorbell Register overflow\n"
        "    is a catastrophic error which may leave the DFA\n"
        "    HW in an unrecoverable state.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_DFA_ERROR;
    info.status_mask        = 0x7ull<<1 /* dc0perr */;
    info.enable_addr        = CVMX_DFA_INTMSK;
    info.enable_mask        = 0x7ull<<1 /* dc0pena */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<40 /* dfa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR DFA_ERROR[DC0PERR]: Cluster#0 RAM[3:1] Parity Error Detected\n"
        "    See also DFA_DTCFADR register which contains the\n"
        "    failing addresses for the internal node cache RAMs.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_DFA_ERROR;
    info.status_mask        = 0x7ull<<4 /* dc1perr */;
    info.enable_addr        = CVMX_DFA_INTMSK;
    info.enable_mask        = 0x7ull<<4 /* dc1pena */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<40 /* dfa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR DFA_ERROR[DC1PERR]: Cluster#1 RAM[3:1] Parity Error Detected\n"
        "    See also DFA_DTCFADR register which contains the\n"
        "    failing addresses for the internal node cache RAMs.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_DFA_ERROR;
    info.status_mask        = 0x7ull<<7 /* dc2perr */;
    info.enable_addr        = CVMX_DFA_INTMSK;
    info.enable_mask        = 0x7ull<<7 /* dc2pena */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<40 /* dfa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR DFA_ERROR[DC2PERR]: Cluster#2 RAM[3:1] Parity Error Detected\n"
        "    See also DFA_DTCFADR register which contains the\n"
        "    failing addresses for the internal node cache RAMs.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_DFA_ERROR;
    info.status_mask        = 1ull<<13 /* dlc0_ovferr */;
    info.enable_addr        = CVMX_DFA_INTMSK;
    info.enable_mask        = 1ull<<13 /* dlc0_ovfena */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<40 /* dfa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR DFA_ERROR[DLC0_OVFERR]: DLC0 Fifo Overflow Error Detected\n"
        "    This condition should NEVER architecturally occur, and\n"
        "    is here in case HW credit/debit scheme is not working.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_DFA_ERROR;
    info.status_mask        = 1ull<<14 /* dlc1_ovferr */;
    info.enable_addr        = CVMX_DFA_INTMSK;
    info.enable_mask        = 1ull<<14 /* dlc1_ovfena */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<40 /* dfa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR DFA_ERROR[DLC1_OVFERR]: DLC1 Fifo Overflow Error Detected\n"
        "    This condition should NEVER architecturally occur, and\n"
        "    is here in case HW credit/debit scheme is not working.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_DFA_ERROR;
    info.status_mask        = 1ull<<17 /* dfanxm */;
    info.enable_addr        = CVMX_DFA_INTMSK;
    info.enable_mask        = 1ull<<17 /* dfanxmena */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<40 /* dfa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR DFA_ERROR[DFANXM]: DFA Non-existent Memory Access\n"
        "    For o68: DTEs (and backdoor CSR DFA Memory REGION reads)\n"
        "    have access to the following 38bit L2/DRAM address space\n"
        "    which maps to a 37bit physical DDR3 SDRAM address space.\n"
        "    see:\n"
        "    DR0: 0x0 0000 0000 0000 to 0x0 0000 0FFF FFFF\n"
        "            maps to lower 256MB of physical DDR3 SDRAM\n"
        "    DR1: 0x0 0000 2000 0000 to 0x0 0020 0FFF FFFF\n"
        "            maps to upper 127.75GB of DDR3 SDRAM\n"
        "               L2/DRAM address space                     Physical DDR3 SDRAM Address space\n"
        "                 (38bit address)                           (37bit address)\n"
        "                  +-----------+ 0x0020.0FFF.FFFF\n"
        "                  |\n"
        "                 ===   DR1   ===                          +-----------+ 0x001F.FFFF.FFFF\n"
        "     (128GB-256MB)|           |                           |\n"
        "                  |           |                     =>    |           |  (128GB-256MB)\n"
        "                  +-----------+ 0x0000.1FFF.FFFF          |   DR1\n"
        "          256MB   |   HOLE    |   (DO NOT USE)            |\n"
        "                  +-----------+ 0x0000.0FFF.FFFF          +-----------+ 0x0000.0FFF.FFFF\n"
        "          256MB   |    DR0    |                           |   DR0     |   (256MB)\n"
        "                  +-----------+ 0x0000.0000.0000          +-----------+ 0x0000.0000.0000\n"
        "    In the event the DFA generates a reference to the L2/DRAM\n"
        "    address hole (0x0000.0FFF.FFFF - 0x0000.1FFF.FFFF) or to\n"
        "    an address above 0x0020.0FFF.FFFF, the DFANXM programmable\n"
        "    interrupt bit will be set.\n"
        "    SWNOTE: Both the 1) SW DFA Graph compiler and the 2) SW NCB-Direct CSR\n"
        "    accesses to DFA Memory REGION MUST avoid making references\n"
        "    to these non-existent memory regions.\n"
        "    NOTE: If DFANXM is set during a DFA Graph Walk operation,\n"
        "    then the walk will prematurely terminate with RWORD0[REA]=ERR.\n"
        "    If DFANXM is set during a NCB-Direct CSR read access to DFA\n"
        "    Memory REGION, then the CSR read response data is forced to\n"
        "    128'hBADE_FEED_DEAD_BEEF_FACE_CAFE_BEAD_C0DE. (NOTE: the QW\n"
        "    being accessed, either the upper or lower QW will be returned).\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_DFA_ERROR;
    info.status_mask        = 1ull<<18 /* replerr */;
    info.enable_addr        = CVMX_DFA_INTMSK;
    info.enable_mask        = 1ull<<18 /* replerrena */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<40 /* dfa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR DFA_ERROR[REPLERR]: DFA Illegal Replication Factor Error\n"
        "    For o68: DFA only supports 1x, 2x, and 4x port replication.\n"
        "    Legal configurations for memory are to support 2 port or\n"
        "    4 port configurations.\n"
        "    The REPLERR interrupt will be set in the following illegal\n"
        "    configuration cases:\n"
        "        1) An 8x replication factor is detected for any memory reference.\n"
        "        2) A 4x replication factor is detected for any memory reference\n"
        "           when only 2 memory ports are enabled.\n"
        "    NOTE: If REPLERR is set during a DFA Graph Walk operation,\n"
        "    then the walk will prematurely terminate with RWORD0[REA]=ERR.\n"
        "    If REPLERR is set during a NCB-Direct CSR read access to DFA\n"
        "    Memory REGION, then the CSR read response data is UNPREDICTABLE.\n";
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<7 /* pko */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<7 /* pko */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<7 /* pko */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PKO_REG_ERROR[CURRZERO]: A packet data pointer has size=0\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PKO_REG_ERROR;
    info.status_mask        = 1ull<<3 /* loopback */;
    info.enable_addr        = CVMX_PKO_REG_INT_MASK;
    info.enable_mask        = 1ull<<3 /* loopback */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<7 /* pko */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PKO_REG_ERROR[LOOPBACK]: A packet was sent to an illegal loopback port\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_DPI_INT_REG */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_DPI_INT_REG;
    info.status_mask        = 1ull<<0 /* nderr */;
    info.enable_addr        = CVMX_DPI_INT_EN;
    info.enable_mask        = 1ull<<0 /* nderr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<33 /* dpi */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR DPI_INT_REG[NDERR]: NCB Decode Error\n"
        "    DPI received a NCB transaction on the outbound\n"
        "    bus to the DPI deviceID, but the command was not\n"
        "    recognized.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_DPI_INT_REG;
    info.status_mask        = 1ull<<1 /* nfovr */;
    info.enable_addr        = CVMX_DPI_INT_EN;
    info.enable_mask        = 1ull<<1 /* nfovr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<33 /* dpi */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR DPI_INT_REG[NFOVR]: CSR Fifo Overflow\n"
        "    DPI can store upto 16 CSR request.  The FIFO will\n"
        "    overflow if that number is exceeded.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_DPI_INT_REG;
    info.status_mask        = 0xffull<<8 /* dmadbo */;
    info.enable_addr        = CVMX_DPI_INT_EN;
    info.enable_mask        = 0xffull<<8 /* dmadbo */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<33 /* dpi */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR DPI_INT_REG[DMADBO]: DMAx doorbell overflow.\n"
        "    DPI has a 32-bit counter for each request's queue\n"
        "    outstanding doorbell counts. Interrupt will fire\n"
        "    if the count overflows.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_DPI_INT_REG;
    info.status_mask        = 1ull<<16 /* req_badadr */;
    info.enable_addr        = CVMX_DPI_INT_EN;
    info.enable_mask        = 1ull<<16 /* req_badadr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<33 /* dpi */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR DPI_INT_REG[REQ_BADADR]: DMA instruction fetch with bad pointer\n"
        "    Interrupt will fire if DPI forms an instruction\n"
        "    fetch to the NULL pointer.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_DPI_INT_REG;
    info.status_mask        = 1ull<<17 /* req_badlen */;
    info.enable_addr        = CVMX_DPI_INT_EN;
    info.enable_mask        = 1ull<<17 /* req_badlen */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<33 /* dpi */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR DPI_INT_REG[REQ_BADLEN]: DMA instruction fetch with length\n"
        "    Interrupt will fire if DPI forms an instruction\n"
        "    fetch with length of zero.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_DPI_INT_REG;
    info.status_mask        = 1ull<<18 /* req_ovrflw */;
    info.enable_addr        = CVMX_DPI_INT_EN;
    info.enable_mask        = 1ull<<18 /* req_ovrflw */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<33 /* dpi */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR DPI_INT_REG[REQ_OVRFLW]: DMA instruction FIFO overflow\n"
        "    DPI tracks outstanding instructions fetches.\n"
        "    Interrupt will fire when FIFO overflows.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_DPI_INT_REG;
    info.status_mask        = 1ull<<19 /* req_undflw */;
    info.enable_addr        = CVMX_DPI_INT_EN;
    info.enable_mask        = 1ull<<19 /* req_undflw */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<33 /* dpi */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR DPI_INT_REG[REQ_UNDFLW]: DMA instruction FIFO underflow\n"
        "    DPI tracks outstanding instructions fetches.\n"
        "    Interrupt will fire when FIFO underflows.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_DPI_INT_REG;
    info.status_mask        = 1ull<<20 /* req_anull */;
    info.enable_addr        = CVMX_DPI_INT_EN;
    info.enable_mask        = 1ull<<20 /* req_anull */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<33 /* dpi */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR DPI_INT_REG[REQ_ANULL]: DMA instruction filled with bad instruction\n"
        "    Fetched instruction word was 0.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_DPI_INT_REG;
    info.status_mask        = 1ull<<21 /* req_inull */;
    info.enable_addr        = CVMX_DPI_INT_EN;
    info.enable_mask        = 1ull<<21 /* req_inull */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<33 /* dpi */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR DPI_INT_REG[REQ_INULL]: DMA instruction filled with NULL pointer\n"
        "    Next pointer was NULL.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_DPI_INT_REG;
    info.status_mask        = 1ull<<22 /* req_badfil */;
    info.enable_addr        = CVMX_DPI_INT_EN;
    info.enable_mask        = 1ull<<22 /* req_badfil */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<33 /* dpi */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR DPI_INT_REG[REQ_BADFIL]: DMA instruction unexpected fill\n"
        "    Instruction fill when none outstanding.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_DPI_INT_REG;
    info.status_mask        = 1ull<<24 /* sprt0_rst */;
    info.enable_addr        = CVMX_DPI_INT_EN;
    info.enable_mask        = 1ull<<24 /* sprt0_rst */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<33 /* dpi */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR DPI_INT_REG[SPRT0_RST]: DMA instruction was dropped because the source or\n"
        "     destination port was in reset.\n"
        "    this bit is set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_DPI_INT_REG;
    info.status_mask        = 1ull<<25 /* sprt1_rst */;
    info.enable_addr        = CVMX_DPI_INT_EN;
    info.enable_mask        = 1ull<<25 /* sprt1_rst */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<33 /* dpi */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR DPI_INT_REG[SPRT1_RST]: DMA instruction was dropped because the source or\n"
        "     destination port was in reset.\n"
        "    this bit is set.\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_DPI_PKT_ERR_RSP */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_DPI_PKT_ERR_RSP;
    info.status_mask        = 1ull<<0 /* pkterr */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<33 /* dpi */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR DPI_PKT_ERR_RSP[PKTERR]: Indicates that an ErrorResponse was received from\n"
        "    the I/O subsystem.\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_DPI_REQ_ERR_RSP */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_DPI_REQ_ERR_RSP;
    info.status_mask        = 0xffull<<0 /* qerr */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<33 /* dpi */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR DPI_REQ_ERR_RSP[QERR]: Indicates which instruction queue received an\n"
        "    ErrorResponse from the I/O subsystem.\n"
        "    SW must clear the bit before the the cooresponding\n"
        "    instruction queue will continue processing\n"
        "    instructions if DPI_REQ_ERR_RSP_EN[EN] is set.\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_DPI_REQ_ERR_RST */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_DPI_REQ_ERR_RST;
    info.status_mask        = 0xffull<<0 /* qerr */;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_RML(0);
    info.parent.status_mask = 1ull<<33 /* dpi */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR DPI_REQ_ERR_RST[QERR]: Indicates which instruction queue dropped an\n"
        "    instruction because the source or destination\n"
        "    was in reset.\n"
        "    SW must clear the bit before the the cooresponding\n"
        "    instruction queue will continue processing\n"
        "    instructions if DPI_REQ_ERR_RST_EN[EN] is set.\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_CIU2_SRC_PPX_IP2_MIO(0) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_CIU2_SRC_PPX_IP2_MIO(0);
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

    /* CVMX_MIO_RST_INT */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_MIO_RST_INT;
    info.status_mask        = 1ull<<0 /* rst_link0 */;
    info.enable_addr        = CVMX_MIO_RST_INT_EN;
    info.enable_mask        = 1ull<<0 /* rst_link0 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_MIO(0);
    info.parent.status_mask = 1ull<<63 /* rst */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR MIO_RST_INT[RST_LINK0]: A controller0 link-down/hot-reset occurred while\n"
        "    MIO_RST_CTL0[RST_LINK]=0.  Software must assert\n"
        "    then de-assert CIU_SOFT_PRST[SOFT_PRST]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_MIO_RST_INT;
    info.status_mask        = 1ull<<1 /* rst_link1 */;
    info.enable_addr        = CVMX_MIO_RST_INT_EN;
    info.enable_mask        = 1ull<<1 /* rst_link1 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_MIO(0);
    info.parent.status_mask = 1ull<<63 /* rst */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR MIO_RST_INT[RST_LINK1]: A controller1 link-down/hot-reset occurred while\n"
        "    MIO_RST_CTL1[RST_LINK]=0.  Software must assert\n"
        "    then de-assert CIU_SOFT_PRST1[SOFT_PRST]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_MIO_RST_INT;
    info.status_mask        = 1ull<<8 /* perst0 */;
    info.enable_addr        = CVMX_MIO_RST_INT_EN;
    info.enable_mask        = 1ull<<8 /* perst0 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_MIO(0);
    info.parent.status_mask = 1ull<<63 /* rst */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR MIO_RST_INT[PERST0]: PERST0_L asserted while MIO_RST_CTL0[RST_RCV]=1\n"
        "    and MIO_RST_CTL0[RST_CHIP]=0\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_MIO_RST_INT;
    info.status_mask        = 1ull<<9 /* perst1 */;
    info.enable_addr        = CVMX_MIO_RST_INT_EN;
    info.enable_mask        = 1ull<<9 /* perst1 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_MIO(0);
    info.parent.status_mask = 1ull<<63 /* rst */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR MIO_RST_INT[PERST1]: PERST1_L asserted while MIO_RST_CTL1[RST_RCV]=1\n"
        "    and MIO_RST_CTL1[RST_CHIP]=0\n";
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_MIO(0);
    info.parent.status_mask = 1ull<<16 /* nand */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_MIO(0);
    info.parent.status_mask = 1ull<<16 /* nand */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_MIO(0);
    info.parent.status_mask = 1ull<<16 /* nand */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_MIO(0);
    info.parent.status_mask = 1ull<<16 /* nand */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_MIO(0);
    info.parent.status_mask = 1ull<<16 /* nand */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR NDF_INT[OVRF]: NDF_CMD write when fifo is full. Generally a\n"
        "    fatal error.\n";
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_MIO(0);
    info.parent.status_mask = 1ull<<17 /* mio */;
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
    info.parent.status_addr = CVMX_CIU2_SRC_PPX_IP2_MIO(0);
    info.parent.status_mask = 1ull<<17 /* mio */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR MIO_BOOT_ERR[WAIT_ERR]: Wait mode error\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_CIU2_SUM_PPX_IP2(0) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_CIU2_SUM_PPX_IP2(0);
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

    /* CVMX_LMCX_INT(0) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_LMCX_INT(0);
    info.status_mask        = 0xfull<<1 /* sec_err */;
    info.enable_addr        = CVMX_LMCX_INT_EN(0);
    info.enable_mask        = 1ull<<1 /* intr_sec_ena */;
    info.flags              = CVMX_ERROR_FLAGS_ECC_SINGLE_BIT;
    info.group              = CVMX_ERROR_GROUP_LMC;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<5 /* mem */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR LMCX_INT(0)[SEC_ERR]: Single Error (corrected) of Rd Data\n"
        "    [0] corresponds to DQ[63:0]_c0_p0\n"
        "    [1] corresponds to DQ[63:0]_c0_p1\n"
        "    [2] corresponds to DQ[63:0]_c1_p0\n"
        "    [3] corresponds to DQ[63:0]_c1_p1\n"
        "    where _cC_pP denotes cycle C and phase P\n"
        "    Write of 1 will clear the corresponding error bit\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_LMCX_INT(0);
    info.status_mask        = 1ull<<0 /* nxm_wr_err */;
    info.enable_addr        = CVMX_LMCX_INT_EN(0);
    info.enable_mask        = 1ull<<0 /* intr_nxm_wr_ena */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_LMC;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<5 /* mem */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR LMCX_INT(0)[NXM_WR_ERR]: Write to non-existent memory\n"
        "    Write of 1 will clear the corresponding error bit\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_LMCX_INT(0);
    info.status_mask        = 0xfull<<5 /* ded_err */;
    info.enable_addr        = CVMX_LMCX_INT_EN(0);
    info.enable_mask        = 1ull<<2 /* intr_ded_ena */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_LMC;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<5 /* mem */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR LMCX_INT(0)[DED_ERR]: Double Error detected (DED) of Rd Data\n"
        "    [0] corresponds to DQ[63:0]_c0_p0\n"
        "    [1] corresponds to DQ[63:0]_c0_p1\n"
        "    [2] corresponds to DQ[63:0]_c1_p0\n"
        "    [3] corresponds to DQ[63:0]_c1_p1\n"
        "    where _cC_pP denotes cycle C and phase P\n"
        "    Write of 1 will clear the corresponding error bit\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_LMCX_INT(1) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_LMCX_INT(1);
    info.status_mask        = 0xfull<<1 /* sec_err */;
    info.enable_addr        = CVMX_LMCX_INT_EN(1);
    info.enable_mask        = 1ull<<1 /* intr_sec_ena */;
    info.flags              = CVMX_ERROR_FLAGS_ECC_SINGLE_BIT;
    info.group              = CVMX_ERROR_GROUP_LMC;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<5 /* mem */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR LMCX_INT(1)[SEC_ERR]: Single Error (corrected) of Rd Data\n"
        "    [0] corresponds to DQ[63:0]_c0_p0\n"
        "    [1] corresponds to DQ[63:0]_c0_p1\n"
        "    [2] corresponds to DQ[63:0]_c1_p0\n"
        "    [3] corresponds to DQ[63:0]_c1_p1\n"
        "    where _cC_pP denotes cycle C and phase P\n"
        "    Write of 1 will clear the corresponding error bit\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_LMCX_INT(1);
    info.status_mask        = 1ull<<0 /* nxm_wr_err */;
    info.enable_addr        = CVMX_LMCX_INT_EN(1);
    info.enable_mask        = 1ull<<0 /* intr_nxm_wr_ena */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_LMC;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<5 /* mem */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR LMCX_INT(1)[NXM_WR_ERR]: Write to non-existent memory\n"
        "    Write of 1 will clear the corresponding error bit\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_LMCX_INT(1);
    info.status_mask        = 0xfull<<5 /* ded_err */;
    info.enable_addr        = CVMX_LMCX_INT_EN(1);
    info.enable_mask        = 1ull<<2 /* intr_ded_ena */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_LMC;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<5 /* mem */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR LMCX_INT(1)[DED_ERR]: Double Error detected (DED) of Rd Data\n"
        "    [0] corresponds to DQ[63:0]_c0_p0\n"
        "    [1] corresponds to DQ[63:0]_c0_p1\n"
        "    [2] corresponds to DQ[63:0]_c1_p0\n"
        "    [3] corresponds to DQ[63:0]_c1_p1\n"
        "    where _cC_pP denotes cycle C and phase P\n"
        "    Write of 1 will clear the corresponding error bit\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_LMCX_INT(2) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_LMCX_INT(2);
    info.status_mask        = 0xfull<<1 /* sec_err */;
    info.enable_addr        = CVMX_LMCX_INT_EN(2);
    info.enable_mask        = 1ull<<1 /* intr_sec_ena */;
    info.flags              = CVMX_ERROR_FLAGS_ECC_SINGLE_BIT;
    info.group              = CVMX_ERROR_GROUP_LMC;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<5 /* mem */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR LMCX_INT(2)[SEC_ERR]: Single Error (corrected) of Rd Data\n"
        "    [0] corresponds to DQ[63:0]_c0_p0\n"
        "    [1] corresponds to DQ[63:0]_c0_p1\n"
        "    [2] corresponds to DQ[63:0]_c1_p0\n"
        "    [3] corresponds to DQ[63:0]_c1_p1\n"
        "    where _cC_pP denotes cycle C and phase P\n"
        "    Write of 1 will clear the corresponding error bit\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_LMCX_INT(2);
    info.status_mask        = 1ull<<0 /* nxm_wr_err */;
    info.enable_addr        = CVMX_LMCX_INT_EN(2);
    info.enable_mask        = 1ull<<0 /* intr_nxm_wr_ena */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_LMC;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<5 /* mem */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR LMCX_INT(2)[NXM_WR_ERR]: Write to non-existent memory\n"
        "    Write of 1 will clear the corresponding error bit\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_LMCX_INT(2);
    info.status_mask        = 0xfull<<5 /* ded_err */;
    info.enable_addr        = CVMX_LMCX_INT_EN(2);
    info.enable_mask        = 1ull<<2 /* intr_ded_ena */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_LMC;
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<5 /* mem */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR LMCX_INT(2)[DED_ERR]: Double Error detected (DED) of Rd Data\n"
        "    [0] corresponds to DQ[63:0]_c0_p0\n"
        "    [1] corresponds to DQ[63:0]_c0_p1\n"
        "    [2] corresponds to DQ[63:0]_c1_p0\n"
        "    [3] corresponds to DQ[63:0]_c1_p1\n"
        "    where _cC_pP denotes cycle C and phase P\n"
        "    Write of 1 will clear the corresponding error bit\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_LMCX_INT(3) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_LMCX_INT(3);
    info.status_mask        = 0xfull<<1 /* sec_err */;
    info.enable_addr        = CVMX_LMCX_INT_EN(3);
    info.enable_mask        = 1ull<<1 /* intr_sec_ena */;
    info.flags              = CVMX_ERROR_FLAGS_ECC_SINGLE_BIT;
    info.group              = CVMX_ERROR_GROUP_LMC;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<5 /* mem */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR LMCX_INT(3)[SEC_ERR]: Single Error (corrected) of Rd Data\n"
        "    [0] corresponds to DQ[63:0]_c0_p0\n"
        "    [1] corresponds to DQ[63:0]_c0_p1\n"
        "    [2] corresponds to DQ[63:0]_c1_p0\n"
        "    [3] corresponds to DQ[63:0]_c1_p1\n"
        "    where _cC_pP denotes cycle C and phase P\n"
        "    Write of 1 will clear the corresponding error bit\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_LMCX_INT(3);
    info.status_mask        = 1ull<<0 /* nxm_wr_err */;
    info.enable_addr        = CVMX_LMCX_INT_EN(3);
    info.enable_mask        = 1ull<<0 /* intr_nxm_wr_ena */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_LMC;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<5 /* mem */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR LMCX_INT(3)[NXM_WR_ERR]: Write to non-existent memory\n"
        "    Write of 1 will clear the corresponding error bit\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_LMCX_INT(3);
    info.status_mask        = 0xfull<<5 /* ded_err */;
    info.enable_addr        = CVMX_LMCX_INT_EN(3);
    info.enable_mask        = 1ull<<2 /* intr_ded_ena */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_LMC;
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<5 /* mem */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR LMCX_INT(3)[DED_ERR]: Double Error detected (DED) of Rd Data\n"
        "    [0] corresponds to DQ[63:0]_c0_p0\n"
        "    [1] corresponds to DQ[63:0]_c0_p1\n"
        "    [2] corresponds to DQ[63:0]_c1_p0\n"
        "    [3] corresponds to DQ[63:0]_c1_p1\n"
        "    where _cC_pP denotes cycle C and phase P\n"
        "    Write of 1 will clear the corresponding error bit\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_GMXX_RXX_INT_REG(0,0) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,0);
    info.status_mask        = 1ull<<1 /* carext */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,0);
    info.enable_mask        = 1ull<<1 /* carext */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2048;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2048;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2048;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2048;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,0)[LOC_FAULT]: Local Fault Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,0);
    info.status_mask        = 1ull<<21 /* rem_fault */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,0);
    info.enable_mask        = 1ull<<21 /* rem_fault */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2048;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,0)[REM_FAULT]: Remote Fault Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,0);
    info.status_mask        = 1ull<<22 /* bad_seq */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,0);
    info.enable_mask        = 1ull<<22 /* bad_seq */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2048;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,0)[BAD_SEQ]: Reserved Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,0);
    info.status_mask        = 1ull<<23 /* bad_term */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,0);
    info.enable_mask        = 1ull<<23 /* bad_term */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2048;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,0)[BAD_TERM]: Frame is terminated by control character other\n"
        "    than /T/.  The error propagation control\n"
        "    character /E/ will be included as part of the\n"
        "    frame and does not cause a frame termination.\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,0);
    info.status_mask        = 1ull<<24 /* unsop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,0);
    info.enable_mask        = 1ull<<24 /* unsop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2048;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,0)[UNSOP]: Unexpected SOP\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,0);
    info.status_mask        = 1ull<<25 /* uneop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,0);
    info.enable_mask        = 1ull<<25 /* uneop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2048;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,0)[UNEOP]: Unexpected EOP\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,0);
    info.status_mask        = 1ull<<26 /* undat */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,0);
    info.enable_mask        = 1ull<<26 /* undat */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2048;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,0)[UNDAT]: Unexpected Data\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,0);
    info.status_mask        = 1ull<<27 /* hg2fld */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,0);
    info.enable_mask        = 1ull<<27 /* hg2fld */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2048;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2048;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2064;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2064;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2064;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2064;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,0)[LOC_FAULT]: Local Fault Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,0);
    info.status_mask        = 1ull<<21 /* rem_fault */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,0);
    info.enable_mask        = 1ull<<21 /* rem_fault */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2064;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,0)[REM_FAULT]: Remote Fault Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,0);
    info.status_mask        = 1ull<<22 /* bad_seq */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,0);
    info.enable_mask        = 1ull<<22 /* bad_seq */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2064;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,0)[BAD_SEQ]: Reserved Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,0);
    info.status_mask        = 1ull<<23 /* bad_term */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,0);
    info.enable_mask        = 1ull<<23 /* bad_term */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2064;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,0)[BAD_TERM]: Frame is terminated by control character other\n"
        "    than /T/.  The error propagation control\n"
        "    character /E/ will be included as part of the\n"
        "    frame and does not cause a frame termination.\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,0);
    info.status_mask        = 1ull<<24 /* unsop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,0);
    info.enable_mask        = 1ull<<24 /* unsop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2064;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,0)[UNSOP]: Unexpected SOP\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,0);
    info.status_mask        = 1ull<<25 /* uneop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,0);
    info.enable_mask        = 1ull<<25 /* uneop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2064;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,0)[UNEOP]: Unexpected EOP\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,0);
    info.status_mask        = 1ull<<26 /* undat */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,0);
    info.enable_mask        = 1ull<<26 /* undat */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2064;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,0)[UNDAT]: Unexpected Data\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,0);
    info.status_mask        = 1ull<<27 /* hg2fld */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,0);
    info.enable_mask        = 1ull<<27 /* hg2fld */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2064;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2064;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2080;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2080;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2080;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2080;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,0)[LOC_FAULT]: Local Fault Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,0);
    info.status_mask        = 1ull<<21 /* rem_fault */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,0);
    info.enable_mask        = 1ull<<21 /* rem_fault */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2080;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,0)[REM_FAULT]: Remote Fault Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,0);
    info.status_mask        = 1ull<<22 /* bad_seq */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,0);
    info.enable_mask        = 1ull<<22 /* bad_seq */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2080;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,0)[BAD_SEQ]: Reserved Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,0);
    info.status_mask        = 1ull<<23 /* bad_term */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,0);
    info.enable_mask        = 1ull<<23 /* bad_term */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2080;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,0)[BAD_TERM]: Frame is terminated by control character other\n"
        "    than /T/.  The error propagation control\n"
        "    character /E/ will be included as part of the\n"
        "    frame and does not cause a frame termination.\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,0);
    info.status_mask        = 1ull<<24 /* unsop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,0);
    info.enable_mask        = 1ull<<24 /* unsop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2080;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,0)[UNSOP]: Unexpected SOP\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,0);
    info.status_mask        = 1ull<<25 /* uneop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,0);
    info.enable_mask        = 1ull<<25 /* uneop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2080;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,0)[UNEOP]: Unexpected EOP\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,0);
    info.status_mask        = 1ull<<26 /* undat */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,0);
    info.enable_mask        = 1ull<<26 /* undat */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2080;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,0)[UNDAT]: Unexpected Data\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,0);
    info.status_mask        = 1ull<<27 /* hg2fld */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,0);
    info.enable_mask        = 1ull<<27 /* hg2fld */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2080;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2080;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2096;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2096;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2096;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2096;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,0)[LOC_FAULT]: Local Fault Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,0);
    info.status_mask        = 1ull<<21 /* rem_fault */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,0);
    info.enable_mask        = 1ull<<21 /* rem_fault */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2096;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,0)[REM_FAULT]: Remote Fault Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,0);
    info.status_mask        = 1ull<<22 /* bad_seq */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,0);
    info.enable_mask        = 1ull<<22 /* bad_seq */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2096;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,0)[BAD_SEQ]: Reserved Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,0);
    info.status_mask        = 1ull<<23 /* bad_term */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,0);
    info.enable_mask        = 1ull<<23 /* bad_term */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2096;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,0)[BAD_TERM]: Frame is terminated by control character other\n"
        "    than /T/.  The error propagation control\n"
        "    character /E/ will be included as part of the\n"
        "    frame and does not cause a frame termination.\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,0);
    info.status_mask        = 1ull<<24 /* unsop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,0);
    info.enable_mask        = 1ull<<24 /* unsop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2096;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,0)[UNSOP]: Unexpected SOP\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,0);
    info.status_mask        = 1ull<<25 /* uneop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,0);
    info.enable_mask        = 1ull<<25 /* uneop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2096;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,0)[UNEOP]: Unexpected EOP\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,0);
    info.status_mask        = 1ull<<26 /* undat */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,0);
    info.enable_mask        = 1ull<<26 /* undat */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2096;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,0)[UNDAT]: Unexpected Data\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,0);
    info.status_mask        = 1ull<<27 /* hg2fld */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,0);
    info.enable_mask        = 1ull<<27 /* hg2fld */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2096;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2096;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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

    /* CVMX_GMXX_RXX_INT_REG(0,1) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,1);
    info.status_mask        = 1ull<<1 /* carext */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,1);
    info.enable_mask        = 1ull<<1 /* carext */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2368;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,1)[CAREXT]: Carrier extend error\n"
        "    (SGMII/1000Base-X only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,1);
    info.status_mask        = 1ull<<8 /* skperr */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,1);
    info.enable_mask        = 1ull<<8 /* skperr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2368;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,1)[SKPERR]: Skipper error\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,1);
    info.status_mask        = 1ull<<10 /* ovrerr */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,1);
    info.enable_mask        = 1ull<<10 /* ovrerr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2368;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,1)[OVRERR]: Internal Data Aggregation Overflow\n"
        "    This interrupt should never assert\n"
        "    (SGMII/1000Base-X only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,1);
    info.status_mask        = 1ull<<20 /* loc_fault */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,1);
    info.enable_mask        = 1ull<<20 /* loc_fault */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2368;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,1)[LOC_FAULT]: Local Fault Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,1);
    info.status_mask        = 1ull<<21 /* rem_fault */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,1);
    info.enable_mask        = 1ull<<21 /* rem_fault */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2368;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,1)[REM_FAULT]: Remote Fault Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,1);
    info.status_mask        = 1ull<<22 /* bad_seq */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,1);
    info.enable_mask        = 1ull<<22 /* bad_seq */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2368;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,1)[BAD_SEQ]: Reserved Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,1);
    info.status_mask        = 1ull<<23 /* bad_term */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,1);
    info.enable_mask        = 1ull<<23 /* bad_term */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2368;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,1)[BAD_TERM]: Frame is terminated by control character other\n"
        "    than /T/.  The error propagation control\n"
        "    character /E/ will be included as part of the\n"
        "    frame and does not cause a frame termination.\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,1);
    info.status_mask        = 1ull<<24 /* unsop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,1);
    info.enable_mask        = 1ull<<24 /* unsop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2368;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,1)[UNSOP]: Unexpected SOP\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,1);
    info.status_mask        = 1ull<<25 /* uneop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,1);
    info.enable_mask        = 1ull<<25 /* uneop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2368;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,1)[UNEOP]: Unexpected EOP\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,1);
    info.status_mask        = 1ull<<26 /* undat */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,1);
    info.enable_mask        = 1ull<<26 /* undat */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2368;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,1)[UNDAT]: Unexpected Data\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,1);
    info.status_mask        = 1ull<<27 /* hg2fld */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,1);
    info.enable_mask        = 1ull<<27 /* hg2fld */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2368;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,1)[HG2FLD]: HiGig2 received message field error, as below\n"
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
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,1);
    info.status_mask        = 1ull<<28 /* hg2cc */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,1);
    info.enable_mask        = 1ull<<28 /* hg2cc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2368;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,1)[HG2CC]: HiGig2 received message CRC or Control char  error\n"
        "    Set when either CRC8 error detected or when\n"
        "    a Control Character is found in the message\n"
        "    bytes after the K.SOM\n"
        "    NOTE: HG2CC has higher priority than HG2FLD\n"
        "          i.e. a HiGig2 message that results in HG2CC\n"
        "          getting set, will never set HG2FLD.\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_GMXX_RXX_INT_REG(0,2) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,2);
    info.status_mask        = 1ull<<1 /* carext */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,2);
    info.enable_mask        = 1ull<<1 /* carext */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2560;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,2)[CAREXT]: Carrier extend error\n"
        "    (SGMII/1000Base-X only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,2);
    info.status_mask        = 1ull<<8 /* skperr */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,2);
    info.enable_mask        = 1ull<<8 /* skperr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2560;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,2)[SKPERR]: Skipper error\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,2);
    info.status_mask        = 1ull<<10 /* ovrerr */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,2);
    info.enable_mask        = 1ull<<10 /* ovrerr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2560;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,2)[OVRERR]: Internal Data Aggregation Overflow\n"
        "    This interrupt should never assert\n"
        "    (SGMII/1000Base-X only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,2);
    info.status_mask        = 1ull<<20 /* loc_fault */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,2);
    info.enable_mask        = 1ull<<20 /* loc_fault */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2560;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,2)[LOC_FAULT]: Local Fault Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,2);
    info.status_mask        = 1ull<<21 /* rem_fault */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,2);
    info.enable_mask        = 1ull<<21 /* rem_fault */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2560;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,2)[REM_FAULT]: Remote Fault Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,2);
    info.status_mask        = 1ull<<22 /* bad_seq */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,2);
    info.enable_mask        = 1ull<<22 /* bad_seq */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2560;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,2)[BAD_SEQ]: Reserved Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,2);
    info.status_mask        = 1ull<<23 /* bad_term */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,2);
    info.enable_mask        = 1ull<<23 /* bad_term */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2560;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,2)[BAD_TERM]: Frame is terminated by control character other\n"
        "    than /T/.  The error propagation control\n"
        "    character /E/ will be included as part of the\n"
        "    frame and does not cause a frame termination.\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,2);
    info.status_mask        = 1ull<<24 /* unsop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,2);
    info.enable_mask        = 1ull<<24 /* unsop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2560;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,2)[UNSOP]: Unexpected SOP\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,2);
    info.status_mask        = 1ull<<25 /* uneop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,2);
    info.enable_mask        = 1ull<<25 /* uneop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2560;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,2)[UNEOP]: Unexpected EOP\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,2);
    info.status_mask        = 1ull<<26 /* undat */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,2);
    info.enable_mask        = 1ull<<26 /* undat */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2560;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,2)[UNDAT]: Unexpected Data\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,2);
    info.status_mask        = 1ull<<27 /* hg2fld */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,2);
    info.enable_mask        = 1ull<<27 /* hg2fld */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2560;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,2)[HG2FLD]: HiGig2 received message field error, as below\n"
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
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,2);
    info.status_mask        = 1ull<<28 /* hg2cc */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,2);
    info.enable_mask        = 1ull<<28 /* hg2cc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2560;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,2)[HG2CC]: HiGig2 received message CRC or Control char  error\n"
        "    Set when either CRC8 error detected or when\n"
        "    a Control Character is found in the message\n"
        "    bytes after the K.SOM\n"
        "    NOTE: HG2CC has higher priority than HG2FLD\n"
        "          i.e. a HiGig2 message that results in HG2CC\n"
        "          getting set, will never set HG2FLD.\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_GMXX_RXX_INT_REG(1,2) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,2);
    info.status_mask        = 1ull<<1 /* carext */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,2);
    info.enable_mask        = 1ull<<1 /* carext */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2576;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,2)[CAREXT]: Carrier extend error\n"
        "    (SGMII/1000Base-X only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,2);
    info.status_mask        = 1ull<<8 /* skperr */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,2);
    info.enable_mask        = 1ull<<8 /* skperr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2576;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,2)[SKPERR]: Skipper error\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,2);
    info.status_mask        = 1ull<<10 /* ovrerr */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,2);
    info.enable_mask        = 1ull<<10 /* ovrerr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2576;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,2)[OVRERR]: Internal Data Aggregation Overflow\n"
        "    This interrupt should never assert\n"
        "    (SGMII/1000Base-X only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,2);
    info.status_mask        = 1ull<<20 /* loc_fault */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,2);
    info.enable_mask        = 1ull<<20 /* loc_fault */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2576;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,2)[LOC_FAULT]: Local Fault Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,2);
    info.status_mask        = 1ull<<21 /* rem_fault */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,2);
    info.enable_mask        = 1ull<<21 /* rem_fault */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2576;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,2)[REM_FAULT]: Remote Fault Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,2);
    info.status_mask        = 1ull<<22 /* bad_seq */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,2);
    info.enable_mask        = 1ull<<22 /* bad_seq */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2576;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,2)[BAD_SEQ]: Reserved Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,2);
    info.status_mask        = 1ull<<23 /* bad_term */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,2);
    info.enable_mask        = 1ull<<23 /* bad_term */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2576;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,2)[BAD_TERM]: Frame is terminated by control character other\n"
        "    than /T/.  The error propagation control\n"
        "    character /E/ will be included as part of the\n"
        "    frame and does not cause a frame termination.\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,2);
    info.status_mask        = 1ull<<24 /* unsop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,2);
    info.enable_mask        = 1ull<<24 /* unsop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2576;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,2)[UNSOP]: Unexpected SOP\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,2);
    info.status_mask        = 1ull<<25 /* uneop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,2);
    info.enable_mask        = 1ull<<25 /* uneop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2576;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,2)[UNEOP]: Unexpected EOP\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,2);
    info.status_mask        = 1ull<<26 /* undat */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,2);
    info.enable_mask        = 1ull<<26 /* undat */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2576;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,2)[UNDAT]: Unexpected Data\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,2);
    info.status_mask        = 1ull<<27 /* hg2fld */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,2);
    info.enable_mask        = 1ull<<27 /* hg2fld */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2576;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,2)[HG2FLD]: HiGig2 received message field error, as below\n"
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
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,2);
    info.status_mask        = 1ull<<28 /* hg2cc */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,2);
    info.enable_mask        = 1ull<<28 /* hg2cc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2576;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,2)[HG2CC]: HiGig2 received message CRC or Control char  error\n"
        "    Set when either CRC8 error detected or when\n"
        "    a Control Character is found in the message\n"
        "    bytes after the K.SOM\n"
        "    NOTE: HG2CC has higher priority than HG2FLD\n"
        "          i.e. a HiGig2 message that results in HG2CC\n"
        "          getting set, will never set HG2FLD.\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_GMXX_RXX_INT_REG(2,2) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,2);
    info.status_mask        = 1ull<<1 /* carext */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,2);
    info.enable_mask        = 1ull<<1 /* carext */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2592;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,2)[CAREXT]: Carrier extend error\n"
        "    (SGMII/1000Base-X only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,2);
    info.status_mask        = 1ull<<8 /* skperr */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,2);
    info.enable_mask        = 1ull<<8 /* skperr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2592;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,2)[SKPERR]: Skipper error\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,2);
    info.status_mask        = 1ull<<10 /* ovrerr */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,2);
    info.enable_mask        = 1ull<<10 /* ovrerr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2592;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,2)[OVRERR]: Internal Data Aggregation Overflow\n"
        "    This interrupt should never assert\n"
        "    (SGMII/1000Base-X only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,2);
    info.status_mask        = 1ull<<20 /* loc_fault */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,2);
    info.enable_mask        = 1ull<<20 /* loc_fault */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2592;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,2)[LOC_FAULT]: Local Fault Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,2);
    info.status_mask        = 1ull<<21 /* rem_fault */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,2);
    info.enable_mask        = 1ull<<21 /* rem_fault */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2592;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,2)[REM_FAULT]: Remote Fault Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,2);
    info.status_mask        = 1ull<<22 /* bad_seq */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,2);
    info.enable_mask        = 1ull<<22 /* bad_seq */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2592;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,2)[BAD_SEQ]: Reserved Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,2);
    info.status_mask        = 1ull<<23 /* bad_term */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,2);
    info.enable_mask        = 1ull<<23 /* bad_term */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2592;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,2)[BAD_TERM]: Frame is terminated by control character other\n"
        "    than /T/.  The error propagation control\n"
        "    character /E/ will be included as part of the\n"
        "    frame and does not cause a frame termination.\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,2);
    info.status_mask        = 1ull<<24 /* unsop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,2);
    info.enable_mask        = 1ull<<24 /* unsop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2592;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,2)[UNSOP]: Unexpected SOP\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,2);
    info.status_mask        = 1ull<<25 /* uneop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,2);
    info.enable_mask        = 1ull<<25 /* uneop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2592;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,2)[UNEOP]: Unexpected EOP\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,2);
    info.status_mask        = 1ull<<26 /* undat */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,2);
    info.enable_mask        = 1ull<<26 /* undat */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2592;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,2)[UNDAT]: Unexpected Data\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,2);
    info.status_mask        = 1ull<<27 /* hg2fld */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,2);
    info.enable_mask        = 1ull<<27 /* hg2fld */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2592;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,2)[HG2FLD]: HiGig2 received message field error, as below\n"
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
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,2);
    info.status_mask        = 1ull<<28 /* hg2cc */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,2);
    info.enable_mask        = 1ull<<28 /* hg2cc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2592;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,2)[HG2CC]: HiGig2 received message CRC or Control char  error\n"
        "    Set when either CRC8 error detected or when\n"
        "    a Control Character is found in the message\n"
        "    bytes after the K.SOM\n"
        "    NOTE: HG2CC has higher priority than HG2FLD\n"
        "          i.e. a HiGig2 message that results in HG2CC\n"
        "          getting set, will never set HG2FLD.\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_GMXX_RXX_INT_REG(3,2) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,2);
    info.status_mask        = 1ull<<1 /* carext */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,2);
    info.enable_mask        = 1ull<<1 /* carext */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2608;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,2)[CAREXT]: Carrier extend error\n"
        "    (SGMII/1000Base-X only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,2);
    info.status_mask        = 1ull<<8 /* skperr */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,2);
    info.enable_mask        = 1ull<<8 /* skperr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2608;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,2)[SKPERR]: Skipper error\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,2);
    info.status_mask        = 1ull<<10 /* ovrerr */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,2);
    info.enable_mask        = 1ull<<10 /* ovrerr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2608;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,2)[OVRERR]: Internal Data Aggregation Overflow\n"
        "    This interrupt should never assert\n"
        "    (SGMII/1000Base-X only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,2);
    info.status_mask        = 1ull<<20 /* loc_fault */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,2);
    info.enable_mask        = 1ull<<20 /* loc_fault */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2608;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,2)[LOC_FAULT]: Local Fault Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,2);
    info.status_mask        = 1ull<<21 /* rem_fault */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,2);
    info.enable_mask        = 1ull<<21 /* rem_fault */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2608;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,2)[REM_FAULT]: Remote Fault Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,2);
    info.status_mask        = 1ull<<22 /* bad_seq */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,2);
    info.enable_mask        = 1ull<<22 /* bad_seq */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2608;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,2)[BAD_SEQ]: Reserved Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,2);
    info.status_mask        = 1ull<<23 /* bad_term */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,2);
    info.enable_mask        = 1ull<<23 /* bad_term */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2608;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,2)[BAD_TERM]: Frame is terminated by control character other\n"
        "    than /T/.  The error propagation control\n"
        "    character /E/ will be included as part of the\n"
        "    frame and does not cause a frame termination.\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,2);
    info.status_mask        = 1ull<<24 /* unsop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,2);
    info.enable_mask        = 1ull<<24 /* unsop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2608;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,2)[UNSOP]: Unexpected SOP\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,2);
    info.status_mask        = 1ull<<25 /* uneop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,2);
    info.enable_mask        = 1ull<<25 /* uneop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2608;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,2)[UNEOP]: Unexpected EOP\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,2);
    info.status_mask        = 1ull<<26 /* undat */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,2);
    info.enable_mask        = 1ull<<26 /* undat */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2608;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,2)[UNDAT]: Unexpected Data\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,2);
    info.status_mask        = 1ull<<27 /* hg2fld */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,2);
    info.enable_mask        = 1ull<<27 /* hg2fld */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2608;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,2)[HG2FLD]: HiGig2 received message field error, as below\n"
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
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,2);
    info.status_mask        = 1ull<<28 /* hg2cc */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,2);
    info.enable_mask        = 1ull<<28 /* hg2cc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2608;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,2)[HG2CC]: HiGig2 received message CRC or Control char  error\n"
        "    Set when either CRC8 error detected or when\n"
        "    a Control Character is found in the message\n"
        "    bytes after the K.SOM\n"
        "    NOTE: HG2CC has higher priority than HG2FLD\n"
        "          i.e. a HiGig2 message that results in HG2CC\n"
        "          getting set, will never set HG2FLD.\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_GMXX_RXX_INT_REG(0,3) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,3);
    info.status_mask        = 1ull<<1 /* carext */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,3);
    info.enable_mask        = 1ull<<1 /* carext */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2816;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,3)[CAREXT]: Carrier extend error\n"
        "    (SGMII/1000Base-X only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,3);
    info.status_mask        = 1ull<<8 /* skperr */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,3);
    info.enable_mask        = 1ull<<8 /* skperr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2816;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,3)[SKPERR]: Skipper error\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,3);
    info.status_mask        = 1ull<<10 /* ovrerr */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,3);
    info.enable_mask        = 1ull<<10 /* ovrerr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2816;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,3)[OVRERR]: Internal Data Aggregation Overflow\n"
        "    This interrupt should never assert\n"
        "    (SGMII/1000Base-X only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,3);
    info.status_mask        = 1ull<<20 /* loc_fault */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,3);
    info.enable_mask        = 1ull<<20 /* loc_fault */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2816;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,3)[LOC_FAULT]: Local Fault Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,3);
    info.status_mask        = 1ull<<21 /* rem_fault */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,3);
    info.enable_mask        = 1ull<<21 /* rem_fault */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2816;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,3)[REM_FAULT]: Remote Fault Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,3);
    info.status_mask        = 1ull<<22 /* bad_seq */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,3);
    info.enable_mask        = 1ull<<22 /* bad_seq */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2816;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,3)[BAD_SEQ]: Reserved Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,3);
    info.status_mask        = 1ull<<23 /* bad_term */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,3);
    info.enable_mask        = 1ull<<23 /* bad_term */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2816;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,3)[BAD_TERM]: Frame is terminated by control character other\n"
        "    than /T/.  The error propagation control\n"
        "    character /E/ will be included as part of the\n"
        "    frame and does not cause a frame termination.\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,3);
    info.status_mask        = 1ull<<24 /* unsop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,3);
    info.enable_mask        = 1ull<<24 /* unsop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2816;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,3)[UNSOP]: Unexpected SOP\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,3);
    info.status_mask        = 1ull<<25 /* uneop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,3);
    info.enable_mask        = 1ull<<25 /* uneop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2816;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,3)[UNEOP]: Unexpected EOP\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,3);
    info.status_mask        = 1ull<<26 /* undat */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,3);
    info.enable_mask        = 1ull<<26 /* undat */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2816;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,3)[UNDAT]: Unexpected Data\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,3);
    info.status_mask        = 1ull<<27 /* hg2fld */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,3);
    info.enable_mask        = 1ull<<27 /* hg2fld */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2816;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,3)[HG2FLD]: HiGig2 received message field error, as below\n"
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
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,3);
    info.status_mask        = 1ull<<28 /* hg2cc */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,3);
    info.enable_mask        = 1ull<<28 /* hg2cc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2816;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,3)[HG2CC]: HiGig2 received message CRC or Control char  error\n"
        "    Set when either CRC8 error detected or when\n"
        "    a Control Character is found in the message\n"
        "    bytes after the K.SOM\n"
        "    NOTE: HG2CC has higher priority than HG2FLD\n"
        "          i.e. a HiGig2 message that results in HG2CC\n"
        "          getting set, will never set HG2FLD.\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_GMXX_RXX_INT_REG(1,3) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,3);
    info.status_mask        = 1ull<<1 /* carext */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,3);
    info.enable_mask        = 1ull<<1 /* carext */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2832;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,3)[CAREXT]: Carrier extend error\n"
        "    (SGMII/1000Base-X only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,3);
    info.status_mask        = 1ull<<8 /* skperr */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,3);
    info.enable_mask        = 1ull<<8 /* skperr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2832;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,3)[SKPERR]: Skipper error\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,3);
    info.status_mask        = 1ull<<10 /* ovrerr */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,3);
    info.enable_mask        = 1ull<<10 /* ovrerr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2832;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,3)[OVRERR]: Internal Data Aggregation Overflow\n"
        "    This interrupt should never assert\n"
        "    (SGMII/1000Base-X only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,3);
    info.status_mask        = 1ull<<20 /* loc_fault */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,3);
    info.enable_mask        = 1ull<<20 /* loc_fault */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2832;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,3)[LOC_FAULT]: Local Fault Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,3);
    info.status_mask        = 1ull<<21 /* rem_fault */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,3);
    info.enable_mask        = 1ull<<21 /* rem_fault */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2832;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,3)[REM_FAULT]: Remote Fault Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,3);
    info.status_mask        = 1ull<<22 /* bad_seq */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,3);
    info.enable_mask        = 1ull<<22 /* bad_seq */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2832;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,3)[BAD_SEQ]: Reserved Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,3);
    info.status_mask        = 1ull<<23 /* bad_term */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,3);
    info.enable_mask        = 1ull<<23 /* bad_term */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2832;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,3)[BAD_TERM]: Frame is terminated by control character other\n"
        "    than /T/.  The error propagation control\n"
        "    character /E/ will be included as part of the\n"
        "    frame and does not cause a frame termination.\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,3);
    info.status_mask        = 1ull<<24 /* unsop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,3);
    info.enable_mask        = 1ull<<24 /* unsop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2832;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,3)[UNSOP]: Unexpected SOP\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,3);
    info.status_mask        = 1ull<<25 /* uneop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,3);
    info.enable_mask        = 1ull<<25 /* uneop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2832;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,3)[UNEOP]: Unexpected EOP\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,3);
    info.status_mask        = 1ull<<26 /* undat */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,3);
    info.enable_mask        = 1ull<<26 /* undat */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2832;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,3)[UNDAT]: Unexpected Data\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,3);
    info.status_mask        = 1ull<<27 /* hg2fld */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,3);
    info.enable_mask        = 1ull<<27 /* hg2fld */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2832;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,3)[HG2FLD]: HiGig2 received message field error, as below\n"
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
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,3);
    info.status_mask        = 1ull<<28 /* hg2cc */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,3);
    info.enable_mask        = 1ull<<28 /* hg2cc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2832;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,3)[HG2CC]: HiGig2 received message CRC or Control char  error\n"
        "    Set when either CRC8 error detected or when\n"
        "    a Control Character is found in the message\n"
        "    bytes after the K.SOM\n"
        "    NOTE: HG2CC has higher priority than HG2FLD\n"
        "          i.e. a HiGig2 message that results in HG2CC\n"
        "          getting set, will never set HG2FLD.\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_GMXX_RXX_INT_REG(2,3) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,3);
    info.status_mask        = 1ull<<1 /* carext */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,3);
    info.enable_mask        = 1ull<<1 /* carext */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2848;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,3)[CAREXT]: Carrier extend error\n"
        "    (SGMII/1000Base-X only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,3);
    info.status_mask        = 1ull<<8 /* skperr */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,3);
    info.enable_mask        = 1ull<<8 /* skperr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2848;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,3)[SKPERR]: Skipper error\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,3);
    info.status_mask        = 1ull<<10 /* ovrerr */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,3);
    info.enable_mask        = 1ull<<10 /* ovrerr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2848;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,3)[OVRERR]: Internal Data Aggregation Overflow\n"
        "    This interrupt should never assert\n"
        "    (SGMII/1000Base-X only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,3);
    info.status_mask        = 1ull<<20 /* loc_fault */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,3);
    info.enable_mask        = 1ull<<20 /* loc_fault */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2848;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,3)[LOC_FAULT]: Local Fault Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,3);
    info.status_mask        = 1ull<<21 /* rem_fault */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,3);
    info.enable_mask        = 1ull<<21 /* rem_fault */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2848;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,3)[REM_FAULT]: Remote Fault Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,3);
    info.status_mask        = 1ull<<22 /* bad_seq */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,3);
    info.enable_mask        = 1ull<<22 /* bad_seq */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2848;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,3)[BAD_SEQ]: Reserved Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,3);
    info.status_mask        = 1ull<<23 /* bad_term */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,3);
    info.enable_mask        = 1ull<<23 /* bad_term */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2848;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,3)[BAD_TERM]: Frame is terminated by control character other\n"
        "    than /T/.  The error propagation control\n"
        "    character /E/ will be included as part of the\n"
        "    frame and does not cause a frame termination.\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,3);
    info.status_mask        = 1ull<<24 /* unsop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,3);
    info.enable_mask        = 1ull<<24 /* unsop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2848;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,3)[UNSOP]: Unexpected SOP\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,3);
    info.status_mask        = 1ull<<25 /* uneop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,3);
    info.enable_mask        = 1ull<<25 /* uneop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2848;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,3)[UNEOP]: Unexpected EOP\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,3);
    info.status_mask        = 1ull<<26 /* undat */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,3);
    info.enable_mask        = 1ull<<26 /* undat */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2848;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,3)[UNDAT]: Unexpected Data\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,3);
    info.status_mask        = 1ull<<27 /* hg2fld */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,3);
    info.enable_mask        = 1ull<<27 /* hg2fld */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2848;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,3)[HG2FLD]: HiGig2 received message field error, as below\n"
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
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,3);
    info.status_mask        = 1ull<<28 /* hg2cc */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,3);
    info.enable_mask        = 1ull<<28 /* hg2cc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2848;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,3)[HG2CC]: HiGig2 received message CRC or Control char  error\n"
        "    Set when either CRC8 error detected or when\n"
        "    a Control Character is found in the message\n"
        "    bytes after the K.SOM\n"
        "    NOTE: HG2CC has higher priority than HG2FLD\n"
        "          i.e. a HiGig2 message that results in HG2CC\n"
        "          getting set, will never set HG2FLD.\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_GMXX_RXX_INT_REG(3,3) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,3);
    info.status_mask        = 1ull<<1 /* carext */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,3);
    info.enable_mask        = 1ull<<1 /* carext */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2864;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,3)[CAREXT]: Carrier extend error\n"
        "    (SGMII/1000Base-X only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,3);
    info.status_mask        = 1ull<<8 /* skperr */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,3);
    info.enable_mask        = 1ull<<8 /* skperr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2864;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,3)[SKPERR]: Skipper error\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,3);
    info.status_mask        = 1ull<<10 /* ovrerr */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,3);
    info.enable_mask        = 1ull<<10 /* ovrerr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2864;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,3)[OVRERR]: Internal Data Aggregation Overflow\n"
        "    This interrupt should never assert\n"
        "    (SGMII/1000Base-X only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,3);
    info.status_mask        = 1ull<<20 /* loc_fault */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,3);
    info.enable_mask        = 1ull<<20 /* loc_fault */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2864;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,3)[LOC_FAULT]: Local Fault Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,3);
    info.status_mask        = 1ull<<21 /* rem_fault */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,3);
    info.enable_mask        = 1ull<<21 /* rem_fault */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2864;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,3)[REM_FAULT]: Remote Fault Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,3);
    info.status_mask        = 1ull<<22 /* bad_seq */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,3);
    info.enable_mask        = 1ull<<22 /* bad_seq */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2864;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,3)[BAD_SEQ]: Reserved Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,3);
    info.status_mask        = 1ull<<23 /* bad_term */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,3);
    info.enable_mask        = 1ull<<23 /* bad_term */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2864;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,3)[BAD_TERM]: Frame is terminated by control character other\n"
        "    than /T/.  The error propagation control\n"
        "    character /E/ will be included as part of the\n"
        "    frame and does not cause a frame termination.\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,3);
    info.status_mask        = 1ull<<24 /* unsop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,3);
    info.enable_mask        = 1ull<<24 /* unsop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2864;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,3)[UNSOP]: Unexpected SOP\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,3);
    info.status_mask        = 1ull<<25 /* uneop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,3);
    info.enable_mask        = 1ull<<25 /* uneop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2864;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,3)[UNEOP]: Unexpected EOP\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,3);
    info.status_mask        = 1ull<<26 /* undat */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,3);
    info.enable_mask        = 1ull<<26 /* undat */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2864;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,3)[UNDAT]: Unexpected Data\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,3);
    info.status_mask        = 1ull<<27 /* hg2fld */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,3);
    info.enable_mask        = 1ull<<27 /* hg2fld */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2864;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,3)[HG2FLD]: HiGig2 received message field error, as below\n"
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
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,3);
    info.status_mask        = 1ull<<28 /* hg2cc */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,3);
    info.enable_mask        = 1ull<<28 /* hg2cc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2864;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,3)[HG2CC]: HiGig2 received message CRC or Control char  error\n"
        "    Set when either CRC8 error detected or when\n"
        "    a Control Character is found in the message\n"
        "    bytes after the K.SOM\n"
        "    NOTE: HG2CC has higher priority than HG2FLD\n"
        "          i.e. a HiGig2 message that results in HG2CC\n"
        "          getting set, will never set HG2FLD.\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_GMXX_RXX_INT_REG(0,4) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,4);
    info.status_mask        = 1ull<<1 /* carext */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,4);
    info.enable_mask        = 1ull<<1 /* carext */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3072;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,4)[CAREXT]: Carrier extend error\n"
        "    (SGMII/1000Base-X only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,4);
    info.status_mask        = 1ull<<8 /* skperr */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,4);
    info.enable_mask        = 1ull<<8 /* skperr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3072;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,4)[SKPERR]: Skipper error\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,4);
    info.status_mask        = 1ull<<10 /* ovrerr */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,4);
    info.enable_mask        = 1ull<<10 /* ovrerr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3072;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,4)[OVRERR]: Internal Data Aggregation Overflow\n"
        "    This interrupt should never assert\n"
        "    (SGMII/1000Base-X only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,4);
    info.status_mask        = 1ull<<20 /* loc_fault */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,4);
    info.enable_mask        = 1ull<<20 /* loc_fault */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3072;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,4)[LOC_FAULT]: Local Fault Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,4);
    info.status_mask        = 1ull<<21 /* rem_fault */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,4);
    info.enable_mask        = 1ull<<21 /* rem_fault */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3072;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,4)[REM_FAULT]: Remote Fault Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,4);
    info.status_mask        = 1ull<<22 /* bad_seq */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,4);
    info.enable_mask        = 1ull<<22 /* bad_seq */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3072;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,4)[BAD_SEQ]: Reserved Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,4);
    info.status_mask        = 1ull<<23 /* bad_term */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,4);
    info.enable_mask        = 1ull<<23 /* bad_term */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3072;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,4)[BAD_TERM]: Frame is terminated by control character other\n"
        "    than /T/.  The error propagation control\n"
        "    character /E/ will be included as part of the\n"
        "    frame and does not cause a frame termination.\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,4);
    info.status_mask        = 1ull<<24 /* unsop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,4);
    info.enable_mask        = 1ull<<24 /* unsop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3072;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,4)[UNSOP]: Unexpected SOP\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,4);
    info.status_mask        = 1ull<<25 /* uneop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,4);
    info.enable_mask        = 1ull<<25 /* uneop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3072;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,4)[UNEOP]: Unexpected EOP\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,4);
    info.status_mask        = 1ull<<26 /* undat */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,4);
    info.enable_mask        = 1ull<<26 /* undat */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3072;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,4)[UNDAT]: Unexpected Data\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,4);
    info.status_mask        = 1ull<<27 /* hg2fld */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,4);
    info.enable_mask        = 1ull<<27 /* hg2fld */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3072;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,4)[HG2FLD]: HiGig2 received message field error, as below\n"
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
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(0,4);
    info.status_mask        = 1ull<<28 /* hg2cc */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(0,4);
    info.enable_mask        = 1ull<<28 /* hg2cc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3072;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(0,4)[HG2CC]: HiGig2 received message CRC or Control char  error\n"
        "    Set when either CRC8 error detected or when\n"
        "    a Control Character is found in the message\n"
        "    bytes after the K.SOM\n"
        "    NOTE: HG2CC has higher priority than HG2FLD\n"
        "          i.e. a HiGig2 message that results in HG2CC\n"
        "          getting set, will never set HG2FLD.\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_GMXX_RXX_INT_REG(1,4) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,4);
    info.status_mask        = 1ull<<1 /* carext */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,4);
    info.enable_mask        = 1ull<<1 /* carext */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3088;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,4)[CAREXT]: Carrier extend error\n"
        "    (SGMII/1000Base-X only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,4);
    info.status_mask        = 1ull<<8 /* skperr */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,4);
    info.enable_mask        = 1ull<<8 /* skperr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3088;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,4)[SKPERR]: Skipper error\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,4);
    info.status_mask        = 1ull<<10 /* ovrerr */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,4);
    info.enable_mask        = 1ull<<10 /* ovrerr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3088;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,4)[OVRERR]: Internal Data Aggregation Overflow\n"
        "    This interrupt should never assert\n"
        "    (SGMII/1000Base-X only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,4);
    info.status_mask        = 1ull<<20 /* loc_fault */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,4);
    info.enable_mask        = 1ull<<20 /* loc_fault */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3088;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,4)[LOC_FAULT]: Local Fault Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,4);
    info.status_mask        = 1ull<<21 /* rem_fault */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,4);
    info.enable_mask        = 1ull<<21 /* rem_fault */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3088;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,4)[REM_FAULT]: Remote Fault Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,4);
    info.status_mask        = 1ull<<22 /* bad_seq */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,4);
    info.enable_mask        = 1ull<<22 /* bad_seq */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3088;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,4)[BAD_SEQ]: Reserved Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,4);
    info.status_mask        = 1ull<<23 /* bad_term */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,4);
    info.enable_mask        = 1ull<<23 /* bad_term */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3088;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,4)[BAD_TERM]: Frame is terminated by control character other\n"
        "    than /T/.  The error propagation control\n"
        "    character /E/ will be included as part of the\n"
        "    frame and does not cause a frame termination.\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,4);
    info.status_mask        = 1ull<<24 /* unsop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,4);
    info.enable_mask        = 1ull<<24 /* unsop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3088;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,4)[UNSOP]: Unexpected SOP\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,4);
    info.status_mask        = 1ull<<25 /* uneop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,4);
    info.enable_mask        = 1ull<<25 /* uneop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3088;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,4)[UNEOP]: Unexpected EOP\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,4);
    info.status_mask        = 1ull<<26 /* undat */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,4);
    info.enable_mask        = 1ull<<26 /* undat */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3088;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,4)[UNDAT]: Unexpected Data\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,4);
    info.status_mask        = 1ull<<27 /* hg2fld */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,4);
    info.enable_mask        = 1ull<<27 /* hg2fld */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3088;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,4)[HG2FLD]: HiGig2 received message field error, as below\n"
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
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(1,4);
    info.status_mask        = 1ull<<28 /* hg2cc */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(1,4);
    info.enable_mask        = 1ull<<28 /* hg2cc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3088;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(1,4)[HG2CC]: HiGig2 received message CRC or Control char  error\n"
        "    Set when either CRC8 error detected or when\n"
        "    a Control Character is found in the message\n"
        "    bytes after the K.SOM\n"
        "    NOTE: HG2CC has higher priority than HG2FLD\n"
        "          i.e. a HiGig2 message that results in HG2CC\n"
        "          getting set, will never set HG2FLD.\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_GMXX_RXX_INT_REG(2,4) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,4);
    info.status_mask        = 1ull<<1 /* carext */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,4);
    info.enable_mask        = 1ull<<1 /* carext */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3104;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,4)[CAREXT]: Carrier extend error\n"
        "    (SGMII/1000Base-X only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,4);
    info.status_mask        = 1ull<<8 /* skperr */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,4);
    info.enable_mask        = 1ull<<8 /* skperr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3104;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,4)[SKPERR]: Skipper error\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,4);
    info.status_mask        = 1ull<<10 /* ovrerr */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,4);
    info.enable_mask        = 1ull<<10 /* ovrerr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3104;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,4)[OVRERR]: Internal Data Aggregation Overflow\n"
        "    This interrupt should never assert\n"
        "    (SGMII/1000Base-X only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,4);
    info.status_mask        = 1ull<<20 /* loc_fault */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,4);
    info.enable_mask        = 1ull<<20 /* loc_fault */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3104;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,4)[LOC_FAULT]: Local Fault Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,4);
    info.status_mask        = 1ull<<21 /* rem_fault */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,4);
    info.enable_mask        = 1ull<<21 /* rem_fault */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3104;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,4)[REM_FAULT]: Remote Fault Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,4);
    info.status_mask        = 1ull<<22 /* bad_seq */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,4);
    info.enable_mask        = 1ull<<22 /* bad_seq */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3104;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,4)[BAD_SEQ]: Reserved Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,4);
    info.status_mask        = 1ull<<23 /* bad_term */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,4);
    info.enable_mask        = 1ull<<23 /* bad_term */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3104;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,4)[BAD_TERM]: Frame is terminated by control character other\n"
        "    than /T/.  The error propagation control\n"
        "    character /E/ will be included as part of the\n"
        "    frame and does not cause a frame termination.\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,4);
    info.status_mask        = 1ull<<24 /* unsop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,4);
    info.enable_mask        = 1ull<<24 /* unsop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3104;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,4)[UNSOP]: Unexpected SOP\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,4);
    info.status_mask        = 1ull<<25 /* uneop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,4);
    info.enable_mask        = 1ull<<25 /* uneop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3104;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,4)[UNEOP]: Unexpected EOP\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,4);
    info.status_mask        = 1ull<<26 /* undat */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,4);
    info.enable_mask        = 1ull<<26 /* undat */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3104;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,4)[UNDAT]: Unexpected Data\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,4);
    info.status_mask        = 1ull<<27 /* hg2fld */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,4);
    info.enable_mask        = 1ull<<27 /* hg2fld */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3104;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,4)[HG2FLD]: HiGig2 received message field error, as below\n"
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
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(2,4);
    info.status_mask        = 1ull<<28 /* hg2cc */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(2,4);
    info.enable_mask        = 1ull<<28 /* hg2cc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3104;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(2,4)[HG2CC]: HiGig2 received message CRC or Control char  error\n"
        "    Set when either CRC8 error detected or when\n"
        "    a Control Character is found in the message\n"
        "    bytes after the K.SOM\n"
        "    NOTE: HG2CC has higher priority than HG2FLD\n"
        "          i.e. a HiGig2 message that results in HG2CC\n"
        "          getting set, will never set HG2FLD.\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_GMXX_RXX_INT_REG(3,4) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,4);
    info.status_mask        = 1ull<<1 /* carext */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,4);
    info.enable_mask        = 1ull<<1 /* carext */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3120;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,4)[CAREXT]: Carrier extend error\n"
        "    (SGMII/1000Base-X only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,4);
    info.status_mask        = 1ull<<8 /* skperr */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,4);
    info.enable_mask        = 1ull<<8 /* skperr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3120;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,4)[SKPERR]: Skipper error\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,4);
    info.status_mask        = 1ull<<10 /* ovrerr */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,4);
    info.enable_mask        = 1ull<<10 /* ovrerr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3120;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,4)[OVRERR]: Internal Data Aggregation Overflow\n"
        "    This interrupt should never assert\n"
        "    (SGMII/1000Base-X only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,4);
    info.status_mask        = 1ull<<20 /* loc_fault */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,4);
    info.enable_mask        = 1ull<<20 /* loc_fault */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3120;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,4)[LOC_FAULT]: Local Fault Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,4);
    info.status_mask        = 1ull<<21 /* rem_fault */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,4);
    info.enable_mask        = 1ull<<21 /* rem_fault */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3120;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,4)[REM_FAULT]: Remote Fault Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,4);
    info.status_mask        = 1ull<<22 /* bad_seq */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,4);
    info.enable_mask        = 1ull<<22 /* bad_seq */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3120;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,4)[BAD_SEQ]: Reserved Sequence Deteted\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,4);
    info.status_mask        = 1ull<<23 /* bad_term */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,4);
    info.enable_mask        = 1ull<<23 /* bad_term */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3120;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,4)[BAD_TERM]: Frame is terminated by control character other\n"
        "    than /T/.  The error propagation control\n"
        "    character /E/ will be included as part of the\n"
        "    frame and does not cause a frame termination.\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,4);
    info.status_mask        = 1ull<<24 /* unsop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,4);
    info.enable_mask        = 1ull<<24 /* unsop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3120;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,4)[UNSOP]: Unexpected SOP\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,4);
    info.status_mask        = 1ull<<25 /* uneop */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,4);
    info.enable_mask        = 1ull<<25 /* uneop */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3120;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,4)[UNEOP]: Unexpected EOP\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,4);
    info.status_mask        = 1ull<<26 /* undat */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,4);
    info.enable_mask        = 1ull<<26 /* undat */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3120;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,4)[UNDAT]: Unexpected Data\n"
        "    (XAUI/RXAUI Mode only)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,4);
    info.status_mask        = 1ull<<27 /* hg2fld */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,4);
    info.enable_mask        = 1ull<<27 /* hg2fld */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3120;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,4)[HG2FLD]: HiGig2 received message field error, as below\n"
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
    info.status_addr        = CVMX_GMXX_RXX_INT_REG(3,4);
    info.status_mask        = 1ull<<28 /* hg2cc */;
    info.enable_addr        = CVMX_GMXX_RXX_INT_EN(3,4);
    info.enable_mask        = 1ull<<28 /* hg2cc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3120;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_RXX_INT_REG(3,4)[HG2CC]: HiGig2 received message CRC or Control char  error\n"
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
    info.group_index        = 2048;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2048;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_TX_INT_REG(0)[UNDFLW]: TX Underflow\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_TX_INT_REG(0);
    info.status_mask        = 0xfull<<20 /* ptp_lost */;
    info.enable_addr        = CVMX_GMXX_TX_INT_EN(0);
    info.enable_mask        = 0xfull<<20 /* ptp_lost */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2048;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_TX_INT_REG(0)[PTP_LOST]: A packet with a PTP request was not able to be\n"
        "    sent due to XSCOL\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_GMXX_TX_INT_REG(1) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_TX_INT_REG(1);
    info.status_mask        = 1ull<<0 /* pko_nxa */;
    info.enable_addr        = CVMX_GMXX_TX_INT_EN(1);
    info.enable_mask        = 1ull<<0 /* pko_nxa */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2368;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_TX_INT_REG(1)[PKO_NXA]: Port address out-of-range from PKO Interface\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_TX_INT_REG(1);
    info.status_mask        = 0xfull<<2 /* undflw */;
    info.enable_addr        = CVMX_GMXX_TX_INT_EN(1);
    info.enable_mask        = 0xfull<<2 /* undflw */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2368;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_TX_INT_REG(1)[UNDFLW]: TX Underflow\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_TX_INT_REG(1);
    info.status_mask        = 0xfull<<20 /* ptp_lost */;
    info.enable_addr        = CVMX_GMXX_TX_INT_EN(1);
    info.enable_mask        = 0xfull<<20 /* ptp_lost */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2368;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_TX_INT_REG(1)[PTP_LOST]: A packet with a PTP request was not able to be\n"
        "    sent due to XSCOL\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_GMXX_TX_INT_REG(2) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_TX_INT_REG(2);
    info.status_mask        = 1ull<<0 /* pko_nxa */;
    info.enable_addr        = CVMX_GMXX_TX_INT_EN(2);
    info.enable_mask        = 1ull<<0 /* pko_nxa */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2560;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_TX_INT_REG(2)[PKO_NXA]: Port address out-of-range from PKO Interface\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_TX_INT_REG(2);
    info.status_mask        = 0xfull<<2 /* undflw */;
    info.enable_addr        = CVMX_GMXX_TX_INT_EN(2);
    info.enable_mask        = 0xfull<<2 /* undflw */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2560;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_TX_INT_REG(2)[UNDFLW]: TX Underflow\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_TX_INT_REG(2);
    info.status_mask        = 0xfull<<20 /* ptp_lost */;
    info.enable_addr        = CVMX_GMXX_TX_INT_EN(2);
    info.enable_mask        = 0xfull<<20 /* ptp_lost */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2560;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_TX_INT_REG(2)[PTP_LOST]: A packet with a PTP request was not able to be\n"
        "    sent due to XSCOL\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_GMXX_TX_INT_REG(3) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_TX_INT_REG(3);
    info.status_mask        = 1ull<<0 /* pko_nxa */;
    info.enable_addr        = CVMX_GMXX_TX_INT_EN(3);
    info.enable_mask        = 1ull<<0 /* pko_nxa */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2816;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_TX_INT_REG(3)[PKO_NXA]: Port address out-of-range from PKO Interface\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_TX_INT_REG(3);
    info.status_mask        = 0xfull<<2 /* undflw */;
    info.enable_addr        = CVMX_GMXX_TX_INT_EN(3);
    info.enable_mask        = 0xfull<<2 /* undflw */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2816;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_TX_INT_REG(3)[UNDFLW]: TX Underflow\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_TX_INT_REG(3);
    info.status_mask        = 0xfull<<20 /* ptp_lost */;
    info.enable_addr        = CVMX_GMXX_TX_INT_EN(3);
    info.enable_mask        = 0xfull<<20 /* ptp_lost */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2816;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_TX_INT_REG(3)[PTP_LOST]: A packet with a PTP request was not able to be\n"
        "    sent due to XSCOL\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_GMXX_TX_INT_REG(4) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_TX_INT_REG(4);
    info.status_mask        = 1ull<<0 /* pko_nxa */;
    info.enable_addr        = CVMX_GMXX_TX_INT_EN(4);
    info.enable_mask        = 1ull<<0 /* pko_nxa */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3072;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_TX_INT_REG(4)[PKO_NXA]: Port address out-of-range from PKO Interface\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_TX_INT_REG(4);
    info.status_mask        = 0xfull<<2 /* undflw */;
    info.enable_addr        = CVMX_GMXX_TX_INT_EN(4);
    info.enable_mask        = 0xfull<<2 /* undflw */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3072;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_TX_INT_REG(4)[UNDFLW]: TX Underflow\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_GMXX_TX_INT_REG(4);
    info.status_mask        = 0xfull<<20 /* ptp_lost */;
    info.enable_addr        = CVMX_GMXX_TX_INT_EN(4);
    info.enable_mask        = 0xfull<<20 /* ptp_lost */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3072;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_TX_INT_REG(4)[PTP_LOST]: A packet with a PTP request was not able to be\n"
        "    sent due to XSCOL\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_PCSX_INTX_REG(0,0) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,0);
    info.status_mask        = 1ull<<2 /* an_err */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,0);
    info.enable_mask        = 1ull<<2 /* an_err_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2048;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2048;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2048;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2048;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2048;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2048;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2048;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2048;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,0)[SYNC_BAD]: Set by HW whenever rx sync st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,0);
    info.status_mask        = 1ull<<12 /* dbg_sync */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,0);
    info.enable_mask        = 1ull<<12 /* dbg_sync_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2048;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,0)[DBG_SYNC]: Code Group sync failure debug help\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_PCSX_INTX_REG(1,0) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(1,0);
    info.status_mask        = 1ull<<2 /* an_err */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(1,0);
    info.enable_mask        = 1ull<<2 /* an_err_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2064;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2064;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2064;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2064;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2064;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2064;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2064;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2064;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(1,0)[SYNC_BAD]: Set by HW whenever rx sync st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(1,0);
    info.status_mask        = 1ull<<12 /* dbg_sync */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(1,0);
    info.enable_mask        = 1ull<<12 /* dbg_sync_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2064;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(1,0)[DBG_SYNC]: Code Group sync failure debug help\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_PCSX_INTX_REG(2,0) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(2,0);
    info.status_mask        = 1ull<<2 /* an_err */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(2,0);
    info.enable_mask        = 1ull<<2 /* an_err_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2080;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2080;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2080;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2080;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2080;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2080;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2080;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2080;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(2,0)[SYNC_BAD]: Set by HW whenever rx sync st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(2,0);
    info.status_mask        = 1ull<<12 /* dbg_sync */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(2,0);
    info.enable_mask        = 1ull<<12 /* dbg_sync_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2080;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(2,0)[DBG_SYNC]: Code Group sync failure debug help\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_PCSX_INTX_REG(3,0) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(3,0);
    info.status_mask        = 1ull<<2 /* an_err */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(3,0);
    info.enable_mask        = 1ull<<2 /* an_err_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2096;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2096;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2096;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2096;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2096;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2096;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2096;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2096;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(3,0)[SYNC_BAD]: Set by HW whenever rx sync st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(3,0);
    info.status_mask        = 1ull<<12 /* dbg_sync */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(3,0);
    info.enable_mask        = 1ull<<12 /* dbg_sync_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2096;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(3,0)[DBG_SYNC]: Code Group sync failure debug help\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_PCSX_INTX_REG(0,1) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,1);
    info.status_mask        = 1ull<<2 /* an_err */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,1);
    info.enable_mask        = 1ull<<2 /* an_err_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2368;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,1)[AN_ERR]: AN Error, AN resolution function failed\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,1);
    info.status_mask        = 1ull<<3 /* txfifu */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,1);
    info.enable_mask        = 1ull<<3 /* txfifu_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2368;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,1)[TXFIFU]: Set whenever HW detects a TX fifo underflowflow\n"
        "    condition\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,1);
    info.status_mask        = 1ull<<4 /* txfifo */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,1);
    info.enable_mask        = 1ull<<4 /* txfifo_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2368;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,1)[TXFIFO]: Set whenever HW detects a TX fifo overflow\n"
        "    condition\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,1);
    info.status_mask        = 1ull<<5 /* txbad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,1);
    info.enable_mask        = 1ull<<5 /* txbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2368;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,1)[TXBAD]: Set by HW whenever tx st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,1);
    info.status_mask        = 1ull<<7 /* rxbad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,1);
    info.enable_mask        = 1ull<<7 /* rxbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2368;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,1)[RXBAD]: Set by HW whenever rx st machine reaches a  bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,1);
    info.status_mask        = 1ull<<8 /* rxlock */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,1);
    info.enable_mask        = 1ull<<8 /* rxlock_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2368;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,1)[RXLOCK]: Set by HW whenever code group Sync or bit lock\n"
        "    failure occurs\n"
        "    Cannot fire in loopback1 mode\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,1);
    info.status_mask        = 1ull<<9 /* an_bad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,1);
    info.enable_mask        = 1ull<<9 /* an_bad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2368;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,1)[AN_BAD]: Set by HW whenever AN st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,1);
    info.status_mask        = 1ull<<10 /* sync_bad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,1);
    info.enable_mask        = 1ull<<10 /* sync_bad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2368;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,1)[SYNC_BAD]: Set by HW whenever rx sync st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,1);
    info.status_mask        = 1ull<<12 /* dbg_sync */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,1);
    info.enable_mask        = 1ull<<12 /* dbg_sync_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2368;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,1)[DBG_SYNC]: Code Group sync failure debug help\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_PCSX_INTX_REG(0,2) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,2);
    info.status_mask        = 1ull<<2 /* an_err */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,2);
    info.enable_mask        = 1ull<<2 /* an_err_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2560;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,2)[AN_ERR]: AN Error, AN resolution function failed\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,2);
    info.status_mask        = 1ull<<3 /* txfifu */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,2);
    info.enable_mask        = 1ull<<3 /* txfifu_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2560;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,2)[TXFIFU]: Set whenever HW detects a TX fifo underflowflow\n"
        "    condition\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,2);
    info.status_mask        = 1ull<<4 /* txfifo */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,2);
    info.enable_mask        = 1ull<<4 /* txfifo_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2560;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,2)[TXFIFO]: Set whenever HW detects a TX fifo overflow\n"
        "    condition\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,2);
    info.status_mask        = 1ull<<5 /* txbad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,2);
    info.enable_mask        = 1ull<<5 /* txbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2560;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,2)[TXBAD]: Set by HW whenever tx st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,2);
    info.status_mask        = 1ull<<7 /* rxbad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,2);
    info.enable_mask        = 1ull<<7 /* rxbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2560;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,2)[RXBAD]: Set by HW whenever rx st machine reaches a  bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,2);
    info.status_mask        = 1ull<<8 /* rxlock */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,2);
    info.enable_mask        = 1ull<<8 /* rxlock_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2560;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,2)[RXLOCK]: Set by HW whenever code group Sync or bit lock\n"
        "    failure occurs\n"
        "    Cannot fire in loopback1 mode\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,2);
    info.status_mask        = 1ull<<9 /* an_bad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,2);
    info.enable_mask        = 1ull<<9 /* an_bad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2560;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,2)[AN_BAD]: Set by HW whenever AN st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,2);
    info.status_mask        = 1ull<<10 /* sync_bad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,2);
    info.enable_mask        = 1ull<<10 /* sync_bad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2560;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,2)[SYNC_BAD]: Set by HW whenever rx sync st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,2);
    info.status_mask        = 1ull<<12 /* dbg_sync */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,2);
    info.enable_mask        = 1ull<<12 /* dbg_sync_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2560;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,2)[DBG_SYNC]: Code Group sync failure debug help\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_PCSX_INTX_REG(1,2) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(1,2);
    info.status_mask        = 1ull<<2 /* an_err */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(1,2);
    info.enable_mask        = 1ull<<2 /* an_err_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2576;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(1,2)[AN_ERR]: AN Error, AN resolution function failed\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(1,2);
    info.status_mask        = 1ull<<3 /* txfifu */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(1,2);
    info.enable_mask        = 1ull<<3 /* txfifu_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2576;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(1,2)[TXFIFU]: Set whenever HW detects a TX fifo underflowflow\n"
        "    condition\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(1,2);
    info.status_mask        = 1ull<<4 /* txfifo */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(1,2);
    info.enable_mask        = 1ull<<4 /* txfifo_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2576;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(1,2)[TXFIFO]: Set whenever HW detects a TX fifo overflow\n"
        "    condition\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(1,2);
    info.status_mask        = 1ull<<5 /* txbad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(1,2);
    info.enable_mask        = 1ull<<5 /* txbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2576;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(1,2)[TXBAD]: Set by HW whenever tx st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(1,2);
    info.status_mask        = 1ull<<7 /* rxbad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(1,2);
    info.enable_mask        = 1ull<<7 /* rxbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2576;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(1,2)[RXBAD]: Set by HW whenever rx st machine reaches a  bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(1,2);
    info.status_mask        = 1ull<<8 /* rxlock */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(1,2);
    info.enable_mask        = 1ull<<8 /* rxlock_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2576;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(1,2)[RXLOCK]: Set by HW whenever code group Sync or bit lock\n"
        "    failure occurs\n"
        "    Cannot fire in loopback1 mode\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(1,2);
    info.status_mask        = 1ull<<9 /* an_bad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(1,2);
    info.enable_mask        = 1ull<<9 /* an_bad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2576;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(1,2)[AN_BAD]: Set by HW whenever AN st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(1,2);
    info.status_mask        = 1ull<<10 /* sync_bad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(1,2);
    info.enable_mask        = 1ull<<10 /* sync_bad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2576;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(1,2)[SYNC_BAD]: Set by HW whenever rx sync st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(1,2);
    info.status_mask        = 1ull<<12 /* dbg_sync */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(1,2);
    info.enable_mask        = 1ull<<12 /* dbg_sync_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2576;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(1,2)[DBG_SYNC]: Code Group sync failure debug help\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_PCSX_INTX_REG(2,2) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(2,2);
    info.status_mask        = 1ull<<2 /* an_err */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(2,2);
    info.enable_mask        = 1ull<<2 /* an_err_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2592;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(2,2)[AN_ERR]: AN Error, AN resolution function failed\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(2,2);
    info.status_mask        = 1ull<<3 /* txfifu */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(2,2);
    info.enable_mask        = 1ull<<3 /* txfifu_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2592;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(2,2)[TXFIFU]: Set whenever HW detects a TX fifo underflowflow\n"
        "    condition\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(2,2);
    info.status_mask        = 1ull<<4 /* txfifo */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(2,2);
    info.enable_mask        = 1ull<<4 /* txfifo_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2592;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(2,2)[TXFIFO]: Set whenever HW detects a TX fifo overflow\n"
        "    condition\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(2,2);
    info.status_mask        = 1ull<<5 /* txbad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(2,2);
    info.enable_mask        = 1ull<<5 /* txbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2592;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(2,2)[TXBAD]: Set by HW whenever tx st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(2,2);
    info.status_mask        = 1ull<<7 /* rxbad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(2,2);
    info.enable_mask        = 1ull<<7 /* rxbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2592;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(2,2)[RXBAD]: Set by HW whenever rx st machine reaches a  bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(2,2);
    info.status_mask        = 1ull<<8 /* rxlock */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(2,2);
    info.enable_mask        = 1ull<<8 /* rxlock_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2592;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(2,2)[RXLOCK]: Set by HW whenever code group Sync or bit lock\n"
        "    failure occurs\n"
        "    Cannot fire in loopback1 mode\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(2,2);
    info.status_mask        = 1ull<<9 /* an_bad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(2,2);
    info.enable_mask        = 1ull<<9 /* an_bad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2592;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(2,2)[AN_BAD]: Set by HW whenever AN st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(2,2);
    info.status_mask        = 1ull<<10 /* sync_bad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(2,2);
    info.enable_mask        = 1ull<<10 /* sync_bad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2592;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(2,2)[SYNC_BAD]: Set by HW whenever rx sync st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(2,2);
    info.status_mask        = 1ull<<12 /* dbg_sync */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(2,2);
    info.enable_mask        = 1ull<<12 /* dbg_sync_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2592;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(2,2)[DBG_SYNC]: Code Group sync failure debug help\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_PCSX_INTX_REG(3,2) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(3,2);
    info.status_mask        = 1ull<<2 /* an_err */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(3,2);
    info.enable_mask        = 1ull<<2 /* an_err_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2608;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(3,2)[AN_ERR]: AN Error, AN resolution function failed\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(3,2);
    info.status_mask        = 1ull<<3 /* txfifu */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(3,2);
    info.enable_mask        = 1ull<<3 /* txfifu_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2608;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(3,2)[TXFIFU]: Set whenever HW detects a TX fifo underflowflow\n"
        "    condition\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(3,2);
    info.status_mask        = 1ull<<4 /* txfifo */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(3,2);
    info.enable_mask        = 1ull<<4 /* txfifo_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2608;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(3,2)[TXFIFO]: Set whenever HW detects a TX fifo overflow\n"
        "    condition\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(3,2);
    info.status_mask        = 1ull<<5 /* txbad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(3,2);
    info.enable_mask        = 1ull<<5 /* txbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2608;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(3,2)[TXBAD]: Set by HW whenever tx st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(3,2);
    info.status_mask        = 1ull<<7 /* rxbad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(3,2);
    info.enable_mask        = 1ull<<7 /* rxbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2608;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(3,2)[RXBAD]: Set by HW whenever rx st machine reaches a  bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(3,2);
    info.status_mask        = 1ull<<8 /* rxlock */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(3,2);
    info.enable_mask        = 1ull<<8 /* rxlock_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2608;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(3,2)[RXLOCK]: Set by HW whenever code group Sync or bit lock\n"
        "    failure occurs\n"
        "    Cannot fire in loopback1 mode\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(3,2);
    info.status_mask        = 1ull<<9 /* an_bad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(3,2);
    info.enable_mask        = 1ull<<9 /* an_bad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2608;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(3,2)[AN_BAD]: Set by HW whenever AN st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(3,2);
    info.status_mask        = 1ull<<10 /* sync_bad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(3,2);
    info.enable_mask        = 1ull<<10 /* sync_bad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2608;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(3,2)[SYNC_BAD]: Set by HW whenever rx sync st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(3,2);
    info.status_mask        = 1ull<<12 /* dbg_sync */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(3,2);
    info.enable_mask        = 1ull<<12 /* dbg_sync_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2608;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(3,2)[DBG_SYNC]: Code Group sync failure debug help\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_PCSX_INTX_REG(0,3) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,3);
    info.status_mask        = 1ull<<2 /* an_err */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,3);
    info.enable_mask        = 1ull<<2 /* an_err_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2816;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,3)[AN_ERR]: AN Error, AN resolution function failed\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,3);
    info.status_mask        = 1ull<<3 /* txfifu */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,3);
    info.enable_mask        = 1ull<<3 /* txfifu_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2816;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,3)[TXFIFU]: Set whenever HW detects a TX fifo underflowflow\n"
        "    condition\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,3);
    info.status_mask        = 1ull<<4 /* txfifo */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,3);
    info.enable_mask        = 1ull<<4 /* txfifo_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2816;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,3)[TXFIFO]: Set whenever HW detects a TX fifo overflow\n"
        "    condition\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,3);
    info.status_mask        = 1ull<<5 /* txbad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,3);
    info.enable_mask        = 1ull<<5 /* txbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2816;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,3)[TXBAD]: Set by HW whenever tx st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,3);
    info.status_mask        = 1ull<<7 /* rxbad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,3);
    info.enable_mask        = 1ull<<7 /* rxbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2816;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,3)[RXBAD]: Set by HW whenever rx st machine reaches a  bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,3);
    info.status_mask        = 1ull<<8 /* rxlock */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,3);
    info.enable_mask        = 1ull<<8 /* rxlock_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2816;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,3)[RXLOCK]: Set by HW whenever code group Sync or bit lock\n"
        "    failure occurs\n"
        "    Cannot fire in loopback1 mode\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,3);
    info.status_mask        = 1ull<<9 /* an_bad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,3);
    info.enable_mask        = 1ull<<9 /* an_bad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2816;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,3)[AN_BAD]: Set by HW whenever AN st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,3);
    info.status_mask        = 1ull<<10 /* sync_bad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,3);
    info.enable_mask        = 1ull<<10 /* sync_bad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2816;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,3)[SYNC_BAD]: Set by HW whenever rx sync st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,3);
    info.status_mask        = 1ull<<12 /* dbg_sync */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,3);
    info.enable_mask        = 1ull<<12 /* dbg_sync_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2816;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,3)[DBG_SYNC]: Code Group sync failure debug help\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_PCSX_INTX_REG(1,3) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(1,3);
    info.status_mask        = 1ull<<2 /* an_err */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(1,3);
    info.enable_mask        = 1ull<<2 /* an_err_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2832;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(1,3)[AN_ERR]: AN Error, AN resolution function failed\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(1,3);
    info.status_mask        = 1ull<<3 /* txfifu */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(1,3);
    info.enable_mask        = 1ull<<3 /* txfifu_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2832;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(1,3)[TXFIFU]: Set whenever HW detects a TX fifo underflowflow\n"
        "    condition\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(1,3);
    info.status_mask        = 1ull<<4 /* txfifo */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(1,3);
    info.enable_mask        = 1ull<<4 /* txfifo_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2832;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(1,3)[TXFIFO]: Set whenever HW detects a TX fifo overflow\n"
        "    condition\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(1,3);
    info.status_mask        = 1ull<<5 /* txbad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(1,3);
    info.enable_mask        = 1ull<<5 /* txbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2832;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(1,3)[TXBAD]: Set by HW whenever tx st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(1,3);
    info.status_mask        = 1ull<<7 /* rxbad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(1,3);
    info.enable_mask        = 1ull<<7 /* rxbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2832;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(1,3)[RXBAD]: Set by HW whenever rx st machine reaches a  bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(1,3);
    info.status_mask        = 1ull<<8 /* rxlock */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(1,3);
    info.enable_mask        = 1ull<<8 /* rxlock_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2832;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(1,3)[RXLOCK]: Set by HW whenever code group Sync or bit lock\n"
        "    failure occurs\n"
        "    Cannot fire in loopback1 mode\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(1,3);
    info.status_mask        = 1ull<<9 /* an_bad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(1,3);
    info.enable_mask        = 1ull<<9 /* an_bad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2832;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(1,3)[AN_BAD]: Set by HW whenever AN st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(1,3);
    info.status_mask        = 1ull<<10 /* sync_bad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(1,3);
    info.enable_mask        = 1ull<<10 /* sync_bad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2832;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(1,3)[SYNC_BAD]: Set by HW whenever rx sync st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(1,3);
    info.status_mask        = 1ull<<12 /* dbg_sync */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(1,3);
    info.enable_mask        = 1ull<<12 /* dbg_sync_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2832;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(1,3)[DBG_SYNC]: Code Group sync failure debug help\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_PCSX_INTX_REG(2,3) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(2,3);
    info.status_mask        = 1ull<<2 /* an_err */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(2,3);
    info.enable_mask        = 1ull<<2 /* an_err_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2848;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(2,3)[AN_ERR]: AN Error, AN resolution function failed\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(2,3);
    info.status_mask        = 1ull<<3 /* txfifu */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(2,3);
    info.enable_mask        = 1ull<<3 /* txfifu_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2848;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(2,3)[TXFIFU]: Set whenever HW detects a TX fifo underflowflow\n"
        "    condition\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(2,3);
    info.status_mask        = 1ull<<4 /* txfifo */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(2,3);
    info.enable_mask        = 1ull<<4 /* txfifo_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2848;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(2,3)[TXFIFO]: Set whenever HW detects a TX fifo overflow\n"
        "    condition\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(2,3);
    info.status_mask        = 1ull<<5 /* txbad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(2,3);
    info.enable_mask        = 1ull<<5 /* txbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2848;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(2,3)[TXBAD]: Set by HW whenever tx st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(2,3);
    info.status_mask        = 1ull<<7 /* rxbad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(2,3);
    info.enable_mask        = 1ull<<7 /* rxbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2848;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(2,3)[RXBAD]: Set by HW whenever rx st machine reaches a  bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(2,3);
    info.status_mask        = 1ull<<8 /* rxlock */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(2,3);
    info.enable_mask        = 1ull<<8 /* rxlock_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2848;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(2,3)[RXLOCK]: Set by HW whenever code group Sync or bit lock\n"
        "    failure occurs\n"
        "    Cannot fire in loopback1 mode\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(2,3);
    info.status_mask        = 1ull<<9 /* an_bad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(2,3);
    info.enable_mask        = 1ull<<9 /* an_bad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2848;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(2,3)[AN_BAD]: Set by HW whenever AN st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(2,3);
    info.status_mask        = 1ull<<10 /* sync_bad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(2,3);
    info.enable_mask        = 1ull<<10 /* sync_bad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2848;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(2,3)[SYNC_BAD]: Set by HW whenever rx sync st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(2,3);
    info.status_mask        = 1ull<<12 /* dbg_sync */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(2,3);
    info.enable_mask        = 1ull<<12 /* dbg_sync_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2848;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(2,3)[DBG_SYNC]: Code Group sync failure debug help\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_PCSX_INTX_REG(3,3) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(3,3);
    info.status_mask        = 1ull<<2 /* an_err */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(3,3);
    info.enable_mask        = 1ull<<2 /* an_err_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2664;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(3,3)[AN_ERR]: AN Error, AN resolution function failed\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(3,3);
    info.status_mask        = 1ull<<3 /* txfifu */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(3,3);
    info.enable_mask        = 1ull<<3 /* txfifu_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2664;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(3,3)[TXFIFU]: Set whenever HW detects a TX fifo underflowflow\n"
        "    condition\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(3,3);
    info.status_mask        = 1ull<<4 /* txfifo */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(3,3);
    info.enable_mask        = 1ull<<4 /* txfifo_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2664;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(3,3)[TXFIFO]: Set whenever HW detects a TX fifo overflow\n"
        "    condition\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(3,3);
    info.status_mask        = 1ull<<5 /* txbad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(3,3);
    info.enable_mask        = 1ull<<5 /* txbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2664;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(3,3)[TXBAD]: Set by HW whenever tx st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(3,3);
    info.status_mask        = 1ull<<7 /* rxbad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(3,3);
    info.enable_mask        = 1ull<<7 /* rxbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2664;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(3,3)[RXBAD]: Set by HW whenever rx st machine reaches a  bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(3,3);
    info.status_mask        = 1ull<<8 /* rxlock */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(3,3);
    info.enable_mask        = 1ull<<8 /* rxlock_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2664;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(3,3)[RXLOCK]: Set by HW whenever code group Sync or bit lock\n"
        "    failure occurs\n"
        "    Cannot fire in loopback1 mode\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(3,3);
    info.status_mask        = 1ull<<9 /* an_bad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(3,3);
    info.enable_mask        = 1ull<<9 /* an_bad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2664;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(3,3)[AN_BAD]: Set by HW whenever AN st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(3,3);
    info.status_mask        = 1ull<<10 /* sync_bad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(3,3);
    info.enable_mask        = 1ull<<10 /* sync_bad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2664;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(3,3)[SYNC_BAD]: Set by HW whenever rx sync st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(3,3);
    info.status_mask        = 1ull<<12 /* dbg_sync */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(3,3);
    info.enable_mask        = 1ull<<12 /* dbg_sync_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2664;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(3,3)[DBG_SYNC]: Code Group sync failure debug help\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_PCSX_INTX_REG(0,4) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,4);
    info.status_mask        = 1ull<<2 /* an_err */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,4);
    info.enable_mask        = 1ull<<2 /* an_err_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3072;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,4)[AN_ERR]: AN Error, AN resolution function failed\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,4);
    info.status_mask        = 1ull<<3 /* txfifu */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,4);
    info.enable_mask        = 1ull<<3 /* txfifu_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3072;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,4)[TXFIFU]: Set whenever HW detects a TX fifo underflowflow\n"
        "    condition\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,4);
    info.status_mask        = 1ull<<4 /* txfifo */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,4);
    info.enable_mask        = 1ull<<4 /* txfifo_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3072;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,4)[TXFIFO]: Set whenever HW detects a TX fifo overflow\n"
        "    condition\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,4);
    info.status_mask        = 1ull<<5 /* txbad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,4);
    info.enable_mask        = 1ull<<5 /* txbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3072;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,4)[TXBAD]: Set by HW whenever tx st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,4);
    info.status_mask        = 1ull<<7 /* rxbad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,4);
    info.enable_mask        = 1ull<<7 /* rxbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3072;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,4)[RXBAD]: Set by HW whenever rx st machine reaches a  bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,4);
    info.status_mask        = 1ull<<8 /* rxlock */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,4);
    info.enable_mask        = 1ull<<8 /* rxlock_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3072;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,4)[RXLOCK]: Set by HW whenever code group Sync or bit lock\n"
        "    failure occurs\n"
        "    Cannot fire in loopback1 mode\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,4);
    info.status_mask        = 1ull<<9 /* an_bad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,4);
    info.enable_mask        = 1ull<<9 /* an_bad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3072;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,4)[AN_BAD]: Set by HW whenever AN st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,4);
    info.status_mask        = 1ull<<10 /* sync_bad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,4);
    info.enable_mask        = 1ull<<10 /* sync_bad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3072;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,4)[SYNC_BAD]: Set by HW whenever rx sync st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(0,4);
    info.status_mask        = 1ull<<12 /* dbg_sync */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(0,4);
    info.enable_mask        = 1ull<<12 /* dbg_sync_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3072;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(0,4)[DBG_SYNC]: Code Group sync failure debug help\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_PCSX_INTX_REG(1,4) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(1,4);
    info.status_mask        = 1ull<<2 /* an_err */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(1,4);
    info.enable_mask        = 1ull<<2 /* an_err_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3088;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(1,4)[AN_ERR]: AN Error, AN resolution function failed\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(1,4);
    info.status_mask        = 1ull<<3 /* txfifu */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(1,4);
    info.enable_mask        = 1ull<<3 /* txfifu_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3088;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(1,4)[TXFIFU]: Set whenever HW detects a TX fifo underflowflow\n"
        "    condition\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(1,4);
    info.status_mask        = 1ull<<4 /* txfifo */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(1,4);
    info.enable_mask        = 1ull<<4 /* txfifo_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3088;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(1,4)[TXFIFO]: Set whenever HW detects a TX fifo overflow\n"
        "    condition\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(1,4);
    info.status_mask        = 1ull<<5 /* txbad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(1,4);
    info.enable_mask        = 1ull<<5 /* txbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3088;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(1,4)[TXBAD]: Set by HW whenever tx st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(1,4);
    info.status_mask        = 1ull<<7 /* rxbad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(1,4);
    info.enable_mask        = 1ull<<7 /* rxbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3088;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(1,4)[RXBAD]: Set by HW whenever rx st machine reaches a  bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(1,4);
    info.status_mask        = 1ull<<8 /* rxlock */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(1,4);
    info.enable_mask        = 1ull<<8 /* rxlock_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3088;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(1,4)[RXLOCK]: Set by HW whenever code group Sync or bit lock\n"
        "    failure occurs\n"
        "    Cannot fire in loopback1 mode\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(1,4);
    info.status_mask        = 1ull<<9 /* an_bad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(1,4);
    info.enable_mask        = 1ull<<9 /* an_bad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3088;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(1,4)[AN_BAD]: Set by HW whenever AN st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(1,4);
    info.status_mask        = 1ull<<10 /* sync_bad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(1,4);
    info.enable_mask        = 1ull<<10 /* sync_bad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3088;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(1,4)[SYNC_BAD]: Set by HW whenever rx sync st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(1,4);
    info.status_mask        = 1ull<<12 /* dbg_sync */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(1,4);
    info.enable_mask        = 1ull<<12 /* dbg_sync_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3088;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(1,4)[DBG_SYNC]: Code Group sync failure debug help\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_PCSX_INTX_REG(2,4) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(2,4);
    info.status_mask        = 1ull<<2 /* an_err */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(2,4);
    info.enable_mask        = 1ull<<2 /* an_err_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3104;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(2,4)[AN_ERR]: AN Error, AN resolution function failed\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(2,4);
    info.status_mask        = 1ull<<3 /* txfifu */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(2,4);
    info.enable_mask        = 1ull<<3 /* txfifu_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3104;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(2,4)[TXFIFU]: Set whenever HW detects a TX fifo underflowflow\n"
        "    condition\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(2,4);
    info.status_mask        = 1ull<<4 /* txfifo */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(2,4);
    info.enable_mask        = 1ull<<4 /* txfifo_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3104;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(2,4)[TXFIFO]: Set whenever HW detects a TX fifo overflow\n"
        "    condition\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(2,4);
    info.status_mask        = 1ull<<5 /* txbad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(2,4);
    info.enable_mask        = 1ull<<5 /* txbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3104;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(2,4)[TXBAD]: Set by HW whenever tx st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(2,4);
    info.status_mask        = 1ull<<7 /* rxbad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(2,4);
    info.enable_mask        = 1ull<<7 /* rxbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3104;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(2,4)[RXBAD]: Set by HW whenever rx st machine reaches a  bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(2,4);
    info.status_mask        = 1ull<<8 /* rxlock */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(2,4);
    info.enable_mask        = 1ull<<8 /* rxlock_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3104;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(2,4)[RXLOCK]: Set by HW whenever code group Sync or bit lock\n"
        "    failure occurs\n"
        "    Cannot fire in loopback1 mode\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(2,4);
    info.status_mask        = 1ull<<9 /* an_bad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(2,4);
    info.enable_mask        = 1ull<<9 /* an_bad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3104;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(2,4)[AN_BAD]: Set by HW whenever AN st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(2,4);
    info.status_mask        = 1ull<<10 /* sync_bad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(2,4);
    info.enable_mask        = 1ull<<10 /* sync_bad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3104;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(2,4)[SYNC_BAD]: Set by HW whenever rx sync st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(2,4);
    info.status_mask        = 1ull<<12 /* dbg_sync */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(2,4);
    info.enable_mask        = 1ull<<12 /* dbg_sync_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3104;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(2,4)[DBG_SYNC]: Code Group sync failure debug help\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_PCSX_INTX_REG(3,4) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(3,4);
    info.status_mask        = 1ull<<2 /* an_err */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(3,4);
    info.enable_mask        = 1ull<<2 /* an_err_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3120;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(3,4)[AN_ERR]: AN Error, AN resolution function failed\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(3,4);
    info.status_mask        = 1ull<<3 /* txfifu */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(3,4);
    info.enable_mask        = 1ull<<3 /* txfifu_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3120;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(3,4)[TXFIFU]: Set whenever HW detects a TX fifo underflowflow\n"
        "    condition\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(3,4);
    info.status_mask        = 1ull<<4 /* txfifo */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(3,4);
    info.enable_mask        = 1ull<<4 /* txfifo_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3120;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(3,4)[TXFIFO]: Set whenever HW detects a TX fifo overflow\n"
        "    condition\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(3,4);
    info.status_mask        = 1ull<<5 /* txbad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(3,4);
    info.enable_mask        = 1ull<<5 /* txbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3120;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(3,4)[TXBAD]: Set by HW whenever tx st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(3,4);
    info.status_mask        = 1ull<<7 /* rxbad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(3,4);
    info.enable_mask        = 1ull<<7 /* rxbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3120;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(3,4)[RXBAD]: Set by HW whenever rx st machine reaches a  bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(3,4);
    info.status_mask        = 1ull<<8 /* rxlock */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(3,4);
    info.enable_mask        = 1ull<<8 /* rxlock_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3120;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(3,4)[RXLOCK]: Set by HW whenever code group Sync or bit lock\n"
        "    failure occurs\n"
        "    Cannot fire in loopback1 mode\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(3,4);
    info.status_mask        = 1ull<<9 /* an_bad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(3,4);
    info.enable_mask        = 1ull<<9 /* an_bad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3120;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(3,4)[AN_BAD]: Set by HW whenever AN st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(3,4);
    info.status_mask        = 1ull<<10 /* sync_bad */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(3,4);
    info.enable_mask        = 1ull<<10 /* sync_bad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3120;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(3,4)[SYNC_BAD]: Set by HW whenever rx sync st machine reaches a bad\n"
        "    state. Should never be set during normal operation\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSX_INTX_REG(3,4);
    info.status_mask        = 1ull<<12 /* dbg_sync */;
    info.enable_addr        = CVMX_PCSX_INTX_EN_REG(3,4);
    info.enable_mask        = 1ull<<12 /* dbg_sync_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3120;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(3,4)[DBG_SYNC]: Code Group sync failure debug help\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_PCSXX_INT_REG(0) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSXX_INT_REG(0);
    info.status_mask        = 1ull<<0 /* txflt */;
    info.enable_addr        = CVMX_PCSXX_INT_EN_REG(0);
    info.enable_mask        = 1ull<<0 /* txflt_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2048;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2048;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2048;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSXX_INT_REG(0)[RXSYNBAD]: Set when RX code grp sync st machine in bad state\n"
        "    in one of the 4 xaui lanes\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSXX_INT_REG(0);
    info.status_mask        = 1ull<<3 /* bitlckls */;
    info.enable_addr        = CVMX_PCSXX_INT_EN_REG(0);
    info.enable_mask        = 1ull<<3 /* bitlckls_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2048;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSXX_INT_REG(0)[BITLCKLS]: Set when Bit lock lost on 1 or more xaui lanes\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSXX_INT_REG(0);
    info.status_mask        = 1ull<<4 /* synlos */;
    info.enable_addr        = CVMX_PCSXX_INT_EN_REG(0);
    info.enable_mask        = 1ull<<4 /* synlos_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2048;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
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
    info.group_index        = 2048;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSXX_INT_REG(0)[ALGNLOS]: Set when XAUI lanes lose alignment\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSXX_INT_REG(0);
    info.status_mask        = 1ull<<6 /* dbg_sync */;
    info.enable_addr        = CVMX_PCSXX_INT_EN_REG(0);
    info.enable_mask        = 1ull<<6 /* dbg_sync_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2048;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSXX_INT_REG(0)[DBG_SYNC]: Code Group sync failure debug help, see Note below\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_PCSXX_INT_REG(1) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSXX_INT_REG(1);
    info.status_mask        = 1ull<<0 /* txflt */;
    info.enable_addr        = CVMX_PCSXX_INT_EN_REG(1);
    info.enable_mask        = 1ull<<0 /* txflt_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2368;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSXX_INT_REG(1)[TXFLT]: None defined at this time, always 0x0\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSXX_INT_REG(1);
    info.status_mask        = 1ull<<1 /* rxbad */;
    info.enable_addr        = CVMX_PCSXX_INT_EN_REG(1);
    info.enable_mask        = 1ull<<1 /* rxbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2368;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSXX_INT_REG(1)[RXBAD]: Set when RX state machine in bad state\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSXX_INT_REG(1);
    info.status_mask        = 1ull<<2 /* rxsynbad */;
    info.enable_addr        = CVMX_PCSXX_INT_EN_REG(1);
    info.enable_mask        = 1ull<<2 /* rxsynbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2368;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSXX_INT_REG(1)[RXSYNBAD]: Set when RX code grp sync st machine in bad state\n"
        "    in one of the 4 xaui lanes\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSXX_INT_REG(1);
    info.status_mask        = 1ull<<3 /* bitlckls */;
    info.enable_addr        = CVMX_PCSXX_INT_EN_REG(1);
    info.enable_mask        = 1ull<<3 /* bitlckls_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2368;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSXX_INT_REG(1)[BITLCKLS]: Set when Bit lock lost on 1 or more xaui lanes\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSXX_INT_REG(1);
    info.status_mask        = 1ull<<4 /* synlos */;
    info.enable_addr        = CVMX_PCSXX_INT_EN_REG(1);
    info.enable_mask        = 1ull<<4 /* synlos_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2368;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSXX_INT_REG(1)[SYNLOS]: Set when Code group sync lost on 1 or more  lanes\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSXX_INT_REG(1);
    info.status_mask        = 1ull<<5 /* algnlos */;
    info.enable_addr        = CVMX_PCSXX_INT_EN_REG(1);
    info.enable_mask        = 1ull<<5 /* algnlos_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2368;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSXX_INT_REG(1)[ALGNLOS]: Set when XAUI lanes lose alignment\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSXX_INT_REG(1);
    info.status_mask        = 1ull<<6 /* dbg_sync */;
    info.enable_addr        = CVMX_PCSXX_INT_EN_REG(1);
    info.enable_mask        = 1ull<<6 /* dbg_sync_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2368;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSXX_INT_REG(1)[DBG_SYNC]: Code Group sync failure debug help, see Note below\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_PCSXX_INT_REG(2) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSXX_INT_REG(2);
    info.status_mask        = 1ull<<0 /* txflt */;
    info.enable_addr        = CVMX_PCSXX_INT_EN_REG(2);
    info.enable_mask        = 1ull<<0 /* txflt_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2560;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSXX_INT_REG(2)[TXFLT]: None defined at this time, always 0x0\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSXX_INT_REG(2);
    info.status_mask        = 1ull<<1 /* rxbad */;
    info.enable_addr        = CVMX_PCSXX_INT_EN_REG(2);
    info.enable_mask        = 1ull<<1 /* rxbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2560;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSXX_INT_REG(2)[RXBAD]: Set when RX state machine in bad state\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSXX_INT_REG(2);
    info.status_mask        = 1ull<<2 /* rxsynbad */;
    info.enable_addr        = CVMX_PCSXX_INT_EN_REG(2);
    info.enable_mask        = 1ull<<2 /* rxsynbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2560;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSXX_INT_REG(2)[RXSYNBAD]: Set when RX code grp sync st machine in bad state\n"
        "    in one of the 4 xaui lanes\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSXX_INT_REG(2);
    info.status_mask        = 1ull<<3 /* bitlckls */;
    info.enable_addr        = CVMX_PCSXX_INT_EN_REG(2);
    info.enable_mask        = 1ull<<3 /* bitlckls_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2560;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSXX_INT_REG(2)[BITLCKLS]: Set when Bit lock lost on 1 or more xaui lanes\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSXX_INT_REG(2);
    info.status_mask        = 1ull<<4 /* synlos */;
    info.enable_addr        = CVMX_PCSXX_INT_EN_REG(2);
    info.enable_mask        = 1ull<<4 /* synlos_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2560;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSXX_INT_REG(2)[SYNLOS]: Set when Code group sync lost on 1 or more  lanes\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSXX_INT_REG(2);
    info.status_mask        = 1ull<<5 /* algnlos */;
    info.enable_addr        = CVMX_PCSXX_INT_EN_REG(2);
    info.enable_mask        = 1ull<<5 /* algnlos_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2560;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSXX_INT_REG(2)[ALGNLOS]: Set when XAUI lanes lose alignment\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSXX_INT_REG(2);
    info.status_mask        = 1ull<<6 /* dbg_sync */;
    info.enable_addr        = CVMX_PCSXX_INT_EN_REG(2);
    info.enable_mask        = 1ull<<6 /* dbg_sync_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2560;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSXX_INT_REG(2)[DBG_SYNC]: Code Group sync failure debug help, see Note below\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_PCSXX_INT_REG(3) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSXX_INT_REG(3);
    info.status_mask        = 1ull<<0 /* txflt */;
    info.enable_addr        = CVMX_PCSXX_INT_EN_REG(3);
    info.enable_mask        = 1ull<<0 /* txflt_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2816;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSXX_INT_REG(3)[TXFLT]: None defined at this time, always 0x0\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSXX_INT_REG(3);
    info.status_mask        = 1ull<<1 /* rxbad */;
    info.enable_addr        = CVMX_PCSXX_INT_EN_REG(3);
    info.enable_mask        = 1ull<<1 /* rxbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2816;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSXX_INT_REG(3)[RXBAD]: Set when RX state machine in bad state\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSXX_INT_REG(3);
    info.status_mask        = 1ull<<2 /* rxsynbad */;
    info.enable_addr        = CVMX_PCSXX_INT_EN_REG(3);
    info.enable_mask        = 1ull<<2 /* rxsynbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2816;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSXX_INT_REG(3)[RXSYNBAD]: Set when RX code grp sync st machine in bad state\n"
        "    in one of the 4 xaui lanes\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSXX_INT_REG(3);
    info.status_mask        = 1ull<<3 /* bitlckls */;
    info.enable_addr        = CVMX_PCSXX_INT_EN_REG(3);
    info.enable_mask        = 1ull<<3 /* bitlckls_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2816;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSXX_INT_REG(3)[BITLCKLS]: Set when Bit lock lost on 1 or more xaui lanes\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSXX_INT_REG(3);
    info.status_mask        = 1ull<<4 /* synlos */;
    info.enable_addr        = CVMX_PCSXX_INT_EN_REG(3);
    info.enable_mask        = 1ull<<4 /* synlos_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2816;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSXX_INT_REG(3)[SYNLOS]: Set when Code group sync lost on 1 or more  lanes\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSXX_INT_REG(3);
    info.status_mask        = 1ull<<5 /* algnlos */;
    info.enable_addr        = CVMX_PCSXX_INT_EN_REG(3);
    info.enable_mask        = 1ull<<5 /* algnlos_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2816;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSXX_INT_REG(3)[ALGNLOS]: Set when XAUI lanes lose alignment\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSXX_INT_REG(3);
    info.status_mask        = 1ull<<6 /* dbg_sync */;
    info.enable_addr        = CVMX_PCSXX_INT_EN_REG(3);
    info.enable_mask        = 1ull<<6 /* dbg_sync_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 2816;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSXX_INT_REG(3)[DBG_SYNC]: Code Group sync failure debug help, see Note below\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_PCSXX_INT_REG(4) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSXX_INT_REG(4);
    info.status_mask        = 1ull<<0 /* txflt */;
    info.enable_addr        = CVMX_PCSXX_INT_EN_REG(4);
    info.enable_mask        = 1ull<<0 /* txflt_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3072;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSXX_INT_REG(4)[TXFLT]: None defined at this time, always 0x0\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSXX_INT_REG(4);
    info.status_mask        = 1ull<<1 /* rxbad */;
    info.enable_addr        = CVMX_PCSXX_INT_EN_REG(4);
    info.enable_mask        = 1ull<<1 /* rxbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3072;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSXX_INT_REG(4)[RXBAD]: Set when RX state machine in bad state\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSXX_INT_REG(4);
    info.status_mask        = 1ull<<2 /* rxsynbad */;
    info.enable_addr        = CVMX_PCSXX_INT_EN_REG(4);
    info.enable_mask        = 1ull<<2 /* rxsynbad_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3072;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSXX_INT_REG(4)[RXSYNBAD]: Set when RX code grp sync st machine in bad state\n"
        "    in one of the 4 xaui lanes\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSXX_INT_REG(4);
    info.status_mask        = 1ull<<3 /* bitlckls */;
    info.enable_addr        = CVMX_PCSXX_INT_EN_REG(4);
    info.enable_mask        = 1ull<<3 /* bitlckls_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3072;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSXX_INT_REG(4)[BITLCKLS]: Set when Bit lock lost on 1 or more xaui lanes\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSXX_INT_REG(4);
    info.status_mask        = 1ull<<4 /* synlos */;
    info.enable_addr        = CVMX_PCSXX_INT_EN_REG(4);
    info.enable_mask        = 1ull<<4 /* synlos_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3072;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSXX_INT_REG(4)[SYNLOS]: Set when Code group sync lost on 1 or more  lanes\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSXX_INT_REG(4);
    info.status_mask        = 1ull<<5 /* algnlos */;
    info.enable_addr        = CVMX_PCSXX_INT_EN_REG(4);
    info.enable_mask        = 1ull<<5 /* algnlos_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3072;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSXX_INT_REG(4)[ALGNLOS]: Set when XAUI lanes lose alignment\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PCSXX_INT_REG(4);
    info.status_mask        = 1ull<<6 /* dbg_sync */;
    info.enable_addr        = CVMX_PCSXX_INT_EN_REG(4);
    info.enable_mask        = 1ull<<6 /* dbg_sync_en */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_ETHERNET;
    info.group_index        = 3072;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU2_SUM_PPX_IP2(0);
    info.parent.status_mask = 1ull<<6 /* pkt */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSXX_INT_REG(4)[DBG_SYNC]: Code Group sync failure debug help, see Note below\n";
    fail |= cvmx_error_add(&info);

    return fail;
}

