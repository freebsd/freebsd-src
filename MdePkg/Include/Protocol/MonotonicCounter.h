/** @file
  Monotonic Counter Architectural Protocol as defined in PI SPEC VOLUME 2 DXE

  This code provides the services required to access the system's monotonic counter

Copyright (c) 2006 - 2010, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials are licensed and made available under 
the terms and conditions of the BSD License that accompanies this distribution.  
The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php.                                          
    
THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

**/

#ifndef __ARCH_PROTOCOL_MONTONIC_COUNTER_H__
#define __ARCH_PROTOCOL_MONTONIC_COUNTER_H__

///
/// Global ID for the Monotonic Counter Architectural Protocol.
///
#define EFI_MONOTONIC_COUNTER_ARCH_PROTOCOL_GUID \
  {0x1da97072, 0xbddc, 0x4b30, {0x99, 0xf1, 0x72, 0xa0, 0xb5, 0x6f, 0xff, 0x2a} }
  
extern EFI_GUID gEfiMonotonicCounterArchProtocolGuid;

#endif
