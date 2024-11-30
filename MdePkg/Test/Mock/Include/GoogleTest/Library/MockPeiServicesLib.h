/** @file
  Google Test mocks for PeiServicesLib

  Copyright (c) 2023, Intel Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MOCK_PEI_SERVICES_LIB_H_
#define MOCK_PEI_SERVICES_LIB_H_

#include <Library/GoogleTestLib.h>
#include <Library/FunctionMockLib.h>
extern "C" {
  #include <PiPei.h>
  #include <Uefi.h>
  #include <Library/PeiServicesLib.h>
}

struct MockPeiServicesLib {
  MOCK_INTERFACE_DECLARATION (MockPeiServicesLib);

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    PeiServicesInstallPpi,
    (IN CONST EFI_PEI_PPI_DESCRIPTOR  *PpiList)
    );
  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    PeiServicesReInstallPpi,
    (IN CONST EFI_PEI_PPI_DESCRIPTOR  *OldPpi,
     IN CONST EFI_PEI_PPI_DESCRIPTOR  *NewPpi)
    );
  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    PeiServicesLocatePpi,
    (IN CONST EFI_GUID              *Guid,
     IN UINTN                       Instance,
     IN OUT EFI_PEI_PPI_DESCRIPTOR  **PpiDescriptor  OPTIONAL,
     IN OUT VOID                    **Ppi)
    );
  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    PeiServicesNotifyPpi,
    (IN CONST EFI_PEI_NOTIFY_DESCRIPTOR  *NotifyList)
    );
  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    PeiServicesGetBootMode,
    (OUT EFI_BOOT_MODE  *BootMode)
    );
  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    PeiServicesSetBootMode,
    (IN EFI_BOOT_MODE  BootMode)
    );
  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    PeiServicesGetHobList,
    (OUT VOID  **HobList)
    );
  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    PeiServicesCreateHob,
    (IN UINT16  Type,
     IN UINT16  Length,
     OUT VOID   **Hob)
    );
  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    PeiServicesFfsFindNextVolume,
    (IN UINTN                  Instance,
     IN OUT EFI_PEI_FV_HANDLE  *VolumeHandle)
    );
  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    PeiServicesFfsFindNextFile,
    (IN EFI_FV_FILETYPE          SearchType,
     IN EFI_PEI_FV_HANDLE        VolumeHandle,
     IN OUT EFI_PEI_FILE_HANDLE  *FileHandle)
    );
  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    PeiServicesFfsFindSectionData,
    (IN EFI_SECTION_TYPE     SectionType,
     IN EFI_PEI_FILE_HANDLE  FileHandle,
     OUT VOID                **SectionData)
    );
  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    PeiServicesFfsFindSectionData3,
    (IN EFI_SECTION_TYPE     SectionType,
     IN UINTN                SectionInstance,
     IN EFI_PEI_FILE_HANDLE  FileHandle,
     OUT VOID                **SectionData,
     OUT UINT32              *AuthenticationStatus)
    );
  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    PeiServicesInstallPeiMemory,
    (IN EFI_PHYSICAL_ADDRESS  MemoryBegin,
     IN UINT64                MemoryLength)
    );
  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    PeiServicesAllocatePages,
    (IN EFI_MEMORY_TYPE        MemoryType,
     IN UINTN                  Pages,
     OUT EFI_PHYSICAL_ADDRESS  *Memory)
    );
  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    PeiServicesFreePages,
    (IN EFI_PHYSICAL_ADDRESS  Memory,
     IN UINTN                 Pages)
    );
  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    PeiServicesAllocatePool,
    (IN UINTN  Size,
     OUT VOID  **Buffer)
    );
  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    PeiServicesResetSystem,
    ()
    );
  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    PeiServicesFfsFindFileByName,
    (IN CONST  EFI_GUID             *FileName,
     IN CONST  EFI_PEI_FV_HANDLE    VolumeHandle,
     OUT       EFI_PEI_FILE_HANDLE  *FileHandle)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    PeiServicesFfsGetFileInfo,
    (IN CONST  EFI_PEI_FILE_HANDLE  FileHandle,
     OUT EFI_FV_FILE_INFO           *FileInfo)
    );
  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    PeiServicesFfsGetFileInfo2,
    (IN CONST  EFI_PEI_FILE_HANDLE  FileHandle,
     OUT EFI_FV_FILE_INFO2          *FileInfo)
    );
  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    PeiServicesFfsGetVolumeInfo,
    (IN  EFI_PEI_FV_HANDLE  VolumeHandle,
     OUT EFI_FV_INFO        *VolumeInfo)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    PeiServicesRegisterForShadow,
    (IN  EFI_PEI_FILE_HANDLE  FileHandle)
    );
  MOCK_FUNCTION_DECLARATION (
    VOID,
    PeiServicesInstallFvInfoPpi,
    (IN CONST EFI_GUID  *FvFormat  OPTIONAL,
     IN CONST VOID      *FvInfo,
     IN       UINT32    FvInfoSize,
     IN CONST EFI_GUID  *ParentFvName  OPTIONAL,
     IN CONST EFI_GUID  *ParentFileName OPTIONAL)
    );

  MOCK_FUNCTION_DECLARATION (
    VOID,
    PeiServicesInstallFvInfo2Ppi,
    (IN CONST EFI_GUID  *FvFormat  OPTIONAL,
     IN CONST VOID      *FvInfo,
     IN       UINT32    FvInfoSize,
     IN CONST EFI_GUID  *ParentFvName  OPTIONAL,
     IN CONST EFI_GUID  *ParentFileName  OPTIONAL,
     IN       UINT32    AuthenticationStatus)
    );
  MOCK_FUNCTION_DECLARATION (
    VOID,
    PeiServicesResetSystem2,
    (IN EFI_RESET_TYPE  ResetType,
     IN EFI_STATUS      ResetStatus,
     IN UINTN           DataSize,
     IN VOID            *ResetData OPTIONAL)
    );
};

#endif
