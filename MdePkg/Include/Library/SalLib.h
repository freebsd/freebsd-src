/** @file
  Provides library services to make SAL Calls.

Copyright (c) 2007 - 2008, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __SAL_LIB__
#define __SAL_LIB__

#include <IndustryStandard/Sal.h>

/**
  Makes a SAL procedure call.
  
  This is a wrapper function to make a SAL procedure call.  
  No parameter checking is performed on the 8 input parameters,
  but there are some common rules that the caller should follow
  when making a SAL call.  Any address passed to SAL as buffers
  for return parameters must be 8-byte aligned.  Unaligned
  addresses may cause undefined results.  For those parameters
  defined as reserved or some fields defined as reserved must be
  zero filled or the invalid argument return value may be returned
  or undefined result may occur during the execution of the procedure.
  This function is only available on Intel Itanium-based platforms.

  @param  Index       The SAL procedure Index number
  @param  Arg2        The 2nd parameter for SAL procedure calls
  @param  Arg3        The 3rd parameter for SAL procedure calls
  @param  Arg4        The 4th parameter for SAL procedure calls
  @param  Arg5        The 5th parameter for SAL procedure calls
  @param  Arg6        The 6th parameter for SAL procedure calls
  @param  Arg7        The 7th parameter for SAL procedure calls
  @param  Arg8        The 8th parameter for SAL procedure calls

  @return SAL returned registers.

**/
SAL_RETURN_REGS
EFIAPI
SalCall (
  IN UINT64  Index,
  IN UINT64  Arg2,
  IN UINT64  Arg3,
  IN UINT64  Arg4,
  IN UINT64  Arg5,
  IN UINT64  Arg6,
  IN UINT64  Arg7,
  IN UINT64  Arg8
  );

#endif
