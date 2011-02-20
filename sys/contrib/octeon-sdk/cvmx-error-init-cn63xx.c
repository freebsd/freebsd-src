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
 * Automatically generated error messages for cn63xx.
 *
 * This file is auto generated. Do not edit.
 *
 * <hr>$Revision$<hr>
 *
 * <hr><h2>Error tree for CN63XX</h2>
 * @dot
 * digraph cn63xx
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
 *     cvmx_ciu_block_int [label="CIU_BLOCK_INT|<l2c>l2c|<ipd>ipd|<pow>pow|<rad>rad|<asxpcs0>asxpcs0|<pip>pip|<pko>pko|<pem0>pem0|<pem1>pem1|<fpa>fpa|<usb>usb|<mio>mio|<dfm>dfm|<tim>tim|<lmc0>lmc0|<key>key|<gmx0>gmx0|<iob>iob|<agl>agl|<zip>zip|<dfa>dfa|<srio0>srio0|<srio1>srio1|<sli>sli|<dpi>dpi"];
 *     cvmx_l2c_int_reg [label="L2C_INT_REG|<holerd>holerd|<holewr>holewr|<vrtwr>vrtwr|<vrtidrng>vrtidrng|<vrtadrng>vrtadrng|<vrtpe>vrtpe|<bigwr>bigwr|<bigrd>bigrd|<tad0>tad0"];
 *     cvmx_l2c_err_tdt0 [label="L2C_ERR_TDTX(0)|<vsbe>vsbe|<vdbe>vdbe|<sbe>sbe|<dbe>dbe"];
 *     cvmx_l2c_int_reg:tad0:e -> cvmx_l2c_err_tdt0 [label="tad0"];
 *     cvmx_l2c_err_ttg0 [label="L2C_ERR_TTGX(0)|<noway>noway|<sbe>sbe|<dbe>dbe"];
 *     cvmx_l2c_int_reg:tad0:e -> cvmx_l2c_err_ttg0 [label="tad0"];
 *     cvmx_ciu_block_int:l2c:e -> cvmx_l2c_int_reg [label="l2c"];
 *     cvmx_ipd_int_sum [label="IPD_INT_SUM|<prc_par0>prc_par0|<prc_par1>prc_par1|<prc_par2>prc_par2|<prc_par3>prc_par3|<bp_sub>bp_sub|<dc_ovr>dc_ovr|<cc_ovr>cc_ovr|<c_coll>c_coll|<d_coll>d_coll|<bc_ovr>bc_ovr"];
 *     cvmx_ciu_block_int:ipd:e -> cvmx_ipd_int_sum [label="ipd"];
 *     cvmx_pow_ecc_err [label="POW_ECC_ERR|<sbe>sbe|<dbe>dbe|<rpe>rpe|<iop>iop"];
 *     cvmx_ciu_block_int:pow:e -> cvmx_pow_ecc_err [label="pow"];
 *     cvmx_rad_reg_error [label="RAD_REG_ERROR|<doorbell>doorbell"];
 *     cvmx_ciu_block_int:rad:e -> cvmx_rad_reg_error [label="rad"];
 *     cvmx_pcs0_int0_reg [label="PCSX_INTX_REG(0,0)|<an_err>an_err|<txfifu>txfifu|<txfifo>txfifo|<txbad>txbad|<rxbad>rxbad|<rxlock>rxlock|<an_bad>an_bad|<sync_bad>sync_bad|<dbg_sync>dbg_sync"];
 *     cvmx_ciu_block_int:asxpcs0:e -> cvmx_pcs0_int0_reg [label="asxpcs0"];
 *     cvmx_pcs0_int1_reg [label="PCSX_INTX_REG(1,0)|<an_err>an_err|<txfifu>txfifu|<txfifo>txfifo|<txbad>txbad|<rxbad>rxbad|<rxlock>rxlock|<an_bad>an_bad|<sync_bad>sync_bad|<dbg_sync>dbg_sync"];
 *     cvmx_ciu_block_int:asxpcs0:e -> cvmx_pcs0_int1_reg [label="asxpcs0"];
 *     cvmx_pcs0_int2_reg [label="PCSX_INTX_REG(2,0)|<an_err>an_err|<txfifu>txfifu|<txfifo>txfifo|<txbad>txbad|<rxbad>rxbad|<rxlock>rxlock|<an_bad>an_bad|<sync_bad>sync_bad|<dbg_sync>dbg_sync"];
 *     cvmx_ciu_block_int:asxpcs0:e -> cvmx_pcs0_int2_reg [label="asxpcs0"];
 *     cvmx_pcs0_int3_reg [label="PCSX_INTX_REG(3,0)|<an_err>an_err|<txfifu>txfifu|<txfifo>txfifo|<txbad>txbad|<rxbad>rxbad|<rxlock>rxlock|<an_bad>an_bad|<sync_bad>sync_bad|<dbg_sync>dbg_sync"];
 *     cvmx_ciu_block_int:asxpcs0:e -> cvmx_pcs0_int3_reg [label="asxpcs0"];
 *     cvmx_pcsx0_int_reg [label="PCSXX_INT_REG(0)|<txflt>txflt|<rxbad>rxbad|<rxsynbad>rxsynbad|<synlos>synlos|<algnlos>algnlos|<dbg_sync>dbg_sync"];
 *     cvmx_ciu_block_int:asxpcs0:e -> cvmx_pcsx0_int_reg [label="asxpcs0"];
 *     cvmx_pip_int_reg [label="PIP_INT_REG|<prtnxa>prtnxa|<badtag>badtag|<skprunt>skprunt|<todoovr>todoovr|<feperr>feperr|<beperr>beperr|<punyerr>punyerr"];
 *     cvmx_ciu_block_int:pip:e -> cvmx_pip_int_reg [label="pip"];
 *     cvmx_pko_reg_error [label="PKO_REG_ERROR|<parity>parity|<doorbell>doorbell|<currzero>currzero"];
 *     cvmx_ciu_block_int:pko:e -> cvmx_pko_reg_error [label="pko"];
 *     cvmx_pem0_int_sum [label="PEMX_INT_SUM(0)|<se>se|<up_b1>up_b1|<up_b2>up_b2|<up_bx>up_bx|<un_b1>un_b1|<un_b2>un_b2|<un_bx>un_bx|<rdlk>rdlk|<crs_er>crs_er|<crs_dr>crs_dr|<exc>exc"];
 *     cvmx_pem0_dbg_info [label="PEMX_DBG_INFO(0)|<spoison>spoison|<rtlplle>rtlplle|<recrce>recrce|<rpoison>rpoison|<rcemrc>rcemrc|<rnfemrc>rnfemrc|<rfemrc>rfemrc|<rpmerc>rpmerc|<rptamrc>rptamrc|<rumep>rumep|<rvdm>rvdm|<acto>acto|<rte>rte|<mre>mre|<rdwdle>rdwdle|<rtwdle>rtwdle|<dpeoosd>dpeoosd|<fcpvwt>fcpvwt|<rpe>rpe|<fcuv>fcuv|<rqo>rqo|<rauc>rauc|<racur>racur|<racca>racca|<caar>caar|<rarwdns>rarwdns|<ramtlp>ramtlp|<racpp>racpp|<rawwpp>rawwpp|<ecrc_e>ecrc_e"];
 *     cvmx_pem0_int_sum:exc:e -> cvmx_pem0_dbg_info [label="exc"];
 *     cvmx_ciu_block_int:pem0:e -> cvmx_pem0_int_sum [label="pem0"];
 *     cvmx_pem1_int_sum [label="PEMX_INT_SUM(1)|<se>se|<up_b1>up_b1|<up_b2>up_b2|<up_bx>up_bx|<un_b1>un_b1|<un_b2>un_b2|<un_bx>un_bx|<rdlk>rdlk|<crs_er>crs_er|<crs_dr>crs_dr|<exc>exc"];
 *     cvmx_pem1_dbg_info [label="PEMX_DBG_INFO(1)|<spoison>spoison|<rtlplle>rtlplle|<recrce>recrce|<rpoison>rpoison|<rcemrc>rcemrc|<rnfemrc>rnfemrc|<rfemrc>rfemrc|<rpmerc>rpmerc|<rptamrc>rptamrc|<rumep>rumep|<rvdm>rvdm|<acto>acto|<rte>rte|<mre>mre|<rdwdle>rdwdle|<rtwdle>rtwdle|<dpeoosd>dpeoosd|<fcpvwt>fcpvwt|<rpe>rpe|<fcuv>fcuv|<rqo>rqo|<rauc>rauc|<racur>racur|<racca>racca|<caar>caar|<rarwdns>rarwdns|<ramtlp>ramtlp|<racpp>racpp|<rawwpp>rawwpp|<ecrc_e>ecrc_e"];
 *     cvmx_pem1_int_sum:exc:e -> cvmx_pem1_dbg_info [label="exc"];
 *     cvmx_ciu_block_int:pem1:e -> cvmx_pem1_int_sum [label="pem1"];
 *     cvmx_fpa_int_sum [label="FPA_INT_SUM|<fed0_sbe>fed0_sbe|<fed0_dbe>fed0_dbe|<fed1_sbe>fed1_sbe|<fed1_dbe>fed1_dbe|<q0_und>q0_und|<q0_coff>q0_coff|<q0_perr>q0_perr|<q1_und>q1_und|<q1_coff>q1_coff|<q1_perr>q1_perr|<q2_und>q2_und|<q2_coff>q2_coff|<q2_perr>q2_perr|<q3_und>q3_und|<q3_coff>q3_coff|<q3_perr>q3_perr|<q4_und>q4_und|<q4_coff>q4_coff|<q4_perr>q4_perr|<q5_und>q5_und|<q5_coff>q5_coff|<q5_perr>q5_perr|<q6_und>q6_und|<q6_coff>q6_coff|<q6_perr>q6_perr|<q7_und>q7_und|<q7_coff>q7_coff|<q7_perr>q7_perr|<pool0th>pool0th|<pool1th>pool1th|<pool2th>pool2th|<pool3th>pool3th|<pool4th>pool4th|<pool5th>pool5th|<pool6th>pool6th|<pool7th>pool7th|<free0>free0|<free1>free1|<free2>free2|<free3>free3|<free4>free4|<free5>free5|<free6>free6|<free7>free7"];
 *     cvmx_ciu_block_int:fpa:e -> cvmx_fpa_int_sum [label="fpa"];
 *     cvmx_uctl0_int_reg [label="UCTLX_INT_REG(0)|<pp_psh_f>pp_psh_f|<er_psh_f>er_psh_f|<or_psh_f>or_psh_f|<cf_psh_f>cf_psh_f|<wb_psh_f>wb_psh_f|<wb_pop_e>wb_pop_e|<oc_ovf_e>oc_ovf_e|<ec_ovf_e>ec_ovf_e"];
 *     cvmx_ciu_block_int:usb:e -> cvmx_uctl0_int_reg [label="usb"];
 *     cvmx_mio_boot_err [label="MIO_BOOT_ERR|<adr_err>adr_err|<wait_err>wait_err"];
 *     cvmx_ciu_block_int:mio:e -> cvmx_mio_boot_err [label="mio"];
 *     cvmx_mio_rst_int [label="MIO_RST_INT|<rst_link0>rst_link0|<rst_link1>rst_link1|<perst0>perst0|<perst1>perst1"];
 *     cvmx_ciu_block_int:mio:e -> cvmx_mio_rst_int [label="mio"];
 *     cvmx_dfm_fnt_stat [label="DFM_FNT_STAT|<sbe_err>sbe_err|<dbe_err>dbe_err"];
 *     cvmx_ciu_block_int:dfm:e -> cvmx_dfm_fnt_stat [label="dfm"];
 *     cvmx_tim_reg_error [label="TIM_REG_ERROR|<mask>mask"];
 *     cvmx_ciu_block_int:tim:e -> cvmx_tim_reg_error [label="tim"];
 *     cvmx_lmc0_int [label="LMCX_INT(0)|<sec_err>sec_err|<nxm_wr_err>nxm_wr_err|<ded_err>ded_err"];
 *     cvmx_ciu_block_int:lmc0:e -> cvmx_lmc0_int [label="lmc0"];
 *     cvmx_key_int_sum [label="KEY_INT_SUM|<ked0_sbe>ked0_sbe|<ked0_dbe>ked0_dbe|<ked1_sbe>ked1_sbe|<ked1_dbe>ked1_dbe"];
 *     cvmx_ciu_block_int:key:e -> cvmx_key_int_sum [label="key"];
 *     cvmx_gmx0_bad_reg [label="GMXX_BAD_REG(0)|<out_ovr>out_ovr|<loststat>loststat|<statovr>statovr|<inb_nxa>inb_nxa"];
 *     cvmx_ciu_block_int:gmx0:e -> cvmx_gmx0_bad_reg [label="gmx0"];
 *     cvmx_gmx0_rx0_int_reg [label="GMXX_RXX_INT_REG(0,0)|<carext>carext|<skperr>skperr|<ovrerr>ovrerr|<loc_fault>loc_fault|<rem_fault>rem_fault|<bad_seq>bad_seq|<bad_term>bad_term|<unsop>unsop|<uneop>uneop|<undat>undat|<hg2fld>hg2fld|<hg2cc>hg2cc"];
 *     cvmx_ciu_block_int:gmx0:e -> cvmx_gmx0_rx0_int_reg [label="gmx0"];
 *     cvmx_gmx0_rx1_int_reg [label="GMXX_RXX_INT_REG(1,0)|<carext>carext|<skperr>skperr|<ovrerr>ovrerr|<loc_fault>loc_fault|<rem_fault>rem_fault|<bad_seq>bad_seq|<bad_term>bad_term|<unsop>unsop|<uneop>uneop|<undat>undat|<hg2fld>hg2fld|<hg2cc>hg2cc"];
 *     cvmx_ciu_block_int:gmx0:e -> cvmx_gmx0_rx1_int_reg [label="gmx0"];
 *     cvmx_gmx0_rx2_int_reg [label="GMXX_RXX_INT_REG(2,0)|<carext>carext|<skperr>skperr|<ovrerr>ovrerr|<loc_fault>loc_fault|<rem_fault>rem_fault|<bad_seq>bad_seq|<bad_term>bad_term|<unsop>unsop|<uneop>uneop|<undat>undat|<hg2fld>hg2fld|<hg2cc>hg2cc"];
 *     cvmx_ciu_block_int:gmx0:e -> cvmx_gmx0_rx2_int_reg [label="gmx0"];
 *     cvmx_gmx0_rx3_int_reg [label="GMXX_RXX_INT_REG(3,0)|<carext>carext|<skperr>skperr|<ovrerr>ovrerr|<loc_fault>loc_fault|<rem_fault>rem_fault|<bad_seq>bad_seq|<bad_term>bad_term|<unsop>unsop|<uneop>uneop|<undat>undat|<hg2fld>hg2fld|<hg2cc>hg2cc"];
 *     cvmx_ciu_block_int:gmx0:e -> cvmx_gmx0_rx3_int_reg [label="gmx0"];
 *     cvmx_gmx0_tx_int_reg [label="GMXX_TX_INT_REG(0)|<pko_nxa>pko_nxa|<undflw>undflw|<ptp_lost>ptp_lost"];
 *     cvmx_ciu_block_int:gmx0:e -> cvmx_gmx0_tx_int_reg [label="gmx0"];
 *     cvmx_iob_int_sum [label="IOB_INT_SUM|<np_sop>np_sop|<np_eop>np_eop|<p_sop>p_sop|<p_eop>p_eop|<np_dat>np_dat|<p_dat>p_dat"];
 *     cvmx_ciu_block_int:iob:e -> cvmx_iob_int_sum [label="iob"];
 *     cvmx_agl_gmx_bad_reg [label="AGL_GMX_BAD_REG|<ovrflw>ovrflw|<txpop>txpop|<txpsh>txpsh|<ovrflw1>ovrflw1|<txpop1>txpop1|<txpsh1>txpsh1|<out_ovr>out_ovr|<loststat>loststat"];
 *     cvmx_ciu_block_int:agl:e -> cvmx_agl_gmx_bad_reg [label="agl"];
 *     cvmx_agl_gmx_rx0_int_reg [label="AGL_GMX_RXX_INT_REG(0)|<skperr>skperr|<ovrerr>ovrerr"];
 *     cvmx_ciu_block_int:agl:e -> cvmx_agl_gmx_rx0_int_reg [label="agl"];
 *     cvmx_agl_gmx_rx1_int_reg [label="AGL_GMX_RXX_INT_REG(1)|<skperr>skperr|<ovrerr>ovrerr"];
 *     cvmx_ciu_block_int:agl:e -> cvmx_agl_gmx_rx1_int_reg [label="agl"];
 *     cvmx_agl_gmx_tx_int_reg [label="AGL_GMX_TX_INT_REG|<pko_nxa>pko_nxa|<undflw>undflw"];
 *     cvmx_ciu_block_int:agl:e -> cvmx_agl_gmx_tx_int_reg [label="agl"];
 *     cvmx_zip_error [label="ZIP_ERROR|<doorbell>doorbell"];
 *     cvmx_ciu_block_int:zip:e -> cvmx_zip_error [label="zip"];
 *     cvmx_dfa_error [label="DFA_ERROR|<dblovf>dblovf|<dc0perr>dc0perr"];
 *     cvmx_ciu_block_int:dfa:e -> cvmx_dfa_error [label="dfa"];
 *     cvmx_srio0_int_reg [label="SRIOX_INT_REG(0)|<bar_err>bar_err|<deny_wr>deny_wr|<sli_err>sli_err|<mce_rx>mce_rx|<log_erb>log_erb|<phy_erb>phy_erb|<omsg_err>omsg_err|<pko_err>pko_err|<rtry_err>rtry_err|<f_error>f_error|<mac_buf>mac_buf|<degrad>degrad|<fail>fail|<ttl_tout>ttl_tout"];
 *     cvmx_ciu_block_int:srio0:e -> cvmx_srio0_int_reg [label="srio0"];
 *     cvmx_srio1_int_reg [label="SRIOX_INT_REG(1)|<bar_err>bar_err|<deny_wr>deny_wr|<sli_err>sli_err|<mce_rx>mce_rx|<log_erb>log_erb|<phy_erb>phy_erb|<omsg_err>omsg_err|<pko_err>pko_err|<rtry_err>rtry_err|<f_error>f_error|<mac_buf>mac_buf|<degrad>degrad|<fail>fail|<ttl_tout>ttl_tout"];
 *     cvmx_ciu_block_int:srio1:e -> cvmx_srio1_int_reg [label="srio1"];
 *     cvmx_sli_int_sum [label="PEXP_SLI_INT_SUM|<rml_to>rml_to|<reserved_1_1>reserved_1_1|<bar0_to>bar0_to|<iob2big>iob2big|<reserved_6_7>reserved_6_7|<m0_up_b0>m0_up_b0|<m0_up_wi>m0_up_wi|<m0_un_b0>m0_un_b0|<m0_un_wi>m0_un_wi|<m1_up_b0>m1_up_b0|<m1_up_wi>m1_up_wi|<m1_un_b0>m1_un_b0|<m1_un_wi>m1_un_wi|<pidbof>pidbof|<psldbof>psldbof|<pout_err>pout_err|<pin_bp>pin_bp|<pgl_err>pgl_err|<pdi_err>pdi_err|<pop_err>pop_err|<pins_err>pins_err|<sprt0_err>sprt0_err|<sprt1_err>sprt1_err|<ill_pad>ill_pad"];
 *     cvmx_ciu_block_int:sli:e -> cvmx_sli_int_sum [label="sli"];
 *     cvmx_dpi_int_reg [label="DPI_INT_REG|<nderr>nderr|<nfovr>nfovr|<dmadbo>dmadbo|<req_badadr>req_badadr|<req_badlen>req_badlen|<req_ovrflw>req_ovrflw|<req_undflw>req_undflw|<req_anull>req_anull|<req_inull>req_inull|<req_badfil>req_badfil|<sprt0_rst>sprt0_rst|<sprt1_rst>sprt1_rst"];
 *     cvmx_ciu_block_int:dpi:e -> cvmx_dpi_int_reg [label="dpi"];
 *     cvmx_dpi_pkt_err_rsp [label="DPI_PKT_ERR_RSP|<pkterr>pkterr"];
 *     cvmx_ciu_block_int:dpi:e -> cvmx_dpi_pkt_err_rsp [label="dpi"];
 *     cvmx_dpi_req_err_rsp [label="DPI_REQ_ERR_RSP|<qerr>qerr"];
 *     cvmx_ciu_block_int:dpi:e -> cvmx_dpi_req_err_rsp [label="dpi"];
 *     cvmx_dpi_req_err_rst [label="DPI_REQ_ERR_RST|<qerr>qerr"];
 *     cvmx_ciu_block_int:dpi:e -> cvmx_dpi_req_err_rst [label="dpi"];
 *     cvmx_pcs0_int0_reg -> cvmx_pcs0_int1_reg [style=invis];
 *     cvmx_pcs0_int1_reg -> cvmx_pcs0_int2_reg [style=invis];
 *     cvmx_pcs0_int2_reg -> cvmx_pcs0_int3_reg [style=invis];
 *     cvmx_pcs0_int3_reg -> cvmx_pcsx0_int_reg [style=invis];
 *     cvmx_mio_boot_err -> cvmx_mio_rst_int [style=invis];
 *     cvmx_gmx0_bad_reg -> cvmx_gmx0_rx0_int_reg [style=invis];
 *     cvmx_gmx0_rx0_int_reg -> cvmx_gmx0_rx1_int_reg [style=invis];
 *     cvmx_gmx0_rx1_int_reg -> cvmx_gmx0_rx2_int_reg [style=invis];
 *     cvmx_gmx0_rx2_int_reg -> cvmx_gmx0_rx3_int_reg [style=invis];
 *     cvmx_gmx0_rx3_int_reg -> cvmx_gmx0_tx_int_reg [style=invis];
 *     cvmx_agl_gmx_bad_reg -> cvmx_agl_gmx_rx0_int_reg [style=invis];
 *     cvmx_agl_gmx_rx0_int_reg -> cvmx_agl_gmx_rx1_int_reg [style=invis];
 *     cvmx_agl_gmx_rx1_int_reg -> cvmx_agl_gmx_tx_int_reg [style=invis];
 *     cvmx_dpi_int_reg -> cvmx_dpi_pkt_err_rsp [style=invis];
 *     cvmx_dpi_pkt_err_rsp -> cvmx_dpi_req_err_rsp [style=invis];
 *     cvmx_dpi_req_err_rsp -> cvmx_dpi_req_err_rst [style=invis];
 *     cvmx_root:root:e -> cvmx_ciu_block_int [label="root"];
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

int cvmx_error_initialize_cn63xx(void);

int cvmx_error_initialize_cn63xx(void)
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

    /* CVMX_CIU_BLOCK_INT */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<16 /* l2c */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<16 /* l2c */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<16 /* l2c */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<16 /* l2c */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<16 /* l2c */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<16 /* l2c */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<16 /* l2c */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<16 /* l2c */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<16 /* l2c */;
    info.func               = __cvmx_error_decode;
    info.user_info          = 0;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<9 /* ipd */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR IPD_INT_SUM[BC_OVR]: Set when the byte-count to send to IOB overflows.\n";
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<12 /* pow */;
    info.func               = __cvmx_error_handle_pow_ecc_err_iop;
    info.user_info          = (long)
        "ERROR POW_ECC_ERR[IOP]: Illegal operation errors\n";
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<14 /* rad */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR RAD_REG_ERROR[DOORBELL]: A doorbell count has overflowed\n";
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
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
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
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
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
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
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
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
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
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
    info.group_index        = 2;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
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
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
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
    info.group_index        = 3;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSX_INTX_REG(3,0)[DBG_SYNC]: Code Group sync failure debug help\n";
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
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
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<22 /* asxpcs0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PCSXX_INT_REG(0)[DBG_SYNC]: Code Group sync failure debug help, see Note below\n";
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<20 /* pip */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<20 /* pip */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PIP_INT_REG[PUNYERR]: Frame was received with length <=4B when CRC\n"
        "    stripping in IPD is enable\n";
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<10 /* pko */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PKO_REG_ERROR[CURRZERO]: A packet data pointer has size=0\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_PEMX_INT_SUM(0) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_INT_SUM(0);
    info.status_mask        = 1ull<<1 /* se */;
    info.enable_addr        = CVMX_PEMX_INT_ENB(0);
    info.enable_mask        = 1ull<<1 /* se */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<25 /* pem0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_INT_SUM(0)[SE]: System Error, RC Mode Only.\n"
        "    (cfg_sys_err_rc)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_INT_SUM(0);
    info.status_mask        = 1ull<<4 /* up_b1 */;
    info.enable_addr        = CVMX_PEMX_INT_ENB(0);
    info.enable_mask        = 1ull<<4 /* up_b1 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<25 /* pem0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_INT_SUM(0)[UP_B1]: Received P-TLP for Bar1 when bar1 index valid\n"
        "    is not set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_INT_SUM(0);
    info.status_mask        = 1ull<<5 /* up_b2 */;
    info.enable_addr        = CVMX_PEMX_INT_ENB(0);
    info.enable_mask        = 1ull<<5 /* up_b2 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<25 /* pem0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_INT_SUM(0)[UP_B2]: Received P-TLP for Bar2 when bar2 is disabeld.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_INT_SUM(0);
    info.status_mask        = 1ull<<6 /* up_bx */;
    info.enable_addr        = CVMX_PEMX_INT_ENB(0);
    info.enable_mask        = 1ull<<6 /* up_bx */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<25 /* pem0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_INT_SUM(0)[UP_BX]: Received P-TLP for an unknown Bar.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_INT_SUM(0);
    info.status_mask        = 1ull<<7 /* un_b1 */;
    info.enable_addr        = CVMX_PEMX_INT_ENB(0);
    info.enable_mask        = 1ull<<7 /* un_b1 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<25 /* pem0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_INT_SUM(0)[UN_B1]: Received N-TLP for Bar1 when bar1 index valid\n"
        "    is not set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_INT_SUM(0);
    info.status_mask        = 1ull<<8 /* un_b2 */;
    info.enable_addr        = CVMX_PEMX_INT_ENB(0);
    info.enable_mask        = 1ull<<8 /* un_b2 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<25 /* pem0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_INT_SUM(0)[UN_B2]: Received N-TLP for Bar2 when bar2 is disabled.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_INT_SUM(0);
    info.status_mask        = 1ull<<9 /* un_bx */;
    info.enable_addr        = CVMX_PEMX_INT_ENB(0);
    info.enable_mask        = 1ull<<9 /* un_bx */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<25 /* pem0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_INT_SUM(0)[UN_BX]: Received N-TLP for an unknown Bar.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_INT_SUM(0);
    info.status_mask        = 1ull<<11 /* rdlk */;
    info.enable_addr        = CVMX_PEMX_INT_ENB(0);
    info.enable_mask        = 1ull<<11 /* rdlk */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<25 /* pem0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_INT_SUM(0)[RDLK]: Received Read Lock TLP.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_INT_SUM(0);
    info.status_mask        = 1ull<<12 /* crs_er */;
    info.enable_addr        = CVMX_PEMX_INT_ENB(0);
    info.enable_mask        = 1ull<<12 /* crs_er */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<25 /* pem0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_INT_SUM(0)[CRS_ER]: Had a CRS Timeout when Retries were enabled.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_INT_SUM(0);
    info.status_mask        = 1ull<<13 /* crs_dr */;
    info.enable_addr        = CVMX_PEMX_INT_ENB(0);
    info.enable_mask        = 1ull<<13 /* crs_dr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<25 /* pem0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_INT_SUM(0)[CRS_DR]: Had a CRS Timeout when Retries were disabled.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_INT_SUM(0);
    info.status_mask        = 0;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<25 /* pem0 */;
    info.func               = __cvmx_error_decode;
    info.user_info          = 0;
    fail |= cvmx_error_add(&info);

    /* CVMX_PEMX_DBG_INFO(0) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(0);
    info.status_mask        = 1ull<<0 /* spoison */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<0 /* spoison */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(0);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(0)[SPOISON]: Poisoned TLP sent\n"
        "    peai__client0_tlp_ep & peai__client0_tlp_hv\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(0);
    info.status_mask        = 1ull<<2 /* rtlplle */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<2 /* rtlplle */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(0);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(0)[RTLPLLE]: Received TLP has link layer error\n"
        "    pedc_radm_trgt1_dllp_abort & pedc__radm_trgt1_eot\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(0);
    info.status_mask        = 1ull<<3 /* recrce */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<3 /* recrce */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(0);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(0)[RECRCE]: Received ECRC Error\n"
        "    pedc_radm_trgt1_ecrc_err & pedc__radm_trgt1_eot\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(0);
    info.status_mask        = 1ull<<4 /* rpoison */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<4 /* rpoison */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(0);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(0)[RPOISON]: Received Poisoned TLP\n"
        "    pedc__radm_trgt1_poisoned & pedc__radm_trgt1_hv\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(0);
    info.status_mask        = 1ull<<5 /* rcemrc */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<5 /* rcemrc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(0);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(0)[RCEMRC]: Received Correctable Error Message (RC Mode only)\n"
        "    pedc_radm_correctable_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(0);
    info.status_mask        = 1ull<<6 /* rnfemrc */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<6 /* rnfemrc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(0);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(0)[RNFEMRC]: Received Non-Fatal Error Message (RC Mode only)\n"
        "    pedc_radm_nonfatal_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(0);
    info.status_mask        = 1ull<<7 /* rfemrc */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<7 /* rfemrc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(0);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(0)[RFEMRC]: Received Fatal Error Message (RC Mode only)\n"
        "    pedc_radm_fatal_err\n"
        "    Bit set when a message with ERR_FATAL is set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(0);
    info.status_mask        = 1ull<<8 /* rpmerc */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<8 /* rpmerc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(0);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(0)[RPMERC]: Received PME Message (RC Mode only)\n"
        "    pedc_radm_pm_pme\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(0);
    info.status_mask        = 1ull<<9 /* rptamrc */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<9 /* rptamrc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(0);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(0)[RPTAMRC]: Received PME Turnoff Acknowledge Message\n"
        "    (RC Mode only)\n"
        "    pedc_radm_pm_to_ack\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(0);
    info.status_mask        = 1ull<<10 /* rumep */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<10 /* rumep */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(0);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(0)[RUMEP]: Received Unlock Message (EP Mode Only)\n"
        "    pedc_radm_msg_unlock\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(0);
    info.status_mask        = 1ull<<11 /* rvdm */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<11 /* rvdm */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(0);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(0)[RVDM]: Received Vendor-Defined Message\n"
        "    pedc_radm_vendor_msg\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(0);
    info.status_mask        = 1ull<<12 /* acto */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<12 /* acto */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(0);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(0)[ACTO]: A Completion Timeout Occured\n"
        "    pedc_radm_cpl_timeout\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(0);
    info.status_mask        = 1ull<<13 /* rte */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<13 /* rte */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(0);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(0)[RTE]: Replay Timer Expired\n"
        "    xdlh_replay_timeout_err\n"
        "    This bit is set when the REPLAY_TIMER expires in\n"
        "    the PCIE core. The probability of this bit being\n"
        "    set will increase with the traffic load.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(0);
    info.status_mask        = 1ull<<14 /* mre */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<14 /* mre */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(0);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(0)[MRE]: Max Retries Exceeded\n"
        "    xdlh_replay_num_rlover_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(0);
    info.status_mask        = 1ull<<15 /* rdwdle */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<15 /* rdwdle */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(0);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(0)[RDWDLE]: Received DLLP with DataLink Layer Error\n"
        "    rdlh_bad_dllp_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(0);
    info.status_mask        = 1ull<<16 /* rtwdle */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<16 /* rtwdle */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(0);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(0)[RTWDLE]: Received TLP with DataLink Layer Error\n"
        "    rdlh_bad_tlp_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(0);
    info.status_mask        = 1ull<<17 /* dpeoosd */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<17 /* dpeoosd */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(0);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(0)[DPEOOSD]: DLLP protocol error (out of sequence DLLP)\n"
        "    rdlh_prot_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(0);
    info.status_mask        = 1ull<<18 /* fcpvwt */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<18 /* fcpvwt */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(0);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(0)[FCPVWT]: Flow Control Protocol Violation (Watchdog Timer)\n"
        "    rtlh_fc_prot_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(0);
    info.status_mask        = 1ull<<19 /* rpe */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<19 /* rpe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(0);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(0)[RPE]: When the PHY reports 8B/10B decode error\n"
        "    (RxStatus = 3b100) or disparity error\n"
        "    (RxStatus = 3b111), the signal rmlh_rcvd_err will\n"
        "    be asserted.\n"
        "    rmlh_rcvd_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(0);
    info.status_mask        = 1ull<<20 /* fcuv */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<20 /* fcuv */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(0);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(0)[FCUV]: Flow Control Update Violation (opt. checks)\n"
        "    int_xadm_fc_prot_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(0);
    info.status_mask        = 1ull<<21 /* rqo */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<21 /* rqo */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(0);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(0)[RQO]: Receive queue overflow. Normally happens only when\n"
        "    flow control advertisements are ignored\n"
        "    radm_qoverflow\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(0);
    info.status_mask        = 1ull<<22 /* rauc */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<22 /* rauc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(0);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(0)[RAUC]: Received an unexpected completion\n"
        "    radm_unexp_cpl_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(0);
    info.status_mask        = 1ull<<23 /* racur */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<23 /* racur */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(0);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(0)[RACUR]: Received a completion with UR status\n"
        "    radm_rcvd_cpl_ur\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(0);
    info.status_mask        = 1ull<<24 /* racca */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<24 /* racca */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(0);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(0)[RACCA]: Received a completion with CA status\n"
        "    radm_rcvd_cpl_ca\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(0);
    info.status_mask        = 1ull<<25 /* caar */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<25 /* caar */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(0);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(0)[CAAR]: Completer aborted a request\n"
        "    radm_rcvd_ca_req\n"
        "    This bit will never be set because Octeon does\n"
        "    not generate Completer Aborts.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(0);
    info.status_mask        = 1ull<<26 /* rarwdns */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<26 /* rarwdns */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(0);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(0)[RARWDNS]: Recieved a request which device does not support\n"
        "    radm_rcvd_ur_req\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(0);
    info.status_mask        = 1ull<<27 /* ramtlp */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<27 /* ramtlp */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(0);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(0)[RAMTLP]: Received a malformed TLP\n"
        "    radm_mlf_tlp_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(0);
    info.status_mask        = 1ull<<28 /* racpp */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<28 /* racpp */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(0);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(0)[RACPP]: Received a completion with poisoned payload\n"
        "    radm_rcvd_cpl_poisoned\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(0);
    info.status_mask        = 1ull<<29 /* rawwpp */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<29 /* rawwpp */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(0);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(0)[RAWWPP]: Received a write with poisoned payload\n"
        "    radm_rcvd_wreq_poisoned\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(0);
    info.status_mask        = 1ull<<30 /* ecrc_e */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(0);
    info.enable_mask        = 1ull<<30 /* ecrc_e */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(0);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(0)[ECRC_E]: Received a ECRC error.\n"
        "    radm_ecrc_err\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_PEMX_INT_SUM(1) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_INT_SUM(1);
    info.status_mask        = 1ull<<1 /* se */;
    info.enable_addr        = CVMX_PEMX_INT_ENB(1);
    info.enable_mask        = 1ull<<1 /* se */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<26 /* pem1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_INT_SUM(1)[SE]: System Error, RC Mode Only.\n"
        "    (cfg_sys_err_rc)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_INT_SUM(1);
    info.status_mask        = 1ull<<4 /* up_b1 */;
    info.enable_addr        = CVMX_PEMX_INT_ENB(1);
    info.enable_mask        = 1ull<<4 /* up_b1 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<26 /* pem1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_INT_SUM(1)[UP_B1]: Received P-TLP for Bar1 when bar1 index valid\n"
        "    is not set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_INT_SUM(1);
    info.status_mask        = 1ull<<5 /* up_b2 */;
    info.enable_addr        = CVMX_PEMX_INT_ENB(1);
    info.enable_mask        = 1ull<<5 /* up_b2 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<26 /* pem1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_INT_SUM(1)[UP_B2]: Received P-TLP for Bar2 when bar2 is disabeld.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_INT_SUM(1);
    info.status_mask        = 1ull<<6 /* up_bx */;
    info.enable_addr        = CVMX_PEMX_INT_ENB(1);
    info.enable_mask        = 1ull<<6 /* up_bx */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<26 /* pem1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_INT_SUM(1)[UP_BX]: Received P-TLP for an unknown Bar.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_INT_SUM(1);
    info.status_mask        = 1ull<<7 /* un_b1 */;
    info.enable_addr        = CVMX_PEMX_INT_ENB(1);
    info.enable_mask        = 1ull<<7 /* un_b1 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<26 /* pem1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_INT_SUM(1)[UN_B1]: Received N-TLP for Bar1 when bar1 index valid\n"
        "    is not set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_INT_SUM(1);
    info.status_mask        = 1ull<<8 /* un_b2 */;
    info.enable_addr        = CVMX_PEMX_INT_ENB(1);
    info.enable_mask        = 1ull<<8 /* un_b2 */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<26 /* pem1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_INT_SUM(1)[UN_B2]: Received N-TLP for Bar2 when bar2 is disabled.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_INT_SUM(1);
    info.status_mask        = 1ull<<9 /* un_bx */;
    info.enable_addr        = CVMX_PEMX_INT_ENB(1);
    info.enable_mask        = 1ull<<9 /* un_bx */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<26 /* pem1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_INT_SUM(1)[UN_BX]: Received N-TLP for an unknown Bar.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_INT_SUM(1);
    info.status_mask        = 1ull<<11 /* rdlk */;
    info.enable_addr        = CVMX_PEMX_INT_ENB(1);
    info.enable_mask        = 1ull<<11 /* rdlk */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<26 /* pem1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_INT_SUM(1)[RDLK]: Received Read Lock TLP.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_INT_SUM(1);
    info.status_mask        = 1ull<<12 /* crs_er */;
    info.enable_addr        = CVMX_PEMX_INT_ENB(1);
    info.enable_mask        = 1ull<<12 /* crs_er */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<26 /* pem1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_INT_SUM(1)[CRS_ER]: Had a CRS Timeout when Retries were enabled.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_INT_SUM(1);
    info.status_mask        = 1ull<<13 /* crs_dr */;
    info.enable_addr        = CVMX_PEMX_INT_ENB(1);
    info.enable_mask        = 1ull<<13 /* crs_dr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<26 /* pem1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_INT_SUM(1)[CRS_DR]: Had a CRS Timeout when Retries were disabled.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_INT_SUM(1);
    info.status_mask        = 0;
    info.enable_addr        = 0;
    info.enable_mask        = 0;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<26 /* pem1 */;
    info.func               = __cvmx_error_decode;
    info.user_info          = 0;
    fail |= cvmx_error_add(&info);

    /* CVMX_PEMX_DBG_INFO(1) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(1);
    info.status_mask        = 1ull<<0 /* spoison */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<0 /* spoison */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(1);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(1)[SPOISON]: Poisoned TLP sent\n"
        "    peai__client0_tlp_ep & peai__client0_tlp_hv\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(1);
    info.status_mask        = 1ull<<2 /* rtlplle */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<2 /* rtlplle */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(1);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(1)[RTLPLLE]: Received TLP has link layer error\n"
        "    pedc_radm_trgt1_dllp_abort & pedc__radm_trgt1_eot\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(1);
    info.status_mask        = 1ull<<3 /* recrce */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<3 /* recrce */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(1);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(1)[RECRCE]: Received ECRC Error\n"
        "    pedc_radm_trgt1_ecrc_err & pedc__radm_trgt1_eot\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(1);
    info.status_mask        = 1ull<<4 /* rpoison */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<4 /* rpoison */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(1);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(1)[RPOISON]: Received Poisoned TLP\n"
        "    pedc__radm_trgt1_poisoned & pedc__radm_trgt1_hv\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(1);
    info.status_mask        = 1ull<<5 /* rcemrc */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<5 /* rcemrc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(1);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(1)[RCEMRC]: Received Correctable Error Message (RC Mode only)\n"
        "    pedc_radm_correctable_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(1);
    info.status_mask        = 1ull<<6 /* rnfemrc */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<6 /* rnfemrc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(1);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(1)[RNFEMRC]: Received Non-Fatal Error Message (RC Mode only)\n"
        "    pedc_radm_nonfatal_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(1);
    info.status_mask        = 1ull<<7 /* rfemrc */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<7 /* rfemrc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(1);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(1)[RFEMRC]: Received Fatal Error Message (RC Mode only)\n"
        "    pedc_radm_fatal_err\n"
        "    Bit set when a message with ERR_FATAL is set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(1);
    info.status_mask        = 1ull<<8 /* rpmerc */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<8 /* rpmerc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(1);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(1)[RPMERC]: Received PME Message (RC Mode only)\n"
        "    pedc_radm_pm_pme\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(1);
    info.status_mask        = 1ull<<9 /* rptamrc */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<9 /* rptamrc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(1);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(1)[RPTAMRC]: Received PME Turnoff Acknowledge Message\n"
        "    (RC Mode only)\n"
        "    pedc_radm_pm_to_ack\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(1);
    info.status_mask        = 1ull<<10 /* rumep */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<10 /* rumep */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(1);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(1)[RUMEP]: Received Unlock Message (EP Mode Only)\n"
        "    pedc_radm_msg_unlock\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(1);
    info.status_mask        = 1ull<<11 /* rvdm */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<11 /* rvdm */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(1);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(1)[RVDM]: Received Vendor-Defined Message\n"
        "    pedc_radm_vendor_msg\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(1);
    info.status_mask        = 1ull<<12 /* acto */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<12 /* acto */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(1);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(1)[ACTO]: A Completion Timeout Occured\n"
        "    pedc_radm_cpl_timeout\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(1);
    info.status_mask        = 1ull<<13 /* rte */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<13 /* rte */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(1);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(1)[RTE]: Replay Timer Expired\n"
        "    xdlh_replay_timeout_err\n"
        "    This bit is set when the REPLAY_TIMER expires in\n"
        "    the PCIE core. The probability of this bit being\n"
        "    set will increase with the traffic load.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(1);
    info.status_mask        = 1ull<<14 /* mre */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<14 /* mre */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(1);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(1)[MRE]: Max Retries Exceeded\n"
        "    xdlh_replay_num_rlover_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(1);
    info.status_mask        = 1ull<<15 /* rdwdle */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<15 /* rdwdle */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(1);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(1)[RDWDLE]: Received DLLP with DataLink Layer Error\n"
        "    rdlh_bad_dllp_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(1);
    info.status_mask        = 1ull<<16 /* rtwdle */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<16 /* rtwdle */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(1);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(1)[RTWDLE]: Received TLP with DataLink Layer Error\n"
        "    rdlh_bad_tlp_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(1);
    info.status_mask        = 1ull<<17 /* dpeoosd */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<17 /* dpeoosd */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(1);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(1)[DPEOOSD]: DLLP protocol error (out of sequence DLLP)\n"
        "    rdlh_prot_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(1);
    info.status_mask        = 1ull<<18 /* fcpvwt */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<18 /* fcpvwt */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(1);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(1)[FCPVWT]: Flow Control Protocol Violation (Watchdog Timer)\n"
        "    rtlh_fc_prot_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(1);
    info.status_mask        = 1ull<<19 /* rpe */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<19 /* rpe */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(1);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(1)[RPE]: When the PHY reports 8B/10B decode error\n"
        "    (RxStatus = 3b100) or disparity error\n"
        "    (RxStatus = 3b111), the signal rmlh_rcvd_err will\n"
        "    be asserted.\n"
        "    rmlh_rcvd_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(1);
    info.status_mask        = 1ull<<20 /* fcuv */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<20 /* fcuv */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(1);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(1)[FCUV]: Flow Control Update Violation (opt. checks)\n"
        "    int_xadm_fc_prot_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(1);
    info.status_mask        = 1ull<<21 /* rqo */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<21 /* rqo */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(1);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(1)[RQO]: Receive queue overflow. Normally happens only when\n"
        "    flow control advertisements are ignored\n"
        "    radm_qoverflow\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(1);
    info.status_mask        = 1ull<<22 /* rauc */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<22 /* rauc */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(1);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(1)[RAUC]: Received an unexpected completion\n"
        "    radm_unexp_cpl_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(1);
    info.status_mask        = 1ull<<23 /* racur */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<23 /* racur */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(1);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(1)[RACUR]: Received a completion with UR status\n"
        "    radm_rcvd_cpl_ur\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(1);
    info.status_mask        = 1ull<<24 /* racca */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<24 /* racca */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(1);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(1)[RACCA]: Received a completion with CA status\n"
        "    radm_rcvd_cpl_ca\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(1);
    info.status_mask        = 1ull<<25 /* caar */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<25 /* caar */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(1);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(1)[CAAR]: Completer aborted a request\n"
        "    radm_rcvd_ca_req\n"
        "    This bit will never be set because Octeon does\n"
        "    not generate Completer Aborts.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(1);
    info.status_mask        = 1ull<<26 /* rarwdns */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<26 /* rarwdns */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(1);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(1)[RARWDNS]: Recieved a request which device does not support\n"
        "    radm_rcvd_ur_req\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(1);
    info.status_mask        = 1ull<<27 /* ramtlp */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<27 /* ramtlp */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(1);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(1)[RAMTLP]: Received a malformed TLP\n"
        "    radm_mlf_tlp_err\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(1);
    info.status_mask        = 1ull<<28 /* racpp */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<28 /* racpp */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(1);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(1)[RACPP]: Received a completion with poisoned payload\n"
        "    radm_rcvd_cpl_poisoned\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(1);
    info.status_mask        = 1ull<<29 /* rawwpp */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<29 /* rawwpp */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(1);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(1)[RAWWPP]: Received a write with poisoned payload\n"
        "    radm_rcvd_wreq_poisoned\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEMX_DBG_INFO(1);
    info.status_mask        = 1ull<<30 /* ecrc_e */;
    info.enable_addr        = CVMX_PEMX_DBG_INFO_EN(1);
    info.enable_mask        = 1ull<<30 /* ecrc_e */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_PCI;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_PEMX_INT_SUM(1);
    info.parent.status_mask = 1ull<<10 /* exc */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEMX_DBG_INFO(1)[ECRC_E]: Received a ECRC error.\n"
        "    radm_ecrc_err\n";
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<5 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<5 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[POOL0TH]: Set when FPA_QUE0_AVAILABLE is equal to\n"
        "    FPA_POOL`_THRESHOLD[THRESH] and a pointer is\n"
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<5 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<5 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<5 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<5 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<5 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<5 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<5 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<5 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<5 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<5 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<5 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<5 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<5 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<5 /* fpa */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<5 /* fpa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR FPA_INT_SUM[FREE7]: When a pointer for POOL7 is freed bit is set.\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_UCTLX_INT_REG(0) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_UCTLX_INT_REG(0);
    info.status_mask        = 1ull<<0 /* pp_psh_f */;
    info.enable_addr        = CVMX_UCTLX_INT_ENA(0);
    info.enable_mask        = 1ull<<0 /* pp_psh_f */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR UCTLX_INT_REG(0)[PP_PSH_F]: PP Access FIFO  Pushed When Full\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_UCTLX_INT_REG(0);
    info.status_mask        = 1ull<<1 /* er_psh_f */;
    info.enable_addr        = CVMX_UCTLX_INT_ENA(0);
    info.enable_mask        = 1ull<<1 /* er_psh_f */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR UCTLX_INT_REG(0)[ER_PSH_F]: EHCI Read Buffer FIFO Pushed When Full\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_UCTLX_INT_REG(0);
    info.status_mask        = 1ull<<2 /* or_psh_f */;
    info.enable_addr        = CVMX_UCTLX_INT_ENA(0);
    info.enable_mask        = 1ull<<2 /* or_psh_f */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR UCTLX_INT_REG(0)[OR_PSH_F]: OHCI Read Buffer FIFO Pushed When Full\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_UCTLX_INT_REG(0);
    info.status_mask        = 1ull<<3 /* cf_psh_f */;
    info.enable_addr        = CVMX_UCTLX_INT_ENA(0);
    info.enable_mask        = 1ull<<3 /* cf_psh_f */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR UCTLX_INT_REG(0)[CF_PSH_F]: Command FIFO Pushed When Full\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_UCTLX_INT_REG(0);
    info.status_mask        = 1ull<<4 /* wb_psh_f */;
    info.enable_addr        = CVMX_UCTLX_INT_ENA(0);
    info.enable_mask        = 1ull<<4 /* wb_psh_f */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR UCTLX_INT_REG(0)[WB_PSH_F]: Write Buffer FIFO Pushed When Full\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_UCTLX_INT_REG(0);
    info.status_mask        = 1ull<<5 /* wb_pop_e */;
    info.enable_addr        = CVMX_UCTLX_INT_ENA(0);
    info.enable_mask        = 1ull<<5 /* wb_pop_e */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR UCTLX_INT_REG(0)[WB_POP_E]: Write Buffer FIFO Poped When Empty\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_UCTLX_INT_REG(0);
    info.status_mask        = 1ull<<6 /* oc_ovf_e */;
    info.enable_addr        = CVMX_UCTLX_INT_ENA(0);
    info.enable_mask        = 1ull<<6 /* oc_ovf_e */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR UCTLX_INT_REG(0)[OC_OVF_E]: Ohci Commit OVerFlow Error\n"
        "    When the error happenes, the whole NCB system needs\n"
        "    to be reset.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_UCTLX_INT_REG(0);
    info.status_mask        = 1ull<<7 /* ec_ovf_e */;
    info.enable_addr        = CVMX_UCTLX_INT_ENA(0);
    info.enable_mask        = 1ull<<7 /* ec_ovf_e */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_USB;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<13 /* usb */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR UCTLX_INT_REG(0)[EC_OVF_E]: Ehci Commit OVerFlow Error\n"
        "    When the error happenes, the whole NCB system needs\n"
        "    to be reset.\n";
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<0 /* mio */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR MIO_BOOT_ERR[WAIT_ERR]: Wait mode error\n";
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<0 /* mio */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<0 /* mio */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<0 /* mio */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<0 /* mio */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR MIO_RST_INT[PERST1]: PERST1_L asserted while MIO_RST_CTL1[RST_RCV]=1\n"
        "    and MIO_RST_CTL1[RST_CHIP]=0\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_DFM_FNT_STAT */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_DFM_FNT_STAT;
    info.status_mask        = 1ull<<0 /* sbe_err */;
    info.enable_addr        = CVMX_DFM_FNT_IENA;
    info.enable_mask        = 1ull<<0 /* sbe_intena */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<40 /* dfm */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR DFM_FNT_STAT[SBE_ERR]: Single bit error detected(corrected) during\n"
        "    Memory Read.\n"
        "    Write of 1 will clear the corresponding error bit\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_DFM_FNT_STAT;
    info.status_mask        = 1ull<<1 /* dbe_err */;
    info.enable_addr        = CVMX_DFM_FNT_IENA;
    info.enable_mask        = 1ull<<1 /* dbe_intena */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<40 /* dfm */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR DFM_FNT_STAT[DBE_ERR]: Double bit error detected(uncorrectable) during\n"
        "    Memory Read.\n"
        "    Write of 1 will clear the corresponding error bit\n";
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<11 /* tim */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR TIM_REG_ERROR[MASK]: Bit mask indicating the rings in error\n";
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<17 /* lmc0 */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<17 /* lmc0 */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<17 /* lmc0 */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<4 /* key */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<4 /* key */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<4 /* key */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<4 /* key */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR KEY_INT_SUM[KED1_DBE]: Error Bit\n"
;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
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
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<1 /* gmx0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR GMXX_TX_INT_REG(0)[PTP_LOST]: A packet with a PTP request was not able to be\n"
        "    sent due to XSCOL\n";
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<30 /* iob */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR IOB_INT_SUM[P_DAT]: Set when a data arrives before a SOP for the same\n"
        "    port for a passthrough packet.\n"
        "    The first detected error associated with bits [5:0]\n"
        "    of this register will only be set here. A new bit\n"
        "    can be set when the previous reported bit is cleared.\n";
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<28 /* agl */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<28 /* agl */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<28 /* agl */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR AGL_GMX_TX_INT_REG[UNDFLW]: TX Underflow\n";
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<7 /* zip */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR ZIP_ERROR[DOORBELL]: A doorbell count has overflowed\n";
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<6 /* dfa */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<6 /* dfa */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR DFA_ERROR[DC0PERR]: RAM[3:1] Parity Error Detected from Node Cluster #0\n"
        "    See also DFA_DTCFADR register which contains the\n"
        "    failing addresses for the internal node cache RAMs.\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_SRIOX_INT_REG(0) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SRIOX_INT_REG(0);
    info.status_mask        = 1ull<<4 /* bar_err */;
    info.enable_addr        = CVMX_SRIOX_INT_ENABLE(0);
    info.enable_mask        = 1ull<<4 /* bar_err */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_SRIO;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<32 /* srio0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SRIOX_INT_REG(0)[BAR_ERR]: Incoming Access Crossing/Missing BAR Address\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SRIOX_INT_REG(0);
    info.status_mask        = 1ull<<5 /* deny_wr */;
    info.enable_addr        = CVMX_SRIOX_INT_ENABLE(0);
    info.enable_mask        = 1ull<<5 /* deny_wr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_SRIO;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<32 /* srio0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SRIOX_INT_REG(0)[DENY_WR]: Incoming Maint_Wr Access to Denied Bar Registers.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SRIOX_INT_REG(0);
    info.status_mask        = 1ull<<6 /* sli_err */;
    info.enable_addr        = CVMX_SRIOX_INT_ENABLE(0);
    info.enable_mask        = 1ull<<6 /* sli_err */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_SRIO;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<32 /* srio0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SRIOX_INT_REG(0)[SLI_ERR]: Unsupported S2M Transaction Received.\n"
        "    See SRIO(0..1)_INT_INFO[1:0]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SRIOX_INT_REG(0);
    info.status_mask        = 1ull<<9 /* mce_rx */;
    info.enable_addr        = CVMX_SRIOX_INT_ENABLE(0);
    info.enable_mask        = 1ull<<9 /* mce_rx */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_SRIO;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<32 /* srio0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SRIOX_INT_REG(0)[MCE_RX]: Incoming Multicast Event Symbol\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SRIOX_INT_REG(0);
    info.status_mask        = 1ull<<12 /* log_erb */;
    info.enable_addr        = CVMX_SRIOX_INT_ENABLE(0);
    info.enable_mask        = 1ull<<12 /* log_erb */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_SRIO;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<32 /* srio0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SRIOX_INT_REG(0)[LOG_ERB]: Logical/Transport Layer Error detected in ERB\n"
        "    See SRIOMAINT(0..1)_ERB_LT_ERR_DET\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SRIOX_INT_REG(0);
    info.status_mask        = 1ull<<13 /* phy_erb */;
    info.enable_addr        = CVMX_SRIOX_INT_ENABLE(0);
    info.enable_mask        = 1ull<<13 /* phy_erb */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_SRIO;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<32 /* srio0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SRIOX_INT_REG(0)[PHY_ERB]: Physical Layer Error detected in ERB\n"
        "    See SRIOMAINT*_ERB_ATTR_CAPT\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SRIOX_INT_REG(0);
    info.status_mask        = 1ull<<18 /* omsg_err */;
    info.enable_addr        = CVMX_SRIOX_INT_ENABLE(0);
    info.enable_mask        = 1ull<<18 /* omsg_err */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_SRIO;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<32 /* srio0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SRIOX_INT_REG(0)[OMSG_ERR]: Outbound Message Invalid Descriptor Error\n"
        "    See SRIO(0..1)_INT_INFO2\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SRIOX_INT_REG(0);
    info.status_mask        = 1ull<<19 /* pko_err */;
    info.enable_addr        = CVMX_SRIOX_INT_ENABLE(0);
    info.enable_mask        = 1ull<<19 /* pko_err */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_SRIO;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<32 /* srio0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SRIOX_INT_REG(0)[PKO_ERR]: Outbound Message Received PKO Error\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SRIOX_INT_REG(0);
    info.status_mask        = 1ull<<20 /* rtry_err */;
    info.enable_addr        = CVMX_SRIOX_INT_ENABLE(0);
    info.enable_mask        = 1ull<<20 /* rtry_err */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_SRIO;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<32 /* srio0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SRIOX_INT_REG(0)[RTRY_ERR]: Outbound Message Retry Threshold Exceeded\n"
        "    See SRIO(0..1)_INT_INFO3\n"
        "    When one or more of the segments in an outgoing\n"
        "    message have a RTRY_ERR, SRIO will not set\n"
        "    OMSG* after the message \"transfer\".\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SRIOX_INT_REG(0);
    info.status_mask        = 1ull<<21 /* f_error */;
    info.enable_addr        = CVMX_SRIOX_INT_ENABLE(0);
    info.enable_mask        = 1ull<<21 /* f_error */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_SRIO;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<32 /* srio0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SRIOX_INT_REG(0)[F_ERROR]: SRIO Fatal Port Error (MAC reset required)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SRIOX_INT_REG(0);
    info.status_mask        = 1ull<<22 /* mac_buf */;
    info.enable_addr        = CVMX_SRIOX_INT_ENABLE(0);
    info.enable_mask        = 1ull<<22 /* mac_buf */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_SRIO;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<32 /* srio0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SRIOX_INT_REG(0)[MAC_BUF]: SRIO MAC Buffer CRC Error (Pass 2)\n"
        "    See SRIO(0..1)_MAC_BUFFERS\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SRIOX_INT_REG(0);
    info.status_mask        = 1ull<<23 /* degrad */;
    info.enable_addr        = CVMX_SRIOX_INT_ENABLE(0);
    info.enable_mask        = 1ull<<23 /* degrade */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_SRIO;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<32 /* srio0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SRIOX_INT_REG(0)[DEGRAD]: ERB Error Rate reached Degrade Count (Pass 2)\n"
        "    See SRIOMAINT(0..1)_ERB_ERR_RATE\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SRIOX_INT_REG(0);
    info.status_mask        = 1ull<<24 /* fail */;
    info.enable_addr        = CVMX_SRIOX_INT_ENABLE(0);
    info.enable_mask        = 1ull<<24 /* fail */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_SRIO;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<32 /* srio0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SRIOX_INT_REG(0)[FAIL]: ERB Error Rate reached Fail Count (Pass 2)\n"
        "    See SRIOMAINT(0..1)_ERB_ERR_RATE\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SRIOX_INT_REG(0);
    info.status_mask        = 1ull<<25 /* ttl_tout */;
    info.enable_addr        = CVMX_SRIOX_INT_ENABLE(0);
    info.enable_mask        = 1ull<<25 /* ttl_tout */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_SRIO;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<32 /* srio0 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SRIOX_INT_REG(0)[TTL_TOUT]: Outgoing Packet Time to Live Timeout (Pass 2)\n"
        "    See SRIOMAINT(0..1)_DROP_PACKET\n";
    fail |= cvmx_error_add(&info);

    /* CVMX_SRIOX_INT_REG(1) */
    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SRIOX_INT_REG(1);
    info.status_mask        = 1ull<<4 /* bar_err */;
    info.enable_addr        = CVMX_SRIOX_INT_ENABLE(1);
    info.enable_mask        = 1ull<<4 /* bar_err */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_SRIO;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<33 /* srio1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SRIOX_INT_REG(1)[BAR_ERR]: Incoming Access Crossing/Missing BAR Address\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SRIOX_INT_REG(1);
    info.status_mask        = 1ull<<5 /* deny_wr */;
    info.enable_addr        = CVMX_SRIOX_INT_ENABLE(1);
    info.enable_mask        = 1ull<<5 /* deny_wr */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_SRIO;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<33 /* srio1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SRIOX_INT_REG(1)[DENY_WR]: Incoming Maint_Wr Access to Denied Bar Registers.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SRIOX_INT_REG(1);
    info.status_mask        = 1ull<<6 /* sli_err */;
    info.enable_addr        = CVMX_SRIOX_INT_ENABLE(1);
    info.enable_mask        = 1ull<<6 /* sli_err */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_SRIO;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<33 /* srio1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SRIOX_INT_REG(1)[SLI_ERR]: Unsupported S2M Transaction Received.\n"
        "    See SRIO(0..1)_INT_INFO[1:0]\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SRIOX_INT_REG(1);
    info.status_mask        = 1ull<<9 /* mce_rx */;
    info.enable_addr        = CVMX_SRIOX_INT_ENABLE(1);
    info.enable_mask        = 1ull<<9 /* mce_rx */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_SRIO;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<33 /* srio1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SRIOX_INT_REG(1)[MCE_RX]: Incoming Multicast Event Symbol\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SRIOX_INT_REG(1);
    info.status_mask        = 1ull<<12 /* log_erb */;
    info.enable_addr        = CVMX_SRIOX_INT_ENABLE(1);
    info.enable_mask        = 1ull<<12 /* log_erb */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_SRIO;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<33 /* srio1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SRIOX_INT_REG(1)[LOG_ERB]: Logical/Transport Layer Error detected in ERB\n"
        "    See SRIOMAINT(0..1)_ERB_LT_ERR_DET\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SRIOX_INT_REG(1);
    info.status_mask        = 1ull<<13 /* phy_erb */;
    info.enable_addr        = CVMX_SRIOX_INT_ENABLE(1);
    info.enable_mask        = 1ull<<13 /* phy_erb */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_SRIO;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<33 /* srio1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SRIOX_INT_REG(1)[PHY_ERB]: Physical Layer Error detected in ERB\n"
        "    See SRIOMAINT*_ERB_ATTR_CAPT\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SRIOX_INT_REG(1);
    info.status_mask        = 1ull<<18 /* omsg_err */;
    info.enable_addr        = CVMX_SRIOX_INT_ENABLE(1);
    info.enable_mask        = 1ull<<18 /* omsg_err */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_SRIO;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<33 /* srio1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SRIOX_INT_REG(1)[OMSG_ERR]: Outbound Message Invalid Descriptor Error\n"
        "    See SRIO(0..1)_INT_INFO2\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SRIOX_INT_REG(1);
    info.status_mask        = 1ull<<19 /* pko_err */;
    info.enable_addr        = CVMX_SRIOX_INT_ENABLE(1);
    info.enable_mask        = 1ull<<19 /* pko_err */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_SRIO;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<33 /* srio1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SRIOX_INT_REG(1)[PKO_ERR]: Outbound Message Received PKO Error\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SRIOX_INT_REG(1);
    info.status_mask        = 1ull<<20 /* rtry_err */;
    info.enable_addr        = CVMX_SRIOX_INT_ENABLE(1);
    info.enable_mask        = 1ull<<20 /* rtry_err */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_SRIO;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<33 /* srio1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SRIOX_INT_REG(1)[RTRY_ERR]: Outbound Message Retry Threshold Exceeded\n"
        "    See SRIO(0..1)_INT_INFO3\n"
        "    When one or more of the segments in an outgoing\n"
        "    message have a RTRY_ERR, SRIO will not set\n"
        "    OMSG* after the message \"transfer\".\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SRIOX_INT_REG(1);
    info.status_mask        = 1ull<<21 /* f_error */;
    info.enable_addr        = CVMX_SRIOX_INT_ENABLE(1);
    info.enable_mask        = 1ull<<21 /* f_error */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_SRIO;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<33 /* srio1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SRIOX_INT_REG(1)[F_ERROR]: SRIO Fatal Port Error (MAC reset required)\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SRIOX_INT_REG(1);
    info.status_mask        = 1ull<<22 /* mac_buf */;
    info.enable_addr        = CVMX_SRIOX_INT_ENABLE(1);
    info.enable_mask        = 1ull<<22 /* mac_buf */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_SRIO;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<33 /* srio1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SRIOX_INT_REG(1)[MAC_BUF]: SRIO MAC Buffer CRC Error (Pass 2)\n"
        "    See SRIO(0..1)_MAC_BUFFERS\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SRIOX_INT_REG(1);
    info.status_mask        = 1ull<<23 /* degrad */;
    info.enable_addr        = CVMX_SRIOX_INT_ENABLE(1);
    info.enable_mask        = 1ull<<23 /* degrade */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_SRIO;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<33 /* srio1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SRIOX_INT_REG(1)[DEGRAD]: ERB Error Rate reached Degrade Count (Pass 2)\n"
        "    See SRIOMAINT(0..1)_ERB_ERR_RATE\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SRIOX_INT_REG(1);
    info.status_mask        = 1ull<<24 /* fail */;
    info.enable_addr        = CVMX_SRIOX_INT_ENABLE(1);
    info.enable_mask        = 1ull<<24 /* fail */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_SRIO;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<33 /* srio1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SRIOX_INT_REG(1)[FAIL]: ERB Error Rate reached Fail Count (Pass 2)\n"
        "    See SRIOMAINT(0..1)_ERB_ERR_RATE\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_SRIOX_INT_REG(1);
    info.status_mask        = 1ull<<25 /* ttl_tout */;
    info.enable_addr        = CVMX_SRIOX_INT_ENABLE(1);
    info.enable_mask        = 1ull<<25 /* ttl_tout */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_SRIO;
    info.group_index        = 1;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<33 /* srio1 */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR SRIOX_INT_REG(1)[TTL_TOUT]: Outgoing Packet Time to Live Timeout (Pass 2)\n"
        "    See SRIOMAINT(0..1)_DROP_PACKET\n";
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<3 /* sli */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<3 /* sli */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<3 /* sli */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<3 /* sli */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<3 /* sli */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<3 /* sli */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<3 /* sli */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<3 /* sli */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<3 /* sli */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<3 /* sli */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<3 /* sli */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<3 /* sli */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<3 /* sli */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<3 /* sli */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<3 /* sli */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<3 /* sli */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_SLI_INT_SUM[POUT_ERR]: Set when PKO sends packet data with the error bit\n"
        "    set.\n";
    fail |= cvmx_error_add(&info);

    info.reg_type           = CVMX_ERROR_REGISTER_IO64;
    info.status_addr        = CVMX_PEXP_SLI_INT_SUM;
    info.status_mask        = 1ull<<51 /* pin_bp */;
    info.enable_addr        = CVMX_PEXP_SLI_INT_ENB_CIU;
    info.enable_mask        = 1ull<<51 /* pin_bp */;
    info.flags              = 0;
    info.group              = CVMX_ERROR_GROUP_INTERNAL;
    info.group_index        = 0;
    info.parent.reg_type    = CVMX_ERROR_REGISTER_IO64;
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<3 /* sli */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_SLI_INT_SUM[PIN_BP]: Packet input count has exceeded the WMARK.\n"
        "    See SLI_PKT_IN_BP\n";
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<3 /* sli */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<3 /* sli */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<3 /* sli */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<3 /* sli */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<3 /* sli */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<3 /* sli */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<3 /* sli */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR PEXP_SLI_INT_SUM[ILL_PAD]: Set when a BAR0 address R/W falls into theaddress\n"
        "    range of the Packet-CSR, but for an unused\n"
        "    address.\n";
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<41 /* dpi */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<41 /* dpi */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<41 /* dpi */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<41 /* dpi */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<41 /* dpi */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<41 /* dpi */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<41 /* dpi */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<41 /* dpi */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<41 /* dpi */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<41 /* dpi */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<41 /* dpi */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<41 /* dpi */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<41 /* dpi */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<41 /* dpi */;
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
    info.parent.status_addr = CVMX_CIU_BLOCK_INT;
    info.parent.status_mask = 1ull<<41 /* dpi */;
    info.func               = __cvmx_error_display;
    info.user_info          = (long)
        "ERROR DPI_REQ_ERR_RST[QERR]: Indicates which instruction queue dropped an\n"
        "    instruction because the source or destination\n"
        "    was in reset.\n"
        "    SW must clear the bit before the the cooresponding\n"
        "    instruction queue will continue processing\n"
        "    instructions if DPI_REQ_ERR_RST_EN[EN] is set.\n";
    fail |= cvmx_error_add(&info);

    return fail;
}

