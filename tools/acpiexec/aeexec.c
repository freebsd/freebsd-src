/******************************************************************************
 *
 * Module Name: aeexec - Support routines for AcpiExec utility
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

#include "aecommon.h"

#define _COMPONENT          ACPI_TOOLS
        ACPI_MODULE_NAME    ("aeexec")

/* Local prototypes */

static ACPI_STATUS
AeSetupConfiguration (
    void                    *RegionAddr);

static void
AfInstallGpeBlock (
    void);

static void
AeTestBufferArgument (
    void);

static void
AeTestPackageArgument (
    void);

static ACPI_STATUS
AeGetDevices (
    ACPI_HANDLE             ObjHandle,
    UINT32                  NestingLevel,
    void                    *Context,
    void                    **ReturnValue);

static ACPI_STATUS
ExecuteOSI (
    char                    *OsiString,
    UINT32                  ExpectedResult);

static void
AeHardwareInterfaces (
    void);

static void
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

static ACPI_STATUS
AeSetupConfiguration (
    void                    *RegionAddr)
{
    ACPI_OBJECT_LIST        ArgList;
    ACPI_OBJECT             Arg[3];


    /*
     * Invoke _CFG method if present
     */
    ArgList.Count = 1;
    ArgList.Pointer = Arg;

    Arg[0].Type = ACPI_TYPE_INTEGER;
    Arg[0].Integer.Value = ACPI_TO_INTEGER (RegionAddr);

    (void) AcpiEvaluateObject (NULL, "\\_CFG", &ArgList, NULL);
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
 * DESCRIPTION: Test GPE block device initialization. Requires test ASL with
 *              A \GPE2 device.
 *
 *****************************************************************************/

static void
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

    ACPI_MEMSET (&BlockAddress, 0, sizeof (ACPI_GENERIC_ADDRESS));
    BlockAddress.SpaceId = ACPI_ADR_SPACE_SYSTEM_MEMORY;
    BlockAddress.Address = 0x76540000;

    Status = AcpiGetHandle (NULL, "\\GPE2", &Handle2);
    if (ACPI_SUCCESS (Status))
    {
        Status = AcpiInstallGpeBlock (Handle2, &BlockAddress, 7, 8);
        AE_CHECK_OK (AcpiInstallGpeBlock, Status);

        Status = AcpiInstallGpeHandler (Handle2, 8,
            ACPI_GPE_LEVEL_TRIGGERED, AeGpeHandler, NULL);
        AE_CHECK_OK (AcpiInstallGpeHandler, Status);

        Status = AcpiEnableGpe (Handle2, 8);
        AE_CHECK_OK (AcpiEnableGpe, Status);

        Status = AcpiGetGpeDevice (0x30, &GpeDevice);
        AE_CHECK_OK (AcpiGetGpeDevice, Status);

        Status = AcpiGetGpeDevice (0x42, &GpeDevice);
        AE_CHECK_OK (AcpiGetGpeDevice, Status);

        Status = AcpiGetGpeDevice (AcpiCurrentGpeCount-1, &GpeDevice);
        AE_CHECK_OK (AcpiGetGpeDevice, Status);

        Status = AcpiGetGpeDevice (AcpiCurrentGpeCount, &GpeDevice);
        AE_CHECK_STATUS (AcpiGetGpeDevice, Status, AE_NOT_EXIST);

        Status = AcpiRemoveGpeHandler (Handle2, 8, AeGpeHandler);
        AE_CHECK_OK (AcpiRemoveGpeHandler, Status);
    }

    Status = AcpiGetHandle (NULL, "\\GPE3", &Handle3);
    if (ACPI_SUCCESS (Status))
    {
        Status = AcpiInstallGpeBlock (Handle3, &BlockAddress, 8, 11);
        AE_CHECK_OK (AcpiInstallGpeBlock, Status);
    }
}


/* Test using a Buffer object as a method argument */

static void
AeTestBufferArgument (
    void)
{
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

    (void) AcpiEvaluateObject (NULL, "\\BUF", &Params, NULL);
}


static ACPI_OBJECT                 PkgArg;
static ACPI_OBJECT                 PkgElements[5];
static ACPI_OBJECT                 Pkg2Elements[5];
static ACPI_OBJECT_LIST            Params;


/*
 * Test using a Package object as an method argument
 */
static void
AeTestPackageArgument (
    void)
{

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

    (void) AcpiEvaluateObject (NULL, "\\_PKG", &Params, NULL);
}


static ACPI_STATUS
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

static ACPI_STATUS
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

static ACPI_GENERIC_ADDRESS       GenericRegister;

static void
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
    AE_CHECK_OK (AcpiRead, Status);

    Status = AcpiWrite (Value, &GenericRegister);
    AE_CHECK_OK (AcpiWrite, Status);

    GenericRegister.Address = 0x12345678;
    GenericRegister.BitOffset = 0;
    GenericRegister.SpaceId = ACPI_ADR_SPACE_SYSTEM_MEMORY;

    Status = AcpiRead (&Value, &GenericRegister);
    AE_CHECK_OK (AcpiRead, Status);

    Status = AcpiWrite (Value, &GenericRegister);
    AE_CHECK_OK (AcpiWrite, Status);
}


/******************************************************************************
 *
 * FUNCTION:    AeHardwareInterfaces
 *
 * DESCRIPTION: Call various hardware support interfaces
 *
 *****************************************************************************/

static void
AeHardwareInterfaces (
    void)
{
    ACPI_STATUS             Status;
    UINT32                  Value;


    Status = AcpiWriteBitRegister (ACPI_BITREG_WAKE_STATUS, 1);
    AE_CHECK_OK (AcpiWriteBitRegister, Status);

    Status = AcpiWriteBitRegister (ACPI_BITREG_GLOBAL_LOCK_ENABLE, 1);
    AE_CHECK_OK (AcpiWriteBitRegister, Status);

    Status = AcpiWriteBitRegister (ACPI_BITREG_SLEEP_ENABLE, 1);
    AE_CHECK_OK (AcpiWriteBitRegister, Status);

    Status = AcpiWriteBitRegister (ACPI_BITREG_ARB_DISABLE, 1);
    AE_CHECK_OK (AcpiWriteBitRegister, Status);


    Status = AcpiReadBitRegister (ACPI_BITREG_WAKE_STATUS, &Value);
    AE_CHECK_OK (AcpiReadBitRegister, Status);

    Status = AcpiReadBitRegister (ACPI_BITREG_GLOBAL_LOCK_ENABLE, &Value);
    AE_CHECK_OK (AcpiReadBitRegister, Status);

    Status = AcpiReadBitRegister (ACPI_BITREG_SLEEP_ENABLE, &Value);
    AE_CHECK_OK (AcpiReadBitRegister, Status);

    Status = AcpiReadBitRegister (ACPI_BITREG_ARB_DISABLE, &Value);
    AE_CHECK_OK (AcpiReadBitRegister, Status);
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


    Status = AcpiInstallInterface ("");
    AE_CHECK_STATUS (AcpiInstallInterface, Status, AE_BAD_PARAMETER);

    Status = AcpiInstallInterface ("TestString");
    AE_CHECK_OK (AcpiInstallInterface, Status);

    Status = AcpiInstallInterface ("TestString");
    AE_CHECK_STATUS (AcpiInstallInterface, Status, AE_ALREADY_EXISTS);

    Status = AcpiRemoveInterface ("Windows 2006");
    AE_CHECK_OK (AcpiRemoveInterface, Status);

    Status = AcpiRemoveInterface ("TestString");
    AE_CHECK_OK (AcpiRemoveInterface, Status);

    Status = AcpiRemoveInterface ("XXXXXX");
    AE_CHECK_STATUS (AcpiRemoveInterface, Status, AE_NOT_EXIST);

    Status = AcpiInstallInterface ("AnotherTestString");
    AE_CHECK_OK (AcpiInstallInterface, Status);


    Status = ExecuteOSI ("Windows 2001", 0xFFFFFFFF);
    AE_CHECK_OK (ExecuteOSI, Status);

    Status = ExecuteOSI ("MichiganTerminalSystem", 0);
    AE_CHECK_OK (ExecuteOSI, Status);


    ReturnBuf.Length = 32;
    ReturnBuf.Pointer = Buffer;

    Status = AcpiGetName (AcpiGbl_RootNode, ACPI_FULL_PATHNAME, &ReturnBuf);
    AE_CHECK_OK (AcpiGetName, Status);

    Status = AcpiEnableEvent (ACPI_EVENT_GLOBAL, 0);
    AE_CHECK_OK (AcpiEnableEvent, Status);

    Status = AcpiInstallGlobalEventHandler (AeGlobalEventHandler, NULL);
    AE_CHECK_OK (AcpiInstallGlobalEventHandler, Status);

    /*
     * GPEs: Handlers, enable/disable, etc.
     */
    Status = AcpiInstallGpeHandler (NULL, 0, ACPI_GPE_LEVEL_TRIGGERED, AeGpeHandler, NULL);
    AE_CHECK_OK (AcpiInstallGpeHandler, Status);

    Status = AcpiEnableGpe (NULL, 0);
    AE_CHECK_OK (AcpiEnableGpe, Status);

    Status = AcpiRemoveGpeHandler (NULL, 0, AeGpeHandler);
    AE_CHECK_OK (AcpiRemoveGpeHandler, Status);

    Status = AcpiInstallGpeHandler (NULL, 0, ACPI_GPE_LEVEL_TRIGGERED, AeGpeHandler, NULL);
    AE_CHECK_OK (AcpiInstallGpeHandler, Status);

    Status = AcpiEnableGpe (NULL, 0);
    AE_CHECK_OK (AcpiEnableGpe, Status);

    Status = AcpiSetGpe (NULL, 0, ACPI_GPE_DISABLE);
    AE_CHECK_OK (AcpiSetGpe, Status);

    Status = AcpiSetGpe (NULL, 0, ACPI_GPE_ENABLE);
    AE_CHECK_OK (AcpiSetGpe, Status);


    Status = AcpiInstallGpeHandler (NULL, 1, ACPI_GPE_EDGE_TRIGGERED, AeGpeHandler, NULL);
    AE_CHECK_OK (AcpiInstallGpeHandler, Status);

    Status = AcpiEnableGpe (NULL, 1);
    AE_CHECK_OK (AcpiEnableGpe, Status);


    Status = AcpiInstallGpeHandler (NULL, 2, ACPI_GPE_LEVEL_TRIGGERED, AeGpeHandler, NULL);
    AE_CHECK_OK (AcpiInstallGpeHandler, Status);

    Status = AcpiEnableGpe (NULL, 2);
    AE_CHECK_OK (AcpiEnableGpe, Status);


    Status = AcpiInstallGpeHandler (NULL, 3, ACPI_GPE_EDGE_TRIGGERED, AeGpeHandler, NULL);
    AE_CHECK_OK (AcpiInstallGpeHandler, Status);

    Status = AcpiInstallGpeHandler (NULL, 4, ACPI_GPE_LEVEL_TRIGGERED, AeGpeHandler, NULL);
    AE_CHECK_OK (AcpiInstallGpeHandler, Status);

    Status = AcpiInstallGpeHandler (NULL, 5, ACPI_GPE_EDGE_TRIGGERED, AeGpeHandler, NULL);
    AE_CHECK_OK (AcpiInstallGpeHandler, Status);

    Status = AcpiGetHandle (NULL, "\\_SB", &Handle);
    AE_CHECK_OK (AcpiGetHandle, Status);

    Status = AcpiSetupGpeForWake (Handle, NULL, 5);
    AE_CHECK_OK (AcpiSetupGpeForWake, Status);

    Status = AcpiSetGpeWakeMask (NULL, 5, ACPI_GPE_ENABLE);
    AE_CHECK_OK (AcpiGpeWakeup, Status);

    Status = AcpiSetupGpeForWake (Handle, NULL, 6);
    AE_CHECK_OK (AcpiSetupGpeForWake, Status);

    Status = AcpiSetupGpeForWake (Handle, NULL, 9);
    AE_CHECK_OK (AcpiSetupGpeForWake, Status);

    Status = AcpiInstallGpeHandler (NULL, 0x19, ACPI_GPE_LEVEL_TRIGGERED, AeGpeHandler, NULL);
    AE_CHECK_OK (AcpiInstallGpeHandler, Status);

    Status = AcpiEnableGpe (NULL, 0x19);
    AE_CHECK_OK (AcpiEnableGpe, Status);


    Status = AcpiInstallGpeHandler (NULL, 0x62, ACPI_GPE_LEVEL_TRIGGERED, AeGpeHandler, NULL);
    AE_CHECK_OK (AcpiInstallGpeHandler, Status);

    Status = AcpiEnableGpe (NULL, 0x62);
    AE_CHECK_OK (AcpiEnableGpe, Status);

    Status = AcpiDisableGpe (NULL, 0x62);
    AE_CHECK_OK (AcpiDisableGpe, Status);

    AfInstallGpeBlock ();

    /* Here is where the GPEs are actually "enabled" */

    Status = AcpiUpdateAllGpes ();
    AE_CHECK_OK (AcpiUpdateAllGpes, Status);

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
    AE_CHECK_OK (AcpiAcquireGlobalLock, Status);

    Status = AcpiAcquireGlobalLock (0x5, &LockHandle2);
    AE_CHECK_OK (AcpiAcquireGlobalLock, Status);

    Status = AcpiReleaseGlobalLock (LockHandle1);
    AE_CHECK_OK (AcpiReleaseGlobalLock, Status);

    Status = AcpiReleaseGlobalLock (LockHandle2);
    AE_CHECK_OK (AcpiReleaseGlobalLock, Status);

    /* Get Devices */

    Status = AcpiGetDevices (NULL, AeGetDevices, NULL, NULL);
    AE_CHECK_OK (AcpiGetDevices, Status);

    Status = AcpiGetStatistics (&Stats);
    AE_CHECK_OK (AcpiGetStatistics, Status);
}

