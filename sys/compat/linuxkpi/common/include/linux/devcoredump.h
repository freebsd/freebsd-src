/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 The FreeBSD Foundation
 *
 * This software was developed by Björn Zeeb under sponsorship from
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
 *
 * $FreeBSD$
 */

#ifndef	_LINUXKPI_LINUX_DEVCOREDUMP_H
#define	_LINUXKPI_LINUX_DEVCOREDUMP_H

#include <linux/slab.h>
#include <linux/scatterlist.h>
#include <linux/device.h>

static inline void
_lkpi_dev_coredumpsg_free(struct scatterlist *table)
{
	struct scatterlist *iter;
	struct page *p;
	int i;

	iter = table;
	for_each_sg(table, iter, sg_nents(table), i) {
		p = sg_page(iter);
		if (p)
			__free_page(p);
	}

	/* XXX what about chained tables? */
	kfree(table);
}

static inline void
dev_coredumpv(struct device *dev __unused, void *data, size_t datalen __unused,
    gfp_t gfp __unused)
{

	/* UNIMPLEMENTED */
	vfree(data);
}

static inline void
dev_coredumpsg(struct device *dev __unused, struct scatterlist *table,
    size_t datalen __unused, gfp_t gfp __unused)
{

	/* UNIMPLEMENTED */
	_lkpi_dev_coredumpsg_free(table);
}

#endif	/* _LINUXKPI_LINUX_DEVCOREDUMP_H */
