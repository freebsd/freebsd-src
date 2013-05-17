/******************************************************************************
 *
 * Name: acmsvc.h - VC specific defines, etc.
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2013, Intel Corp.
 * All rights reserved.
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
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */

#ifndef __ACMSVC_H__
#define __ACMSVC_H__


/*
 * Map low I/O functions for MS. This allows us to disable MS language
 * extensions for maximum portability.
 */
#define open            _open
#define read            _read
#define write           _write
#define close           _close
#define stat            _stat
#define fstat           _fstat
#define mkdir           _mkdir
#define strlwr          _strlwr
#define O_RDONLY        _O_RDONLY
#define O_BINARY        _O_BINARY
#define O_CREAT         _O_CREAT
#define O_WRONLY        _O_WRONLY
#define O_TRUNC         _O_TRUNC
#define S_IREAD         _S_IREAD
#define S_IWRITE        _S_IWRITE
#define S_IFDIR         _S_IFDIR

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

#ifndef _LINT
/*
 * Math helper functions
 */
#define ACPI_DIV_64_BY_32(n_hi, n_lo, d32, q32, r32) \
{                           \
    __asm mov    edx, n_hi  \
    __asm mov    eax, n_lo  \
    __asm div    d32        \
    __asm mov    q32, eax   \
    __asm mov    r32, edx   \
}

#define ACPI_SHIFT_RIGHT_64(n_hi, n_lo) \
{                           \
    __asm shr    n_hi, 1    \
    __asm rcr    n_lo, 1    \
}
#else

/* Fake versions to make lint happy */

#define ACPI_DIV_64_BY_32(n_hi, n_lo, d32, q32, r32) \
{                           \
    q32 = n_hi / d32;       \
    r32 = n_lo / d32;       \
}

#define ACPI_SHIFT_RIGHT_64(n_hi, n_lo) \
{                           \
    n_hi >>= 1;    \
    n_lo >>= 1;    \
}
#endif

/* warn C4100: unreferenced formal parameter */
#pragma warning(disable:4100)

/* warn C4127: conditional expression is constant */
#pragma warning(disable:4127)

/* warn C4706: assignment within conditional expression */
#pragma warning(disable:4706)

/* warn C4131: uses old-style declarator (iASL compiler only) */
#pragma warning(disable:4131)

#if _MSC_VER > 1200 /* Versions above VC++ 6 */
#pragma warning( disable : 4295 ) /* needed for acpredef.h array */
#endif


/* Debug support. Must be last in this file, do not move. */

#ifdef _DEBUG
#include <crtdbg.h>

/*
 * Debugging memory corruption issues with windows:
 * Add #include <crtdbg.h> to accommon.h if necessary.
 * Add _ASSERTE(_CrtCheckMemory()); where needed to test memory integrity.
 * This can quickly localize the memory corruption.
 */
#define ACPI_DEBUG_INITIALIZE() \
    _CrtSetDbgFlag (_CRTDBG_CHECK_ALWAYS_DF | \
        _CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_CRT_DF | \
        _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG))
#endif

#endif /* __ACMSVC_H__ */
