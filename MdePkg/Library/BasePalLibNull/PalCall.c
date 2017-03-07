/** @file
  
  Template and Sample instance of PalCallLib.
  
  Copyright (c) 2006 - 2010, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials                          
  are licensed and made available under the terms and conditions of the BSD License         
  which accompanies this distribution.  The full text of the license may be found at        
  http://opensource.org/licenses/bsd-license.php.                                            

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
     
**/

#include <Base.h>
#include <Library/PalLib.h>
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

  @return The structure returned from the PAL Call procedure, including the status and return value.

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
  PAL_CALL_RETURN Ret;

  Ret.Status = (UINT64) -1;
  ASSERT (!RETURN_ERROR (RETURN_UNSUPPORTED));
  return Ret;
}
