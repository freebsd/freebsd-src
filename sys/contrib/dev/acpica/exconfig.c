/******************************************************************************
 *
 * Module Name: exconfig - Namespace reconfiguration (Load/Unload opcodes)
 *              $Revision: 47 $
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

#define __EXCONFIG_C__

#include "acpi.h"
#include "acparser.h"
#include "acinterp.h"
#include "amlcode.h"
#include "acnamesp.h"
#include "acevents.h"
#include "actables.h"
#include "acdispat.h"


#define _COMPONENT          ACPI_EXECUTER
        MODULE_NAME         ("exconfig")


/*****************************************************************************
 *
 * FUNCTION:    AcpiExLoadTableOp
 *
 * PARAMETERS:  RgnDesc         - Op region where the table will be obtained
 *              DdbHandle       - Where a handle to the table will be returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Load an ACPI table
 *
 ****************************************************************************/

ACPI_STATUS
AcpiExLoadOp (
    ACPI_OPERAND_OBJECT     *RgnDesc,
    ACPI_OPERAND_OBJECT     *DdbHandle)
{
    ACPI_STATUS             Status;
    ACPI_OPERAND_OBJECT     *TableDesc = NULL;
    UINT8                   *TablePtr;
    UINT8                   *TableDataPtr;
    ACPI_TABLE_HEADER       TableHeader;
    ACPI_TABLE_DESC         TableInfo;
    UINT32                  i;


    FUNCTION_TRACE ("ExLoadOp");

    /* Object can be either a field or an opregion */

    /* TBD: Handle field vs. Opregion *?


    /* Get the table header */

    TableHeader.Length = 0;
    for (i = 0; i < sizeof (ACPI_TABLE_HEADER); i++)
    {
        Status = AcpiEvAddressSpaceDispatch (RgnDesc, ACPI_READ,
                            (ACPI_PHYSICAL_ADDRESS) i, 8,
                            (ACPI_INTEGER *) ((UINT8 *) &TableHeader + i));
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

    MEMCPY (TablePtr, &TableHeader, sizeof (ACPI_TABLE_HEADER));
    TableDataPtr = TablePtr + sizeof (ACPI_TABLE_HEADER);


    /* Get the table from the op region */

    for (i = 0; i < TableHeader.Length; i++)
    {
        Status = AcpiEvAddressSpaceDispatch (RgnDesc, ACPI_READ,
                            (ACPI_PHYSICAL_ADDRESS) i, 8,
                            (ACPI_INTEGER *) (TableDataPtr + i));
        if (ACPI_FAILURE (Status))
        {
            goto Cleanup;
        }
    }

    /* Table must be either an SSDT or a PSDT */

    if ((!STRNCMP (TableHeader.Signature,
                    AcpiGbl_AcpiTableData[ACPI_TABLE_PSDT].Signature,
                    AcpiGbl_AcpiTableData[ACPI_TABLE_PSDT].SigLength)) &&
        (!STRNCMP (TableHeader.Signature,
                    AcpiGbl_AcpiTableData[ACPI_TABLE_SSDT].Signature,
                    AcpiGbl_AcpiTableData[ACPI_TABLE_SSDT].SigLength)))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
            "Table has invalid signature [%4.4s], must be SSDT or PSDT\n",
            (char*)TableHeader.Signature));
        Status = AE_BAD_SIGNATURE;
        goto Cleanup;
    }

    /* Create an object to be the table handle */

    TableDesc = AcpiUtCreateInternalObject (INTERNAL_TYPE_REFERENCE);
    if (!TableDesc)
    {
        Status = AE_NO_MEMORY;
        goto Cleanup;
    }

    /* Install the new table into the local data structures */

    TableInfo.Pointer      = (ACPI_TABLE_HEADER *) TablePtr;
    TableInfo.Length       = TableHeader.Length;
    TableInfo.Allocation   = ACPI_MEM_ALLOCATED;
    TableInfo.BasePointer  = TablePtr;

    Status = AcpiTbInstallTable (NULL, &TableInfo);
    if (ACPI_FAILURE (Status))
    {
        goto Cleanup;
    }

    /* Add the table to the namespace */

    Status = AcpiNsLoadTable (TableInfo.InstalledDesc, AcpiGbl_RootNode);
    if (ACPI_FAILURE (Status))
    {
        /* Uninstall table and free the buffer */

        AcpiTbUninstallTable (TableInfo.InstalledDesc);
        goto Cleanup;
    }


    /* We need a pointer to the table desc */

    /* Init the table handle */

    TableDesc->Reference.Opcode = AML_LOAD_OP;
    TableDesc->Reference.Object = TableInfo.InstalledDesc;

    /* Store the tabledesc into the DdbHandle target */
    /* DdbHandle = TableDesc; */

    return_ACPI_STATUS (Status);


Cleanup:

    ACPI_MEM_FREE (TableDesc);
    ACPI_MEM_FREE (TablePtr);
    return_ACPI_STATUS (Status);
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiExUnloadTable
 *
 * PARAMETERS:  DdbHandle           - Handle to a previously loaded table
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Unload an ACPI table
 *
 ****************************************************************************/

ACPI_STATUS
AcpiExUnloadTable (
    ACPI_OPERAND_OBJECT     *DdbHandle)
{
    ACPI_STATUS             Status = AE_NOT_IMPLEMENTED;
    ACPI_OPERAND_OBJECT     *TableDesc = DdbHandle;
    ACPI_TABLE_DESC         *TableInfo;


    FUNCTION_TRACE ("ExUnloadTable");


    /*
     * Validate the handle
     * Although the handle is partially validated in AcpiExReconfiguration(),
     * when it calls AcpiExResolveOperands(), the handle is more completely
     * validated here.
     */
    if ((!DdbHandle) ||
        (!VALID_DESCRIPTOR_TYPE (DdbHandle, ACPI_DESC_TYPE_INTERNAL)) ||
        (((ACPI_OPERAND_OBJECT  *)DdbHandle)->Common.Type !=
                INTERNAL_TYPE_REFERENCE))
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /* Get the actual table descriptor from the DdbHandle */

    TableInfo = (ACPI_TABLE_DESC *) TableDesc->Reference.Object;

    /*
     * Delete the entire namespace under this table Node
     * (Offset contains the TableId)
     */
    Status = AcpiNsDeleteNamespaceByOwner (TableInfo->TableId);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Delete the table itself */

    AcpiTbUninstallTable (TableInfo->InstalledDesc);

    /* Delete the table descriptor (DdbHandle) */

    AcpiUtRemoveReference (TableDesc);

    return_ACPI_STATUS (Status);
}

