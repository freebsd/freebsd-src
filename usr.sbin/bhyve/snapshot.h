/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2016 Flavius Anton
 * Copyright (c) 2016 Mihai Tiganus
 * Copyright (c) 2016-2019 Mihai Carabas
 * Copyright (c) 2017-2019 Darius Mihai
 * Copyright (c) 2017-2019 Elena Mihailescu
 * Copyright (c) 2018-2019 Sergiu Weisz
 * All rights reserved.
 * The bhyve-snapshot feature was developed under sponsorships
 * from Matthew Grooms.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
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

#ifndef _BHYVE_SNAPSHOT_
#define _BHYVE_SNAPSHOT_

#include <machine/vmm_snapshot.h>
#include <libxo/xo.h>
#include <ucl.h>

#define BHYVE_RUN_DIR "/var/run/bhyve/"
#define MAX_SNAPSHOT_FILENAME PATH_MAX

struct vmctx;

struct restore_state {
	int kdata_fd;
	int vmmem_fd;

	void *kdata_map;
	size_t kdata_len;

	size_t vmmem_len;

	struct ucl_parser *meta_parser;
	ucl_object_t *meta_root_obj;
};

struct checkpoint_thread_info {
	struct vmctx *ctx;
	int socket_fd;
};

typedef int (*vm_snapshot_dev_cb)(struct vm_snapshot_meta *);
typedef int (*vm_pause_dev_cb) (const char *);
typedef int (*vm_resume_dev_cb) (const char *);

struct vm_snapshot_dev_info {
	const char *dev_name;		/* device name */
	vm_snapshot_dev_cb snapshot_cb;	/* callback for device snapshot */
	vm_pause_dev_cb pause_cb;	/* callback for device pause */
	vm_resume_dev_cb resume_cb;	/* callback for device resume */
};

struct vm_snapshot_kern_info {
	const char *struct_name;	/* kernel structure name*/
	enum snapshot_req req;		/* request type */
};

void destroy_restore_state(struct restore_state *rstate);

const char *lookup_vmname(struct restore_state *rstate);
int lookup_memflags(struct restore_state *rstate);
size_t lookup_memsize(struct restore_state *rstate);
int lookup_guest_ncpus(struct restore_state *rstate);

void checkpoint_cpu_add(int vcpu);
void checkpoint_cpu_resume(int vcpu);
void checkpoint_cpu_suspend(int vcpu);

int restore_vm_mem(struct vmctx *ctx, struct restore_state *rstate);
int vm_restore_kern_structs(struct vmctx *ctx, struct restore_state *rstate);

int vm_restore_devices(struct restore_state *rstate);
int vm_pause_devices(void);
int vm_resume_devices(void);

int get_checkpoint_msg(int conn_fd, struct vmctx *ctx);
void *checkpoint_thread(void *param);
int init_checkpoint_thread(struct vmctx *ctx);
void init_snapshot(void);

int load_restore_file(const char *filename, struct restore_state *rstate);

int vm_snapshot_guest2host_addr(struct vmctx *ctx, void **addrp, size_t len,
    bool restore_null, struct vm_snapshot_meta *meta);

/*
 * Address variables are pointers to guest memory.
 *
 * When RNULL != 0, do not enforce invalid address checks; instead, make the
 * pointer NULL at restore time.
 */
#define	SNAPSHOT_GUEST2HOST_ADDR_OR_LEAVE(CTX, ADDR, LEN, RNULL, META, RES, LABEL) \
do {										\
	(RES) = vm_snapshot_guest2host_addr((CTX), (void **)&(ADDR), (LEN),	\
	    (RNULL), (META));							\
	if ((RES) != 0) {							\
		if ((RES) == EFAULT)						\
			fprintf(stderr, "%s: invalid address: %s\r\n",		\
				__func__, #ADDR);				\
		goto LABEL;							\
	}									\
} while (0)

#endif
