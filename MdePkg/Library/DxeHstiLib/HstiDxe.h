/** @file

  Copyright (c) 2015, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef _HSTI_DXE_H_
#define _HSTI_DXE_H_

#include <PiDxe.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>

#include <IndustryStandard/Hsti.h>

#include <Protocol/AdapterInformation.h>

#define HSTI_AIP_PRIVATE_SIGNATURE  SIGNATURE_32('H', 'S', 'T', 'I')

typedef struct {
  UINT32                            Signature;
  LIST_ENTRY                        Link;
  EFI_ADAPTER_INFORMATION_PROTOCOL  Aip;
  VOID                              *Hsti;
  UINTN                             HstiSize;
  UINTN                             HstiMaxSize;
} HSTI_AIP_PRIVATE_DATA;

#define HSTI_AIP_PRIVATE_DATA_FROM_THIS(a) \
  CR (a, \
      HSTI_AIP_PRIVATE_DATA, \
      Aip, \
      HSTI_AIP_PRIVATE_SIGNATURE \
      )

#define HSTI_DEFAULT_ERROR_STRING_LEN  255

extern EFI_ADAPTER_INFORMATION_PROTOCOL mAdapterInformationProtocol;

/**
  Return if input HSTI data follows HSTI specification.

  @param HstiData  HSTI data
  @param HstiSize  HSTI size

  @retval TRUE  HSTI data follows HSTI specification.
  @retval FALSE HSTI data does not follow HSTI specification.
**/
BOOLEAN
InternalHstiIsValidTable (
  IN VOID                     *HstiData,
  IN UINTN                    HstiSize
  );

#endif