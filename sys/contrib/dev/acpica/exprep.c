
/******************************************************************************
 *
 * Module Name: exprep - ACPI AML (p-code) execution - field prep utilities
 *              $Revision: 109 $
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

#define __EXPREP_C__

#include "acpi.h"
#include "acinterp.h"
#include "amlcode.h"
#include "acnamesp.h"
#include "acparser.h"


#define _COMPONENT          ACPI_EXECUTER
        MODULE_NAME         ("exprep")


/*******************************************************************************
 *
 * FUNCTION:    AcpiExDecodeFieldAccess
 *
 * PARAMETERS:  Access          - Encoded field access bits
 *              Length          - Field length.
 *
 * RETURN:      Field granularity (8, 16, 32 or 64) and
 *              ByteAlignment (1, 2, 3, or 4)
 *
 * DESCRIPTION: Decode the AccessType bits of a field definition.
 *
 ******************************************************************************/

static UINT32
AcpiExDecodeFieldAccess (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    UINT8                   FieldFlags,
    UINT32                  *ReturnByteAlignment)
{
    UINT32                  Access;
    UINT16                  Length;
    UINT8                   ByteAlignment;
    UINT8                   BitLength;


    PROC_NAME ("ExDecodeFieldAccess");


    Access = (FieldFlags & AML_FIELD_ACCESS_TYPE_MASK);
    Length = ObjDesc->CommonField.BitLength;

    switch (Access)
    {
    case AML_FIELD_ACCESS_ANY:

        ByteAlignment = 1;

        /* Use the length to set the access type */

        if (Length <= 8)
        {
            BitLength = 8;
        }
        else if (Length <= 16)
        {
            BitLength = 16;
        }
        else if (Length <= 32)
        {
            BitLength = 32;
        }
        else if (Length <= 64)
        {
            BitLength = 64;
        }
        else
        {
            /* Larger than Qword - just use byte-size chunks */

            BitLength = 8;        
        }
        break;

    case AML_FIELD_ACCESS_BYTE:
        ByteAlignment = 1;
        BitLength = 8;
        break;

    case AML_FIELD_ACCESS_WORD:
        ByteAlignment = 2;
        BitLength = 16;
        break;

    case AML_FIELD_ACCESS_DWORD:
        ByteAlignment = 4;
        BitLength = 32;
        break;

    case AML_FIELD_ACCESS_QWORD:  /* ACPI 2.0 */
        ByteAlignment = 8;
        BitLength = 64;
        break;

    case AML_FIELD_ACCESS_BUFFER:  /* ACPI 2.0 */
        ByteAlignment = 8;
        BitLength = 8;
        break;

    default:
        /* Invalid field access type */

        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
            "Unknown field access type %x\n",
            Access));
        return (0);
    }

    if (ObjDesc->Common.Type == ACPI_TYPE_BUFFER_FIELD)
    {
        /*
         * BufferField access can be on any byte boundary, so the
         * ByteAlignment is always 1 byte -- regardless of any ByteAlignment
         * implied by the field access type.
         */
        ByteAlignment = 1;
    }

    *ReturnByteAlignment = ByteAlignment;
    return (BitLength);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExPrepCommonFieldObject
 *
 * PARAMETERS:  ObjDesc             - The field object
 *              FieldFlags          - Access, LockRule, and UpdateRule.
 *                                    The format of a FieldFlag is described
 *                                    in the ACPI specification
 *              FieldBitPosition    - Field start position
 *              FieldBitLength      - Field length in number of bits
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Initialize the areas of the field object that are common
 *              to the various types of fields.  Note: This is very "sensitive"
 *              code because we are solving the general case for field
 *              alignment.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExPrepCommonFieldObject (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    UINT8                   FieldFlags,
    UINT8                   FieldAttribute,
    UINT32                  FieldBitPosition,
    UINT32                  FieldBitLength)
{
    UINT32                  AccessBitWidth;
    UINT32                  ByteAlignment;
    UINT32                  NearestByteAddress;


    FUNCTION_TRACE ("ExPrepCommonFieldObject");


    if (FieldBitLength > ACPI_UINT16_MAX)
    {
        REPORT_ERROR (("Field size too long (> 0xFFFF), not supported\n"));
        return_ACPI_STATUS (AE_SUPPORT);
    }

    /*
     * Note: the structure being initialized is the
     * ACPI_COMMON_FIELD_INFO;  No structure fields outside of the common
     * area are initialized by this procedure.
     */
    ObjDesc->CommonField.FieldFlags = FieldFlags;
    ObjDesc->CommonField.Attribute  = FieldAttribute;
    ObjDesc->CommonField.BitLength  = (UINT16) FieldBitLength;

    /*
     * Decode the access type so we can compute offsets.  The access type gives
     * two pieces of information - the width of each field access and the
     * necessary ByteAlignment (address granularity) of the access.  
     * 
     * For AnyAcc, the AccessBitWidth is the largest width that is both necessary
     * and possible in an attempt to access the whole field in one
     * I/O operation.  However, for AnyAcc, the ByteAlignment is always one byte.  
     * 
     * For all Buffer Fields, the ByteAlignment is always one byte.
     *
     * For all other access types (Byte, Word, Dword, Qword), the Bitwidth is the 
     * same (equivalent) as the ByteAlignment.
     */
    AccessBitWidth = AcpiExDecodeFieldAccess (ObjDesc, FieldFlags, &ByteAlignment);
    if (!AccessBitWidth)
    {
        return_ACPI_STATUS (AE_AML_OPERAND_VALUE);
    }

    /* Setup width (access granularity) fields */

    ObjDesc->CommonField.AccessBitWidth      = (UINT8) AccessBitWidth;         /* 8, 16, 32, 64 */
    ObjDesc->CommonField.AccessByteWidth     = (UINT8) DIV_8 (AccessBitWidth); /* 1,  2,  4,  8 */

    /*
     * BaseByteOffset is the address of the start of the field within the region.  It is
     * the byte address of the first *datum* (field-width data unit) of the field.
     * (i.e., the first datum that contains at least the first *bit* of the field.)
     * Note: ByteAlignment is always either equal to the AccessBitWidth or 8 (Byte access),
     * and it defines the addressing granularity of the parent region or buffer.
     */
    NearestByteAddress                       = ROUND_BITS_DOWN_TO_BYTES (FieldBitPosition);
    ObjDesc->CommonField.BaseByteOffset      = ROUND_DOWN (NearestByteAddress, ByteAlignment);

    /*
     * StartFieldBitOffset is the offset of the first bit of the field within a field datum.
     */
    ObjDesc->CommonField.StartFieldBitOffset = (UINT8) (FieldBitPosition - 
                                                        MUL_8 (ObjDesc->CommonField.BaseByteOffset));

    /*
     * Valid bits -- the number of bits that compose a partial datum,
     * 1) At the end of the field within the region (arbitrary starting bit offset)
     * 2) At the end of a buffer used to contain the field (starting offset always zero)
     */
    ObjDesc->CommonField.EndFieldValidBits   = (UINT8) ((ObjDesc->CommonField.StartFieldBitOffset + FieldBitLength) %
                                                            AccessBitWidth);
    ObjDesc->CommonField.EndBufferValidBits  = (UINT8) (FieldBitLength % AccessBitWidth); /* StartBufferBitOffset always = 0 */

    /*
     * DatumValidBits is the number of valid field bits in the first field datum.
     */
    ObjDesc->CommonField.DatumValidBits      = (UINT8) (AccessBitWidth -
                                                        ObjDesc->CommonField.StartFieldBitOffset);

    /*
     * Does the entire field fit within a single field access element? (datum)
     * (i.e., without crossing a datum boundary)
     */
    if ((ObjDesc->CommonField.StartFieldBitOffset + FieldBitLength) <=
        (UINT16) AccessBitWidth)
    {
        ObjDesc->Common.Flags |= AOPOBJ_SINGLE_DATUM;
    }

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExPrepFieldValue
 *
 * PARAMETERS:  Node                - Owning Node
 *              RegionNode          - Region in which field is being defined
 *              FieldFlags          - Access, LockRule, and UpdateRule.
 *              FieldBitPosition    - Field start position
 *              FieldBitLength      - Field length in number of bits
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Construct an ACPI_OPERAND_OBJECT of type DefField and
 *              connect it to the parent Node.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExPrepFieldValue (
    ACPI_CREATE_FIELD_INFO  *Info)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    UINT32                  Type;
    ACPI_STATUS             Status;


    FUNCTION_TRACE ("ExPrepFieldValue");


    /* Parameter validation */

    if (Info->FieldType != INTERNAL_TYPE_INDEX_FIELD)
    {
        if (!Info->RegionNode)
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Null RegionNode\n"));
            return_ACPI_STATUS (AE_AML_NO_OPERAND);
        }

        Type = AcpiNsGetType (Info->RegionNode);
        if (Type != ACPI_TYPE_REGION)
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, 
                "Needed Region, found type %X %s\n",
                Type, AcpiUtGetTypeName (Type)));

            return_ACPI_STATUS (AE_AML_OPERAND_TYPE);
        }
    }

    /* Allocate a new field object */

    ObjDesc = AcpiUtCreateInternalObject (Info->FieldType);
    if (!ObjDesc)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    /* Initialize areas of the object that are common to all fields */

    ObjDesc->CommonField.Node = Info->FieldNode;
    Status = AcpiExPrepCommonFieldObject (ObjDesc, Info->FieldFlags, 
                Info->Attribute, Info->FieldBitPosition, Info->FieldBitLength);
    if (ACPI_FAILURE (Status))
    {
        AcpiUtDeleteObjectDesc (ObjDesc);
        return_ACPI_STATUS (Status);
    }

    /* Initialize areas of the object that are specific to the field type */

    switch (Info->FieldType)
    {
    case INTERNAL_TYPE_REGION_FIELD:

        ObjDesc->Field.RegionObj     = AcpiNsGetAttachedObject (Info->RegionNode);

        /* An additional reference for the container */

        AcpiUtAddReference (ObjDesc->Field.RegionObj);

        ACPI_DEBUG_PRINT ((ACPI_DB_INFO, 
            "RegionField: Bitoff=%X Off=%X Gran=%X Region %p\n",
            ObjDesc->Field.StartFieldBitOffset, ObjDesc->Field.BaseByteOffset,
            ObjDesc->Field.AccessBitWidth, ObjDesc->Field.RegionObj));
        break;


    case INTERNAL_TYPE_BANK_FIELD:

        ObjDesc->BankField.Value     = Info->BankValue;
        ObjDesc->BankField.RegionObj = AcpiNsGetAttachedObject (Info->RegionNode);
        ObjDesc->BankField.BankObj   = AcpiNsGetAttachedObject (Info->RegisterNode);

        /* An additional reference for the attached objects */

        AcpiUtAddReference (ObjDesc->BankField.RegionObj);
        AcpiUtAddReference (ObjDesc->BankField.BankObj);

        ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "Bank Field: BitOff=%X Off=%X Gran=%X Region %p BankReg %p\n",
            ObjDesc->BankField.StartFieldBitOffset, ObjDesc->BankField.BaseByteOffset,
            ObjDesc->Field.AccessBitWidth, ObjDesc->BankField.RegionObj,
            ObjDesc->BankField.BankObj));
        break;


    case INTERNAL_TYPE_INDEX_FIELD:

        ObjDesc->IndexField.IndexObj = AcpiNsGetAttachedObject (Info->RegisterNode);
        ObjDesc->IndexField.DataObj  = AcpiNsGetAttachedObject (Info->DataRegisterNode);
        ObjDesc->IndexField.Value    = (UINT32) (Info->FieldBitPosition /
                                                ObjDesc->Field.AccessBitWidth);

        if (!ObjDesc->IndexField.DataObj || !ObjDesc->IndexField.IndexObj)
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Null Index Object\n"));
            return_ACPI_STATUS (AE_AML_INTERNAL);
        }

        /* An additional reference for the attached objects */

        AcpiUtAddReference (ObjDesc->IndexField.DataObj);
        AcpiUtAddReference (ObjDesc->IndexField.IndexObj);

        ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "IndexField: bitoff=%X off=%X gran=%X Index %p Data %p\n",
            ObjDesc->IndexField.StartFieldBitOffset, ObjDesc->IndexField.BaseByteOffset,
            ObjDesc->Field.AccessBitWidth, ObjDesc->IndexField.IndexObj,
            ObjDesc->IndexField.DataObj));
        break;
    }

    /*
     * Store the constructed descriptor (ObjDesc) into the parent Node,
     * preserving the current type of that NamedObj.
     */
    Status = AcpiNsAttachObject (Info->FieldNode, ObjDesc,
                    (UINT8) AcpiNsGetType (Info->FieldNode));

    ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "set NamedObj %p (%4.4s) val = %p\n",
            Info->FieldNode, (char*)&(Info->FieldNode->Name), ObjDesc));

    /* Remove local reference to the object */

    AcpiUtRemoveReference (ObjDesc);
    return_ACPI_STATUS (Status);
}

