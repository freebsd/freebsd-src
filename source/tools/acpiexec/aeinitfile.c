/******************************************************************************
 *
 * Module Name: aeinitfile - Support for optional initialization file
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

#include "aecommon.h"
#include "acdispat.h"
#include "acnamesp.h"


#define _COMPONENT          ACPI_TOOLS
        ACPI_MODULE_NAME    ("aeinitfile")


#define AE_FILE_BUFFER_SIZE     512

static char                 LineBuffer[AE_FILE_BUFFER_SIZE];
static char                 NameBuffer[AE_FILE_BUFFER_SIZE];
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
        fprintf (stderr,
            "Could not open initialization file: %s\n", Filename);
        return (-1);
    }

    AcpiOsPrintf ("Opened initialization file [%s]\n", Filename);
    return (0);
}


/******************************************************************************
 *
 * FUNCTION:    AeProcessInitFile
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Read the initialization file and perform all namespace
 *              initializations. AcpiGbl_InitEntries will be used for all
 *              object initialization.
 *
 * NOTE:        The format of the file is multiple lines, each of format:
 *                  <ACPI-pathname> <New Value>
 *
 *****************************************************************************/

ACPI_STATUS
AeProcessInitFile (
    void)
{
    ACPI_WALK_STATE         *WalkState;
    UINT64                  idx;
    ACPI_STATUS             Status = AE_OK;
    char                    *Token;
    char                    *ValueBuffer;
    char                    *TempNameBuffer;
    ACPI_OBJECT_TYPE        Type;
    ACPI_OBJECT             TempObject;


    if (!InitFile)
    {
        return (AE_OK);
    }

    /* Create needed objects to be reused for each init entry */

    WalkState = AcpiDsCreateWalkState (0, NULL, NULL, NULL);
    NameBuffer[0] = '\\';
    NameBuffer[1] = 0;

    while (fgets (LineBuffer, AE_FILE_BUFFER_SIZE, InitFile) != NULL)
    {
        ++AcpiGbl_InitFileLineCount;
    }
    rewind (InitFile);

    /*
     * Allocate and populate the Gbl_InitEntries array
     */
    AcpiGbl_InitEntries =
        AcpiOsAllocateZeroed (sizeof (INIT_FILE_ENTRY) * AcpiGbl_InitFileLineCount);
    for (idx = 0; fgets (LineBuffer, AE_FILE_BUFFER_SIZE, InitFile); ++idx)
    {
        TempNameBuffer = AcpiDbGetNextToken (LineBuffer, &Token, &Type);
        if (!TempNameBuffer)
        {
            AcpiGbl_InitEntries[idx].Name = NULL;
            continue;
        }

        if (LineBuffer[0] == '\\')
        {
            strcpy (NameBuffer, TempNameBuffer);
        }
        else
        {
            /* Add a root prefix if not present in the string */

            strcpy (NameBuffer + 1, TempNameBuffer);
        }

        AcpiNsNormalizePathname (NameBuffer);
        AcpiGbl_InitEntries[idx].Name =
            AcpiOsAllocateZeroed (strnlen (NameBuffer, AE_FILE_BUFFER_SIZE) + 1);
        strcpy (AcpiGbl_InitEntries[idx].Name, NameBuffer);

        ValueBuffer = AcpiDbGetNextToken (Token, &Token, &Type);
        if (!ValueBuffer)
        {
            AcpiGbl_InitEntries[idx].Value = NULL;
            continue;
        }

        AcpiGbl_InitEntries[idx].Value =
            AcpiOsAllocateZeroed (strnlen (ValueBuffer, AE_FILE_BUFFER_SIZE) + 1);
        strcpy (AcpiGbl_InitEntries[idx].Value, ValueBuffer);

        if (Type == ACPI_TYPE_FIELD_UNIT)
        {
            Status = AcpiDbConvertToObject (ACPI_TYPE_BUFFER, ValueBuffer,
                &TempObject);
        }
        else
        {
            Status = AcpiDbConvertToObject (Type, ValueBuffer, &TempObject);
        }

        if (ACPI_FAILURE (Status))
        {
            AcpiOsPrintf ("%s[%s]: %s\n", NameBuffer, AcpiUtGetTypeName (Type),
                AcpiFormatException (Status));
            goto CleanupAndExit;
        }

        Status = AcpiUtCopyEobjectToIobject (&TempObject,
            &AcpiGbl_InitEntries[idx].ObjDesc);

        /* Cleanup the external object created by DbConvertToObject above */

        if (ACPI_SUCCESS (Status))
        {
            if (Type == ACPI_TYPE_BUFFER || Type == ACPI_TYPE_FIELD_UNIT)
            {
                ACPI_FREE (TempObject.Buffer.Pointer);
            }
            else if (Type == ACPI_TYPE_PACKAGE)
            {
                AcpiDbDeleteObjects (1, &TempObject);
            }
        }
        else
        {
            AcpiOsPrintf ("%s[%s]: %s\n", NameBuffer, AcpiUtGetTypeName (Type),
                AcpiFormatException (Status));
            goto CleanupAndExit;
        }

        /*
         * Initialize the namespace node with the value found in the init file.
         */
        AcpiOsPrintf ("Namespace object init from file: %16s, Value \"%s\", Type %s\n",
            AcpiGbl_InitEntries[idx].Name, AcpiGbl_InitEntries[idx].Value, AcpiUtGetTypeName (Type));
    }

    /* Cleanup */

CleanupAndExit:
    fclose (InitFile);
    AcpiDsDeleteWalkState (WalkState);
    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AeLookupInitFileEntry
 *
 * PARAMETERS:  Pathname            - AML namepath in external format
 *              ObjDesc             - Where the object is returned if it exists
 *
 * RETURN:      Status. AE_OK if a match was found
 *
 * DESCRIPTION: Search the init file for a particular name and its value.
 *
 *****************************************************************************/

ACPI_STATUS
AeLookupInitFileEntry (
    char                    *Pathname,
    ACPI_OPERAND_OBJECT     **ObjDesc)
{
    UINT32                  i;

    ACPI_FUNCTION_TRACE (AeLookupInitFileEntry);

    ACPI_DEBUG_PRINT ((ACPI_DB_NAMES, "Lookup: %s\n", Pathname));

    if (!AcpiGbl_InitEntries)
    {
        return (AE_NOT_FOUND);
    }

    AcpiNsNormalizePathname (Pathname);

    for (i = 0; i < AcpiGbl_InitFileLineCount; ++i)
    {
        if (AcpiGbl_InitEntries[i].Name &&
            !strcmp (AcpiGbl_InitEntries[i].Name, Pathname))
        {
            *ObjDesc = AcpiGbl_InitEntries[i].ObjDesc;
            AcpiGbl_InitEntries[i].IsUsed = TRUE;
            ACPI_DEBUG_PRINT ((ACPI_DB_NAMES, "Found match: %s, %p\n", Pathname, *ObjDesc));
            return_ACPI_STATUS (AE_OK);
        }
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_NAMES, "No match found: %s\n", Pathname));
    return_ACPI_STATUS (AE_NOT_FOUND);
}


/******************************************************************************
 *
 * FUNCTION:    AeDisplayUnusedInitFileItems
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display all init file items that have not been referenced
 *              (i.e., items that have not been found in the namespace).
 *
 *****************************************************************************/

void
AeDisplayUnusedInitFileItems (
    void)
{
    UINT32                  i;


    if (!AcpiGbl_InitEntries)
    {
        return;
    }

    for (i = 0; i < AcpiGbl_InitFileLineCount; ++i)
    {
        if (AcpiGbl_InitEntries[i].Name &&
            !AcpiGbl_InitEntries[i].IsUsed)
        {
            AcpiOsPrintf ("Init file entry not found in namespace "
                "(or is a non-data type): %s\n",
                AcpiGbl_InitEntries[i].Name);
        }
    }
}


/******************************************************************************
 *
 * FUNCTION:    AeDeleteInitFileList
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Delete the global namespace initialization file data
 *
 *****************************************************************************/

void
AeDeleteInitFileList (
    void)
{
    UINT32                  i;


    if (!AcpiGbl_InitEntries)
    {
        return;
    }

    for (i = 0; i < AcpiGbl_InitFileLineCount; ++i)
    {

        if ((AcpiGbl_InitEntries[i].ObjDesc) && (AcpiGbl_InitEntries[i].Value))
        {
            /* Remove one reference on the object (and all subobjects) */

            AcpiUtRemoveReference (AcpiGbl_InitEntries[i].ObjDesc);
        }
    }

    AcpiOsFree (AcpiGbl_InitEntries);
}
