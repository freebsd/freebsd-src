
/******************************************************************************
 *
 * Module Name: aslresource - Resource templates and descriptors
 *              $Revision: 31 $
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


#include "aslcompiler.h"
#include "aslcompiler.y.h"
#include "amlcode.h"


#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslresource")


/*******************************************************************************
 *
 * FUNCTION:    RsAllocateResourceNode
 *
 * PARAMETERS:  Size        - Size of node in bytes
 *
 * RETURN:      The allocated node - aborts on allocation failure
 *
 * DESCRIPTION: Allocate a resource description node and the resource
 *              descriptor itself (the nodes are used to link descriptors).
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsAllocateResourceNode (
    UINT32                  Size)
{
    ASL_RESOURCE_NODE       *Rnode;


    /* Allocate the node */

    Rnode = UtLocalCalloc (sizeof (ASL_RESOURCE_NODE));

    /* Allocate the resource descriptor itself */

    Rnode->Buffer = UtLocalCalloc (Size);
    Rnode->BufferLength = Size;

    return (Rnode);
}


/*******************************************************************************
 *
 * FUNCTION:    RsCreateBitField
 *
 * PARAMETERS:  Op            - Resource field node
 *              Name            - Name of the field (Used only to reference
 *                                the field in the ASL, not in the AML)
 *              ByteOffset      - Offset from the field start
 *              BitOffset       - Additional bit offset
 *
 * RETURN:      None, sets fields within the input node
 *
 * DESCRIPTION: Utility function to generate a named bit field within a
 *              resource descriptor.  Mark a node as 1) a field in a resource
 *              descriptor, and 2) set the value to be a BIT offset
 *
 ******************************************************************************/

void
RsCreateBitField (
    ACPI_PARSE_OBJECT       *Op,
    char                    *Name,
    UINT32                  ByteOffset,
    UINT32                  BitOffset)
{

    Op->Asl.ExternalName      = Name;
    Op->Asl.Value.Integer     = (ByteOffset * 8) + BitOffset;
    Op->Asl.CompileFlags     |= (NODE_IS_RESOURCE_FIELD | NODE_IS_BIT_OFFSET);
}


/*******************************************************************************
 *
 * FUNCTION:    RsCreateByteField
 *
 * PARAMETERS:  Op            - Resource field node
 *              Name            - Name of the field (Used only to reference
 *                                the field in the ASL, not in the AML)
 *              ByteOffset      - Offset from the field start
 *
 * RETURN:      None, sets fields within the input node
 *
 * DESCRIPTION: Utility function to generate a named byte field within a
 *              resource descriptor.  Mark a node as 1) a field in a resource
 *              descriptor, and 2) set the value to be a BYTE offset
 *
 ******************************************************************************/

void
RsCreateByteField (
    ACPI_PARSE_OBJECT       *Op,
    char                    *Name,
    UINT32                  ByteOffset)
{

    Op->Asl.ExternalName      = Name;
    Op->Asl.Value.Integer     = ByteOffset;
    Op->Asl.CompileFlags     |= NODE_IS_RESOURCE_FIELD;
}


/*******************************************************************************
 *
 * FUNCTION:    RsSetFlagBits
 *
 * PARAMETERS:  *Flags          - Pointer to the flag byte
 *              Op            - Flag initialization node
 *              Position        - Bit position within the flag byte
 *              Default         - Used if the node is DEFAULT.
 *
 * RETURN:      Sets bits within the *Flags output byte.
 *
 * DESCRIPTION: Set a bit in a cumulative flags word from an initialization
 *              node.  Will use a default value if the node is DEFAULT, meaning
 *              that no value was specified in the ASL.  Used to merge multiple
 *              keywords into a single flags byte.
 *
 ******************************************************************************/

void
RsSetFlagBits (
    UINT8                   *Flags,
    ACPI_PARSE_OBJECT       *Op,
    UINT8                   Position,
    UINT8                   DefaultBit)
{

    if (Op->Asl.ParseOpcode == PARSEOP_DEFAULT_ARG)
    {
        /* Use the default bit */

        *Flags |= (DefaultBit << Position);
    }
    else
    {
        /* Use the bit specified in the initialization node */

        *Flags |= (((UINT8) Op->Asl.Value.Integer) << Position);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    RsCompleteNodeAndGetNext
 *
 * PARAMETERS:  Op            - Resource node to be completed
 *
 * RETURN:      The next peer to the input node.
 *
 * DESCRIPTION: Mark the current node completed and return the next peer.
 *              The node ParseOpcode is set to DEFAULT_ARG, meaning that
 *              this node is to be ignored from now on.
 *
 ******************************************************************************/

ACPI_PARSE_OBJECT *
RsCompleteNodeAndGetNext (
    ACPI_PARSE_OBJECT       *Op)
{

    /* Mark this node unused */

    Op->Asl.ParseOpcode = PARSEOP_DEFAULT_ARG;

    /* Move on to the next peer node in the initializer list */

    return (ASL_GET_PEER_NODE (Op));
}


/*******************************************************************************
 *
 * FUNCTION:    RsDoOneResourceDescriptor
 *
 * PARAMETERS:  DescriptorTypeOp    - Parent parse node of the descriptor
 *              CurrentByteOffset   - Offset in the resource descriptor
 *                                    buffer.
 *
 * RETURN:      A valid resource node for the descriptor
 *
 * DESCRIPTION: Dispatches the processing of one resource descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoOneResourceDescriptor (
    ACPI_PARSE_OBJECT       *DescriptorTypeOp,
    UINT32                  CurrentByteOffset,
    UINT8                   *State)
{
    ASL_RESOURCE_NODE       *Rnode = NULL;


    /* Determine type of resource */

    switch (DescriptorTypeOp->Asl.ParseOpcode)
    {
    case PARSEOP_DMA:
        Rnode = RsDoDmaDescriptor (DescriptorTypeOp, CurrentByteOffset);
        break;

    case PARSEOP_DWORDIO:
        Rnode = RsDoDwordIoDescriptor (DescriptorTypeOp, CurrentByteOffset);
        break;

    case PARSEOP_DWORDMEMORY:
        Rnode = RsDoDwordMemoryDescriptor (DescriptorTypeOp, CurrentByteOffset);
        break;

    case PARSEOP_ENDDEPENDENTFN:
        switch (*State)
        {
        case ACPI_RSTATE_NORMAL:
            AslError (ASL_ERROR, ASL_MSG_MISSING_STARTDEPENDENT, DescriptorTypeOp, NULL);
            break;

        case ACPI_RSTATE_START_DEPENDENT:
            AslError (ASL_ERROR, ASL_MSG_DEPENDENT_NESTING, DescriptorTypeOp, NULL);
            break;

        case ACPI_RSTATE_DEPENDENT_LIST:
        default:
            break;
        }

        *State = ACPI_RSTATE_NORMAL;
        Rnode = RsDoEndDependentDescriptor (DescriptorTypeOp, CurrentByteOffset);
        break;

    case PARSEOP_FIXEDIO:
        Rnode = RsDoFixedIoDescriptor (DescriptorTypeOp, CurrentByteOffset);
        break;

    case PARSEOP_INTERRUPT:
        Rnode = RsDoInterruptDescriptor (DescriptorTypeOp, CurrentByteOffset);
        break;

    case PARSEOP_IO:
        Rnode = RsDoIoDescriptor (DescriptorTypeOp, CurrentByteOffset);
        break;

    case PARSEOP_IRQ:
        Rnode = RsDoIrqDescriptor (DescriptorTypeOp, CurrentByteOffset);
        break;

    case PARSEOP_IRQNOFLAGS:
        Rnode = RsDoIrqNoFlagsDescriptor (DescriptorTypeOp, CurrentByteOffset);
        break;

    case PARSEOP_MEMORY24:
        Rnode = RsDoMemory24Descriptor (DescriptorTypeOp, CurrentByteOffset);
        break;

    case PARSEOP_MEMORY32:
        Rnode = RsDoMemory32Descriptor (DescriptorTypeOp, CurrentByteOffset);
        break;

    case PARSEOP_MEMORY32FIXED:
        Rnode = RsDoMemory32FixedDescriptor (DescriptorTypeOp, CurrentByteOffset);
        break;

    case PARSEOP_QWORDIO:
        Rnode = RsDoQwordIoDescriptor (DescriptorTypeOp, CurrentByteOffset);
        break;

    case PARSEOP_QWORDMEMORY:
        Rnode = RsDoQwordMemoryDescriptor (DescriptorTypeOp, CurrentByteOffset);
        break;

    case PARSEOP_REGISTER:
        Rnode = RsDoGeneralRegisterDescriptor (DescriptorTypeOp, CurrentByteOffset);
        break;

    case PARSEOP_STARTDEPENDENTFN:
        switch (*State)
        {
        case ACPI_RSTATE_START_DEPENDENT:
            AslError (ASL_ERROR, ASL_MSG_DEPENDENT_NESTING, DescriptorTypeOp, NULL);
            break;

        case ACPI_RSTATE_NORMAL:
        case ACPI_RSTATE_DEPENDENT_LIST:
        default:
            break;
        }

        *State = ACPI_RSTATE_START_DEPENDENT;
        Rnode = RsDoStartDependentDescriptor (DescriptorTypeOp, CurrentByteOffset);
        *State = ACPI_RSTATE_DEPENDENT_LIST;
        break;

    case PARSEOP_STARTDEPENDENTFN_NOPRI:
        switch (*State)
        {
        case ACPI_RSTATE_START_DEPENDENT:
            AslError (ASL_ERROR, ASL_MSG_DEPENDENT_NESTING, DescriptorTypeOp, NULL);
            break;

        case ACPI_RSTATE_NORMAL:
        case ACPI_RSTATE_DEPENDENT_LIST:
        default:
            break;
        }

        *State = ACPI_RSTATE_START_DEPENDENT;
        Rnode = RsDoStartDependentNoPriDescriptor (DescriptorTypeOp, CurrentByteOffset);
        *State = ACPI_RSTATE_DEPENDENT_LIST;
        break;

    case PARSEOP_VENDORLONG:
        Rnode = RsDoVendorLargeDescriptor (DescriptorTypeOp, CurrentByteOffset);
        break;

    case PARSEOP_VENDORSHORT:
        Rnode = RsDoVendorSmallDescriptor (DescriptorTypeOp, CurrentByteOffset);
        break;

    case PARSEOP_WORDBUSNUMBER:
        Rnode = RsDoWordBusNumberDescriptor (DescriptorTypeOp, CurrentByteOffset);
        break;

    case PARSEOP_WORDIO:
        Rnode = RsDoWordIoDescriptor (DescriptorTypeOp, CurrentByteOffset);
        break;

    case PARSEOP_DEFAULT_ARG:
        /* Just ignore any of these, they are used as fillers/placeholders */
        break;

    default:
        printf ("Unknown resource descriptor type [%s]\n",
                    DescriptorTypeOp->Asl.ParseOpName);
        break;
    }

    /*
     * Mark original node as unused, but head of a resource descriptor.
     * This allows the resource to be installed in the namespace so that
     * references to the descriptor can be resolved.
     */
    DescriptorTypeOp->Asl.ParseOpcode = PARSEOP_DEFAULT_ARG;
    DescriptorTypeOp->Asl.CompileFlags = NODE_IS_RESOURCE_DESC;

    return (Rnode);
}


/*******************************************************************************
 *
 * FUNCTION:    RsLinkDescriptorChain
 *
 * PARAMETERS:  PreviousRnode       - Pointer to the node that will be previous
 *                                    to the linked node,  At exit, set to the
 *                                    last node in the new chain.
 *              Rnode               - Resource node to link into the list
 *
 * RETURN:      Cumulative buffer byte offset of the new segment of chain
 *
 * DESCRIPTION: Link a descriptor chain at the end of an existing chain.
 *
 ******************************************************************************/

UINT32
RsLinkDescriptorChain (
    ASL_RESOURCE_NODE       **PreviousRnode,
    ASL_RESOURCE_NODE       *Rnode)
{
    ASL_RESOURCE_NODE       *LastRnode;
    UINT32                  CurrentByteOffset;


    /* Anything to do? */

    if (!Rnode)
    {
        return 0;
    }

    /* Point the previous node to the new node */

    (*PreviousRnode)->Next = Rnode;
    CurrentByteOffset = Rnode->BufferLength;

    /* Walk to the end of the chain headed by Rnode */

    LastRnode = Rnode;
    while (LastRnode->Next)
    {
        LastRnode = LastRnode->Next;
        CurrentByteOffset += LastRnode->BufferLength;
    }

    /* Previous node becomes the last node in the chain */

    *PreviousRnode = LastRnode;
    return CurrentByteOffset;
}


/*******************************************************************************
 *
 * FUNCTION:    RsDoResourceTemplate
 *
 * PARAMETERS:  Op        - Parent of a resource template list
 *
 * RETURN:      None.  Sets input node to point to a list of AML code
 *
 * DESCRIPTION: Merge a list of resource descriptors into a single AML buffer,
 *              in preparation for output to the AML output file.
 *
 ******************************************************************************/

void
RsDoResourceTemplate (
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_PARSE_OBJECT       *BufferLengthOp;
    ACPI_PARSE_OBJECT       *BufferOp;
    ACPI_PARSE_OBJECT       *DescriptorTypeOp;
    ACPI_PARSE_OBJECT       *LastOp = NULL;
    ASL_RESOURCE_DESC       *Descriptor;
    UINT32                  CurrentByteOffset = 0;
    ASL_RESOURCE_NODE       HeadRnode;
    ASL_RESOURCE_NODE       *PreviousRnode;
    ASL_RESOURCE_NODE       *Rnode;
    UINT8                   State;


    /* ResourceTemplate Opcode is first (Op) */
    /* Buffer Length node is first child */

    BufferLengthOp = ASL_GET_CHILD_NODE (Op);

    /* Buffer Op is first peer */

    BufferOp = ASL_GET_PEER_NODE (BufferLengthOp);

    /* First Descriptor type is next */

    DescriptorTypeOp = ASL_GET_PEER_NODE (BufferOp);

    /* Process all resource descriptors in the list */

    State = ACPI_RSTATE_NORMAL;
    PreviousRnode = &HeadRnode;
    while (DescriptorTypeOp)
    {
        Rnode = RsDoOneResourceDescriptor (DescriptorTypeOp, CurrentByteOffset, &State);

        /*
         * Update current byte offset to indicate the number of bytes from the
         * start of the buffer.  Buffer can include multiple descriptors, we
         * must keep track of the offset of not only each descriptor, but each
         * element (field) within each descriptor as well.
         */
        CurrentByteOffset += RsLinkDescriptorChain (&PreviousRnode, Rnode);

        /* Get the next descriptor in the list */

        LastOp = DescriptorTypeOp;
        DescriptorTypeOp = ASL_GET_PEER_NODE (DescriptorTypeOp);
    }

    if (State == ACPI_RSTATE_DEPENDENT_LIST)
    {
        if (LastOp)
        {
            LastOp = LastOp->Asl.Parent;
        }
        AslError (ASL_ERROR, ASL_MSG_MISSING_ENDDEPENDENT, LastOp, NULL);
    }

    /*
     * Insert the EndTag descriptor after all other descriptors have been processed
     */
    Rnode = RsAllocateResourceNode (sizeof (ASL_END_TAG_DESC));

    Descriptor = Rnode->Buffer;
    Descriptor->Et.DescriptorType = ACPI_RDESC_TYPE_END_TAG |
                                        ASL_RDESC_END_TAG_SIZE;
    Descriptor->Et.Checksum = 0;

    CurrentByteOffset += RsLinkDescriptorChain (&PreviousRnode, Rnode);

    /*
     * Transform the nodes into the following
     *
     * Op           -> AML_BUFFER_OP
     * First Child  -> BufferLength
     * Second Child -> Descriptor Buffer (raw byte data)
     */
    Op->Asl.ParseOpcode               = PARSEOP_BUFFER;
    Op->Asl.AmlOpcode                 = AML_BUFFER_OP;
    Op->Asl.CompileFlags              = NODE_AML_PACKAGE;

    BufferLengthOp->Asl.ParseOpcode   = PARSEOP_INTEGER;
    BufferLengthOp->Asl.Value.Integer = CurrentByteOffset;

    (void) OpcSetOptimalIntegerSize (BufferLengthOp);

    BufferOp->Asl.ParseOpcode         = PARSEOP_RAW_DATA;
    BufferOp->Asl.AmlOpcode           = AML_RAW_DATA_CHAIN;
    BufferOp->Asl.AmlOpcodeLength     = 0;
    BufferOp->Asl.AmlLength           = CurrentByteOffset;
    BufferOp->Asl.Value.Buffer        = (UINT8 *) HeadRnode.Next;

    return;
}


