/******************************************************************************
 *
 * Module Name: exconfig - Namespace reconfiguration (Load/Unload opcodes)
 *              $Revision: 74 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2003, Intel Corp.
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

#define __EXCONFIG_C__

#include "acpi.h"
#include "acinterp.h"
#include "amlcode.h"
#include "acnamesp.h"
#include "acevents.h"
#include "actables.h"


#define _COMPONENT          ACPI_EXECUTER
        ACPI_MODULE_NAME    ("exconfig")


/*******************************************************************************
 *
 * FUNCTION:    AcpiExAddTable
 *
 * PARAMETERS:  Table               - Pointer to raw table
 *              ParentNode          - Where to load the table (scope)
 *              DdbHandle           - Where to return the table handle.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Common function to Install and Load an ACPI table with a
 *              returned table handle.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExAddTable (
    ACPI_TABLE_HEADER       *Table,
    ACPI_NAMESPACE_NODE     *ParentNode,
    ACPI_OPERAND_OBJECT     **DdbHandle)
{
    ACPI_STATUS             Status;
    ACPI_TABLE_DESC         TableInfo;
    ACPI_OPERAND_OBJECT     *ObjDesc;


    ACPI_FUNCTION_TRACE ("ExAddTable");


    /* Create an object to be the table handle */

    ObjDesc = AcpiUtCreateInternalObject (ACPI_TYPE_LOCAL_REFERENCE);
    if (!ObjDesc)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    /* Install the new table into the local data structures */

    ACPI_MEMSET (&TableInfo, 0, sizeof (ACPI_TABLE_DESC));

    TableInfo.Type         = 5;
    TableInfo.Pointer      = Table;
    TableInfo.Length       = (ACPI_SIZE) Table->Length;
    TableInfo.Allocation   = ACPI_MEM_ALLOCATED;

    Status = AcpiTbInstallTable (&TableInfo);
    if (ACPI_FAILURE (Status))
    {
        goto Cleanup;
    }

    /* Add the table to the namespace */

    Status = AcpiNsLoadTable (TableInfo.InstalledDesc, ParentNode);
    if (ACPI_FAILURE (Status))
    {
        /* Uninstall table on error */

        (void) AcpiTbUninstallTable (TableInfo.InstalledDesc);
        goto Cleanup;
    }

    /* Init the table handle */

    ObjDesc->Reference.Opcode = AML_LOAD_OP;
    ObjDesc->Reference.Object = TableInfo.InstalledDesc;
    *DdbHandle = ObjDesc;
    return_ACPI_STATUS (AE_OK);


Cleanup:
    AcpiUtRemoveReference (ObjDesc);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExLoadTableOp
 *
 * PARAMETERS:  WalkState           - Current state with operands
 *              ReturnDesc          - Where to store the return object
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Load an ACPI table
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExLoadTableOp (
    ACPI_WALK_STATE         *WalkState,
    ACPI_OPERAND_OBJECT     **ReturnDesc)
{
    ACPI_STATUS             Status;
    ACPI_OPERAND_OBJECT     **Operand = &WalkState->Operands[0];
    ACPI_TABLE_HEADER       *Table;
    ACPI_NAMESPACE_NODE     *ParentNode;
    ACPI_NAMESPACE_NODE     *StartNode;
    ACPI_NAMESPACE_NODE     *ParameterNode = NULL;
    ACPI_OPERAND_OBJECT     *DdbHandle;


    ACPI_FUNCTION_TRACE ("ExLoadTableOp");


#if 0
    /*
     * Make sure that the signature does not match one of the tables that
     * is already loaded.
     */
    Status = AcpiTbMatchSignature (Operand[0]->String.Pointer, NULL);
    if (Status == AE_OK)
    {
        /* Signature matched -- don't allow override */

        return_ACPI_STATUS (AE_ALREADY_EXISTS);
    }
#endif

    /* Find the ACPI table */

    Status = AcpiTbFindTable (Operand[0]->String.Pointer,
                              Operand[1]->String.Pointer,
                              Operand[2]->String.Pointer, &Table);
    if (ACPI_FAILURE (Status))
    {
        if (Status != AE_NOT_FOUND)
        {
            return_ACPI_STATUS (Status);
        }

        /* Table not found, return an Integer=0 and AE_OK */

        DdbHandle = AcpiUtCreateInternalObject (ACPI_TYPE_INTEGER);
        if (!DdbHandle)
        {
            return_ACPI_STATUS (AE_NO_MEMORY);
        }

        DdbHandle->Integer.Value = 0;
        *ReturnDesc = DdbHandle;

        return_ACPI_STATUS (AE_OK);
    }

    /* Default nodes */

    StartNode = WalkState->ScopeInfo->Scope.Node;
    ParentNode = AcpiGbl_RootNode;

    /* RootPath (optional parameter) */

    if (Operand[3]->String.Length > 0)
    {
        /*
         * Find the node referenced by the RootPathString.  This is the
         * location within the namespace where the table will be loaded.
         */
        Status = AcpiNsGetNodeByPath (Operand[3]->String.Pointer, StartNode,
                                        ACPI_NS_SEARCH_PARENT, &ParentNode);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }
    }

    /* ParameterPath (optional parameter) */

    if (Operand[4]->String.Length > 0)
    {
        if ((Operand[4]->String.Pointer[0] != '\\') &&
            (Operand[4]->String.Pointer[0] != '^'))
        {
            /*
             * Path is not absolute, so it will be relative to the node
             * referenced by the RootPathString (or the NS root if omitted)
             */
            StartNode = ParentNode;
        }

        /*
         * Find the node referenced by the ParameterPathString
         */
        Status = AcpiNsGetNodeByPath (Operand[4]->String.Pointer, StartNode,
                                        ACPI_NS_SEARCH_PARENT, &ParameterNode);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }
    }

    /* Load the table into the namespace */

    Status = AcpiExAddTable (Table, ParentNode, &DdbHandle);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Parameter Data (optional) */

    if (ParameterNode)
    {
        /* Store the parameter data into the optional parameter object */

        Status = AcpiExStore (Operand[5], ACPI_CAST_PTR (ACPI_OPERAND_OBJECT, ParameterNode),
                                WalkState);
        if (ACPI_FAILURE (Status))
        {
            (void) AcpiExUnloadTable (DdbHandle);
            return_ACPI_STATUS (Status);
        }
    }

    *ReturnDesc = DdbHandle;
    return_ACPI_STATUS  (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExLoadOp
 *
 * PARAMETERS:  ObjDesc         - Region or Field where the table will be
 *                                obtained
 *              Target          - Where a handle to the table will be stored
 *              WalkState       - Current state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Load an ACPI table from a field or operation region
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExLoadOp (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_OPERAND_OBJECT     *Target,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_STATUS             Status;
    ACPI_OPERAND_OBJECT     *DdbHandle;
    ACPI_OPERAND_OBJECT     *BufferDesc = NULL;
    ACPI_TABLE_HEADER       *TablePtr = NULL;
    UINT8                   *TableDataPtr;
    ACPI_TABLE_HEADER       TableHeader;
    UINT32                  i;

    ACPI_FUNCTION_TRACE ("ExLoadOp");


    /* Object can be either an OpRegion or a Field */

    switch (ACPI_GET_OBJECT_TYPE (ObjDesc))
    {
    case ACPI_TYPE_REGION:

        ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "Load from Region %p %s\n",
            ObjDesc, AcpiUtGetObjectTypeName (ObjDesc)));

        /* Get the table header */

        TableHeader.Length = 0;
        for (i = 0; i < sizeof (ACPI_TABLE_HEADER); i++)
        {
            Status = AcpiEvAddressSpaceDispatch (ObjDesc, ACPI_READ,
                                (ACPI_PHYSICAL_ADDRESS) i, 8,
                                ((UINT8 *) &TableHeader) + i);
            if (ACPI_FAILURE (Status))
            {
                return_ACPI_STATUS (Status);
            }
        }

        /* Allocate a buffer for the entire table */

        TablePtr = ACPI_MEM_ALLOCATE (TableHeader.Length);
        if (!TablePtr)
        {
            return_ACPI_STATUS (AE_NO_MEMORY);
        }

        /* Copy the header to the buffer */

        ACPI_MEMCPY (TablePtr, &TableHeader, sizeof (ACPI_TABLE_HEADER));
        TableDataPtr = ACPI_PTR_ADD (UINT8, TablePtr, sizeof (ACPI_TABLE_HEADER));

        /* Get the table from the op region */

        for (i = 0; i < TableHeader.Length; i++)
        {
            Status = AcpiEvAddressSpaceDispatch (ObjDesc, ACPI_READ,
                                (ACPI_PHYSICAL_ADDRESS) i, 8,
                                ((UINT8 *) TableDataPtr + i));
            if (ACPI_FAILURE (Status))
            {
                goto Cleanup;
            }
        }
        break;


    case ACPI_TYPE_LOCAL_REGION_FIELD:
    case ACPI_TYPE_LOCAL_BANK_FIELD:
    case ACPI_TYPE_LOCAL_INDEX_FIELD:

        ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "Load from Field %p %s\n",
            ObjDesc, AcpiUtGetObjectTypeName (ObjDesc)));

        /*
         * The length of the field must be at least as large as the table.
         * Read the entire field and thus the entire table.  Buffer is
         * allocated during the read.
         */
        Status = AcpiExReadDataFromField (WalkState, ObjDesc, &BufferDesc);
        if (ACPI_FAILURE (Status))
        {
            goto Cleanup;
        }

        TablePtr = ACPI_CAST_PTR (ACPI_TABLE_HEADER, BufferDesc->Buffer.Pointer);
        break;


    default:
        return_ACPI_STATUS (AE_AML_OPERAND_TYPE);
    }

    /* The table must be either an SSDT or a PSDT */

    if ((!ACPI_STRNCMP (TablePtr->Signature,
                    AcpiGbl_TableData[ACPI_TABLE_PSDT].Signature,
                    AcpiGbl_TableData[ACPI_TABLE_PSDT].SigLength)) &&
        (!ACPI_STRNCMP (TablePtr->Signature,
                    AcpiGbl_TableData[ACPI_TABLE_SSDT].Signature,
                    AcpiGbl_TableData[ACPI_TABLE_SSDT].SigLength)))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
            "Table has invalid signature [%4.4s], must be SSDT or PSDT\n",
            TablePtr->Signature));
        Status = AE_BAD_SIGNATURE;
        goto Cleanup;
    }

    /* Install the new table into the local data structures */

    Status = AcpiExAddTable (TablePtr, AcpiGbl_RootNode, &DdbHandle);
    if (ACPI_FAILURE (Status))
    {
        goto Cleanup;
    }

    /* Store the DdbHandle into the Target operand */

    Status = AcpiExStore (DdbHandle, Target, WalkState);
    if (ACPI_FAILURE (Status))
    {
        (void) AcpiExUnloadTable (DdbHandle);
    }

    return_ACPI_STATUS (Status);


Cleanup:

    if (BufferDesc)
    {
        AcpiUtRemoveReference (BufferDesc);
    }
    else
    {
        ACPI_MEM_FREE (TablePtr);
    }
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExUnloadTable
 *
 * PARAMETERS:  DdbHandle           - Handle to a previously loaded table
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Unload an ACPI table
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExUnloadTable (
    ACPI_OPERAND_OBJECT     *DdbHandle)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_OPERAND_OBJECT     *TableDesc = DdbHandle;
    ACPI_TABLE_DESC         *TableInfo;


    ACPI_FUNCTION_TRACE ("ExUnloadTable");


    /*
     * Validate the handle
     * Although the handle is partially validated in AcpiExReconfiguration(),
     * when it calls AcpiExResolveOperands(), the handle is more completely
     * validated here.
     */
    if ((!DdbHandle) ||
        (ACPI_GET_DESCRIPTOR_TYPE (DdbHandle) != ACPI_DESC_TYPE_OPERAND) ||
        (ACPI_GET_OBJECT_TYPE (DdbHandle) != ACPI_TYPE_LOCAL_REFERENCE))
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /* Get the actual table descriptor from the DdbHandle */

    TableInfo = (ACPI_TABLE_DESC *) TableDesc->Reference.Object;

    /*
     * Delete the entire namespace under this table Node
     * (Offset contains the TableId)
     */
    AcpiNsDeleteNamespaceByOwner (TableInfo->TableId);

    /* Delete the table itself */

    (void) AcpiTbUninstallTable (TableInfo->InstalledDesc);

    /* Delete the table descriptor (DdbHandle) */

    AcpiUtRemoveReference (TableDesc);
    return_ACPI_STATUS (Status);
}

