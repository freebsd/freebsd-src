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

#ifndef	VXGE_HAL_SWAPPER_H
#define	VXGE_HAL_SWAPPER_H

__EXTERN_BEGIN_DECLS

#define	VXGE_HAL_SWAPPER_INITIAL_VALUE			0x0123456789abcdefULL
#define	VXGE_HAL_SWAPPER_BYTE_SWAPPED			0xefcdab8967452301ULL
#define	VXGE_HAL_SWAPPER_BIT_FLIPPED			0x80c4a2e691d5b3f7ULL
#define	VXGE_HAL_SWAPPER_BYTE_SWAPPED_BIT_FLIPPED	0xf7b3d591e6a2c480ULL

#define	VXGE_HAL_SWAPPER_READ_BYTE_SWAP_ENABLE		0xFFFFFFFFFFFFFFFFULL
#define	VXGE_HAL_SWAPPER_READ_BYTE_SWAP_DISABLE		0x0000000000000000ULL

#define	VXGE_HAL_SWAPPER_READ_BIT_FLAP_ENABLE		0xFFFFFFFFFFFFFFFFULL
#define	VXGE_HAL_SWAPPER_READ_BIT_FLAP_DISABLE		0x0000000000000000ULL

#define	VXGE_HAL_SWAPPER_WRITE_BYTE_SWAP_ENABLE		0xFFFFFFFFFFFFFFFFULL
#define	VXGE_HAL_SWAPPER_WRITE_BYTE_SWAP_DISABLE	0x0000000000000000ULL

#define	VXGE_HAL_SWAPPER_WRITE_BIT_FLAP_ENABLE		0xFFFFFFFFFFFFFFFFULL
#define	VXGE_HAL_SWAPPER_WRITE_BIT_FLAP_DISABLE		0x0000000000000000ULL

vxge_hal_status_e
__hal_legacy_swapper_set(
    pci_dev_h pdev,
    pci_reg_h regh,
    vxge_hal_legacy_reg_t *legacy_reg);
vxge_hal_status_e
__hal_vpath_swapper_set(
    vxge_hal_device_t *hldev,
    u32 vp_id);

vxge_hal_status_e
__hal_kdfc_swapper_set(
    vxge_hal_device_t *hldev,
    u32 vp_id);

__EXTERN_END_DECLS

#endif	/* VXGE_HAL_SWAPPER_H */
