/** @file
  GUIDs used to locate the SMBIOS tables in the UEFI 2.5 system table.

  These GUIDs in the system table are the only legal ways to search for and
  locate the SMBIOS tables. Do not search the 0xF0000 segment to find SMBIOS
  tables.

  Copyright (c) 2006 - 2015, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials                          
  are licensed and made available under the terms and conditions of the BSD License         
  which accompanies this distribution.  The full text of the license may be found at        
  http://opensource.org/licenses/bsd-license.php                                            

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

  @par Revision Reference:
  GUIDs defined in UEFI 2.5 spec.

**/

#ifndef __SMBIOS_GUID_H__
#define __SMBIOS_GUID_H__

#define SMBIOS_TABLE_GUID \
  { \
    0xeb9d2d31, 0x2d88, 0x11d3, {0x9a, 0x16, 0x0, 0x90, 0x27, 0x3f, 0xc1, 0x4d } \
  }

#define SMBIOS3_TABLE_GUID \
  { \
    0xf2fd1544, 0x9794, 0x4a2c, {0x99, 0x2e, 0xe5, 0xbb, 0xcf, 0x20, 0xe3, 0x94 } \
  }

extern EFI_GUID       gEfiSmbiosTableGuid;
extern EFI_GUID       gEfiSmbios3TableGuid;

#endif
