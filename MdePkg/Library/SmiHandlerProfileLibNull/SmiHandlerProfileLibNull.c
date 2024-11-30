/** @file
  NULL instance of SmiHandlerProfile Library.

  Copyright (c) 2017, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiMm.h>
#include <Library/SmiHandlerProfileLib.h>

/**
  This function is called by SmmChildDispatcher module to report
  a new SMI handler is registered, to SmmCore.

  @param HandlerGuid     The GUID to identify the type of the handler.
                         For the SmmChildDispatch protocol, the HandlerGuid
                         must be the GUID of SmmChildDispatch protocol.
  @param Handler         The SMI handler.
  @param CallerAddress   The address of the module who registers the SMI handler.
  @param Context         The context of the SMI handler.
                         For the SmmChildDispatch protocol, the Context
                         must match the one defined for SmmChildDispatch protocol.
  @param ContextSize     The size of the context in bytes.
                         For the SmmChildDispatch protocol, the Context
                         must match the one defined for SmmChildDispatch protocol.

  @retval EFI_SUCCESS           The information is recorded.
  @retval EFI_UNSUPPORTED       The feature is unsupported.
  @retval EFI_OUT_OF_RESOURCES  There is no enough resource to record the information.
**/
EFI_STATUS
EFIAPI
SmiHandlerProfileRegisterHandler (
  IN EFI_GUID                      *HandlerGuid,
  IN EFI_SMM_HANDLER_ENTRY_POINT2  Handler,
  IN PHYSICAL_ADDRESS              CallerAddress,
  IN VOID                          *Context  OPTIONAL,
  IN UINTN                         ContextSize OPTIONAL
  )
{
  return EFI_UNSUPPORTED;
}

/**
  This function is called by SmmChildDispatcher module to report
  an existing SMI handler is unregistered, to SmmCore.

  @param HandlerGuid     The GUID to identify the type of the handler.
                         For the SmmChildDispatch protocol, the HandlerGuid
                         must be the GUID of SmmChildDispatch protocol.
  @param Handler         The SMI handler.
  @param Context         The context of the SMI handler.
                         If it is NOT NULL, it will be used to check what is registered.
  @param ContextSize     The size of the context in bytes.
                         If Context is NOT NULL, it will be used to check what is registered.

  @retval EFI_SUCCESS           The original record is removed.
  @retval EFI_UNSUPPORTED       The feature is unsupported.
  @retval EFI_NOT_FOUND         There is no record for the HandlerGuid and handler.
**/
EFI_STATUS
EFIAPI
SmiHandlerProfileUnregisterHandler (
  IN EFI_GUID                      *HandlerGuid,
  IN EFI_SMM_HANDLER_ENTRY_POINT2  Handler,
  IN VOID                          *Context  OPTIONAL,
  IN UINTN                         ContextSize OPTIONAL
  )
{
  return EFI_UNSUPPORTED;
}
