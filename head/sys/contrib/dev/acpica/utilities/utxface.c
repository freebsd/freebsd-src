/******************************************************************************
 *
 * Module Name: utxface - External interfaces for "global" ACPI functions
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


#define __UTXFACE_C__

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acevents.h>
#include <contrib/dev/acpica/include/acnamesp.h>
#include <contrib/dev/acpica/include/acdebug.h>
#include <contrib/dev/acpica/include/actables.h>

#define _COMPONENT          ACPI_UTILITIES
        ACPI_MODULE_NAME    ("utxface")


#ifndef ACPI_ASL_COMPILER

/*******************************************************************************
 *
 * FUNCTION:    AcpiInitializeSubsystem
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Initializes all global variables.  This is the first function
 *              called, so any early initialization belongs here.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiInitializeSubsystem (
    void)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (AcpiInitializeSubsystem);


    AcpiGbl_StartupFlags = ACPI_SUBSYSTEM_INITIALIZE;
    ACPI_DEBUG_EXEC (AcpiUtInitStackPtrTrace ());

    /* Initialize the OS-Dependent layer */

    Status = AcpiOsInitialize ();
    if (ACPI_FAILURE (Status))
    {
        ACPI_EXCEPTION ((AE_INFO, Status, "During OSL initialization"));
        return_ACPI_STATUS (Status);
    }

    /* Initialize all globals used by the subsystem */

    Status = AcpiUtInitGlobals ();
    if (ACPI_FAILURE (Status))
    {
        ACPI_EXCEPTION ((AE_INFO, Status, "During initialization of globals"));
        return_ACPI_STATUS (Status);
    }

    /* Create the default mutex objects */

    Status = AcpiUtMutexInitialize ();
    if (ACPI_FAILURE (Status))
    {
        ACPI_EXCEPTION ((AE_INFO, Status, "During Global Mutex creation"));
        return_ACPI_STATUS (Status);
    }

    /*
     * Initialize the namespace manager and
     * the root of the namespace tree
     */
    Status = AcpiNsRootInitialize ();
    if (ACPI_FAILURE (Status))
    {
        ACPI_EXCEPTION ((AE_INFO, Status, "During Namespace initialization"));
        return_ACPI_STATUS (Status);
    }

    /* Initialize the global OSI interfaces list with the static names */

    Status = AcpiUtInitializeInterfaces ();
    if (ACPI_FAILURE (Status))
    {
        ACPI_EXCEPTION ((AE_INFO, Status, "During OSI interfaces initialization"));
        return_ACPI_STATUS (Status);
    }

    /* If configured, initialize the AML debugger */

    ACPI_DEBUGGER_EXEC (Status = AcpiDbInitialize ());
    return_ACPI_STATUS (Status);
}

ACPI_EXPORT_SYMBOL (AcpiInitializeSubsystem)


/*******************************************************************************
 *
 * FUNCTION:    AcpiEnableSubsystem
 *
 * PARAMETERS:  Flags           - Init/enable Options
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Completes the subsystem initialization including hardware.
 *              Puts system into ACPI mode if it isn't already.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEnableSubsystem (
    UINT32                  Flags)
{
    ACPI_STATUS             Status = AE_OK;


    ACPI_FUNCTION_TRACE (AcpiEnableSubsystem);


    /* Enable ACPI mode */

    if (!(Flags & ACPI_NO_ACPI_ENABLE))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "[Init] Going into ACPI mode\n"));

        AcpiGbl_OriginalMode = AcpiHwGetMode();

        Status = AcpiEnable ();
        if (ACPI_FAILURE (Status))
        {
            ACPI_WARNING ((AE_INFO, "AcpiEnable failed"));
            return_ACPI_STATUS (Status);
        }
    }

    /*
     * Obtain a permanent mapping for the FACS. This is required for the
     * Global Lock and the Firmware Waking Vector
     */
    Status = AcpiTbInitializeFacs ();
    if (ACPI_FAILURE (Status))
    {
        ACPI_WARNING ((AE_INFO, "Could not map the FACS table"));
        return_ACPI_STATUS (Status);
    }

    /*
     * Install the default OpRegion handlers.  These are installed unless
     * other handlers have already been installed via the
     * InstallAddressSpaceHandler interface.
     */
    if (!(Flags & ACPI_NO_ADDRESS_SPACE_INIT))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_EXEC,
            "[Init] Installing default address space handlers\n"));

        Status = AcpiEvInstallRegionHandlers ();
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }
    }

    /*
     * Initialize ACPI Event handling (Fixed and General Purpose)
     *
     * Note1: We must have the hardware and events initialized before we can
     * execute any control methods safely. Any control method can require
     * ACPI hardware support, so the hardware must be fully initialized before
     * any method execution!
     *
     * Note2: Fixed events are initialized and enabled here. GPEs are
     * initialized, but cannot be enabled until after the hardware is
     * completely initialized (SCI and GlobalLock activated) and the various
     * initialization control methods are run (_REG, _STA, _INI) on the
     * entire namespace.
     */
    if (!(Flags & ACPI_NO_EVENT_INIT))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_EXEC,
            "[Init] Initializing ACPI events\n"));

        Status = AcpiEvInitializeEvents ();
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }
    }

    /*
     * Install the SCI handler and Global Lock handler. This completes the
     * hardware initialization.
     */
    if (!(Flags & ACPI_NO_HANDLER_INIT))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_EXEC,
            "[Init] Installing SCI/GL handlers\n"));

        Status = AcpiEvInstallXruptHandlers ();
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }
    }

    return_ACPI_STATUS (Status);
}

ACPI_EXPORT_SYMBOL (AcpiEnableSubsystem)


/*******************************************************************************
 *
 * FUNCTION:    AcpiInitializeObjects
 *
 * PARAMETERS:  Flags           - Init/enable Options
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Completes namespace initialization by initializing device
 *              objects and executing AML code for Regions, buffers, etc.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiInitializeObjects (
    UINT32                  Flags)
{
    ACPI_STATUS             Status = AE_OK;


    ACPI_FUNCTION_TRACE (AcpiInitializeObjects);


    /*
     * Run all _REG methods
     *
     * Note: Any objects accessed by the _REG methods will be automatically
     * initialized, even if they contain executable AML (see the call to
     * AcpiNsInitializeObjects below).
     */
    if (!(Flags & ACPI_NO_ADDRESS_SPACE_INIT))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_EXEC,
            "[Init] Executing _REG OpRegion methods\n"));

        Status = AcpiEvInitializeOpRegions ();
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }
    }

    /*
     * Execute any module-level code that was detected during the table load
     * phase. Although illegal since ACPI 2.0, there are many machines that
     * contain this type of code. Each block of detected executable AML code
     * outside of any control method is wrapped with a temporary control
     * method object and placed on a global list. The methods on this list
     * are executed below.
     */
    AcpiNsExecModuleCodeList ();

    /*
     * Initialize the objects that remain uninitialized. This runs the
     * executable AML that may be part of the declaration of these objects:
     * OperationRegions, BufferFields, Buffers, and Packages.
     */
    if (!(Flags & ACPI_NO_OBJECT_INIT))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_EXEC,
            "[Init] Completing Initialization of ACPI Objects\n"));

        Status = AcpiNsInitializeObjects ();
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }
    }

    /*
     * Initialize all device objects in the namespace. This runs the device
     * _STA and _INI methods.
     */
    if (!(Flags & ACPI_NO_DEVICE_INIT))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_EXEC,
            "[Init] Initializing ACPI Devices\n"));

        Status = AcpiNsInitializeDevices ();
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }
    }

    /*
     * Empty the caches (delete the cached objects) on the assumption that
     * the table load filled them up more than they will be at runtime --
     * thus wasting non-paged memory.
     */
    Status = AcpiPurgeCachedObjects ();

    AcpiGbl_StartupFlags |= ACPI_INITIALIZED_OK;
    return_ACPI_STATUS (Status);
}

ACPI_EXPORT_SYMBOL (AcpiInitializeObjects)


#endif

/*******************************************************************************
 *
 * FUNCTION:    AcpiTerminate
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Shutdown the ACPICA subsystem and release all resources.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTerminate (
    void)
{
    ACPI_STATUS         Status;


    ACPI_FUNCTION_TRACE (AcpiTerminate);


    /* Just exit if subsystem is already shutdown */

    if (AcpiGbl_Shutdown)
    {
        ACPI_ERROR ((AE_INFO, "ACPI Subsystem is already terminated"));
        return_ACPI_STATUS (AE_OK);
    }

    /* Subsystem appears active, go ahead and shut it down */

    AcpiGbl_Shutdown = TRUE;
    AcpiGbl_StartupFlags = 0;
    ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "Shutting down ACPI Subsystem\n"));

    /* Terminate the AML Debugger if present */

    ACPI_DEBUGGER_EXEC (AcpiGbl_DbTerminateThreads = TRUE);

    /* Shutdown and free all resources */

    AcpiUtSubsystemShutdown ();

    /* Free the mutex objects */

    AcpiUtMutexTerminate ();


#ifdef ACPI_DEBUGGER

    /* Shut down the debugger */

    AcpiDbTerminate ();
#endif

    /* Now we can shutdown the OS-dependent layer */

    Status = AcpiOsTerminate ();
    return_ACPI_STATUS (Status);
}

ACPI_EXPORT_SYMBOL (AcpiTerminate)


#ifndef ACPI_ASL_COMPILER
/*******************************************************************************
 *
 * FUNCTION:    AcpiSubsystemStatus
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status of the ACPI subsystem
 *
 * DESCRIPTION: Other drivers that use the ACPI subsystem should call this
 *              before making any other calls, to ensure the subsystem
 *              initialized successfully.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiSubsystemStatus (
    void)
{

    if (AcpiGbl_StartupFlags & ACPI_INITIALIZED_OK)
    {
        return (AE_OK);
    }
    else
    {
        return (AE_ERROR);
    }
}

ACPI_EXPORT_SYMBOL (AcpiSubsystemStatus)


/*******************************************************************************
 *
 * FUNCTION:    AcpiGetSystemInfo
 *
 * PARAMETERS:  OutBuffer       - A buffer to receive the resources for the
 *                                device
 *
 * RETURN:      Status          - the status of the call
 *
 * DESCRIPTION: This function is called to get information about the current
 *              state of the ACPI subsystem.  It will return system information
 *              in the OutBuffer.
 *
 *              If the function fails an appropriate status will be returned
 *              and the value of OutBuffer is undefined.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiGetSystemInfo (
    ACPI_BUFFER             *OutBuffer)
{
    ACPI_SYSTEM_INFO        *InfoPtr;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (AcpiGetSystemInfo);


    /* Parameter validation */

    Status = AcpiUtValidateBuffer (OutBuffer);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Validate/Allocate/Clear caller buffer */

    Status = AcpiUtInitializeBuffer (OutBuffer, sizeof (ACPI_SYSTEM_INFO));
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /*
     * Populate the return buffer
     */
    InfoPtr = (ACPI_SYSTEM_INFO *) OutBuffer->Pointer;

    InfoPtr->AcpiCaVersion = ACPI_CA_VERSION;

    /* System flags (ACPI capabilities) */

    InfoPtr->Flags = ACPI_SYS_MODE_ACPI;

    /* Timer resolution - 24 or 32 bits  */

    if (AcpiGbl_FADT.Flags & ACPI_FADT_32BIT_TIMER)
    {
        InfoPtr->TimerResolution = 24;
    }
    else
    {
        InfoPtr->TimerResolution = 32;
    }

    /* Clear the reserved fields */

    InfoPtr->Reserved1 = 0;
    InfoPtr->Reserved2 = 0;

    /* Current debug levels */

    InfoPtr->DebugLayer = AcpiDbgLayer;
    InfoPtr->DebugLevel = AcpiDbgLevel;

    return_ACPI_STATUS (AE_OK);
}

ACPI_EXPORT_SYMBOL (AcpiGetSystemInfo)


/*******************************************************************************
 *
 * FUNCTION:    AcpiGetStatistics
 *
 * PARAMETERS:  Stats           - Where the statistics are returned
 *
 * RETURN:      Status          - the status of the call
 *
 * DESCRIPTION: Get the contents of the various system counters
 *
 ******************************************************************************/

ACPI_STATUS
AcpiGetStatistics (
    ACPI_STATISTICS         *Stats)
{
    ACPI_FUNCTION_TRACE (AcpiGetStatistics);


    /* Parameter validation */

    if (!Stats)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /* Various interrupt-based event counters */

    Stats->SciCount = AcpiSciCount;
    Stats->GpeCount = AcpiGpeCount;

    ACPI_MEMCPY (Stats->FixedEventCount, AcpiFixedEventCount,
        sizeof (AcpiFixedEventCount));


    /* Other counters */

    Stats->MethodCount = AcpiMethodCount;

    return_ACPI_STATUS (AE_OK);
}

ACPI_EXPORT_SYMBOL (AcpiGetStatistics)


/*****************************************************************************
 *
 * FUNCTION:    AcpiInstallInitializationHandler
 *
 * PARAMETERS:  Handler             - Callback procedure
 *              Function            - Not (currently) used, see below
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Install an initialization handler
 *
 * TBD: When a second function is added, must save the Function also.
 *
 ****************************************************************************/

ACPI_STATUS
AcpiInstallInitializationHandler (
    ACPI_INIT_HANDLER       Handler,
    UINT32                  Function)
{

    if (!Handler)
    {
        return (AE_BAD_PARAMETER);
    }

    if (AcpiGbl_InitHandler)
    {
        return (AE_ALREADY_EXISTS);
    }

    AcpiGbl_InitHandler = Handler;
    return AE_OK;
}

ACPI_EXPORT_SYMBOL (AcpiInstallInitializationHandler)


/*****************************************************************************
 *
 * FUNCTION:    AcpiPurgeCachedObjects
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Empty all caches (delete the cached objects)
 *
 ****************************************************************************/

ACPI_STATUS
AcpiPurgeCachedObjects (
    void)
{
    ACPI_FUNCTION_TRACE (AcpiPurgeCachedObjects);

    (void) AcpiOsPurgeCache (AcpiGbl_StateCache);
    (void) AcpiOsPurgeCache (AcpiGbl_OperandCache);
    (void) AcpiOsPurgeCache (AcpiGbl_PsNodeCache);
    (void) AcpiOsPurgeCache (AcpiGbl_PsNodeExtCache);
    return_ACPI_STATUS (AE_OK);
}

ACPI_EXPORT_SYMBOL (AcpiPurgeCachedObjects)


/*****************************************************************************
 *
 * FUNCTION:    AcpiInstallInterface
 *
 * PARAMETERS:  InterfaceName       - The interface to install
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Install an _OSI interface to the global list
 *
 ****************************************************************************/

ACPI_STATUS
AcpiInstallInterface (
    ACPI_STRING             InterfaceName)
{
    ACPI_STATUS             Status;
    ACPI_INTERFACE_INFO     *InterfaceInfo;


    /* Parameter validation */

    if (!InterfaceName || (ACPI_STRLEN (InterfaceName) == 0))
    {
        return (AE_BAD_PARAMETER);
    }

    (void) AcpiOsAcquireMutex (AcpiGbl_OsiMutex, ACPI_WAIT_FOREVER);

    /* Check if the interface name is already in the global list */

    InterfaceInfo = AcpiUtGetInterface (InterfaceName);
    if (InterfaceInfo)
    {
        /*
         * The interface already exists in the list. This is OK if the
         * interface has been marked invalid -- just clear the bit.
         */
        if (InterfaceInfo->Flags & ACPI_OSI_INVALID)
        {
            InterfaceInfo->Flags &= ~ACPI_OSI_INVALID;
            Status = AE_OK;
        }
        else
        {
            Status = AE_ALREADY_EXISTS;
        }
    }
    else
    {
        /* New interface name, install into the global list */

        Status = AcpiUtInstallInterface (InterfaceName);
    }

    AcpiOsReleaseMutex (AcpiGbl_OsiMutex);
    return (Status);
}

ACPI_EXPORT_SYMBOL (AcpiInstallInterface)


/*****************************************************************************
 *
 * FUNCTION:    AcpiRemoveInterface
 *
 * PARAMETERS:  InterfaceName       - The interface to remove
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Remove an _OSI interface from the global list
 *
 ****************************************************************************/

ACPI_STATUS
AcpiRemoveInterface (
    ACPI_STRING             InterfaceName)
{
    ACPI_STATUS             Status;


    /* Parameter validation */

    if (!InterfaceName || (ACPI_STRLEN (InterfaceName) == 0))
    {
        return (AE_BAD_PARAMETER);
    }

    (void) AcpiOsAcquireMutex (AcpiGbl_OsiMutex, ACPI_WAIT_FOREVER);

    Status = AcpiUtRemoveInterface (InterfaceName);

    AcpiOsReleaseMutex (AcpiGbl_OsiMutex);
    return (Status);
}

ACPI_EXPORT_SYMBOL (AcpiRemoveInterface)


/*****************************************************************************
 *
 * FUNCTION:    AcpiInstallInterfaceHandler
 *
 * PARAMETERS:  Handler             - The _OSI interface handler to install
 *                                    NULL means "remove existing handler"
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Install a handler for the predefined _OSI ACPI method.
 *              invoked during execution of the internal implementation of
 *              _OSI. A NULL handler simply removes any existing handler.
 *
 ****************************************************************************/

ACPI_STATUS
AcpiInstallInterfaceHandler (
    ACPI_INTERFACE_HANDLER  Handler)
{
    ACPI_STATUS             Status = AE_OK;


    (void) AcpiOsAcquireMutex (AcpiGbl_OsiMutex, ACPI_WAIT_FOREVER);

    if (Handler && AcpiGbl_InterfaceHandler)
    {
        Status = AE_ALREADY_EXISTS;
    }
    else
    {
        AcpiGbl_InterfaceHandler = Handler;
    }

    AcpiOsReleaseMutex (AcpiGbl_OsiMutex);
    return (Status);
}

ACPI_EXPORT_SYMBOL (AcpiInstallInterfaceHandler)

#endif /* !ACPI_ASL_COMPILER */

