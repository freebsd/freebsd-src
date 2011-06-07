/*-
 * Copyright(c) 2002-2011 Exar Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification are permitted provided the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *    3. Neither the name of the Exar Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*$FreeBSD$*/

#ifndef	VXGE_HAL_PCICFGMGMT_REGS_H
#define	VXGE_HAL_PCICFGMGMT_REGS_H

__EXTERN_BEGIN_DECLS

typedef struct vxge_hal_pcicfgmgmt_reg_t {

/* 0x00000 */	u64	resource_no;
#define	VXGE_HAL_RESOURCE_NO_PFN_OR_VF	BIT(3)
/* 0x00008 */	u64	bargrp_pf_or_vf_bar0_mask;
#define	VXGE_HAL_BARGRP_PF_OR_VF_BAR0_MASK_BARGRP_PF_OR_VF_BAR0_MASK(val)\
							    vBIT(val, 2, 6)
/* 0x00010 */	u64	bargrp_pf_or_vf_bar1_mask;
#define	VXGE_HAL_BARGRP_PF_OR_VF_BAR1_MASK_BARGRP_PF_OR_VF_BAR1_MASK(val)\
							    vBIT(val, 2, 6)
/* 0x00018 */	u64	bargrp_pf_or_vf_bar2_mask;
#define	VXGE_HAL_BARGRP_PF_OR_VF_BAR2_MASK_BARGRP_PF_OR_VF_BAR2_MASK(val)\
							    vBIT(val, 2, 6)
/* 0x00020 */	u64	msixgrp_no;
#define	VXGE_HAL_MSIXGRP_NO_TABLE_SIZE(val)		    vBIT(val, 5, 11)

} vxge_hal_pcicfgmgmt_reg_t;

__EXTERN_END_DECLS

#endif	/* VXGE_HAL_PCICFGMGMT_REGS_H */
