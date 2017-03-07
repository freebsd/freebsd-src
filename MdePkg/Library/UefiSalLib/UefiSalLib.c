/** @file
  SAL Library implementation retrieving the SAL Entry Point from the SAL System Table
  register in the EFI System Configuration Table.

  Copyright (c) 2007 - 2010, Intel Corporation. All rights reserved.<BR>
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

#include <Library/SalLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>

#include <Guid/SalSystemTable.h>

EFI_PLABEL       mPlabel;
SAL_PROC         mSalProcEntry;

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

  @param  Index       The SAL procedure Index number.
  @param  Arg2        The 2nd parameter for SAL procedure calls.
  @param  Arg3        The 3rd parameter for SAL procedure calls.
  @param  Arg4        The 4th parameter for SAL procedure calls.
  @param  Arg5        The 5th parameter for SAL procedure calls.
  @param  Arg6        The 6th parameter for SAL procedure calls.
  @param  Arg7        The 7th parameter for SAL procedure calls.
  @param  Arg8        The 8th parameter for SAL procedure calls.

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
  //
  // mSalProcEntry is initialized in library constructor as SAL entry.
  //
  return mSalProcEntry(
           Index,
           Arg2,
           Arg3,
           Arg4,
           Arg5,
           Arg6,
           Arg7,
           Arg8
           );

}

/**
  The constructor function of UEFI SAL Lib.

  The constructor function looks up the SAL System Table in the EFI System Configuration
  Table. Once the SAL System Table is found, the SAL Entry Point in the SAL System Table
  will be derived and stored into a global variable for library usage.
  It will ASSERT() if the SAL System Table cannot be found or the data in the SAL System
  Table is not the valid data.

  @param  ImageHandle   The firmware allocated handle for the EFI image.
  @param  SystemTable   A pointer to the EFI System Table.

  @retval EFI_SUCCESS   The constructor always returns EFI_SUCCESS.

**/
EFI_STATUS
EFIAPI
UefiSalLibConstructor (
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

  mPlabel.EntryPoint = SalStEntryDes->SalProcEntry;
  mPlabel.GP = SalStEntryDes->SalGlobalDataPointer;
  //
  // Make sure the EntryPoint has the valid value
  //
  ASSERT ((mPlabel.EntryPoint != 0) && (mPlabel.GP != 0));

  mSalProcEntry = (SAL_PROC)((UINT64)&(mPlabel.EntryPoint));

  return EFI_SUCCESS;
}
