
/******************************************************************************
 *
 * Name: hwsleep.c - ACPI Hardware Sleep/Wake Interface
 *              $Revision: 11 $
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
#include "acnamesp.h"
#include "achware.h"

#define _COMPONENT          ACPI_HARDWARE
        MODULE_NAME         ("hwsleep")


/******************************************************************************
 *
 * FUNCTION:    AcpiSetFirmwareWakingVector
 *
 * PARAMETERS:  PhysicalAddress     - Physical address of ACPI real mode
 *                                    entry point.
 *
 * RETURN:      AE_OK or AE_ERROR
 *
 * DESCRIPTION: Access function for dFirmwareWakingVector field in FACS
 *
 ******************************************************************************/

ACPI_STATUS
AcpiSetFirmwareWakingVector (
    ACPI_PHYSICAL_ADDRESS PhysicalAddress)
{

    FUNCTION_TRACE ("AcpiSetFirmwareWakingVector");


    /* Make sure that we have an FACS */

    if (!AcpiGbl_FACS)
    {
        return_ACPI_STATUS (AE_NO_ACPI_TABLES);
    }

    /* Set the vector */

    if (AcpiGbl_FACS->VectorWidth == 32)
    {
        * (UINT32 *) AcpiGbl_FACS->FirmwareWakingVector = (UINT32) PhysicalAddress;
    }
    else
    {
        *AcpiGbl_FACS->FirmwareWakingVector = PhysicalAddress;
    }

    return_ACPI_STATUS (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiGetFirmwareWakingVector
 *
 * PARAMETERS:  *PhysicalAddress    - Output buffer where contents of
 *                                    the FirmwareWakingVector field of
 *                                    the FACS will be stored.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Access function for dFirmwareWakingVector field in FACS
 *
 ******************************************************************************/

ACPI_STATUS
AcpiGetFirmwareWakingVector (
    ACPI_PHYSICAL_ADDRESS *PhysicalAddress)
{

    FUNCTION_TRACE ("AcpiGetFirmwareWakingVector");


    if (!PhysicalAddress)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /* Make sure that we have an FACS */

    if (!AcpiGbl_FACS)
    {
        return_ACPI_STATUS (AE_NO_ACPI_TABLES);
    }

    /* Get the vector */

    if (AcpiGbl_FACS->VectorWidth == 32)
    {
        *PhysicalAddress = * (UINT32 *) AcpiGbl_FACS->FirmwareWakingVector;
    }
    else
    {
        *PhysicalAddress = *AcpiGbl_FACS->FirmwareWakingVector;
    }

    return_ACPI_STATUS (AE_OK);
}

/******************************************************************************
 *
 * FUNCTION:    AcpiEnterSleepState
 *
 * PARAMETERS:  SleepState          - Which sleep state to enter
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Enter a system sleep state (see ACPI 2.0 spec p 231)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEnterSleepState (
    UINT8               SleepState)
{
    ACPI_STATUS         Status;
    ACPI_OBJECT_LIST    ArgList;
    ACPI_OBJECT         Arg;
    UINT8               TypeA;
    UINT8               TypeB;
    UINT16              PM1AControl;
    UINT16              PM1BControl;


    FUNCTION_TRACE ("AcpiEnterSleepState");


    /*
     * _PSW methods could be run here to enable wake-on keyboard, LAN, etc.
     */

    Status = AcpiHwObtainSleepTypeRegisterData (SleepState, &TypeA, &TypeB);
    if (!ACPI_SUCCESS (Status))
    {
        return Status;
    }

    /* run the _PTS and _GTS methods */

    MEMSET(&ArgList, 0, sizeof(ArgList));
    ArgList.Count = 1;
    ArgList.Pointer = &Arg;

    MEMSET(&Arg, 0, sizeof(Arg));
    Arg.Type = ACPI_TYPE_INTEGER;
    Arg.Integer.Value = SleepState;

    AcpiEvaluateObject(NULL, "\\_PTS", &ArgList, NULL);
    AcpiEvaluateObject(NULL, "\\_GTS", &ArgList, NULL);

    /* clear wake status */

    AcpiHwRegisterBitAccess(ACPI_WRITE, ACPI_MTX_LOCK, WAK_STS, 1);

    PM1AControl = (UINT16) AcpiHwRegisterRead(ACPI_MTX_LOCK, PM1_CONTROL);

    /* mask off SLP_EN and SLP_TYP fields */

    PM1AControl &= 0xC3FF;

    /* mask in SLP_EN */

    PM1AControl |= (1 << AcpiHwGetBitShift (SLP_EN_MASK));

    PM1BControl = PM1AControl;

    /* mask in SLP_TYP */

    PM1AControl |= (TypeA << AcpiHwGetBitShift (SLP_TYPE_X_MASK));
    PM1BControl |= (TypeB << AcpiHwGetBitShift (SLP_TYPE_X_MASK));

    DEBUG_PRINT(ACPI_OK, ("Entering S%d\n", SleepState));

    disable();

    AcpiHwRegisterWrite(ACPI_MTX_LOCK, PM1A_CONTROL, PM1AControl);
    AcpiHwRegisterWrite(ACPI_MTX_LOCK, PM1B_CONTROL, PM1BControl);

    /* one system won't work with this, one won't work without */
    /*AcpiHwRegisterWrite(ACPI_MTX_LOCK, PM1_CONTROL,
        (1 << AcpiHwGetBitShift (SLP_EN_MASK)));*/

    enable();

    return_ACPI_STATUS (AE_OK);
}
