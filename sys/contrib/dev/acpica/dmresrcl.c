/*******************************************************************************
 *
 * Module Name: dmresrcl.c - "Large" Resource Descriptor disassembly
 *              $Revision: 12 $
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


#include "acpi.h"
#include "acdisasm.h"


#ifdef ACPI_DISASSEMBLER

#define _COMPONENT          ACPI_CA_DEBUGGER
        ACPI_MODULE_NAME    ("dbresrcl")


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmIoFlags
 *
 * PARAMETERS:  Flags               - Flag byte to be decoded
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode the flags specific to IO Address space descriptors
 *
 ******************************************************************************/

void
AcpiDmIoFlags (
        UINT8               Flags)
{
    AcpiOsPrintf ("%s, %s, %s, %s,",
        AcpiGbl_ConsumeDecode [(Flags & 1)],
        AcpiGbl_MinDecode [(Flags & 0x4) >> 2],
        AcpiGbl_MaxDecode [(Flags & 0x8) >> 3],
        AcpiGbl_DECDecode [(Flags & 0x2) >> 1]);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmMemoryFlags
 *
 * PARAMETERS:  Flags               - Flag byte to be decoded
 *              SpecificFlags       - "Specific" flag byte to be decoded
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode flags specific to Memory Address Space descriptors
 *
 ******************************************************************************/

void
AcpiDmMemoryFlags (
    UINT8                   Flags,
    UINT8                   SpecificFlags)
{
    AcpiOsPrintf ("%s, %s, %s, %s, %s, %s,",
        AcpiGbl_ConsumeDecode [(Flags & 1)],
        AcpiGbl_DECDecode [(Flags & 0x2) >> 1],
        AcpiGbl_MinDecode [(Flags & 0x4) >> 2],
        AcpiGbl_MaxDecode [(Flags & 0x8) >> 3],
        AcpiGbl_MEMDecode [(SpecificFlags & 0x6) >> 1],
        AcpiGbl_RWDecode [(SpecificFlags & 0x1)]);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmWordDescriptor
 *
 * PARAMETERS:  Resource            - Pointer to the resource descriptor
 *              Length              - Length of the descriptor in bytes
 *              Level               - Current source code indentation level
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode a Word Address Space descriptor
 *
 ******************************************************************************/

void
AcpiDmWordDescriptor (
    ASL_WORD_ADDRESS_DESC   *Resource,
    UINT32                  Length,
    UINT32                  Level)
{

    AcpiDmIndent (Level);
    AcpiOsPrintf ("%s (",
        AcpiGbl_WordDecode [(Resource->ResourceType & 3)]);

    AcpiDmIoFlags (Resource->Flags);

    if ((Resource->ResourceType & 0x3) == 1)
    {
        AcpiOsPrintf (" %s,",
            AcpiGbl_RNGDecode [(Resource->SpecificFlags & 0x3)]);
    }

    /* The WORD values */

    AcpiOsPrintf ("\n");
    AcpiDmIndent (Level + 1);
    AcpiOsPrintf ("0x%4.4X,\n",
        (UINT32) Resource->Granularity);
    AcpiDmIndent (Level + 1);
    AcpiOsPrintf ("0x%4.4X,\n",
        (UINT32) Resource->AddressMin);
    AcpiDmIndent (Level + 1);
    AcpiOsPrintf ("0x%4.4X,\n",
        (UINT32) Resource->AddressMax);
    AcpiDmIndent (Level + 1);
    AcpiOsPrintf ("0x%4.4X,\n",
        (UINT32) Resource->TranslationOffset);
    AcpiDmIndent (Level + 1);
    AcpiOsPrintf ("0x%4.4X",
        (UINT32) Resource->AddressLength);

    /* Optional fields */

    if (Length > 13)
    {
        AcpiOsPrintf (", 0x%2.2X",
            (UINT32) Resource->OptionalFields[0]);
    }

    if (Length > 14)
    {
        AcpiOsPrintf (", %s",
            &Resource->OptionalFields[1]);
    }
    AcpiOsPrintf (")\n");
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDwordDescriptor
 *
 * PARAMETERS:  Resource            - Pointer to the resource descriptor
 *              Length              - Length of the descriptor in bytes
 *              Level               - Current source code indentation level
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode a DWord Address Space descriptor
 *
 ******************************************************************************/

void
AcpiDmDwordDescriptor (
    ASL_DWORD_ADDRESS_DESC  *Resource,
    UINT32                  Length,
    UINT32                  Level)
{

    AcpiDmIndent (Level);
    AcpiOsPrintf ("D%s (",
        AcpiGbl_WordDecode [(Resource->ResourceType & 3)]);

    if ((Resource->ResourceType & 0x3) == 1)
    {
        AcpiDmIoFlags (Resource->Flags);
        AcpiOsPrintf (" %s,",
            AcpiGbl_RNGDecode [(Resource->SpecificFlags & 0x3)]);
    }
    else
    {
        AcpiDmMemoryFlags (Resource->Flags, Resource->SpecificFlags);
    }

    /* The DWORD values */

    AcpiOsPrintf ("\n");
    AcpiDmIndent (Level + 1);
    AcpiOsPrintf ("0x%8.8X,\n",
        Resource->Granularity);
    AcpiDmIndent (Level + 1);
    AcpiOsPrintf ("0x%8.8X,\n",
        Resource->AddressMin);
    AcpiDmIndent (Level + 1);
    AcpiOsPrintf ("0x%8.8X,\n",
        Resource->AddressMax);
    AcpiDmIndent (Level + 1);
    AcpiOsPrintf ("0x%8.8X,\n",
        Resource->TranslationOffset);
    AcpiDmIndent (Level + 1);
    AcpiOsPrintf ("0x%8.8X",
        Resource->AddressLength);

    /* Optional fields */

    if (Length > 23)
    {
        AcpiOsPrintf (", 0x%2.2X",
            Resource->OptionalFields[0]);
    }
    if (Length > 24)
    {
        AcpiOsPrintf (", %s",
            &Resource->OptionalFields[1]);
    }
    AcpiOsPrintf (")\n");
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmQwordDescriptor
 *
 * PARAMETERS:  Resource            - Pointer to the resource descriptor
 *              Length              - Length of the descriptor in bytes
 *              Level               - Current source code indentation level
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode a QWord Address Space descriptor
 *
 ******************************************************************************/

void
AcpiDmQwordDescriptor (
    ASL_QWORD_ADDRESS_DESC  *Resource,
    UINT32                  Length,
    UINT32                  Level)
{

    AcpiDmIndent (Level);
    AcpiOsPrintf ("Q%s (",
        AcpiGbl_WordDecode [(Resource->ResourceType & 3)]);

    if ((Resource->ResourceType & 0x3) == 1)
    {
        AcpiDmIoFlags (Resource->Flags);
        AcpiOsPrintf (" %s,",
            AcpiGbl_RNGDecode [(Resource->SpecificFlags & 0x3)]);
    }
    else
    {
        AcpiDmMemoryFlags (Resource->Flags, Resource->SpecificFlags);
    }

    /* The QWORD values */

    AcpiOsPrintf ("\n");
    AcpiDmIndent (Level + 1);
    AcpiOsPrintf ("0x%8.8X%8.8X,\n",
        ACPI_FORMAT_UINT64 (ACPI_GET_ADDRESS (Resource->Granularity)));

    AcpiDmIndent (Level + 1);
    AcpiOsPrintf ("0x%8.8X%8.8X,\n",
        ACPI_FORMAT_UINT64 (ACPI_GET_ADDRESS (Resource->AddressMin)));

    AcpiDmIndent (Level + 1);
    AcpiOsPrintf ("0x%8.8X%8.8X,\n",
        ACPI_FORMAT_UINT64 (ACPI_GET_ADDRESS (Resource->AddressMax)));

    AcpiDmIndent (Level + 1);
    AcpiOsPrintf ("0x%8.8X%8.8X,\n",
        ACPI_FORMAT_UINT64 (ACPI_GET_ADDRESS (Resource->TranslationOffset)));

    AcpiDmIndent (Level + 1);
    AcpiOsPrintf ("0x%8.8X%8.8X",
        ACPI_FORMAT_UINT64 (ACPI_GET_ADDRESS (Resource->AddressLength)));

    /* Optional fields */

    if (Length > 43)
    {
        AcpiOsPrintf (", 0x%2.2X",
            Resource->OptionalFields[0]);
    }
    if (Length > 44)
    {
        AcpiOsPrintf (", %s",
            &Resource->OptionalFields[1]);
    }

    AcpiOsPrintf (")\n");
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmMemory24Descriptor
 *
 * PARAMETERS:  Resource            - Pointer to the resource descriptor
 *              Length              - Length of the descriptor in bytes
 *              Level               - Current source code indentation level
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode a Memory24 descriptor
 *
 ******************************************************************************/

void
AcpiDmMemory24Descriptor (
    ASL_MEMORY_24_DESC      *Resource,
    UINT32                  Length,
    UINT32                  Level)
{

    AcpiDmIndent (Level);
    AcpiOsPrintf ("Memory24 (%s, 0x%4.4X, 0x%4.4X, 0x%4.4X, 0x%4.4X)\n",
        AcpiGbl_RWDecode [Resource->Information & 1],
        (UINT32) Resource->AddressMin,
        (UINT32) Resource->AddressMax,
        (UINT32) Resource->Alignment,
        (UINT32) Resource->RangeLength);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmMemory32Descriptor
 *
 * PARAMETERS:  Resource            - Pointer to the resource descriptor
 *              Length              - Length of the descriptor in bytes
 *              Level               - Current source code indentation level
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode a Memory32 descriptor
 *
 ******************************************************************************/

void
AcpiDmMemory32Descriptor (
    ASL_MEMORY_32_DESC      *Resource,
    UINT32                  Length,
    UINT32                  Level)
{

    AcpiDmIndent (Level);
    AcpiOsPrintf ("Memory32 (%s, 0x%8.8X, 0x%8.8X, 0x%8.8X, 0x%8.8X)\n",
        AcpiGbl_RWDecode [Resource->Information & 1],
        Resource->AddressMin,
        Resource->AddressMax,
        Resource->Alignment,
        Resource->RangeLength);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmFixedMem32Descriptor
 *
 * PARAMETERS:  Resource            - Pointer to the resource descriptor
 *              Length              - Length of the descriptor in bytes
 *              Level               - Current source code indentation level
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode a Fixed Memory32 descriptor
 *
 ******************************************************************************/

void
AcpiDmFixedMem32Descriptor (
    ASL_FIXED_MEMORY_32_DESC *Resource,
    UINT32                  Length,
    UINT32                  Level)
{

    AcpiDmIndent (Level);
    AcpiOsPrintf ("Memory32Fixed (%s, 0x%8.8X, 0x%8.8X)\n",
        AcpiGbl_RWDecode [Resource->Information & 1],
        Resource->BaseAddress,
        Resource->RangeLength);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmGenericRegisterDescriptor
 *
 * PARAMETERS:  Resource            - Pointer to the resource descriptor
 *              Length              - Length of the descriptor in bytes
 *              Level               - Current source code indentation level
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode a Generic Register descriptor
 *
 ******************************************************************************/

void
AcpiDmGenericRegisterDescriptor (
    ASL_GENERAL_REGISTER_DESC *Resource,
    UINT32                  Length,
    UINT32                  Level)
{

    AcpiDmIndent (Level);
    AcpiOsPrintf ("Register (");

    AcpiDmAddressSpace (Resource->AddressSpaceId);

    AcpiOsPrintf ("0x%2.2X, 0x%2.2X, 0x%8.8X%8.8X)\n",
        (UINT32) Resource->BitWidth,
        (UINT32) Resource->BitOffset,
        ACPI_FORMAT_UINT64 (ACPI_GET_ADDRESS (Resource->Address)));
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmInterruptDescriptor
 *
 * PARAMETERS:  Resource            - Pointer to the resource descriptor
 *              Length              - Length of the descriptor in bytes
 *              Level               - Current source code indentation level
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode a extended Interrupt descriptor
 *
 ******************************************************************************/

void
AcpiDmInterruptDescriptor (
    ASL_EXTENDED_XRUPT_DESC *Resource,
    UINT32                  Length,
    UINT32                  Level)
{
    UINT32                  i;
    UINT8                   *Rover;


    AcpiDmIndent (Level);
    AcpiOsPrintf ("Interrupt (%s, %s, %s, %s",
        AcpiGbl_ConsumeDecode [(Resource->Flags & 1)],
        AcpiGbl_HEDecode [(Resource->Flags >> 1) & 1],
        AcpiGbl_LLDecode [(Resource->Flags >> 2) & 1],
        AcpiGbl_SHRDecode [(Resource->Flags >> 3) & 1]);

    /* Resource Index/Source, optional -- at end of descriptor */

    if (Resource->Length > (UINT16) (4 * Resource->TableLength) + 2)
    {
        /* Get a pointer past the interrupt values */

        Rover = ((UINT8 *) Resource) + ((4 * Resource->TableLength) + 5);

        /* Resource Index */
        /* Resource Source */

        AcpiOsPrintf (", 0x%X, \"%s\"", (UINT32) Rover[0], (char *) &Rover[1]);
    }

    AcpiOsPrintf (")\n");
    AcpiDmIndent (Level);
    AcpiOsPrintf ("{\n");
    for (i = 0; i < Resource->TableLength; i++)
    {
        AcpiDmIndent (Level + 1);
        AcpiOsPrintf ("0x%8.8X,\n", (UINT32) Resource->InterruptNumber[i]);
    }

    AcpiDmIndent (Level);
    AcpiOsPrintf ("}\n");
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmVendorLargeDescriptor
 *
 * PARAMETERS:  Resource            - Pointer to the resource descriptor
 *              Length              - Length of the descriptor in bytes
 *              Level               - Current source code indentation level
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode a Vendor Large descriptor
 *
 ******************************************************************************/

void
AcpiDmVendorLargeDescriptor (
    ASL_LARGE_VENDOR_DESC   *Resource,
    UINT32                  Length,
    UINT32                  Level)
{

    AcpiDmIndent (Level);
    AcpiOsPrintf ("VendorLong ()\n");
    AcpiDmIndent (Level);

    AcpiOsPrintf ("{\n");

    AcpiDmDisasmByteList (Level + 1, (UINT8 *) Resource->VendorDefined, Length);
    AcpiDmIndent (Level);
    AcpiOsPrintf ("}\n");
}


#endif

