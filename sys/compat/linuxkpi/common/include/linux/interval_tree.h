/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 Vladimir Kondratyev <wulf@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 */

#ifndef _LINUX_INTERVAL_TREE_H
#define _LINUX_INTERVAL_TREE_H

#include <linux/rbtree.h>

struct interval_tree_node {
	struct rb_node rb;
	unsigned long start;
	unsigned long last;
};

#define	interval_tree_iter_first(...)	\
	lkpi_interval_tree_iter_first(__VA_ARGS__)
#define	interval_tree_iter_next(...)	\
	lkpi_interval_tree_iter_next(__VA_ARGS__)
#define	interval_tree_insert(...)	lkpi_interval_tree_insert(__VA_ARGS__)
#define	interval_tree_remove(...)	lkpi_interval_tree_remove(__VA_ARGS__)

struct interval_tree_node *lkpi_interval_tree_iter_first(
    struct rb_root_cached *, unsigned long, unsigned long);
struct interval_tree_node *lkpi_interval_tree_iter_next(
    struct interval_tree_node *, unsigned long, unsigned long);
void lkpi_interval_tree_insert(struct interval_tree_node *,
    struct rb_root_cached *);
void lkpi_interval_tree_remove(struct interval_tree_node *,
    struct rb_root_cached *);

#endif	/* _LINUX_INTERVAL_TREE_H */
