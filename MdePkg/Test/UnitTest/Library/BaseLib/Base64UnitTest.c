/** @file
  Unit tests of Base64 conversion APIs in BaseLib.

  Copyright (C) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UnitTestLib.h>

#define UNIT_TEST_APP_NAME     "BaseLib Unit Test Application"
#define UNIT_TEST_APP_VERSION  "1.0"

/**
  RFC 4648  https://tools.ietf.org/html/rfc4648 test vectors

  BASE64("") = ""
  BASE64("f") = "Zg=="
  BASE64("fo") = "Zm8="
  BASE64("foo") = "Zm9v"
  BASE64("foob") = "Zm9vYg=="
  BASE64("fooba") = "Zm9vYmE="
  BASE64("foobar") = "Zm9vYmFy"

  The test vectors are using ascii strings for the binary data
 */

typedef struct {
    CHAR8      *TestInput;
    CHAR8      *TestOutput;
    EFI_STATUS  ExpectedStatus;
    VOID       *BufferToFree;
    UINTN       ExpectedSize;
} BASIC_TEST_CONTEXT;

#define B64_TEST_1     ""
#define BIN_TEST_1     ""

#define B64_TEST_2     "Zg=="
#define BIN_TEST_2     "f"

#define B64_TEST_3     "Zm8="
#define BIN_TEST_3     "fo"

#define B64_TEST_4     "Zm9v"
#define BIN_TEST_4     "foo"

#define B64_TEST_5     "Zm9vYg=="
#define BIN_TEST_5     "foob"

#define B64_TEST_6     "Zm9vYmE="
#define BIN_TEST_6     "fooba"

#define B64_TEST_7     "Zm9vYmFy"
#define BIN_TEST_7     "foobar"

// Adds all white space - also ends the last quantum with only spaces afterwards
#define B64_TEST_8_IN   " \t\v  Zm9\r\nvYmFy \f  "
#define BIN_TEST_8      "foobar"

// Not a quantum multiple of 4
#define B64_ERROR_1  "Zm9vymFy="

// Invalid characters in the string
#define B64_ERROR_2  "Zm$vymFy"

// Too many '=' characters
#define B64_ERROR_3 "Z==="

// Poorly placed '='
#define B64_ERROR_4 "Zm=vYmFy"

#define MAX_TEST_STRING_SIZE (200)

// ------------------------------------------------ Input----------Output-----------Result-------Free--Expected Output Size
static BASIC_TEST_CONTEXT    mBasicEncodeTest1  = {BIN_TEST_1,     B64_TEST_1,      EFI_SUCCESS, NULL, sizeof(B64_TEST_1)};
static BASIC_TEST_CONTEXT    mBasicEncodeTest2  = {BIN_TEST_2,     B64_TEST_2,      EFI_SUCCESS, NULL, sizeof(B64_TEST_2)};
static BASIC_TEST_CONTEXT    mBasicEncodeTest3  = {BIN_TEST_3,     B64_TEST_3,      EFI_SUCCESS, NULL, sizeof(B64_TEST_3)};
static BASIC_TEST_CONTEXT    mBasicEncodeTest4  = {BIN_TEST_4,     B64_TEST_4,      EFI_SUCCESS, NULL, sizeof(B64_TEST_4)};
static BASIC_TEST_CONTEXT    mBasicEncodeTest5  = {BIN_TEST_5,     B64_TEST_5,      EFI_SUCCESS, NULL, sizeof(B64_TEST_5)};
static BASIC_TEST_CONTEXT    mBasicEncodeTest6  = {BIN_TEST_6,     B64_TEST_6,      EFI_SUCCESS, NULL, sizeof(B64_TEST_6)};
static BASIC_TEST_CONTEXT    mBasicEncodeTest7  = {BIN_TEST_7,     B64_TEST_7,      EFI_SUCCESS, NULL, sizeof(B64_TEST_7)};
static BASIC_TEST_CONTEXT    mBasicEncodeError1 = {BIN_TEST_7,     B64_TEST_1,      EFI_BUFFER_TOO_SMALL, NULL, sizeof(B64_TEST_7)};

static BASIC_TEST_CONTEXT    mBasicDecodeTest1  = {B64_TEST_1,     BIN_TEST_1,      EFI_SUCCESS, NULL, sizeof(BIN_TEST_1)-1};
static BASIC_TEST_CONTEXT    mBasicDecodeTest2  = {B64_TEST_2,     BIN_TEST_2,      EFI_SUCCESS, NULL, sizeof(BIN_TEST_2)-1};
static BASIC_TEST_CONTEXT    mBasicDecodeTest3  = {B64_TEST_3,     BIN_TEST_3,      EFI_SUCCESS, NULL, sizeof(BIN_TEST_3)-1};
static BASIC_TEST_CONTEXT    mBasicDecodeTest4  = {B64_TEST_4,     BIN_TEST_4,      EFI_SUCCESS, NULL, sizeof(BIN_TEST_4)-1};
static BASIC_TEST_CONTEXT    mBasicDecodeTest5  = {B64_TEST_5,     BIN_TEST_5,      EFI_SUCCESS, NULL, sizeof(BIN_TEST_5)-1};
static BASIC_TEST_CONTEXT    mBasicDecodeTest6  = {B64_TEST_6,     BIN_TEST_6,      EFI_SUCCESS, NULL, sizeof(BIN_TEST_6)-1};
static BASIC_TEST_CONTEXT    mBasicDecodeTest7  = {B64_TEST_7,     BIN_TEST_7,      EFI_SUCCESS, NULL, sizeof(BIN_TEST_7)-1};
static BASIC_TEST_CONTEXT    mBasicDecodeTest8  = {B64_TEST_8_IN,  BIN_TEST_8,      EFI_SUCCESS, NULL, sizeof(BIN_TEST_8)-1};

static BASIC_TEST_CONTEXT    mBasicDecodeError1 = {B64_ERROR_1,    B64_ERROR_1,     EFI_INVALID_PARAMETER, NULL, 0};
static BASIC_TEST_CONTEXT    mBasicDecodeError2 = {B64_ERROR_2,    B64_ERROR_2,     EFI_INVALID_PARAMETER, NULL, 0};
static BASIC_TEST_CONTEXT    mBasicDecodeError3 = {B64_ERROR_3,    B64_ERROR_3,     EFI_INVALID_PARAMETER, NULL, 0};
static BASIC_TEST_CONTEXT    mBasicDecodeError4 = {B64_ERROR_4,    B64_ERROR_4,     EFI_INVALID_PARAMETER, NULL, 0};
static BASIC_TEST_CONTEXT    mBasicDecodeError5 = {B64_TEST_7,     BIN_TEST_1,      EFI_BUFFER_TOO_SMALL,  NULL, sizeof(BIN_TEST_7)-1};

/**
  Simple clean up method to make sure tests clean up even if interrupted and fail
  in the middle.
**/
STATIC
VOID
EFIAPI
CleanUpB64TestContext (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  BASIC_TEST_CONTEXT  *Btc;

  Btc = (BASIC_TEST_CONTEXT *)Context;
  if (Btc != NULL) {
    //free string if set
    if (Btc->BufferToFree != NULL) {
      FreePool (Btc->BufferToFree);
      Btc->BufferToFree = NULL;
    }
  }
}

/**
  Unit test for Base64 encode APIs of BaseLib.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
RfcEncodeTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  BASIC_TEST_CONTEXT  *Btc;
  CHAR8               *b64String;
  CHAR8               *binString;
  UINTN               b64StringSize;
  EFI_STATUS          Status;
  UINT8               *BinData;
  UINTN               BinSize;
  CHAR8               *b64WorkString;
  UINTN               ReturnSize;
  INTN                CompareStatus;
  UINTN               indx;

  Btc = (BASIC_TEST_CONTEXT *) Context;
  binString = Btc->TestInput;
  b64String = Btc->TestOutput;

  //
  // Only testing the the translate functionality, so preallocate the proper
  // string buffer.
  //

  b64StringSize = AsciiStrnSizeS(b64String, MAX_TEST_STRING_SIZE);
  BinSize = AsciiStrnLenS(binString, MAX_TEST_STRING_SIZE);
  BinData = (UINT8 *)  binString;

  b64WorkString = (CHAR8 *) AllocatePool(b64StringSize);
  UT_ASSERT_NOT_NULL(b64WorkString);

  Btc->BufferToFree = b64WorkString;
  ReturnSize = b64StringSize;

  Status = Base64Encode(BinData, BinSize, b64WorkString, &ReturnSize);

  UT_ASSERT_STATUS_EQUAL(Status, Btc->ExpectedStatus);

  UT_ASSERT_EQUAL(ReturnSize, Btc->ExpectedSize);

  if (!EFI_ERROR (Btc->ExpectedStatus)) {
    if (ReturnSize != 0) {
      CompareStatus = AsciiStrnCmp (b64String, b64WorkString, ReturnSize);
      if (CompareStatus != 0) {
        UT_LOG_ERROR ("b64 string compare error - size=%d\n", ReturnSize);
        for (indx = 0; indx < ReturnSize; indx++) {
          UT_LOG_ERROR (" %2.2x", 0xff & b64String[indx]);
        }
        UT_LOG_ERROR ("\n b64 work string:\n");
        for (indx = 0; indx < ReturnSize; indx++) {
          UT_LOG_ERROR (" %2.2x", 0xff & b64WorkString[indx]);
        }
        UT_LOG_ERROR ("\n");
      }
      UT_ASSERT_EQUAL (CompareStatus, 0);
    }
  }

  Btc->BufferToFree = NULL;
  FreePool (b64WorkString);
  return UNIT_TEST_PASSED;
}

/**
  Unit test for Base64 decode APIs of BaseLib.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
STATIC
UNIT_TEST_STATUS
EFIAPI
RfcDecodeTest(
  IN UNIT_TEST_CONTEXT  Context
  )
{
  BASIC_TEST_CONTEXT *Btc;
  CHAR8              *b64String;
  CHAR8              *binString;
  EFI_STATUS          Status;
  UINTN               b64StringLen;
  UINTN               ReturnSize;
  UINT8              *BinData;
  UINTN               BinSize;
  INTN                CompareStatus;
  UINTN               indx;

  Btc = (BASIC_TEST_CONTEXT *)Context;
  b64String = Btc->TestInput;
  binString = Btc->TestOutput;

  //
  //  Only testing the the translate functionality
  //

  b64StringLen = AsciiStrnLenS (b64String, MAX_TEST_STRING_SIZE);
  BinSize = AsciiStrnLenS (binString, MAX_TEST_STRING_SIZE);

  BinData = AllocatePool (BinSize);
  UT_ASSERT_NOT_NULL(BinData);

  Btc->BufferToFree = BinData;
  ReturnSize = BinSize;

  Status = Base64Decode (b64String, b64StringLen, BinData, &ReturnSize);

  UT_ASSERT_STATUS_EQUAL (Status, Btc->ExpectedStatus);

  // If an error is not expected, check the results
  if (EFI_ERROR (Btc->ExpectedStatus)) {
    if (Btc->ExpectedStatus == EFI_BUFFER_TOO_SMALL) {
      UT_ASSERT_EQUAL (ReturnSize, Btc->ExpectedSize);
    }
  } else {
    UT_ASSERT_EQUAL (ReturnSize, Btc->ExpectedSize);
    if (ReturnSize != 0) {
      CompareStatus = CompareMem (binString, BinData, ReturnSize);
      if (CompareStatus != 0) {
        UT_LOG_ERROR ("bin string compare error - size=%d\n", ReturnSize);
        for (indx = 0; indx < ReturnSize; indx++) {
          UT_LOG_ERROR (" %2.2x", 0xff & binString[indx]);
        }
        UT_LOG_ERROR ("\nBinData:\n");
        for (indx = 0; indx < ReturnSize; indx++) {
          UT_LOG_ERROR (" %2.2x", 0xff & BinData[indx]);
        }
        UT_LOG_ERROR ("\n");
      }
      UT_ASSERT_EQUAL (CompareStatus, 0);
    }
  }

  Btc->BufferToFree = NULL;
  FreePool (BinData);
  return UNIT_TEST_PASSED;
}

#define SOURCE_STRING  L"Hello"

STATIC
UNIT_TEST_STATUS
EFIAPI
SafeStringContraintCheckTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  RETURN_STATUS  Status;
  CHAR16         Destination[20];
  CHAR16         AllZero[20];

  //
  // Zero buffer used to verify Destination is not modified
  //
  ZeroMem (AllZero, sizeof (AllZero));

  //
  // Positive test case copy source unicode string to destination
  //
  ZeroMem (Destination, sizeof (Destination));
  Status = StrCpyS (Destination, sizeof (Destination) / sizeof (CHAR16), SOURCE_STRING);
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_MEM_EQUAL (Destination, SOURCE_STRING, sizeof (SOURCE_STRING));

  //
  // Positive test case with DestMax the same as Source size
  //
  ZeroMem (Destination, sizeof (Destination));
  Status = StrCpyS (Destination, sizeof (SOURCE_STRING) / sizeof (CHAR16), SOURCE_STRING);
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_MEM_EQUAL (Destination, SOURCE_STRING, sizeof (SOURCE_STRING));

  //
  // Negative test case with Destination NULL
  //
  ZeroMem (Destination, sizeof (Destination));
  Status = StrCpyS (NULL, sizeof (Destination) / sizeof (CHAR16), SOURCE_STRING);
  UT_ASSERT_STATUS_EQUAL (Status, RETURN_INVALID_PARAMETER);
  UT_ASSERT_MEM_EQUAL (Destination, AllZero, sizeof (AllZero));

  //
  // Negative test case with Source NULL
  //
  ZeroMem (Destination, sizeof (Destination));
  Status = StrCpyS (Destination, sizeof (Destination) / sizeof (CHAR16), NULL);
  UT_ASSERT_STATUS_EQUAL (Status, RETURN_INVALID_PARAMETER);
  UT_ASSERT_MEM_EQUAL (Destination, AllZero, sizeof (AllZero));

  //
  // Negative test case with DestMax too big
  //
  ZeroMem (Destination, sizeof (Destination));
  Status = StrCpyS (Destination, MAX_UINTN, SOURCE_STRING);
  UT_ASSERT_STATUS_EQUAL (Status, RETURN_INVALID_PARAMETER);
  UT_ASSERT_MEM_EQUAL (Destination, AllZero, sizeof (AllZero));

  //
  // Negative test case with DestMax 0
  //
  ZeroMem (Destination, sizeof (Destination));
  Status = StrCpyS (Destination, 0, SOURCE_STRING);
  UT_ASSERT_STATUS_EQUAL (Status, RETURN_INVALID_PARAMETER);
  UT_ASSERT_MEM_EQUAL (Destination, AllZero, sizeof (AllZero));

  //
  // Negative test case with DestMax smaller than Source size
  //
  ZeroMem (Destination, sizeof (Destination));
  Status = StrCpyS (Destination, 1, SOURCE_STRING);
  UT_ASSERT_STATUS_EQUAL (Status, RETURN_BUFFER_TOO_SMALL);
  UT_ASSERT_MEM_EQUAL (Destination, AllZero, sizeof (AllZero));

  //
  // Negative test case with DestMax smaller than Source size by one character
  //
  ZeroMem (Destination, sizeof (Destination));
  Status = StrCpyS (Destination, sizeof (SOURCE_STRING) / sizeof (CHAR16) - 1, SOURCE_STRING);
  UT_ASSERT_STATUS_EQUAL (Status, RETURN_BUFFER_TOO_SMALL);
  UT_ASSERT_MEM_EQUAL (Destination, AllZero, sizeof (AllZero));

  //
  // Negative test case with overlapping Destination and Source
  //
  ZeroMem (Destination, sizeof (Destination));
  Status = StrCpyS (Destination, sizeof (Destination) / sizeof (CHAR16), Destination);
  UT_ASSERT_STATUS_EQUAL (Status, RETURN_ACCESS_DENIED);
  UT_ASSERT_MEM_EQUAL (Destination, AllZero, sizeof (AllZero));

  return UNIT_TEST_PASSED;
}

/**
  Initialze the unit test framework, suite, and unit tests for the
  Base64 conversion APIs of BaseLib and run the unit tests.

  @retval  EFI_SUCCESS           All test cases were dispatched.
  @retval  EFI_OUT_OF_RESOURCES  There are not enough resources available to
                                 initialize the unit tests.
**/
STATIC
EFI_STATUS
EFIAPI
UnitTestingEntry (
  VOID
  )
{
  EFI_STATUS                  Status;
  UNIT_TEST_FRAMEWORK_HANDLE  Fw;
  UNIT_TEST_SUITE_HANDLE      b64EncodeTests;
  UNIT_TEST_SUITE_HANDLE      b64DecodeTests;
  UNIT_TEST_SUITE_HANDLE      SafeStringTests;

  Fw = NULL;

  DEBUG ((DEBUG_INFO, "%a v%a\n", UNIT_TEST_APP_NAME, UNIT_TEST_APP_VERSION));

  //
  // Start setting up the test framework for running the tests.
  //
  Status = InitUnitTestFramework (&Fw, UNIT_TEST_APP_NAME, gEfiCallerBaseName, UNIT_TEST_APP_VERSION);
  if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed in InitUnitTestFramework. Status = %r\n", Status));
      goto EXIT;
  }

  //
  // Populate the B64 Encode Unit Test Suite.
  //
  Status = CreateUnitTestSuite (&b64EncodeTests, Fw, "b64 Encode binary to Ascii string", "BaseLib.b64Encode", NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed in CreateUnitTestSuite for b64EncodeTests\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  // --------------Suite-----------Description--------------Class Name----------Function--------Pre---Post-------------------Context-----------
  AddTestCase (b64EncodeTests, "RFC 4686 Test Vector - Empty", "Test1", RfcEncodeTest, NULL, CleanUpB64TestContext, &mBasicEncodeTest1);
  AddTestCase (b64EncodeTests, "RFC 4686 Test Vector - f", "Test2", RfcEncodeTest, NULL, CleanUpB64TestContext, &mBasicEncodeTest2);
  AddTestCase (b64EncodeTests, "RFC 4686 Test Vector - fo", "Test3", RfcEncodeTest, NULL, CleanUpB64TestContext, &mBasicEncodeTest3);
  AddTestCase (b64EncodeTests, "RFC 4686 Test Vector - foo", "Test4", RfcEncodeTest, NULL, CleanUpB64TestContext, &mBasicEncodeTest4);
  AddTestCase (b64EncodeTests, "RFC 4686 Test Vector - foob", "Test5", RfcEncodeTest, NULL, CleanUpB64TestContext, &mBasicEncodeTest5);
  AddTestCase (b64EncodeTests, "RFC 4686 Test Vector - fooba", "Test6", RfcEncodeTest, NULL, CleanUpB64TestContext, &mBasicEncodeTest6);
  AddTestCase (b64EncodeTests, "RFC 4686 Test Vector - foobar", "Test7", RfcEncodeTest, NULL, CleanUpB64TestContext, &mBasicEncodeTest7);
  AddTestCase (b64EncodeTests, "Too small of output buffer", "Error1", RfcEncodeTest, NULL, CleanUpB64TestContext, &mBasicEncodeError1);
  //
  // Populate the B64 Decode Unit Test Suite.
  //
  Status = CreateUnitTestSuite (&b64DecodeTests, Fw, "b64 Decode Ascii string to binary", "BaseLib.b64Decode", NULL, NULL);
  if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed in CreateUnitTestSuite for b64Decode Tests\n"));
      Status = EFI_OUT_OF_RESOURCES;
      goto EXIT;
  }

  AddTestCase (b64DecodeTests, "RFC 4686 Test Vector - Empty", "Test1",  RfcDecodeTest, NULL, CleanUpB64TestContext, &mBasicDecodeTest1);
  AddTestCase (b64DecodeTests, "RFC 4686 Test Vector - f", "Test2",  RfcDecodeTest, NULL, CleanUpB64TestContext, &mBasicDecodeTest2);
  AddTestCase (b64DecodeTests, "RFC 4686 Test Vector - fo", "Test3",  RfcDecodeTest, NULL, CleanUpB64TestContext, &mBasicDecodeTest3);
  AddTestCase (b64DecodeTests, "RFC 4686 Test Vector - foo", "Test4",  RfcDecodeTest, NULL, CleanUpB64TestContext, &mBasicDecodeTest4);
  AddTestCase (b64DecodeTests, "RFC 4686 Test Vector - foob", "Test5",  RfcDecodeTest, NULL, CleanUpB64TestContext, &mBasicDecodeTest5);
  AddTestCase (b64DecodeTests, "RFC 4686 Test Vector - fooba", "Test6",  RfcDecodeTest, NULL, CleanUpB64TestContext, &mBasicDecodeTest6);
  AddTestCase (b64DecodeTests, "RFC 4686 Test Vector - foobar", "Test7",  RfcDecodeTest, NULL, CleanUpB64TestContext, &mBasicDecodeTest7);
  AddTestCase (b64DecodeTests, "Ignore Whitespace test", "Test8",  RfcDecodeTest, NULL, CleanUpB64TestContext, &mBasicDecodeTest8);

  AddTestCase (b64DecodeTests, "Not a quantum multiple of 4", "Error1", RfcDecodeTest, NULL, CleanUpB64TestContext, &mBasicDecodeError1);
  AddTestCase (b64DecodeTests, "Invalid characters in the string", "Error2", RfcDecodeTest, NULL, CleanUpB64TestContext, &mBasicDecodeError2);
  AddTestCase (b64DecodeTests, "Too many padding characters", "Error3", RfcDecodeTest, NULL, CleanUpB64TestContext, &mBasicDecodeError3);
  AddTestCase (b64DecodeTests, "Incorrectly placed padding character", "Error4", RfcDecodeTest, NULL, CleanUpB64TestContext, &mBasicDecodeError4);
  AddTestCase (b64DecodeTests, "Too small of output buffer", "Error5", RfcDecodeTest, NULL, CleanUpB64TestContext, &mBasicDecodeError5);

  //
  // Populate the safe string Unit Test Suite.
  //
  Status = CreateUnitTestSuite (&SafeStringTests, Fw, "Safe String", "BaseLib.SafeString", NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed in CreateUnitTestSuite for SafeStringTests\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  // --------------Suite-----------Description--------------Class Name----------Function--------Pre---Post-------------------Context-----------
  AddTestCase (SafeStringTests, "SAFE_STRING_CONSTRAINT_CHECK", "SafeStringContraintCheckTest", SafeStringContraintCheckTest, NULL, NULL, NULL);

  //
  // Execute the tests.
  //
  Status = RunAllTestSuites (Fw);

EXIT:
  if (Fw) {
    FreeUnitTestFramework (Fw);
  }

  return Status;
}

/**
  Standard UEFI entry point for target based unit test execution from UEFI Shell.
**/
EFI_STATUS
EFIAPI
BaseLibUnitTestAppEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  return UnitTestingEntry ();
}

/**
  Standard POSIX C entry point for host based unit test execution.
**/
int
main (
  int argc,
  char *argv[]
  )
{
  return UnitTestingEntry ();
}
