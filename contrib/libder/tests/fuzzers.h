/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <stdint.h>

enum stream_type {
	STREAM_FD = 0,		/* read_fd() type */
	STREAM_FILE = 1,	/* read_file() type */
	STREAM_PLAIN = 2,

	STREAM_END
} __attribute__((packed));

enum stream_buffer {
	BUFFER_NONE = 0,
	BUFFER_FULL = 1,

	BUFFER_END,
} __attribute__((packed));

struct fuzz_params {
	enum stream_type	 type;
	enum stream_buffer	 buftype;

#define	PARAM_PAD_START	_pad0
	uint8_t			 strict;
	uint8_t			 _pad0[5];

	/* Give me plenty of padding. */
	uint64_t		 padding[3];
};

_Static_assert(sizeof(struct fuzz_params) == 32,
    "fuzz_params ABI broken, will invalidate CORPUS");

