/******************************************************************************
 *
 * Module Name: hwacpi - ACPI hardware functions - mode and timer
 *              $Revision: 31 $
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

#define __HWACPI_C__

#include "acpi.h"
#include "achware.h"


#define _COMPONENT          HARDWARE
        MODULE_NAME         ("hwacpi")


/******************************************************************************
 *
 * FUNCTION:    AcpiHwInitialize
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Initialize and validate various ACPI registers
 *
 ******************************************************************************/

ACPI_STATUS
AcpiHwInitialize (
    void)
{
    ACPI_STATUS             Status = AE_OK;
    UINT32                  Index;


    FUNCTION_TRACE ("HwInitialize");


    /* We must have the ACPI tables by the time we get here */

    if (!AcpiGbl_FADT)
    {
        AcpiGbl_RestoreAcpiChipset = FALSE;

        DEBUG_PRINT (ACPI_ERROR, ("HwInitialize: No FADT!\n"));

        return_ACPI_STATUS (AE_NO_ACPI_TABLES);
    }

    /* Must support *some* mode! */
/*
    if (!(SystemFlags & SYS_MODES_MASK))
    {
        RestoreAcpiChipset = FALSE;

        DEBUG_PRINT (ACPI_ERROR,
            ("CmHardwareInitialize: Supported modes uninitialized!\n"));
        return_ACPI_STATUS (AE_ERROR);
    }

*/


    switch (AcpiGbl_SystemFlags & SYS_MODES_MASK)
    {
        /* Identify current ACPI/legacy mode   */

    case (SYS_MODE_ACPI):

        AcpiGbl_OriginalMode = SYS_MODE_ACPI;
        DEBUG_PRINT (ACPI_INFO, ("System supports ACPI mode only.\n"));
        break;


    case (SYS_MODE_LEGACY):

        AcpiGbl_OriginalMode = SYS_MODE_LEGACY;
        DEBUG_PRINT (ACPI_INFO,
            ("Tables loaded from buffer, hardware assumed to support LEGACY mode only.\n"));
        break;


    case (SYS_MODE_ACPI | SYS_MODE_LEGACY):

        if (AcpiHwGetMode () == SYS_MODE_ACPI)
        {
            AcpiGbl_OriginalMode = SYS_MODE_ACPI;
        }
        else
        {
            AcpiGbl_OriginalMode = SYS_MODE_LEGACY;
        }

        DEBUG_PRINT (ACPI_INFO,
            ("System supports both ACPI and LEGACY modes.\n"));

        DEBUG_PRINT (ACPI_INFO,
            ("System is currently in %s mode.\n",
            (AcpiGbl_OriginalMode == SYS_MODE_ACPI) ? "ACPI" : "LEGACY"));
        break;
    }


    if (AcpiGbl_SystemFlags & SYS_MODE_ACPI)
    {
        /* Target system supports ACPI mode */

        /*
         * The purpose of this code is to save the initial state
         * of the ACPI event enable registers. An exit function will be
         * registered which will restore this state when the application
         * exits. The exit function will also clear all of the ACPI event
         * status bits prior to restoring the original mode.
         *
         * The location of the PM1aEvtBlk enable registers is defined as the
         * base of PM1aEvtBlk + DIV_2(PM1aEvtBlkLength). Since the spec further
         * fully defines the PM1aEvtBlk to be a total of 4 bytes, the offset
         * for the enable registers is always 2 from the base. It is hard
         * coded here. If this changes in the spec, this code will need to
         * be modified. The PM1bEvtBlk behaves as expected.
         */

        AcpiGbl_Pm1EnableRegisterSave = (UINT16) AcpiHwRegisterRead (ACPI_MTX_LOCK, PM1_EN);


        /*
         * The GPEs behave similarly, except that the length of the register
         * block is not fixed, so the buffer must be allocated with malloc
         */

        if (AcpiGbl_FADT->XGpe0Blk.Address && AcpiGbl_FADT->Gpe0BlkLen)
        {
            /* GPE0 specified in FADT  */

            AcpiGbl_Gpe0EnableRegisterSave =
                AcpiCmAllocate (DIV_2 (AcpiGbl_FADT->Gpe0BlkLen));
            if (!AcpiGbl_Gpe0EnableRegisterSave)
            {
                return_ACPI_STATUS (AE_NO_MEMORY);
            }

            /* Save state of GPE0 enable bits */

            for (Index = 0; Index < DIV_2 (AcpiGbl_FADT->Gpe0BlkLen); Index++)
            {
                AcpiGbl_Gpe0EnableRegisterSave[Index] =
                    (UINT8) AcpiHwRegisterRead (ACPI_MTX_LOCK, GPE0_EN_BLOCK | Index);
            }
        }

        else
        {
            AcpiGbl_Gpe0EnableRegisterSave = NULL;
        }

        if (AcpiGbl_FADT->XGpe1Blk.Address && AcpiGbl_FADT->Gpe1BlkLen)
        {
            /* GPE1 defined */

            AcpiGbl_Gpe1EnableRegisterSave =
                AcpiCmAllocate (DIV_2 (AcpiGbl_FADT->Gpe1BlkLen));
            if (!AcpiGbl_Gpe1EnableRegisterSave)
            {
                return_ACPI_STATUS (AE_NO_MEMORY);
            }

            /* save state of GPE1 enable bits */

            for (Index = 0; Index < DIV_2 (AcpiGbl_FADT->Gpe1BlkLen); Index++)
            {
                AcpiGbl_Gpe1EnableRegisterSave[Index] =
                    (UINT8) AcpiHwRegisterRead (ACPI_MTX_LOCK, GPE1_EN_BLOCK | Index);
            }
        }

        else
        {
            AcpiGbl_Gpe1EnableRegisterSave = NULL;
        }
    }

    return_ACPI_STATUS (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiHwSetMode
 *
 * PARAMETERS:  Mode            - SYS_MODE_ACPI or SYS_MODE_LEGACY
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Transitions the system into the requested mode or does nothing
 *              if the system is already in that mode.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiHwSetMode (
    UINT32                  Mode)
{

    ACPI_STATUS             Status = AE_ERROR;

    FUNCTION_TRACE ("HwSetMode");


    if (Mode == SYS_MODE_ACPI)
    {
        /* BIOS should have disabled ALL fixed and GP events */

        AcpiOsOut8 (AcpiGbl_FADT->SmiCmd, AcpiGbl_FADT->AcpiEnable);
        DEBUG_PRINT (ACPI_INFO, ("Attempting to enable ACPI mode\n"));
    }

    else if (Mode == SYS_MODE_LEGACY)
    {
        /*
         * BIOS should clear all fixed status bits and restore fixed event
         * enable bits to default
         */

        AcpiOsOut8 (AcpiGbl_FADT->SmiCmd, AcpiGbl_FADT->AcpiDisable);
        DEBUG_PRINT (ACPI_INFO,
                    ("Attempting to enable Legacy (non-ACPI) mode\n"));
    }

    if (AcpiHwGetMode () == Mode)
    {
        DEBUG_PRINT (ACPI_INFO, ("Mode %d successfully enabled\n", Mode));
        Status = AE_OK;
    }

    return_ACPI_STATUS (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiHwGetMode
 *
 * PARAMETERS:  none
 *
 * RETURN:      SYS_MODE_ACPI or SYS_MODE_LEGACY
 *
 * DESCRIPTION: Return current operating state of system.  Determined by
 *              querying the SCI_EN bit.
 *
 ******************************************************************************/

UINT32
AcpiHwGetMode (void)
{

    FUNCTION_TRACE ("HwGetMode");


    if (AcpiHwRegisterBitAccess (ACPI_READ, ACPI_MTX_LOCK, SCI_EN))
    {
        return_VALUE (SYS_MODE_ACPI);
    }
    else
    {
        return_VALUE (SYS_MODE_LEGACY);
    }
}

/******************************************************************************
 *
 * FUNCTION:    AcpiHwGetModeCapabilities
 *
 * PARAMETERS:  none
 *
 * RETURN:      logical OR of SYS_MODE_ACPI and SYS_MODE_LEGACY determined at initial
 *              system state.
 *
 * DESCRIPTION: Returns capablities of system
 *
 ******************************************************************************/

UINT32
AcpiHwGetModeCapabilities (void)
{

    FUNCTION_TRACE ("HwGetModeCapabilities");


    if (!(AcpiGbl_SystemFlags & SYS_MODES_MASK))
    {
        if (AcpiHwGetMode () == SYS_MODE_LEGACY)
        {
            /*
             * Assume that if this call is being made, AcpiInit has been called
             * and ACPI support has been established by the presence of the
             * tables.  Therefore since we're in SYS_MODE_LEGACY, the system
             * must support both modes
             */

            AcpiGbl_SystemFlags |= (SYS_MODE_ACPI | SYS_MODE_LEGACY);
        }

        else
        {
            /* TBD: [Investigate] !!! this may be unsafe... */
            /*
             * system is is ACPI mode, so try to switch back to LEGACY to see if
             * it is supported
             */
            AcpiHwSetMode (SYS_MODE_LEGACY);

            if (AcpiHwGetMode () == SYS_MODE_LEGACY)
            {
                /* Now in SYS_MODE_LEGACY, so both are supported */

                AcpiGbl_SystemFlags |= (SYS_MODE_ACPI | SYS_MODE_LEGACY);
                AcpiHwSetMode (SYS_MODE_ACPI);
            }

            else
            {
                /* Still in SYS_MODE_ACPI so this must be an ACPI only system */

                AcpiGbl_SystemFlags |= SYS_MODE_ACPI;
            }
        }
    }

    return_VALUE (AcpiGbl_SystemFlags & SYS_MODES_MASK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiHwPmtTicks
 *
 * PARAMETERS:  none
 *
 * RETURN:      Current value of the ACPI PMT (timer)
 *
 * DESCRIPTION: Obtains current value of ACPI PMT
 *
 ******************************************************************************/

UINT32
AcpiHwPmtTicks (void)
{
    UINT32                   Ticks;

    FUNCTION_TRACE ("AcpiPmtTicks");

    Ticks = AcpiOsIn32 ((ACPI_IO_ADDRESS) AcpiGbl_FADT->XPmTmrBlk.Address);

    return_VALUE (Ticks);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiHwPmtResolution
 *
 * PARAMETERS:  none
 *
 * RETURN:      Number of bits of resolution in the PMT (either 24 or 32)
 *
 * DESCRIPTION: Obtains resolution of the ACPI PMT (either 24bit or 32bit)
 *
 ******************************************************************************/

UINT32
AcpiHwPmtResolution (void)
{
    FUNCTION_TRACE ("AcpiPmtResolution");

    if (0 == AcpiGbl_FADT->TmrValExt)
    {
        return_VALUE (24);
    }

    return_VALUE (32);
}

