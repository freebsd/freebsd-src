/******************************************************************************
 *
 * Module Name: tbrsdt - ACPI RSDT table utilities
 *              $Revision: 3 $
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

#define __TBRSDT_C__

#include "acpi.h"
#include "actables.h"


#define _COMPONENT          ACPI_TABLES
        ACPI_MODULE_NAME    ("tbrsdt")


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbVerifyRsdp
 *
 * PARAMETERS:  Address         - RSDP (Pointer to RSDT)
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Load and validate the RSDP (ptr) and RSDT (table)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbVerifyRsdp (
    ACPI_POINTER            *Address)
{
    ACPI_TABLE_DESC         TableInfo;
    ACPI_STATUS             Status;
    RSDP_DESCRIPTOR         *Rsdp;


    ACPI_FUNCTION_TRACE ("TbVerifyRsdp");


    switch (Address->PointerType)
    {
    case ACPI_LOGICAL_POINTER:

        Rsdp = Address->Pointer.Logical;
        break;

    case ACPI_PHYSICAL_POINTER:
        /*
         * Obtain access to the RSDP structure
         */
        Status = AcpiOsMapMemory (Address->Pointer.Physical, sizeof (RSDP_DESCRIPTOR),
                                    (void **) &Rsdp);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }
        break;

    default:
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /*
     *  The signature and checksum must both be correct
     */
    if (ACPI_STRNCMP ((NATIVE_CHAR *) Rsdp, RSDP_SIG, sizeof (RSDP_SIG)-1) != 0)
    {
        /* Nope, BAD Signature */

        Status = AE_BAD_SIGNATURE;
        goto Cleanup;
    }

    /* Check the standard checksum */

    if (AcpiTbChecksum (Rsdp, ACPI_RSDP_CHECKSUM_LENGTH) != 0)
    {
        Status = AE_BAD_CHECKSUM;
        goto Cleanup;
    }

    /* Check extended checksum if table version >= 2 */

    if (Rsdp->Revision >= 2)
    {
        if (AcpiTbChecksum (Rsdp, ACPI_RSDP_XCHECKSUM_LENGTH) != 0)
        {
            Status = AE_BAD_CHECKSUM;
            goto Cleanup;
        }
    }

    /* The RSDP supplied is OK */

    TableInfo.Pointer      = ACPI_CAST_PTR (ACPI_TABLE_HEADER, Rsdp);
    TableInfo.Length       = sizeof (RSDP_DESCRIPTOR);
    TableInfo.Allocation   = ACPI_MEM_MAPPED;
    TableInfo.BasePointer  = Rsdp;

    /* Save the table pointers and allocation info */

    Status = AcpiTbInitTableDescriptor (ACPI_TABLE_RSDP, &TableInfo);
    if (ACPI_FAILURE (Status))
    {
        goto Cleanup;
    }

    /* Save the RSDP in a global for easy access */

    AcpiGbl_RSDP = ACPI_CAST_PTR (RSDP_DESCRIPTOR, TableInfo.Pointer);
    return_ACPI_STATUS (Status);


    /* Error exit */
Cleanup:

    if (AcpiGbl_TableFlags & ACPI_PHYSICAL_POINTER)
    {
        AcpiOsUnmapMemory (Rsdp, sizeof (RSDP_DESCRIPTOR));
    }
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbGetRsdtAddress
 *
 * PARAMETERS:  None
 *
 * RETURN:      RSDT physical address
 *
 * DESCRIPTION: Extract the address of the RSDT or XSDT, depending on the
 *              version of the RSDP
 *
 ******************************************************************************/

void
AcpiTbGetRsdtAddress (
    ACPI_POINTER            *OutAddress)
{

    ACPI_FUNCTION_ENTRY ();


    OutAddress->PointerType = AcpiGbl_TableFlags | ACPI_LOGICAL_ADDRESSING;

    /*
     * For RSDP revision 0 or 1, we use the RSDT.
     * For RSDP revision 2 (and above), we use the XSDT
     */
    if (AcpiGbl_RSDP->Revision < 2)
    {
        OutAddress->Pointer.Value = AcpiGbl_RSDP->RsdtPhysicalAddress;
    }
    else
    {
        OutAddress->Pointer.Value = ACPI_GET_ADDRESS (AcpiGbl_RSDP->XsdtPhysicalAddress);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbValidateRsdt
 *
 * PARAMETERS:  TablePtr        - Addressable pointer to the RSDT.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Validate signature for the RSDT or XSDT
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbValidateRsdt (
    ACPI_TABLE_HEADER       *TablePtr)
{
    int                     NoMatch;


    ACPI_FUNCTION_NAME ("TbValidateRsdt");


    /*
     * For RSDP revision 0 or 1, we use the RSDT.
     * For RSDP revision 2 and above, we use the XSDT
     */
    if (AcpiGbl_RSDP->Revision < 2)
    {
        NoMatch = ACPI_STRNCMP ((char *) TablePtr, RSDT_SIG,
                        sizeof (RSDT_SIG) -1);
    }
    else
    {
        NoMatch = ACPI_STRNCMP ((char *) TablePtr, XSDT_SIG,
                        sizeof (XSDT_SIG) -1);
    }

    if (NoMatch)
    {
        /* Invalid RSDT or XSDT signature */

        ACPI_REPORT_ERROR (("Invalid signature where RSDP indicates RSDT/XSDT should be located\n"));

        ACPI_DUMP_BUFFER (AcpiGbl_RSDP, 20);

        ACPI_DEBUG_PRINT_RAW ((ACPI_DB_ERROR,
            "RSDT/XSDT signature at %X (%p) is invalid\n",
            AcpiGbl_RSDP->RsdtPhysicalAddress,
            (void *) (NATIVE_UINT) AcpiGbl_RSDP->RsdtPhysicalAddress));

        return (AE_BAD_SIGNATURE);
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbGetTableRsdt
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Load and validate the RSDP (ptr) and RSDT (table)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbGetTableRsdt (
    void)
{
    ACPI_TABLE_DESC         TableInfo;
    ACPI_STATUS             Status;
    ACPI_POINTER            Address;


    ACPI_FUNCTION_TRACE ("TbGetTableRsdt");


    /* Get the RSDT/XSDT via the RSDP */

    AcpiTbGetRsdtAddress (&Address);

    Status = AcpiTbGetTable (&Address, &TableInfo);
    if (ACPI_FAILURE (Status))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Could not get the RSDT/XSDT, %s\n",
            AcpiFormatException (Status)));
        return_ACPI_STATUS (Status);
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_INFO,
        "RSDP located at %p, points to RSDT physical=%8.8X%8.8X \n",
        AcpiGbl_RSDP,
        ACPI_HIDWORD (Address.Pointer.Value),
        ACPI_LODWORD (Address.Pointer.Value)));

    /* Check the RSDT or XSDT signature */

    Status = AcpiTbValidateRsdt (TableInfo.Pointer);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Get the number of tables defined in the RSDT or XSDT */

    AcpiGbl_RsdtTableCount = AcpiTbGetTableCount (AcpiGbl_RSDP, TableInfo.Pointer);

    /* Convert and/or copy to an XSDT structure */

    Status = AcpiTbConvertToXsdt (&TableInfo);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Save the table pointers and allocation info */

    Status = AcpiTbInitTableDescriptor (ACPI_TABLE_XSDT, &TableInfo);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    AcpiGbl_XSDT = (XSDT_DESCRIPTOR *) TableInfo.Pointer;

    ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "XSDT located at %p\n", AcpiGbl_XSDT));
    return_ACPI_STATUS (Status);
}


