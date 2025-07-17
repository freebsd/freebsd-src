/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */
#include <linux/kernel.h>
#include "adf_accel_devices.h"
#include "adf_common_drv.h"
#include "adf_pfvf_msg.h"
#include "adf_pfvf_utils.h"
#include "adf_pfvf_vf_msg.h"
#include "adf_pfvf_vf_proto.h"

#define __bf_shf(x) (__builtin_ffsll(x) - 1)

#define FIELD_MAX(_mask) ({ (typeof(_mask))((_mask) >> __bf_shf(_mask)); })

#define FIELD_PREP(_mask, _val)                                                \
	({ ((typeof(_mask))(_val) << __bf_shf(_mask)) & (_mask); })

#define FIELD_GET(_mask, _reg)                                                 \
	({ (typeof(_mask))(((_reg) & (_mask)) >> __bf_shf(_mask)); })

/**
 * adf_send_vf2pf_msg() - send VF to PF message
 * @accel_dev:	Pointer to acceleration device
 * @msg:	Message to send
 *
 * This function allows the VF to send a message to the PF.
 *
 * Return: 0 on success, error code otherwise.
 */
int
adf_send_vf2pf_msg(struct adf_accel_dev *accel_dev, struct pfvf_message msg)
{
	struct adf_pfvf_ops *pfvf_ops = GET_PFVF_OPS(accel_dev);
	u32 pfvf_offset = pfvf_ops->get_pf2vf_offset(0);

	int ret = pfvf_ops->send_msg(accel_dev,
				     msg,
				     pfvf_offset,
				     &accel_dev->u1.vf.vf2pf_lock);
	return ret;
}

/**
 * adf_recv_pf2vf_msg() - receive a PF to VF message
 * @accel_dev:	Pointer to acceleration device
 *
 * This function allows the VF to receive a message from the PF.
 *
 * Return: a valid message on success, zero otherwise.
 */
static struct pfvf_message
adf_recv_pf2vf_msg(struct adf_accel_dev *accel_dev)
{
	struct adf_pfvf_ops *pfvf_ops = GET_PFVF_OPS(accel_dev);
	u32 pfvf_offset = pfvf_ops->get_vf2pf_offset(0); // 1008
	return pfvf_ops->recv_msg(accel_dev,
				  pfvf_offset,
				  accel_dev->u1.vf.pf_compat_ver);
}

/**
 * adf_send_vf2pf_req() - send VF2PF request message
 * @accel_dev:	Pointer to acceleration device.
 * @msg:	Request message to send
 * @resp:	Returned PF response
 *
 * This function sends a message that requires a response from the VF to the PF
 * and waits for a reply.
 *
 * Return: 0 on success, error code otherwise.
 */
int
adf_send_vf2pf_req(struct adf_accel_dev *accel_dev,
		   struct pfvf_message msg,
		   struct pfvf_message *resp)
{
	unsigned long timeout = msecs_to_jiffies(ADF_PFVF_MSG_RESP_TIMEOUT);
	unsigned int retries = ADF_PFVF_MSG_RESP_RETRIES;
	int ret;

	reinit_completion(&accel_dev->u1.vf.msg_received);
	/* Send request from VF to PF */
	do {
		ret = adf_send_vf2pf_msg(accel_dev, msg);
		if (ret) {
			device_printf(GET_DEV(accel_dev),
				      "Failed to send request msg to PF\n");
			return ret;
		}

		/* Wait for response, if it times out retry */
		if (!cold) {
			ret = wait_for_completion_timeout(
			    &accel_dev->u1.vf.msg_received, timeout);
		} else {
			/* In cold start timers may not be initialized yet */
			DELAY(ADF_PFVF_MSG_RESP_TIMEOUT * 1000);
			ret = try_wait_for_completion(
			    &accel_dev->u1.vf.msg_received);
		}
		if (ret) {
			if (likely(resp))
				*resp = accel_dev->u1.vf.response;

			/* Once copied, set to an invalid value */
			accel_dev->u1.vf.response.type = 0;

			return 0;
		}

		device_printf(GET_DEV(accel_dev),
			      "PFVF response message timeout\n");
	} while (--retries);

	return -EIO;
}

static int
adf_vf2pf_blkmsg_data_req(struct adf_accel_dev *accel_dev,
			  bool crc,
			  u8 *type,
			  u8 *data)
{
	struct pfvf_message req = { 0 };
	struct pfvf_message resp = { 0 };
	u8 blk_type;
	u8 blk_byte;
	u8 msg_type;
	u8 max_data;
	int err;

	/* Convert the block type to {small, medium, large} size category */
	if (*type <= ADF_VF2PF_SMALL_BLOCK_TYPE_MAX) {
		msg_type = ADF_VF2PF_MSGTYPE_SMALL_BLOCK_REQ;
		blk_type = FIELD_PREP(ADF_VF2PF_SMALL_BLOCK_TYPE_MASK, *type);
		blk_byte = FIELD_PREP(ADF_VF2PF_SMALL_BLOCK_BYTE_MASK, *data);
		max_data = ADF_VF2PF_SMALL_BLOCK_BYTE_MAX;
	} else if (*type <= ADF_VF2PF_MEDIUM_BLOCK_TYPE_MAX) {
		msg_type = ADF_VF2PF_MSGTYPE_MEDIUM_BLOCK_REQ;
		blk_type = FIELD_PREP(ADF_VF2PF_MEDIUM_BLOCK_TYPE_MASK,
				      *type - ADF_VF2PF_SMALL_BLOCK_TYPE_MAX);
		blk_byte = FIELD_PREP(ADF_VF2PF_MEDIUM_BLOCK_BYTE_MASK, *data);
		max_data = ADF_VF2PF_MEDIUM_BLOCK_BYTE_MAX;
	} else if (*type <= ADF_VF2PF_LARGE_BLOCK_TYPE_MAX) {
		msg_type = ADF_VF2PF_MSGTYPE_LARGE_BLOCK_REQ;
		blk_type = FIELD_PREP(ADF_VF2PF_LARGE_BLOCK_TYPE_MASK,
				      *type - ADF_VF2PF_MEDIUM_BLOCK_TYPE_MAX);
		blk_byte = FIELD_PREP(ADF_VF2PF_LARGE_BLOCK_BYTE_MASK, *data);
		max_data = ADF_VF2PF_LARGE_BLOCK_BYTE_MAX;
	} else {
		device_printf(GET_DEV(accel_dev),
			      "Invalid message type %u\n",
			      *type);
		return -EINVAL;
	}

	/* Sanity check */
	if (*data > max_data) {
		device_printf(GET_DEV(accel_dev),
			      "Invalid byte %s %u for message type %u\n",
			      crc ? "count" : "index",
			      *data,
			      *type);
		return -EINVAL;
	}

	/* Build the block message */
	req.type = msg_type;
	req.data =
	    blk_type | blk_byte | FIELD_PREP(ADF_VF2PF_BLOCK_CRC_REQ_MASK, crc);

	err = adf_send_vf2pf_req(accel_dev, req, &resp);
	if (err)
		return err;

	*type = FIELD_GET(ADF_PF2VF_BLKMSG_RESP_TYPE_MASK, resp.data);
	*data = FIELD_GET(ADF_PF2VF_BLKMSG_RESP_DATA_MASK, resp.data);

	return 0;
}

static int
adf_vf2pf_blkmsg_get_byte(struct adf_accel_dev *accel_dev,
			  u8 type,
			  u8 index,
			  u8 *data)
{
	int ret;

	ret = adf_vf2pf_blkmsg_data_req(accel_dev, false, &type, &index);
	if (ret < 0)
		return ret;

	if (unlikely(type != ADF_PF2VF_BLKMSG_RESP_TYPE_DATA)) {
		device_printf(GET_DEV(accel_dev),
			      "Unexpected BLKMSG response type %u, byte 0x%x\n",
			      type,
			      index);
		return -EFAULT;
	}

	*data = index;
	return 0;
}

static int
adf_vf2pf_blkmsg_get_crc(struct adf_accel_dev *accel_dev,
			 u8 type,
			 u8 bytes,
			 u8 *crc)
{
	int ret;

	/* The count of bytes refers to a length, however shift it to a 0-based
	 * count to avoid overflows. Thus, a request for 0 bytes is technically
	 * valid.
	 */
	--bytes;

	ret = adf_vf2pf_blkmsg_data_req(accel_dev, true, &type, &bytes);
	if (ret < 0)
		return ret;

	if (unlikely(type != ADF_PF2VF_BLKMSG_RESP_TYPE_CRC)) {
		device_printf(
		    GET_DEV(accel_dev),
		    "Unexpected CRC BLKMSG response type %u, crc 0x%x\n",
		    type,
		    bytes);
		return -EFAULT;
	}

	*crc = bytes;
	return 0;
}

/**
 * adf_send_vf2pf_blkmsg_req() - retrieve block message
 * @accel_dev:	Pointer to acceleration VF device.
 * @type:	The block message type, see adf_pfvf_msg.h for allowed values
 * @buffer:	input buffer where to place the received data
 * @buffer_len:	buffer length as input, the amount of written bytes on output
 *
 * Request a message of type 'type' over the block message transport.
 * This function will send the required amount block message requests and
 * return the overall content back to the caller through the provided buffer.
 * The buffer should be large enough to contain the requested message type,
 * otherwise the response will be truncated.
 *
 * Return: 0 on success, error code otherwise.
 */
int
adf_send_vf2pf_blkmsg_req(struct adf_accel_dev *accel_dev,
			  u8 type,
			  u8 *buffer,
			  unsigned int *buffer_len)
{
	unsigned int index;
	unsigned int msg_len;
	int ret;
	u8 remote_crc;
	u8 local_crc;

	if (unlikely(type > ADF_VF2PF_LARGE_BLOCK_TYPE_MAX)) {
		device_printf(GET_DEV(accel_dev),
			      "Invalid block message type %d\n",
			      type);
		return -EINVAL;
	}

	if (unlikely(*buffer_len < ADF_PFVF_BLKMSG_HEADER_SIZE)) {
		device_printf(GET_DEV(accel_dev),
			      "Buffer size too small for a block message\n");
		return -EINVAL;
	}

	ret = adf_vf2pf_blkmsg_get_byte(accel_dev,
					type,
					ADF_PFVF_BLKMSG_VER_BYTE,
					&buffer[ADF_PFVF_BLKMSG_VER_BYTE]);
	if (unlikely(ret))
		return ret;

	if (unlikely(!buffer[ADF_PFVF_BLKMSG_VER_BYTE])) {
		device_printf(GET_DEV(accel_dev),
			      "Invalid version 0 received for block request %u",
			      type);
		return -EFAULT;
	}

	ret = adf_vf2pf_blkmsg_get_byte(accel_dev,
					type,
					ADF_PFVF_BLKMSG_LEN_BYTE,
					&buffer[ADF_PFVF_BLKMSG_LEN_BYTE]);
	if (unlikely(ret))
		return ret;

	if (unlikely(!buffer[ADF_PFVF_BLKMSG_LEN_BYTE])) {
		device_printf(GET_DEV(accel_dev),
			      "Invalid size 0 received for block request %u",
			      type);
		return -EFAULT;
	}

	/* We need to pick the minimum since there is no way to request a
	 * specific version. As a consequence any scenario is possible:
	 * - PF has a newer (longer) version which doesn't fit in the buffer
	 * - VF expects a newer (longer) version, so we must not ask for
	 *   bytes in excess
	 * - PF and VF share the same version, no problem
	 */
	msg_len =
	    ADF_PFVF_BLKMSG_HEADER_SIZE + buffer[ADF_PFVF_BLKMSG_LEN_BYTE];
	msg_len = min(*buffer_len, msg_len);

	/* Get the payload */
	for (index = ADF_PFVF_BLKMSG_HEADER_SIZE; index < msg_len; index++) {
		ret = adf_vf2pf_blkmsg_get_byte(accel_dev,
						type,
						index,
						&buffer[index]);
		if (unlikely(ret))
			return ret;
	}

	ret = adf_vf2pf_blkmsg_get_crc(accel_dev, type, msg_len, &remote_crc);
	if (unlikely(ret))
		return ret;

	local_crc = adf_pfvf_calc_blkmsg_crc(buffer, msg_len);
	if (unlikely(local_crc != remote_crc)) {
		device_printf(
		    GET_DEV(accel_dev),
		    "CRC error on msg type %d. Local %02X, remote %02X\n",
		    type,
		    local_crc,
		    remote_crc);
		return -EIO;
	}

	*buffer_len = msg_len;
	return 0;
}

static bool
adf_handle_pf2vf_msg(struct adf_accel_dev *accel_dev, struct pfvf_message msg)
{
	switch (msg.type) {
	case ADF_PF2VF_MSGTYPE_RESTARTING:
		adf_pf2vf_handle_pf_restarting(accel_dev);
		return false;
	case ADF_PF2VF_MSGTYPE_RP_RESET_RESP:
		adf_pf2vf_handle_pf_rp_reset(accel_dev, msg);
		return true;
	case ADF_PF2VF_MSGTYPE_FATAL_ERROR:
		adf_pf2vf_handle_pf_error(accel_dev);
		return true;
	case ADF_PF2VF_MSGTYPE_VERSION_RESP:
	case ADF_PF2VF_MSGTYPE_BLKMSG_RESP:
		accel_dev->u1.vf.response = msg;
		complete(&accel_dev->u1.vf.msg_received);
		return true;
	default:
		device_printf(
		    GET_DEV(accel_dev),
		    "Unknown message from PF (type 0x%.4x, data: 0x%.4x)\n",
		    msg.type,
		    msg.data);
	}

	return false;
}

bool
adf_recv_and_handle_pf2vf_msg(struct adf_accel_dev *accel_dev)
{
	struct pfvf_message msg;

	msg = adf_recv_pf2vf_msg(accel_dev);
	if (msg.type) /* Invalid or no message */
		return adf_handle_pf2vf_msg(accel_dev, msg);

	/* No replies for PF->VF messages at present */

	return true;
}

/**
 * adf_enable_vf2pf_comms() - Function enables communication from vf to pf
 *
 * @accel_dev:	Pointer to acceleration device virtual function.
 *
 * Return: 0 on success, error code otherwise.
 */
int
adf_enable_vf2pf_comms(struct adf_accel_dev *accel_dev)
{
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	int ret;

	/* init workqueue for VF */
	ret = adf_init_vf_wq();
	if (ret)
		return ret;

	hw_data->enable_pf2vf_interrupt(accel_dev);

	ret = adf_vf2pf_request_version(accel_dev);
	if (ret)
		return ret;

	ret = adf_vf2pf_get_capabilities(accel_dev);
	if (ret)
		return ret;

	ret = adf_vf2pf_get_ring_to_svc(accel_dev);
	return ret;
}
