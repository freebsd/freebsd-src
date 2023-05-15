/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Beckhoff Automation GmbH & Co. KG
 * Author: Corvin Köhne <corvink@FreeBSD.org>
 */

#pragma once

#include <sys/linker_set.h>

#include "config.h"

struct tpm_device;

struct tpm_emul {
	const char *name;

	int (*init)(void **sc, nvlist_t *nvl);
	void (*deinit)(void *sc);
};
#define TPM_EMUL_SET(x) DATA_SET(tpm_emul_set, x)
