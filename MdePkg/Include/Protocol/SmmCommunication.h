/** @file
  EFI SMM Communication Protocol as defined in the PI 1.2 specification.

  This protocol provides a means of communicating between drivers outside of SMM and SMI
  handlers inside of SMM.

  Copyright (c) 2009 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _SMM_COMMUNICATION_H_
#define _SMM_COMMUNICATION_H_

#include <Protocol/MmCommunication.h>

typedef EFI_MM_COMMUNICATE_HEADER EFI_SMM_COMMUNICATE_HEADER;

#define EFI_SMM_COMMUNICATION_PROTOCOL_GUID  EFI_MM_COMMUNICATION_PROTOCOL_GUID

typedef EFI_MM_COMMUNICATION_PROTOCOL EFI_SMM_COMMUNICATION_PROTOCOL;

extern EFI_GUID  gEfiSmmCommunicationProtocolGuid;

#endif
