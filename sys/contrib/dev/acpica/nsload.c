/******************************************************************************
 *
 * Module Name: nsload - namespace loading/expanding/contracting procedures
 *              $Revision: 57 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2002, Intel Corp.
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

#define __NSLOAD_C__

#include "acpi.h"
#include "acnamesp.h"
#include "amlcode.h"
#include "acparser.h"
#include "acdispat.h"


#define _COMPONENT          ACPI_NAMESPACE
        ACPI_MODULE_NAME    ("nsload")


/*******************************************************************************
 *
 * FUNCTION:    NsOneCompleteParse
 *
 * PARAMETERS:  PassNumber              - 1 or 2
 *              TableDesc               - The table to be parsed.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Perform one complete parse of an ACPI/AML table.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiNsOneCompleteParse (
    UINT32                  PassNumber,
    ACPI_TABLE_DESC         *TableDesc)
{
    ACPI_PARSE_OBJECT       *ParseRoot;
    ACPI_STATUS             Status;
    ACPI_WALK_STATE         *WalkState;


    ACPI_FUNCTION_TRACE ("NsOneCompleteParse");


    /* Create and init a Root Node */

    ParseRoot = AcpiPsCreateScopeOp ();
    if (!ParseRoot)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }


    /* Create and initialize a new walk state */

    WalkState = AcpiDsCreateWalkState (TABLE_ID_DSDT,
                                    NULL, NULL, NULL);
    if (!WalkState)
    {
        AcpiPsFreeOp (ParseRoot);
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    Status = AcpiDsInitAmlWalk (WalkState, ParseRoot, NULL, TableDesc->AmlStart,
                    TableDesc->AmlLength, NULL, NULL, PassNumber);
    if (ACPI_FAILURE (Status))
    {
        AcpiDsDeleteWalkState (WalkState);
        return_ACPI_STATUS (Status);
    }

    /* Parse the AML */

    ACPI_DEBUG_PRINT ((ACPI_DB_PARSE, "*PARSE* pass %d parse\n", PassNumber));
    Status = AcpiPsParseAml (WalkState);

    AcpiPsDeleteParseTree (ParseRoot);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsParseTable
 *
 * PARAMETERS:  TableDesc       - An ACPI table descriptor for table to parse
 *              StartNode       - Where to enter the table into the namespace
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Parse AML within an ACPI table and return a tree of ops
 *
 ******************************************************************************/

ACPI_STATUS
AcpiNsParseTable (
    ACPI_TABLE_DESC         *TableDesc,
    ACPI_NAMESPACE_NODE     *StartNode)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("NsParseTable");


    /*
     * AML Parse, pass 1
     *
     * In this pass, we load most of the namespace.  Control methods
     * are not parsed until later.  A parse tree is not created.  Instead,
     * each Parser Op subtree is deleted when it is finished.  This saves
     * a great deal of memory, and allows a small cache of parse objects
     * to service the entire parse.  The second pass of the parse then
     * performs another complete parse of the AML..
     */
    Status = AcpiNsOneCompleteParse (1, TableDesc);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /*
     * AML Parse, pass 2
     *
     * In this pass, we resolve forward references and other things
     * that could not be completed during the first pass.
     * Another complete parse of the AML is performed, but the
     * overhead of this is compensated for by the fact that the
     * parse objects are all cached.
     */
    Status = AcpiNsOneCompleteParse (2, TableDesc);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    return_ACPI_STATUS (Status);
}

#ifndef ACPI_NO_METHOD_EXECUTION

/*******************************************************************************
 *
 * FUNCTION:    AcpiNsLoadTable
 *
 * PARAMETERS:  TableDesc       - Descriptor for table to be loaded
 *              Node            - Owning NS node
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Load one ACPI table into the namespace
 *
 ******************************************************************************/

ACPI_STATUS
AcpiNsLoadTable (
    ACPI_TABLE_DESC         *TableDesc,
    ACPI_NAMESPACE_NODE     *Node)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("NsLoadTable");


    /* Check if table contains valid AML (must be DSDT, PSDT, SSDT, etc.) */

    if (!(AcpiGbl_AcpiTableData[TableDesc->Type].Flags & ACPI_TABLE_EXECUTABLE))
    {
        /* Just ignore this table */

        return_ACPI_STATUS (AE_OK);
    }

    /* Check validity of the AML start and length */

    if (!TableDesc->AmlStart)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Null AML pointer\n"));
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "AML block at %p\n", TableDesc->AmlStart));

    if (!TableDesc->AmlLength)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Zero-length AML block\n"));
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /*
     * Parse the table and load the namespace with all named
     * objects found within.  Control methods are NOT parsed
     * at this time.  In fact, the control methods cannot be
     * parsed until the entire namespace is loaded, because
     * if a control method makes a forward reference (call)
     * to another control method, we can't continue parsing
     * because we don't know how many arguments to parse next!
     */
    ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "**** Loading table into namespace ****\n"));

    Status = AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    Status = AcpiNsParseTable (TableDesc, Node->Child);
    (void) AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);

    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /*
     * Now we can parse the control methods.  We always parse
     * them here for a sanity check, and if configured for
     * just-in-time parsing, we delete the control method
     * parse trees.
     */
    ACPI_DEBUG_PRINT ((ACPI_DB_INFO,
        "**** Begin Table Method Parsing and Object Initialization ****\n"));

    Status = AcpiDsInitializeObjects (TableDesc, Node);

    ACPI_DEBUG_PRINT ((ACPI_DB_INFO,
        "**** Completed Table Method Parsing and Object Initialization ****\n"));

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsLoadTableByType
 *
 * PARAMETERS:  TableType           - Id of the table type to load
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Load an ACPI table or tables into the namespace.  All tables
 *              of the given type are loaded.  The mechanism allows this
 *              routine to be called repeatedly.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiNsLoadTableByType (
    ACPI_TABLE_TYPE         TableType)
{
    UINT32                  i;
    ACPI_STATUS             Status;
    ACPI_TABLE_DESC         *TableDesc;


    ACPI_FUNCTION_TRACE ("NsLoadTableByType");


    Status = AcpiUtAcquireMutex (ACPI_MTX_TABLES);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /*
     * Table types supported are:
     * DSDT (one), SSDT/PSDT (multiple)
     */
    switch (TableType)
    {
    case ACPI_TABLE_DSDT:

        ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "Loading DSDT\n"));

        TableDesc = &AcpiGbl_AcpiTables[ACPI_TABLE_DSDT];

        /* If table already loaded into namespace, just return */

        if (TableDesc->LoadedIntoNamespace)
        {
            goto UnlockAndExit;
        }

        TableDesc->TableId = TABLE_ID_DSDT;

        /* Now load the single DSDT */

        Status = AcpiNsLoadTable (TableDesc, AcpiGbl_RootNode);
        if (ACPI_SUCCESS (Status))
        {
            TableDesc->LoadedIntoNamespace = TRUE;
        }

        break;


    case ACPI_TABLE_SSDT:

        ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "Loading %d SSDTs\n",
            AcpiGbl_AcpiTables[ACPI_TABLE_SSDT].Count));

        /*
         * Traverse list of SSDT tables
         */
        TableDesc = &AcpiGbl_AcpiTables[ACPI_TABLE_SSDT];
        for (i = 0; i < AcpiGbl_AcpiTables[ACPI_TABLE_SSDT].Count; i++)
        {
            /*
             * Only attempt to load table if it is not
             * already loaded!
             */
            if (!TableDesc->LoadedIntoNamespace)
            {
                Status = AcpiNsLoadTable (TableDesc, AcpiGbl_RootNode);
                if (ACPI_FAILURE (Status))
                {
                    break;
                }

                TableDesc->LoadedIntoNamespace = TRUE;
            }

            TableDesc = TableDesc->Next;
        }
        break;


    case ACPI_TABLE_PSDT:

        ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "Loading %d PSDTs\n",
            AcpiGbl_AcpiTables[ACPI_TABLE_PSDT].Count));

        /*
         * Traverse list of PSDT tables
         */
        TableDesc = &AcpiGbl_AcpiTables[ACPI_TABLE_PSDT];

        for (i = 0; i < AcpiGbl_AcpiTables[ACPI_TABLE_PSDT].Count; i++)
        {
            /* Only attempt to load table if it is not already loaded! */

            if (!TableDesc->LoadedIntoNamespace)
            {
                Status = AcpiNsLoadTable (TableDesc, AcpiGbl_RootNode);
                if (ACPI_FAILURE (Status))
                {
                    break;
                }

                TableDesc->LoadedIntoNamespace = TRUE;
            }

            TableDesc = TableDesc->Next;
        }

        break;


    default:
        Status = AE_SUPPORT;
        break;
    }


UnlockAndExit:
    (void) AcpiUtReleaseMutex (ACPI_MTX_TABLES);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiLoadNamespace
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Load the name space from what ever is pointed to by DSDT.
 *              (DSDT points to either the BIOS or a buffer.)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiNsLoadNamespace (
    void)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("AcpiLoadNameSpace");


    /* There must be at least a DSDT installed */

    if (AcpiGbl_DSDT == NULL)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "DSDT is not in memory\n"));
        return_ACPI_STATUS (AE_NO_ACPI_TABLES);
    }

    /*
     * Load the namespace.  The DSDT is required,
     * but the SSDT and PSDT tables are optional.
     */
    Status = AcpiNsLoadTableByType (ACPI_TABLE_DSDT);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Ignore exceptions from these */

    (void) AcpiNsLoadTableByType (ACPI_TABLE_SSDT);
    (void) AcpiNsLoadTableByType (ACPI_TABLE_PSDT);

    ACPI_DEBUG_PRINT_RAW ((ACPI_DB_OK,
        "ACPI Namespace successfully loaded at root %p\n",
        AcpiGbl_RootNode));

    return_ACPI_STATUS (Status);
}



/*******************************************************************************
 *
 * FUNCTION:    AcpiNsDeleteSubtree
 *
 * PARAMETERS:  StartHandle         - Handle in namespace where search begins
 *
 * RETURNS      Status
 *
 * DESCRIPTION: Walks the namespace starting at the given handle and deletes
 *              all objects, entries, and scopes in the entire subtree.
 *
 *              Namespace/Interpreter should be locked or the subsystem should
 *              be in shutdown before this routine is called.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiNsDeleteSubtree (
    ACPI_HANDLE             StartHandle)
{
    ACPI_STATUS             Status;
    ACPI_HANDLE             ChildHandle;
    ACPI_HANDLE             ParentHandle;
    ACPI_HANDLE             NextChildHandle;
    ACPI_HANDLE             Dummy;
    UINT32                  Level;


    ACPI_FUNCTION_TRACE ("NsDeleteSubtree");


    ParentHandle = StartHandle;
    ChildHandle  = 0;
    Level        = 1;

    /*
     * Traverse the tree of objects until we bubble back up
     * to where we started.
     */
    while (Level > 0)
    {
        /* Attempt to get the next object in this scope */

        Status = AcpiGetNextObject (ACPI_TYPE_ANY, ParentHandle,
                                    ChildHandle, &NextChildHandle);

        ChildHandle = NextChildHandle;

        /* Did we get a new object? */

        if (ACPI_SUCCESS (Status))
        {
            /* Check if this object has any children */

            if (ACPI_SUCCESS (AcpiGetNextObject (ACPI_TYPE_ANY, ChildHandle,
                                    0, &Dummy)))
            {
                /*
                 * There is at least one child of this object,
                 * visit the object
                 */
                Level++;
                ParentHandle = ChildHandle;
                ChildHandle  = 0;
            }
        }
        else
        {
            /*
             * No more children in this object, go back up to
             * the object's parent
             */
            Level--;

            /* Delete all children now */

            AcpiNsDeleteChildren (ChildHandle);

            ChildHandle = ParentHandle;
            Status = AcpiGetParent (ParentHandle, &ParentHandle);
            if (ACPI_FAILURE (Status))
            {
                return_ACPI_STATUS (Status);
            }
        }
    }

    /* Now delete the starting object, and we are done */

    AcpiNsDeleteNode (ChildHandle);

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 *  FUNCTION:       AcpiNsUnloadNameSpace
 *
 *  PARAMETERS:     Handle          - Root of namespace subtree to be deleted
 *
 *  RETURN:         Status
 *
 *  DESCRIPTION:    Shrinks the namespace, typically in response to an undocking
 *                  event.  Deletes an entire subtree starting from (and
 *                  including) the given handle.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiNsUnloadNamespace (
    ACPI_HANDLE             Handle)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("NsUnloadNameSpace");


    /* Parameter validation */

    if (!AcpiGbl_RootNode)
    {
        return_ACPI_STATUS (AE_NO_NAMESPACE);
    }

    if (!Handle)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /* This function does the real work */

    Status = AcpiNsDeleteSubtree (Handle);

    return_ACPI_STATUS (Status);
}

#endif

