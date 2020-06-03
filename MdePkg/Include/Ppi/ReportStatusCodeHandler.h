/** @file
  This PPI provides registering and unregistering services to status code consumers.

  Copyright (c) 2007 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __REPORT_STATUS_CODE_HANDLER_PPI_H__
#define __REPORT_STATUS_CODE_HANDLER_PPI_H__

#define EFI_PEI_RSC_HANDLER_PPI_GUID \
  { \
    0x65d394, 0x9951, 0x4144, {0x82, 0xa3, 0xa, 0xfc, 0x85, 0x79, 0xc2, 0x51} \
  }

typedef
EFI_STATUS
(EFIAPI *EFI_PEI_RSC_HANDLER_CALLBACK)(
  IN CONST  EFI_PEI_SERVICES        **PeiServices,
  IN        EFI_STATUS_CODE_TYPE    Type,
  IN        EFI_STATUS_CODE_VALUE   Value,
  IN        UINT32                  Instance,
  IN CONST  EFI_GUID                *CallerId,
  IN CONST  EFI_STATUS_CODE_DATA    *Data
);

/**
  Register the callback function for ReportStatusCode() notification.

  When this function is called the function pointer is added to an internal list and any future calls to
  ReportStatusCode() will be forwarded to the Callback function.

  @param[in] Callback           A pointer to a function of type EFI_PEI_RSC_HANDLER_CALLBACK that is called
                                when a call to ReportStatusCode() occurs.

  @retval EFI_SUCCESS           Function was successfully registered.
  @retval EFI_INVALID_PARAMETER The callback function was NULL.
  @retval EFI_OUT_OF_RESOURCES  The internal buffer ran out of space. No more functions can be
                                registered.
  @retval EFI_ALREADY_STARTED   The function was already registered. It can't be registered again.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_RSC_HANDLER_REGISTER)(
  IN EFI_PEI_RSC_HANDLER_CALLBACK Callback
);

/**
  Remove a previously registered callback function from the notification list.

  ReportStatusCode() messages will no longer be forwarded to the Callback function.

  @param[in] Callback           A pointer to a function of type EFI_PEI_RSC_HANDLER_CALLBACK that is to be
                                unregistered.

  @retval EFI_SUCCESS           The function was successfully unregistered.
  @retval EFI_INVALID_PARAMETER The callback function was NULL.
  @retval EFI_NOT_FOUND         The callback function was not found to be unregistered.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_RSC_HANDLER_UNREGISTER)(
  IN EFI_PEI_RSC_HANDLER_CALLBACK Callback
);

typedef struct _EFI_PEI_RSC_HANDLER_PPI {
  EFI_PEI_RSC_HANDLER_REGISTER Register;
  EFI_PEI_RSC_HANDLER_UNREGISTER Unregister;
} EFI_PEI_RSC_HANDLER_PPI;

extern EFI_GUID gEfiPeiRscHandlerPpiGuid;

#endif // __REPORT_STATUS_CODE_HANDLER_PPI_H__
