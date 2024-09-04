/*
 * Copyright (c) 2024, Broadcom. All rights reserved.  The term
 * Broadcom refers to Broadcom Limited and/or its subsidiaries.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/mman.h>

#include <malloc.h>
#include <string.h>

#include "main.h"

void bnxt_re_free_mem(struct bnxt_re_mem *mem)
{
	if (mem->va_head) {
		ibv_dofork_range(mem->va_head, mem->size);
		munmap(mem->va_head, mem->size);
	}

	free(mem);
}

void *bnxt_re_alloc_mem(size_t size, uint32_t pg_size)
{
	struct bnxt_re_mem *mem;

	mem = calloc(1, sizeof(*mem));
	if (!mem)
		return NULL;

	size = get_aligned(size, pg_size);
	mem->size = size;
	mem->va_head = mmap(NULL, size, PROT_READ | PROT_WRITE,
			    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (mem->va_head == MAP_FAILED)
		goto bail;

	if (ibv_dontfork_range(mem->va_head, size))
		goto unmap;

	mem->head = 0;
	mem->tail = 0;
	mem->va_tail = (void *)((char *)mem->va_head + size);
	return mem;
unmap:
	munmap(mem->va_head, size);
bail:
	free(mem);
	return NULL;
}

void *bnxt_re_get_obj(struct bnxt_re_mem *mem, size_t req)
{
	void *va;

	if ((mem->size - mem->tail - req) < mem->head)
		return NULL;
	mem->tail += req;
	va = (void *)((char *)mem->va_tail - mem->tail);
	return va;
}

void *bnxt_re_get_ring(struct bnxt_re_mem *mem, size_t req)
{
	void *va;

	if ((mem->head + req) > (mem->size - mem->tail))
		return NULL;
	va = (void *)((char *)mem->va_head + mem->head);
	mem->head += req;
	return va;
}
