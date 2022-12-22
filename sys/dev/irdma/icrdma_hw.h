/*-
 * SPDX-License-Identifier: GPL-2.0 or Linux-OpenIB
 *
 * Copyright (c) 2017 - 2022 Intel Corporation
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenFabrics.org BSD license below:
 *
 *   Redistribution and use in source and binary forms, with or
 *   without modification, are permitted provided that the following
 *   conditions are met:
 *
 *    - Redistributions of source code must retain the above
 *	copyright notice, this list of conditions and the following
 *	disclaimer.
 *
 *    - Redistributions in binary form must reproduce the above
 *	copyright notice, this list of conditions and the following
 *	disclaimer in the documentation and/or other materials
 *	provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
/*$FreeBSD$*/

#ifndef ICRDMA_HW_H
#define ICRDMA_HW_H

#include "irdma.h"

#define VFPE_CQPTAIL1		0x0000a000
#define VFPE_CQPDB1		0x0000bc00
#define VFPE_CCQPSTATUS1	0x0000b800
#define VFPE_CCQPHIGH1		0x00009800
#define VFPE_CCQPLOW1		0x0000ac00
#define VFPE_CQARM1		0x0000b400
#define VFPE_CQARM1		0x0000b400
#define VFPE_CQACK1		0x0000b000
#define VFPE_AEQALLOC1		0x0000a400
#define VFPE_CQPERRCODES1	0x00009c00
#define VFPE_WQEALLOC1		0x0000c000
#define VFINT_DYN_CTLN(_i)	(0x00003800 + ((_i) * 4)) /* _i=0...63 */

#define PFPE_CQPTAIL		0x00500880
#define PFPE_CQPDB		0x00500800
#define PFPE_CCQPSTATUS		0x0050a000
#define PFPE_CCQPHIGH		0x0050a100
#define PFPE_CCQPLOW		0x0050a080
#define PFPE_CQARM		0x00502c00
#define PFPE_CQACK		0x00502c80
#define PFPE_AEQALLOC		0x00502d00
#define GLINT_DYN_CTL(_INT)	(0x00160000 + ((_INT) * 4)) /* _i=0...2047 */
#define GLPCI_LBARCTRL		0x0009de74
#define GLPE_CPUSTATUS0		0x0050ba5c
#define GLPE_CPUSTATUS1		0x0050ba60
#define GLPE_CPUSTATUS2		0x0050ba64
#define PFINT_AEQCTL		0x0016cb00
#define PFPE_CQPERRCODES	0x0050a200
#define PFPE_WQEALLOC		0x00504400
#define GLINT_CEQCTL(_INT)	(0x0015c000 + ((_INT) * 4)) /* _i=0...2047 */
#define VSIQF_PE_CTL1(_VSI)	(0x00414000 + ((_VSI) * 4)) /* _i=0...767 */
#define PFHMC_PDINV		0x00520300
#define GLHMC_VFPDINV(_i)	(0x00528300 + ((_i) * 4)) /* _i=0...31 */
#define GLPE_CRITERR		0x00534000
#define GLINT_RATE(_INT)	(0x0015A000 + ((_INT) * 4)) /* _i=0...2047 */ /* Reset Source: CORER */

#define PRTMAC_HSEC_CTL_RX_PAUSE_ENABLE_0	0x001e3180
#define PRTMAC_HSEC_CTL_RX_PAUSE_ENABLE_1	0x001e3184
#define PRTMAC_HSEC_CTL_RX_PAUSE_ENABLE_2	0x001e3188
#define PRTMAC_HSEC_CTL_RX_PAUSE_ENABLE_3	0x001e318c

#define PRTMAC_HSEC_CTL_TX_PAUSE_ENABLE_0	0x001e31a0
#define PRTMAC_HSEC_CTL_TX_PAUSE_ENABLE_1	0x001e31a4
#define PRTMAC_HSEC_CTL_TX_PAUSE_ENABLE_2	0x001e31a8
#define PRTMAC_HSEC_CTL_TX_PAUSE_ENABLE_3	0x001e31aC

#define PRTMAC_HSEC_CTL_RX_ENABLE_GPP_0		0x001e34c0
#define PRTMAC_HSEC_CTL_RX_ENABLE_GPP_1		0x001e34c4
#define PRTMAC_HSEC_CTL_RX_ENABLE_GPP_2		0x001e34c8
#define PRTMAC_HSEC_CTL_RX_ENABLE_GPP_3		0x001e34cC

#define PRTMAC_HSEC_CTL_RX_ENABLE_PPP_0		0x001e35c0
#define PRTMAC_HSEC_CTL_RX_ENABLE_PPP_1		0x001e35c4
#define PRTMAC_HSEC_CTL_RX_ENABLE_PPP_2		0x001e35c8
#define PRTMAC_HSEC_CTL_RX_ENABLE_PPP_3		0x001e35cC

#define GLDCB_TC2PFC				0x001d2694
#define PRTMAC_HSEC_CTL_RX_ENABLE_GCP		0x001e31c0

#define ICRDMA_DB_ADDR_OFFSET		(8 * 1024 * 1024 - 64 * 1024)

#define ICRDMA_VF_DB_ADDR_OFFSET	(64 * 1024)

#define ICRDMA_CCQPSTATUS_CCQP_DONE_S 0
#define ICRDMA_CCQPSTATUS_CCQP_DONE BIT_ULL(0)
#define ICRDMA_CCQPSTATUS_CCQP_ERR_S 31
#define ICRDMA_CCQPSTATUS_CCQP_ERR BIT_ULL(31)
#define ICRDMA_CQPSQ_STAG_PDID_S 46
#define ICRDMA_CQPSQ_STAG_PDID GENMASK_ULL(63, 46)
#define ICRDMA_CQPSQ_CQ_CEQID_S 22
#define ICRDMA_CQPSQ_CQ_CEQID GENMASK_ULL(31, 22)
#define ICRDMA_CQPSQ_CQ_CQID_S 0
#define ICRDMA_CQPSQ_CQ_CQID GENMASK_ULL(18, 0)
#define ICRDMA_COMMIT_FPM_CQCNT_S 0
#define ICRDMA_COMMIT_FPM_CQCNT GENMASK_ULL(19, 0)
#define ICRDMA_CQPSQ_UPESD_HMCFNID_S 0
#define ICRDMA_CQPSQ_UPESD_HMCFNID GENMASK_ULL(5, 0)

enum icrdma_device_caps_const {
	ICRDMA_MAX_WQ_FRAGMENT_COUNT		= 13,
	ICRDMA_MAX_SGE_RD			= 13,
	ICRDMA_MAX_STATS_COUNT = 128,

	ICRDMA_MAX_IRD_SIZE			= 32,
	ICRDMA_MAX_ORD_SIZE			= 64,
	ICRDMA_MIN_WQ_SIZE			= 8 /* WQEs */,

};

void icrdma_init_hw(struct irdma_sc_dev *dev);
void irdma_init_config_check(struct irdma_config_check *cc,
			     u8 traffic_class,
			     u16 qs_handle);
bool irdma_is_config_ok(struct irdma_config_check *cc, struct irdma_sc_vsi *vsi);
void irdma_check_fc_for_tc_update(struct irdma_sc_vsi *vsi,
				  struct irdma_l2params *l2params);
void irdma_check_fc_for_qp(struct irdma_sc_vsi *vsi, struct irdma_sc_qp *sc_qp);
#endif /* ICRDMA_HW_H*/
