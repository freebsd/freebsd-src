/*++

Copyright (c)  1999 - 2002 Intel Corporation. All rights reserved
This software and associated documentation (if any) is furnished
under a license and may only be used or copied in accordance
with the terms of the license. Except as permitted by such
license, no part of this software or documentation may be
reproduced, stored in a retrieval system, or transmitted in any
form or by any means without the express written consent of
Intel Corporation.

Module Name:

    efi.h

Abstract:

    Public EFI header files



Revision History

--*/

//
// Build flags on input
//  EFI32
//  EFI_DEBUG               - Enable debugging code
//  EFI_NT_EMULATOR         - Building for running under NT
//


#ifndef _EFI_INCLUDE_
#define _EFI_INCLUDE_

#define EFI_FIRMWARE_VENDOR         L"INTEL"
#define EFI_FIRMWARE_MAJOR_REVISION 14
#define EFI_FIRMWARE_MINOR_REVISION 62
#define EFI_FIRMWARE_REVISION ((EFI_FIRMWARE_MAJOR_REVISION <<16) | (EFI_FIRMWARE_MINOR_REVISION))

//
// Basic EFI types of various widths.
//

#include <stdint.h>
#ifndef ACPI_THREAD_ID		/* ACPI's definitions are fine */
#define ACPI_USE_SYSTEM_INTTYPES 1	/* Tell ACPI we've defined types */

typedef uint64_t   UINT64;
typedef int64_t    INT64;
typedef uint32_t   UINT32;
typedef int32_t    INT32;
typedef uint16_t   UINT16;
typedef int16_t    INT16;
typedef uint8_t    UINT8;
typedef int8_t     INT8;

#ifdef __LP64__
typedef int64_t    INTN;
typedef uint64_t   UINTN;
#else
typedef int32_t    INTN;
typedef uint32_t   UINTN;
#endif

#endif

#undef VOID
#define VOID    void


#include "efibind.h"
#include "efidef.h"
#include "efidevp.h"
#include "efipciio.h"
#include "efiprot.h"
#include "eficon.h"
#include "eficonsctl.h"
#include "efiser.h"
#include "efi_nii.h"
#include "efipxebc.h"
#include "efinet.h"
#include "efiapi.h"
#include "efifs.h"
#include "efierr.h"
#include "efigop.h"
#include "efiip.h"
#include "efiudp.h"
#include "efitcp.h"
#include "efipoint.h"
#include "efiuga.h"
#include <sys/types.h>

/*
 * Global variables
 */
extern EFI_LOADED_IMAGE *boot_img;
extern bool boot_services_active;

/*
 * FreeBSD UUID
 */
#define FREEBSD_BOOT_VAR_GUID \
	{ 0xCFEE69AD, 0xA0DE, 0x47A9, {0x93, 0xA8, 0xF6, 0x31, 0x06, 0xF8, 0xAE, 0x99} }

#endif
