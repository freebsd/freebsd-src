/** @file
  This file declares Vector Handoff Info PPI that describes an array of
  interrupt and/or exception vectors that are in use and need to persist.

  This is an optional PPI that may be produced by SEC. If present, it provides
  a description of the interrupt and/or exception vectors that were established
  in the SEC Phase and need to persist into PEI and DXE. 

  Copyright (c) 2013, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials                          
  are licensed and made available under the terms and conditions of the BSD License         
  which accompanies this distribution.  The full text of the license may be found at        
  http://opensource.org/licenses/bsd-license.php                                            

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

  @par Revision Reference:
  This PPI is introduced in PI Version 1.2.1.

**/

#ifndef __VECTOR_HANDOFF_INFO_H__
#define __VECTOR_HANDOFF_INFO_H__

///
/// NOTE: EFI_PEI_VECTOR_HANDOFF_INFO_PPI_GUID can also be used in the PEI Phase
/// to build a GUIDed HOB that contains an array of EFI_VECTOR_HANDOFF_INFO.
///
#define EFI_PEI_VECTOR_HANDOFF_INFO_PPI_GUID \
  { 0x3cd652b4, 0x6d33, 0x4dce, { 0x89, 0xdb, 0x83, 0xdf, 0x97, 0x66, 0xfc, 0xca }}

///
/// Vector Handoff Info Attributes
///@{
#define EFI_VECTOR_HANDOFF_DO_NOT_HOOK 0x00000000
#define EFI_VECTOR_HANDOFF_HOOK_BEFORE 0x00000001
#define EFI_VECTOR_HANDOFF_HOOK_AFTER  0x00000002
#define EFI_VECTOR_HANDOFF_LAST_ENTRY  0x80000000
///@}

///
/// EFI_VECTOR_HANDOFF_INFO entries that describes the interrupt and/or
/// exception vectors in use in the PEI Phase.
///
typedef struct {
  //
  // The interrupt or exception vector that is in use and must be preserved.
  //
  UINT32    VectorNumber;
  //
  // A bitmask that describes the attributes ofthe interrupt or exception vector.
  //
  UINT32    Attribute;
  //
  // The GUID identifies the party who created the entry. For the 
  // EFI_VECTOR_HANDOFF_DO_NOT_HOOK case, this establishes the single owner.
  //
  EFI_GUID  Owner;
} EFI_VECTOR_HANDOFF_INFO;

///
/// Provides a description of the interrupt and/or exception vectors that
/// were established in the SEC Phase and need to persist into PEI and DXE.  
///
typedef struct _EFI_PEI_VECTOR_HANDOFF_INFO_PPI {
  //
  // Pointer to an array of interrupt and /or exception vectors.
  //
  EFI_VECTOR_HANDOFF_INFO  *Info;
} EFI_PEI_VECTOR_HANDOFF_INFO_PPI;

extern EFI_GUID gEfiVectorHandoffInfoPpiGuid;

#endif
