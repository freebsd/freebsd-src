/** @file
  IA32-specific functions for unit-testing INTN and UINTN functions in
  SafeIntLib.

  Copyright (c) Microsoft Corporation.<BR>
  Copyright (c) 2019 - 2020, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <gtest/gtest.h>
extern "C" {
  #include <Base.h>
  #include <Library/SafeIntLib.h>
}

TEST (ConversionTestSuite, TestSafeInt32ToUintn) {
  RETURN_STATUS  Status;
  INT32          Operand;
  UINTN          Result;

  //
  // If Operand is non-negative, then it's a cast
  //
  Operand = 0x5bababab;
  Result  = 0;
  Status  = SafeInt32ToUintn (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((UINTN)0x5bababab, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (-1537977259);
  Status  = SafeInt32ToUintn (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeUint32ToIntn) {
  RETURN_STATUS  Status;
  UINT32         Operand;
  INTN           Result;

  //
  // If Operand is <= MAX_INTN, then it's a cast
  //
  Operand = 0x5bababab;
  Result  = 0;
  Status  = SafeUint32ToIntn (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5bababab, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0xabababab);
  Status  = SafeUint32ToIntn (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeIntnToInt32) {
  RETURN_STATUS  Status;
  INTN           Operand;
  INT32          Result;

  //
  // INTN is same as INT32 in IA32, so this is just a cast
  //
  Operand = 0x5bababab;
  Result  = 0;
  Status  = SafeIntnToInt32 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5bababab, Result);
}

TEST (ConversionTestSuite, TestSafeIntnToUint32) {
  RETURN_STATUS  Status;
  INTN           Operand;
  UINT32         Result;

  //
  // If Operand is non-negative, then it's a cast
  //
  Operand = 0x5bababab;
  Result  = 0;
  Status  = SafeIntnToUint32 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((UINT32)0x5bababab, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (-1537977259);
  Status  = SafeIntnToUint32 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeUintnToUint32) {
  RETURN_STATUS  Status;
  UINTN          Operand;
  UINT32         Result;

  //
  // UINTN is same as UINT32 in IA32, so this is just a cast
  //
  Operand = 0xabababab;
  Result  = 0;
  Status  = SafeUintnToUint32 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0xabababab, Result);
}

TEST (ConversionTestSuite, TestSafeUintnToIntn) {
  RETURN_STATUS  Status;
  UINTN          Operand;
  INTN           Result;

  //
  // If Operand is <= MAX_INTN, then it's a cast
  //
  Operand = 0x5bababab;
  Result  = 0;
  Status  = SafeUintnToIntn (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5bababab, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0xabababab);
  Status  = SafeUintnToIntn (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeUintnToInt64) {
  RETURN_STATUS  Status;
  UINTN          Operand;
  INT64          Result;

  //
  // UINTN is same as UINT32 in IA32, and UINT32 is a subset of
  // INT64, so this is just a cast
  //
  Operand = 0xabababab;
  Result  = 0;
  Status  = SafeUintnToInt64 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0xabababab, Result);
}

TEST (ConversionTestSuite, TestSafeInt64ToIntn) {
  RETURN_STATUS  Status;
  INT64          Operand;
  INTN           Result;

  //
  // If Operand is between MIN_INTN and  MAX_INTN2 inclusive, then it's a cast
  //
  Operand = 0x5bababab;
  Result  = 0;
  Status  = SafeInt64ToIntn (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5bababab, Result);

  Operand = (-1537977259);
  Status  = SafeInt64ToIntn (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((-1537977259), Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0x5babababefefefef);
  Status  = SafeInt64ToIntn (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Operand =  (-6605562033422200815);
  Status  = SafeInt64ToIntn (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeInt64ToUintn) {
  RETURN_STATUS  Status;
  INT64          Operand;
  UINTN          Result;

  //
  // If Operand is between 0 and  MAX_UINTN inclusive, then it's a cast
  //
  Operand = 0xabababab;
  Result  = 0;
  Status  = SafeInt64ToUintn (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0xabababab, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0x5babababefefefef);
  Status  = SafeInt64ToUintn (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Operand =  (-6605562033422200815);
  Status  = SafeInt64ToUintn (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeUint64ToIntn) {
  RETURN_STATUS  Status;
  UINT64         Operand;
  INTN           Result;

  //
  // If Operand is <= MAX_INTN, then it's a cast
  //
  Operand = 0x5bababab;
  Result  = 0;
  Status  = SafeUint64ToIntn (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5bababab, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0xababababefefefef);
  Status  = SafeUint64ToIntn (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeUint64ToUintn) {
  RETURN_STATUS  Status;
  UINT64         Operand;
  UINTN          Result;

  //
  // If Operand is <= MAX_UINTN, then it's a cast
  //
  Operand = 0xabababab;
  Result  = 0;
  Status  = SafeUint64ToUintn (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0xabababab, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0xababababefefefef);
  Status  = SafeUint64ToUintn (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (AdditionSubtractionTestSuite, TestSafeUintnAdd) {
  RETURN_STATUS  Status;
  UINTN          Augend;
  UINTN          Addend;
  UINTN          Result;

  //
  // If the result of addition doesn't overflow MAX_UINTN, then it's addition
  //
  Augend = 0x3a3a3a3a;
  Addend = 0x3a3a3a3a;
  Result = 0;
  Status = SafeUintnAdd (Augend, Addend, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((UINTN)0x74747474, Result);

  //
  // Otherwise should result in an error status
  //
  Augend = 0xabababab;
  Addend = 0xbcbcbcbc;
  Status = SafeUintnAdd (Augend, Addend, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (AdditionSubtractionTestSuite, TestSafeIntnAdd) {
  RETURN_STATUS  Status;
  INTN           Augend;
  INTN           Addend;
  INTN           Result;

  //
  // If the result of addition doesn't overflow MAX_INTN
  // and doesn't underflow MIN_INTN, then it's addition
  //
  Augend = 0x3a3a3a3a;
  Addend = 0x3a3a3a3a;
  Result = 0;
  Status = SafeIntnAdd (Augend, Addend, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x74747474, Result);

  Augend = (-976894522);
  Addend = (-976894522);
  Status = SafeIntnAdd (Augend, Addend, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((-1953789044), Result);

  //
  // Otherwise should result in an error status
  //
  Augend = 0x5a5a5a5a;
  Addend = 0x5a5a5a5a;
  Status = SafeIntnAdd (Augend, Addend, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Augend = (-1515870810);
  Addend = (-1515870810);
  Status = SafeIntnAdd (Augend, Addend, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (AdditionSubtractionTestSuite, TestSafeUintnSub) {
  RETURN_STATUS  Status;
  UINTN          Minuend;
  UINTN          Subtrahend;
  UINTN          Result;

  //
  // If Minuend >= Subtrahend, then it's subtraction
  //
  Minuend    = 0x5a5a5a5a;
  Subtrahend = 0x3b3b3b3b;
  Result     = 0;
  Status     = SafeUintnSub (Minuend, Subtrahend, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((UINTN)0x1f1f1f1f, Result);

  //
  // Otherwise should result in an error status
  //
  Minuend    = 0x5a5a5a5a;
  Subtrahend = 0x6d6d6d6d;
  Status     = SafeUintnSub (Minuend, Subtrahend, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (AdditionSubtractionTestSuite, TestSafeIntnSub) {
  RETURN_STATUS  Status;
  INTN           Minuend;
  INTN           Subtrahend;
  INTN           Result;

  //
  // If the result of subtractions doesn't overflow MAX_INTN or
  // underflow MIN_INTN, then it's subtraction
  //
  Minuend    = 0x5a5a5a5a;
  Subtrahend = 0x3a3a3a3a;
  Result     = 0;
  Status     = SafeIntnSub (Minuend, Subtrahend, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x20202020, Result);

  Minuend    = 0x3a3a3a3a;
  Subtrahend = 0x5a5a5a5a;
  Status     = SafeIntnSub (Minuend, Subtrahend, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((-538976288), Result);

  //
  // Otherwise should result in an error status
  //
  Minuend    = (-2054847098);
  Subtrahend = 2054847098;
  Status     = SafeIntnSub (Minuend, Subtrahend, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Minuend    = (2054847098);
  Subtrahend = (-2054847098);
  Status     = SafeIntnSub (Minuend, Subtrahend, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (MultiplicationTestSuite, TestSafeUintnMult) {
  RETURN_STATUS  Status;
  UINTN          Multiplicand;
  UINTN          Multiplier;
  UINTN          Result;

  //
  // If the result of multiplication doesn't overflow MAX_UINTN, it will succeed
  //
  Multiplicand = 0xa122a;
  Multiplier   = 0xd23;
  Result       = 0;
  Status       = SafeUintnMult (Multiplicand, Multiplier, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x844c9dbe, Result);

  //
  // Otherwise should result in an error status
  //
  Multiplicand = 0xa122a;
  Multiplier   = 0xed23;
  Status       = SafeUintnMult (Multiplicand, Multiplier, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (MultiplicationTestSuite, TestSafeIntnMult) {
  RETURN_STATUS  Status;
  INTN           Multiplicand;
  INTN           Multiplier;
  INTN           Result;

  //
  // If the result of multiplication doesn't overflow MAX_INTN and doesn't
  // underflow MIN_UINTN, it will succeed
  //
  Multiplicand = 0x123456;
  Multiplier   = 0x678;
  Result       = 0;
  Status       = SafeIntnMult (Multiplicand, Multiplier, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x75c28c50, Result);

  //
  // Otherwise should result in an error status
  //
  Multiplicand = 0x123456;
  Multiplier   = 0xabc;
  Status       = SafeIntnMult (Multiplicand, Multiplier, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}
