/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
#ifndef ADF_PFVF_UTILS_H
#define ADF_PFVF_UTILS_H

#include <linux/types.h>
#include "adf_pfvf_msg.h"

/* How long to wait for far side to acknowledge receipt */
#define ADF_PFVF_MSG_ACK_DELAY_US 4
#define ADF_PFVF_MSG_ACK_MAX_DELAY_US (1 * USEC_PER_SEC)

u8 adf_pfvf_calc_blkmsg_crc(u8 const *buf, u8 buf_len);

struct pfvf_field_format {
	u8 offset;
	u32 mask;
};

struct pfvf_csr_format {
	struct pfvf_field_format type;
	struct pfvf_field_format data;
};

u32 adf_pfvf_csr_msg_of(struct adf_accel_dev *accel_dev,
			struct pfvf_message msg,
			const struct pfvf_csr_format *fmt);
struct pfvf_message adf_pfvf_message_of(struct adf_accel_dev *accel_dev,
					u32 raw_msg,
					const struct pfvf_csr_format *fmt);

static inline struct resource *
adf_get_pmisc_base(struct adf_accel_dev *accel_dev)
{
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	struct adf_bar *pmisc;

	pmisc = &GET_BARS(accel_dev)[hw_data->get_misc_bar_id(hw_data)];

	return pmisc->virt_addr;
}

#endif /* ADF_PFVF_UTILS_H */
