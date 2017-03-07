/** @file
  This file declares Reset PPI used to reset the platform.
  
  This PPI is installed by some platform- or chipset-specific PEIM that 
  abstracts the Reset Service to other agents.

  Copyright (c) 2006 - 2009, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials                          
  are licensed and made available under the terms and conditions of the BSD License         
  which accompanies this distribution.  The full text of the license may be found at        
  http://opensource.org/licenses/bsd-license.php                                            

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

  @par Revision Reference:
  This PPI is introduced in PI Version 1.0.

**/

#ifndef __RESET_PPI_H__
#define __RESET_PPI_H__

#define EFI_PEI_RESET_PPI_GUID \
  { \
    0xef398d58, 0x9dfd, 0x4103, {0xbf, 0x94, 0x78, 0xc6, 0xf4, 0xfe, 0x71, 0x2f } \
  }

//
// EFI_PEI_RESET_PPI.ResetSystem() is equivalent to the
// PEI Service ResetSystem().
// It is introduced in PIPeiCis.h. 
//

///
/// This PPI provides provide a simple reset service. 
///
typedef struct {
  EFI_PEI_RESET_SYSTEM  ResetSystem;
} EFI_PEI_RESET_PPI;

extern EFI_GUID gEfiPeiResetPpiGuid;

#endif
