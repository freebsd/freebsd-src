/** @file
  NULL instance of SMM Library.

  Copyright (c) 2009 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>
#include <Library/SmmLib.h>

/**
  Triggers an SMI at boot time.

  This function triggers a software SMM interrupt at boot time.

**/
VOID
EFIAPI
TriggerBootServiceSoftwareSmi (
  VOID
  )
{
  return;
}

/**
  Triggers an SMI at run time.

  This function triggers a software SMM interrupt at run time.

**/
VOID
EFIAPI
TriggerRuntimeSoftwareSmi (
  VOID
  )
{
  return;
}

/**
  Test if a boot time software SMI happened.

  This function tests if a software SMM interrupt happened. If a software SMM
  interrupt happened and it was triggered at boot time, it returns TRUE. Otherwise,
  it returns FALSE.

  @retval TRUE   A software SMI triggered at boot time happened.
  @retval FALSE  No software SMI happened or the software SMI was triggered at run time.

**/
BOOLEAN
EFIAPI
IsBootServiceSoftwareSmi (
  VOID
  )
{
  return FALSE;
}

/**
  Test if a run time software SMI happened.

  This function tests if a software SMM interrupt happened. If a software SMM
  interrupt happened and it was triggered at run time, it returns TRUE. Otherwise,
  it returns FALSE.

  @retval TRUE   A software SMI triggered at run time happened.
  @retval FALSE  No software SMI happened or the software SMI was triggered at boot time.

**/
BOOLEAN
EFIAPI
IsRuntimeSoftwareSmi (
  VOID
  )
{
  return FALSE;
}

/**
  Clear APM SMI Status Bit; Set the EOS bit.

**/
VOID
EFIAPI
ClearSmi (
  VOID
  )
{
  return;
}
