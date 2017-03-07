/** @file
  GUID for hardware error record variables.

  Copyright (c) 2007 - 2009, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  @par Revision Reference:
  GUID defined in UEFI 2.1.

**/

#ifndef _HARDWARE_ERROR_VARIABLE_GUID_H_
#define _HARDWARE_ERROR_VARIABLE_GUID_H_

#define EFI_HARDWARE_ERROR_VARIABLE \
  { \
    0x414E6BDD, 0xE47B, 0x47cc, {0xB2, 0x44, 0xBB, 0x61, 0x02, 0x0C, 0xF5, 0x16} \
  }

extern EFI_GUID gEfiHardwareErrorVariableGuid;

#endif
