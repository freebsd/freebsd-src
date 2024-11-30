/** @file
  This protocol provides registering and unregistering services to status code consumers while in DXE SMM.

  Copyright (c) 2007 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol was introduced in PI Specification 1.1.

**/

#ifndef __SMM_REPORT_STATUS_CODE_HANDLER_PROTOCOL_H__
#define __SMM_REPORT_STATUS_CODE_HANDLER_PROTOCOL_H__

#include <Protocol/MmReportStatusCodeHandler.h>

#define EFI_SMM_RSC_HANDLER_PROTOCOL_GUID  EFI_MM_RSC_HANDLER_PROTOCOL_GUID

typedef EFI_MM_RSC_HANDLER_CALLBACK EFI_SMM_RSC_HANDLER_CALLBACK;

typedef EFI_MM_RSC_HANDLER_REGISTER EFI_SMM_RSC_HANDLER_REGISTER;

typedef EFI_MM_RSC_HANDLER_UNREGISTER EFI_SMM_RSC_HANDLER_UNREGISTER;

typedef EFI_MM_RSC_HANDLER_PROTOCOL EFI_SMM_RSC_HANDLER_PROTOCOL;

extern EFI_GUID  gEfiSmmRscHandlerProtocolGuid;

#endif // __SMM_REPORT_STATUS_CODE_HANDLER_PROTOCOL_H__
