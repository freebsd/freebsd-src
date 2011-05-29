/******************************************************************************
 *
 * Module Name: dmextern - Support for External() ASL statements
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

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/amlcode.h>
#include <contrib/dev/acpica/include/acnamesp.h>
#include <contrib/dev/acpica/include/acdisasm.h>


/*
 * This module is used for application-level code (iASL disassembler) only.
 *
 * It contains the code to create and emit any necessary External() ASL
 * statements for the module being disassembled.
 */
#define _COMPONENT          ACPI_CA_DISASSEMBLER
        ACPI_MODULE_NAME    ("dmextern")


/*
 * This table maps ACPI_OBJECT_TYPEs to the corresponding ASL
 * ObjectTypeKeyword. Used to generate typed external declarations
 */
static const char           *AcpiGbl_DmTypeNames[] =
{
    /* 00 */ "",                    /* Type ANY */
    /* 01 */ ", IntObj",
    /* 02 */ ", StrObj",
    /* 03 */ ", BuffObj",
    /* 04 */ ", PkgObj",
    /* 05 */ ", FieldUnitObj",
    /* 06 */ ", DeviceObj",
    /* 07 */ ", EventObj",
    /* 08 */ ", MethodObj",
    /* 09 */ ", MutexObj",
    /* 10 */ ", OpRegionObj",
    /* 11 */ ", PowerResObj",
    /* 12 */ ", ProcessorObj",
    /* 13 */ ", ThermalZoneObj",
    /* 14 */ ", BuffFieldObj",
    /* 15 */ ", DDBHandleObj",
    /* 16 */ "",                    /* Debug object */
    /* 17 */ ", FieldUnitObj",
    /* 18 */ ", FieldUnitObj",
    /* 19 */ ", FieldUnitObj"
};


/* Local prototypes */

static const char *
AcpiDmGetObjectTypeName (
    ACPI_OBJECT_TYPE        Type);

static char *
AcpiDmNormalizeParentPrefix (
    ACPI_PARSE_OBJECT       *Op,
    char                    *Path);


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmGetObjectTypeName
 *
 * PARAMETERS:  Type                - An ACPI_OBJECT_TYPE
 *
 * RETURN:      Pointer to a string
 *
 * DESCRIPTION: Map an object type to the ASL object type string.
 *
 ******************************************************************************/

static const char *
AcpiDmGetObjectTypeName (
    ACPI_OBJECT_TYPE        Type)
{

    if (Type == ACPI_TYPE_LOCAL_SCOPE)
    {
        Type = ACPI_TYPE_DEVICE;
    }

    else if (Type > ACPI_TYPE_LOCAL_INDEX_FIELD)
    {
        return ("");
    }

    return (AcpiGbl_DmTypeNames[Type]);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmNormalizeParentPrefix
 *
 * PARAMETERS:  Op                  - Parse op
 *              Path                - Path with parent prefix
 *
 * RETURN:      The full pathname to the object (from the namespace root)
 *
 * DESCRIPTION: Returns the full pathname of a path with parent prefix
 *              The caller must free the fullpath returned.
 *
 ******************************************************************************/

static char *
AcpiDmNormalizeParentPrefix (
    ACPI_PARSE_OBJECT       *Op,
    char                    *Path)
{
    ACPI_NAMESPACE_NODE     *Node;
    char                    *Fullpath;
    char                    *ParentPath;
    ACPI_SIZE               Length;


    /* Search upwards in the parse tree until we reach a namespace node */

    while (Op)
    {
        if (Op->Common.Node)
        {
            break;
        }

        Op = Op->Common.Parent;
    }

    if (!Op)
    {
        return (NULL);
    }

    /*
     * Find the actual parent node for the reference:
     * Remove all carat prefixes from the input path.
     * There may be multiple parent prefixes (For example, ^^^M000)
     */
    Node = Op->Common.Node;
    while (Node && (*Path == (UINT8) AML_PARENT_PREFIX))
    {
        Node = Node->Parent;
        Path++;
    }

    if (!Node)
    {
        return (NULL);
    }

    /* Get the full pathname for the parent node */

    ParentPath = AcpiNsGetExternalPathname (Node);
    if (!ParentPath)
    {
        return (NULL);
    }

    Length = (ACPI_STRLEN (ParentPath) + ACPI_STRLEN (Path) + 1);
    if (ParentPath[1])
    {
        /*
         * If ParentPath is not just a simple '\', increment the length
         * for the required dot separator (ParentPath.Path)
         */
        Length++;
    }

    Fullpath = ACPI_ALLOCATE_ZEROED (Length);
    if (!Fullpath)
    {
        goto Cleanup;
    }

    /*
     * Concatenate parent fullpath and path. For example,
     * parent fullpath "\_SB_", Path "^INIT", Fullpath "\_SB_.INIT"
     *
     * Copy the parent path
     */
    ACPI_STRCAT (Fullpath, ParentPath);

    /* Add dot separator (don't need dot if parent fullpath is a single "\") */

    if (ParentPath[1])
    {
        ACPI_STRCAT (Fullpath, ".");
    }

    /* Copy child path (carat parent prefix(es) were skipped above) */

    ACPI_STRCAT (Fullpath, Path);

Cleanup:
    ACPI_FREE (ParentPath);
    return (Fullpath);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmAddToExternalFileList
 *
 * PARAMETERS:  PathList            - Single path or list separated by comma
 *
 * RETURN:      None
 *
 * DESCRIPTION: Add external files to global list
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDmAddToExternalFileList (
    char                    *PathList)
{
    ACPI_EXTERNAL_FILE      *ExternalFile;
    char                    *Path;
    char                    *TmpPath;


    if (!PathList)
    {
        return (AE_OK);
    }

    Path = strtok (PathList, ",");

    while (Path)
    {
        TmpPath = ACPI_ALLOCATE_ZEROED (ACPI_STRLEN (Path) + 1);
        if (!TmpPath)
        {
            return (AE_NO_MEMORY);
        }

        ACPI_STRCPY (TmpPath, Path);

        ExternalFile = ACPI_ALLOCATE_ZEROED (sizeof (ACPI_EXTERNAL_FILE));
        if (!ExternalFile)
        {
            ACPI_FREE (TmpPath);
            return (AE_NO_MEMORY);
        }

        ExternalFile->Path = TmpPath;

        if (AcpiGbl_ExternalFileList)
        {
            ExternalFile->Next = AcpiGbl_ExternalFileList;
        }

        AcpiGbl_ExternalFileList = ExternalFile;
        Path = strtok (NULL, ",");
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmClearExternalFileList
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Clear the external file list
 *
 ******************************************************************************/

void
AcpiDmClearExternalFileList (
    void)
{
    ACPI_EXTERNAL_FILE      *NextExternal;


    while (AcpiGbl_ExternalFileList)
    {
        NextExternal = AcpiGbl_ExternalFileList->Next;
        ACPI_FREE (AcpiGbl_ExternalFileList->Path);
        ACPI_FREE (AcpiGbl_ExternalFileList);
        AcpiGbl_ExternalFileList = NextExternal;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmAddToExternalList
 *
 * PARAMETERS:  Op                  - Current parser Op
 *              Path                - Internal (AML) path to the object
 *              Type                - ACPI object type to be added
 *              Value               - Arg count if adding a Method object
 *
 * RETURN:      None
 *
 * DESCRIPTION: Insert a new name into the global list of Externals which
 *              will in turn be later emitted as an External() declaration
 *              in the disassembled output.
 *
 ******************************************************************************/

void
AcpiDmAddToExternalList (
    ACPI_PARSE_OBJECT       *Op,
    char                    *Path,
    UINT8                   Type,
    UINT32                  Value)
{
    char                    *ExternalPath;
    char                    *Fullpath = NULL;
    ACPI_EXTERNAL_LIST      *NewExternal;
    ACPI_EXTERNAL_LIST      *NextExternal;
    ACPI_EXTERNAL_LIST      *PrevExternal = NULL;
    ACPI_STATUS             Status;


    if (!Path)
    {
        return;
    }

    /* Externalize the ACPI path */

    Status = AcpiNsExternalizeName (ACPI_UINT32_MAX, Path,
                NULL, &ExternalPath);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Get the full pathname from root if "Path" has a parent prefix */

    if (*Path == (UINT8) AML_PARENT_PREFIX)
    {
        Fullpath = AcpiDmNormalizeParentPrefix (Op, ExternalPath);
        if (Fullpath)
        {
            /* Set new external path */

            ACPI_FREE (ExternalPath);
            ExternalPath = Fullpath;
        }
    }

    /* Check all existing externals to ensure no duplicates */

    NextExternal = AcpiGbl_ExternalList;
    while (NextExternal)
    {
        if (!ACPI_STRCMP (ExternalPath, NextExternal->Path))
        {
            /* Duplicate method, check that the Value (ArgCount) is the same */

            if ((NextExternal->Type == ACPI_TYPE_METHOD) &&
                (NextExternal->Value != Value))
            {
                ACPI_ERROR ((AE_INFO,
                    "Argument count mismatch for method %s %u %u",
                    NextExternal->Path, NextExternal->Value, Value));
            }

            /* Allow upgrade of type from ANY */

            else if (NextExternal->Type == ACPI_TYPE_ANY)
            {
                NextExternal->Type = Type;
                NextExternal->Value = Value;
            }

            ACPI_FREE (ExternalPath);
            return;
        }

        NextExternal = NextExternal->Next;
    }

    /* Allocate and init a new External() descriptor */

    NewExternal = ACPI_ALLOCATE_ZEROED (sizeof (ACPI_EXTERNAL_LIST));
    if (!NewExternal)
    {
        ACPI_FREE (ExternalPath);
        return;
    }

    NewExternal->Path = ExternalPath;
    NewExternal->Type = Type;
    NewExternal->Value = Value;
    NewExternal->Length = (UINT16) ACPI_STRLEN (ExternalPath);

    /* Was the external path with parent prefix normalized to a fullpath? */

    if (Fullpath == ExternalPath)
    {
        /* Get new internal path */

        Status = AcpiNsInternalizeName (ExternalPath, &Path);
        if (ACPI_FAILURE (Status))
        {
            ACPI_FREE (ExternalPath);
            ACPI_FREE (NewExternal);
            return;
        }

        /* Set flag to indicate External->InternalPath need to be freed */

        NewExternal->Flags |= ACPI_IPATH_ALLOCATED;
    }

    NewExternal->InternalPath = Path;

    /* Link the new descriptor into the global list, ordered by string length */

    NextExternal = AcpiGbl_ExternalList;
    while (NextExternal)
    {
        if (NewExternal->Length <= NextExternal->Length)
        {
            if (PrevExternal)
            {
                PrevExternal->Next = NewExternal;
            }
            else
            {
                AcpiGbl_ExternalList = NewExternal;
            }

            NewExternal->Next = NextExternal;
            return;
        }

        PrevExternal = NextExternal;
        NextExternal = NextExternal->Next;
    }

    if (PrevExternal)
    {
        PrevExternal->Next = NewExternal;
    }
    else
    {
        AcpiGbl_ExternalList = NewExternal;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmAddExternalsToNamespace
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Add all externals to the namespace. Allows externals to be
 *              "resolved".
 *
 ******************************************************************************/

void
AcpiDmAddExternalsToNamespace (
    void)
{
    ACPI_STATUS             Status;
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_OPERAND_OBJECT     *MethodDesc;
    ACPI_EXTERNAL_LIST      *External = AcpiGbl_ExternalList;


    while (External)
    {
        /* Add the external name (object) into the namespace */

        Status = AcpiNsLookup (NULL, External->InternalPath, External->Type,
                   ACPI_IMODE_LOAD_PASS1,
                   ACPI_NS_EXTERNAL | ACPI_NS_DONT_OPEN_SCOPE,
                   NULL, &Node);

        if (ACPI_FAILURE (Status))
        {
            ACPI_EXCEPTION ((AE_INFO, Status,
                "while adding external to namespace [%s]",
                External->Path));
        }
        else if (External->Type == ACPI_TYPE_METHOD)
        {
            /* For methods, we need to save the argument count */

            MethodDesc = AcpiUtCreateInternalObject (ACPI_TYPE_METHOD);
            MethodDesc->Method.ParamCount = (UINT8) External->Value;
            Node->Object = MethodDesc;
        }

        External = External->Next;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmGetExternalMethodCount
 *
 * PARAMETERS:  None
 *
 * RETURN:      The number of control method externals in the external list
 *
 * DESCRIPTION: Return the number of method externals that have been generated.
 *              If any control method externals have been found, we must
 *              re-parse the entire definition block with the new information
 *              (number of arguments for the methods.) This is limitation of
 *              AML, we don't know the number of arguments from the control
 *              method invocation itself.
 *
 ******************************************************************************/

UINT32
AcpiDmGetExternalMethodCount (
    void)
{
    ACPI_EXTERNAL_LIST      *External = AcpiGbl_ExternalList;
    UINT32                  Count = 0;


    while (External)
    {
        if (External->Type == ACPI_TYPE_METHOD)
        {
            Count++;
        }

        External = External->Next;
    }

    return (Count);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmClearExternalList
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Free the entire External info list
 *
 ******************************************************************************/

void
AcpiDmClearExternalList (
    void)
{
    ACPI_EXTERNAL_LIST      *NextExternal;


    while (AcpiGbl_ExternalList)
    {
        NextExternal = AcpiGbl_ExternalList->Next;
        ACPI_FREE (AcpiGbl_ExternalList->Path);
        ACPI_FREE (AcpiGbl_ExternalList);
        AcpiGbl_ExternalList = NextExternal;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmEmitExternals
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Emit an External() ASL statement for each of the externals in
 *              the global external info list.
 *
 ******************************************************************************/

void
AcpiDmEmitExternals (
    void)
{
    ACPI_EXTERNAL_LIST      *NextExternal;


    if (!AcpiGbl_ExternalList)
    {
        return;
    }

    /*
     * Walk the list of externals (unresolved references)
     * found during the AML parsing
     */
    while (AcpiGbl_ExternalList)
    {
        AcpiOsPrintf ("    External (%s%s",
            AcpiGbl_ExternalList->Path,
            AcpiDmGetObjectTypeName (AcpiGbl_ExternalList->Type));

        if (AcpiGbl_ExternalList->Type == ACPI_TYPE_METHOD)
        {
            AcpiOsPrintf (")    // %u Arguments\n",
                AcpiGbl_ExternalList->Value);
        }
        else
        {
            AcpiOsPrintf (")\n");
        }

        /* Free this external info block and move on to next external */

        NextExternal = AcpiGbl_ExternalList->Next;
        if (AcpiGbl_ExternalList->Flags & ACPI_IPATH_ALLOCATED)
        {
            ACPI_FREE (AcpiGbl_ExternalList->InternalPath);
        }

        ACPI_FREE (AcpiGbl_ExternalList->Path);
        ACPI_FREE (AcpiGbl_ExternalList);
        AcpiGbl_ExternalList = NextExternal;
    }

    AcpiOsPrintf ("\n");
}

