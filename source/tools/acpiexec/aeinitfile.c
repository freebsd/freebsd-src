/******************************************************************************
 *
 * Module Name: aeinitfile - Support for optional initialization file
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

#include "aecommon.h"
#include "acdispat.h"

#define _COMPONENT          ACPI_TOOLS
        ACPI_MODULE_NAME    ("aeinitfile")


/* Local prototypes */

static void
AeDoOneOverride (
    char                    *Pathname,
    char                    *ValueString,
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_WALK_STATE         *WalkState);


#define AE_FILE_BUFFER_SIZE     512

static char                 LineBuffer[AE_FILE_BUFFER_SIZE];
static char                 NameBuffer[AE_FILE_BUFFER_SIZE];
static char                 ValueBuffer[AE_FILE_BUFFER_SIZE];
static FILE                 *InitFile;


/******************************************************************************
 *
 * FUNCTION:    AeOpenInitializationFile
 *
 * PARAMETERS:  Filename            - Path to the init file
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Open the initialization file for the -fi option
 *
 *****************************************************************************/

int
AeOpenInitializationFile (
    char                    *Filename)
{

    InitFile = fopen (Filename, "r");
    if (!InitFile)
    {
        perror ("Could not open initialization file");
        return (-1);
    }

    AcpiOsPrintf ("Opened initialization file [%s]\n", Filename);
    return (0);
}


/******************************************************************************
 *
 * FUNCTION:    AeDoObjectOverrides
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Read the initialization file and perform all overrides
 *
 * NOTE:        The format of the file is multiple lines, each of format:
 *                  <ACPI-pathname> <Integer Value>
 *
 *****************************************************************************/

void
AeDoObjectOverrides (
    void)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_WALK_STATE         *WalkState;
    int                     i;


    if (!InitFile)
    {
        return;
    }

    /* Create needed objects to be reused for each init entry */

    ObjDesc = AcpiUtCreateIntegerObject (0);
    WalkState = AcpiDsCreateWalkState (0, NULL, NULL, NULL);
    NameBuffer[0] = '\\';

    /* Read the entire file line-by-line */

    while (fgets (LineBuffer, AE_FILE_BUFFER_SIZE, InitFile) != NULL)
    {
        if (sscanf (LineBuffer, "%s %s\n",
                &NameBuffer[1], ValueBuffer) != 2)
        {
            goto CleanupAndExit;
        }

        /* Add a root prefix if not present in the string */

        i = 0;
        if (NameBuffer[1] == '\\')
        {
            i = 1;
        }

        AeDoOneOverride (&NameBuffer[i], ValueBuffer, ObjDesc, WalkState);
    }

    /* Cleanup */

CleanupAndExit:
    fclose (InitFile);
    AcpiDsDeleteWalkState (WalkState);
    AcpiUtRemoveReference (ObjDesc);
}


/******************************************************************************
 *
 * FUNCTION:    AeDoOneOverride
 *
 * PARAMETERS:  Pathname            - AML namepath
 *              ValueString         - New integer value to be stored
 *              ObjDesc             - Descriptor with integer override value
 *              WalkState           - Used for the Store operation
 *
 * RETURN:      None
 *
 * DESCRIPTION: Perform an overrided for a single namespace object
 *
 *****************************************************************************/

static void
AeDoOneOverride (
    char                    *Pathname,
    char                    *ValueString,
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_HANDLE             Handle;
    ACPI_STATUS             Status;
    UINT64                  Value;


    AcpiOsPrintf ("Value Override: %s, ", Pathname);

    /*
     * Get the namespace node associated with the override
     * pathname from the init file.
     */
    Status = AcpiGetHandle (NULL, Pathname, &Handle);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("%s\n", AcpiFormatException (Status));
        return;
    }

    /* Extract the 64-bit integer */

    Status = AcpiUtStrtoul64 (ValueString, 0, &Value);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("%s %s\n", ValueString,
            AcpiFormatException (Status));
        return;
    }

    ObjDesc->Integer.Value = Value;

    /*
     * At the point this function is called, the namespace is fully
     * built and initialized. We can simply store the new object to
     * the target node.
     */
    AcpiExEnterInterpreter ();
    Status = AcpiExStore (ObjDesc, Handle, WalkState);
    AcpiExExitInterpreter ();

    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("%s\n", AcpiFormatException (Status));
        return;
    }

    AcpiOsPrintf ("New value: 0x%8.8X%8.8X\n",
        ACPI_FORMAT_UINT64 (Value));
}
