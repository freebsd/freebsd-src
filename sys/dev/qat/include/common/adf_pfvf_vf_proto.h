/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
#ifndef ADF_PFVF_VF_PROTO_H
#define ADF_PFVF_VF_PROTO_H

#include <linux/types.h>
#include "adf_accel_devices.h"

#define ADF_PFVF_MSG_COLLISION_DETECT_DELAY 10
#define ADF_PFVF_MSG_ACK_DELAY 2
#define ADF_PFVF_MSG_ACK_MAX_RETRY 100

/* How often to retry if there is no response */
#define ADF_PFVF_MSG_RESP_RETRIES 5
#define ADF_PFVF_MSG_RESP_TIMEOUT                                              \
	(ADF_PFVF_MSG_ACK_DELAY * ADF_PFVF_MSG_ACK_MAX_RETRY +                 \
	 ADF_PFVF_MSG_COLLISION_DETECT_DELAY)

int adf_send_vf2pf_msg(struct adf_accel_dev *accel_dev,
		       struct pfvf_message msg);
int adf_send_vf2pf_req(struct adf_accel_dev *accel_dev,
		       struct pfvf_message msg,
		       struct pfvf_message *resp);
int adf_send_vf2pf_blkmsg_req(struct adf_accel_dev *accel_dev,
			      u8 type,
			      u8 *buffer,
			      unsigned int *buffer_len);

int adf_enable_vf2pf_comms(struct adf_accel_dev *accel_dev);

#endif /* ADF_PFVF_VF_PROTO_H */
