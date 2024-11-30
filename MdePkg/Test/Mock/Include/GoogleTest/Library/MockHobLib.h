/** @file
  Google Test mocks for HobLib

  Copyright (c) 2023, Intel Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MOCK_HOB_LIB_H_
#define MOCK_HOB_LIB_H_

#include <Library/GoogleTestLib.h>
#include <Library/FunctionMockLib.h>
extern "C" {
  #include <Pi/PiMultiPhase.h>
  #include <Uefi.h>
  #include <Library/HobLib.h>
}

struct MockHobLib {
  MOCK_INTERFACE_DECLARATION (MockHobLib);

  MOCK_FUNCTION_DECLARATION (
    VOID *,
    GetHobList,
    ()
    );
  MOCK_FUNCTION_DECLARATION (
    VOID *,
    GetNextHob,
    (IN UINT16      Type,
     IN CONST VOID  *HobStart)
    );
  MOCK_FUNCTION_DECLARATION (
    VOID *,
    GetFirstHob,
    (IN UINT16      Type)
    );
  MOCK_FUNCTION_DECLARATION (
    VOID *,
    GetNextGuidHob,
    (IN CONST EFI_GUID  *Guid,
     IN CONST VOID      *HobStart)
    );
  MOCK_FUNCTION_DECLARATION (
    VOID *,
    GetFirstGuidHob,
    (IN CONST EFI_GUID  *Guid)
    );
  MOCK_FUNCTION_DECLARATION (
    EFI_BOOT_MODE,
    GetBootModeHob,
    ()
    );
  MOCK_FUNCTION_DECLARATION (
    VOID,
    BuildModuleHob,
    (IN CONST EFI_GUID        *ModuleName,
     IN EFI_PHYSICAL_ADDRESS  MemoryAllocationModule,
     IN UINT64                ModuleLength,
     IN EFI_PHYSICAL_ADDRESS  EntryPoint)
    );
  MOCK_FUNCTION_DECLARATION (
    VOID,
    BuildResourceDescriptorWithOwnerHob,
    (IN EFI_RESOURCE_TYPE            ResourceType,
     IN EFI_RESOURCE_ATTRIBUTE_TYPE  ResourceAttribute,
     IN EFI_PHYSICAL_ADDRESS         PhysicalStart,
     IN UINT64                       NumberOfBytes,
     IN EFI_GUID                     *OwnerGUID)
    );
  MOCK_FUNCTION_DECLARATION (
    VOID,
    BuildResourceDescriptorHob,
    (IN EFI_RESOURCE_TYPE            ResourceType,
     IN EFI_RESOURCE_ATTRIBUTE_TYPE  ResourceAttribute,
     IN EFI_PHYSICAL_ADDRESS         PhysicalStart,
     IN UINT64                       NumberOfBytes)
    );
  MOCK_FUNCTION_DECLARATION (
    VOID *,
    BuildGuidHob,
    (IN CONST EFI_GUID  *Guid,
     IN UINTN           DataLength)
    );
  MOCK_FUNCTION_DECLARATION (
    VOID *,
    BuildGuidDataHob,
    (IN CONST EFI_GUID  *Guid,
     IN VOID            *Data,
     IN UINTN           DataLength)
    );
  MOCK_FUNCTION_DECLARATION (
    VOID,
    BuildFvHob,
    (IN EFI_PHYSICAL_ADDRESS  BaseAddress,
     IN UINT64                Length)
    );
  MOCK_FUNCTION_DECLARATION (
    VOID,
    BuildFv2Hob,
    (IN          EFI_PHYSICAL_ADDRESS  BaseAddress,
     IN          UINT64                Length,
     IN CONST    EFI_GUID              *FvName,
     IN CONST    EFI_GUID              *FileName)
    );
  MOCK_FUNCTION_DECLARATION (
    VOID,
    BuildFv3Hob,
    (IN          EFI_PHYSICAL_ADDRESS  BaseAddress,
     IN          UINT64                Length,
     IN          UINT32                AuthenticationStatus,
     IN          BOOLEAN               ExtractedFv,
     IN CONST    EFI_GUID              *FvName  OPTIONAL,
     IN CONST    EFI_GUID              *FileName OPTIONAL)
    );
  MOCK_FUNCTION_DECLARATION (
    VOID,
    BuildCvHob,
    (IN EFI_PHYSICAL_ADDRESS  BaseAddress,
     IN UINT64                Length)
    );
  MOCK_FUNCTION_DECLARATION (
    VOID,
    BuildCpuHob,
    (IN UINT8  SizeOfMemorySpace,
     IN UINT8  SizeOfIoSpace)
    );
  MOCK_FUNCTION_DECLARATION (
    VOID,
    BuildStackHob,
    (IN EFI_PHYSICAL_ADDRESS  BaseAddress,
     IN UINT64                Length)
    );
  MOCK_FUNCTION_DECLARATION (
    VOID,
    BuildBspStoreHob,
    (IN EFI_PHYSICAL_ADDRESS  BaseAddress,
     IN UINT64                Length,
     IN EFI_MEMORY_TYPE       MemoryType)
    );
  MOCK_FUNCTION_DECLARATION (
    VOID,
    BuildMemoryAllocationHob,
    (IN EFI_PHYSICAL_ADDRESS  BaseAddress,
     IN UINT64                Length,
     IN EFI_MEMORY_TYPE       MemoryType)
    );
};

#endif
