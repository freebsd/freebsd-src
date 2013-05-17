/******************************************************************************
 *
 * Module Name: aslresource - Resource template/descriptor utilities
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2013, Intel Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */


#include "aslcompiler.h"
#include "aslcompiler.y.h"
#include "amlcode.h"


#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslresource")


/*******************************************************************************
 *
 * FUNCTION:    RsSmallAddressCheck
 *
 * PARAMETERS:  Minimum             - Address Min value
 *              Maximum             - Address Max value
 *              Length              - Address range value
 *              Alignment           - Address alignment value
 *              MinOp               - Original Op for Address Min
 *              MaxOp               - Original Op for Address Max
 *              LengthOp            - Original Op for address range
 *              AlignOp             - Original Op for address alignment. If
 *                                    NULL, means "zero value for alignment is
 *                                    OK, and means 64K alignment" (for
 *                                    Memory24 descriptor)
 *              Op                  - Parent Op for entire construct
 *
 * RETURN:      None. Adds error messages to error log if necessary
 *
 * DESCRIPTION: Perform common value checks for "small" address descriptors.
 *              Currently:
 *                  Io, Memory24, Memory32
 *
 ******************************************************************************/

void
RsSmallAddressCheck (
    UINT8                   Type,
    UINT32                  Minimum,
    UINT32                  Maximum,
    UINT32                  Length,
    UINT32                  Alignment,
    ACPI_PARSE_OBJECT       *MinOp,
    ACPI_PARSE_OBJECT       *MaxOp,
    ACPI_PARSE_OBJECT       *LengthOp,
    ACPI_PARSE_OBJECT       *AlignOp,
    ACPI_PARSE_OBJECT       *Op)
{

    if (Gbl_NoResourceChecking)
    {
        return;
    }

    /*
     * Check for a so-called "null descriptor". These are descriptors that are
     * created with most fields set to zero. The intent is that the descriptor
     * will be updated/completed at runtime via a BufferField.
     *
     * If the descriptor does NOT have a resource tag, it cannot be referenced
     * by a BufferField and we will flag this as an error. Conversely, if
     * the descriptor has a resource tag, we will assume that a BufferField
     * will be used to dynamically update it, so no error.
     *
     * A possible enhancement to this check would be to verify that in fact
     * a BufferField is created using the resource tag, and perhaps even
     * verify that a Store is performed to the BufferField.
     *
     * Note: for these descriptors, Alignment is allowed to be zero
     */
    if (!Minimum && !Maximum && !Length)
    {
        if (!Op->Asl.ExternalName)
        {
            /* No resource tag. Descriptor is fixed and is also illegal */

            AslError (ASL_ERROR, ASL_MSG_NULL_DESCRIPTOR, Op, NULL);
        }

        return;
    }

    /* Special case for Memory24, values are compressed */

    if (Type == ACPI_RESOURCE_NAME_MEMORY24)
    {
        if (!Alignment) /* Alignment==0 means 64K - no invalid alignment */
        {
            Alignment = ACPI_UINT16_MAX + 1;
        }

        Minimum <<= 8;
        Maximum <<= 8;
        Length *= 256;
    }

    /* IO descriptor has different definition of min/max, don't check */

    if (Type != ACPI_RESOURCE_NAME_IO)
    {
        /* Basic checks on Min/Max/Length */

        if (Minimum > Maximum)
        {
            AslError (ASL_ERROR, ASL_MSG_INVALID_MIN_MAX, MinOp, NULL);
        }
        else if (Length > (Maximum - Minimum + 1))
        {
            AslError (ASL_ERROR, ASL_MSG_INVALID_LENGTH, LengthOp, NULL);
        }
    }

    /* Alignment of zero is not in ACPI spec, but is used to mean byte acc */

    if (!Alignment)
    {
        Alignment = 1;
    }

    /* Addresses must be an exact multiple of the alignment value */

    if (Minimum % Alignment)
    {
        AslError (ASL_ERROR, ASL_MSG_ALIGNMENT, MinOp, NULL);
    }
    if (Maximum % Alignment)
    {
        AslError (ASL_ERROR, ASL_MSG_ALIGNMENT, MaxOp, NULL);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    RsLargeAddressCheck
 *
 * PARAMETERS:  Minimum             - Address Min value
 *              Maximum             - Address Max value
 *              Length              - Address range value
 *              Granularity         - Address granularity value
 *              Flags               - General flags for address descriptors:
 *                                    _MIF, _MAF, _DEC
 *              MinOp               - Original Op for Address Min
 *              MaxOp               - Original Op for Address Max
 *              LengthOp            - Original Op for address range
 *              GranOp              - Original Op for address granularity
 *              Op                  - Parent Op for entire construct
 *
 * RETURN:      None. Adds error messages to error log if necessary
 *
 * DESCRIPTION: Perform common value checks for "large" address descriptors.
 *              Currently:
 *                  WordIo,     WordBusNumber,  WordSpace
 *                  DWordIo,    DWordMemory,    DWordSpace
 *                  QWordIo,    QWordMemory,    QWordSpace
 *                  ExtendedIo, ExtendedMemory, ExtendedSpace
 *
 * _MIF flag set means that the minimum address is fixed and is not relocatable
 * _MAF flag set means that the maximum address is fixed and is not relocatable
 * Length of zero means that the record size is variable
 *
 * This function implements the LEN/MIF/MAF/MIN/MAX/GRA rules within Table 6-40
 * of the ACPI 4.0a specification. Added 04/2010.
 *
 ******************************************************************************/

void
RsLargeAddressCheck (
    UINT64                  Minimum,
    UINT64                  Maximum,
    UINT64                  Length,
    UINT64                  Granularity,
    UINT8                   Flags,
    ACPI_PARSE_OBJECT       *MinOp,
    ACPI_PARSE_OBJECT       *MaxOp,
    ACPI_PARSE_OBJECT       *LengthOp,
    ACPI_PARSE_OBJECT       *GranOp,
    ACPI_PARSE_OBJECT       *Op)
{

    if (Gbl_NoResourceChecking)
    {
        return;
    }

    /*
     * Check for a so-called "null descriptor". These are descriptors that are
     * created with most fields set to zero. The intent is that the descriptor
     * will be updated/completed at runtime via a BufferField.
     *
     * If the descriptor does NOT have a resource tag, it cannot be referenced
     * by a BufferField and we will flag this as an error. Conversely, if
     * the descriptor has a resource tag, we will assume that a BufferField
     * will be used to dynamically update it, so no error.
     *
     * A possible enhancement to this check would be to verify that in fact
     * a BufferField is created using the resource tag, and perhaps even
     * verify that a Store is performed to the BufferField.
     */
    if (!Minimum && !Maximum && !Length && !Granularity)
    {
        if (!Op->Asl.ExternalName)
        {
            /* No resource tag. Descriptor is fixed and is also illegal */

            AslError (ASL_ERROR, ASL_MSG_NULL_DESCRIPTOR, Op, NULL);
        }

        return;
    }

    /* Basic checks on Min/Max/Length */

    if (Minimum > Maximum)
    {
        AslError (ASL_ERROR, ASL_MSG_INVALID_MIN_MAX, MinOp, NULL);
        return;
    }
    else if (Length > (Maximum - Minimum + 1))
    {
        AslError (ASL_ERROR, ASL_MSG_INVALID_LENGTH, LengthOp, NULL);
        return;
    }

    /* If specified (non-zero), ensure granularity is a power-of-two minus one */

    if (Granularity)
    {
        if ((Granularity + 1) &
             Granularity)
        {
            AslError (ASL_ERROR, ASL_MSG_INVALID_GRANULARITY, GranOp, NULL);
            return;
        }
    }

    /*
     * Check the various combinations of Length, MinFixed, and MaxFixed
     */
    if (Length)
    {
        /* Fixed non-zero length */

        switch (Flags & (ACPI_RESOURCE_FLAG_MIF | ACPI_RESOURCE_FLAG_MAF))
        {
        case 0:
            /*
             * Fixed length, variable locations (both _MIN and _MAX).
             * Length must be a multiple of granularity
             */
            if (Granularity & Length)
            {
                AslError (ASL_ERROR, ASL_MSG_ALIGNMENT, LengthOp, NULL);
            }
            break;

        case (ACPI_RESOURCE_FLAG_MIF | ACPI_RESOURCE_FLAG_MAF):

            /* Fixed length, fixed location. Granularity must be zero */

            if (Granularity != 0)
            {
                AslError (ASL_ERROR, ASL_MSG_INVALID_GRAN_FIXED, GranOp, NULL);
            }

            /* Length must be exactly the size of the min/max window */

            if (Length != (Maximum - Minimum + 1))
            {
                AslError (ASL_ERROR, ASL_MSG_INVALID_LENGTH_FIXED, LengthOp, NULL);
            }
            break;

        /* All other combinations are invalid */

        case ACPI_RESOURCE_FLAG_MIF:
        case ACPI_RESOURCE_FLAG_MAF:
        default:

            AslError (ASL_ERROR, ASL_MSG_INVALID_ADDR_FLAGS, LengthOp, NULL);
        }
    }
    else
    {
        /* Variable length (length==0) */

        switch (Flags & (ACPI_RESOURCE_FLAG_MIF | ACPI_RESOURCE_FLAG_MAF))
        {
        case 0:
            /*
             * Both _MIN and _MAX are variable.
             * No additional requirements, just exit
             */
            break;

        case ACPI_RESOURCE_FLAG_MIF:

            /* _MIN is fixed. _MIN must be multiple of _GRA */

            /*
             * The granularity is defined by the ACPI specification to be a
             * power-of-two minus one, therefore the granularity is a
             * bitmask which can be used to easily validate the addresses.
             */
            if (Granularity & Minimum)
            {
                AslError (ASL_ERROR, ASL_MSG_ALIGNMENT, MinOp, NULL);
            }
            break;

        case ACPI_RESOURCE_FLAG_MAF:

            /* _MAX is fixed. (_MAX + 1) must be multiple of _GRA */

            if (Granularity & (Maximum + 1))
            {
                AslError (ASL_ERROR, ASL_MSG_ALIGNMENT, MaxOp, "-1");
            }
            break;

        /* Both MIF/MAF set is invalid if length is zero */

        case (ACPI_RESOURCE_FLAG_MIF | ACPI_RESOURCE_FLAG_MAF):
        default:

            AslError (ASL_ERROR, ASL_MSG_INVALID_ADDR_FLAGS, LengthOp, NULL);
        }
    }
}


/*******************************************************************************
 *
 * FUNCTION:    RsGetStringDataLength
 *
 * PARAMETERS:  InitializerOp     - Start of a subtree of init nodes
 *
 * RETURN:      Valid string length if a string node is found (otherwise 0)
 *
 * DESCRIPTION: In a list of peer nodes, find the first one that contains a
 *              string and return the length of the string.
 *
 ******************************************************************************/

UINT16
RsGetStringDataLength (
    ACPI_PARSE_OBJECT       *InitializerOp)
{

    while (InitializerOp)
    {
        if (InitializerOp->Asl.ParseOpcode == PARSEOP_STRING_LITERAL)
        {
            return ((UINT16) (strlen (InitializerOp->Asl.Value.String) + 1));
        }
        InitializerOp = ASL_GET_PEER_NODE (InitializerOp);
    }

    return (0);
}


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
 * FUNCTION:    RsCreateResourceField
 *
 * PARAMETERS:  Op              - Resource field node
 *              Name            - Name of the field (Used only to reference
 *                                the field in the ASL, not in the AML)
 *              ByteOffset      - Offset from the field start
 *              BitOffset       - Additional bit offset
 *              BitLength       - Number of bits in the field
 *
 * RETURN:      None, sets fields within the input node
 *
 * DESCRIPTION: Utility function to generate a named bit field within a
 *              resource descriptor. Mark a node as 1) a field in a resource
 *              descriptor, and 2) set the value to be a BIT offset
 *
 ******************************************************************************/

void
RsCreateResourceField (
    ACPI_PARSE_OBJECT       *Op,
    char                    *Name,
    UINT32                  ByteOffset,
    UINT32                  BitOffset,
    UINT32                  BitLength)
{

    Op->Asl.ExternalName = Name;
    Op->Asl.CompileFlags |= NODE_IS_RESOURCE_FIELD;


    Op->Asl.Value.Tag.BitOffset = (ByteOffset * 8) + BitOffset;
    Op->Asl.Value.Tag.BitLength = BitLength;
}


/*******************************************************************************
 *
 * FUNCTION:    RsSetFlagBits
 *
 * PARAMETERS:  *Flags          - Pointer to the flag byte
 *              Op              - Flag initialization node
 *              Position        - Bit position within the flag byte
 *              Default         - Used if the node is DEFAULT.
 *
 * RETURN:      Sets bits within the *Flags output byte.
 *
 * DESCRIPTION: Set a bit in a cumulative flags word from an initialization
 *              node. Will use a default value if the node is DEFAULT, meaning
 *              that no value was specified in the ASL. Used to merge multiple
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


void
RsSetFlagBits16 (
    UINT16                  *Flags,
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

        *Flags |= (((UINT16) Op->Asl.Value.Integer) << Position);
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
 * FUNCTION:    RsCheckListForDuplicates
 *
 * PARAMETERS:  Op                  - First op in the initializer list
 *
 * RETURN:      None
 *
 * DESCRIPTION: Check an initializer list for duplicate values. Emits an error
 *              if any duplicates are found.
 *
 ******************************************************************************/

void
RsCheckListForDuplicates (
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_PARSE_OBJECT       *NextValueOp = Op;
    ACPI_PARSE_OBJECT       *NextOp;
    UINT32                  Value;


    if (!Op)
    {
        return;
    }

    /* Search list once for each value in the list */

    while (NextValueOp)
    {
        Value = (UINT32) NextValueOp->Asl.Value.Integer;

        /* Compare this value to all remaining values in the list */

        NextOp = ASL_GET_PEER_NODE (NextValueOp);
        while (NextOp)
        {
            if (NextOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG)
            {
                /* Compare values */

                if (Value == (UINT32) NextOp->Asl.Value.Integer)
                {
                    /* Emit error only once per duplicate node */

                    if (!(NextOp->Asl.CompileFlags & NODE_IS_DUPLICATE))
                    {
                        NextOp->Asl.CompileFlags |= NODE_IS_DUPLICATE;
                        AslError (ASL_ERROR, ASL_MSG_DUPLICATE_ITEM,
                            NextOp, NULL);
                    }
                }
            }

            NextOp = ASL_GET_PEER_NODE (NextOp);
        }

        NextValueOp = ASL_GET_PEER_NODE (NextValueOp);
    }
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


    /* Construct the resource */

    switch (DescriptorTypeOp->Asl.ParseOpcode)
    {
    case PARSEOP_DMA:

        Rnode = RsDoDmaDescriptor (DescriptorTypeOp,
                    CurrentByteOffset);
        break;

    case PARSEOP_FIXEDDMA:

        Rnode = RsDoFixedDmaDescriptor (DescriptorTypeOp,
                    CurrentByteOffset);
        break;

    case PARSEOP_DWORDIO:

        Rnode = RsDoDwordIoDescriptor (DescriptorTypeOp,
                    CurrentByteOffset);
        break;

    case PARSEOP_DWORDMEMORY:

        Rnode = RsDoDwordMemoryDescriptor (DescriptorTypeOp,
                    CurrentByteOffset);
        break;

    case PARSEOP_DWORDSPACE:

        Rnode = RsDoDwordSpaceDescriptor (DescriptorTypeOp,
                    CurrentByteOffset);
        break;

    case PARSEOP_ENDDEPENDENTFN:

        switch (*State)
        {
        case ACPI_RSTATE_NORMAL:

            AslError (ASL_ERROR, ASL_MSG_MISSING_STARTDEPENDENT,
                DescriptorTypeOp, NULL);
            break;

        case ACPI_RSTATE_START_DEPENDENT:

            AslError (ASL_ERROR, ASL_MSG_DEPENDENT_NESTING,
                DescriptorTypeOp, NULL);
            break;

        case ACPI_RSTATE_DEPENDENT_LIST:
        default:

            break;
        }

        *State = ACPI_RSTATE_NORMAL;
        Rnode = RsDoEndDependentDescriptor (DescriptorTypeOp,
                    CurrentByteOffset);
        break;

    case PARSEOP_ENDTAG:

        Rnode = RsDoEndTagDescriptor (DescriptorTypeOp,
                    CurrentByteOffset);
        break;

    case PARSEOP_EXTENDEDIO:

        Rnode = RsDoExtendedIoDescriptor (DescriptorTypeOp,
                    CurrentByteOffset);
        break;

    case PARSEOP_EXTENDEDMEMORY:

        Rnode = RsDoExtendedMemoryDescriptor (DescriptorTypeOp,
                    CurrentByteOffset);
        break;

    case PARSEOP_EXTENDEDSPACE:

        Rnode = RsDoExtendedSpaceDescriptor (DescriptorTypeOp,
                    CurrentByteOffset);
        break;

    case PARSEOP_FIXEDIO:

        Rnode = RsDoFixedIoDescriptor (DescriptorTypeOp,
                    CurrentByteOffset);
        break;

    case PARSEOP_INTERRUPT:

        Rnode = RsDoInterruptDescriptor (DescriptorTypeOp,
                    CurrentByteOffset);
        break;

    case PARSEOP_IO:

        Rnode = RsDoIoDescriptor (DescriptorTypeOp,
                    CurrentByteOffset);
        break;

    case PARSEOP_IRQ:

        Rnode = RsDoIrqDescriptor (DescriptorTypeOp,
                    CurrentByteOffset);
        break;

    case PARSEOP_IRQNOFLAGS:

        Rnode = RsDoIrqNoFlagsDescriptor (DescriptorTypeOp,
                    CurrentByteOffset);
        break;

    case PARSEOP_MEMORY24:

        Rnode = RsDoMemory24Descriptor (DescriptorTypeOp,
                    CurrentByteOffset);
        break;

    case PARSEOP_MEMORY32:

        Rnode = RsDoMemory32Descriptor (DescriptorTypeOp,
                    CurrentByteOffset);
        break;

    case PARSEOP_MEMORY32FIXED:

        Rnode = RsDoMemory32FixedDescriptor (DescriptorTypeOp,
                    CurrentByteOffset);
        break;

    case PARSEOP_QWORDIO:

        Rnode = RsDoQwordIoDescriptor (DescriptorTypeOp,
                    CurrentByteOffset);
        break;

    case PARSEOP_QWORDMEMORY:

        Rnode = RsDoQwordMemoryDescriptor (DescriptorTypeOp,
                    CurrentByteOffset);
        break;

    case PARSEOP_QWORDSPACE:

        Rnode = RsDoQwordSpaceDescriptor (DescriptorTypeOp,
                    CurrentByteOffset);
        break;

    case PARSEOP_REGISTER:

        Rnode = RsDoGeneralRegisterDescriptor (DescriptorTypeOp,
                    CurrentByteOffset);
        break;

    case PARSEOP_STARTDEPENDENTFN:

        switch (*State)
        {
        case ACPI_RSTATE_START_DEPENDENT:

            AslError (ASL_ERROR, ASL_MSG_DEPENDENT_NESTING,
                DescriptorTypeOp, NULL);
            break;

        case ACPI_RSTATE_NORMAL:
        case ACPI_RSTATE_DEPENDENT_LIST:
        default:

            break;
        }

        *State = ACPI_RSTATE_START_DEPENDENT;
        Rnode = RsDoStartDependentDescriptor (DescriptorTypeOp,
                    CurrentByteOffset);
        *State = ACPI_RSTATE_DEPENDENT_LIST;
        break;

    case PARSEOP_STARTDEPENDENTFN_NOPRI:

        switch (*State)
        {
        case ACPI_RSTATE_START_DEPENDENT:

            AslError (ASL_ERROR, ASL_MSG_DEPENDENT_NESTING,
                DescriptorTypeOp, NULL);
            break;

        case ACPI_RSTATE_NORMAL:
        case ACPI_RSTATE_DEPENDENT_LIST:
        default:

            break;
        }

        *State = ACPI_RSTATE_START_DEPENDENT;
        Rnode = RsDoStartDependentNoPriDescriptor (DescriptorTypeOp,
                    CurrentByteOffset);
        *State = ACPI_RSTATE_DEPENDENT_LIST;
        break;

    case PARSEOP_VENDORLONG:

        Rnode = RsDoVendorLargeDescriptor (DescriptorTypeOp,
                    CurrentByteOffset);
        break;

    case PARSEOP_VENDORSHORT:

        Rnode = RsDoVendorSmallDescriptor (DescriptorTypeOp,
                    CurrentByteOffset);
        break;

    case PARSEOP_WORDBUSNUMBER:

        Rnode = RsDoWordBusNumberDescriptor (DescriptorTypeOp,
                    CurrentByteOffset);
        break;

    case PARSEOP_WORDIO:

        Rnode = RsDoWordIoDescriptor (DescriptorTypeOp,
                    CurrentByteOffset);
        break;

    case PARSEOP_WORDSPACE:

        Rnode = RsDoWordSpaceDescriptor (DescriptorTypeOp,
                    CurrentByteOffset);
        break;

    case PARSEOP_GPIO_INT:

        Rnode = RsDoGpioIntDescriptor (DescriptorTypeOp,
                    CurrentByteOffset);
        break;

    case PARSEOP_GPIO_IO:

        Rnode = RsDoGpioIoDescriptor (DescriptorTypeOp,
                    CurrentByteOffset);
        break;

    case PARSEOP_I2C_SERIALBUS:

        Rnode = RsDoI2cSerialBusDescriptor (DescriptorTypeOp,
                    CurrentByteOffset);
        break;

    case PARSEOP_SPI_SERIALBUS:

        Rnode = RsDoSpiSerialBusDescriptor (DescriptorTypeOp,
                    CurrentByteOffset);
        break;

    case PARSEOP_UART_SERIALBUS:

        Rnode = RsDoUartSerialBusDescriptor (DescriptorTypeOp,
                    CurrentByteOffset);
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
    DescriptorTypeOp->Asl.Value.Integer = CurrentByteOffset;

    if (Rnode)
    {
        DescriptorTypeOp->Asl.FinalAmlLength = Rnode->BufferLength;
        DescriptorTypeOp->Asl.Extra = ((AML_RESOURCE *) Rnode->Buffer)->DescriptorType;
    }

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
        return (0);
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
    return (CurrentByteOffset);
}


/*******************************************************************************
 *
 * FUNCTION:    RsDoResourceTemplate
 *
 * PARAMETERS:  Op        - Parent of a resource template list
 *
 * RETURN:      None. Sets input node to point to a list of AML code
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
    UINT32                  CurrentByteOffset = 0;
    ASL_RESOURCE_NODE       HeadRnode;
    ASL_RESOURCE_NODE       *PreviousRnode;
    ASL_RESOURCE_NODE       *Rnode;
    UINT8                   State;


    /* Mark parent as containing a resource template */

    if (Op->Asl.Parent)
    {
        Op->Asl.Parent->Asl.CompileFlags |= NODE_IS_RESOURCE_DESC;
    }

    /* ResourceTemplate Opcode is first (Op) */
    /* Buffer Length node is first child */

    BufferLengthOp = ASL_GET_CHILD_NODE (Op);

    /* Buffer Op is first peer */

    BufferOp = ASL_GET_PEER_NODE (BufferLengthOp);

    /* First Descriptor type is next */

    DescriptorTypeOp = ASL_GET_PEER_NODE (BufferOp);

    /*
     * Process all resource descriptors in the list
     * Note: It is assumed that the EndTag node has been automatically
     * inserted at the end of the template by the parser.
     */
    State = ACPI_RSTATE_NORMAL;
    PreviousRnode = &HeadRnode;
    while (DescriptorTypeOp)
    {
        DescriptorTypeOp->Asl.CompileFlags |= NODE_IS_RESOURCE_DESC;
        Rnode = RsDoOneResourceDescriptor (DescriptorTypeOp, CurrentByteOffset,
                    &State);

        /*
         * Update current byte offset to indicate the number of bytes from the
         * start of the buffer. Buffer can include multiple descriptors, we
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
     * Transform the nodes into the following
     *
     * Op           -> AML_BUFFER_OP
     * First Child  -> BufferLength
     * Second Child -> Descriptor Buffer (raw byte data)
     */
    Op->Asl.ParseOpcode               = PARSEOP_BUFFER;
    Op->Asl.AmlOpcode                 = AML_BUFFER_OP;
    Op->Asl.CompileFlags              = NODE_AML_PACKAGE | NODE_IS_RESOURCE_DESC;
    UtSetParseOpName (Op);

    BufferLengthOp->Asl.ParseOpcode   = PARSEOP_INTEGER;
    BufferLengthOp->Asl.Value.Integer = CurrentByteOffset;
    (void) OpcSetOptimalIntegerSize (BufferLengthOp);
    UtSetParseOpName (BufferLengthOp);

    BufferOp->Asl.ParseOpcode         = PARSEOP_RAW_DATA;
    BufferOp->Asl.AmlOpcode           = AML_RAW_DATA_CHAIN;
    BufferOp->Asl.AmlOpcodeLength     = 0;
    BufferOp->Asl.AmlLength           = CurrentByteOffset;
    BufferOp->Asl.Value.Buffer        = (UINT8 *) HeadRnode.Next;
    BufferOp->Asl.CompileFlags       |= NODE_IS_RESOURCE_DATA;
    UtSetParseOpName (BufferOp);

    return;
}
