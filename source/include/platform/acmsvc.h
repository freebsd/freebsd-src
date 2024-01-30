/******************************************************************************
 *
 * Name: acmsvc.h - VC specific defines, etc.
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2023, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights. You may have additional license terms from the party that provided
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
 * to or modifications of the Original Intel Code. No other license or right
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
 * and the following Disclaimer and Export Compliance provision. In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change. Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee. Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution. In
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
 * HERE. ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT, ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES. INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS. INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES. THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government. In the
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
 *****************************************************************************
 *
 * Alternatively, you may choose to be licensed under the terms of the
 * following license:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Alternatively, you may choose to be licensed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 *****************************************************************************/

#ifndef __ACMSVC_H__
#define __ACMSVC_H__

/* Note: do not include any C library headers here */

/*
 * Note: MSVC project files should define ACPI_DEBUGGER and ACPI_DISASSEMBLER
 * as appropriate to enable editor functions like "Find all references".
 * The editor isn't smart enough to dig through the include files to find
 * out if these are actually defined.
 */

/* Eliminate warnings for "old" (non-secure) versions of clib functions */

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

/* Eliminate warnings for POSIX clib function names (open, write, etc.) */

#ifndef _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_NONSTDC_NO_DEPRECATE
#endif

#define COMPILER_DEPENDENT_INT64    __int64
#define COMPILER_DEPENDENT_UINT64   unsigned __int64
#define ACPI_INLINE                 __inline

/*
 * Calling conventions:
 *
 * ACPI_SYSTEM_XFACE        - Interfaces to host OS (handlers, threads)
 * ACPI_EXTERNAL_XFACE      - External ACPI interfaces
 * ACPI_INTERNAL_XFACE      - Internal ACPI interfaces
 * ACPI_INTERNAL_VAR_XFACE  - Internal variable-parameter list interfaces
 */
#define ACPI_SYSTEM_XFACE           __cdecl
#define ACPI_EXTERNAL_XFACE
#define ACPI_INTERNAL_XFACE
#define ACPI_INTERNAL_VAR_XFACE     __cdecl


/* Do not maintain the architecture specific stuffs for the EFI ports */

#if defined(__i386__) && !defined(_GNU_EFI) && !defined(_EDK2_EFI)
/*
 * Math helper functions
 */
#ifndef ACPI_DIV_64_BY_32
#define ACPI_DIV_64_BY_32(n_hi, n_lo, d32, q32, r32) \
{                           \
    __asm mov    edx, n_hi  \
    __asm mov    eax, n_lo  \
    __asm div    d32        \
    __asm mov    q32, eax   \
    __asm mov    r32, edx   \
}
#endif

#ifndef ACPI_MUL_64_BY_32
#define ACPI_MUL_64_BY_32(n_hi, n_lo, m32, p32, c32) \
{                           \
    __asm mov    edx, n_hi  \
    __asm mov    eax, n_lo  \
    __asm mul    m32        \
    __asm mov    p32, eax   \
    __asm mov    c32, edx   \
}
#endif

#ifndef ACPI_SHIFT_LEFT_64_BY_32
#define ACPI_SHIFT_LEFT_64_BY_32(n_hi, n_lo, s32) \
{                               \
    __asm mov    edx, n_hi      \
    __asm mov    eax, n_lo      \
    __asm mov    ecx, s32       \
    __asm and    ecx, 31        \
    __asm shld   edx, eax, cl   \
    __asm shl    eax, cl        \
    __asm mov    n_hi, edx      \
    __asm mov    n_lo, eax      \
}
#endif

#ifndef ACPI_SHIFT_RIGHT_64_BY_32
#define ACPI_SHIFT_RIGHT_64_BY_32(n_hi, n_lo, s32) \
{                               \
    __asm mov    edx, n_hi      \
    __asm mov    eax, n_lo      \
    __asm mov    ecx, s32       \
    __asm and    ecx, 31        \
    __asm shrd   eax, edx, cl   \
    __asm shr    edx, cl        \
    __asm mov    n_hi, edx      \
    __asm mov    n_lo, eax      \
}
#endif

#ifndef ACPI_SHIFT_RIGHT_64
#define ACPI_SHIFT_RIGHT_64(n_hi, n_lo) \
{                           \
    __asm shr    n_hi, 1    \
    __asm rcr    n_lo, 1    \
}
#endif
#endif

/* warn C4001: use of slash-slash comments */
/* NOTE: MSVC 2015 headers use these extensively */
#pragma warning(disable:4001)

/* warn C4100: unreferenced formal parameter */
#pragma warning(disable:4100)

/* warn C4127: conditional expression is constant */
#pragma warning(disable:4127)

/* warn C4706: assignment within conditional expression */
#pragma warning(disable:4706)

/* warn C4131: uses old-style declarator (iASL compiler only) */
#pragma warning(disable:4131)

/* warn C4131: uses old-style declarator (iASL compiler only) */
#pragma warning(disable:4459)

/* warn c4200: allow flexible arrays (of zero length) */
#pragma warning(disable:4200)

#if _MSC_VER > 1200 /* Versions above VC++ 6 */
#pragma warning( disable : 4295 ) /* needed for acpredef.h array */
#endif

/*
 * MSVC 2015+
 */

 /* warn C4459: xxxx (identifier) hides global declaration */
#pragma warning(disable:4459)


/* Debug support. */

#ifdef _DEBUG

/*
 * Debugging memory corruption issues with windows:
 * Add #include <crtdbg.h> to accommon.h if necessary.
 * Add _ASSERTE(_CrtCheckMemory()); where needed to test memory integrity.
 * This can quickly localize the memory corruption.
 */
#define ACPI_DEBUG_INITIALIZE() \
    _CrtSetDbgFlag (\
        _CRTDBG_CHECK_ALWAYS_DF | \
        _CRTDBG_ALLOC_MEM_DF | \
        _CRTDBG_DELAY_FREE_MEM_DF | \
        _CRTDBG_LEAK_CHECK_DF | \
        _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG));

#if 0
/*
 * _CrtSetBreakAlloc can be used to set a breakpoint at a particular
 * memory leak, add to the macro above.
 */
Detected memory leaks!
Dumping objects ->
..\..\source\os_specific\service_layers\oswinxf.c(701) : {937} normal block at 0x002E9190, 40 bytes long.
 Data: <                > 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00

_CrtSetBreakAlloc (937);
#endif

#endif

#if _MSC_VER > 1200 /* Versions above VC++ 6 */
#define COMPILER_VA_MACRO               1
#else
#endif

/* Begin standard headers */

/*
 * warn C4001: nonstandard extension 'single line comment' was used
 *
 * We need to enable this for ACPICA internal files, but disable it for
 * buggy MS runtime headers.
 */
#pragma warning(push)
#pragma warning(disable:4001)

#endif /* __ACMSVC_H__ */
