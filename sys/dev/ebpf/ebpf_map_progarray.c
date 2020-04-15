/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Ryan Stone
 * All rights reserved.
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/ebpf.h>

#include <sys/ebpf.h>
#include <dev/ebpf/ebpf_map.h>
#include <dev/ebpf/ebpf_map_array.h>
#include <dev/ebpf/ebpf_map_freebsd.h>
#include <dev/ebpf/ebpf_dev_platform.h>
#include <dev/ebpf/ebpf_dev_freebsd.h>

static int
progarray_map_init(struct ebpf_map *map, struct ebpf_map_attr *attr)
{

	if (attr->value_size != sizeof(int)) {
		return (EINVAL);
	}

	return (array_map_init(map, attr));
}

static int
progarray_map_update_elem(struct ebpf_map *map, void *key,
    void *value, uint64_t flags)
{
	int error, fd;
	ebpf_file *fp;
	struct thread *td;

	td = curthread;
	fd = *(int*)value;
	fp = NULL;

	/* Test that the FD currently points to a program.*/
	error = ebpf_fd_to_program(td, fd, &fp, NULL);
	if (error != 0) {
		goto out;
	}

	error = array_map_update_elem(map, key, value, flags);

out:
	if (fp != NULL) {
		ebpf_fdrop(fp, td);
	}

	return (error);
}

const struct ebpf_map_type emt_progarray = {
	.name = "progarray",
	.ops = {
		.init = progarray_map_init,
		.update_elem = progarray_map_update_elem,
		.lookup_elem = array_map_lookup_elem,
		.delete_elem = array_map_delete_elem,
		.update_elem_from_user = progarray_map_update_elem,
		.lookup_elem_from_user = array_map_lookup_elem_from_user,
		.delete_elem_from_user = array_map_delete_elem,
		.get_next_key_from_user = array_map_get_next_key,
		.deinit = array_map_deinit
	}
};

