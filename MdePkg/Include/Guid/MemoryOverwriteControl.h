/** @file
  GUID used for MemoryOverwriteRequestControl UEFI variable defined in 
  TCG Platform Reset Attack Mitigation Specification 1.00.
  See http://trustedcomputinggroup.org for the latest specification

  The purpose of the MemoryOverwriteRequestControl UEFI variable is to give users (e.g., OS, loader) the ability to 
  indicate to the platform that secrets are present in memory and that the platform firmware must clear memory upon 
  a restart. The OS loader should not create the variable. Rather, the  firmware is required to create it. 

  Copyright (c) 2009, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials                          
  are licensed and made available under the terms and conditions of the BSD License         
  which accompanies this distribution.  The full text of the license may be found at        
  http://opensource.org/licenses/bsd-license.php                                            

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

**/

#ifndef _MEMORY_OVERWRITE_CONTROL_DATA_GUID_H_
#define _MEMORY_OVERWRITE_CONTROL_DATA_GUID_H_

#define MEMORY_ONLY_RESET_CONTROL_GUID \
  { \
    0xe20939be, 0x32d4, 0x41be, {0xa1, 0x50, 0x89, 0x7f, 0x85, 0xd4, 0x98, 0x29} \
  }

///
///  Variable name is "MemoryOverwriteRequestControl" and it is a 1 byte unsigned value. 
///  The attributes should be: 
///  EFI_VARIABLE_NON_VOLATILE | 
///  EFI_VARIABLE_BOOTSERVICE_ACCESS | 
///  EFI_VARIABLE_RUNTIME_ACCESS 
///
#define MEMORY_OVERWRITE_REQUEST_VARIABLE_NAME L"MemoryOverwriteRequestControl"

///
/// 0 = Firmware MUST clear the MOR bi
/// 1 = Firmware MUST set the MOR bit 
///
#define MOR_CLEAR_MEMORY_BIT_MASK        0x01

///
/// 0 = Firmware MAY autodetect a clean shutdown of the Static RTM OS.
/// 1 = Firmware MUST NOT autodetect a clean shutdown of the Static RTM OS.
///
#define MOR_DISABLEAUTODETECT_BIT_MASK   0x10

///
/// MOR field bit offset
///
#define MOR_CLEAR_MEMORY_BIT_OFFSET      0
#define MOR_DISABLEAUTODETECT_BIT_OFFSET 4

/**
  Return the ClearMemory bit value 0 or 1.

  @param mor   1 byte value that contains ClearMemory and DisableAutoDetect bit.

  @return ClearMemory bit value
**/
#define MOR_CLEAR_MEMORY_VALUE(mor)        (((UINT8)(mor) & MOR_CLEAR_MEMORY_BIT_MASK) >> MOR_CLEAR_MEMORY_BIT_OFFSET)

/**
  Return the DisableAutoDetect bit value 0 or 1.

  @param mor   1 byte value that contains ClearMemory and DisableAutoDetect bit.

  @return DisableAutoDetect bit value
**/
#define MOR_DISABLE_AUTO_DETECT_VALUE(mor) (((UINT8)(mor) & MOR_DISABLEAUTODETECT_BIT_MASK) >> MOR_DISABLEAUTODETECT_BIT_OFFSET)

extern EFI_GUID gEfiMemoryOverwriteControlDataGuid;

#endif
