/** @file
  Provides JEDEC JEP-106 Manufacturer functions.

  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef JEDEC_JEP106_LIB_H_
#define JEDEC_JEP106_LIB_H_

/**
  Looks up the JEP-106 manufacturer.

  @param Code              Last non-zero byte of the manufacturer's ID code.
  @param ContinuationBytes Number of continuation bytes indicated in JEP-106.

  @return The manufacturer string, or NULL if an error occurred or the
          combination of Code and ContinuationBytes are not valid.

**/
CONST CHAR8 *
EFIAPI
Jep106GetManufacturerName (
  IN UINT8  Code,
  IN UINT8  ContinuationBytes
  );

#endif /* JEDEC_JEP106_LIB_H_ */
