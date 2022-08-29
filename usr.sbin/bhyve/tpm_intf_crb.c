/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2022 Beckhoff Automation GmbH & Co. KG
 * Author: Corvin Köhne <c.koehne@beckhoff.com>
 */

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/linker_set.h>

#include <machine/vmm.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <pthread.h>
#include <pthread_np.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <vmmapi.h>

#include "basl.h"
#include "config.h"
#include "mem.h"
#include "qemu_fwcfg.h"
#include "tpm_device.h"
#include "tpm_intf.h"

#define TPM_CRB_ADDRESS 0xFED40000
#define TPM_CRB_REGS_SIZE 0x1000

#define TPM_CRB_CONTROL_AREA_ADDRESS \
	(TPM_CRB_ADDRESS + offsetof(struct tpm_crb_regs, ctrl_req))
#define TPM_CRB_CONTROL_AREA_SIZE TPM_CRB_REGS_SIZE

#define TPM_CRB_DATA_BUFFER_ADDRESS \
	(TPM_CRB_ADDRESS + offsetof(struct tpm_crb_regs, data_buffer))
#define TPM_CRB_DATA_BUFFER_SIZE 0xF80

#define TPM_CRB_LOCALITIES_MAX 5

#define TPM_CRB_LOG_AREA_MINIMUM_SIZE (64 * 1024)

#define TPM_CRB_LOG_AREA_FWCFG_NAME "etc/tpm/log"

#define TPM_CRB_INTF_NAME "crb"

struct tpm_crb_regs {
	union tpm_crb_reg_loc_state {
		struct {
			uint32_t tpm_established : 1;
			uint32_t loc_assigned : 1;
			uint32_t active_locality : 3;
			uint32_t _reserved : 2;
			uint32_t tpm_req_valid_sts : 1;
		};
		uint32_t val;
	} loc_state;	       /* 0h */
	uint8_t _reserved1[4]; /* 4h */
	union tpm_crb_reg_loc_ctrl {
		struct {
			uint32_t request_access : 1;
			uint32_t relinquish : 1;
			uint32_t seize : 1;
			uint32_t reset_establishment_bit : 1;
		};
		uint32_t val;
	} loc_ctrl; /* 8h */
	union tpm_crb_reg_loc_sts {
		struct {
			uint32_t granted : 1;
			uint32_t been_seized : 1;
		};
		uint32_t val;
	} loc_sts;		  /* Ch */
	uint8_t _reserved2[0x20]; /* 10h */
	union tpm_crb_reg_intf_id {
		struct {
			uint64_t interface_type : 4;
			uint64_t interface_version : 4;
			uint64_t cap_locality : 1;
			uint64_t cap_crb_idle_bypass : 1;
			uint64_t _reserved1 : 1;
			uint64_t cap_data_xfer_size_support : 2;
			uint64_t cap_fifo : 1;
			uint64_t cap_crb : 1;
			uint64_t _reserved2 : 2;
			uint64_t interface_selector : 2;
			uint64_t intf_sel_lock : 1;
			uint64_t _reserved3 : 4;
			uint64_t rid : 8;
			uint64_t vid : 16;
			uint64_t did : 16;
		};
		uint64_t val;
	} intf_id; /* 30h */
	union tpm_crb_reg_ctrl_ext {
		struct {
			uint32_t clear;
			uint32_t remaining_bytes;
		};
		uint64_t val;
	} ctrl_ext; /* 38 */
	union tpm_crb_reg_ctrl_req {
		struct {
			uint32_t cmd_ready : 1;
			uint32_t go_idle : 1;
		};
		uint32_t val;
	} ctrl_req; /* 40h */
	union tpm_crb_reg_ctrl_sts {
		struct {
			uint32_t tpm_sts : 1;
			uint32_t tpm_idle : 1;
		};
		uint32_t val;
	} ctrl_sts; /* 44h */
	union tpm_crb_reg_ctrl_cancel {
		struct {
			uint32_t cancel : 1;
		};
		uint32_t val;
	} ctrl_cancel; /* 48h */
	union tpm_crb_reg_ctrl_start {
		struct {
			uint32_t start : 1;
		};
		uint32_t val;
	} ctrl_start;				       /* 4Ch*/
	uint32_t int_enable;			       /* 50h */
	uint32_t int_sts;			       /* 54h */
	uint32_t cmd_size;			       /* 58h */
	uint32_t cmd_addr_lo;			       /* 5Ch */
	uint32_t cmd_addr_hi;			       /* 60h */
	uint32_t rsp_size;			       /* 64h */
	uint64_t rsp_addr;			       /* 68h */
	uint8_t _reserved3[0x10];		       /* 70h */
	uint8_t data_buffer[TPM_CRB_DATA_BUFFER_SIZE]; /* 80h */
} __packed;
static_assert(sizeof(struct tpm_crb_regs) == TPM_CRB_REGS_SIZE,
    "Invalid size of tpm_crb");

#define CRB_CMD_SIZE_READ(regs) (regs.cmd_size)
#define CRB_CMD_SIZE_WRITE(regs, val) \
	do {                          \
		regs.cmd_size = val;  \
	} while (0)
#define CRB_CMD_ADDR_READ(regs) \
	(((uint64_t)regs.cmd_addr_hi << 32) | regs.cmd_addr_lo)
#define CRB_CMD_ADDR_WRITE(regs, val)                \
	do {                                         \
		regs.cmd_addr_lo = val & 0xFFFFFFFF; \
		regs.cmd_addr_hi = val >> 32;        \
	} while (0)
#define CRB_RSP_SIZE_READ(regs) (regs.rsp_size)
#define CRB_RSP_SIZE_WRITE(regs, val) \
	do {                          \
		regs.rsp_size = val;  \
	} while (0)
#define CRB_RSP_ADDR_READ(regs) (regs.rsp_addr)
#define CRB_RSP_ADDR_WRITE(regs, val) \
	do {                          \
		regs.rsp_addr = val;  \
	} while (0)

struct tpm_crb {
	struct tpm_emul *emul;
	void *emul_sc;
	uint8_t tpm_log_area[TPM_CRB_LOG_AREA_MINIMUM_SIZE];
	struct tpm_crb_regs regs;
	pthread_t thread;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	bool closing;
};

static void *
tpm_crb_thread(void *const arg)
{
	struct tpm_crb *const crb = arg;

	pthread_mutex_lock(&crb->mutex);
	for (;;) {
		pthread_cond_wait(&crb->cond, &crb->mutex);

		if (crb->closing)
			break;

		const uint64_t cmd_addr = CRB_CMD_ADDR_READ(crb->regs);
		const uint64_t rsp_addr = CRB_RSP_ADDR_READ(crb->regs);
		const uint32_t cmd_size = CRB_CMD_SIZE_READ(crb->regs);
		const uint32_t rsp_size = CRB_RSP_SIZE_READ(crb->regs);

		const uint64_t cmd_off = cmd_addr - TPM_CRB_DATA_BUFFER_ADDRESS;
		const uint64_t rsp_off = rsp_addr - TPM_CRB_DATA_BUFFER_ADDRESS;

		if (cmd_off > TPM_CRB_DATA_BUFFER_SIZE ||
		    cmd_off + cmd_size > TPM_CRB_DATA_BUFFER_SIZE ||
		    rsp_off > TPM_CRB_DATA_BUFFER_SIZE ||
		    rsp_off + rsp_size > TPM_CRB_DATA_BUFFER_SIZE) {
			warnx(
			    "%s: invalid cmd [%16lx, %16lx] --> [%16lx, %16lx]\n\r",
			    __func__, cmd_addr, cmd_addr + cmd_size, rsp_addr,
			    rsp_addr + rsp_size);
			break;
		}

		/*
		 * The command response buffer interface uses a single buffer
		 * for sending a command to and receiving a response from the
		 * tpm. To avoid reading old data from the command buffer which
		 * might be a security issue, we zero out the command buffer
		 * before writing the response into it. The rsp_size parameter
		 * is controlled by the guest and it's not guaranteed that the
		 * response has a size of rsp_size (e.g. if the tpm returned an
		 * error, the response would have a different size than
		 * expected). For that reason, use a second buffer for the
		 * response.
		 */
		uint8_t rsp[TPM_CRB_DATA_BUFFER_SIZE] = { 0 };
		crb->emul->execute_cmd(crb->emul_sc,
		    &crb->regs.data_buffer[cmd_off], cmd_size, &rsp[rsp_off],
		    rsp_size);

		memset(crb->regs.data_buffer, 0, TPM_CRB_DATA_BUFFER_SIZE);
		memcpy(&crb->regs.data_buffer[rsp_off], &rsp[rsp_off], rsp_size);

		crb->regs.ctrl_start.start = false;
	}
	pthread_mutex_unlock(&crb->mutex);

	return (NULL);
}

static int
tpm_crb_init(void **sc, struct tpm_emul *emul, void *emul_sc)
{
	struct tpm_crb *crb = NULL;
	int error;

	assert(sc != NULL);
	assert(emul != NULL);

	crb = calloc(1, sizeof(struct tpm_crb));
	if (crb == NULL) {
		warnx("%s: failed to allocate tpm crb", __func__);
		error = ENOMEM;
		goto err_out;
	}

	memset(crb, 0, sizeof(*crb));

	crb->emul = emul;
	crb->emul_sc = emul_sc;

	crb->regs.loc_state.tpm_req_valid_sts = true;
	crb->regs.loc_state.tpm_established = true;

	crb->regs.intf_id.interface_type = TPM_INTF_TYPE_CRB;
	crb->regs.intf_id.interface_version = TPM_INTF_VERSION_CRB;
	crb->regs.intf_id.cap_locality = false;
	crb->regs.intf_id.cap_crb_idle_bypass = false;
	crb->regs.intf_id.cap_data_xfer_size_support =
	    TPM_INTF_CAP_CRB_DATA_XFER_SIZE_64;
	crb->regs.intf_id.cap_fifo = false;
	crb->regs.intf_id.cap_crb = true;
	crb->regs.intf_id.interface_selector = TPM_INTF_SELECTOR_CRB;
	crb->regs.intf_id.intf_sel_lock = false;
	crb->regs.intf_id.rid = 0;
	crb->regs.intf_id.vid = 0x1014; /* IBM */
	crb->regs.intf_id.did = 0x1014; /* IBM */

	crb->regs.ctrl_sts.tpm_idle = true;

	CRB_CMD_SIZE_WRITE(crb->regs, TPM_CRB_DATA_BUFFER_SIZE);
	CRB_CMD_ADDR_WRITE(crb->regs, TPM_CRB_DATA_BUFFER_ADDRESS);
	CRB_RSP_SIZE_WRITE(crb->regs, TPM_CRB_DATA_BUFFER_SIZE);
	CRB_RSP_ADDR_WRITE(crb->regs, TPM_CRB_DATA_BUFFER_ADDRESS);

	error = qemu_fwcfg_add_file(TPM_CRB_LOG_AREA_FWCFG_NAME,
	    TPM_CRB_LOG_AREA_MINIMUM_SIZE, crb->tpm_log_area);
	if (error) {
		warnx("%s: failed to add fwcfg file", __func__);
		goto err_out;
	}

	error = pthread_mutex_init(&crb->mutex, NULL);
	if (error) {
		warnc(error, "%s: failed to init mutex", __func__);
		goto err_out;
	}

	error = pthread_cond_init(&crb->cond, NULL);
	if (error) {
		warnc(error, "%s: failed to init cond", __func__);
		goto err_out;
	}

	error = pthread_create(&crb->thread, NULL, tpm_crb_thread, crb);
	if (error) {
		warnx("%s: failed to create thread\n", __func__);
		goto err_out;
	}

	pthread_set_name_np(crb->thread, "tpm_intf_crb");

	*sc = crb;

	return (0);

err_out:
	free(crb);

	return (error);
}

static void
tpm_crb_deinit(void *sc)
{
	struct tpm_crb *crb;

	if (sc == NULL) {
		return;
	}

	crb = sc;

	crb->closing = true;
	pthread_cond_signal(&crb->cond);
	pthread_join(crb->thread, NULL);

	pthread_cond_destroy(&crb->cond);
	pthread_mutex_destroy(&crb->mutex);

	free(crb);
}

static int
tpm_crb_build_acpi_table(void *sc __unused, struct vmctx *vm_ctx)
{
	struct basl_table *table;

	BASL_EXEC(basl_table_create(&table, vm_ctx, ACPI_SIG_TPM2,
	    BASL_TABLE_ALIGNMENT));

	/* Header */
	BASL_EXEC(basl_table_append_header(table, ACPI_SIG_TPM2, 4, 1));
	/* Platform Class */
	BASL_EXEC(basl_table_append_int(table, 0, 2));
	/* Reserved */
	BASL_EXEC(basl_table_append_int(table, 0, 2));
	/* Control Address */
	BASL_EXEC(
	    basl_table_append_int(table, TPM_CRB_CONTROL_AREA_ADDRESS, 8));
	/* Start Method == (7) Command Response Buffer */
	BASL_EXEC(basl_table_append_int(table, 7, 4));
	/* Start Method Specific Parameters */
	uint8_t parameters[12] = { 0 };
	BASL_EXEC(basl_table_append_bytes(table, parameters, 12));
	/* Log Area Minimum Length */
	BASL_EXEC(
	    basl_table_append_int(table, TPM_CRB_LOG_AREA_MINIMUM_SIZE, 4));
	/* Log Area Start Address */
	BASL_EXEC(
	    basl_table_append_fwcfg(table, TPM_CRB_LOG_AREA_FWCFG_NAME, 1, 8));

	BASL_EXEC(basl_table_register_to_rsdt(table));

	return (0);
}

static struct tpm_intf tpm_intf_crb = {
	.name = TPM_CRB_INTF_NAME,
	.init = tpm_crb_init,
	.deinit = tpm_crb_deinit,
	.build_acpi_table = tpm_crb_build_acpi_table,
};
TPM_INTF_SET(tpm_intf_crb);
