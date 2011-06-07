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
 * __hal_srpcim_alarm_process - Process Alarms.
 * @hldev: HAL Device
 * @srpcim_id: srpcim index
 * @skip_alarms: Flag to indicate if not to clear the alarms
 *
 * Process srpcim alarms.
 *
 */
vxge_hal_status_e
__hal_srpcim_alarm_process(
    __hal_device_t * hldev,
    u32 srpcim_id,
    u32 skip_alarms)
{
	u64 val64;
	u64 alarm_status;
	u64 pic_status;
	u64 xgmac_status;
	vxge_hal_srpcim_reg_t *srpcim_reg;

	vxge_assert(hldev != NULL);

	vxge_hal_trace_log_srpcim_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_srpcim_irq("hldev = 0x"VXGE_OS_STXFMT,
	    (ptr_t) hldev);

	srpcim_reg = hldev->srpcim_reg[srpcim_id];

	alarm_status = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &srpcim_reg->srpcim_general_int_status);

	vxge_hal_info_log_srpcim_irq("alarm_status = 0x"VXGE_OS_STXFMT,
	    (ptr_t) alarm_status);

	if (alarm_status & VXGE_HAL_SRPCIM_GENERAL_INT_STATUS_XMAC_INT) {

		xgmac_status = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &srpcim_reg->xgmac_sr_int_status);

		vxge_hal_info_log_srpcim_irq("xgmac_status = 0x"VXGE_OS_STXFMT,
		    (ptr_t) xgmac_status);

		if (xgmac_status &
		    VXGE_HAL_XGMAC_SR_INT_STATUS_ASIC_NTWK_SR_ERR_INT) {

			val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
			    hldev->header.regh0,
			    &srpcim_reg->asic_ntwk_sr_err_reg);

			vxge_hal_info_log_srpcim_irq("asic_ntwk_sr_err_reg = \
			    0x"VXGE_OS_STXFMT, (ptr_t) val64);

			if (!skip_alarms)
				vxge_os_pio_mem_write64(hldev->header.pdev,
				    hldev->header.regh0,
				    VXGE_HAL_INTR_MASK_ALL,
				    &srpcim_reg->asic_ntwk_sr_err_reg);

		}
	}

	if (alarm_status & VXGE_HAL_SRPCIM_GENERAL_INT_STATUS_PIC_INT) {

		pic_status = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &srpcim_reg->srpcim_ppif_int_status);

		vxge_hal_info_log_srpcim_irq("pic_status = 0x"VXGE_OS_STXFMT,
		    (ptr_t) pic_status);

		if (pic_status &
		    VXGE_HAL_SRPCIM_PPIF_INT_STATUS_SRPCIM_GEN_ERRORS_INT) {

			val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
			    hldev->header.regh0,
			    &srpcim_reg->srpcim_gen_errors_reg);

			vxge_hal_info_log_srpcim_irq("srpcim_gen_errors_reg = \
			    0x"VXGE_OS_STXFMT, (ptr_t) val64);

			if (!skip_alarms)
				vxge_os_pio_mem_write64(hldev->header.pdev,
				    hldev->header.regh0,
				    VXGE_HAL_INTR_MASK_ALL,
				    &srpcim_reg->srpcim_gen_errors_reg);
		}

		if (pic_status &
		    VXGE_HAL_SRPCIM_PPIF_INT_STATUS_MRPCIM_TO_SRPCIM_ALARM) {

			val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
			    hldev->header.regh0,
			    &srpcim_reg->mrpcim_to_srpcim_alarm_reg);

			vxge_hal_info_log_srpcim_irq("mrpcim_to_srpcim_alarm_reg = \
			    0x"VXGE_OS_STXFMT, (ptr_t) val64);

			if (!skip_alarms)
				vxge_os_pio_mem_write64(hldev->header.pdev,
				    hldev->header.regh0,
				    VXGE_HAL_INTR_MASK_ALL,
				    &srpcim_reg->mrpcim_to_srpcim_alarm_reg);

		}
	}

	if (alarm_status & ~(
	    VXGE_HAL_SRPCIM_GENERAL_INT_STATUS_PIC_INT |
	    VXGE_HAL_SRPCIM_GENERAL_INT_STATUS_XMAC_INT)) {
		vxge_hal_trace_log_srpcim_irq("%s:%s:%d Unknown Alarm",
		    __FILE__, __func__, __LINE__);
	}

	vxge_hal_trace_log_srpcim_irq("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_srpcim_alarm_process - Process srpcim Alarms.
 * @devh: Device Handle.
 * @skip_alarms: Flag to indicate if not to clear the alarms
 *
 * Process srpcim alarms.
 *
 */
vxge_hal_status_e
vxge_hal_srpcim_alarm_process(
    vxge_hal_device_h devh,
    u32 skip_alarms)
{
	u32 i;
	u64 val64;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh != NULL);

	vxge_hal_trace_log_srpcim_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_srpcim_irq("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	if (!(hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_SRPCIM)) {
		vxge_hal_trace_log_srpcim_irq("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

		return (VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

	}

	if (hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM) {

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->srpcim_to_mrpcim_alarm_reg);

		vxge_hal_trace_log_srpcim_irq("srpcim_to_mrpcim_alarm_reg = \
		    0x"VXGE_OS_STXFMT, (ptr_t) val64);

		for (i = 0; i < VXGE_HAL_TITAN_SRPCIM_REG_SPACES; i++) {

			if (val64 & mBIT(i)) {
				status = __hal_srpcim_alarm_process(hldev,
				    i, skip_alarms);
			}
		}

		if (!skip_alarms)
			vxge_os_pio_mem_write64(hldev->header.pdev,
			    hldev->header.regh0,
			    VXGE_HAL_INTR_MASK_ALL,
			    &hldev->mrpcim_reg->srpcim_to_mrpcim_alarm_reg);
	} else {
		status = __hal_srpcim_alarm_process(hldev,
		    hldev->srpcim_id, skip_alarms);
	}

	vxge_hal_trace_log_srpcim_irq("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * __hal_srpcim_intr_enable - Enable srpcim interrupts.
 * @hldev: Hal Device.
 * @srpcim_id: SRPCIM Id
 *
 * Enable srpcim interrupts.
 *
 * See also: __hal_srpcim_intr_disable()
 */
vxge_hal_status_e
__hal_srpcim_intr_enable(
    __hal_device_t * hldev,
    u32 srpcim_id)
{
	vxge_hal_srpcim_reg_t *srpcim_reg;

	vxge_assert(hldev != NULL);

	vxge_hal_trace_log_srpcim("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_srpcim("hldev = 0x"VXGE_OS_STXFMT,
	    (ptr_t) hldev);

	srpcim_reg = hldev->srpcim_reg[srpcim_id];

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    VXGE_HAL_INTR_MASK_ALL,
	    &srpcim_reg->srpcim_gen_errors_reg);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) VXGE_HAL_INTR_MASK_ALL,
	    &srpcim_reg->mrpcim_to_srpcim_alarm_reg);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) VXGE_HAL_INTR_MASK_ALL,
	    &srpcim_reg->vpath_to_srpcim_alarm_reg);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) VXGE_HAL_INTR_MASK_ALL,
	    &srpcim_reg->srpcim_ppif_int_status);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) VXGE_HAL_INTR_MASK_ALL,
	    &srpcim_reg->mrpcim_msg_reg);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) VXGE_HAL_INTR_MASK_ALL,
	    &srpcim_reg->vpath_msg_reg);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) VXGE_HAL_INTR_MASK_ALL,
	    &srpcim_reg->srpcim_pcipif_int_status);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) VXGE_HAL_INTR_MASK_ALL,
	    &srpcim_reg->asic_ntwk_sr_err_reg);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) VXGE_HAL_INTR_MASK_ALL,
	    &srpcim_reg->xgmac_sr_int_status);

	vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &srpcim_reg->srpcim_general_int_status);

	/* Unmask the individual interrupts. */
	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    0,
	    &srpcim_reg->vpath_msg_mask);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    0,
	    &srpcim_reg->srpcim_pcipif_int_mask);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) bVAL32(~VXGE_HAL_SRPCIM_GENERAL_INT_MASK_PCI_INT, 0),
	    &srpcim_reg->srpcim_general_int_mask);

	vxge_hal_trace_log_srpcim("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_srpcim_intr_enable - Enable srpcim interrupts.
 * @devh: Hal Device.
 *
 * Enable srpcim interrupts.
 *
 * See also: vxge_hal_srpcim_intr_disable()
 */
vxge_hal_status_e
vxge_hal_srpcim_intr_enable(
    vxge_hal_device_h devh)
{
	u32 i;
	vxge_hal_status_e status;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh != NULL);

	vxge_hal_trace_log_srpcim("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_srpcim("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	if (!(hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_SRPCIM)) {
		vxge_hal_trace_log_srpcim("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

		return (VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

	}

	if (hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM) {

		for (i = 0; i < VXGE_HAL_TITAN_SRPCIM_REG_SPACES; i++) {

			status = __hal_srpcim_intr_enable(hldev, i);

		}

	} else {
		status = __hal_srpcim_intr_enable(hldev, hldev->srpcim_id);
	}

	vxge_hal_trace_log_srpcim("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * __hal_srpcim_intr_disable - Disable srpcim interrupts.
 * @hldev: Hal Device.
 * @srpcim_id: SRPCIM Id
 *
 * Disable srpcim interrupts.
 *
 * See also: __hal_srpcim_intr_enable()
 */
vxge_hal_status_e
__hal_srpcim_intr_disable(
    __hal_device_t * hldev,
    u32 srpcim_id)
{
	vxge_hal_srpcim_reg_t *srpcim_reg;

	vxge_assert(hldev != NULL);

	vxge_hal_trace_log_srpcim("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_srpcim("hldev = 0x"VXGE_OS_STXFMT,
	    (ptr_t) hldev);

	srpcim_reg = hldev->srpcim_reg[srpcim_id];

	/* Mask the individual interrupts. */
	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) VXGE_HAL_INTR_MASK_ALL,
	    &srpcim_reg->vpath_msg_mask);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) VXGE_HAL_INTR_MASK_ALL,
	    &srpcim_reg->srpcim_pcipif_int_mask);

	vxge_hal_pio_mem_write32_upper(
	    hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) VXGE_HAL_INTR_MASK_ALL,
	    &srpcim_reg->srpcim_general_int_mask);

	vxge_hal_trace_log_srpcim("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);

}

/*
 * vxge_hal_srpcim_intr_disable - Disable srpcim interrupts.
 * @devh: Hal Device.
 *
 * Disable srpcim interrupts.
 *
 * See also: vxge_hal_srpcim_intr_enable()
 */
vxge_hal_status_e
vxge_hal_srpcim_intr_disable(
    vxge_hal_device_h devh)
{
	u32 i;
	vxge_hal_status_e status;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh != NULL);

	vxge_hal_trace_log_srpcim("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_srpcim("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	if (!(hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_SRPCIM)) {
		vxge_hal_trace_log_srpcim("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

		return (VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

	}

	if (hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM) {

		for (i = 0; i < VXGE_HAL_TITAN_SRPCIM_REG_SPACES; i++) {

			status = __hal_srpcim_intr_disable(hldev, i);

		}

	} else {
		status = __hal_srpcim_intr_disable(hldev, hldev->srpcim_id);
	}

	vxge_hal_trace_log_srpcim("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * vxge_hal_srpcim_msix_set - Associate MSIX vector with srpcim alarm
 * @hldev: HAL device.
 * @alarm_msix_id: MSIX vector for alarm.
 *
 * This API will associate a given MSIX vector numbers with srpcim alarm
 */
vxge_hal_status_e
vxge_hal_srpcim_msix_set(vxge_hal_device_h devh, int alarm_msix_id)
{
	u32 i;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh != NULL);

	vxge_hal_trace_log_srpcim("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_srpcim("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	if (!(hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_SRPCIM)) {
		vxge_hal_trace_log_srpcim("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

		return (VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

	}

	if (hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM) {

		for (i = 0; i < VXGE_HAL_TITAN_SRPCIM_REG_SPACES; i++) {

			vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
			    hldev->header.regh0,
			    (u32) bVAL32(
			    VXGE_HAL_SRPCIM_INTERRUPT_CFG1_ALARM_MAP_TO_MSG(
			    alarm_msix_id),
			    0),
			    &hldev->srpcim_reg[i]->srpcim_interrupt_cfg1);

		}

	} else {
		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0,
		    (u32) bVAL32(
		    VXGE_HAL_SRPCIM_INTERRUPT_CFG1_ALARM_MAP_TO_MSG(
		    alarm_msix_id),
		    0),
		    &hldev->srpcim_reg[hldev->srpcim_id]->
		    srpcim_interrupt_cfg1);
	}

	vxge_hal_trace_log_srpcim("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * vxge_hal_srpcim_msix_mask - Mask MSIX Vector.
 * @hldev: HAL device.
 *
 * The function masks the srpcim msix interrupt
 *
 */
void
vxge_hal_srpcim_msix_mask(vxge_hal_device_h devh)
{
	u32 i;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh != NULL);

	vxge_hal_trace_log_srpcim("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_srpcim("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	if (!(hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_SRPCIM)) {
		vxge_hal_trace_log_srpcim("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

		return;

	}

	if (hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM) {

		for (i = 0; i < VXGE_HAL_TITAN_SRPCIM_REG_SPACES; i++) {

			vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
			    hldev->header.regh0,
			    (u32) bVAL32(
			    VXGE_HAL_SRPCIM_SET_MSIX_MASK_SRPCIM_SET_MSIX_MASK,
			    0),
			    &hldev->srpcim_reg[i]->srpcim_set_msix_mask);

		}

	} else {
		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0,
		    (u32) bVAL32(
		    VXGE_HAL_SRPCIM_SET_MSIX_MASK_SRPCIM_SET_MSIX_MASK,
		    0),
		    &hldev->srpcim_reg[hldev->srpcim_id]->srpcim_set_msix_mask);
	}

	vxge_hal_trace_log_srpcim("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_srpcim_msix_clear - Clear MSIX Vector.
 * @hldev: HAL device.
 *
 * The function clears the srpcim msix interrupt
 *
 */
void
vxge_hal_srpcim_msix_clear(vxge_hal_device_h devh)
{
	u32 i;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh != NULL);

	vxge_hal_trace_log_srpcim("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_srpcim("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	if (!(hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_SRPCIM)) {
		vxge_hal_trace_log_srpcim("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

		return;

	}

	if (hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM) {

		for (i = 0; i < VXGE_HAL_TITAN_SRPCIM_REG_SPACES; i++) {

			vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
			    hldev->header.regh0,
			    (u32) bVAL32(
			    VXGE_HAL_SRPCIM_CLEAR_MSIX_MASK_SRPCIM_CLEAR_MSIX_MASK,
			    0),
			    &hldev->srpcim_reg[i]->srpcim_clear_msix_mask);

		}

	} else {
		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0,
		    (u32) bVAL32(
		    VXGE_HAL_SRPCIM_CLEAR_MSIX_MASK_SRPCIM_CLEAR_MSIX_MASK,
		    0),
		    &hldev->srpcim_reg[hldev->srpcim_id]->
		    srpcim_clear_msix_mask);
	}

	vxge_hal_trace_log_srpcim("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_srpcim_msix_unmask - Unmask MSIX Vector.
 * @hldev: HAL device.
 *
 * The function unmasks the srpcim msix interrupt
 *
 */
void
vxge_hal_srpcim_msix_unmask(vxge_hal_device_h devh)
{
	u32 i;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh != NULL);

	vxge_hal_trace_log_srpcim("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_srpcim("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	if (!(hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_SRPCIM)) {
		vxge_hal_trace_log_srpcim("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

		return;

	}

	if (hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM) {

		for (i = 0; i < VXGE_HAL_TITAN_SRPCIM_REG_SPACES; i++) {

			vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
			    hldev->header.regh0,
			    (u32) bVAL32(
			    VXGE_HAL_SRPCIM_CLR_MSIX_ONE_SHOT_SRPCIM_CLR_MSIX_ONE_SHOT,
			    0),
			    &hldev->srpcim_reg[i]->srpcim_clr_msix_one_shot);

		}

	} else {
		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0,
		    (u32) bVAL32(
		    VXGE_HAL_SRPCIM_CLR_MSIX_ONE_SHOT_SRPCIM_CLR_MSIX_ONE_SHOT,
		    0),
		    &hldev->srpcim_reg[hldev->srpcim_id]->
		    srpcim_clr_msix_one_shot);
	}

	vxge_hal_trace_log_srpcim("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * __hal_srpcim_initialize - Initialize srpcim.
 * @hldev: HAL Device
 *
 * Initialize srpcim.
 *
 */
vxge_hal_status_e
__hal_srpcim_initialize(
    __hal_device_t * hldev)
{
	vxge_assert(hldev != NULL);

	vxge_hal_trace_log_srpcim("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_srpcim("hldev = 0x"VXGE_OS_STXFMT,
	    (ptr_t) hldev);

	if (!(hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_SRPCIM)) {
		vxge_hal_trace_log_srpcim("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_PRIVILAGED_OPEARATION);
		return (VXGE_HAL_ERR_PRIVILAGED_OPEARATION);
	}

	hldev->srpcim = (__hal_srpcim_t *)
	    vxge_os_malloc(hldev->header.pdev, sizeof(__hal_srpcim_t));

	if (hldev->srpcim == NULL) {
		vxge_hal_trace_log_srpcim("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_OUT_OF_MEMORY);
		return (VXGE_HAL_ERR_OUT_OF_MEMORY);
	}

	vxge_os_memzero(hldev->srpcim, sizeof(__hal_srpcim_t));

	vxge_hal_trace_log_srpcim("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * __hal_srpcim_terminate - Terminate srpcim.
 * @hldev: HAL Device
 *
 * Terminate srpcim.
 *
 */
vxge_hal_status_e
__hal_srpcim_terminate(
    __hal_device_t * hldev)
{
	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_assert(hldev != NULL);

	vxge_hal_trace_log_srpcim("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_srpcim("hldev = 0x"VXGE_OS_STXFMT,
	    (ptr_t) hldev);

	if (hldev->srpcim == NULL) {
		vxge_hal_trace_log_srpcim("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	vxge_os_free(hldev->header.pdev,
	    hldev->srpcim, sizeof(__hal_srpcim_t));

	hldev->srpcim = NULL;

	vxge_hal_trace_log_srpcim("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}
