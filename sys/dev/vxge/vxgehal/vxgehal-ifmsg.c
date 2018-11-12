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
 * __hal_ifmsg_wmsg_process - Process the srpcim to vpath wmsg
 * @vpath: vpath
 * @wmsg: wsmsg
 *
 * Processes the wmsg and invokes appropriate action
 */
void
__hal_ifmsg_wmsg_process(
    __hal_virtualpath_t *vpath,
    u64 wmsg)
{
	u32 msg_type;
	__hal_device_t *hldev = vpath->hldev;

	vxge_assert(vpath);

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath = 0x"VXGE_OS_STXFMT
	    ",wmsg = 0x"VXGE_OS_LLXFMT"", (ptr_t) vpath, wmsg);

	if ((vpath->vp_id != vpath->hldev->first_vp_id) ||
	    (vpath->hldev->vpath_assignments &
	    mBIT((u32) VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_MSG_SRC(wmsg)))) {
		vxge_hal_trace_log_vpath("<== %s:%s:%d Result = 0",
		    __FILE__, __func__, __LINE__);
		return;
	}

	msg_type = (u32) VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_MSG_TYPE(wmsg);

	switch (msg_type) {
	default:
	case VXGE_HAL_RTS_ACCESS_STEER_DATA0_MSG_TYPE_UNKNOWN:
		break;
	case VXGE_HAL_RTS_ACCESS_STEER_DATA0_MSG_TYPE_DEVICE_RESET_BEGIN:
		__hal_device_handle_error(hldev,
		    vpath->vp_id,
		    VXGE_HAL_EVENT_DEVICE_RESET_START);
		break;
	case VXGE_HAL_RTS_ACCESS_STEER_DATA0_MSG_TYPE_DEVICE_RESET_END:
		vpath->hldev->manager_up = TRUE;
		__hal_device_handle_error(hldev,
		    vpath->vp_id,
		    VXGE_HAL_EVENT_DEVICE_RESET_COMPLETE);
		break;
	case VXGE_HAL_RTS_ACCESS_STEER_DATA0_MSG_TYPE_VPATH_RESET_BEGIN:
		__hal_device_handle_error(hldev,
		    vpath->vp_id,
		    VXGE_HAL_EVENT_VPATH_RESET_START);
		break;
	case VXGE_HAL_RTS_ACCESS_STEER_DATA0_MSG_TYPE_VPATH_RESET_END:
		vpath->hldev->manager_up = TRUE;
		__hal_device_handle_error(hldev,
		    vpath->vp_id,
		    VXGE_HAL_EVENT_VPATH_RESET_COMPLETE);
		break;
	case VXGE_HAL_RTS_ACCESS_STEER_DATA0_MSG_TYPE_PRIV_DRIVER_UP:
		vpath->hldev->manager_up = TRUE;
		break;
	case VXGE_HAL_RTS_ACCESS_STEER_DATA0_MSG_TYPE_PRIV_DRIVER_DOWN:
		vpath->hldev->manager_up = FALSE;
		break;
	case VXGE_HAL_RTS_ACCESS_STEER_DATA0_MSG_TYPE_ACK:
		break;
	}

	vxge_hal_trace_log_vpath("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * __hal_ifmsg_device_reset_end_poll - Polls for the
 *			     srpcim to vpath reset end
 * @hldev: HAL Device
 * @vp_id: Vpath id
 *
 * Polls for the srpcim to vpath reset end
 */
vxge_hal_status_e
__hal_ifmsg_device_reset_end_poll(
    __hal_device_t *hldev,
    u32 vp_id)
{
	vxge_hal_status_e status;

	vxge_assert(hldev);

	vxge_hal_trace_log_mrpcim("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_mrpcim("hldev = 0x"VXGE_OS_STXFMT", vp_id = %d",
	    (ptr_t) hldev, vp_id);

	status = vxge_hal_device_register_poll(
	    hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->vpmgmt_reg[vp_id]->srpcim_to_vpath_wmsg, 0,
	    ~((u64) VXGE_HAL_IFMSG_DEVICE_RESET_END_MSG),
	    WAIT_FACTOR * hldev->header.config.device_poll_millis);

	vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (status);

}

/*
 * __hal_ifmsg_wmsg_post - Posts the srpcim to vpath req
 * @hldev: Hal device
 * @src_vp_id: Source vpath id
 * @dest_vp_id: Vpath id, VXGE_HAL_RTS_ACCESS_STEER_MSG_DEST_MRPCIM, or
 *	    VXGE_HAL_RTS_ACCESS_STEER_MSG_DEST_BROADCAST
 * @msg_type: wsmsg type
 * @msg_data: wsmsg data
 *
 * Posts the req
 */
vxge_hal_status_e
__hal_ifmsg_wmsg_post(
    __hal_device_t *hldev,
    u32 src_vp_id,
    u32 dest_vp_id,
    u32 msg_type,
    u32 msg_data)
{
	u64 val64;
	vxge_hal_vpath_reg_t *vp_reg;
	vxge_hal_status_e status;

	vxge_assert(hldev);

	vp_reg = hldev->vpath_reg[src_vp_id];

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_srpcim(
	    "hldev = 0x"VXGE_OS_STXFMT", src_vp_id = %d, dest_vp_id = %d, "
	    "msg_type = %d, msg_data = %d", (ptr_t) hldev, src_vp_id,
	    dest_vp_id, msg_type, msg_data);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    0,
	    &vp_reg->rts_access_steer_ctrl);

	vxge_os_wmb();


	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_IGNORE_IN_SVC_CHECK |
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_MSG_TYPE(msg_type) |
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_MSG_DEST(dest_vp_id) |
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_MSG_SRC(src_vp_id) |
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_SEQ_NUM(++hldev->ifmsg_seqno) |
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_MSG_DATA(msg_data),
	    &vp_reg->rts_access_steer_data0);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    0,
	    &vp_reg->rts_access_steer_data1);

	vxge_os_wmb();

	val64 = VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION(
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_SEND_MSG) |
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL(
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_FW_MEMO) |
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_STROBE |
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_OFFSET(0);

	vxge_hal_pio_mem_write32_lower(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) bVAL32(val64, 32),
	    &vp_reg->rts_access_steer_ctrl);

	vxge_os_wmb();

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) bVAL32(val64, 0),
	    &vp_reg->rts_access_steer_ctrl);

	vxge_os_wmb();

	status = vxge_hal_device_register_poll(hldev->header.pdev,
	    hldev->header.regh0,
	    &vp_reg->rts_access_steer_ctrl, 0,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_STROBE,
	    WAIT_FACTOR * hldev->header.config.device_poll_millis);

	if (status != VXGE_HAL_OK) {

		vxge_hal_trace_log_driver("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vp_reg->rts_access_steer_ctrl);

	if (val64 & VXGE_HAL_RTS_ACCESS_STEER_CTRL_RMACJ_STATUS) {

		vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &vp_reg->rts_access_steer_data0);

		status = VXGE_HAL_OK;

	} else {
		status = VXGE_HAL_FAIL;
	}

	vxge_hal_trace_log_srpcim("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}
