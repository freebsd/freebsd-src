/** @file
  This file declares Stall PPI.

  This ppi abstracts the blocking stall service to other agents.

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials                          
  are licensed and made available under the terms and conditions of the BSD License         
  which accompanies this distribution.  The full text of the license may be found at        
  http://opensource.org/licenses/bsd-license.php                                            

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

  @par Revision Reference:
  This PPI is introduced in PI Version 1.0.

**/

#ifndef __STALL_PPI_H__
#define __STALL_PPI_H__

#define EFI_PEI_STALL_PPI_GUID \
  { 0x1f4c6f90, 0xb06b, 0x48d8, {0xa2, 0x01, 0xba, 0xe5, 0xf1, 0xcd, 0x7d, 0x56 } }

typedef struct _EFI_PEI_STALL_PPI EFI_PEI_STALL_PPI;

/**
  The Stall() function provides a blocking stall for at least the number 
  of microseconds stipulated in the final argument of the API.

  @param  PeiServices    An indirect pointer to the PEI Services Table
                         published by the PEI Foundation.
  @param  This           Pointer to the local data for the interface.
  @param  Microseconds   Number of microseconds for which to stall.

  @retval EFI_SUCCESS    The service provided at least the required delay.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_STALL)(
  IN CONST EFI_PEI_SERVICES     **PeiServices,
  IN CONST EFI_PEI_STALL_PPI    *This,
  IN UINTN                      Microseconds
  );

///
/// This service provides a simple, blocking stall with platform-specific resolution. 
///
struct _EFI_PEI_STALL_PPI {
  ///
  /// The resolution in microseconds of the stall services.
  ///
  UINTN          Resolution;

  EFI_PEI_STALL  Stall;
};

extern EFI_GUID gEfiPeiStallPpiGuid;

#endif
