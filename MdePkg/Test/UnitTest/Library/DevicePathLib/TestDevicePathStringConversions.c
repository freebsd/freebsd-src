/** @file
  UEFI OS based application for unit testing the DevicePathLib.

  Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "TestDevicePathLib.h"

typedef struct {
  EFI_DEVICE_PATH_PROTOCOL    *DevPath;
  CONST CHAR16                *DevPathString;
} DEVICE_PATH_CONVERSIONS_TEST_CONTEXT;

GLOBAL_REMOVE_IF_UNREFERENCED PCI_DEVICE_PATH  mPciDevicePathNode =
{
  // PCI device path - root port (0x2:0x0)
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
};

GLOBAL_REMOVE_IF_UNREFERENCED ACPI_HID_DEVICE_PATH  mAcpiPciRootHidDevicePathNode =
{
  // ACPI PCI root - PciRoot(0x0)
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
};

GLOBAL_REMOVE_IF_UNREFERENCED ACPI_HID_DEVICE_PATH  mAcpiNonPciRootHidDevicePathNode =
{
  // Random ACPI device - ACPI(PNPB0C0, 1)
  {
    ACPI_DEVICE_PATH,
    ACPI_DP,
    {
      (UINT8)(sizeof (ACPI_HID_DEVICE_PATH)),
      (UINT8)((sizeof (ACPI_HID_DEVICE_PATH)) >> 8)
    }
  },
  EISA_PNP_ID (0xB0C0),
  1
};

#define HID_STR_SIZE  8
#define CID_STR_SIZE  8
#define UID_STR_SIZE  8

typedef struct {
  ACPI_EXTENDED_HID_DEVICE_PATH    AcpiEx;
  CHAR8                            HidStr[HID_STR_SIZE];
  CHAR8                            CidStr[CID_STR_SIZE];
  CHAR8                            UidStr[UID_STR_SIZE];
} ACPI_EXTENDED_HID_DEVICE_PATH_FULL;

GLOBAL_REMOVE_IF_UNREFERENCED ACPI_EXTENDED_HID_DEVICE_PATH_FULL  mAcpiExtendedDevicePathFull =
{
  // ACPI Extended HID PciRoot device node
  {
    {
      ACPI_DEVICE_PATH,
      ACPI_EXTENDED_DP,
      {
        (UINT8)(sizeof (ACPI_EXTENDED_HID_DEVICE_PATH_FULL)),
        (UINT8)((sizeof (ACPI_EXTENDED_HID_DEVICE_PATH_FULL)) >> 8)
      }
    },
    0,
    0,
    0,
  },
  { 'P', 'N', 'P', 'B', '0', 'C', '0', '\0' }, // HIDSTR
  { 'P', 'N', 'P', '0', '0', '0', '1', '\0' }, // CIDSTR
  { 'U', 'I', 'D', '0', '0', '0', '0', '\0' } // UIDSTR
};

typedef struct {
  ACPI_EXTENDED_HID_DEVICE_PATH    AcpiEx;
  CHAR8                            HidStr[HID_STR_SIZE];
} ACPI_EXTENDED_HID_DEVICE_PATH_PARTIAL;

GLOBAL_REMOVE_IF_UNREFERENCED ACPI_EXTENDED_HID_DEVICE_PATH_PARTIAL  mAcpiExtendedDevicePathPartial =
{
  {
    {
      ACPI_DEVICE_PATH,
      ACPI_EXTENDED_DP,
      {
        (UINT8)(sizeof (ACPI_EXTENDED_HID_DEVICE_PATH_PARTIAL)),
        (UINT8)((sizeof (ACPI_EXTENDED_HID_DEVICE_PATH_PARTIAL)) >> 8)
      }
    },
    0,
    2,
    0,
  },
  { 'P', 'N', 'P', 'B', '0', '\0', '\0', '\0' }, // HIDSTR
};

typedef struct {
  ACPI_EXTENDED_HID_DEVICE_PATH    AcpiEx;
  CHAR8                            UidStr[UID_STR_SIZE];
} ACPI_EXPANDED_DEVICE_PATH;

GLOBAL_REMOVE_IF_UNREFERENCED ACPI_EXPANDED_DEVICE_PATH  mAcpiExpandedDevicePathUidOnly =
{
  {
    {
      ACPI_DEVICE_PATH,
      ACPI_EXTENDED_DP,
      {
        (UINT8)(sizeof (ACPI_EXPANDED_DEVICE_PATH)),
        (UINT8)((sizeof (ACPI_EXPANDED_DEVICE_PATH)) >> 8)
      }
    },
    EISA_PNP_ID (0xAAAA),
    0,
    0,
  },
  { '\0', 'U', 'I', 'D', '0', '0', '\0', '\0' } // UIDSTR
};

GLOBAL_REMOVE_IF_UNREFERENCED ACPI_EXPANDED_DEVICE_PATH  mAcpiExpandedDevicePathUidOnlyWithCid =
{
  {
    {
      ACPI_DEVICE_PATH,
      ACPI_EXTENDED_DP,
      {
        (UINT8)(sizeof (ACPI_EXPANDED_DEVICE_PATH)),
        (UINT8)((sizeof (ACPI_EXPANDED_DEVICE_PATH)) >> 8)
      }
    },
    EISA_PNP_ID (0xAAAA),
    0,
    EISA_PNP_ID (0xAADD),
  },
  { '\0', 'U', 'I', 'D', '0', '0', '\0', '\0' } // UIDSTR
};

GLOBAL_REMOVE_IF_UNREFERENCED DEVICE_PATH_CONVERSIONS_TEST_CONTEXT  mDevPathNodeToFromTextContext[] = {
  {
    (EFI_DEVICE_PATH_PROTOCOL *)&mPciDevicePathNode,
    L"Pci(0x0,0x2)"
  },
  {
    (EFI_DEVICE_PATH_PROTOCOL *)&mAcpiPciRootHidDevicePathNode,
    L"PciRoot(0x0)"
  },
  {
    (EFI_DEVICE_PATH_PROTOCOL *)&mAcpiNonPciRootHidDevicePathNode,
    L"Acpi(PNPB0C0,0x1)"
  },
  {
    (EFI_DEVICE_PATH_PROTOCOL *)&mAcpiExtendedDevicePathFull,
    L"AcpiEx(@@@0000,@@@0000,0x0,PNPB0C0,UID0000,PNP0001)"
  },
  {
    (EFI_DEVICE_PATH_PROTOCOL *)&mAcpiExtendedDevicePathPartial,
    L"AcpiEx(@@@0000,@@@0000,0x2,PNPB0,,)"
  },
  {
    (EFI_DEVICE_PATH_PROTOCOL *)&mAcpiExpandedDevicePathUidOnly,
    L"AcpiExp(PNPAAAA,0,UID00)"
  },
  {
    (EFI_DEVICE_PATH_PROTOCOL *)&mAcpiExpandedDevicePathUidOnlyWithCid,
    L"AcpiExp(PNPAAAA,PNPAADD,UID00)"
  }
};

typedef struct {
  ACPI_HID_DEVICE_PATH        AcpiPath;
  PCI_DEVICE_PATH             PciPathRootPort;
  PCI_DEVICE_PATH             PciPathEndPoint;
  USB_DEVICE_PATH             UsbPath;
  EFI_DEVICE_PATH_PROTOCOL    End;
} TEST_CONVERSIONS_DEVICE_PATH;

GLOBAL_REMOVE_IF_UNREFERENCED TEST_CONVERSIONS_DEVICE_PATH  mConversionsDevicePath = {
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

GLOBAL_REMOVE_IF_UNREFERENCED DEVICE_PATH_CONVERSIONS_TEST_CONTEXT  mDevPathToFromTextContext[] = {
  {
    (EFI_DEVICE_PATH_PROTOCOL *)&mConversionsDevicePath,
    L"PciRoot(0x0)/Pci(0x0,0x2)/Pci(0x0,0x0)/USB(0x0,0x2)"
  }
};

UNIT_TEST_STATUS
EFIAPI
TestConvertDevicePathToText (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CHAR16                                *DevPathString;
  DEVICE_PATH_CONVERSIONS_TEST_CONTEXT  *TestContext;

  TestContext = (DEVICE_PATH_CONVERSIONS_TEST_CONTEXT *)Context;

  DevPathString = ConvertDevicePathToText (TestContext->DevPath, FALSE, FALSE);
  UT_ASSERT_EQUAL (StrLen (DevPathString), StrLen (TestContext->DevPathString));
  UT_ASSERT_MEM_EQUAL (DevPathString, TestContext->DevPathString, StrLen (TestContext->DevPathString));
  FreePool (DevPathString);

  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
TestConvertTextToDevicePath (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_DEVICE_PATH_PROTOCOL              *ConvertedDevPath;
  DEVICE_PATH_CONVERSIONS_TEST_CONTEXT  *TestContext;

  TestContext = (DEVICE_PATH_CONVERSIONS_TEST_CONTEXT *)Context;

  ConvertedDevPath = ConvertTextToDevicePath (TestContext->DevPathString);
  UT_ASSERT_EQUAL (GetDevicePathSize (ConvertedDevPath), GetDevicePathSize (TestContext->DevPath));
  UT_ASSERT_MEM_EQUAL (ConvertedDevPath, TestContext->DevPath, GetDevicePathSize (TestContext->DevPath));
  FreePool (ConvertedDevPath);

  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
TestConvertDeviceNodeToText (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CHAR16                                *DevPathNodeString;
  DEVICE_PATH_CONVERSIONS_TEST_CONTEXT  *TestContext;

  TestContext = (DEVICE_PATH_CONVERSIONS_TEST_CONTEXT *)Context;

  DevPathNodeString = ConvertDeviceNodeToText (TestContext->DevPath, FALSE, FALSE);
  UT_ASSERT_EQUAL (StrLen (DevPathNodeString), StrLen (TestContext->DevPathString));
  UT_ASSERT_MEM_EQUAL (DevPathNodeString, TestContext->DevPathString, StrLen (TestContext->DevPathString));
  FreePool (DevPathNodeString);

  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
TestConvertTextToDeviceNode (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_DEVICE_PATH_PROTOCOL              *DevPath;
  DEVICE_PATH_CONVERSIONS_TEST_CONTEXT  *TestContext;

  TestContext = (DEVICE_PATH_CONVERSIONS_TEST_CONTEXT *)Context;

  DevPath = ConvertTextToDeviceNode (TestContext->DevPathString);
  UT_ASSERT_EQUAL (DevicePathNodeLength (DevPath), DevicePathNodeLength (TestContext->DevPath));
  UT_ASSERT_MEM_EQUAL (DevPath, TestContext->DevPath, DevicePathNodeLength (TestContext->DevPath));
  FreePool (DevPath);

  return UNIT_TEST_PASSED;
}

EFI_STATUS
CreateDevicePathStringConversionsTestSuite (
  IN UNIT_TEST_FRAMEWORK_HANDLE  Framework
  )
{
  EFI_STATUS              Status;
  UNIT_TEST_SUITE_HANDLE  DevicePathTextConversionSuite = NULL;
  UINTN                   Index;

  Status = CreateUnitTestSuite (&DevicePathTextConversionSuite, Framework, "Device path text conversion operations test suite", "Common.DevicePath.TextConversions", NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to create device path text conversions test suite\n"));
    return Status;
  }

  for (Index = 0; Index < ARRAY_SIZE (mDevPathNodeToFromTextContext); Index++) {
    AddTestCase (DevicePathTextConversionSuite, "Test ConvertDeviceNodeToText", "TestConvertDeviceNodeToText", TestConvertDeviceNodeToText, NULL, NULL, &mDevPathNodeToFromTextContext[Index]);
  }

  for (Index = 0; Index < ARRAY_SIZE (mDevPathNodeToFromTextContext); Index++) {
    AddTestCase (DevicePathTextConversionSuite, "Test ConvertTextToDeviceNode", "TestConvertTextToDeviceNode", TestConvertTextToDeviceNode, NULL, NULL, &mDevPathNodeToFromTextContext[Index]);
  }

  for (Index = 0; Index < ARRAY_SIZE (mDevPathToFromTextContext); Index++) {
    AddTestCase (DevicePathTextConversionSuite, "Test ConvertDevicePathToText", "TestConvertDevicePathToText", TestConvertDevicePathToText, NULL, NULL, &mDevPathToFromTextContext[Index]);
  }

  for (Index = 0; Index < ARRAY_SIZE (mDevPathToFromTextContext); Index++) {
    AddTestCase (DevicePathTextConversionSuite, "Test ConvertTextToDevicePath", "TestConvertTextToDevicePath", TestConvertTextToDevicePath, NULL, NULL, &mDevPathToFromTextContext[Index]);
  }

  return EFI_SUCCESS;
}
