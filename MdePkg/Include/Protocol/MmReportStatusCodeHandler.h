/** @file
  This protocol provides registering and unregistering services to status code consumers while in DXE MM.

  Copyright (c) 2017, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol was introduced in PI Specification 1.1.

**/

#ifndef __MM_REPORT_STATUS_CODE_HANDLER_PROTOCOL_H__
#define __MM_REPORT_STATUS_CODE_HANDLER_PROTOCOL_H__

#define EFI_MM_RSC_HANDLER_PROTOCOL_GUID \
  { \
    0x2ff29fa7, 0x5e80, 0x4ed9, {0xb3, 0x80, 0x1, 0x7d, 0x3c, 0x55, 0x4f, 0xf4} \
  }

typedef
EFI_STATUS
(EFIAPI *EFI_MM_RSC_HANDLER_CALLBACK)(
  IN EFI_STATUS_CODE_TYPE   CodeType,
  IN EFI_STATUS_CODE_VALUE  Value,
  IN UINT32                 Instance,
  IN EFI_GUID               *CallerId,
  IN EFI_STATUS_CODE_DATA   *Data
  );

/**
  Register the callback function for ReportStatusCode() notification.

  When this function is called the function pointer is added to an internal list and any future calls to
  ReportStatusCode() will be forwarded to the Callback function.

  @param[in] Callback               A pointer to a function of type EFI_MM_RSC_HANDLER_CALLBACK that is
                                    called when a call to ReportStatusCode() occurs.

  @retval EFI_SUCCESS               Function was successfully registered.
  @retval EFI_INVALID_PARAMETER     The callback function was NULL.
  @retval EFI_OUT_OF_RESOURCES      The internal buffer ran out of space. No more functions can be
                                    registered.
  @retval EFI_ALREADY_STARTED       The function was already registered. It can't be registered again.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_MM_RSC_HANDLER_REGISTER)(
  IN EFI_MM_RSC_HANDLER_CALLBACK Callback
  );

/**
  Remove a previously registered callback function from the notification list.

  A callback function must be unregistered before it is deallocated. It is important that any registered
  callbacks that are not runtime complaint be unregistered when ExitBootServices() is called.

  @param[in] Callback           A pointer to a function of type EFI_MM_RSC_HANDLER_CALLBACK that is to be
                                unregistered.

  @retval EFI_SUCCESS           The function was successfully unregistered.
  @retval EFI_INVALID_PARAMETER The callback function was NULL.
  @retval EFI_NOT_FOUND         The callback function was not found to be unregistered.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_MM_RSC_HANDLER_UNREGISTER)(
  IN EFI_MM_RSC_HANDLER_CALLBACK Callback
  );

typedef struct _EFI_MM_RSC_HANDLER_PROTOCOL {
  EFI_MM_RSC_HANDLER_REGISTER      Register;
  EFI_MM_RSC_HANDLER_UNREGISTER    Unregister;
} EFI_MM_RSC_HANDLER_PROTOCOL;

extern EFI_GUID  gEfiMmRscHandlerProtocolGuid;

#endif
