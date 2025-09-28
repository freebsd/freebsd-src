/*-
 * Copyright (c) 2018 Hans Petter Selasky
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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

#include <sys/queue.h>
#include <sys/types.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#include "int.h"

int
vring_alloc(struct virtual_ring *pvr, size_t size)
{

	if (pvr->buf_start != NULL)
		return (EBUSY);
	pvr->buf_start = malloc(size);
	if (pvr->buf_start == NULL)
		return (ENOMEM);
	pvr->pos_read = 0;
	pvr->total_size = size;
	pvr->len_write = 0;
	return (0);
}

void
vring_free(struct virtual_ring *pvr)
{

	if (pvr->buf_start != NULL) {
		free(pvr->buf_start);
		pvr->buf_start = NULL;
	}
}

void
vring_reset(struct virtual_ring *pvr)
{
	pvr->pos_read = 0;
	pvr->len_write = 0;
}

void
vring_get_read(struct virtual_ring *pvr, uint8_t **pptr, size_t *plen)
{
	uint32_t delta;

	if (pvr->buf_start == NULL) {
		*pptr = NULL;
		*plen = 0;
		return;
	}
	delta = pvr->total_size - pvr->pos_read;
	if (delta > pvr->len_write)
		delta = pvr->len_write;

	*pptr = pvr->buf_start + pvr->pos_read;
	*plen = delta;
}

void
vring_get_write(struct virtual_ring *pvr, uint8_t **pptr, size_t *plen)
{
	uint32_t delta;
	uint32_t len_read;
	uint32_t pos_write;

	if (pvr->buf_start == NULL) {
		*pptr = NULL;
		*plen = 0;
		return;
	}
	pos_write = pvr->pos_read + pvr->len_write;
	if (pos_write >= pvr->total_size)
		pos_write -= pvr->total_size;

	len_read = pvr->total_size - pvr->len_write;

	delta = pvr->total_size - pos_write;
	if (delta > len_read)
		delta = len_read;

	*pptr = pvr->buf_start + pos_write;
	*plen = delta;
}

void
vring_inc_read(struct virtual_ring *pvr, size_t len)
{

	pvr->pos_read += len;
	pvr->len_write -= len;

	/* check for wrap-around */
	if (pvr->pos_read == pvr->total_size)
		pvr->pos_read = 0;
}

void
vring_inc_write(struct virtual_ring *pvr, size_t len)
{

	pvr->len_write += len;
}

size_t
vring_total_read_len(struct virtual_ring *pvr)
{

	return (pvr->len_write);
}

size_t
vring_total_write_len(struct virtual_ring *pvr)
{

	return (pvr->total_size - pvr->len_write);
}

size_t
vring_write_linear(struct virtual_ring *pvr, const uint8_t *src, size_t total)
{
	uint8_t *buf_ptr;
	size_t buf_len;
	size_t sum = 0;

	while (total != 0) {
		vring_get_write(pvr, &buf_ptr, &buf_len);
		if (buf_len == 0)
			break;
		if (buf_len > total)
			buf_len = total;
		memcpy(buf_ptr, src, buf_len);
		vring_inc_write(pvr, buf_len);
		src += buf_len;
		sum += buf_len;
		total -= buf_len;
	}
	return (sum);
}

size_t
vring_read_linear(struct virtual_ring *pvr, uint8_t *dst, size_t total)
{
	uint8_t *buf_ptr;
	size_t buf_len;
	size_t sum = 0;

	if (total > vring_total_read_len(pvr))
		return (0);

	while (total != 0) {
		vring_get_read(pvr, &buf_ptr, &buf_len);
		if (buf_len == 0)
			break;
		if (buf_len > total)
			buf_len = total;
		memcpy(dst, buf_ptr, buf_len);
		vring_inc_read(pvr, buf_len);
		dst += buf_len;
		sum += buf_len;
		total -= buf_len;
	}
	return (sum);
}

size_t
vring_write_zero(struct virtual_ring *pvr, size_t total)
{
	uint8_t *buf_ptr;
	size_t buf_len;
	size_t sum = 0;

	while (total != 0) {
		vring_get_write(pvr, &buf_ptr, &buf_len);
		if (buf_len == 0)
			break;
		if (buf_len > total)
			buf_len = total;
		memset(buf_ptr, 0, buf_len);
		vring_inc_write(pvr, buf_len);
		sum += buf_len;
		total -= buf_len;
	}
	return (sum);
}
