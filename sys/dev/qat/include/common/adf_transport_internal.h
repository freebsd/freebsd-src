/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
#ifndef ADF_TRANSPORT_INTRN_H
#define ADF_TRANSPORT_INTRN_H

#include "adf_transport.h"

struct adf_etr_ring_debug_entry {
	char ring_name[ADF_CFG_MAX_KEY_LEN_IN_BYTES];
	struct sysctl_oid *debug;
};

struct adf_etr_ring_data {
	void *base_addr;
	atomic_t *inflights;
	struct mtx lock; /* protects ring data struct */
	adf_callback_fn callback;
	struct adf_etr_bank_data *bank;
	bus_addr_t dma_addr;
	uint16_t head;
	uint16_t tail;
	uint8_t ring_number;
	uint8_t ring_size;
	uint8_t msg_size;
	uint8_t reserved;
	struct adf_etr_ring_debug_entry *ring_debug;
	struct bus_dmamem dma_mem;
	u32 csr_tail_offset;
	u32 max_inflights;
};

struct adf_etr_bank_data {
	struct adf_etr_ring_data *rings;
	struct task resp_handler;
	struct resource *csr_addr;
	struct adf_accel_dev *accel_dev;
	uint32_t irq_coalesc_timer;
	uint16_t ring_mask;
	uint16_t irq_mask;
	struct mtx lock; /* protects bank data struct */
	struct sysctl_oid *bank_debug_dir;
	struct sysctl_oid *bank_debug_cfg;
	uint32_t bank_number;
};

struct adf_etr_data {
	struct adf_etr_bank_data *banks;
	struct sysctl_oid *debug;
};

void adf_response_handler(uintptr_t bank_addr);
int adf_handle_response(struct adf_etr_ring_data *ring, u32 quota);
int adf_bank_debugfs_add(struct adf_etr_bank_data *bank);
void adf_bank_debugfs_rm(struct adf_etr_bank_data *bank);
int adf_ring_debugfs_add(struct adf_etr_ring_data *ring, const char *name);
void adf_ring_debugfs_rm(struct adf_etr_ring_data *ring);
#endif
