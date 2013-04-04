/******************************************************************************
 *
 * Module Name: utosi - Support for the _OSI predefined control method
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

#define __UTOSI_C__

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>


#define _COMPONENT          ACPI_UTILITIES
        ACPI_MODULE_NAME    ("utosi")

/*
 * Strings supported by the _OSI predefined control method (which is
 * implemented internally within this module.)
 *
 * March 2009: Removed "Linux" as this host no longer wants to respond true
 * for this string. Basically, the only safe OS strings are windows-related
 * and in many or most cases represent the only test path within the
 * BIOS-provided ASL code.
 *
 * The last element of each entry is used to track the newest version of
 * Windows that the BIOS has requested.
 */
static ACPI_INTERFACE_INFO    AcpiDefaultSupportedInterfaces[] =
{
    /* Operating System Vendor Strings */

    {"Windows 2000",        NULL, 0, ACPI_OSI_WIN_2000},         /* Windows 2000 */
    {"Windows 2001",        NULL, 0, ACPI_OSI_WIN_XP},           /* Windows XP */
    {"Windows 2001 SP1",    NULL, 0, ACPI_OSI_WIN_XP_SP1},       /* Windows XP SP1 */
    {"Windows 2001.1",      NULL, 0, ACPI_OSI_WINSRV_2003},      /* Windows Server 2003 */
    {"Windows 2001 SP2",    NULL, 0, ACPI_OSI_WIN_XP_SP2},       /* Windows XP SP2 */
    {"Windows 2001.1 SP1",  NULL, 0, ACPI_OSI_WINSRV_2003_SP1},  /* Windows Server 2003 SP1 - Added 03/2006 */
    {"Windows 2006",        NULL, 0, ACPI_OSI_WIN_VISTA},        /* Windows Vista - Added 03/2006 */
    {"Windows 2006.1",      NULL, 0, ACPI_OSI_WINSRV_2008},      /* Windows Server 2008 - Added 09/2009 */
    {"Windows 2006 SP1",    NULL, 0, ACPI_OSI_WIN_VISTA_SP1},    /* Windows Vista SP1 - Added 09/2009 */
    {"Windows 2006 SP2",    NULL, 0, ACPI_OSI_WIN_VISTA_SP2},    /* Windows Vista SP2 - Added 09/2010 */
    {"Windows 2009",        NULL, 0, ACPI_OSI_WIN_7},            /* Windows 7 and Server 2008 R2 - Added 09/2009 */
    {"Windows 2012",        NULL, 0, ACPI_OSI_WIN_8},            /* Windows 8 and Server 2012 - Added 08/2012 */

    /* Feature Group Strings */

    {"Extended Address Space Descriptor", NULL, 0, 0}

    /*
     * All "optional" feature group strings (features that are implemented
     * by the host) should be dynamically added by the host via
     * AcpiInstallInterface and should not be manually added here.
     *
     * Examples of optional feature group strings:
     *
     * "Module Device"
     * "Processor Device"
     * "3.0 Thermal Model"
     * "3.0 _SCP Extensions"
     * "Processor Aggregator Device"
     */
};


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtInitializeInterfaces
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Initialize the global _OSI supported interfaces list
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtInitializeInterfaces (
    void)
{
    ACPI_STATUS             Status;
    UINT32                  i;


    Status = AcpiOsAcquireMutex (AcpiGbl_OsiMutex, ACPI_WAIT_FOREVER);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    AcpiGbl_SupportedInterfaces = AcpiDefaultSupportedInterfaces;

    /* Link the static list of supported interfaces */

    for (i = 0; i < (ACPI_ARRAY_LENGTH (AcpiDefaultSupportedInterfaces) - 1); i++)
    {
        AcpiDefaultSupportedInterfaces[i].Next =
            &AcpiDefaultSupportedInterfaces[(ACPI_SIZE) i + 1];
    }

    AcpiOsReleaseMutex (AcpiGbl_OsiMutex);
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtInterfaceTerminate
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Delete all interfaces in the global list. Sets
 *              AcpiGbl_SupportedInterfaces to NULL.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtInterfaceTerminate (
    void)
{
    ACPI_STATUS             Status;
    ACPI_INTERFACE_INFO     *NextInterface;


    Status = AcpiOsAcquireMutex (AcpiGbl_OsiMutex, ACPI_WAIT_FOREVER);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    NextInterface = AcpiGbl_SupportedInterfaces;
    while (NextInterface)
    {
        AcpiGbl_SupportedInterfaces = NextInterface->Next;

        /* Only interfaces added at runtime can be freed */

        if (NextInterface->Flags & ACPI_OSI_DYNAMIC)
        {
            ACPI_FREE (NextInterface->Name);
            ACPI_FREE (NextInterface);
        }

        NextInterface = AcpiGbl_SupportedInterfaces;
    }

    AcpiOsReleaseMutex (AcpiGbl_OsiMutex);
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtInstallInterface
 *
 * PARAMETERS:  InterfaceName       - The interface to install
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Install the interface into the global interface list.
 *              Caller MUST hold AcpiGbl_OsiMutex
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtInstallInterface (
    ACPI_STRING             InterfaceName)
{
    ACPI_INTERFACE_INFO     *InterfaceInfo;


    /* Allocate info block and space for the name string */

    InterfaceInfo = ACPI_ALLOCATE_ZEROED (sizeof (ACPI_INTERFACE_INFO));
    if (!InterfaceInfo)
    {
        return (AE_NO_MEMORY);
    }

    InterfaceInfo->Name = ACPI_ALLOCATE_ZEROED (ACPI_STRLEN (InterfaceName) + 1);
    if (!InterfaceInfo->Name)
    {
        ACPI_FREE (InterfaceInfo);
        return (AE_NO_MEMORY);
    }

    /* Initialize new info and insert at the head of the global list */

    ACPI_STRCPY (InterfaceInfo->Name, InterfaceName);
    InterfaceInfo->Flags = ACPI_OSI_DYNAMIC;
    InterfaceInfo->Next = AcpiGbl_SupportedInterfaces;

    AcpiGbl_SupportedInterfaces = InterfaceInfo;
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtRemoveInterface
 *
 * PARAMETERS:  InterfaceName       - The interface to remove
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Remove the interface from the global interface list.
 *              Caller MUST hold AcpiGbl_OsiMutex
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtRemoveInterface (
    ACPI_STRING             InterfaceName)
{
    ACPI_INTERFACE_INFO     *PreviousInterface;
    ACPI_INTERFACE_INFO     *NextInterface;


    PreviousInterface = NextInterface = AcpiGbl_SupportedInterfaces;
    while (NextInterface)
    {
        if (!ACPI_STRCMP (InterfaceName, NextInterface->Name))
        {
            /* Found: name is in either the static list or was added at runtime */

            if (NextInterface->Flags & ACPI_OSI_DYNAMIC)
            {
                /* Interface was added dynamically, remove and free it */

                if (PreviousInterface == NextInterface)
                {
                    AcpiGbl_SupportedInterfaces = NextInterface->Next;
                }
                else
                {
                    PreviousInterface->Next = NextInterface->Next;
                }

                ACPI_FREE (NextInterface->Name);
                ACPI_FREE (NextInterface);
            }
            else
            {
                /*
                 * Interface is in static list. If marked invalid, then it
                 * does not actually exist. Else, mark it invalid.
                 */
                if (NextInterface->Flags & ACPI_OSI_INVALID)
                {
                    return (AE_NOT_EXIST);
                }

                NextInterface->Flags |= ACPI_OSI_INVALID;
            }

            return (AE_OK);
        }

        PreviousInterface = NextInterface;
        NextInterface = NextInterface->Next;
    }

    /* Interface was not found */

    return (AE_NOT_EXIST);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtGetInterface
 *
 * PARAMETERS:  InterfaceName       - The interface to find
 *
 * RETURN:      ACPI_INTERFACE_INFO if found. NULL if not found.
 *
 * DESCRIPTION: Search for the specified interface name in the global list.
 *              Caller MUST hold AcpiGbl_OsiMutex
 *
 ******************************************************************************/

ACPI_INTERFACE_INFO *
AcpiUtGetInterface (
    ACPI_STRING             InterfaceName)
{
    ACPI_INTERFACE_INFO     *NextInterface;


    NextInterface = AcpiGbl_SupportedInterfaces;
    while (NextInterface)
    {
        if (!ACPI_STRCMP (InterfaceName, NextInterface->Name))
        {
            return (NextInterface);
        }

        NextInterface = NextInterface->Next;
    }

    return (NULL);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtOsiImplementation
 *
 * PARAMETERS:  WalkState           - Current walk state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Implementation of the _OSI predefined control method. When
 *              an invocation of _OSI is encountered in the system AML,
 *              control is transferred to this function.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtOsiImplementation (
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_OPERAND_OBJECT     *StringDesc;
    ACPI_OPERAND_OBJECT     *ReturnDesc;
    ACPI_INTERFACE_INFO     *InterfaceInfo;
    ACPI_INTERFACE_HANDLER  InterfaceHandler;
    ACPI_STATUS             Status;
    UINT32                  ReturnValue;


    ACPI_FUNCTION_TRACE (UtOsiImplementation);


    /* Validate the string input argument (from the AML caller) */

    StringDesc = WalkState->Arguments[0].Object;
    if (!StringDesc ||
        (StringDesc->Common.Type != ACPI_TYPE_STRING))
    {
        return_ACPI_STATUS (AE_TYPE);
    }

    /* Create a return object */

    ReturnDesc = AcpiUtCreateInternalObject (ACPI_TYPE_INTEGER);
    if (!ReturnDesc)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    /* Default return value is 0, NOT SUPPORTED */

    ReturnValue = 0;
    Status = AcpiOsAcquireMutex (AcpiGbl_OsiMutex, ACPI_WAIT_FOREVER);
    if (ACPI_FAILURE (Status))
    {
        AcpiUtDeleteObjectDesc (ReturnDesc);
        return_ACPI_STATUS (Status);
    }

    /* Lookup the interface in the global _OSI list */

    InterfaceInfo = AcpiUtGetInterface (StringDesc->String.Pointer);
    if (InterfaceInfo &&
        !(InterfaceInfo->Flags & ACPI_OSI_INVALID))
    {
        /*
         * The interface is supported.
         * Update the OsiData if necessary. We keep track of the latest
         * version of Windows that has been requested by the BIOS.
         */
        if (InterfaceInfo->Value > AcpiGbl_OsiData)
        {
            AcpiGbl_OsiData = InterfaceInfo->Value;
        }

        ReturnValue = ACPI_UINT32_MAX;
    }

    AcpiOsReleaseMutex (AcpiGbl_OsiMutex);

    /*
     * Invoke an optional _OSI interface handler. The host OS may wish
     * to do some interface-specific handling. For example, warn about
     * certain interfaces or override the true/false support value.
     */
    InterfaceHandler = AcpiGbl_InterfaceHandler;
    if (InterfaceHandler)
    {
        ReturnValue = InterfaceHandler (
            StringDesc->String.Pointer, ReturnValue);
    }

    ACPI_DEBUG_PRINT_RAW ((ACPI_DB_INFO,
        "ACPI: BIOS _OSI(\"%s\") is %ssupported\n",
        StringDesc->String.Pointer, ReturnValue == 0 ? "not " : ""));

    /* Complete the return object */

    ReturnDesc->Integer.Value = ReturnValue;
    WalkState->ReturnDesc = ReturnDesc;
    return_ACPI_STATUS (AE_OK);
}
