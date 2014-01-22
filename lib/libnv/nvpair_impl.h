/*-
 * Copyright (c) 2009-2013 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Pawel Jakub Dawidek under sponsorship from
 * the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef	_NVPAIR_IMPL_H_
#define	_NVPAIR_IMPL_H_

#include <sys/queue.h>

#include <stdint.h>

#include "nv.h"

TAILQ_HEAD(nvl_head, nvpair);

void nvpair_assert(const nvpair_t *nvp);
const nvlist_t *nvpair_nvlist(const nvpair_t *nvp);
nvpair_t *nvpair_next(const nvpair_t *nvp);
nvpair_t *nvpair_prev(const nvpair_t *nvp);
void nvpair_insert(struct nvl_head *head, nvpair_t *nvp, nvlist_t *nvl);
void nvpair_remove(struct nvl_head *head, nvpair_t *nvp, const nvlist_t *nvl);
size_t nvpair_header_size(void);
size_t nvpair_size(const nvpair_t *nvp);
unsigned char *nvpair_pack(nvpair_t *nvp, unsigned char *ptr, int64_t *fdidxp,
    size_t *leftp);
const unsigned char *nvpair_unpack(int flags, const unsigned char *ptr,
    size_t *leftp, const int *fds, size_t nfds, nvpair_t **nvpp);
void nvpair_free_structure(nvpair_t *nvp);
const char *nvpair_type_string(int type);

#endif	/* !_NVPAIR_IMPL_H_ */
