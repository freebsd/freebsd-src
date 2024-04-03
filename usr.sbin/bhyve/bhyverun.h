/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2011 NetApp, Inc.
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
 */

#ifndef	_BHYVERUN_H_
#define	_BHYVERUN_H_

#include <stdbool.h>

#define	VMEXIT_CONTINUE		(0)
#define	VMEXIT_ABORT		(-1)

extern int guest_ncpus;
extern uint16_t cpu_cores, cpu_sockets, cpu_threads;

struct vcpu;
struct vmctx;
struct vm_run;

void *paddr_guest2host(struct vmctx *ctx, uintptr_t addr, size_t len);
#ifdef BHYVE_SNAPSHOT
uintptr_t paddr_host2guest(struct vmctx *ctx, void *addr);
#endif

struct vcpu;
struct vcpu *fbsdrun_vcpu(int vcpuid);
void fbsdrun_addcpu(int vcpuid);
void fbsdrun_deletecpu(int vcpuid);
int fbsdrun_suspendcpu(int vcpuid);

int  fbsdrun_virtio_msix(void);

typedef int (*vmexit_handler_t)(struct vmctx *, struct vcpu *, struct vm_run *);

/* Interfaces implemented by machine-dependent code. */
void bhyve_init_config(void);
void bhyve_optparse(int argc, char **argv);
void bhyve_usage(int code);

/* Interfaces used by command-line option-parsing code. */
bool bhyve_parse_config_option(const char *option);
void bhyve_parse_simple_config_file(const char *path);
#ifdef BHYVE_GDB
void bhyve_parse_gdb_options(const char *opt);
#endif
int bhyve_pincpu_parse(const char *opt);
int bhyve_topology_parse(const char *opt);

void bhyve_init_vcpu(struct vcpu *vcpu);
void bhyve_start_vcpu(struct vcpu *vcpu, bool bsp);
int bhyve_init_platform(struct vmctx *ctx, struct vcpu *bsp);
int bhyve_init_platform_late(struct vmctx *ctx, struct vcpu *bsp);

#endif
