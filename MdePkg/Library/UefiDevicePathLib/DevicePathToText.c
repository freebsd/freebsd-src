/** @file
  DevicePathToText protocol as defined in the UEFI 2.0 specification.

  (C) Copyright 2015 Hewlett-Packard Development Company, L.P.<BR>
Copyright (c) 2013 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "UefiDevicePathLib.h"

/**
  Concatenates a formatted unicode string to allocated pool. The caller must
  free the resulting buffer.

  @param Str             Tracks the allocated pool, size in use, and
                         amount of pool allocated.
  @param Fmt             The format string
  @param ...             Variable arguments based on the format string.

  @return Allocated buffer with the formatted string printed in it.
          The caller must free the allocated buffer. The buffer
          allocation is not packed.

**/
CHAR16 *
EFIAPI
UefiDevicePathLibCatPrint (
  IN OUT POOL_PRINT   *Str,
  IN CHAR16           *Fmt,
  ...
  )
{
  UINTN   Count;
  VA_LIST Args;

  VA_START (Args, Fmt);
  Count = SPrintLength (Fmt, Args);
  VA_END(Args);

  if ((Str->Count + (Count + 1)) * sizeof (CHAR16) > Str->Capacity) {
    Str->Capacity = (Str->Count + (Count + 1) * 2) * sizeof (CHAR16);
    Str->Str = ReallocatePool (
                 Str->Count * sizeof (CHAR16),
                 Str->Capacity,
                 Str->Str
                 );
    ASSERT (Str->Str != NULL);
  }
  VA_START (Args, Fmt);
  UnicodeVSPrint (&Str->Str[Str->Count], Str->Capacity - Str->Count * sizeof (CHAR16), Fmt, Args);
  Str->Count += Count;

  VA_END (Args);
  return Str->Str;
}

/**
  Converts a PCI device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextPci (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  PCI_DEVICE_PATH *Pci;

  Pci = DevPath;
  UefiDevicePathLibCatPrint (Str, L"Pci(0x%x,0x%x)", Pci->Device, Pci->Function);
}

/**
  Converts a PC Card device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextPccard (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  PCCARD_DEVICE_PATH  *Pccard;

  Pccard = DevPath;
  UefiDevicePathLibCatPrint (Str, L"PcCard(0x%x)", Pccard->FunctionNumber);
}

/**
  Converts a Memory Map device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextMemMap (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  MEMMAP_DEVICE_PATH  *MemMap;

  MemMap = DevPath;
  UefiDevicePathLibCatPrint (
    Str,
    L"MemoryMapped(0x%x,0x%lx,0x%lx)",
    MemMap->MemoryType,
    MemMap->StartingAddress,
    MemMap->EndingAddress
    );
}

/**
  Converts a Vendor device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextVendor (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  VENDOR_DEVICE_PATH  *Vendor;
  CHAR16              *Type;
  UINTN               Index;
  UINTN               DataLength;
  UINT32              FlowControlMap;
  UINT16              Info;

  Vendor = (VENDOR_DEVICE_PATH *) DevPath;
  switch (DevicePathType (&Vendor->Header)) {
  case HARDWARE_DEVICE_PATH:
    Type = L"Hw";
    break;

  case MESSAGING_DEVICE_PATH:
    Type = L"Msg";
    if (AllowShortcuts) {
      if (CompareGuid (&Vendor->Guid, &gEfiPcAnsiGuid)) {
        UefiDevicePathLibCatPrint (Str, L"VenPcAnsi()");
        return ;
      } else if (CompareGuid (&Vendor->Guid, &gEfiVT100Guid)) {
        UefiDevicePathLibCatPrint (Str, L"VenVt100()");
        return ;
      } else if (CompareGuid (&Vendor->Guid, &gEfiVT100PlusGuid)) {
        UefiDevicePathLibCatPrint (Str, L"VenVt100Plus()");
        return ;
      } else if (CompareGuid (&Vendor->Guid, &gEfiVTUTF8Guid)) {
        UefiDevicePathLibCatPrint (Str, L"VenUtf8()");
        return ;
      } else if (CompareGuid (&Vendor->Guid, &gEfiUartDevicePathGuid)) {
        FlowControlMap = (((UART_FLOW_CONTROL_DEVICE_PATH *) Vendor)->FlowControlMap);
        switch (FlowControlMap & 0x00000003) {
        case 0:
          UefiDevicePathLibCatPrint (Str, L"UartFlowCtrl(%s)", L"None");
          break;

        case 1:
          UefiDevicePathLibCatPrint (Str, L"UartFlowCtrl(%s)", L"Hardware");
          break;

        case 2:
          UefiDevicePathLibCatPrint (Str, L"UartFlowCtrl(%s)", L"XonXoff");
          break;

        default:
          break;
        }

        return ;
      } else if (CompareGuid (&Vendor->Guid, &gEfiSasDevicePathGuid)) {
        UefiDevicePathLibCatPrint (
          Str,
          L"SAS(0x%lx,0x%lx,0x%x,",
          ((SAS_DEVICE_PATH *) Vendor)->SasAddress,
          ((SAS_DEVICE_PATH *) Vendor)->Lun,
          ((SAS_DEVICE_PATH *) Vendor)->RelativeTargetPort
          );
        Info = (((SAS_DEVICE_PATH *) Vendor)->DeviceTopology);
        if (((Info & 0x0f) == 0) && ((Info & BIT7) == 0)) {
          UefiDevicePathLibCatPrint (Str, L"NoTopology,0,0,0,");
        } else if (((Info & 0x0f) <= 2) && ((Info & BIT7) == 0)) {
          UefiDevicePathLibCatPrint (
            Str,
            L"%s,%s,%s,",
            ((Info & BIT4) != 0) ? L"SATA" : L"SAS",
            ((Info & BIT5) != 0) ? L"External" : L"Internal",
            ((Info & BIT6) != 0) ? L"Expanded" : L"Direct"
            );
          if ((Info & 0x0f) == 1) {
            UefiDevicePathLibCatPrint (Str, L"0,");
          } else {
            //
            // Value 0x0 thru 0xFF -> Drive 1 thru Drive 256
            //
            UefiDevicePathLibCatPrint (Str, L"0x%x,", ((Info >> 8) & 0xff) + 1);
          }
        } else {
          UefiDevicePathLibCatPrint (Str, L"0x%x,0,0,0,", Info);
        }

        UefiDevicePathLibCatPrint (Str, L"0x%x)", ((SAS_DEVICE_PATH *) Vendor)->Reserved);
        return ;
      } else if (CompareGuid (&Vendor->Guid, &gEfiDebugPortProtocolGuid)) {
        UefiDevicePathLibCatPrint (Str, L"DebugPort()");
        return ;
      }
    }
    break;

  case MEDIA_DEVICE_PATH:
    Type = L"Media";
    break;

  default:
    Type = L"?";
    break;
  }

  DataLength = DevicePathNodeLength (&Vendor->Header) - sizeof (VENDOR_DEVICE_PATH);
  UefiDevicePathLibCatPrint (Str, L"Ven%s(%g", Type, &Vendor->Guid);
  if (DataLength != 0) {
    UefiDevicePathLibCatPrint (Str, L",");
    for (Index = 0; Index < DataLength; Index++) {
      UefiDevicePathLibCatPrint (Str, L"%02x", ((VENDOR_DEVICE_PATH_WITH_DATA *) Vendor)->VendorDefinedData[Index]);
    }
  }

  UefiDevicePathLibCatPrint (Str, L")");
}

/**
  Converts a Controller device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextController (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  CONTROLLER_DEVICE_PATH  *Controller;

  Controller = DevPath;
  UefiDevicePathLibCatPrint (
    Str,
    L"Ctrl(0x%x)",
    Controller->ControllerNumber
    );
}

/**
  Converts a BMC device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextBmc (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  BMC_DEVICE_PATH    *Bmc;

  Bmc = DevPath;
  UefiDevicePathLibCatPrint (
    Str,
    L"BMC(0x%x,0x%lx)",
    Bmc->InterfaceType,
    ReadUnaligned64 ((UINT64 *) (&Bmc->BaseAddress))
    );
}

/**
  Converts a ACPI device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextAcpi (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  ACPI_HID_DEVICE_PATH  *Acpi;

  Acpi = DevPath;
  if ((Acpi->HID & PNP_EISA_ID_MASK) == PNP_EISA_ID_CONST) {
    switch (EISA_ID_TO_NUM (Acpi->HID)) {
    case 0x0a03:
      UefiDevicePathLibCatPrint (Str, L"PciRoot(0x%x)", Acpi->UID);
      break;

    case 0x0a08:
      UefiDevicePathLibCatPrint (Str, L"PcieRoot(0x%x)", Acpi->UID);
      break;

    case 0x0604:
      UefiDevicePathLibCatPrint (Str, L"Floppy(0x%x)", Acpi->UID);
      break;

    case 0x0301:
      UefiDevicePathLibCatPrint (Str, L"Keyboard(0x%x)", Acpi->UID);
      break;

    case 0x0501:
      UefiDevicePathLibCatPrint (Str, L"Serial(0x%x)", Acpi->UID);
      break;

    case 0x0401:
      UefiDevicePathLibCatPrint (Str, L"ParallelPort(0x%x)", Acpi->UID);
      break;

    default:
      UefiDevicePathLibCatPrint (Str, L"Acpi(PNP%04x,0x%x)", EISA_ID_TO_NUM (Acpi->HID), Acpi->UID);
      break;
    }
  } else {
    UefiDevicePathLibCatPrint (Str, L"Acpi(0x%08x,0x%x)", Acpi->HID, Acpi->UID);
  }
}

/**
  Converts a ACPI extended HID device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextAcpiEx (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  ACPI_EXTENDED_HID_DEVICE_PATH  *AcpiEx;
  CHAR8                          *HIDStr;
  CHAR8                          *UIDStr;
  CHAR8                          *CIDStr;
  CHAR16                         HIDText[11];
  CHAR16                         CIDText[11];

  AcpiEx = DevPath;
  HIDStr = (CHAR8 *) (((UINT8 *) AcpiEx) + sizeof (ACPI_EXTENDED_HID_DEVICE_PATH));
  UIDStr = HIDStr + AsciiStrLen (HIDStr) + 1;
  CIDStr = UIDStr + AsciiStrLen (UIDStr) + 1;

  if (DisplayOnly) {
    if ((EISA_ID_TO_NUM (AcpiEx->HID) == 0x0A03) ||
        (EISA_ID_TO_NUM (AcpiEx->CID) == 0x0A03 && EISA_ID_TO_NUM (AcpiEx->HID) != 0x0A08)) {
      if (AcpiEx->UID == 0) {
        UefiDevicePathLibCatPrint (Str, L"PciRoot(%a)", UIDStr);
      } else {
        UefiDevicePathLibCatPrint (Str, L"PciRoot(0x%x)", AcpiEx->UID);
      }
      return;
    }

    if (EISA_ID_TO_NUM (AcpiEx->HID) == 0x0A08 || EISA_ID_TO_NUM (AcpiEx->CID) == 0x0A08) {
      if (AcpiEx->UID == 0) {
        UefiDevicePathLibCatPrint (Str, L"PcieRoot(%a)", UIDStr);
      } else {
        UefiDevicePathLibCatPrint (Str, L"PcieRoot(0x%x)", AcpiEx->UID);
      }
      return;
    }
  }

  //
  // Converts EISA identification to string.
  //
  UnicodeSPrint (
    HIDText,
    sizeof (HIDText),
    L"%c%c%c%04X",
    ((AcpiEx->HID >> 10) & 0x1f) + 'A' - 1,
    ((AcpiEx->HID >>  5) & 0x1f) + 'A' - 1,
    ((AcpiEx->HID >>  0) & 0x1f) + 'A' - 1,
    (AcpiEx->HID >> 16) & 0xFFFF
    );
  UnicodeSPrint (
    CIDText,
    sizeof (CIDText),
    L"%c%c%c%04X",
    ((AcpiEx->CID >> 10) & 0x1f) + 'A' - 1,
    ((AcpiEx->CID >>  5) & 0x1f) + 'A' - 1,
    ((AcpiEx->CID >>  0) & 0x1f) + 'A' - 1,
    (AcpiEx->CID >> 16) & 0xFFFF
    );

  if ((*HIDStr == '\0') && (*CIDStr == '\0') && (*UIDStr != '\0')) {
    //
    // use AcpiExp()
    //
    if (AcpiEx->CID == 0) {
      UefiDevicePathLibCatPrint (
        Str,
        L"AcpiExp(%s,0,%a)",
        HIDText,
        UIDStr
       );
    } else {
      UefiDevicePathLibCatPrint (
        Str,
        L"AcpiExp(%s,%s,%a)",
        HIDText,
        CIDText,
        UIDStr
       );
    }
  } else {
    if (DisplayOnly) {
      //
      // display only
      //
      if (AcpiEx->HID == 0) {
        UefiDevicePathLibCatPrint (Str, L"AcpiEx(%a,", HIDStr);
      } else {
        UefiDevicePathLibCatPrint (Str, L"AcpiEx(%s,", HIDText);
      }

      if (AcpiEx->CID == 0) {
        UefiDevicePathLibCatPrint (Str, L"%a,", CIDStr);
      } else {
        UefiDevicePathLibCatPrint (Str, L"%s,", CIDText);
      }

      if (AcpiEx->UID == 0) {
        UefiDevicePathLibCatPrint (Str, L"%a)", UIDStr);
      } else {
        UefiDevicePathLibCatPrint (Str, L"0x%x)", AcpiEx->UID);
      }
    } else {
      UefiDevicePathLibCatPrint (
        Str,
        L"AcpiEx(%s,%s,0x%x,%a,%a,%a)",
        HIDText,
        CIDText,
        AcpiEx->UID,
        HIDStr,
        CIDStr,
        UIDStr
        );
    }
  }
}

/**
  Converts a ACPI address device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextAcpiAdr (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  ACPI_ADR_DEVICE_PATH    *AcpiAdr;
  UINT16                  Index;
  UINT16                  Length;
  UINT16                  AdditionalAdrCount;

  AcpiAdr            = DevPath;
  Length             = (UINT16) DevicePathNodeLength ((EFI_DEVICE_PATH_PROTOCOL *) AcpiAdr);
  AdditionalAdrCount = (UINT16) ((Length - 8) / 4);

  UefiDevicePathLibCatPrint (Str, L"AcpiAdr(0x%x", AcpiAdr->ADR);
  for (Index = 0; Index < AdditionalAdrCount; Index++) {
    UefiDevicePathLibCatPrint (Str, L",0x%x", *(UINT32 *) ((UINT8 *) AcpiAdr + 8 + Index * 4));
  }
  UefiDevicePathLibCatPrint (Str, L")");
}

/**
  Converts a ATAPI device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextAtapi (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  ATAPI_DEVICE_PATH *Atapi;

  Atapi = DevPath;

  if (DisplayOnly) {
    UefiDevicePathLibCatPrint (Str, L"Ata(0x%x)", Atapi->Lun);
  } else {
    UefiDevicePathLibCatPrint (
      Str,
      L"Ata(%s,%s,0x%x)",
      (Atapi->PrimarySecondary == 1) ? L"Secondary" : L"Primary",
      (Atapi->SlaveMaster == 1) ? L"Slave" : L"Master",
      Atapi->Lun
      );
  }
}

/**
  Converts a SCSI device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextScsi (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  SCSI_DEVICE_PATH  *Scsi;

  Scsi = DevPath;
  UefiDevicePathLibCatPrint (Str, L"Scsi(0x%x,0x%x)", Scsi->Pun, Scsi->Lun);
}

/**
  Converts a Fibre device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextFibre (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  FIBRECHANNEL_DEVICE_PATH  *Fibre;

  Fibre = DevPath;
  UefiDevicePathLibCatPrint (Str, L"Fibre(0x%lx,0x%lx)", Fibre->WWN, Fibre->Lun);
}

/**
  Converts a FibreEx device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextFibreEx (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  FIBRECHANNELEX_DEVICE_PATH  *FibreEx;
  UINTN                       Index;

  FibreEx = DevPath;
  UefiDevicePathLibCatPrint (Str, L"FibreEx(0x");
  for (Index = 0; Index < sizeof (FibreEx->WWN) / sizeof (FibreEx->WWN[0]); Index++) {
    UefiDevicePathLibCatPrint (Str, L"%02x", FibreEx->WWN[Index]);
  }
  UefiDevicePathLibCatPrint (Str, L",0x");
  for (Index = 0; Index < sizeof (FibreEx->Lun) / sizeof (FibreEx->Lun[0]); Index++) {
    UefiDevicePathLibCatPrint (Str, L"%02x", FibreEx->Lun[Index]);
  }
  UefiDevicePathLibCatPrint (Str, L")");
}

/**
  Converts a Sas Ex device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextSasEx (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  SASEX_DEVICE_PATH  *SasEx;
  UINTN              Index;

  SasEx = DevPath;
  UefiDevicePathLibCatPrint (Str, L"SasEx(0x");

  for (Index = 0; Index < sizeof (SasEx->SasAddress) / sizeof (SasEx->SasAddress[0]); Index++) {
    UefiDevicePathLibCatPrint (Str, L"%02x", SasEx->SasAddress[Index]);
  }
  UefiDevicePathLibCatPrint (Str, L",0x");
  for (Index = 0; Index < sizeof (SasEx->Lun) / sizeof (SasEx->Lun[0]); Index++) {
    UefiDevicePathLibCatPrint (Str, L"%02x", SasEx->Lun[Index]);
  }
  UefiDevicePathLibCatPrint (Str, L",0x%x,", SasEx->RelativeTargetPort);

  if (((SasEx->DeviceTopology & 0x0f) == 0) && ((SasEx->DeviceTopology & BIT7) == 0)) {
    UefiDevicePathLibCatPrint (Str, L"NoTopology,0,0,0");
  } else if (((SasEx->DeviceTopology & 0x0f) <= 2) && ((SasEx->DeviceTopology & BIT7) == 0)) {
    UefiDevicePathLibCatPrint (
      Str,
      L"%s,%s,%s,",
      ((SasEx->DeviceTopology & BIT4) != 0) ? L"SATA" : L"SAS",
      ((SasEx->DeviceTopology & BIT5) != 0) ? L"External" : L"Internal",
      ((SasEx->DeviceTopology & BIT6) != 0) ? L"Expanded" : L"Direct"
      );
    if ((SasEx->DeviceTopology & 0x0f) == 1) {
      UefiDevicePathLibCatPrint (Str, L"0");
    } else {
      //
      // Value 0x0 thru 0xFF -> Drive 1 thru Drive 256
      //
      UefiDevicePathLibCatPrint (Str, L"0x%x", ((SasEx->DeviceTopology >> 8) & 0xff) + 1);
    }
  } else {
    UefiDevicePathLibCatPrint (Str, L"0x%x,0,0,0", SasEx->DeviceTopology);
  }

  UefiDevicePathLibCatPrint (Str, L")");
  return ;

}

/**
  Converts a NVM Express Namespace device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextNVMe (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  NVME_NAMESPACE_DEVICE_PATH *Nvme;
  UINT8                      *Uuid;

  Nvme = DevPath;
  Uuid = (UINT8 *) &Nvme->NamespaceUuid;
  UefiDevicePathLibCatPrint (
    Str,
    L"NVMe(0x%x,%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x)",
    Nvme->NamespaceId,
    Uuid[7], Uuid[6], Uuid[5], Uuid[4],
    Uuid[3], Uuid[2], Uuid[1], Uuid[0]
    );
}

/**
  Converts a UFS device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextUfs (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  UFS_DEVICE_PATH  *Ufs;

  Ufs = DevPath;
  UefiDevicePathLibCatPrint (Str, L"UFS(0x%x,0x%x)", Ufs->Pun, Ufs->Lun);
}

/**
  Converts a SD (Secure Digital) device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextSd (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  SD_DEVICE_PATH             *Sd;

  Sd = DevPath;
  UefiDevicePathLibCatPrint (
    Str,
    L"SD(0x%x)",
    Sd->SlotNumber
    );
}

/**
  Converts a EMMC (Embedded MMC) device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextEmmc (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  EMMC_DEVICE_PATH             *Emmc;

  Emmc = DevPath;
  UefiDevicePathLibCatPrint (
    Str,
    L"eMMC(0x%x)",
    Emmc->SlotNumber
    );
}

/**
  Converts a 1394 device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToText1394 (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  F1394_DEVICE_PATH *F1394DevPath;

  F1394DevPath = DevPath;
  //
  // Guid has format of IEEE-EUI64
  //
  UefiDevicePathLibCatPrint (Str, L"I1394(%016lx)", F1394DevPath->Guid);
}

/**
  Converts a USB device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextUsb (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  USB_DEVICE_PATH *Usb;

  Usb = DevPath;
  UefiDevicePathLibCatPrint (Str, L"USB(0x%x,0x%x)", Usb->ParentPortNumber, Usb->InterfaceNumber);
}

/**
  Converts a USB WWID device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextUsbWWID (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  USB_WWID_DEVICE_PATH  *UsbWWId;
  CHAR16                *SerialNumberStr;
  CHAR16                *NewStr;
  UINT16                Length;

  UsbWWId = DevPath;

  SerialNumberStr = (CHAR16 *) ((UINT8 *) UsbWWId + sizeof (USB_WWID_DEVICE_PATH));
  Length = (UINT16) ((DevicePathNodeLength ((EFI_DEVICE_PATH_PROTOCOL *) UsbWWId) - sizeof (USB_WWID_DEVICE_PATH)) / sizeof (CHAR16));
  if (Length >= 1 && SerialNumberStr [Length - 1] != 0) {
    //
    // In case no NULL terminator in SerialNumber, create a new one with NULL terminator
    //
    NewStr = AllocateCopyPool ((Length + 1) * sizeof (CHAR16), SerialNumberStr);
    ASSERT (NewStr != NULL);
    NewStr [Length] = 0;
    SerialNumberStr = NewStr;
  }

  UefiDevicePathLibCatPrint (
    Str,
    L"UsbWwid(0x%x,0x%x,0x%x,\"%s\")",
    UsbWWId->VendorId,
    UsbWWId->ProductId,
    UsbWWId->InterfaceNumber,
    SerialNumberStr
    );
}

/**
  Converts a Logic Unit device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextLogicalUnit (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  DEVICE_LOGICAL_UNIT_DEVICE_PATH *LogicalUnit;

  LogicalUnit = DevPath;
  UefiDevicePathLibCatPrint (Str, L"Unit(0x%x)", LogicalUnit->Lun);
}

/**
  Converts a USB class device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextUsbClass (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  USB_CLASS_DEVICE_PATH *UsbClass;
  BOOLEAN               IsKnownSubClass;


  UsbClass = DevPath;

  IsKnownSubClass = TRUE;
  switch (UsbClass->DeviceClass) {
  case USB_CLASS_AUDIO:
    UefiDevicePathLibCatPrint (Str, L"UsbAudio");
    break;

  case USB_CLASS_CDCCONTROL:
    UefiDevicePathLibCatPrint (Str, L"UsbCDCControl");
    break;

  case USB_CLASS_HID:
    UefiDevicePathLibCatPrint (Str, L"UsbHID");
    break;

  case USB_CLASS_IMAGE:
    UefiDevicePathLibCatPrint (Str, L"UsbImage");
    break;

  case USB_CLASS_PRINTER:
    UefiDevicePathLibCatPrint (Str, L"UsbPrinter");
    break;

  case USB_CLASS_MASS_STORAGE:
    UefiDevicePathLibCatPrint (Str, L"UsbMassStorage");
    break;

  case USB_CLASS_HUB:
    UefiDevicePathLibCatPrint (Str, L"UsbHub");
    break;

  case USB_CLASS_CDCDATA:
    UefiDevicePathLibCatPrint (Str, L"UsbCDCData");
    break;

  case USB_CLASS_SMART_CARD:
    UefiDevicePathLibCatPrint (Str, L"UsbSmartCard");
    break;

  case USB_CLASS_VIDEO:
    UefiDevicePathLibCatPrint (Str, L"UsbVideo");
    break;

  case USB_CLASS_DIAGNOSTIC:
    UefiDevicePathLibCatPrint (Str, L"UsbDiagnostic");
    break;

  case USB_CLASS_WIRELESS:
    UefiDevicePathLibCatPrint (Str, L"UsbWireless");
    break;

  default:
    IsKnownSubClass = FALSE;
    break;
  }

  if (IsKnownSubClass) {
    UefiDevicePathLibCatPrint (
      Str,
      L"(0x%x,0x%x,0x%x,0x%x)",
      UsbClass->VendorId,
      UsbClass->ProductId,
      UsbClass->DeviceSubClass,
      UsbClass->DeviceProtocol
      );
    return;
  }

  if (UsbClass->DeviceClass == USB_CLASS_RESERVE) {
    if (UsbClass->DeviceSubClass == USB_SUBCLASS_FW_UPDATE) {
      UefiDevicePathLibCatPrint (
        Str,
        L"UsbDeviceFirmwareUpdate(0x%x,0x%x,0x%x)",
        UsbClass->VendorId,
        UsbClass->ProductId,
        UsbClass->DeviceProtocol
        );
      return;
    } else if (UsbClass->DeviceSubClass == USB_SUBCLASS_IRDA_BRIDGE) {
      UefiDevicePathLibCatPrint (
        Str,
        L"UsbIrdaBridge(0x%x,0x%x,0x%x)",
        UsbClass->VendorId,
        UsbClass->ProductId,
        UsbClass->DeviceProtocol
        );
      return;
    } else if (UsbClass->DeviceSubClass == USB_SUBCLASS_TEST) {
      UefiDevicePathLibCatPrint (
        Str,
        L"UsbTestAndMeasurement(0x%x,0x%x,0x%x)",
        UsbClass->VendorId,
        UsbClass->ProductId,
        UsbClass->DeviceProtocol
        );
      return;
    }
  }

  UefiDevicePathLibCatPrint (
    Str,
    L"UsbClass(0x%x,0x%x,0x%x,0x%x,0x%x)",
    UsbClass->VendorId,
    UsbClass->ProductId,
    UsbClass->DeviceClass,
    UsbClass->DeviceSubClass,
    UsbClass->DeviceProtocol
    );
}

/**
  Converts a SATA device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextSata (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  SATA_DEVICE_PATH *Sata;

  Sata = DevPath;
  UefiDevicePathLibCatPrint (
    Str,
    L"Sata(0x%x,0x%x,0x%x)",
    Sata->HBAPortNumber,
    Sata->PortMultiplierPortNumber,
    Sata->Lun
    );
}

/**
  Converts a I20 device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextI2O (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  I2O_DEVICE_PATH *I2ODevPath;

  I2ODevPath = DevPath;
  UefiDevicePathLibCatPrint (Str, L"I2O(0x%x)", I2ODevPath->Tid);
}

/**
  Converts a MAC address device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextMacAddr (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  MAC_ADDR_DEVICE_PATH  *MacDevPath;
  UINTN                 HwAddressSize;
  UINTN                 Index;

  MacDevPath = DevPath;

  HwAddressSize = sizeof (EFI_MAC_ADDRESS);
  if (MacDevPath->IfType == 0x01 || MacDevPath->IfType == 0x00) {
    HwAddressSize = 6;
  }

  UefiDevicePathLibCatPrint (Str, L"MAC(");

  for (Index = 0; Index < HwAddressSize; Index++) {
    UefiDevicePathLibCatPrint (Str, L"%02x", MacDevPath->MacAddress.Addr[Index]);
  }

  UefiDevicePathLibCatPrint (Str, L",0x%x)", MacDevPath->IfType);
}

/**
  Converts network protocol string to its text representation.

  @param Str             The string representative of input device.
  @param Protocol        The network protocol ID.

**/
VOID
CatNetworkProtocol (
  IN OUT POOL_PRINT  *Str,
  IN UINT16          Protocol
  )
{
  if (Protocol == RFC_1700_TCP_PROTOCOL) {
    UefiDevicePathLibCatPrint (Str, L"TCP");
  } else if (Protocol == RFC_1700_UDP_PROTOCOL) {
    UefiDevicePathLibCatPrint (Str, L"UDP");
  } else {
    UefiDevicePathLibCatPrint (Str, L"0x%x", Protocol);
  }
}

/**
  Converts IP v4 address to its text representation.

  @param Str             The string representative of input device.
  @param Address         The IP v4 address.
**/
VOID
CatIPv4Address (
  IN OUT POOL_PRINT   *Str,
  IN EFI_IPv4_ADDRESS *Address
  )
{
  UefiDevicePathLibCatPrint (Str, L"%d.%d.%d.%d", Address->Addr[0], Address->Addr[1], Address->Addr[2], Address->Addr[3]);
}

/**
  Converts IP v6 address to its text representation.

  @param Str             The string representative of input device.
  @param Address         The IP v6 address.
**/
VOID
CatIPv6Address (
  IN OUT POOL_PRINT   *Str,
  IN EFI_IPv6_ADDRESS *Address
  )
{
  UefiDevicePathLibCatPrint (
    Str, L"%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x",
    Address->Addr[0],  Address->Addr[1],
    Address->Addr[2],  Address->Addr[3],
    Address->Addr[4],  Address->Addr[5],
    Address->Addr[6],  Address->Addr[7],
    Address->Addr[8],  Address->Addr[9],
    Address->Addr[10], Address->Addr[11],
    Address->Addr[12], Address->Addr[13],
    Address->Addr[14], Address->Addr[15]
  );
}

/**
  Converts a IPv4 device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextIPv4 (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  IPv4_DEVICE_PATH  *IPDevPath;

  IPDevPath = DevPath;
  UefiDevicePathLibCatPrint (Str, L"IPv4(");
  CatIPv4Address (Str, &IPDevPath->RemoteIpAddress);

  if (DisplayOnly) {
    UefiDevicePathLibCatPrint (Str, L")");
    return ;
  }

  UefiDevicePathLibCatPrint (Str, L",");
  CatNetworkProtocol (Str, IPDevPath->Protocol);

  UefiDevicePathLibCatPrint (Str, L",%s,", IPDevPath->StaticIpAddress ? L"Static" : L"DHCP");
  CatIPv4Address (Str, &IPDevPath->LocalIpAddress);
  if (DevicePathNodeLength (IPDevPath) == sizeof (IPv4_DEVICE_PATH)) {
    UefiDevicePathLibCatPrint (Str, L",");
    CatIPv4Address (Str, &IPDevPath->GatewayIpAddress);
    UefiDevicePathLibCatPrint (Str, L",");
    CatIPv4Address (Str, &IPDevPath->SubnetMask);
  }
  UefiDevicePathLibCatPrint (Str, L")");
}

/**
  Converts a IPv6 device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextIPv6 (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  IPv6_DEVICE_PATH  *IPDevPath;

  IPDevPath = DevPath;
  UefiDevicePathLibCatPrint (Str, L"IPv6(");
  CatIPv6Address (Str, &IPDevPath->RemoteIpAddress);
  if (DisplayOnly) {
    UefiDevicePathLibCatPrint (Str, L")");
    return ;
  }

  UefiDevicePathLibCatPrint (Str, L",");
  CatNetworkProtocol (Str, IPDevPath->Protocol);

  switch (IPDevPath->IpAddressOrigin) {
    case 0:
      UefiDevicePathLibCatPrint (Str, L",Static,");
      break;
    case 1:
      UefiDevicePathLibCatPrint (Str, L",StatelessAutoConfigure,");
      break;
    default:
      UefiDevicePathLibCatPrint (Str, L",StatefulAutoConfigure,");
      break;
  }

  CatIPv6Address (Str, &IPDevPath->LocalIpAddress);

  if (DevicePathNodeLength (IPDevPath) == sizeof (IPv6_DEVICE_PATH)) {
    UefiDevicePathLibCatPrint (Str, L",0x%x,", IPDevPath->PrefixLength);
    CatIPv6Address (Str, &IPDevPath->GatewayIpAddress);
  }
  UefiDevicePathLibCatPrint (Str, L")");
}

/**
  Converts an Infini Band device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextInfiniBand (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  INFINIBAND_DEVICE_PATH  *InfiniBand;

  InfiniBand = DevPath;
  UefiDevicePathLibCatPrint (
    Str,
    L"Infiniband(0x%x,%g,0x%lx,0x%lx,0x%lx)",
    InfiniBand->ResourceFlags,
    InfiniBand->PortGid,
    InfiniBand->ServiceId,
    InfiniBand->TargetPortId,
    InfiniBand->DeviceId
    );
}

/**
  Converts a UART device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextUart (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  UART_DEVICE_PATH  *Uart;
  CHAR8             Parity;

  Uart = DevPath;
  switch (Uart->Parity) {
  case 0:
    Parity = 'D';
    break;

  case 1:
    Parity = 'N';
    break;

  case 2:
    Parity = 'E';
    break;

  case 3:
    Parity = 'O';
    break;

  case 4:
    Parity = 'M';
    break;

  case 5:
    Parity = 'S';
    break;

  default:
    Parity = 'x';
    break;
  }

  if (Uart->BaudRate == 0) {
    UefiDevicePathLibCatPrint (Str, L"Uart(DEFAULT,");
  } else {
    UefiDevicePathLibCatPrint (Str, L"Uart(%ld,", Uart->BaudRate);
  }

  if (Uart->DataBits == 0) {
    UefiDevicePathLibCatPrint (Str, L"DEFAULT,");
  } else {
    UefiDevicePathLibCatPrint (Str, L"%d,", Uart->DataBits);
  }

  UefiDevicePathLibCatPrint (Str, L"%c,", Parity);

  switch (Uart->StopBits) {
  case 0:
    UefiDevicePathLibCatPrint (Str, L"D)");
    break;

  case 1:
    UefiDevicePathLibCatPrint (Str, L"1)");
    break;

  case 2:
    UefiDevicePathLibCatPrint (Str, L"1.5)");
    break;

  case 3:
    UefiDevicePathLibCatPrint (Str, L"2)");
    break;

  default:
    UefiDevicePathLibCatPrint (Str, L"x)");
    break;
  }
}

/**
  Converts an iSCSI device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextiSCSI (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  ISCSI_DEVICE_PATH_WITH_NAME *ISCSIDevPath;
  UINT16                      Options;
  UINTN                       Index;

  ISCSIDevPath = DevPath;
  UefiDevicePathLibCatPrint (
    Str,
    L"iSCSI(%a,0x%x,0x",
    ISCSIDevPath->TargetName,
    ISCSIDevPath->TargetPortalGroupTag
    );
  for (Index = 0; Index < sizeof (ISCSIDevPath->Lun) / sizeof (UINT8); Index++) {
    UefiDevicePathLibCatPrint (Str, L"%02x", ((UINT8 *)&ISCSIDevPath->Lun)[Index]);
  }
  Options = ISCSIDevPath->LoginOption;
  UefiDevicePathLibCatPrint (Str, L",%s,", (((Options >> 1) & 0x0001) != 0) ? L"CRC32C" : L"None");
  UefiDevicePathLibCatPrint (Str, L"%s,", (((Options >> 3) & 0x0001) != 0) ? L"CRC32C" : L"None");
  if (((Options >> 11) & 0x0001) != 0) {
    UefiDevicePathLibCatPrint (Str, L"%s,", L"None");
  } else if (((Options >> 12) & 0x0001) != 0) {
    UefiDevicePathLibCatPrint (Str, L"%s,", L"CHAP_UNI");
  } else {
    UefiDevicePathLibCatPrint (Str, L"%s,", L"CHAP_BI");

  }

  UefiDevicePathLibCatPrint (Str, L"%s)", (ISCSIDevPath->NetworkProtocol == 0) ? L"TCP" : L"reserved");
}

/**
  Converts a VLAN device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextVlan (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  VLAN_DEVICE_PATH  *Vlan;

  Vlan = DevPath;
  UefiDevicePathLibCatPrint (Str, L"Vlan(%d)", Vlan->VlanId);
}

/**
  Converts a Bluetooth device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextBluetooth (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  BLUETOOTH_DEVICE_PATH  *Bluetooth;

  Bluetooth = DevPath;
  UefiDevicePathLibCatPrint (
    Str,
    L"Bluetooth(%02x%02x%02x%02x%02x%02x)",
    Bluetooth->BD_ADDR.Address[0],
    Bluetooth->BD_ADDR.Address[1],
    Bluetooth->BD_ADDR.Address[2],
    Bluetooth->BD_ADDR.Address[3],
    Bluetooth->BD_ADDR.Address[4],
    Bluetooth->BD_ADDR.Address[5]
    );
}

/**
  Converts a Wi-Fi device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextWiFi (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  WIFI_DEVICE_PATH      *WiFi;
  UINT8                 SSId[33];

  WiFi = DevPath;

  SSId[32] = '\0';
  CopyMem (SSId, WiFi->SSId, 32);

  UefiDevicePathLibCatPrint (Str, L"Wi-Fi(%a)", SSId);
}

/**
  Converts a Bluetooth device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextBluetoothLE (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  BLUETOOTH_LE_DEVICE_PATH  *BluetoothLE;

  BluetoothLE = DevPath;
  UefiDevicePathLibCatPrint (
    Str,
    L"BluetoothLE(%02x%02x%02x%02x%02x%02x,0x%02x)",
    BluetoothLE->Address.Address[0],
    BluetoothLE->Address.Address[1],
    BluetoothLE->Address.Address[2],
    BluetoothLE->Address.Address[3],
    BluetoothLE->Address.Address[4],
    BluetoothLE->Address.Address[5],
    BluetoothLE->Address.Type
    );
}

/**
  Converts a DNS device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextDns (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  DNS_DEVICE_PATH  *DnsDevPath;
  UINT32           DnsServerIpCount;
  UINT32           DnsServerIpIndex;

  DnsDevPath     = DevPath;
  DnsServerIpCount = (UINT32) (DevicePathNodeLength(DnsDevPath) - sizeof (EFI_DEVICE_PATH_PROTOCOL) - sizeof (DnsDevPath->IsIPv6)) / sizeof (EFI_IP_ADDRESS);

  UefiDevicePathLibCatPrint (Str, L"Dns(");

  for (DnsServerIpIndex = 0; DnsServerIpIndex < DnsServerIpCount; DnsServerIpIndex++) {
    if (DnsDevPath->IsIPv6 == 0x00) {
      CatIPv4Address (Str, &(DnsDevPath->DnsServerIp[DnsServerIpIndex].v4));
    } else {
      CatIPv6Address (Str, &(DnsDevPath->DnsServerIp[DnsServerIpIndex].v6));
    }

    if (DnsServerIpIndex < DnsServerIpCount - 1) {
      UefiDevicePathLibCatPrint (Str, L",");
    }
  }

  UefiDevicePathLibCatPrint (Str, L")");
}

/**
  Converts a URI device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextUri (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  URI_DEVICE_PATH    *Uri;
  UINTN              UriLength;
  CHAR8              *UriStr;

  //
  // Uri in the device path may not be null terminated.
  //
  Uri       = DevPath;
  UriLength = DevicePathNodeLength (Uri) - sizeof (URI_DEVICE_PATH);
  UriStr = AllocatePool (UriLength + 1);
  ASSERT (UriStr != NULL);

  CopyMem (UriStr, Uri->Uri, UriLength);
  UriStr[UriLength] = '\0';
  UefiDevicePathLibCatPrint (Str, L"Uri(%a)", UriStr);
  FreePool (UriStr);
}

/**
  Converts a Hard drive device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextHardDrive (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  HARDDRIVE_DEVICE_PATH *Hd;

  Hd = DevPath;
  switch (Hd->SignatureType) {
  case SIGNATURE_TYPE_MBR:
    UefiDevicePathLibCatPrint (
      Str,
      L"HD(%d,%s,0x%08x,",
      Hd->PartitionNumber,
      L"MBR",
      *((UINT32 *) (&(Hd->Signature[0])))
      );
    break;

  case SIGNATURE_TYPE_GUID:
    UefiDevicePathLibCatPrint (
      Str,
      L"HD(%d,%s,%g,",
      Hd->PartitionNumber,
      L"GPT",
      (EFI_GUID *) &(Hd->Signature[0])
      );
    break;

  default:
    UefiDevicePathLibCatPrint (
      Str,
      L"HD(%d,%d,0,",
      Hd->PartitionNumber,
      Hd->SignatureType
      );
    break;
  }

  UefiDevicePathLibCatPrint (Str, L"0x%lx,0x%lx)", Hd->PartitionStart, Hd->PartitionSize);
}

/**
  Converts a CDROM device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextCDROM (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  CDROM_DEVICE_PATH *Cd;

  Cd = DevPath;
  if (DisplayOnly) {
    UefiDevicePathLibCatPrint (Str, L"CDROM(0x%x)", Cd->BootEntry);
    return ;
  }

  UefiDevicePathLibCatPrint (Str, L"CDROM(0x%x,0x%lx,0x%lx)", Cd->BootEntry, Cd->PartitionStart, Cd->PartitionSize);
}

/**
  Converts a File device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextFilePath (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  FILEPATH_DEVICE_PATH  *Fp;

  Fp = DevPath;
  UefiDevicePathLibCatPrint (Str, L"%s", Fp->PathName);
}

/**
  Converts a Media protocol device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextMediaProtocol (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  MEDIA_PROTOCOL_DEVICE_PATH  *MediaProt;

  MediaProt = DevPath;
  UefiDevicePathLibCatPrint (Str, L"Media(%g)", &MediaProt->Protocol);
}

/**
  Converts a Firmware Volume device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextFv (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  MEDIA_FW_VOL_DEVICE_PATH  *Fv;

  Fv = DevPath;
  UefiDevicePathLibCatPrint (Str, L"Fv(%g)", &Fv->FvName);
}

/**
  Converts a Firmware Volume File device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextFvFile (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  MEDIA_FW_VOL_FILEPATH_DEVICE_PATH  *FvFile;

  FvFile = DevPath;
  UefiDevicePathLibCatPrint (Str, L"FvFile(%g)", &FvFile->FvFileName);
}

/**
  Converts a Relative Offset device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathRelativeOffsetRange (
  IN OUT POOL_PRINT       *Str,
  IN VOID                 *DevPath,
  IN BOOLEAN              DisplayOnly,
  IN BOOLEAN              AllowShortcuts
  )
{
  MEDIA_RELATIVE_OFFSET_RANGE_DEVICE_PATH *Offset;

  Offset = DevPath;
  UefiDevicePathLibCatPrint (
    Str,
    L"Offset(0x%lx,0x%lx)",
    Offset->StartingOffset,
    Offset->EndingOffset
    );
}

/**
  Converts a Ram Disk device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextRamDisk (
  IN OUT POOL_PRINT       *Str,
  IN VOID                 *DevPath,
  IN BOOLEAN              DisplayOnly,
  IN BOOLEAN              AllowShortcuts
  )
{
  MEDIA_RAM_DISK_DEVICE_PATH *RamDisk;

  RamDisk = DevPath;

  if (CompareGuid (&RamDisk->TypeGuid, &gEfiVirtualDiskGuid)) {
    UefiDevicePathLibCatPrint (
      Str,
      L"VirtualDisk(0x%lx,0x%lx,%d)",
      LShiftU64 ((UINT64)RamDisk->StartingAddr[1], 32) | RamDisk->StartingAddr[0],
      LShiftU64 ((UINT64)RamDisk->EndingAddr[1], 32) | RamDisk->EndingAddr[0],
      RamDisk->Instance
      );
  } else if (CompareGuid (&RamDisk->TypeGuid, &gEfiVirtualCdGuid)) {
    UefiDevicePathLibCatPrint (
      Str,
      L"VirtualCD(0x%lx,0x%lx,%d)",
      LShiftU64 ((UINT64)RamDisk->StartingAddr[1], 32) | RamDisk->StartingAddr[0],
      LShiftU64 ((UINT64)RamDisk->EndingAddr[1], 32) | RamDisk->EndingAddr[0],
      RamDisk->Instance
      );
  } else if (CompareGuid (&RamDisk->TypeGuid, &gEfiPersistentVirtualDiskGuid)) {
    UefiDevicePathLibCatPrint (
      Str,
      L"PersistentVirtualDisk(0x%lx,0x%lx,%d)",
      LShiftU64 ((UINT64)RamDisk->StartingAddr[1], 32) | RamDisk->StartingAddr[0],
      LShiftU64 ((UINT64)RamDisk->EndingAddr[1], 32) | RamDisk->EndingAddr[0],
      RamDisk->Instance
      );
  } else if (CompareGuid (&RamDisk->TypeGuid, &gEfiPersistentVirtualCdGuid)) {
    UefiDevicePathLibCatPrint (
      Str,
      L"PersistentVirtualCD(0x%lx,0x%lx,%d)",
      LShiftU64 ((UINT64)RamDisk->StartingAddr[1], 32) | RamDisk->StartingAddr[0],
      LShiftU64 ((UINT64)RamDisk->EndingAddr[1], 32) | RamDisk->EndingAddr[0],
      RamDisk->Instance
      );
  } else {
    UefiDevicePathLibCatPrint (
      Str,
      L"RamDisk(0x%lx,0x%lx,%d,%g)",
      LShiftU64 ((UINT64)RamDisk->StartingAddr[1], 32) | RamDisk->StartingAddr[0],
      LShiftU64 ((UINT64)RamDisk->EndingAddr[1], 32) | RamDisk->EndingAddr[0],
      RamDisk->Instance,
      &RamDisk->TypeGuid
      );
  }
}

/**
  Converts a BIOS Boot Specification device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextBBS (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  BBS_BBS_DEVICE_PATH *Bbs;
  CHAR16              *Type;

  Bbs = DevPath;
  switch (Bbs->DeviceType) {
  case BBS_TYPE_FLOPPY:
    Type = L"Floppy";
    break;

  case BBS_TYPE_HARDDRIVE:
    Type = L"HD";
    break;

  case BBS_TYPE_CDROM:
    Type = L"CDROM";
    break;

  case BBS_TYPE_PCMCIA:
    Type = L"PCMCIA";
    break;

  case BBS_TYPE_USB:
    Type = L"USB";
    break;

  case BBS_TYPE_EMBEDDED_NETWORK:
    Type = L"Network";
    break;

  default:
    Type = NULL;
    break;
  }

  if (Type != NULL) {
    UefiDevicePathLibCatPrint (Str, L"BBS(%s,%a", Type, Bbs->String);
  } else {
    UefiDevicePathLibCatPrint (Str, L"BBS(0x%x,%a", Bbs->DeviceType, Bbs->String);
  }

  if (DisplayOnly) {
    UefiDevicePathLibCatPrint (Str, L")");
    return ;
  }

  UefiDevicePathLibCatPrint (Str, L",0x%x)", Bbs->StatusFlag);
}

/**
  Converts an End-of-Device-Path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextEndInstance (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  UefiDevicePathLibCatPrint (Str, L",");
}

GLOBAL_REMOVE_IF_UNREFERENCED const DEVICE_PATH_TO_TEXT_GENERIC_TABLE mUefiDevicePathLibToTextTableGeneric[] = {
  {HARDWARE_DEVICE_PATH,  L"HardwarePath"   },
  {ACPI_DEVICE_PATH,      L"AcpiPath"       },
  {MESSAGING_DEVICE_PATH, L"Msg"            },
  {MEDIA_DEVICE_PATH,     L"MediaPath"      },
  {BBS_DEVICE_PATH,       L"BbsPath"        },
  {0, NULL}
};

/**
  Converts an unknown device path structure to its string representative.

  @param Str             The string representative of input device.
  @param DevPath         The input device path structure.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

**/
VOID
DevPathToTextNodeGeneric (
  IN OUT POOL_PRINT  *Str,
  IN VOID            *DevPath,
  IN BOOLEAN         DisplayOnly,
  IN BOOLEAN         AllowShortcuts
  )
{
  EFI_DEVICE_PATH_PROTOCOL *Node;
  UINTN                    Index;

  Node = DevPath;

  for (Index = 0; mUefiDevicePathLibToTextTableGeneric[Index].Text != NULL; Index++) {
    if (DevicePathType (Node) == mUefiDevicePathLibToTextTableGeneric[Index].Type) {
      break;
    }
  }

  if (mUefiDevicePathLibToTextTableGeneric[Index].Text == NULL) {
    //
    // It's a node whose type cannot be recognized
    //
    UefiDevicePathLibCatPrint (Str, L"Path(%d,%d", DevicePathType (Node), DevicePathSubType (Node));
  } else {
    //
    // It's a node whose type can be recognized
    //
    UefiDevicePathLibCatPrint (Str, L"%s(%d", mUefiDevicePathLibToTextTableGeneric[Index].Text, DevicePathSubType (Node));
  }

  Index = sizeof (EFI_DEVICE_PATH_PROTOCOL);
  if (Index < DevicePathNodeLength (Node)) {
    UefiDevicePathLibCatPrint (Str, L",");
    for (; Index < DevicePathNodeLength (Node); Index++) {
      UefiDevicePathLibCatPrint (Str, L"%02x", ((UINT8 *) Node)[Index]);
    }
  }

  UefiDevicePathLibCatPrint (Str, L")");
}

GLOBAL_REMOVE_IF_UNREFERENCED const DEVICE_PATH_TO_TEXT_TABLE mUefiDevicePathLibToTextTable[] = {
  {HARDWARE_DEVICE_PATH,  HW_PCI_DP,                        DevPathToTextPci            },
  {HARDWARE_DEVICE_PATH,  HW_PCCARD_DP,                     DevPathToTextPccard         },
  {HARDWARE_DEVICE_PATH,  HW_MEMMAP_DP,                     DevPathToTextMemMap         },
  {HARDWARE_DEVICE_PATH,  HW_VENDOR_DP,                     DevPathToTextVendor         },
  {HARDWARE_DEVICE_PATH,  HW_CONTROLLER_DP,                 DevPathToTextController     },
  {HARDWARE_DEVICE_PATH,  HW_BMC_DP,                        DevPathToTextBmc            },
  {ACPI_DEVICE_PATH,      ACPI_DP,                          DevPathToTextAcpi           },
  {ACPI_DEVICE_PATH,      ACPI_EXTENDED_DP,                 DevPathToTextAcpiEx         },
  {ACPI_DEVICE_PATH,      ACPI_ADR_DP,                      DevPathToTextAcpiAdr        },
  {MESSAGING_DEVICE_PATH, MSG_ATAPI_DP,                     DevPathToTextAtapi          },
  {MESSAGING_DEVICE_PATH, MSG_SCSI_DP,                      DevPathToTextScsi           },
  {MESSAGING_DEVICE_PATH, MSG_FIBRECHANNEL_DP,              DevPathToTextFibre          },
  {MESSAGING_DEVICE_PATH, MSG_FIBRECHANNELEX_DP,            DevPathToTextFibreEx        },
  {MESSAGING_DEVICE_PATH, MSG_SASEX_DP,                     DevPathToTextSasEx          },
  {MESSAGING_DEVICE_PATH, MSG_NVME_NAMESPACE_DP,            DevPathToTextNVMe           },
  {MESSAGING_DEVICE_PATH, MSG_UFS_DP,                       DevPathToTextUfs            },
  {MESSAGING_DEVICE_PATH, MSG_SD_DP,                        DevPathToTextSd             },
  {MESSAGING_DEVICE_PATH, MSG_EMMC_DP,                      DevPathToTextEmmc           },
  {MESSAGING_DEVICE_PATH, MSG_1394_DP,                      DevPathToText1394           },
  {MESSAGING_DEVICE_PATH, MSG_USB_DP,                       DevPathToTextUsb            },
  {MESSAGING_DEVICE_PATH, MSG_USB_WWID_DP,                  DevPathToTextUsbWWID        },
  {MESSAGING_DEVICE_PATH, MSG_DEVICE_LOGICAL_UNIT_DP,       DevPathToTextLogicalUnit    },
  {MESSAGING_DEVICE_PATH, MSG_USB_CLASS_DP,                 DevPathToTextUsbClass       },
  {MESSAGING_DEVICE_PATH, MSG_SATA_DP,                      DevPathToTextSata           },
  {MESSAGING_DEVICE_PATH, MSG_I2O_DP,                       DevPathToTextI2O            },
  {MESSAGING_DEVICE_PATH, MSG_MAC_ADDR_DP,                  DevPathToTextMacAddr        },
  {MESSAGING_DEVICE_PATH, MSG_IPv4_DP,                      DevPathToTextIPv4           },
  {MESSAGING_DEVICE_PATH, MSG_IPv6_DP,                      DevPathToTextIPv6           },
  {MESSAGING_DEVICE_PATH, MSG_INFINIBAND_DP,                DevPathToTextInfiniBand     },
  {MESSAGING_DEVICE_PATH, MSG_UART_DP,                      DevPathToTextUart           },
  {MESSAGING_DEVICE_PATH, MSG_VENDOR_DP,                    DevPathToTextVendor         },
  {MESSAGING_DEVICE_PATH, MSG_ISCSI_DP,                     DevPathToTextiSCSI          },
  {MESSAGING_DEVICE_PATH, MSG_VLAN_DP,                      DevPathToTextVlan           },
  {MESSAGING_DEVICE_PATH, MSG_DNS_DP,                       DevPathToTextDns            },
  {MESSAGING_DEVICE_PATH, MSG_URI_DP,                       DevPathToTextUri            },
  {MESSAGING_DEVICE_PATH, MSG_BLUETOOTH_DP,                 DevPathToTextBluetooth      },
  {MESSAGING_DEVICE_PATH, MSG_WIFI_DP,                      DevPathToTextWiFi           },
  {MESSAGING_DEVICE_PATH, MSG_BLUETOOTH_LE_DP,              DevPathToTextBluetoothLE    },
  {MEDIA_DEVICE_PATH,     MEDIA_HARDDRIVE_DP,               DevPathToTextHardDrive      },
  {MEDIA_DEVICE_PATH,     MEDIA_CDROM_DP,                   DevPathToTextCDROM          },
  {MEDIA_DEVICE_PATH,     MEDIA_VENDOR_DP,                  DevPathToTextVendor         },
  {MEDIA_DEVICE_PATH,     MEDIA_PROTOCOL_DP,                DevPathToTextMediaProtocol  },
  {MEDIA_DEVICE_PATH,     MEDIA_FILEPATH_DP,                DevPathToTextFilePath       },
  {MEDIA_DEVICE_PATH,     MEDIA_PIWG_FW_VOL_DP,             DevPathToTextFv             },
  {MEDIA_DEVICE_PATH,     MEDIA_PIWG_FW_FILE_DP,            DevPathToTextFvFile         },
  {MEDIA_DEVICE_PATH,     MEDIA_RELATIVE_OFFSET_RANGE_DP,   DevPathRelativeOffsetRange  },
  {MEDIA_DEVICE_PATH,     MEDIA_RAM_DISK_DP,                DevPathToTextRamDisk        },
  {BBS_DEVICE_PATH,       BBS_BBS_DP,                       DevPathToTextBBS            },
  {END_DEVICE_PATH_TYPE,  END_INSTANCE_DEVICE_PATH_SUBTYPE, DevPathToTextEndInstance    },
  {0, 0, NULL}
};

/**
  Converts a device node to its string representation.

  @param DeviceNode        A Pointer to the device node to be converted.
  @param DisplayOnly       If DisplayOnly is TRUE, then the shorter text representation
                           of the display node is used, where applicable. If DisplayOnly
                           is FALSE, then the longer text representation of the display node
                           is used.
  @param AllowShortcuts    If AllowShortcuts is TRUE, then the shortcut forms of text
                           representation for a device node can be used, where applicable.

  @return A pointer to the allocated text representation of the device node or NULL if DeviceNode
          is NULL or there was insufficient memory.

**/
CHAR16 *
EFIAPI
UefiDevicePathLibConvertDeviceNodeToText (
  IN CONST EFI_DEVICE_PATH_PROTOCOL  *DeviceNode,
  IN BOOLEAN                         DisplayOnly,
  IN BOOLEAN                         AllowShortcuts
  )
{
  POOL_PRINT          Str;
  UINTN               Index;
  DEVICE_PATH_TO_TEXT ToText;

  if (DeviceNode == NULL) {
    return NULL;
  }

  ZeroMem (&Str, sizeof (Str));

  //
  // Process the device path node
  // If not found, use a generic function
  //
  ToText = DevPathToTextNodeGeneric;
  for (Index = 0; mUefiDevicePathLibToTextTable[Index].Function != NULL; Index++) {
    if (DevicePathType (DeviceNode) == mUefiDevicePathLibToTextTable[Index].Type &&
        DevicePathSubType (DeviceNode) == mUefiDevicePathLibToTextTable[Index].SubType
        ) {
      ToText = mUefiDevicePathLibToTextTable[Index].Function;
      break;
    }
  }

  //
  // Print this node
  //
  ToText (&Str, (VOID *) DeviceNode, DisplayOnly, AllowShortcuts);

  ASSERT (Str.Str != NULL);
  return Str.Str;
}

/**
  Converts a device path to its text representation.

  @param DevicePath      A Pointer to the device to be converted.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

  @return A pointer to the allocated text representation of the device path or
          NULL if DeviceNode is NULL or there was insufficient memory.

**/
CHAR16 *
EFIAPI
UefiDevicePathLibConvertDevicePathToText (
  IN CONST EFI_DEVICE_PATH_PROTOCOL   *DevicePath,
  IN BOOLEAN                          DisplayOnly,
  IN BOOLEAN                          AllowShortcuts
  )
{
  POOL_PRINT               Str;
  EFI_DEVICE_PATH_PROTOCOL *Node;
  EFI_DEVICE_PATH_PROTOCOL *AlignedNode;
  UINTN                    Index;
  DEVICE_PATH_TO_TEXT      ToText;

  if (DevicePath == NULL) {
    return NULL;
  }

  ZeroMem (&Str, sizeof (Str));

  //
  // Process each device path node
  //
  Node = (EFI_DEVICE_PATH_PROTOCOL *) DevicePath;
  while (!IsDevicePathEnd (Node)) {
    //
    // Find the handler to dump this device path node
    // If not found, use a generic function
    //
    ToText = DevPathToTextNodeGeneric;
    for (Index = 0; mUefiDevicePathLibToTextTable[Index].Function != NULL; Index += 1) {

      if (DevicePathType (Node) == mUefiDevicePathLibToTextTable[Index].Type &&
          DevicePathSubType (Node) == mUefiDevicePathLibToTextTable[Index].SubType
          ) {
        ToText = mUefiDevicePathLibToTextTable[Index].Function;
        break;
      }
    }
    //
    //  Put a path separator in if needed
    //
    if ((Str.Count != 0) && (ToText != DevPathToTextEndInstance)) {
      if (Str.Str[Str.Count] != L',') {
        UefiDevicePathLibCatPrint (&Str, L"/");
      }
    }

    AlignedNode = AllocateCopyPool (DevicePathNodeLength (Node), Node);
    //
    // Print this node of the device path
    //
    ToText (&Str, AlignedNode, DisplayOnly, AllowShortcuts);
    FreePool (AlignedNode);

    //
    // Next device path node
    //
    Node = NextDevicePathNode (Node);
  }

  if (Str.Str == NULL) {
    return AllocateZeroPool (sizeof (CHAR16));
  } else {
    return Str.Str;
  }
}
