/*
 * Copyright (c) 2005 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <string.h>

struct {
	DATA_TYPE aligned;
	char _;
	char misaligned[sizeof(DATA_TYPE)];
} data;

int
main()
{
	DATA_TYPE *aligned = &data.aligned;
	DATA_TYPE *misaligned = (DATA_TYPE *)data.misaligned;
	DATA_TYPE value = DATA_VALUE;

	/* Set PSR.ac. */
	asm volatile("sum 8");

#if defined(TEST_STORE)
	/* Misaligned store. */
	*misaligned = value;
	memcpy(aligned, misaligned, sizeof(DATA_TYPE));
#elif defined(TEST_LOAD)
	memcpy(misaligned, &value, sizeof(DATA_TYPE));
	/* Misaligned load. */
	*aligned = *misaligned;
#else
#error Define TEST_LOAD or TEST_STORE
#endif

	return (*aligned == value) ? 0 : 1;
}
