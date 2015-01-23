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

#ifndef	VXGE_HAL_LEGACY_REGS_H
#define	VXGE_HAL_LEGACY_REGS_H

__EXTERN_BEGIN_DECLS

typedef struct vxge_hal_legacy_reg_t {

	u8	unused00010[0x00010];

/* 0x00010 */	u64	toc_swapper_fb;
#define	VXGE_HAL_TOC_SWAPPER_FB_INITIAL_VAL(val)	vBIT(val, 0, 64)
/* 0x00018 */	u64	pifm_rd_swap_en;
#define	VXGE_HAL_PIFM_RD_SWAP_EN_PIFM_RD_SWAP_EN(val)	vBIT(val, 0, 64)
/* 0x00020 */	u64	pifm_rd_flip_en;
#define	VXGE_HAL_PIFM_RD_FLIP_EN_PIFM_RD_FLIP_EN(val)	vBIT(val, 0, 64)
/* 0x00028 */	u64	pifm_wr_swap_en;
#define	VXGE_HAL_PIFM_WR_SWAP_EN_PIFM_WR_SWAP_EN(val)	vBIT(val, 0, 64)
/* 0x00030 */	u64	pifm_wr_flip_en;
#define	VXGE_HAL_PIFM_WR_FLIP_EN_PIFM_WR_FLIP_EN(val)	vBIT(val, 0, 64)
/* 0x00038 */	u64	toc_first_pointer;
#define	VXGE_HAL_TOC_FIRST_POINTER_INITIAL_VAL(val)	vBIT(val, 0, 64)
/* 0x00040 */	u64	host_access_en;
#define	VXGE_HAL_HOST_ACCESS_EN_HOST_ACCESS_EN(val)	vBIT(val, 0, 64)

} vxge_hal_legacy_reg_t;

__EXTERN_END_DECLS

#endif	/* VXGE_HAL_LEGACY_REGS_H */
