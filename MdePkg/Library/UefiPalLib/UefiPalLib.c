/** @file
  PAL Library implementation retrieving the PAL Entry Point from the SAL System Table
  register in the EFI System Confguration Table.

  Copyright (c) 2007 - 2008, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials are
  licensed and made available under the terms and conditions of
  the BSD License which accompanies this distribution.  The full
  text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.
  
  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <PiDxe.h>

#include <IndustryStandard/Sal.h>
#include <Library/UefiLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

#include <Guid/SalSystemTable.h>

UINT64               mPalProcEntry;

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
  //
  // mPalProcEntry is initialized in library constructor as PAL entry.
  //
  return AsmPalCall (
           mPalProcEntry,
           Index,
           Arg2,
           Arg3,
           Arg4
           );

}

/**
  The constructor function of UEFI Pal Lib.

  The constructor function looks up the SAL System Table in the EFI System Configuration
  Table. Once the SAL System Table is found, the PAL Entry Point in the SAL System Table
  will be derived and stored into a global variable for library usage.
  It will ASSERT() if the SAL System Table cannot be found or the data in the SAL System
  Table is not the valid data.

  @param  ImageHandle   The firmware allocated handle for the EFI image.
  @param  SystemTable   A pointer to the EFI System Table.

  @retval EFI_SUCCESS   The constructor always returns EFI_SUCCESS.

**/
EFI_STATUS
EFIAPI
UefiPalLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                     Status;
  SAL_ST_ENTRY_POINT_DESCRIPTOR  *SalStEntryDes;
  SAL_SYSTEM_TABLE_HEADER        *SalSystemTable;

  Status = EfiGetSystemConfigurationTable (
             &gEfiSalSystemTableGuid,
             (VOID **) &SalSystemTable
             );
  ASSERT_EFI_ERROR (Status);
  ASSERT (SalSystemTable != NULL);

  //
  // Check the first entry of SAL System Table,
  // because the SAL entry is in ascending order with the entry type,
  // the type 0 entry should be the first if exist.
  //
  SalStEntryDes = (SAL_ST_ENTRY_POINT_DESCRIPTOR *)(SalSystemTable + 1);

  //
  // Assure the SAL ENTRY Type is 0
  //
  ASSERT (SalStEntryDes->Type == EFI_SAL_ST_ENTRY_POINT);

  mPalProcEntry = SalStEntryDes->PalProcEntry;
  //
  // Make sure the PalCallAddress has the valid value
  //
  ASSERT (mPalProcEntry != 0);

  return EFI_SUCCESS;
}
