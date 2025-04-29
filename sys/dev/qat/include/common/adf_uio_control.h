/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */
#ifndef QAT_UIO_CONTROL_H
#define QAT_UIO_CONTROL_H
#include <sys/condvar.h>

struct adf_uio_instance_rings {
	unsigned int user_pid;
	u16 ring_mask;
	struct list_head list;
};

struct adf_uio_control_bundle {
	uint8_t hardware_bundle_number;
	bool used;
	struct list_head list;
	struct mutex list_lock; /* protects list struct */
	struct mutex lock;      /* protects rings_used and csr_addr */
	u16 rings_used;
	u32 rings_enabled;
	void *csr_addr;
	struct qat_uio_bundle_dev uio_priv;
	vm_object_t obj;
};

struct adf_uio_control_accel {
	struct adf_accel_dev *accel_dev;
	struct cdev *cdev;
	struct mtx lock;
	struct adf_bar *bar;
	unsigned int nb_bundles;
	unsigned int num_ker_bundles;
	unsigned int total_used_bundles;
	unsigned int num_handles;
	struct cv cleanup_ok;
	/* bundle[] must be last to allow dynamic size allocation. */
	struct adf_uio_control_bundle bundle[0];

};

#endif /* end of include guard: QAT_UIO_CONTROL_H */
