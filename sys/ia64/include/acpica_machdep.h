/*-
 * Copyright (c) 2002 Mitsuru IWASAKI
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
 *
 * $FreeBSD$
 */

/******************************************************************************
 *
 * Name: acpica_machdep.h - arch-specific defines, etc.
 *       $Revision$
 *
 *****************************************************************************/

#ifndef __ACPICA_MACHDEP_H__
#define __ACPICA_MACHDEP_H__

#ifdef _KERNEL
#define _IA64

/*
 * Calling conventions:
 *
 * ACPI_SYSTEM_XFACE        - Interfaces to host OS (handlers, threads)
 * ACPI_EXTERNAL_XFACE      - External ACPI interfaces 
 * ACPI_INTERNAL_XFACE      - Internal ACPI interfaces
 * ACPI_INTERNAL_VAR_XFACE  - Internal variable-parameter list interfaces
 */
#define ACPI_SYSTEM_XFACE
#define ACPI_EXTERNAL_XFACE
#define ACPI_INTERNAL_XFACE
#define ACPI_INTERNAL_VAR_XFACE

/* Asm macros */

#define ACPI_ASM_MACROS
#define BREAKPOINT3
#define ACPI_DISABLE_IRQS() disable_intr()
#define ACPI_ENABLE_IRQS()  enable_intr()

#define ACPI_FLUSH_CPU_CACHE()	/* XXX ia64_fc()? */

/*! [Begin] no source code translation */

#define ACPI_ACQUIRE_GLOBAL_LOCK(GLptr, Acq) \
    do { \
    __asm__ volatile ("1:  ld4      r29=%1\n"  \
        ";;\n"                  \
        "mov    ar.ccv=r29\n"   \
        "mov    r2=r29\n"       \
        "shr.u  r30=r29,1\n"    \
        "and    r29=-4,r29\n"   \
        ";;\n"                  \
        "add    r29=2,r29\n"    \
        "and    r30=1,r30\n"    \
        ";;\n"                  \
        "add    r29=r29,r30\n"  \
        ";;\n"                  \
        "cmpxchg4.acq   r30=%1,r29,ar.ccv\n" \
        ";;\n"                  \
        "cmp.eq p6,p7=r2,r30\n" \
        "(p7) br.dpnt.few 1b\n" \
        "cmp.gt p8,p9=3,r29\n"  \
        ";;\n"                  \
        "(p8) mov %0=-1\n"      \
        "(p9) mov %0=r0\n"      \
        :"=r"(Acq):"m"(GLptr):"r2","r29","r30","memory"); \
    } while (0)

#define ACPI_RELEASE_GLOBAL_LOCK(GLptr, Acq) \
    do { \
    __asm__ volatile ("1:  ld4      r29=%1\n" \
        ";;\n"                  \
        "mov    ar.ccv=r29\n"   \
        "mov    r2=r29\n"       \
        "and    r29=-4,r29\n"   \
        ";;\n"                  \
        "cmpxchg4.acq   r30=%1,r29,ar.ccv\n" \
        ";;\n"                  \
        "cmp.eq p6,p7=r2,r30\n" \
        "(p7) br.dpnt.few 1b\n" \
        "and    %0=1,r2\n"      \
        ";;\n"                  \
        :"=r"(Acq):"m"(GLptr):"r2","r29","r30","memory"); \
    } while (0)
/*! [End] no source code translation !*/

#endif /* _KERNEL */

#define ACPI_MACHINE_WIDTH             64
#define COMPILER_DEPENDENT_INT64       long
#define COMPILER_DEPENDENT_UINT64      unsigned long

#endif /* __ACPICA_MACHDEP_H__ */
