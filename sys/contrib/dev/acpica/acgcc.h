/******************************************************************************
 *
 * Name: acgcc.h - GCC specific defines, etc.
 *       $Revision: 17 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2002, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights.  You may have additional license terms from the party that provided
 * you this software, covering your right to use that party's intellectual
 * property rights.
 *
 * 2.2. Intel grants, free of charge, to any person ("Licensee") obtaining a
 * copy of the source code appearing in this file ("Covered Code") an
 * irrevocable, perpetual, worldwide license under Intel's copyrights in the
 * base code distributed originally by Intel ("Original Intel Code") to copy,
 * make derivatives, distribute, use and display any portion of the Covered
 * Code in any form, with the right to sublicense such rights; and
 *
 * 2.3. Intel grants Licensee a non-exclusive and non-transferable patent
 * license (with the right to sublicense), under only those claims of Intel
 * patents that are infringed by the Original Intel Code, to make, use, sell,
 * offer to sell, and import the Covered Code and derivative works thereof
 * solely to the minimum extent necessary to exercise the above copyright
 * license, and in no event shall the patent license extend to any additions
 * to or modifications of the Original Intel Code.  No other license or right
 * is granted directly or by implication, estoppel or otherwise;
 *
 * The above copyright and patent license is granted only if the following
 * conditions are met:
 *
 * 3. Conditions
 *
 * 3.1. Redistribution of Source with Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification with rights to further distribute source must include
 * the above Copyright Notice, the above License, this list of Conditions,
 * and the following Disclaimer and Export Compliance provision.  In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change.  Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee.  Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution.  In
 * addition, Licensee may not authorize further sublicense of source of any
 * portion of the Covered Code, and must include terms to the effect that the
 * license from Licensee to its licensee is limited to the intellectual
 * property embodied in the software Licensee provides to its licensee, and
 * not to intellectual property embodied in modifications its licensee may
 * make.
 *
 * 3.3. Redistribution of Executable. Redistribution in executable form of any
 * substantial portion of the Covered Code or modification must reproduce the
 * above Copyright Notice, and the following Disclaimer and Export Compliance
 * provision in the documentation and/or other materials provided with the
 * distribution.
 *
 * 3.4. Intel retains all right, title, and interest in and to the Original
 * Intel Code.
 *
 * 3.5. Neither the name Intel nor any other trademark owned or controlled by
 * Intel shall be used in advertising or otherwise to promote the sale, use or
 * other dealings in products derived from or relating to the Covered Code
 * without prior written authorization from Intel.
 *
 * 4. Disclaimer and Export Compliance
 *
 * 4.1. INTEL MAKES NO WARRANTY OF ANY KIND REGARDING ANY SOFTWARE PROVIDED
 * HERE.  ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT,  ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES.  INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS.  INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.  THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government.  In the
 * event Licensee exports any such software from the United States or
 * re-exports any such software from a foreign destination, Licensee shall
 * ensure that the distribution and export/re-export of the software is in
 * compliance with all laws, regulations, orders, or other restrictions of the
 * U.S. Export Administration Regulations. Licensee agrees that neither it nor
 * any of its subsidiaries will export/re-export any technical data, process,
 * software, or service, directly or indirectly, to any country for which the
 * United States government or any agency thereof requires an export license,
 * other governmental approval, or letter of assurance, without first obtaining
 * such license, approval or letter.
 *
 *****************************************************************************/

#ifndef __ACGCC_H__
#define __ACGCC_H__


#ifdef __ia64__
#define _IA64

#define COMPILER_DEPENDENT_UINT64   unsigned long
/* Single threaded */
#define ACPI_APPLICATION

#define ACPI_ASM_MACROS
#define causeinterrupt(level)
#define BREAKPOINT3
#define acpi_disable_irqs() __cli()
#define acpi_enable_irqs()  __sti()

/*! [Begin] no source code translation */

#include <asm/pal.h>

#define halt()              ia64_pal_halt_light()           /* PAL_HALT[_LIGHT] */
#define safe_halt()         ia64_pal_halt(1)                /* PAL_HALT */


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


#else /* DO IA32 */

#define COMPILER_DEPENDENT_UINT64   unsigned long long
#define ACPI_ASM_MACROS
#define causeinterrupt(level)
#define BREAKPOINT3
#define acpi_disable_irqs() __cli()
#define acpi_enable_irqs()  __sti()
#define halt()    __asm__ __volatile__ ("sti; hlt":::"memory")

/*! [Begin] no source code translation
 *
 * A brief explanation as GNU inline assembly is a bit hairy
 *  %0 is the output parameter in EAX ("=a")
 *  %1 and %2 are the input parameters in ECX ("c")
 *  and an immediate value ("i") respectively
 *  All actual register references are preceded with "%%" as in "%%edx"
 *  Immediate values in the assembly are preceded by "$" as in "$0x1"
 *  The final asm parameter are the operation altered non-output registers.
 */
#define ACPI_ACQUIRE_GLOBAL_LOCK(GLptr, Acq) \
    do { \
        int dummy; \
        asm("1:     movl (%1),%%eax;" \
            "movl   %%eax,%%edx;" \
            "andl   %2,%%edx;" \
            "btsl   $0x1,%%edx;" \
            "adcl   $0x0,%%edx;" \
            "lock;  cmpxchgl %%edx,(%1);" \
            "jnz    1b;" \
            "cmpb   $0x3,%%dl;" \
            "sbbl   %%eax,%%eax" \
            :"=a"(Acq),"=c"(dummy):"c"(GLptr),"i"(~1L):"dx"); \
    } while(0)

#define ACPI_RELEASE_GLOBAL_LOCK(GLptr, Acq) \
    do { \
        int dummy; \
        asm("1:     movl (%1),%%eax;" \
            "movl   %%eax,%%edx;" \
            "andl   %2,%%edx;" \
            "lock;  cmpxchgl %%edx,(%1);" \
            "jnz    1b;" \
            "andl   $0x1,%%eax" \
            :"=a"(Acq),"=c"(dummy):"c"(GLptr),"i"(~3L):"dx"); \
    } while(0)


/*
 * Math helper asm macros
 */
#define ACPI_DIV_64_BY_32(n_hi, n_lo, d32, q32, r32) \
        asm("divl %2;"        \
        :"=a"(q32), "=d"(r32) \
        :"r"(d32),            \
        "0"(n_lo), "1"(n_hi))


#define ACPI_SHIFT_RIGHT_64(n_hi, n_lo) \
    asm("shrl   $1,%2;"             \
        "rcrl   $1,%3;"             \
        :"=r"(n_hi), "=r"(n_lo)     \
        :"0"(n_hi), "1"(n_lo))

/*! [End] no source code translation !*/

#endif /* IA 32 */

/* This macro is used to tag functions as "printf-like" because
 * some compilers (like GCC) can catch printf format string problems.
 */
#define ACPI_PRINTF_LIKE_FUNC __attribute__ ((__format__ (__printf__, 4, 5)))

#endif /* __ACGCC_H__ */
