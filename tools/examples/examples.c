/******************************************************************************
 *
 * Module Name: examples - Example ACPICA code
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2010, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights.  You may have additional license terms from the party that provided
 * you this software, covering your right to use that party's intellectual
 * property rights.
 *
 * 2.2. Intel grants, free of charge, to any person ("Licensee") obtaining a
 * copy of the source code appearing in this file ("Covered Code") an
 * irrevocable, perpetual, worldwide license under Intel's copyrights in the
 * base code distributed originally by Intel ("Original Intel Code") to copy,
 * make derivatives, distribute, use and display any portion of the Covered
 * Code in any form, with the right to sublicense such rights; and
 *
 * 2.3. Intel grants Licensee a non-exclusive and non-transferable patent
 * license (with the right to sublicense), under only those claims of Intel
 * patents that are infringed by the Original Intel Code, to make, use, sell,
 * offer to sell, and import the Covered Code and derivative works thereof
 * solely to the minimum extent necessary to exercise the above copyright
 * license, and in no event shall the patent license extend to any additions
 * to or modifications of the Original Intel Code.  No other license or right
 * is granted directly or by implication, estoppel or otherwise;
 *
 * The above copyright and patent license is granted only if the following
 * conditions are met:
 *
 * 3. Conditions
 *
 * 3.1. Redistribution of Source with Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification with rights to further distribute source must include
 * the above Copyright Notice, the above License, this list of Conditions,
 * and the following Disclaimer and Export Compliance provision.  In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change.  Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee.  Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution.  In
 * addition, Licensee may not authorize further sublicense of source of any
 * portion of the Covered Code, and must include terms to the effect that the
 * license from Licensee to its licensee is limited to the intellectual
 * property embodied in the software Licensee provides to its licensee, and
 * not to intellectual property embodied in modifications its licensee may
 * make.
 *
 * 3.3. Redistribution of Executable. Redistribution in executable form of any
 * substantial portion of the Covered Code or modification must reproduce the
 * above Copyright Notice, and the following Disclaimer and Export Compliance
 * provision in the documentation and/or other materials provided with the
 * distribution.
 *
 * 3.4. Intel retains all right, title, and interest in and to the Original
 * Intel Code.
 *
 * 3.5. Neither the name Intel nor any other trademark owned or controlled by
 * Intel shall be used in advertising or otherwise to promote the sale, use or
 * other dealings in products derived from or relating to the Covered Code
 * without prior written authorization from Intel.
 *
 * 4. Disclaimer and Export Compliance
 *
 * 4.1. INTEL MAKES NO WARRANTY OF ANY KIND REGARDING ANY SOFTWARE PROVIDED
 * HERE.  ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT,  ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES.  INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS.  INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.  THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government.  In the
 * event Licensee exports any such software from the United States or
 * re-exports any such software from a foreign destination, Licensee shall
 * ensure that the distribution and export/re-export of the software is in
 * compliance with all laws, regulations, orders, or other restrictions of the
 * U.S. Export Administration Regulations. Licensee agrees that neither it nor
 * any of its subsidiaries will export/re-export any technical data, process,
 * software, or service, directly or indirectly, to any country for which the
 * United States government or any agency thereof requires an export license,
 * other governmental approval, or letter of assurance, without first obtaining
 * such license, approval or letter.
 *
 *****************************************************************************/


/* Set the ACPICA application type for use in include/platform/acenv.h */

#ifndef WIN32
#define WIN32
#endif

#define ACPI_DEBUG_OUTPUT

/* ACPICA public headers */

#include "acpi.h"

#define _COMPONENT          ACPI_EXAMPLE
        ACPI_MODULE_NAME    ("examples")


/******************************************************************************
 *
 * ACPICA Example Code
 *
 * This module contains examples of how the host OS should interface to the
 * ACPICA subsystem.
 *
 * 1) How to use the platform/acenv.h file and how to set configuration
 *      options.
 *
 * 2) main - using the debug output mechanism and the error/warning output
 *      macros.
 *
 * 3) Two examples of the ACPICA initialization sequence. The first is a
 *      initialization with no "early" ACPI table access. The second shows
 *      how to use ACPICA to obtain the tables very early during kernel
 *      initialization, even before dynamic memory is available.
 *
 * 4) How to invoke a control method, including argument setup and how to
 *      access the return value.
 *
 *****************************************************************************/

/* Standard Clib headers */

#include <stdio.h>
#include <string.h>

/* Local Prototypes */

ACPI_STATUS
InitializeFullAcpi (void);

ACPI_STATUS
InstallHandlers (void);

void
ExecuteOSI (void);


/******************************************************************************
 *
 * FUNCTION:    main
 *
 * PARAMETERS:  argc, argv
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Main routine. Shows the use of the various output macros, as
 *              well as the use of the debug layer/level globals.
 *
 *****************************************************************************/

int ACPI_SYSTEM_XFACE
main (
    int                     argc,
    char                    **argv)
{
    ACPI_FUNCTION_NAME (Examples-main);


    InitializeFullAcpi ();

    /* Enable debug output, example debug print */

    AcpiDbgLayer = ACPI_EXAMPLE;
    AcpiDbgLevel = ACPI_LV_INIT;
    ACPI_DEBUG_PRINT ((ACPI_DB_INIT, "Example Debug output\n"));

    /* Example warning and error output */

    ACPI_INFO        ((AE_INFO, "ACPICA example info message"));
    ACPI_WARNING     ((AE_INFO, "ACPICA example warning message"));
    ACPI_ERROR       ((AE_INFO, "ACPICA example error message"));
    ACPI_EXCEPTION   ((AE_INFO, AE_AML_OPERAND_TYPE, "Example exception message"));

    ExecuteOSI ();
    return (0);
}


/******************************************************************************
 *
 * Example ACPICA initialization code. This shows a full initialization with
 * no early ACPI table access.
 *
 *****************************************************************************/

ACPI_STATUS
InitializeFullAcpi (void)
{
    ACPI_STATUS             Status;


    /* Initialize the ACPICA subsystem */

    Status = AcpiInitializeSubsystem ();
    if (ACPI_FAILURE (Status))
    {
        ACPI_EXCEPTION ((AE_INFO, Status, "While initializing ACPICA"));
        return (Status);
    }

    /* Initialize the ACPICA Table Manager and get all ACPI tables */

    Status = AcpiInitializeTables (NULL, 16, FALSE);
    if (ACPI_FAILURE (Status))
    {
        ACPI_EXCEPTION ((AE_INFO, Status, "While initializing Table Manager"));
        return (Status);
    }

    /* Create the ACPI namespace from ACPI tables */

    Status = AcpiLoadTables ();
    if (ACPI_FAILURE (Status))
    {
        ACPI_EXCEPTION ((AE_INFO, Status, "While loading ACPI tables"));
        return (Status);
    }

    /* Install local handlers */

    Status = InstallHandlers ();
    if (ACPI_FAILURE (Status))
    {
        ACPI_EXCEPTION ((AE_INFO, Status, "While installing handlers"));
        return (Status);
    }

    /* Initialize the ACPI hardware */

    Status = AcpiEnableSubsystem (ACPI_FULL_INITIALIZATION);
    if (ACPI_FAILURE (Status))
    {
        ACPI_EXCEPTION ((AE_INFO, Status, "While enabling ACPICA"));
        return (Status);
    }

    /* Complete the ACPI namespace object initialization */

    Status = AcpiInitializeObjects (ACPI_FULL_INITIALIZATION);
    if (ACPI_FAILURE (Status))
    {
        ACPI_EXCEPTION ((AE_INFO, Status, "While initializing ACPICA objects"));
        return (Status);
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * Example ACPICA initialization code with early ACPI table access. This shows
 * an initialization that requires early access to ACPI tables (before
 * kernel dynamic memory is available)
 *
 *****************************************************************************/

/*
 * The purpose of this static table array is to avoid the use of kernel
 * dynamic memory which may not be available during early ACPI table
 * access.
 */
#define ACPI_MAX_INIT_TABLES    16
static ACPI_TABLE_DESC      TableArray[ACPI_MAX_INIT_TABLES];


/*
 * This function would be called early in kernel initialization. After this
 * is called, all ACPI tables are available to the host.
 */
ACPI_STATUS
InitializeAcpiTables (void)
{
    ACPI_STATUS             Status;


    /* Initialize the ACPICA Table Manager and get all ACPI tables */

    Status = AcpiInitializeTables (TableArray, ACPI_MAX_INIT_TABLES, TRUE);
    return (Status);
}


/*
 * This function would be called after the kernel is initialized and
 * dynamic/virtual memory is available. It completes the initialization of
 * the ACPICA subsystem.
 */
ACPI_STATUS
InitializeAcpi (void)
{
    ACPI_STATUS             Status;


    /* Initialize the ACPICA subsystem */

    Status = AcpiInitializeSubsystem ();
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /* Copy the root table list to dynamic memory */

    Status = AcpiReallocateRootTable ();
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /* Create the ACPI namespace from ACPI tables */

    Status = AcpiLoadTables ();
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /* Install local handlers */

    Status = InstallHandlers ();
    if (ACPI_FAILURE (Status))
    {
        ACPI_EXCEPTION ((AE_INFO, Status, "While installing handlers"));
        return (Status);
    }

    /* Initialize the ACPI hardware */

    Status = AcpiEnableSubsystem (ACPI_FULL_INITIALIZATION);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /* Complete the ACPI namespace object initialization */

    Status = AcpiInitializeObjects (ACPI_FULL_INITIALIZATION);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * Example ACPICA handler and handler installation
 *
 *****************************************************************************/

void
NotifyHandler (
    ACPI_HANDLE                 Device,
    UINT32                      Value,
    void                        *Context)
{

    ACPI_INFO ((AE_INFO, "Received a notify %X", Value));
}


ACPI_STATUS
InstallHandlers (void)
{
    ACPI_STATUS             Status;


    /* Install global notify handler */

    Status = AcpiInstallNotifyHandler (ACPI_ROOT_OBJECT, ACPI_SYSTEM_NOTIFY,
                                        NotifyHandler, NULL);
    if (ACPI_FAILURE (Status))
    {
        ACPI_EXCEPTION ((AE_INFO, Status, "While installing Notify handler"));
        return (Status);
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * Example control method execution.
 *
 * _OSI is a predefined method that is implemented internally within ACPICA.
 *
 * Shows the following elements:
 *
 * 1) How to setup a control method argument and argument list
 * 2) How to setup the return value object
 * 3) How to invoke AcpiEvaluateObject
 * 4) How to check the returned ACPI_STATUS
 * 5) How to analyze the return value
 *
 *****************************************************************************/

void
ExecuteOSI (void)
{
    ACPI_STATUS             Status;
    ACPI_OBJECT_LIST        ArgList;
    ACPI_OBJECT             Arg[1];
    ACPI_BUFFER             ReturnValue;
    ACPI_OBJECT             *Object;


    ACPI_INFO ((AE_INFO, "Executing OSI method"));

    /* Setup input argument */

    ArgList.Count = 1;
    ArgList.Pointer = Arg;

    Arg[0].Type = ACPI_TYPE_STRING;
    Arg[0].String.Pointer = "Windows 2001";
    Arg[0].String.Length = strlen (Arg[0].String.Pointer);

    /* Ask ACPICA to allocate space for the return object */

    ReturnValue.Length = ACPI_ALLOCATE_BUFFER;

    Status = AcpiEvaluateObject (NULL, "\\_OSI", &ArgList, &ReturnValue);
    if (ACPI_FAILURE (Status))
    {
        ACPI_EXCEPTION ((AE_INFO, Status, "While executing _OSI"));
        return;
    }

    /* Ensure that the return object is large enough */

    if (ReturnValue.Length < sizeof (ACPI_OBJECT))
    {
        AcpiOsPrintf ("Return value from _OSI method too small, %.8X\n",
            ReturnValue.Length);
        return;
    }

    /* Expect an integer return value from execution of _OSI */

    Object = ReturnValue.Pointer;
    if (Object->Type != ACPI_TYPE_INTEGER)
    {
        AcpiOsPrintf ("Invalid return type from _OSI, %.2X\n", Object->Type);
    }

    ACPI_INFO ((AE_INFO, "_OSI returned %.8X", (UINT32) Object->Integer.Value));
    AcpiOsFree (Object);
    return;
}


/******************************************************************************
 *
 * OSL support (only needed to link to the windows OSL)
 *
 *****************************************************************************/

FILE    *AcpiGbl_DebugFile;

ACPI_PHYSICAL_ADDRESS
AeLocalGetRootPointer (
    void)
{

    return (0);
}

