/******************************************************************************
 *
 * Module Name: tbgetall - Get all required ACPI tables
 *              $Revision: 7 $
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

#define __TBGETALL_C__

#include "acpi.h"
#include "actables.h"


#define _COMPONENT          ACPI_TABLES
        ACPI_MODULE_NAME    ("tbgetall")


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbGetPrimaryTable
 *
 * PARAMETERS:  Address             - Physical address of table to retrieve
 *              *TableInfo          - Where the table info is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Maps the physical address of table into a logical address
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbGetPrimaryTable (
    ACPI_POINTER            *Address,
    ACPI_TABLE_DESC         *TableInfo)
{
    ACPI_STATUS             Status;
    ACPI_TABLE_HEADER       Header;


    ACPI_FUNCTION_TRACE ("TbGetPrimaryTable");


    /* Ignore a NULL address in the RSDT */

    if (!Address->Pointer.Value)
    {
        return_ACPI_STATUS (AE_OK);
    }

    /*
     * Get the header in order to get signature and table size
     */
    Status = AcpiTbGetTableHeader (Address, &Header);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Clear the TableInfo */

    ACPI_MEMSET (TableInfo, 0, sizeof (ACPI_TABLE_DESC));

    /*
     * Check the table signature and make sure it is recognized.
     * Also checks the header checksum
     */
    TableInfo->Pointer = &Header;
    Status = AcpiTbRecognizeTable (TableInfo, ACPI_TABLE_PRIMARY);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Get the entire table */

    Status = AcpiTbGetTableBody (Address, &Header, TableInfo);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Install the table */

    Status = AcpiTbInstallTable (TableInfo);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbGetSecondaryTable
 *
 * PARAMETERS:  Address             - Physical address of table to retrieve
 *              *TableInfo          - Where the table info is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Maps the physical address of table into a logical address
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbGetSecondaryTable (
    ACPI_POINTER            *Address,
    ACPI_STRING             Signature,
    ACPI_TABLE_DESC         *TableInfo)
{
    ACPI_STATUS             Status;
    ACPI_TABLE_HEADER       Header;


    ACPI_FUNCTION_TRACE_STR ("TbGetSecondaryTable", Signature);


    /* Get the header in order to match the signature */

    Status = AcpiTbGetTableHeader (Address, &Header);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Signature must match request */

    if (ACPI_STRNCMP (Header.Signature, Signature, ACPI_NAME_SIZE))
    {
        ACPI_REPORT_ERROR (("Incorrect table signature - wanted [%s] found [%4.4s]\n",
            Signature, Header.Signature));
        return_ACPI_STATUS (AE_BAD_SIGNATURE);
    }

    /*
     * Check the table signature and make sure it is recognized.
     * Also checks the header checksum
     */
    TableInfo->Pointer = &Header;
    Status = AcpiTbRecognizeTable (TableInfo, ACPI_TABLE_SECONDARY);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Get the entire table */

    Status = AcpiTbGetTableBody (Address, &Header, TableInfo);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Install the table */

    Status = AcpiTbInstallTable (TableInfo);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbGetRequiredTables
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Load and validate tables other than the RSDT.  The RSDT must
 *              already be loaded and validated.
 *
 *              Get the minimum set of ACPI tables, namely:
 *
 *              1) FADT (via RSDT in loop below)
 *              2) FACS (via FADT)
 *              3) DSDT (via FADT)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbGetRequiredTables (
    void)
{
    ACPI_STATUS             Status = AE_OK;
    UINT32                  i;
    ACPI_TABLE_DESC         TableInfo;
    ACPI_POINTER            Address;


    ACPI_FUNCTION_TRACE ("TbGetRequiredTables");

    ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "%d ACPI tables in RSDT\n",
        AcpiGbl_RsdtTableCount));


    Address.PointerType   = AcpiGbl_TableFlags | ACPI_LOGICAL_ADDRESSING;

    /*
     * Loop through all table pointers found in RSDT.
     * This will NOT include the FACS and DSDT - we must get
     * them after the loop.
     *
     * The only tables we are interested in getting here is the FADT and
     * any SSDTs.
     */
    for (i = 0; i < AcpiGbl_RsdtTableCount; i++)
    {
        /* Get the table address from the common internal XSDT */

        Address.Pointer.Value = ACPI_GET_ADDRESS (AcpiGbl_XSDT->TableOffsetEntry[i]);

        /*
         * Get the tables needed by this subsystem (FADT and any SSDTs).
         * NOTE: All other tables are completely ignored at this time.
         */
        Status = AcpiTbGetPrimaryTable (&Address, &TableInfo);
        if ((Status != AE_OK) && (Status != AE_TABLE_NOT_SUPPORTED))
        {
            ACPI_REPORT_WARNING (("%s, while getting table at %8.8X%8.8X\n",
                AcpiFormatException (Status),
                ACPI_HIDWORD (Address.Pointer.Value),
                ACPI_LODWORD (Address.Pointer.Value)));
        }
    }

    /* We must have a FADT to continue */

    if (!AcpiGbl_FADT)
    {
        ACPI_REPORT_ERROR (("No FADT present in RSDT/XSDT\n"));
        return_ACPI_STATUS (AE_NO_ACPI_TABLES);
    }

    /*
     * Convert the FADT to a common format.  This allows earlier revisions of the
     * table to coexist with newer versions, using common access code.
     */
    Status = AcpiTbConvertTableFadt ();
    if (ACPI_FAILURE (Status))
    {
        ACPI_REPORT_ERROR (("Could not convert FADT to internal common format\n"));
        return_ACPI_STATUS (Status);
    }

    /*
     * Get the FACS (Pointed to by the FADT)
     */
    Address.Pointer.Value = ACPI_GET_ADDRESS (AcpiGbl_FADT->XFirmwareCtrl);

    Status = AcpiTbGetSecondaryTable (&Address, FACS_SIG, &TableInfo);
    if (ACPI_FAILURE (Status))
    {
        ACPI_REPORT_ERROR (("Could not get/install the FACS, %s\n",
            AcpiFormatException (Status)));
        return_ACPI_STATUS (Status);
    }

    /*
     * Create the common FACS pointer table
     * (Contains pointers to the original table)
     */
    Status = AcpiTbBuildCommonFacs (&TableInfo);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /*
     * Get/install the DSDT (Pointed to by the FADT)
     */
    Address.Pointer.Value = ACPI_GET_ADDRESS (AcpiGbl_FADT->XDsdt);

    Status = AcpiTbGetSecondaryTable (&Address, DSDT_SIG, &TableInfo);
    if (ACPI_FAILURE (Status))
    {
        ACPI_REPORT_ERROR (("Could not get/install the DSDT\n"));
        return_ACPI_STATUS (Status);
    }

    /* Set Integer Width (32/64) based upon DSDT revision */

    AcpiUtSetIntegerWidth (AcpiGbl_DSDT->Revision);

    /* Dump the entire DSDT */

    ACPI_DEBUG_PRINT ((ACPI_DB_TABLES,
        "Hex dump of entire DSDT, size %d (0x%X), Integer width = %d\n",
        AcpiGbl_DSDT->Length, AcpiGbl_DSDT->Length, AcpiGbl_IntegerBitWidth));
    ACPI_DUMP_BUFFER ((UINT8 *) AcpiGbl_DSDT, AcpiGbl_DSDT->Length);

    /* Always delete the RSDP mapping, we are done with it */

    AcpiTbDeleteTablesByType (ACPI_TABLE_RSDP);
    return_ACPI_STATUS (Status);
}


