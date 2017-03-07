/** @file

  Copyright (c) 2015, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "HstiDxe.h"

/**
  Returns the current state information for the adapter.

  This function returns information of type InformationType from the adapter.
  If an adapter does not support the requested informational type, then
  EFI_UNSUPPORTED is returned. 

  @param[in]  This                   A pointer to the EFI_ADAPTER_INFORMATION_PROTOCOL instance.
  @param[in]  InformationType        A pointer to an EFI_GUID that defines the contents of InformationBlock.
  @param[out] InformationBlock       The service returns a pointer to the buffer with the InformationBlock
                                     structure which contains details about the data specific to InformationType.
  @param[out] InformationBlockSize   The driver returns the size of the InformationBlock in bytes.

  @retval EFI_SUCCESS                The InformationType information was retrieved.
  @retval EFI_UNSUPPORTED            The InformationType is not known.
  @retval EFI_DEVICE_ERROR           The device reported an error.
  @retval EFI_OUT_OF_RESOURCES       The request could not be completed due to a lack of resources.
  @retval EFI_INVALID_PARAMETER      This is NULL. 
  @retval EFI_INVALID_PARAMETER      InformationBlock is NULL. 
  @retval EFI_INVALID_PARAMETER      InformationBlockSize is NULL.

**/
EFI_STATUS
EFIAPI
HstiAipGetInfo (
  IN  EFI_ADAPTER_INFORMATION_PROTOCOL  *This,
  IN  EFI_GUID                          *InformationType,
  OUT VOID                              **InformationBlock,
  OUT UINTN                             *InformationBlockSize
  )
{
  HSTI_AIP_PRIVATE_DATA  *HstiAip;

  if ((This == NULL) || (InformationBlock == NULL) || (InformationBlockSize == NULL)) {
    return EFI_INVALID_PARAMETER;
  }
  if (!CompareGuid (InformationType, &gAdapterInfoPlatformSecurityGuid)) {
    return EFI_UNSUPPORTED;
  }

  HstiAip = HSTI_AIP_PRIVATE_DATA_FROM_THIS(This);

  *InformationBlock = AllocateCopyPool (HstiAip->HstiSize, HstiAip->Hsti);
  if (*InformationBlock == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  *InformationBlockSize = HstiAip->HstiSize;
  return EFI_SUCCESS;
}

/**
  Sets state information for an adapter.

  This function sends information of type InformationType for an adapter.
  If an adapter does not support the requested information type, then EFI_UNSUPPORTED
  is returned.

  @param[in]  This                   A pointer to the EFI_ADAPTER_INFORMATION_PROTOCOL instance.
  @param[in]  InformationType        A pointer to an EFI_GUID that defines the contents of InformationBlock.
  @param[in]  InformationBlock       A pointer to the InformationBlock structure which contains details
                                     about the data specific to InformationType.
  @param[in]  InformationBlockSize   The size of the InformationBlock in bytes.

  @retval EFI_SUCCESS                The information was received and interpreted successfully.
  @retval EFI_UNSUPPORTED            The InformationType is not known.
  @retval EFI_DEVICE_ERROR           The device reported an error.
  @retval EFI_INVALID_PARAMETER      This is NULL.
  @retval EFI_INVALID_PARAMETER      InformationBlock is NULL.
  @retval EFI_WRITE_PROTECTED        The InformationType cannot be modified using EFI_ADAPTER_INFO_SET_INFO().

**/
EFI_STATUS
EFIAPI
HstiAipSetInfo (
  IN  EFI_ADAPTER_INFORMATION_PROTOCOL  *This,
  IN  EFI_GUID                          *InformationType,
  IN  VOID                              *InformationBlock,
  IN  UINTN                             InformationBlockSize
  )
{
  HSTI_AIP_PRIVATE_DATA  *HstiAip;
  VOID                   *NewHsti;

  if ((This == NULL) || (InformationBlock == NULL)) {
    return EFI_INVALID_PARAMETER;
  }
  if (!CompareGuid (InformationType, &gAdapterInfoPlatformSecurityGuid)) {
    return EFI_UNSUPPORTED;
  }

  if (!InternalHstiIsValidTable (InformationBlock, InformationBlockSize)) {
    return EFI_VOLUME_CORRUPTED;
  }

  HstiAip = HSTI_AIP_PRIVATE_DATA_FROM_THIS(This);

  if (InformationBlockSize > HstiAip->HstiMaxSize) {
    NewHsti = AllocateZeroPool (InformationBlockSize);
    if (NewHsti == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }
    FreePool (HstiAip->Hsti);
    HstiAip->Hsti = NewHsti;
    HstiAip->HstiSize = 0;
    HstiAip->HstiMaxSize = InformationBlockSize;
  }

  CopyMem (HstiAip->Hsti, InformationBlock, InformationBlockSize);
  HstiAip->HstiSize = InformationBlockSize;
  return EFI_SUCCESS;
}

/**
  Get a list of supported information types for this instance of the protocol.

  This function returns a list of InformationType GUIDs that are supported on an
  adapter with this instance of EFI_ADAPTER_INFORMATION_PROTOCOL. The list is returned
  in InfoTypesBuffer, and the number of GUID pointers in InfoTypesBuffer is returned in
  InfoTypesBufferCount.

  @param[in]  This                  A pointer to the EFI_ADAPTER_INFORMATION_PROTOCOL instance.
  @param[out] InfoTypesBuffer       A pointer to the array of InformationType GUIDs that are supported
                                    by This.
  @param[out] InfoTypesBufferCount  A pointer to the number of GUIDs present in InfoTypesBuffer.

  @retval EFI_SUCCESS               The list of information type GUIDs that are supported on this adapter was
                                    returned in InfoTypesBuffer. The number of information type GUIDs was
                                    returned in InfoTypesBufferCount.
  @retval EFI_INVALID_PARAMETER     This is NULL.
  @retval EFI_INVALID_PARAMETER     InfoTypesBuffer is NULL.
  @retval EFI_INVALID_PARAMETER     InfoTypesBufferCount is NULL.
  @retval EFI_OUT_OF_RESOURCES      There is not enough pool memory to store the results.

**/
EFI_STATUS
EFIAPI
HstiAipGetSupportedTypes (
  IN  EFI_ADAPTER_INFORMATION_PROTOCOL  *This,
  OUT EFI_GUID                          **InfoTypesBuffer,
  OUT UINTN                             *InfoTypesBufferCount
  )
{
  if ((This == NULL) || (InfoTypesBuffer == NULL) || (InfoTypesBufferCount == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *InfoTypesBuffer = AllocateCopyPool (sizeof(gAdapterInfoPlatformSecurityGuid), &gAdapterInfoPlatformSecurityGuid);
  if (*InfoTypesBuffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  *InfoTypesBufferCount = 1;

  return EFI_SUCCESS;
}

EFI_ADAPTER_INFORMATION_PROTOCOL mAdapterInformationProtocol = {
  HstiAipGetInfo,
  HstiAipSetInfo,
  HstiAipGetSupportedTypes,
};
