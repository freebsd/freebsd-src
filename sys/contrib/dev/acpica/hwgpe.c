
/******************************************************************************
 *
 * Module Name: hwgpe - Low level GPE enable/disable/clear functions
 *              $Revision: 53 $
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

#include "acpi.h"
#include "acevents.h"

#define _COMPONENT          ACPI_HARDWARE
        ACPI_MODULE_NAME    ("hwgpe")


/******************************************************************************
 *
 * FUNCTION:    AcpiHwEnableGpe
 *
 * PARAMETERS:  GpeNumber       - The GPE
 *
 * RETURN:      None
 *
 * DESCRIPTION: Enable a single GPE.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiHwEnableGpe (
    ACPI_GPE_EVENT_INFO     *GpeEventInfo)
{
    UINT32                  InByte;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_ENTRY ();


    /*
     * Read the current value of the register, set the appropriate bit
     * to enable the GPE, and write out the new register.
     */
    Status = AcpiHwLowLevelRead (8, &InByte,
                    &GpeEventInfo->RegisterInfo->EnableAddress);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /* Write with the new GPE bit enabled */

    Status = AcpiHwLowLevelWrite (8, (InByte | GpeEventInfo->BitMask),
                    &GpeEventInfo->RegisterInfo->EnableAddress);

    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiHwEnableGpeForWakeup
 *
 * PARAMETERS:  GpeNumber       - The GPE
 *
 * RETURN:      None
 *
 * DESCRIPTION: Keep track of which GPEs the OS has requested not be
 *              disabled when going to sleep.
 *
 ******************************************************************************/

void
AcpiHwEnableGpeForWakeup (
    ACPI_GPE_EVENT_INFO     *GpeEventInfo)
{
    ACPI_GPE_REGISTER_INFO  *GpeRegisterInfo;


    ACPI_FUNCTION_ENTRY ();


    /* Get the info block for the entire GPE register */

    GpeRegisterInfo = GpeEventInfo->RegisterInfo;
    if (!GpeRegisterInfo)
    {
        return;
    }

    /*
     * Set the bit so we will not disable this when sleeping
     */
    GpeRegisterInfo->WakeEnable |= GpeEventInfo->BitMask;
}


/******************************************************************************
 *
 * FUNCTION:    AcpiHwDisableGpe
 *
 * PARAMETERS:  GpeNumber       - The GPE
 *
 * RETURN:      None
 *
 * DESCRIPTION: Disable a single GPE.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiHwDisableGpe (
    ACPI_GPE_EVENT_INFO     *GpeEventInfo)
{
    UINT32                  InByte;
    ACPI_STATUS             Status;
    ACPI_GPE_REGISTER_INFO  *GpeRegisterInfo;


    ACPI_FUNCTION_ENTRY ();


    /* Get the info block for the entire GPE register */

    GpeRegisterInfo = GpeEventInfo->RegisterInfo;
    if (!GpeRegisterInfo)
    {
        return (AE_BAD_PARAMETER);
    }

    /*
     * Read the current value of the register, clear the appropriate bit,
     * and write out the new register value to disable the GPE.
     */
    Status = AcpiHwLowLevelRead (8, &InByte,
                    &GpeRegisterInfo->EnableAddress);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /* Write the byte with this GPE bit cleared */

    Status = AcpiHwLowLevelWrite (8, (InByte & ~(GpeEventInfo->BitMask)),
                    &GpeRegisterInfo->EnableAddress);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    AcpiHwDisableGpeForWakeup (GpeEventInfo);
    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiHwDisableGpeForWakeup
 *
 * PARAMETERS:  GpeNumber       - The GPE
 *
 * RETURN:      None
 *
 * DESCRIPTION: Keep track of which GPEs the OS has requested not be
 *              disabled when going to sleep.
 *
 ******************************************************************************/

void
AcpiHwDisableGpeForWakeup (
    ACPI_GPE_EVENT_INFO     *GpeEventInfo)
{
    ACPI_GPE_REGISTER_INFO  *GpeRegisterInfo;


    ACPI_FUNCTION_ENTRY ();


    /* Get the info block for the entire GPE register */

    GpeRegisterInfo = GpeEventInfo->RegisterInfo;
    if (!GpeRegisterInfo)
    {
        return;
    }

    /*
     * Clear the bit so we will disable this when sleeping
     */
    GpeRegisterInfo->WakeEnable &= ~(GpeEventInfo->BitMask);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiHwClearGpe
 *
 * PARAMETERS:  GpeNumber       - The GPE
 *
 * RETURN:      None
 *
 * DESCRIPTION: Clear a single GPE.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiHwClearGpe (
    ACPI_GPE_EVENT_INFO     *GpeEventInfo)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_ENTRY ();


    /*
     * Write a one to the appropriate bit in the status register to
     * clear this GPE.
     */
    Status = AcpiHwLowLevelWrite (8, GpeEventInfo->BitMask,
                    &GpeEventInfo->RegisterInfo->StatusAddress);

    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiHwGetGpeStatus
 *
 * PARAMETERS:  GpeNumber       - The GPE
 *
 * RETURN:      None
 *
 * DESCRIPTION: Return the status of a single GPE.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiHwGetGpeStatus (
    ACPI_GPE_EVENT_INFO     *GpeEventInfo,
    ACPI_EVENT_STATUS       *EventStatus)
{
    UINT32                  InByte;
    UINT8                   BitMask;
    ACPI_GPE_REGISTER_INFO  *GpeRegisterInfo;
    ACPI_STATUS             Status;
    ACPI_EVENT_STATUS       LocalEventStatus = 0;


    ACPI_FUNCTION_ENTRY ();


    if (!EventStatus)
    {
        return (AE_BAD_PARAMETER);
    }

    /* Get the info block for the entire GPE register */

    GpeRegisterInfo = GpeEventInfo->RegisterInfo;

    /* Get the register bitmask for this GPE */

    BitMask = GpeEventInfo->BitMask;

    /* GPE Enabled? */

    Status = AcpiHwLowLevelRead (8, &InByte, &GpeRegisterInfo->EnableAddress);
    if (ACPI_FAILURE (Status))
    {
        goto UnlockAndExit;
    }

    if (BitMask & InByte)
    {
        LocalEventStatus |= ACPI_EVENT_FLAG_ENABLED;
    }

    /* GPE Enabled for wake? */

    if (BitMask & GpeRegisterInfo->WakeEnable)
    {
        LocalEventStatus |= ACPI_EVENT_FLAG_WAKE_ENABLED;
    }

    /* GPE active (set)? */

    Status = AcpiHwLowLevelRead (8, &InByte, &GpeRegisterInfo->StatusAddress);
    if (ACPI_FAILURE (Status))
    {
        goto UnlockAndExit;
    }

    if (BitMask & InByte)
    {
        LocalEventStatus |= ACPI_EVENT_FLAG_SET;
    }

    /* Set return value */

    (*EventStatus) = LocalEventStatus;


UnlockAndExit:
    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiHwDisableGpeBlock
 *
 * PARAMETERS:  GpeXruptInfo        - GPE Interrupt info
 *              GpeBlock            - Gpe Block info
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Disable all GPEs within a GPE block
 *
 ******************************************************************************/

ACPI_STATUS
AcpiHwDisableGpeBlock (
    ACPI_GPE_XRUPT_INFO     *GpeXruptInfo,
    ACPI_GPE_BLOCK_INFO     *GpeBlock)
{
    UINT32                  i;
    ACPI_GPE_REGISTER_INFO  *GpeRegisterInfo;
    ACPI_STATUS             Status;


    /* Get the register info for the entire GPE block */

    GpeRegisterInfo = GpeBlock->RegisterInfo;

    /* Examine each GPE Register within the block */

    for (i = 0; i < GpeBlock->RegisterCount; i++)
    {
        Status = AcpiHwLowLevelWrite (8, 0x00,
                    &GpeBlock->RegisterInfo[i].EnableAddress);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiHwClearGpeBlock
 *
 * PARAMETERS:  GpeXruptInfo        - GPE Interrupt info
 *              GpeBlock            - Gpe Block info
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Clear all GPEs within a GPE block
 *
 ******************************************************************************/

ACPI_STATUS
AcpiHwClearGpeBlock (
    ACPI_GPE_XRUPT_INFO     *GpeXruptInfo,
    ACPI_GPE_BLOCK_INFO     *GpeBlock)
{
    UINT32                  i;
    ACPI_GPE_REGISTER_INFO  *GpeRegisterInfo;
    ACPI_STATUS             Status;


    /* Get the register info for the entire GPE block */

    GpeRegisterInfo = GpeBlock->RegisterInfo;

    /* Examine each GPE Register within the block */

    for (i = 0; i < GpeBlock->RegisterCount; i++)
    {
        Status = AcpiHwLowLevelWrite (8, 0xFF,
                    &GpeBlock->RegisterInfo[i].StatusAddress);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiHwDisableNonWakeupGpeBlock
 *
 * PARAMETERS:  GpeXruptInfo        - GPE Interrupt info
 *              GpeBlock            - Gpe Block info
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Disable all GPEs except wakeup GPEs in a GPE block
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiHwDisableNonWakeupGpeBlock (
    ACPI_GPE_XRUPT_INFO     *GpeXruptInfo,
    ACPI_GPE_BLOCK_INFO     *GpeBlock)
{
    UINT32                  i;
    ACPI_GPE_REGISTER_INFO  *GpeRegisterInfo;
    UINT32                  InValue;
    ACPI_STATUS             Status;


    /* Get the register info for the entire GPE block */

    GpeRegisterInfo = GpeBlock->RegisterInfo;

    /* Examine each GPE Register within the block */

    for (i = 0; i < GpeBlock->RegisterCount; i++)
    {
        /*
         * Read the enabled status of all GPEs. We
         * will be using it to restore all the GPEs later.
         */
        Status = AcpiHwLowLevelRead (8, &InValue,
                    &GpeRegisterInfo->EnableAddress);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        GpeRegisterInfo->Enable = (UINT8) InValue;

        /*
         * Disable all GPEs except wakeup GPEs.
         */
        Status = AcpiHwLowLevelWrite (8, GpeRegisterInfo->WakeEnable,
                &GpeRegisterInfo->EnableAddress);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        GpeRegisterInfo++;
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiHwDisableNonWakeupGpes
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Disable all non-wakeup GPEs
 *              Called with interrupts disabled. The interrupt handler also
 *              modifies GpeRegisterInfo->Enable, so it should not be
 *              given the chance to run until after non-wake GPEs are
 *              re-enabled.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiHwDisableNonWakeupGpes (
    void)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_ENTRY ();


    Status = AcpiEvWalkGpeList (AcpiHwDisableNonWakeupGpeBlock);

    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiHwEnableNonWakeupGpeBlock
 *
 * PARAMETERS:  GpeXruptInfo        - GPE Interrupt info
 *              GpeBlock            - Gpe Block info
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Enable a single GPE.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiHwEnableNonWakeupGpeBlock (
    ACPI_GPE_XRUPT_INFO     *GpeXruptInfo,
    ACPI_GPE_BLOCK_INFO     *GpeBlock)
{
    UINT32                  i;
    ACPI_GPE_REGISTER_INFO  *GpeRegisterInfo;
    ACPI_STATUS             Status;


    /* This callback processes one entire GPE block */

    /* Get the register info for the entire GPE block */

    GpeRegisterInfo = GpeBlock->RegisterInfo;

    /* Examine each GPE register within the block */

    for (i = 0; i < GpeBlock->RegisterCount; i++)
    {
        /*
         * We previously stored the enabled status of all GPEs.
         * Blast them back in.
         */
        Status = AcpiHwLowLevelWrite (8, GpeRegisterInfo->Enable,
                    &GpeRegisterInfo->EnableAddress);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        GpeRegisterInfo++;
    }


    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiHwEnableNonWakeupGpes
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Enable all non-wakeup GPEs we previously enabled.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiHwEnableNonWakeupGpes (
    void)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_ENTRY ();


    Status = AcpiEvWalkGpeList (AcpiHwEnableNonWakeupGpeBlock);

    return (Status);
}
