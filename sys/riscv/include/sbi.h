/*-
 * Copyright (c) 2016 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * Portions of this software were developed by SRI International and the
 * University of Cambridge Computer Laboratory under DARPA/AFRL contract
 * FA8750-10-C-0237 ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Portions of this software were developed by the University of Cambridge
 * Computer Laboratory as part of the CTSRD Project, with support from the
 * UK Higher Education Innovation Fund (HEIF).
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

#ifndef _MACHINE_SBI_H_
#define	_MACHINE_SBI_H_

typedef struct {
	uint64_t base;
	uint64_t size;
	uint64_t node_id;
} memory_block_info;

uint64_t sbi_query_memory(uint64_t id, memory_block_info *p);
uint64_t sbi_hart_id(void);
uint64_t sbi_num_harts(void);
uint64_t sbi_timebase(void);
void sbi_set_timer(uint64_t stime_value);
void sbi_send_ipi(uint64_t hart_id);
uint64_t sbi_clear_ipi(void);
void sbi_shutdown(void);

void sbi_console_putchar(unsigned char ch);
int sbi_console_getchar(void);

void sbi_remote_sfence_vm(uint64_t hart_mask_ptr, uint64_t asid);
void sbi_remote_sfence_vm_range(uint64_t hart_mask_ptr, uint64_t asid, uint64_t start, uint64_t size);
void sbi_remote_fence_i(uint64_t hart_mask_ptr);

uint64_t sbi_mask_interrupt(uint64_t which);
uint64_t sbi_unmask_interrupt(uint64_t which);

#endif /* !_MACHINE_SBI_H_ */
