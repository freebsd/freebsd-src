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
};
TPM_EMUL_SET(tpm_emul_passthru);
