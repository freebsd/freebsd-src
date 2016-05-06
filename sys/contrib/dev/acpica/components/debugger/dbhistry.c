/******************************************************************************
 *
 * Module Name: dbhistry - debugger HISTORY command
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2016, Intel Corp.
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

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acdebug.h>


#define _COMPONENT          ACPI_CA_DEBUGGER
        ACPI_MODULE_NAME    ("dbhistry")


#define HI_NO_HISTORY       0
#define HI_RECORD_HISTORY   1
#define HISTORY_SIZE        40


typedef struct HistoryInfo
{
    char                    *Command;
    UINT32                  CmdNum;

} HISTORY_INFO;


static HISTORY_INFO         AcpiGbl_HistoryBuffer[HISTORY_SIZE];
static UINT16               AcpiGbl_LoHistory = 0;
static UINT16               AcpiGbl_NumHistory = 0;
static UINT16               AcpiGbl_NextHistoryIndex = 0;
UINT32                      AcpiGbl_NextCmdNum = 1;


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbAddToHistory
 *
 * PARAMETERS:  CommandLine     - Command to add
 *
 * RETURN:      None
 *
 * DESCRIPTION: Add a command line to the history buffer.
 *
 ******************************************************************************/

void
AcpiDbAddToHistory (
    char                    *CommandLine)
{
    UINT16                  CmdLen;
    UINT16                  BufferLen;

    /* Put command into the next available slot */

    CmdLen = (UINT16) strlen (CommandLine);
    if (!CmdLen)
    {
        return;
    }

    if (AcpiGbl_HistoryBuffer[AcpiGbl_NextHistoryIndex].Command != NULL)
    {
        BufferLen = (UINT16) strlen (
            AcpiGbl_HistoryBuffer[AcpiGbl_NextHistoryIndex].Command);

        if (CmdLen > BufferLen)
        {
            AcpiOsFree (AcpiGbl_HistoryBuffer[AcpiGbl_NextHistoryIndex].
                Command);
            AcpiGbl_HistoryBuffer[AcpiGbl_NextHistoryIndex].Command =
                AcpiOsAllocate (CmdLen + 1);
        }
    }
    else
    {
        AcpiGbl_HistoryBuffer[AcpiGbl_NextHistoryIndex].Command =
            AcpiOsAllocate (CmdLen + 1);
    }

    strcpy (AcpiGbl_HistoryBuffer[AcpiGbl_NextHistoryIndex].Command,
        CommandLine);

    AcpiGbl_HistoryBuffer[AcpiGbl_NextHistoryIndex].CmdNum =
        AcpiGbl_NextCmdNum;

    /* Adjust indexes */

    if ((AcpiGbl_NumHistory == HISTORY_SIZE) &&
        (AcpiGbl_NextHistoryIndex == AcpiGbl_LoHistory))
    {
        AcpiGbl_LoHistory++;
        if (AcpiGbl_LoHistory >= HISTORY_SIZE)
        {
            AcpiGbl_LoHistory = 0;
        }
    }

    AcpiGbl_NextHistoryIndex++;
    if (AcpiGbl_NextHistoryIndex >= HISTORY_SIZE)
    {
        AcpiGbl_NextHistoryIndex = 0;
    }

    AcpiGbl_NextCmdNum++;
    if (AcpiGbl_NumHistory < HISTORY_SIZE)
    {
        AcpiGbl_NumHistory++;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbDisplayHistory
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display the contents of the history buffer
 *
 ******************************************************************************/

void
AcpiDbDisplayHistory (
    void)
{
    UINT32                  i;
    UINT16                  HistoryIndex;


    HistoryIndex = AcpiGbl_LoHistory;

    /* Dump entire history buffer */

    for (i = 0; i < AcpiGbl_NumHistory; i++)
    {
        if (AcpiGbl_HistoryBuffer[HistoryIndex].Command)
        {
            AcpiOsPrintf ("%3ld  %s\n",
                AcpiGbl_HistoryBuffer[HistoryIndex].CmdNum,
                AcpiGbl_HistoryBuffer[HistoryIndex].Command);
        }

        HistoryIndex++;
        if (HistoryIndex >= HISTORY_SIZE)
        {
            HistoryIndex = 0;
        }
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbGetFromHistory
 *
 * PARAMETERS:  CommandNumArg           - String containing the number of the
 *                                        command to be retrieved
 *
 * RETURN:      Pointer to the retrieved command. Null on error.
 *
 * DESCRIPTION: Get a command from the history buffer
 *
 ******************************************************************************/

char *
AcpiDbGetFromHistory (
    char                    *CommandNumArg)
{
    UINT32                  CmdNum;


    if (CommandNumArg == NULL)
    {
        CmdNum = AcpiGbl_NextCmdNum - 1;
    }

    else
    {
        CmdNum = strtoul (CommandNumArg, NULL, 0);
    }

    return (AcpiDbGetHistoryByIndex (CmdNum));
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbGetHistoryByIndex
 *
 * PARAMETERS:  CmdNum              - Index of the desired history entry.
 *                                    Values are 0...(AcpiGbl_NextCmdNum - 1)
 *
 * RETURN:      Pointer to the retrieved command. Null on error.
 *
 * DESCRIPTION: Get a command from the history buffer
 *
 ******************************************************************************/

char *
AcpiDbGetHistoryByIndex (
    UINT32                  CmdNum)
{
    UINT32                  i;
    UINT16                  HistoryIndex;


    /* Search history buffer */

    HistoryIndex = AcpiGbl_LoHistory;
    for (i = 0; i < AcpiGbl_NumHistory; i++)
    {
        if (AcpiGbl_HistoryBuffer[HistoryIndex].CmdNum == CmdNum)
        {
            /* Found the command, return it */

            return (AcpiGbl_HistoryBuffer[HistoryIndex].Command);
        }

        /* History buffer is circular */

        HistoryIndex++;
        if (HistoryIndex >= HISTORY_SIZE)
        {
            HistoryIndex = 0;
        }
    }

    AcpiOsPrintf ("Invalid history number: %u\n", HistoryIndex);
    return (NULL);
}
