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

#include <sys/efi-edk2.h>

#if __SIZEOF_LONG__ == 4
#define EFI_ERROR_MASK      0x80000000
#define EFIERR(a)           (0x80000000 | a)
#define EFIERR_OEM(a)       (0xc0000000 | a)
#else
#define EFI_ERROR_MASK      0x8000000000000000
#define EFIERR(a)           (0x8000000000000000 | a)
#define EFIERR_OEM(a)       (0xc000000000000000 | a)
#endif

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
