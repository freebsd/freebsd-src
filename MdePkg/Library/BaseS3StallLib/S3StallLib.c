/** @file
  Stall Services that do stall and also enable the Stall operatation
  to be replayed during an S3 resume. This library class maps directly on top
  of the Timer class. 

  Copyright (c) 2007, Intel Corporation. All rights reserved.<BR>

  This program and the accompanying materials
  are licensed and made available under the terms and conditions
  of the BSD License which accompanies this distribution.  The
  full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Base.h>

#include <Library/TimerLib.h>
#include <Library/DebugLib.h>
#include <Library/S3BootScriptLib.h>
#include <Library/S3StallLib.h>


/**
  Stalls the CPU for at least the given number of microseconds and and saves
  the value in the S3 script to be replayed on S3 resume.

  Stalls the CPU for the number of microseconds specified by MicroSeconds.

  @param  MicroSeconds  The minimum number of microseconds to delay.

  @return MicroSeconds

**/
UINTN
EFIAPI
S3Stall (
  IN UINTN                     MicroSeconds
  )
{
  RETURN_STATUS    Status;
  
  Status = S3BootScriptSaveStall (MicroSecondDelay (MicroSeconds));
  ASSERT (Status == RETURN_SUCCESS);
  
  return MicroSeconds;
}


