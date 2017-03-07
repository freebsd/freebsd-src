/** @file
  Guid used to define the Firmware File System 3.

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials                          
  are licensed and made available under the terms and conditions of the BSD License         
  which accompanies this distribution.  The full text of the license may be found at        
  http://opensource.org/licenses/bsd-license.php                                            

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

  @par Revision Reference:
  GUIDs introduced in PI Version 1.0.

**/

#ifndef __FIRMWARE_FILE_SYSTEM3_GUID_H__
#define __FIRMWARE_FILE_SYSTEM3_GUID_H__

/// 
/// The firmware volume header contains a data field for the file system GUID
/// {5473C07A-3DCB-4dca-BD6F-1E9689E7349A}
///
#define EFI_FIRMWARE_FILE_SYSTEM3_GUID \
  { 0x5473c07a, 0x3dcb, 0x4dca, { 0xbd, 0x6f, 0x1e, 0x96, 0x89, 0xe7, 0x34, 0x9a }}

extern EFI_GUID gEfiFirmwareFileSystem3Guid;

#endif // __FIRMWARE_FILE_SYSTEM3_GUID_H__
