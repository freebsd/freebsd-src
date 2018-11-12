/*
 * Copyright (c) 2013,  Mellanox Technologies. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#if !defined(PEER_MEM_H)
#define PEER_MEM_H

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/scatterlist.h>
#include <linux/mutex.h>


#define IB_PEER_MEMORY_NAME_MAX 64
#define IB_PEER_MEMORY_VER_MAX 16

struct peer_memory_client {
	char	name[IB_PEER_MEMORY_NAME_MAX];
	char	version[IB_PEER_MEMORY_VER_MAX];
	/* acquire return code: 1 mine, 0 - not mine */
	int (*acquire) (unsigned long addr, size_t size, void *peer_mem_private_data,
					char *peer_mem_name, void **client_context);
	int (*get_pages) (unsigned long addr,
			  size_t size, int write, int force,
			  struct sg_table *sg_head,
			  void *client_context, void *core_context);
	int (*dma_map) (struct sg_table *sg_head, void *client_context,
			struct device *dma_device, int dmasync, int *nmap);
	int (*dma_unmap) (struct sg_table *sg_head, void *client_context,
			   struct device  *dma_device);
	void (*put_pages) (struct sg_table *sg_head, void *client_context);
	unsigned long (*get_page_size) (void *client_context);
	void (*release) (void *client_context);

};

typedef int (*invalidate_peer_memory)(void *reg_handle,
					  void *core_context);

void *ib_register_peer_memory_client(struct peer_memory_client *peer_client,
					  invalidate_peer_memory *invalidate_callback);
void ib_unregister_peer_memory_client(void *reg_handle);

#endif
