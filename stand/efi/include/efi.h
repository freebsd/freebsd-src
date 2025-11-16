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
#include <Uefi.h>
#include <Protocol/LoadedImage.h>

/* old efierr.h */
#define DECODE_ERROR(a)   (unsigned long)(a & ~MAX_BIT)

/* old efidevp.h */
#define EFI_DP_TYPE_MASK                    0x7F
#define EFI_DP_TYPE_UNPACKED                0x80

#define END_DEVICE_PATH_TYPE                0x7f

#define END_INSTANCE_DEVICE_PATH_SUBTYPE    0x01
#define END_DEVICE_PATH_LENGTH              (sizeof(EFI_DEVICE_PATH))


#define DP_IS_END_TYPE(a)
#define DP_IS_END_SUBTYPE(a)        ( ((a)->SubType == END_ENTIRE_DEVICE_PATH_SUBTYPE )

#define DevicePathType(a)           ( ((a)->Type) & EFI_DP_TYPE_MASK )
#define DevicePathSubType(a)        ( (a)->SubType )
#define DevicePathNodeLength(a)     ((size_t)(((a)->Length[0]) | ((a)->Length[1] << 8)))
#define NextDevicePathNode(a)       ( (EFI_DEVICE_PATH *) ( ((UINT8 *) (a)) + DevicePathNodeLength(a)))
#define IsDevicePathType(a, t)      ( DevicePathType(a) == t )
#define IsDevicePathEndType(a)      IsDevicePathType(a, END_DEVICE_PATH_TYPE)
#define IsDevicePathEndSubType(a)   ( (a)->SubType == END_ENTIRE_DEVICE_PATH_SUBTYPE )
#define IsDevicePathEnd(a)          ( IsDevicePathEndType(a) && IsDevicePathEndSubType(a) )
#define IsDevicePathUnpacked(a)     ( (a)->Type & EFI_DP_TYPE_UNPACKED )


#define SetDevicePathNodeLength(a,l) {                  \
            (a)->Length[0] = (UINT8) (l);               \
            (a)->Length[1] = (UINT8) ((l) >> 8);        \
            }

#define SetDevicePathEndNode(a)  {                      \
            (a)->Type = END_DEVICE_PATH_TYPE;           \
            (a)->SubType = END_ENTIRE_DEVICE_PATH_SUBTYPE;     \
            (a)->Length[0] = sizeof(EFI_DEVICE_PATH);   \
            (a)->Length[1] = 0;                         \
            }

#define NextMemoryDescriptor(Ptr,Size)  ((EFI_MEMORY_DESCRIPTOR *) (((UINT8 *) Ptr) + Size))
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
