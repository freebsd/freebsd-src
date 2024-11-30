/** @file

  Copyright (c) 2015 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "HstiDxe.h"

/**
  Find HSTI table in AIP protocol, and return the data.
  This API will return the HSTI table with indicated Role and ImplementationID,
  NULL ImplementationID means to find the first HSTI table with indicated Role.

  @param Role             Role of HSTI data.
  @param ImplementationID ImplementationID of HSTI data.
                          NULL means find the first one match Role.
  @param HstiData         HSTI data. This buffer is allocated by callee, and it
                          is the responsibility of the caller to free it after
                          using it.
  @param HstiSize         HSTI size

  @return Aip             The AIP protocol having this HSTI.
  @return NULL            There is not HSTI table with the Role and ImplementationID published in system.
**/
VOID *
InternalHstiFindAip (
  IN UINT32  Role,
  IN CHAR16  *ImplementationID OPTIONAL,
  OUT VOID   **HstiData OPTIONAL,
  OUT UINTN  *HstiSize OPTIONAL
  )
{
  EFI_STATUS                        Status;
  EFI_ADAPTER_INFORMATION_PROTOCOL  *Aip;
  UINTN                             NoHandles;
  EFI_HANDLE                        *Handles;
  UINTN                             Index;
  EFI_GUID                          *InfoTypesBuffer;
  UINTN                             InfoTypesBufferCount;
  UINTN                             InfoTypesIndex;
  EFI_ADAPTER_INFORMATION_PROTOCOL  *AipCandidate;
  VOID                              *InformationBlock;
  UINTN                             InformationBlockSize;
  ADAPTER_INFO_PLATFORM_SECURITY    *Hsti;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiAdapterInformationProtocolGuid,
                  NULL,
                  &NoHandles,
                  &Handles
                  );
  if (EFI_ERROR (Status)) {
    return NULL;
  }

  Hsti                 = NULL;
  Aip                  = NULL;
  InformationBlock     = NULL;
  InformationBlockSize = 0;
  for (Index = 0; Index < NoHandles; Index++) {
    Status = gBS->HandleProtocol (
                    Handles[Index],
                    &gEfiAdapterInformationProtocolGuid,
                    (VOID **)&Aip
                    );
    if (EFI_ERROR (Status)) {
      continue;
    }

    //
    // Check AIP
    //
    Status = Aip->GetSupportedTypes (
                    Aip,
                    &InfoTypesBuffer,
                    &InfoTypesBufferCount
                    );
    if (EFI_ERROR (Status) || (InfoTypesBuffer == NULL) || (InfoTypesBufferCount == 0)) {
      continue;
    }

    AipCandidate = NULL;
    for (InfoTypesIndex = 0; InfoTypesIndex < InfoTypesBufferCount; InfoTypesIndex++) {
      if (CompareGuid (&InfoTypesBuffer[InfoTypesIndex], &gAdapterInfoPlatformSecurityGuid)) {
        AipCandidate = Aip;
        break;
      }
    }

    FreePool (InfoTypesBuffer);

    if (AipCandidate == NULL) {
      continue;
    }

    //
    // Check HSTI Role
    //
    Aip    = AipCandidate;
    Status = Aip->GetInformation (
                    Aip,
                    &gAdapterInfoPlatformSecurityGuid,
                    &InformationBlock,
                    &InformationBlockSize
                    );
    if (EFI_ERROR (Status)) {
      continue;
    }

    Hsti = InformationBlock;
    if ((Hsti->Role == Role) &&
        ((ImplementationID == NULL) || (StrCmp (ImplementationID, Hsti->ImplementationID) == 0)))
    {
      break;
    } else {
      Hsti = NULL;
      FreePool (InformationBlock);
      continue;
    }
  }

  FreePool (Handles);

  if (Hsti == NULL) {
    return NULL;
  }

  if (HstiData != NULL) {
    *HstiData = InformationBlock;
  }

  if (HstiSize != NULL) {
    *HstiSize = InformationBlockSize;
  }

  return Aip;
}

/**
  Return if input HSTI data follows HSTI specification.

  @param HstiData  HSTI data
  @param HstiSize  HSTI size

  @retval TRUE  HSTI data follows HSTI specification.
  @retval FALSE HSTI data does not follow HSTI specification.
**/
BOOLEAN
InternalHstiIsValidTable (
  IN VOID   *HstiData,
  IN UINTN  HstiSize
  )
{
  ADAPTER_INFO_PLATFORM_SECURITY  *Hsti;
  UINTN                           Index;
  CHAR16                          *ErrorString;
  CHAR16                          ErrorChar;
  UINTN                           ErrorStringSize;
  UINTN                           ErrorStringLength;

  Hsti = HstiData;

  //
  // basic check for header
  //
  if (HstiData == NULL) {
    DEBUG ((DEBUG_ERROR, "HstiData == NULL\n"));
    return FALSE;
  }

  if (HstiSize < sizeof (ADAPTER_INFO_PLATFORM_SECURITY)) {
    DEBUG ((DEBUG_ERROR, "HstiSize < sizeof(ADAPTER_INFO_PLATFORM_SECURITY)\n"));
    return FALSE;
  }

  if (((HstiSize - sizeof (ADAPTER_INFO_PLATFORM_SECURITY)) / 3) < Hsti->SecurityFeaturesSize) {
    DEBUG ((DEBUG_ERROR, "((HstiSize - sizeof(ADAPTER_INFO_PLATFORM_SECURITY)) / 3) < SecurityFeaturesSize\n"));
    return FALSE;
  }

  //
  // Check Version
  //
  if (Hsti->Version != PLATFORM_SECURITY_VERSION_VNEXTCS) {
    DEBUG ((DEBUG_ERROR, "Version != PLATFORM_SECURITY_VERSION_VNEXTCS\n"));
    return FALSE;
  }

  //
  // Check Role
  //
  if ((Hsti->Role < PLATFORM_SECURITY_ROLE_PLATFORM_REFERENCE) ||
      (Hsti->Role > PLATFORM_SECURITY_ROLE_IMPLEMENTOR_ODM))
  {
    DEBUG ((DEBUG_ERROR, "Role < PLATFORM_SECURITY_ROLE_PLATFORM_REFERENCE ||\n"));
    DEBUG ((DEBUG_ERROR, "Role > PLATFORM_SECURITY_ROLE_IMPLEMENTOR_ODM\n"));
    return FALSE;
  }

  //
  // Check ImplementationID
  //
  for (Index = 0; Index < sizeof (Hsti->ImplementationID)/sizeof (Hsti->ImplementationID[0]); Index++) {
    if (Hsti->ImplementationID[Index] == 0) {
      break;
    }
  }

  if (Index == sizeof (Hsti->ImplementationID)/sizeof (Hsti->ImplementationID[0])) {
    DEBUG ((DEBUG_ERROR, "ImplementationID has no NUL CHAR\n"));
    return FALSE;
  }

  ErrorStringSize = HstiSize - sizeof (ADAPTER_INFO_PLATFORM_SECURITY) - Hsti->SecurityFeaturesSize * 3;
  ErrorString     = (CHAR16 *)((UINTN)Hsti + sizeof (ADAPTER_INFO_PLATFORM_SECURITY) + Hsti->SecurityFeaturesSize * 3);

  //
  // basic check for ErrorString
  //
  if (ErrorStringSize == 0) {
    DEBUG ((DEBUG_ERROR, "ErrorStringSize == 0\n"));
    return FALSE;
  }

  if ((ErrorStringSize & BIT0) != 0) {
    DEBUG ((DEBUG_ERROR, "(ErrorStringSize & BIT0) != 0\n"));
    return FALSE;
  }

  //
  // ErrorString might not be CHAR16 aligned.
  //
  CopyMem (&ErrorChar, ErrorString, sizeof (ErrorChar));
  for (ErrorStringLength = 0; (ErrorChar != 0) && (ErrorStringLength < (ErrorStringSize/2)); ErrorStringLength++) {
    ErrorString++;
    CopyMem (&ErrorChar, ErrorString, sizeof (ErrorChar));
  }

  //
  // check the length of ErrorString
  //
  if (ErrorChar != 0) {
    DEBUG ((DEBUG_ERROR, "ErrorString has no NUL CHAR\n"));
    return FALSE;
  }

  if (ErrorStringLength == (ErrorStringSize/2)) {
    DEBUG ((DEBUG_ERROR, "ErrorString Length incorrect\n"));
    return FALSE;
  }

  return TRUE;
}

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
  IN VOID   *Hsti,
  IN UINTN  HstiSize
  )
{
  EFI_STATUS                        Status;
  EFI_HANDLE                        Handle;
  HSTI_AIP_PRIVATE_DATA             *HstiAip;
  EFI_ADAPTER_INFORMATION_PROTOCOL  *Aip;
  UINT32                            Role;
  CHAR16                            *ImplementationID;
  UINT32                            SecurityFeaturesSize;
  UINT8                             *SecurityFeaturesRequired;

  if (!InternalHstiIsValidTable (Hsti, HstiSize)) {
    return EFI_VOLUME_CORRUPTED;
  }

  Role             = ((ADAPTER_INFO_PLATFORM_SECURITY *)Hsti)->Role;
  ImplementationID = ((ADAPTER_INFO_PLATFORM_SECURITY *)Hsti)->ImplementationID;
  Aip              = InternalHstiFindAip (Role, ImplementationID, NULL, NULL);
  if (Aip != NULL) {
    return EFI_ALREADY_STARTED;
  }

  HstiAip = AllocateZeroPool (sizeof (HSTI_AIP_PRIVATE_DATA));
  if (HstiAip == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  HstiAip->Hsti = AllocateCopyPool (HstiSize, Hsti);
  if (HstiAip->Hsti == NULL) {
    FreePool (HstiAip);
    return EFI_OUT_OF_RESOURCES;
  }

  if (Role != PLATFORM_SECURITY_ROLE_PLATFORM_REFERENCE) {
    SecurityFeaturesRequired = (UINT8 *)HstiAip->Hsti + sizeof (ADAPTER_INFO_PLATFORM_SECURITY);
    SecurityFeaturesSize     = ((ADAPTER_INFO_PLATFORM_SECURITY *)Hsti)->SecurityFeaturesSize;
    ZeroMem (SecurityFeaturesRequired, SecurityFeaturesSize);
  }

  HstiAip->Signature = HSTI_AIP_PRIVATE_SIGNATURE;
  CopyMem (&HstiAip->Aip, &mAdapterInformationProtocol, sizeof (EFI_ADAPTER_INFORMATION_PROTOCOL));
  HstiAip->HstiSize    = HstiSize;
  HstiAip->HstiMaxSize = HstiSize;

  Handle = NULL;
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Handle,
                  &gEfiAdapterInformationProtocolGuid,
                  &HstiAip->Aip,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    FreePool (HstiAip->Hsti);
    FreePool (HstiAip);
  }

  return Status;
}

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
  IN UINT32  Role,
  IN CHAR16  *ImplementationID OPTIONAL,
  OUT VOID   **Hsti,
  OUT UINTN  *HstiSize
  )
{
  EFI_ADAPTER_INFORMATION_PROTOCOL  *Aip;

  Aip = InternalHstiFindAip (Role, ImplementationID, Hsti, HstiSize);
  if (Aip == NULL) {
    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
}

/**
  Record FeaturesVerified in published HSTI table.
  This API will update the HSTI table with indicated Role and ImplementationID,
  NULL ImplementationID means to find the first HSTI table with indicated Role.

  @param Role             Role of HSTI data.
  @param ImplementationID ImplementationID of HSTI data.
                          NULL means find the first one match Role.
  @param ByteIndex        Byte index of FeaturesVerified of HSTI data.
  @param BitMask          Bit mask of FeaturesVerified of HSTI data.
  @param Set              TRUE means to set the FeaturesVerified bit.
                          FALSE means to clear the FeaturesVerified bit.

  @retval EFI_SUCCESS          The FeaturesVerified of HSTI data updated in AIP protocol.
  @retval EFI_NOT_STARTED      There is not HSTI table with the Role and ImplementationID published in system.
  @retval EFI_UNSUPPORTED      The ByteIndex is invalid.
**/
EFI_STATUS
InternalHstiRecordFeaturesVerified (
  IN UINT32   Role,
  IN CHAR16   *ImplementationID  OPTIONAL,
  IN UINT32   ByteIndex,
  IN UINT8    Bit,
  IN BOOLEAN  Set
  )
{
  EFI_ADAPTER_INFORMATION_PROTOCOL  *Aip;
  ADAPTER_INFO_PLATFORM_SECURITY    *Hsti;
  UINTN                             HstiSize;
  UINT8                             *SecurityFeaturesVerified;
  EFI_STATUS                        Status;

  Aip = InternalHstiFindAip (Role, ImplementationID, (VOID **)&Hsti, &HstiSize);
  if (Aip == NULL) {
    return EFI_NOT_STARTED;
  }

  if (ByteIndex >= Hsti->SecurityFeaturesSize) {
    return EFI_UNSUPPORTED;
  }

  SecurityFeaturesVerified = (UINT8 *)((UINTN)Hsti + sizeof (ADAPTER_INFO_PLATFORM_SECURITY) + Hsti->SecurityFeaturesSize * 2);

  if (Set) {
    SecurityFeaturesVerified[ByteIndex] = (UINT8)(SecurityFeaturesVerified[ByteIndex] | (Bit));
  } else {
    SecurityFeaturesVerified[ByteIndex] = (UINT8)(SecurityFeaturesVerified[ByteIndex] & (~Bit));
  }

  Status = Aip->SetInformation (
                  Aip,
                  &gAdapterInfoPlatformSecurityGuid,
                  Hsti,
                  HstiSize
                  );
  FreePool (Hsti);
  return Status;
}

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
  IN UINT32  Role,
  IN CHAR16  *ImplementationID  OPTIONAL,
  IN UINT32  ByteIndex,
  IN UINT8   BitMask
  )
{
  return InternalHstiRecordFeaturesVerified (
           Role,
           ImplementationID,
           ByteIndex,
           BitMask,
           TRUE
           );
}

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
  IN UINT32  Role,
  IN CHAR16  *ImplementationID  OPTIONAL,
  IN UINT32  ByteIndex,
  IN UINT8   BitMask
  )
{
  return InternalHstiRecordFeaturesVerified (
           Role,
           ImplementationID,
           ByteIndex,
           BitMask,
           FALSE
           );
}

/**
  Record ErrorString in published HSTI table.
  This API will update the HSTI table with indicated Role and ImplementationID,
  NULL ImplementationID means to find the first HSTI table with indicated Role.

  @param Role             Role of HSTI data.
  @param ImplementationID ImplementationID of HSTI data.
                          NULL means find the first one match Role.
  @param ErrorString      ErrorString of HSTI data.
  @param Append           TRUE means to append the ErrorString to HSTI table.
                          FALSE means to set the ErrorString in HSTI table.

  @retval EFI_SUCCESS          The ErrorString of HSTI data is published in AIP protocol.
  @retval EFI_NOT_STARTED      There is not HSTI table with the Role and ImplementationID published in system.
  @retval EFI_OUT_OF_RESOURCES There is not enough system resource to update ErrorString.
**/
EFI_STATUS
InternalHstiRecordErrorString (
  IN UINT32   Role,
  IN CHAR16   *ImplementationID  OPTIONAL,
  IN CHAR16   *ErrorString,
  IN BOOLEAN  Append
  )
{
  EFI_ADAPTER_INFORMATION_PROTOCOL  *Aip;
  ADAPTER_INFO_PLATFORM_SECURITY    *Hsti;
  UINTN                             HstiSize;
  UINTN                             StringSize;
  VOID                              *NewHsti;
  UINTN                             NewHstiSize;
  UINTN                             Offset;
  EFI_STATUS                        Status;

  Aip = InternalHstiFindAip (Role, ImplementationID, (VOID **)&Hsti, &HstiSize);
  if (Aip == NULL) {
    return EFI_NOT_STARTED;
  }

  if (Append) {
    Offset = HstiSize - sizeof (CHAR16);
  } else {
    Offset = sizeof (ADAPTER_INFO_PLATFORM_SECURITY) + Hsti->SecurityFeaturesSize * 3;
  }

  StringSize = StrSize (ErrorString);

  NewHstiSize = Offset + StringSize;
  NewHsti     = AllocatePool (NewHstiSize);
  if (NewHsti == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  CopyMem (NewHsti, Hsti, Offset);
  CopyMem ((UINT8 *)NewHsti + Offset, ErrorString, StringSize);

  Status = Aip->SetInformation (
                  Aip,
                  &gAdapterInfoPlatformSecurityGuid,
                  NewHsti,
                  NewHstiSize
                  );
  FreePool (Hsti);
  FreePool (NewHsti);
  return Status;
}

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
  IN UINT32  Role,
  IN CHAR16  *ImplementationID  OPTIONAL,
  IN CHAR16  *ErrorString
  )
{
  return InternalHstiRecordErrorString (
           Role,
           ImplementationID,
           ErrorString,
           TRUE
           );
}

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
  IN UINT32  Role,
  IN CHAR16  *ImplementationID  OPTIONAL,
  IN CHAR16  *ErrorString
  )
{
  return InternalHstiRecordErrorString (
           Role,
           ImplementationID,
           ErrorString,
           FALSE
           );
}
