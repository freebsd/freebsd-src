/** @file
  SMM End Of Dxe protocol introduced in the PI 1.2.1 specification.

  According to PI 1.4a specification, this protocol indicates end of the
  execution phase when all of the components are under the authority of
  the platform manufacturer.
  This protocol is a mandatory protocol published by SMM Foundation code.
  This protocol is an SMM counterpart of the End of DXE Event.
  This protocol prorogates End of DXE notification into SMM environment.
  This protocol is installed prior to installation of the SMM Ready to Lock Protocol.

  Copyright (c) 2012 - 2017, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _SMM_END_OF_DXE_H_
#define _SMM_END_OF_DXE_H_

#include <Protocol/MmEndOfDxe.h>

#define EFI_SMM_END_OF_DXE_PROTOCOL_GUID EFI_MM_END_OF_DXE_PROTOCOL_GUID

extern EFI_GUID gEfiSmmEndOfDxeProtocolGuid;

#endif
