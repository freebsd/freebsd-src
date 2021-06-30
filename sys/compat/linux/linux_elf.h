/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2018 Chuck Tuffli
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

#ifndef _COMPAT_LINUX_ELF_H_
#define _COMPAT_LINUX_ELF_H_

struct l_elf_siginfo {
	l_int		si_signo;
	l_int		si_code;
	l_int		si_errno;
};

typedef struct linux_pt_regset l_elf_gregset_t;

struct linux_elf_prstatus {
	struct l_elf_siginfo pr_info;
	l_short		pr_cursig;
	l_ulong		pr_sigpend;
	l_ulong		pr_sighold;
	l_pid_t		pr_pid;
	l_pid_t		pr_ppid;
	l_pid_t		pr_pgrp;
	l_pid_t		pr_sid;
	l_timeval	pr_utime;
	l_timeval	pr_stime;
	l_timeval	pr_cutime;
	l_timeval	pr_cstime;
	l_elf_gregset_t	pr_reg;
	l_int		pr_fpvalid;
};

#endif
