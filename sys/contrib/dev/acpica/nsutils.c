/******************************************************************************
 *
 * Module Name: nsutils - Utilities for accessing ACPI namespace, accessing
 *                        parents and siblings and Scope manipulation
 *              $Revision: 93 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999, 2000, 2001, Intel Corp.
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

#define __NSUTILS_C__

#include "acpi.h"
#include "acnamesp.h"
#include "acinterp.h"
#include "amlcode.h"
#include "actables.h"

#define _COMPONENT          ACPI_NAMESPACE
        MODULE_NAME         ("nsutils")


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsValidRootPrefix
 *
 * PARAMETERS:  Prefix          - Character to be checked
 *
 * RETURN:      TRUE if a valid prefix
 *
 * DESCRIPTION: Check if a character is a valid ACPI Root prefix
 *
 ******************************************************************************/

BOOLEAN
AcpiNsValidRootPrefix (
    NATIVE_CHAR             Prefix)
{

    return ((BOOLEAN) (Prefix == '\\'));
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsValidPathSeparator
 *
 * PARAMETERS:  Sep              - Character to be checked
 *
 * RETURN:      TRUE if a valid path separator
 *
 * DESCRIPTION: Check if a character is a valid ACPI path separator
 *
 ******************************************************************************/

BOOLEAN
AcpiNsValidPathSeparator (
    NATIVE_CHAR             Sep)
{

    return ((BOOLEAN) (Sep == '.'));
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsGetType
 *
 * PARAMETERS:  Handle              - Parent Node to be examined
 *
 * RETURN:      Type field from Node whose handle is passed
 *
 ******************************************************************************/

ACPI_OBJECT_TYPE8
AcpiNsGetType (
    ACPI_NAMESPACE_NODE     *Node)
{
    FUNCTION_TRACE ("NsGetType");


    if (!Node)
    {
        REPORT_WARNING (("NsGetType: Null Node ptr"));
        return_VALUE (ACPI_TYPE_ANY);
    }

    return_VALUE (Node->Type);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsLocal
 *
 * PARAMETERS:  Type            - A namespace object type
 *
 * RETURN:      LOCAL if names must be found locally in objects of the
 *              passed type, 0 if enclosing scopes should be searched
 *
 ******************************************************************************/

UINT32
AcpiNsLocal (
    ACPI_OBJECT_TYPE8       Type)
{
    FUNCTION_TRACE ("NsLocal");


    if (!AcpiUtValidObjectType (Type))
    {
        /* Type code out of range  */

        REPORT_WARNING (("NsLocal: Invalid Object Type\n"));
        return_VALUE (NSP_NORMAL);
    }

    return_VALUE ((UINT32) AcpiGbl_NsProperties[Type] & NSP_LOCAL);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsGetInternalNameLength
 *
 * PARAMETERS:  Info            - Info struct initialized with the
 *                                external name pointer.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Calculate the length of the internal (AML) namestring
 *              corresponding to the external (ASL) namestring.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiNsGetInternalNameLength (
    ACPI_NAMESTRING_INFO    *Info)
{
    NATIVE_CHAR             *NextExternalChar;
    UINT32                  i;


    FUNCTION_ENTRY ();


    NextExternalChar = Info->ExternalName;
    Info->NumCarats = 0;
    Info->NumSegments = 0;
    Info->FullyQualified = FALSE;

    /*
     * For the internal name, the required length is 4 bytes
     * per segment, plus 1 each for RootPrefix, MultiNamePrefixOp,
     * segment count, trailing null (which is not really needed,
     * but no there's harm in putting it there)
     *
     * strlen() + 1 covers the first NameSeg, which has no
     * path separator
     */
    if (AcpiNsValidRootPrefix (NextExternalChar[0]))
    {
        Info->FullyQualified = TRUE;
        NextExternalChar++;
    }

    else
    {
        /*
         * Handle Carat prefixes
         */
        while (*NextExternalChar == '^')
        {
            Info->NumCarats++;
            NextExternalChar++;
        }
    }

    /*
     * Determine the number of ACPI name "segments" by counting
     * the number of path separators within the string.  Start
     * with one segment since the segment count is (# separators)
     * + 1, and zero separators is ok.
     */
    if (*NextExternalChar)
    {
        Info->NumSegments = 1;
        for (i = 0; NextExternalChar[i]; i++)
        {
            if (AcpiNsValidPathSeparator (NextExternalChar[i]))
            {
                Info->NumSegments++;
            }
        }
    }

    Info->Length = (ACPI_NAME_SIZE * Info->NumSegments) +
                    4 + Info->NumCarats;

    Info->NextExternalChar = NextExternalChar;

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsBuildInternalName
 *
 * PARAMETERS:  Info            - Info struct fully initialized
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Construct the internal (AML) namestring
 *              corresponding to the external (ASL) namestring.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiNsBuildInternalName (
    ACPI_NAMESTRING_INFO    *Info)
{
    UINT32                  NumSegments = Info->NumSegments;
    NATIVE_CHAR             *InternalName = Info->InternalName;
    NATIVE_CHAR             *ExternalName = Info->NextExternalChar;
    NATIVE_CHAR             *Result = NULL;
    UINT32                  i;


    FUNCTION_TRACE ("NsBuildInternalName");


    /* Setup the correct prefixes, counts, and pointers */

    if (Info->FullyQualified)
    {
        InternalName[0] = '\\';

        if (NumSegments <= 1)
        {
            Result = &InternalName[1];
        }
        else if (NumSegments == 2)
        {
            InternalName[1] = AML_DUAL_NAME_PREFIX;
            Result = &InternalName[2];
        }
        else
        {
            InternalName[1] = AML_MULTI_NAME_PREFIX_OP;
            InternalName[2] = (char) NumSegments;
            Result = &InternalName[3];
        }
    }

    else
    {
        /*
         * Not fully qualified.
         * Handle Carats first, then append the name segments
         */
        i = 0;
        if (Info->NumCarats)
        {
            for (i = 0; i < Info->NumCarats; i++)
            {
                InternalName[i] = '^';
            }
        }

        if (NumSegments == 1)
        {
            Result = &InternalName[i];
        }

        else if (NumSegments == 2)
        {
            InternalName[i] = AML_DUAL_NAME_PREFIX;
            Result = &InternalName[i+1];
        }

        else
        {
            InternalName[i] = AML_MULTI_NAME_PREFIX_OP;
            InternalName[i+1] = (char) NumSegments;
            Result = &InternalName[i+2];
        }
    }


    /* Build the name (minus path separators) */

    for (; NumSegments; NumSegments--)
    {
        for (i = 0; i < ACPI_NAME_SIZE; i++)
        {
            if (AcpiNsValidPathSeparator (*ExternalName) ||
               (*ExternalName == 0))
            {
                /* Pad the segment with underscore(s) if segment is short */

                Result[i] = '_';
            }

            else
            {
                /* Convert the character to uppercase and save it */

                Result[i] = (char) TOUPPER (*ExternalName);
                ExternalName++;
            }
        }

        /* Now we must have a path separator, or the pathname is bad */

        if (!AcpiNsValidPathSeparator (*ExternalName) &&
            (*ExternalName != 0))
        {
            return_ACPI_STATUS (AE_BAD_PARAMETER);
        }

        /* Move on the next segment */

        ExternalName++;
        Result += ACPI_NAME_SIZE;
    }


    /* Terminate the string */

    *Result = 0;

    if (Info->FullyQualified)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "returning [%p] (abs) \"\\%s\"\n",
            InternalName, &InternalName[0]));
    }
    else
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "returning [%p] (rel) \"%s\"\n",
            InternalName, &InternalName[2]));
    }

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsInternalizeName
 *
 * PARAMETERS:  *ExternalName           - External representation of name
 *              **Converted Name        - Where to return the resulting
 *                                        internal represention of the name
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Convert an external representation (e.g. "\_PR_.CPU0")
 *              to internal form (e.g. 5c 2f 02 5f 50 52 5f 43 50 55 30)
 *
 *******************************************************************************/

ACPI_STATUS
AcpiNsInternalizeName (
    NATIVE_CHAR             *ExternalName,
    NATIVE_CHAR             **ConvertedName)
{
    NATIVE_CHAR             *InternalName;
    ACPI_NAMESTRING_INFO    Info;
    ACPI_STATUS             Status;


    FUNCTION_TRACE ("NsInternalizeName");


    if ((!ExternalName)      ||
        (*ExternalName == 0) ||
        (!ConvertedName))
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }


    /* Get the length of the new internal name */

    Info.ExternalName = ExternalName;
    AcpiNsGetInternalNameLength (&Info);

    /* We need a segment to store the internal  name */

    InternalName = ACPI_MEM_CALLOCATE (Info.Length);
    if (!InternalName)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    /* Build the name */

    Info.InternalName = InternalName;
    Status = AcpiNsBuildInternalName (&Info);
    if (ACPI_FAILURE (Status))
    {
        ACPI_MEM_FREE (InternalName);
        return_ACPI_STATUS (Status);
    }

    *ConvertedName = InternalName;
    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsExternalizeName
 *
 * PARAMETERS:  *InternalName          - Internal representation of name
 *              **ConvertedName        - Where to return the resulting
 *                                       external representation of name
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Convert internal name (e.g. 5c 2f 02 5f 50 52 5f 43 50 55 30)
 *              to its external form (e.g. "\_PR_.CPU0")
 *
 ******************************************************************************/

ACPI_STATUS
AcpiNsExternalizeName (
    UINT32                  InternalNameLength,
    char                    *InternalName,
    UINT32                  *ConvertedNameLength,
    char                    **ConvertedName)
{
    UINT32                  PrefixLength = 0;
    UINT32                  NamesIndex = 0;
    UINT32                  NamesCount = 0;
    UINT32                  i = 0;
    UINT32                  j = 0;


    FUNCTION_TRACE ("NsExternalizeName");


    if (!InternalNameLength     ||
        !InternalName           ||
        !ConvertedNameLength    ||
        !ConvertedName)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }


    /*
     * Check for a prefix (one '\' | one or more '^').
     */
    switch (InternalName[0])
    {
    case '\\':
        PrefixLength = 1;
        break;

    case '^':
        for (i = 0; i < InternalNameLength; i++)
        {
            if (InternalName[i] != '^')
            {
                PrefixLength = i + 1;
            }
        }

        if (i == InternalNameLength)
        {
            PrefixLength = i;
        }

        break;
    }

    /*
     * Check for object names.  Note that there could be 0-255 of these
     * 4-byte elements.
     */
    if (PrefixLength < InternalNameLength)
    {
        switch (InternalName[PrefixLength])
        {

        /* <count> 4-byte names */

        case AML_MULTI_NAME_PREFIX_OP:
            NamesIndex = PrefixLength + 2;
            NamesCount = (UINT32) InternalName[PrefixLength + 1];
            break;


        /* two 4-byte names */

        case AML_DUAL_NAME_PREFIX:
            NamesIndex = PrefixLength + 1;
            NamesCount = 2;
            break;


        /* NullName */

        case 0:
            NamesIndex = 0;
            NamesCount = 0;
            break;


        /* one 4-byte name */

        default:
            NamesIndex = PrefixLength;
            NamesCount = 1;
            break;
        }
    }

    /*
     * Calculate the length of ConvertedName, which equals the length
     * of the prefix, length of all object names, length of any required
     * punctuation ('.') between object names, plus the NULL terminator.
     */
    *ConvertedNameLength = PrefixLength + (4 * NamesCount) +
                        ((NamesCount > 0) ? (NamesCount - 1) : 0) + 1;

    /*
     * Check to see if we're still in bounds.  If not, there's a problem
     * with InternalName (invalid format).
     */
    if (*ConvertedNameLength > InternalNameLength)
    {
        REPORT_ERROR (("NsExternalizeName: Invalid internal name\n"));
        return_ACPI_STATUS (AE_BAD_PATHNAME);
    }

    /*
     * Build ConvertedName...
     */

    (*ConvertedName) = ACPI_MEM_CALLOCATE (*ConvertedNameLength);
    if (!(*ConvertedName))
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    j = 0;

    for (i = 0; i < PrefixLength; i++)
    {
        (*ConvertedName)[j++] = InternalName[i];
    }

    if (NamesCount > 0)
    {
        for (i = 0; i < NamesCount; i++)
        {
            if (i > 0)
            {
                (*ConvertedName)[j++] = '.';
            }

            (*ConvertedName)[j++] = InternalName[NamesIndex++];
            (*ConvertedName)[j++] = InternalName[NamesIndex++];
            (*ConvertedName)[j++] = InternalName[NamesIndex++];
            (*ConvertedName)[j++] = InternalName[NamesIndex++];
        }
    }

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsMapHandleToNode
 *
 * PARAMETERS:  Handle          - Handle to be converted to an Node
 *
 * RETURN:      A Name table entry pointer
 *
 * DESCRIPTION: Convert a namespace handle to a real Node
 *
 * Note: Real integer handles allow for more verification
 *       and keep all pointers within this subsystem.
 *
 ******************************************************************************/

ACPI_NAMESPACE_NODE *
AcpiNsMapHandleToNode (
    ACPI_HANDLE             Handle)
{

    FUNCTION_ENTRY ();


    /*
     * Simple implementation.
     */
    if (!Handle)
    {
        return (NULL);
    }

    if (Handle == ACPI_ROOT_OBJECT)
    {
        return (AcpiGbl_RootNode);
    }

    /* We can at least attempt to verify the handle */

    if (!VALID_DESCRIPTOR_TYPE (Handle, ACPI_DESC_TYPE_NAMED))
    {
        return (NULL);
    }

    return ((ACPI_NAMESPACE_NODE *) Handle);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsConvertEntryToHandle
 *
 * PARAMETERS:  Node          - Node to be converted to a Handle
 *
 * RETURN:      An USER ACPI_HANDLE
 *
 * DESCRIPTION: Convert a real Node to a namespace handle
 *
 ******************************************************************************/

ACPI_HANDLE
AcpiNsConvertEntryToHandle (
    ACPI_NAMESPACE_NODE         *Node)
{


    /*
     * Simple implementation for now;
     */
    return ((ACPI_HANDLE) Node);


/* ---------------------------------------------------

    if (!Node)
    {
        return (NULL);
    }

    if (Node == AcpiGbl_RootNode)
    {
        return (ACPI_ROOT_OBJECT);
    }


    return ((ACPI_HANDLE) Node);
------------------------------------------------------*/
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsTerminate
 *
 * PARAMETERS:  none
 *
 * RETURN:      none
 *
 * DESCRIPTION: free memory allocated for table storage.
 *
 ******************************************************************************/

void
AcpiNsTerminate (void)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_NAMESPACE_NODE     *ThisNode;


    FUNCTION_TRACE ("NsTerminate");


    ThisNode = AcpiGbl_RootNode;

    /*
     * 1) Free the entire namespace -- all objects, tables, and stacks
     *
     * Delete all objects linked to the root
     * (additional table descriptors)
     */
    AcpiNsDeleteNamespaceSubtree (ThisNode);

    /* Detach any object(s) attached to the root */

    ObjDesc = AcpiNsGetAttachedObject (ThisNode);
    if (ObjDesc)
    {
        AcpiNsDetachObject (ThisNode);
        AcpiUtRemoveReference (ObjDesc);
    }

    AcpiNsDeleteChildren (ThisNode);
    ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "Namespace freed\n"));


    /*
     * 2) Now we can delete the ACPI tables
     */
    AcpiTbDeleteAcpiTables ();
    ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "ACPI Tables freed\n"));

    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsOpensScope
 *
 * PARAMETERS:  Type        - A valid namespace type
 *
 * RETURN:      NEWSCOPE if the passed type "opens a name scope" according
 *              to the ACPI specification, else 0
 *
 ******************************************************************************/

UINT32
AcpiNsOpensScope (
    ACPI_OBJECT_TYPE8       Type)
{
    FUNCTION_TRACE_U32 ("NsOpensScope", Type);


    if (!AcpiUtValidObjectType (Type))
    {
        /* type code out of range  */

        REPORT_WARNING (("NsOpensScope: Invalid Object Type\n"));
        return_VALUE (NSP_NORMAL);
    }

    return_VALUE (((UINT32) AcpiGbl_NsProperties[Type]) & NSP_NEWSCOPE);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsGetNode
 *
 * PARAMETERS:  *Pathname   - Name to be found, in external (ASL) format. The
 *                            \ (backslash) and ^ (carat) prefixes, and the
 *                            . (period) to separate segments are supported.
 *              StartNode   - Root of subtree to be searched, or NS_ALL for the
 *                            root of the name space.  If Name is fully
 *                            qualified (first INT8 is '\'), the passed value
 *                            of Scope will not be accessed.
 *              ReturnNode  - Where the Node is returned
 *
 * DESCRIPTION: Look up a name relative to a given scope and return the
 *              corresponding Node.  NOTE: Scope can be null.
 *
 * MUTEX:       Locks namespace
 *
 ******************************************************************************/

ACPI_STATUS
AcpiNsGetNode (
    NATIVE_CHAR             *Pathname,
    ACPI_NAMESPACE_NODE     *StartNode,
    ACPI_NAMESPACE_NODE     **ReturnNode)
{
    ACPI_GENERIC_STATE      ScopeInfo;
    ACPI_STATUS             Status;
    NATIVE_CHAR             *InternalPath = NULL;


    FUNCTION_TRACE_PTR ("NsGetNode", Pathname);


    /* Ensure that the namespace has been initialized */

    if (!AcpiGbl_RootNode)
    {
        return_ACPI_STATUS (AE_NO_NAMESPACE);
    }

    if (!Pathname)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }


    /* Convert path to internal representation */

    Status = AcpiNsInternalizeName (Pathname, &InternalPath);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }


    AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);

    /* Setup lookup scope (search starting point) */

    ScopeInfo.Scope.Node = StartNode;

    /* Lookup the name in the namespace */

    Status = AcpiNsLookup (&ScopeInfo, InternalPath,
                            ACPI_TYPE_ANY, IMODE_EXECUTE,
                            NS_NO_UPSEARCH | NS_DONT_OPEN_SCOPE,
                            NULL, ReturnNode);

    if (ACPI_FAILURE (Status))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "%s, %s\n",
                InternalPath, AcpiFormatException (Status)));
    }


    AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);

    /* Cleanup */

    ACPI_MEM_FREE (InternalPath);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsFindParentName
 *
 * PARAMETERS:  *ChildNode             - Named Obj whose name is to be found
 *
 * RETURN:      The ACPI name
 *
 * DESCRIPTION: Search for the given obj in its parent scope and return the
 *              name segment, or "????" if the parent name can't be found
 *              (which "should not happen").
 *
 ******************************************************************************/

ACPI_NAME
AcpiNsFindParentName (
    ACPI_NAMESPACE_NODE     *ChildNode)
{
    ACPI_NAMESPACE_NODE     *ParentNode;


    FUNCTION_TRACE ("NsFindParentName");


    if (ChildNode)
    {
        /* Valid entry.  Get the parent Node */

        ParentNode = AcpiNsGetParentObject (ChildNode);
        if (ParentNode)
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "Parent of %p [%4.4s] is %p [%4.4s]\n",
                ChildNode, (char*)&ChildNode->Name, ParentNode, (char*)&ParentNode->Name));

            if (ParentNode->Name)
            {
                return_VALUE (ParentNode->Name);
            }
        }

        ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "unable to find parent of %p (%4.4s)\n",
            ChildNode, (char*)&ChildNode->Name));
    }

    return_VALUE (ACPI_UNKNOWN_NAME);
}


#if defined(ACPI_DEBUG) || defined(ENABLE_DEBUGGER)

/*******************************************************************************
 *
 * FUNCTION:    AcpiNsExistDownstreamSibling
 *
 * PARAMETERS:  *Node          - pointer to first Node to examine
 *
 * RETURN:      TRUE if sibling is found, FALSE otherwise
 *
 * DESCRIPTION: Searches remainder of scope being processed to determine
 *              whether there is a downstream sibling to the current
 *              object.  This function is used to determine what type of
 *              line drawing character to use when displaying namespace
 *              trees.
 *
 ******************************************************************************/

BOOLEAN
AcpiNsExistDownstreamSibling (
    ACPI_NAMESPACE_NODE     *Node)
{

    if (!Node)
    {
        return (FALSE);
    }

    if (Node->Name)
    {
        return (TRUE);
    }

    return (FALSE);
}

#endif /* ACPI_DEBUG */


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsGetParentObject
 *
 * PARAMETERS:  Node       - Current table entry
 *
 * RETURN:      Parent entry of the given entry
 *
 * DESCRIPTION: Obtain the parent entry for a given entry in the namespace.
 *
 ******************************************************************************/


ACPI_NAMESPACE_NODE *
AcpiNsGetParentObject (
    ACPI_NAMESPACE_NODE     *Node)
{


    FUNCTION_ENTRY ();


    if (!Node)
    {
        return (NULL);
    }

    /*
     * Walk to the end of this peer list.
     * The last entry is marked with a flag and the peer
     * pointer is really a pointer back to the parent.
     * This saves putting a parent back pointer in each and
     * every named object!
     */
    while (!(Node->Flags & ANOBJ_END_OF_PEER_LIST))
    {
        Node = Node->Peer;
    }


    return (Node->Peer);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsGetNextValidNode
 *
 * PARAMETERS:  Node       - Current table entry
 *
 * RETURN:      Next valid Node in the linked node list.  NULL if no more valid
 *              nodess
 *
 * DESCRIPTION: Find the next valid node within a name table.
 *              Useful for implementing NULL-end-of-list loops.
 *
 ******************************************************************************/


ACPI_NAMESPACE_NODE *
AcpiNsGetNextValidNode (
    ACPI_NAMESPACE_NODE     *Node)
{

    /* If we are at the end of this peer list, return NULL */

    if (Node->Flags & ANOBJ_END_OF_PEER_LIST)
    {
        return NULL;
    }

    /* Otherwise just return the next peer */

    return (Node->Peer);
}


