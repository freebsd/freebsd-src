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

#ifndef	VXGE_HAL_IFMSG_H
#define	VXGE_HAL_IFMSG_H

__EXTERN_BEGIN_DECLS


#define	VXGE_HAL_IFMSG_PRIV_DRIVER_UP_MSG				\
	VXGE_HAL_RTS_ACCESS_STEER_DATA0_MSG_TYPE(VXGE_HAL_IFMSG_MSV_OPCODE_UP)

#define	VXGE_HAL_IFMSG_PRIV_DRIVER_DOWN_MSG				\
	VXGE_HAL_RTS_ACCESS_STEER_DATA0_MSG_TYPE(VXGE_HAL_IFMSG_MSV_OPCODE_DOWN)

#define	VXGE_HAL_IFMSG_DEVICE_RESET_BEGIN_MSG				\
	VXGE_HAL_RTS_ACCESS_STEER_DATA0_MSG_TYPE(			\
	VXGE_HAL_RTS_ACCESS_STEER_DATA0_MSG_TYPE_DEVICE_RESET_BEGIN)

#define	VXGE_HAL_IFMSG_DEVICE_RESET_END_MSG				\
	VXGE_HAL_RTS_ACCESS_STEER_DATA0_MSG_TYPE(			\
	VXGE_HAL_RTS_ACCESS_STEER_DATA0_MSG_TYPE_DEVICE_RESET_END)

static inline u32
/* LINTED */
__hal_ifmsg_is_manager_up(u64 wmsg)
{
	return (((u32) VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_MSG_TYPE(wmsg) ==
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_MSG_TYPE_PRIV_DRIVER_UP) ||
	    ((u32) VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_MSG_TYPE(wmsg) ==
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_MSG_TYPE_DEVICE_RESET_END));
}

vxge_hal_status_e
__hal_ifmsg_device_reset_end_poll(
    __hal_device_t *hldev,
    u32 vp_id);

void
__hal_ifmsg_wmsg_process(
    __hal_virtualpath_t *vpath,
    u64 wmsg);

vxge_hal_status_e
__hal_ifmsg_wmsg_post(
    __hal_device_t *hldev,
    u32 src_vp_id,
    u32 dest_vp_id,
    u32 msg_type,
    u32 msg_data);

__EXTERN_END_DECLS

#endif	/* VXGE_HAL_IFMSG_H */
