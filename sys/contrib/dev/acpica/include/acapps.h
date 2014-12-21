/******************************************************************************
 *
 * Module Name: acapps - common include for ACPI applications/tools
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2014, Intel Corp.
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

#ifndef _ACAPPS
#define _ACAPPS


#ifdef _MSC_VER                 /* disable some level-4 warnings */
#pragma warning(disable:4100)   /* warning C4100: unreferenced formal parameter */
#endif

/* Common info for tool signons */

#define ACPICA_NAME                 "Intel ACPI Component Architecture"
#define ACPICA_COPYRIGHT            "Copyright (c) 2000 - 2014 Intel Corporation"

#if ACPI_MACHINE_WIDTH == 64
#define ACPI_WIDTH          "-64"

#elif ACPI_MACHINE_WIDTH == 32
#define ACPI_WIDTH          "-32"

#else
#error unknown ACPI_MACHINE_WIDTH
#define ACPI_WIDTH          "-??"

#endif

/* Macros for signons and file headers */

#define ACPI_COMMON_SIGNON(UtilityName) \
    "\n%s\n%s version %8.8X%s\n%s\n\n", \
    ACPICA_NAME, \
    UtilityName, ((UINT32) ACPI_CA_VERSION), ACPI_WIDTH, \
    ACPICA_COPYRIGHT

#define ACPI_COMMON_HEADER(UtilityName, Prefix) \
    "%s%s\n%s%s version %8.8X%s\n%s%s\n%s\n", \
    Prefix, ACPICA_NAME, \
    Prefix, UtilityName, ((UINT32) ACPI_CA_VERSION), ACPI_WIDTH, \
    Prefix, ACPICA_COPYRIGHT, \
    Prefix

/* Macros for usage messages */

#define ACPI_USAGE_HEADER(Usage) \
    AcpiOsPrintf ("Usage: %s\nOptions:\n", Usage);

#define ACPI_USAGE_TEXT(Description) \
    AcpiOsPrintf (Description);

#define ACPI_OPTION(Name, Description) \
    AcpiOsPrintf ("  %-18s%s\n", Name, Description);


#define FILE_SUFFIX_DISASSEMBLY     "dsl"
#define ACPI_TABLE_FILE_SUFFIX      ".dat"


/*
 * getopt
 */
int
AcpiGetopt(
    int                     argc,
    char                    **argv,
    char                    *opts);

int
AcpiGetoptArgument (
    int                     argc,
    char                    **argv);

extern int                  AcpiGbl_Optind;
extern int                  AcpiGbl_Opterr;
extern int                  AcpiGbl_SubOptChar;
extern char                 *AcpiGbl_Optarg;


/*
 * cmfsize - Common get file size function
 */
UINT32
CmGetFileSize (
    ACPI_FILE               File);


#ifndef ACPI_DUMP_APP
/*
 * adisasm
 */
ACPI_STATUS
AdAmlDisassemble (
    BOOLEAN                 OutToFile,
    char                    *Filename,
    char                    *Prefix,
    char                    **OutFilename);

void
AdPrintStatistics (
    void);

ACPI_STATUS
AdFindDsdt(
    UINT8                   **DsdtPtr,
    UINT32                  *DsdtLength);

void
AdDumpTables (
    void);

ACPI_STATUS
AdGetLocalTables (
    void);

ACPI_STATUS
AdParseTable (
    ACPI_TABLE_HEADER       *Table,
    ACPI_OWNER_ID           *OwnerId,
    BOOLEAN                 LoadTable,
    BOOLEAN                 External);

ACPI_STATUS
AdDisplayTables (
    char                    *Filename,
    ACPI_TABLE_HEADER       *Table);

ACPI_STATUS
AdDisplayStatistics (
    void);


/*
 * adwalk
 */
void
AcpiDmCrossReferenceNamespace (
    ACPI_PARSE_OBJECT       *ParseTreeRoot,
    ACPI_NAMESPACE_NODE     *NamespaceRoot,
    ACPI_OWNER_ID           OwnerId);

void
AcpiDmDumpTree (
    ACPI_PARSE_OBJECT       *Origin);

void
AcpiDmFindOrphanMethods (
    ACPI_PARSE_OBJECT       *Origin);

void
AcpiDmFinishNamespaceLoad (
    ACPI_PARSE_OBJECT       *ParseTreeRoot,
    ACPI_NAMESPACE_NODE     *NamespaceRoot,
    ACPI_OWNER_ID           OwnerId);

void
AcpiDmConvertResourceIndexes (
    ACPI_PARSE_OBJECT       *ParseTreeRoot,
    ACPI_NAMESPACE_NODE     *NamespaceRoot);


/*
 * adfile
 */
ACPI_STATUS
AdInitialize (
    void);

char *
FlGenerateFilename (
    char                    *InputFilename,
    char                    *Suffix);

ACPI_STATUS
FlSplitInputPathname (
    char                    *InputPath,
    char                    **OutDirectoryPath,
    char                    **OutFilename);

char *
AdGenerateFilename (
    char                    *Prefix,
    char                    *TableId);

void
AdWriteTable (
    ACPI_TABLE_HEADER       *Table,
    UINT32                  Length,
    char                    *TableName,
    char                    *OemTableId);
#endif

#endif /* _ACAPPS */
