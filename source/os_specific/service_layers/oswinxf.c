/******************************************************************************
 *
 * Module Name: oswinxf - Windows OSL
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2023, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights. You may have additional license terms from the party that provided
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
 * to or modifications of the Original Intel Code. No other license or right
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
 * and the following Disclaimer and Export Compliance provision. In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change. Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee. Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution. In
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
 * HERE. ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT, ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES. INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS. INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES. THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government. In the
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
 *****************************************************************************
 *
 * Alternatively, you may choose to be licensed under the terms of the
 * following license:
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Alternatively, you may choose to be licensed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 *****************************************************************************/

#include "acpi.h"
#include "accommon.h"

#ifdef WIN32
#pragma warning(disable:4115)   /* warning C4115: named type definition in parentheses (caused by rpcasync.h> */

#include <windows.h>
#include <winbase.h>

#elif WIN64
#include <windowsx.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <process.h>
#include <time.h>

#define _COMPONENT          ACPI_OS_SERVICES
        ACPI_MODULE_NAME    ("oswinxf")


UINT64                      TimerFrequency;
char                        TableName[ACPI_NAMESEG_SIZE + 1];

#define ACPI_OS_DEBUG_TIMEOUT   30000 /* 30 seconds */


/* Upcalls to AcpiExec application */

void
AeTableOverride (
    ACPI_TABLE_HEADER       *ExistingTable,
    ACPI_TABLE_HEADER       **NewTable);

/*
 * Real semaphores are only used for a multi-threaded application
 */
#ifndef ACPI_SINGLE_THREADED

/* Semaphore information structure */

typedef struct acpi_os_semaphore_info
{
    UINT16                  MaxUnits;
    UINT16                  CurrentUnits;
    void                    *OsHandle;

} ACPI_OS_SEMAPHORE_INFO;

/* Need enough semaphores to run the large aslts suite */

#define ACPI_OS_MAX_SEMAPHORES  256

ACPI_OS_SEMAPHORE_INFO          AcpiGbl_Semaphores[ACPI_OS_MAX_SEMAPHORES];

#endif /* ACPI_SINGLE_THREADED */

/******************************************************************************
 *
 * FUNCTION:    AcpiOsTerminate
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Nothing to do for windows
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsTerminate (
    void)
{
    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsInitialize
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Init this OSL
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsInitialize (
    void)
{
    ACPI_STATUS             Status;
    LARGE_INTEGER           LocalTimerFrequency;


#ifndef ACPI_SINGLE_THREADED
    /* Clear the semaphore info array */

    memset (AcpiGbl_Semaphores, 0x00, sizeof (AcpiGbl_Semaphores));
#endif

    AcpiGbl_OutputFile = stdout;

    /* Get the timer frequency for use in AcpiOsGetTimer */

    TimerFrequency = 0;
    if (QueryPerformanceFrequency (&LocalTimerFrequency))
    {
        /* Frequency is in ticks per second */

        TimerFrequency = LocalTimerFrequency.QuadPart;
    }

    Status = AcpiOsCreateLock (&AcpiGbl_PrintLock);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    return (AE_OK);
}


#ifndef ACPI_USE_NATIVE_RSDP_POINTER
/******************************************************************************
 *
 * FUNCTION:    AcpiOsGetRootPointer
 *
 * PARAMETERS:  None
 *
 * RETURN:      RSDP physical address
 *
 * DESCRIPTION: Gets the root pointer (RSDP)
 *
 *****************************************************************************/

ACPI_PHYSICAL_ADDRESS
AcpiOsGetRootPointer (
    void)
{

    return (0);
}
#endif


/******************************************************************************
 *
 * FUNCTION:    AcpiOsPredefinedOverride
 *
 * PARAMETERS:  InitVal             - Initial value of the predefined object
 *              NewVal              - The new value for the object
 *
 * RETURN:      Status, pointer to value. Null pointer returned if not
 *              overriding.
 *
 * DESCRIPTION: Allow the OS to override predefined names
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsPredefinedOverride (
    const ACPI_PREDEFINED_NAMES *InitVal,
    ACPI_STRING                 *NewVal)
{

    if (!InitVal || !NewVal)
    {
        return (AE_BAD_PARAMETER);
    }

    *NewVal = NULL;
    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsTableOverride
 *
 * PARAMETERS:  ExistingTable       - Header of current table (probably firmware)
 *              NewTable            - Where an entire new table is returned.
 *
 * RETURN:      Status, pointer to new table. Null pointer returned if no
 *              table is available to override
 *
 * DESCRIPTION: Return a different version of a table if one is available
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsTableOverride (
    ACPI_TABLE_HEADER       *ExistingTable,
    ACPI_TABLE_HEADER       **NewTable)
{

    if (!ExistingTable || !NewTable)
    {
        return (AE_BAD_PARAMETER);
    }

    *NewTable = NULL;


#ifdef ACPI_EXEC_APP

    /* Call back up to AcpiExec */

    AeTableOverride (ExistingTable, NewTable);
#endif

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsPhysicalTableOverride
 *
 * PARAMETERS:  ExistingTable       - Header of current table (probably firmware)
 *              NewAddress          - Where new table address is returned
 *                                    (Physical address)
 *              NewTableLength      - Where new table length is returned
 *
 * RETURN:      Status, address/length of new table. Null pointer returned
 *              if no table is available to override.
 *
 * DESCRIPTION: Returns AE_SUPPORT, function not used in user space.
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsPhysicalTableOverride (
    ACPI_TABLE_HEADER       *ExistingTable,
    ACPI_PHYSICAL_ADDRESS   *NewAddress,
    UINT32                  *NewTableLength)
{

    return (AE_SUPPORT);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsEnterSleep
 *
 * PARAMETERS:  SleepState          - Which sleep state to enter
 *              RegaValue           - Register A value
 *              RegbValue           - Register B value
 *
 * RETURN:      Status
 *
 * DESCRIPTION: A hook before writing sleep registers to enter the sleep
 *              state. Return AE_CTRL_SKIP to skip further sleep register
 *              writes.
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsEnterSleep (
    UINT8                   SleepState,
    UINT32                  RegaValue,
    UINT32                  RegbValue)
{

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsGetTimer
 *
 * PARAMETERS:  None
 *
 * RETURN:      Current ticks in 100-nanosecond units
 *
 * DESCRIPTION: Get the value of a system timer
 *
 ******************************************************************************/

UINT64
AcpiOsGetTimer (
    void)
{
    LARGE_INTEGER           Timer;


    /* Attempt to use hi-granularity timer first */

    if (TimerFrequency &&
        QueryPerformanceCounter (&Timer))
    {
        /* Convert to 100 nanosecond ticks */

        return ((UINT64) ((Timer.QuadPart * (UINT64) ACPI_100NSEC_PER_SEC) /
            TimerFrequency));
    }

    /* Fall back to the lo-granularity timer */

    else
    {
        /* Convert milliseconds to 100 nanosecond ticks */

        return (GetTickCount64() * ACPI_100NSEC_PER_MSEC);
    }
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsReadable
 *
 * PARAMETERS:  Pointer             - Area to be verified
 *              Length              - Size of area
 *
 * RETURN:      TRUE if readable for entire length
 *
 * DESCRIPTION: Verify that a pointer is valid for reading
 *
 *****************************************************************************/

BOOLEAN
AcpiOsReadable (
    void                    *Pointer,
    ACPI_SIZE               Length)
{

    return ((BOOLEAN) !IsBadReadPtr (Pointer, Length));
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsWritable
 *
 * PARAMETERS:  Pointer             - Area to be verified
 *              Length              - Size of area
 *
 * RETURN:      TRUE if writable for entire length
 *
 * DESCRIPTION: Verify that a pointer is valid for writing
 *
 *****************************************************************************/

BOOLEAN
AcpiOsWritable (
    void                    *Pointer,
    ACPI_SIZE               Length)
{

    return ((BOOLEAN) !IsBadWritePtr (Pointer, Length));
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsRedirectOutput
 *
 * PARAMETERS:  Destination         - An open file handle/pointer
 *
 * RETURN:      None
 *
 * DESCRIPTION: Causes redirect of AcpiOsPrintf and AcpiOsVprintf
 *
 *****************************************************************************/

void
AcpiOsRedirectOutput (
    void                    *Destination)
{

    AcpiGbl_OutputFile = Destination;
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsPrintf
 *
 * PARAMETERS:  Fmt, ...            - Standard printf format
 *
 * RETURN:      None
 *
 * DESCRIPTION: Formatted output
 *
 *****************************************************************************/

void ACPI_INTERNAL_VAR_XFACE
AcpiOsPrintf (
    const char              *Fmt,
    ...)
{
    va_list                 Args;
    UINT8                   Flags;


    Flags = AcpiGbl_DbOutputFlags;
    if (Flags & ACPI_DB_REDIRECTABLE_OUTPUT)
    {
        /* Output is directable to either a file (if open) or the console */

        if (AcpiGbl_DebugFile)
        {
            /* Output file is open, send the output there */

            va_start (Args, Fmt);
            vfprintf (AcpiGbl_DebugFile, Fmt, Args);
            va_end (Args);
        }
        else
        {
            /* No redirection, send output to console (once only!) */

            Flags |= ACPI_DB_CONSOLE_OUTPUT;
        }
    }

    if (Flags & ACPI_DB_CONSOLE_OUTPUT)
    {
        va_start (Args, Fmt);
        vfprintf (AcpiGbl_OutputFile, Fmt, Args);
        va_end (Args);
    }

    return;
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsVprintf
 *
 * PARAMETERS:  Fmt                 - Standard printf format
 *              Args                - Argument list
 *
 * RETURN:      None
 *
 * DESCRIPTION: Formatted output with argument list pointer
 *
 *****************************************************************************/

void
AcpiOsVprintf (
    const char              *Fmt,
    va_list                 Args)
{
    INT32                   Count = 0;
    UINT8                   Flags;


    Flags = AcpiGbl_DbOutputFlags;
    if (Flags & ACPI_DB_REDIRECTABLE_OUTPUT)
    {
        /* Output is directable to either a file (if open) or the console */

        if (AcpiGbl_DebugFile)
        {
            /* Output file is open, send the output there */

            Count = vfprintf (AcpiGbl_DebugFile, Fmt, Args);
        }
        else
        {
            /* No redirection, send output to console (once only!) */

            Flags |= ACPI_DB_CONSOLE_OUTPUT;
        }
    }

    if (Flags & ACPI_DB_CONSOLE_OUTPUT)
    {
        Count = vfprintf (AcpiGbl_OutputFile, Fmt, Args);
    }

    return;
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsGetLine
 *
 * PARAMETERS:  Buffer              - Where to return the command line
 *              BufferLength        - Maximum length of Buffer
 *              BytesRead           - Where the actual byte count is returned
 *
 * RETURN:      Status and actual bytes read
 *
 * DESCRIPTION: Formatted input with argument list pointer
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsGetLine (
    char                    *Buffer,
    UINT32                  BufferLength,
    UINT32                  *BytesRead)
{
    int                     Temp;
    UINT32                  i;


    for (i = 0; ; i++)
    {
        if (i >= BufferLength)
        {
            return (AE_BUFFER_OVERFLOW);
        }

        if ((Temp = getchar ()) == EOF)
        {
            return (AE_ERROR);
        }

        if (!Temp || Temp == '\n')
        {
            break;
        }

        Buffer [i] = (char) Temp;
    }

    /* Null terminate the buffer */

    Buffer [i] = 0;

    /* Return the number of bytes in the string */

    if (BytesRead)
    {
        *BytesRead = i;
    }

    return (AE_OK);
}


#ifndef ACPI_USE_NATIVE_MEMORY_MAPPING
/******************************************************************************
 *
 * FUNCTION:    AcpiOsMapMemory
 *
 * PARAMETERS:  Where               - Physical address of memory to be mapped
 *              Length              - How much memory to map
 *
 * RETURN:      Pointer to mapped memory. Null on error.
 *
 * DESCRIPTION: Map physical memory into caller's address space
 *
 *****************************************************************************/

void *
AcpiOsMapMemory (
    ACPI_PHYSICAL_ADDRESS   Where,
    ACPI_SIZE               Length)
{

    return (ACPI_TO_POINTER ((ACPI_SIZE) Where));
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsUnmapMemory
 *
 * PARAMETERS:  Where               - Logical address of memory to be unmapped
 *              Length              - How much memory to unmap
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Delete a previously created mapping. Where and Length must
 *              correspond to a previous mapping exactly.
 *
 *****************************************************************************/

void
AcpiOsUnmapMemory (
    void                    *Where,
    ACPI_SIZE               Length)
{

    return;
}
#endif


/******************************************************************************
 *
 * FUNCTION:    AcpiOsAllocate
 *
 * PARAMETERS:  Size                - Amount to allocate, in bytes
 *
 * RETURN:      Pointer to the new allocation. Null on error.
 *
 * DESCRIPTION: Allocate memory. Algorithm is dependent on the OS.
 *
 *****************************************************************************/

void *
AcpiOsAllocate (
    ACPI_SIZE               Size)
{
    void                    *Mem;


    Mem = (void *) malloc ((size_t) Size);
    return (Mem);
}


#ifdef USE_NATIVE_ALLOCATE_ZEROED
/******************************************************************************
 *
 * FUNCTION:    AcpiOsAllocateZeroed
 *
 * PARAMETERS:  Size                - Amount to allocate, in bytes
 *
 * RETURN:      Pointer to the new allocation. Null on error.
 *
 * DESCRIPTION: Allocate and zero memory. Algorithm is dependent on the OS.
 *
 *****************************************************************************/

void *
AcpiOsAllocateZeroed (
    ACPI_SIZE               Size)
{
    void                    *Mem;


    Mem = (void *) calloc (1, (size_t) Size);
    return (Mem);
}
#endif


/******************************************************************************
 *
 * FUNCTION:    AcpiOsFree
 *
 * PARAMETERS:  Mem                 - Pointer to previously allocated memory
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Free memory allocated via AcpiOsAllocate
 *
 *****************************************************************************/

void
AcpiOsFree (
    void                    *Mem)
{

    free (Mem);
}


#ifdef ACPI_SINGLE_THREADED
/******************************************************************************
 *
 * FUNCTION:    Semaphore stub functions
 *
 * DESCRIPTION: Stub functions used for single-thread applications that do
 *              not require semaphore synchronization. Full implementations
 *              of these functions appear after the stubs.
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsCreateSemaphore (
    UINT32              MaxUnits,
    UINT32              InitialUnits,
    ACPI_HANDLE         *OutHandle)
{
    *OutHandle = (ACPI_HANDLE) 1;
    return (AE_OK);
}

ACPI_STATUS
AcpiOsDeleteSemaphore (
    ACPI_HANDLE         Handle)
{
    return (AE_OK);
}

ACPI_STATUS
AcpiOsWaitSemaphore (
    ACPI_HANDLE         Handle,
    UINT32              Units,
    UINT16              Timeout)
{
    return (AE_OK);
}

ACPI_STATUS
AcpiOsSignalSemaphore (
    ACPI_HANDLE         Handle,
    UINT32              Units)
{
    return (AE_OK);
}

#else
/******************************************************************************
 *
 * FUNCTION:    AcpiOsCreateSemaphore
 *
 * PARAMETERS:  MaxUnits            - Maximum units that can be sent
 *              InitialUnits        - Units to be assigned to the new semaphore
 *              OutHandle           - Where a handle will be returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create an OS semaphore
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsCreateSemaphore (
    UINT32              MaxUnits,
    UINT32              InitialUnits,
    ACPI_SEMAPHORE      *OutHandle)
{
    void                *Mutex;
    UINT32              i;

    ACPI_FUNCTION_NAME (OsCreateSemaphore);


    if (MaxUnits == ACPI_UINT32_MAX)
    {
        MaxUnits = 255;
    }

    if (InitialUnits == ACPI_UINT32_MAX)
    {
        InitialUnits = MaxUnits;
    }

    if (InitialUnits > MaxUnits)
    {
        return (AE_BAD_PARAMETER);
    }

    /* Find an empty slot */

    for (i = 0; i < ACPI_OS_MAX_SEMAPHORES; i++)
    {
        if (!AcpiGbl_Semaphores[i].OsHandle)
        {
            break;
        }
    }
    if (i >= ACPI_OS_MAX_SEMAPHORES)
    {
        ACPI_EXCEPTION ((AE_INFO, AE_LIMIT,
            "Reached max semaphores (%u), could not create",
            ACPI_OS_MAX_SEMAPHORES));
        return (AE_LIMIT);
    }

    /* Create an OS semaphore */

    Mutex = CreateSemaphore (NULL, InitialUnits, MaxUnits, NULL);
    if (!Mutex)
    {
        ACPI_ERROR ((AE_INFO, "Could not create semaphore"));
        return (AE_NO_MEMORY);
    }

    AcpiGbl_Semaphores[i].MaxUnits = (UINT16) MaxUnits;
    AcpiGbl_Semaphores[i].CurrentUnits = (UINT16) InitialUnits;
    AcpiGbl_Semaphores[i].OsHandle = Mutex;

    ACPI_DEBUG_PRINT ((ACPI_DB_MUTEX,
        "Handle=%u, Max=%u, Current=%u, OsHandle=%p\n",
        i, MaxUnits, InitialUnits, Mutex));

    *OutHandle = (void *) i;
    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsDeleteSemaphore
 *
 * PARAMETERS:  Handle              - Handle returned by AcpiOsCreateSemaphore
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Delete an OS semaphore
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsDeleteSemaphore (
    ACPI_SEMAPHORE      Handle)
{
    UINT32              Index = (UINT32) Handle;


    if ((Index >= ACPI_OS_MAX_SEMAPHORES) ||
        !AcpiGbl_Semaphores[Index].OsHandle)
    {
        return (AE_BAD_PARAMETER);
    }

    CloseHandle (AcpiGbl_Semaphores[Index].OsHandle);
    AcpiGbl_Semaphores[Index].OsHandle = NULL;
    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsWaitSemaphore
 *
 * PARAMETERS:  Handle              - Handle returned by AcpiOsCreateSemaphore
 *              Units               - How many units to wait for
 *              Timeout             - How long to wait
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Wait for units
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsWaitSemaphore (
    ACPI_SEMAPHORE      Handle,
    UINT32              Units,
    UINT16              Timeout)
{
    UINT32              Index = (UINT32) Handle;
    UINT32              WaitStatus;
    UINT32              OsTimeout = Timeout;


    ACPI_FUNCTION_ENTRY ();


    if ((Index >= ACPI_OS_MAX_SEMAPHORES) ||
        !AcpiGbl_Semaphores[Index].OsHandle)
    {
        return (AE_BAD_PARAMETER);
    }

    if (Units > 1)
    {
        printf ("WaitSemaphore: Attempt to receive %u units\n", Units);
        return (AE_NOT_IMPLEMENTED);
    }

    if (Timeout == ACPI_WAIT_FOREVER)
    {
        OsTimeout = INFINITE;
        if (AcpiGbl_DebugTimeout)
        {
            /* The debug timeout will prevent hang conditions */

            OsTimeout = ACPI_OS_DEBUG_TIMEOUT;
        }
    }
    else
    {
        /* Add 10ms to account for clock tick granularity */

        OsTimeout += 10;
    }

    WaitStatus = WaitForSingleObject (
        AcpiGbl_Semaphores[Index].OsHandle, OsTimeout);
    if (WaitStatus == WAIT_TIMEOUT)
    {
        if (AcpiGbl_DebugTimeout)
        {
            ACPI_EXCEPTION ((AE_INFO, AE_TIME,
                "Debug timeout on semaphore 0x%04X (%ums)\n",
                Index, ACPI_OS_DEBUG_TIMEOUT));
        }

        return (AE_TIME);
    }

    if (AcpiGbl_Semaphores[Index].CurrentUnits == 0)
    {
        ACPI_ERROR ((AE_INFO,
            "%s - No unit received. Timeout 0x%X, OS_Status 0x%X",
            AcpiUtGetMutexName (Index), Timeout, WaitStatus));

        return (AE_OK);
    }

    AcpiGbl_Semaphores[Index].CurrentUnits--;
    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsSignalSemaphore
 *
 * PARAMETERS:  Handle              - Handle returned by AcpiOsCreateSemaphore
 *              Units               - Number of units to send
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Send units
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsSignalSemaphore (
    ACPI_SEMAPHORE      Handle,
    UINT32              Units)
{
    UINT32              Index = (UINT32) Handle;


    ACPI_FUNCTION_ENTRY ();


    if (Index >= ACPI_OS_MAX_SEMAPHORES)
    {
        printf ("SignalSemaphore: Index/Handle out of range: %2.2X\n", Index);
        return (AE_BAD_PARAMETER);
    }

    if (!AcpiGbl_Semaphores[Index].OsHandle)
    {
        printf ("SignalSemaphore: Null OS handle, Index %2.2X\n", Index);
        return (AE_BAD_PARAMETER);
    }

    if (Units > 1)
    {
        printf ("SignalSemaphore: Attempt to signal %u units, Index %2.2X\n", Units, Index);
        return (AE_NOT_IMPLEMENTED);
    }

    if ((AcpiGbl_Semaphores[Index].CurrentUnits + 1) >
        AcpiGbl_Semaphores[Index].MaxUnits)
    {
        ACPI_ERROR ((AE_INFO,
            "Oversignalled semaphore[%u]! Current %u Max %u",
            Index, AcpiGbl_Semaphores[Index].CurrentUnits,
            AcpiGbl_Semaphores[Index].MaxUnits));

        return (AE_LIMIT);
    }

    AcpiGbl_Semaphores[Index].CurrentUnits++;
    ReleaseSemaphore (AcpiGbl_Semaphores[Index].OsHandle, Units, NULL);

    return (AE_OK);
}

#endif /* ACPI_SINGLE_THREADED */


/******************************************************************************
 *
 * FUNCTION:    Spinlock interfaces
 *
 * DESCRIPTION: Map these interfaces to semaphore interfaces
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsCreateLock (
    ACPI_SPINLOCK           *OutHandle)
{
    return (AcpiOsCreateSemaphore (1, 1, OutHandle));
}

void
AcpiOsDeleteLock (
    ACPI_SPINLOCK           Handle)
{
    AcpiOsDeleteSemaphore (Handle);
}

ACPI_CPU_FLAGS
AcpiOsAcquireLock (
    ACPI_SPINLOCK           Handle)
{
    AcpiOsWaitSemaphore (Handle, 1, 0xFFFF);
    return (0);
}

void
AcpiOsReleaseLock (
    ACPI_SPINLOCK           Handle,
    ACPI_CPU_FLAGS          Flags)
{
    AcpiOsSignalSemaphore (Handle, 1);
}


#if ACPI_FUTURE_IMPLEMENTATION

/* Mutex interfaces, just implement with a semaphore */

ACPI_STATUS
AcpiOsCreateMutex (
    ACPI_MUTEX              *OutHandle)
{
    return (AcpiOsCreateSemaphore (1, 1, OutHandle));
}

void
AcpiOsDeleteMutex (
    ACPI_MUTEX              Handle)
{
    AcpiOsDeleteSemaphore (Handle);
}

ACPI_STATUS
AcpiOsAcquireMutex (
    ACPI_MUTEX              Handle,
    UINT16                  Timeout)
{
    AcpiOsWaitSemaphore (Handle, 1, Timeout);
    return (0);
}

void
AcpiOsReleaseMutex (
    ACPI_MUTEX              Handle)
{
    AcpiOsSignalSemaphore (Handle, 1);
}
#endif


/******************************************************************************
 *
 * FUNCTION:    AcpiOsInstallInterruptHandler
 *
 * PARAMETERS:  InterruptNumber     - Level handler should respond to.
 *              ServiceRoutine      - Address of the ACPI interrupt handler
 *              Context             - User context
 *
 * RETURN:      Handle to the newly installed handler.
 *
 * DESCRIPTION: Install an interrupt handler. Used to install the ACPI
 *              OS-independent handler.
 *
 *****************************************************************************/

UINT32
AcpiOsInstallInterruptHandler (
    UINT32                  InterruptNumber,
    ACPI_OSD_HANDLER        ServiceRoutine,
    void                    *Context)
{

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsRemoveInterruptHandler
 *
 * PARAMETERS:  Handle              - Returned when handler was installed
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Uninstalls an interrupt handler.
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsRemoveInterruptHandler (
    UINT32                  InterruptNumber,
    ACPI_OSD_HANDLER        ServiceRoutine)
{

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsStall
 *
 * PARAMETERS:  Microseconds        - Time to stall
 *
 * RETURN:      None. Blocks until stall is completed.
 *
 * DESCRIPTION: Sleep at microsecond granularity
 *
 *****************************************************************************/

void
AcpiOsStall (
    UINT32                  Microseconds)
{

    Sleep ((Microseconds / ACPI_USEC_PER_MSEC) + 1);
    return;
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsSleep
 *
 * PARAMETERS:  Milliseconds        - Time to sleep
 *
 * RETURN:      None. Blocks until sleep is completed.
 *
 * DESCRIPTION: Sleep at millisecond granularity
 *
 *****************************************************************************/

void
AcpiOsSleep (
    UINT64                  Milliseconds)
{

    /* Add 10ms to account for clock tick granularity */

    Sleep (((unsigned long) Milliseconds) + 10);
    return;
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsReadPciConfiguration
 *
 * PARAMETERS:  PciId               - Seg/Bus/Dev
 *              Register            - Device Register
 *              Value               - Buffer where value is placed
 *              Width               - Number of bits
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Read data from PCI configuration space
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsReadPciConfiguration (
    ACPI_PCI_ID             *PciId,
    UINT32                  Register,
    UINT64                  *Value,
    UINT32                  Width)
{

    *Value = 0;
    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsWritePciConfiguration
 *
 * PARAMETERS:  PciId               - Seg/Bus/Dev
 *              Register            - Device Register
 *              Value               - Value to be written
 *              Width               - Number of bits
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Write data to PCI configuration space
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsWritePciConfiguration (
    ACPI_PCI_ID             *PciId,
    UINT32                  Register,
    UINT64                  Value,
    UINT32                  Width)
{

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsReadPort
 *
 * PARAMETERS:  Address             - Address of I/O port/register to read
 *              Value               - Where value is placed
 *              Width               - Number of bits
 *
 * RETURN:      Value read from port
 *
 * DESCRIPTION: Read data from an I/O port or register
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsReadPort (
    ACPI_IO_ADDRESS         Address,
    UINT32                  *Value,
    UINT32                  Width)
{
    ACPI_FUNCTION_NAME (OsReadPort);


    switch (Width)
    {
    case 8:

        *Value = 0xFF;
        break;

    case 16:

        *Value = 0xFFFF;
        break;

    case 32:

        *Value = 0xFFFFFFFF;
        break;

    default:

        ACPI_ERROR ((AE_INFO, "Bad width parameter: %X", Width));
        return (AE_BAD_PARAMETER);
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsWritePort
 *
 * PARAMETERS:  Address             - Address of I/O port/register to write
 *              Value               - Value to write
 *              Width               - Number of bits
 *
 * RETURN:      None
 *
 * DESCRIPTION: Write data to an I/O port or register
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsWritePort (
    ACPI_IO_ADDRESS         Address,
    UINT32                  Value,
    UINT32                  Width)
{
    ACPI_FUNCTION_NAME (OsWritePort);


    if ((Width == 8) || (Width == 16) || (Width == 32))
    {
        return (AE_OK);
    }

    ACPI_ERROR ((AE_INFO, "Bad width parameter: %X", Width));
    return (AE_BAD_PARAMETER);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsReadMemory
 *
 * PARAMETERS:  Address             - Physical Memory Address to read
 *              Value               - Where value is placed
 *              Width               - Number of bits (8,16,32, or 64)
 *
 * RETURN:      Value read from physical memory address. Always returned
 *              as a 64-bit integer, regardless of the read width.
 *
 * DESCRIPTION: Read data from a physical memory address
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsReadMemory (
    ACPI_PHYSICAL_ADDRESS   Address,
    UINT64                  *Value,
    UINT32                  Width)
{

    switch (Width)
    {
    case 8:
    case 16:
    case 32:
    case 64:

        *Value = 0;
        break;

    default:

        return (AE_BAD_PARAMETER);
        break;
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsWriteMemory
 *
 * PARAMETERS:  Address             - Physical Memory Address to write
 *              Value               - Value to write
 *              Width               - Number of bits (8,16,32, or 64)
 *
 * RETURN:      None
 *
 * DESCRIPTION: Write data to a physical memory address
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsWriteMemory (
    ACPI_PHYSICAL_ADDRESS   Address,
    UINT64                  Value,
    UINT32                  Width)
{

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsSignal
 *
 * PARAMETERS:  Function            - ACPICA signal function code
 *              Info                - Pointer to function-dependent structure
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Miscellaneous functions. Example implementation only.
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsSignal (
    UINT32                  Function,
    void                    *Info)
{

    switch (Function)
    {
    case ACPI_SIGNAL_FATAL:

        break;

    case ACPI_SIGNAL_BREAKPOINT:

        break;

    default:

        break;
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    Local cache interfaces
 *
 * DESCRIPTION: Implements cache interfaces via malloc/free for testing
 *              purposes only.
 *
 *****************************************************************************/

#ifndef ACPI_USE_LOCAL_CACHE

ACPI_STATUS
AcpiOsCreateCache (
    char                    *CacheName,
    UINT16                  ObjectSize,
    UINT16                  MaxDepth,
    ACPI_CACHE_T            **ReturnCache)
{
    ACPI_MEMORY_LIST        *NewCache;


    NewCache = malloc (sizeof (ACPI_MEMORY_LIST));
    if (!NewCache)
    {
        return (AE_NO_MEMORY);
    }

    memset (NewCache, 0, sizeof (ACPI_MEMORY_LIST));
    NewCache->ListName = CacheName;
    NewCache->ObjectSize = ObjectSize;
    NewCache->MaxDepth = MaxDepth;

    *ReturnCache = (ACPI_CACHE_T) NewCache;
    return (AE_OK);
}

ACPI_STATUS
AcpiOsDeleteCache (
    ACPI_CACHE_T            *Cache)
{
    free (Cache);
    return (AE_OK);
}

ACPI_STATUS
AcpiOsPurgeCache (
    ACPI_CACHE_T            *Cache)
{
    return (AE_OK);
}

void *
AcpiOsAcquireObject (
    ACPI_CACHE_T            *Cache)
{
    void                    *NewObject;

    NewObject = malloc (((ACPI_MEMORY_LIST *) Cache)->ObjectSize);
    memset (NewObject, 0, ((ACPI_MEMORY_LIST *) Cache)->ObjectSize);

    return (NewObject);
}

ACPI_STATUS
AcpiOsReleaseObject (
    ACPI_CACHE_T            *Cache,
    void                    *Object)
{
    free (Object);
    return (AE_OK);
}

#endif /* ACPI_USE_LOCAL_CACHE */


/* Optional multi-thread support */

#ifndef ACPI_SINGLE_THREADED
/******************************************************************************
 *
 * FUNCTION:    AcpiOsGetThreadId
 *
 * PARAMETERS:  None
 *
 * RETURN:      Id of the running thread
 *
 * DESCRIPTION: Get the Id of the current (running) thread
 *
 *****************************************************************************/

ACPI_THREAD_ID
AcpiOsGetThreadId (
    void)
{
    DWORD                   ThreadId;

    /* Ensure ID is never 0 */

    ThreadId = GetCurrentThreadId ();
    return ((ACPI_THREAD_ID) (ThreadId + 1));
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsExecute
 *
 * PARAMETERS:  Type                - Type of execution
 *              Function            - Address of the function to execute
 *              Context             - Passed as a parameter to the function
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute a new thread
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsExecute (
    ACPI_EXECUTE_TYPE       Type,
    ACPI_OSD_EXEC_CALLBACK  Function,
    void                    *Context)
{

    _beginthread (Function, (unsigned) 0, Context);
    return (0);
}

#else /* ACPI_SINGLE_THREADED */
ACPI_THREAD_ID
AcpiOsGetThreadId (
    void)
{
    return (1);
}

ACPI_STATUS
AcpiOsExecute (
    ACPI_EXECUTE_TYPE       Type,
    ACPI_OSD_EXEC_CALLBACK  Function,
    void                    *Context)
{

    Function (Context);
    return (AE_OK);
}

#endif /* ACPI_SINGLE_THREADED */


/******************************************************************************
 *
 * FUNCTION:    AcpiOsWaitEventsComplete
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Wait for all asynchronous events to complete. This
 *              implementation does nothing.
 *
 *****************************************************************************/

void
AcpiOsWaitEventsComplete (
    void)
{

    return;
}
