/** @file
  Common definitions in the Platform Initialization Specification version 1.5
  VOLUME 4 Management Mode Core Interface version.

  Copyright (c) 2017, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _PI_MMCIS_H_
#define _PI_MMCIS_H_

#include <Pi/PiMultiPhase.h>
#include <Protocol/MmCpuIo.h>

typedef struct _EFI_MM_SYSTEM_TABLE  EFI_MM_SYSTEM_TABLE;

///
/// The Management Mode System Table (MMST) signature
///
#define MM_MMST_SIGNATURE            SIGNATURE_32 ('S', 'M', 'S', 'T')
///
/// The Management Mode System Table (MMST) revision is 1.6
///
#define MM_SPECIFICATION_MAJOR_REVISION  1
#define MM_SPECIFICATION_MINOR_REVISION  60
#define EFI_MM_SYSTEM_TABLE_REVISION    ((MM_SPECIFICATION_MAJOR_REVISION<<16) | (MM_SPECIFICATION_MINOR_REVISION))

/**
  Adds, updates, or removes a configuration table entry from the Management Mode System Table.

  The MmInstallConfigurationTable() function is used to maintain the list
  of configuration tables that are stored in the Management Mode System
  Table.  The list is stored as an array of (GUID, Pointer) pairs.  The list
  must be allocated from pool memory with PoolType set to EfiRuntimeServicesData.

  @param[in] SystemTable         A pointer to the MM System Table (MMST).
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
(EFIAPI *EFI_MM_INSTALL_CONFIGURATION_TABLE)(
  IN CONST EFI_MM_SYSTEM_TABLE    *SystemTable,
  IN CONST EFI_GUID               *Guid,
  IN VOID                         *Table,
  IN UINTN                        TableSize
  );

/**
  This service lets the caller to get one distinct application processor (AP) to execute
  a caller-provided code stream while in MM.

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
(EFIAPI *EFI_MM_STARTUP_THIS_AP)(
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
(EFIAPI *EFI_MM_NOTIFY_FN)(
  IN CONST EFI_GUID  *Protocol,
  IN VOID            *Interface,
  IN EFI_HANDLE      Handle
  );

/**
  Register a callback function be called when a particular protocol interface is installed.

  The MmRegisterProtocolNotify() function creates a registration Function that is to be
  called whenever a protocol interface is installed for Protocol by
  MmInstallProtocolInterface().
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
(EFIAPI *EFI_MM_REGISTER_PROTOCOL_NOTIFY)(
  IN  CONST EFI_GUID     *Protocol,
  IN  EFI_MM_NOTIFY_FN   Function,
  OUT VOID               **Registration
  );

/**
  Manage MMI of a particular type.

  @param[in]     HandlerType     Points to the handler type or NULL for root MMI handlers.
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
(EFIAPI *EFI_MM_INTERRUPT_MANAGE)(
  IN CONST EFI_GUID  *HandlerType,
  IN CONST VOID      *Context         OPTIONAL,
  IN OUT VOID        *CommBuffer      OPTIONAL,
  IN OUT UINTN       *CommBufferSize  OPTIONAL
  );

/**
  Main entry point for an MM handler dispatch or communicate-based callback.

  @param[in]     DispatchHandle  The unique handle assigned to this handler by MmiHandlerRegister().
  @param[in]     Context         Points to an optional handler context which was specified when the
                                 handler was registered.
  @param[in,out] CommBuffer      A pointer to a collection of data in memory that will
                                 be conveyed from a non-MM environment into an MM environment.
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
(EFIAPI *EFI_MM_HANDLER_ENTRY_POINT)(
  IN EFI_HANDLE  DispatchHandle,
  IN CONST VOID  *Context         OPTIONAL,
  IN OUT VOID    *CommBuffer      OPTIONAL,
  IN OUT UINTN   *CommBufferSize  OPTIONAL
  );

/**
  Registers a handler to execute within MM.

  @param[in]  Handler            Handler service function pointer.
  @param[in]  HandlerType        Points to the handler type or NULL for root MMI handlers.
  @param[out] DispatchHandle     On return, contains a unique handle which can be used to later
                                 unregister the handler function.

  @retval EFI_SUCCESS            MMI handler added successfully.
  @retval EFI_INVALID_PARAMETER  Handler is NULL or DispatchHandle is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_MM_INTERRUPT_REGISTER)(
  IN  EFI_MM_HANDLER_ENTRY_POINT    Handler,
  IN  CONST EFI_GUID                *HandlerType OPTIONAL,
  OUT EFI_HANDLE                    *DispatchHandle
  );

/**
  Unregister a handler in MM.

  @param[in] DispatchHandle      The handle that was specified when the handler was registered.

  @retval EFI_SUCCESS            Handler function was successfully unregistered.
  @retval EFI_INVALID_PARAMETER  DispatchHandle does not refer to a valid handle.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_MM_INTERRUPT_UNREGISTER)(
  IN EFI_HANDLE  DispatchHandle
  );

///
/// Processor information and functionality needed by MM Foundation.
///
typedef struct _EFI_MM_ENTRY_CONTEXT {
  EFI_MM_STARTUP_THIS_AP   MmStartupThisAp;
  ///
  /// A number between zero and the NumberOfCpus field. This field designates which
  /// processor is executing the MM Foundation.
  ///
  UINTN                    CurrentlyExecutingCpu;
  ///
  /// The number of possible processors in the platform.  This is a 1 based
  /// counter.  This does not indicate the number of processors that entered MM.
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
} EFI_MM_ENTRY_CONTEXT;

/**
  This function is the main entry point to the MM Foundation.

  @param[in] MmEntryContext  Processor information and functionality needed by MM Foundation.
**/
typedef
VOID
(EFIAPI *EFI_MM_ENTRY_POINT)(
  IN CONST EFI_MM_ENTRY_CONTEXT  *MmEntryContext
  );

///
/// Management Mode System Table (MMST)
///
/// The Management Mode System Table (MMST) is a table that contains a collection of common
/// services for managing MMRAM allocation and providing basic I/O services. These services are
/// intended for both preboot and runtime usage.
///
struct _EFI_MM_SYSTEM_TABLE {
  ///
  /// The table header for the SMST.
  ///
  EFI_TABLE_HEADER                     Hdr;
  ///
  /// A pointer to a NULL-terminated Unicode string containing the vendor name.
  /// It is permissible for this pointer to be NULL.
  ///
  CHAR16                               *MmFirmwareVendor;
  ///
  /// The particular revision of the firmware.
  ///
  UINT32                               MmFirmwareRevision;

  EFI_MM_INSTALL_CONFIGURATION_TABLE   MmInstallConfigurationTable;

  ///
  /// I/O Service
  ///
  EFI_MM_CPU_IO_PROTOCOL               MmIo;

  ///
  /// Runtime memory services
  ///
  EFI_ALLOCATE_POOL                    MmAllocatePool;
  EFI_FREE_POOL                        MmFreePool;
  EFI_ALLOCATE_PAGES                   MmAllocatePages;
  EFI_FREE_PAGES                       MmFreePages;

  ///
  /// MP service
  ///
  EFI_MM_STARTUP_THIS_AP               MmStartupThisAp;

  ///
  /// CPU information records
  ///

  ///
  /// A number between zero and and the NumberOfCpus field. This field designates
  /// which processor is executing the MM infrastructure.
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
  /// The number of UEFI Configuration Tables in the buffer MmConfigurationTable.
  ///
  UINTN                                NumberOfTableEntries;
  ///
  /// A pointer to the UEFI Configuration Tables. The number of entries in the table is
  /// NumberOfTableEntries.
  ///
  EFI_CONFIGURATION_TABLE              *MmConfigurationTable;

  ///
  /// Protocol services
  ///
  EFI_INSTALL_PROTOCOL_INTERFACE       MmInstallProtocolInterface;
  EFI_UNINSTALL_PROTOCOL_INTERFACE     MmUninstallProtocolInterface;
  EFI_HANDLE_PROTOCOL                  MmHandleProtocol;
  EFI_MM_REGISTER_PROTOCOL_NOTIFY      MmRegisterProtocolNotify;
  EFI_LOCATE_HANDLE                    MmLocateHandle;
  EFI_LOCATE_PROTOCOL                  MmLocateProtocol;

  ///
  /// MMI Management functions
  ///
  EFI_MM_INTERRUPT_MANAGE              MmiManage;
  EFI_MM_INTERRUPT_REGISTER            MmiHandlerRegister;
  EFI_MM_INTERRUPT_UNREGISTER          MmiHandlerUnRegister;
};

#endif
