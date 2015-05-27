/******************************************************************************
 *
 * Module Name: osunixxf - UNIX OSL interfaces
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2015, Intel Corp.
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
 * These interfaces are required in order to compile the ASL compiler and the
 * various ACPICA tools under Linux or other Unix-like system.
 */
#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/amlcode.h>
#include <contrib/dev/acpica/include/acparser.h>
#include <contrib/dev/acpica/include/acdebug.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/time.h>
#include <semaphore.h>
#include <pthread.h>
#include <errno.h>

#define _COMPONENT          ACPI_OS_SERVICES
        ACPI_MODULE_NAME    ("osunixxf")


BOOLEAN                        AcpiGbl_DebugTimeout = FALSE;


/* Upcalls to AcpiExec */

void
AeTableOverride (
    ACPI_TABLE_HEADER       *ExistingTable,
    ACPI_TABLE_HEADER       **NewTable);

typedef void* (*PTHREAD_CALLBACK) (void *);

/* Buffer used by AcpiOsVprintf */

#define ACPI_VPRINTF_BUFFER_SIZE    512
#define _ASCII_NEWLINE              '\n'

/* Terminal support for AcpiExec only */

#ifdef ACPI_EXEC_APP
#include <termios.h>

struct termios              OriginalTermAttributes;
int                         TermAttributesWereSet = 0;

ACPI_STATUS
AcpiUtReadLine (
    char                    *Buffer,
    UINT32                  BufferLength,
    UINT32                  *BytesRead);

static void
OsEnterLineEditMode (
    void);

static void
OsExitLineEditMode (
    void);


/******************************************************************************
 *
 * FUNCTION:    OsEnterLineEditMode, OsExitLineEditMode
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Enter/Exit the raw character input mode for the terminal.
 *
 * Interactive line-editing support for the AML debugger. Used with the
 * common/acgetline module.
 *
 * readline() is not used because of non-portability. It is not available
 * on all systems, and if it is, often the package must be manually installed.
 *
 * Therefore, we use the POSIX tcgetattr/tcsetattr and do the minimal line
 * editing that we need in AcpiOsGetLine.
 *
 * If the POSIX tcgetattr/tcsetattr interfaces are unavailable, these
 * calls will also work:
 *     For OsEnterLineEditMode: system ("stty cbreak -echo")
 *     For OsExitLineEditMode:  system ("stty cooked echo")
 *
 *****************************************************************************/

static void
OsEnterLineEditMode (
    void)
{
    struct termios          LocalTermAttributes;


    TermAttributesWereSet = 0;

    /* STDIN must be a terminal */

    if (!isatty (STDIN_FILENO))
    {
        return;
    }

    /* Get and keep the original attributes */

    if (tcgetattr (STDIN_FILENO, &OriginalTermAttributes))
    {
        fprintf (stderr, "Could not get terminal attributes!\n");
        return;
    }

    /* Set the new attributes to enable raw character input */

    memcpy (&LocalTermAttributes, &OriginalTermAttributes,
        sizeof (struct termios));

    LocalTermAttributes.c_lflag &= ~(ICANON | ECHO);
    LocalTermAttributes.c_cc[VMIN] = 1;
    LocalTermAttributes.c_cc[VTIME] = 0;

    if (tcsetattr (STDIN_FILENO, TCSANOW, &LocalTermAttributes))
    {
        fprintf (stderr, "Could not set terminal attributes!\n");
        return;
    }

    TermAttributesWereSet = 1;
}


static void
OsExitLineEditMode (
    void)
{

    if (!TermAttributesWereSet)
    {
        return;
    }

    /* Set terminal attributes back to the original values */

    if (tcsetattr (STDIN_FILENO, TCSANOW, &OriginalTermAttributes))
    {
        fprintf (stderr, "Could not restore terminal attributes!\n");
    }
}


#else

/* These functions are not needed for other ACPICA utilities */

#define OsEnterLineEditMode()
#define OsExitLineEditMode()
#endif


/******************************************************************************
 *
 * FUNCTION:    AcpiOsInitialize, AcpiOsTerminate
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Initialize and terminate this module.
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsInitialize (
    void)
{
    ACPI_STATUS            Status;


    AcpiGbl_OutputFile = stdout;

    OsEnterLineEditMode ();

    Status = AcpiOsCreateLock (&AcpiGbl_PrintLock);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    return (AE_OK);
}

ACPI_STATUS
AcpiOsTerminate (
    void)
{

    OsExitLineEditMode ();
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
 * DESCRIPTION: Gets the ACPI root pointer (RSDP)
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
 * PARAMETERS:  ExistingTable       - Header of current table (probably
 *                                    firmware)
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

    AeTableOverride (ExistingTable, NewTable);
    return (AE_OK);
#else

    return (AE_NO_ACPI_TABLES);
#endif
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
 * PARAMETERS:  fmt, ...            - Standard printf format
 *
 * RETURN:      None
 *
 * DESCRIPTION: Formatted output. Note: very similar to AcpiOsVprintf
 *              (performance), changes should be tracked in both functions.
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
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsVprintf
 *
 * PARAMETERS:  fmt                 - Standard printf format
 *              args                - Argument list
 *
 * RETURN:      None
 *
 * DESCRIPTION: Formatted output with argument list pointer. Note: very
 *              similar to AcpiOsPrintf, changes should be tracked in both
 *              functions.
 *
 *****************************************************************************/

void
AcpiOsVprintf (
    const char              *Fmt,
    va_list                 Args)
{
    UINT8                   Flags;
    char                    Buffer[ACPI_VPRINTF_BUFFER_SIZE];


    /*
     * We build the output string in a local buffer because we may be
     * outputting the buffer twice. Using vfprintf is problematic because
     * some implementations modify the args pointer/structure during
     * execution. Thus, we use the local buffer for portability.
     *
     * Note: Since this module is intended for use by the various ACPICA
     * utilities/applications, we can safely declare the buffer on the stack.
     * Also, This function is used for relatively small error messages only.
     */
    vsnprintf (Buffer, ACPI_VPRINTF_BUFFER_SIZE, Fmt, Args);

    Flags = AcpiGbl_DbOutputFlags;
    if (Flags & ACPI_DB_REDIRECTABLE_OUTPUT)
    {
        /* Output is directable to either a file (if open) or the console */

        if (AcpiGbl_DebugFile)
        {
            /* Output file is open, send the output there */

            fputs (Buffer, AcpiGbl_DebugFile);
        }
        else
        {
            /* No redirection, send output to console (once only!) */

            Flags |= ACPI_DB_CONSOLE_OUTPUT;
        }
    }

    if (Flags & ACPI_DB_CONSOLE_OUTPUT)
    {
        fputs (Buffer, AcpiGbl_OutputFile);
    }
}


#ifndef ACPI_EXEC_APP
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
 * DESCRIPTION: Get the next input line from the terminal. NOTE: For the
 *              AcpiExec utility, we use the acgetline module instead to
 *              provide line-editing and history support.
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsGetLine (
    char                    *Buffer,
    UINT32                  BufferLength,
    UINT32                  *BytesRead)
{
    int                     InputChar;
    UINT32                  EndOfLine;


    /* Standard AcpiOsGetLine for all utilities except AcpiExec */

    for (EndOfLine = 0; ; EndOfLine++)
    {
        if (EndOfLine >= BufferLength)
        {
            return (AE_BUFFER_OVERFLOW);
        }

        if ((InputChar = getchar ()) == EOF)
        {
            return (AE_ERROR);
        }

        if (!InputChar || InputChar == _ASCII_NEWLINE)
        {
            break;
        }

        Buffer[EndOfLine] = (char) InputChar;
    }

    /* Null terminate the buffer */

    Buffer[EndOfLine] = 0;

    /* Return the number of bytes in the string */

    if (BytesRead)
    {
        *BytesRead = EndOfLine;
    }

    return (AE_OK);
}
#endif


#ifndef ACPI_USE_NATIVE_MEMORY_MAPPING
/******************************************************************************
 *
 * FUNCTION:    AcpiOsMapMemory
 *
 * PARAMETERS:  where               - Physical address of memory to be mapped
 *              length              - How much memory to map
 *
 * RETURN:      Pointer to mapped memory. Null on error.
 *
 * DESCRIPTION: Map physical memory into caller's address space
 *
 *****************************************************************************/

void *
AcpiOsMapMemory (
    ACPI_PHYSICAL_ADDRESS   where,
    ACPI_SIZE               length)
{

    return (ACPI_TO_POINTER ((ACPI_SIZE) where));
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsUnmapMemory
 *
 * PARAMETERS:  where               - Logical address of memory to be unmapped
 *              length              - How much memory to unmap
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Delete a previously created mapping. Where and Length must
 *              correspond to a previous mapping exactly.
 *
 *****************************************************************************/

void
AcpiOsUnmapMemory (
    void                    *where,
    ACPI_SIZE               length)
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
    ACPI_SIZE               size)
{
    void                    *Mem;


    Mem = (void *) malloc ((size_t) size);
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
    ACPI_SIZE               size)
{
    void                    *Mem;


    Mem = (void *) calloc (1, (size_t) size);
    return (Mem);
}
#endif


/******************************************************************************
 *
 * FUNCTION:    AcpiOsFree
 *
 * PARAMETERS:  mem                 - Pointer to previously allocated memory
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Free memory allocated via AcpiOsAllocate
 *
 *****************************************************************************/

void
AcpiOsFree (
    void                    *mem)
{

    free (mem);
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
 * PARAMETERS:  InitialUnits        - Units to be assigned to the new semaphore
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
    ACPI_HANDLE         *OutHandle)
{
    sem_t               *Sem;


    if (!OutHandle)
    {
        return (AE_BAD_PARAMETER);
    }

#ifdef __APPLE__
    {
        char            *SemaphoreName = tmpnam (NULL);

        Sem = sem_open (SemaphoreName, O_EXCL|O_CREAT, 0755, InitialUnits);
        if (!Sem)
        {
            return (AE_NO_MEMORY);
        }
        sem_unlink (SemaphoreName); /* This just deletes the name */
    }

#else
    Sem = AcpiOsAllocate (sizeof (sem_t));
    if (!Sem)
    {
        return (AE_NO_MEMORY);
    }

    if (sem_init (Sem, 0, InitialUnits) == -1)
    {
        AcpiOsFree (Sem);
        return (AE_BAD_PARAMETER);
    }
#endif

    *OutHandle = (ACPI_HANDLE) Sem;
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
    ACPI_HANDLE         Handle)
{
    sem_t               *Sem = (sem_t *) Handle;


    if (!Sem)
    {
        return (AE_BAD_PARAMETER);
    }

    if (sem_destroy (Sem) == -1)
    {
        return (AE_BAD_PARAMETER);
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsWaitSemaphore
 *
 * PARAMETERS:  Handle              - Handle returned by AcpiOsCreateSemaphore
 *              Units               - How many units to wait for
 *              MsecTimeout         - How long to wait (milliseconds)
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Wait for units
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsWaitSemaphore (
    ACPI_HANDLE         Handle,
    UINT32              Units,
    UINT16              MsecTimeout)
{
    ACPI_STATUS         Status = AE_OK;
    sem_t               *Sem = (sem_t *) Handle;
#ifndef ACPI_USE_ALTERNATE_TIMEOUT
    struct timespec     Time;
    int                 RetVal;
#endif


    if (!Sem)
    {
        return (AE_BAD_PARAMETER);
    }

    switch (MsecTimeout)
    {
    /*
     * No Wait:
     * --------
     * A zero timeout value indicates that we shouldn't wait - just
     * acquire the semaphore if available otherwise return AE_TIME
     * (a.k.a. 'would block').
     */
    case 0:

        if (sem_trywait(Sem) == -1)
        {
            Status = (AE_TIME);
        }
        break;

    /* Wait Indefinitely */

    case ACPI_WAIT_FOREVER:

        if (sem_wait (Sem))
        {
            Status = (AE_TIME);
        }
        break;

    /* Wait with MsecTimeout */

    default:

#ifdef ACPI_USE_ALTERNATE_TIMEOUT
        /*
         * Alternate timeout mechanism for environments where
         * sem_timedwait is not available or does not work properly.
         */
        while (MsecTimeout)
        {
            if (sem_trywait (Sem) == 0)
            {
                /* Got the semaphore */
                return (AE_OK);
            }

            if (MsecTimeout >= 10)
            {
                MsecTimeout -= 10;
                usleep (10 * ACPI_USEC_PER_MSEC); /* ten milliseconds */
            }
            else
            {
                MsecTimeout--;
                usleep (ACPI_USEC_PER_MSEC); /* one millisecond */
            }
        }
        Status = (AE_TIME);
#else
        /*
         * The interface to sem_timedwait is an absolute time, so we need to
         * get the current time, then add in the millisecond Timeout value.
         */
        if (clock_gettime (CLOCK_REALTIME, &Time) == -1)
        {
            perror ("clock_gettime");
            return (AE_TIME);
        }

        Time.tv_sec += (MsecTimeout / ACPI_MSEC_PER_SEC);
        Time.tv_nsec += ((MsecTimeout % ACPI_MSEC_PER_SEC) * ACPI_NSEC_PER_MSEC);

        /* Handle nanosecond overflow (field must be less than one second) */

        if (Time.tv_nsec >= ACPI_NSEC_PER_SEC)
        {
            Time.tv_sec += (Time.tv_nsec / ACPI_NSEC_PER_SEC);
            Time.tv_nsec = (Time.tv_nsec % ACPI_NSEC_PER_SEC);
        }

        while (((RetVal = sem_timedwait (Sem, &Time)) == -1) && (errno == EINTR))
        {
            continue;
        }

        if (RetVal != 0)
        {
            if (errno != ETIMEDOUT)
            {
                perror ("sem_timedwait");
            }
            Status = (AE_TIME);
        }
#endif
        break;
    }

    return (Status);
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
    ACPI_HANDLE         Handle,
    UINT32              Units)
{
    sem_t               *Sem = (sem_t *)Handle;


    if (!Sem)
    {
        return (AE_BAD_PARAMETER);
    }

    if (sem_post (Sem) == -1)
    {
        return (AE_LIMIT);
    }

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
    ACPI_HANDLE             Handle)
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


/******************************************************************************
 *
 * FUNCTION:    AcpiOsInstallInterruptHandler
 *
 * PARAMETERS:  InterruptNumber     - Level handler should respond to.
 *              Isr                 - Address of the ACPI interrupt handler
 *              ExceptPtr           - Where status is returned
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
 * PARAMETERS:  microseconds        - Time to sleep
 *
 * RETURN:      Blocks until sleep is completed.
 *
 * DESCRIPTION: Sleep at microsecond granularity
 *
 *****************************************************************************/

void
AcpiOsStall (
    UINT32                  microseconds)
{

    if (microseconds)
    {
        usleep (microseconds);
    }
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsSleep
 *
 * PARAMETERS:  milliseconds        - Time to sleep
 *
 * RETURN:      Blocks until sleep is completed.
 *
 * DESCRIPTION: Sleep at millisecond granularity
 *
 *****************************************************************************/

void
AcpiOsSleep (
    UINT64                  milliseconds)
{

    /* Sleep for whole seconds */

    sleep (milliseconds / ACPI_MSEC_PER_SEC);

    /*
     * Sleep for remaining microseconds.
     * Arg to usleep() is in usecs and must be less than 1,000,000 (1 second).
     */
    usleep ((milliseconds % ACPI_MSEC_PER_SEC) * ACPI_USEC_PER_MSEC);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsGetTimer
 *
 * PARAMETERS:  None
 *
 * RETURN:      Current time in 100 nanosecond units
 *
 * DESCRIPTION: Get the current system time
 *
 *****************************************************************************/

UINT64
AcpiOsGetTimer (
    void)
{
    struct timeval          time;


    /* This timer has sufficient resolution for user-space application code */

    gettimeofday (&time, NULL);

    /* (Seconds * 10^7 = 100ns(10^-7)) + (Microseconds(10^-6) * 10^1 = 100ns) */

    return (((UINT64) time.tv_sec * ACPI_100NSEC_PER_SEC) +
            ((UINT64) time.tv_usec * ACPI_100NSEC_PER_USEC));
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsReadPciConfiguration
 *
 * PARAMETERS:  PciId               - Seg/Bus/Dev
 *              PciRegister         - Device Register
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
    UINT32                  PciRegister,
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
 *              PciRegister         - Device Register
 *              Value               - Value to be written
 *              Width               - Number of bits
 *
 * RETURN:      Status.
 *
 * DESCRIPTION: Write data to PCI configuration space
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsWritePciConfiguration (
    ACPI_PCI_ID             *PciId,
    UINT32                  PciRegister,
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

    return (AE_OK);
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

    return (TRUE);
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

    return (TRUE);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsSignal
 *
 * PARAMETERS:  Function            - ACPI A signal function code
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
 * DESCRIPTION: Get the ID of the current (running) thread
 *
 *****************************************************************************/

ACPI_THREAD_ID
AcpiOsGetThreadId (
    void)
{
    pthread_t               thread;


    thread = pthread_self();
    return (ACPI_CAST_PTHREAD_T (thread));
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsExecute
 *
 * PARAMETERS:  Type                - Type of execution
 *              Function            - Address of the function to execute
 *              Context             - Passed as a parameter to the function
 *
 * RETURN:      Status.
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
    pthread_t               thread;
    int                     ret;


    ret = pthread_create (&thread, NULL, (PTHREAD_CALLBACK) Function, Context);
    if (ret)
    {
        AcpiOsPrintf("Create thread failed");
    }
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
