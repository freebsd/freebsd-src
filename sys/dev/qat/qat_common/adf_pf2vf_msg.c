/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
#include <linux/delay.h>
#include "adf_accel_devices.h"
#include "adf_common_drv.h"
#include "adf_pf2vf_msg.h"

adf_iov_block_provider
    pf2vf_message_providers[ADF_VF2PF_MAX_LARGE_MESSAGE_TYPE + 1];
unsigned char pfvf_crc8_table[] =
    { 0x00, 0x97, 0xB9, 0x2E, 0xE5, 0x72, 0x5C, 0xCB, 0x5D, 0xCA, 0xE4, 0x73,
      0xB8, 0x2F, 0x01, 0x96, 0xBA, 0x2D, 0x03, 0x94, 0x5F, 0xC8, 0xE6, 0x71,
      0xE7, 0x70, 0x5E, 0xC9, 0x02, 0x95, 0xBB, 0x2C, 0xE3, 0x74, 0x5A, 0xCD,
      0x06, 0x91, 0xBF, 0x28, 0xBE, 0x29, 0x07, 0x90, 0x5B, 0xCC, 0xE2, 0x75,
      0x59, 0xCE, 0xE0, 0x77, 0xBC, 0x2B, 0x05, 0x92, 0x04, 0x93, 0xBD, 0x2A,
      0xE1, 0x76, 0x58, 0xCF, 0x51, 0xC6, 0xE8, 0x7F, 0xB4, 0x23, 0x0D, 0x9A,
      0x0C, 0x9B, 0xB5, 0x22, 0xE9, 0x7E, 0x50, 0xC7, 0xEB, 0x7C, 0x52, 0xC5,
      0x0E, 0x99, 0xB7, 0x20, 0xB6, 0x21, 0x0F, 0x98, 0x53, 0xC4, 0xEA, 0x7D,
      0xB2, 0x25, 0x0B, 0x9C, 0x57, 0xC0, 0xEE, 0x79, 0xEF, 0x78, 0x56, 0xC1,
      0x0A, 0x9D, 0xB3, 0x24, 0x08, 0x9F, 0xB1, 0x26, 0xED, 0x7A, 0x54, 0xC3,
      0x55, 0xC2, 0xEC, 0x7B, 0xB0, 0x27, 0x09, 0x9E, 0xA2, 0x35, 0x1B, 0x8C,
      0x47, 0xD0, 0xFE, 0x69, 0xFF, 0x68, 0x46, 0xD1, 0x1A, 0x8D, 0xA3, 0x34,
      0x18, 0x8F, 0xA1, 0x36, 0xFD, 0x6A, 0x44, 0xD3, 0x45, 0xD2, 0xFC, 0x6B,
      0xA0, 0x37, 0x19, 0x8E, 0x41, 0xD6, 0xF8, 0x6F, 0xA4, 0x33, 0x1D, 0x8A,
      0x1C, 0x8B, 0xA5, 0x32, 0xF9, 0x6E, 0x40, 0xD7, 0xFB, 0x6C, 0x42, 0xD5,
      0x1E, 0x89, 0xA7, 0x30, 0xA6, 0x31, 0x1F, 0x88, 0x43, 0xD4, 0xFA, 0x6D,
      0xF3, 0x64, 0x4A, 0xDD, 0x16, 0x81, 0xAF, 0x38, 0xAE, 0x39, 0x17, 0x80,
      0x4B, 0xDC, 0xF2, 0x65, 0x49, 0xDE, 0xF0, 0x67, 0xAC, 0x3B, 0x15, 0x82,
      0x14, 0x83, 0xAD, 0x3A, 0xF1, 0x66, 0x48, 0xDF, 0x10, 0x87, 0xA9, 0x3E,
      0xF5, 0x62, 0x4C, 0xDB, 0x4D, 0xDA, 0xF4, 0x63, 0xA8, 0x3F, 0x11, 0x86,
      0xAA, 0x3D, 0x13, 0x84, 0x4F, 0xD8, 0xF6, 0x61, 0xF7, 0x60, 0x4E, 0xD9,
      0x12, 0x85, 0xAB, 0x3C };

void
adf_enable_pf2vf_interrupts(struct adf_accel_dev *accel_dev)
{
	struct adf_accel_pci *pci_info = &accel_dev->accel_pci_dev;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	struct resource *pmisc_bar_addr =
	    pci_info->pci_bars[hw_data->get_misc_bar_id(hw_data)].virt_addr;

	ADF_CSR_WR(pmisc_bar_addr, hw_data->get_vintmsk_offset(0), 0x0);
}

void
adf_disable_pf2vf_interrupts(struct adf_accel_dev *accel_dev)
{
	struct adf_accel_pci *pci_info = &accel_dev->accel_pci_dev;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	struct resource *pmisc_bar_addr =
	    pci_info->pci_bars[hw_data->get_misc_bar_id(hw_data)].virt_addr;

	ADF_CSR_WR(pmisc_bar_addr, hw_data->get_vintmsk_offset(0), 0x2);
}

static int
__adf_iov_putmsg(struct adf_accel_dev *accel_dev,
		 u32 msg,
		 u8 vf_nr,
		 bool is_notification)
{
	struct adf_accel_pci *pci_info = &accel_dev->accel_pci_dev;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	struct resource *pmisc_bar_addr =
	    pci_info->pci_bars[hw_data->get_misc_bar_id(hw_data)].virt_addr;
	u32 val, pf2vf_offset;
	u32 total_delay = 0, mdelay = ADF_IOV_MSG_ACK_DELAY_MS,
	    udelay = ADF_IOV_MSG_ACK_DELAY_US;
	u32 local_in_use_mask, local_in_use_pattern;
	u32 remote_in_use_mask, remote_in_use_pattern;
	struct mutex *lock; /* lock preventing concurrent acces of CSR */
	u32 int_bit;
	int ret = 0;
	struct pfvf_stats *pfvf_counters = NULL;

	if (accel_dev->is_vf) {
		pf2vf_offset = hw_data->get_pf2vf_offset(0);
		lock = &accel_dev->u1.vf.vf2pf_lock;
		local_in_use_mask = ADF_VF2PF_IN_USE_BY_VF_MASK;
		local_in_use_pattern = ADF_VF2PF_IN_USE_BY_VF;
		remote_in_use_mask = ADF_PF2VF_IN_USE_BY_PF_MASK;
		remote_in_use_pattern = ADF_PF2VF_IN_USE_BY_PF;
		int_bit = ADF_VF2PF_INT;
		pfvf_counters = &accel_dev->u1.vf.pfvf_counters;
	} else {
		pf2vf_offset = hw_data->get_pf2vf_offset(vf_nr);
		lock = &accel_dev->u1.pf.vf_info[vf_nr].pf2vf_lock;
		local_in_use_mask = ADF_PF2VF_IN_USE_BY_PF_MASK;
		local_in_use_pattern = ADF_PF2VF_IN_USE_BY_PF;
		remote_in_use_mask = ADF_VF2PF_IN_USE_BY_VF_MASK;
		remote_in_use_pattern = ADF_VF2PF_IN_USE_BY_VF;
		int_bit = ADF_PF2VF_INT;
		pfvf_counters = &accel_dev->u1.pf.vf_info[vf_nr].pfvf_counters;
	}

	mutex_lock(lock);

	/* Check if PF2VF CSR is in use by remote function */
	val = ADF_CSR_RD(pmisc_bar_addr, pf2vf_offset);
	if ((val & remote_in_use_mask) == remote_in_use_pattern) {
		device_printf(GET_DEV(accel_dev),
			      "PF2VF CSR in use by remote function\n");
		ret = EAGAIN;
		pfvf_counters->busy++;
		goto out;
	}

	/* Attempt to get ownership of PF2VF CSR */
	msg &= ~local_in_use_mask;
	msg |= local_in_use_pattern;
	ADF_CSR_WR(pmisc_bar_addr, pf2vf_offset, msg | int_bit);
	pfvf_counters->tx++;

	/* Wait for confirmation from remote func it received the message */
	do {
		if (udelay < ADF_IOV_MSG_ACK_EXP_MAX_DELAY_US) {
			usleep_range(udelay, udelay * 2);
			udelay = udelay * 2;
			total_delay = total_delay + udelay;
		} else {
			pause_ms("adfstop", mdelay);
			total_delay = total_delay + (mdelay * 1000);
		}
		val = ADF_CSR_RD(pmisc_bar_addr, pf2vf_offset);
	} while ((val & int_bit) &&
		 (total_delay < ADF_IOV_MSG_ACK_LIN_MAX_DELAY_US));

	if (val & int_bit) {
		device_printf(GET_DEV(accel_dev),
			      "ACK not received from remote\n");
		pfvf_counters->no_ack++;
		val &= ~int_bit;
		ret = EIO;
	}

	/* For fire-and-forget notifications, the receiver does not clear
	 * the in-use pattern. This is used to detect collisions.
	 */
	if (is_notification && (val & ~int_bit) != msg) {
		/* Collision must have overwritten the message */
		device_printf(GET_DEV(accel_dev),
			      "Collision on notification\n");
		pfvf_counters->collision++;
		ret = EAGAIN;
		goto out;
	}

	/*
	 * If the far side did not clear the in-use pattern it is either
	 * 1) Notification - message left intact to detect collision
	 * 2) Older protocol (compatibility version < 3) on the far side
	 *    where the sender is responsible for clearing the in-use
	 *    pattern after the received has acknowledged receipt.
	 * In either case, clear the in-use pattern now.
	 */
	if ((val & local_in_use_mask) == local_in_use_pattern)
		ADF_CSR_WR(pmisc_bar_addr,
			   pf2vf_offset,
			   val & ~local_in_use_mask);

out:
	mutex_unlock(lock);
	return ret;
}

static int
adf_iov_put(struct adf_accel_dev *accel_dev,
	    u32 msg,
	    u8 vf_nr,
	    bool is_notification)
{
	u32 count = 0, delay = ADF_IOV_MSG_RETRY_DELAY;
	int ret;
	struct pfvf_stats *pfvf_counters = NULL;

	if (accel_dev->is_vf)
		pfvf_counters = &accel_dev->u1.vf.pfvf_counters;
	else
		pfvf_counters = &accel_dev->u1.pf.vf_info[vf_nr].pfvf_counters;

	do {
		ret = __adf_iov_putmsg(accel_dev, msg, vf_nr, is_notification);
		if (ret == EAGAIN)
			pause_ms("adfstop", delay);
		delay = delay * 2;
	} while (ret == EAGAIN && ++count < ADF_IOV_MSG_MAX_RETRIES);
	if (ret == EAGAIN) {
		if (is_notification)
			pfvf_counters->event_timeout++;
		else
			pfvf_counters->tx_timeout++;
	}

	return ret;
}

/**
 * adf_iov_putmsg() - send PF2VF message
 * @accel_dev:  Pointer to acceleration device.
 * @msg:	Message to send
 * @vf_nr:	VF number to which the message will be sent
 *
 * Function sends a messge from the PF to a VF
 *
 * Return: 0 on success, error code otherwise.
 */
int
adf_iov_putmsg(struct adf_accel_dev *accel_dev, u32 msg, u8 vf_nr)
{
	return adf_iov_put(accel_dev, msg, vf_nr, false);
}

/**
 * adf_iov_notify() - send PF2VF notification message
 * @accel_dev:  Pointer to acceleration device.
 * @msg:	Message to send
 * @vf_nr:	VF number to which the message will be sent
 *
 * Function sends a notification messge from the PF to a VF
 *
 * Return: 0 on success, error code otherwise.
 */
int
adf_iov_notify(struct adf_accel_dev *accel_dev, u32 msg, u8 vf_nr)
{
	return adf_iov_put(accel_dev, msg, vf_nr, true);
}

u8
adf_pfvf_crc(u8 start_crc, u8 *buf, u8 len)
{
	u8 crc = start_crc;

	while (len-- > 0)
		crc = pfvf_crc8_table[(crc ^ *buf++) & 0xff];

	return crc;
}

int
adf_iov_block_provider_register(u8 msg_type,
				const adf_iov_block_provider provider)
{
	if (msg_type >= ARRAY_SIZE(pf2vf_message_providers)) {
		pr_err("QAT: invalid message type %d for PF2VF provider\n",
		       msg_type);
		return -EINVAL;
	}
	if (pf2vf_message_providers[msg_type]) {
		pr_err("QAT: Provider %ps already registered for message %d\n",
		       pf2vf_message_providers[msg_type],
		       msg_type);
		return -EINVAL;
	}

	pf2vf_message_providers[msg_type] = provider;
	return 0;
}

u8
adf_iov_is_block_provider_registered(u8 msg_type)
{
	if (pf2vf_message_providers[msg_type])
		return 1;
	else
		return 0;
}

int
adf_iov_block_provider_unregister(u8 msg_type,
				  const adf_iov_block_provider provider)
{
	if (msg_type >= ARRAY_SIZE(pf2vf_message_providers)) {
		pr_err("QAT: invalid message type %d for PF2VF provider\n",
		       msg_type);
		return -EINVAL;
	}
	if (pf2vf_message_providers[msg_type] != provider) {
		pr_err("QAT: Provider %ps not registered for message %d\n",
		       provider,
		       msg_type);
		return -EINVAL;
	}

	pf2vf_message_providers[msg_type] = NULL;
	return 0;
}

static int
adf_iov_block_get_data(struct adf_accel_dev *accel_dev,
		       u8 msg_type,
		       u8 byte_num,
		       u8 *data,
		       u8 compatibility,
		       bool crc)
{
	u8 *buffer;
	u8 size;
	u8 msg_ver;
	u8 crc8;

	if (msg_type >= ARRAY_SIZE(pf2vf_message_providers)) {
		pr_err("QAT: invalid message type %d for PF2VF provider\n",
		       msg_type);
		*data = ADF_PF2VF_INVALID_BLOCK_TYPE;
		return -EINVAL;
	}

	if (!pf2vf_message_providers[msg_type]) {
		pr_err("QAT: No registered provider for message %d\n",
		       msg_type);
		*data = ADF_PF2VF_INVALID_BLOCK_TYPE;
		return -EINVAL;
	}

	if ((*pf2vf_message_providers[msg_type])(
		accel_dev, &buffer, &size, &msg_ver, compatibility, byte_num)) {
		pr_err("QAT: unknown error from provider for message %d\n",
		       msg_type);
		*data = ADF_PF2VF_UNSPECIFIED_ERROR;
		return -EINVAL;
	}

	if ((msg_type <= ADF_VF2PF_MAX_SMALL_MESSAGE_TYPE &&
	     size > ADF_VF2PF_SMALL_PAYLOAD_SIZE) ||
	    (msg_type <= ADF_VF2PF_MAX_MEDIUM_MESSAGE_TYPE &&
	     size > ADF_VF2PF_MEDIUM_PAYLOAD_SIZE) ||
	    size > ADF_VF2PF_LARGE_PAYLOAD_SIZE) {
		pr_err("QAT: Invalid size %d provided for message type %d\n",
		       size,
		       msg_type);
		*data = ADF_PF2VF_PAYLOAD_TRUNCATED;
		return -EINVAL;
	}

	if ((!byte_num && crc) || byte_num >= size + ADF_VF2PF_BLOCK_DATA) {
		pr_err("QAT: Invalid byte number %d for message %d\n",
		       byte_num,
		       msg_type);
		*data = ADF_PF2VF_INVALID_BYTE_NUM_REQ;
		return -EINVAL;
	}

	if (crc) {
		crc8 = adf_pfvf_crc(ADF_CRC8_INIT_VALUE, &msg_ver, 1);
		crc8 = adf_pfvf_crc(crc8, &size, 1);
		*data = adf_pfvf_crc(crc8, buffer, byte_num - 1);
	} else {
		if (byte_num == 0)
			*data = msg_ver;
		else if (byte_num == 1)
			*data = size;
		else
			*data = buffer[byte_num - 2];
	}

	return 0;
}

static int
adf_iov_block_get_byte(struct adf_accel_dev *accel_dev,
		       u8 msg_type,
		       u8 byte_num,
		       u8 *data,
		       u8 compatibility)
{
	return adf_iov_block_get_data(
	    accel_dev, msg_type, byte_num, data, compatibility, false);
}

static int
adf_iov_block_get_crc(struct adf_accel_dev *accel_dev,
		      u8 msg_type,
		      u8 byte_num,
		      u8 *data,
		      u8 compatibility)
{
	return adf_iov_block_get_data(
	    accel_dev, msg_type, byte_num, data, compatibility, true);
}

int adf_iov_compatibility_check(struct adf_accel_dev *accel_dev, u8 compat_ver);

void
adf_vf2pf_req_hndl(struct adf_accel_vf_info *vf_info)
{
	struct adf_accel_dev *accel_dev = vf_info->accel_dev;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	int bar_id = hw_data->get_misc_bar_id(hw_data);
	struct adf_bar *pmisc = &GET_BARS(accel_dev)[bar_id];
	struct resource *pmisc_addr = pmisc->virt_addr;
	u32 msg, resp = 0, vf_nr = vf_info->vf_nr;
	u8 byte_num = 0;
	u8 msg_type = 0;
	u8 resp_type;
	int res;
	u8 data;
	u8 compat = 0x0;
	int vf_compat_ver = 0;
	bool is_notification = false;

	/* Read message from the VF */
	msg = ADF_CSR_RD(pmisc_addr, hw_data->get_pf2vf_offset(vf_nr));
	if (!(msg & ADF_VF2PF_INT)) {
		device_printf(GET_DEV(accel_dev),
			      "Spurious VF2PF interrupt. msg %X. Ignored\n",
			      msg);
		vf_info->pfvf_counters.spurious++;
		goto out;
	}
	vf_info->pfvf_counters.rx++;

	if (!(msg & ADF_VF2PF_MSGORIGIN_SYSTEM)) {
		/* Ignore legacy non-system (non-kernel) VF2PF messages */
		device_printf(GET_DEV(accel_dev),
			      "Ignored non-system message from VF%d (0x%x);\n",
			      vf_nr + 1,
			      msg);
		/*
		 * To ack, clear the VF2PFINT bit.
		 * Because this must be a legacy message, the far side
		 * must clear the in-use pattern.
		 */
		msg &= ~(ADF_VF2PF_INT);
		ADF_CSR_WR(pmisc_addr, hw_data->get_pf2vf_offset(vf_nr), msg);

		goto out;
	}

	switch ((msg & ADF_VF2PF_MSGTYPE_MASK) >> ADF_VF2PF_MSGTYPE_SHIFT) {
	case ADF_VF2PF_MSGTYPE_COMPAT_VER_REQ:

	{
		is_notification = false;
		vf_compat_ver = msg >> ADF_VF2PF_COMPAT_VER_REQ_SHIFT;
		vf_info->compat_ver = vf_compat_ver;

		resp = (ADF_PF2VF_MSGORIGIN_SYSTEM |
			(ADF_PF2VF_MSGTYPE_VERSION_RESP
			 << ADF_PF2VF_MSGTYPE_SHIFT) |
			(ADF_PFVF_COMPATIBILITY_VERSION
			 << ADF_PF2VF_VERSION_RESP_VERS_SHIFT));

		device_printf(
		    GET_DEV(accel_dev),
		    "Compatibility Version Request from VF%d vers=%u\n",
		    vf_nr + 1,
		    vf_info->compat_ver);

		if (vf_compat_ver < ADF_PFVF_COMPATIBILITY_VERSION)
			compat = adf_iov_compatibility_check(accel_dev,
							     vf_compat_ver);
		else if (vf_compat_ver == ADF_PFVF_COMPATIBILITY_VERSION)
			compat = ADF_PF2VF_VF_COMPATIBLE;
		else
			compat = ADF_PF2VF_VF_COMPAT_UNKNOWN;

		resp |= compat << ADF_PF2VF_VERSION_RESP_RESULT_SHIFT;

		if (compat == ADF_PF2VF_VF_INCOMPATIBLE)
			device_printf(GET_DEV(accel_dev),
				      "VF%d and PF are incompatible.\n",
				      vf_nr + 1);
	} break;
	case ADF_VF2PF_MSGTYPE_VERSION_REQ:
		device_printf(GET_DEV(accel_dev),
			      "Legacy VersionRequest received from VF%d 0x%x\n",
			      vf_nr + 1,
			      msg);
		is_notification = false;

		/* legacy driver, VF compat_ver is 0 */
		vf_info->compat_ver = 0;

		resp = (ADF_PF2VF_MSGORIGIN_SYSTEM |
			(ADF_PF2VF_MSGTYPE_VERSION_RESP
			 << ADF_PF2VF_MSGTYPE_SHIFT));

		/* PF always newer than legacy VF */
		compat =
		    adf_iov_compatibility_check(accel_dev, vf_info->compat_ver);
		resp |= compat << ADF_PF2VF_VERSION_RESP_RESULT_SHIFT;

		/* Set legacy major and minor version num */
		resp |= 1 << ADF_PF2VF_MAJORVERSION_SHIFT |
		    1 << ADF_PF2VF_MINORVERSION_SHIFT;

		if (compat == ADF_PF2VF_VF_INCOMPATIBLE)
			device_printf(GET_DEV(accel_dev),
				      "VF%d and PF are incompatible.\n",
				      vf_nr + 1);
		break;
	case ADF_VF2PF_MSGTYPE_INIT: {
		device_printf(GET_DEV(accel_dev),
			      "Init message received from VF%d 0x%x\n",
			      vf_nr + 1,
			      msg);
		is_notification = true;
		vf_info->init = true;
	} break;
	case ADF_VF2PF_MSGTYPE_SHUTDOWN: {
		device_printf(GET_DEV(accel_dev),
			      "Shutdown message received from VF%d 0x%x\n",
			      vf_nr + 1,
			      msg);
		is_notification = true;
		vf_info->init = false;
	} break;
	case ADF_VF2PF_MSGTYPE_GET_LARGE_BLOCK_REQ:
	case ADF_VF2PF_MSGTYPE_GET_MEDIUM_BLOCK_REQ:
	case ADF_VF2PF_MSGTYPE_GET_SMALL_BLOCK_REQ: {
		is_notification = false;
		switch ((msg & ADF_VF2PF_MSGTYPE_MASK) >>
			ADF_VF2PF_MSGTYPE_SHIFT) {
		case ADF_VF2PF_MSGTYPE_GET_LARGE_BLOCK_REQ:
			byte_num =
			    ((msg & ADF_VF2PF_LARGE_BLOCK_BYTE_NUM_MASK) >>
			     ADF_VF2PF_LARGE_BLOCK_BYTE_NUM_SHIFT);
			msg_type =
			    ((msg & ADF_VF2PF_LARGE_BLOCK_REQ_TYPE_MASK) >>
			     ADF_VF2PF_BLOCK_REQ_TYPE_SHIFT);
			msg_type += ADF_VF2PF_MIN_LARGE_MESSAGE_TYPE;
			break;
		case ADF_VF2PF_MSGTYPE_GET_MEDIUM_BLOCK_REQ:
			byte_num =
			    ((msg & ADF_VF2PF_MEDIUM_BLOCK_BYTE_NUM_MASK) >>
			     ADF_VF2PF_MEDIUM_BLOCK_BYTE_NUM_SHIFT);
			msg_type =
			    ((msg & ADF_VF2PF_MEDIUM_BLOCK_REQ_TYPE_MASK) >>
			     ADF_VF2PF_BLOCK_REQ_TYPE_SHIFT);
			msg_type += ADF_VF2PF_MIN_MEDIUM_MESSAGE_TYPE;
			break;
		case ADF_VF2PF_MSGTYPE_GET_SMALL_BLOCK_REQ:
			byte_num =
			    ((msg & ADF_VF2PF_SMALL_BLOCK_BYTE_NUM_MASK) >>
			     ADF_VF2PF_SMALL_BLOCK_BYTE_NUM_SHIFT);
			msg_type =
			    ((msg & ADF_VF2PF_SMALL_BLOCK_REQ_TYPE_MASK) >>
			     ADF_VF2PF_BLOCK_REQ_TYPE_SHIFT);
			msg_type += ADF_VF2PF_MIN_SMALL_MESSAGE_TYPE;
			break;
		}

		if (msg >> ADF_VF2PF_BLOCK_REQ_CRC_SHIFT) {
			res = adf_iov_block_get_crc(accel_dev,
						    msg_type,
						    byte_num,
						    &data,
						    vf_info->compat_ver);
			if (res)
				resp_type = ADF_PF2VF_BLOCK_RESP_TYPE_ERROR;
			else
				resp_type = ADF_PF2VF_BLOCK_RESP_TYPE_CRC;
		} else {
			if (!byte_num)
				vf_info->pfvf_counters.blk_tx++;

			res = adf_iov_block_get_byte(accel_dev,
						     msg_type,
						     byte_num,
						     &data,
						     vf_info->compat_ver);
			if (res)
				resp_type = ADF_PF2VF_BLOCK_RESP_TYPE_ERROR;
			else
				resp_type = ADF_PF2VF_BLOCK_RESP_TYPE_DATA;
		}
		resp =
		    (ADF_PF2VF_MSGORIGIN_SYSTEM |
		     (ADF_PF2VF_MSGTYPE_BLOCK_RESP << ADF_PF2VF_MSGTYPE_SHIFT) |
		     (resp_type << ADF_PF2VF_BLOCK_RESP_TYPE_SHIFT) |
		     (data << ADF_PF2VF_BLOCK_RESP_DATA_SHIFT));
	} break;
	default:
		device_printf(GET_DEV(accel_dev),
			      "Unknown message from VF%d (0x%x);\n",
			      vf_nr + 1,
			      msg);
	}

	/* To ack, clear the VF2PFINT bit and the in-use-by */
	msg &= ~ADF_VF2PF_INT;
	/*
	 * Clear the in-use pattern if the sender won't do it.
	 * Because the compatibility version must be the first message
	 * exchanged between the VF and PF, the vf_info->compat_ver must be
	 * set at this time.
	 * The in-use pattern is not cleared for notifications so that
	 * it can be used for collision detection.
	 */
	if (vf_info->compat_ver >= ADF_PFVF_COMPATIBILITY_FAST_ACK &&
	    !is_notification)
		msg &= ~ADF_VF2PF_IN_USE_BY_VF_MASK;
	ADF_CSR_WR(pmisc_addr, hw_data->get_pf2vf_offset(vf_nr), msg);

	if (resp && adf_iov_putmsg(accel_dev, resp, vf_nr))
		device_printf(GET_DEV(accel_dev),
			      "Failed to send response to VF\n");

out:
	return;
}

void
adf_pf2vf_notify_restarting(struct adf_accel_dev *accel_dev)
{
	struct adf_accel_vf_info *vf;
	u32 msg = (ADF_PF2VF_MSGORIGIN_SYSTEM |
		   (ADF_PF2VF_MSGTYPE_RESTARTING << ADF_PF2VF_MSGTYPE_SHIFT));

	int i, num_vfs = accel_dev->u1.pf.num_vfs;
	for (i = 0, vf = accel_dev->u1.pf.vf_info; i < num_vfs; i++, vf++) {
		if (vf->init && adf_iov_notify(accel_dev, msg, i))
			device_printf(GET_DEV(accel_dev),
				      "Failed to send restarting msg to VF%d\n",
				      i);
	}
}

void
adf_pf2vf_notify_fatal_error(struct adf_accel_dev *accel_dev)
{
	struct adf_accel_vf_info *vf;
	int i, num_vfs = accel_dev->u1.pf.num_vfs;
	u32 msg = (ADF_PF2VF_MSGORIGIN_SYSTEM |
		   (ADF_PF2VF_MSGTYPE_FATAL_ERROR << ADF_PF2VF_MSGTYPE_SHIFT));

	for (i = 0, vf = accel_dev->u1.pf.vf_info; i < num_vfs; i++, vf++) {
		if (vf->init && adf_iov_notify(accel_dev, msg, i))
			device_printf(
			    GET_DEV(accel_dev),
			    "Failed to send fatal error msg 0x%x to VF%d\n",
			    msg,
			    i);
	}
}

int
adf_iov_register_compat_checker(struct adf_accel_dev *accel_dev,
				const adf_iov_compat_checker_t cc)
{
	struct adf_accel_compat_manager *cm = accel_dev->cm;
	int num = 0;

	if (!cm) {
		device_printf(GET_DEV(accel_dev),
			      "QAT: compatibility manager not initialized\n");
		return ENOMEM;
	}

	for (num = 0; num < ADF_COMPAT_CHECKER_MAX; num++) {
		if (cm->iov_compat_checkers[num]) {
			if (cc == cm->iov_compat_checkers[num]) {
				device_printf(GET_DEV(accel_dev),
					      "QAT: already registered\n");
				return EFAULT;
			}
		} else {
			/* registering the new checker */
			cm->iov_compat_checkers[num] = cc;
			break;
		}
	}

	if (num >= ADF_COMPAT_CHECKER_MAX) {
		device_printf(GET_DEV(accel_dev),
			      "QAT: compatibility checkers are overflow.\n");
		return EFAULT;
	}

	cm->num_chker = num;
	return 0;
}

int
adf_iov_unregister_compat_checker(struct adf_accel_dev *accel_dev,
				  const adf_iov_compat_checker_t cc)
{
	struct adf_accel_compat_manager *cm = accel_dev->cm;
	int num = 0;

	if (!cm) {
		device_printf(GET_DEV(accel_dev),
			      "QAT: compatibility manager not initialized\n");
		return ENOMEM;
	}
	num = cm->num_chker - 1;

	if (num < 0) {
		device_printf(
		    GET_DEV(accel_dev),
		    "QAT: Array 'iov_compat_checkers' may use index value(s) -1\n");
		return EFAULT;
	}
	if (cc == cm->iov_compat_checkers[num]) {
		/* unregistering the given checker */
		cm->iov_compat_checkers[num] = NULL;
	} else {
		device_printf(
		    GET_DEV(accel_dev),
		    "QAT: unregistering not in the registered order\n");
		return EFAULT;
	}

	cm->num_chker--;
	return 0;
}

int
adf_iov_init_compat_manager(struct adf_accel_dev *accel_dev,
			    struct adf_accel_compat_manager **cm)
{
	if (!(*cm)) {
		*cm = malloc(sizeof(**cm), M_QAT, M_WAITOK | M_ZERO);
	} else {
		/* zero the struct */
		explicit_bzero(*cm, sizeof(**cm));
	}

	return 0;
}

int
adf_iov_shutdown_compat_manager(struct adf_accel_dev *accel_dev,
				struct adf_accel_compat_manager **cm)
{
	if (*cm) {
		free(*cm, M_QAT);
		*cm = NULL;
	}
	return 0;
}

int
adf_iov_compatibility_check(struct adf_accel_dev *accel_dev, u8 compat_ver)
{
	int compatible = ADF_PF2VF_VF_COMPATIBLE;
	int i = 0;
	struct adf_accel_compat_manager *cm = accel_dev->cm;

	if (!cm) {
		device_printf(GET_DEV(accel_dev),
			      "QAT: compatibility manager not initialized\n");
		return ADF_PF2VF_VF_INCOMPATIBLE;
	}
	for (i = 0; i < cm->num_chker; i++) {
		compatible = cm->iov_compat_checkers[i](accel_dev, compat_ver);
		if (compatible == ADF_PF2VF_VF_INCOMPATIBLE) {
			device_printf(
			    GET_DEV(accel_dev),
			    "QAT: PF and VF are incompatible [checker%d]\n",
			    i);
			break;
		}
	}
	return compatible;
}

static int
adf_vf2pf_request_version(struct adf_accel_dev *accel_dev)
{
	unsigned long timeout = msecs_to_jiffies(ADF_IOV_MSG_RESP_TIMEOUT);
	u32 msg = 0;
	int ret = 0;
	int comp = 0;
	int response_received = 0;
	int retry_count = 0;
	struct pfvf_stats *pfvf_counters = NULL;

	pfvf_counters = &accel_dev->u1.vf.pfvf_counters;

	msg = ADF_VF2PF_MSGORIGIN_SYSTEM;
	msg |= ADF_VF2PF_MSGTYPE_COMPAT_VER_REQ << ADF_VF2PF_MSGTYPE_SHIFT;
	msg |= ADF_PFVF_COMPATIBILITY_VERSION << ADF_VF2PF_COMPAT_VER_REQ_SHIFT;
	BUILD_BUG_ON(ADF_PFVF_COMPATIBILITY_VERSION > 255);
	/* Clear communication flag - without that VF will not be waiting for
	 * the response from host driver, and start sending init.
	 */
	accel_dev->u1.vf.iov_msg_completion = 0;
	do {
		/* Send request from VF to PF */
		if (retry_count)
			pfvf_counters->retry++;
		if (adf_iov_putmsg(accel_dev, msg, 0)) {
			device_printf(
			    GET_DEV(accel_dev),
			    "Failed to send Compat Version Request.\n");
			return EIO;
		}
		mutex_lock(&accel_dev->u1.vf.vf2pf_lock);
		if (accel_dev->u1.vf.iov_msg_completion == 0 &&
		    sx_sleep(&accel_dev->u1.vf.iov_msg_completion,
			     &accel_dev->u1.vf.vf2pf_lock.sx,
			     0,
			     "pfver",
			     timeout) == EWOULDBLOCK) {
			/* It's possible that wakeup could be missed */
			if (accel_dev->u1.vf.iov_msg_completion) {
				response_received = 1;
			} else {
				device_printf(
				    GET_DEV(accel_dev),
				    "IOV request/response message timeout expired\n");
			}
		} else {
			response_received = 1;
		}
		mutex_unlock(&accel_dev->u1.vf.vf2pf_lock);
	} while (!response_received &&
		 ++retry_count < ADF_IOV_MSG_RESP_RETRIES);

	if (!response_received)
		pfvf_counters->rx_timeout++;
	else
		pfvf_counters->rx_rsp++;
	if (!response_received)
		return EIO;

	if (accel_dev->u1.vf.compatible == ADF_PF2VF_VF_COMPAT_UNKNOWN)
		/* Response from PF received, check compatibility */
		comp = adf_iov_compatibility_check(accel_dev,
						   accel_dev->u1.vf.pf_version);
	else
		comp = accel_dev->u1.vf.compatible;

	ret = (comp == ADF_PF2VF_VF_COMPATIBLE) ? 0 : EFAULT;
	if (ret)
		device_printf(
		    GET_DEV(accel_dev),
		    "VF is not compatible with PF, due to the reason %d\n",
		    comp);

	return ret;
}

/**
 * adf_enable_vf2pf_comms() - Function enables communication from vf to pf
 *
 * @accel_dev: Pointer to acceleration device virtual function.
 *
 * Return: 0 on success, error code otherwise.
 */
int
adf_enable_vf2pf_comms(struct adf_accel_dev *accel_dev)
{
	int ret = 0;

	/* init workqueue for VF */
	ret = adf_init_vf_wq();
	if (ret)
		return ret;

	adf_enable_pf2vf_interrupts(accel_dev);
	adf_iov_init_compat_manager(accel_dev, &accel_dev->cm);
	return adf_vf2pf_request_version(accel_dev);
}
/**
 * adf_disable_vf2pf_comms() - Function disables communication from vf to pf
 *
 * @accel_dev: Pointer to acceleration device virtual function.
 *
 * Return: 0 on success, error code otherwise.
 */
int
adf_disable_vf2pf_comms(struct adf_accel_dev *accel_dev)
{
	return adf_iov_shutdown_compat_manager(accel_dev, &accel_dev->cm);
}

/**
 * adf_pf_enable_vf2pf_comms() - Function enables communication from pf
 *
 * @accel_dev: Pointer to acceleration device physical function.
 *
 * Return: 0 on success, error code otherwise.
 */
int
adf_pf_enable_vf2pf_comms(struct adf_accel_dev *accel_dev)
{
	adf_iov_init_compat_manager(accel_dev, &accel_dev->cm);
	return 0;
}

/**
 * adf_pf_disable_vf2pf_comms() - Function disables communication from pf
 *
 * @accel_dev: Pointer to acceleration device physical function.
 *
 * Return: 0 on success, error code otherwise.
 */
int
adf_pf_disable_vf2pf_comms(struct adf_accel_dev *accel_dev)
{
	return adf_iov_shutdown_compat_manager(accel_dev, &accel_dev->cm);
}
