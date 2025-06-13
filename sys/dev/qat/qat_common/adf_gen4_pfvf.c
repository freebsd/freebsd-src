/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */
#include <linux/iopoll.h>
#include <linux/types.h>
#include "adf_accel_devices.h"
#include "adf_common_drv.h"
#include "adf_gen4_pfvf.h"
#include "adf_pfvf_utils.h"
#include "adf_pfvf_vf_proto.h"

#define ADF_4XXX_PF2VM_OFFSET(i) (0x40B010 + ((i)*0x20))
#define ADF_4XXX_VM2PF_OFFSET(i) (0x40B014 + ((i)*0x20))

/* VF2PF interrupt source registers */
#define ADF_4XXX_VM2PF_SOU 0x41A180
#define ADF_4XXX_VM2PF_MSK 0x41A1C0
#define ADF_GEN4_VF_MSK 0xFFFF

#define ADF_PFVF_GEN4_MSGTYPE_SHIFT 2
#define ADF_PFVF_GEN4_MSGTYPE_MASK 0x3F
#define ADF_PFVF_GEN4_MSGDATA_SHIFT 8
#define ADF_PFVF_GEN4_MSGDATA_MASK 0xFFFFFF

#define ADF_4XXXIOV_PF2VM_OFFSET 0x100C
#define ADF_4XXXIOV_VM2PF_OFFSET 0x1008
static const struct pfvf_csr_format csr_gen4_fmt = {
	{ ADF_PFVF_GEN4_MSGTYPE_SHIFT, ADF_PFVF_GEN4_MSGTYPE_MASK },
	{ ADF_PFVF_GEN4_MSGDATA_SHIFT, ADF_PFVF_GEN4_MSGDATA_MASK },
};

static u32
adf_gen4_vf_get_pfvf_offset(u32 i)
{
	return ADF_4XXXIOV_PF2VM_OFFSET;
}

static u32
adf_gen4_vf_get_vfpf_offset(u32 i)
{
	return ADF_4XXXIOV_VM2PF_OFFSET;
}

static int
adf_gen4_pfvf_send(struct adf_accel_dev *accel_dev,
		   struct pfvf_message msg,
		   u32 pfvf_offset,
		   struct mutex *csr_lock)
{
	struct resource *pmisc_addr = adf_get_pmisc_base(accel_dev);
	u32 csr_val;
	int ret;
	csr_val = adf_pfvf_csr_msg_of(accel_dev, msg, &csr_gen4_fmt);
	if (unlikely(!csr_val))
		return -EINVAL;

	mutex_lock(csr_lock);

	ADF_CSR_WR(pmisc_addr, pfvf_offset, csr_val | ADF_PFVF_INT);

	/* Wait for confirmation from remote that it received the message */
	ret = read_poll_timeout(ADF_CSR_RD,
				csr_val,
				!(csr_val & ADF_PFVF_INT),
				ADF_PFVF_MSG_ACK_DELAY_US,
				ADF_PFVF_MSG_ACK_MAX_DELAY_US,
				true,
				pmisc_addr,
				pfvf_offset);
	if (ret < 0)
		device_printf(GET_DEV(accel_dev),
			      "ACK not received from remote\n");

	mutex_unlock(csr_lock);
	return ret;
}

static int
adf_gen4_vf2pf_send(struct adf_accel_dev *accel_dev,
		    struct pfvf_message msg,
		    u32 pfvf_offset,
		    struct mutex *csr_lock)
{
	return adf_gen4_pfvf_send(accel_dev, msg, pfvf_offset, csr_lock);
}

static struct pfvf_message
adf_gen4_pfvf_recv(struct adf_accel_dev *accel_dev,
		   u32 pfvf_offset,
		   u8 compat_ver)
{
	struct resource *pmisc_addr = adf_get_pmisc_base(accel_dev);
	struct pfvf_message msg = { 0 };
	u32 csr_val;

	/* Read message from the CSR */
	csr_val = ADF_CSR_RD(pmisc_addr, pfvf_offset);
	if (!(csr_val & ADF_PFVF_INT)) {
		device_printf(GET_DEV(accel_dev),
			      "Spurious PFVF interrupt, msg 0x%.8x. Ignored\n",
			      csr_val);
		return msg;
	}

	/* We can now acknowledge the message reception by clearing the
	 * interrupt bit
	 */
	ADF_CSR_WR(pmisc_addr, pfvf_offset, csr_val & ~ADF_PFVF_INT);

	/* Return the pfvf_message format */
	return adf_pfvf_message_of(accel_dev, csr_val, &csr_gen4_fmt);
}

static struct pfvf_message
adf_gen4_pf2vf_recv(struct adf_accel_dev *accel_dev,
		    u32 pfvf_offset,
		    u8 compat_ver)
{
	return adf_gen4_pfvf_recv(accel_dev, pfvf_offset, compat_ver);
}

void
adf_gen4_init_vf_pfvf_ops(struct adf_pfvf_ops *pfvf_ops)
{
	pfvf_ops->enable_comms = adf_enable_vf2pf_comms;
	pfvf_ops->get_pf2vf_offset = adf_gen4_vf_get_pfvf_offset;
	pfvf_ops->get_vf2pf_offset = adf_gen4_vf_get_vfpf_offset;
	pfvf_ops->send_msg = adf_gen4_vf2pf_send;
	pfvf_ops->recv_msg = adf_gen4_pf2vf_recv;
}
