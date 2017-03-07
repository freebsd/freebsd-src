/** @file
  SMM IO Trap Dispatch2 Protocol as defined in PI 1.1 Specification
  Volume 4 System Management Mode Core Interface.

  This protocol provides a parent dispatch service for IO trap SMI sources.

  Copyright (c) 2009, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  @par Revision Reference:
  This protocol is from PI Version 1.1.

**/

#ifndef _SMM_IO_TRAP_DISPATCH2_H_
#define _SMM_IO_TRAP_DISPATCH2_H_

#include <Pi/PiSmmCis.h>

#define EFI_SMM_IO_TRAP_DISPATCH2_PROTOCOL_GUID \
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
} EFI_SMM_IO_TRAP_DISPATCH_TYPE;

///
/// IO Trap context structure containing information about the
/// IO trap event that should invoke the handler
///
typedef struct {
  UINT16                         Address;
  UINT16                         Length;
  EFI_SMM_IO_TRAP_DISPATCH_TYPE  Type;
} EFI_SMM_IO_TRAP_REGISTER_CONTEXT;

///
/// IO Trap context structure containing information about the IO trap that occurred
///
typedef struct {
  UINT32  WriteData;
} EFI_SMM_IO_TRAP_CONTEXT;

typedef struct _EFI_SMM_IO_TRAP_DISPATCH2_PROTOCOL EFI_SMM_IO_TRAP_DISPATCH2_PROTOCOL;

/**
  Register an IO trap SMI child handler for a specified SMI.

  This service registers a function (DispatchFunction) which will be called when an SMI is 
  generated because of an access to an I/O port specified by RegisterContext. On return, 
  DispatchHandle contains a unique handle which may be used later to unregister the function 
  using UnRegister(). If the base of the I/O range specified is zero, then an I/O range with the 
  specified length and characteristics will be allocated and the Address field in RegisterContext 
  updated. If no range could be allocated, then EFI_OUT_OF_RESOURCES will be returned. 

  The service will not perform GCD allocation if the base address is non-zero or 
  EFI_SMM_READY_TO_LOCK has been installed.  In this case, the caller is responsible for the 
  existence and allocation of the specific IO range.
  An error may be returned if some or all of the requested resources conflict with an existing IO trap 
  child handler.

  It is not required that implementations will allow multiple children for a single IO trap SMI source.  
  Some implementations may support multiple children.
  The DispatchFunction will be called with Context updated to contain information 
  concerning the I/O action that actually happened and is passed in RegisterContext, with 
  CommBuffer pointing to the data actually written and CommBufferSize pointing to the size of 
  the data in CommBuffer.

  @param[in]  This               Pointer to the EFI_SMM_IO_TRAP_DISPATCH2_PROTOCOL instance.
  @param[in]  DispatchFunction   Function to register for handler when I/O trap location is accessed.
  @param[in]  RegisterContext    Pointer to the dispatch function's context.  The caller fills this
                                 context in before calling the register function to indicate to the register
                                 function the IO trap SMI source for which the dispatch function should be invoked.
  @param[out] DispatchHandle     Handle of the dispatch function, for when interfacing with the parent SMM driver.

  @retval EFI_SUCCESS            The dispatch function has been successfully registered.
  @retval EFI_DEVICE_ERROR       The driver was unable to complete due to hardware error.
  @retval EFI_OUT_OF_RESOURCES   Insufficient resources are available to fulfill the IO trap range request.
  @retval EFI_INVALID_PARAMETER  RegisterContext is invalid.  The input value is not within a valid range.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_SMM_IO_TRAP_DISPATCH2_REGISTER)(
  IN CONST EFI_SMM_IO_TRAP_DISPATCH2_PROTOCOL  *This,
  IN       EFI_SMM_HANDLER_ENTRY_POINT2        DispatchFunction,
  IN OUT   EFI_SMM_IO_TRAP_REGISTER_CONTEXT    *RegisterContext,
     OUT   EFI_HANDLE                          *DispatchHandle
  );

/**
  Unregister a child SMI source dispatch function with a parent SMM driver.

  This service removes a previously installed child dispatch handler. This does not guarantee that the 
  system resources will be freed from the GCD.

  @param[in] This                Pointer to the EFI_SMM_IO_TRAP_DISPATCH2_PROTOCOL instance. 
  @param[in] DispatchHandle      Handle of the child service to remove.

  @retval EFI_SUCCESS            The dispatch function has been successfully unregistered.
  @retval EFI_INVALID_PARAMETER  The DispatchHandle was not valid.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_SMM_IO_TRAP_DISPATCH2_UNREGISTER)(
  IN CONST EFI_SMM_IO_TRAP_DISPATCH2_PROTOCOL  *This,
  IN       EFI_HANDLE                          DispatchHandle
  );

///
/// Interface structure for the SMM IO Trap Dispatch2 Protocol.
///
/// This protocol provides a parent dispatch service for IO trap SMI sources.
///
struct _EFI_SMM_IO_TRAP_DISPATCH2_PROTOCOL {
  EFI_SMM_IO_TRAP_DISPATCH2_REGISTER    Register;
  EFI_SMM_IO_TRAP_DISPATCH2_UNREGISTER  UnRegister;
};

extern EFI_GUID gEfiSmmIoTrapDispatch2ProtocolGuid;

#endif

