/** @file MockSmmServicesTableLib.h
  Google Test mocks for SmmServicesTableLib

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MOCK_SMM_SERVICES_TABLE_LIB_H_
#define MOCK_SMM_SERVICES_TABLE_LIB_H_

#include <Library/GoogleTestLib.h>
#include <Library/FunctionMockLib.h>
extern "C" {
  #include <Uefi.h>
  #include <Library/SmmServicesTableLib.h>
}

//
// Declarations to handle usage of the SmmServicesTableLib by creating mock
//
struct MockSmmServicesTableLib {
  MOCK_INTERFACE_DECLARATION (MockSmmServicesTableLib);

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    gSmst_SmmAllocatePool,
    (
     IN  EFI_MEMORY_TYPE             PoolType,
     IN  UINTN                       Size,
     OUT VOID                        **Buffer
    )
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    gSmst_SmmFreePool,
    (
     IN  VOID                        *Buffer
    )
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    gSmst_SmmAllocatePages,
    (
     IN  EFI_ALLOCATE_TYPE           Type,
     IN  EFI_MEMORY_TYPE             MemoryType,
     IN  UINTN                       Pages,
     OUT EFI_PHYSICAL_ADDRESS        *Memory
    )
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    gSmst_SmmFreePages,
    (
     IN  EFI_PHYSICAL_ADDRESS        Memory,
     IN  UINTN                       Pages
    )
    );

  // MP service
  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    gSmst_SmmStartupThisAp,
    (
     IN     EFI_AP_PROCEDURE  Procedure,
     IN     UINTN             CpuNumber,
     IN OUT VOID              *ProcArguments OPTIONAL
    )
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    gSmst_SmmInstallProtocolInterface,
    (
     IN OUT EFI_HANDLE               *Handle,
     IN     EFI_GUID                 *Protocol,
     IN     EFI_INTERFACE_TYPE       InterfaceType,
     IN     VOID                     *Interface
    )
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    gSmst_SmmUninstallProtocolInterface,
    (
     IN EFI_HANDLE               Handle,
     IN EFI_GUID                 *Protocol,
     IN VOID                     *Interface
    )
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    gSmst_SmmHandleProtocol,
    (
     IN  EFI_HANDLE              Handle,
     IN  EFI_GUID                *Protocol,
     OUT VOID                    **Interface
    )
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    gSmst_SmmRegisterProtocolNotify,
    (
     IN  CONST EFI_GUID     *Protocol,
     IN  EFI_MM_NOTIFY_FN   Function,
     OUT VOID               **Registration
    )
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    gSmst_SmmLocateHandle,
    (
     IN     EFI_LOCATE_SEARCH_TYPE  SearchType,
     IN     EFI_GUID                *Protocol,
     IN     VOID                    *SearchKey,
     IN OUT UINTN                   *BufferSize,
     OUT    EFI_HANDLE              *Buffer
    )
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    gSmst_SmmLocateProtocol,
    (
     IN  EFI_GUID  *Protocol,
     IN  VOID      *Registration  OPTIONAL,
     OUT VOID      **Interface
    )
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    gSmst_SmiManage,
    (
     IN CONST EFI_GUID  *HandlerType,
     IN CONST VOID      *Context,
     IN OUT VOID        *CommBuffer,
     IN OUT UINTN       *CommBufferSize
    )
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    gSmst_SmmInterruptRegister,
    (
     IN  EFI_SMM_HANDLER_ENTRY_POINT2 Handler,
     IN  CONST EFI_GUID *HandlerType,
     OUT EFI_HANDLE    *DispatchHandle
    )
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    gSmst_SmmInterruptUnRegister,
    (
     IN EFI_HANDLE  DispatchHandle
    )
    );
};

#endif
