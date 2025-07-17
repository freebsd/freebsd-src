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

#define	MD_OBJ_ENTRY	\
    Elf_Addr *gotptr;		/* GOT pointer (secure-plt only) */

/* Return the address of the .dynamic section in the dynamic linker. */
#define rtld_dynamic(obj)    (&_DYNAMIC)

bool arch_digest_dynamic(struct Struct_Obj_Entry *, const Elf_Dyn *);

/* No architecture specific notes */
#define	arch_digest_note(obj, note)	false

Elf_Addr reloc_jmpslot(Elf_Addr *where, Elf_Addr target,
    const struct Struct_Obj_Entry *defobj, const struct Struct_Obj_Entry *obj,
    const Elf_Rel *rel);
void reloc_non_plt_self(Elf_Dyn *dynp, Elf_Addr relocbase);

#define make_function_pointer(def, defobj) \
	((defobj)->relocbase + (def)->st_value)

#define call_initfini_pointer(obj, target) \
	(((InitFunc)(target))())

#define call_init_pointer(obj, target) \
	(((InitArrFunc)(target))(main_argc, main_argv, environ))

extern u_long cpu_features; /* r3 */
extern u_long cpu_features2; /* r4 */
/* r5-10: ifunc resolver parameters reserved for future assignment. */
#define	call_ifunc_resolver(ptr) \
	(((Elf_Addr (*)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, \
	    uint32_t, uint32_t, uint32_t))ptr)((uint32_t)cpu_features, \
	    (uint32_t)cpu_features2, 0, 0, 0, 0, 0, 0))

/*
 * PLT functions. Not really correct prototypes, but the
 * symbol values are needed.
 */
void _rtld_powerpc_pltlongresolve(void);
void _rtld_powerpc_pltresolve(void);
void _rtld_powerpc_pltcall(void);

/*
 * TLS
 */

#define round(size, align) \
    (((size) + (align) - 1) & ~((align) - 1))
#define calculate_first_tls_offset(size, align, offset)	\
    TLS_TCB_SIZE
#define calculate_tls_offset(prev_offset, prev_size, size, align, offset) \
    round(prev_offset + prev_size, align)
#define calculate_tls_post_size(align)  0
 
typedef struct {
	unsigned long ti_module;
	unsigned long ti_offset;
} tls_index;

extern void *__tls_get_addr(tls_index* ti);

extern void powerpc_abi_variant_hook(Elf_Auxinfo **);
#define md_abi_variant_hook(x) powerpc_abi_variant_hook(x)

#endif
