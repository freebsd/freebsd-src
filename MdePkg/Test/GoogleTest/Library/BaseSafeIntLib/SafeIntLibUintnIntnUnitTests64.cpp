/** @file
  x64-specific functions for unit-testing INTN and UINTN functions in
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
  // For x64, INTN is same as INT64 which is a superset of INT32
  // This is just a cast then, and it'll never fail
  //

  //
  // If Operand is non-negative, then it's a cast
  //
  Operand = 0xabababab;
  Result  = 0;
  Status  = SafeUint32ToIntn (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0xabababab, Result);
}

TEST (ConversionTestSuite, TestSafeIntnToInt32) {
  RETURN_STATUS  Status;
  INTN           Operand;
  INT32          Result;

  //
  // If Operand is between MIN_INT32 and  MAX_INT32 inclusive, then it's a cast
  //
  Operand = 0x5bababab;
  Result  = 0;
  Status  = SafeIntnToInt32 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5bababab, Result);

  Operand = (-1537977259);
  Status  = SafeIntnToInt32 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((-1537977259), Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0x5babababefefefef);
  Status  = SafeIntnToInt32 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Operand =  (-6605562033422200815);
  Status  = SafeIntnToInt32 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeIntnToUint32) {
  RETURN_STATUS  Status;
  INTN           Operand;
  UINT32         Result;

  //
  // If Operand is between 0 and  MAX_UINT32 inclusive, then it's a cast
  //
  Operand = 0xabababab;
  Result  = 0;
  Status  = SafeIntnToUint32 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0xabababab, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0x5babababefefefef);
  Status  = SafeIntnToUint32 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Operand =  (-6605562033422200815);
  Status  = SafeIntnToUint32 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeUintnToUint32) {
  RETURN_STATUS  Status;
  UINTN          Operand;
  UINT32         Result;

  //
  // If Operand is <= MAX_UINT32, then it's a cast
  //
  Operand = 0xabababab;
  Result  = 0;
  Status  = SafeUintnToUint32 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0xabababab, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0xababababefefefef);
  Status  = SafeUintnToUint32 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeUintnToIntn) {
  RETURN_STATUS  Status;
  UINTN          Operand;
  INTN           Result;

  //
  // If Operand is <= MAX_INTN (0x7fff_ffff_ffff_ffff), then it's a cast
  //
  Operand = 0x5babababefefefef;
  Result  = 0;
  Status  = SafeUintnToIntn (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5babababefefefef, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0xababababefefefef);
  Status  = SafeUintnToIntn (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeUintnToInt64) {
  RETURN_STATUS  Status;
  UINTN          Operand;
  INT64          Result;

  //
  // If Operand is <= MAX_INT64, then it's a cast
  //
  Operand = 0x5babababefefefef;
  Result  = 0;
  Status  = SafeUintnToInt64 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5babababefefefef, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0xababababefefefef);
  Status  = SafeUintnToInt64 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeInt64ToIntn) {
  RETURN_STATUS  Status;
  INT64          Operand;
  INTN           Result;

  //
  // INTN is same as INT64 in x64, so this is just a cast
  //
  Operand = 0x5babababefefefef;
  Result  = 0;
  Status  = SafeInt64ToIntn (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5babababefefefef, Result);
}

TEST (ConversionTestSuite, TestSafeInt64ToUintn) {
  RETURN_STATUS  Status;
  INT64          Operand;
  UINTN          Result;

  //
  // If Operand is non-negative, then it's a cast
  //
  Operand = 0x5babababefefefef;
  Result  = 0;
  Status  = SafeInt64ToUintn (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((UINTN)0x5babababefefefef, Result);

  //
  // Otherwise should result in an error status
  //
  Operand =  (-6605562033422200815);
  Status  = SafeInt64ToUintn (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeUint64ToIntn) {
  RETURN_STATUS  Status;
  UINT64         Operand;
  INTN           Result;

  //
  // If Operand is <= MAX_INTN (0x7fff_ffff_ffff_ffff), then it's a cast
  //
  Operand = 0x5babababefefefef;
  Result  = 0;
  Status  = SafeUint64ToIntn (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5babababefefefef, Result);

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
  // UINTN is same as UINT64 in x64, so this is just a cast
  //
  Operand = 0xababababefefefef;
  Result  = 0;
  Status  = SafeUint64ToUintn (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0xababababefefefef, Result);
}

TEST (AdditionSubtractionTestSuite, TestSafeUintnAdd) {
  RETURN_STATUS  Status;
  UINTN          Augend;
  UINTN          Addend;
  UINTN          Result;

  //
  // If the result of addition doesn't overflow MAX_UINTN, then it's addition
  //
  Augend = 0x3a3a3a3a12121212;
  Addend = 0x3a3a3a3a12121212;
  Result = 0;
  Status = SafeUintnAdd (Augend, Addend, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((UINTN)0x7474747424242424, Result);

  //
  // Otherwise should result in an error status
  //
  Augend = 0xababababefefefef;
  Addend = 0xbcbcbcbcdededede;
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
  Augend = 0x3a3a3a3a3a3a3a3a;
  Addend = 0x3a3a3a3a3a3a3a3a;
  Result = 0;
  Status = SafeIntnAdd (Augend, Addend, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x7474747474747474, Result);

  Augend = (-4195730024608447034);
  Addend = (-4195730024608447034);
  Status = SafeIntnAdd (Augend, Addend, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((-8391460049216894068), Result);

  //
  // Otherwise should result in an error status
  //
  Augend = 0x5a5a5a5a5a5a5a5a;
  Addend = 0x5a5a5a5a5a5a5a5a;
  Status = SafeIntnAdd (Augend, Addend, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Augend = (-6510615555426900570);
  Addend = (-6510615555426900570);
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
  Minuend    = 0x5a5a5a5a5a5a5a5a;
  Subtrahend = 0x3b3b3b3b3b3b3b3b;
  Result     = 0;
  Status     = SafeUintnSub (Minuend, Subtrahend, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((UINTN)0x1f1f1f1f1f1f1f1f, Result);

  //
  // Otherwise should result in an error status
  //
  Minuend    = 0x5a5a5a5a5a5a5a5a;
  Subtrahend = 0x6d6d6d6d6d6d6d6d;
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
  Minuend    = 0x5a5a5a5a5a5a5a5a;
  Subtrahend = 0x3a3a3a3a3a3a3a3a;
  Result     = 0;
  Status     = SafeIntnSub (Minuend, Subtrahend, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x2020202020202020, Result);

  Minuend    = 0x3a3a3a3a3a3a3a3a;
  Subtrahend = 0x5a5a5a5a5a5a5a5a;
  Status     = SafeIntnSub (Minuend, Subtrahend, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((-2314885530818453536), Result);

  //
  // Otherwise should result in an error status
  //
  Minuend    = (-8825501086245354106);
  Subtrahend = 8825501086245354106;
  Status     = SafeIntnSub (Minuend, Subtrahend, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Minuend    = (8825501086245354106);
  Subtrahend = (-8825501086245354106);
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
  Multiplicand = 0x123456789a;
  Multiplier   = 0x1234567;
  Result       = 0;
  Status       = SafeUintnMult (Multiplicand, Multiplier, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((UINTN)0x14b66db9745a07f6, Result);

  //
  // Otherwise should result in an error status
  //
  Multiplicand = 0x123456789a;
  Multiplier   = 0x12345678;
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
  Multiplicand = 0x123456789;
  Multiplier   = 0x6789abcd;
  Result       = 0;
  Status       = SafeIntnMult (Multiplicand, Multiplier, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x75cd9045220d6bb5, Result);

  //
  // Otherwise should result in an error status
  //
  Multiplicand = 0x123456789;
  Multiplier   = 0xa789abcd;
  Status       = SafeIntnMult (Multiplicand, Multiplier, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}
