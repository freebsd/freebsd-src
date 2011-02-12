/******************************************************************************
 *
 * Module Name: aecommon - common include for the AcpiExec utility
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2011, Intel Corp.
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

#ifndef _AECOMMON
#define _AECOMMON

#ifdef _MSC_VER                 /* disable some level-4 warnings */
#pragma warning(disable:4100)   /* warning C4100: unreferenced formal parameter */
#endif

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acparser.h>
#include <contrib/dev/acpica/include/amlcode.h>
#include <contrib/dev/acpica/include/acnamesp.h>
#include <contrib/dev/acpica/include/acdebug.h>
#include <contrib/dev/acpica/include/actables.h>
#include <contrib/dev/acpica/include/acinterp.h>
#include <contrib/dev/acpica/include/acapps.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

extern FILE                 *AcpiGbl_DebugFile;
extern BOOLEAN              AcpiGbl_IgnoreErrors;
extern UINT8                AcpiGbl_RegionFillValue;

/* Check for unexpected exceptions */

#define AE_CHECK_STATUS(Name, Status, Expected) \
    if (Status != Expected) \
    { \
        AcpiOsPrintf ("Unexpected %s from %s (%s-%d)\n", \
            AcpiFormatException (Status), #Name, _AcpiModuleName, __LINE__); \
    }

/* Check for unexpected non-AE_OK errors */

#define AE_CHECK_OK(Name, Status)   AE_CHECK_STATUS (Name, Status, AE_OK);

typedef struct ae_table_desc
{
    ACPI_TABLE_HEADER       *Table;
    struct ae_table_desc    *Next;

} AE_TABLE_DESC;

/*
 * Debug Regions
 */
typedef struct ae_region
{
    ACPI_PHYSICAL_ADDRESS   Address;
    UINT32                  Length;
    void                    *Buffer;
    void                    *NextRegion;
    UINT8                   SpaceId;

} AE_REGION;

typedef struct ae_debug_regions
{
    UINT32                  NumberOfRegions;
    AE_REGION               *RegionList;

} AE_DEBUG_REGIONS;


#define TEST_OUTPUT_LEVEL(lvl)          if ((lvl) & OutputLevel)

#define OSD_PRINT(lvl,fp)               TEST_OUTPUT_LEVEL(lvl) {\
                                            AcpiOsPrintf PARAM_LIST(fp);}

void ACPI_SYSTEM_XFACE
AeCtrlCHandler (
    int                     Sig);

ACPI_STATUS
AeBuildLocalTables (
    UINT32                  TableCount,
    AE_TABLE_DESC           *TableList);

ACPI_STATUS
AeInstallTables (
    void);

void
AeDumpNamespace (
    void);

void
AeDumpObject (
    char                    *MethodName,
    ACPI_BUFFER             *ReturnObj);

void
AeDumpBuffer (
    UINT32                  Address);

void
AeExecute (
    char                    *Name);

void
AeSetScope (
    char                    *Name);

void
AeCloseDebugFile (
    void);

void
AeOpenDebugFile (
    char                    *Name);

ACPI_STATUS
AeDisplayAllMethods (
    UINT32                  DisplayCount);

ACPI_STATUS
AeInstallEarlyHandlers (
    void);

ACPI_STATUS
AeInstallLateHandlers (
    void);

void
AeMiscellaneousTests (
    void);

ACPI_STATUS
AeRegionHandler (
    UINT32                  Function,
    ACPI_PHYSICAL_ADDRESS   Address,
    UINT32                  BitWidth,
    UINT64                  *Value,
    void                    *HandlerContext,
    void                    *RegionContext);

UINT32
AeGpeHandler (
    ACPI_HANDLE             GpeDevice,
    UINT32                  GpeNumber,
    void                    *Context);

void
AeGlobalEventHandler (
    UINT32                  Type,
    ACPI_HANDLE             GpeDevice,
    UINT32                  EventNumber,
    void                    *Context);

#endif /* _AECOMMON */

