/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Netflix, Inc.
 */

#ifndef _SYS_TPMEVENTLOG_H_
#define _SYS_TPMEVENTLOG_H_

#include <sys/types.h>

/* Loader metadata; the device exports only the unmodified log bytes. */
#define TPM_EVENTLOG_VERSION	1
#define TPM_EVENTLOG_TCG12	1
#define TPM_EVENTLOG_TCG2		2
#define TPM_EVENTLOG_TRUNCATED	0x00000001
#define TPM_EVENTLOG_MAX_SIZE	(1024 * 1024)

struct tpm_eventlog_header {
	uint32_t	version;
	uint32_t	format;
	uint32_t	length;
	uint32_t	flags;
	uint8_t		log[];
};

#endif /* _SYS_TPMEVENTLOG_H_ */
