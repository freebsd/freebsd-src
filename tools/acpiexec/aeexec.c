/******************************************************************************
 *
 * Module Name: aeexec - Support routines for AcpiExec utility
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2009, Intel Corp.
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

#include "aecommon.h"

#define _COMPONENT          ACPI_TOOLS
        ACPI_MODULE_NAME    ("aeexec")

/* Local prototypes */

ACPI_STATUS
AeSetupConfiguration (
    void                    *RegionAddr);

void
AfInstallGpeBlock (
    void);

void
AeTestBufferArgument (
    void);

void
AeTestPackageArgument (
    void);

ACPI_STATUS
AeGetDevices (
    ACPI_HANDLE             ObjHandle,
    UINT32                  NestingLevel,
    void                    *Context,
    void                    **ReturnValue);

ACPI_STATUS
ExecuteOSI (
    char                    *OsiString,
    UINT32                  ExpectedResult);

void
AeHardwareInterfaces (
    void);

void
AeGenericRegisters (
    void);

extern unsigned char Ssdt3Code[];


/******************************************************************************
 *
 * FUNCTION:    AeSetupConfiguration
 *
 * PARAMETERS:  RegionAddr          - Address for an ACPI table to be loaded
 *                                    dynamically. Test purposes only.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Call AML _CFG configuration control method
 *
 *****************************************************************************/

ACPI_STATUS
AeSetupConfiguration (
    void                    *RegionAddr)
{
    ACPI_STATUS             Status;
    ACPI_OBJECT_LIST        ArgList;
    ACPI_OBJECT             Arg[3];


    /*
     * Invoke _CFG method if present
     */
    ArgList.Count = 1;
    ArgList.Pointer = Arg;

    Arg[0].Type = ACPI_TYPE_INTEGER;
    Arg[0].Integer.Value = ACPI_TO_INTEGER (RegionAddr);

    Status = AcpiEvaluateObject (NULL, "\\_CFG", &ArgList, NULL);

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AfInstallGpeBlock
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Various GPE initialization
 *
 *****************************************************************************/

void
AfInstallGpeBlock (
    void)
{
    ACPI_STATUS                 Status;
    ACPI_HANDLE                 Handle;
    ACPI_HANDLE                 Handle2 = NULL;
    ACPI_HANDLE                 Handle3 = NULL;
    ACPI_GENERIC_ADDRESS        BlockAddress;
    ACPI_HANDLE                 GpeDevice;


    Status = AcpiGetHandle (NULL, "\\_GPE", &Handle);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    BlockAddress.SpaceId = 0;
    BlockAddress.Address = 0x76540000;

#ifdef _OBSOLETE
    Status = AcpiInstallGpeBlock (Handle, &BlockAddress, 4, 8);
#endif

    /* Above should fail, ignore */

    Status = AcpiGetHandle (NULL, "\\GPE2", &Handle2);
    if (ACPI_SUCCESS (Status))
    {
        Status = AcpiInstallGpeBlock (Handle2, &BlockAddress, 8, 8);

        AcpiInstallGpeHandler (Handle2, 8, ACPI_GPE_LEVEL_TRIGGERED, AeGpeHandler, NULL);
        AcpiSetGpeType (Handle2, 8, ACPI_GPE_TYPE_WAKE);
        AcpiEnableGpe (Handle2, 8, 0);

        Status = AcpiGetGpeDevice (0x30, &GpeDevice);
        Status = AcpiGetGpeDevice (0x42, &GpeDevice);
        Status = AcpiGetGpeDevice (AcpiCurrentGpeCount-1, &GpeDevice);
        Status = AcpiGetGpeDevice (AcpiCurrentGpeCount, &GpeDevice);

        AcpiRemoveGpeHandler (Handle2, 8, AeGpeHandler);

        Status = AcpiRemoveGpeBlock (Handle2);
    }

    Status = AcpiGetHandle (NULL, "\\GPE3", &Handle3);
    if (ACPI_SUCCESS (Status))
    {
        Status = AcpiInstallGpeBlock (Handle3, &BlockAddress, 8, 11);
    }

#ifdef _OBSOLETE
    Status = AcpiRemoveGpeBlock (Handle);
    Status = AcpiRemoveGpeBlock (Handle2);
    Status = AcpiRemoveGpeBlock (Handle3);
#endif
}


/* Test using a Buffer object as a method argument */

void
AeTestBufferArgument (
    void)
{
    ACPI_STATUS             Status;
    ACPI_OBJECT_LIST        Params;
    ACPI_OBJECT             BufArg;
    UINT8                   Buffer[] = {
        0,0,0,0,
        4,0,0,0,
        1,2,3,4};


    BufArg.Type = ACPI_TYPE_BUFFER;
    BufArg.Buffer.Length = 12;
    BufArg.Buffer.Pointer = Buffer;

    Params.Count = 1;
    Params.Pointer = &BufArg;


    Status = AcpiEvaluateObject (NULL, "\\BUF", &Params, NULL);
}


ACPI_OBJECT                 PkgArg;
ACPI_OBJECT                 PkgElements[5];
ACPI_OBJECT                 Pkg2Elements[5];
ACPI_OBJECT_LIST            Params;


/*
 * Test using a Package object as an method argument
 */
void
AeTestPackageArgument (
    void)
{
    ACPI_STATUS             Status;


    /* Main package */

    PkgArg.Type = ACPI_TYPE_PACKAGE;
    PkgArg.Package.Count = 4;
    PkgArg.Package.Elements = PkgElements;

    /* Main package elements */

    PkgElements[0].Type = ACPI_TYPE_INTEGER;
    PkgElements[0].Integer.Value = 0x22228888;

    PkgElements[1].Type = ACPI_TYPE_STRING;
    PkgElements[1].String.Length = sizeof ("Top-level package");
    PkgElements[1].String.Pointer = "Top-level package";

    PkgElements[2].Type = ACPI_TYPE_BUFFER;
    PkgElements[2].Buffer.Length = sizeof ("XXXX");
    PkgElements[2].Buffer.Pointer = (UINT8 *) "XXXX";

    PkgElements[3].Type = ACPI_TYPE_PACKAGE;
    PkgElements[3].Package.Count = 2;
    PkgElements[3].Package.Elements = Pkg2Elements;

    /* Sub-package elements */

    Pkg2Elements[0].Type = ACPI_TYPE_INTEGER;
    Pkg2Elements[0].Integer.Value = 0xAAAABBBB;

    Pkg2Elements[1].Type = ACPI_TYPE_STRING;
    Pkg2Elements[1].String.Length = sizeof ("Nested Package");
    Pkg2Elements[1].String.Pointer = "Nested Package";

    /* Parameter object */

    Params.Count = 1;
    Params.Pointer = &PkgArg;

    Status = AcpiEvaluateObject (NULL, "\\_PKG", &Params, NULL);
}


ACPI_STATUS
AeGetDevices (
    ACPI_HANDLE                     ObjHandle,
    UINT32                          NestingLevel,
    void                            *Context,
    void                            **ReturnValue)
{

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    ExecuteOSI
 *
 * PARAMETERS:  OsiString           - String passed to _OSI method
 *              ExpectedResult      - 0 (FALSE) or 0xFFFFFFFF (TRUE)
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute the internally implemented (in ACPICA) _OSI method.
 *
 *****************************************************************************/

ACPI_STATUS
ExecuteOSI (
    char                    *OsiString,
    UINT32                  ExpectedResult)
{
    ACPI_STATUS             Status;
    ACPI_OBJECT_LIST        ArgList;
    ACPI_OBJECT             Arg[1];
    ACPI_BUFFER             ReturnValue;
    ACPI_OBJECT             *Obj;


    /* Setup input argument */

    ArgList.Count = 1;
    ArgList.Pointer = Arg;

    Arg[0].Type = ACPI_TYPE_STRING;
    Arg[0].String.Pointer = OsiString;
    Arg[0].String.Length = strlen (Arg[0].String.Pointer);

    /* Ask ACPICA to allocate space for the return object */

    ReturnValue.Length = ACPI_ALLOCATE_BUFFER;

    Status = AcpiEvaluateObject (NULL, "\\_OSI", &ArgList, &ReturnValue);

    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Could not execute _OSI method, %s\n",
            AcpiFormatException (Status));
        return (Status);
    }

    if (ReturnValue.Length < sizeof (ACPI_OBJECT))
    {
        AcpiOsPrintf ("Return value from _OSI method too small, %.8X\n",
            ReturnValue.Length);
        return (AE_ERROR);
    }

    Obj = ReturnValue.Pointer;
    if (Obj->Type != ACPI_TYPE_INTEGER)
    {
        AcpiOsPrintf ("Invalid return type from _OSI method, %.2X\n", Obj->Type);
        return (AE_ERROR);
    }

    if (Obj->Integer.Value != ExpectedResult)
    {
        AcpiOsPrintf ("Invalid return value from _OSI, expected %.8X found %.8X\n",
            ExpectedResult, (UINT32) Obj->Integer.Value);
        return (AE_ERROR);
    }

    /* Reset the OSI data */

    AcpiGbl_OsiData = 0;
    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AeGenericRegisters
 *
 * DESCRIPTION: Call the AcpiRead/Write interfaces.
 *
 *****************************************************************************/

ACPI_GENERIC_ADDRESS       GenericRegister;

void
AeGenericRegisters (
    void)
{
    ACPI_STATUS             Status;
    UINT64                  Value;


    GenericRegister.Address = 0x1234;
    GenericRegister.BitWidth = 64;
    GenericRegister.BitOffset = 0;
    GenericRegister.SpaceId = ACPI_ADR_SPACE_SYSTEM_IO;

    Status = AcpiRead (&Value, &GenericRegister);
    Status = AcpiWrite (Value, &GenericRegister);

    GenericRegister.Address = 0x12345678;
    GenericRegister.BitOffset = 0;
    GenericRegister.SpaceId = ACPI_ADR_SPACE_SYSTEM_MEMORY;

    Status = AcpiRead (&Value, &GenericRegister);
    Status = AcpiWrite (Value, &GenericRegister);
}


/******************************************************************************
 *
 * FUNCTION:    AeHardwareInterfaces
 *
 * DESCRIPTION: Call various hardware support interfaces
 *
 *****************************************************************************/

void
AeHardwareInterfaces (
    void)
{
    ACPI_STATUS             Status;
    UINT32                  Value;



    Status = AcpiWriteBitRegister (ACPI_BITREG_WAKE_STATUS, 1);
    Status = AcpiWriteBitRegister (ACPI_BITREG_GLOBAL_LOCK_ENABLE, 1);
    Status = AcpiWriteBitRegister (ACPI_BITREG_SLEEP_ENABLE, 1);
    Status = AcpiWriteBitRegister (ACPI_BITREG_ARB_DISABLE, 1);

    Status = AcpiReadBitRegister (ACPI_BITREG_WAKE_STATUS, &Value);
    Status = AcpiReadBitRegister (ACPI_BITREG_GLOBAL_LOCK_ENABLE, &Value);
    Status = AcpiReadBitRegister (ACPI_BITREG_SLEEP_ENABLE, &Value);
    Status = AcpiReadBitRegister (ACPI_BITREG_ARB_DISABLE, &Value);
}


/******************************************************************************
 *
 * FUNCTION:    AeMiscellaneousTests
 *
 * DESCRIPTION: Various ACPICA validation tests.
 *
 *****************************************************************************/

void
AeMiscellaneousTests (
    void)
{
    ACPI_HANDLE             Handle;
    ACPI_BUFFER             ReturnBuf;
    char                    Buffer[32];
    ACPI_VENDOR_UUID        Uuid = {0, {ACPI_INIT_UUID (0,0,0,0,0,0,0,0,0,0,0)}};
    ACPI_STATUS             Status;
    UINT32                  LockHandle1;
    UINT32                  LockHandle2;
    ACPI_STATISTICS         Stats;


    AeHardwareInterfaces ();
    AeGenericRegisters ();
    AeSetupConfiguration (Ssdt3Code);

    AeTestBufferArgument();
    AeTestPackageArgument ();
    ExecuteOSI ("Windows 2001", 0xFFFFFFFF);
    ExecuteOSI ("MichiganTerminalSystem", 0);


    ReturnBuf.Length = 32;
    ReturnBuf.Pointer = Buffer;

    AcpiGetName (AcpiGbl_RootNode, ACPI_FULL_PATHNAME, &ReturnBuf);
    AcpiEnableEvent (ACPI_EVENT_GLOBAL, 0);

    AcpiInstallGpeHandler (NULL, 0, ACPI_GPE_LEVEL_TRIGGERED, AeGpeHandler, NULL);
    AcpiSetGpeType (NULL, 0, ACPI_GPE_TYPE_WAKE_RUN);
    AcpiEnableGpe (NULL, 0, ACPI_NOT_ISR);
    AcpiRemoveGpeHandler (NULL, 0, AeGpeHandler);

    AcpiInstallGpeHandler (NULL, 0, ACPI_GPE_LEVEL_TRIGGERED, AeGpeHandler, NULL);
    AcpiSetGpeType (NULL, 0, ACPI_GPE_TYPE_WAKE_RUN);
    AcpiEnableGpe (NULL, 0, ACPI_NOT_ISR);

    AcpiInstallGpeHandler (NULL, 1, ACPI_GPE_EDGE_TRIGGERED, AeGpeHandler, NULL);
    AcpiSetGpeType (NULL, 1, ACPI_GPE_TYPE_RUNTIME);
    AcpiEnableGpe (NULL, 1, ACPI_NOT_ISR);

    AcpiInstallGpeHandler (NULL, 2, ACPI_GPE_LEVEL_TRIGGERED, AeGpeHandler, NULL);
    AcpiSetGpeType (NULL, 2, ACPI_GPE_TYPE_WAKE);
    AcpiEnableGpe (NULL, 2, ACPI_NOT_ISR);

    AcpiInstallGpeHandler (NULL, 3, ACPI_GPE_EDGE_TRIGGERED, AeGpeHandler, NULL);
    AcpiSetGpeType (NULL, 3, ACPI_GPE_TYPE_WAKE_RUN);

    AcpiInstallGpeHandler (NULL, 4, ACPI_GPE_LEVEL_TRIGGERED, AeGpeHandler, NULL);
    AcpiSetGpeType (NULL, 4, ACPI_GPE_TYPE_RUNTIME);

    AcpiInstallGpeHandler (NULL, 5, ACPI_GPE_EDGE_TRIGGERED, AeGpeHandler, NULL);
    AcpiSetGpeType (NULL, 5, ACPI_GPE_TYPE_WAKE);

    AcpiInstallGpeHandler (NULL, 0x19, ACPI_GPE_LEVEL_TRIGGERED, AeGpeHandler, NULL);
    AcpiSetGpeType (NULL, 0x19, ACPI_GPE_TYPE_WAKE_RUN);
    AcpiEnableGpe (NULL, 0x19, ACPI_NOT_ISR);

    AfInstallGpeBlock ();


    Status = AcpiGetHandle (NULL, "RSRC", &Handle);
    if (ACPI_SUCCESS (Status))
    {
        ReturnBuf.Length = ACPI_ALLOCATE_BUFFER;

        Status = AcpiGetVendorResource (Handle, "_CRS", &Uuid, &ReturnBuf);
        if (ACPI_SUCCESS (Status))
        {
            AcpiOsFree (ReturnBuf.Pointer);
        }
    }

    /* Test global lock */

    Status = AcpiAcquireGlobalLock (0xFFFF, &LockHandle1);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Could not get GlobalLock, %X\n", Status);
    }

    Status = AcpiAcquireGlobalLock (0x5, &LockHandle2);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Could not get GlobalLock, %X\n", Status);
    }

    Status = AcpiReleaseGlobalLock (LockHandle1);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Could not release GlobalLock, %X\n", Status);
    }

    Status = AcpiReleaseGlobalLock (LockHandle2);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Could not release GlobalLock, %X\n", Status);
    }

    /* Get Devices */

    Status = AcpiGetDevices (NULL, AeGetDevices, NULL, NULL);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Could not AcpiGetDevices, %X\n", Status);
    }

    Status = AcpiGetStatistics (&Stats);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Could not AcpiGetStatistics, %X\n", Status);
    }
}

