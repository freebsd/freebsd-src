/******************************************************************************
 *
 * Module Name: acpidump.h - Include file for AcpiDump utility
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2017, Intel Corp.
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

/*
 * Global variables. Defined in main.c only, externed in all other files
 */
#ifdef _DECLARE_GLOBALS
#define EXTERN
#define INIT_GLOBAL(a,b)        a=b
#else
#define EXTERN                  extern
#define INIT_GLOBAL(a,b)        a
#endif

#include "acpi.h"
#include "accommon.h"
#include "actables.h"
#include "acapps.h"

/* Globals */

EXTERN BOOLEAN              INIT_GLOBAL (Gbl_SummaryMode, FALSE);
EXTERN BOOLEAN              INIT_GLOBAL (Gbl_VerboseMode, FALSE);
EXTERN BOOLEAN              INIT_GLOBAL (Gbl_BinaryMode, FALSE);
EXTERN BOOLEAN              INIT_GLOBAL (Gbl_DumpCustomizedTables, TRUE);
EXTERN BOOLEAN              INIT_GLOBAL (Gbl_DoNotDumpXsdt, FALSE);
EXTERN ACPI_FILE            INIT_GLOBAL (Gbl_OutputFile, NULL);
EXTERN char                 INIT_GLOBAL (*Gbl_OutputFilename, NULL);
EXTERN UINT64               INIT_GLOBAL (Gbl_RsdpBase, 0);

/* Action table used to defer requested options */

typedef struct ap_dump_action
{
    char                    *Argument;
    UINT32                  ToBeDone;

} AP_DUMP_ACTION;

#define AP_MAX_ACTIONS              32

#define AP_DUMP_ALL_TABLES          0
#define AP_DUMP_TABLE_BY_ADDRESS    1
#define AP_DUMP_TABLE_BY_NAME       2
#define AP_DUMP_TABLE_BY_FILE       3

#define AP_MAX_ACPI_FILES           256 /* Prevent infinite loops */

/* Minimum FADT sizes for various table addresses */

#define MIN_FADT_FOR_DSDT           (ACPI_FADT_OFFSET (Dsdt) + sizeof (UINT32))
#define MIN_FADT_FOR_FACS           (ACPI_FADT_OFFSET (Facs) + sizeof (UINT32))
#define MIN_FADT_FOR_XDSDT          (ACPI_FADT_OFFSET (XDsdt) + sizeof (UINT64))
#define MIN_FADT_FOR_XFACS          (ACPI_FADT_OFFSET (XFacs) + sizeof (UINT64))


/*
 * apdump - Table get/dump routines
 */
int
ApDumpTableFromFile (
    char                    *Pathname);

int
ApDumpTableByName (
    char                    *Signature);

int
ApDumpTableByAddress (
    char                    *AsciiAddress);

int
ApDumpAllTables (
    void);

BOOLEAN
ApIsValidHeader (
    ACPI_TABLE_HEADER       *Table);

BOOLEAN
ApIsValidChecksum (
    ACPI_TABLE_HEADER       *Table);

UINT32
ApGetTableLength (
    ACPI_TABLE_HEADER       *Table);


/*
 * apfiles - File I/O utilities
 */
int
ApOpenOutputFile (
    char                    *Pathname);

int
ApWriteToBinaryFile (
    ACPI_TABLE_HEADER       *Table,
    UINT32                  Instance);

ACPI_TABLE_HEADER *
ApGetTableFromFile (
    char                    *Pathname,
    UINT32                  *FileSize);
