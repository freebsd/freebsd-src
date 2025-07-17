/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 *  Copyright (c) 2009-2020 Alexander Motin <mav@FreeBSD.org>
 *  Copyright (c) 1997-2009 by Matthew Jacob
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
 * 
 */
#ifndef _ISP_LIBRARY_H
#define _ISP_LIBRARY_H

/*
 * Common command shipping routine.
 *
 * This used to be platform specific, but basically once you get the segment
 * stuff figured out, you can make all the code in one spot.
 */
int isp_send_cmd(ispsoftc_t *, void *, void *, uint32_t);

/*
 * Handle management functions.
 *
 * These handles are associate with a command.
 */
uint32_t isp_allocate_handle(ispsoftc_t *, void *, int);
void *isp_find_xs(ispsoftc_t *, uint32_t);
uint32_t isp_find_handle(ispsoftc_t *, void *);
void isp_destroy_handle(ispsoftc_t *, uint32_t);

/*
 * Request Queue allocation
 */
static inline int
isp_rqentry_avail(ispsoftc_t *isp, uint32_t num)
{
	if (ISP_QAVAIL(isp) >= num)
		return (1);
	/* We don't have enough in cached.  Reread the hardware. */
	isp->isp_reqodx = ISP_READ(isp, BIU2400_REQOUTP);
	return (ISP_QAVAIL(isp) >= num);
}

static inline void *
isp_getrqentry(ispsoftc_t *isp)
{
	if (!isp_rqentry_avail(isp, 1))
		return (NULL);
	return (ISP_QUEUE_ENTRY(isp->isp_rquest, isp->isp_reqidx));
}

/*
 * Queue Entry debug functions
 */
void isp_print_qentry (ispsoftc_t *, const char *, int, void *);
void isp_print_bytes(ispsoftc_t *, const char *, int, void *);

/*
 * Fibre Channel specific routines and data.
 */
extern const char *isp_class3_roles[4];
int isp_fc_runstate(ispsoftc_t *, int, int);
void isp_dump_portdb(ispsoftc_t *, int);
void isp_gen_role_str(char *, size_t, uint16_t);

const char *isp_fc_fw_statename(int);
const char *isp_fc_loop_statename(int);
const char *isp_fc_toponame(fcparam *);

/*
 * Cleanup
 */
void isp_clear_commands(ispsoftc_t *);

/*
 * Put/Get routines to push from CPU view to device view
 * or to pull from device view to CPU view for various
 * data structures (IOCB)
 */
void isp_put_hdr(ispsoftc_t *, isphdr_t *, isphdr_t *);
void isp_get_hdr(ispsoftc_t *, isphdr_t *, isphdr_t *);
int isp_get_response_type(ispsoftc_t *, isphdr_t *);
void isp_put_marker_24xx(ispsoftc_t *, isp_marker_24xx_t *, isp_marker_24xx_t *);
void isp_put_request_t7(ispsoftc_t *, ispreqt7_t *, ispreqt7_t *);
void isp_put_24xx_tmf(ispsoftc_t *, isp24xx_tmf_t *, isp24xx_tmf_t *);
void isp_put_24xx_abrt(ispsoftc_t *, isp24xx_abrt_t *, isp24xx_abrt_t *);
void isp_put_cont64_req(ispsoftc_t *, ispcontreq64_t *, ispcontreq64_t *);
void isp_get_cont_response(ispsoftc_t *, ispstatus_cont_t *, ispstatus_cont_t *);
void isp_get_24xx_response(ispsoftc_t *, isp24xx_statusreq_t *, isp24xx_statusreq_t *);
void isp_get_24xx_abrt(ispsoftc_t *, isp24xx_abrt_t *, isp24xx_abrt_t *);
void isp_put_icb_2400(ispsoftc_t *, isp_icb_2400_t *, isp_icb_2400_t *);
void isp_put_icb_2400_vpinfo(ispsoftc_t *, isp_icb_2400_vpinfo_t *, isp_icb_2400_vpinfo_t *);
void isp_put_vp_port_info(ispsoftc_t *, vp_port_info_t *, vp_port_info_t *);
void isp_get_vp_port_info(ispsoftc_t *, vp_port_info_t *, vp_port_info_t *);
void isp_put_vp_ctrl_info(ispsoftc_t *, vp_ctrl_info_t *, vp_ctrl_info_t *);
void isp_get_vp_ctrl_info(ispsoftc_t *, vp_ctrl_info_t *, vp_ctrl_info_t *);
void isp_put_vp_modify(ispsoftc_t *, vp_modify_t *, vp_modify_t *);
void isp_get_vp_modify(ispsoftc_t *, vp_modify_t *, vp_modify_t *);
void isp_get_pdb_24xx(ispsoftc_t *, isp_pdb_24xx_t *, isp_pdb_24xx_t *);
void isp_get_pnhle_24xx(ispsoftc_t *, isp_pnhle_24xx_t *, isp_pnhle_24xx_t *);
void isp_get_ridacq(ispsoftc_t *, isp_ridacq_t *, isp_ridacq_t *);
void isp_get_plogx(ispsoftc_t *, isp_plogx_t *, isp_plogx_t *);
void isp_put_plogx(ispsoftc_t *, isp_plogx_t *, isp_plogx_t *);
void isp_get_ct_pt(ispsoftc_t *isp, isp_ct_pt_t *, isp_ct_pt_t *);
void isp_put_ct_pt(ispsoftc_t *isp, isp_ct_pt_t *, isp_ct_pt_t *);
void isp_put_gid_ft_request(ispsoftc_t *, sns_gid_ft_req_t *, sns_gid_ft_req_t *);
void isp_get_gid_xx_response(ispsoftc_t *, sns_gid_xx_rsp_t *, sns_gid_xx_rsp_t *, int);
void isp_get_gxn_id_response(ispsoftc_t *, sns_gxn_id_rsp_t *, sns_gxn_id_rsp_t *);
void isp_get_gft_id_response(ispsoftc_t *, sns_gft_id_rsp_t *, sns_gft_id_rsp_t *);
void isp_get_gff_id_response(ispsoftc_t *, sns_gff_id_rsp_t *, sns_gff_id_rsp_t *);
void isp_get_ga_nxt_response(ispsoftc_t *, sns_ga_nxt_rsp_t *, sns_ga_nxt_rsp_t *);
void isp_get_fc_hdr(ispsoftc_t *, fc_hdr_t *, fc_hdr_t *);
void isp_put_fc_hdr(ispsoftc_t *, fc_hdr_t *, fc_hdr_t *);
void isp_get_fcp_cmnd_iu(ispsoftc_t *, fcp_cmnd_iu_t *, fcp_cmnd_iu_t *);
void isp_put_rft_id(ispsoftc_t *, rft_id_t *, rft_id_t *);
void isp_put_rspn_id(ispsoftc_t *, rspn_id_t *, rspn_id_t *);
void isp_put_rff_id(ispsoftc_t *, rff_id_t *, rff_id_t *);
void isp_put_rsnn_nn(ispsoftc_t *, rsnn_nn_t *, rsnn_nn_t *);
void isp_get_ct_hdr(ispsoftc_t *isp, ct_hdr_t *, ct_hdr_t *);
void isp_put_ct_hdr(ispsoftc_t *isp, ct_hdr_t *, ct_hdr_t *);
void isp_put_fcp_rsp_iu(ispsoftc_t *isp, fcp_rsp_iu_t *, fcp_rsp_iu_t *);
void isp_get_atio7(ispsoftc_t *isp, at7_entry_t *, at7_entry_t *);
void isp_put_ctio7(ispsoftc_t *, ct7_entry_t *, ct7_entry_t *);
void isp_get_ctio7(ispsoftc_t *, ct7_entry_t *, ct7_entry_t *);
void isp_put_notify_24xx(ispsoftc_t *, in_fcentry_24xx_t *, in_fcentry_24xx_t *);
void isp_get_notify_24xx(ispsoftc_t *, in_fcentry_24xx_t *, in_fcentry_24xx_t *);
void isp_put_notify_ack_24xx(ispsoftc_t *, na_fcentry_24xx_t *, na_fcentry_24xx_t *);
void isp_get_notify_ack_24xx(ispsoftc_t *, na_fcentry_24xx_t *, na_fcentry_24xx_t *);
void isp_get_abts(ispsoftc_t *, abts_t *, abts_t *);
void isp_put_abts_rsp(ispsoftc_t *, abts_rsp_t *, abts_rsp_t *);
void isp_get_abts_rsp(ispsoftc_t *, abts_rsp_t *, abts_rsp_t *);

void isp_put_entry(ispsoftc_t *, void *, void *);
void isp_get_entry(ispsoftc_t *, void *, void *);
int isp_send_entry(ispsoftc_t *, void *);
int isp_exec_entry_mbox(ispsoftc_t *, void *, void *, int);
int isp_exec_entry_queue(ispsoftc_t *, void *, void *, int);

#define ISP_HANDLE_MASK  0x7fff

#ifdef ISP_TARGET_MODE
#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <dev/ic/isp_target.h>
#elif  defined(__FreeBSD__)
#include <dev/isp/isp_target.h>
#else
#include "isp_target.h"
#endif
#endif

int isp_find_pdb_empty(ispsoftc_t *, int, fcportdb_t **);
int isp_find_pdb_by_wwpn(ispsoftc_t *, int, uint64_t, fcportdb_t **);
int isp_find_pdb_by_handle(ispsoftc_t *, int, uint16_t, fcportdb_t **);
int isp_find_pdb_by_portid(ispsoftc_t *, int, uint32_t, fcportdb_t **);
#ifdef ISP_TARGET_MODE
void isp_find_chan_by_did(ispsoftc_t *, uint32_t, uint16_t *);
void isp_add_wwn_entry(ispsoftc_t *, int, uint64_t, uint64_t, uint16_t, uint32_t, uint16_t);
void isp_del_wwn_entry(ispsoftc_t *, int, uint64_t, uint16_t, uint32_t);
#endif /* ISP_TARGET_MODE */
#endif /* _ISP_LIBRARY_H */
