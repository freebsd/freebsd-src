/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#include <sys/param.h>
#include <sys/linker.h>
#include <sys/nv.h>
#include <sys/time.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libnvmf.h>
#include <string.h>

#include <cam/ctl/ctl.h>
#include <cam/ctl/ctl_io.h>
#include <cam/ctl/ctl_ioctl.h>

#include "internal.h"

static int ctl_fd = -1;
static int ctl_port;

static void
open_ctl(void)
{
	if (ctl_fd > 0)
		return;

	ctl_fd = open(CTL_DEFAULT_DEV, O_RDWR);
	if (ctl_fd == -1 && errno == ENOENT) {
		if (kldload("ctl") == -1)
			err(1, "Failed to load ctl.ko");
		ctl_fd = open(CTL_DEFAULT_DEV, O_RDWR);
	}
	if (ctl_fd == -1)
		err(1, "Failed to open %s", CTL_DEFAULT_DEV);
}

void
init_ctl_port(const char *subnqn, const struct nvmf_association_params *params)
{
	char result_buf[256];
	struct ctl_port_entry entry;
	struct ctl_req req;
	nvlist_t *nvl;

	open_ctl();

	nvl = nvlist_create(0);

	nvlist_add_string(nvl, "subnqn", subnqn);

	/* XXX: Hardcoded in discovery.c */
	nvlist_add_stringf(nvl, "portid", "%u", 1);

	nvlist_add_stringf(nvl, "max_io_qsize", "%u", params->max_io_qsize);

	memset(&req, 0, sizeof(req));
	strlcpy(req.driver, "nvmf", sizeof(req.driver));
	req.reqtype = CTL_REQ_CREATE;
	req.args = nvlist_pack(nvl, &req.args_len);
	if (req.args == NULL)
		errx(1, "Failed to pack nvlist for CTL_PORT/CTL_REQ_CREATE");
	req.result = result_buf;
	req.result_len = sizeof(result_buf);
	if (ioctl(ctl_fd, CTL_PORT_REQ, &req) != 0)
		err(1, "ioctl(CTL_PORT/CTL_REQ_CREATE)");
	if (req.status == CTL_LUN_ERROR)
		errx(1, "Failed to create CTL port: %s", req.error_str);
	if (req.status != CTL_LUN_OK)
		errx(1, "Failed to create CTL port: %d", req.status);

	nvlist_destroy(nvl);
	nvl = nvlist_unpack(result_buf, req.result_len, 0);
	if (nvl == NULL)
		errx(1, "Failed to unpack nvlist from CTL_PORT/CTL_REQ_CREATE");

	ctl_port = nvlist_get_number(nvl, "port_id");
	nvlist_destroy(nvl);

	memset(&entry, 0, sizeof(entry));
	entry.targ_port = ctl_port;
	if (ioctl(ctl_fd, CTL_ENABLE_PORT, &entry) != 0)
		errx(1, "ioctl(CTL_ENABLE_PORT)");
}

void
shutdown_ctl_port(const char *subnqn)
{
	struct ctl_req req;
	nvlist_t *nvl;

	open_ctl();

	nvl = nvlist_create(0);

	nvlist_add_string(nvl, "subnqn", subnqn);

	memset(&req, 0, sizeof(req));
	strlcpy(req.driver, "nvmf", sizeof(req.driver));
	req.reqtype = CTL_REQ_REMOVE;
	req.args = nvlist_pack(nvl, &req.args_len);
	if (req.args == NULL)
		errx(1, "Failed to pack nvlist for CTL_PORT/CTL_REQ_REMOVE");
	if (ioctl(ctl_fd, CTL_PORT_REQ, &req) != 0)
		err(1, "ioctl(CTL_PORT/CTL_REQ_REMOVE)");
	if (req.status == CTL_LUN_ERROR)
		errx(1, "Failed to remove CTL port: %s", req.error_str);
	if (req.status != CTL_LUN_OK)
		errx(1, "Failed to remove CTL port: %d", req.status);

	nvlist_destroy(nvl);
}

void
ctl_handoff_qpair(struct nvmf_qpair *qp,
    const struct nvmf_fabric_connect_cmd *cmd,
    const struct nvmf_fabric_connect_data *data)
{
	struct ctl_nvmf req;
	int error;

	memset(&req, 0, sizeof(req));
	req.type = CTL_NVMF_HANDOFF;
	error = nvmf_handoff_controller_qpair(qp, &req.data.handoff);
	if (error != 0) {
		warnc(error, "Failed to prepare qpair for handoff");
		return;
	}

	req.data.handoff.cmd = cmd;
	req.data.handoff.data = data;
	if (ioctl(ctl_fd, CTL_NVMF, &req) != 0)
		warn("ioctl(CTL_NVMF/CTL_NVMF_HANDOFF)");
}
