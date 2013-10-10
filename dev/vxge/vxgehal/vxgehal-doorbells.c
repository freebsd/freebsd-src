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
 * __hal_non_offload_db_post - Post non offload doorbell
 *
 * @vpath_handle: vpath handle
 * @txdl_ptr: The starting location of the TxDL in host memory
 * @num_txds: The highest TxD in this TxDL (0 to 255 means 1 to 256)
 * @no_snoop: No snoop flags
 *
 * This function posts a non-offload doorbell to doorbell FIFO
 *
 */
void
__hal_non_offload_db_post(vxge_hal_vpath_h vpath_handle,
    u64 txdl_ptr,
    u32 num_txds,
    u32 no_snoop)
{
	u64 *db_ptr;
	__hal_device_t *hldev;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert((vpath_handle != NULL) && (txdl_ptr != 0));

	hldev = (__hal_device_t *) vp->vpath->hldev;

	vxge_hal_trace_log_fifo("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_fifo(
	    "vpath_handle = 0x"VXGE_OS_STXFMT", txdl_ptr = 0x"VXGE_OS_STXFMT
	    ", num_txds = %d, no_snoop = %d", (ptr_t) vpath_handle,
	    (ptr_t) txdl_ptr, num_txds, no_snoop);

	db_ptr = &vp->vpath->nofl_db->control_0;

	vxge_os_pio_mem_write64(vp->vpath->hldev->header.pdev,
	    vp->vpath->hldev->header.regh0,
	    VXGE_HAL_NODBW_TYPE(VXGE_HAL_NODBW_TYPE_NODBW) |
	    VXGE_HAL_NODBW_LAST_TXD_NUMBER(num_txds) |
	    VXGE_HAL_NODBW_GET_NO_SNOOP(no_snoop),
	    db_ptr++);

	vxge_os_pio_mem_write64(vp->vpath->hldev->header.pdev,
	    vp->vpath->hldev->header.regh0,
	    txdl_ptr,
	    db_ptr);

	vxge_hal_trace_log_fifo("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * __hal_non_offload_db_reset - Reset non offload doorbell fifo
 *
 * @vpath_handle: vpath handle
 *
 * This function resets non-offload doorbell FIFO
 *
 */
vxge_hal_status_e
__hal_non_offload_db_reset(vxge_hal_vpath_h vpath_handle)
{
	vxge_hal_status_e status;
	__hal_device_t *hldev;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert(vpath_handle != NULL);

	hldev = (__hal_device_t *) vp->vpath->hldev;

	vxge_hal_trace_log_fifo("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_fifo(
	    "vpath_handle = 0x"VXGE_OS_STXFMT, (ptr_t) vpath_handle);

	vxge_os_pio_mem_write64(vp->vpath->hldev->header.pdev,
	    vp->vpath->hldev->header.regh0,
	    VXGE_HAL_CMN_RSTHDLR_CFG2_SW_RESET_FIFO0(
	    1 << (16 - vp->vpath->vp_id)),
	    &vp->vpath->hldev->common_reg->cmn_rsthdlr_cfg2);

	vxge_os_wmb();

	status = vxge_hal_device_register_poll(vp->vpath->hldev->header.pdev,
	    vp->vpath->hldev->header.regh0,
	    &vp->vpath->hldev->common_reg->cmn_rsthdlr_cfg2, 0,
	    (u64) VXGE_HAL_CMN_RSTHDLR_CFG2_SW_RESET_FIFO0(
	    1 << (16 - vp->vpath->vp_id)),
	    VXGE_HAL_DEF_DEVICE_POLL_MILLIS);

	vxge_hal_trace_log_fifo("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);

	return (status);
}

/*
 * __hal_rxd_db_post - Post rxd doorbell
 *
 * @vpath_handle: vpath handle
 * @num_bytes: The number of bytes
 *
 * This function posts a rxd doorbell
 *
 */
void
__hal_rxd_db_post(vxge_hal_vpath_h vpath_handle,
    u32 num_bytes)
{
	__hal_device_t *hldev;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert(vpath_handle != NULL);

	hldev = (__hal_device_t *) vp->vpath->hldev;

	vxge_hal_trace_log_ring("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_fifo(
	    "vpath_handle = 0x"VXGE_OS_STXFMT", num_bytes = %d",
	    (ptr_t) vpath_handle, num_bytes);

	vxge_os_pio_mem_write64(vp->vpath->hldev->header.pdev,
	    vp->vpath->hldev->header.regh0,
	    VXGE_HAL_PRC_RXD_DOORBELL_NEW_QW_CNT((num_bytes >> 3)),
	    &vp->vpath->vp_reg->prc_rxd_doorbell);

	vxge_hal_trace_log_ring("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
}


/*
 * __hal_message_db_post - Post message doorbell
 *
 * @vpath_handle: VPATH handle
 * @num_msg_bytes: The number of new message bytes made available
 *		by this doorbell entry.
 * @immed_msg: Immediate message to be sent
 * @immed_msg_len: Immediate message length
 *
 * This function posts a message doorbell to doorbell FIFO
 *
 */
void
__hal_message_db_post(vxge_hal_vpath_h vpath_handle,
    u32 num_msg_bytes,
    u8 *immed_msg,
    u32 immed_msg_len)
{
	u32 i;
	u64 *db_ptr;
	__hal_device_t *hldev;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert((vpath_handle != NULL) && (num_msg_bytes != 0));

	hldev = (__hal_device_t *) vp->vpath->hldev;

	vxge_hal_trace_log_dmq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_dmq("vpath_handle = 0x"VXGE_OS_STXFMT", "
	    "num_msg_bytes = %d, immed_msg = 0x"VXGE_OS_STXFMT", "
	    "immed_msg_len = %d", (ptr_t) vpath_handle, num_msg_bytes,
	    (ptr_t) immed_msg, immed_msg_len);

	db_ptr = &vp->vpath->msg_db->control_0;

	vxge_os_pio_mem_write64(vp->vpath->hldev->header.pdev,
	    vp->vpath->hldev->header.regh0,
	    VXGE_HAL_MDBW_TYPE(VXGE_HAL_MDBW_TYPE_MDBW) |
	    VXGE_HAL_MDBW_MESSAGE_BYTE_COUNT(num_msg_bytes),
	    db_ptr++);

	vxge_os_pio_mem_write64(vp->vpath->hldev->header.pdev,
	    vp->vpath->hldev->header.regh0,
	    VXGE_HAL_MDBW_IMMEDIATE_BYTE_COUNT(immed_msg_len),
	    db_ptr++);

	for (i = 0; i < immed_msg_len / 8; i++) {
		vxge_os_pio_mem_write64(vp->vpath->hldev->header.pdev,
		    vp->vpath->hldev->header.regh0,
		    *((u64 *) ((void *)&immed_msg[i * 8])),
		    db_ptr++);
	}

	vxge_hal_trace_log_dmq("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * __hal_message_db_reset - Reset message doorbell fifo
 *
 * @vpath_handle: vpath handle
 *
 * This function resets message doorbell FIFO
 *
 */
vxge_hal_status_e
__hal_message_db_reset(vxge_hal_vpath_h vpath_handle)
{
	vxge_hal_status_e status;
	__hal_device_t *hldev;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert(vpath_handle != NULL);

	hldev = (__hal_device_t *) vp->vpath->hldev;

	vxge_hal_trace_log_dmq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_dmq("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	vxge_os_pio_mem_write64(vp->vpath->hldev->header.pdev,
	    vp->vpath->hldev->header.regh0,
	    VXGE_HAL_CMN_RSTHDLR_CFG3_SW_RESET_FIFO1(
	    1 << (16 - vp->vpath->vp_id)),
	    &vp->vpath->hldev->common_reg->cmn_rsthdlr_cfg3);

	vxge_os_wmb();

	status = vxge_hal_device_register_poll(vp->vpath->hldev->header.pdev,
	    vp->vpath->hldev->header.regh0,
	    &vp->vpath->hldev->common_reg->cmn_rsthdlr_cfg3, 0,
	    (u64) VXGE_HAL_CMN_RSTHDLR_CFG3_SW_RESET_FIFO1(
	    1 << (16 - vp->vpath->vp_id)),
	    VXGE_HAL_DEF_DEVICE_POLL_MILLIS);

	vxge_hal_trace_log_dmq("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);

	return (status);
}
