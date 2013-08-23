/******************************************************************************
 *
 * Name: actables.h - ACPI table management
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2013, Intel Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */

#ifndef __ACTABLES_H__
#define __ACTABLES_H__


ACPI_STATUS
AcpiAllocateRootTable (
    UINT32                  InitialTableCount);

/*
 * tbxfroot - Root pointer utilities
 */
ACPI_STATUS
AcpiTbValidateRsdp (
    ACPI_TABLE_RSDP         *Rsdp);

UINT8 *
AcpiTbScanMemoryForRsdp (
    UINT8                   *StartAddress,
    UINT32                  Length);


/*
 * tbfadt - FADT parse/convert/validate
 */
void
AcpiTbParseFadt (
    UINT32                  TableIndex);

void
AcpiTbCreateLocalFadt (
    ACPI_TABLE_HEADER       *Table,
    UINT32                  Length);


/*
 * tbfind - find ACPI table
 */
ACPI_STATUS
AcpiTbFindTable (
    char                    *Signature,
    char                    *OemId,
    char                    *OemTableId,
    UINT32                  *TableIndex);


/*
 * tbinstal - Table removal and deletion
 */
ACPI_STATUS
AcpiTbResizeRootTableList (
    void);

ACPI_STATUS
AcpiTbVerifyTable (
    ACPI_TABLE_DESC         *TableDesc);

ACPI_TABLE_HEADER *
AcpiTbTableOverride (
    ACPI_TABLE_HEADER       *TableHeader,
    ACPI_TABLE_DESC         *TableDesc);

ACPI_STATUS
AcpiTbAddTable (
    ACPI_TABLE_DESC         *TableDesc,
    UINT32                  *TableIndex);

ACPI_STATUS
AcpiTbStoreTable (
    ACPI_PHYSICAL_ADDRESS   Address,
    ACPI_TABLE_HEADER       *Table,
    UINT32                  Length,
    UINT8                   Flags,
    UINT32                  *TableIndex);

void
AcpiTbDeleteTable (
    ACPI_TABLE_DESC        *TableDesc);

void
AcpiTbTerminate (
    void);

ACPI_STATUS
AcpiTbDeleteNamespaceByOwner (
    UINT32                  TableIndex);

ACPI_STATUS
AcpiTbAllocateOwnerId (
    UINT32                  TableIndex);

ACPI_STATUS
AcpiTbReleaseOwnerId (
    UINT32                  TableIndex);

ACPI_STATUS
AcpiTbGetOwnerId (
    UINT32                  TableIndex,
    ACPI_OWNER_ID           *OwnerId);

BOOLEAN
AcpiTbIsTableLoaded (
    UINT32                  TableIndex);

void
AcpiTbSetTableLoadedFlag (
    UINT32                  TableIndex,
    BOOLEAN                 IsLoaded);


/*
 * tbutils - table manager utilities
 */
ACPI_STATUS
AcpiTbInitializeFacs (
    void);

BOOLEAN
AcpiTbTablesLoaded (
    void);

void
AcpiTbPrintTableHeader(
    ACPI_PHYSICAL_ADDRESS   Address,
    ACPI_TABLE_HEADER       *Header);

UINT8
AcpiTbChecksum (
    UINT8                   *Buffer,
    UINT32                  Length);

ACPI_STATUS
AcpiTbVerifyChecksum (
    ACPI_TABLE_HEADER       *Table,
    UINT32                  Length);

void
AcpiTbCheckDsdtHeader (
    void);

ACPI_TABLE_HEADER *
AcpiTbCopyDsdt (
    UINT32                  TableIndex);

void
AcpiTbInstallTable (
    ACPI_PHYSICAL_ADDRESS   Address,
    char                    *Signature,
    UINT32                  TableIndex);

ACPI_STATUS
AcpiTbParseRootTable (
    ACPI_PHYSICAL_ADDRESS   RsdpAddress);

#endif /* __ACTABLES_H__ */
