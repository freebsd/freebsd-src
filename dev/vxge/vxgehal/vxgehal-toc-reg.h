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

#ifndef	VXGE_HAL_TOC_REGS_H
#define	VXGE_HAL_TOC_REGS_H

__EXTERN_BEGIN_DECLS

typedef struct vxge_hal_toc_reg_t {

	u8	unused00050[0x00050];

/* 0x00050 */	u64	toc_common_pointer;
#define	VXGE_HAL_TOC_COMMON_POINTER_INITIAL_VAL(val)	    vBIT(val, 0, 64)
/* 0x00058 */	u64	toc_memrepair_pointer;
#define	VXGE_HAL_TOC_MEMREPAIR_POINTER_INITIAL_VAL(val)	    vBIT(val, 0, 64)
/* 0x00060 */	u64	toc_pcicfgmgmt_pointer[17];
#define	VXGE_HAL_TOC_PCICFGMGMT_POINTER_INITIAL_VAL(val)    vBIT(val, 0, 64)
	u8	unused001e0[0x001e0 - 0x000e8];

/* 0x001e0 */	u64	toc_mrpcim_pointer;
#define	VXGE_HAL_TOC_MRPCIM_POINTER_INITIAL_VAL(val)	    vBIT(val, 0, 64)
/* 0x001e8 */	u64	toc_srpcim_pointer[17];
#define	VXGE_HAL_TOC_SRPCIM_POINTER_INITIAL_VAL(val)	    vBIT(val, 0, 64)
	u8	unused00278[0x00278 - 0x00270];

/* 0x00278 */	u64	toc_vpmgmt_pointer[17];
#define	VXGE_HAL_TOC_VPMGMT_POINTER_INITIAL_VAL(val)	    vBIT(val, 0, 64)
	u8	unused00390[0x00390 - 0x00300];

/* 0x00390 */	u64	toc_vpath_pointer[17];
#define	VXGE_HAL_TOC_VPATH_POINTER_INITIAL_VAL(val)	    vBIT(val, 0, 64)
	u8	unused004a0[0x004a0 - 0x00418];

/* 0x004a0 */	u64	toc_kdfc;
#define	VXGE_HAL_TOC_KDFC_INITIAL_OFFSET(val)		    vBIT(val, 0, 61)
#define	VXGE_HAL_TOC_KDFC_INITIAL_BIR(val)		    vBIT(val, 61, 3)
/* 0x004a8 */	u64	toc_usdc;
#define	VXGE_HAL_TOC_USDC_INITIAL_OFFSET(val)		    vBIT(val, 0, 61)
#define	VXGE_HAL_TOC_USDC_INITIAL_BIR(val)		    vBIT(val, 61, 3)
/* 0x004b0 */	u64	toc_kdfc_vpath_stride;
#define	VXGE_HAL_TOC_KDFC_VPATH_STRIDE_INITIAL_TOC_KDFC_VPATH_STRIDE(val)\
							    vBIT(val, 0, 64)
/* 0x004b8 */	u64	toc_kdfc_fifo_stride;
#define	VXGE_HAL_TOC_KDFC_FIFO_STRIDE_INITIAL_TOC_KDFC_FIFO_STRIDE(val)\
							    vBIT(val, 0, 64)

} vxge_hal_toc_reg_t;

__EXTERN_END_DECLS

#endif	/* VXGE_HAL_TOC_REGS_H */
