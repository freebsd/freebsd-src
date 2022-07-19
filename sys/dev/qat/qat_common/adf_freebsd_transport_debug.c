/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
#include "qat_freebsd.h"
#include "adf_cfg.h"
#include "adf_common_drv.h"
#include "adf_accel_devices.h"
#include "icp_qat_uclo.h"
#include "icp_qat_fw.h"
#include "icp_qat_fw_init_admin.h"
#include "adf_cfg_strings.h"
#include "adf_transport_access_macros.h"
#include "adf_transport_internal.h"
#include <sys/malloc.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

static int adf_ring_show(SYSCTL_HANDLER_ARGS)
{
	struct adf_etr_ring_data *ring = arg1;
	struct adf_etr_bank_data *bank = ring->bank;
	struct resource *csr = ring->bank->csr_addr;
	struct sbuf sb;
	int error, word;
	uint32_t *wp, *end;

	sbuf_new_for_sysctl(&sb, NULL, 128, req);
	{
		int head, tail, empty;

		head = READ_CSR_RING_HEAD(csr,
					  bank->bank_number,
					  ring->ring_number);
		tail = READ_CSR_RING_TAIL(csr,
					  bank->bank_number,
					  ring->ring_number);
		empty = READ_CSR_E_STAT(csr, bank->bank_number);

		sbuf_cat(&sb, "\n------- Ring configuration -------\n");
		sbuf_printf(&sb,
			    "ring name: %s\n",
			    ring->ring_debug->ring_name);
		sbuf_printf(&sb,
			    "ring num %d, bank num %d\n",
			    ring->ring_number,
			    ring->bank->bank_number);
		sbuf_printf(&sb,
			    "head %x, tail %x, empty: %d\n",
			    head,
			    tail,
			    (empty & 1 << ring->ring_number) >>
				ring->ring_number);
		sbuf_printf(&sb,
			    "ring size %d, msg size %d\n",
			    ADF_SIZE_TO_RING_SIZE_IN_BYTES(ring->ring_size),
			    ADF_MSG_SIZE_TO_BYTES(ring->msg_size));
		sbuf_cat(&sb, "----------- Ring data ------------\n");
	}
	wp = ring->base_addr;
	end = (uint32_t *)((char *)ring->base_addr +
			   ADF_SIZE_TO_RING_SIZE_IN_BYTES(ring->ring_size));
	while (wp < end) {
		sbuf_printf(&sb, "%p:", wp);
		for (word = 0; word < 32 / 4; word++, wp++)
			sbuf_printf(&sb, " %08x", *wp);
		sbuf_printf(&sb, "\n");
	}
	error = sbuf_finish(&sb);
	sbuf_delete(&sb);
	return (error);
}

int
adf_ring_debugfs_add(struct adf_etr_ring_data *ring, const char *name)
{
	struct adf_etr_ring_debug_entry *ring_debug;
	char entry_name[8];

	ring_debug = malloc(sizeof(*ring_debug), M_QAT, M_WAITOK | M_ZERO);

	strlcpy(ring_debug->ring_name, name, sizeof(ring_debug->ring_name));
	snprintf(entry_name,
		 sizeof(entry_name),
		 "ring_%02d",
		 ring->ring_number);

	ring_debug->debug =
	    SYSCTL_ADD_PROC(&ring->bank->accel_dev->sysctl_ctx,
			    SYSCTL_CHILDREN(ring->bank->bank_debug_dir),
			    OID_AUTO,
			    entry_name,
			    CTLFLAG_RD | CTLTYPE_STRING,
			    ring,
			    0,
			    adf_ring_show,
			    "A",
			    "Ring configuration");

	if (!ring_debug->debug) {
		printf("QAT: Failed to create ring debug entry.\n");
		free(ring_debug, M_QAT);
		return EFAULT;
	}
	ring->ring_debug = ring_debug;
	return 0;
}

void
adf_ring_debugfs_rm(struct adf_etr_ring_data *ring)
{
	if (ring->ring_debug) {
		free(ring->ring_debug, M_QAT);
		ring->ring_debug = NULL;
	}
}

static int adf_bank_show(SYSCTL_HANDLER_ARGS)
{
	struct adf_etr_bank_data *bank;
	struct adf_accel_dev *accel_dev = NULL;
	struct adf_hw_device_data *hw_data = NULL;
	u8 num_rings_per_bank = 0;
	struct sbuf sb;
	int error, ring_id;

	sbuf_new_for_sysctl(&sb, NULL, 128, req);
	bank = arg1;
	accel_dev = bank->accel_dev;
	hw_data = accel_dev->hw_device;
	num_rings_per_bank = hw_data->num_rings_per_bank;
	sbuf_printf(&sb,
		    "\n------- Bank %d configuration -------\n",
		    bank->bank_number);
	for (ring_id = 0; ring_id < num_rings_per_bank; ring_id++) {
		struct adf_etr_ring_data *ring = &bank->rings[ring_id];
		struct resource *csr = bank->csr_addr;
		int head, tail, empty;

		if (!(bank->ring_mask & 1 << ring_id))
			continue;

		head = READ_CSR_RING_HEAD(csr,
					  bank->bank_number,
					  ring->ring_number);
		tail = READ_CSR_RING_TAIL(csr,
					  bank->bank_number,
					  ring->ring_number);
		empty = READ_CSR_E_STAT(csr, bank->bank_number);

		sbuf_printf(&sb,
			    "ring num %02d, head %04x, tail %04x, empty: %d\n",
			    ring->ring_number,
			    head,
			    tail,
			    (empty & 1 << ring->ring_number) >>
				ring->ring_number);
	}
	error = sbuf_finish(&sb);
	sbuf_delete(&sb);
	return (error);
}

int
adf_bank_debugfs_add(struct adf_etr_bank_data *bank)
{
	struct adf_accel_dev *accel_dev = bank->accel_dev;
	struct sysctl_oid *parent = accel_dev->transport->debug;
	char name[9];

	snprintf(name, sizeof(name), "bank_%03d", bank->bank_number);

	bank->bank_debug_dir = SYSCTL_ADD_NODE(&accel_dev->sysctl_ctx,
					       SYSCTL_CHILDREN(parent),
					       OID_AUTO,
					       name,
					       CTLFLAG_RD | CTLFLAG_SKIP,
					       NULL,
					       "");

	if (!bank->bank_debug_dir) {
		printf("QAT: Failed to create bank debug dir.\n");
		return EFAULT;
	}

	bank->bank_debug_cfg =
	    SYSCTL_ADD_PROC(&accel_dev->sysctl_ctx,
			    SYSCTL_CHILDREN(bank->bank_debug_dir),
			    OID_AUTO,
			    "config",
			    CTLFLAG_RD | CTLTYPE_STRING,
			    bank,
			    0,
			    adf_bank_show,
			    "A",
			    "Bank configuration");

	if (!bank->bank_debug_cfg) {
		printf("QAT: Failed to create bank debug entry.\n");
		return EFAULT;
	}

	return 0;
}

void
adf_bank_debugfs_rm(struct adf_etr_bank_data *bank)
{
}
