/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
#ifndef ADF_TRANSPORT_H
#define ADF_TRANSPORT_H

#include "adf_accel_devices.h"

struct adf_etr_ring_data;

typedef void (*adf_callback_fn)(void *resp_msg);

int adf_create_ring(struct adf_accel_dev *accel_dev,
		    const char *section,
		    u32 bank_num,
		    u32 num_mgs,
		    u32 msg_size,
		    const char *ring_name,
		    adf_callback_fn callback,
		    int poll_mode,
		    struct adf_etr_ring_data **ring_ptr);

int adf_send_message(struct adf_etr_ring_data *ring, u32 *msg);
void adf_remove_ring(struct adf_etr_ring_data *ring);
int adf_poll_bank(u32 accel_id, u32 bank_num, u32 quota);
int adf_poll_all_banks(u32 accel_id, u32 quota);
#endif /* ADF_TRANSPORT_H */
