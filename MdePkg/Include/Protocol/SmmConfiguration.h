/** @file
  EFI SMM Configuration Protocol as defined in the PI 1.2 specification.

  This protocol is used to:
  1) report the portions of SMRAM regions which cannot be used for the SMRAM heap.
  2) register the SMM Foundation entry point with the processor code. The entry
     point will be invoked by the SMM processor entry code.
  
  Copyright (c) 2009, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials                          
  are licensed and made available under the terms and conditions of the BSD License         
  which accompanies this distribution.  The full text of the license may be found at        
  http://opensource.org/licenses/bsd-license.php                                            

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

**/

#ifndef _SMM_CONFIGURATION_H_
#define _SMM_CONFIGURATION_H_

#include <Pi/PiSmmCis.h>

#define EFI_SMM_CONFIGURATION_PROTOCOL_GUID \
  { \
    0x26eeb3de, 0xb689, 0x492e, {0x80, 0xf0, 0xbe, 0x8b, 0xd7, 0xda, 0x4b, 0xa7 }  \
  }

///
/// Structure describing a SMRAM region which cannot be used for the SMRAM heap.
///
typedef struct _EFI_SMM_RESERVED_SMRAM_REGION {
  ///
  /// Starting address of the reserved SMRAM area, as it appears while SMRAM is open. 
  /// Ignored if SmramReservedSize is 0.
  ///
  EFI_PHYSICAL_ADDRESS    SmramReservedStart;
  ///
  /// Number of bytes occupied by the reserved SMRAM area. A size of zero indicates the 
  /// last SMRAM area.
  ///
  UINT64                  SmramReservedSize;
} EFI_SMM_RESERVED_SMRAM_REGION;

typedef struct _EFI_SMM_CONFIGURATION_PROTOCOL  EFI_SMM_CONFIGURATION_PROTOCOL;

/**
  Register the SMM Foundation entry point.
  
  This function registers the SMM Foundation entry point with the processor code. This entry point 
  will be invoked by the SMM Processor entry code.

  @param[in] This                The EFI_SMM_CONFIGURATION_PROTOCOL instance.
  @param[in] SmmEntryPoint       SMM Foundation entry point.
  
  @retval EFI_SUCCESS            Success to register SMM Entry Point.
  @retval EFI_INVALID_PARAMETER  SmmEntryPoint is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_SMM_REGISTER_SMM_ENTRY)(
  IN CONST EFI_SMM_CONFIGURATION_PROTOCOL  *This,
  IN EFI_SMM_ENTRY_POINT                   SmmEntryPoint
  );

///
/// The EFI SMM Configuration Protocol is a mandatory protocol published by a DXE CPU driver to
/// indicate which areas within SMRAM are reserved for use by the CPU for any purpose, 
/// such as stack, save state or SMM entry point.
///
/// The RegisterSmmEntry() function allows the SMM IPL DXE driver to register the SMM 
/// Foundation entry point with the SMM entry vector code.
///
struct _EFI_SMM_CONFIGURATION_PROTOCOL {
  ///
  /// A pointer to an array SMRAM ranges used by the initial SMM entry code.
  ///
  EFI_SMM_RESERVED_SMRAM_REGION  *SmramReservedRegions;
  EFI_SMM_REGISTER_SMM_ENTRY     RegisterSmmEntry;
};

extern EFI_GUID gEfiSmmConfigurationProtocolGuid;

#endif

