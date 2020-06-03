/** @file
  Stall Services that perform stalls and also enable the Stall operatation
  to be replayed during an S3 resume. This library class maps directly on top
  of the Timer class.

  Copyright (c) 2007 - 2018, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

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
