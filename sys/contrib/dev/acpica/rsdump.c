/*******************************************************************************
 *
 * Module Name: rsdump - Functions do dump out the resource structures.
 *              $Revision: 11 $
 *
 ******************************************************************************/

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


#define __RSDUMP_C__

#include "acpi.h"

#define _COMPONENT          RESOURCE_MANAGER
        MODULE_NAME         ("rsdump")


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsDumpIrq
 *
 * PARAMETERS:  Data            - pointer to the resource structure to dump.
 *
 * RETURN:
 *
 * DESCRIPTION: Prints out the various members of the Data structure type.
 *
 ******************************************************************************/

void
AcpiRsDumpIrq (
    RESOURCE_DATA           *Data)
{
    IRQ_RESOURCE            *IrqData = (IRQ_RESOURCE*) Data;
    UINT8                   Index = 0;


    AcpiOsPrintf ("\tIRQ Resource\n");

    AcpiOsPrintf ("\t\t%s Triggered\n",
                LEVEL_SENSITIVE == IrqData->EdgeLevel ?
                "Level" : "Edge");

    AcpiOsPrintf ("\t\tActive %s\n",
                ACTIVE_LOW == IrqData->ActiveHighLow ?
                "Low" : "High");

    AcpiOsPrintf ("\t\t%s\n",
                SHARED == IrqData->SharedExclusive ?
                "Shared" : "Exclusive");

    AcpiOsPrintf ("\t\t%d Interrupts ( ",
                IrqData->NumberOfInterrupts);

    for (Index = 0; Index < IrqData->NumberOfInterrupts; Index++)
    {
        AcpiOsPrintf ("%d ", IrqData->Interrupts[Index]);
    }

    AcpiOsPrintf (")\n");
    return;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsDumpDma
 *
 * PARAMETERS:  Data            - pointer to the resource structure to dump.
 *
 * RETURN:
 *
 * DESCRIPTION: Prints out the various members of the Data structure type.
 *
 ******************************************************************************/

void
AcpiRsDumpDma (
    RESOURCE_DATA           *Data)
{
    DMA_RESOURCE            *DmaData = (DMA_RESOURCE*) Data;
    UINT8                   Index = 0;


    AcpiOsPrintf ("\tDMA Resource\n");

    switch (DmaData->Type)
    {
    case COMPATIBILITY:
        AcpiOsPrintf ("\t\tCompatibility mode\n");
        break;

    case TYPE_A:
        AcpiOsPrintf ("\t\tType A\n");
        break;

    case TYPE_B:
        AcpiOsPrintf ("\t\tType B\n");
        break;

    case TYPE_F:
        AcpiOsPrintf ("\t\tType F\n");
        break;

    default:
        AcpiOsPrintf ("\t\tInvalid DMA type\n");
        break;
    }

    AcpiOsPrintf ("\t\t%sBus Master\n",
                BUS_MASTER == DmaData->BusMaster ?
                "" : "Not a ");

    switch (DmaData->Transfer)
    {
    case TRANSFER_8:
        AcpiOsPrintf ("\t\t8-bit only transfer\n");
        break;

    case TRANSFER_8_16:
        AcpiOsPrintf ("\t\t8 and 16-bit transfer\n");
        break;

    case TRANSFER_16:
        AcpiOsPrintf ("\t\t16 bit only transfer\n");
        break;

    default:
        AcpiOsPrintf ("\t\tInvalid transfer preference\n");
        break;
    }

    AcpiOsPrintf ("\t\tNumber of Channels: %d ( ",
                DmaData->NumberOfChannels);

    for (Index = 0; Index < DmaData->NumberOfChannels; Index++)
    {
        AcpiOsPrintf ("%d ", DmaData->Channels[Index]);
    }

    AcpiOsPrintf (")\n");
    return;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsDumpStartDependentFunctions
 *
 * PARAMETERS:  Data            - pointer to the resource structure to dump.
 *
 * RETURN:
 *
 * DESCRIPTION: Prints out the various members of the Data structure type.
 *
 ******************************************************************************/

void
AcpiRsDumpStartDependentFunctions (
    RESOURCE_DATA               *Data)
{
    START_DEPENDENT_FUNCTIONS_RESOURCE *SdfData =
                        (START_DEPENDENT_FUNCTIONS_RESOURCE*) Data;


    AcpiOsPrintf ("\tStart Dependent Functions Resource\n");

    switch (SdfData->CompatibilityPriority)
    {
    case GOOD_CONFIGURATION:
        AcpiOsPrintf ("\t\tGood configuration\n");
        break;

    case ACCEPTABLE_CONFIGURATION:
        AcpiOsPrintf ("\t\tAcceptable configuration\n");
        break;

    case SUB_OPTIMAL_CONFIGURATION:
        AcpiOsPrintf ("\t\tSub-optimal configuration\n");
        break;

    default:
        AcpiOsPrintf ("\t\tInvalid compatibility priority\n");
        break;
    }

    switch(SdfData->PerformanceRobustness)
    {
    case GOOD_CONFIGURATION:
        AcpiOsPrintf ("\t\tGood configuration\n");
        break;

    case ACCEPTABLE_CONFIGURATION:
        AcpiOsPrintf ("\t\tAcceptable configuration\n");
        break;

    case SUB_OPTIMAL_CONFIGURATION:
        AcpiOsPrintf ("\t\tSub-optimal configuration\n");
        break;

    default:
        AcpiOsPrintf ("\t\tInvalid performance "
                        "robustness preference\n");
        break;
    }

    return;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsDumpIo
 *
 * PARAMETERS:  Data            - pointer to the resource structure to dump.
 *
 * RETURN:
 *
 * DESCRIPTION: Prints out the various members of the Data structure type.
 *
 ******************************************************************************/

void
AcpiRsDumpIo (
    RESOURCE_DATA       *Data)
{
    IO_RESOURCE         *IoData = (IO_RESOURCE*) Data;


    AcpiOsPrintf ("\tIo Resource\n");

    AcpiOsPrintf ("\t\t%d bit decode\n",
                DECODE_16 == IoData->IoDecode ? 16 : 10);

    AcpiOsPrintf ("\t\tRange minimum base: 0x%08x\n",
                IoData->MinBaseAddress);

    AcpiOsPrintf ("\t\tRange maximum base: 0x%08x\n",
                IoData->MaxBaseAddress);

    AcpiOsPrintf ("\t\tAlignment: 0x%08x\n",
                IoData->Alignment);

    AcpiOsPrintf ("\t\tRange Length: 0x%08x\n",
                IoData->RangeLength);

    return;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsDumpFixedIo
 *
 * PARAMETERS:  Data            - pointer to the resource structure to dump.
 *
 * RETURN:
 *
 * DESCRIPTION: Prints out the various members of the Data structure type.
 *
 ******************************************************************************/

void
AcpiRsDumpFixedIo (
    RESOURCE_DATA           *Data)
{
    FIXED_IO_RESOURCE       *FixedIoData = (FIXED_IO_RESOURCE*) Data;


    AcpiOsPrintf ("\tFixed Io Resource\n");
    AcpiOsPrintf ("\t\tRange base address: 0x%08x",
                FixedIoData->BaseAddress);

    AcpiOsPrintf ("\t\tRange length: 0x%08x",
                FixedIoData->RangeLength);

    return;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsDumpVendorSpecific
 *
 * PARAMETERS:  Data            - pointer to the resource structure to dump.
 *
 * RETURN:
 *
 * DESCRIPTION: Prints out the various members of the Data structure type.
 *
 ******************************************************************************/

void
AcpiRsDumpVendorSpecific (
    RESOURCE_DATA           *Data)
{
    VENDOR_RESOURCE         *VendorData = (VENDOR_RESOURCE*) Data;
    UINT16                  Index = 0;


    AcpiOsPrintf ("\tVendor Specific Resource\n");

    AcpiOsPrintf ("\t\tLength: 0x%08x\n", VendorData->Length);

    for (Index = 0; Index < VendorData->Length; Index++)
    {
        AcpiOsPrintf ("\t\tByte %d: 0x%08x\n",
                    Index, VendorData->Reserved[Index]);
    }

    return;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsDumpMemory24
 *
 * PARAMETERS:  Data            - pointer to the resource structure to dump.
 *
 * RETURN:
 *
 * DESCRIPTION: Prints out the various members of the Data structure type.
 *
 ******************************************************************************/

void
AcpiRsDumpMemory24 (
    RESOURCE_DATA           *Data)
{
    MEMORY24_RESOURCE       *Memory24Data = (MEMORY24_RESOURCE*) Data;


    AcpiOsPrintf ("\t24-Bit Memory Range Resource\n");

    AcpiOsPrintf ("\t\tRead%s\n",
                READ_WRITE_MEMORY ==
                Memory24Data->ReadWriteAttribute ?
                "/Write" : " only");

    AcpiOsPrintf ("\t\tRange minimum base: 0x%08x\n",
                Memory24Data->MinBaseAddress);

    AcpiOsPrintf ("\t\tRange maximum base: 0x%08x\n",
                Memory24Data->MaxBaseAddress);

    AcpiOsPrintf ("\t\tAlignment: 0x%08x\n",
                Memory24Data->Alignment);

    AcpiOsPrintf ("\t\tRange length: 0x%08x\n",
                Memory24Data->RangeLength);

    return;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsDumpMemory32
 *
 * PARAMETERS:  Data            - pointer to the resource structure to dump.
 *
 * RETURN:
 *
 * DESCRIPTION: Prints out the various members of the Data structure type.
 *
 ******************************************************************************/

void
AcpiRsDumpMemory32 (
    RESOURCE_DATA           *Data)
{
    MEMORY32_RESOURCE       *Memory32Data = (MEMORY32_RESOURCE*) Data;


    AcpiOsPrintf ("\t32-Bit Memory Range Resource\n");

    AcpiOsPrintf ("\t\tRead%s\n",
                READ_WRITE_MEMORY ==
                Memory32Data->ReadWriteAttribute ?
                "/Write" : " only");

    AcpiOsPrintf ("\t\tRange minimum base: 0x%08x\n",
                Memory32Data->MinBaseAddress);

    AcpiOsPrintf ("\t\tRange maximum base: 0x%08x\n",
                Memory32Data->MaxBaseAddress);

    AcpiOsPrintf ("\t\tAlignment: 0x%08x\n",
                Memory32Data->Alignment);

    AcpiOsPrintf ("\t\tRange length: 0x%08x\n",
                Memory32Data->RangeLength);

    return;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsDumpFixedMemory32
 *
 * PARAMETERS:  Data            - pointer to the resource structure to dump.
 *
 * RETURN:
 *
 * DESCRIPTION: Prints out the various members of the Data structure type.
 *
 ******************************************************************************/

void
AcpiRsDumpFixedMemory32 (
    RESOURCE_DATA           *Data)
{
    FIXED_MEMORY32_RESOURCE *FixedMemory32Data = (FIXED_MEMORY32_RESOURCE*) Data;


    AcpiOsPrintf ("\t32-Bit Fixed Location Memory Range Resource\n");

    AcpiOsPrintf ("\t\tRead%s\n",
                READ_WRITE_MEMORY ==
                FixedMemory32Data->ReadWriteAttribute ?
                "/Write" : " Only");

    AcpiOsPrintf ("\t\tRange base address: 0x%08x\n",
                FixedMemory32Data->RangeBaseAddress);

    AcpiOsPrintf ("\t\tRange length: 0x%08x\n",
                FixedMemory32Data->RangeLength);

    return;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsDumpAddress16
 *
 * PARAMETERS:  Data            - pointer to the resource structure to dump.
 *
 * RETURN:
 *
 * DESCRIPTION: Prints out the various members of the Data structure type.
 *
 ******************************************************************************/

void
AcpiRsDumpAddress16 (
    RESOURCE_DATA           *Data)
{
    ADDRESS16_RESOURCE      *Address16Data = (ADDRESS16_RESOURCE*) Data;


    AcpiOsPrintf ("\t16-Bit Address Space Resource\n");
    AcpiOsPrintf ("\t\tResource Type: ");

    switch (Address16Data->ResourceType)
    {
    case MEMORY_RANGE:

        AcpiOsPrintf ("Memory Range\n");

        switch (Address16Data->Attribute.Memory.CacheAttribute)
        {
        case NON_CACHEABLE_MEMORY:
            AcpiOsPrintf ("\t\tType Specific: "
                            "Noncacheable memory\n");
            break;

        case CACHABLE_MEMORY:
            AcpiOsPrintf ("\t\tType Specific: "
                            "Cacheable memory\n");
            break;

        case WRITE_COMBINING_MEMORY:
            AcpiOsPrintf ("\t\tType Specific: "
                            "Write-combining memory\n");
            break;

        case PREFETCHABLE_MEMORY:
            AcpiOsPrintf ("\t\tType Specific: "
                            "Prefetchable memory\n");
            break;

        default:
            AcpiOsPrintf ("\t\tType Specific: "
                            "Invalid cache attribute\n");
            break;
        }

        AcpiOsPrintf ("\t\tType Specific: Read%s\n",
            READ_WRITE_MEMORY ==
            Address16Data->Attribute.Memory.ReadWriteAttribute ?
            "/Write" : " Only");
        break;

    case IO_RANGE:

        AcpiOsPrintf ("I/O Range\n");

        switch (Address16Data->Attribute.Io.RangeAttribute)
        {
        case NON_ISA_ONLY_RANGES:
            AcpiOsPrintf ("\t\tType Specific: "
                            "Non-ISA Io Addresses\n");
            break;

        case ISA_ONLY_RANGES:
            AcpiOsPrintf ("\t\tType Specific: "
                            "ISA Io Addresses\n");
            break;

        case ENTIRE_RANGE:
            AcpiOsPrintf ("\t\tType Specific: "
                            "ISA and non-ISA Io Addresses\n");
            break;

        default:
            AcpiOsPrintf ("\t\tType Specific: "
                            "Invalid range attribute\n");
            break;
        }
        break;

    case BUS_NUMBER_RANGE:

        AcpiOsPrintf ("Bus Number Range\n");
        break;

    default:

        AcpiOsPrintf ("Invalid resource type. Exiting.\n");
        return;
    }

    AcpiOsPrintf ("\t\tResource %s\n",
            CONSUMER == Address16Data->ProducerConsumer ?
            "Consumer" : "Producer");

    AcpiOsPrintf ("\t\t%s decode\n",
                SUB_DECODE == Address16Data->Decode ?
                "Subtractive" : "Positive");

    AcpiOsPrintf ("\t\tMin address is %s fixed\n",
                ADDRESS_FIXED == Address16Data->MinAddressFixed ?
                "" : "not");

    AcpiOsPrintf ("\t\tMax address is %s fixed\n",
                ADDRESS_FIXED == Address16Data->MaxAddressFixed ?
                "" : "not");

    AcpiOsPrintf ("\t\tGranularity: 0x%08x\n",
                Address16Data->Granularity);

    AcpiOsPrintf ("\t\tAddress range min: 0x%08x\n",
                Address16Data->MinAddressRange);

    AcpiOsPrintf ("\t\tAddress range max: 0x%08x\n",
                Address16Data->MaxAddressRange);

    AcpiOsPrintf ("\t\tAddress translation offset: 0x%08x\n",
                Address16Data->AddressTranslationOffset);

    AcpiOsPrintf ("\t\tAddress Length: 0x%08x\n",
                Address16Data->AddressLength);

    if (0xFF != Address16Data->ResourceSourceIndex)
    {
        AcpiOsPrintf ("\t\tResource Source Index: %d\n",
                    Address16Data->ResourceSourceIndex);
        AcpiOsPrintf ("\t\tResource Source: %s\n",
                    Address16Data->ResourceSource);
    }

    return;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsDumpAddress32
 *
 * PARAMETERS:  Data            - pointer to the resource structure to dump.
 *
 * RETURN:
 *
 * DESCRIPTION: Prints out the various members of the Data structure type.
 *
 ******************************************************************************/

void
AcpiRsDumpAddress32 (
    RESOURCE_DATA           *Data)
{
    ADDRESS32_RESOURCE      *Address32Data = (ADDRESS32_RESOURCE*) Data;


    AcpiOsPrintf ("\t32-Bit Address Space Resource\n");

    switch (Address32Data->ResourceType)
    {
    case MEMORY_RANGE:

        AcpiOsPrintf ("\t\tResource Type: Memory Range\n");

        switch (Address32Data->Attribute.Memory.CacheAttribute)
        {
        case NON_CACHEABLE_MEMORY:
            AcpiOsPrintf ("\t\tType Specific: "
                            "Noncacheable memory\n");
            break;

        case CACHABLE_MEMORY:
            AcpiOsPrintf ("\t\tType Specific: "
                            "Cacheable memory\n");
            break;

        case WRITE_COMBINING_MEMORY:
            AcpiOsPrintf ("\t\tType Specific: "
                            "Write-combining memory\n");
            break;

        case PREFETCHABLE_MEMORY:
            AcpiOsPrintf ("\t\tType Specific: "
                            "Prefetchable memory\n");
            break;

        default:
            AcpiOsPrintf ("\t\tType Specific: "
                            "Invalid cache attribute\n");
            break;
        }

        AcpiOsPrintf ("\t\tType Specific: Read%s\n",
            READ_WRITE_MEMORY ==
            Address32Data->Attribute.Memory.ReadWriteAttribute ?
            "/Write" : " Only");
        break;

    case IO_RANGE:

        AcpiOsPrintf ("\t\tResource Type: Io Range\n");

        switch (Address32Data->Attribute.Io.RangeAttribute)
            {
            case NON_ISA_ONLY_RANGES:
                AcpiOsPrintf ("\t\tType Specific: "
                                "Non-ISA Io Addresses\n");
                break;

            case ISA_ONLY_RANGES:
                AcpiOsPrintf ("\t\tType Specific: "
                                "ISA Io Addresses\n");
                break;

            case ENTIRE_RANGE:
                AcpiOsPrintf ("\t\tType Specific: "
                                "ISA and non-ISA Io Addresses\n");
                break;

            default:
                AcpiOsPrintf ("\t\tType Specific: "
                                "Invalid Range attribute");
                break;
            }
        break;

    case BUS_NUMBER_RANGE:

        AcpiOsPrintf ("\t\tResource Type: Bus Number Range\n");
        break;

    default:

        AcpiOsPrintf ("\t\tInvalid Resource Type..exiting.\n");
        return;
    }

    AcpiOsPrintf ("\t\tResource %s\n",
                CONSUMER == Address32Data->ProducerConsumer ?
                "Consumer" : "Producer");

    AcpiOsPrintf ("\t\t%s decode\n",
                SUB_DECODE == Address32Data->Decode ?
                "Subtractive" : "Positive");

    AcpiOsPrintf ("\t\tMin address is %s fixed\n",
                ADDRESS_FIXED == Address32Data->MinAddressFixed ?
                "" : "not ");

    AcpiOsPrintf ("\t\tMax address is %s fixed\n",
                ADDRESS_FIXED == Address32Data->MaxAddressFixed ?
                "" : "not ");

    AcpiOsPrintf ("\t\tGranularity: 0x%08x\n",
                Address32Data->Granularity);

    AcpiOsPrintf ("\t\tAddress range min: 0x%08x\n",
                Address32Data->MinAddressRange);

    AcpiOsPrintf ("\t\tAddress range max: 0x%08x\n",
                Address32Data->MaxAddressRange);

    AcpiOsPrintf ("\t\tAddress translation offset: 0x%08x\n",
                Address32Data->AddressTranslationOffset);

    AcpiOsPrintf ("\t\tAddress Length: 0x%08x\n",
                Address32Data->AddressLength);

    if(0xFF != Address32Data->ResourceSourceIndex)
    {
        AcpiOsPrintf ("\t\tResource Source Index: %d\n",
                    Address32Data->ResourceSourceIndex);
        AcpiOsPrintf ("\t\tResource Source: %s\n",
                    Address32Data->ResourceSource);
    }

    return;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsDumpExtendedIrq
 *
 * PARAMETERS:  Data            - pointer to the resource structure to dump.
 *
 * RETURN:
 *
 * DESCRIPTION: Prints out the various members of the Data structure type.
 *
 ******************************************************************************/

void
AcpiRsDumpExtendedIrq (
    RESOURCE_DATA           *Data)
{
    EXTENDED_IRQ_RESOURCE   *ExtIrqData = (EXTENDED_IRQ_RESOURCE*) Data;
    UINT8                   Index = 0;


    AcpiOsPrintf ("\tExtended IRQ Resource\n");

    AcpiOsPrintf ("\t\tResource %s\n",
                CONSUMER == ExtIrqData->ProducerConsumer ?
                "Consumer" : "Producer");

    AcpiOsPrintf ("\t\t%s\n",
                LEVEL_SENSITIVE == ExtIrqData->EdgeLevel ?
                "Level" : "Edge");

    AcpiOsPrintf ("\t\tActive %s\n",
                ACTIVE_LOW == ExtIrqData->ActiveHighLow ?
                "low" : "high");

    AcpiOsPrintf ("\t\t%s\n",
                SHARED == ExtIrqData->SharedExclusive ?
                "Shared" : "Exclusive");

    AcpiOsPrintf ("\t\tInterrupts : %d ( ",
                ExtIrqData->NumberOfInterrupts);

    for (Index = 0; Index < ExtIrqData->NumberOfInterrupts; Index++)
    {
        AcpiOsPrintf ("%d ", ExtIrqData->Interrupts[Index]);
    }

    AcpiOsPrintf (")\n");

    if(0xFF != ExtIrqData->ResourceSourceIndex)
    {
        AcpiOsPrintf ("\t\tResource Source Index: %d",
                    ExtIrqData->ResourceSourceIndex);
        AcpiOsPrintf ("\t\tResource Source: %s",
                    ExtIrqData->ResourceSource);
    }

    return;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsDumpResourceList
 *
 * PARAMETERS:  Data            - pointer to the resource structure to dump.
 *
 * RETURN:
 *
 * DESCRIPTION: Dispatches the structure to the correct dump routine.
 *
 ******************************************************************************/

void
AcpiRsDumpResourceList (
    RESOURCE            *Resource)
{
    UINT8               Count = 0;
    BOOLEAN             Done = FALSE;


    if (AcpiDbgLevel & TRACE_RESOURCES && _COMPONENT & AcpiDbgLayer)
    {
        while (!Done)
        {
            AcpiOsPrintf ("\tResource structure %x.\n", Count++);

            switch (Resource->Id)
            {
            case Irq:
                AcpiRsDumpIrq (&Resource->Data);
                break;

            case Dma:
                AcpiRsDumpDma (&Resource->Data);
                break;

            case StartDependentFunctions:
                AcpiRsDumpStartDependentFunctions (&Resource->Data);
                break;

            case EndDependentFunctions:
                AcpiOsPrintf ("\tEndDependentFunctions Resource\n");
                /* AcpiRsDumpEndDependentFunctions (Resource->Data);*/
                break;

            case Io:
                AcpiRsDumpIo (&Resource->Data);
                break;

            case FixedIo:
                AcpiRsDumpFixedIo (&Resource->Data);
                break;

            case VendorSpecific:
                AcpiRsDumpVendorSpecific (&Resource->Data);
                break;

            case EndTag:
                /*RsDumpEndTag (Resource->Data);*/
                AcpiOsPrintf ("\tEndTag Resource\n");
                Done = TRUE;
                break;

            case Memory24:
                AcpiRsDumpMemory24 (&Resource->Data);
                break;

            case Memory32:
                AcpiRsDumpMemory32 (&Resource->Data);
                break;

            case FixedMemory32:
                AcpiRsDumpFixedMemory32 (&Resource->Data);
                break;

            case Address16:
                AcpiRsDumpAddress16 (&Resource->Data);
                break;

            case Address32:
                AcpiRsDumpAddress32 (&Resource->Data);
                break;

            case ExtendedIrq:
                AcpiRsDumpExtendedIrq (&Resource->Data);
                break;

            default:
                AcpiOsPrintf ("Invalid resource type\n");
                break;

            }

            Resource = (RESOURCE *) ((NATIVE_UINT) Resource +
                        (NATIVE_UINT) Resource->Length);
        }
    }

    return;
}

/*******************************************************************************
 *
 * FUNCTION:    AcpiRsDumpIrqList
 *
 * PARAMETERS:  Data            - pointer to the routing table to dump.
 *
 * RETURN:
 *
 * DESCRIPTION: Dispatches the structures to the correct dump routine.
 *
 ******************************************************************************/

void
AcpiRsDumpIrqList (
    UINT8               *RouteTable)
{
    UINT8               *Buffer = RouteTable;
    UINT8               Count = 0;
    BOOLEAN             Done = FALSE;
    PCI_ROUTING_TABLE   *PrtElement;


    if (AcpiDbgLevel & TRACE_RESOURCES && _COMPONENT & AcpiDbgLayer)
    {
        PrtElement = (PCI_ROUTING_TABLE *)Buffer;

        while (!Done)
        {
            AcpiOsPrintf ("\tPCI IRQ Routing Table structure %x.\n", Count++);

            AcpiOsPrintf ("\t\tAddress: 0x%x\n",
                        PrtElement->Data.Address);

            AcpiOsPrintf ("\t\tPin: 0x%x\n", PrtElement->Data.Pin);

            AcpiOsPrintf ("\t\tSource: %s\n", PrtElement->Data.Source);

            AcpiOsPrintf ("\t\tSourceIndex: 0x%x\n",
                        PrtElement->Data.SourceIndex);

            Buffer += PrtElement->Length;

            PrtElement = (PCI_ROUTING_TABLE *)Buffer;

            if(0 == PrtElement->Length)
            {
                Done = TRUE;
            }
        }
    }

    return;
}

