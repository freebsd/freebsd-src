/******************************************************************************
 *
 * Module Name: evevent - Fixed and General Purpose Even handling and dispatch
 *              $Revision: 69 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2002, Intel Corp.
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
        ACPI_MODULE_NAME    ("evevent")


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvInitialize
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Initialize global data structures for events.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEvInitialize (
    void)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("EvInitialize");


    /* Make sure we have ACPI tables */

    if (!AcpiGbl_DSDT)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_WARN, "No ACPI tables present!\n"));
        return_ACPI_STATUS (AE_NO_ACPI_TABLES);
    }

    /*
     * Initialize the Fixed and General Purpose AcpiEvents prior.  This is
     * done prior to enabling SCIs to prevent interrupts from occuring
     * before handers are installed.
     */
    Status = AcpiEvFixedEventInitialize ();
    if (ACPI_FAILURE (Status))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_FATAL,
                "Unable to initialize fixed events, %s\n",
                AcpiFormatException (Status)));
        return_ACPI_STATUS (Status);
    }

    Status = AcpiEvGpeInitialize ();
    if (ACPI_FAILURE (Status))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_FATAL,
                "Unable to initialize general purpose events, %s\n",
                AcpiFormatException (Status)));
        return_ACPI_STATUS (Status);
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvHandlerInitialize
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Install handlers for the SCI, Global Lock, and GPEs.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEvHandlerInitialize (
    void)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("EvInitialize");


    /* Install the SCI handler */

    Status = AcpiEvInstallSciHandler ();
    if (ACPI_FAILURE (Status))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_FATAL,
                "Unable to install System Control Interrupt Handler, %s\n",
                AcpiFormatException (Status)));
        return_ACPI_STATUS (Status);
    }

    /* Install handlers for control method GPE handlers (_Lxx, _Exx) */

    Status = AcpiEvInitGpeControlMethods ();
    if (ACPI_FAILURE (Status))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_FATAL,
                "Unable to initialize GPE control methods, %s\n",
                AcpiFormatException (Status)));
        return_ACPI_STATUS (Status);
    }

    /* Install the handler for the Global Lock */

    Status = AcpiEvInitGlobalLockHandler ();
    if (ACPI_FAILURE (Status))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_FATAL,
                "Unable to initialize Global Lock handler, %s\n",
                AcpiFormatException (Status)));
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
 * DESCRIPTION: Install the fixed event handlers and enable the fixed events.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEvFixedEventInitialize (
    void)
{
    NATIVE_UINT             i;


    /*
     * Initialize the structure that keeps track of fixed event handlers
     * and enable the fixed events.
     */
    for (i = 0; i < ACPI_NUM_FIXED_EVENTS; i++)
    {
        AcpiGbl_FixedEventHandlers[i].Handler = NULL;
        AcpiGbl_FixedEventHandlers[i].Context = NULL;

        /* Enable the fixed event */

        if (AcpiGbl_FixedEventInfo[i].EnableRegisterId != 0xFF)
        {
            AcpiHwBitRegisterWrite (AcpiGbl_FixedEventInfo[i].EnableRegisterId,
                                    0, ACPI_MTX_LOCK);
        }
    }

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
AcpiEvFixedEventDetect (
    void)
{
    UINT32                  IntStatus = ACPI_INTERRUPT_NOT_HANDLED;
    UINT32                  GpeStatus;
    UINT32                  GpeEnable;
    NATIVE_UINT             i;


    ACPI_FUNCTION_NAME ("EvFixedEventDetect");


    /*
     * Read the fixed feature status and enable registers, as all the cases
     * depend on their values.
     */
    GpeStatus = AcpiHwRegisterRead (ACPI_MTX_DO_NOT_LOCK, ACPI_REGISTER_PM1_STATUS);
    GpeEnable = AcpiHwRegisterRead (ACPI_MTX_DO_NOT_LOCK, ACPI_REGISTER_PM1_ENABLE);

    ACPI_DEBUG_PRINT ((ACPI_DB_INTERRUPTS,
        "Fixed AcpiEvent Block: Enable %08X Status %08X\n",
        GpeEnable, GpeStatus));

    /*
     * Check for all possible Fixed Events and dispatch those that are active
     */
    for (i = 0; i < ACPI_NUM_FIXED_EVENTS; i++)
    {
        /* Both the status and enable bits must be on for this event */

        if ((GpeStatus & AcpiGbl_FixedEventInfo[i].StatusBitMask) &&
            (GpeEnable & AcpiGbl_FixedEventInfo[i].EnableBitMask))
        {
            /* Found an active (signalled) event */

            IntStatus |= AcpiEvFixedEventDispatch (i);
        }
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


    ACPI_FUNCTION_ENTRY ();


    /* Clear the status bit */

    AcpiHwBitRegisterWrite (AcpiGbl_FixedEventInfo[Event].StatusRegisterId,
                1, ACPI_MTX_DO_NOT_LOCK);

    /*
     * Make sure we've got a handler.  If not, report an error.
     * The event is disabled to prevent further interrupts.
     */
    if (NULL == AcpiGbl_FixedEventHandlers[Event].Handler)
    {
        AcpiHwBitRegisterWrite (AcpiGbl_FixedEventInfo[Event].EnableRegisterId,
                0, ACPI_MTX_DO_NOT_LOCK);

        ACPI_REPORT_ERROR (
            ("EvGpeDispatch: No installed handler for fixed event [%08X]\n",
            Event));

        return (ACPI_INTERRUPT_NOT_HANDLED);
    }

    /* Invoke the Fixed Event handler */

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
    NATIVE_UINT             i;
    NATIVE_UINT             j;
    NATIVE_UINT             GpeBlock;
    UINT32                  GpeRegister;
    UINT32                  GpeNumberIndex;
    UINT32                  GpeNumber;
    ACPI_GPE_REGISTER_INFO  *GpeRegisterInfo;


    ACPI_FUNCTION_TRACE ("EvGpeInitialize");


    /*
     * Initialize the GPE Block globals
     *
     * Why the GPE register block lengths divided by 2:  From the ACPI Spec,
     * section "General-Purpose Event Registers", we have:
     *
     * "Each register block contains two registers of equal length
     *  GPEx_STS and GPEx_EN (where x is 0 or 1). The length of the
     *  GPE0_STS and GPE0_EN registers is equal to half the GPE0_LEN
     *  The length of the GPE1_STS and GPE1_EN registers is equal to
     *  half the GPE1_LEN. If a generic register block is not supported
     *  then its respective block pointer and block length values in the
     *  FADT table contain zeros. The GPE0_LEN and GPE1_LEN do not need
     *  to be the same size."
     */
    AcpiGbl_GpeBlockInfo[0].RegisterCount   = (UINT16) ACPI_DIV_2 (AcpiGbl_FADT->Gpe0BlkLen);
    AcpiGbl_GpeBlockInfo[1].RegisterCount   = (UINT16) ACPI_DIV_2 (AcpiGbl_FADT->Gpe1BlkLen);

    AcpiGbl_GpeBlockInfo[0].BlockAddress    = (UINT16) ACPI_GET_ADDRESS (AcpiGbl_FADT->XGpe0Blk.Address);
    AcpiGbl_GpeBlockInfo[1].BlockAddress    = (UINT16) ACPI_GET_ADDRESS (AcpiGbl_FADT->XGpe1Blk.Address);

    AcpiGbl_GpeBlockInfo[0].BlockBaseNumber = 0;
    AcpiGbl_GpeBlockInfo[1].BlockBaseNumber = AcpiGbl_FADT->Gpe1Base;

    AcpiGbl_GpeRegisterCount = AcpiGbl_GpeBlockInfo[0].RegisterCount +
                               AcpiGbl_GpeBlockInfo[1].RegisterCount;
    if (!AcpiGbl_GpeRegisterCount)
    {
        ACPI_REPORT_WARNING (("Zero GPEs are defined in the FADT\n"));
        return_ACPI_STATUS (AE_OK);
    }

    /* Determine the maximum GPE number for this machine */

    AcpiGbl_GpeNumberMax = ACPI_MUL_8 (AcpiGbl_GpeBlockInfo[0].RegisterCount) - 1;

    if (AcpiGbl_GpeBlockInfo[1].RegisterCount)
    {
        /* Check for GPE0/GPE1 overlap */

        if (AcpiGbl_GpeNumberMax >= AcpiGbl_FADT->Gpe1Base)
        {
            ACPI_REPORT_ERROR (("GPE0 block overlaps the GPE1 block\n"));
            return_ACPI_STATUS (AE_BAD_VALUE);
        }

        /* GPE0 and GPE1 do not have to be contiguous in the GPE number space */

        AcpiGbl_GpeNumberMax = AcpiGbl_FADT->Gpe1Base + (ACPI_MUL_8 (AcpiGbl_GpeBlockInfo[1].RegisterCount) - 1);
    }

    /* Check for Max GPE number out-of-range */

    if (AcpiGbl_GpeNumberMax > ACPI_GPE_MAX)
    {
        ACPI_REPORT_ERROR (("Maximum GPE number from FADT is too large: 0x%X\n", AcpiGbl_GpeNumberMax));
        return_ACPI_STATUS (AE_BAD_VALUE);
    }

    /*
     * Allocate the GPE number-to-index translation table
     */
    AcpiGbl_GpeNumberToIndex = ACPI_MEM_CALLOCATE (sizeof (ACPI_GPE_INDEX_INFO) *
                                    (AcpiGbl_GpeNumberMax + 1));
    if (!AcpiGbl_GpeNumberToIndex)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
            "Could not allocate the GpeNumberToIndex table\n"));
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    /* Set the Gpe index table to GPE_INVALID */

    ACPI_MEMSET (AcpiGbl_GpeNumberToIndex, (int) ACPI_GPE_INVALID,
            sizeof (ACPI_GPE_INDEX_INFO) * (AcpiGbl_GpeNumberMax + 1));

    /*
     * Allocate the GPE register information block
     */
    AcpiGbl_GpeRegisterInfo = ACPI_MEM_CALLOCATE (AcpiGbl_GpeRegisterCount *
                                sizeof (ACPI_GPE_REGISTER_INFO));
    if (!AcpiGbl_GpeRegisterInfo)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
            "Could not allocate the GpeRegisterInfo table\n"));
        goto ErrorExit1;
    }

    /*
     * Allocate the GPE dispatch handler block.  There are eight distinct GPEs
     * per register.  Initialization to zeros is sufficient.
     */
    AcpiGbl_GpeNumberInfo = ACPI_MEM_CALLOCATE (ACPI_MUL_8 (AcpiGbl_GpeRegisterCount) *
                                            sizeof (ACPI_GPE_NUMBER_INFO));
    if (!AcpiGbl_GpeNumberInfo)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Could not allocate the GpeNumberInfo table\n"));
        goto ErrorExit2;
    }

    /*
     * Initialize the GPE information and validation tables.  A goal of these
     * tables is to hide the fact that there are two separate GPE register sets
     * in a given gpe hardware block, the status registers occupy the first half,
     * and the enable registers occupy the second half.  Another goal is to hide
     * the fact that there may be multiple GPE hardware blocks.
     */
    GpeRegister = 0;
    GpeNumberIndex = 0;

    for (GpeBlock = 0; GpeBlock < ACPI_MAX_GPE_BLOCKS; GpeBlock++)
    {
        for (i = 0; i < AcpiGbl_GpeBlockInfo[GpeBlock].RegisterCount; i++)
        {
            GpeRegisterInfo = &AcpiGbl_GpeRegisterInfo[GpeRegister];

            /* Init the Register info for this entire GPE register (8 GPEs) */

            GpeRegisterInfo->BaseGpeNumber = (UINT8)  (AcpiGbl_GpeBlockInfo[GpeBlock].BlockBaseNumber + (ACPI_MUL_8 (i)));
            GpeRegisterInfo->StatusAddr    = (UINT16) (AcpiGbl_GpeBlockInfo[GpeBlock].BlockAddress + i);
            GpeRegisterInfo->EnableAddr    = (UINT16) (AcpiGbl_GpeBlockInfo[GpeBlock].BlockAddress + i +
                                                       AcpiGbl_GpeBlockInfo[GpeBlock].RegisterCount);

            /* Init the Index mapping info for each GPE number within this register */

            for (j = 0; j < 8; j++)
            {
                GpeNumber = GpeRegisterInfo->BaseGpeNumber + j;
                AcpiGbl_GpeNumberToIndex[GpeNumber].NumberIndex = (UINT8) GpeNumberIndex;

                AcpiGbl_GpeNumberInfo[GpeNumberIndex].BitMask = AcpiGbl_DecodeTo8bit[i];
                GpeNumberIndex++;
            }

            /*
             * Clear the status/enable registers.  Note that status registers
             * are cleared by writing a '1', while enable registers are cleared
             * by writing a '0'.
             */
            AcpiOsWritePort (GpeRegisterInfo->EnableAddr, 0x00, 8);
            AcpiOsWritePort (GpeRegisterInfo->StatusAddr, 0xFF, 8);

            GpeRegister++;
        }
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "GPE registers: %X@%8.8X%8.8X (Blk0) %X@%8.8X%8.8X (Blk1)\n",
        AcpiGbl_GpeBlockInfo[0].RegisterCount,
        ACPI_HIDWORD (AcpiGbl_FADT->XGpe0Blk.Address), ACPI_LODWORD (AcpiGbl_FADT->XGpe0Blk.Address),
        AcpiGbl_GpeBlockInfo[1].RegisterCount,
        ACPI_HIDWORD (AcpiGbl_FADT->XGpe1Blk.Address), ACPI_LODWORD (AcpiGbl_FADT->XGpe1Blk.Address)));

    return_ACPI_STATUS (AE_OK);


    /* Error cleanup */

ErrorExit2:
    ACPI_MEM_FREE (AcpiGbl_GpeRegisterInfo);

ErrorExit1:
    ACPI_MEM_FREE (AcpiGbl_GpeNumberToIndex);
    return_ACPI_STATUS (AE_NO_MEMORY);
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
 *                  nn     - is the GPE number [in HEX]
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
    UINT32                  GpeNumberIndex;
    NATIVE_CHAR             Name[ACPI_NAME_SIZE + 1];
    UINT8                   Type;


    ACPI_FUNCTION_NAME ("EvSaveMethodInfo");


    /* Extract the name from the object and convert to a string */

    ACPI_MOVE_UNALIGNED32_TO_32 (Name,
                                &((ACPI_NAMESPACE_NODE *) ObjHandle)->Name);
    Name[ACPI_NAME_SIZE] = 0;

    /*
     * Edge/Level determination is based on the 2nd character of the method name
     */
    switch (Name[1])
    {
    case 'L':
        Type = ACPI_EVENT_LEVEL_TRIGGERED;
        break;

    case 'E':
        Type = ACPI_EVENT_EDGE_TRIGGERED;
        break;

    default:
        /* Unknown method type, just ignore it! */

        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
            "Unknown GPE method type: %s (name not of form _Lnn or _Enn)\n",
            Name));
        return (AE_OK);
    }

    /* Convert the last two characters of the name to the GPE Number */

    GpeNumber = ACPI_STRTOUL (&Name[2], NULL, 16);
    if (GpeNumber == ACPI_UINT32_MAX)
    {
        /* Conversion failed; invalid method, just ignore it */

        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
            "Could not extract GPE number from name: %s (name not of form _Lnn or _Enn)\n",
            Name));
        return (AE_OK);
    }

    /* Get GPE index and ensure that we have a valid GPE number */

    GpeNumberIndex = AcpiEvGetGpeNumberIndex (GpeNumber);
    if (GpeNumberIndex == ACPI_GPE_INVALID)
    {
        /* Not valid, all we can do here is ignore it */

        return (AE_OK);
    }

    /*
     * Now we can add this information to the GpeInfo block
     * for use during dispatch of this GPE.
     */
    AcpiGbl_GpeNumberInfo [GpeNumberIndex].Type         = Type;
    AcpiGbl_GpeNumberInfo [GpeNumberIndex].MethodHandle = ObjHandle;

    /*
     * Enable the GPE (SCIs should be disabled at this point)
     */
    AcpiHwEnableGpe (GpeNumber);

    ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "Registered GPE method %s as GPE number %X\n",
        Name, GpeNumber));
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvInitGpeControlMethods
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Obtain the control methods associated with the GPEs.
 *              NOTE: Must be called AFTER namespace initialization!
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEvInitGpeControlMethods (void)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("EvInitGpeControlMethods");


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
 * DESCRIPTION: Detect if any GP events have occurred.  This function is
 *              executed at interrupt level.
 *
 ******************************************************************************/

UINT32
AcpiEvGpeDetect (void)
{
    UINT32                  IntStatus = ACPI_INTERRUPT_NOT_HANDLED;
    UINT32                  i;
    UINT32                  j;
    UINT8                   EnabledStatusByte;
    UINT8                   BitMask;
    ACPI_GPE_REGISTER_INFO  *GpeRegisterInfo;


    ACPI_FUNCTION_NAME ("EvGpeDetect");


    /*
     * Read all of the 8-bit GPE status and enable registers
     * in both of the register blocks, saving all of it.
     * Find all currently active GP events.
     */
    for (i = 0; i < AcpiGbl_GpeRegisterCount; i++)
    {
        GpeRegisterInfo = &AcpiGbl_GpeRegisterInfo[i];

        AcpiOsReadPort (GpeRegisterInfo->StatusAddr,
                        &GpeRegisterInfo->Status, 8);

        AcpiOsReadPort (GpeRegisterInfo->EnableAddr,
                        &GpeRegisterInfo->Enable, 8);

        ACPI_DEBUG_PRINT ((ACPI_DB_INTERRUPTS,
            "GPE block at %X - Enable %08X Status %08X\n",
            GpeRegisterInfo->EnableAddr,
            GpeRegisterInfo->Status,
            GpeRegisterInfo->Enable));

        /* First check if there is anything active at all in this register */

        EnabledStatusByte = (UINT8) (GpeRegisterInfo->Status &
                                     GpeRegisterInfo->Enable);
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
                IntStatus |= AcpiEvGpeDispatch (
                                GpeRegisterInfo->BaseGpeNumber + j);
            }
        }
    }

    return (IntStatus);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvAsynchExecuteGpeMethod
 *
 * PARAMETERS:  GpeNumber       - The 0-based GPE number
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
    UINT32                  GpeNumber = (UINT32) ACPI_TO_INTEGER (Context);
    UINT32                  GpeNumberIndex;
    ACPI_GPE_NUMBER_INFO    GpeInfo;


    ACPI_FUNCTION_TRACE ("EvAsynchExecuteGpeMethod");


    GpeNumberIndex = AcpiEvGetGpeNumberIndex (GpeNumber);
    if (GpeNumberIndex == ACPI_GPE_INVALID)
    {
        return_VOID;
    }

    /*
     * Take a snapshot of the GPE info for this level - we copy the
     * info to prevent a race condition with RemoveHandler.
     */
    if (ACPI_FAILURE (AcpiUtAcquireMutex (ACPI_MTX_EVENTS)))
    {
        return_VOID;
    }

    GpeInfo = AcpiGbl_GpeNumberInfo [GpeNumberIndex];
    if (ACPI_FAILURE (AcpiUtReleaseMutex (ACPI_MTX_EVENTS)))
    {
        return_VOID;
    }

    if (GpeInfo.MethodHandle)
    {
        /*
         * Invoke the GPE Method (_Lxx, _Exx):
         * (Evaluate the _Lxx/_Exx control method that corresponds to this GPE.)
         */
        AcpiNsEvaluateByHandle (GpeInfo.MethodHandle, NULL, NULL);
    }

    if (GpeInfo.Type & ACPI_EVENT_LEVEL_TRIGGERED)
    {
        /*
         * GPE is level-triggered, we clear the GPE status bit after handling
         * the event.
         */
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
 * PARAMETERS:  GpeNumber       - The 0-based GPE number
 *
 * RETURN:      INTERRUPT_HANDLED or INTERRUPT_NOT_HANDLED
 *
 * DESCRIPTION: Dispatch a General Purpose Event to either a function (e.g. EC)
 *              or method (e.g. _Lxx/_Exx) handler.  This function executes
 *              at interrupt level.
 *
 ******************************************************************************/

UINT32
AcpiEvGpeDispatch (
    UINT32                  GpeNumber)
{
    UINT32                  GpeNumberIndex;
    ACPI_GPE_NUMBER_INFO    *GpeInfo;


    ACPI_FUNCTION_TRACE ("EvGpeDispatch");


    GpeNumberIndex = AcpiEvGetGpeNumberIndex (GpeNumber);
    if (GpeNumberIndex == ACPI_GPE_INVALID)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Invalid event, GPE[%X].\n", GpeNumber));
        return_VALUE (ACPI_INTERRUPT_NOT_HANDLED);
    }

    /*
     * We don't have to worry about mutex on GpeInfo because we are
     * executing at interrupt level.
     */
    GpeInfo = &AcpiGbl_GpeNumberInfo [GpeNumberIndex];

    /*
     * If edge-triggered, clear the GPE status bit now.  Note that
     * level-triggered events are cleared after the GPE is serviced.
     */
    if (GpeInfo->Type & ACPI_EVENT_EDGE_TRIGGERED)
    {
        AcpiHwClearGpe (GpeNumber);
    }

    /*
     * Dispatch the GPE to either an installed handler, or the control
     * method associated with this GPE (_Lxx or _Exx).
     * If a handler exists, we invoke it and do not attempt to run the method.
     * If there is neither a handler nor a method, we disable the level to
     * prevent further events from coming in here.
     */
    if (GpeInfo->Handler)
    {
        /* Invoke the installed handler (at interrupt level) */

        GpeInfo->Handler (GpeInfo->Context);
    }
    else if (GpeInfo->MethodHandle)
    {
        /*
         * Execute the method associated with the GPE.
         */
        if (ACPI_FAILURE (AcpiOsQueueForExecution (OSD_PRIORITY_GPE,
                                AcpiEvAsynchExecuteGpeMethod,
                                ACPI_TO_POINTER (GpeNumber))))
        {
            ACPI_REPORT_ERROR (("AcpiEvGpeDispatch: Unable to queue handler for GPE[%X], disabling event\n", GpeNumber));

            /*
             * Disable the GPE on error.  The GPE will remain disabled until the ACPI
             * Core Subsystem is restarted, or the handler is reinstalled.
             */
            AcpiHwDisableGpe (GpeNumber);
        }
    }
    else
    {
        /* No handler or method to run! */

        ACPI_REPORT_ERROR (("AcpiEvGpeDispatch: No handler or method for GPE[%X], disabling event\n", GpeNumber));

        /*
         * Disable the GPE.  The GPE will remain disabled until the ACPI
         * Core Subsystem is restarted, or the handler is reinstalled.
         */
        AcpiHwDisableGpe (GpeNumber);
    }

    /*
     * It is now safe to clear level-triggered evnets.
     */
    if (GpeInfo->Type & ACPI_EVENT_LEVEL_TRIGGERED)
    {
        AcpiHwClearGpe (GpeNumber);
    }

    return_VALUE (ACPI_INTERRUPT_HANDLED);
}
