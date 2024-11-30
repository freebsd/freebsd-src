/** @file

  Null stub of TdxLib

  Copyright (c) 2021, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Uefi/UefiBaseType.h>

/**
  The TDCALL instruction causes a VM exit to the Intel TDX module.  It is
  used to call guest-side Intel TDX functions, either local or a TD exit
  to the host VMM, as selected by Leaf.
  Leaf functions are described at <https://software.intel.com/content/
  www/us/en/develop/articles/intel-trust-domain-extensions.html>

  @param[in]      Leaf        Leaf number of TDCALL instruction
  @param[in]      Arg1        Arg1
  @param[in]      Arg2        Arg2
  @param[in]      Arg3        Arg3
  @param[in,out]  Results  Returned result of the Leaf function

  @return EFI_SUCCESS
  @return Other           See individual leaf functions
**/
UINTN
EFIAPI
TdCall (
  IN UINT64    Leaf,
  IN UINT64    Arg1,
  IN UINT64    Arg2,
  IN UINT64    Arg3,
  IN OUT VOID  *Results
  )
{
  return EFI_UNSUPPORTED;
}

/**
  TDVMALL is a leaf function 0 for TDCALL. It helps invoke services from the
  host VMM to pass/receive information.

  @param[in]     Leaf        Number of sub-functions
  @param[in]     Arg1        Arg1
  @param[in]     Arg2        Arg2
  @param[in]     Arg3        Arg3
  @param[in]     Arg4        Arg4
  @param[in,out] Results     Returned result of the sub-function

  @return EFI_SUCCESS
  @return Other           See individual sub-functions

**/
UINTN
EFIAPI
TdVmCall (
  IN UINT64    Leaf,
  IN UINT64    Arg1,
  IN UINT64    Arg2,
  IN UINT64    Arg3,
  IN UINT64    Arg4,
  IN OUT VOID  *Results
  )
{
  return EFI_UNSUPPORTED;
}

/**
  Probe if TD is enabled.

  @return TRUE    TD is enabled.
  @return FALSE   TD is not enabled.
**/
BOOLEAN
EFIAPI
TdIsEnabled (
  )
{
  return FALSE;
}
