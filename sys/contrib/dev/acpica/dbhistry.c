/******************************************************************************
 *
 * Module Name: dbhistry - debugger HISTORY command
 *              $Revision: 29 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2004, Intel Corp.
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


#include "acpi.h"
#include "acdebug.h"

#ifdef ACPI_DEBUGGER

#define _COMPONENT          ACPI_CA_DEBUGGER
        ACPI_MODULE_NAME    ("dbhistry")


#define HI_NO_HISTORY       0
#define HI_RECORD_HISTORY   1
#define HISTORY_SIZE        20


typedef struct HistoryInfo
{
    char                    Command[80];
    UINT32                  CmdNum;

} HISTORY_INFO;


static HISTORY_INFO         AcpiGbl_HistoryBuffer[HISTORY_SIZE];
static UINT16               AcpiGbl_LoHistory = 0;
static UINT16               AcpiGbl_NumHistory = 0;
static UINT16               AcpiGbl_NextHistoryIndex = 0;
static UINT32               AcpiGbl_NextCmdNum = 1;


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

    /* Put command into the next available slot */

    ACPI_STRCPY (AcpiGbl_HistoryBuffer[AcpiGbl_NextHistoryIndex].Command, CommandLine);

    AcpiGbl_HistoryBuffer[AcpiGbl_NextHistoryIndex].CmdNum = AcpiGbl_NextCmdNum;

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
AcpiDbDisplayHistory (void)
{
    ACPI_NATIVE_UINT        i;
    UINT16                  HistoryIndex;


    HistoryIndex = AcpiGbl_LoHistory;

    /* Dump entire history buffer */

    for (i = 0; i < AcpiGbl_NumHistory; i++)
    {
        AcpiOsPrintf ("%ld  %s\n", AcpiGbl_HistoryBuffer[HistoryIndex].CmdNum,
                                   AcpiGbl_HistoryBuffer[HistoryIndex].Command);

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
 * RETURN:      None
 *
 * DESCRIPTION: Get a command from the history buffer
 *
 ******************************************************************************/

char *
AcpiDbGetFromHistory (
    char                    *CommandNumArg)
{
    ACPI_NATIVE_UINT        i;
    UINT16                  HistoryIndex;
    UINT32                  CmdNum;


    if (CommandNumArg == NULL)
    {
        CmdNum = AcpiGbl_NextCmdNum - 1;
    }

    else
    {
        CmdNum = ACPI_STRTOUL (CommandNumArg, NULL, 0);
    }

    /* Search history buffer */

    HistoryIndex = AcpiGbl_LoHistory;
    for (i = 0; i < AcpiGbl_NumHistory; i++)
    {
        if (AcpiGbl_HistoryBuffer[HistoryIndex].CmdNum == CmdNum)
        {
            /* Found the commnad, return it */

            return (AcpiGbl_HistoryBuffer[HistoryIndex].Command);
        }


        HistoryIndex++;
        if (HistoryIndex >= HISTORY_SIZE)
        {
            HistoryIndex = 0;
        }
    }

    AcpiOsPrintf ("Invalid history number: %d\n", HistoryIndex);
    return (NULL);
}


#endif /* ACPI_DEBUGGER */

