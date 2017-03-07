/** @file
  Stall Services that perform stalls and also enable the Stall operatation
  to be replayed during an S3 resume. This library class maps directly on top
  of the Timer class. 

  Copyright (c) 2007 - 2010, Intel Corporation. All rights reserved.<BR>

  This program and the accompanying materials
  are licensed and made available under the terms and conditions
  of the BSD License which accompanies this distribution.  The
  full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __S3_STALL_LIB_H__
#define __S3_STALL_LIB_H__

/**
  Stalls the CPU for at least the given number of microseconds and saves
  the value in the S3 script to be replayed on S3 resume.

  Stalls the CPU for the number of microseconds specified by MicroSeconds.

  @param[in] MicroSeconds   The minimum number of microseconds to delay.

  @return   MicroSeconds.

**/
UINTN
EFIAPI
S3Stall (
  IN UINTN  MicroSeconds
  );

#endif
