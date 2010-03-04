
/******************************************************************************
 *
 * Module Name: aslmap - parser to AML opcode mapping table
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2010, Intel Corp.
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


#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include <contrib/dev/acpica/include/amlcode.h>
#include <contrib/dev/acpica/include/acparser.h>


#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslmap")


/*******************************************************************************
 *
 * FUNCTION:    AslMapNamedOpcodeToDataType
 *
 * PARAMETERS:  Opcode              - The Named AML opcode to map
 *
 * RETURN:      The ACPI type associated with the named opcode
 *
 * DESCRIPTION: Convert a raw Named AML opcode to the associated data type.
 *              Named opcodes are a subset of the AML opcodes.
 *
 ******************************************************************************/

ACPI_OBJECT_TYPE
AslMapNamedOpcodeToDataType (
    UINT16                  Opcode)
{
    const ACPI_OPCODE_INFO  *OpInfo;


    /*
     * There are some differences from the opcode table types, we
     * catch them here.
     */
    OpInfo = AcpiPsGetOpcodeInfo (Opcode);

    if (Opcode == AML_INT_NAMEPATH_OP)
    {
        return (ACPI_TYPE_ANY);
    }

    if (Opcode == AML_INT_METHODCALL_OP)
    {
        return (ACPI_TYPE_ANY);
    }

    if (OpInfo->Flags & AML_NSOBJECT)
    {
        return (OpInfo->ObjectType);
    }

    return (ACPI_TYPE_ANY);
}


/*******************************************************************************
 *
 * FUNCTION:    MpDisplayReservedNames
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print the table above
 *
 ******************************************************************************/

void
MpDisplayReservedNames (
    void)
{
    UINT32              i;

    printf ("Reserved name information\n\n");

    for (i = 0; ReservedMethods[i].Name; i++)
    {
        printf ("%s    ", ReservedMethods[i].Name);

        if (ReservedMethods[i].Flags & ASL_RSVD_SCOPE)
        {
            printf ("Reserved scope name\n");
        }
        else if (ReservedMethods[i].Flags & ASL_RSVD_RESOURCE_NAME)
        {
            printf ("Resource data type reserved field name\n");
        }
        else
        {
            printf ("Method with %d arguments, ",
                ReservedMethods[i].NumArguments & 0x0F);

            if (ReservedMethods[i].Flags & ASL_RSVD_RETURN_VALUE)
            {
                printf ("must return a value\n");
            }
            else
            {
                printf ("no return value\n");
            }
        }
    }
}


/*******************************************************************************
 *
 * DATA STRUCTURE:  ReservedMethods
 *
 * DESCRIPTION:     Contains all reserved methods and names as defined in the
 *                  ACPI specification.  Used during the analysis phase to
 *                  ensure that reserved methods have the required number of
 *                  arguments and the proper return type.
 *
 * Each entry in the table contains the following items:
 *
 * Name         - The ACPI reserved name
 * Args         - Number of arguments to the method
 * Flags        - Whether this method must return a value or not. Or if the
 *                name is a resource descriptor label.
 *
 ******************************************************************************/

const ASL_RESERVED_INFO         ReservedMethods[] = {
    {"_AC0",     0,      ASL_RSVD_RETURN_VALUE},
    {"_AC1",     0,      ASL_RSVD_RETURN_VALUE},
    {"_AC2",     0,      ASL_RSVD_RETURN_VALUE},
    {"_AC3",     0,      ASL_RSVD_RETURN_VALUE},
    {"_AC4",     0,      ASL_RSVD_RETURN_VALUE},
    {"_AC5",     0,      ASL_RSVD_RETURN_VALUE},
    {"_AC6",     0,      ASL_RSVD_RETURN_VALUE},
    {"_AC7",     0,      ASL_RSVD_RETURN_VALUE},
    {"_AC8",     0,      ASL_RSVD_RETURN_VALUE},
    {"_AC9",     0,      ASL_RSVD_RETURN_VALUE},
    {"_ADR",     0,      ASL_RSVD_RETURN_VALUE},
    {"_AL0",     0,      ASL_RSVD_RETURN_VALUE},
    {"_AL1",     0,      ASL_RSVD_RETURN_VALUE},
    {"_AL2",     0,      ASL_RSVD_RETURN_VALUE},
    {"_AL3",     0,      ASL_RSVD_RETURN_VALUE},
    {"_AL4",     0,      ASL_RSVD_RETURN_VALUE},
    {"_AL5",     0,      ASL_RSVD_RETURN_VALUE},
    {"_AL6",     0,      ASL_RSVD_RETURN_VALUE},
    {"_AL7",     0,      ASL_RSVD_RETURN_VALUE},
    {"_AL8",     0,      ASL_RSVD_RETURN_VALUE},
    {"_AL9",     0,      ASL_RSVD_RETURN_VALUE},
    {"_ALC",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 3.0 */
    {"_ALI",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 3.0 */
    {"_ALN",     0,      ASL_RSVD_RESOURCE_NAME},
    {"_ALP",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 3.0 */
    {"_ALR",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 3.0 */
    {"_ALT",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 3.0 */
    {"_ART",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 4.0 */
    {"_ASI",     0,      ASL_RSVD_RESOURCE_NAME},
    {"_ASZ",     0,      ASL_RSVD_RESOURCE_NAME},
    {"_BAS",     0,      ASL_RSVD_RESOURCE_NAME},
    {"_BBN",     0,      ASL_RSVD_RETURN_VALUE},
    {"_BCL",     0,      ASL_RSVD_RETURN_VALUE},
    {"_BCM",     1,      0},
    {"_BCT",     1,      ASL_RSVD_RETURN_VALUE},    /* Acpi 4.0 */
    {"_BDN",     0,      ASL_RSVD_RETURN_VALUE},
    {"_BFS",     1,      0},
    {"_BIF",     0,      ASL_RSVD_RETURN_VALUE},
    {"_BIX",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 4.0 */
    {"_BLT",     3,      0},                        /* Acpi 3.0 */
    {"_BM_",     0,      ASL_RSVD_RESOURCE_NAME},
    {"_BMA",     1,      ASL_RSVD_RETURN_VALUE},    /* Acpi 4.0 */
    {"_BMC",     1,      0},                        /* Acpi 3.0 */
    {"_BMD",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 3.0 */
    {"_BMS",     1,      ASL_RSVD_RETURN_VALUE},    /* Acpi 4.0 */
    {"_BQC",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 3.0 */
    {"_BST",     0,      ASL_RSVD_RETURN_VALUE},
    {"_BTM",     1,      ASL_RSVD_RETURN_VALUE},    /* Acpi 3.0 */
    {"_BTP",     1,      0},
    {"_CBA",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 3.0 */
    {"_CDM",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 4.0 */
    {"_CID",     0,      ASL_RSVD_RETURN_VALUE},
    {"_CRS",     0,      ASL_RSVD_RETURN_VALUE},
    {"_CRT",     0,      ASL_RSVD_RETURN_VALUE},
    {"_CSD",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 3.0 */
    {"_CST",     0,      ASL_RSVD_RETURN_VALUE},
    {"_DCK",     1,      ASL_RSVD_RETURN_VALUE},
    {"_DCS",     0,      ASL_RSVD_RETURN_VALUE},
    {"_DDC",     1,      ASL_RSVD_RETURN_VALUE},
    {"_DDN",     0,      0},
    {"_DEC",     0,      ASL_RSVD_RESOURCE_NAME},
    {"_DGS",     0,      ASL_RSVD_RETURN_VALUE},
    {"_DIS",     0,      0},
    {"_DMA",     0,      ASL_RSVD_RETURN_VALUE},
    {"_DOD",     0,      ASL_RSVD_RETURN_VALUE},
    {"_DOS",     1,      0},
    {"_DSM",     4,      ASL_RSVD_RETURN_VALUE},    /* Acpi 3.0 */
    {"_DSS",     1,      0},
    {"_DSW",     3,      0},                        /* Acpi 3.0 */
    {"_DTI",     1,      0},                        /* Acpi 4.0 */
    {"_EC_",     0,      ASL_RSVD_RETURN_VALUE},
    {"_EDL",     0,      ASL_RSVD_RETURN_VALUE},
    {"_EJ0",     1,      0},
    {"_EJ1",     1,      0},
    {"_EJ2",     1,      0},
    {"_EJ3",     1,      0},
    {"_EJ4",     1,      0},
    {"_EJD",     0,      ASL_RSVD_RETURN_VALUE},
    {"_ERR",     3,      ASL_RSVD_RETURN_VALUE},
    {"_FDE",     0,      ASL_RSVD_RETURN_VALUE},
    {"_FDI",     0,      ASL_RSVD_RETURN_VALUE},
    {"_FDM",     1,      0},
    {"_FIF",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 4.0 */
    {"_FIX",     0,      ASL_RSVD_RETURN_VALUE},
    {"_FPS",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 4.0 */
    {"_FSL",     1,      0},                        /* Acpi 4.0 */
    {"_FST",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 4.0 */
    {"_GAI",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 4.0 */
    {"_GHL",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 4.0 */
    {"_GL_",     0,      ASL_RSVD_RETURN_VALUE},
    {"_GLK",     0,      ASL_RSVD_RETURN_VALUE},
    {"_GPD",     0,      ASL_RSVD_RETURN_VALUE},
    {"_GPE",     0,      ASL_RSVD_RETURN_VALUE},
    {"_GRA",     0,      ASL_RSVD_RESOURCE_NAME},
    {"_GSB",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 3.0 */
    {"_GTF",     0,      ASL_RSVD_RETURN_VALUE},
    {"_GTM",     0,      ASL_RSVD_RETURN_VALUE},
    {"_GTS",     1,      0},
    {"_HE_",     0,      ASL_RSVD_RESOURCE_NAME},
    {"_HID",     0,      ASL_RSVD_RETURN_VALUE},
    {"_HOT",     0,      ASL_RSVD_RETURN_VALUE},
    {"_HPP",     0,      ASL_RSVD_RETURN_VALUE},
    {"_HPX",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 3.0 */
    {"_IFT",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 3.0 */
    {"_INI",     0,      0},
    {"_INT",     0,      ASL_RSVD_RESOURCE_NAME},
    {"_IRC",     0,      0},
    {"_LCK",     1,      0},
    {"_LEN",     0,      ASL_RSVD_RESOURCE_NAME},
    {"_LID",     0,      ASL_RSVD_RETURN_VALUE},
    {"_LL_",     0,      ASL_RSVD_RESOURCE_NAME},
    {"_MAF",     0,      ASL_RSVD_RESOURCE_NAME},
    {"_MAT",     0,      ASL_RSVD_RETURN_VALUE},
    {"_MAX",     0,      ASL_RSVD_RESOURCE_NAME},
    {"_MBM",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 4.0 */
    {"_MEM",     0,      ASL_RSVD_RESOURCE_NAME},
    {"_MIF",     0,      ASL_RSVD_RESOURCE_NAME},
    {"_MIN",     0,      ASL_RSVD_RESOURCE_NAME},
    {"_MLS",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 3.0 */
    {"_MSG",     1,      0},
    {"_MSM",     4,      ASL_RSVD_RETURN_VALUE},    /* Acpi 4.0 */
    {"_MTP",     0,      ASL_RSVD_RESOURCE_NAME},
    {"_NTT",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 4.0 */
    {"_OFF",     0,      0},
    {"_ON_",     0,      0},
    {"_OS_",     0,      ASL_RSVD_RETURN_VALUE},
    {"_OSC",     4,      ASL_RSVD_RETURN_VALUE},    /* Acpi 3.0 */
    {"_OSI",     1,      ASL_RSVD_RETURN_VALUE},
    {"_OST",     3,      0},                        /* Acpi 3.0 */
    {"_PAI",     1,      ASL_RSVD_RETURN_VALUE},    /* Acpi 4.0 */
    {"_PCL",     0,      ASL_RSVD_RETURN_VALUE},
    {"_PCT",     0,      ASL_RSVD_RETURN_VALUE},
    {"_PDC",     1,      0},
    {"_PDL",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 4.0 */
    {"_PIC",     1,      0},
    {"_PIF",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 4.0 */
    {"_PLD",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 3.0 */
    {"_PMC",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 4.0 */
    {"_PMD",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 4.0 */
    {"_PMM",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 4.0 */
    {"_PPC",     0,      ASL_RSVD_RETURN_VALUE},
    {"_PPE",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 3.0 */
    {"_PR0",     0,      ASL_RSVD_RETURN_VALUE},
    {"_PR1",     0,      ASL_RSVD_RETURN_VALUE},
    {"_PR2",     0,      ASL_RSVD_RETURN_VALUE},
    {"_PR3",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 4.0 */
    {"_PRL",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 4.0 */
    {"_PRS",     0,      ASL_RSVD_RETURN_VALUE},
    {"_PRT",     0,      ASL_RSVD_RETURN_VALUE},
    {"_PRW",     0,      ASL_RSVD_RETURN_VALUE},
    {"_PS0",     0,      0},
    {"_PS1",     0,      0},
    {"_PS2",     0,      0},
    {"_PS3",     0,      0},
    {"_PSC",     0,      ASL_RSVD_RETURN_VALUE},
    {"_PSD",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 3.0 */
    {"_PSL",     0,      ASL_RSVD_RETURN_VALUE},
    {"_PSR",     0,      ASL_RSVD_RETURN_VALUE},
    {"_PSS",     0,      ASL_RSVD_RETURN_VALUE},
    {"_PSV",     0,      ASL_RSVD_RETURN_VALUE},
    {"_PSW",     1,      0},
    {"_PTC",     0,      ASL_RSVD_RETURN_VALUE},
    {"_PTP",     2,      ASL_RSVD_RETURN_VALUE},    /* Acpi 4.0 */
    {"_PTS",     1,      0},
    {"_PUR",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 4.0 */
    {"_PXM",     0,      ASL_RSVD_RETURN_VALUE},
    {"_RBO",     0,      ASL_RSVD_RESOURCE_NAME},
    {"_RBW",     0,      ASL_RSVD_RESOURCE_NAME},
    {"_REG",     2,      0},
    {"_REV",     0,      ASL_RSVD_RETURN_VALUE},
    {"_RMV",     0,      ASL_RSVD_RETURN_VALUE},
    {"_RNG",     0,      ASL_RSVD_RESOURCE_NAME},
    {"_ROM",     2,      ASL_RSVD_RETURN_VALUE},
    {"_RT_",     0,      ASL_RSVD_RESOURCE_NAME},   /* Acpi 3.0 */
    {"_RTV",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 3.0 */
    {"_RW_",     0,      ASL_RSVD_RESOURCE_NAME},
    {"_S0_",     0,      ASL_RSVD_RETURN_VALUE},
    {"_S1_",     0,      ASL_RSVD_RETURN_VALUE},
    {"_S2_",     0,      ASL_RSVD_RETURN_VALUE},
    {"_S3_",     0,      ASL_RSVD_RETURN_VALUE},
    {"_S4_",     0,      ASL_RSVD_RETURN_VALUE},
    {"_S5_",     0,      ASL_RSVD_RETURN_VALUE},
    {"_S1D",     0,      ASL_RSVD_RETURN_VALUE},
    {"_S2D",     0,      ASL_RSVD_RETURN_VALUE},
    {"_S3D",     0,      ASL_RSVD_RETURN_VALUE},
    {"_S4D",     0,      ASL_RSVD_RETURN_VALUE},
    {"_S0W",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 3.0 */
    {"_S1W",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 3.0 */
    {"_S2W",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 3.0 */
    {"_S3W",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 3.0 */
    {"_S4W",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 3.0 */
    {"_SB_",     0,      ASL_RSVD_SCOPE},
    {"_SBS",     0,      ASL_RSVD_RETURN_VALUE},
    {"_SCP",     0x13,   0},                        /* Acpi 1.0 - one arg; Acpi 3.0 - three args */
    {"_SDD",     1,      0},                        /* Acpi 3.0 */
    {"_SEG",     0,      ASL_RSVD_RETURN_VALUE},
    {"_SHL",     1,      ASL_RSVD_RETURN_VALUE},    /* Acpi 4.0 */
    {"_SHR",     0,      ASL_RSVD_RESOURCE_NAME},
    {"_SI_",     0,      ASL_RSVD_SCOPE},
    {"_SIZ",     0,      ASL_RSVD_RESOURCE_NAME},
    {"_SLI",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 3.0 */
    {"_SPD",     1,      ASL_RSVD_RETURN_VALUE},
    {"_SRS",     1,      0},
    {"_SRV",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 3.0 */
    {"_SST",     1,      0},
    {"_STA",     0,      ASL_RSVD_RETURN_VALUE},
    {"_STM",     3,      0},
    {"_STP",     2,      ASL_RSVD_RETURN_VALUE},    /* Acpi 4.0 */
    {"_STR",     0,      ASL_RSVD_RETURN_VALUE},
    {"_STV",     2,      ASL_RSVD_RETURN_VALUE},    /* Acpi 4.0 */
    {"_SUN",     0,      ASL_RSVD_RETURN_VALUE},
    {"_SWS",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 3.0 */
    {"_TC1",     0,      ASL_RSVD_RETURN_VALUE},
    {"_TC2",     0,      ASL_RSVD_RETURN_VALUE},
    {"_TDL",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 3.0b */
    {"_TIP",     1,      ASL_RSVD_RETURN_VALUE},    /* Acpi 4.0 */
    {"_TIV",     1,      ASL_RSVD_RETURN_VALUE},    /* Acpi 4.0 */
    {"_TMP",     0,      ASL_RSVD_RETURN_VALUE},
    {"_TPC",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 3.0 */
    {"_TPT",     1,      0},                        /* Acpi 3.0 */
    {"_TRA",     0,      ASL_RSVD_RESOURCE_NAME},
    {"_TRS",     0,      ASL_RSVD_RESOURCE_NAME},
    {"_TRT",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 3.0 */
    {"_TSD",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 3.0 */
    {"_TSF",     0,      ASL_RSVD_RESOURCE_NAME},   /* Acpi 3.0 */
    {"_TSP",     0,      ASL_RSVD_RETURN_VALUE},
    {"_TSS",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 3.0 */
    {"_TST",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 3.0 */
    {"_TTP",     0,      ASL_RSVD_RESOURCE_NAME},
    {"_TTS",     1,      0},                        /* Acpi 3.0 */
    {"_TYP",     0,      ASL_RSVD_RESOURCE_NAME},
    {"_TZ_",     0,      ASL_RSVD_SCOPE},
    {"_TZD",     0,      ASL_RSVD_RETURN_VALUE},
    {"_TZM",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 3.0 */
    {"_TZP",     0,      ASL_RSVD_RETURN_VALUE},
    {"_UID",     0,      ASL_RSVD_RETURN_VALUE},
    {"_UPC",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 3.0 */
    {"_UPD",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 3.0 */
    {"_UPP",     0,      ASL_RSVD_RETURN_VALUE},    /* Acpi 3.0 */
    {"_VPO",     0,      ASL_RSVD_RETURN_VALUE},
    {"_WAK",     1,      ASL_RSVD_RETURN_VALUE},
    {"_WDG",     0,      ASL_RSVD_RETURN_VALUE},    /* MS Extension */
    {"_WED",     1,      ASL_RSVD_RETURN_VALUE},    /* MS Extension */
    {NULL,       0,      0},
};


/*******************************************************************************
 *
 * DATA STRUCTURE:  AslKeywordMapping
 *
 * DESCRIPTION:     Maps the ParseOpcode to the actual AML opcode.  The parse
 *                  opcodes are generated from Bison, and this table must
 *                  track any additions to them.
 *
 * Each entry in the table contains the following items:
 *
 * AML opcode   - Opcode that is written to the AML file
 * Value        - Value of the object to be written (if applicable)
 * Flags        - 1) Whether this opcode opens an AML "package".
 *
 ******************************************************************************/
/*
 * TBD:
 * AccessAttrib
 * AccessType
 * AMlop for DMA?
 * ObjectType keywords
 * Register
 */

const ASL_MAPPING_ENTRY     AslKeywordMapping [] =
{
/*! [Begin] no source code translation (keep the table structure) */


/* ACCESSAS */                  OP_TABLE_ENTRY (AML_INT_ACCESSFIELD_OP,     0,                              0,                  0),
/* ACCESSATTRIB_BLOCK */        OP_TABLE_ENTRY (AML_BYTE_OP,                AML_FIELD_ATTRIB_SMB_BLOCK,     0,                  0),
/* ACCESSATTRIB_BLOCK_CALL */   OP_TABLE_ENTRY (AML_BYTE_OP,                AML_FIELD_ATTRIB_SMB_BLOCK_CALL,0,                  0),
/* ACCESSATTRIB_BYTE */         OP_TABLE_ENTRY (AML_BYTE_OP,                AML_FIELD_ATTRIB_SMB_BYTE,      0,                  0),
/* ACCESSATTRIB_WORD_CALL */    OP_TABLE_ENTRY (AML_BYTE_OP,                AML_FIELD_ATTRIB_SMB_WORD_CALL, 0,                  0),
/* ACCESSATTRIB_QUICK */        OP_TABLE_ENTRY (AML_BYTE_OP,                AML_FIELD_ATTRIB_SMB_QUICK,     0,                  0),
/* ACCESSATTRIB_SND_RCV */      OP_TABLE_ENTRY (AML_BYTE_OP,                AML_FIELD_ATTRIB_SMB_SEND_RCV,  0,                  0),
/* ACCESSATTRIB_WORD */         OP_TABLE_ENTRY (AML_BYTE_OP,                AML_FIELD_ATTRIB_SMB_WORD,      0,                  0),
/* ACCESSTYPE_ANY */            OP_TABLE_ENTRY (AML_BYTE_OP,                AML_FIELD_ACCESS_ANY,           0,                  0),
/* ACCESSTYPE_BUF */            OP_TABLE_ENTRY (AML_BYTE_OP,                AML_FIELD_ACCESS_BUFFER,        0,                  0),
/* ACCESSTYPE_BYTE */           OP_TABLE_ENTRY (AML_BYTE_OP,                AML_FIELD_ACCESS_BYTE,          0,                  0),
/* ACCESSTYPE_DWORD */          OP_TABLE_ENTRY (AML_BYTE_OP,                AML_FIELD_ACCESS_DWORD,         0,                  0),
/* ACCESSTYPE_QWORD */          OP_TABLE_ENTRY (AML_BYTE_OP,                AML_FIELD_ACCESS_QWORD,         0,                  0),
/* ACCESSTYPE_WORD */           OP_TABLE_ENTRY (AML_BYTE_OP,                AML_FIELD_ACCESS_WORD,          0,                  0),
/* ACQUIRE */                   OP_TABLE_ENTRY (AML_ACQUIRE_OP,             0,                              0,                  ACPI_BTYPE_INTEGER),
/* ADD */                       OP_TABLE_ENTRY (AML_ADD_OP,                 0,                              0,                  ACPI_BTYPE_INTEGER),
/* ADDRESSSPACE_FFIXEDHW */     OP_TABLE_ENTRY (AML_BYTE_OP,                REGION_FIXED_HW,                0,                  0),
/* ADDRESSTYPE_ACPI */          OP_TABLE_ENTRY (AML_BYTE_OP,                2,                              0,                  0),
/* ADDRESSTYPE_MEMORY */        OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* ADDRESSTYPE_NVS */           OP_TABLE_ENTRY (AML_BYTE_OP,                3,                              0,                  0),
/* ADDRESSTYPE_RESERVED */      OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* ALIAS */                     OP_TABLE_ENTRY (AML_ALIAS_OP,               0,                              0,                  0),
/* AND */                       OP_TABLE_ENTRY (AML_BIT_AND_OP,             0,                              0,                  ACPI_BTYPE_INTEGER),
/* ARG0 */                      OP_TABLE_ENTRY (AML_ARG0,                   0,                              0,                  ACPI_BTYPE_OBJECTS_AND_REFS),
/* ARG1 */                      OP_TABLE_ENTRY (AML_ARG1,                   0,                              0,                  ACPI_BTYPE_OBJECTS_AND_REFS),
/* ARG2 */                      OP_TABLE_ENTRY (AML_ARG2,                   0,                              0,                  ACPI_BTYPE_OBJECTS_AND_REFS),
/* ARG3 */                      OP_TABLE_ENTRY (AML_ARG3,                   0,                              0,                  ACPI_BTYPE_OBJECTS_AND_REFS),
/* ARG4 */                      OP_TABLE_ENTRY (AML_ARG4,                   0,                              0,                  ACPI_BTYPE_OBJECTS_AND_REFS),
/* ARG5 */                      OP_TABLE_ENTRY (AML_ARG5,                   0,                              0,                  ACPI_BTYPE_OBJECTS_AND_REFS),
/* ARG6 */                      OP_TABLE_ENTRY (AML_ARG6,                   0,                              0,                  ACPI_BTYPE_OBJECTS_AND_REFS),
/* BANKFIELD */                 OP_TABLE_ENTRY (AML_BANK_FIELD_OP,          0,                              NODE_AML_PACKAGE,   0),
/* BREAK */                     OP_TABLE_ENTRY (AML_BREAK_OP,               0,                              0,                  0),
/* BREAKPOINT */                OP_TABLE_ENTRY (AML_BREAK_POINT_OP,         0,                              0,                  0),
/* BUFFER */                    OP_TABLE_ENTRY (AML_BUFFER_OP,              0,                              NODE_AML_PACKAGE,   ACPI_BTYPE_BUFFER),
/* BUSMASTERTYPE_MASTER */      OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* BUSMASTERTYPE_NOTMASTER */   OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* BYTECONST */                 OP_TABLE_ENTRY (AML_RAW_DATA_BYTE,          0,                              0,                  ACPI_BTYPE_INTEGER),
/* CASE */                      OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* CONCATENATE */               OP_TABLE_ENTRY (AML_CONCAT_OP,              0,                              0,                  ACPI_BTYPE_COMPUTE_DATA),
/* CONCATENATERESTEMPLATE */    OP_TABLE_ENTRY (AML_CONCAT_RES_OP,          0,                              0,                  ACPI_BTYPE_BUFFER),
/* CONDREFOF */                 OP_TABLE_ENTRY (AML_COND_REF_OF_OP,         0,                              0,                  ACPI_BTYPE_INTEGER),
/* CONTINUE */                  OP_TABLE_ENTRY (AML_CONTINUE_OP,            0,                              0,                  0),
/* COPY */                      OP_TABLE_ENTRY (AML_COPY_OP,                0,                              0,                  ACPI_BTYPE_DATA_REFERENCE),
/* CREATEBITFIELD */            OP_TABLE_ENTRY (AML_CREATE_BIT_FIELD_OP,    0,                              0,                  0),
/* CREATEBYTEFIELD */           OP_TABLE_ENTRY (AML_CREATE_BYTE_FIELD_OP,   0,                              0,                  0),
/* CREATEDWORDFIELD */          OP_TABLE_ENTRY (AML_CREATE_DWORD_FIELD_OP,  0,                              0,                  0),
/* CREATEFIELD */               OP_TABLE_ENTRY (AML_CREATE_FIELD_OP,        0,                              0,                  0),
/* CREATEQWORDFIELD */          OP_TABLE_ENTRY (AML_CREATE_QWORD_FIELD_OP,  0,                              0,                  0),
/* CREATEWORDFIELD */           OP_TABLE_ENTRY (AML_CREATE_WORD_FIELD_OP,   0,                              0,                  0),
/* DATATABLEREGION */           OP_TABLE_ENTRY (AML_DATA_REGION_OP,         0,                              0,                  0),
/* DEBUG */                     OP_TABLE_ENTRY (AML_DEBUG_OP,               0,                              0,                  ACPI_BTYPE_DEBUG_OBJECT),
/* DECODETYPE_POS */            OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* DECODETYPE_SUB */            OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* DECREMENT */                 OP_TABLE_ENTRY (AML_DECREMENT_OP,           0,                              0,                  ACPI_BTYPE_INTEGER),
/* DEFAULT */                   OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* DEFAULT_ARG */               OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* DEFINITIONBLOCK */           OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* DEREFOF */                   OP_TABLE_ENTRY (AML_DEREF_OF_OP,            0,                              0,                  ACPI_BTYPE_DATA_REFERENCE | ACPI_BTYPE_STRING),
/* DEVICE */                    OP_TABLE_ENTRY (AML_DEVICE_OP,              0,                              NODE_AML_PACKAGE,   0),
/* DIVIDE */                    OP_TABLE_ENTRY (AML_DIVIDE_OP,              0,                              0,                  ACPI_BTYPE_INTEGER),
/* DMA */                       OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* DMATYPE_A */                 OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* DMATYPE_COMPATIBILITY */     OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* DMATYPE_B */                 OP_TABLE_ENTRY (AML_BYTE_OP,                2,                              0,                  0),
/* DMATYPE_F */                 OP_TABLE_ENTRY (AML_BYTE_OP,                3,                              0,                  0),
/* DWORDCONST */                OP_TABLE_ENTRY (AML_RAW_DATA_DWORD,         0,                              0,                  ACPI_BTYPE_INTEGER),
/* DWORDIO */                   OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* DWORDMEMORY */               OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* DWORDSPACE */                OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* EISAID */                    OP_TABLE_ENTRY (AML_DWORD_OP,               0,                              0,                  ACPI_BTYPE_INTEGER),
/* ELSE */                      OP_TABLE_ENTRY (AML_ELSE_OP,                0,                              NODE_AML_PACKAGE,   0),
/* ELSEIF */                    OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              NODE_AML_PACKAGE,   0),
/* ENDDEPENDENTFN */            OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* ENDTAG */                    OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* ERRORNODE */                 OP_TABLE_ENTRY (AML_NOOP_OP,                0,                              0,                  0),
/* EVENT */                     OP_TABLE_ENTRY (AML_EVENT_OP,               0,                              0,                  0),
/* EXTENDEDIO */                OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* EXTENDEDMEMORY */            OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* EXTENDEDSPACE */             OP_TABLE_ENTRY (AML_RAW_DATA_QWORD,         0,                              0,                  ACPI_BTYPE_INTEGER),
/* EXTERNAL */                  OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* FATAL */                     OP_TABLE_ENTRY (AML_FATAL_OP,               0,                              0,                  0),
/* FIELD */                     OP_TABLE_ENTRY (AML_FIELD_OP,               0,                              NODE_AML_PACKAGE,   0),
/* FINDSETLEFTBIT */            OP_TABLE_ENTRY (AML_FIND_SET_LEFT_BIT_OP,   0,                              0,                  ACPI_BTYPE_INTEGER),
/* FINDSETRIGHTBIT */           OP_TABLE_ENTRY (AML_FIND_SET_RIGHT_BIT_OP,  0,                              0,                  ACPI_BTYPE_INTEGER),
/* FIXEDIO */                   OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* FROMBCD */                   OP_TABLE_ENTRY (AML_FROM_BCD_OP,            0,                              0,                  ACPI_BTYPE_INTEGER),
/* FUNCTION */                  OP_TABLE_ENTRY (AML_METHOD_OP,              0,                              NODE_AML_PACKAGE,   0),
/* IF */                        OP_TABLE_ENTRY (AML_IF_OP,                  0,                              NODE_AML_PACKAGE,   0),
/* INCLUDE */                   OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* INCLUDE_CSTYLE */            OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* INCLUDE_END */               OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* INCREMENT */                 OP_TABLE_ENTRY (AML_INCREMENT_OP,           0,                              0,                  ACPI_BTYPE_INTEGER),
/* INDEX */                     OP_TABLE_ENTRY (AML_INDEX_OP,               0,                              0,                  ACPI_BTYPE_REFERENCE),
/* INDEXFIELD */                OP_TABLE_ENTRY (AML_INDEX_FIELD_OP,         0,                              NODE_AML_PACKAGE,   0),
/* INTEGER */                   OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  ACPI_BTYPE_INTEGER),
/* INTERRUPT */                 OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* INTLEVEL_ACTIVEHIGH */       OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* INTLEVEL_ACTIVELOW */        OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* INTTYPE_EDGE */              OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* INTTYPE_LEVEL */             OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* IO */                        OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* IODECODETYPE_10 */           OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* IODECODETYPE_16 */           OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* IRQ */                       OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* IRQNOFLAGS */                OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* LAND */                      OP_TABLE_ENTRY (AML_LAND_OP,                0,                              0,                  ACPI_BTYPE_INTEGER),
/* LEQUAL */                    OP_TABLE_ENTRY (AML_LEQUAL_OP,              0,                              0,                  ACPI_BTYPE_INTEGER),
/* LGREATER */                  OP_TABLE_ENTRY (AML_LGREATER_OP,            0,                              0,                  ACPI_BTYPE_INTEGER),
/* LGREATEREQUAL */             OP_TABLE_ENTRY (AML_LGREATEREQUAL_OP,       0,                              0,                  ACPI_BTYPE_INTEGER),
/* LINE */                      OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* LLESS */                     OP_TABLE_ENTRY (AML_LLESS_OP,               0,                              0,                  ACPI_BTYPE_INTEGER),
/* LLESSEQUAL */                OP_TABLE_ENTRY (AML_LLESSEQUAL_OP,          0,                              0,                  ACPI_BTYPE_INTEGER),
/* LNOT */                      OP_TABLE_ENTRY (AML_LNOT_OP,                0,                              0,                  ACPI_BTYPE_INTEGER),
/* LNOTEQUAL */                 OP_TABLE_ENTRY (AML_LNOTEQUAL_OP,           0,                              0,                  ACPI_BTYPE_INTEGER),
/* LOAD */                      OP_TABLE_ENTRY (AML_LOAD_OP,                0,                              0,                  0),
/* LOADTABLE */                 OP_TABLE_ENTRY (AML_LOAD_TABLE_OP,          0,                              0,                  ACPI_BTYPE_DDB_HANDLE),
/* LOCAL0 */                    OP_TABLE_ENTRY (AML_LOCAL0,                 0,                              0,                  ACPI_BTYPE_OBJECTS_AND_REFS),
/* LOCAL1 */                    OP_TABLE_ENTRY (AML_LOCAL1,                 0,                              0,                  ACPI_BTYPE_OBJECTS_AND_REFS),
/* LOCAL2 */                    OP_TABLE_ENTRY (AML_LOCAL2,                 0,                              0,                  ACPI_BTYPE_OBJECTS_AND_REFS),
/* LOCAL3 */                    OP_TABLE_ENTRY (AML_LOCAL3,                 0,                              0,                  ACPI_BTYPE_OBJECTS_AND_REFS),
/* LOCAL4 */                    OP_TABLE_ENTRY (AML_LOCAL4,                 0,                              0,                  ACPI_BTYPE_OBJECTS_AND_REFS),
/* LOCAL5 */                    OP_TABLE_ENTRY (AML_LOCAL5,                 0,                              0,                  ACPI_BTYPE_OBJECTS_AND_REFS),
/* LOCAL6 */                    OP_TABLE_ENTRY (AML_LOCAL6,                 0,                              0,                  ACPI_BTYPE_OBJECTS_AND_REFS),
/* LOCAL7 */                    OP_TABLE_ENTRY (AML_LOCAL7,                 0,                              0,                  ACPI_BTYPE_OBJECTS_AND_REFS),
/* LOCKRULE_LOCK */             OP_TABLE_ENTRY (AML_BYTE_OP,                AML_FIELD_LOCK_ALWAYS,          0,                  0),
/* LOCKRULE_NOLOCK */           OP_TABLE_ENTRY (AML_BYTE_OP,                AML_FIELD_LOCK_NEVER,           0,                  0),
/* LOR */                       OP_TABLE_ENTRY (AML_LOR_OP,                 0,                              0,                  ACPI_BTYPE_INTEGER),
/* MATCH */                     OP_TABLE_ENTRY (AML_MATCH_OP,               0,                              0,                  ACPI_BTYPE_INTEGER),
/* MATCHTYPE_MEQ */             OP_TABLE_ENTRY (AML_RAW_DATA_BYTE,          MATCH_MEQ,                      0,                  ACPI_BTYPE_INTEGER),
/* MATCHTYPE_MGE */             OP_TABLE_ENTRY (AML_RAW_DATA_BYTE,          MATCH_MGE,                      0,                  ACPI_BTYPE_INTEGER),
/* MATCHTYPE_MGT */             OP_TABLE_ENTRY (AML_RAW_DATA_BYTE,          MATCH_MGT,                      0,                  ACPI_BTYPE_INTEGER),
/* MATCHTYPE_MLE */             OP_TABLE_ENTRY (AML_RAW_DATA_BYTE,          MATCH_MLE,                      0,                  ACPI_BTYPE_INTEGER),
/* MATCHTYPE_MLT */             OP_TABLE_ENTRY (AML_RAW_DATA_BYTE,          MATCH_MLT,                      0,                  ACPI_BTYPE_INTEGER),
/* MATCHTYPE_MTR */             OP_TABLE_ENTRY (AML_RAW_DATA_BYTE,          MATCH_MTR,                      0,                  ACPI_BTYPE_INTEGER),
/* MAXTYPE_FIXED */             OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* MAXTYPE_NOTFIXED */          OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* MEMORY24 */                  OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* MEMORY32 */                  OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* MEMORY32FIXED */             OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* MEMTYPE_CACHEABLE */         OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* MEMTYPE_NONCACHEABLE */      OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* MEMTYPE_PREFETCHABLE */      OP_TABLE_ENTRY (AML_BYTE_OP,                3,                              0,                  0),
/* MEMTYPE_WRITECOMBINING */    OP_TABLE_ENTRY (AML_BYTE_OP,                2,                              0,                  0),
/* METHOD */                    OP_TABLE_ENTRY (AML_METHOD_OP,              0,                              NODE_AML_PACKAGE,   0),
/* METHODCALL */                OP_TABLE_ENTRY (AML_INT_METHODCALL_OP,      0,                              0,                  ACPI_BTYPE_OBJECTS_AND_REFS),
/* MID */                       OP_TABLE_ENTRY (AML_MID_OP,                 0,                              0,                  ACPI_BTYPE_STRING | ACPI_BTYPE_BUFFER),
/* MINTYPE_FIXED */             OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* MINTYPE_NOTFIXED */          OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* MOD */                       OP_TABLE_ENTRY (AML_MOD_OP,                 0,                              0,                  ACPI_BTYPE_INTEGER),
/* MULTIPLY */                  OP_TABLE_ENTRY (AML_MULTIPLY_OP,            0,                              0,                  ACPI_BTYPE_INTEGER),
/* MUTEX */                     OP_TABLE_ENTRY (AML_MUTEX_OP,               0,                              0,                  0),
/* NAME */                      OP_TABLE_ENTRY (AML_NAME_OP,                0,                              0,                  0),
/* NAMESEG */                   OP_TABLE_ENTRY (AML_INT_NAMEPATH_OP,        0,                              0,                  0),
/* NAMESTRING */                OP_TABLE_ENTRY (AML_INT_NAMEPATH_OP,        0,                              0,                  0),
/* NAND */                      OP_TABLE_ENTRY (AML_BIT_NAND_OP,            0,                              0,                  ACPI_BTYPE_INTEGER),
/* NOOP */                      OP_TABLE_ENTRY (AML_NOOP_OP,                0,                              0,                  0),
/* NOR */                       OP_TABLE_ENTRY (AML_BIT_NOR_OP,             0,                              0,                  ACPI_BTYPE_INTEGER),
/* NOT */                       OP_TABLE_ENTRY (AML_BIT_NOT_OP,             0,                              0,                  ACPI_BTYPE_INTEGER),
/* NOTIFY */                    OP_TABLE_ENTRY (AML_NOTIFY_OP,              0,                              0,                  0),
/* OBJECTTYPE */                OP_TABLE_ENTRY (AML_TYPE_OP,                0,                              0,                  ACPI_BTYPE_INTEGER),
/* OBJECTTYPE_BFF */            OP_TABLE_ENTRY (AML_BYTE_OP,                ACPI_TYPE_BUFFER_FIELD,         0,                  0),
/* OBJECTTYPE_BUF */            OP_TABLE_ENTRY (AML_BYTE_OP,                ACPI_TYPE_BUFFER,               0,                  0),
/* OBJECTTYPE_DDB */            OP_TABLE_ENTRY (AML_BYTE_OP,                ACPI_TYPE_DDB_HANDLE,           0,                  0),
/* OBJECTTYPE_DEV */            OP_TABLE_ENTRY (AML_BYTE_OP,                ACPI_TYPE_DEVICE,               0,                  0),
/* OBJECTTYPE_EVT */            OP_TABLE_ENTRY (AML_BYTE_OP,                ACPI_TYPE_EVENT,                0,                  0),
/* OBJECTTYPE_FLD */            OP_TABLE_ENTRY (AML_BYTE_OP,                ACPI_TYPE_FIELD_UNIT,           0,                  0),
/* OBJECTTYPE_INT */            OP_TABLE_ENTRY (AML_BYTE_OP,                ACPI_TYPE_INTEGER,              0,                  0),
/* OBJECTTYPE_MTH */            OP_TABLE_ENTRY (AML_BYTE_OP,                ACPI_TYPE_METHOD,               0,                  0),
/* OBJECTTYPE_MTX */            OP_TABLE_ENTRY (AML_BYTE_OP,                ACPI_TYPE_MUTEX,                0,                  0),
/* OBJECTTYPE_OPR */            OP_TABLE_ENTRY (AML_BYTE_OP,                ACPI_TYPE_REGION,               0,                  0),
/* OBJECTTYPE_PKG */            OP_TABLE_ENTRY (AML_BYTE_OP,                ACPI_TYPE_PACKAGE,              0,                  0),
/* OBJECTTYPE_POW */            OP_TABLE_ENTRY (AML_BYTE_OP,                ACPI_TYPE_POWER,                0,                  0),
/* OBJECTTYPE_PRO */            OP_TABLE_ENTRY (AML_BYTE_OP,                ACPI_TYPE_PROCESSOR,            0,                  0),
/* OBJECTTYPE_STR */            OP_TABLE_ENTRY (AML_BYTE_OP,                ACPI_TYPE_STRING,               0,                  0),
/* OBJECTTYPE_THZ */            OP_TABLE_ENTRY (AML_BYTE_OP,                ACPI_TYPE_THERMAL,              0,                  0),
/* OBJECTTYPE_UNK */            OP_TABLE_ENTRY (AML_BYTE_OP,                ACPI_TYPE_ANY,                  0,                  0),
/* OFFSET */                    OP_TABLE_ENTRY (AML_INT_RESERVEDFIELD_OP,   0,                              0,                  0),
/* ONE */                       OP_TABLE_ENTRY (AML_ONE_OP,                 0,                              0,                  ACPI_BTYPE_INTEGER),
/* ONES */                      OP_TABLE_ENTRY (AML_ONES_OP,                0,                              0,                  ACPI_BTYPE_INTEGER),
/* OPERATIONREGION */           OP_TABLE_ENTRY (AML_REGION_OP,              0,                              0,                  0),
/* OR */                        OP_TABLE_ENTRY (AML_BIT_OR_OP,              0,                              0,                  ACPI_BTYPE_INTEGER),
/* PACKAGE */                   OP_TABLE_ENTRY (AML_PACKAGE_OP,             0,                              NODE_AML_PACKAGE,   ACPI_BTYPE_PACKAGE),
/* PACKAGEP_LENGTH */           OP_TABLE_ENTRY (AML_PACKAGE_LENGTH,         0,                              NODE_AML_PACKAGE,   0),
/* POWERRESOURCE */             OP_TABLE_ENTRY (AML_POWER_RES_OP,           0,                              NODE_AML_PACKAGE,   0),
/* PROCESSOR */                 OP_TABLE_ENTRY (AML_PROCESSOR_OP,           0,                              NODE_AML_PACKAGE,   0),
/* QWORDCONST */                OP_TABLE_ENTRY (AML_RAW_DATA_QWORD,         0,                              0,                  ACPI_BTYPE_INTEGER),
/* QWORDIO */                   OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* QWORDMEMORY */               OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* QWORDSPACE */                OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* RANGE_TYPE_ENTIRE */         OP_TABLE_ENTRY (AML_BYTE_OP,                3,                              0,                  0),
/* RANGE_TYPE_ISAONLY */        OP_TABLE_ENTRY (AML_BYTE_OP,                2,                              0,                  0),
/* RANGE_TYPE_NONISAONLY */     OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* RAW_DATA */                  OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* READWRITETYPE_BOTH */        OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* READWRITETYPE_READONLY */    OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* REFOF */                     OP_TABLE_ENTRY (AML_REF_OF_OP,              0,                              0,                  ACPI_BTYPE_REFERENCE),
/* REGIONSPACE_CMOS */          OP_TABLE_ENTRY (AML_RAW_DATA_BYTE,          REGION_CMOS,                    0,                  0),
/* REGIONSPACE_EC */            OP_TABLE_ENTRY (AML_RAW_DATA_BYTE,          REGION_EC,                      0,                  0),
/* REGIONSPACE_IO */            OP_TABLE_ENTRY (AML_RAW_DATA_BYTE,          REGION_IO,                      0,                  0),
/* REGIONSPACE_IPMI */          OP_TABLE_ENTRY (AML_RAW_DATA_BYTE,          REGION_IPMI,                    0,                  0),
/* REGIONSPACE_MEM */           OP_TABLE_ENTRY (AML_RAW_DATA_BYTE,          REGION_MEMORY,                  0,                  0),
/* REGIONSPACE_PCI */           OP_TABLE_ENTRY (AML_RAW_DATA_BYTE,          REGION_PCI_CONFIG,              0,                  0),
/* REGIONSPACE_PCIBAR */        OP_TABLE_ENTRY (AML_RAW_DATA_BYTE,          REGION_PCI_BAR,                 0,                  0),
/* REGIONSPACE_SMBUS */         OP_TABLE_ENTRY (AML_RAW_DATA_BYTE,          REGION_SMBUS,                   0,                  0),
/* REGISTER */                  OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* RELEASE */                   OP_TABLE_ENTRY (AML_RELEASE_OP,             0,                              0,                  0),
/* RESERVED_BYTES */            OP_TABLE_ENTRY (AML_INT_RESERVEDFIELD_OP,   0,                              0,                  0),
/* RESET */                     OP_TABLE_ENTRY (AML_RESET_OP,               0,                              0,                  0),
/* RESOURCETEMPLATE */          OP_TABLE_ENTRY (AML_BUFFER_OP,              0,                              0,                  ACPI_BTYPE_BUFFER),
/* RESOURCETYPE_CONSUMER */     OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* RESOURCETYPE_PRODUCER */     OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* RETURN */                    OP_TABLE_ENTRY (AML_RETURN_OP,              0,                              0,                  0),
/* REVISION */                  OP_TABLE_ENTRY (AML_REVISION_OP,            0,                              0,                  ACPI_BTYPE_INTEGER),
/* SCOPE */                     OP_TABLE_ENTRY (AML_SCOPE_OP,               0,                              NODE_AML_PACKAGE,   0),
/* SERIALIZERULE_NOTSERIAL */   OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* SERIALIZERULE_SERIAL */      OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* SHARETYPE_EXCLUSIVE */       OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* SHARETYPE_SHARED */          OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* SHIFTLEFT */                 OP_TABLE_ENTRY (AML_SHIFT_LEFT_OP,          0,                              0,                  ACPI_BTYPE_INTEGER),
/* SHIFTRIGHT */                OP_TABLE_ENTRY (AML_SHIFT_RIGHT_OP,         0,                              0,                  ACPI_BTYPE_INTEGER),
/* SIGNAL */                    OP_TABLE_ENTRY (AML_SIGNAL_OP,              0,                              0,                  0),
/* SIZEOF */                    OP_TABLE_ENTRY (AML_SIZE_OF_OP,             0,                              0,                  ACPI_BTYPE_INTEGER),
/* SLEEP */                     OP_TABLE_ENTRY (AML_SLEEP_OP,               0,                              0,                  0),
/* STALL */                     OP_TABLE_ENTRY (AML_STALL_OP,               0,                              0,                  0),
/* STARTDEPENDENTFN */          OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* STARTDEPENDENTFN_NOPRI */    OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* STORE */                     OP_TABLE_ENTRY (AML_STORE_OP,               0,                              0,                  ACPI_BTYPE_DATA_REFERENCE),
/* STRING_LITERAL */            OP_TABLE_ENTRY (AML_STRING_OP,              0,                              0,                  ACPI_BTYPE_STRING),
/* SUBTRACT */                  OP_TABLE_ENTRY (AML_SUBTRACT_OP,            0,                              0,                  ACPI_BTYPE_INTEGER),
/* SWITCH */                    OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* THERMALZONE */               OP_TABLE_ENTRY (AML_THERMAL_ZONE_OP,        0,                              NODE_AML_PACKAGE,   0),
/* TIMER */                     OP_TABLE_ENTRY (AML_TIMER_OP,               0,                              0,                  ACPI_BTYPE_INTEGER),
/* TOBCD */                     OP_TABLE_ENTRY (AML_TO_BCD_OP,              0,                              0,                  ACPI_BTYPE_INTEGER),
/* TOBUFFER */                  OP_TABLE_ENTRY (AML_TO_BUFFER_OP,           0,                              0,                  ACPI_BTYPE_BUFFER),
/* TODECIMALSTRING */           OP_TABLE_ENTRY (AML_TO_DECSTRING_OP,        0,                              0,                  ACPI_BTYPE_STRING),
/* TOHEXSTRING */               OP_TABLE_ENTRY (AML_TO_HEXSTRING_OP,        0,                              0,                  ACPI_BTYPE_STRING),
/* TOINTEGER */                 OP_TABLE_ENTRY (AML_TO_INTEGER_OP,          0,                              0,                  ACPI_BTYPE_INTEGER),
/* TOSTRING */                  OP_TABLE_ENTRY (AML_TO_STRING_OP,           0,                              0,                  ACPI_BTYPE_STRING),
/* TOUUID */                    OP_TABLE_ENTRY (AML_DWORD_OP,               0,                              NODE_AML_PACKAGE,   ACPI_BTYPE_INTEGER),
/* TRANSLATIONTYPE_DENSE */     OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* TRANSLATIONTYPE_SPARSE */    OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* TYPE_STATIC */               OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* TYPE_TRANSLATION */          OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* UNICODE */                   OP_TABLE_ENTRY (AML_BUFFER_OP,              0,                              NODE_AML_PACKAGE,   0),
/* UNLOAD */                    OP_TABLE_ENTRY (AML_UNLOAD_OP,              0,                              0,                  0),
/* UPDATERULE_ONES */           OP_TABLE_ENTRY (AML_BYTE_OP,                AML_FIELD_UPDATE_WRITE_AS_ONES, 0,                  0),
/* UPDATERULE_PRESERVE */       OP_TABLE_ENTRY (AML_BYTE_OP,                AML_FIELD_UPDATE_PRESERVE,      0,                  0),
/* UPDATERULE_ZEROS */          OP_TABLE_ENTRY (AML_BYTE_OP,                AML_FIELD_UPDATE_WRITE_AS_ZEROS,0,                  0),
/* VAR_PACKAGE */               OP_TABLE_ENTRY (AML_VAR_PACKAGE_OP,         0,                              NODE_AML_PACKAGE,   ACPI_BTYPE_PACKAGE),
/* VENDORLONG */                OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* VENDORSHORT */               OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* WAIT */                      OP_TABLE_ENTRY (AML_WAIT_OP,                0,                              0,                  ACPI_BTYPE_INTEGER),
/* WHILE */                     OP_TABLE_ENTRY (AML_WHILE_OP,               0,                              NODE_AML_PACKAGE,   0),
/* WORDBUSNUMBER */             OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* WORDCONST */                 OP_TABLE_ENTRY (AML_RAW_DATA_WORD,          0,                              0,                  ACPI_BTYPE_INTEGER),
/* WORDIO */                    OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* WORDSPACE */                 OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* XFERTYPE_8 */                OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* XFERTYPE_8_16 */             OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* XFERTYPE_16 */               OP_TABLE_ENTRY (AML_BYTE_OP,                2,                              0,                  0),
/* XOR */                       OP_TABLE_ENTRY (AML_BIT_XOR_OP,             0,                              0,                  ACPI_BTYPE_INTEGER),
/* ZERO */                      OP_TABLE_ENTRY (AML_ZERO_OP,                0,                              0,                  ACPI_BTYPE_INTEGER),

/*! [End] no source code translation !*/

};


