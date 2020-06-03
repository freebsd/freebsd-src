/** @file
  SMM USB Dispatch2 Protocol as defined in PI 1.1 Specification
  Volume 4 System Management Mode Core Interface.

  Provides the parent dispatch service for the USB SMI source generator.

  Copyright (c) 2009 - 2017, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This protocol is from PI Version 1.1.

**/

#ifndef _SMM_USB_DISPATCH2_H_
#define _SMM_USB_DISPATCH2_H_

#include <Protocol/MmUsbDispatch.h>

#define EFI_SMM_USB_DISPATCH2_PROTOCOL_GUID EFI_MM_USB_DISPATCH_PROTOCOL_GUID

///
/// USB SMI event types
///
typedef EFI_USB_MMI_TYPE EFI_USB_SMI_TYPE;

///
/// The dispatch function's context.
///
typedef EFI_MM_USB_REGISTER_CONTEXT EFI_SMM_USB_REGISTER_CONTEXT;

typedef EFI_MM_USB_DISPATCH_PROTOCOL EFI_SMM_USB_DISPATCH2_PROTOCOL;

typedef EFI_MM_USB_REGISTER EFI_SMM_USB_REGISTER2;

typedef EFI_MM_USB_UNREGISTER EFI_SMM_USB_UNREGISTER2;

extern EFI_GUID gEfiSmmUsbDispatch2ProtocolGuid;

#endif

