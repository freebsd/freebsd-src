/** @file
  The file provides the protocol to install or remove an ACPI
  table from a platform.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol was introduced in UEFI Specification 2.3.

**/

#ifndef __ACPI_TABLE_H___
#define __ACPI_TABLE_H___

#define EFI_ACPI_TABLE_PROTOCOL_GUID \
  { 0xffe06bdd, 0x6107, 0x46a6, { 0x7b, 0xb2, 0x5a, 0x9c, 0x7e, 0xc5, 0x27, 0x5c }}

typedef struct _EFI_ACPI_TABLE_PROTOCOL EFI_ACPI_TABLE_PROTOCOL;

/**

  The InstallAcpiTable() function allows a caller to install an
  ACPI table. When successful, the table will be linked by the
  RSDT/XSDT. AcpiTableBuffer specifies the table to be installed.
  InstallAcpiTable() will make a copy of the table and insert the
  copy into the RSDT/XSDT. InstallAcpiTable() must insert the new
  table at the end of the RSDT/XSDT. To prevent namespace
  collision, ACPI tables may be created using UEFI ACPI table
  format. If this protocol is used to install a table with a
  signature already present in the system, the new table will not
  replace the existing table. It is a platform implementation
  decision to add a new table with a signature matching an
  existing table or disallow duplicate table signatures and
  return EFI_ACCESS_DENIED. On successful output, TableKey is
  initialized with a unique key. Its value may be used in a
  subsequent call to UninstallAcpiTable to remove an ACPI table.
  If an EFI application is running at the time of this call, the
  relevant EFI_CONFIGURATION_TABLE pointer to the RSDT is no
  longer considered valid.


  @param This                 A pointer to a EFI_ACPI_TABLE_PROTOCOL.

  @param AcpiTableBuffer      A pointer to a buffer containing the
                              ACPI table to be installed.

  @param AcpiTableBufferSize  Specifies the size, in bytes, of
                              the AcpiTableBuffer buffer.


  @param TableKey             Returns a key to refer to the ACPI table.

  @retval EFI_SUCCESS           The table was successfully inserted

  @retval EFI_INVALID_PARAMETER Either AcpiTableBuffer is NULL,
                                TableKey is NULL, or
                                AcpiTableBufferSize and the size
                                field embedded in the ACPI table
                                pointed to by AcpiTableBuffer
                                are not in sync.

  @retval EFI_OUT_OF_RESOURCES  Insufficient resources exist to
                                complete the request.
  @retval EFI_ACCESS_DENIED     The table signature matches a table already
                                present in the system and platform policy
                                does not allow duplicate tables of this type.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_ACPI_TABLE_INSTALL_ACPI_TABLE)(
  IN   EFI_ACPI_TABLE_PROTOCOL       *This,
  IN   VOID                          *AcpiTableBuffer,
  IN   UINTN                         AcpiTableBufferSize,
  OUT  UINTN                         *TableKey
  );

/**

  The UninstallAcpiTable() function allows a caller to remove an
  ACPI table. The routine will remove its reference from the
  RSDT/XSDT. A table is referenced by the TableKey parameter
  returned from a prior call to InstallAcpiTable(). If an EFI
  application is running at the time of this call, the relevant
  EFI_CONFIGURATION_TABLE pointer to the RSDT is no longer
  considered valid.

  @param This                   A pointer to a EFI_ACPI_TABLE_PROTOCOL.

  @param TableKey               Specifies the table to uninstall. The key was
                                returned from InstallAcpiTable().

  @retval EFI_SUCCESS           The table was successfully inserted

  @retval EFI_NOT_FOUND         TableKey does not refer to a valid key
                                for a table entry.

  @retval EFI_OUT_OF_RESOURCES  Insufficient resources exist to
                                complete the request.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_ACPI_TABLE_UNINSTALL_ACPI_TABLE)(
  IN  EFI_ACPI_TABLE_PROTOCOL       *This,
  IN  UINTN                         TableKey
  );

///
/// The EFI_ACPI_TABLE_PROTOCOL provides the ability for a component
/// to install and uninstall ACPI tables from a platform.
///
struct _EFI_ACPI_TABLE_PROTOCOL {
  EFI_ACPI_TABLE_INSTALL_ACPI_TABLE      InstallAcpiTable;
  EFI_ACPI_TABLE_UNINSTALL_ACPI_TABLE    UninstallAcpiTable;
};

extern EFI_GUID  gEfiAcpiTableProtocolGuid;

#endif
