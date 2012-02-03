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

#ifndef	VXGE_HAL_DOOR_BELLS_H
#define	VXGE_HAL_DOOR_BELLS_H

__EXTERN_BEGIN_DECLS

/*
 * struct __hal_non_offload_db_wrapper_t - Non-offload Doorbell Wrapper
 * @control_0:	Bits 0 to 7 - Doorbell type.
 *		Bits 8 to 31 - Reserved.
 *		Bits 32 to 39 - The highest TxD in this TxDL.
 *		Bits 40 to 47 - Reserved.
 *		Bits 48 to 55 - Reserved.
 *		Bits 56 to 63 - No snoop flags.
 * @txdl_ptr:	The starting location of the TxDL in host memory.
 *
 * Created by the host and written to the adapter via PIO to a Kernel Doorbell
 * FIFO. All non-offload doorbell wrapper fields must be written by the host as
 * part of a doorbell write. Consumed by the adapter but is not written by the
 * adapter.
 */
typedef __vxge_os_attr_cacheline_aligned struct __hal_non_offload_db_wrapper_t {
	u64		control_0;
#define	VXGE_HAL_NODBW_GET_TYPE(ctrl0)				bVAL8(ctrl0, 0)
#define	VXGE_HAL_NODBW_TYPE(val)				vBIT(val, 0, 8)
#define	VXGE_HAL_NODBW_TYPE_NODBW				0

#define	VXGE_HAL_NODBW_GET_LAST_TXD_NUMBER(ctrl0)		bVAL8(ctrl0, 32)
#define	VXGE_HAL_NODBW_LAST_TXD_NUMBER(val)			vBIT(val, 32, 8)

#define	VXGE_HAL_NODBW_GET_NO_SNOOP(ctrl0)			bVAL8(ctrl0, 56)
#define	VXGE_HAL_NODBW_LIST_NO_SNOOP(val)			vBIT(val, 56, 8)
#define	VXGE_HAL_NODBW_LIST_NO_SNOOP_TXD_READ_TXD0_WRITE	0x2
#define	VXGE_HAL_NODBW_LIST_NO_SNOOP_TX_FRAME_DATA_READ		0x1

	u64		txdl_ptr;
} __hal_non_offload_db_wrapper_t;

/*
 * struct __hal_offload_db_wrapper_t - Tx-Offload Doorbell Wrapper
 * @control_0:	Bits 0 to 7 - Doorbell type.
 *		Bits 8 to 31 - Identifies the session to which this Tx
 *		offload doorbell applies.
 *		Bits 32 to 40 - Identifies the incarnation of this Session
 *		Number. The adapter assigns a Session Instance
 *		Number of 0 to a session when that Session Number
 *		is first used. Each subsequent assignment of that
 *		Session Number from the free pool causes this
 *		number to be incremented, with wrap eventually
 *		occurring from 255 back to 0.
 *		Bits 40 to 63 - Identifies the end of the TOWI list for
 *		this session to the adapter.
 * @control_1:	Bits 0 to 7 - Identifies what is included in this doorbell
 *		Bits 8 to 15 - The number of Immediate data bytes included in
 *		this doorbell.
 *		Bits 16 to 63 - Reserved.
 *
 * Created by the host and written to the adapter via PIO to a Kernel Doorbell
 * FIFO. All Tx Offload doorbell wrapper fields must be written by the host as
 * part of a doorbell write. Consumed by the adapter but is never written by the
 * adapter.
 */
typedef __vxge_os_attr_cacheline_aligned struct __hal_offload_db_wrapper_t {
	u64		control_0;
#define	VXGE_HAL_ODBW_GET_TYPE(ctrl0)			bVAL8(ctrl0, 0)
#define	VXGE_HAL_ODBW_TYPE(val)				vBIT(val, 0, 8)
#define	VXGE_HAL_ODBW_TYPE_ODBW				1

#define	VXGE_HAL_ODBW_GET_SESSION_NUMBER(ctrl0)		bVAL24(ctrl0, 8)
#define	VXGE_HAL_ODBW_SESSION_NUMBER(val)		vBIT(val, 8, 24)

#define	VXGE_HAL_ODBW_GET_SESSION_INST_NUMBER(ctrl0)	bVAL8(ctrl0, 32)
#define	VXGE_HAL_ODBW_SESSION_INST_NUMBER(val)		vBIT(val, 32, 8)

#define	VXGE_HAL_ODBW_GET_HIGH_TOWI_NUMBER(ctrl0)	bVAL24(ctrl0, 40)
#define	VXGE_HAL_ODBW_HIGH_TOWI_NUMBER(val)		vBIT(val, 40, 24)

	u64		control_1;
#define	VXGE_HAL_ODBW_GET_ENTRY_TYPE(ctrl1)		bVAL8(ctrl1, 0)
#define	VXGE_HAL_ODBW_ENTRY_TYPE(val)			vBIT(val, 0, 8)
#define	VXGE_HAL_ODBW_ENTRY_TYPE_WRAPPER_ONLY		0x0
#define	VXGE_HAL_ODBW_ENTRY_TYPE_WRAPPER_TOWI		0x1
#define	VXGE_HAL_ODBW_ENTRY_TYPE_WRAPPER_TOWI_DATA	0x2

#define	VXGE_HAL_ODBW_GET_IMMEDIATE_BYTE_COUNT(ctrl1)	bVAL8(ctrl1, 8)
#define	VXGE_HAL_ODBW_IMMEDIATE_BYTE_COUNT(val)		vBIT(val, 8, 8)

} __hal_offload_db_wrapper_t;

/*
 * struct __hal_offload_atomic_db_wrapper_t - Atomic Tx-Offload Doorbell
 *						 Wrapper
 * @control_0:	Bits 0 to 7 - Doorbell type.
 *		Bits 8 to 31 - Identifies the session to which this Tx
 *		offload doorbell applies.
 *		Bits 32 to 40 - Identifies the incarnation of this Session
 *		Number. The adapter assigns a Session Instance
 *		Number of 0 to a session when that Session Number
 *		is first used. Each subsequent assignment of that
 *		Session Number from the free pool causes this
 *		number to be incremented, with wrap eventually
 *		occurring from 255 back to 0.
 *		Bits 40 to 63 - Identifies the end of the TOWI list for
 *		this session to the adapter.
 *
 * Created by the host and written to the adapter via PIO to a Kernel Doorbell
 * FIFO.  All Tx Offload doorbell wrapper fields must be written by the host as
 * part of a doorbell write. Consumed by the adapter but is never written by the
 * adapter.
 */
typedef	__vxge_os_attr_cacheline_aligned
struct __hal_offload_atomic_db_wrapper_t {
	u64		control_0;
#define	VXGE_HAL_ODBW_GET_TYPE(ctrl0)			bVAL8(ctrl0, 0)
#define	VXGE_HAL_ODBW_TYPE(val)				vBIT(val, 0, 8)
#define	VXGE_HAL_ODBW_TYPE_ATOMIC			2

#define	VXGE_HAL_ODBW_GET_SESSION_NUMBER(ctrl0)		bVAL24(ctrl0, 8)
#define	VXGE_HAL_ODBW_SESSION_NUMBER(val)		vBIT(val, 8, 24)

#define	VXGE_HAL_ODBW_GET_SESSION_INST_NUMBER(ctrl0)	bVAL8(ctrl0, 32)
#define	VXGE_HAL_ODBW_SESSION_INST_NUMBER(val)		vBIT(val, 32, 8)

#define	VXGE_HAL_ODBW_GET_HIGH_TOWI_NUMBER(ctrl0)	bVAL24(ctrl0, 40)
#define	VXGE_HAL_ODBW_HIGH_TOWI_NUMBER(val)		vBIT(val, 40, 24)

} __hal_offload_atomic_db_wrapper_t;



/*
 * struct __hal_messaging_db_wrapper_t - Messaging Doorbell Wrapper
 * @control_0:	Bits 0 to 7 - Doorbell type.
 *		Bits 8 to 31 - Reserved.
 *		Bits 32 to 63 - The number of new message bytes made available
 *		by this doorbell entry.
 * @control_1:	Bits 0 to 7 - Reserved.
 *		Bits 8 to 15 - The number of Immediate messaging bytes included
 *		in this doorbell.
 *		Bits 16 to 63 - Reserved.
 *
 * Created by the host and written to the adapter via PIO to a Kernel Doorbell
 * FIFO. All message doorbell wrapper fields must be written by the host as
 * part of a doorbell write. Consumed by the adapter but not written by adapter.
 */
typedef __vxge_os_attr_cacheline_aligned struct __hal_messaging_db_wrapper_t {
	u64		control_0;
#define	VXGE_HAL_MDBW_GET_TYPE(ctrl0)			bVAL8(ctrl0, 0)
#define	VXGE_HAL_MDBW_TYPE(val)				vBIT(val, 0, 8)
#define	VXGE_HAL_MDBW_TYPE_MDBW				3

#define	VXGE_HAL_MDBW_GET_MESSAGE_BYTE_COUNT(ctrl0)	bVAL32(ctrl0, 32)
#define	VXGE_HAL_MDBW_MESSAGE_BYTE_COUNT(val)		vBIT(val, 32, 32)

	u64		control_1;
#define	VXGE_HAL_MDBW_GET_IMMEDIATE_BYTE_COUNT(ctrl1)	bVAL8(ctrl1, 8)
#define	VXGE_HAL_MDBW_IMMEDIATE_BYTE_COUNT(val)		vBIT(val, 8, 8)

} __hal_messaging_db_wrapper_t;


void
__hal_non_offload_db_post(vxge_hal_vpath_h vpath_handle,
    u64 txdl_ptr,
    u32 num_txds,
    u32 no_snoop);

void
__hal_rxd_db_post(vxge_hal_vpath_h vpath_handle,
    u32 num_bytes);

vxge_hal_status_e
__hal_non_offload_db_reset(vxge_hal_vpath_h vpath_handle);


void
__hal_message_db_post(vxge_hal_vpath_h vpath_handle,
    u32 num_msg_bytes,
    u8 *immed_msg,
    u32 immed_msg_len);

vxge_hal_status_e
__hal_message_db_reset(vxge_hal_vpath_h vpath_handle);

__EXTERN_END_DECLS

#endif	/* VXGE_HAL_DOOR_BELLS_H */
