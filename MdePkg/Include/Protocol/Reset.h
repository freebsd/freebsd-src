/** @file
  Reset Architectural Protocol as defined in PI Specification VOLUME 2 DXE

  Used to provide ResetSystem runtime services

  The ResetSystem () UEFI 2.0 service is added to the EFI system table and the 
  EFI_RESET_ARCH_PROTOCOL_GUID protocol is registered with a NULL pointer.

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials                          
  are licensed and made available under the terms and conditions of the BSD License         
  which accompanies this distribution.  The full text of the license may be found at        
  http://opensource.org/licenses/bsd-license.php                                            

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

**/

#ifndef __ARCH_PROTOCOL_RESET_H__
#define __ARCH_PROTOCOL_RESET_H__

///
/// Global ID for the Reset Architectural Protocol
///
#define EFI_RESET_ARCH_PROTOCOL_GUID  \
  { 0x27CFAC88, 0x46CC, 0x11d4, {0x9A, 0x38, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D } }

extern EFI_GUID gEfiResetArchProtocolGuid;

#endif
