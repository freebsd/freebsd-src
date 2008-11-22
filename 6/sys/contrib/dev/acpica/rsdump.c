/*******************************************************************************
 *
 * Module Name: rsdump - Functions to display the resource structures.
 *              $Revision: 38 $
 *
 ******************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2004, Intel Corp.
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


#define __RSDUMP_C__

#include <contrib/dev/acpica/acpi.h>
#include <contrib/dev/acpica/acresrc.h>

#define _COMPONENT          ACPI_RESOURCES
        ACPI_MODULE_NAME    ("rsdump")


#if defined(ACPI_DEBUG_OUTPUT) || defined(ACPI_DEBUGGER)

/*******************************************************************************
 *
 * FUNCTION:    AcpiRsDumpIrq
 *
 * PARAMETERS:  Data            - pointer to the resource structure to dump.
 *
 * RETURN:      None
 *
 * DESCRIPTION: Prints out the various members of the Data structure type.
 *
 ******************************************************************************/

void
AcpiRsDumpIrq (
    ACPI_RESOURCE_DATA      *Data)
{
    ACPI_RESOURCE_IRQ       *IrqData = (ACPI_RESOURCE_IRQ *) Data;
    UINT8                   Index = 0;


    ACPI_FUNCTION_ENTRY ();


    AcpiOsPrintf ("IRQ Resource\n");

    AcpiOsPrintf ("    %s Triggered\n",
                ACPI_LEVEL_SENSITIVE == IrqData->EdgeLevel ? "Level" : "Edge");

    AcpiOsPrintf ("    Active %s\n",
                ACPI_ACTIVE_LOW == IrqData->ActiveHighLow ? "Low" : "High");

    AcpiOsPrintf ("    %s\n",
                ACPI_SHARED == IrqData->SharedExclusive ? "Shared" : "Exclusive");

    AcpiOsPrintf ("    %X Interrupts ( ", IrqData->NumberOfInterrupts);

    for (Index = 0; Index < IrqData->NumberOfInterrupts; Index++)
    {
        AcpiOsPrintf ("%X ", IrqData->Interrupts[Index]);
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
 * RETURN:      None
 *
 * DESCRIPTION: Prints out the various members of the Data structure type.
 *
 ******************************************************************************/

void
AcpiRsDumpDma (
    ACPI_RESOURCE_DATA      *Data)
{
    ACPI_RESOURCE_DMA       *DmaData = (ACPI_RESOURCE_DMA *) Data;
    UINT8                   Index = 0;


    ACPI_FUNCTION_ENTRY ();


    AcpiOsPrintf ("DMA Resource\n");

    switch (DmaData->Type)
    {
    case ACPI_COMPATIBILITY:
        AcpiOsPrintf ("    Compatibility mode\n");
        break;

    case ACPI_TYPE_A:
        AcpiOsPrintf ("    Type A\n");
        break;

    case ACPI_TYPE_B:
        AcpiOsPrintf ("    Type B\n");
        break;

    case ACPI_TYPE_F:
        AcpiOsPrintf ("    Type F\n");
        break;

    default:
        AcpiOsPrintf ("    Invalid DMA type\n");
        break;
    }

    AcpiOsPrintf ("    %sBus Master\n",
                ACPI_BUS_MASTER == DmaData->BusMaster ? "" : "Not a ");


    switch (DmaData->Transfer)
    {
    case ACPI_TRANSFER_8:
        AcpiOsPrintf ("    8-bit only transfer\n");
        break;

    case ACPI_TRANSFER_8_16:
        AcpiOsPrintf ("    8 and 16-bit transfer\n");
        break;

    case ACPI_TRANSFER_16:
        AcpiOsPrintf ("    16 bit only transfer\n");
        break;

    default:
        AcpiOsPrintf ("    Invalid transfer preference\n");
        break;
    }

    AcpiOsPrintf ("    Number of Channels: %X ( ", DmaData->NumberOfChannels);

    for (Index = 0; Index < DmaData->NumberOfChannels; Index++)
    {
        AcpiOsPrintf ("%X ", DmaData->Channels[Index]);
    }

    AcpiOsPrintf (")\n");
    return;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsDumpStartDependFns
 *
 * PARAMETERS:  Data            - pointer to the resource structure to dump.
 *
 * RETURN:      None
 *
 * DESCRIPTION: Prints out the various members of the Data structure type.
 *
 ******************************************************************************/

void
AcpiRsDumpStartDependFns (
    ACPI_RESOURCE_DATA      *Data)
{
    ACPI_RESOURCE_START_DPF *SdfData = (ACPI_RESOURCE_START_DPF *) Data;


    ACPI_FUNCTION_ENTRY ();


    AcpiOsPrintf ("Start Dependent Functions Resource\n");

    switch (SdfData->CompatibilityPriority)
    {
    case ACPI_GOOD_CONFIGURATION:
        AcpiOsPrintf ("    Good configuration\n");
        break;

    case ACPI_ACCEPTABLE_CONFIGURATION:
        AcpiOsPrintf ("    Acceptable configuration\n");
        break;

    case ACPI_SUB_OPTIMAL_CONFIGURATION:
        AcpiOsPrintf ("    Sub-optimal configuration\n");
        break;

    default:
        AcpiOsPrintf ("    Invalid compatibility priority\n");
        break;
    }

    switch(SdfData->PerformanceRobustness)
    {
    case ACPI_GOOD_CONFIGURATION:
        AcpiOsPrintf ("    Good configuration\n");
        break;

    case ACPI_ACCEPTABLE_CONFIGURATION:
        AcpiOsPrintf ("    Acceptable configuration\n");
        break;

    case ACPI_SUB_OPTIMAL_CONFIGURATION:
        AcpiOsPrintf ("    Sub-optimal configuration\n");
        break;

    default:
        AcpiOsPrintf ("    Invalid performance "
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
 * RETURN:      None
 *
 * DESCRIPTION: Prints out the various members of the Data structure type.
 *
 ******************************************************************************/

void
AcpiRsDumpIo (
    ACPI_RESOURCE_DATA      *Data)
{
    ACPI_RESOURCE_IO        *IoData = (ACPI_RESOURCE_IO *) Data;


    ACPI_FUNCTION_ENTRY ();


    AcpiOsPrintf ("Io Resource\n");

    AcpiOsPrintf ("    %d bit decode\n",
                ACPI_DECODE_16 == IoData->IoDecode ? 16 : 10);

    AcpiOsPrintf ("    Range minimum base: %08X\n",
                IoData->MinBaseAddress);

    AcpiOsPrintf ("    Range maximum base: %08X\n",
                IoData->MaxBaseAddress);

    AcpiOsPrintf ("    Alignment: %08X\n",
                IoData->Alignment);

    AcpiOsPrintf ("    Range Length: %08X\n",
                IoData->RangeLength);

    return;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsDumpFixedIo
 *
 * PARAMETERS:  Data            - pointer to the resource structure to dump.
 *
 * RETURN:      None
 *
 * DESCRIPTION: Prints out the various members of the Data structure type.
 *
 ******************************************************************************/

void
AcpiRsDumpFixedIo (
    ACPI_RESOURCE_DATA      *Data)
{
    ACPI_RESOURCE_FIXED_IO  *FixedIoData = (ACPI_RESOURCE_FIXED_IO *) Data;


    ACPI_FUNCTION_ENTRY ();


    AcpiOsPrintf ("Fixed Io Resource\n");
    AcpiOsPrintf ("    Range base address: %08X",
                FixedIoData->BaseAddress);

    AcpiOsPrintf ("    Range length: %08X",
                FixedIoData->RangeLength);

    return;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsDumpVendorSpecific
 *
 * PARAMETERS:  Data            - pointer to the resource structure to dump.
 *
 * RETURN:      None
 *
 * DESCRIPTION: Prints out the various members of the Data structure type.
 *
 ******************************************************************************/

void
AcpiRsDumpVendorSpecific (
    ACPI_RESOURCE_DATA      *Data)
{
    ACPI_RESOURCE_VENDOR    *VendorData = (ACPI_RESOURCE_VENDOR *) Data;
    UINT16                  Index = 0;


    ACPI_FUNCTION_ENTRY ();


    AcpiOsPrintf ("Vendor Specific Resource\n");

    AcpiOsPrintf ("    Length: %08X\n", VendorData->Length);

    for (Index = 0; Index < VendorData->Length; Index++)
    {
        AcpiOsPrintf ("    Byte %X: %08X\n",
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
 * RETURN:      None
 *
 * DESCRIPTION: Prints out the various members of the Data structure type.
 *
 ******************************************************************************/

void
AcpiRsDumpMemory24 (
    ACPI_RESOURCE_DATA      *Data)
{
    ACPI_RESOURCE_MEM24     *Memory24Data = (ACPI_RESOURCE_MEM24 *) Data;


    ACPI_FUNCTION_ENTRY ();


    AcpiOsPrintf ("24-Bit Memory Range Resource\n");

    AcpiOsPrintf ("    Read%s\n",
                ACPI_READ_WRITE_MEMORY ==
                Memory24Data->ReadWriteAttribute ?
                "/Write" : " only");

    AcpiOsPrintf ("    Range minimum base: %08X\n",
                Memory24Data->MinBaseAddress);

    AcpiOsPrintf ("    Range maximum base: %08X\n",
                Memory24Data->MaxBaseAddress);

    AcpiOsPrintf ("    Alignment: %08X\n",
                Memory24Data->Alignment);

    AcpiOsPrintf ("    Range length: %08X\n",
                Memory24Data->RangeLength);

    return;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsDumpMemory32
 *
 * PARAMETERS:  Data            - pointer to the resource structure to dump.
 *
 * RETURN:      None
 *
 * DESCRIPTION: Prints out the various members of the Data structure type.
 *
 ******************************************************************************/

void
AcpiRsDumpMemory32 (
    ACPI_RESOURCE_DATA      *Data)
{
    ACPI_RESOURCE_MEM32     *Memory32Data = (ACPI_RESOURCE_MEM32 *) Data;


    ACPI_FUNCTION_ENTRY ();


    AcpiOsPrintf ("32-Bit Memory Range Resource\n");

    AcpiOsPrintf ("    Read%s\n",
                ACPI_READ_WRITE_MEMORY ==
                Memory32Data->ReadWriteAttribute ?
                "/Write" : " only");

    AcpiOsPrintf ("    Range minimum base: %08X\n",
                Memory32Data->MinBaseAddress);

    AcpiOsPrintf ("    Range maximum base: %08X\n",
                Memory32Data->MaxBaseAddress);

    AcpiOsPrintf ("    Alignment: %08X\n",
                Memory32Data->Alignment);

    AcpiOsPrintf ("    Range length: %08X\n",
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
    ACPI_RESOURCE_DATA          *Data)
{
    ACPI_RESOURCE_FIXED_MEM32   *FixedMemory32Data = (ACPI_RESOURCE_FIXED_MEM32 *) Data;


    ACPI_FUNCTION_ENTRY ();


    AcpiOsPrintf ("32-Bit Fixed Location Memory Range Resource\n");

    AcpiOsPrintf ("    Read%s\n",
                ACPI_READ_WRITE_MEMORY ==
                FixedMemory32Data->ReadWriteAttribute ?
                "/Write" : " Only");

    AcpiOsPrintf ("    Range base address: %08X\n",
                FixedMemory32Data->RangeBaseAddress);

    AcpiOsPrintf ("    Range length: %08X\n",
                FixedMemory32Data->RangeLength);

    return;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsDumpAddress16
 *
 * PARAMETERS:  Data            - pointer to the resource structure to dump.
 *
 * RETURN:      None
 *
 * DESCRIPTION: Prints out the various members of the Data structure type.
 *
 ******************************************************************************/

void
AcpiRsDumpAddress16 (
    ACPI_RESOURCE_DATA      *Data)
{
    ACPI_RESOURCE_ADDRESS16 *Address16Data = (ACPI_RESOURCE_ADDRESS16 *) Data;


    ACPI_FUNCTION_ENTRY ();


    AcpiOsPrintf ("16-Bit Address Space Resource\n");
    AcpiOsPrintf ("    Resource Type: ");

    switch (Address16Data->ResourceType)
    {
    case ACPI_MEMORY_RANGE:

        AcpiOsPrintf ("Memory Range\n");

        switch (Address16Data->Attribute.Memory.CacheAttribute)
        {
        case ACPI_NON_CACHEABLE_MEMORY:
            AcpiOsPrintf ("    Type Specific: "
                            "Noncacheable memory\n");
            break;

        case ACPI_CACHABLE_MEMORY:
            AcpiOsPrintf ("    Type Specific: "
                            "Cacheable memory\n");
            break;

        case ACPI_WRITE_COMBINING_MEMORY:
            AcpiOsPrintf ("    Type Specific: "
                            "Write-combining memory\n");
            break;

        case ACPI_PREFETCHABLE_MEMORY:
            AcpiOsPrintf ("    Type Specific: "
                            "Prefetchable memory\n");
            break;

        default:
            AcpiOsPrintf ("    Type Specific: "
                            "Invalid cache attribute\n");
            break;
        }

        AcpiOsPrintf ("    Type Specific: Read%s\n",
            ACPI_READ_WRITE_MEMORY ==
            Address16Data->Attribute.Memory.ReadWriteAttribute ?
            "/Write" : " Only");
        break;

    case ACPI_IO_RANGE:

        AcpiOsPrintf ("I/O Range\n");

        switch (Address16Data->Attribute.Io.RangeAttribute)
        {
        case ACPI_NON_ISA_ONLY_RANGES:
            AcpiOsPrintf ("    Type Specific: "
                            "Non-ISA Io Addresses\n");
            break;

        case ACPI_ISA_ONLY_RANGES:
            AcpiOsPrintf ("    Type Specific: "
                            "ISA Io Addresses\n");
            break;

        case ACPI_ENTIRE_RANGE:
            AcpiOsPrintf ("    Type Specific: "
                            "ISA and non-ISA Io Addresses\n");
            break;

        default:
            AcpiOsPrintf ("    Type Specific: "
                            "Invalid range attribute\n");
            break;
        }

        AcpiOsPrintf ("  Type Specific: %s Translation\n",
            ACPI_SPARSE_TRANSLATION ==
            Address16Data->Attribute.Io.TranslationAttribute ?
            "Sparse" : "Dense");
        break;

    case ACPI_BUS_NUMBER_RANGE:

        AcpiOsPrintf ("Bus Number Range\n");
        break;

    default:

        AcpiOsPrintf ("Invalid resource type. Exiting.\n");
        return;
    }

    AcpiOsPrintf ("    Resource %s\n",
            ACPI_CONSUMER == Address16Data->ProducerConsumer ?
            "Consumer" : "Producer");

    AcpiOsPrintf ("    %s decode\n",
                ACPI_SUB_DECODE == Address16Data->Decode ?
                "Subtractive" : "Positive");

    AcpiOsPrintf ("    Min address is %s fixed\n",
                ACPI_ADDRESS_FIXED == Address16Data->MinAddressFixed ?
                "" : "not");

    AcpiOsPrintf ("    Max address is %s fixed\n",
                ACPI_ADDRESS_FIXED == Address16Data->MaxAddressFixed ?
                "" : "not");

    AcpiOsPrintf ("    Granularity: %08X\n",
                Address16Data->Granularity);

    AcpiOsPrintf ("    Address range min: %08X\n",
                Address16Data->MinAddressRange);

    AcpiOsPrintf ("    Address range max: %08X\n",
                Address16Data->MaxAddressRange);

    AcpiOsPrintf ("    Address translation offset: %08X\n",
                Address16Data->AddressTranslationOffset);

    AcpiOsPrintf ("    Address Length: %08X\n",
                Address16Data->AddressLength);

    if (0xFF != Address16Data->ResourceSource.Index)
    {
        AcpiOsPrintf ("    Resource Source Index: %X\n",
                    Address16Data->ResourceSource.Index);
        AcpiOsPrintf ("    Resource Source: %s\n",
                    Address16Data->ResourceSource.StringPtr);
    }

    return;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsDumpAddress32
 *
 * PARAMETERS:  Data            - pointer to the resource structure to dump.
 *
 * RETURN:      None
 *
 * DESCRIPTION: Prints out the various members of the Data structure type.
 *
 ******************************************************************************/

void
AcpiRsDumpAddress32 (
    ACPI_RESOURCE_DATA      *Data)
{
    ACPI_RESOURCE_ADDRESS32 *Address32Data = (ACPI_RESOURCE_ADDRESS32 *) Data;


    ACPI_FUNCTION_ENTRY ();


    AcpiOsPrintf ("32-Bit Address Space Resource\n");

    switch (Address32Data->ResourceType)
    {
    case ACPI_MEMORY_RANGE:

        AcpiOsPrintf ("    Resource Type: Memory Range\n");

        switch (Address32Data->Attribute.Memory.CacheAttribute)
        {
        case ACPI_NON_CACHEABLE_MEMORY:
            AcpiOsPrintf ("    Type Specific: "
                            "Noncacheable memory\n");
            break;

        case ACPI_CACHABLE_MEMORY:
            AcpiOsPrintf ("    Type Specific: "
                            "Cacheable memory\n");
            break;

        case ACPI_WRITE_COMBINING_MEMORY:
            AcpiOsPrintf ("    Type Specific: "
                            "Write-combining memory\n");
            break;

        case ACPI_PREFETCHABLE_MEMORY:
            AcpiOsPrintf ("    Type Specific: "
                            "Prefetchable memory\n");
            break;

        default:
            AcpiOsPrintf ("    Type Specific: "
                            "Invalid cache attribute\n");
            break;
        }

        AcpiOsPrintf ("    Type Specific: Read%s\n",
            ACPI_READ_WRITE_MEMORY ==
            Address32Data->Attribute.Memory.ReadWriteAttribute ?
            "/Write" : " Only");
        break;

    case ACPI_IO_RANGE:

        AcpiOsPrintf ("    Resource Type: Io Range\n");

        switch (Address32Data->Attribute.Io.RangeAttribute)
        {
        case ACPI_NON_ISA_ONLY_RANGES:
            AcpiOsPrintf ("    Type Specific: "
                            "Non-ISA Io Addresses\n");
            break;

        case ACPI_ISA_ONLY_RANGES:
            AcpiOsPrintf ("    Type Specific: "
                            "ISA Io Addresses\n");
            break;

        case ACPI_ENTIRE_RANGE:
            AcpiOsPrintf ("    Type Specific: "
                            "ISA and non-ISA Io Addresses\n");
            break;

        default:
            AcpiOsPrintf ("    Type Specific: "
                            "Invalid Range attribute");
            break;
        }

        AcpiOsPrintf ("  Type Specific: %s Translation\n",
            ACPI_SPARSE_TRANSLATION ==
            Address32Data->Attribute.Io.TranslationAttribute ?
            "Sparse" : "Dense");
        break;

    case ACPI_BUS_NUMBER_RANGE:

        AcpiOsPrintf ("    Resource Type: Bus Number Range\n");
        break;

    default:

        AcpiOsPrintf ("    Invalid Resource Type..exiting.\n");
        return;
    }

    AcpiOsPrintf ("    Resource %s\n",
                ACPI_CONSUMER == Address32Data->ProducerConsumer ?
                "Consumer" : "Producer");

    AcpiOsPrintf ("    %s decode\n",
                ACPI_SUB_DECODE == Address32Data->Decode ?
                "Subtractive" : "Positive");

    AcpiOsPrintf ("    Min address is %s fixed\n",
                ACPI_ADDRESS_FIXED == Address32Data->MinAddressFixed ?
                "" : "not ");

    AcpiOsPrintf ("    Max address is %s fixed\n",
                ACPI_ADDRESS_FIXED == Address32Data->MaxAddressFixed ?
                "" : "not ");

    AcpiOsPrintf ("    Granularity: %08X\n",
                Address32Data->Granularity);

    AcpiOsPrintf ("    Address range min: %08X\n",
                Address32Data->MinAddressRange);

    AcpiOsPrintf ("    Address range max: %08X\n",
                Address32Data->MaxAddressRange);

    AcpiOsPrintf ("    Address translation offset: %08X\n",
                Address32Data->AddressTranslationOffset);

    AcpiOsPrintf ("    Address Length: %08X\n",
                Address32Data->AddressLength);

    if(0xFF != Address32Data->ResourceSource.Index)
    {
        AcpiOsPrintf ("    Resource Source Index: %X\n",
                    Address32Data->ResourceSource.Index);
        AcpiOsPrintf ("    Resource Source: %s\n",
                    Address32Data->ResourceSource.StringPtr);
    }

    return;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsDumpAddress64
 *
 * PARAMETERS:  Data            - pointer to the resource structure to dump.
 *
 * RETURN:      None
 *
 * DESCRIPTION: Prints out the various members of the Data structure type.
 *
 ******************************************************************************/

void
AcpiRsDumpAddress64 (
    ACPI_RESOURCE_DATA      *Data)
{
    ACPI_RESOURCE_ADDRESS64 *Address64Data = (ACPI_RESOURCE_ADDRESS64 *) Data;


    ACPI_FUNCTION_ENTRY ();


    AcpiOsPrintf ("64-Bit Address Space Resource\n");

    switch (Address64Data->ResourceType)
    {
    case ACPI_MEMORY_RANGE:

        AcpiOsPrintf ("    Resource Type: Memory Range\n");

        switch (Address64Data->Attribute.Memory.CacheAttribute)
        {
        case ACPI_NON_CACHEABLE_MEMORY:
            AcpiOsPrintf ("    Type Specific: "
                            "Noncacheable memory\n");
            break;

        case ACPI_CACHABLE_MEMORY:
            AcpiOsPrintf ("    Type Specific: "
                            "Cacheable memory\n");
            break;

        case ACPI_WRITE_COMBINING_MEMORY:
            AcpiOsPrintf ("    Type Specific: "
                            "Write-combining memory\n");
            break;

        case ACPI_PREFETCHABLE_MEMORY:
            AcpiOsPrintf ("    Type Specific: "
                            "Prefetchable memory\n");
            break;

        default:
            AcpiOsPrintf ("    Type Specific: "
                            "Invalid cache attribute\n");
            break;
        }

        AcpiOsPrintf ("    Type Specific: Read%s\n",
            ACPI_READ_WRITE_MEMORY ==
            Address64Data->Attribute.Memory.ReadWriteAttribute ?
            "/Write" : " Only");
        break;

    case ACPI_IO_RANGE:

        AcpiOsPrintf ("    Resource Type: Io Range\n");

        switch (Address64Data->Attribute.Io.RangeAttribute)
        {
        case ACPI_NON_ISA_ONLY_RANGES:
            AcpiOsPrintf ("    Type Specific: "
                            "Non-ISA Io Addresses\n");
            break;

        case ACPI_ISA_ONLY_RANGES:
            AcpiOsPrintf ("    Type Specific: "
                            "ISA Io Addresses\n");
            break;

        case ACPI_ENTIRE_RANGE:
            AcpiOsPrintf ("    Type Specific: "
                            "ISA and non-ISA Io Addresses\n");
            break;

        default:
            AcpiOsPrintf ("    Type Specific: "
                            "Invalid Range attribute");
            break;
        }

        AcpiOsPrintf ("  Type Specific: %s Translation\n",
            ACPI_SPARSE_TRANSLATION ==
            Address64Data->Attribute.Io.TranslationAttribute ?
            "Sparse" : "Dense");
        break;

    case ACPI_BUS_NUMBER_RANGE:

        AcpiOsPrintf ("    Resource Type: Bus Number Range\n");
        break;

    default:

        AcpiOsPrintf ("    Invalid Resource Type..exiting.\n");
        return;
    }

    AcpiOsPrintf ("    Resource %s\n",
                ACPI_CONSUMER == Address64Data->ProducerConsumer ?
                "Consumer" : "Producer");

    AcpiOsPrintf ("    %s decode\n",
                ACPI_SUB_DECODE == Address64Data->Decode ?
                "Subtractive" : "Positive");

    AcpiOsPrintf ("    Min address is %s fixed\n",
                ACPI_ADDRESS_FIXED == Address64Data->MinAddressFixed ?
                "" : "not ");

    AcpiOsPrintf ("    Max address is %s fixed\n",
                ACPI_ADDRESS_FIXED == Address64Data->MaxAddressFixed ?
                "" : "not ");

    AcpiOsPrintf ("    Granularity: %8.8X%8.8X\n",
                ACPI_FORMAT_UINT64 (Address64Data->Granularity));

    AcpiOsPrintf ("    Address range min: %8.8X%8.8X\n",
                ACPI_FORMAT_UINT64 (Address64Data->MinAddressRange));

    AcpiOsPrintf ("    Address range max: %8.8X%8.8X\n",
                ACPI_FORMAT_UINT64 (Address64Data->MaxAddressRange));

    AcpiOsPrintf ("    Address translation offset: %8.8X%8.8X\n",
                ACPI_FORMAT_UINT64 (Address64Data->AddressTranslationOffset));

    AcpiOsPrintf ("    Address Length: %8.8X%8.8X\n",
                ACPI_FORMAT_UINT64 (Address64Data->AddressLength));

    if(0xFF != Address64Data->ResourceSource.Index)
    {
        AcpiOsPrintf ("    Resource Source Index: %X\n",
                    Address64Data->ResourceSource.Index);
        AcpiOsPrintf ("    Resource Source: %s\n",
                    Address64Data->ResourceSource.StringPtr);
    }

    return;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsDumpExtendedIrq
 *
 * PARAMETERS:  Data            - pointer to the resource structure to dump.
 *
 * RETURN:      None
 *
 * DESCRIPTION: Prints out the various members of the Data structure type.
 *
 ******************************************************************************/

void
AcpiRsDumpExtendedIrq (
    ACPI_RESOURCE_DATA      *Data)
{
    ACPI_RESOURCE_EXT_IRQ   *ExtIrqData = (ACPI_RESOURCE_EXT_IRQ *) Data;
    UINT8                   Index = 0;


    ACPI_FUNCTION_ENTRY ();


    AcpiOsPrintf ("Extended IRQ Resource\n");

    AcpiOsPrintf ("    Resource %s\n",
                ACPI_CONSUMER == ExtIrqData->ProducerConsumer ?
                "Consumer" : "Producer");

    AcpiOsPrintf ("    %s\n",
                ACPI_LEVEL_SENSITIVE == ExtIrqData->EdgeLevel ?
                "Level" : "Edge");

    AcpiOsPrintf ("    Active %s\n",
                ACPI_ACTIVE_LOW == ExtIrqData->ActiveHighLow ?
                "low" : "high");

    AcpiOsPrintf ("    %s\n",
                ACPI_SHARED == ExtIrqData->SharedExclusive ?
                "Shared" : "Exclusive");

    AcpiOsPrintf ("    Interrupts : %X ( ",
                ExtIrqData->NumberOfInterrupts);

    for (Index = 0; Index < ExtIrqData->NumberOfInterrupts; Index++)
    {
        AcpiOsPrintf ("%X ", ExtIrqData->Interrupts[Index]);
    }

    AcpiOsPrintf (")\n");

    if(0xFF != ExtIrqData->ResourceSource.Index)
    {
        AcpiOsPrintf ("    Resource Source Index: %X",
                    ExtIrqData->ResourceSource.Index);
        AcpiOsPrintf ("    Resource Source: %s",
                    ExtIrqData->ResourceSource.StringPtr);
    }

    return;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsDumpResourceList
 *
 * PARAMETERS:  Data            - pointer to the resource structure to dump.
 *
 * RETURN:      None
 *
 * DESCRIPTION: Dispatches the structure to the correct dump routine.
 *
 ******************************************************************************/

void
AcpiRsDumpResourceList (
    ACPI_RESOURCE       *Resource)
{
    UINT8               Count = 0;
    BOOLEAN             Done = FALSE;


    ACPI_FUNCTION_ENTRY ();


    if (AcpiDbgLevel & ACPI_LV_RESOURCES && _COMPONENT & AcpiDbgLayer)
    {
        while (!Done)
        {
            AcpiOsPrintf ("Resource structure %X.\n", Count++);

            switch (Resource->Id)
            {
            case ACPI_RSTYPE_IRQ:
                AcpiRsDumpIrq (&Resource->Data);
                break;

            case ACPI_RSTYPE_DMA:
                AcpiRsDumpDma (&Resource->Data);
                break;

            case ACPI_RSTYPE_START_DPF:
                AcpiRsDumpStartDependFns (&Resource->Data);
                break;

            case ACPI_RSTYPE_END_DPF:
                AcpiOsPrintf ("EndDependentFunctions Resource\n");
                /* AcpiRsDumpEndDependentFunctions (Resource->Data);*/
                break;

            case ACPI_RSTYPE_IO:
                AcpiRsDumpIo (&Resource->Data);
                break;

            case ACPI_RSTYPE_FIXED_IO:
                AcpiRsDumpFixedIo (&Resource->Data);
                break;

            case ACPI_RSTYPE_VENDOR:
                AcpiRsDumpVendorSpecific (&Resource->Data);
                break;

            case ACPI_RSTYPE_END_TAG:
                /*RsDumpEndTag (Resource->Data);*/
                AcpiOsPrintf ("EndTag Resource\n");
                Done = TRUE;
                break;

            case ACPI_RSTYPE_MEM24:
                AcpiRsDumpMemory24 (&Resource->Data);
                break;

            case ACPI_RSTYPE_MEM32:
                AcpiRsDumpMemory32 (&Resource->Data);
                break;

            case ACPI_RSTYPE_FIXED_MEM32:
                AcpiRsDumpFixedMemory32 (&Resource->Data);
                break;

            case ACPI_RSTYPE_ADDRESS16:
                AcpiRsDumpAddress16 (&Resource->Data);
                break;

            case ACPI_RSTYPE_ADDRESS32:
                AcpiRsDumpAddress32 (&Resource->Data);
                break;

            case ACPI_RSTYPE_ADDRESS64:
                AcpiRsDumpAddress64 (&Resource->Data);
                break;

            case ACPI_RSTYPE_EXT_IRQ:
                AcpiRsDumpExtendedIrq (&Resource->Data);
                break;

            default:
                AcpiOsPrintf ("Invalid resource type\n");
                break;

            }

            Resource = ACPI_PTR_ADD (ACPI_RESOURCE, Resource, Resource->Length);
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
 * RETURN:      None
 *
 * DESCRIPTION: Dispatches the structures to the correct dump routine.
 *
 ******************************************************************************/

void
AcpiRsDumpIrqList (
    UINT8                   *RouteTable)
{
    UINT8                   *Buffer = RouteTable;
    UINT8                   Count = 0;
    BOOLEAN                 Done = FALSE;
    ACPI_PCI_ROUTING_TABLE  *PrtElement;


    ACPI_FUNCTION_ENTRY ();


    if (AcpiDbgLevel & ACPI_LV_RESOURCES && _COMPONENT & AcpiDbgLayer)
    {
        PrtElement = ACPI_CAST_PTR (ACPI_PCI_ROUTING_TABLE, Buffer);

        while (!Done)
        {
            AcpiOsPrintf ("PCI IRQ Routing Table structure %X.\n", Count++);

            AcpiOsPrintf ("    Address: %8.8X%8.8X\n",
                        ACPI_FORMAT_UINT64 (PrtElement->Address));

            AcpiOsPrintf ("    Pin: %X\n", PrtElement->Pin);

            AcpiOsPrintf ("    Source: %s\n", PrtElement->Source);

            AcpiOsPrintf ("    SourceIndex: %X\n",
                        PrtElement->SourceIndex);

            Buffer += PrtElement->Length;

            PrtElement = ACPI_CAST_PTR (ACPI_PCI_ROUTING_TABLE, Buffer);

            if(0 == PrtElement->Length)
            {
                Done = TRUE;
            }
        }
    }

    return;
}

#endif

