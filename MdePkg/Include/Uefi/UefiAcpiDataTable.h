/** @file
  UEFI ACPI Data Table Definition.

Copyright (c) 2011 - 2016, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials are licensed and made available under 
the terms and conditions of the BSD License that accompanies this distribution.  
The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php.                                          
    
THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __UEFI_ACPI_DATA_TABLE_H__
#define __UEFI_ACPI_DATA_TABLE_H__

#include <IndustryStandard/Acpi.h>

#pragma pack(1)
typedef struct {
  EFI_ACPI_DESCRIPTION_HEADER   Header;
  GUID                          Identifier;
  UINT16                        DataOffset;
} EFI_ACPI_DATA_TABLE;

typedef struct {
  EFI_ACPI_DATA_TABLE  UefiAcpiDataTable;
  UINT32               SwSmiNumber;
  UINT64               BufferPtrAddress;
} EFI_SMM_COMMUNICATION_ACPI_TABLE;

typedef struct {
  EFI_SMM_COMMUNICATION_ACPI_TABLE        UefiSmmCommunicationAcpiTable;
  EFI_ACPI_6_0_GENERIC_ADDRESS_STRUCTURE  InvocationRegister;
} EFI_SMM_COMMUNICATION_ACPI_TABLE_2;

///
/// To avoid confusion in interpreting frames, the communication buffer should always 
/// begin with EFI_SMM_COMMUNICATE_HEADER
///
typedef struct {
  ///
  /// Allows for disambiguation of the message format.
  ///
  EFI_GUID  HeaderGuid;
  ///
  /// Describes the size of Data (in bytes) and does not include the size of the header.
  ///
  UINTN     MessageLength;
  ///
  /// Designates an array of bytes that is MessageLength in size.
  ///
  UINT8     Data[1];
} EFI_SMM_COMMUNICATE_HEADER;

#pragma pack()

#endif

