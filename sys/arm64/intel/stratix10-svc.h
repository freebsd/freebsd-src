/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Ruslan Bukin <br@bsdpad.com>
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
 * DARPA SSITH research programme.
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

#ifndef _ARM64_INTEL_STRATIX10_SVC_H_
#define	_ARM64_INTEL_STRATIX10_SVC_H_

struct s10_svc_msg {
	int command;
#define	COMMAND_RECONFIG		(1 << 0)
#define	COMMAND_RECONFIG_DATA_SUBMIT	(1 << 1)
#define	COMMAND_RECONFIG_DATA_CLAIM	(1 << 2)
	int flags;
#define	COMMAND_RECONFIG_FLAG_PARTIAL	(1 << 0)
	void *payload;
	int payload_length;
};

struct s10_svc_mem {
	vm_offset_t paddr;
	vm_offset_t vaddr;
	int size;
	int fill;
};

int s10_svc_send(device_t dev, struct s10_svc_msg *msg);
int s10_svc_allocate_memory(device_t dev, struct s10_svc_mem *mem, int size);
void s10_svc_free_memory(device_t dev, struct s10_svc_mem *mem);

#endif	/* !_ARM64_INTEL_STRATIX10_SVC_H_ */
