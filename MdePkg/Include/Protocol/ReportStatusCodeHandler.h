/** @file
  This protocol provide registering and unregistering services to status code 
  consumers while in DXE.
  
  Copyright (c) 2007 - 2009, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __REPORT_STATUS_CODE_HANDLER_PROTOCOL_H__
#define __REPORT_STATUS_CODE_HANDLER_PROTOCOL_H__

#define EFI_RSC_HANDLER_PROTOCOL_GUID \
  { \
    0x86212936, 0xe76, 0x41c8, {0xa0, 0x3a, 0x2a, 0xf2, 0xfc, 0x1c, 0x39, 0xe2} \
  }

typedef
EFI_STATUS
(EFIAPI *EFI_RSC_HANDLER_CALLBACK)(
  IN EFI_STATUS_CODE_TYPE   CodeType,
  IN EFI_STATUS_CODE_VALUE  Value,
  IN UINT32                 Instance,
  IN EFI_GUID               *CallerId,
  IN EFI_STATUS_CODE_DATA   *Data
);

/**
  Register the callback function for ReportStatusCode() notification.
  
  When this function is called the function pointer is added to an internal list and any future calls to
  ReportStatusCode() will be forwarded to the Callback function. During the bootservices,
  this is the callback for which this service can be invoked. The report status code router
  will create an event such that the callback function is only invoked at the TPL for which it was
  registered. The entity that registers for the callback should also register for an event upon
  generation of exit boot services and invoke the unregister service.
  If the handler does not have a TPL dependency, it should register for a callback at TPL high. The
  router infrastructure will support making callbacks at runtime, but the caller for runtime invocation
  must meet the following criteria:
  1. must be a runtime driver type so that its memory is not reclaimed
  2. not unregister at exit boot services so that the router will still have its callback address
  3. the caller must be self-contained (eg. Not call out into any boot-service interfaces) and be
  runtime safe, in general.
  
  @param[in] Callback   A pointer to a function of type EFI_RSC_HANDLER_CALLBACK that is called when
                        a call to ReportStatusCode() occurs.
  @param[in] Tpl        TPL at which callback can be safely invoked.   
  
  @retval  EFI_SUCCESS              Function was successfully registered.
  @retval  EFI_INVALID_PARAMETER    The callback function was NULL.
  @retval  EFI_OUT_OF_RESOURCES     The internal buffer ran out of space. No more functions can be
                                    registered.
  @retval  EFI_ALREADY_STARTED      The function was already registered. It can't be registered again.                              
**/
typedef
EFI_STATUS
(EFIAPI *EFI_RSC_HANDLER_REGISTER)(
  IN EFI_RSC_HANDLER_CALLBACK   Callback,
  IN EFI_TPL                    Tpl
);

/**
  Remove a previously registered callback function from the notification list.
  
  A callback function must be unregistered before it is deallocated. It is important that any registered
  callbacks that are not runtime complaint be unregistered when ExitBootServices() is called.
  
  @param[in]  Callback  A pointer to a function of type EFI_RSC_HANDLER_CALLBACK that is to be
                        unregistered.
                        
  @retval EFI_SUCCESS           The function was successfully unregistered.
  @retval EFI_INVALID_PARAMETER The callback function was NULL.
  @retval EFI_NOT_FOUND         The callback function was not found to be unregistered.                        
**/
typedef
EFI_STATUS
(EFIAPI *EFI_RSC_HANDLER_UNREGISTER)(
  IN EFI_RSC_HANDLER_CALLBACK Callback
);

typedef struct {
  EFI_RSC_HANDLER_REGISTER Register;
  EFI_RSC_HANDLER_UNREGISTER Unregister;
} EFI_RSC_HANDLER_PROTOCOL;

extern EFI_GUID gEfiRscHandlerProtocolGuid;

#endif // __REPORT_STATUS_CODE_HANDLER_H__
