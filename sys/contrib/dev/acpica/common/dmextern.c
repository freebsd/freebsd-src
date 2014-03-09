/******************************************************************************
 *
 * Module Name: dmextern - Support for External() ASL statements
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

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/amlcode.h>
#include <contrib/dev/acpica/include/acnamesp.h>
#include <contrib/dev/acpica/include/acdisasm.h>
#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include <stdio.h>
#include <errno.h>


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

#define METHOD_SEPARATORS           " \t,()\n"


/* Local prototypes */

static const char *
AcpiDmGetObjectTypeName (
    ACPI_OBJECT_TYPE        Type);

static char *
AcpiDmNormalizeParentPrefix (
    ACPI_PARSE_OBJECT       *Op,
    char                    *Path);

static void
AcpiDmAddToExternalListFromFile (
    char                    *Path,
    UINT8                   Type,
    UINT32                  Value);


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
    UINT32                  Index = 0;


    if (!Op)
    {
        return (NULL);
    }

    /* Search upwards in the parse tree until we reach the next namespace node */

    Op = Op->Common.Parent;
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

        /* For External() statements, we do not want a leading '\' */

        if (*ParentPath == AML_ROOT_PREFIX)
        {
            Index = 1;
        }
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
    ACPI_STRCPY (Fullpath, &ParentPath[Index]);

    /*
     * Add dot separator
     * (don't need dot if parent fullpath is a single backslash)
     */
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
    BOOLEAN                 Resolved = FALSE;


    if (!Path)
    {
        return;
    }

    if (Type == ACPI_TYPE_METHOD)
    {
        if (Value & 0x80)
        {
            Resolved = TRUE;
        }
        Value &= 0x07;
    }

    /*
     * We don't want External() statements to contain a leading '\'.
     * This prevents duplicate external statements of the form:
     *
     *    External (\ABCD)
     *    External (ABCD)
     *
     * This would cause a compile time error when the disassembled
     * output file is recompiled.
     */
    if ((*Path == AML_ROOT_PREFIX) && (Path[1]))
    {
        Path++;
    }

    /* Externalize the ACPI pathname */

    Status = AcpiNsExternalizeName (ACPI_UINT32_MAX, Path,
                NULL, &ExternalPath);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /*
     * Get the full pathname from the root if "Path" has one or more
     * parent prefixes (^). Note: path will not contain a leading '\'.
     */
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
                    "External method arg count mismatch %s: Current %u, attempted %u",
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
    NewExternal->Resolved = Resolved;
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

    /* Link the new descriptor into the global list, alphabetically ordered */

    NextExternal = AcpiGbl_ExternalList;
    while (NextExternal)
    {
        if (AcpiUtStricmp (NewExternal->Path, NextExternal->Path) < 0)
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
 * FUNCTION:    AcpiDmGetExternalsFromFile
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Process the optional external reference file.
 *
 * Each line in the file should be of the form:
 *      External (<Method namepath>, MethodObj, <ArgCount>)
 *
 * Example:
 *      External (_SB_.PCI0.XHC_.PS0X, MethodObj, 4)
 *
 ******************************************************************************/

void
AcpiDmGetExternalsFromFile (
    void)
{
    FILE                    *ExternalRefFile;
    char                    *Token;
    char                    *MethodName;
    UINT32                  ArgCount;
    UINT32                  ImportCount = 0;


    if (!Gbl_ExternalRefFilename)
    {
        return;
    }

    /* Open the file */

    ExternalRefFile = fopen (Gbl_ExternalRefFilename, "r");
    if (!ExternalRefFile)
    {
        fprintf (stderr, "Could not open external reference file \"%s\"\n",
            Gbl_ExternalRefFilename);
        return;
    }

    /* Each line defines a method */

    while (fgets (StringBuffer, ASL_MSG_BUFFER_SIZE, ExternalRefFile))
    {
        Token = strtok (StringBuffer, METHOD_SEPARATORS);   /* "External" */
        if (!Token) continue;
        if (strcmp (Token, "External")) continue;

        MethodName = strtok (NULL, METHOD_SEPARATORS);      /* Method namepath */
        if (!MethodName) continue;

        Token = strtok (NULL, METHOD_SEPARATORS);           /* "MethodObj" */
        if (!Token) continue;
        if (strcmp (Token, "MethodObj")) continue;

        Token = strtok (NULL, METHOD_SEPARATORS);           /* Arg count */
        if (!Token) continue;

        /* Convert arg count string to an integer */

        errno = 0;
        ArgCount = strtoul (Token, NULL, 0);
        if (errno)
        {
            fprintf (stderr, "Invalid argument count (%s)\n", Token);
            continue;
        }
        if (ArgCount > 7)
        {
            fprintf (stderr, "Invalid argument count (%u)\n", ArgCount);
            continue;
        }

        /* Add this external to the global list */

        AcpiOsPrintf ("%s: Importing method external (%u arguments) %s\n",
            Gbl_ExternalRefFilename, ArgCount, MethodName);

        AcpiDmAddToExternalListFromFile (MethodName, ACPI_TYPE_METHOD, ArgCount | 0x80);
        ImportCount++;
    }

    if (!ImportCount)
    {
        fprintf (stderr, "Did not find any external methods in reference file \"%s\"\n",
            Gbl_ExternalRefFilename);
    }
    else
    {
        /* Add the external(s) to the namespace */

        AcpiDmAddExternalsToNamespace ();

        AcpiOsPrintf ("%s: Imported %u external method definitions\n",
            Gbl_ExternalRefFilename, ImportCount);
    }

    fclose (ExternalRefFile);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmAddToExternalListFromFile
 *
 * PARAMETERS:  Path                - Internal (AML) path to the object
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

static void
AcpiDmAddToExternalListFromFile (
    char                    *Path,
    UINT8                   Type,
    UINT32                  Value)
{
    char                    *InternalPath;
    char                    *ExternalPath;
    ACPI_EXTERNAL_LIST      *NewExternal;
    ACPI_EXTERNAL_LIST      *NextExternal;
    ACPI_EXTERNAL_LIST      *PrevExternal = NULL;
    ACPI_STATUS             Status;
    BOOLEAN                 Resolved = FALSE;


    if (!Path)
    {
        return;
    }

    /* TBD: Add a flags parameter */

    if (Type == ACPI_TYPE_METHOD)
    {
        if (Value & 0x80)
        {
            Resolved = TRUE;
        }
        Value &= 0x07;
    }

    /*
     * We don't want External() statements to contain a leading '\'.
     * This prevents duplicate external statements of the form:
     *
     *    External (\ABCD)
     *    External (ABCD)
     *
     * This would cause a compile time error when the disassembled
     * output file is recompiled.
     */
    if ((*Path == AML_ROOT_PREFIX) && (Path[1]))
    {
        Path++;
    }

    /* Check all existing externals to ensure no duplicates */

    NextExternal = AcpiGbl_ExternalList;
    while (NextExternal)
    {
        if (!ACPI_STRCMP (Path, NextExternal->Path))
        {
            /* Duplicate method, check that the Value (ArgCount) is the same */

            if ((NextExternal->Type == ACPI_TYPE_METHOD) &&
                (NextExternal->Value != Value))
            {
                ACPI_ERROR ((AE_INFO,
                    "(File) External method arg count mismatch %s: Current %u, override to %u",
                    NextExternal->Path, NextExternal->Value, Value));

                /* Override, since new value came from external reference file */

                NextExternal->Value = Value;
            }

            /* Allow upgrade of type from ANY */

            else if (NextExternal->Type == ACPI_TYPE_ANY)
            {
                NextExternal->Type = Type;
                NextExternal->Value = Value;
            }

            return;
        }

        NextExternal = NextExternal->Next;
    }

    /* Get the internal pathname (AML format) */

    Status = AcpiNsInternalizeName (Path, &InternalPath);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Allocate and init a new External() descriptor */

    NewExternal = ACPI_ALLOCATE_ZEROED (sizeof (ACPI_EXTERNAL_LIST));
    if (!NewExternal)
    {
        ACPI_FREE (InternalPath);
        return;
    }

    /* Must copy and normalize the input path */

    AcpiNsExternalizeName (ACPI_UINT32_MAX, InternalPath, NULL, &ExternalPath);

    NewExternal->Path = ExternalPath;
    NewExternal->Type = Type;
    NewExternal->Value = Value;
    NewExternal->Resolved = Resolved;
    NewExternal->Length = (UINT16) ACPI_STRLEN (Path);
    NewExternal->InternalPath = InternalPath;

    /* Set flag to indicate External->InternalPath needs to be freed */

    NewExternal->Flags |= ACPI_IPATH_ALLOCATED | ACPI_FROM_REFERENCE_FILE;

    /* Link the new descriptor into the global list, alphabetically ordered */

    NextExternal = AcpiGbl_ExternalList;
    while (NextExternal)
    {
        if (AcpiUtStricmp (NewExternal->Path, NextExternal->Path) < 0)
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
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_EXTERNAL_LIST      *External = AcpiGbl_ExternalList;


    while (External)
    {
        /* Add the external name (object) into the namespace */

        Status = AcpiNsLookup (NULL, External->InternalPath, External->Type,
                   ACPI_IMODE_LOAD_PASS1,
                   ACPI_NS_ERROR_IF_FOUND | ACPI_NS_EXTERNAL | ACPI_NS_DONT_OPEN_SCOPE,
                   NULL, &Node);

        if (ACPI_FAILURE (Status))
        {
            ACPI_EXCEPTION ((AE_INFO, Status,
                "while adding external to namespace [%s]",
                External->Path));
        }

        else switch (External->Type)
        {
        case ACPI_TYPE_METHOD:

            /* For methods, we need to save the argument count */

            ObjDesc = AcpiUtCreateInternalObject (ACPI_TYPE_METHOD);
            ObjDesc->Method.ParamCount = (UINT8) External->Value;
            Node->Object = ObjDesc;
            break;

        case ACPI_TYPE_REGION:

            /* Regions require a region sub-object */

            ObjDesc = AcpiUtCreateInternalObject (ACPI_TYPE_REGION);
            ObjDesc->Region.Node = Node;
            Node->Object = ObjDesc;
            break;

        default:

            break;
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
     * Determine the number of control methods in the external list, and
     * also how many of those externals were resolved via the namespace.
     */
    NextExternal = AcpiGbl_ExternalList;
    while (NextExternal)
    {
        if (NextExternal->Type == ACPI_TYPE_METHOD)
        {
            AcpiGbl_NumExternalMethods++;
            if (NextExternal->Resolved)
            {
                AcpiGbl_ResolvedExternalMethods++;
            }
        }

        NextExternal = NextExternal->Next;
    }

    /* Check if any control methods were unresolved */

    AcpiDmUnresolvedWarning (1);

    /* Emit any unresolved method externals in a single text block */

    NextExternal = AcpiGbl_ExternalList;
    while (NextExternal)
    {
        if ((NextExternal->Type == ACPI_TYPE_METHOD) &&
            (!NextExternal->Resolved))
        {
            AcpiOsPrintf ("    External (%s%s",
                NextExternal->Path,
                AcpiDmGetObjectTypeName (NextExternal->Type));

            AcpiOsPrintf (
                ")    // Warning: Unresolved Method, "
                "guessing %u arguments (may be incorrect, see warning above)\n",
                NextExternal->Value);

            NextExternal->Emitted = TRUE;
        }

        NextExternal = NextExternal->Next;
    }

    AcpiOsPrintf ("\n");


    /* Emit externals that were imported from a file */

    if (Gbl_ExternalRefFilename)
    {
        AcpiOsPrintf (
            "    /*\n     * External declarations that were imported from\n"
            "     * the reference file [%s]\n     */\n",
            Gbl_ExternalRefFilename);

        NextExternal = AcpiGbl_ExternalList;
        while (NextExternal)
        {
            if (!NextExternal->Emitted && (NextExternal->Flags & ACPI_FROM_REFERENCE_FILE))
            {
                AcpiOsPrintf ("    External (%s%s",
                    NextExternal->Path,
                    AcpiDmGetObjectTypeName (NextExternal->Type));

                if (NextExternal->Type == ACPI_TYPE_METHOD)
                {
                    AcpiOsPrintf (")    // %u Arguments\n",
                        NextExternal->Value);
                }
                else
                {
                    AcpiOsPrintf (")\n");
                }
                NextExternal->Emitted = TRUE;
            }

            NextExternal = NextExternal->Next;
        }

        AcpiOsPrintf ("\n");
    }

    /*
     * Walk the list of externals found during the AML parsing
     */
    while (AcpiGbl_ExternalList)
    {
        if (!AcpiGbl_ExternalList->Emitted)
        {
            AcpiOsPrintf ("    External (%s%s",
                AcpiGbl_ExternalList->Path,
                AcpiDmGetObjectTypeName (AcpiGbl_ExternalList->Type));

            /* For methods, add a comment with the number of arguments */

            if (AcpiGbl_ExternalList->Type == ACPI_TYPE_METHOD)
            {
                AcpiOsPrintf (")    // %u Arguments\n",
                    AcpiGbl_ExternalList->Value);
            }
            else
            {
                AcpiOsPrintf (")\n");
            }
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


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmUnresolvedWarning
 *
 * PARAMETERS:  Type                - Where to output the warning.
 *                                    0 means write to stderr
 *                                    1 means write to AcpiOsPrintf
 *
 * RETURN:      None
 *
 * DESCRIPTION: Issue warning message if there are unresolved external control
 *              methods within the disassembly.
 *
 ******************************************************************************/

#if 0
Summary of the external control method problem:

When the -e option is used with disassembly, the various SSDTs are simply
loaded into a global namespace for the disassembler to use in order to
resolve control method references (invocations).

The disassembler tracks any such references, and will emit an External()
statement for these types of methods, with the proper number of arguments .

Without the SSDTs, the AML does not contain enough information to properly
disassemble the control method invocation -- because the disassembler does
not know how many arguments to parse.

An example: Assume we have two control methods. ABCD has one argument, and
EFGH has zero arguments. Further, we have two additional control methods
that invoke ABCD and EFGH, named T1 and T2:

    Method (ABCD, 1)
    {
    }
    Method (EFGH, 0)
    {
    }
    Method (T1)
    {
        ABCD (Add (2, 7, Local0))
    }
    Method (T2)
    {
        EFGH ()
        Add (2, 7, Local0)
    }

Here is the AML code that is generated for T1 and T2:

     185:      Method (T1)

0000034C:  14 10 54 31 5F 5F 00 ...    "..T1__."

     186:      {
     187:          ABCD (Add (2, 7, Local0))

00000353:  41 42 43 44 ............    "ABCD"
00000357:  72 0A 02 0A 07 60 ......    "r....`"

     188:      }

     190:      Method (T2)

0000035D:  14 10 54 32 5F 5F 00 ...    "..T2__."

     191:      {
     192:          EFGH ()

00000364:  45 46 47 48 ............    "EFGH"

     193:          Add (2, 7, Local0)

00000368:  72 0A 02 0A 07 60 ......    "r....`"
     194:      }

Note that the AML code for T1 and T2 is essentially identical. When
disassembling this code, the methods ABCD and EFGH must be known to the
disassembler, otherwise it does not know how to handle the method invocations.

In other words, if ABCD and EFGH are actually external control methods
appearing in an SSDT, the disassembler does not know what to do unless
the owning SSDT has been loaded via the -e option.
#endif

void
AcpiDmUnresolvedWarning (
    UINT8                   Type)
{

    if (!AcpiGbl_NumExternalMethods)
    {
        return;
    }

    if (Type)
    {
        if (!AcpiGbl_ExternalFileList)
        {
            /* The -e option was not specified */

           AcpiOsPrintf ("    /*\n"
                "     * iASL Warning: There were %u external control methods found during\n"
                "     * disassembly, but additional ACPI tables to resolve these externals\n"
                "     * were not specified. This resulting disassembler output file may not\n"
                "     * compile because the disassembler did not know how many arguments\n"
                "     * to assign to these methods. To specify the tables needed to resolve\n"
                "     * external control method references, use the one of the following\n"
                "     * example iASL invocations:\n"
                "     *     iasl -e <ssdt1.aml,ssdt2.aml...> -d <dsdt.aml>\n"
                "     *     iasl -e <dsdt.aml,ssdt2.aml...> -d <ssdt1.aml>\n"
                "     */\n",
                AcpiGbl_NumExternalMethods);
        }
        else if (AcpiGbl_NumExternalMethods != AcpiGbl_ResolvedExternalMethods)
        {
            /* The -e option was specified, but there are still some unresolved externals */

            AcpiOsPrintf ("    /*\n"
                "     * iASL Warning: There were %u external control methods found during\n"
                "     * disassembly, but only %u %s resolved (%u unresolved). Additional\n"
                "     * ACPI tables are required to properly disassemble the code. This\n"
                "     * resulting disassembler output file may not compile because the\n"
                "     * disassembler did not know how many arguments to assign to the\n"
                "     * unresolved methods.\n"
                "     */\n",
                AcpiGbl_NumExternalMethods, AcpiGbl_ResolvedExternalMethods,
                (AcpiGbl_ResolvedExternalMethods > 1 ? "were" : "was"),
                (AcpiGbl_NumExternalMethods - AcpiGbl_ResolvedExternalMethods));
        }
    }
    else
    {
        if (!AcpiGbl_ExternalFileList)
        {
            /* The -e option was not specified */

            fprintf (stderr, "\n"
                "iASL Warning: There were %u external control methods found during\n"
                "disassembly, but additional ACPI tables to resolve these externals\n"
                "were not specified. The resulting disassembler output file may not\n"
                "compile because the disassembler did not know how many arguments\n"
                "to assign to these methods. To specify the tables needed to resolve\n"
                "external control method references, use the one of the following\n"
                "example iASL invocations:\n"
                "    iasl -e <ssdt1.aml,ssdt2.aml...> -d <dsdt.aml>\n"
                "    iasl -e <dsdt.aml,ssdt2.aml...> -d <ssdt1.aml>\n",
                AcpiGbl_NumExternalMethods);
        }
        else if (AcpiGbl_NumExternalMethods != AcpiGbl_ResolvedExternalMethods)
        {
            /* The -e option was specified, but there are still some unresolved externals */

            fprintf (stderr, "\n"
                "iASL Warning: There were %u external control methods found during\n"
                "disassembly, but only %u %s resolved (%u unresolved). Additional\n"
                "ACPI tables are required to properly disassemble the code. The\n"
                "resulting disassembler output file may not compile because the\n"
                "disassembler did not know how many arguments to assign to the\n"
                "unresolved methods.\n",
                AcpiGbl_NumExternalMethods, AcpiGbl_ResolvedExternalMethods,
                (AcpiGbl_ResolvedExternalMethods > 1 ? "were" : "was"),
                (AcpiGbl_NumExternalMethods - AcpiGbl_ResolvedExternalMethods));
        }
    }
}
