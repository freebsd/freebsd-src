
/******************************************************************************
 *
 * Module Name: amprep - ACPI AML (p-code) execution - field prep utilities
 *              $Revision: 68 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999, Intel Corp.  All rights
 * reserved.
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

#define __AMPREP_C__

#include "acpi.h"
#include "acinterp.h"
#include "amlcode.h"
#include "acnamesp.h"
#include "acparser.h"


#define _COMPONENT          INTERPRETER
        MODULE_NAME         ("amprep")


/*******************************************************************************
 *
 * FUNCTION:    AcpiAmlDecodeFieldAccessType
 *
 * PARAMETERS:  Access          - Encoded field access bits
 *
 * RETURN:      Field granularity (8, 16, or 32)
 *
 * DESCRIPTION: Decode the AccessType bits of a field definition.
 *
 ******************************************************************************/

static UINT32
AcpiAmlDecodeFieldAccessType (
    UINT32                  Access)
{

    switch (Access)
    {
    case ACCESS_ANY_ACC:
        return (8);
        break;

    case ACCESS_BYTE_ACC:
        return (8);
        break;

    case ACCESS_WORD_ACC:
        return (16);
        break;

    case ACCESS_DWORD_ACC:
        return (32);
        break;

    default:
        /* Invalid field access type */

        DEBUG_PRINT (ACPI_ERROR,
            ("AmlDecodeFieldAccessType: Unknown field access type %x\n",
            Access));
        return (0);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiAmlPrepCommonFieldObjec
 *
 * PARAMETERS:  ObjDesc             - The field object
 *              FieldFlags          - Access, LockRule, or UpdateRule.
 *                                    The format of a FieldFlag is described
 *                                    in the ACPI specification
 *              FieldPosition       - Field position
 *              FieldLength         - Field length
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Initialize the areas of the field object that are common
 *              to the various types of fields.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiAmlPrepCommonFieldObject (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    UINT8                   FieldFlags,
    UINT8                   FieldAttribute,
    UINT32                  FieldPosition,
    UINT32                  FieldLength)
{
    UINT32                  Granularity;


    FUNCTION_TRACE ("AmlPrepCommonFieldObject");


    /*
     * Note: the structure being initialized is the
     * ACPI_COMMON_FIELD_INFO;  Therefore, we can just use the Field union to
     * access this common area.  No structure fields outside of the common area
     * are initialized by this procedure.
     */

    /* Decode the FieldFlags */

    ObjDesc->Field.Access           = (UINT8) ((FieldFlags & ACCESS_TYPE_MASK)
                                                    >> ACCESS_TYPE_SHIFT);
    ObjDesc->Field.LockRule         = (UINT8) ((FieldFlags & LOCK_RULE_MASK)
                                                    >> LOCK_RULE_SHIFT);
    ObjDesc->Field.UpdateRule       = (UINT8) ((FieldFlags & UPDATE_RULE_MASK)
                                                    >> UPDATE_RULE_SHIFT);

    /* Other misc fields */

    ObjDesc->Field.Length           = (UINT16) FieldLength;
    ObjDesc->Field.AccessAttribute  = FieldAttribute;

    /* Decode the access type so we can compute offsets */

    Granularity = AcpiAmlDecodeFieldAccessType (ObjDesc->Field.Access);
    if (!Granularity)
    {
        return_ACPI_STATUS (AE_AML_OPERAND_VALUE);
    }

    /* Access granularity based fields */

    ObjDesc->Field.Granularity      = (UINT8) Granularity;
    ObjDesc->Field.BitOffset        = (UINT8) (FieldPosition % Granularity);
    ObjDesc->Field.Offset           = (UINT32) FieldPosition / Granularity;


    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiAmlPrepDefFieldValue
 *
 * PARAMETERS:  Node            - Owning Node
 *              Region              - Region in which field is being defined
 *              FieldFlags          - Access, LockRule, or UpdateRule.
 *                                    The format of a FieldFlag is described
 *                                    in the ACPI specification
 *              FieldPosition       - Field position
 *              FieldLength         - Field length
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Construct an ACPI_OPERAND_OBJECT  of type DefField and
 *              connect it to the parent Node.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiAmlPrepDefFieldValue (
    ACPI_NAMESPACE_NODE     *Node,
    ACPI_HANDLE             Region,
    UINT8                   FieldFlags,
    UINT8                   FieldAttribute,
    UINT32                  FieldPosition,
    UINT32                  FieldLength)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    UINT32                  Type;
    ACPI_STATUS             Status;


    FUNCTION_TRACE ("AmlPrepDefFieldValue");


    /* Parameter validation */

    if (!Region)
    {
        DEBUG_PRINT (ACPI_ERROR, ("AmlPrepDefFieldValue: null Region\n"));
        return_ACPI_STATUS (AE_AML_NO_OPERAND);
    }

    Type = AcpiNsGetType (Region);
    if (Type != ACPI_TYPE_REGION)
    {
        DEBUG_PRINT (ACPI_ERROR,
            ("AmlPrepDefFieldValue: Needed Region, found %d %s\n",
            Type, AcpiCmGetTypeName (Type)));
        return_ACPI_STATUS (AE_AML_OPERAND_TYPE);
    }

    /* Allocate a new object */

    ObjDesc = AcpiCmCreateInternalObject (INTERNAL_TYPE_DEF_FIELD);
    if (!ObjDesc)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }


    /* ObjDesc and Region valid */

    DUMP_OPERANDS ((ACPI_OPERAND_OBJECT  **) &Node, IMODE_EXECUTE,
                    "AmlPrepDefFieldValue", 1, "case DefField");
    DUMP_OPERANDS ((ACPI_OPERAND_OBJECT  **) &Region, IMODE_EXECUTE,
                    "AmlPrepDefFieldValue", 1, "case DefField");

    /* Initialize areas of the object that are common to all fields */

    Status = AcpiAmlPrepCommonFieldObject (ObjDesc, FieldFlags, FieldAttribute,
                                            FieldPosition, FieldLength);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Initialize areas of the object that are specific to this field type */

    ObjDesc->Field.Container = AcpiNsGetAttachedObject (Region);

    /* An additional reference for the container */

    AcpiCmAddReference (ObjDesc->Field.Container);


    /* Debug info */

    DEBUG_PRINT (ACPI_INFO,
        ("AmlPrepDefFieldValue: bitoff=%X off=%X gran=%X\n",
        ObjDesc->Field.BitOffset, ObjDesc->Field.Offset,
        ObjDesc->Field.Granularity));

    DEBUG_PRINT (ACPI_INFO,
        ("AmlPrepDefFieldValue: set NamedObj %p (%4.4s) val = %p\n",
        Node, &(Node->Name), ObjDesc));

    DUMP_STACK_ENTRY (ObjDesc);
    DUMP_ENTRY (Region, ACPI_INFO);
    DEBUG_PRINT (ACPI_INFO, ("\t%p \n", ObjDesc->Field.Container));
    if (ObjDesc->Field.Container)
    {
        DUMP_STACK_ENTRY (ObjDesc->Field.Container);
    }
    DEBUG_PRINT (ACPI_INFO,
        ("============================================================\n"));

    /*
     * Store the constructed descriptor (ObjDesc) into the NamedObj whose
     * handle is on TOS, preserving the current type of that NamedObj.
     */
    Status = AcpiNsAttachObject ((ACPI_HANDLE) Node, ObjDesc,
                    (UINT8) AcpiNsGetType ((ACPI_HANDLE) Node));

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiAmlPrepBankFieldValue
 *
 * PARAMETERS:  Node            - Owning Node
 *              Region              - Region in which field is being defined
 *              BankReg             - Bank selection register
 *              BankVal             - Value to store in selection register
 *              FieldFlags          - Access, LockRule, or UpdateRule
 *              FieldPosition       - Field position
 *              FieldLength         - Field length
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Construct an ACPI_OPERAND_OBJECT  of type BankField and
 *              connect it to the parent Node.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiAmlPrepBankFieldValue (
    ACPI_NAMESPACE_NODE     *Node,
    ACPI_HANDLE             Region,
    ACPI_HANDLE             BankReg,
    UINT32                  BankVal,
    UINT8                   FieldFlags,
    UINT8                   FieldAttribute,
    UINT32                  FieldPosition,
    UINT32                  FieldLength)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    UINT32                  Type;
    ACPI_STATUS             Status;


    FUNCTION_TRACE ("AmlPrepBankFieldValue");


    /* Parameter validation */

    if (!Region)
    {
        DEBUG_PRINT (ACPI_ERROR, ("AmlPrepBankFieldValue: null Region\n"));
        return_ACPI_STATUS (AE_AML_NO_OPERAND);
    }

    Type = AcpiNsGetType (Region);
    if (Type != ACPI_TYPE_REGION)
    {
        DEBUG_PRINT (ACPI_ERROR,
            ("AmlPrepBankFieldValue: Needed Region, found %d %s\n",
            Type, AcpiCmGetTypeName (Type)));
        return_ACPI_STATUS (AE_AML_OPERAND_TYPE);
    }

    /* Allocate a new object */

    ObjDesc = AcpiCmCreateInternalObject (INTERNAL_TYPE_BANK_FIELD);
    if (!ObjDesc)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    /*  ObjDesc and Region valid    */

    DUMP_OPERANDS ((ACPI_OPERAND_OBJECT  **) &Node, IMODE_EXECUTE,
                    "AmlPrepBankFieldValue", 1, "case BankField");
    DUMP_OPERANDS ((ACPI_OPERAND_OBJECT  **) &Region, IMODE_EXECUTE,
                    "AmlPrepBankFieldValue", 1, "case BankField");

    /* Initialize areas of the object that are common to all fields */

    Status = AcpiAmlPrepCommonFieldObject (ObjDesc, FieldFlags, FieldAttribute,
                                            FieldPosition, FieldLength);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Initialize areas of the object that are specific to this field type */

    ObjDesc->BankField.Value        = BankVal;
    ObjDesc->BankField.Container    = AcpiNsGetAttachedObject (Region);
    ObjDesc->BankField.BankSelect   = AcpiNsGetAttachedObject (BankReg);

    /* An additional reference for the container and bank select */
    /* TBD: [Restructure] is "BankSelect" ever a real internal object?? */

    AcpiCmAddReference (ObjDesc->BankField.Container);
    AcpiCmAddReference (ObjDesc->BankField.BankSelect);

    /* Debug info */

    DEBUG_PRINT (ACPI_INFO,
        ("AmlPrepBankFieldValue: bitoff=%X off=%X gran=%X\n",
        ObjDesc->BankField.BitOffset, ObjDesc->BankField.Offset,
        ObjDesc->Field.Granularity));

    DEBUG_PRINT (ACPI_INFO,
        ("AmlPrepBankFieldValue: set NamedObj %p (%4.4s) val = %p\n",
        Node, &(Node->Name), ObjDesc));

    DUMP_STACK_ENTRY (ObjDesc);
    DUMP_ENTRY (Region, ACPI_INFO);
    DUMP_ENTRY (BankReg, ACPI_INFO);
    DEBUG_PRINT (ACPI_INFO,
        ("============================================================\n"));

    /*
     * Store the constructed descriptor (ObjDesc) into the NamedObj whose
     * handle is on TOS, preserving the current type of that NamedObj.
     */
    Status = AcpiNsAttachObject ((ACPI_HANDLE) Node, ObjDesc,
                (UINT8) AcpiNsGetType ((ACPI_HANDLE) Node));

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiAmlPrepIndexFieldValue
 *
 * PARAMETERS:  Node            - Owning Node
 *              IndexReg            - Index register
 *              DataReg             - Data register
 *              FieldFlags          - Access, LockRule, or UpdateRule
 *              FieldPosition       - Field position
 *              FieldLength         - Field length
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Construct an ACPI_OPERAND_OBJECT  of type IndexField and
 *              connect it to the parent Node.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiAmlPrepIndexFieldValue (
    ACPI_NAMESPACE_NODE     *Node,
    ACPI_HANDLE             IndexReg,
    ACPI_HANDLE             DataReg,
    UINT8                   FieldFlags,
    UINT8                   FieldAttribute,
    UINT32                  FieldPosition,
    UINT32                  FieldLength)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_STATUS             Status;


    FUNCTION_TRACE ("AmlPrepIndexFieldValue");


    /* Parameter validation */

    if (!IndexReg || !DataReg)
    {
        DEBUG_PRINT (ACPI_ERROR, ("AmlPrepIndexFieldValue: null handle\n"));
        return_ACPI_STATUS (AE_AML_NO_OPERAND);
    }

    /* Allocate a new object descriptor */

    ObjDesc = AcpiCmCreateInternalObject (INTERNAL_TYPE_INDEX_FIELD);
    if (!ObjDesc)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    /* Initialize areas of the object that are common to all fields */

    Status = AcpiAmlPrepCommonFieldObject (ObjDesc, FieldFlags, FieldAttribute,
                                            FieldPosition, FieldLength);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Initialize areas of the object that are specific to this field type */

    ObjDesc->IndexField.Value       = (UINT32) (FieldPosition /
                                                ObjDesc->Field.Granularity);
    ObjDesc->IndexField.Index       = IndexReg;
    ObjDesc->IndexField.Data        = DataReg;

    /* Debug info */

    DEBUG_PRINT (ACPI_INFO,
        ("AmlPrepIndexFieldValue: bitoff=%X off=%X gran=%X\n",
        ObjDesc->IndexField.BitOffset, ObjDesc->IndexField.Offset,
        ObjDesc->Field.Granularity));

    DEBUG_PRINT (ACPI_INFO,
        ("AmlPrepIndexFieldValue: set NamedObj %p (%4.4s) val = %p\n",
        Node, &(Node->Name), ObjDesc));

    DUMP_STACK_ENTRY (ObjDesc);
    DUMP_ENTRY (IndexReg, ACPI_INFO);
    DUMP_ENTRY (DataReg, ACPI_INFO);
    DEBUG_PRINT (ACPI_INFO,
        ("============================================================\n"));

    /*
     * Store the constructed descriptor (ObjDesc) into the NamedObj whose
     * handle is on TOS, preserving the current type of that NamedObj.
     */
    Status = AcpiNsAttachObject ((ACPI_HANDLE) Node, ObjDesc,
                (UINT8) AcpiNsGetType ((ACPI_HANDLE) Node));

    return_ACPI_STATUS (Status);
}

