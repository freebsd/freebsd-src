/*-
 * Copyright (c) 2012 Sandvine, Inc.
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
 *
 * $FreeBSD$
 */

#ifndef _INSTRUCTION_EMUL_H_
#define _INSTRUCTION_EMUL_H_

struct memory_region;

typedef int (*emulated_read_func_t)(struct vmctx *vm, int vcpu, uintptr_t addr, 
				    int size, uint64_t *data, void *arg);
typedef int (*emulated_write_func_t)(struct vmctx *vm, int vcpu, uintptr_t addr, 
				     int size, uint64_t data, void *arg);

int emulate_instruction(struct vmctx *vm, int vcpu, uint64_t rip, 
			uint64_t cr3);
struct memory_region *register_emulated_memory(uintptr_t start, size_t len, 
					       emulated_read_func_t memread, 
					       emulated_write_func_t memwrite, 
					       void *arg);
void move_memory_region(struct memory_region *memory_region, uintptr_t start);

#endif
