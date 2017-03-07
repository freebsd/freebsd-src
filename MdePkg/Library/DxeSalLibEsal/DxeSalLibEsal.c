/** @file
  This library implements the SAL Library Class using Extended SAL functions

  Copyright (c) 2006 - 2011, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <PiDxe.h>

#include <Protocol/ExtendedSalServiceClasses.h>

#include <Library/SalLib.h>
#include <Library/ExtendedSalLib.h>

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
  This function is only available on IPF.

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
  )
{
  SAL_RETURN_REGS Regs;
  
  //
  // Initial all members in this structure.
  //
  Regs.r9     = 0;
  Regs.r10    = 0;
  Regs.r11    = 0;
  Regs.Status = EFI_SAL_INVALID_ARGUMENT;

  switch (Index) {
  case EFI_SAL_SET_VECTORS:
    return EsalCall (
             EFI_EXTENDED_SAL_BASE_SERVICES_PROTOCOL_GUID_LO,
             EFI_EXTENDED_SAL_BASE_SERVICES_PROTOCOL_GUID_HI,
             SalSetVectorsFunctionId, 
             Arg2, 
             Arg3, 
             Arg4, 
             Arg5, 
             Arg6, 
             Arg7, 
             Arg8
             );
    break;

  case EFI_SAL_GET_STATE_INFO:
    return EsalCall (
             EFI_EXTENDED_SAL_MCA_LOG_SERVICES_PROTOCOL_GUID_LO,
             EFI_EXTENDED_SAL_MCA_LOG_SERVICES_PROTOCOL_GUID_HI,
             SalGetStateInfoFunctionId, 
             Arg2, 
             Arg3, 
             Arg4, 
             Arg5, 
             Arg6, 
             Arg7, 
             Arg8
             );
    break;

  case EFI_SAL_GET_STATE_INFO_SIZE:
    return EsalCall (
             EFI_EXTENDED_SAL_MCA_LOG_SERVICES_PROTOCOL_GUID_LO,
             EFI_EXTENDED_SAL_MCA_LOG_SERVICES_PROTOCOL_GUID_HI,
             SalGetStateInfoSizeFunctionId, 
             Arg2, 
             Arg3, 
             Arg4, 
             Arg5, 
             Arg6, 
             Arg7, 
             Arg8
             );
    break;

  case EFI_SAL_CLEAR_STATE_INFO:
    return EsalCall (
             EFI_EXTENDED_SAL_MCA_LOG_SERVICES_PROTOCOL_GUID_LO,
             EFI_EXTENDED_SAL_MCA_LOG_SERVICES_PROTOCOL_GUID_HI,
             SalClearStateInfoFunctionId, 
             Arg2, 
             Arg3, 
             Arg4, 
             Arg5, 
             Arg6, 
             Arg7, 
             Arg8
             );
    break;

  case EFI_SAL_MC_RENDEZ:
    return EsalCall (
             EFI_EXTENDED_SAL_BASE_SERVICES_PROTOCOL_GUID_LO,
             EFI_EXTENDED_SAL_BASE_SERVICES_PROTOCOL_GUID_HI,
             SalMcRendezFunctionId, 
             Arg2, 
             Arg3, 
             Arg4, 
             Arg5, 
             Arg6, 
             Arg7, 
             Arg8
             );
   break;

  case EFI_SAL_MC_SET_PARAMS:
    return EsalCall (
             EFI_EXTENDED_SAL_BASE_SERVICES_PROTOCOL_GUID_LO,
             EFI_EXTENDED_SAL_BASE_SERVICES_PROTOCOL_GUID_HI,
             SalMcSetParamsFunctionId, 
             Arg2, 
             Arg3, 
             Arg4, 
             Arg5, 
             Arg6, 
             Arg7, 
             Arg8
             );
    break;

  case EFI_SAL_REGISTER_PHYSICAL_ADDR:
    return EsalCall (
             EFI_EXTENDED_SAL_BASE_SERVICES_PROTOCOL_GUID_LO,
             EFI_EXTENDED_SAL_BASE_SERVICES_PROTOCOL_GUID_HI,
             EsalRegisterPhysicalAddrFunctionId, 
             Arg2, 
             Arg3, 
             Arg4, 
             Arg5, 
             Arg6, 
             Arg7, 
             Arg8
             );
    break;

  case EFI_SAL_CACHE_FLUSH:
    return EsalCall (
             EFI_EXTENDED_SAL_CACHE_SERVICES_PROTOCOL_GUID_LO,
             EFI_EXTENDED_SAL_CACHE_SERVICES_PROTOCOL_GUID_HI,
             SalCacheFlushFunctionId, 
             Arg2, 
             Arg3, 
             Arg4, 
             Arg5, 
             Arg6, 
             Arg7, 
             Arg8
             );
    break;

  case EFI_SAL_CACHE_INIT:
    return EsalCall (
             EFI_EXTENDED_SAL_CACHE_SERVICES_PROTOCOL_GUID_LO,
             EFI_EXTENDED_SAL_CACHE_SERVICES_PROTOCOL_GUID_HI,
             SalCacheInitFunctionId, 
             Arg2, 
             Arg3, 
             Arg4, 
             Arg5, 
             Arg6, 
             Arg7, 
             Arg8
             );
    break;

  case EFI_SAL_PCI_CONFIG_READ:
    return EsalCall (
             EFI_EXTENDED_SAL_PCI_SERVICES_PROTOCOL_GUID_LO,
             EFI_EXTENDED_SAL_PCI_SERVICES_PROTOCOL_GUID_HI,
             SalPciConfigReadFunctionId, 
             Arg2, 
             Arg3, 
             Arg4, 
             Arg5, 
             Arg6, 
             Arg7, 
             Arg8
             );
    break;

  case EFI_SAL_PCI_CONFIG_WRITE:
    return EsalCall (
             EFI_EXTENDED_SAL_PCI_SERVICES_PROTOCOL_GUID_LO,
             EFI_EXTENDED_SAL_PCI_SERVICES_PROTOCOL_GUID_HI,
             SalPciConfigWriteFunctionId, 
             Arg2, 
             Arg3, 
             Arg4, 
             Arg5, 
             Arg6, 
             Arg7, 
             Arg8
             );
    break;

  case EFI_SAL_FREQ_BASE:
    return EsalCall (
             EFI_EXTENDED_SAL_BASE_SERVICES_PROTOCOL_GUID_LO,
             EFI_EXTENDED_SAL_BASE_SERVICES_PROTOCOL_GUID_HI,
             EsalGetPlatformBaseFreqFunctionId, 
             Arg2, 
             Arg3, 
             Arg4, 
             Arg5, 
             Arg6, 
             Arg7, 
             Arg8
             );
    break;

  case EFI_SAL_PHYSICAL_ID_INFO:
    return EsalCall (
             EFI_EXTENDED_SAL_BASE_SERVICES_PROTOCOL_GUID_LO,
             EFI_EXTENDED_SAL_BASE_SERVICES_PROTOCOL_GUID_HI,
             EsalPhysicalIdInfoFunctionId, 
             Arg2, 
             Arg3, 
             Arg4, 
             Arg5, 
             Arg6, 
             Arg7, 
             Arg8
             );
    break;

  case EFI_SAL_UPDATE_PAL:
    return EsalCall (
             EFI_EXTENDED_SAL_PAL_SERVICES_PROTOCOL_GUID_LO,
             EFI_EXTENDED_SAL_PAL_SERVICES_PROTOCOL_GUID_HI,
             EsalUpdatePalFunctionId, 
             Arg2, 
             Arg3, 
             Arg4, 
             Arg5, 
             Arg6, 
             Arg7, 
             Arg8
             );
    break;

  default:
    return Regs;
    break;
  }
}
