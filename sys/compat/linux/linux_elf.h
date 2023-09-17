/*-
 * SPDX-License-Identifier: BSD-2-Clause
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

struct note_info_list;

/* Linux core notes are labeled "CORE" */
#define	LINUX_ABI_VENDOR	"CORE"

/* Elf notes */
#define	GNU_ABI_VENDOR		"GNU"
#define	GNU_ABI_LINUX		0

/* This adds "linux32_" and "linux64_" prefixes. */
#define	__linuxN(x)	__CONCAT(__CONCAT(__CONCAT(linux,__ELF_WORD_SIZE),_),x)

void 	__linuxN(prepare_notes)(struct thread *, struct note_info_list *,
	    size_t *);
void	__linuxN(arch_copyout_auxargs)(struct image_params *, Elf_Auxinfo **);
int	__linuxN(copyout_auxargs)(struct image_params *, uintptr_t);
int	__linuxN(copyout_strings)(struct image_params *, uintptr_t *);
bool	linux_trans_osrel(const Elf_Note *note, int32_t *osrel);

#endif
