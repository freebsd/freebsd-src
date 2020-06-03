/** @file
  Provides a unit test framework.  This allows tests to focus on testing logic
  and the framework to focus on runnings, reporting, statistics, etc.

  Copyright (c) Microsoft Corporation.<BR>
  Copyright (c) 2020, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef __UNIT_TEST_LIB_H__
#define __UNIT_TEST_LIB_H__

///
/// Unit Test Status
///
typedef UINT32  UNIT_TEST_STATUS;
#define UNIT_TEST_PASSED                      (0)
#define UNIT_TEST_ERROR_PREREQUISITE_NOT_MET  (1)
#define UNIT_TEST_ERROR_TEST_FAILED           (2)
#define UNIT_TEST_ERROR_CLEANUP_FAILED        (3)
#define UNIT_TEST_SKIPPED                     (0xFFFFFFFD)
#define UNIT_TEST_RUNNING                     (0xFFFFFFFE)
#define UNIT_TEST_PENDING                     (0xFFFFFFFF)

///
/// Declare PcdUnitTestLogLevel bits and UnitTestLog() ErrorLevel parameter.
///
#define UNIT_TEST_LOG_LEVEL_ERROR    BIT0
#define UNIT_TEST_LOG_LEVEL_WARN     BIT1
#define UNIT_TEST_LOG_LEVEL_INFO     BIT2
#define UNIT_TEST_LOG_LEVEL_VERBOSE  BIT3

///
/// Unit Test Framework Handle
///
struct UNIT_TEST_FRAMEWORK_OBJECT;
typedef struct UNIT_TEST_FRAMEWORK_OBJECT  *UNIT_TEST_FRAMEWORK_HANDLE;

///
/// Unit Test Suite Handle
///
struct UNIT_TEST_SUITE_OBJECT;
typedef struct UNIT_TEST_SUITE_OBJECT  *UNIT_TEST_SUITE_HANDLE;

///
/// Unit Test Handle
///
struct UNIT_TEST_OBJECT;
typedef struct UNIT_TEST_OBJECT  *UNIT_TEST_HANDLE;

///
/// Unit Test Context
///
typedef VOID*  UNIT_TEST_CONTEXT;

/**
  The prototype for a single UnitTest case function.

  Functions with this prototype are registered to be dispatched by the
  UnitTest framework, and results are recorded as test Pass or Fail.

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
typedef
UNIT_TEST_STATUS
(EFIAPI *UNIT_TEST_FUNCTION)(
  IN UNIT_TEST_CONTEXT  Context
  );

/**
  Unit-Test Prerequisite Function pointer type.

  Functions with this prototype are registered to be dispatched by the unit test
  framework prior to a given test case. If this prereq function returns
  UNIT_TEST_ERROR_PREREQUISITE_NOT_MET, the test case will be skipped.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED                      Unit test case prerequisites
                                                 are met.
  @retval  UNIT_TEST_ERROR_PREREQUISITE_NOT_MET  Test case should be skipped.

**/
typedef
UNIT_TEST_STATUS
(EFIAPI *UNIT_TEST_PREREQUISITE)(
  IN UNIT_TEST_CONTEXT  Context
  );

/**
  Unit-Test Cleanup (after) function pointer type.

  Functions with this prototype are registered to be dispatched by the
  unit test framework after a given test case. This will be called even if the
  test case returns an error, but not if the prerequisite fails and the test is
  skipped.  The purpose of this function is to clean up any global state or
  test data.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED                Test case cleanup succeeded.
  @retval  UNIT_TEST_ERROR_CLEANUP_FAILED  Test case cleanup failed.

**/
typedef
VOID
(EFIAPI *UNIT_TEST_CLEANUP)(
  IN UNIT_TEST_CONTEXT  Context
  );

/**
  Unit-Test Test Suite Setup (before) function pointer type. Functions with this
  prototype are registered to be dispatched by the UnitTest framework prior to
  running any of the test cases in a test suite.  It will only be run once at
  the beginning of the suite (not prior to each case).

  The purpose of this function is to set up any global state or test data.
**/
typedef
VOID
(EFIAPI *UNIT_TEST_SUITE_SETUP)(
  VOID
  );

/**
  Unit-Test Test Suite Teardown (after) function pointer type.  Functions with
  this prototype are registered to be dispatched by the UnitTest framework after
  running all of the test cases in a test suite.  It will only be run once at
  the end of the suite.

  The purpose of this function is to clean up any global state or test data.
**/
typedef
VOID
(EFIAPI *UNIT_TEST_SUITE_TEARDOWN)(
  VOID
  );

/**
  Method to Initialize the Unit Test framework.  This function registers the
  test name and also initializes the internal state of the test framework to
  receive any new suites and tests.

  @param[out]  FrameworkHandle  Unit test framework to be created.
  @param[in]   Title            Null-terminated ASCII string that is the user
                                friendly name of the framework. String is
                                copied.
  @param[in]   ShortTitle       Null-terminated ASCII short string that is the
                                short name of the framework with no spaces.
                                String is copied.
  @param[in]   VersionString    Null-terminated ASCII version string for the
                                framework. String is copied.

  @retval  EFI_SUCCESS            The unit test framework was initialized.
  @retval  EFI_INVALID_PARAMETER  FrameworkHandle is NULL.
  @retval  EFI_INVALID_PARAMETER  Title is NULL.
  @retval  EFI_INVALID_PARAMETER  ShortTitle is NULL.
  @retval  EFI_INVALID_PARAMETER  VersionString is NULL.
  @retval  EFI_INVALID_PARAMETER  ShortTitle is invalid.
  @retval  EFI_OUT_OF_RESOURCES   There are not enough resources available to
                                  initialize the unit test framework.
**/
EFI_STATUS
EFIAPI
InitUnitTestFramework (
  OUT UNIT_TEST_FRAMEWORK_HANDLE  *FrameworkHandle,
  IN  CHAR8                       *Title,
  IN  CHAR8                       *ShortTitle,
  IN  CHAR8                       *VersionString
  );

/**
  Registers a Unit Test Suite in the Unit Test Framework.
  At least one test suite must be registered, because all test cases must be
  within a unit test suite.

  @param[out]  SuiteHandle      Unit test suite to create
  @param[in]   FrameworkHandle  Unit test framework to add unit test suite to
  @param[in]   Title            Null-terminated ASCII string that is the user
                                friendly name of the test suite.  String is
                                copied.
  @param[in]   Name             Null-terminated ASCII string that is the short
                                name of the test suite with no spaces.  String
                                is copied.
  @param[in]   Setup            Setup function, runs before suite.  This is an
                                optional parameter that may be NULL.
  @param[in]   Teardown         Teardown function, runs after suite.  This is an
                                optional parameter that may be NULL.

  @retval  EFI_SUCCESS            The unit test suite was created.
  @retval  EFI_INVALID_PARAMETER  SuiteHandle is NULL.
  @retval  EFI_INVALID_PARAMETER  FrameworkHandle is NULL.
  @retval  EFI_INVALID_PARAMETER  Title is NULL.
  @retval  EFI_INVALID_PARAMETER  Name is NULL.
  @retval  EFI_OUT_OF_RESOURCES   There are not enough resources available to
                                  initialize the unit test suite.
**/
EFI_STATUS
EFIAPI
CreateUnitTestSuite (
  OUT UNIT_TEST_SUITE_HANDLE      *SuiteHandle,
  IN  UNIT_TEST_FRAMEWORK_HANDLE  FrameworkHandle,
  IN  CHAR8                       *Title,
  IN  CHAR8                       *Name,
  IN  UNIT_TEST_SUITE_SETUP       Setup     OPTIONAL,
  IN  UNIT_TEST_SUITE_TEARDOWN    Teardown  OPTIONAL
  );

/**
  Adds test case to Suite

  @param[in]  SuiteHandle   Unit test suite to add test to.
  @param[in]  Description   Null-terminated ASCII string that is the user
                            friendly description of a test.  String is copied.
  @param[in]  Name          Null-terminated ASCII string that is the short name
                            of the test with no spaces.  String is copied.
  @param[in]  Function      Unit test function.
  @param[in]  Prerequisite  Prerequisite function, runs before test.  This is
                            an optional parameter that may be NULL.
  @param[in]  CleanUp       Clean up function, runs after test.  This is an
                            optional parameter that may be NULL.
  @param[in]  Context       Pointer to context.    This is an optional parameter
                            that may be NULL.

  @retval  EFI_SUCCESS            The unit test case was added to Suite.
  @retval  EFI_INVALID_PARAMETER  SuiteHandle is NULL.
  @retval  EFI_INVALID_PARAMETER  Description is NULL.
  @retval  EFI_INVALID_PARAMETER  Name is NULL.
  @retval  EFI_INVALID_PARAMETER  Function is NULL.
  @retval  EFI_OUT_OF_RESOURCES   There are not enough resources available to
                                  add the unit test case to Suite.
**/
EFI_STATUS
EFIAPI
AddTestCase (
  IN UNIT_TEST_SUITE_HANDLE  SuiteHandle,
  IN CHAR8                   *Description,
  IN CHAR8                   *Name,
  IN UNIT_TEST_FUNCTION      Function,
  IN UNIT_TEST_PREREQUISITE  Prerequisite  OPTIONAL,
  IN UNIT_TEST_CLEANUP       CleanUp       OPTIONAL,
  IN UNIT_TEST_CONTEXT       Context       OPTIONAL
  );

/**
  Execute all unit test cases in all unit test suites added to a Framework.

  Once a unit test framework is initialized and all unit test suites and unit
  test cases are registered, this function will cause the unit test framework to
  dispatch all unit test cases in sequence and record the results for reporting.

  @param[in]  FrameworkHandle  A handle to the current running framework that
                               dispatched the test.  Necessary for recording
                               certain test events with the framework.

  @retval  EFI_SUCCESS            All test cases were dispatched.
  @retval  EFI_INVALID_PARAMETER  FrameworkHandle is NULL.
**/
EFI_STATUS
EFIAPI
RunAllTestSuites (
  IN UNIT_TEST_FRAMEWORK_HANDLE  FrameworkHandle
  );

/**
  Cleanup a test framework.

  After tests are run, this will teardown the entire framework and free all
  allocated data within.

  @param[in]  FrameworkHandle  A handle to the current running framework that
                               dispatched the test.  Necessary for recording
                               certain test events with the framework.

  @retval  EFI_SUCCESS            All resources associated with framework were
                                  freed.
  @retval  EFI_INVALID_PARAMETER  FrameworkHandle is NULL.
**/
EFI_STATUS
EFIAPI
FreeUnitTestFramework (
  IN UNIT_TEST_FRAMEWORK_HANDLE  FrameworkHandle
  );

/**
  Leverages a framework-specific mechanism (see UnitTestPersistenceLib if you're
  a framework author) to save the state of the executing framework along with
  any allocated data so that the test may be resumed upon reentry. A test case
  should pass any needed context (which, to prevent an infinite loop, should be
  at least the current execution count) which will be saved by the framework and
  passed to the test case upon resume.

  Generally called from within a test case prior to quitting or rebooting.

  @param[in]  FrameworkHandle    A handle to the current running framework that
                                 dispatched the test.  Necessary for recording
                                 certain test events with the framework.
  @param[in]  ContextToSave      A buffer of test case-specific data to be saved
                                 along with framework state.  Will be passed as
                                 "Context" to the test case upon resume.  This
                                 is an optional parameter that may be NULL.
  @param[in]  ContextToSaveSize  Size of the ContextToSave buffer.

  @retval  EFI_SUCCESS            The framework state and context were saved.
  @retval  EFI_INVALID_PARAMETER  FrameworkHandle is NULL.
  @retval  EFI_INVALID_PARAMETER  ContextToSave is not NULL and
                                  ContextToSaveSize is 0.
  @retval  EFI_INVALID_PARAMETER  ContextToSave is >= 4GB.
  @retval  EFI_OUT_OF_RESOURCES   There are not enough resources available to
                                  save the framework and context state.
  @retval  EFI_DEVICE_ERROR       The framework and context state could not be
                                  saved to a persistent storage device due to a
                                  device error.
**/
EFI_STATUS
EFIAPI
SaveFrameworkState (
  IN UNIT_TEST_FRAMEWORK_HANDLE  FrameworkHandle,
  IN UNIT_TEST_CONTEXT           ContextToSave     OPTIONAL,
  IN UINTN                       ContextToSaveSize
  );

/**
  This macro uses the framework assertion logic to check an expression for
  "TRUE". If the expression evaluates to TRUE, execution continues.
  Otherwise, the test case immediately returns UNIT_TEST_ERROR_TEST_FAILED.

  @param[in]  Expression  Expression to be evaluated for TRUE.
**/
#define UT_ASSERT_TRUE(Expression)                                                        \
  if(!UnitTestAssertTrue ((Expression), __FUNCTION__, __LINE__, __FILE__, #Expression)) { \
    return UNIT_TEST_ERROR_TEST_FAILED;                                                   \
  }

/**
  This macro uses the framework assertion logic to check an expression for
  "FALSE". If the expression evaluates to FALSE, execution continues.
  Otherwise, the test case immediately returns UNIT_TEST_ERROR_TEST_FAILED.

  @param[in]  Expression  Expression to be evaluated for FALSE.
**/
#define UT_ASSERT_FALSE(Expression)                                                        \
  if(!UnitTestAssertFalse ((Expression), __FUNCTION__, __LINE__, __FILE__, #Expression)) { \
    return UNIT_TEST_ERROR_TEST_FAILED;                                                    \
  }

/**
  This macro uses the framework assertion logic to check whether two simple
  values are equal.  If the values are equal, execution continues.
  Otherwise, the test case immediately returns UNIT_TEST_ERROR_TEST_FAILED.

  @param[in]  ValueA  Value to be compared for equality (64-bit comparison).
  @param[in]  ValueB  Value to be compared for equality (64-bit comparison).
**/
#define UT_ASSERT_EQUAL(ValueA, ValueB)                                                                           \
  if(!UnitTestAssertEqual ((UINT64)(ValueA), (UINT64)(ValueB), __FUNCTION__, __LINE__, __FILE__, #ValueA, #ValueB)) { \
    return UNIT_TEST_ERROR_TEST_FAILED;                                                                           \
  }

/**
  This macro uses the framework assertion logic to check whether two memory
  buffers are equal.  If the buffers are equal, execution continues.
  Otherwise, the test case immediately returns UNIT_TEST_ERROR_TEST_FAILED.

  @param[in]  BufferA  Pointer to a buffer for comparison.
  @param[in]  BufferB  Pointer to a buffer for comparison.
  @param[in]  Length   Number of bytes to compare in BufferA and BufferB.
**/
#define UT_ASSERT_MEM_EQUAL(BufferA, BufferB, Length)                                                                               \
  if(!UnitTestAssertMemEqual ((VOID *)(UINTN)(BufferA), (VOID *)(UINTN)(BufferB), (UINTN)Length, __FUNCTION__, __LINE__, __FILE__, #BufferA, #BufferB)) { \
    return UNIT_TEST_ERROR_TEST_FAILED;                                                                                           \
  }

/**
  This macro uses the framework assertion logic to check whether two simple
  values are non-equal.  If the values are non-equal, execution continues.
  Otherwise, the test case immediately returns UNIT_TEST_ERROR_TEST_FAILED.

  @param[in]  ValueA  Value to be compared for inequality (64-bit comparison).
  @param[in]  ValueB  Value to be compared for inequality (64-bit comparison).
**/
#define UT_ASSERT_NOT_EQUAL(ValueA, ValueB)                                                                          \
  if(!UnitTestAssertNotEqual ((UINT64)(ValueA), (UINT64)(ValueB), __FUNCTION__, __LINE__, __FILE__, #ValueA, #ValueB)) { \
    return UNIT_TEST_ERROR_TEST_FAILED;                                                                              \
  }

/**
  This macro uses the framework assertion logic to check whether an EFI_STATUS
  value is !EFI_ERROR().  If the status is !EFI_ERROR(), execution continues.
  Otherwise, the test case immediately returns UNIT_TEST_ERROR_TEST_FAILED.

  @param[in]  Status  EFI_STATUS value to check.
**/
#define UT_ASSERT_NOT_EFI_ERROR(Status)                                                \
  if(!UnitTestAssertNotEfiError ((Status), __FUNCTION__, __LINE__, __FILE__, #Status)) { \
    return UNIT_TEST_ERROR_TEST_FAILED;                                                \
  }

/**
  This macro uses the framework assertion logic to check whether two EFI_STATUS
  values are equal.  If the values are equal, execution continues.
  Otherwise, the test case immediately returns UNIT_TEST_ERROR_TEST_FAILED.

  @param[in]  Status    EFI_STATUS values to compare for equality.
  @param[in]  Expected  EFI_STATUS values to compare for equality.
**/
#define UT_ASSERT_STATUS_EQUAL(Status, Expected)                                                 \
  if(!UnitTestAssertStatusEqual ((Status), (Expected), __FUNCTION__, __LINE__, __FILE__, #Status)) { \
    return UNIT_TEST_ERROR_TEST_FAILED;                                                          \
  }

/**
  This macro uses the framework assertion logic to check whether a pointer is
  not NULL.  If the pointer is not NULL, execution continues. Otherwise, the
  test case immediately returns UNIT_TEST_ERROR_TEST_FAILED.

  @param[in]  Pointer  Pointer to be checked against NULL.
**/
#define UT_ASSERT_NOT_NULL(Pointer)                                                  \
  if(!UnitTestAssertNotNull ((Pointer), __FUNCTION__, __LINE__, __FILE__, #Pointer)) { \
    return UNIT_TEST_ERROR_TEST_FAILED;                                              \
  }

/**
  If Expression is TRUE, then TRUE is returned.
  If Expression is FALSE, then an assert is triggered and the location of the
  assert provided by FunctionName, LineNumber, FileName, and Description are
  recorded and FALSE is returned.

  @param[in]  Expression    The BOOLEAN result of the expression evaluation.
  @param[in]  FunctionName  Null-terminated ASCII string of the function
                            executing the assert macro.
  @param[in]  LineNumber    The source file line number of the assert macro.
  @param[in]  FileName      Null-terminated ASCII string of the filename
                            executing the assert macro.
  @param[in]  Description   Null-terminated ASCII string of the expression being
                            evaluated.

  @retval  TRUE   Expression is TRUE.
  @retval  FALSE  Expression is FALSE.
**/
BOOLEAN
EFIAPI
UnitTestAssertTrue (
  IN BOOLEAN      Expression,
  IN CONST CHAR8  *FunctionName,
  IN UINTN        LineNumber,
  IN CONST CHAR8  *FileName,
  IN CONST CHAR8  *Description
  );

/**
  If Expression is FALSE, then TRUE is returned.
  If Expression is TRUE, then an assert is triggered and the location of the
  assert provided by FunctionName, LineNumber, FileName, and Description are
  recorded and FALSE is returned.

  @param[in]  Expression    The BOOLEAN result of the expression evaluation.
  @param[in]  FunctionName  Null-terminated ASCII string of the function
                            executing the assert macro.
  @param[in]  LineNumber    The source file line number of the assert macro.
  @param[in]  FileName      Null-terminated ASCII string of the filename
                            executing the assert macro.
  @param[in]  Description   Null-terminated ASCII string of the expression being
                            evaluated.

  @retval  TRUE   Expression is FALSE.
  @retval  FALSE  Expression is TRUE.
**/
BOOLEAN
EFIAPI
UnitTestAssertFalse (
  IN BOOLEAN      Expression,
  IN CONST CHAR8  *FunctionName,
  IN UINTN        LineNumber,
  IN CONST CHAR8  *FileName,
  IN CONST CHAR8  *Description
  );

/**
  If Status is not an EFI_ERROR(), then TRUE is returned.
  If Status is an EFI_ERROR(), then an assert is triggered and the location of
  the assert provided by FunctionName, LineNumber, FileName, and Description are
  recorded and FALSE is returned.

  @param[in]  Status        The EFI_STATUS value to evaluate.
  @param[in]  FunctionName  Null-terminated ASCII string of the function
                            executing the assert macro.
  @param[in]  LineNumber    The source file line number of the assert macro.
  @param[in]  FileName      Null-terminated ASCII string of the filename
                            executing the assert macro.
  @param[in]  Description   Null-terminated ASCII string of the status
                            expression being evaluated.

  @retval  TRUE   Status is not an EFI_ERROR().
  @retval  FALSE  Status is an EFI_ERROR().
**/
BOOLEAN
EFIAPI
UnitTestAssertNotEfiError (
  IN EFI_STATUS   Status,
  IN CONST CHAR8  *FunctionName,
  IN UINTN        LineNumber,
  IN CONST CHAR8  *FileName,
  IN CONST CHAR8  *Description
  );

/**
  If ValueA is equal ValueB, then TRUE is returned.
  If ValueA is not equal to ValueB, then an assert is triggered and the location
  of the assert provided by FunctionName, LineNumber, FileName, DescriptionA,
  and DescriptionB are recorded and FALSE is returned.

  @param[in]  ValueA        64-bit value.
  @param[in]  ValueB        64-bit value.
  @param[in]  FunctionName  Null-terminated ASCII string of the function
                            executing the assert macro.
  @param[in]  LineNumber    The source file line number of the assert macro.
  @param[in]  FileName      Null-terminated ASCII string of the filename
                            executing the assert macro.
  @param[in]  DescriptionA  Null-terminated ASCII string that is a description
                            of ValueA.
  @param[in]  DescriptionB  Null-terminated ASCII string that is a description
                            of ValueB.

  @retval  TRUE   ValueA is equal to ValueB.
  @retval  FALSE  ValueA is not equal to ValueB.
**/
BOOLEAN
EFIAPI
UnitTestAssertEqual (
  IN UINT64       ValueA,
  IN UINT64       ValueB,
  IN CONST CHAR8  *FunctionName,
  IN UINTN        LineNumber,
  IN CONST CHAR8  *FileName,
  IN CONST CHAR8  *DescriptionA,
  IN CONST CHAR8  *DescriptionB
  );

/**
  If the contents of BufferA are identical to the contents of BufferB, then TRUE
  is returned.  If the contents of BufferA are not identical to the contents of
  BufferB, then an assert is triggered and the location of the assert provided
  by FunctionName, LineNumber, FileName, DescriptionA, and DescriptionB are
  recorded and FALSE is returned.

  @param[in]  BufferA       Pointer to a buffer for comparison.
  @param[in]  BufferB       Pointer to a buffer for comparison.
  @param[in]  Length        Number of bytes to compare in BufferA and BufferB.
  @param[in]  FunctionName  Null-terminated ASCII string of the function
                            executing the assert macro.
  @param[in]  LineNumber    The source file line number of the assert macro.
  @param[in]  FileName      Null-terminated ASCII string of the filename
                            executing the assert macro.
  @param[in]  DescriptionA  Null-terminated ASCII string that is a description
                            of BufferA.
  @param[in]  DescriptionB  Null-terminated ASCII string that is a description
                            of BufferB.

  @retval  TRUE   The contents of BufferA are identical to the contents of
                  BufferB.
  @retval  FALSE  The contents of BufferA are not identical to the contents of
                  BufferB.
**/
BOOLEAN
EFIAPI
UnitTestAssertMemEqual (
  IN VOID         *BufferA,
  IN VOID         *BufferB,
  IN UINTN        Length,
  IN CONST CHAR8  *FunctionName,
  IN UINTN        LineNumber,
  IN CONST CHAR8  *FileName,
  IN CONST CHAR8  *DescriptionA,
  IN CONST CHAR8  *DescriptionB
  );

/**
  If ValueA is not equal ValueB, then TRUE is returned.
  If ValueA is equal to ValueB, then an assert is triggered and the location
  of the assert provided by FunctionName, LineNumber, FileName, DescriptionA
  and DescriptionB are recorded and FALSE is returned.

  @param[in]  ValueA        64-bit value.
  @param[in]  ValueB        64-bit value.
  @param[in]  FunctionName  Null-terminated ASCII string of the function
                            executing the assert macro.
  @param[in]  LineNumber    The source file line number of the assert macro.
  @param[in]  FileName      Null-terminated ASCII string of the filename
                            executing the assert macro.
  @param[in]  DescriptionA  Null-terminated ASCII string that is a description
                            of ValueA.
  @param[in]  DescriptionB  Null-terminated ASCII string that is a description
                            of ValueB.

  @retval  TRUE   ValueA is not equal to ValueB.
  @retval  FALSE  ValueA is equal to ValueB.
**/
BOOLEAN
EFIAPI
UnitTestAssertNotEqual (
  IN UINT64       ValueA,
  IN UINT64       ValueB,
  IN CONST CHAR8  *FunctionName,
  IN UINTN        LineNumber,
  IN CONST CHAR8  *FileName,
  IN CONST CHAR8  *DescriptionA,
  IN CONST CHAR8  *DescriptionB
  );

/**
  If Status is equal to Expected, then TRUE is returned.
  If Status is not equal to Expected, then an assert is triggered and the
  location of the assert provided by FunctionName, LineNumber, FileName, and
  Description are recorded and FALSE is returned.

  @param[in]  Status        EFI_STATUS value returned from an API under test.
  @param[in]  Expected      The expected EFI_STATUS return value from an API
                            under test.
  @param[in]  FunctionName  Null-terminated ASCII string of the function
                            executing the assert macro.
  @param[in]  LineNumber    The source file line number of the assert macro.
  @param[in]  FileName      Null-terminated ASCII string of the filename
                            executing the assert macro.
  @param[in]  Description   Null-terminated ASCII string that is a description
                            of Status.

  @retval  TRUE   Status is equal to Expected.
  @retval  FALSE  Status is not equal to Expected.
**/
BOOLEAN
EFIAPI
UnitTestAssertStatusEqual (
  IN EFI_STATUS   Status,
  IN EFI_STATUS   Expected,
  IN CONST CHAR8  *FunctionName,
  IN UINTN        LineNumber,
  IN CONST CHAR8  *FileName,
  IN CONST CHAR8  *Description
  );

/**
  If Pointer is not equal to NULL, then TRUE is returned.
  If Pointer is equal to NULL, then an assert is triggered and the location of
  the assert provided by FunctionName, LineNumber, FileName, and PointerName
  are recorded and FALSE is returned.

  @param[in]  Pointer       Pointer value to be checked against NULL.
  @param[in]  Expected      The expected EFI_STATUS return value from a function
                            under test.
  @param[in]  FunctionName  Null-terminated ASCII string of the function
                            executing the assert macro.
  @param[in]  LineNumber    The source file line number of the assert macro.
  @param[in]  FileName      Null-terminated ASCII string of the filename
                            executing the assert macro.
  @param[in]  PointerName   Null-terminated ASCII string that is a description
                            of Pointer.

  @retval  TRUE   Pointer is not equal to NULL.
  @retval  FALSE  Pointer is equal to NULL.
**/
BOOLEAN
EFIAPI
UnitTestAssertNotNull (
  IN VOID         *Pointer,
  IN CONST CHAR8  *FunctionName,
  IN UINTN        LineNumber,
  IN CONST CHAR8  *FileName,
  IN CONST CHAR8  *PointerName
  );

/**
  Test logging macro that records an ERROR message in the test framework log.
  Record is associated with the currently executing test case.

  @param[in]  Format  Formatting string following the format defined in
                      MdePkg/Include/Library/PrintLib.h.
  @param[in]  ...     Print args.
**/
#define UT_LOG_ERROR(Format, ...)  \
  UnitTestLog (UNIT_TEST_LOG_LEVEL_ERROR, Format, ##__VA_ARGS__)

/**
  Test logging macro that records a WARNING message in the test framework log.
  Record is associated with the currently executing test case.

  @param[in]  Format  Formatting string following the format defined in
                      MdePkg/Include/Library/PrintLib.h.
  @param[in]  ...     Print args.
**/
#define UT_LOG_WARNING(Format, ...)  \
  UnitTestLog (UNIT_TEST_LOG_LEVEL_WARN, Format, ##__VA_ARGS__)

/**
  Test logging macro that records an INFO message in the test framework log.
  Record is associated with the currently executing test case.

  @param[in]  Format  Formatting string following the format defined in
                      MdePkg/Include/Library/PrintLib.h.
  @param[in]  ...     Print args.
**/
#define UT_LOG_INFO(Format, ...)  \
  UnitTestLog (UNIT_TEST_LOG_LEVEL_INFO, Format, ##__VA_ARGS__)

/**
  Test logging macro that records a VERBOSE message in the test framework log.
  Record is associated with the currently executing test case.

  @param[in]  Format  Formatting string following the format defined in
                      MdePkg/Include/Library/PrintLib.h.
  @param[in]  ...     Print args.
**/
#define UT_LOG_VERBOSE(Format, ...)  \
  UnitTestLog (UNIT_TEST_LOG_LEVEL_VERBOSE, Format, ##__VA_ARGS__)

/**
  Test logging function that records a messages in the test framework log.
  Record is associated with the currently executing test case.

  @param[in]  ErrorLevel  The error level of the unit test log message.
  @param[in]  Format      Formatting string following the format defined in the
                          MdePkg/Include/Library/PrintLib.h.
  @param[in]  ...         Print args.
**/
VOID
EFIAPI
UnitTestLog (
  IN  UINTN        ErrorLevel,
  IN  CONST CHAR8  *Format,
  ...
  );

#endif
