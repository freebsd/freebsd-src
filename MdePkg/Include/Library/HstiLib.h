/** @file
  Provides services to create, get and update HSTI table in AIP protocol.

  Copyright (c) 2015, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __HSTI_LIB_H__
#define __HSTI_LIB_H__

/**
  Publish HSTI table in AIP protocol.

  One system should have only one PLATFORM_SECURITY_ROLE_PLATFORM_REFERENCE.

  If the Role is NOT PLATFORM_SECURITY_ROLE_PLATFORM_REFERENCE,
  SecurityFeaturesRequired field will be ignored.

  @param Hsti      HSTI data
  @param HstiSize  HSTI size

  @retval EFI_SUCCESS          The HSTI data is published in AIP protocol.
  @retval EFI_ALREADY_STARTED  There is already HSTI table with Role and ImplementationID published in system.
  @retval EFI_VOLUME_CORRUPTED The input HSTI data does not follow HSTI specification.
  @retval EFI_OUT_OF_RESOURCES There is not enough system resource to publish HSTI data in AIP protocol.
**/
EFI_STATUS
EFIAPI
HstiLibSetTable (
  IN VOID                     *Hsti,
  IN UINTN                    HstiSize
  );

/**
  Search HSTI table in AIP protocol, and return the data.
  This API will return the HSTI table with indicated Role and ImplementationID,
  NULL ImplementationID means to find the first HSTI table with indicated Role.

  @param Role             Role of HSTI data.
  @param ImplementationID ImplementationID of HSTI data.
                          NULL means find the first one match Role.
  @param Hsti             HSTI data. This buffer is allocated by callee, and it
                          is the responsibility of the caller to free it after
                          using it.
  @param HstiSize         HSTI size

  @retval EFI_SUCCESS          The HSTI data in AIP protocol is returned.
  @retval EFI_NOT_FOUND        There is not HSTI table with the Role and ImplementationID published in system.
**/
EFI_STATUS
EFIAPI
HstiLibGetTable (
  IN UINT32                   Role,
  IN CHAR16                   *ImplementationID OPTIONAL,
  OUT VOID                    **Hsti,
  OUT UINTN                   *HstiSize
  );

/**
  Set FeaturesVerified in published HSTI table.
  This API will update the HSTI table with indicated Role and ImplementationID,
  NULL ImplementationID means to find the first HSTI table with indicated Role.

  @param Role             Role of HSTI data.
  @param ImplementationID ImplementationID of HSTI data.
                          NULL means find the first one match Role.
  @param ByteIndex        Byte index of FeaturesVerified of HSTI data.
  @param BitMask          Bit mask of FeaturesVerified of HSTI data.

  @retval EFI_SUCCESS          The FeaturesVerified of HSTI data updated in AIP protocol.
  @retval EFI_NOT_STARTED      There is not HSTI table with the Role and ImplementationID published in system.
  @retval EFI_UNSUPPORTED      The ByteIndex is invalid.
**/
EFI_STATUS
EFIAPI
HstiLibSetFeaturesVerified (
  IN UINT32                   Role,
  IN CHAR16                   *ImplementationID, OPTIONAL
  IN UINT32                   ByteIndex,
  IN UINT8                    BitMask
  );

/**
  Clear FeaturesVerified in published HSTI table.
  This API will update the HSTI table with indicated Role and ImplementationID,
  NULL ImplementationID means to find the first HSTI table with indicated Role.

  @param Role             Role of HSTI data.
  @param ImplementationID ImplementationID of HSTI data.
                          NULL means find the first one match Role.
  @param ByteIndex        Byte index of FeaturesVerified of HSTI data.
  @param BitMask          Bit mask of FeaturesVerified of HSTI data.

  @retval EFI_SUCCESS          The FeaturesVerified of HSTI data updated in AIP protocol.
  @retval EFI_NOT_STARTED      There is not HSTI table with the Role and ImplementationID published in system.
  @retval EFI_UNSUPPORTED      The ByteIndex is invalid.
**/
EFI_STATUS
EFIAPI
HstiLibClearFeaturesVerified (
  IN UINT32                   Role,
  IN CHAR16                   *ImplementationID, OPTIONAL
  IN UINT32                   ByteIndex,
  IN UINT8                    BitMask
  );

/**
  Append ErrorString in published HSTI table.
  This API will update the HSTI table with indicated Role and ImplementationID,
  NULL ImplementationID means to find the first HSTI table with indicated Role.

  @param Role             Role of HSTI data.
  @param ImplementationID ImplementationID of HSTI data.
                          NULL means find the first one match Role.
  @param ErrorString      ErrorString of HSTI data.

  @retval EFI_SUCCESS          The ErrorString of HSTI data is updated in AIP protocol.
  @retval EFI_NOT_STARTED      There is not HSTI table with the Role and ImplementationID published in system.
  @retval EFI_OUT_OF_RESOURCES There is not enough system resource to update ErrorString.
**/
EFI_STATUS
EFIAPI
HstiLibAppendErrorString (
  IN UINT32                   Role,
  IN CHAR16                   *ImplementationID, OPTIONAL
  IN CHAR16                   *ErrorString
  );

/**
  Set a new ErrorString in published HSTI table.
  This API will update the HSTI table with indicated Role and ImplementationID,
  NULL ImplementationID means to find the first HSTI table with indicated Role.

  @param Role             Role of HSTI data.
  @param ImplementationID ImplementationID of HSTI data.
                          NULL means find the first one match Role.
  @param ErrorString      ErrorString of HSTI data.

  @retval EFI_SUCCESS          The ErrorString of HSTI data is updated in AIP protocol.
  @retval EFI_NOT_STARTED      There is not HSTI table with the Role and ImplementationID published in system.
  @retval EFI_OUT_OF_RESOURCES There is not enough system resource to update ErrorString.
**/
EFI_STATUS
EFIAPI
HstiLibSetErrorString (
  IN UINT32                   Role,
  IN CHAR16                   *ImplementationID, OPTIONAL
  IN CHAR16                   *ErrorString
  );

#endif
