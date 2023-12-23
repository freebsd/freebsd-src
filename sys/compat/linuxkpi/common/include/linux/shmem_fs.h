/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013-2018 Mellanox Technologies, Ltd.
 * All rights reserved.
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
#ifndef	_LINUXKPI_LINUX_SHMEM_FS_H_
#define	_LINUXKPI_LINUX_SHMEM_FS_H_

#include <linux/pagemap.h>

/* Shared memory support */
struct page *linux_shmem_read_mapping_page_gfp(vm_object_t obj, int pindex,
    gfp_t gfp);
struct linux_file *linux_shmem_file_setup(const char *name, loff_t size,
    unsigned long flags);
void linux_shmem_truncate_range(vm_object_t obj, loff_t lstart,
    loff_t lend);

#define	shmem_read_mapping_page(...) \
  linux_shmem_read_mapping_page_gfp(__VA_ARGS__, 0)

#define	shmem_read_mapping_page_gfp(...) \
  linux_shmem_read_mapping_page_gfp(__VA_ARGS__)

#define	shmem_file_setup(...) \
  linux_shmem_file_setup(__VA_ARGS__)

#define	shmem_truncate_range(...) \
  linux_shmem_truncate_range(__VA_ARGS__)

#endif /* _LINUXKPI_LINUX_SHMEM_FS_H_ */
