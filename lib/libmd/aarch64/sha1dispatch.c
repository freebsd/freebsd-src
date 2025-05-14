/*-
 * Copyright (c) 2024 Robert Clausecker <fuz@freebsd.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <machine/ifunc.h>
#include <sha.h>
#include <sys/auxv.h>

extern void _libmd_sha1block_scalar(SHA1_CTX *, const void *, size_t);
extern void _libmd_sha1block_sha1(SHA1_CTX *, const void *, size_t);

DEFINE_IFUNC(, void, sha1_block, (SHA1_CTX *, const void *, size_t))
{
	unsigned long hwcap = 0;

	elf_aux_info(AT_HWCAP, &hwcap, sizeof(hwcap));

	if (hwcap & HWCAP_SHA1)
		return (_libmd_sha1block_sha1);
	else
		return (_libmd_sha1block_scalar);
}
