/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 ARM Ltd
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>

#include <sys/param.h>

#include <machine/cpu.h>
#include <machine/elf.h>
#include <machine/ifunc.h>
#include <machine/md_var.h>

int copyout_std(const void *kaddr, void *udaddr, size_t len);
int copyout_mops(const void *kaddr, void *udaddr, size_t len);
int copyin_std(const void *uaddr, void *kdaddr, size_t len);
int copyin_mops(const void *uaddr, void *kdaddr, size_t len);

DEFINE_IFUNC(, int, copyout, (const void *, void *, size_t))
{
        return ((elf_hwcap2 & HWCAP2_MOPS) != 0 ? copyout_mops : copyout_std);
}

DEFINE_IFUNC(, int, copyin, (const void *, void *, size_t))
{
        return ((elf_hwcap2 & HWCAP2_MOPS) != 0 ? copyin_mops : copyin_std);
}
