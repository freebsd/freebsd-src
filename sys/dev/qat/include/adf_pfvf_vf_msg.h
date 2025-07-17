/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */
#ifndef ADF_PFVF_VF_MSG_H
#define ADF_PFVF_VF_MSG_H

int adf_vf2pf_notify_init(struct adf_accel_dev *accel_dev);
void adf_vf2pf_notify_shutdown(struct adf_accel_dev *accel_dev);
int adf_vf2pf_request_version(struct adf_accel_dev *accel_dev);
int adf_vf2pf_get_capabilities(struct adf_accel_dev *accel_dev);
int adf_vf2pf_get_ring_to_svc(struct adf_accel_dev *accel_dev);
void adf_vf2pf_restarting_complete(struct adf_accel_dev *accel_dev);

#endif /* ADF_PFVF_VF_MSG_H */
