/** @file
  UEFI OS based application for unit testing the DevicePathLib.

  Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "TestDevicePathLib.h"

#define UNIT_TEST_NAME     "DevicePathLib Unit Test Application"
#define UNIT_TEST_VERSION  "0.1"

typedef struct {
  ACPI_HID_DEVICE_PATH        AcpiPath;
  PCI_DEVICE_PATH             PciPathRootPort;
  PCI_DEVICE_PATH             PciPathEndPoint;
  USB_DEVICE_PATH             UsbPath;
  EFI_DEVICE_PATH_PROTOCOL    End;
} TEST_COMPLEX_DEVICE_PATH;

GLOBAL_REMOVE_IF_UNREFERENCED TEST_COMPLEX_DEVICE_PATH  mComplexDevicePath = {
  { // ACPI device path with root bridge EISA_PNP_ID
    {
      ACPI_DEVICE_PATH,
      ACPI_DP,
      {
        (UINT8)(sizeof (ACPI_HID_DEVICE_PATH)),
        (UINT8)((sizeof (ACPI_HID_DEVICE_PATH)) >> 8)
      }
    },
    EISA_PNP_ID (0x0A03),
    0
  },
  { // PCI device path - root port (0x2:0x0)
    {
      HARDWARE_DEVICE_PATH,
      HW_PCI_DP,
      {
        (UINT8)(sizeof (PCI_DEVICE_PATH)),
        (UINT8)((sizeof (PCI_DEVICE_PATH)) >> 8)
      }
    },
    0x2,
    0x0
  },
  { // PCI device path - endpoint (0x0:0x0)
    {
      HARDWARE_DEVICE_PATH,
      HW_PCI_DP,
      {
        (UINT8)(sizeof (PCI_DEVICE_PATH)),
        (UINT8)((sizeof (PCI_DEVICE_PATH)) >> 8)
      }
    },
    0x0,
    0x0
  },
  { // USB interface
    {
      MESSAGING_DEVICE_PATH,
      MSG_USB_DP,
      {
        (UINT8)(sizeof (USB_DEVICE_PATH)),
        (UINT8)((sizeof (USB_CLASS_DEVICE_PATH)) >> 8)
      }
    },
    0,
    2
  },
  {
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE,
    {
      (UINT8)(sizeof (EFI_DEVICE_PATH_PROTOCOL)),
      (UINT8)((sizeof (EFI_DEVICE_PATH_PROTOCOL)) >> 8)
    }
  }
};

CONST GLOBAL_REMOVE_IF_UNREFERENCED CHAR16  *mComplexDevicePathString = L"PciRoot(0x0)/Pci(0x0,0x2)/Pci(0x0,0x0)/USB(0x0,0x2)";

CONST GLOBAL_REMOVE_IF_UNREFERENCED CHAR16  *mPciEndPointPathString = L"Pci(0x0, 0x0)";

typedef struct {
  ACPI_HID_DEVICE_PATH        AcpiPath;
  EFI_DEVICE_PATH_PROTOCOL    End;
} TEST_SIMPLE_DEVICE_PATH;

GLOBAL_REMOVE_IF_UNREFERENCED TEST_SIMPLE_DEVICE_PATH  mSimpleDevicePath = {
  { // ACPI device path with root bridge EISA_PNP_ID
    {
      ACPI_DEVICE_PATH,
      ACPI_DP,
      {
        (UINT8)(sizeof (ACPI_HID_DEVICE_PATH)),
        (UINT8)((sizeof (ACPI_HID_DEVICE_PATH)) >> 8)
      }
    },
    EISA_PNP_ID (0x0A03),
    0
  },
  {
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE,
    {
      (UINT8)(sizeof (EFI_DEVICE_PATH_PROTOCOL)),
      (UINT8)((sizeof (EFI_DEVICE_PATH_PROTOCOL)) >> 8)
    }
  }
};

GLOBAL_REMOVE_IF_UNREFERENCED TEST_SIMPLE_DEVICE_PATH  mInvalidSimpleDevicePath = {
  { // ACPI device path with root bridge EISA_PNP_ID
    {
      ACPI_DEVICE_PATH,
      ACPI_DP,
      {
        0,
        0
      }
    },
    EISA_PNP_ID (0x0A03),
    0
  },
  {
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE,
    {
      (UINT8)(sizeof (EFI_DEVICE_PATH_PROTOCOL)),
      (UINT8)((sizeof (EFI_DEVICE_PATH_PROTOCOL)) >> 8)
    }
  }
};

typedef struct {
  TEST_SIMPLE_DEVICE_PATH     *SimpleDevicePath;
  TEST_SIMPLE_DEVICE_PATH     *InvalidDevicePath;
  TEST_COMPLEX_DEVICE_PATH    *ComplexDevicePath;
} SIMPLE_TEST_SUITE_CONTEXT;

UNIT_TEST_STATUS
EFIAPI
TestIsDevicePathValid (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  BOOLEAN                    IsValid;
  SIMPLE_TEST_SUITE_CONTEXT  *TestContext;

  TestContext = (SIMPLE_TEST_SUITE_CONTEXT *)Context;

  IsValid = IsDevicePathValid ((EFI_DEVICE_PATH_PROTOCOL *)TestContext->SimpleDevicePath, sizeof (TEST_SIMPLE_DEVICE_PATH));
  UT_ASSERT_TRUE (IsValid);

  IsValid = IsDevicePathValid ((EFI_DEVICE_PATH_PROTOCOL *)TestContext->ComplexDevicePath, sizeof (TEST_COMPLEX_DEVICE_PATH));
  UT_ASSERT_TRUE (IsValid);

  IsValid = IsDevicePathValid ((EFI_DEVICE_PATH_PROTOCOL *)TestContext->ComplexDevicePath, 0);
  UT_ASSERT_TRUE (IsValid);

  // Device path can't be NULL
  IsValid = IsDevicePathValid (NULL, 0);
  UT_ASSERT_FALSE (IsValid);

  // MaxSize can't be less then the size of the device path
  IsValid = IsDevicePathValid ((EFI_DEVICE_PATH_PROTOCOL *)TestContext->SimpleDevicePath, sizeof (TEST_SIMPLE_DEVICE_PATH) - 1);
  UT_ASSERT_FALSE (IsValid);

  // If MaxSize != 0 it must be bigger then EFI_DEVICE_PATH_PROTOCOL
  IsValid = IsDevicePathValid ((EFI_DEVICE_PATH_PROTOCOL *)TestContext->SimpleDevicePath, sizeof (EFI_DEVICE_PATH_PROTOCOL) - 1);
  UT_ASSERT_FALSE (IsValid);

  IsValid = IsDevicePathValid ((EFI_DEVICE_PATH_PROTOCOL *)TestContext->InvalidDevicePath, 0);
  UT_ASSERT_FALSE (IsValid);

  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
TestDevicePathType (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UINT8                      Type;
  SIMPLE_TEST_SUITE_CONTEXT  *TestContext;

  TestContext = (SIMPLE_TEST_SUITE_CONTEXT *)Context;

  // Test 2 types just in case the implementation is returning constant value
  // NOTE: passing NULL to this function causes NULL pointer dereference.
  Type = DevicePathType (&TestContext->ComplexDevicePath->AcpiPath);
  UT_ASSERT_EQUAL (Type, ACPI_DEVICE_PATH);

  Type = DevicePathType (&TestContext->ComplexDevicePath->PciPathRootPort);
  UT_ASSERT_EQUAL (Type, HARDWARE_DEVICE_PATH);

  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
TestDevicePathSubType (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UINT8                      SubType;
  SIMPLE_TEST_SUITE_CONTEXT  *TestContext;

  TestContext = (SIMPLE_TEST_SUITE_CONTEXT *)Context;

  // Test 2 sub types just in case the implementation is returning constant value
  // NOTE: passing NULL to this function causes NULL pointer dereference.
  SubType = DevicePathSubType (&TestContext->ComplexDevicePath->AcpiPath);
  UT_ASSERT_EQUAL (SubType, ACPI_DP);

  SubType = DevicePathSubType (&TestContext->ComplexDevicePath->PciPathRootPort);
  UT_ASSERT_EQUAL (SubType, HW_PCI_DP);

  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
TestDevicePathNodeLength (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UINTN                      Length;
  SIMPLE_TEST_SUITE_CONTEXT  *TestContext;

  TestContext = (SIMPLE_TEST_SUITE_CONTEXT *)Context;

  // Test 2 nodes just in case the implementation is returning constant value
  // NOTE: passing NULL to this function causes NULL pointer dereference.
  Length = DevicePathNodeLength (&TestContext->ComplexDevicePath->AcpiPath);
  UT_ASSERT_EQUAL (Length, sizeof (ACPI_HID_DEVICE_PATH));

  Length = DevicePathNodeLength (&TestContext->ComplexDevicePath->PciPathRootPort);
  UT_ASSERT_EQUAL (Length, sizeof (PCI_DEVICE_PATH));

  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
TestNextDevicePathNode (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  VOID                       *Node;
  SIMPLE_TEST_SUITE_CONTEXT  *TestContext;

  TestContext = (SIMPLE_TEST_SUITE_CONTEXT *)Context;

  Node = &mComplexDevicePath;
  Node = NextDevicePathNode (Node);
  UT_ASSERT_MEM_EQUAL (Node, &TestContext->ComplexDevicePath->PciPathRootPort, DevicePathNodeLength (Node));

  Node = NextDevicePathNode (Node);
  UT_ASSERT_MEM_EQUAL (Node, &TestContext->ComplexDevicePath->PciPathEndPoint, DevicePathNodeLength (Node));

  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
TestIsDevicePathEndType (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  BOOLEAN                    IsEndType;
  SIMPLE_TEST_SUITE_CONTEXT  *TestContext;

  TestContext = (SIMPLE_TEST_SUITE_CONTEXT *)Context;

  IsEndType = IsDevicePathEndType (&TestContext->ComplexDevicePath->PciPathRootPort);
  UT_ASSERT_FALSE (IsEndType);

  IsEndType = IsDevicePathEndType (&TestContext->ComplexDevicePath->End);
  UT_ASSERT_TRUE (IsEndType);

  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
TestIsDevicePathEnd (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  BOOLEAN                    IsEnd;
  SIMPLE_TEST_SUITE_CONTEXT  *TestContext;

  TestContext = (SIMPLE_TEST_SUITE_CONTEXT *)Context;

  IsEnd = IsDevicePathEnd (&TestContext->ComplexDevicePath->PciPathRootPort);
  UT_ASSERT_FALSE (IsEnd);

  IsEnd = IsDevicePathEnd (&TestContext->ComplexDevicePath->End);
  UT_ASSERT_TRUE (IsEnd);

  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
TestSetDevicePathNodeLength (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_DEVICE_PATH_PROTOCOL  DevPath;

  // NOTE: Node == NULL or NodeLength >= 0x10000 NodeLength < sizeof (EFI_DEVICE_PATH_PROTOCOL)
  // are all invalid parameters. However there are only ASSERTS added to catch them so there is no
  // way to test it.
  SetDevicePathNodeLength (&DevPath, sizeof (EFI_DEVICE_PATH_PROTOCOL));
  UT_ASSERT_EQUAL (DevicePathNodeLength (&DevPath), sizeof (EFI_DEVICE_PATH_PROTOCOL));

  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
TestSetDevicePathEndNode (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_DEVICE_PATH_PROTOCOL  EndNode;

  SetDevicePathEndNode (&EndNode);
  UT_ASSERT_EQUAL (EndNode.Type, END_DEVICE_PATH_TYPE);
  UT_ASSERT_EQUAL (EndNode.SubType, END_ENTIRE_DEVICE_PATH_SUBTYPE);
  UT_ASSERT_EQUAL (DevicePathNodeLength (&EndNode), END_DEVICE_PATH_LENGTH);

  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
TestGetDevicePathSize (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UINTN                      Size;
  SIMPLE_TEST_SUITE_CONTEXT  *TestContext;

  TestContext = (SIMPLE_TEST_SUITE_CONTEXT *)Context;

  Size = GetDevicePathSize ((EFI_DEVICE_PATH_PROTOCOL *)TestContext->SimpleDevicePath);
  UT_ASSERT_EQUAL (Size, sizeof (TEST_SIMPLE_DEVICE_PATH));

  Size = GetDevicePathSize ((EFI_DEVICE_PATH_PROTOCOL *)TestContext->ComplexDevicePath);
  UT_ASSERT_EQUAL (Size, sizeof (TEST_COMPLEX_DEVICE_PATH));

  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
TestDuplicateDevicePath (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_DEVICE_PATH_PROTOCOL   *Duplicate;
  SIMPLE_TEST_SUITE_CONTEXT  *TestContext;

  TestContext = (SIMPLE_TEST_SUITE_CONTEXT *)Context;

  Duplicate = DuplicateDevicePath ((EFI_DEVICE_PATH_PROTOCOL *)TestContext->ComplexDevicePath);
  UT_ASSERT_EQUAL (GetDevicePathSize (Duplicate), GetDevicePathSize ((EFI_DEVICE_PATH_PROTOCOL *)TestContext->ComplexDevicePath));
  UT_ASSERT_MEM_EQUAL (Duplicate, TestContext->ComplexDevicePath, GetDevicePathSize ((EFI_DEVICE_PATH_PROTOCOL *)TestContext->ComplexDevicePath));
  FreePool (Duplicate);

  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
TestAppendDevicePath (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_DEVICE_PATH_PROTOCOL  *Appended;
  EFI_DEVICE_PATH_PROTOCOL  *NextNode;

  Appended = AppendDevicePath ((EFI_DEVICE_PATH_PROTOCOL *)&mSimpleDevicePath, (EFI_DEVICE_PATH_PROTOCOL *)&mComplexDevicePath);
  NextNode = NextDevicePathNode (Appended);
  UT_ASSERT_MEM_EQUAL (NextNode, &mSimpleDevicePath.AcpiPath, sizeof (ACPI_HID_DEVICE_PATH));
  FreePool (Appended);

  // If one of the paths is invalid result device path should be NULL
  Appended = AppendDevicePath ((EFI_DEVICE_PATH_PROTOCOL *)&mSimpleDevicePath, (EFI_DEVICE_PATH_PROTOCOL *)&mInvalidSimpleDevicePath);
  UT_ASSERT_EQUAL ((uintptr_t)Appended, (uintptr_t)NULL);

  Appended = AppendDevicePath (NULL, NULL);
  UT_ASSERT_EQUAL (Appended->Type, END_DEVICE_PATH_TYPE);
  UT_ASSERT_EQUAL (Appended->SubType, END_ENTIRE_DEVICE_PATH_SUBTYPE);
  UT_ASSERT_EQUAL (DevicePathNodeLength (Appended), END_DEVICE_PATH_LENGTH);
  FreePool (Appended);

  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
TestAppendDevicePathNode (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_DEVICE_PATH_PROTOCOL  *Appended;
  EFI_DEVICE_PATH_PROTOCOL  *NextNode;
  BOOLEAN                   IsValid;
  ACPI_HID_DEVICE_PATH      AcpiPath =
  {
    {
      ACPI_DEVICE_PATH,
      ACPI_DP,
      {
        (UINT8)(sizeof (ACPI_HID_DEVICE_PATH)),
        (UINT8)((sizeof (ACPI_HID_DEVICE_PATH)) >> 8)
      }
    },
    EISA_PNP_ID (0x0AAB),
    0
  };

  Appended = AppendDevicePathNode ((EFI_DEVICE_PATH_PROTOCOL *)&mSimpleDevicePath, (EFI_DEVICE_PATH_PROTOCOL *)&AcpiPath);
  NextNode = NextDevicePathNode (Appended);
  UT_ASSERT_MEM_EQUAL (NextNode, &AcpiPath, sizeof (ACPI_HID_DEVICE_PATH));
  FreePool (Appended);

  Appended = AppendDevicePathNode (NULL, (EFI_DEVICE_PATH_PROTOCOL *)&AcpiPath);
  UT_ASSERT_MEM_EQUAL (Appended, &AcpiPath, sizeof (ACPI_HID_DEVICE_PATH));
  IsValid = IsDevicePathValid (Appended, 0);
  UT_ASSERT_TRUE (IsValid);
  FreePool (Appended);

  Appended = AppendDevicePathNode (NULL, NULL);
  UT_ASSERT_EQUAL (Appended->Type, END_DEVICE_PATH_TYPE);
  UT_ASSERT_EQUAL (Appended->SubType, END_ENTIRE_DEVICE_PATH_SUBTYPE);
  UT_ASSERT_EQUAL (DevicePathNodeLength (Appended), END_DEVICE_PATH_LENGTH);
  FreePool (Appended);

  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
TestAppendDevicePathInstance (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_DEVICE_PATH_PROTOCOL  *Appended;
  EFI_DEVICE_PATH_PROTOCOL  *NextInstance;
  EFI_DEVICE_PATH_PROTOCOL  *NextInstanceRet;
  BOOLEAN                   IsMultiInstance;
  UINTN                     Size;

  Appended        = AppendDevicePathInstance ((EFI_DEVICE_PATH_PROTOCOL *)&mSimpleDevicePath, (EFI_DEVICE_PATH_PROTOCOL *)&mComplexDevicePath);
  IsMultiInstance = IsDevicePathMultiInstance (Appended);
  UT_ASSERT_TRUE (IsMultiInstance);
  UT_ASSERT_MEM_EQUAL (Appended, &mSimpleDevicePath, sizeof (ACPI_DEVICE_PATH));
  NextInstance    = Appended;
  NextInstanceRet = GetNextDevicePathInstance (&NextInstance, &Size);
  UT_ASSERT_MEM_EQUAL (NextInstance, &mComplexDevicePath, Size);
  FreePool (Appended);
  FreePool (NextInstanceRet);

  Appended = AppendDevicePathInstance (NULL, (EFI_DEVICE_PATH_PROTOCOL *)&mSimpleDevicePath);
  UT_ASSERT_MEM_EQUAL (Appended, &mSimpleDevicePath, sizeof (TEST_SIMPLE_DEVICE_PATH));
  FreePool (Appended);

  Appended = AppendDevicePathInstance (NULL, NULL);
  UT_ASSERT_EQUAL ((uintptr_t)Appended, (uintptr_t)NULL);

  Appended = AppendDevicePathInstance ((EFI_DEVICE_PATH_PROTOCOL *)&mSimpleDevicePath, (EFI_DEVICE_PATH_PROTOCOL *)&mInvalidSimpleDevicePath);
  UT_ASSERT_EQUAL ((uintptr_t)Appended, (uintptr_t)NULL);

  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
TestDevicePathFromHandle (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_HANDLE                Handle;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
  UINTN                     FakeHandle;

  Handle     = NULL;
  DevicePath = DevicePathFromHandle (Handle);
  UT_ASSERT_EQUAL ((uintptr_t)DevicePath, (uintptr_t)NULL);

  Handle     = (EFI_HANDLE)&FakeHandle;
  DevicePath = DevicePathFromHandle (Handle);
  UT_ASSERT_EQUAL ((uintptr_t)DevicePath, (uintptr_t)NULL);

  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
TestCreateDeviceNode (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_DEVICE_PATH_PROTOCOL  *DevNode;

  DevNode = CreateDeviceNode (HARDWARE_DEVICE_PATH, HW_PCI_DP, sizeof (PCI_DEVICE_PATH));
  UT_ASSERT_EQUAL (DevNode->Type, HARDWARE_DEVICE_PATH);
  UT_ASSERT_EQUAL (DevNode->SubType, HW_PCI_DP);
  UT_ASSERT_EQUAL (DevicePathNodeLength (DevNode), sizeof (PCI_DEVICE_PATH));

  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
TestFileDevicePath (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_HANDLE            Handle;
  FILEPATH_DEVICE_PATH  *DevicePath;
  CONST CHAR16          *TestFilePath = L"FS0:/Boot/EFI/BootMgr.efi";

  Handle     = NULL;
  DevicePath = (FILEPATH_DEVICE_PATH *)FileDevicePath (Handle, TestFilePath);
  UT_ASSERT_NOT_NULL (DevicePath);
  UT_ASSERT_EQUAL (DevicePath->Header.Type, MEDIA_DEVICE_PATH);
  UT_ASSERT_EQUAL (DevicePath->Header.Type, MEDIA_FILEPATH_DP);
  UT_ASSERT_MEM_EQUAL (DevicePath->PathName, TestFilePath, StrSize (TestFilePath));

  return UNIT_TEST_PASSED;
}

/**

  Main fuction sets up the unit test environment

**/
EFI_STATUS
EFIAPI
UefiTestMain (
  VOID
  )
{
  EFI_STATUS                  Status;
  UNIT_TEST_FRAMEWORK_HANDLE  Framework;
  UNIT_TEST_SUITE_HANDLE      DevicePathSimpleTestSuite;
  UNIT_TEST_SUITE_HANDLE      DevicePathAppendTestSuite;
  UNIT_TEST_SUITE_HANDLE      DevicePathFileTestSuite;
  SIMPLE_TEST_SUITE_CONTEXT   SimpleTestContext;

  DEBUG ((DEBUG_INFO, "%a v%a\n", UNIT_TEST_NAME, UNIT_TEST_VERSION));

  Framework                 = NULL;
  DevicePathSimpleTestSuite = NULL;
  DevicePathAppendTestSuite = NULL;
  DevicePathFileTestSuite   = NULL;

  Status = InitUnitTestFramework (&Framework, UNIT_TEST_NAME, gEfiCallerBaseName, UNIT_TEST_VERSION);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed in InitUnitTestFramework. Status = %r\n", Status));
    goto EXIT;
  }

  Status = CreateUnitTestSuite (&DevicePathSimpleTestSuite, Framework, "Simple device path operations test suite", "Common.DevicePath.SimpleOps", NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to create simple device path test suite\n"));
    goto EXIT;
  }

  SimpleTestContext.SimpleDevicePath  = &mSimpleDevicePath;
  SimpleTestContext.InvalidDevicePath = &mInvalidSimpleDevicePath;
  SimpleTestContext.ComplexDevicePath = &mComplexDevicePath;

  AddTestCase (DevicePathSimpleTestSuite, "Test IsDevicePathValid", "TestIsDevicePathValid", TestIsDevicePathValid, NULL, NULL, &SimpleTestContext);
  AddTestCase (DevicePathSimpleTestSuite, "Test DevicePathType", "TestDevicePathType", TestDevicePathType, NULL, NULL, &SimpleTestContext);
  AddTestCase (DevicePathSimpleTestSuite, "Test DevicePathSubType", "TestDevicePathSubType", TestDevicePathSubType, NULL, NULL, &SimpleTestContext);
  AddTestCase (DevicePathSimpleTestSuite, "Test DevicePathNodeLength", "TestDevicePathNodeLength", TestDevicePathNodeLength, NULL, NULL, &SimpleTestContext);
  AddTestCase (DevicePathSimpleTestSuite, "Test NextDevicePathNode", "TestNextDevicePathNode", TestNextDevicePathNode, NULL, NULL, &SimpleTestContext);
  AddTestCase (DevicePathSimpleTestSuite, "Test IsDevicePathEndType", "TestIsDevicePathEndType", TestIsDevicePathEndType, NULL, NULL, &SimpleTestContext);
  AddTestCase (DevicePathSimpleTestSuite, "Test IsDevicePathEnd", "TestIsDevicePathEnd", TestIsDevicePathEnd, NULL, NULL, &SimpleTestContext);
  AddTestCase (DevicePathSimpleTestSuite, "Test SetDevicePathNodeLength", "TestSetDevicePathNodeLength", TestSetDevicePathNodeLength, NULL, NULL, &SimpleTestContext);
  AddTestCase (DevicePathSimpleTestSuite, "Test GetDevicePathSize", "TestGetDevicePathSize", TestGetDevicePathSize, NULL, NULL, &SimpleTestContext);
  AddTestCase (DevicePathSimpleTestSuite, "Test CreateDeviceNode", "TestCreateDeviceNode", TestCreateDeviceNode, NULL, NULL, &SimpleTestContext);
  AddTestCase (DevicePathSimpleTestSuite, "Test SetDevicePathEndNode", "TestSetDevicePathEndNode", TestSetDevicePathEndNode, NULL, NULL, &SimpleTestContext);
  AddTestCase (DevicePathAppendTestSuite, "Test DuplicateDevicePath", "TestDuplicateDevicePath", TestDuplicateDevicePath, NULL, NULL, &SimpleTestContext);

  Status = CreateUnitTestSuite (&DevicePathAppendTestSuite, Framework, "Device path append operations test suite", "Common.DevicePath.Append", NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to create append device path test suite\n"));
    goto EXIT;
  }

  AddTestCase (DevicePathAppendTestSuite, "Test AppendDevicePath", "TestAppendDevicePath", TestAppendDevicePath, NULL, NULL, NULL);
  AddTestCase (DevicePathAppendTestSuite, "Test AppendDevicePathNode", "TestAppendDevicePathNode", TestAppendDevicePathNode, NULL, NULL, NULL);
  AddTestCase (DevicePathAppendTestSuite, "Test AppendDevicePathInstance", "TestAppendDevicePathInstance", TestAppendDevicePathInstance, NULL, NULL, NULL);

  Status = CreateDevicePathStringConversionsTestSuite (Framework);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to create conversions test suite\n"));
    goto EXIT;
  }

  Status = CreateUnitTestSuite (&DevicePathFileTestSuite, Framework, "Device path file operations test suite", "Common.DevicePath.FileDevPath", NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to create device path file test suite\n"));
    goto EXIT;
  }

  AddTestCase (DevicePathFileTestSuite, "Test DevicePathFromHandle", "TestDevicePathFromHandle", TestDevicePathFromHandle, NULL, NULL, NULL);
  AddTestCase (DevicePathFileTestSuite, "Test FileDevicePath", "TestFileDevicePath", TestFileDevicePath, NULL, NULL, NULL);

  Status = RunAllTestSuites (Framework);

EXIT:
  if (Framework != NULL) {
    FreeUnitTestFramework (Framework);
  }

  return Status;
}

int
main (
  int   argc,
  char  *argv[]
  )
{
  return UefiTestMain ();
}
