
/******************************************************************************
 *
 * Module Name: amregion - ACPI default OpRegion (address space) handlers
 *              $Revision: 40 $
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


#define __AMREGION_C__

#include "acpi.h"
#include "acinterp.h"
#include "amlcode.h"
#include "acnamesp.h"
#include "achware.h"
#include "acevents.h"


#define _COMPONENT          INTERPRETER
        MODULE_NAME         ("amregion")


/*******************************************************************************
 *
 * FUNCTION:    AcpiAmlSystemMemorySpaceHandler
 *
 * PARAMETERS:  Function            - Read or Write operation
 *              Address             - Where in the space to read or write
 *              BitWidth            - Field width in bits (8, 16, or 32)
 *              Value               - Pointer to in or out value
 *              HandlerContext      - Pointer to Handler's context
 *              RegionContext       - Pointer to context specific to the
 *                                      accessed region
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Handler for the System Memory address space (Op Region)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiAmlSystemMemorySpaceHandler (
    UINT32                  Function,
    ACPI_PHYSICAL_ADDRESS   Address,
    UINT32                  BitWidth,
    UINT32                  *Value,
    void                    *HandlerContext,
    void                    *RegionContext)
{
    ACPI_STATUS             Status = AE_OK;
    void                    *LogicalAddrPtr = NULL;
    MEM_HANDLER_CONTEXT     *MemInfo = RegionContext;
    UINT32                  Length;


    FUNCTION_TRACE ("AmlSystemMemorySpaceHandler");


    /* Validate and translate the bit width */

    switch (BitWidth)
    {
    case 8:
        Length = 1;
        break;

    case 16:
        Length = 2;
        break;

    case 32:
        Length = 4;
        break;

    default:
        DEBUG_PRINT (ACPI_ERROR,
            ("AmlSystemMemorySpaceHandler: Invalid SystemMemory width %d\n",
            BitWidth));
        return_ACPI_STATUS (AE_AML_OPERAND_VALUE);
        break;
    }


    /*
     * Does the request fit into the cached memory mapping?
     * Is 1) Address below the current mapping? OR
     *    2) Address beyond the current mapping?
     */

    if ((Address < MemInfo->MappedPhysicalAddress) ||
        ((Address + Length) >
            (MemInfo->MappedPhysicalAddress + MemInfo->MappedLength)))
    {
        /*
         * The request cannot be resolved by the current memory mapping;
         * Delete the existing mapping and create a new one.
         */

        if (MemInfo->MappedLength)
        {
            /* Valid mapping, delete it */

            AcpiOsUnmapMemory (MemInfo->MappedLogicalAddress,
                                MemInfo->MappedLength);
        }

        MemInfo->MappedLength = 0;  /* In case of failure below */

        /* Create a new mapping starting at the address given */

        Status = AcpiOsMapMemory (Address, SYSMEM_REGION_WINDOW_SIZE,
                                    (void **) &MemInfo->MappedLogicalAddress);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

        /* TBD: should these pointers go to 64-bit in all cases ? */

        MemInfo->MappedPhysicalAddress = Address;
        MemInfo->MappedLength = SYSMEM_REGION_WINDOW_SIZE;
    }


    /*
     * Generate a logical pointer corresponding to the address we want to
     * access
     */

    /* TBD: should these pointers go to 64-bit in all cases ? */

    LogicalAddrPtr = MemInfo->MappedLogicalAddress +
                    (Address - MemInfo->MappedPhysicalAddress);

    /* Perform the memory read or write */

    switch (Function)
    {

    case ADDRESS_SPACE_READ:

        DEBUG_PRINT ((TRACE_OPREGION | VERBOSE_INFO),
            ("Read (%d width) Address:0x%X\n", BitWidth, Address));

        switch (BitWidth)
        {
        case 8:
            *Value = (UINT32)* (UINT8 *) LogicalAddrPtr;
            break;

        case 16:
            MOVE_UNALIGNED16_TO_32 (Value, LogicalAddrPtr);
            break;

        case 32:
            MOVE_UNALIGNED32_TO_32 (Value, LogicalAddrPtr);
            break;
        }

        break;


    case ADDRESS_SPACE_WRITE:

        DEBUG_PRINT ((TRACE_OPREGION | VERBOSE_INFO),
            ("Write (%d width) Address:0x%p Value 0x%X\n",
            BitWidth, Address, *Value));

        switch (BitWidth)
        {
        case 8:
            *(UINT8 *) LogicalAddrPtr = (UINT8) *Value;
            break;

        case 16:
            MOVE_UNALIGNED16_TO_16 (LogicalAddrPtr, Value);
            break;

        case 32:
            MOVE_UNALIGNED32_TO_32 (LogicalAddrPtr, Value);
            break;
        }

        break;


    default:
        Status = AE_BAD_PARAMETER;
        break;
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiAmlSystemIoSpaceHandler
 *
 * PARAMETERS:  Function            - Read or Write operation
 *              Address             - Where in the space to read or write
 *              BitWidth            - Field width in bits (8, 16, or 32)
 *              Value               - Pointer to in or out value
 *              HandlerContext      - Pointer to Handler's context
 *              RegionContext       - Pointer to context specific to the
 *                                      accessed region
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Handler for the System IO address space (Op Region)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiAmlSystemIoSpaceHandler (
    UINT32                  Function,
    ACPI_PHYSICAL_ADDRESS   Address,
    UINT32                  BitWidth,
    UINT32                  *Value,
    void                    *HandlerContext,
    void                    *RegionContext)
{
    ACPI_STATUS             Status = AE_OK;


    FUNCTION_TRACE ("AmlSystemIoSpaceHandler");


    /* Decode the function parameter */

    switch (Function)
    {

    case ADDRESS_SPACE_READ:

        DEBUG_PRINT ((TRACE_OPREGION | VERBOSE_INFO),
            ("Read(%d width) Address:0x%08x\n", BitWidth, Address));

        switch (BitWidth)
        {
        /* I/O Port width */

        case 8:
            *Value = (UINT32) AcpiOsIn8 ((ACPI_IO_ADDRESS) Address);
            break;

        case 16:
            *Value = (UINT32) AcpiOsIn16 ((ACPI_IO_ADDRESS) Address);
            break;

        case 32:
            *Value = AcpiOsIn32 ((ACPI_IO_ADDRESS) Address);
            break;

        default:
            DEBUG_PRINT (ACPI_ERROR,
                    ("AmlSystemIoSpaceHandler: Invalid SystemIO width %d\n",
                    BitWidth));
            Status = AE_AML_OPERAND_VALUE;
        }

        break;


    case ADDRESS_SPACE_WRITE:

        DEBUG_PRINT ((TRACE_OPREGION | VERBOSE_INFO),
            ("Write(%d width) Address:0x%08x Value 0x%08x\n",
            BitWidth, Address, *Value));

        switch (BitWidth)
        {
        /* I/O Port width */
        case 8:
            AcpiOsOut8 ((ACPI_IO_ADDRESS) Address, (UINT8) *Value);
            break;

        case 16:
            AcpiOsOut16 ((ACPI_IO_ADDRESS) Address, (UINT16) *Value);
            break;

        case 32:
            AcpiOsOut32 ((ACPI_IO_ADDRESS) Address, *Value);
            break;

        default:
            DEBUG_PRINT (ACPI_ERROR, (
                    "AmlSystemIoSpaceHandler: Invalid SystemIO width %d\n",
                    BitWidth));
            Status = AE_AML_OPERAND_VALUE;
        }

        break;


    default:
        Status = AE_BAD_PARAMETER;
        break;
    }

    return_ACPI_STATUS (Status);
}

/*******************************************************************************
 *
 * FUNCTION:    AcpiAmlPciConfigSpaceHandler
 *
 * PARAMETERS:  Function            - Read or Write operation
 *              Address             - Where in the space to read or write
 *              BitWidth            - Field width in bits (8, 16, or 32)
 *              Value               - Pointer to in or out value
 *              HandlerContext      - Pointer to Handler's context
 *              RegionContext       - Pointer to context specific to the
 *                                      accessed region
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Handler for the PCI Config address space (Op Region)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiAmlPciConfigSpaceHandler (
    UINT32                  Function,
    ACPI_PHYSICAL_ADDRESS   Address,
    UINT32                  BitWidth,
    UINT32                  *Value,
    void                    *HandlerContext,
    void                    *RegionContext)
{
    ACPI_STATUS             Status = AE_OK;
    UINT32                  PciBus;
    UINT32                  DevFunc;
    UINT8                   PciReg;
    PCI_HANDLER_CONTEXT    *PCIContext;


    FUNCTION_TRACE ("AmlPciConfigSpaceHandler");

    /*
     *  The arguments to AcpiOs(Read|Write)PciCfg(Byte|Word|Dword) are:
     *
     *  SegBus - 0xSSSSBBBB     - SSSS is the PCI bus segment
     *                            BBBB is the PCI bus number
     *
     *  DevFunc - 0xDDDDFFFF    - DDDD is the PCI device number
     *                            FFFF is the PCI device function number
     *
     *  RegNum - Config space register must be < 40h
     *
     *  Value - input value for write, output for read
     *
     */

    PCIContext = (PCI_HANDLER_CONTEXT *) RegionContext;

    PciBus = LOWORD(PCIContext->Seg) << 16;
    PciBus |= LOWORD(PCIContext->Bus);

    DevFunc = PCIContext->DevFunc;

    PciReg  = (UINT8) Address;

    switch (Function)
    {

    case ADDRESS_SPACE_READ:

        DEBUG_PRINT ((TRACE_OPREGION | VERBOSE_INFO),
            ("R%d S(%04x) B(%04x) DF(%08x) R(%04x)\n", BitWidth,
            PCIContext->Seg,PCIContext->Bus,PCIContext->DevFunc, PciReg));

        *Value  = 0;

        switch (BitWidth)
        {
        /* PCI Register width */

        case 8:
            Status = AcpiOsReadPciCfgByte (PciBus, DevFunc, PciReg,
                                            (UINT8 *) Value);
            break;

        case 16:
            Status = AcpiOsReadPciCfgWord (PciBus, DevFunc, PciReg,
                                            (UINT16 *) Value);
            break;

        case 32:
            Status = AcpiOsReadPciCfgDword (PciBus, DevFunc, PciReg,
                                            Value);
            break;

        default:
            DEBUG_PRINT (ACPI_ERROR,
                ("AmlPciConfigSpaceHandler: Invalid PCIConfig width %d\n",
                BitWidth));
            Status = AE_AML_OPERAND_VALUE;

        } /* Switch bitWidth */

        break;


    case ADDRESS_SPACE_WRITE:

        DEBUG_PRINT ((TRACE_OPREGION | VERBOSE_INFO),
            ("W%d S(%04x) B(%04x) DF(%08x) R(%04x) D(%08x)\n", BitWidth,
            PCIContext->Seg,PCIContext->Bus,PCIContext->DevFunc,
            PciReg,*Value));

        switch (BitWidth)
        {
        /* PCI Register width */

        case 8:
            Status = AcpiOsWritePciCfgByte (PciBus, DevFunc, PciReg,
                                                *(UINT8 *) Value);
            break;

        case 16:
            Status = AcpiOsWritePciCfgWord (PciBus, DevFunc, PciReg,
                                                *(UINT16 *) Value);
            break;

        case 32:
            Status = AcpiOsWritePciCfgDword (PciBus, DevFunc, PciReg,
                                                *Value);
            break;

        default:
            DEBUG_PRINT (ACPI_ERROR, (
                "AmlPciConfigSpaceHandler: Invalid PCIConfig width %d\n",
                BitWidth));
            Status = AE_AML_OPERAND_VALUE;

        } /* Switch bitWidth */

        break;


    default:

        Status = AE_BAD_PARAMETER;
        break;

    }

    return_ACPI_STATUS (Status);
}

