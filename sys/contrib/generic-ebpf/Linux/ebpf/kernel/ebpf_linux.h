/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019, Yutaro Hayakawa
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
 */

/*-
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Copyright (c) 2019 Yutaro Hayakawa
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/cpumask.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/jhash.h>
#include <linux/rcupdate.h>
#include <linux/rculist.h>
#include <asm/byteorder.h>

#define UINT64_MAX U64_MAX
#define UINT32_MAX U32_MAX
#define INT32_MAX S32_MAX
#define INT32_MIN S32_MIN

#define htole16(x) cpu_to_le16(x)
#define htole32(x) cpu_to_le32(x)
#define htole64(x) cpu_to_le64(x)

#define htobe16(x) cpu_to_be16(x)
#define htobe32(x) cpu_to_be32(x)
#define htobe64(x) cpu_to_be64(x)

#define ENOTSUP EOPNOTSUPP

typedef struct rcu_head ebpf_epoch_context;
typedef struct mutex ebpf_mtx;
typedef raw_spinlock_t ebpf_spinmtx;

#define ebpf_assert(_expr) BUG_ON(!(_expr));

#define EBPF_EPOCH_LIST_ENTRY(_type) struct hlist_node
#define EBPF_EPOCH_LIST_EMPTY(_type) hlist_empty(_type)
#define EBPF_EPOCH_LIST_FIRST(_headp, _type, _name) \
  hlist_entry(hlist_first_rcu(_headp), _type, _name)
#define EBPF_EPOCH_LIST_HEAD(_name, _type) struct hlist_head
#define EBPF_EPOCH_LIST_INIT(_headp) INIT_HLIST_HEAD(_headp)
#define EBPF_EPOCH_LIST_FOREACH(_var, _head, _name) hlist_for_each_entry_rcu(_var, _head, _name)
#define EBPF_EPOCH_LIST_INSERT_HEAD(_head, _elem, _name) hlist_add_head_rcu(&_elem->_name, _head)
#define EBPF_EPOCH_LIST_REMOVE(_elem, _name) hlist_del_rcu(&_elem->_name)
#define EBPF_EPOCH_LIST_NEXT(_elem, _name) \
  hlist_entry(hlist_next_rcu(&_elem->_name), typeof(*_elem), _name)
