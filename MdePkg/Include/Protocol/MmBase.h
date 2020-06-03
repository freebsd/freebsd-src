/** @file
  EFI MM Base Protocol as defined in the PI 1.5 specification.

  This protocol is utilized by all MM drivers to locate the MM infrastructure services and determine
  whether the driver is being invoked inside MMRAM or outside of MMRAM.

  Copyright (c) 2017, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _MM_BASE_H_
#define _MM_BASE_H_

#include <Pi/PiMmCis.h>

#define EFI_MM_BASE_PROTOCOL_GUID \
  { \
    0xf4ccbfb7, 0xf6e0, 0x47fd, {0x9d, 0xd4, 0x10, 0xa8, 0xf1, 0x50, 0xc1, 0x91 }  \
  }

typedef struct _EFI_MM_BASE_PROTOCOL  EFI_MM_BASE_PROTOCOL;

/**
  Service to indicate whether the driver is currently executing in the MM Initialization phase.

  This service is used to indicate whether the driver is currently executing in the MM Initialization
  phase. For MM drivers, this will return TRUE in InMmram while inside the driver's entry point and
  otherwise FALSE. For combination MM/DXE drivers, this will return FALSE in the DXE launch. For the
  MM launch, it behaves as an MM driver.

  @param[in]  This               The EFI_MM_BASE_PROTOCOL instance.
  @param[out] InMmram            Pointer to a Boolean which, on return, indicates that the driver is
                                 currently executing inside of MMRAM (TRUE) or outside of MMRAM (FALSE).

  @retval EFI_SUCCESS            The call returned successfully.
  @retval EFI_INVALID_PARAMETER  InMmram was NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_MM_INSIDE_OUT)(
  IN CONST EFI_MM_BASE_PROTOCOL    *This,
  OUT BOOLEAN                      *InMmram
  )
;

/**
  Returns the location of the Management Mode Service Table (MMST).

  This function returns the location of the Management Mode Service Table (MMST).  The use of the
  API is such that a driver can discover the location of the MMST in its entry point and then cache it in
  some driver global variable so that the MMST can be invoked in subsequent handlers.

  @param[in]     This            The EFI_MM_BASE_PROTOCOL instance.
  @param[in,out] Mmst            On return, points to a pointer to the Management Mode Service Table (MMST).

  @retval EFI_SUCCESS            The operation was successful.
  @retval EFI_INVALID_PARAMETER  Mmst was invalid.
  @retval EFI_UNSUPPORTED        Not in MM.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_MM_GET_MMST_LOCATION)(
  IN CONST EFI_MM_BASE_PROTOCOL  *This,
  IN OUT EFI_MM_SYSTEM_TABLE     **Mmst
  )
;

///
/// EFI MM Base Protocol is utilized by all MM drivers to locate the MM infrastructure
/// services and determine whether the driver is being invoked inside MMRAM or outside of MMRAM.
///
struct _EFI_MM_BASE_PROTOCOL {
  EFI_MM_INSIDE_OUT         InMm;
  EFI_MM_GET_MMST_LOCATION  GetMmstLocation;
};

extern EFI_GUID gEfiMmBaseProtocolGuid;

#endif

