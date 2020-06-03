/** @file
  This module provides help function for finding ACPI table.

  Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "UefiLibInternal.h"
#include <IndustryStandard/Acpi.h>
#include <Guid/Acpi.h>

/**
  This function scans ACPI table in XSDT/RSDT.

  @param Sdt                    ACPI XSDT/RSDT.
  @param TablePointerSize       Size of table pointer: 8(XSDT) or 4(RSDT).
  @param Signature              ACPI table signature.
  @param PreviousTable          Pointer to previous returned table to locate
                                next table, or NULL to locate first table.
  @param PreviousTableLocated   Pointer to the indicator about whether the
                                previous returned table could be located, or
                                NULL if PreviousTable is NULL.

  If PreviousTable is NULL and PreviousTableLocated is not NULL, then ASSERT().
  If PreviousTable is not NULL and PreviousTableLocated is NULL, then ASSERT().

  @return ACPI table or NULL if not found.

**/
EFI_ACPI_COMMON_HEADER *
ScanTableInSDT (
  IN  EFI_ACPI_DESCRIPTION_HEADER   *Sdt,
  IN  UINTN                         TablePointerSize,
  IN  UINT32                        Signature,
  IN  EFI_ACPI_COMMON_HEADER        *PreviousTable, OPTIONAL
  OUT BOOLEAN                       *PreviousTableLocated OPTIONAL
  )
{
  UINTN                             Index;
  UINTN                             EntryCount;
  UINT64                            EntryPtr;
  UINTN                             BasePtr;
  EFI_ACPI_COMMON_HEADER            *Table;

  if (PreviousTableLocated != NULL) {
    ASSERT (PreviousTable != NULL);
    *PreviousTableLocated = FALSE;
  } else {
    ASSERT (PreviousTable == NULL);
  }

  if (Sdt == NULL) {
    return NULL;
  }

  EntryCount = (Sdt->Length - sizeof (EFI_ACPI_DESCRIPTION_HEADER)) / TablePointerSize;

  BasePtr = (UINTN)(Sdt + 1);
  for (Index = 0; Index < EntryCount; Index ++) {
    EntryPtr = 0;
    CopyMem (&EntryPtr, (VOID *)(BasePtr + Index * TablePointerSize), TablePointerSize);
    Table = (EFI_ACPI_COMMON_HEADER *)((UINTN)(EntryPtr));
    if ((Table != NULL) && (Table->Signature == Signature)) {
      if (PreviousTable != NULL) {
        if (Table == PreviousTable) {
          *PreviousTableLocated = TRUE;
        } else if (*PreviousTableLocated) {
          //
          // Return next table.
          //
          return Table;
        }
      } else {
        //
        // Return first table.
        //
        return Table;
      }

    }
  }

  return NULL;
}

/**
  To locate FACS in FADT.

  @param Fadt   FADT table pointer.

  @return FACS table pointer or NULL if not found.

**/
EFI_ACPI_COMMON_HEADER *
LocateAcpiFacsFromFadt (
  IN EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE  *Fadt
  )
{
  EFI_ACPI_COMMON_HEADER                        *Facs;
  UINT64                                        Data64;

  if (Fadt == NULL) {
    return NULL;
  }

  if (Fadt->Header.Revision < EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE_REVISION) {
    Facs = (EFI_ACPI_COMMON_HEADER *)(UINTN)Fadt->FirmwareCtrl;
  } else {
    CopyMem (&Data64, &Fadt->XFirmwareCtrl, sizeof(UINT64));
    if (Data64 != 0) {
      Facs = (EFI_ACPI_COMMON_HEADER *)(UINTN)Data64;
    } else {
      Facs = (EFI_ACPI_COMMON_HEADER *)(UINTN)Fadt->FirmwareCtrl;
    }
  }
  return Facs;
}

/**
  To locate DSDT in FADT.

  @param Fadt   FADT table pointer.

  @return DSDT table pointer or NULL if not found.

**/
EFI_ACPI_COMMON_HEADER *
LocateAcpiDsdtFromFadt (
  IN EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE  *Fadt
  )
{
  EFI_ACPI_COMMON_HEADER                        *Dsdt;
  UINT64                                        Data64;

  if (Fadt == NULL) {
    return NULL;
  }

  if (Fadt->Header.Revision < EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE_REVISION) {
    Dsdt = (EFI_ACPI_COMMON_HEADER *)(UINTN)Fadt->Dsdt;
  } else {
    CopyMem (&Data64, &Fadt->XDsdt, sizeof(UINT64));
    if (Data64 != 0) {
      Dsdt = (EFI_ACPI_COMMON_HEADER *)(UINTN)Data64;
    } else {
      Dsdt = (EFI_ACPI_COMMON_HEADER *)(UINTN)Fadt->Dsdt;
    }
  }
  return Dsdt;
}

/**
  To locate ACPI table in ACPI ConfigurationTable.

  @param AcpiGuid               The GUID used to get ACPI ConfigurationTable.
  @param Signature              ACPI table signature.
  @param PreviousTable          Pointer to previous returned table to locate
                                next table, or NULL to locate first table.
  @param PreviousTableLocated   Pointer to the indicator to return whether the
                                previous returned table could be located or not,
                                or NULL if PreviousTable is NULL.

  If PreviousTable is NULL and PreviousTableLocated is not NULL, then ASSERT().
  If PreviousTable is not NULL and PreviousTableLocated is NULL, then ASSERT().
  If AcpiGuid is NULL, then ASSERT().

  @return ACPI table or NULL if not found.

**/
EFI_ACPI_COMMON_HEADER *
LocateAcpiTableInAcpiConfigurationTable (
  IN  EFI_GUID                  *AcpiGuid,
  IN  UINT32                    Signature,
  IN  EFI_ACPI_COMMON_HEADER    *PreviousTable, OPTIONAL
  OUT BOOLEAN                   *PreviousTableLocated OPTIONAL
  )
{
  EFI_STATUS                                    Status;
  EFI_ACPI_COMMON_HEADER                        *Table;
  EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER  *Rsdp;
  EFI_ACPI_DESCRIPTION_HEADER                   *Rsdt;
  EFI_ACPI_DESCRIPTION_HEADER                   *Xsdt;
  EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE     *Fadt;

  if (PreviousTableLocated != NULL) {
    ASSERT (PreviousTable != NULL);
    *PreviousTableLocated = FALSE;
  } else {
    ASSERT (PreviousTable == NULL);
  }

  Rsdp = NULL;
  //
  // Get ACPI ConfigurationTable (RSD_PTR)
  //
  Status = EfiGetSystemConfigurationTable(AcpiGuid, (VOID **)&Rsdp);
  if (EFI_ERROR (Status) || (Rsdp == NULL)) {
    return NULL;
  }

  Table = NULL;

  //
  // Search XSDT
  //
  if (Rsdp->Revision >= EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER_REVISION) {
    Xsdt = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN) Rsdp->XsdtAddress;
    if (Signature == EFI_ACPI_2_0_DIFFERENTIATED_SYSTEM_DESCRIPTION_TABLE_SIGNATURE) {
      ASSERT (PreviousTable == NULL);
      //
      // It is to locate DSDT,
      // need to locate FADT first.
      //
      Fadt = (EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE *) ScanTableInSDT (
               Xsdt,
               sizeof (UINT64),
               EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE_SIGNATURE,
               NULL,
               NULL
               );
      Table = LocateAcpiDsdtFromFadt (Fadt);
    } else if (Signature == EFI_ACPI_2_0_FIRMWARE_ACPI_CONTROL_STRUCTURE_SIGNATURE) {
      ASSERT (PreviousTable == NULL);
      //
      // It is to locate FACS,
      // need to locate FADT first.
      //
      Fadt = (EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE *) ScanTableInSDT (
               Xsdt,
               sizeof (UINT64),
               EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE_SIGNATURE,
               NULL,
               NULL
               );
      Table = LocateAcpiFacsFromFadt (Fadt);
    } else {
      Table = ScanTableInSDT (
                Xsdt,
                sizeof (UINT64),
                Signature,
                PreviousTable,
                PreviousTableLocated
                );
    }
  }

  if (Table != NULL) {
    return Table;
  } else if ((PreviousTableLocated != NULL) &&
              *PreviousTableLocated) {
    //
    // PreviousTable could be located in XSDT,
    // but next table could not be located in XSDT.
    //
    return NULL;
  }

  //
  // Search RSDT
  //
  Rsdt = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN) Rsdp->RsdtAddress;
  if (Signature == EFI_ACPI_2_0_DIFFERENTIATED_SYSTEM_DESCRIPTION_TABLE_SIGNATURE) {
    ASSERT (PreviousTable == NULL);
    //
    // It is to locate DSDT,
    // need to locate FADT first.
    //
    Fadt = (EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE *) ScanTableInSDT (
             Rsdt,
             sizeof (UINT32),
             EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE_SIGNATURE,
             NULL,
             NULL
             );
    Table = LocateAcpiDsdtFromFadt (Fadt);
  } else if (Signature == EFI_ACPI_2_0_FIRMWARE_ACPI_CONTROL_STRUCTURE_SIGNATURE) {
    ASSERT (PreviousTable == NULL);
    //
    // It is to locate FACS,
    // need to locate FADT first.
    //
    Fadt = (EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE *) ScanTableInSDT (
             Rsdt,
             sizeof (UINT32),
             EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE_SIGNATURE,
             NULL,
             NULL
             );
    Table = LocateAcpiFacsFromFadt (Fadt);
  } else {
    Table = ScanTableInSDT (
              Rsdt,
              sizeof (UINT32),
              Signature,
              PreviousTable,
              PreviousTableLocated
              );
  }

  return Table;
}

/**
  This function locates next ACPI table in XSDT/RSDT based on Signature and
  previous returned Table.

  If PreviousTable is NULL:
  This function will locate the first ACPI table in XSDT/RSDT based on
  Signature in gEfiAcpi20TableGuid system configuration table first, and then
  gEfiAcpi10TableGuid system configuration table.
  This function will locate in XSDT first, and then RSDT.
  For DSDT, this function will locate XDsdt in FADT first, and then Dsdt in
  FADT.
  For FACS, this function will locate XFirmwareCtrl in FADT first, and then
  FirmwareCtrl in FADT.

  If PreviousTable is not NULL:
  1. If it could be located in XSDT in gEfiAcpi20TableGuid system configuration
     table, then this function will just locate next table in XSDT in
     gEfiAcpi20TableGuid system configuration table.
  2. If it could be located in RSDT in gEfiAcpi20TableGuid system configuration
     table, then this function will just locate next table in RSDT in
     gEfiAcpi20TableGuid system configuration table.
  3. If it could be located in RSDT in gEfiAcpi10TableGuid system configuration
     table, then this function will just locate next table in RSDT in
     gEfiAcpi10TableGuid system configuration table.

  It's not supported that PreviousTable is not NULL but PreviousTable->Signature
  is not same with Signature, NULL will be returned.

  @param Signature          ACPI table signature.
  @param PreviousTable      Pointer to previous returned table to locate next
                            table, or NULL to locate first table.

  @return Next ACPI table or NULL if not found.

**/
EFI_ACPI_COMMON_HEADER *
EFIAPI
EfiLocateNextAcpiTable (
  IN UINT32                     Signature,
  IN EFI_ACPI_COMMON_HEADER     *PreviousTable OPTIONAL
  )
{
  EFI_ACPI_COMMON_HEADER        *Table;
  BOOLEAN                       TempPreviousTableLocated;
  BOOLEAN                       *PreviousTableLocated;

  if (PreviousTable != NULL) {
    if (PreviousTable->Signature != Signature) {
      //
      // PreviousTable->Signature is not same with Signature.
      //
      return NULL;
    } else if ((Signature == EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE_SIGNATURE) ||
               (Signature == EFI_ACPI_2_0_DIFFERENTIATED_SYSTEM_DESCRIPTION_TABLE_SIGNATURE) ||
               (Signature == EFI_ACPI_2_0_FIRMWARE_ACPI_CONTROL_STRUCTURE_SIGNATURE)) {
      //
      // There is only one FADT/DSDT/FACS table,
      // so don't try to locate next one.
      //
      return NULL;
    }

    PreviousTableLocated = &TempPreviousTableLocated;
    *PreviousTableLocated = FALSE;
  } else {
    PreviousTableLocated = NULL;
  }

  Table = LocateAcpiTableInAcpiConfigurationTable (
            &gEfiAcpi20TableGuid,
            Signature,
            PreviousTable,
            PreviousTableLocated
            );
  if (Table != NULL) {
    return Table;
  } else if ((PreviousTableLocated != NULL) &&
              *PreviousTableLocated) {
    //
    // PreviousTable could be located in gEfiAcpi20TableGuid system
    // configuration table, but next table could not be located in
    // gEfiAcpi20TableGuid system configuration table.
    //
    return NULL;
  }

  return LocateAcpiTableInAcpiConfigurationTable (
           &gEfiAcpi10TableGuid,
           Signature,
           PreviousTable,
           PreviousTableLocated
           );
}

/**
  This function locates first ACPI table in XSDT/RSDT based on Signature.

  This function will locate the first ACPI table in XSDT/RSDT based on
  Signature in gEfiAcpi20TableGuid system configuration table first, and then
  gEfiAcpi10TableGuid system configuration table.
  This function will locate in XSDT first, and then RSDT.
  For DSDT, this function will locate XDsdt in FADT first, and then Dsdt in
  FADT.
  For FACS, this function will locate XFirmwareCtrl in FADT first, and then
  FirmwareCtrl in FADT.

  @param Signature          ACPI table signature.

  @return First ACPI table or NULL if not found.

**/
EFI_ACPI_COMMON_HEADER *
EFIAPI
EfiLocateFirstAcpiTable (
  IN UINT32                     Signature
  )
{
  return EfiLocateNextAcpiTable (Signature, NULL);
}
