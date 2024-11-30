/** @file
  UEFI OS based application for unit testing the SafeIntLib.

  Copyright (c) Microsoft Corporation.<BR>
  Copyright (c) 2018 - 2022, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <gtest/gtest.h>
extern "C" {
  #include <Base.h>
  #include <Library/SafeIntLib.h>
}

//
// Conversion function tests:
//
TEST (ConversionTestSuite, TestSafeInt8ToUint8) {
  RETURN_STATUS  Status;
  INT8           Operand;
  UINT8          Result;

  //
  // Positive UINT8 should result in just a cast
  //
  Operand = 0x5b;
  Result  = 0;
  Status  = SafeInt8ToUint8 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5b, Result);

  //
  // Negative number should result in an error status
  //
  Operand = (-56);
  Status  = SafeInt8ToUint8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeInt8ToUint16) {
  RETURN_STATUS  Status;
  INT8           Operand;
  UINT16         Result;

  //
  // Positive UINT8 should result in just a cast
  //
  Operand = 0x5b;
  Result  = 0;
  Status  = SafeInt8ToUint16 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5b, Result);

  //
  // Negative number should result in an error status
  //
  Operand = (-56);
  Status  = SafeInt8ToUint16 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeInt8ToUint32) {
  RETURN_STATUS  Status;
  INT8           Operand;
  UINT32         Result;

  //
  // Positive UINT8 should result in just a cast
  //
  Operand = 0x5b;
  Result  = 0;
  Status  = SafeInt8ToUint32 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((UINT32)0x5b, Result);

  //
  // Negative number should result in an error status
  //
  Operand = (-56);
  Status  = SafeInt8ToUint32 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeInt8ToUintn) {
  RETURN_STATUS  Status;
  INT8           Operand;
  UINTN          Result;

  //
  // Positive UINT8 should result in just a cast
  //
  Operand = 0x5b;
  Result  = 0;
  Status  = SafeInt8ToUintn (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((UINTN)0x5b, Result);

  //
  // Negative number should result in an error status
  //
  Operand = (-56);
  Status  = SafeInt8ToUintn (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeInt8ToUint64) {
  RETURN_STATUS  Status;
  INT8           Operand;
  UINT64         Result;

  //
  // Positive UINT8 should result in just a cast
  //
  Operand = 0x5b;
  Result  = 0;
  Status  = SafeInt8ToUint64 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((UINT64)0x5b, Result);

  //
  // Negative number should result in an error status
  //
  Operand = (-56);
  Status  = SafeInt8ToUint64 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeUint8ToInt8) {
  RETURN_STATUS  Status;
  UINT8          Operand;
  INT8           Result;

  //
  // Operand <= 0x7F (MAX_INT8) should result in a cast
  //
  Operand = 0x5b;
  Result  = 0;
  Status  = SafeUint8ToInt8 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5b, Result);

  //
  // Operand larger than 0x7f should result in an error status
  //
  Operand = 0xaf;
  Status  = SafeUint8ToInt8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeUint8ToChar8) {
  RETURN_STATUS  Status;
  UINT8          Operand;
  CHAR8          Result;

  //
  // CHAR8 is typedefed as char, which by default is signed, thus
  // CHAR8 is same as INT8, so same tests as above:
  //

  //
  // Operand <= 0x7F (MAX_INT8) should result in a cast
  //
  Operand = 0x5b;
  Result  = 0;
  Status  = SafeUint8ToChar8 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5b, Result);

  //
  // Operand larger than 0x7f should result in an error status
  //
  Operand = 0xaf;
  Status  = SafeUint8ToChar8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeInt16ToInt8) {
  RETURN_STATUS  Status;
  INT16          Operand;
  INT8           Result;

  //
  // If Operand is between MIN_INT8 and MAX_INT8 inclusive, then it's a cast
  //
  Operand = 0x5b;
  Result  = 0;
  Status  = SafeInt16ToInt8 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5b, Result);

  Operand = (-35);
  Status  = SafeInt16ToInt8 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((-35), Result);

  //
  // Otherwise should result in an error status
  //
  Operand = 0x1234;
  Status  = SafeInt16ToInt8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Operand = (-17835);
  Status  = SafeInt16ToInt8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeInt16ToChar8) {
  RETURN_STATUS  Status;
  INT16          Operand;
  CHAR8          Result;

  //
  // CHAR8 is typedefed as char, which may be signed or unsigned based
  // on the compiler. Thus, for compatibility CHAR8 should be between 0 and MAX_INT8.
  //

  //
  // If Operand is between 0 and MAX_INT8 inclusive, then it's a cast
  //
  Operand = 0x5b;
  Result  = 0;
  Status  = SafeInt16ToChar8 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5b, Result);

  Operand = 0;
  Result  = 0;
  Status  = SafeInt16ToChar8 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0, Result);

  Operand = MAX_INT8;
  Result  = 0;
  Status  = SafeInt16ToChar8 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (MAX_INT8, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (-35);
  Status  = SafeInt16ToChar8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Operand = 0x1234;
  Status  = SafeInt16ToChar8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Operand = (-17835);
  Status  = SafeInt16ToChar8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeInt16ToUint8) {
  RETURN_STATUS  Status;
  INT16          Operand;
  UINT8          Result;

  //
  // If Operand is between 0 and MAX_INT8 inclusive, then it's a cast
  //
  Operand = 0x5b;
  Result  = 0;
  Status  = SafeInt16ToUint8 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5b, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = 0x1234;
  Status  = SafeInt16ToUint8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Operand = (-17835);
  Status  = SafeInt16ToUint8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeInt16ToUint16) {
  RETURN_STATUS  Status;
  INT16          Operand = 0x5b5b;
  UINT16         Result  = 0;

  //
  // If Operand is non-negative, then it's a cast
  //
  Status = SafeInt16ToUint16 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5b5b, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (-17835);
  Status  = SafeInt16ToUint16 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeInt16ToUint32) {
  RETURN_STATUS  Status;
  INT16          Operand;
  UINT32         Result;

  //
  // If Operand is non-negative, then it's a cast
  //
  Operand = 0x5b5b;
  Result  = 0;
  Status  = SafeInt16ToUint32 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((UINT32)0x5b5b, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (-17835);
  Status  = SafeInt16ToUint32 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeInt16ToUintn) {
  RETURN_STATUS  Status;
  INT16          Operand;
  UINTN          Result;

  //
  // If Operand is non-negative, then it's a cast
  //
  Operand = 0x5b5b;
  Result  = 0;
  Status  = SafeInt16ToUintn (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((UINTN)0x5b5b, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (-17835);
  Status  = SafeInt16ToUintn (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeInt16ToUint64) {
  RETURN_STATUS  Status;
  INT16          Operand;
  UINT64         Result;

  //
  // If Operand is non-negative, then it's a cast
  //
  Operand = 0x5b5b;
  Result  = 0;
  Status  = SafeInt16ToUint64 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((UINT64)0x5b5b, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (-17835);
  Status  = SafeInt16ToUint64 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeUint16ToInt8) {
  RETURN_STATUS  Status;
  UINT16         Operand;
  INT8           Result;

  //
  // If Operand is <= MAX_INT8, it's a cast
  //
  Operand = 0x5b;
  Result  = 0;
  Status  = SafeUint16ToInt8 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5b, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0x5b5b);
  Status  = SafeUint16ToInt8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeUint16ToChar8) {
  RETURN_STATUS  Status;
  UINT16         Operand;
  CHAR8          Result;

  // CHAR8 is typedefed as char, which by default is signed, thus
  // CHAR8 is same as INT8, so same tests as above:

  //
  // If Operand is <= MAX_INT8, it's a cast
  //
  Operand = 0x5b;
  Result  = 0;
  Status  = SafeUint16ToChar8 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5b, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0x5b5b);
  Status  = SafeUint16ToChar8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeUint16ToUint8) {
  RETURN_STATUS  Status;
  UINT16         Operand;
  UINT8          Result;

  //
  // If Operand is <= MAX_UINT8 (0xff), it's a cast
  //
  Operand = 0xab;
  Result  = 0;
  Status  = SafeUint16ToUint8 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0xab, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0x5b5b);
  Status  = SafeUint16ToUint8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeUint16ToInt16) {
  RETURN_STATUS  Status;
  UINT16         Operand;
  INT16          Result;

  //
  // If Operand is <= MAX_INT16 (0x7fff), it's a cast
  //
  Operand = 0x5b5b;
  Result  = 0;
  Status  = SafeUint16ToInt16 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5b5b, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0xabab);
  Status  = SafeUint16ToInt16 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeInt32ToInt8) {
  RETURN_STATUS  Status;
  INT32          Operand;
  INT8           Result;

  //
  // If Operand is between MIN_INT8 and MAX_INT8 inclusive, then it's a cast
  //
  Operand = 0x5b;
  Result  = 0;
  Status  = SafeInt32ToInt8 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5b, Result);

  Operand = (-57);
  Status  = SafeInt32ToInt8 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((-57), Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0x5bababab);
  Status  = SafeInt32ToInt8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Operand = (-1537977259);
  Status  = SafeInt32ToInt8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeInt32ToChar8) {
  RETURN_STATUS  Status;
  INT32          Operand;
  CHAR8          Result;

  //
  // CHAR8 is typedefed as char, which may be signed or unsigned based
  // on the compiler. Thus, for compatibility CHAR8 should be between 0 and MAX_INT8.
  //

  //
  // If Operand is between 0 and MAX_INT8 inclusive, then it's a cast
  //
  Operand = 0x5b;
  Result  = 0;
  Status  = SafeInt32ToChar8 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5b, Result);

  Operand = 0;
  Result  = 0;
  Status  = SafeInt32ToChar8 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0, Result);

  Operand = MAX_INT8;
  Result  = 0;
  Status  = SafeInt32ToChar8 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (MAX_INT8, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (-57);
  Status  = SafeInt32ToChar8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Operand = (0x5bababab);
  Status  = SafeInt32ToChar8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Operand = (-1537977259);
  Status  = SafeInt32ToChar8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeInt32ToUint8) {
  RETURN_STATUS  Status;
  INT32          Operand;
  UINT8          Result;

  //
  // If Operand is between 0 and MAX_INT8 inclusive, then it's a cast
  //
  Operand = 0x5b;
  Result  = 0;
  Status  = SafeInt32ToUint8 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5b, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (-57);
  Status  = SafeInt32ToUint8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Operand = (0x5bababab);
  Status  = SafeInt32ToUint8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Operand = (-1537977259);
  Status  = SafeInt32ToUint8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeInt32ToInt16) {
  RETURN_STATUS  Status;
  INT32          Operand;
  INT16          Result;

  //
  // If Operand is between MIN_INT16 and MAX_INT16 inclusive, then it's a cast
  //
  Operand = 0x5b5b;
  Result  = 0;
  Status  = SafeInt32ToInt16 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5b5b, Result);

  Operand = (-17857);
  Status  = SafeInt32ToInt16 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((-17857), Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0x5bababab);
  Status  = SafeInt32ToInt16 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Operand = (-1537977259);
  Status  = SafeInt32ToInt16 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeInt32ToUint16) {
  RETURN_STATUS  Status;
  INT32          Operand;
  UINT16         Result;

  //
  // If Operand is between 0 and MAX_UINT16 inclusive, then it's a cast
  //
  Operand = 0xabab;
  Result  = 0;
  Status  = SafeInt32ToUint16 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0xabab, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (-17857);
  Status  = SafeInt32ToUint16 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Operand = (0x5bababab);
  Status  = SafeInt32ToUint16 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Operand = (-1537977259);
  Status  = SafeInt32ToUint16 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeInt32ToUint32) {
  RETURN_STATUS  Status;
  INT32          Operand;
  UINT32         Result;

  //
  // If Operand is non-negative, then it's a cast
  //
  Operand = 0x5bababab;
  Result  = 0;
  Status  = SafeInt32ToUint32 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((UINT32)0x5bababab, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (-1537977259);
  Status  = SafeInt32ToUint32 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeInt32ToUint64) {
  RETURN_STATUS  Status;
  INT32          Operand;
  UINT64         Result;

  //
  // If Operand is non-negative, then it's a cast
  //
  Operand = 0x5bababab;
  Result  = 0;
  Status  = SafeInt32ToUint64 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((UINT64)0x5bababab, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (-1537977259);
  Status  = SafeInt32ToUint64 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeUint32ToInt8) {
  RETURN_STATUS  Status;
  UINT32         Operand;
  INT8           Result;

  //
  // If Operand is <= MAX_INT8, then it's a cast
  //
  Operand = 0x5b;
  Result  = 0;
  Status  = SafeUint32ToInt8 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5b, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0x5bababab);
  Status  = SafeUint32ToInt8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeUint32ToChar8) {
  RETURN_STATUS  Status;
  UINT32         Operand;
  CHAR8          Result;

  // CHAR8 is typedefed as char, which by default is signed, thus
  // CHAR8 is same as INT8, so same tests as above:

  //
  // If Operand is <= MAX_INT8, then it's a cast
  //
  Operand = 0x5b;
  Result  = 0;
  Status  = SafeUint32ToChar8 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5b, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0x5bababab);
  Status  = SafeUint32ToChar8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeUint32ToUint8) {
  RETURN_STATUS  Status;
  UINT32         Operand;
  UINT8          Result;

  //
  // If Operand is <= MAX_UINT8, then it's a cast
  //
  Operand = 0xab;
  Result  = 0;
  Status  = SafeUint32ToUint8 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0xab, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0xabababab);
  Status  = SafeUint32ToUint8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeUint32ToInt16) {
  RETURN_STATUS  Status;
  UINT32         Operand;
  INT16          Result;

  //
  // If Operand is <= MAX_INT16, then it's a cast
  //
  Operand = 0x5bab;
  Result  = 0;
  Status  = SafeUint32ToInt16 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5bab, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0xabababab);
  Status  = SafeUint32ToInt16 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeUint32ToUint16) {
  RETURN_STATUS  Status;
  UINT32         Operand;
  UINT16         Result;

  //
  // If Operand is <= MAX_UINT16, then it's a cast
  //
  Operand = 0xabab;
  Result  = 0;
  Status  = SafeUint32ToUint16 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0xabab, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0xabababab);
  Status  = SafeUint32ToUint16 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeUint32ToInt32) {
  RETURN_STATUS  Status;
  UINT32         Operand;
  INT32          Result;

  //
  // If Operand is <= MAX_INT32, then it's a cast
  //
  Operand = 0x5bababab;
  Result  = 0;
  Status  = SafeUint32ToInt32 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5bababab, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0xabababab);
  Status  = SafeUint32ToInt32 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeIntnToInt8) {
  RETURN_STATUS  Status;
  INTN           Operand;
  INT8           Result;

  //
  // If Operand is between MIN_INT8 and MAX_INT8 inclusive, then it's a cast
  //
  Operand = 0x5b;
  Result  = 0;
  Status  = SafeIntnToInt8 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5b, Result);

  Operand = (-53);
  Status  = SafeIntnToInt8 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((-53), Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0x5bababab);
  Status  = SafeIntnToInt8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Operand = (-1537977259);
  Status  = SafeIntnToInt8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeIntnToChar8) {
  RETURN_STATUS  Status;
  INTN           Operand;
  CHAR8          Result;

  //
  // CHAR8 is typedefed as char, which may be signed or unsigned based
  // on the compiler. Thus, for compatibility CHAR8 should be between 0 and MAX_INT8.
  //

  //
  // If Operand is between MIN_INT8 and MAX_INT8 inclusive, then it's a cast
  //
  Operand = 0x5b;
  Result  = 0;
  Status  = SafeIntnToChar8 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5b, Result);

  Operand = 0;
  Result  = 0;
  Status  = SafeIntnToChar8 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0, Result);

  Operand = MAX_INT8;
  Result  = 0;
  Status  = SafeIntnToChar8 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (MAX_INT8, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (-53);
  Status  = SafeIntnToChar8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Operand = (0x5bababab);
  Status  = SafeIntnToChar8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Operand = (-1537977259);
  Status  = SafeIntnToChar8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeIntnToUint8) {
  RETURN_STATUS  Status;
  INTN           Operand;
  UINT8          Result;

  //
  // If Operand is between 0 and MAX_UINT8 inclusive, then it's a cast
  //
  Operand = 0xab;
  Result  = 0;
  Status  = SafeIntnToUint8 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0xab, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0x5bababab);
  Status  = SafeIntnToUint8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Operand = (-1537977259);
  Status  = SafeIntnToUint8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeIntnToInt16) {
  RETURN_STATUS  Status;
  INTN           Operand;
  INT16          Result;

  //
  // If Operand is between MIN_INT16 and MAX_INT16 inclusive, then it's a cast
  //
  Operand = 0x5bab;
  Result  = 0;
  Status  = SafeIntnToInt16 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5bab, Result);

  Operand = (-23467);
  Status  = SafeIntnToInt16 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((-23467), Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0x5bababab);
  Status  = SafeIntnToInt16 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Operand = (-1537977259);
  Status  = SafeIntnToInt16 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeIntnToUint16) {
  RETURN_STATUS  Status;
  INTN           Operand;
  UINT16         Result;

  //
  // If Operand is between 0 and MAX_UINT16 inclusive, then it's a cast
  //
  Operand = 0xabab;
  Result  = 0;
  Status  = SafeIntnToUint16 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0xabab, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0x5bababab);
  Status  = SafeIntnToUint16 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Operand = (-1537977259);
  Status  = SafeIntnToUint16 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeIntnToUintn) {
  RETURN_STATUS  Status;
  INTN           Operand;
  UINTN          Result;

  //
  // If Operand is non-negative, then it's a cast
  //
  Operand = 0x5bababab;
  Result  = 0;
  Status  = SafeIntnToUintn (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((UINTN)0x5bababab, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (-1537977259);
  Status  = SafeIntnToUintn (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeIntnToUint64) {
  RETURN_STATUS  Status;
  INTN           Operand;
  UINT64         Result;

  //
  // If Operand is non-negative, then it's a cast
  //
  Operand = 0x5bababab;
  Result  = 0;
  Status  = SafeIntnToUint64 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((UINT64)0x5bababab, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (-1537977259);
  Status  = SafeIntnToUint64 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeUintnToInt8) {
  RETURN_STATUS  Status;
  UINTN          Operand;
  INT8           Result;

  //
  // If Operand is <= MAX_INT8, then it's a cast
  //
  Operand = 0x5b;
  Result  = 0;
  Status  = SafeUintnToInt8 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5b, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0xabab);
  Status  = SafeUintnToInt8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeUintnToChar8) {
  RETURN_STATUS  Status;
  UINTN          Operand;
  CHAR8          Result;

  // CHAR8 is typedefed as char, which by default is signed, thus
  // CHAR8 is same as INT8, so same tests as above:

  //
  // If Operand is <= MAX_INT8, then it's a cast
  //
  Operand = 0x5b;
  Result  = 0;
  Status  = SafeUintnToChar8 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5b, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0xabab);
  Status  = SafeUintnToChar8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeUintnToUint8) {
  RETURN_STATUS  Status;
  UINTN          Operand;
  UINT8          Result;

  //
  // If Operand is <= MAX_UINT8, then it's a cast
  //
  Operand = 0xab;
  Result  = 0;
  Status  = SafeUintnToUint8 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0xab, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0xabab);
  Status  = SafeUintnToUint8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeUintnToInt16) {
  RETURN_STATUS  Status;
  UINTN          Operand;
  INT16          Result;

  //
  // If Operand is <= MAX_INT16, then it's a cast
  //
  Operand = 0x5bab;
  Result  = 0;
  Status  = SafeUintnToInt16 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5bab, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0xabab);
  Status  = SafeUintnToInt16 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeUintnToUint16) {
  RETURN_STATUS  Status;
  UINTN          Operand;
  UINT16         Result;

  //
  // If Operand is <= MAX_UINT16, then it's a cast
  //
  Operand = 0xabab;
  Result  = 0;
  Status  = SafeUintnToUint16 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0xabab, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0xabababab);
  Status  = SafeUintnToUint16 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeUintnToInt32) {
  RETURN_STATUS  Status;
  UINTN          Operand;
  INT32          Result;

  //
  // If Operand is <= MAX_INT32, then it's a cast
  //
  Operand = 0x5bababab;
  Result  = 0;
  Status  = SafeUintnToInt32 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5bababab, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0xabababab);
  Status  = SafeUintnToInt32 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeInt64ToInt8) {
  RETURN_STATUS  Status;
  INT64          Operand;
  INT8           Result;

  //
  // If Operand is between MIN_INT8 and  MAX_INT8 inclusive, then it's a cast
  //
  Operand = 0x5b;
  Result  = 0;
  Status  = SafeInt64ToInt8 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5b, Result);

  Operand = (-37);
  Status  = SafeInt64ToInt8 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((-37), Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0x5babababefefefef);
  Status  = SafeInt64ToInt8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Operand =  (-6605562033422200815);
  Status  = SafeInt64ToInt8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeInt64ToChar8) {
  RETURN_STATUS  Status;
  INT64          Operand;
  CHAR8          Result;

  //
  // CHAR8 is typedefed as char, which may be signed or unsigned based
  // on the compiler. Thus, for compatibility CHAR8 should be between 0 and MAX_INT8.
  //

  //
  // If Operand is between MIN_INT8 and  MAX_INT8 inclusive, then it's a cast
  //
  Operand = 0x5b;
  Result  = 0;
  Status  = SafeInt64ToChar8 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5b, Result);

  Operand = 0;
  Result  = 0;
  Status  = SafeInt64ToChar8 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0, Result);

  Operand = MAX_INT8;
  Result  = 0;
  Status  = SafeInt64ToChar8 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (MAX_INT8, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (-37);
  Status  = SafeInt64ToChar8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Operand = (0x5babababefefefef);
  Status  = SafeInt64ToChar8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Operand =  (-6605562033422200815);
  Status  = SafeInt64ToChar8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeInt64ToUint8) {
  RETURN_STATUS  Status;
  INT64          Operand;
  UINT8          Result;

  //
  // If Operand is between 0 and  MAX_UINT8 inclusive, then it's a cast
  //
  Operand = 0xab;
  Result  = 0;
  Status  = SafeInt64ToUint8 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0xab, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0x5babababefefefef);
  Status  = SafeInt64ToUint8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Operand =  (-6605562033422200815);
  Status  = SafeInt64ToUint8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeInt64ToInt16) {
  RETURN_STATUS  Status;
  INT64          Operand;
  INT16          Result;

  //
  // If Operand is between MIN_INT16 and  MAX_INT16 inclusive, then it's a cast
  //
  Operand = 0x5bab;
  Result  = 0;
  Status  = SafeInt64ToInt16 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5bab, Result);

  Operand = (-23467);
  Status  = SafeInt64ToInt16 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((-23467), Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0x5babababefefefef);
  Status  = SafeInt64ToInt16 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Operand =  (-6605562033422200815);
  Status  = SafeInt64ToInt16 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeInt64ToUint16) {
  RETURN_STATUS  Status;
  INT64          Operand;
  UINT16         Result;

  //
  // If Operand is between 0 and  MAX_UINT16 inclusive, then it's a cast
  //
  Operand = 0xabab;
  Result  = 0;
  Status  = SafeInt64ToUint16 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0xabab, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0x5babababefefefef);
  Status  = SafeInt64ToUint16 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Operand =  (-6605562033422200815);
  Status  = SafeInt64ToUint16 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeInt64ToInt32) {
  RETURN_STATUS  Status;
  INT64          Operand;
  INT32          Result;

  //
  // If Operand is between MIN_INT32 and  MAX_INT32 inclusive, then it's a cast
  //
  Operand = 0x5bababab;
  Result  = 0;
  Status  = SafeInt64ToInt32 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5bababab, Result);

  Operand = (-1537977259);
  Status  = SafeInt64ToInt32 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((-1537977259), Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0x5babababefefefef);
  Status  = SafeInt64ToInt32 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Operand =  (-6605562033422200815);
  Status  = SafeInt64ToInt32 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeInt64ToUint32) {
  RETURN_STATUS  Status;
  INT64          Operand;
  UINT32         Result;

  //
  // If Operand is between 0 and  MAX_UINT32 inclusive, then it's a cast
  //
  Operand = 0xabababab;
  Result  = 0;
  Status  = SafeInt64ToUint32 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0xabababab, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0x5babababefefefef);
  Status  = SafeInt64ToUint32 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Operand =  (-6605562033422200815);
  Status  = SafeInt64ToUint32 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeInt64ToUint64) {
  RETURN_STATUS  Status;
  INT64          Operand;
  UINT64         Result;

  //
  // If Operand is non-negative, then it's a cast
  //
  Operand = 0x5babababefefefef;
  Result  = 0;
  Status  = SafeInt64ToUint64 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((UINT64)0x5babababefefefef, Result);

  //
  // Otherwise should result in an error status
  //
  Operand =  (-6605562033422200815);
  Status  = SafeInt64ToUint64 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeUint64ToInt8) {
  RETURN_STATUS  Status;
  UINT64         Operand;
  INT8           Result;

  //
  // If Operand is <= MAX_INT8, then it's a cast
  //
  Operand = 0x5b;
  Result  = 0;
  Status  = SafeUint64ToInt8 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5b, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0xababababefefefef);
  Status  = SafeUint64ToInt8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeUint64ToChar8) {
  RETURN_STATUS  Status;
  UINT64         Operand;
  CHAR8          Result;

  // CHAR8 is typedefed as char, which by default is signed, thus
  // CHAR8 is same as INT8, so same tests as above:

  //
  // If Operand is <= MAX_INT8, then it's a cast
  //
  Operand = 0x5b;
  Result  = 0;
  Status  = SafeUint64ToChar8 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5b, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0xababababefefefef);
  Status  = SafeUint64ToChar8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeUint64ToUint8) {
  RETURN_STATUS  Status;
  UINT64         Operand;
  UINT8          Result;

  //
  // If Operand is <= MAX_UINT8, then it's a cast
  //
  Operand = 0xab;
  Result  = 0;
  Status  = SafeUint64ToUint8 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0xab, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0xababababefefefef);
  Status  = SafeUint64ToUint8 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeUint64ToInt16) {
  RETURN_STATUS  Status;
  UINT64         Operand;
  INT16          Result;

  //
  // If Operand is <= MAX_INT16, then it's a cast
  //
  Operand = 0x5bab;
  Result  = 0;
  Status  = SafeUint64ToInt16 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5bab, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0xababababefefefef);
  Status  = SafeUint64ToInt16 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeUint64ToUint16) {
  RETURN_STATUS  Status;
  UINT64         Operand;
  UINT16         Result;

  //
  // If Operand is <= MAX_UINT16, then it's a cast
  //
  Operand = 0xabab;
  Result  = 0;
  Status  = SafeUint64ToUint16 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0xabab, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0xababababefefefef);
  Status  = SafeUint64ToUint16 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeUint64ToInt32) {
  RETURN_STATUS  Status;
  UINT64         Operand;
  INT32          Result;

  //
  // If Operand is <= MAX_INT32, then it's a cast
  //
  Operand = 0x5bababab;
  Result  = 0;
  Status  = SafeUint64ToInt32 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5bababab, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0xababababefefefef);
  Status  = SafeUint64ToInt32 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeUint64ToUint32) {
  RETURN_STATUS  Status;
  UINT64         Operand;
  UINT32         Result;

  //
  // If Operand is <= MAX_UINT32, then it's a cast
  //
  Operand = 0xabababab;
  Result  = 0;
  Status  = SafeUint64ToUint32 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0xabababab, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0xababababefefefef);
  Status  = SafeUint64ToUint32 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (ConversionTestSuite, TestSafeUint64ToInt64) {
  RETURN_STATUS  Status;
  UINT64         Operand;
  INT64          Result;

  //
  // If Operand is <= MAX_INT64, then it's a cast
  //
  Operand = 0x5babababefefefef;
  Result  = 0;
  Status  = SafeUint64ToInt64 (Operand, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x5babababefefefef, Result);

  //
  // Otherwise should result in an error status
  //
  Operand = (0xababababefefefef);
  Status  = SafeUint64ToInt64 (Operand, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

//
// Addition function tests:
//
TEST (AdditionSubtractionTestSuite, TestSafeUint8Add) {
  RETURN_STATUS  Status;
  UINT8          Augend;
  UINT8          Addend;
  UINT8          Result;

  //
  // If the result of addition doesn't overflow MAX_UINT8, then it's addition
  //
  Augend = 0x3a;
  Addend = 0x3a;
  Result = 0;
  Status = SafeUint8Add (Augend, Addend, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x74, Result);

  //
  // Otherwise should result in an error status
  //
  Augend = 0xab;
  Addend = 0xbc;
  Status = SafeUint8Add (Augend, Addend, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (AdditionSubtractionTestSuite, TestSafeUint16Add) {
  RETURN_STATUS  Status;
  UINT16         Augend = 0x3a3a;
  UINT16         Addend = 0x3a3a;
  UINT16         Result = 0;

  //
  // If the result of addition doesn't overflow MAX_UINT16, then it's addition
  //
  Status = SafeUint16Add (Augend, Addend, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x7474, Result);

  //
  // Otherwise should result in an error status
  //
  Augend = 0xabab;
  Addend = 0xbcbc;
  Status = SafeUint16Add (Augend, Addend, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (AdditionSubtractionTestSuite, TestSafeUint32Add) {
  RETURN_STATUS  Status;
  UINT32         Augend;
  UINT32         Addend;
  UINT32         Result;

  //
  // If the result of addition doesn't overflow MAX_UINT32, then it's addition
  //
  Augend = 0x3a3a3a3a;
  Addend = 0x3a3a3a3a;
  Result = 0;
  Status = SafeUint32Add (Augend, Addend, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((UINT32)0x74747474, Result);

  //
  // Otherwise should result in an error status
  //
  Augend = 0xabababab;
  Addend = 0xbcbcbcbc;
  Status = SafeUint32Add (Augend, Addend, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (AdditionSubtractionTestSuite, TestSafeUint64Add) {
  RETURN_STATUS  Status;
  UINT64         Augend;
  UINT64         Addend;
  UINT64         Result;

  //
  // If the result of addition doesn't overflow MAX_UINT64, then it's addition
  //
  Augend = 0x3a3a3a3a12121212;
  Addend = 0x3a3a3a3a12121212;
  Result = 0;
  Status = SafeUint64Add (Augend, Addend, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((UINT64)0x7474747424242424, Result);

  //
  // Otherwise should result in an error status
  //
  Augend = 0xababababefefefef;
  Addend = 0xbcbcbcbcdededede;
  Status = SafeUint64Add (Augend, Addend, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (AdditionSubtractionTestSuite, TestSafeInt8Add) {
  RETURN_STATUS  Status;
  INT8           Augend;
  INT8           Addend;
  INT8           Result;

  //
  // If the result of addition doesn't overflow MAX_INT8
  // and doesn't underflow MIN_INT8, then it's addition
  //
  Augend = 0x3a;
  Addend = 0x3a;
  Result = 0;
  Status = SafeInt8Add (Augend, Addend, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x74, Result);

  Augend = (-58);
  Addend = (-58);
  Status = SafeInt8Add (Augend, Addend, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((-116), Result);

  //
  // Otherwise should result in an error status
  //
  Augend = 0x5a;
  Addend = 0x5a;
  Status = SafeInt8Add (Augend, Addend, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Augend = (-90);
  Addend = (-90);
  Status = SafeInt8Add (Augend, Addend, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (AdditionSubtractionTestSuite, TestSafeInt16Add) {
  RETURN_STATUS  Status;
  INT16          Augend;
  INT16          Addend;
  INT16          Result;

  //
  // If the result of addition doesn't overflow MAX_INT16
  // and doesn't underflow MIN_INT16, then it's addition
  //
  Augend = 0x3a3a;
  Addend = 0x3a3a;
  Result = 0;
  Status = SafeInt16Add (Augend, Addend, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x7474, Result);

  Augend = (-14906);
  Addend = (-14906);
  Status = SafeInt16Add (Augend, Addend, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((-29812), Result);

  //
  // Otherwise should result in an error status
  //
  Augend = 0x5a5a;
  Addend = 0x5a5a;
  Status = SafeInt16Add (Augend, Addend, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Augend = (-23130);
  Addend = (-23130);
  Status = SafeInt16Add (Augend, Addend, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (AdditionSubtractionTestSuite, TestSafeInt32Add) {
  RETURN_STATUS  Status;
  INT32          Augend;
  INT32          Addend;
  INT32          Result;

  //
  // If the result of addition doesn't overflow MAX_INT32
  // and doesn't underflow MIN_INT32, then it's addition
  //
  Augend = 0x3a3a3a3a;
  Addend = 0x3a3a3a3a;
  Result = 0;
  Status = SafeInt32Add (Augend, Addend, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x74747474, Result);

  Augend = (-976894522);
  Addend = (-976894522);
  Status = SafeInt32Add (Augend, Addend, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((-1953789044), Result);

  //
  // Otherwise should result in an error status
  //
  Augend = 0x5a5a5a5a;
  Addend = 0x5a5a5a5a;
  Status = SafeInt32Add (Augend, Addend, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Augend = (-1515870810);
  Addend = (-1515870810);
  Status = SafeInt32Add (Augend, Addend, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (AdditionSubtractionTestSuite, TestSafeInt64Add) {
  RETURN_STATUS  Status;
  INT64          Augend;
  INT64          Addend;
  INT64          Result;

  //
  // If the result of addition doesn't overflow MAX_INT64
  // and doesn't underflow MIN_INT64, then it's addition
  //
  Augend = 0x3a3a3a3a3a3a3a3a;
  Addend = 0x3a3a3a3a3a3a3a3a;
  Result = 0;
  Status = SafeInt64Add (Augend, Addend, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x7474747474747474, Result);

  Augend = (-4195730024608447034);
  Addend = (-4195730024608447034);
  Status = SafeInt64Add (Augend, Addend, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((-8391460049216894068), Result);

  //
  // Otherwise should result in an error status
  //
  Augend = 0x5a5a5a5a5a5a5a5a;
  Addend = 0x5a5a5a5a5a5a5a5a;
  Status = SafeInt64Add (Augend, Addend, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Augend = (-6510615555426900570);
  Addend = (-6510615555426900570);
  Status = SafeInt64Add (Augend, Addend, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

//
// Subtraction function tests:
//
TEST (AdditionSubtractionTestSuite, TestSafeUint8Sub) {
  RETURN_STATUS  Status;
  UINT8          Minuend;
  UINT8          Subtrahend;
  UINT8          Result;

  //
  // If Minuend >= Subtrahend, then it's subtraction
  //
  Minuend    = 0x5a;
  Subtrahend = 0x3b;
  Result     = 0;
  Status     = SafeUint8Sub (Minuend, Subtrahend, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x1f, Result);

  //
  // Otherwise should result in an error status
  //
  Minuend    = 0x5a;
  Subtrahend = 0x6d;
  Status     = SafeUint8Sub (Minuend, Subtrahend, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (AdditionSubtractionTestSuite, TestSafeUint16Sub) {
  RETURN_STATUS  Status;
  UINT16         Minuend;
  UINT16         Subtrahend;
  UINT16         Result;

  //
  // If Minuend >= Subtrahend, then it's subtraction
  //
  Minuend    = 0x5a5a;
  Subtrahend = 0x3b3b;
  Result     = 0;
  Status     = SafeUint16Sub (Minuend, Subtrahend, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x1f1f, Result);

  //
  // Otherwise should result in an error status
  //
  Minuend    = 0x5a5a;
  Subtrahend = 0x6d6d;
  Status     = SafeUint16Sub (Minuend, Subtrahend, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (AdditionSubtractionTestSuite, TestSafeUint32Sub) {
  RETURN_STATUS  Status;
  UINT32         Minuend;
  UINT32         Subtrahend;
  UINT32         Result;

  //
  // If Minuend >= Subtrahend, then it's subtraction
  //
  Minuend    = 0x5a5a5a5a;
  Subtrahend = 0x3b3b3b3b;
  Result     = 0;
  Status     = SafeUint32Sub (Minuend, Subtrahend, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((UINT32)0x1f1f1f1f, Result);

  //
  // Otherwise should result in an error status
  //
  Minuend    = 0x5a5a5a5a;
  Subtrahend = 0x6d6d6d6d;
  Status     = SafeUint32Sub (Minuend, Subtrahend, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (AdditionSubtractionTestSuite, TestSafeUint64Sub) {
  RETURN_STATUS  Status;
  UINT64         Minuend;
  UINT64         Subtrahend;
  UINT64         Result;

  //
  // If Minuend >= Subtrahend, then it's subtraction
  //
  Minuend    = 0x5a5a5a5a5a5a5a5a;
  Subtrahend = 0x3b3b3b3b3b3b3b3b;
  Result     = 0;
  Status     = SafeUint64Sub (Minuend, Subtrahend, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((UINT64)0x1f1f1f1f1f1f1f1f, Result);

  //
  // Otherwise should result in an error status
  //
  Minuend    = 0x5a5a5a5a5a5a5a5a;
  Subtrahend = 0x6d6d6d6d6d6d6d6d;
  Status     = SafeUint64Sub (Minuend, Subtrahend, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (AdditionSubtractionTestSuite, TestSafeInt8Sub) {
  RETURN_STATUS  Status;
  INT8           Minuend;
  INT8           Subtrahend;
  INT8           Result;

  //
  // If the result of subtractions doesn't overflow MAX_INT8 or
  // underflow MIN_INT8, then it's subtraction
  //
  Minuend    = 0x5a;
  Subtrahend = 0x3a;
  Result     = 0;
  Status     = SafeInt8Sub (Minuend, Subtrahend, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x20, Result);

  Minuend    = 58;
  Subtrahend = 78;
  Status     = SafeInt8Sub (Minuend, Subtrahend, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((-20), Result);

  //
  // Otherwise should result in an error status
  //
  Minuend    = (-80);
  Subtrahend = 80;
  Status     = SafeInt8Sub (Minuend, Subtrahend, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Minuend    = (80);
  Subtrahend = (-80);
  Status     = SafeInt8Sub (Minuend, Subtrahend, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (AdditionSubtractionTestSuite, TestSafeInt16Sub) {
  RETURN_STATUS  Status;
  INT16          Minuend;
  INT16          Subtrahend;
  INT16          Result;

  //
  // If the result of subtractions doesn't overflow MAX_INT16 or
  // underflow MIN_INT16, then it's subtraction
  //
  Minuend    = 0x5a5a;
  Subtrahend = 0x3a3a;
  Result     = 0;
  Status     = SafeInt16Sub (Minuend, Subtrahend, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x2020, Result);

  Minuend    = 0x3a3a;
  Subtrahend = 0x5a5a;
  Status     = SafeInt16Sub (Minuend, Subtrahend, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((-8224), Result);

  //
  // Otherwise should result in an error status
  //
  Minuend    = (-31354);
  Subtrahend = 31354;
  Status     = SafeInt16Sub (Minuend, Subtrahend, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Minuend    = (31354);
  Subtrahend = (-31354);
  Status     = SafeInt16Sub (Minuend, Subtrahend, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (AdditionSubtractionTestSuite, TestSafeInt32Sub) {
  RETURN_STATUS  Status;
  INT32          Minuend;
  INT32          Subtrahend;
  INT32          Result;

  //
  // If the result of subtractions doesn't overflow MAX_INT32 or
  // underflow MIN_INT32, then it's subtraction
  //
  Minuend    = 0x5a5a5a5a;
  Subtrahend = 0x3a3a3a3a;
  Result     = 0;
  Status     = SafeInt32Sub (Minuend, Subtrahend, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x20202020, Result);

  Minuend    = 0x3a3a3a3a;
  Subtrahend = 0x5a5a5a5a;
  Status     = SafeInt32Sub (Minuend, Subtrahend, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((-538976288), Result);

  //
  // Otherwise should result in an error status
  //
  Minuend    = (-2054847098);
  Subtrahend = 2054847098;
  Status     = SafeInt32Sub (Minuend, Subtrahend, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Minuend    = (2054847098);
  Subtrahend = (-2054847098);
  Status     = SafeInt32Sub (Minuend, Subtrahend, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (AdditionSubtractionTestSuite, TestSafeInt64Sub) {
  RETURN_STATUS  Status;
  INT64          Minuend;
  INT64          Subtrahend;
  INT64          Result;

  //
  // If the result of subtractions doesn't overflow MAX_INT64 or
  // underflow MIN_INT64, then it's subtraction
  //
  Minuend    = 0x5a5a5a5a5a5a5a5a;
  Subtrahend = 0x3a3a3a3a3a3a3a3a;
  Result     = 0;
  Status     = SafeInt64Sub (Minuend, Subtrahend, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x2020202020202020, Result);

  Minuend    = 0x3a3a3a3a3a3a3a3a;
  Subtrahend = 0x5a5a5a5a5a5a5a5a;
  Status     = SafeInt64Sub (Minuend, Subtrahend, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((-2314885530818453536), Result);

  //
  // Otherwise should result in an error status
  //
  Minuend    = (-8825501086245354106);
  Subtrahend = 8825501086245354106;
  Status     = SafeInt64Sub (Minuend, Subtrahend, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);

  Minuend    = (8825501086245354106);
  Subtrahend = (-8825501086245354106);
  Status     = SafeInt64Sub (Minuend, Subtrahend, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

//
// Multiplication function tests:
//
TEST (MultiplicationTestSuite, TestSafeUint8Mult) {
  RETURN_STATUS  Status;
  UINT8          Multiplicand;
  UINT8          Multiplier;
  UINT8          Result;

  //
  // If the result of multiplication doesn't overflow MAX_UINT8, it will succeed
  //
  Multiplicand = 0x12;
  Multiplier   = 0xa;
  Result       = 0;
  Status       = SafeUint8Mult (Multiplicand, Multiplier, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0xb4, Result);

  //
  // Otherwise should result in an error status
  //
  Multiplicand = 0x12;
  Multiplier   = 0x23;
  Status       = SafeUint8Mult (Multiplicand, Multiplier, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (MultiplicationTestSuite, TestSafeUint16Mult) {
  RETURN_STATUS  Status;
  UINT16         Multiplicand;
  UINT16         Multiplier;
  UINT16         Result;

  //
  // If the result of multiplication doesn't overflow MAX_UINT16, it will succeed
  //
  Multiplicand = 0x212;
  Multiplier   = 0x7a;
  Result       = 0;
  Status       = SafeUint16Mult (Multiplicand, Multiplier, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0xfc94, Result);

  //
  // Otherwise should result in an error status
  //
  Multiplicand = 0x1234;
  Multiplier   = 0x213;
  Status       = SafeUint16Mult (Multiplicand, Multiplier, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (MultiplicationTestSuite, TestSafeUint32Mult) {
  RETURN_STATUS  Status;
  UINT32         Multiplicand;
  UINT32         Multiplier;
  UINT32         Result;

  //
  // If the result of multiplication doesn't overflow MAX_UINT32, it will succeed
  //
  Multiplicand = 0xa122a;
  Multiplier   = 0xd23;
  Result       = 0;
  Status       = SafeUint32Mult (Multiplicand, Multiplier, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x844c9dbe, Result);

  //
  // Otherwise should result in an error status
  //
  Multiplicand = 0xa122a;
  Multiplier   = 0xed23;
  Status       = SafeUint32Mult (Multiplicand, Multiplier, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (MultiplicationTestSuite, TestSafeUint64Mult) {
  RETURN_STATUS  Status;
  UINT64         Multiplicand;
  UINT64         Multiplier;
  UINT64         Result;

  //
  // If the result of multiplication doesn't overflow MAX_UINT64, it will succeed
  //
  Multiplicand = 0x123456789a;
  Multiplier   = 0x1234567;
  Result       = 0;
  Status       = SafeUint64Mult (Multiplicand, Multiplier, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ ((UINT64)0x14b66db9745a07f6, Result);

  //
  // Otherwise should result in an error status
  //
  Multiplicand = 0x123456789a;
  Multiplier   = 0x12345678;
  Status       = SafeUint64Mult (Multiplicand, Multiplier, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (MultiplicationTestSuite, TestSafeInt8Mult) {
  RETURN_STATUS  Status;
  INT8           Multiplicand;
  INT8           Multiplier;
  INT8           Result;

  //
  // If the result of multiplication doesn't overflow MAX_INT8 and doesn't
  // underflow MIN_UINT8, it will succeed
  //
  Multiplicand = 0x12;
  Multiplier   = 0x7;
  Result       = 0;
  Status       = SafeInt8Mult (Multiplicand, Multiplier, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x7e, Result);

  //
  // Otherwise should result in an error status
  //
  Multiplicand = 0x12;
  Multiplier   = 0xa;
  Status       = SafeInt8Mult (Multiplicand, Multiplier, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (MultiplicationTestSuite, TestSafeInt16Mult) {
  RETURN_STATUS  Status;
  INT16          Multiplicand;
  INT16          Multiplier;
  INT16          Result;

  //
  // If the result of multiplication doesn't overflow MAX_INT16 and doesn't
  // underflow MIN_UINT16, it will succeed
  //
  Multiplicand = 0x123;
  Multiplier   = 0x67;
  Result       = 0;
  Status       = SafeInt16Mult (Multiplicand, Multiplier, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x7515, Result);

  //
  // Otherwise should result in an error status
  //
  Multiplicand = 0x123;
  Multiplier   = 0xab;
  Status       = SafeInt16Mult (Multiplicand, Multiplier, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (MultiplicationTestSuite, TestSafeInt32Mult) {
  RETURN_STATUS  Status;
  INT32          Multiplicand;
  INT32          Multiplier;
  INT32          Result;

  //
  // If the result of multiplication doesn't overflow MAX_INT32 and doesn't
  // underflow MIN_UINT32, it will succeed
  //
  Multiplicand = 0x123456;
  Multiplier   = 0x678;
  Result       = 0;
  Status       = SafeInt32Mult (Multiplicand, Multiplier, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x75c28c50, Result);

  //
  // Otherwise should result in an error status
  //
  Multiplicand = 0x123456;
  Multiplier   = 0xabc;
  Status       = SafeInt32Mult (Multiplicand, Multiplier, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

TEST (MultiplicationTestSuite, TestSafeInt64Mult) {
  RETURN_STATUS  Status;
  INT64          Multiplicand;
  INT64          Multiplier;
  INT64          Result;

  //
  // If the result of multiplication doesn't overflow MAX_INT64 and doesn't
  // underflow MIN_UINT64, it will succeed
  //
  Multiplicand = 0x123456789;
  Multiplier   = 0x6789abcd;
  Result       = 0;
  Status       = SafeInt64Mult (Multiplicand, Multiplier, &Result);
  ASSERT_EQ (Status, RETURN_SUCCESS);
  ASSERT_EQ (0x75cd9045220d6bb5, Result);

  //
  // Otherwise should result in an error status
  //
  Multiplicand = 0x123456789;
  Multiplier   = 0xa789abcd;
  Status       = SafeInt64Mult (Multiplicand, Multiplier, &Result);
  ASSERT_EQ (RETURN_BUFFER_TOO_SMALL, Status);
}

int
main (
  int   argc,
  char  *argv[]
  )
{
  testing::InitGoogleTest (&argc, argv);
  return RUN_ALL_TESTS ();
}
