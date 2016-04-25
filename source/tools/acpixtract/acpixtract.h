/******************************************************************************
 *
 * Module Name: acpixtract.h - Include for acpixtract utility
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2016, Intel Corp.
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

#include "acpi.h"
#include "accommon.h"
#include "acapps.h"
#include <stdio.h>


#undef ACPI_GLOBAL

#ifdef DEFINE_ACPIXTRACT_GLOBALS
#define ACPI_GLOBAL(type,name) \
    extern type name; \
    type name

#else
#define ACPI_GLOBAL(type,name) \
    extern type name
#endif


/* Options */

#define AX_EXTRACT_ALL              0
#define AX_LIST_ALL                 1
#define AX_EXTRACT_SIGNATURE        2
#define AX_EXTRACT_AML_TABLES       3
#define AX_EXTRACT_MULTI_TABLE      4

#define AX_OPTIONAL_TABLES          0
#define AX_REQUIRED_TABLE           1

#define AX_UTILITY_NAME             "ACPI Binary Table Extraction Utility"
#define AX_SUPPORTED_OPTIONS        "ahlms:v"
#define AX_MULTI_TABLE_FILENAME     "amltables.dat"
#define AX_TABLE_INFO_FORMAT        "Acpi table [%4.4s] - %7u bytes written to %s\n"

/* Extraction states */

#define AX_STATE_FIND_HEADER        0
#define AX_STATE_EXTRACT_DATA       1

/* Miscellaneous constants */

#define AX_LINE_BUFFER_SIZE         256
#define AX_MIN_BLOCK_HEADER_LENGTH  6   /* strlen ("DSDT @") */


typedef struct AxTableInfo
{
    UINT32                  Signature;
    unsigned int            Instances;
    unsigned int            NextInstance;
    struct AxTableInfo      *Next;

} AX_TABLE_INFO;


/* Globals */

ACPI_GLOBAL (char,           Gbl_LineBuffer[AX_LINE_BUFFER_SIZE]);
ACPI_GLOBAL (char,           Gbl_HeaderBuffer[AX_LINE_BUFFER_SIZE]);
ACPI_GLOBAL (char,           Gbl_InstanceBuffer[AX_LINE_BUFFER_SIZE]);

ACPI_GLOBAL (AX_TABLE_INFO, *Gbl_TableListHead);
ACPI_GLOBAL (char,           Gbl_OutputFilename[32]);
ACPI_GLOBAL (unsigned char,  Gbl_BinaryData[16]);
ACPI_GLOBAL (unsigned int,   Gbl_TableCount);

/*
 * acpixtract.c
 */
int
AxExtractTables (
    char                    *InputPathname,
    char                    *Signature,
    unsigned int            MinimumInstances);

int
AxExtractToMultiAmlFile (
    char                    *InputPathname);

int
AxListTables (
    char                    *InputPathname);


/*
 * axutils.c
 */
size_t
AxGetTableHeader (
    FILE                    *InputFile,
    unsigned char           *OutputData);

unsigned int
AxCountTableInstances (
    char                    *InputPathname,
    char                    *Signature);

unsigned int
AxGetNextInstance (
    char                    *InputPathname,
    char                    *Signature);

void
AxNormalizeSignature (
    char                    *Signature);

void
AxCheckAscii (
    char                    *Name,
    int                     Count);

int
AxIsEmptyLine (
    char                    *Buffer);

int
AxIsDataBlockHeader (
    void);

long
AxProcessOneTextLine (
    FILE                    *OutputFile,
    char                    *ThisSignature,
    unsigned int            ThisTableBytesWritten);

size_t
AxConvertLine (
    char                    *InputLine,
    unsigned char           *OutputData);
