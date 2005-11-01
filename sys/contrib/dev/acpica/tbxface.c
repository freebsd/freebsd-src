/******************************************************************************
 *
 * Module Name: tbxface - Public interfaces to the ACPI subsystem
 *                         ACPI table oriented interfaces
 *              $Revision: 1.70 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2005, Intel Corp.
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

#define __TBXFACE_C__

#include <contrib/dev/acpica/acpi.h>
#include <contrib/dev/acpica/acnamesp.h>
#include <contrib/dev/acpica/actables.h>


#define _COMPONENT          ACPI_TABLES
        ACPI_MODULE_NAME    ("tbxface")


/*******************************************************************************
 *
 * FUNCTION:    AcpiLoadTables
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This function is called to load the ACPI tables from the
 *              provided RSDT
 *
 ******************************************************************************/

ACPI_STATUS
AcpiLoadTables (
    void)
{
    ACPI_POINTER            RsdpAddress;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("AcpiLoadTables");


    /* Get the RSDP */

    Status = AcpiOsGetRootPointer (ACPI_LOGICAL_ADDRESSING,
                    &RsdpAddress);
    if (ACPI_FAILURE (Status))
    {
        ACPI_REPORT_ERROR (("AcpiLoadTables: Could not get RSDP, %s\n",
            AcpiFormatException (Status)));
        goto ErrorExit;
    }

    /* Map and validate the RSDP */

    AcpiGbl_TableFlags = RsdpAddress.PointerType;

    Status = AcpiTbVerifyRsdp (&RsdpAddress);
    if (ACPI_FAILURE (Status))
    {
        ACPI_REPORT_ERROR (("AcpiLoadTables: RSDP Failed validation: %s\n",
            AcpiFormatException (Status)));
        goto ErrorExit;
    }

    /* Get the RSDT via the RSDP */

    Status = AcpiTbGetTableRsdt ();
    if (ACPI_FAILURE (Status))
    {
        ACPI_REPORT_ERROR (("AcpiLoadTables: Could not load RSDT: %s\n",
            AcpiFormatException (Status)));
        goto ErrorExit;
    }

    /* Now get the tables needed by this subsystem (FADT, DSDT, etc.) */

    Status = AcpiTbGetRequiredTables ();
    if (ACPI_FAILURE (Status))
    {
        ACPI_REPORT_ERROR ((
            "AcpiLoadTables: Error getting required tables (DSDT/FADT/FACS): %s\n",
            AcpiFormatException (Status)));
        goto ErrorExit;
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_INIT, "ACPI Tables successfully acquired\n"));

    /* Load the namespace from the tables */

    Status = AcpiNsLoadNamespace ();
    if (ACPI_FAILURE (Status))
    {
        ACPI_REPORT_ERROR (("AcpiLoadTables: Could not load namespace: %s\n",
            AcpiFormatException (Status)));
        goto ErrorExit;
    }

    return_ACPI_STATUS (AE_OK);


ErrorExit:
    ACPI_REPORT_ERROR (("AcpiLoadTables: Could not load tables: %s\n",
                    AcpiFormatException (Status)));

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiLoadTable
 *
 * PARAMETERS:  TablePtr        - pointer to a buffer containing the entire
 *                                table to be loaded
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This function is called to load a table from the caller's
 *              buffer.  The buffer must contain an entire ACPI Table including
 *              a valid header.  The header fields will be verified, and if it
 *              is determined that the table is invalid, the call will fail.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiLoadTable (
    ACPI_TABLE_HEADER       *TablePtr)
{
    ACPI_STATUS             Status;
    ACPI_TABLE_DESC         TableInfo;
    ACPI_POINTER            Address;


    ACPI_FUNCTION_TRACE ("AcpiLoadTable");


    if (!TablePtr)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /* Copy the table to a local buffer */

    Address.PointerType     = ACPI_LOGICAL_POINTER | ACPI_LOGICAL_ADDRESSING;
    Address.Pointer.Logical = TablePtr;

    Status = AcpiTbGetTableBody (&Address, TablePtr, &TableInfo);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Check signature for a valid table type */

    Status = AcpiTbRecognizeTable (&TableInfo, ACPI_TABLE_ALL);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Install the new table into the local data structures */

    Status = AcpiTbInstallTable (&TableInfo);
    if (ACPI_FAILURE (Status))
    {
        if (Status == AE_ALREADY_EXISTS)
        {
            /* Table already exists, no error */

            Status = AE_OK;
        }

        /* Free table allocated by AcpiTbGetTableBody */

        AcpiTbDeleteSingleTable (&TableInfo);
        return_ACPI_STATUS (Status);
    }

    /* Convert the table to common format if necessary */

    switch (TableInfo.Type)
    {
    case ACPI_TABLE_FADT:

        Status = AcpiTbConvertTableFadt ();
        break;

    case ACPI_TABLE_FACS:

        Status = AcpiTbBuildCommonFacs (&TableInfo);
        break;

    default:
        /* Load table into namespace if it contains executable AML */

        Status = AcpiNsLoadTable (TableInfo.InstalledDesc, AcpiGbl_RootNode);
        break;
    }

    if (ACPI_FAILURE (Status))
    {
        /* Uninstall table and free the buffer */

        (void) AcpiTbUninstallTable (TableInfo.InstalledDesc);
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUnloadTable
 *
 * PARAMETERS:  TableType     - Type of table to be unloaded
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This routine is used to force the unload of a table
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUnloadTable (
    ACPI_TABLE_TYPE         TableType)
{
    ACPI_TABLE_DESC         *TableDesc;


    ACPI_FUNCTION_TRACE ("AcpiUnloadTable");


    /* Parameter validation */

    if (TableType > ACPI_TABLE_MAX)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /* Find all tables of the requested type */

    TableDesc = AcpiGbl_TableLists[TableType].Next;
    while (TableDesc)
    {
        /*
         * Delete all namespace entries owned by this table.  Note that these
         * entries can appear anywhere in the namespace by virtue of the AML
         * "Scope" operator.  Thus, we need to track ownership by an ID, not
         * simply a position within the hierarchy
         */
        AcpiNsDeleteNamespaceByOwner (TableDesc->OwnerId);
        AcpiUtReleaseOwnerId (&TableDesc->OwnerId);
        TableDesc = TableDesc->Next;
    }

    /* Delete (or unmap) all tables of this type */

    AcpiTbDeleteTablesByType (TableType);
    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiGetTableHeader
 *
 * PARAMETERS:  TableType       - one of the defined table types
 *              Instance        - the non zero instance of the table, allows
 *                                support for multiple tables of the same type
 *                                see AcpiGbl_AcpiTableFlag
 *              OutTableHeader  - pointer to the ACPI_TABLE_HEADER if successful
 *
 * DESCRIPTION: This function is called to get an ACPI table header.  The caller
 *              supplies an pointer to a data area sufficient to contain an ACPI
 *              ACPI_TABLE_HEADER structure.
 *
 *              The header contains a length field that can be used to determine
 *              the size of the buffer needed to contain the entire table.  This
 *              function is not valid for the RSD PTR table since it does not
 *              have a standard header and is fixed length.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiGetTableHeader (
    ACPI_TABLE_TYPE         TableType,
    UINT32                  Instance,
    ACPI_TABLE_HEADER       *OutTableHeader)
{
    ACPI_TABLE_HEADER       *TblPtr;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("AcpiGetTableHeader");


    if ((Instance == 0)                 ||
        (TableType == ACPI_TABLE_RSDP)  ||
        (!OutTableHeader))
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /* Check the table type and instance */

    if ((TableType > ACPI_TABLE_MAX)    ||
        (ACPI_IS_SINGLE_TABLE (AcpiGbl_TableData[TableType].Flags) &&
         Instance > 1))
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /* Get a pointer to the entire table */

    Status = AcpiTbGetTablePtr (TableType, Instance, &TblPtr);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* The function will return a NULL pointer if the table is not loaded */

    if (TblPtr == NULL)
    {
        return_ACPI_STATUS (AE_NOT_EXIST);
    }

    /* Copy the header to the caller's buffer */

    ACPI_MEMCPY ((void *) OutTableHeader, (void *) TblPtr,
        sizeof (ACPI_TABLE_HEADER));

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiGetTable
 *
 * PARAMETERS:  TableType       - one of the defined table types
 *              Instance        - the non zero instance of the table, allows
 *                                support for multiple tables of the same type
 *                                see AcpiGbl_AcpiTableFlag
 *              RetBuffer       - pointer to a structure containing a buffer to
 *                                receive the table
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This function is called to get an ACPI table.  The caller
 *              supplies an OutBuffer large enough to contain the entire ACPI
 *              table.  The caller should call the AcpiGetTableHeader function
 *              first to determine the buffer size needed.  Upon completion
 *              the OutBuffer->Length field will indicate the number of bytes
 *              copied into the OutBuffer->BufPtr buffer.  This table will be
 *              a complete table including the header.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiGetTable (
    ACPI_TABLE_TYPE         TableType,
    UINT32                  Instance,
    ACPI_BUFFER             *RetBuffer)
{
    ACPI_TABLE_HEADER       *TblPtr;
    ACPI_STATUS             Status;
    ACPI_SIZE               TableLength;


    ACPI_FUNCTION_TRACE ("AcpiGetTable");


    /* Parameter validation */

    if (Instance == 0)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    Status = AcpiUtValidateBuffer (RetBuffer);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Check the table type and instance */

    if ((TableType > ACPI_TABLE_MAX)    ||
        (ACPI_IS_SINGLE_TABLE (AcpiGbl_TableData[TableType].Flags) &&
         Instance > 1))
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /* Get a pointer to the entire table */

    Status = AcpiTbGetTablePtr (TableType, Instance, &TblPtr);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /*
     * AcpiTbGetTablePtr will return a NULL pointer if the
     * table is not loaded.
     */
    if (TblPtr == NULL)
    {
        return_ACPI_STATUS (AE_NOT_EXIST);
    }

    /* Get the table length */

    if (TableType == ACPI_TABLE_RSDP)
    {
        /* RSD PTR is the only "table" without a header */

        TableLength = sizeof (RSDP_DESCRIPTOR);
    }
    else
    {
        TableLength = (ACPI_SIZE) TblPtr->Length;
    }

    /* Validate/Allocate/Clear caller buffer */

    Status = AcpiUtInitializeBuffer (RetBuffer, TableLength);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Copy the table to the buffer */

    ACPI_MEMCPY ((void *) RetBuffer->Pointer, (void *) TblPtr, TableLength);
    return_ACPI_STATUS (AE_OK);
}


