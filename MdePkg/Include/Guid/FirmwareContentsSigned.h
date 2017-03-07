/** @file
  GUID is used to define the signed section.

  Copyright (c) 2013, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials                          
  are licensed and made available under the terms and conditions of the BSD License         
  which accompanies this distribution.  The full text of the license may be found at        
  http://opensource.org/licenses/bsd-license.php                                            

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

  @par Revision Reference:
  GUID introduced in PI Version 1.2.1.

**/

#ifndef __FIRMWARE_CONTENTS_SIGNED_GUID_H__
#define __FIRMWARE_CONTENTS_SIGNED_GUID_H__

#define EFI_FIRMWARE_CONTENTS_SIGNED_GUID \
   { 0xf9d89e8, 0x9259, 0x4f76, {0xa5, 0xaf, 0xc, 0x89, 0xe3, 0x40, 0x23, 0xdf } }

extern EFI_GUID gEfiFirmwareContentsSignedGuid;

#endif
