/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Beckhoff Automation GmbH & Co. KG
 * Author: Corvin Köhne <corvink@FreeBSD.org>
 */

#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc_np.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "tpm_device.h"
#include "tpm_emul.h"

struct tpm_passthru {
	int fd;
};

struct tpm_resp_hdr {
	uint16_t tag;
	uint32_t len;
	uint32_t errcode;
} __packed;

static int
tpm_passthru_init(void **sc, nvlist_t *nvl)
{
	struct tpm_passthru *tpm;
	const char *path;
	
	tpm = calloc(1, sizeof(struct tpm_passthru));
	if (tpm == NULL) {
		warnx("%s: failed to allocate tpm passthru", __func__);
		return (ENOMEM);
	}

	path = get_config_value_node(nvl, "path");
	tpm->fd = open(path, O_RDWR);
	if (tpm->fd < 0) {
		warnx("%s: unable to open tpm device \"%s\"", __func__, path);
		return (ENOENT);
	}

	*sc = tpm;

	return (0);
}

static int
tpm_passthru_execute_cmd(void *sc, void *cmd, uint32_t cmd_size, void *rsp,
    uint32_t rsp_size)
{
	struct tpm_passthru *tpm;
	ssize_t len;

	if (rsp_size < (ssize_t)sizeof(struct tpm_resp_hdr)) {
		warn("%s: rsp_size of %u is too small", __func__, rsp_size);
		return (EINVAL);
	}

	tpm = sc;

	len = write(tpm->fd, cmd, cmd_size);
	if (len != cmd_size) {
		warn("%s: cmd write failed (bytes written: %zd / %d)", __func__,
		    len, cmd_size);
		return (EFAULT);
	}

	len = read(tpm->fd, rsp, rsp_size);
	if (len < (ssize_t)sizeof(struct tpm_resp_hdr)) {
		warn("%s: rsp read failed (bytes read: %zd / %d)", __func__,
		    len, rsp_size);
		return (EFAULT);
	}

	return (0);
}

static void
tpm_passthru_deinit(void *sc)
{
	struct tpm_passthru *tpm;

	tpm = sc;
	if (tpm == NULL)
		return;

	if (tpm->fd >= 0)
		close(tpm->fd);

	free(tpm);
}

static const struct tpm_emul tpm_emul_passthru = {
	.name = "passthru",
	.init = tpm_passthru_init,
	.deinit = tpm_passthru_deinit,
	.execute_cmd = tpm_passthru_execute_cmd,
};
TPM_EMUL_SET(tpm_emul_passthru);
