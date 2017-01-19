/******************************************************************************
 *
 * Module Name: osgendbg - Generic debugger command singalling
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

#include "acpi.h"
#include "accommon.h"
#include "acdebug.h"


#define _COMPONENT          ACPI_CA_DEBUGGER
        ACPI_MODULE_NAME    ("osgendbg")


/* Local prototypes */

static void
AcpiDbRunRemoteDebugger (
    char                    *BatchBuffer);


static ACPI_MUTEX           AcpiGbl_DbCommandReady;
static ACPI_MUTEX           AcpiGbl_DbCommandComplete;
static BOOLEAN              AcpiGbl_DbCommandSignalsInitialized = FALSE;

/******************************************************************************
 *
 * FUNCTION:    AcpiDbRunRemoteDebugger
 *
 * PARAMETERS:  BatchBuffer         - Buffer containing commands running in
 *                                    the batch mode
 *
 * RETURN:      None
 *
 * DESCRIPTION: Run multi-threading debugger remotely
 *
 *****************************************************************************/

static void
AcpiDbRunRemoteDebugger (
    char                    *BatchBuffer)
{
    ACPI_STATUS             Status;
    char                    *Ptr = BatchBuffer;
    char                    *Cmd = Ptr;


    while (!AcpiGbl_DbTerminateLoop)
    {
        if (BatchBuffer)
        {
            if (*Ptr)
            {
                while (*Ptr)
                {
                    if (*Ptr == ',')
                    {
                        /* Convert commas to spaces */
                        *Ptr = ' ';
                    }
                    else if (*Ptr == ';')
                    {
                        *Ptr = '\0';
                        continue;
                    }

                    Ptr++;
                }

                strncpy (AcpiGbl_DbLineBuf, Cmd, ACPI_DB_LINE_BUFFER_SIZE);
                Ptr++;
                Cmd = Ptr;
            }
            else
            {
                return;
            }
        }
        else
        {
            /* Force output to console until a command is entered */

            AcpiDbSetOutputDestination (ACPI_DB_CONSOLE_OUTPUT);

            /* Different prompt if method is executing */

            if (!AcpiGbl_MethodExecuting)
            {
                AcpiOsPrintf ("%1c ", ACPI_DEBUGGER_COMMAND_PROMPT);
            }
            else
            {
                AcpiOsPrintf ("%1c ", ACPI_DEBUGGER_EXECUTE_PROMPT);
            }

            /* Get the user input line */

            Status = AcpiOsGetLine (AcpiGbl_DbLineBuf,
                ACPI_DB_LINE_BUFFER_SIZE, NULL);
            if (ACPI_FAILURE (Status))
            {
                return;
            }
        }

        /*
         * Signal the debug thread that we have a command to execute,
         * and wait for the command to complete.
         */
        AcpiOsReleaseMutex (AcpiGbl_DbCommandReady);

        Status = AcpiOsAcquireMutex (AcpiGbl_DbCommandComplete,
            ACPI_WAIT_FOREVER);
        if (ACPI_FAILURE (Status))
        {
            return;
        }
    }
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsWaitCommandReady
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Negotiate with the debugger foreground thread (the user
 *              thread) to wait the readiness of a command.
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsWaitCommandReady (
    void)
{
    ACPI_STATUS             Status = AE_OK;


    if (AcpiGbl_DebuggerConfiguration == DEBUGGER_MULTI_THREADED)
    {
        Status = AE_TIME;

        while (Status == AE_TIME)
        {
            if (AcpiGbl_DbTerminateLoop)
            {
                Status = AE_CTRL_TERMINATE;
            }
            else
            {
                Status = AcpiOsAcquireMutex (AcpiGbl_DbCommandReady, 1000);
            }
        }
    }
    else
    {
        /* Force output to console until a command is entered */

        AcpiDbSetOutputDestination (ACPI_DB_CONSOLE_OUTPUT);

        /* Different prompt if method is executing */

        if (!AcpiGbl_MethodExecuting)
        {
            AcpiOsPrintf ("%1c ", ACPI_DEBUGGER_COMMAND_PROMPT);
        }
        else
        {
            AcpiOsPrintf ("%1c ", ACPI_DEBUGGER_EXECUTE_PROMPT);
        }

        /* Get the user input line */

        Status = AcpiOsGetLine (AcpiGbl_DbLineBuf,
            ACPI_DB_LINE_BUFFER_SIZE, NULL);
    }

    if (ACPI_FAILURE (Status) && Status != AE_CTRL_TERMINATE)
    {
        ACPI_EXCEPTION ((AE_INFO, Status,
            "While parsing/handling command line"));
    }
    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsNotifyCommandComplete
 *
 * PARAMETERS:  void
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Negotiate with the debugger foreground thread (the user
 *              thread) to notify the completion of a command.
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsNotifyCommandComplete (
    void)
{

    if (AcpiGbl_DebuggerConfiguration == DEBUGGER_MULTI_THREADED)
    {
        AcpiOsReleaseMutex (AcpiGbl_DbCommandComplete);
    }
    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsInitializeDebugger
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Initialize OSPM specific part of the debugger
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsInitializeDebugger (
    void)
{
    ACPI_STATUS             Status;


    /* Create command signals */

    Status = AcpiOsCreateMutex (&AcpiGbl_DbCommandReady);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }
    Status = AcpiOsCreateMutex (&AcpiGbl_DbCommandComplete);
    if (ACPI_FAILURE (Status))
    {
        goto ErrorReady;
    }

    /* Initialize the states of the command signals */

    Status = AcpiOsAcquireMutex (AcpiGbl_DbCommandComplete,
        ACPI_WAIT_FOREVER);
    if (ACPI_FAILURE (Status))
    {
        goto ErrorComplete;
    }
    Status = AcpiOsAcquireMutex (AcpiGbl_DbCommandReady,
        ACPI_WAIT_FOREVER);
    if (ACPI_FAILURE (Status))
    {
        goto ErrorComplete;
    }

    AcpiGbl_DbCommandSignalsInitialized = TRUE;
    return (Status);

ErrorComplete:
    AcpiOsDeleteMutex (AcpiGbl_DbCommandComplete);
ErrorReady:
    AcpiOsDeleteMutex (AcpiGbl_DbCommandReady);
    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsTerminateDebugger
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Terminate signals used by the multi-threading debugger
 *
 *****************************************************************************/

void
AcpiOsTerminateDebugger (
    void)
{

    if (AcpiGbl_DbCommandSignalsInitialized)
    {
        AcpiOsDeleteMutex (AcpiGbl_DbCommandReady);
        AcpiOsDeleteMutex (AcpiGbl_DbCommandComplete);
    }
}


/******************************************************************************
 *
 * FUNCTION:    AcpiRunDebugger
 *
 * PARAMETERS:  BatchBuffer         - Buffer containing commands running in
 *                                    the batch mode
 *
 * RETURN:      None
 *
 * DESCRIPTION: Run a local/remote debugger
 *
 *****************************************************************************/

void
AcpiRunDebugger (
    char                    *BatchBuffer)
{
    /* Check for single or multithreaded debug */

    if (AcpiGbl_DebuggerConfiguration & DEBUGGER_MULTI_THREADED)
    {
        AcpiDbRunRemoteDebugger (BatchBuffer);
    }
    else
    {
        AcpiDbUserCommands ();
    }
}

ACPI_EXPORT_SYMBOL (AcpiRunDebugger)
