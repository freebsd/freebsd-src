/******************************************************************************
 *
 * Module Name: evevent - Fixed and General Purpose AcpiEvent
 *                          handling and dispatch
 *              $Revision: 43 $
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

#include "acpi.h"
#include "achware.h"
#include "acevents.h"
#include "acnamesp.h"

#define _COMPONENT          ACPI_EVENTS
        MODULE_NAME         ("evevent")


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvInitialize
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Ensures that the system control interrupt (SCI) is properly
 *              configured, disables SCI event sources, installs the SCI
 *              handler
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEvInitialize (
    void)
{
    ACPI_STATUS             Status;


    FUNCTION_TRACE ("EvInitialize");


    /* Make sure we have ACPI tables */

    if (!AcpiGbl_DSDT)
    {
        DEBUG_PRINTP (ACPI_WARN, ("No ACPI tables present!\n"));
        return_ACPI_STATUS (AE_NO_ACPI_TABLES);
    }


    /* Make sure the BIOS supports ACPI mode */

    if (SYS_MODE_LEGACY == AcpiHwGetModeCapabilities())
    {
        DEBUG_PRINTP (ACPI_WARN, ("ACPI Mode is not supported!\n"));
        return_ACPI_STATUS (AE_ERROR);
    }


    AcpiGbl_OriginalMode = AcpiHwGetMode();

    /*
     * Initialize the Fixed and General Purpose AcpiEvents prior.  This is
     * done prior to enabling SCIs to prevent interrupts from occuring
     * before handers are installed.
     */

    Status = AcpiEvFixedEventInitialize ();
    if (ACPI_FAILURE (Status))
    {
        DEBUG_PRINTP (ACPI_FATAL, ("Unable to initialize fixed events.\n"));
        return_ACPI_STATUS (Status);
    }

    Status = AcpiEvGpeInitialize ();
    if (ACPI_FAILURE (Status))
    {
        DEBUG_PRINTP (ACPI_FATAL, ("Unable to initialize general purpose events.\n"));
        return_ACPI_STATUS (Status);
    }

    /* Install the SCI handler */

    Status = AcpiEvInstallSciHandler ();
    if (ACPI_FAILURE (Status))
    {
        DEBUG_PRINTP (ACPI_FATAL, ("Unable to install System Control Interrupt Handler\n"));
        return_ACPI_STATUS (Status);
    }


    /* Install handlers for control method GPE handlers (_Lxx, _Exx) */

    Status = AcpiEvInitGpeControlMethods ();
    if (ACPI_FAILURE (Status))
    {
        DEBUG_PRINTP (ACPI_FATAL, ("Unable to initialize Gpe control methods\n"));
        return_ACPI_STATUS (Status);
    }

    /* Install the handler for the Global Lock */

    Status = AcpiEvInitGlobalLockHandler ();
    if (ACPI_FAILURE (Status))
    {
        DEBUG_PRINTP (ACPI_FATAL, ("Unable to initialize Global Lock handler\n"));
        return_ACPI_STATUS (Status);
    }


    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvFixedEventInitialize
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Initialize the Fixed AcpiEvent data structures
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEvFixedEventInitialize(void)
{
    int                     i = 0;

    /* Initialize the structure that keeps track of fixed event handlers */

    for (i = 0; i < ACPI_NUM_FIXED_EVENTS; i++)
    {
        AcpiGbl_FixedEventHandlers[i].Handler = NULL;
        AcpiGbl_FixedEventHandlers[i].Context = NULL;
    }

    AcpiHwRegisterBitAccess (ACPI_WRITE, ACPI_MTX_LOCK, TMR_EN, 0);
    AcpiHwRegisterBitAccess (ACPI_WRITE, ACPI_MTX_LOCK, GBL_EN, 0);
    AcpiHwRegisterBitAccess (ACPI_WRITE, ACPI_MTX_LOCK, PWRBTN_EN, 0);
    AcpiHwRegisterBitAccess (ACPI_WRITE, ACPI_MTX_LOCK, SLPBTN_EN, 0);
    AcpiHwRegisterBitAccess (ACPI_WRITE, ACPI_MTX_LOCK, RTC_EN, 0);

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvFixedEventDetect
 *
 * PARAMETERS:  None
 *
 * RETURN:      INTERRUPT_HANDLED or INTERRUPT_NOT_HANDLED
 *
 * DESCRIPTION: Checks the PM status register for fixed events
 *
 ******************************************************************************/

UINT32
AcpiEvFixedEventDetect(void)
{
    UINT32                  IntStatus = INTERRUPT_NOT_HANDLED;
    UINT32                  StatusRegister;
    UINT32                  EnableRegister;

    /*
     * Read the fixed feature status and enable registers, as all the cases
     * depend on their values.
     */

    StatusRegister = AcpiHwRegisterRead (ACPI_MTX_DO_NOT_LOCK, PM1_STS);
    EnableRegister = AcpiHwRegisterRead (ACPI_MTX_DO_NOT_LOCK, PM1_EN);

    DEBUG_PRINT (TRACE_INTERRUPTS,
        ("Fixed AcpiEvent Block: Enable %08X Status %08X\n",
        EnableRegister, StatusRegister));


    /* power management timer roll over */

    if ((StatusRegister & ACPI_STATUS_PMTIMER) &&
        (EnableRegister & ACPI_ENABLE_PMTIMER))
    {
        IntStatus |= AcpiEvFixedEventDispatch (ACPI_EVENT_PMTIMER);
    }

    /* global event (BIOS wants the global lock) */

    if ((StatusRegister & ACPI_STATUS_GLOBAL) &&
        (EnableRegister & ACPI_ENABLE_GLOBAL))
    {
        IntStatus |= AcpiEvFixedEventDispatch (ACPI_EVENT_GLOBAL);
    }

    /* power button event */

    if ((StatusRegister & ACPI_STATUS_POWER_BUTTON) &&
        (EnableRegister & ACPI_ENABLE_POWER_BUTTON))
    {
        IntStatus |= AcpiEvFixedEventDispatch (ACPI_EVENT_POWER_BUTTON);
    }

    /* sleep button event */

    if ((StatusRegister & ACPI_STATUS_SLEEP_BUTTON) &&
        (EnableRegister & ACPI_ENABLE_SLEEP_BUTTON))
    {
        IntStatus |= AcpiEvFixedEventDispatch (ACPI_EVENT_SLEEP_BUTTON);
    }

    return (IntStatus);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvFixedEventDispatch
 *
 * PARAMETERS:  Event               - Event type
 *
 * RETURN:      INTERRUPT_HANDLED or INTERRUPT_NOT_HANDLED
 *
 * DESCRIPTION: Clears the status bit for the requested event, calls the
 *              handler that previously registered for the event.
 *
 ******************************************************************************/

UINT32
AcpiEvFixedEventDispatch (
    UINT32                  Event)
{
    UINT32 RegisterId;

    /* Clear the status bit */

    switch (Event)
    {
    case ACPI_EVENT_PMTIMER:
        RegisterId = TMR_STS;
        break;

    case ACPI_EVENT_GLOBAL:
        RegisterId = GBL_STS;
        break;

    case ACPI_EVENT_POWER_BUTTON:
        RegisterId = PWRBTN_STS;
        break;

    case ACPI_EVENT_SLEEP_BUTTON:
        RegisterId = SLPBTN_STS;
        break;

    case ACPI_EVENT_RTC:
        RegisterId = RTC_STS;
        break;

    default:
        return 0;
        break;
    }

    AcpiHwRegisterBitAccess (ACPI_WRITE, ACPI_MTX_DO_NOT_LOCK, RegisterId, 1);

    /*
     * Make sure we've got a handler.  If not, report an error.
     * The event is disabled to prevent further interrupts.
     */
    if (NULL == AcpiGbl_FixedEventHandlers[Event].Handler)
    {
        RegisterId = (PM1_EN | REGISTER_BIT_ID(RegisterId));

        AcpiHwRegisterBitAccess (ACPI_WRITE, ACPI_MTX_DO_NOT_LOCK,
                                RegisterId, 0);

        REPORT_ERROR (
            ("EvGpeDispatch: No installed handler for fixed event [%08X]\n",
            Event));

        return (INTERRUPT_NOT_HANDLED);
    }

    /* Invoke the handler */

    return ((AcpiGbl_FixedEventHandlers[Event].Handler)(
                                AcpiGbl_FixedEventHandlers[Event].Context));
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvGpeInitialize
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Initialize the GPE data structures
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEvGpeInitialize (void)
{
    UINT32                  i;
    UINT32                  j;
    UINT32                  RegisterIndex;
    UINT32                  GpeNumber;
    UINT16                  Gpe0RegisterCount;
    UINT16                  Gpe1RegisterCount;


    FUNCTION_TRACE ("EvGpeInitialize");

    /*
     * Set up various GPE counts
     *
     * You may ask,why are the GPE register block lengths divided by 2?
     * From the ACPI 2.0 Spec, section, 4.7.1.6 General-Purpose Event
     * Registers, we have,
     *
     * "Each register block contains two registers of equal length
     * GPEx_STS and GPEx_EN (where x is 0 or 1). The length of the
     * GPE0_STS and GPE0_EN registers is equal to half the GPE0_LEN
     * The length of the GPE1_STS and GPE1_EN registers is equal to
     * half the GPE1_LEN. If a generic register block is not supported
     * then its respective block pointer and block length values in the
     * FADT table contain zeros. The GPE0_LEN and GPE1_LEN do not need
     * to be the same size."
     */

    Gpe0RegisterCount           = (UINT16) DIV_2 (AcpiGbl_FADT->Gpe0BlkLen);
    Gpe1RegisterCount           = (UINT16) DIV_2 (AcpiGbl_FADT->Gpe1BlkLen);
    AcpiGbl_GpeRegisterCount    = Gpe0RegisterCount + Gpe1RegisterCount;

    if (!AcpiGbl_GpeRegisterCount)
    {
        REPORT_WARNING (("Zero GPEs are defined in the FADT\n"));
        return_ACPI_STATUS (AE_OK);
    }

    /*
     * Allocate the Gpe information block
     */

    AcpiGbl_GpeRegisters = AcpiUtCallocate (AcpiGbl_GpeRegisterCount *
                            sizeof (ACPI_GPE_REGISTERS));
    if (!AcpiGbl_GpeRegisters)
    {
        DEBUG_PRINTP (ACPI_ERROR,
            ("Could not allocate the GpeRegisters block\n"));
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    /*
     * Allocate the Gpe dispatch handler block
     * There are eight distinct GP events per register.
     * Initialization to zeros is sufficient
     */

    AcpiGbl_GpeInfo = AcpiUtCallocate (MUL_8 (AcpiGbl_GpeRegisterCount) *
                                        sizeof (ACPI_GPE_LEVEL_INFO));
    if (!AcpiGbl_GpeInfo)
    {
        AcpiUtFree (AcpiGbl_GpeRegisters);
        DEBUG_PRINTP (ACPI_ERROR, ("Could not allocate the GpeInfo block\n"));
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    /* Set the Gpe validation table to GPE_INVALID */

    MEMSET (AcpiGbl_GpeValid, (int) ACPI_GPE_INVALID, ACPI_NUM_GPE);

    /*
     * Initialize the Gpe information and validation blocks.  A goal of these
     * blocks is to hide the fact that there are two separate GPE register sets
     * In a given block, the status registers occupy the first half, and
     * the enable registers occupy the second half.
     */

    /* GPE Block 0 */

    RegisterIndex = 0;

    for (i = 0; i < Gpe0RegisterCount; i++)
    {
        AcpiGbl_GpeRegisters[RegisterIndex].StatusAddr  =
                    (UINT16) (ACPI_GET_ADDRESS (AcpiGbl_FADT->XGpe0Blk.Address) + i);

        AcpiGbl_GpeRegisters[RegisterIndex].EnableAddr  =
                    (UINT16) (ACPI_GET_ADDRESS (AcpiGbl_FADT->XGpe0Blk.Address) + i + Gpe0RegisterCount);

        AcpiGbl_GpeRegisters[RegisterIndex].GpeBase     = (UINT8) MUL_8 (i);

        for (j = 0; j < 8; j++)
        {
            GpeNumber = AcpiGbl_GpeRegisters[RegisterIndex].GpeBase + j;
            AcpiGbl_GpeValid[GpeNumber] = (UINT8) RegisterIndex;
        }

        /*
         * Clear the status/enable registers.  Note that status registers
         * are cleared by writing a '1', while enable registers are cleared
         * by writing a '0'.
         */
        AcpiOsOut8 (AcpiGbl_GpeRegisters[RegisterIndex].EnableAddr, 0x00);
        AcpiOsOut8 (AcpiGbl_GpeRegisters[RegisterIndex].StatusAddr, 0xFF);

        RegisterIndex++;
    }

    /* GPE Block 1 */

    for (i = 0; i < Gpe1RegisterCount; i++)
    {
        AcpiGbl_GpeRegisters[RegisterIndex].StatusAddr  =
                    (UINT16) (ACPI_GET_ADDRESS (AcpiGbl_FADT->XGpe1Blk.Address) + i);

        AcpiGbl_GpeRegisters[RegisterIndex].EnableAddr  =
                    (UINT16) (ACPI_GET_ADDRESS (AcpiGbl_FADT->XGpe1Blk.Address) + i + Gpe1RegisterCount);

        AcpiGbl_GpeRegisters[RegisterIndex].GpeBase     =
                    (UINT8) (AcpiGbl_FADT->Gpe1Base + MUL_8 (i));

        for (j = 0; j < 8; j++)
        {
            GpeNumber = AcpiGbl_GpeRegisters[RegisterIndex].GpeBase + j;
            AcpiGbl_GpeValid[GpeNumber] = (UINT8) RegisterIndex;
        }

        /*
         * Clear the status/enable registers.  Note that status registers
         * are cleared by writing a '1', while enable registers are cleared
         * by writing a '0'.
         */
        AcpiOsOut8 (AcpiGbl_GpeRegisters[RegisterIndex].EnableAddr, 0x00);
        AcpiOsOut8 (AcpiGbl_GpeRegisters[RegisterIndex].StatusAddr, 0xFF);

        RegisterIndex++;
    }

    DEBUG_PRINTP (ACPI_INFO, ("GPE registers: %X@%p (Blk0) %X@%p (Blk1)\n",
        Gpe0RegisterCount, AcpiGbl_FADT->XGpe0Blk.Address, Gpe1RegisterCount,
        AcpiGbl_FADT->XGpe1Blk.Address));

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvSaveMethodInfo
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Called from AcpiWalkNamespace.  Expects each object to be a
 *              control method under the _GPE portion of the namespace.
 *              Extract the name and GPE type from the object, saving this
 *              information for quick lookup during GPE dispatch
 *
 *              The name of each GPE control method is of the form:
 *                  "_Lnn" or "_Enn"
 *              Where:
 *                  L      - means that the GPE is level triggered
 *                  E      - means that the GPE is edge triggered
 *                  nn     - is the GPE number
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiEvSaveMethodInfo (
    ACPI_HANDLE             ObjHandle,
    UINT32                  Level,
    void                    *ObjDesc,
    void                    **ReturnValue)
{
    UINT32                  GpeNumber;
    NATIVE_CHAR             Name[ACPI_NAME_SIZE + 1];
    UINT8                   Type;


    PROC_NAME ("EvSaveMethodInfo");


    /* Extract the name from the object and convert to a string */

    MOVE_UNALIGNED32_TO_32 (Name, &((ACPI_NAMESPACE_NODE *) ObjHandle)->Name);
    Name[ACPI_NAME_SIZE] = 0;

    /*
     * Edge/Level determination is based on the 2nd INT8 of the method name
     */
    if (Name[1] == 'L')
    {
        Type = ACPI_EVENT_LEVEL_TRIGGERED;
    }
    else if (Name[1] == 'E')
    {
        Type = ACPI_EVENT_EDGE_TRIGGERED;
    }
    else
    {
        /* Unknown method type, just ignore it! */

        DEBUG_PRINTP (ACPI_ERROR,
            ("Unknown GPE method type: %s (name not of form _Lnn or _Enn)\n",
            Name));
        return (AE_OK);
    }

    /* Convert the last two characters of the name to the Gpe Number */

    GpeNumber = STRTOUL (&Name[2], NULL, 16);
    if (GpeNumber == ACPI_UINT32_MAX)
    {
        /* Conversion failed; invalid method, just ignore it */

        DEBUG_PRINTP (ACPI_ERROR,
            ("Could not extract GPE number from name: %s (name not of form _Lnn or _Enn)\n",
            Name));
        return (AE_OK);
    }

    /* Ensure that we have a valid GPE number */

    if (AcpiGbl_GpeValid[GpeNumber] == ACPI_GPE_INVALID)
    {
        /* Not valid, all we can do here is ignore it */

        return (AE_OK);
    }

    /*
     * Now we can add this information to the GpeInfo block
     * for use during dispatch of this GPE.
     */

    AcpiGbl_GpeInfo [GpeNumber].Type            = Type;
    AcpiGbl_GpeInfo [GpeNumber].MethodHandle    = ObjHandle;


    /*
     * Enable the GPE (SCIs should be disabled at this point)
     */

    AcpiHwEnableGpe (GpeNumber);

    DEBUG_PRINTP (ACPI_INFO, ("Registered GPE method %s as GPE number %X\n",
        Name, GpeNumber));
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvInitGpeControlMethods
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Obtain the control methods associated with the GPEs.
 *
 *              NOTE: Must be called AFTER namespace initialization!
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEvInitGpeControlMethods (void)
{
    ACPI_STATUS             Status;


    FUNCTION_TRACE ("EvInitGpeControlMethods");


    /* Get a permanent handle to the _GPE object */

    Status = AcpiGetHandle (NULL, "\\_GPE", &AcpiGbl_GpeObjHandle);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Traverse the namespace under \_GPE to find all methods there */

    Status = AcpiWalkNamespace (ACPI_TYPE_METHOD, AcpiGbl_GpeObjHandle,
                                ACPI_UINT32_MAX, AcpiEvSaveMethodInfo,
                                NULL, NULL);

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvGpeDetect
 *
 * PARAMETERS:  None
 *
 * RETURN:      INTERRUPT_HANDLED or INTERRUPT_NOT_HANDLED
 *
 * DESCRIPTION: Detect if any GP events have occurred
 *
 ******************************************************************************/

UINT32
AcpiEvGpeDetect (void)
{
    UINT32                  IntStatus = INTERRUPT_NOT_HANDLED;
    UINT32                  i;
    UINT32                  j;
    UINT8                   EnabledStatusByte;
    UINT8                   BitMask;


    /*
     * Read all of the 8-bit GPE status and enable registers
     * in both of the register blocks, saving all of it.
     * Find all currently active GP events.
     */

    for (i = 0; i < AcpiGbl_GpeRegisterCount; i++)
    {
        AcpiGbl_GpeRegisters[i].Status =
                            AcpiOsIn8 (AcpiGbl_GpeRegisters[i].StatusAddr);

        AcpiGbl_GpeRegisters[i].Enable =
                            AcpiOsIn8 (AcpiGbl_GpeRegisters[i].EnableAddr);

        DEBUG_PRINT (TRACE_INTERRUPTS,
            ("GPE block at %X - Enable %08X Status %08X\n",
            AcpiGbl_GpeRegisters[i].EnableAddr, 
            AcpiGbl_GpeRegisters[i].Status, 
            AcpiGbl_GpeRegisters[i].Enable));

        /* First check if there is anything active at all in this register */

        EnabledStatusByte = (UINT8) (AcpiGbl_GpeRegisters[i].Status &
                                    AcpiGbl_GpeRegisters[i].Enable);

        if (!EnabledStatusByte)
        {
            /* No active GPEs in this register, move on */

            continue;
        }

        /* Now look at the individual GPEs in this byte register */

        for (j = 0, BitMask = 1; j < 8; j++, BitMask <<= 1)
        {
            /* Examine one GPE bit */

            if (EnabledStatusByte & BitMask)
            {
                /*
                 * Found an active GPE.  Dispatch the event to a handler
                 * or method.
                 */
                IntStatus |=
                    AcpiEvGpeDispatch (AcpiGbl_GpeRegisters[i].GpeBase + j);
            }
        }
    }

    return (IntStatus);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvAsynchExecuteGpeMethod
 *
 * PARAMETERS:  GpeNumber       - The 0-based Gpe number
 *
 * RETURN:      None
 *
 * DESCRIPTION: Perform the actual execution of a GPE control method.  This
 *              function is called from an invocation of AcpiOsQueueForExecution
 *              (and therefore does NOT execute at interrupt level) so that
 *              the control method itself is not executed in the context of
 *              the SCI interrupt handler.
 *
 ******************************************************************************/

static void
AcpiEvAsynchExecuteGpeMethod (
    void                    *Context)
{
    UINT32                  GpeNumber = (UINT32) Context;
    ACPI_GPE_LEVEL_INFO     GpeInfo;


    FUNCTION_TRACE ("EvAsynchExecuteGpeMethod");

    /*
     * Take a snapshot of the GPE info for this level
     */
    AcpiUtAcquireMutex (ACPI_MTX_EVENTS);
    GpeInfo = AcpiGbl_GpeInfo [GpeNumber];
    AcpiUtReleaseMutex (ACPI_MTX_EVENTS);

    /*
     * Method Handler (_Lxx, _Exx):
     * ----------------------------
     * Evaluate the _Lxx/_Exx control method that corresponds to this GPE.
     */
    if (GpeInfo.MethodHandle)
    {
        AcpiNsEvaluateByHandle (GpeInfo.MethodHandle, NULL, NULL);
    }

    /*
     * Level-Triggered?
     * ----------------
     * If level-triggered we clear the GPE status bit after handling the event.
     */
    if (GpeInfo.Type & ACPI_EVENT_LEVEL_TRIGGERED)
    {
        AcpiHwClearGpe (GpeNumber);
    }

    /*
     * Enable the GPE.
     */
    AcpiHwEnableGpe (GpeNumber);

    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvGpeDispatch
 *
 * PARAMETERS:  GpeNumber       - The 0-based Gpe number
 *
 * RETURN:      INTERRUPT_HANDLED or INTERRUPT_NOT_HANDLED
 *
 * DESCRIPTION: Handle and dispatch a General Purpose AcpiEvent.
 *              Clears the status bit for the requested event.
 *
 * TBD: [Investigate] is this still valid or necessary:
 * The Gpe handler differs from the fixed events in that it clears the enable
 * bit rather than the status bit to clear the interrupt.  This allows
 * software outside of interrupt context to determine what caused the SCI and
 * dispatch the correct AML.
 *
 ******************************************************************************/

UINT32
AcpiEvGpeDispatch (
    UINT32                  GpeNumber)
{
    ACPI_GPE_LEVEL_INFO     GpeInfo;

    FUNCTION_TRACE ("EvGpeDispatch");


    /*
     * Valid GPE number?
     */
    if (AcpiGbl_GpeValid[GpeNumber] == ACPI_GPE_INVALID)
    {
        DEBUG_PRINTP (ACPI_ERROR, ("Invalid GPE bit [%X].\n", GpeNumber));
        return_VALUE (INTERRUPT_NOT_HANDLED);
    }

    /*
     * Disable the GPE.
     */
    AcpiHwDisableGpe (GpeNumber);

    GpeInfo = AcpiGbl_GpeInfo [GpeNumber];

    /*
     * Edge-Triggered?
     * ---------------
     * If edge-triggered, clear the GPE status bit now.  Note that
     * level-triggered events are cleared after the GPE is serviced.
     */
    if (GpeInfo.Type & ACPI_EVENT_EDGE_TRIGGERED)
    {
        AcpiHwClearGpe (GpeNumber);
    }
        /*
         * Function Handler (e.g. EC)?
         */
    if (GpeInfo.Handler)
    {
        /* Invoke function handler (at interrupt level). */
        GpeInfo.Handler (GpeInfo.Context);

        /* Level-Triggered? */
        if (GpeInfo.Type & ACPI_EVENT_LEVEL_TRIGGERED)
        {
            AcpiHwClearGpe (GpeNumber);
        }

        /* Enable GPE */
        AcpiHwEnableGpe (GpeNumber);
    }
    /*
     * Method Handler (e.g. _Exx/_Lxx)?
     */
    else if (GpeInfo.MethodHandle)
    {
        if (ACPI_FAILURE(AcpiOsQueueForExecution (OSD_PRIORITY_GPE,
            AcpiEvAsynchExecuteGpeMethod, (void*) GpeNumber)))
        {
            /*
             * Shoudn't occur, but if it does report an error. Note that
             * the GPE will remain disabled until the ACPI Core Subsystem
             * is restarted, or the handler is removed/reinstalled.
             */
            REPORT_ERROR (("AcpiEvGpeDispatch: Unable to queue handler for GPE bit [%X]\n", GpeNumber));
        }
    }
    /*
     * No Handler? Report an error and leave the GPE disabled.
     */
    else
    {
        REPORT_ERROR (("AcpiEvGpeDispatch: No installed handler for GPE [%X]\n", GpeNumber));

        /* Level-Triggered? */
        if (GpeInfo.Type & ACPI_EVENT_LEVEL_TRIGGERED)
        {
            AcpiHwClearGpe (GpeNumber);
        }
    }

    return_VALUE (INTERRUPT_HANDLED);
}
