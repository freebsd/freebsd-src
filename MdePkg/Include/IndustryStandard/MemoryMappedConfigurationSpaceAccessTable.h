/** @file
  ACPI memory mapped configuration space access table definition, defined at 
  in the PCI Firmware Specification, version 3.0.
  Specification is available at http://www.pcisig.com.
    
  Copyright (c) 2007 - 2008, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials                          
  are licensed and made available under the terms and conditions of the BSD License         
  which accompanies this distribution.  The full text of the license may be found at        
  http://opensource.org/licenses/bsd-license.php                                            

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             
**/

#ifndef _MEMORY_MAPPED_CONFIGURATION_SPACE_ACCESS_TABLE_H_
#define _MEMORY_MAPPED_CONFIGURATION_SPACE_ACCESS_TABLE_H_

//
// Ensure proper structure formats
//
#pragma pack(1)

///
/// Memory Mapped Configuration Space Access Table (MCFG)
/// This table is a basic description table header followed by
/// a number of base address allocation structures.
///
typedef struct {
  UINT64  BaseAddress;
  UINT16  PciSegmentGroupNumber;
  UINT8   StartBusNumber;
  UINT8   EndBusNumber;
  UINT32  Reserved;
} EFI_ACPI_MEMORY_MAPPED_ENHANCED_CONFIGURATION_SPACE_BASE_ADDRESS_ALLOCATION_STRUCTURE;

///
/// MCFG Table header definition.  The rest of the table 
/// must be defined in a platform specific manner.
///
typedef struct {
  EFI_ACPI_DESCRIPTION_HEADER                       Header;
  UINT64                                            Reserved;
} EFI_ACPI_MEMORY_MAPPED_CONFIGURATION_BASE_ADDRESS_TABLE_HEADER;

///
/// MCFG Revision (defined in spec)
///
#define EFI_ACPI_MEMORY_MAPPED_CONFIGURATION_SPACE_ACCESS_TABLE_REVISION  0x01

#pragma pack()

#endif
