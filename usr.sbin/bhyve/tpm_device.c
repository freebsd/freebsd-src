/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Beckhoff Automation GmbH & Co. KG
 * Author: Corvin Köhne <corvink@FreeBSD.org>
 */

#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <vmmapi.h>

#include "config.h"
#include "tpm_device.h"

struct tpm_device {
	struct vmctx *vm_ctx;
};

void
tpm_device_destroy(struct tpm_device *const dev)
{
	if (dev == NULL)
		return;

	free(dev);
}

int
tpm_device_create(struct tpm_device **const new_dev, struct vmctx *const vm_ctx,
    nvlist_t *const nvl)
{
	struct tpm_device *dev = NULL;
	const char *value;
	int error;

	if (new_dev == NULL || vm_ctx == NULL) {
		error = EINVAL;
		goto err_out;
	}

	value = get_config_value_node(nvl, "version");
	if (value == NULL || strcmp(value, "2.0")) {
		warnx("%s: unsupported tpm version %s", __func__, value);
		error = EINVAL;
		goto err_out;
	}

	dev = calloc(1, sizeof(*dev));
	if (dev == NULL) {
		error = ENOMEM;
		goto err_out;
	}

	dev->vm_ctx = vm_ctx;

	*new_dev = dev;

	return (0);

err_out:
	tpm_device_destroy(dev);

	return (error);
}
