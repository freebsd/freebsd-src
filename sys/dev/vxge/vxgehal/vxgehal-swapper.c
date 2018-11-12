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

#include <dev/vxge/vxgehal/vxgehal.h>

/*
 * _hal_legacy_swapper_set - Set the swapper bits for the legacy secion.
 * @pdev: PCI device object.
 * @regh: BAR0 mapped memory handle (Solaris), or simply PCI device @pdev
 *	(Linux and the rest.)
 * @legacy_reg: Address of the legacy register space.
 *
 * Set the swapper bits appropriately for the lagacy section.
 *
 * Returns:  VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_SWAPPER_CTRL - failed.
 *
 * See also: vxge_hal_status_e {}.
 */
vxge_hal_status_e
__hal_legacy_swapper_set(
    pci_dev_h pdev,
    pci_reg_h regh,
    vxge_hal_legacy_reg_t *legacy_reg)
{
	u64 val64;
	vxge_hal_status_e status;

	vxge_assert(legacy_reg != NULL);

	vxge_hal_trace_log_driver("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_driver(
	    "pdev = 0x"VXGE_OS_STXFMT", regh = 0x"VXGE_OS_STXFMT", "
	    "legacy_reg = 0x"VXGE_OS_STXFMT, (ptr_t) pdev, (ptr_t) regh,
	    (ptr_t) legacy_reg);

	val64 = vxge_os_pio_mem_read64(pdev, regh, &legacy_reg->toc_swapper_fb);

	vxge_hal_info_log_driver("TOC Swapper Fb: 0x"VXGE_OS_LLXFMT, val64);

	vxge_os_wmb();

	switch (val64) {

	case VXGE_HAL_SWAPPER_INITIAL_VALUE:
		return (VXGE_HAL_OK);

	case VXGE_HAL_SWAPPER_BYTE_SWAPPED_BIT_FLIPPED:
		vxge_os_pio_mem_write64(pdev, regh,
		    VXGE_HAL_SWAPPER_READ_BYTE_SWAP_ENABLE,
		    &legacy_reg->pifm_rd_swap_en);
		vxge_os_pio_mem_write64(pdev, regh,
		    VXGE_HAL_SWAPPER_READ_BIT_FLAP_ENABLE,
		    &legacy_reg->pifm_rd_flip_en);
		vxge_os_pio_mem_write64(pdev, regh,
		    VXGE_HAL_SWAPPER_WRITE_BYTE_SWAP_ENABLE,
		    &legacy_reg->pifm_wr_swap_en);
		vxge_os_pio_mem_write64(pdev, regh,
		    VXGE_HAL_SWAPPER_WRITE_BIT_FLAP_ENABLE,
		    &legacy_reg->pifm_wr_flip_en);
		break;

	case VXGE_HAL_SWAPPER_BYTE_SWAPPED:
		vxge_os_pio_mem_write64(pdev, regh,
		    VXGE_HAL_SWAPPER_READ_BYTE_SWAP_ENABLE,
		    &legacy_reg->pifm_rd_swap_en);
		vxge_os_pio_mem_write64(pdev, regh,
		    VXGE_HAL_SWAPPER_WRITE_BYTE_SWAP_ENABLE,
		    &legacy_reg->pifm_wr_swap_en);
		break;

	case VXGE_HAL_SWAPPER_BIT_FLIPPED:
		vxge_os_pio_mem_write64(pdev, regh,
		    VXGE_HAL_SWAPPER_READ_BIT_FLAP_ENABLE,
		    &legacy_reg->pifm_rd_flip_en);
		vxge_os_pio_mem_write64(pdev, regh,
		    VXGE_HAL_SWAPPER_WRITE_BIT_FLAP_ENABLE,
		    &legacy_reg->pifm_wr_flip_en);
		break;

	}

	vxge_os_wmb();

	val64 = vxge_os_pio_mem_read64(pdev, regh, &legacy_reg->toc_swapper_fb);

	if (val64 == VXGE_HAL_SWAPPER_INITIAL_VALUE) {
		status = VXGE_HAL_OK;
	} else {
		vxge_hal_err_log_driver("%s:TOC Swapper setting failed",
		    __func__);
		status = VXGE_HAL_ERR_SWAPPER_CTRL;
	}

	vxge_hal_info_log_driver("TOC Swapper Fb: 0x"VXGE_OS_LLXFMT, val64);

	vxge_hal_trace_log_driver("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * __hal_vpath_swapper_set - Set the swapper bits for the vpath.
 * @hldev: HAL device object.
 * @vp_id: Vpath Id
 *
 * Set the swapper bits appropriately for the vpath.
 *
 * Returns:  VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_SWAPPER_CTRL - failed.
 *
 * See also: vxge_hal_status_e {}.
 */
vxge_hal_status_e
__hal_vpath_swapper_set(
    vxge_hal_device_t *hldev,
    u32 vp_id)
{
#if !defined(VXGE_OS_HOST_BIG_ENDIAN)
	u64 val64;
	vxge_hal_vpath_reg_t *vpath_reg;

	vxge_assert(hldev != NULL);

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath(
	    "hldev = 0x"VXGE_OS_STXFMT", vp_id = %d",
	    (ptr_t) hldev, vp_id);

	vpath_reg = ((__hal_device_t *) hldev)->vpath_reg[vp_id];

	val64 = vxge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	    &vpath_reg->vpath_general_cfg1);

	vxge_os_wmb();

	val64 |= VXGE_HAL_VPATH_GENERAL_CFG1_CTL_BYTE_SWAPEN;

	vxge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	    val64,
	    &vpath_reg->vpath_general_cfg1);
	vxge_os_wmb();


	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
#endif
	return (VXGE_HAL_OK);
}


/*
 * __hal_kdfc_swapper_set - Set the swapper bits for the kdfc.
 * @hldev: HAL device object.
 * @vp_id: Vpath Id
 *
 * Set the swapper bits appropriately for the vpath.
 *
 * Returns:  VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_SWAPPER_CTRL - failed.
 *
 * See also: vxge_hal_status_e {}.
 */
vxge_hal_status_e
__hal_kdfc_swapper_set(
    vxge_hal_device_t *hldev,
    u32 vp_id)
{
	u64 val64;
	vxge_hal_vpath_reg_t *vpath_reg;
	vxge_hal_legacy_reg_t *legacy_reg;

	vxge_assert(hldev != NULL);

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("hldev = 0x"VXGE_OS_STXFMT", vp_id = %d",
	    (ptr_t) hldev, vp_id);

	vpath_reg = ((__hal_device_t *) hldev)->vpath_reg[vp_id];
	legacy_reg = ((__hal_device_t *) hldev)->legacy_reg;

	val64 = vxge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	    &legacy_reg->pifm_wr_swap_en);

	if (val64 == VXGE_HAL_SWAPPER_WRITE_BYTE_SWAP_ENABLE) {

		val64 = vxge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
		    &vpath_reg->kdfcctl_cfg0);

		vxge_os_wmb();

		val64 |= VXGE_HAL_KDFCCTL_CFG0_BYTE_SWAPEN_FIFO0 |
		    VXGE_HAL_KDFCCTL_CFG0_BYTE_SWAPEN_FIFO1 |
		    VXGE_HAL_KDFCCTL_CFG0_BYTE_SWAPEN_FIFO2;

		vxge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
		    val64,
		    &vpath_reg->kdfcctl_cfg0);
		vxge_os_wmb();

	}

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}
