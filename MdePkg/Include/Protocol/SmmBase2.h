/** @file
  EFI SMM Base2 Protocol as defined in the PI 1.2 specification.

  This protocol is utilized by all SMM drivers to locate the SMM infrastructure services and determine
  whether the driver is being invoked inside SMRAM or outside of SMRAM.

  Copyright (c) 2009 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _SMM_BASE2_H_
#define _SMM_BASE2_H_

#include <Pi/PiSmmCis.h>
#include <Protocol/MmBase.h>

#define EFI_SMM_BASE2_PROTOCOL_GUID  EFI_MM_BASE_PROTOCOL_GUID

typedef struct _EFI_SMM_BASE2_PROTOCOL EFI_SMM_BASE2_PROTOCOL;

/**
  Service to indicate whether the driver is currently executing in the SMM Initialization phase.

  This service is used to indicate whether the driver is currently executing in the SMM Initialization
  phase. For SMM drivers, this will return TRUE in InSmram while inside the driver's entry point and
  otherwise FALSE. For combination SMM/DXE drivers, this will return FALSE in the DXE launch. For the
  SMM launch, it behaves as an SMM driver.

  @param[in]  This               The EFI_SMM_BASE2_PROTOCOL instance.
  @param[out] InSmram            Pointer to a Boolean which, on return, indicates that the driver is
                                 currently executing inside of SMRAM (TRUE) or outside of SMRAM (FALSE).

  @retval EFI_SUCCESS            The call returned successfully.
  @retval EFI_INVALID_PARAMETER  InSmram was NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_SMM_INSIDE_OUT2)(
  IN CONST EFI_SMM_BASE2_PROTOCOL  *This,
  OUT BOOLEAN                      *InSmram
  )
;

/**
  Returns the location of the System Management Service Table (SMST).

  This function returns the location of the System Management Service Table (SMST).  The use of the
  API is such that a driver can discover the location of the SMST in its entry point and then cache it in
  some driver global variable so that the SMST can be invoked in subsequent handlers.

  @param[in]     This            The EFI_SMM_BASE2_PROTOCOL instance.
  @param[in,out] Smst            On return, points to a pointer to the System Management Service Table (SMST).

  @retval EFI_SUCCESS            The operation was successful.
  @retval EFI_INVALID_PARAMETER  Smst was invalid.
  @retval EFI_UNSUPPORTED        Not in SMM.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_SMM_GET_SMST_LOCATION2)(
  IN CONST EFI_SMM_BASE2_PROTOCOL  *This,
  IN OUT EFI_SMM_SYSTEM_TABLE2     **Smst
  )
;

///
/// EFI SMM Base2 Protocol is utilized by all SMM drivers to locate the SMM infrastructure
/// services and determine whether the driver is being invoked inside SMRAM or outside of SMRAM.
///
struct _EFI_SMM_BASE2_PROTOCOL {
  EFI_SMM_INSIDE_OUT2           InSmm;
  EFI_SMM_GET_SMST_LOCATION2    GetSmstLocation;
};

extern EFI_GUID  gEfiSmmBase2ProtocolGuid;

#endif
