/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2016 Jakub Klama <jceel@FreeBSD.org>.
 * Copyright (c) 2018 Alexander Motin <mav@FreeBSD.org>
 * Copyright (c) 2026 Hans Rosenfeld
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/uio.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "iov.h"


/*
 * Given an iovec and an offset, split the iovec into two at the offset and
 * return a pointer to the beginning of the second iovec.
 *
 * The caller is responsible for providing an extra iovec in the array for the
 * split.  That is, there should be space for *niov1 + 1 iovecs in the array.
 */
struct iovec *
split_iov(struct iovec *iov, size_t *niov1, size_t offset, size_t *niov2)
{
	size_t count, resid;

	/* Find the iovec entry that contains the offset. */
	resid = offset;
	for (count = 0; count < *niov1; count++) {
		if (resid < iov[count].iov_len)
			break;
		resid -= iov[count].iov_len;
	}

	if (resid == 0 || count == *niov1) {
		/* Easy case, we don't have to split an iovec entry. */
		*niov2 = *niov1 - count;
		*niov1 = count;
		if (*niov2 == 0)
			return (NULL);
		return (&iov[count]);
	}

	/* The entry iov[count] needs to be split. */
	*niov1 = count + 1;
	*niov2 = *niov1 - count;
	memmove(&iov[count + 1], &iov[count], sizeof(struct iovec) * (*niov2));
	iov[count].iov_len = resid;
	iov[count + 1].iov_base = (char *)iov[count].iov_base + resid;
	iov[count + 1].iov_len -= resid;
	return (&iov[count + 1]);
}

size_t
count_iov(const struct iovec *iov, size_t niov)
{
	size_t total = 0;
	size_t i;

	for (i = 0; i < niov; i++) {
		assert(total <= SIZE_MAX - iov[i].iov_len);
		total += iov[i].iov_len;
	}

	return (total);
}

bool
check_iov_len(const struct iovec *iov, size_t niov, size_t len)
{
	size_t total = 0;
	size_t i;

	for (i = 0; i < niov; i++) {
		assert(total <= SIZE_MAX - iov[i].iov_len);
		total += iov[i].iov_len;
		if (total >= len)
			return (true);
	}

	return (false);
}

size_t
iov_to_buf(const struct iovec *iov, size_t niov, void **buf)
{
	size_t ptr, total;
	size_t i;

	total = count_iov(iov, niov);
	*buf = reallocf(*buf, total);
	if (*buf == NULL)
		return (0);

	for (i = 0, ptr = 0; i < niov; i++) {
		memcpy((uint8_t *)*buf + ptr, iov[i].iov_base, iov[i].iov_len);
		ptr += iov[i].iov_len;
	}

	return (total);
}

size_t
buf_to_iov(const void *buf, size_t buflen, const struct iovec *iov, size_t niov)
{
	size_t off = 0, len;
	size_t  i;

	for (i = 0; i < niov && off < buflen; i++) {
		len = MIN(iov[i].iov_len, buflen - off);
		memcpy(iov[i].iov_base, (const uint8_t *)buf + off, len);
		off += len;
	}

	return (off);
}
