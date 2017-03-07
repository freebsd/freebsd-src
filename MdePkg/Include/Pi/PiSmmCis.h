/** @file
  Common definitions in the Platform Initialization Specification version 1.4a
  VOLUME 4 System Management Mode Core Interface version.

  Copyright (c) 2009 - 2016, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef _PI_SMMCIS_H_
#define _PI_SMMCIS_H_

#include <Pi/PiMultiPhase.h>
#include <Protocol/SmmCpuIo2.h>

typedef struct _EFI_SMM_SYSTEM_TABLE2  EFI_SMM_SYSTEM_TABLE2;

///
/// The System Management System Table (SMST) signature
///
#define SMM_SMST_SIGNATURE            SIGNATURE_32 ('S', 'M', 'S', 'T')
///
/// The System Management System Table (SMST) revision is 1.4
///
#define SMM_SPECIFICATION_MAJOR_REVISION  1
#define SMM_SPECIFICATION_MINOR_REVISION  40
#define EFI_SMM_SYSTEM_TABLE2_REVISION    ((SMM_SPECIFICATION_MAJOR_REVISION<<16) | (SMM_SPECIFICATION_MINOR_REVISION))

/**
  Adds, updates, or removes a configuration table entry from the System Management System Table.

  The SmmInstallConfigurationTable() function is used to maintain the list
  of configuration tables that are stored in the System Management System
  Table.  The list is stored as an array of (GUID, Pointer) pairs.  The list
  must be allocated from pool memory with PoolType set to EfiRuntimeServicesData.

  @param[in] SystemTable         A pointer to the SMM System Table (SMST).
  @param[in] Guid                A pointer to the GUID for the entry to add, update, or remove.
  @param[in] Table               A pointer to the buffer of the table to add.
  @param[in] TableSize           The size of the table to install.

  @retval EFI_SUCCESS            The (Guid, Table) pair was added, updated, or removed.
  @retval EFI_INVALID_PARAMETER  Guid is not valid.
  @retval EFI_NOT_FOUND          An attempt was made to delete a non-existent entry.
  @retval EFI_OUT_OF_RESOURCES   There is not enough memory available to complete the operation.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_SMM_INSTALL_CONFIGURATION_TABLE2)(
  IN CONST EFI_SMM_SYSTEM_TABLE2  *SystemTable,
  IN CONST EFI_GUID               *Guid,
  IN VOID                         *Table,
  IN UINTN                        TableSize
  );

/**
  This service lets the caller to get one distinct application processor (AP) to execute
  a caller-provided code stream while in SMM.

  @param[in]     Procedure       A pointer to the code stream to be run on the designated
                                 AP of the system.
  @param[in]     CpuNumber       The zero-based index of the processor number of the AP
                                 on which the code stream is supposed to run.
  @param[in,out] ProcArguments   Allows the caller to pass a list of parameters to the code
                                 that is run by the AP.

  @retval EFI_SUCCESS            The call was successful and the return parameters are valid.
  @retval EFI_INVALID_PARAMETER  The input arguments are out of range.
  @retval EFI_INVALID_PARAMETER  The CPU requested is not available on this SMI invocation.
  @retval EFI_INVALID_PARAMETER  The CPU cannot support an additional service invocation.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_SMM_STARTUP_THIS_AP)(
  IN EFI_AP_PROCEDURE  Procedure,
  IN UINTN             CpuNumber,
  IN OUT VOID          *ProcArguments OPTIONAL
  );

/**
  Function prototype for protocol install notification.

  @param[in] Protocol   Points to the protocol's unique identifier.
  @param[in] Interface  Points to the interface instance.
  @param[in] Handle     The handle on which the interface was installed.

  @return Status Code
**/
typedef
EFI_STATUS
(EFIAPI *EFI_SMM_NOTIFY_FN)(
  IN CONST EFI_GUID  *Protocol,
  IN VOID            *Interface,
  IN EFI_HANDLE      Handle
  );

/**
  Register a callback function be called when a particular protocol interface is installed.

  The SmmRegisterProtocolNotify() function creates a registration Function that is to be 
  called whenever a protocol interface is installed for Protocol by 
  SmmInstallProtocolInterface().
  If Function == NULL and Registration is an existing registration, then the callback is unhooked.

  @param[in]  Protocol          The unique ID of the protocol for which the event is to be registered.
  @param[in]  Function          Points to the notification function.
  @param[out] Registration      A pointer to a memory location to receive the registration value.

  @retval EFI_SUCCESS           Successfully returned the registration record
                                that has been added or unhooked.
  @retval EFI_INVALID_PARAMETER Protocol is NULL or Registration is NULL.
  @retval EFI_OUT_OF_RESOURCES  Not enough memory resource to finish the request.
  @retval EFI_NOT_FOUND         If the registration is not found when Function == NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_SMM_REGISTER_PROTOCOL_NOTIFY)(
  IN  CONST EFI_GUID     *Protocol,
  IN  EFI_SMM_NOTIFY_FN  Function,
  OUT VOID               **Registration
  );

/**
  Manage SMI of a particular type.

  @param[in]     HandlerType     Points to the handler type or NULL for root SMI handlers.
  @param[in]     Context         Points to an optional context buffer.
  @param[in,out] CommBuffer      Points to the optional communication buffer.
  @param[in,out] CommBufferSize  Points to the size of the optional communication buffer.

  @retval EFI_WARN_INTERRUPT_SOURCE_PENDING  Interrupt source was processed successfully but not quiesced.
  @retval EFI_INTERRUPT_PENDING              One or more SMI sources could not be quiesced.
  @retval EFI_NOT_FOUND                      Interrupt source was not handled or quiesced.
  @retval EFI_SUCCESS                        Interrupt source was handled and quiesced.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_SMM_INTERRUPT_MANAGE)(
  IN CONST EFI_GUID  *HandlerType,
  IN CONST VOID      *Context         OPTIONAL,
  IN OUT VOID        *CommBuffer      OPTIONAL,
  IN OUT UINTN       *CommBufferSize  OPTIONAL
  );

/**
  Main entry point for an SMM handler dispatch or communicate-based callback.

  @param[in]     DispatchHandle  The unique handle assigned to this handler by SmiHandlerRegister().
  @param[in]     Context         Points to an optional handler context which was specified when the
                                 handler was registered.
  @param[in,out] CommBuffer      A pointer to a collection of data in memory that will
                                 be conveyed from a non-SMM environment into an SMM environment.
  @param[in,out] CommBufferSize  The size of the CommBuffer.

  @retval EFI_SUCCESS                         The interrupt was handled and quiesced. No other handlers 
                                              should still be called.
  @retval EFI_WARN_INTERRUPT_SOURCE_QUIESCED  The interrupt has been quiesced but other handlers should 
                                              still be called.
  @retval EFI_WARN_INTERRUPT_SOURCE_PENDING   The interrupt is still pending and other handlers should still 
                                              be called.
  @retval EFI_INTERRUPT_PENDING               The interrupt could not be quiesced.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_SMM_HANDLER_ENTRY_POINT2)(
  IN EFI_HANDLE  DispatchHandle,
  IN CONST VOID  *Context         OPTIONAL,
  IN OUT VOID    *CommBuffer      OPTIONAL,
  IN OUT UINTN   *CommBufferSize  OPTIONAL
  );

/**
  Registers a handler to execute within SMM.

  @param[in]  Handler            Handler service function pointer.
  @param[in]  HandlerType        Points to the handler type or NULL for root SMI handlers.
  @param[out] DispatchHandle     On return, contains a unique handle which can be used to later
                                 unregister the handler function.

  @retval EFI_SUCCESS            SMI handler added successfully.
  @retval EFI_INVALID_PARAMETER  Handler is NULL or DispatchHandle is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_SMM_INTERRUPT_REGISTER)(
  IN  EFI_SMM_HANDLER_ENTRY_POINT2  Handler,
  IN  CONST EFI_GUID                *HandlerType OPTIONAL,
  OUT EFI_HANDLE                    *DispatchHandle
  );

/**
  Unregister a handler in SMM.

  @param[in] DispatchHandle      The handle that was specified when the handler was registered.

  @retval EFI_SUCCESS            Handler function was successfully unregistered.
  @retval EFI_INVALID_PARAMETER  DispatchHandle does not refer to a valid handle.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_SMM_INTERRUPT_UNREGISTER)(
  IN EFI_HANDLE  DispatchHandle
  );

///
/// Processor information and functionality needed by SMM Foundation.
///
typedef struct _EFI_SMM_ENTRY_CONTEXT {
  EFI_SMM_STARTUP_THIS_AP  SmmStartupThisAp;
  ///
  /// A number between zero and the NumberOfCpus field. This field designates which 
  /// processor is executing the SMM Foundation.
  ///
  UINTN                    CurrentlyExecutingCpu;
  ///
  /// The number of possible processors in the platform.  This is a 1 based 
  /// counter.  This does not indicate the number of processors that entered SMM.
  ///
  UINTN                    NumberOfCpus;
  ///
  /// Points to an array, where each element describes the number of bytes in the 
  /// corresponding save state specified by CpuSaveState. There are always 
  /// NumberOfCpus entries in the array. 
  ///
  UINTN                    *CpuSaveStateSize;
  ///
  /// Points to an array, where each element is a pointer to a CPU save state. The 
  /// corresponding element in CpuSaveStateSize specifies the number of bytes in the 
  /// save state area. There are always NumberOfCpus entries in the array.
  ///
  VOID                     **CpuSaveState;
} EFI_SMM_ENTRY_CONTEXT;

/**
  This function is the main entry point to the SMM Foundation.

  @param[in] SmmEntryContext  Processor information and functionality needed by SMM Foundation.
**/
typedef
VOID
(EFIAPI *EFI_SMM_ENTRY_POINT)(
  IN CONST EFI_SMM_ENTRY_CONTEXT  *SmmEntryContext
  );

///
/// System Management System Table (SMST)
///
/// The System Management System Table (SMST) is a table that contains a collection of common 
/// services for managing SMRAM allocation and providing basic I/O services. These services are 
/// intended for both preboot and runtime usage.
///
struct _EFI_SMM_SYSTEM_TABLE2 {
  ///
  /// The table header for the SMST.
  ///
  EFI_TABLE_HEADER                     Hdr;
  ///
  /// A pointer to a NULL-terminated Unicode string containing the vendor name.
  /// It is permissible for this pointer to be NULL.
  ///
  CHAR16                               *SmmFirmwareVendor;
  ///
  /// The particular revision of the firmware.
  ///
  UINT32                               SmmFirmwareRevision;

  EFI_SMM_INSTALL_CONFIGURATION_TABLE2 SmmInstallConfigurationTable;

  ///
  /// I/O Service
  ///
  EFI_SMM_CPU_IO2_PROTOCOL             SmmIo;

  ///
  /// Runtime memory services
  ///
  EFI_ALLOCATE_POOL                    SmmAllocatePool;
  EFI_FREE_POOL                        SmmFreePool;
  EFI_ALLOCATE_PAGES                   SmmAllocatePages;
  EFI_FREE_PAGES                       SmmFreePages;

  ///
  /// MP service
  ///
  EFI_SMM_STARTUP_THIS_AP              SmmStartupThisAp;

  ///
  /// CPU information records
  ///

  ///
  /// A number between zero and and the NumberOfCpus field. This field designates 
  /// which processor is executing the SMM infrastructure.
  ///
  UINTN                                CurrentlyExecutingCpu;
  ///
  /// The number of possible processors in the platform.  This is a 1 based counter.
  ///
  UINTN                                NumberOfCpus;
  ///
  /// Points to an array, where each element describes the number of bytes in the 
  /// corresponding save state specified by CpuSaveState. There are always 
  /// NumberOfCpus entries in the array. 
  ///
  UINTN                                *CpuSaveStateSize;
  ///
  /// Points to an array, where each element is a pointer to a CPU save state. The 
  /// corresponding element in CpuSaveStateSize specifies the number of bytes in the 
  /// save state area. There are always NumberOfCpus entries in the array.
  ///
  VOID                                 **CpuSaveState;

  ///
  /// Extensibility table
  ///

  ///
  /// The number of UEFI Configuration Tables in the buffer SmmConfigurationTable.
  ///
  UINTN                                NumberOfTableEntries;
  ///
  /// A pointer to the UEFI Configuration Tables. The number of entries in the table is 
  /// NumberOfTableEntries. 
  ///
  EFI_CONFIGURATION_TABLE              *SmmConfigurationTable;

  ///
  /// Protocol services
  ///
  EFI_INSTALL_PROTOCOL_INTERFACE       SmmInstallProtocolInterface;
  EFI_UNINSTALL_PROTOCOL_INTERFACE     SmmUninstallProtocolInterface;
  EFI_HANDLE_PROTOCOL                  SmmHandleProtocol;
  EFI_SMM_REGISTER_PROTOCOL_NOTIFY     SmmRegisterProtocolNotify;
  EFI_LOCATE_HANDLE                    SmmLocateHandle;
  EFI_LOCATE_PROTOCOL                  SmmLocateProtocol;

  ///
  /// SMI Management functions
  ///
  EFI_SMM_INTERRUPT_MANAGE             SmiManage;
  EFI_SMM_INTERRUPT_REGISTER           SmiHandlerRegister;
  EFI_SMM_INTERRUPT_UNREGISTER         SmiHandlerUnRegister;
};

#endif
