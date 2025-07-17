/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Dmitry Chagin <dchagin@FreeBSD.org>
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

#ifndef _X86_INCLUDE_LINUX_LINUX_X86_H_
#define _X86_INCLUDE_LINUX_LINUX_X86_H_

#define	LINUX_VDSO_CPU_DEFAULT		0
#define	LINUX_VDSO_CPU_RDPID		1
#define	LINUX_VDSO_CPU_RDTSCP		2

/* More machine dependent hints about processor capabilities. */
#define	LINUX_HWCAP2_RING3MWAIT		0x00000001
#define	LINUX_HWCAP2_FSGSBASE		0x00000002

int	linux_vdso_tsc_selector_idx(void);
int	linux_vdso_cpu_selector_idx(void);

int	linux_translate_traps(int, int);
int	bsd_to_linux_trapcode(int);

u_int	linux_x86_elf_hwcap2(void);

#endif /* _X86_INCLUDE_LINUX_LINUX_X86_H_ */
