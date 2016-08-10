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
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD$");

.globl sbi_hart_id; sbi_hart_id = -2048
.globl sbi_num_harts; sbi_num_harts = -2032
.globl sbi_query_memory; sbi_query_memory = -2016
.globl sbi_console_putchar; sbi_console_putchar = -2000
.globl sbi_console_getchar; sbi_console_getchar = -1984
.globl sbi_send_ipi; sbi_send_ipi = -1952
.globl sbi_clear_ipi; sbi_clear_ipi = -1936
.globl sbi_timebase; sbi_timebase = -1920
.globl sbi_shutdown; sbi_shutdown = -1904
.globl sbi_set_timer; sbi_set_timer = -1888
.globl sbi_mask_interrupt; sbi_mask_interrupt = -1872
.globl sbi_unmask_interrupt; sbi_unmask_interrupt = -1856
.globl sbi_remote_sfence_vm; sbi_remote_sfence_vm = -1840
.globl sbi_remote_sfence_vm_range; sbi_remote_sfence_vm_range = -1824
.globl sbi_remote_fence_i; sbi_remote_fence_i = -1808
