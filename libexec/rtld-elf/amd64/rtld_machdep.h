/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 1999, 2000 John D. Polstra.
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

#ifndef RTLD_MACHDEP_H
#define RTLD_MACHDEP_H	1

#include <sys/types.h>
#include <machine/atomic.h>
#include <machine/tls.h>

struct Struct_Obj_Entry;

/* Return the address of the .dynamic section in the dynamic linker. */
Elf_Dyn *rtld_dynamic_addr(void);
#define	rtld_dynamic(obj)	rtld_dynamic_addr()

/* No architecture specific notes */
#define	arch_digest_note(obj, note)	false

Elf_Addr reloc_jmpslot(Elf_Addr *where, Elf_Addr target,
    const struct Struct_Obj_Entry *obj, const struct Struct_Obj_Entry *refobj,
    const Elf_Rel *rel);

#define make_function_pointer(def, defobj)	\
	((defobj)->relocbase + (def)->st_value)

#define call_initfini_pointer(obj, target) \
	(((InitFunc)(target))())

#define call_init_pointer(obj, target) \
	(((InitArrFunc)(target))(main_argc, main_argv, environ))

extern uint32_t cpu_feature;
extern uint32_t cpu_feature2;
extern uint32_t cpu_stdext_feature;
extern uint32_t cpu_stdext_feature2;
#define	call_ifunc_resolver(ptr) \
	(((Elf_Addr (*)(uint32_t, uint32_t, uint32_t, uint32_t))ptr)( \
	    cpu_feature, cpu_feature2, cpu_stdext_feature, cpu_stdext_feature2))

typedef struct {
    unsigned long ti_module;
    unsigned long ti_offset;
} tls_index;

void *__tls_get_addr(tls_index *ti) __exported;

#define md_abi_variant_hook(x)

size_t calculate_first_tls_offset(size_t size, size_t align, size_t offset);
size_t calculate_tls_offset(size_t prev_offset, size_t prev_size, size_t size,
    size_t align, size_t offset);
#endif
