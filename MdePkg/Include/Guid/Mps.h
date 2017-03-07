/** @file
  GUIDs used for MPS entries in the UEFI 2.0 system table
  ACPI is the primary means of exporting MPS information to the OS. MPS only was
  included to support Itanium-based platform power on. So don't use it if you don't have too.

  Copyright (c) 2006, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials                          
  are licensed and made available under the terms and conditions of the BSD License         
  which accompanies this distribution.  The full text of the license may be found at        
  http://opensource.org/licenses/bsd-license.php                                            

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

  @par Revision Reference:
  GUIDs defined in UEFI 2.0 spec.

**/

#ifndef __MPS_GUID_H__
#define __MPS_GUID_H__

#define EFI_MPS_TABLE_GUID \
  { \
    0xeb9d2d2f, 0x2d88, 0x11d3, {0x9a, 0x16, 0x0, 0x90, 0x27, 0x3f, 0xc1, 0x4d } \
  }

//
// GUID name defined in spec.
//
#define MPS_TABLE_GUID EFI_MPS_TABLE_GUID

extern EFI_GUID gEfiMpsTableGuid;

#endif
