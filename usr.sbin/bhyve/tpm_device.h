/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Beckhoff Automation GmbH & Co. KG
 * Author: Corvin Köhne <corvink@FreeBSD.org>
 */

#pragma once

#include <vmmapi.h>

#include "config.h"

struct tpm_device;

int tpm_device_create(struct tpm_device **new_dev, struct vmctx *vm_ctx,
    nvlist_t *nvl);
void tpm_device_destroy(struct tpm_device *dev);

int init_tpm(struct vmctx *ctx);
