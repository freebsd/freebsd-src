/** @file
  PAL Call Services Function.

  Copyright (c) 2006 - 2010, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials                          
  are licensed and made available under the terms and conditions of the BSD License         
  which accompanies this distribution.  The full text of the license may be found at        
  http://opensource.org/licenses/bsd-license.php.                                            

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED. 
    
**/


#include <PiPei.h>

#include <Ppi/SecPlatformInformation.h>

#include <Library/PalLib.h>
#include <Library/PeiServicesTablePointerLib.h>
#include <Library/PeiServicesLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

/**
  Makes a PAL procedure call.

  This is a wrapper function to make a PAL procedure call.  Based on the Index value,
  this API will make static or stacked PAL call. Architected procedures may be designated
  as required or optional.  If a PAL procedure is specified as optional, a unique return
  code of 0xFFFFFFFFFFFFFFFF is returned in the Status field of the PAL_CALL_RETURN structure.
  This indicates that the procedure is not present in this PAL implementation.  It is the
  caller's responsibility to check for this return code after calling any optional PAL
  procedure. No parameter checking is performed on the 4 input parameters, but there are
  some common rules that the caller should follow when making a PAL call.  Any address
  passed to PAL as buffers for return parameters must be 8-byte aligned.  Unaligned addresses
  may cause undefined results.  For those parameters defined as reserved or some fields
  defined as reserved must be zero filled or the invalid argument return value may be
  returned or undefined result may occur during the execution of the procedure.
  This function is only available on IPF.

  @param Index  The PAL procedure Index number.
  @param Arg2   The 2nd parameter for PAL procedure calls.
  @param Arg3   The 3rd parameter for PAL procedure calls.
  @param Arg4   The 4th parameter for PAL procedure calls.

  @return Structure returned from the PAL Call procedure, including the status and return value.

**/
PAL_CALL_RETURN
EFIAPI
PalCall (
  IN UINT64                  Index,
  IN UINT64                  Arg2,
  IN UINT64                  Arg3,
  IN UINT64                  Arg4
  )
{
  UINT64                              PalCallAddress;
  PAL_CALL_RETURN                     ReturnVal;
  CONST EFI_PEI_SERVICES              **PeiServices;
  EFI_STATUS                          Status;
  EFI_SEC_PLATFORM_INFORMATION_PPI    *SecPlatformPpi;
  EFI_SEC_PLATFORM_INFORMATION_RECORD SecPlatformInfoRecord;
  UINT64                              RecordSize;

  //
  // Get PEI Service Table Pointer
  //
  PeiServices = GetPeiServicesTablePointer ();

  //
  // Locate SEC Platform Information PPI
  //
  Status = PeiServicesLocatePpi (
             &gEfiSecPlatformInformationPpiGuid,
             0,
             NULL,
             (VOID **)&SecPlatformPpi
             );
  ASSERT_EFI_ERROR (Status);

  //
  // Retrieve PAL call address from platform information reported by the PPI
  //
  RecordSize = sizeof (SecPlatformInfoRecord);
  SecPlatformPpi->PlatformInformation (
                    PeiServices,
                    &RecordSize,
                    &SecPlatformInfoRecord
                    );
  PalCallAddress = SecPlatformInfoRecord.ItaniumHealthFlags.PalCallAddress;

  ReturnVal = AsmPalCall (PalCallAddress, Index, Arg2, Arg3, Arg4);

  return ReturnVal;
}

