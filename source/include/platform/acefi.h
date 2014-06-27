/******************************************************************************
 *
 * Name: acefi.h - OS specific defines, etc.
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2014, Intel Corp.
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

#ifndef __ACEFI_H__
#define __ACEFI_H__

#include <stdarg.h>
#if defined(_GNU_EFI)
#include <stdint.h>
#include <unistd.h>
#endif
#include <efi.h>
#include <efistdarg.h>
#include <efilib.h>


/* AED EFI definitions */

#if defined(_AED_EFI)

/* _int64 works for both IA32 and IA64 */

#define COMPILER_DEPENDENT_INT64   __int64
#define COMPILER_DEPENDENT_UINT64  unsigned __int64

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

/* warn C4142: redefinition of type */

#pragma warning(disable:4142)

#endif


/* GNU EFI definitions */

#if defined(_GNU_EFI)

/* Using GCC for GNU EFI */

#include "acgcc.h"

#undef ACPI_USE_SYSTEM_CLIBRARY
#undef ACPI_USE_STANDARD_HEADERS
#undef ACPI_USE_NATIVE_DIVIDE
#define ACPI_USE_SYSTEM_INTTYPES

#define ACPI_FILE           SIMPLE_TEXT_OUTPUT_INTERFACE *
#define ACPI_FILE_OUT       ST->ConOut
#define ACPI_FILE_ERR       ST->ConOut

/*
 * Math helpers
 */
#define ACPI_DIV_64_BY_32(n_hi, n_lo, d32, q32, r32) \
    do {                                             \
        UINT64 __n = ((UINT64) n_hi) << 32 | (n_lo); \
        (q32) = DivU64x32 ((__n), (d32), &(r32));    \
    } while (0)

#define ACPI_SHIFT_RIGHT_64(n_hi, n_lo) \
    do {                                \
        (n_lo) >>= 1;                   \
        (n_lo) |= (((n_hi) & 1) << 31); \
        (n_hi) >>= 1;                   \
    } while (0)

/*
 * EFI specific prototypes
 */
EFI_STATUS
efi_main (
    EFI_HANDLE              Image,
    EFI_SYSTEM_TABLE        *SystemTab);

int
acpi_main (
    int                     argc,
    char                    *argv[]);


#endif


#endif /* __ACEFI_H__ */
