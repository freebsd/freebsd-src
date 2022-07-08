/*
 * Copyright 2022, Netflix, Inc
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Hack for aarch64... There's no way to tell it omit the SIMD
 * versions, so we fake it here.
 */
#include "blake3_impl.c"

static inline boolean_t blake3_is_not_supported(void)
{
	return (B_FALSE);
}

const blake3_impl_ops_t blake3_sse2_impl = {
	.is_supported = blake3_is_not_supported,
	.degree = 4,
	.name = "fakesse2"
};

const blake3_impl_ops_t blake3_sse41_impl = {
	.is_supported = blake3_is_not_supported,
	.degree = 4,
	.name = "fakesse41"
};
