/** @file
  Security Policy protocol as defined in PI Specification VOLUME 2 DXE

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _SECURITY_POLICY_H_
#define _SECURITY_POLICY_H_

///
/// Security policy protocol GUID definition
///
#define EFI_SECURITY_POLICY_PROTOCOL_GUID  \
  {0x78E4D245, 0xCD4D, 0x4a05, {0xA2, 0xBA, 0x47, 0x43, 0xE8, 0x6C, 0xFC, 0xAB} }

extern EFI_GUID  gEfiSecurityPolicyProtocolGuid;

#endif
