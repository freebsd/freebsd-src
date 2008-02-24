/* $FreeBSD: src/sys/dev/isp/isp_library.h,v 1.8 2007/04/02 01:04:20 mjacob Exp $ */
/*-
 *  Copyright (c) 1997-2007 by Matthew Jacob
 *  All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 * 
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 
 *  THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *  SUCH DAMAGE.
 */
#ifndef	_ISP_LIBRARY_H
#define	_ISP_LIBRARY_H

extern int isp_save_xs(ispsoftc_t *, XS_T *, uint32_t *);
extern XS_T *isp_find_xs(ispsoftc_t *, uint32_t);
extern uint32_t isp_find_handle(ispsoftc_t *, XS_T *);
extern uint32_t isp_handle_index(uint32_t);
extern void isp_destroy_handle(ispsoftc_t *, uint32_t);
extern int isp_getrqentry(ispsoftc_t *, uint32_t *, uint32_t *, void **);
extern void isp_print_qentry (ispsoftc_t *, char *, int, void *);
extern void isp_print_bytes(ispsoftc_t *, const char *, int, void *);
extern int isp_fc_runstate(ispsoftc_t *, int);
extern void isp_dump_portdb(ispsoftc_t *);
extern void isp_shutdown(ispsoftc_t *);
extern void isp_put_hdr(ispsoftc_t *, isphdr_t *, isphdr_t *);
extern void isp_get_hdr(ispsoftc_t *, isphdr_t *, isphdr_t *);
extern int isp_get_response_type(ispsoftc_t *, isphdr_t *);
extern void
isp_put_request(ispsoftc_t *, ispreq_t *, ispreq_t *);
extern void
isp_put_marker(ispsoftc_t *, isp_marker_t *, isp_marker_t *);
extern void
isp_put_marker_24xx(ispsoftc_t *, isp_marker_24xx_t *, isp_marker_24xx_t *);
extern void
isp_put_request_t2(ispsoftc_t *, ispreqt2_t *, ispreqt2_t *);
extern void
isp_put_request_t2e(ispsoftc_t *, ispreqt2e_t *, ispreqt2e_t *);
extern void
isp_put_request_t3(ispsoftc_t *, ispreqt3_t *, ispreqt3_t *);
extern void
isp_put_request_t3e(ispsoftc_t *, ispreqt3e_t *, ispreqt3e_t *);
extern void
isp_put_extended_request(ispsoftc_t *, ispextreq_t *, ispextreq_t *);
extern void
isp_put_request_t7(ispsoftc_t *, ispreqt7_t *, ispreqt7_t *);
extern void
isp_put_24xx_abrt(ispsoftc_t *, isp24xx_abrt_t *, isp24xx_abrt_t *);
extern void
isp_put_cont_req(ispsoftc_t *, ispcontreq_t *, ispcontreq_t *);
extern void
isp_put_cont64_req(ispsoftc_t *, ispcontreq64_t *, ispcontreq64_t *);
extern void
isp_get_response(ispsoftc_t *, ispstatusreq_t *, ispstatusreq_t *);
extern void isp_get_24xx_response(ispsoftc_t *, isp24xx_statusreq_t *,
    isp24xx_statusreq_t *);
void
isp_get_24xx_abrt(ispsoftc_t *, isp24xx_abrt_t *, isp24xx_abrt_t *);
extern void
isp_get_rio2(ispsoftc_t *, isp_rio2_t *, isp_rio2_t *);
extern void
isp_put_icb(ispsoftc_t *, isp_icb_t *, isp_icb_t *);
extern void
isp_put_icb_2400(ispsoftc_t *, isp_icb_2400_t *, isp_icb_2400_t *);
extern void
isp_get_pdb_21xx(ispsoftc_t *, isp_pdb_21xx_t *, isp_pdb_21xx_t *);
extern void
isp_get_pdb_24xx(ispsoftc_t *, isp_pdb_24xx_t *, isp_pdb_24xx_t *);
extern void
isp_get_plogx(ispsoftc_t *, isp_plogx_t *, isp_plogx_t *);
extern void
isp_put_plogx(ispsoftc_t *, isp_plogx_t *, isp_plogx_t *);
extern void
isp_get_ct_pt(ispsoftc_t *isp, isp_ct_pt_t *, isp_ct_pt_t *);
extern void
isp_get_ms(ispsoftc_t *isp, isp_ms_t *, isp_ms_t *);
extern void
isp_put_ct_pt(ispsoftc_t *isp, isp_ct_pt_t *, isp_ct_pt_t *);
extern void
isp_put_ms(ispsoftc_t *isp, isp_ms_t *, isp_ms_t *);
extern void
isp_put_sns_request(ispsoftc_t *, sns_screq_t *, sns_screq_t *);
extern void
isp_put_gid_ft_request(ispsoftc_t *, sns_gid_ft_req_t *,
    sns_gid_ft_req_t *);
extern void
isp_put_gxn_id_request(ispsoftc_t *, sns_gxn_id_req_t *,
    sns_gxn_id_req_t *);
extern void
isp_get_sns_response(ispsoftc_t *, sns_scrsp_t *, sns_scrsp_t *, int);
extern void
isp_get_gid_ft_response(ispsoftc_t *, sns_gid_ft_rsp_t *,
    sns_gid_ft_rsp_t *, int);
extern void
isp_get_gxn_id_response(ispsoftc_t *, sns_gxn_id_rsp_t *,
    sns_gxn_id_rsp_t *);
extern void
isp_get_gff_id_response(ispsoftc_t *, sns_gff_id_rsp_t *,
    sns_gff_id_rsp_t *);
extern void
isp_get_ga_nxt_response(ispsoftc_t *, sns_ga_nxt_rsp_t *,
    sns_ga_nxt_rsp_t *);
extern void
isp_get_els(ispsoftc_t *, els_t *, els_t *);
extern void
isp_put_els(ispsoftc_t *, els_t *, els_t *);
extern void
isp_get_fc_hdr(ispsoftc_t *, fc_hdr_t *, fc_hdr_t *);
extern void
isp_get_fcp_cmnd_iu(ispsoftc_t *, fcp_cmnd_iu_t *, fcp_cmnd_iu_t *);
extern void isp_put_rft_id(ispsoftc_t *, rft_id_t *, rft_id_t *);
extern void isp_get_ct_hdr(ispsoftc_t *isp, ct_hdr_t *, ct_hdr_t *);
extern void isp_put_ct_hdr(ispsoftc_t *isp, ct_hdr_t *, ct_hdr_t *);

#define	ISP_HANDLE_MASK		0x7fff

#ifdef	ISP_TARGET_MODE
#if	defined(__NetBSD__) || defined(__OpenBSD__)
#include <dev/ic/isp_target.h>
#elif 	defined(__FreeBSD__)
#include <dev/isp/isp_target.h>
#else
#include "isp_target.h"
#endif

#define	IS_TARGET_HANDLE(x)     ((x) & 0x8000)

extern int isp_save_xs_tgt(ispsoftc_t *, void *, uint32_t *);
extern void *isp_find_xs_tgt(ispsoftc_t *, uint32_t);
extern uint32_t isp_find_tgt_handle(ispsoftc_t *, void *);
extern void isp_destroy_tgt_handle(ispsoftc_t *, uint32_t);

extern void
isp_put_atio(ispsoftc_t *, at_entry_t *, at_entry_t *);
extern void
isp_get_atio(ispsoftc_t *, at_entry_t *, at_entry_t *);
extern void
isp_put_atio2(ispsoftc_t *, at2_entry_t *, at2_entry_t *);
extern void
isp_put_atio2e(ispsoftc_t *, at2e_entry_t *, at2e_entry_t *);
extern void
isp_get_atio2(ispsoftc_t *, at2_entry_t *, at2_entry_t *);
extern void
isp_get_atio2e(ispsoftc_t *, at2e_entry_t *, at2e_entry_t *);
extern void
isp_get_atio7(ispsoftc_t *isp, at7_entry_t *, at7_entry_t *);
extern void
isp_put_ctio(ispsoftc_t *, ct_entry_t *, ct_entry_t *);
extern void
isp_get_ctio(ispsoftc_t *, ct_entry_t *, ct_entry_t *);
extern void
isp_put_ctio2(ispsoftc_t *, ct2_entry_t *, ct2_entry_t *);
extern void
isp_put_ctio2e(ispsoftc_t *, ct2e_entry_t *, ct2e_entry_t *);
extern void
isp_put_ctio7(ispsoftc_t *, ct7_entry_t *, ct7_entry_t *);
extern void
isp_get_ctio2(ispsoftc_t *, ct2_entry_t *, ct2_entry_t *);
extern void
isp_get_ctio2e(ispsoftc_t *, ct2e_entry_t *, ct2e_entry_t *);
extern void
isp_get_ctio7(ispsoftc_t *, ct7_entry_t *, ct7_entry_t *);
extern void
isp_put_enable_lun(ispsoftc_t *, lun_entry_t *, lun_entry_t *);
extern void
isp_get_enable_lun(ispsoftc_t *, lun_entry_t *, lun_entry_t *);
extern void
isp_put_notify(ispsoftc_t *, in_entry_t *, in_entry_t *);
extern void
isp_get_notify(ispsoftc_t *, in_entry_t *, in_entry_t *);
extern void
isp_put_notify_fc(ispsoftc_t *, in_fcentry_t *, in_fcentry_t *);
extern void
isp_put_notify_fc_e(ispsoftc_t *, in_fcentry_e_t *, in_fcentry_e_t *);
extern void
isp_put_notify_24xx(ispsoftc_t *, in_fcentry_24xx_t *, in_fcentry_24xx_t *);
extern void
isp_get_notify_fc(ispsoftc_t *, in_fcentry_t *, in_fcentry_t *);
extern void
isp_get_notify_fc_e(ispsoftc_t *, in_fcentry_e_t *, in_fcentry_e_t *);
extern void
isp_get_notify_24xx(ispsoftc_t *, in_fcentry_24xx_t *, in_fcentry_24xx_t *);
extern void
isp_put_notify_ack(ispsoftc_t *, na_entry_t *, na_entry_t *);
extern void
isp_get_notify_ack(ispsoftc_t *, na_entry_t *, na_entry_t *);
extern void
isp_put_notify_24xx_ack(ispsoftc_t *, na_fcentry_24xx_t *, na_fcentry_24xx_t *);
extern void
isp_put_notify_ack_fc(ispsoftc_t *, na_fcentry_t *, na_fcentry_t *);
extern void
isp_put_notify_ack_fc_e(ispsoftc_t *, na_fcentry_e_t *, na_fcentry_e_t *);
extern void isp_put_notify_ack_24xx(ispsoftc_t *, na_fcentry_24xx_t *,
    na_fcentry_24xx_t *);
extern void
isp_get_notify_ack_fc(ispsoftc_t *, na_fcentry_t *, na_fcentry_t *);
extern void
isp_get_notify_ack_fc_e(ispsoftc_t *, na_fcentry_e_t *, na_fcentry_e_t *);
extern void isp_get_notify_ack_24xx(ispsoftc_t *, na_fcentry_24xx_t *,
    na_fcentry_24xx_t *);
extern void
isp_get_abts(ispsoftc_t *, abts_t *, abts_t *);
extern void
isp_put_abts_rsp(ispsoftc_t *, abts_rsp_t *, abts_rsp_t *);
extern void
isp_get_abts_rsp(ispsoftc_t *, abts_rsp_t *, abts_rsp_t *);
#endif	/* ISP_TARGET_MODE */
#endif	/* _ISP_LIBRARY_H */
