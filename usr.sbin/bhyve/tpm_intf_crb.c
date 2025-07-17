/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Beckhoff Automation GmbH & Co. KG
 * Author: Corvin Köhne <c.koehne@beckhoff.com>
 */

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

struct tpm_cmd_hdr {
	uint16_t tag;
	uint32_t len;
	union {
		uint32_t ordinal;
		uint32_t errcode;
	};
} __packed;

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
		/*
		 * We're releasing the lock after wake up. Therefore, we have to
		 * check the closing condition before and after going to sleep.
		 */
		if (crb->closing)
			break;

		pthread_cond_wait(&crb->cond, &crb->mutex);

		if (crb->closing)
			break;

		const uint64_t cmd_addr = CRB_CMD_ADDR_READ(crb->regs);
		const uint64_t rsp_addr = CRB_RSP_ADDR_READ(crb->regs);
		const uint32_t cmd_size = CRB_CMD_SIZE_READ(crb->regs);
		const uint32_t rsp_size = CRB_RSP_SIZE_READ(crb->regs);

		if ((cmd_addr < TPM_CRB_DATA_BUFFER_ADDRESS) ||
		    (cmd_size < sizeof (struct tpm_cmd_hdr)) ||
		    (cmd_size > TPM_CRB_DATA_BUFFER_SIZE) ||
		    (cmd_addr + cmd_size >
		     TPM_CRB_DATA_BUFFER_ADDRESS + TPM_CRB_DATA_BUFFER_SIZE)) {
			warnx("%s: invalid cmd [%16lx/%8x] outside of TPM "
			    "buffer", __func__, cmd_addr, cmd_size);
			break;
		}

		if ((rsp_addr < TPM_CRB_DATA_BUFFER_ADDRESS) ||
		    (rsp_size < sizeof (struct tpm_cmd_hdr)) ||
		    (rsp_size > TPM_CRB_DATA_BUFFER_SIZE) ||
		    (rsp_addr + rsp_size >
		     TPM_CRB_DATA_BUFFER_ADDRESS + TPM_CRB_DATA_BUFFER_SIZE)) {
			warnx("%s: invalid rsp [%16lx/%8x] outside of TPM "
			    "buffer", __func__, rsp_addr, rsp_size);
			break;
		}

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

		uint8_t cmd[TPM_CRB_DATA_BUFFER_SIZE];
		memcpy(cmd, crb->regs.data_buffer, TPM_CRB_DATA_BUFFER_SIZE);

		/*
		 * Do a basic sanity check of the TPM request header. We'll need
		 * the TPM request length for execute_cmd() below.
		 */
		struct tpm_cmd_hdr *req = (struct tpm_cmd_hdr *)&cmd[cmd_off];
		if (be32toh(req->len) < sizeof (struct tpm_cmd_hdr) ||
		    be32toh(req->len) > cmd_size) {
			warnx("%s: invalid TPM request header", __func__);
			break;
		}

		/*
		 * A TPM command can take multiple seconds to execute. As we've
		 * copied all required values and buffers at this point, we can
		 * release the mutex.
		 */
		pthread_mutex_unlock(&crb->mutex);

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
		(void) crb->emul->execute_cmd(crb->emul_sc, req,
		    be32toh(req->len), &rsp[rsp_off], rsp_size);

		pthread_mutex_lock(&crb->mutex);
		memset(crb->regs.data_buffer, 0, TPM_CRB_DATA_BUFFER_SIZE);
		memcpy(&crb->regs.data_buffer[rsp_off], &rsp[rsp_off], rsp_size);

		crb->regs.ctrl_start.start = false;
	}
	pthread_mutex_unlock(&crb->mutex);

	return (NULL);
}

static int
tpm_crb_mmiocpy(void *const dst, void *const src, const int size)
{
	if (!(size == 1 || size == 2 || size == 4 || size == 8))
		return (EINVAL);
	memcpy(dst, src, size);

	return (0);
}

static int
tpm_crb_mem_handler(struct vcpu *vcpu __unused, const int dir,
    const uint64_t addr, const int size, uint64_t *const val, void *const arg1,
    const long arg2 __unused)
{
	struct tpm_crb *crb;
	uint8_t *ptr;
	uint64_t off, shift;
	int error = 0;

	if ((addr & (size - 1)) != 0) {
		warnx("%s: unaligned %s access @ %16lx [size = %x]", __func__,
		    (dir == MEM_F_READ) ? "read" : "write", addr, size);
		return (EINVAL);
	}

	crb = arg1;

	off = addr - TPM_CRB_ADDRESS;
	if (off > TPM_CRB_REGS_SIZE || off + size >= TPM_CRB_REGS_SIZE) {
		return (EINVAL);
	}

	shift = 8 * (off & 3);
	ptr = (uint8_t *)&crb->regs + off;

	if (dir == MEM_F_READ) {
		error = tpm_crb_mmiocpy(val, ptr, size);
		if (error)
			goto err_out;
	} else {
		switch (off & ~0x3) {
		case offsetof(struct tpm_crb_regs, loc_ctrl): {
			union tpm_crb_reg_loc_ctrl loc_ctrl;

			if ((size_t)size > sizeof(loc_ctrl))
				goto err_out;

			*val = *val << shift;
			tpm_crb_mmiocpy(&loc_ctrl, val, size);

			if (loc_ctrl.relinquish) {
				crb->regs.loc_sts.granted = false;
				crb->regs.loc_state.loc_assigned = false;
			} else if (loc_ctrl.request_access) {
				crb->regs.loc_sts.granted = true;
				crb->regs.loc_state.loc_assigned = true;
			}

			break;
		}
		case offsetof(struct tpm_crb_regs, ctrl_req): {
			union tpm_crb_reg_ctrl_req req;

			if ((size_t)size > sizeof(req))
				goto err_out;

			*val = *val << shift;
			tpm_crb_mmiocpy(&req, val, size);

			if (req.cmd_ready && !req.go_idle) {
				crb->regs.ctrl_sts.tpm_idle = false;
			} else if (!req.cmd_ready && req.go_idle) {
				crb->regs.ctrl_sts.tpm_idle = true;
			}

			break;
		}
		case offsetof(struct tpm_crb_regs, ctrl_cancel): {
			/* TODO: cancel the tpm command */
			warnx(
			    "%s: cancelling a TPM command is not implemented yet",
			    __func__);

			break;
		}
		case offsetof(struct tpm_crb_regs, int_enable):
			/* No interrupt support. Ignore writes to int_enable. */
			break;

		case offsetof(struct tpm_crb_regs, ctrl_start): {
			union tpm_crb_reg_ctrl_start start;

			if ((size_t)size > sizeof(start))
				goto err_out;

			*val = *val << shift;

			pthread_mutex_lock(&crb->mutex);
			tpm_crb_mmiocpy(&start, val, size);

			if (!start.start || crb->regs.ctrl_start.start) {
				pthread_mutex_unlock(&crb->mutex);
				break;
			}

			crb->regs.ctrl_start.start = true;

			pthread_cond_signal(&crb->cond);
			pthread_mutex_unlock(&crb->mutex);

			break;
		}
		case offsetof(struct tpm_crb_regs, cmd_size):
		case offsetof(struct tpm_crb_regs, cmd_addr_lo):
		case offsetof(struct tpm_crb_regs, cmd_addr_hi):
		case offsetof(struct tpm_crb_regs, rsp_size):
		case offsetof(struct tpm_crb_regs,
		    rsp_addr) ... offsetof(struct tpm_crb_regs, rsp_addr) +
		    4:
		case offsetof(struct tpm_crb_regs,
		    data_buffer) ... offsetof(struct tpm_crb_regs, data_buffer) +
		    TPM_CRB_DATA_BUFFER_SIZE / 4:
			/*
			 * Those fields are used to execute a TPM command. The
			 * crb_thread will access them. For that reason, we have
			 * to acquire the crb mutex in order to write them.
			 */
			pthread_mutex_lock(&crb->mutex);
			error = tpm_crb_mmiocpy(ptr, val, size);
			pthread_mutex_unlock(&crb->mutex);
			if (error)
				goto err_out;
			break;
		default:
			/*
			 * The other fields are either readonly or we do not
			 * support writing them.
			 */
			error = EINVAL;
			goto err_out;
		}
	}

	return (0);

err_out:
	warnx("%s: invalid %s @ %16lx [size = %d]", __func__,
	    dir == MEM_F_READ ? "read" : "write", addr, size);

	return (error);
}

static int
tpm_crb_modify_mmio_registration(const bool registration, void *const arg1)
{
	struct mem_range crb_mmio = {
		.name = "crb-mmio",
		.base = TPM_CRB_ADDRESS,
		.size = TPM_CRB_LOCALITIES_MAX * TPM_CRB_CONTROL_AREA_SIZE,
		.flags = MEM_F_RW,
		.arg1 = arg1,
		.handler = tpm_crb_mem_handler,
	};

	if (registration)
		return (register_mem(&crb_mmio));
	else
		return (unregister_mem(&crb_mmio));
}

static int
tpm_crb_init(void **sc, struct tpm_emul *emul, void *emul_sc,
    struct acpi_device *acpi_dev)
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

	error = acpi_device_add_res_fixed_memory32(acpi_dev, false,
	    TPM_CRB_ADDRESS, TPM_CRB_CONTROL_AREA_SIZE);
	if (error) {
		warnx("%s: failed to add acpi resources\n", __func__);
		goto err_out;
	}

	error = tpm_crb_modify_mmio_registration(true, crb);
	if (error) {
		warnx("%s: failed to register crb mmio", __func__);
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
	int error;

	if (sc == NULL) {
		return;
	}

	crb = sc;

	crb->closing = true;
	pthread_cond_signal(&crb->cond);
	pthread_join(crb->thread, NULL);

	pthread_cond_destroy(&crb->cond);
	pthread_mutex_destroy(&crb->mutex);

	error = tpm_crb_modify_mmio_registration(false, NULL);
	assert(error == 0);

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
