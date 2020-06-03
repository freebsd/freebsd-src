/** @file
  UEFI OS based application for unit testing the SafeIntLib.

  Copyright (c) Microsoft Corporation.<BR>
  Copyright (c) 2018 - 2020, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _TEST_BASE_SAFE_INT_LIB_H_
#define _TEST_BASE_SAFE_INT_LIB_H_

#include <PiPei.h>
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UnitTestLib.h>
#include <Library/SafeIntLib.h>

UNIT_TEST_STATUS
EFIAPI
TestSafeInt32ToUintn(
  IN UNIT_TEST_CONTEXT           Context
  );

UNIT_TEST_STATUS
EFIAPI
TestSafeUint32ToIntn(
  IN UNIT_TEST_CONTEXT           Context
  );

UNIT_TEST_STATUS
EFIAPI
TestSafeIntnToInt32(
  IN UNIT_TEST_CONTEXT           Context
  );

UNIT_TEST_STATUS
EFIAPI
TestSafeIntnToUint32(
  IN UNIT_TEST_CONTEXT           Context
  );

UNIT_TEST_STATUS
EFIAPI
TestSafeUintnToUint32(
  IN UNIT_TEST_CONTEXT           Context
  );

UNIT_TEST_STATUS
EFIAPI
TestSafeUintnToIntn(
  IN UNIT_TEST_CONTEXT           Context
  );

UNIT_TEST_STATUS
EFIAPI
TestSafeUintnToInt64(
  IN UNIT_TEST_CONTEXT           Context
  );

UNIT_TEST_STATUS
EFIAPI
TestSafeInt64ToIntn(
  IN UNIT_TEST_CONTEXT           Context
  );

UNIT_TEST_STATUS
EFIAPI
TestSafeInt64ToUintn(
  IN UNIT_TEST_CONTEXT           Context
  );

UNIT_TEST_STATUS
EFIAPI
TestSafeUint64ToIntn(
  IN UNIT_TEST_CONTEXT           Context
  );

UNIT_TEST_STATUS
EFIAPI
TestSafeUint64ToUintn(
  IN UNIT_TEST_CONTEXT           Context
  );

UNIT_TEST_STATUS
EFIAPI
TestSafeUintnAdd(
  IN UNIT_TEST_CONTEXT           Context
  );

UNIT_TEST_STATUS
EFIAPI
TestSafeIntnAdd(
  IN UNIT_TEST_CONTEXT           Context
  );

UNIT_TEST_STATUS
EFIAPI
TestSafeUintnSub(
  IN UNIT_TEST_CONTEXT           Context
  );

UNIT_TEST_STATUS
EFIAPI
TestSafeIntnSub(
  IN UNIT_TEST_CONTEXT           Context
  );

UNIT_TEST_STATUS
EFIAPI
TestSafeUintnMult(
  IN UNIT_TEST_CONTEXT           Context
  );

UNIT_TEST_STATUS
EFIAPI
TestSafeIntnMult(
  IN UNIT_TEST_CONTEXT           Context
  );

#endif
