/** @file
  MM IO Trap Dispatch Protocol as defined in PI 1.5 Specification
  Volume 4 Management Mode Core Interface.

  This protocol provides a parent dispatch service for IO trap MMI sources.

  Copyright (c) 2017, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This protocol is from PI Version 1.5.

**/

#ifndef _MM_IO_TRAP_DISPATCH_H_
#define _MM_IO_TRAP_DISPATCH_H_

#include <Pi/PiMmCis.h>

#define EFI_MM_IO_TRAP_DISPATCH_PROTOCOL_GUID \
  { \
    0x58dc368d, 0x7bfa, 0x4e77, {0xab, 0xbc, 0xe, 0x29, 0x41, 0x8d, 0xf9, 0x30 } \
  }

///
/// IO Trap valid types
///
typedef enum {
  WriteTrap,
  ReadTrap,
  ReadWriteTrap,
  IoTrapTypeMaximum
} EFI_MM_IO_TRAP_DISPATCH_TYPE;

///
/// IO Trap context structure containing information about the
/// IO trap event that should invoke the handler
///
typedef struct {
  UINT16                         Address;
  UINT16                         Length;
  EFI_MM_IO_TRAP_DISPATCH_TYPE   Type;
} EFI_MM_IO_TRAP_REGISTER_CONTEXT;

///
/// IO Trap context structure containing information about the IO trap that occurred
///
typedef struct {
  UINT32  WriteData;
} EFI_MM_IO_TRAP_CONTEXT;

typedef struct _EFI_MM_IO_TRAP_DISPATCH_PROTOCOL EFI_MM_IO_TRAP_DISPATCH_PROTOCOL;

/**
  Register an IO trap MMI child handler for a specified MMI.

  This service registers a function (DispatchFunction) which will be called when an MMI is
  generated because of an access to an I/O port specified by RegisterContext. On return,
  DispatchHandle contains a unique handle which may be used later to unregister the function
  using UnRegister(). If the base of the I/O range specified is zero, then an I/O range with the
  specified length and characteristics will be allocated and the Address field in RegisterContext
  updated. If no range could be allocated, then EFI_OUT_OF_RESOURCES will be returned.

  The service will not perform GCD allocation if the base address is non-zero or
  EFI_MM_READY_TO_LOCK has been installed.  In this case, the caller is responsible for the
  existence and allocation of the specific IO range.
  An error may be returned if some or all of the requested resources conflict with an existing IO trap
  child handler.

  It is not required that implementations will allow multiple children for a single IO trap MMI source.
  Some implementations may support multiple children.
  The DispatchFunction will be called with Context updated to contain information
  concerning the I/O action that actually happened and is passed in RegisterContext, with
  CommBuffer pointing to the data actually written and CommBufferSize pointing to the size of
  the data in CommBuffer.

  @param[in]  This               Pointer to the EFI_MM_IO_TRAP_DISPATCH_PROTOCOL instance.
  @param[in]  DispatchFunction   Function to register for handler when I/O trap location is accessed.
  @param[in]  RegisterContext    Pointer to the dispatch function's context.  The caller fills this
                                 context in before calling the register function to indicate to the register
                                 function the IO trap MMI source for which the dispatch function should be invoked.
  @param[out] DispatchHandle     Handle of the dispatch function, for when interfacing with the parent MM driver.

  @retval EFI_SUCCESS            The dispatch function has been successfully registered.
  @retval EFI_DEVICE_ERROR       The driver was unable to complete due to hardware error.
  @retval EFI_OUT_OF_RESOURCES   Insufficient resources are available to fulfill the IO trap range request.
  @retval EFI_INVALID_PARAMETER  RegisterContext is invalid.  The input value is not within a valid range.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_MM_IO_TRAP_DISPATCH_REGISTER)(
  IN CONST EFI_MM_IO_TRAP_DISPATCH_PROTOCOL    *This,
  IN       EFI_MM_HANDLER_ENTRY_POINT          DispatchFunction,
  IN OUT   EFI_MM_IO_TRAP_REGISTER_CONTEXT     *RegisterContext,
     OUT   EFI_HANDLE                          *DispatchHandle
  );

/**
  Unregister a child MMI source dispatch function with a parent MM driver.

  This service removes a previously installed child dispatch handler. This does not guarantee that the
  system resources will be freed from the GCD.

  @param[in] This                Pointer to the EFI_MM_IO_TRAP_DISPATCH_PROTOCOL instance.
  @param[in] DispatchHandle      Handle of the child service to remove.

  @retval EFI_SUCCESS            The dispatch function has been successfully unregistered.
  @retval EFI_INVALID_PARAMETER  The DispatchHandle was not valid.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_MM_IO_TRAP_DISPATCH_UNREGISTER)(
  IN CONST EFI_MM_IO_TRAP_DISPATCH_PROTOCOL    *This,
  IN       EFI_HANDLE                          DispatchHandle
  );

///
/// Interface structure for the MM IO Trap Dispatch Protocol.
///
/// This protocol provides a parent dispatch service for IO trap MMI sources.
///
struct _EFI_MM_IO_TRAP_DISPATCH_PROTOCOL {
  EFI_MM_IO_TRAP_DISPATCH_REGISTER    Register;
  EFI_MM_IO_TRAP_DISPATCH_UNREGISTER  UnRegister;
};

extern EFI_GUID gEfiMmIoTrapDispatchProtocolGuid;

#endif

