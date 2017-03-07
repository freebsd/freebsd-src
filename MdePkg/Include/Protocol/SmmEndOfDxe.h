/** @file
  SMM End Of Dxe protocol introduced in the PI 1.2.1 specification.

  According to PI 1.4a specification, this protocol indicates end of the
  execution phase when all of the components are under the authority of
  the platform manufacturer.
  This protocol is a mandatory protocol published by SMM Foundation code.
  This protocol is an SMM counterpart of the End of DXE Event.
  This protocol prorogates End of DXE notification into SMM environment.
  This protocol is installed prior to installation of the SMM Ready to Lock Protocol.

  Copyright (c) 2012 - 2016, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef _SMM_END_OF_DXE_H_
#define _SMM_END_OF_DXE_H_

#define EFI_SMM_END_OF_DXE_PROTOCOL_GUID \
  { \
    0x24e70042, 0xd5c5, 0x4260, { 0x8c, 0x39, 0xa, 0xd3, 0xaa, 0x32, 0xe9, 0x3d } \
  }

extern EFI_GUID gEfiSmmEndOfDxeProtocolGuid;

#endif
