/*-
 * Copyright (c) 2016 The Go Authors. All rights reserved.
 * Copyright (c) 2024 Robert Clausecker <fuz@freebsd.org>
 *
 * Adapted from Go's crypto/sha1/sha1block_amd64.go.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *   * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <machine/specialreg.h>
#include <sha.h>
#include <x86/ifunc.h>

extern void _libmd_sha1block_scalar(SHA1_CTX *, const void *, size_t);
extern void _libmd_sha1block_avx2(SHA1_CTX *, const void *, size_t);
extern void _libmd_sha1block_shani(SHA1_CTX *, const void *, size_t);
static void sha1block_avx2_wrapper(SHA1_CTX *, const void *, size_t);

#define AVX2_STDEXT_NEEDED \
	(CPUID_STDEXT_BMI1 | CPUID_STDEXT_AVX2 | CPUID_STDEXT_BMI2)

DEFINE_UIFUNC(, void, sha1_block, (SHA1_CTX *, const void *, size_t))
{
	if (cpu_stdext_feature & CPUID_STDEXT_SHA)
		return (_libmd_sha1block_shani);
	if ((cpu_stdext_feature & AVX2_STDEXT_NEEDED) == AVX2_STDEXT_NEEDED)
		return (sha1block_avx2_wrapper);
	else
		return (_libmd_sha1block_scalar);
}

static void
sha1block_avx2_wrapper(SHA1_CTX *c, const void *data, size_t len)
{
	if (len >= 256) {
		/*
		 * sha1block_avx2 calculates sha1 for 2 block per iteration.
                 * It also interleaves the precalculation for next the block.
                 * So it may read up-to 192 bytes past the end of p.
                 * We may add checks inside sha1block_avx2, but this will
                 * just turn it into a copy of sha1block_scalar,
                 * so call it directly, instead.
		 */
		size_t safe_len = len - 128;

		if (safe_len % 128 != 0)
			safe_len -= 64;

		_libmd_sha1block_avx2(c, data, safe_len);
		_libmd_sha1block_scalar(c, data + safe_len, len - safe_len);
	} else
		_libmd_sha1block_scalar(c, data, len);
}
