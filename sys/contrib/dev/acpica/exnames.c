
/******************************************************************************
 *
 * Module Name: exnames - interpreter/scanner name load/execute
 *              $Revision: 90 $
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

#define __EXNAMES_C__

#include "acpi.h"
#include "acinterp.h"
#include "amlcode.h"
#include "acnamesp.h"

#define _COMPONENT          ACPI_EXECUTER
        ACPI_MODULE_NAME    ("exnames")


/* AML Package Length encodings */

#define ACPI_AML_PACKAGE_TYPE1   0x40
#define ACPI_AML_PACKAGE_TYPE2   0x4000
#define ACPI_AML_PACKAGE_TYPE3   0x400000
#define ACPI_AML_PACKAGE_TYPE4   0x40000000


/*******************************************************************************
 *
 * FUNCTION:    AcpiExAllocateNameString
 *
 * PARAMETERS:  PrefixCount         - Count of parent levels. Special cases:
 *                                    (-1) = root,  0 = none
 *              NumNameSegs         - count of 4-character name segments
 *
 * RETURN:      A pointer to the allocated string segment.  This segment must
 *              be deleted by the caller.
 *
 * DESCRIPTION: Allocate a buffer for a name string. Ensure allocated name
 *              string is long enough, and set up prefix if any.
 *
 ******************************************************************************/

NATIVE_CHAR *
AcpiExAllocateNameString (
    UINT32                  PrefixCount,
    UINT32                  NumNameSegs)
{
    NATIVE_CHAR             *TempPtr;
    NATIVE_CHAR             *NameString;
    UINT32                   SizeNeeded;

    ACPI_FUNCTION_TRACE ("ExAllocateNameString");


    /*
     * Allow room for all \ and ^ prefixes, all segments, and a MultiNamePrefix.
     * Also, one byte for the null terminator.
     * This may actually be somewhat longer than needed.
     */
    if (PrefixCount == ACPI_UINT32_MAX)
    {
        /* Special case for root */

        SizeNeeded = 1 + (ACPI_NAME_SIZE * NumNameSegs) + 2 + 1;
    }
    else
    {
        SizeNeeded = PrefixCount + (ACPI_NAME_SIZE * NumNameSegs) + 2 + 1;
    }

    /*
     * Allocate a buffer for the name.
     * This buffer must be deleted by the caller!
     */
    NameString = ACPI_MEM_ALLOCATE (SizeNeeded);
    if (!NameString)
    {
        ACPI_REPORT_ERROR (("ExAllocateNameString: Could not allocate size %d\n", SizeNeeded));
        return_PTR (NULL);
    }

    TempPtr = NameString;

    /* Set up Root or Parent prefixes if needed */

    if (PrefixCount == ACPI_UINT32_MAX)
    {
        *TempPtr++ = AML_ROOT_PREFIX;
    }
    else
    {
        while (PrefixCount--)
        {
            *TempPtr++ = AML_PARENT_PREFIX;
        }
    }


    /* Set up Dual or Multi prefixes if needed */

    if (NumNameSegs > 2)
    {
        /* Set up multi prefixes   */

        *TempPtr++ = AML_MULTI_NAME_PREFIX_OP;
        *TempPtr++ = (char) NumNameSegs;
    }
    else if (2 == NumNameSegs)
    {
        /* Set up dual prefixes */

        *TempPtr++ = AML_DUAL_NAME_PREFIX;
    }

    /*
     * Terminate string following prefixes. AcpiExNameSegment() will
     * append the segment(s)
     */
    *TempPtr = 0;

    return_PTR (NameString);
}

/*******************************************************************************
 *
 * FUNCTION:    AcpiExNameSegment
 *
 * PARAMETERS:  InterpreterMode     - Current running mode (load1/Load2/Exec)
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute a name segment (4 bytes)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExNameSegment (
    UINT8                   **InAmlAddress,
    NATIVE_CHAR             *NameString)
{
    UINT8                   *AmlAddress = *InAmlAddress;
    ACPI_STATUS             Status = AE_OK;
    UINT32                  Index;
    NATIVE_CHAR             CharBuf[5];


    ACPI_FUNCTION_TRACE ("ExNameSegment");


    /*
     * If first character is a digit, then we know that we aren't looking at a
     * valid name segment
     */
    CharBuf[0] = *AmlAddress;

    if ('0' <= CharBuf[0] && CharBuf[0] <= '9')
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "leading digit: %c\n", CharBuf[0]));
        return_ACPI_STATUS (AE_CTRL_PENDING);
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_LOAD, "Bytes from stream:\n"));

    for (Index = 4;
        (Index > 0) && (AcpiUtValidAcpiCharacter (*AmlAddress));
        Index--)
    {
        CharBuf[4 - Index] = *AmlAddress++;
        ACPI_DEBUG_PRINT ((ACPI_DB_LOAD, "%c\n", CharBuf[4 - Index]));
    }


    /* Valid name segment  */

    if (0 == Index)
    {
        /* Found 4 valid characters */

        CharBuf[4] = '\0';

        if (NameString)
        {
            ACPI_STRCAT (NameString, CharBuf);
            ACPI_DEBUG_PRINT ((ACPI_DB_NAMES,
                "Appended to - %s \n", NameString));
        }
        else
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_NAMES,
                "No Name string - %s \n", CharBuf));
        }
    }
    else if (4 == Index)
    {
        /*
         * First character was not a valid name character,
         * so we are looking at something other than a name.
         */
        ACPI_DEBUG_PRINT ((ACPI_DB_INFO,
            "Leading character is not alpha: %02Xh (not a name)\n",
            CharBuf[0]));
        Status = AE_CTRL_PENDING;
    }
    else
    {
        /* Segment started with one or more valid characters, but fewer than 4 */

        Status = AE_AML_BAD_NAME;
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Bad character %02x in name, at %p\n",
            *AmlAddress, AmlAddress));
    }

    *InAmlAddress = AmlAddress;
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExGetNameString
 *
 * PARAMETERS:  DataType            - Data type to be associated with this name
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Get a name, including any prefixes.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExGetNameString (
    ACPI_OBJECT_TYPE        DataType,
    UINT8                   *InAmlAddress,
    NATIVE_CHAR             **OutNameString,
    UINT32                  *OutNameLength)
{
    ACPI_STATUS             Status = AE_OK;
    UINT8                   *AmlAddress = InAmlAddress;
    NATIVE_CHAR             *NameString = NULL;
    UINT32                  NumSegments;
    UINT32                  PrefixCount = 0;
    BOOLEAN                 HasPrefix = FALSE;


    ACPI_FUNCTION_TRACE_PTR ("ExGetNameString", AmlAddress);


    if (INTERNAL_TYPE_REGION_FIELD == DataType   ||
        INTERNAL_TYPE_BANK_FIELD == DataType     ||
        INTERNAL_TYPE_INDEX_FIELD == DataType)
    {
        /* Disallow prefixes for types associated with FieldUnit names */

        NameString = AcpiExAllocateNameString (0, 1);
        if (!NameString)
        {
            Status = AE_NO_MEMORY;
        }
        else
        {
            Status = AcpiExNameSegment (&AmlAddress, NameString);
        }
    }
    else
    {
        /*
         * DataType is not a field name.
         * Examine first character of name for root or parent prefix operators
         */
        switch (*AmlAddress)
        {
        case AML_ROOT_PREFIX:

            ACPI_DEBUG_PRINT ((ACPI_DB_LOAD, "RootPrefix(\\) at %p\n", AmlAddress));

            /*
             * Remember that we have a RootPrefix --
             * see comment in AcpiExAllocateNameString()
             */
            AmlAddress++;
            PrefixCount = ACPI_UINT32_MAX;
            HasPrefix = TRUE;
            break;


        case AML_PARENT_PREFIX:

            /* Increment past possibly multiple parent prefixes */

            do
            {
                ACPI_DEBUG_PRINT ((ACPI_DB_LOAD, "ParentPrefix (^) at %p\n", AmlAddress));

                AmlAddress++;
                PrefixCount++;

            } while (*AmlAddress == AML_PARENT_PREFIX);

            HasPrefix = TRUE;
            break;


        default:

            /* Not a prefix character */

            break;
        }


        /* Examine first character of name for name segment prefix operator */

        switch (*AmlAddress)
        {
        case AML_DUAL_NAME_PREFIX:

            ACPI_DEBUG_PRINT ((ACPI_DB_LOAD, "DualNamePrefix at %p\n", AmlAddress));

            AmlAddress++;
            NameString = AcpiExAllocateNameString (PrefixCount, 2);
            if (!NameString)
            {
                Status = AE_NO_MEMORY;
                break;
            }

            /* Indicate that we processed a prefix */

            HasPrefix = TRUE;

            Status = AcpiExNameSegment (&AmlAddress, NameString);
            if (ACPI_SUCCESS (Status))
            {
                Status = AcpiExNameSegment (&AmlAddress, NameString);
            }
            break;


        case AML_MULTI_NAME_PREFIX_OP:

            ACPI_DEBUG_PRINT ((ACPI_DB_LOAD, "MultiNamePrefix at %p\n", AmlAddress));

            /* Fetch count of segments remaining in name path */

            AmlAddress++;
            NumSegments = *AmlAddress;

            NameString = AcpiExAllocateNameString (PrefixCount, NumSegments);
            if (!NameString)
            {
                Status = AE_NO_MEMORY;
                break;
            }

            /* Indicate that we processed a prefix */

            AmlAddress++;
            HasPrefix = TRUE;

            while (NumSegments &&
                    (Status = AcpiExNameSegment (&AmlAddress, NameString)) == AE_OK)
            {
                NumSegments--;
            }

            break;


        case 0:

            /* NullName valid as of 8-12-98 ASL/AML Grammar Update */

            if (PrefixCount == ACPI_UINT32_MAX)
            {
                ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "NameSeg is \"\\\" followed by NULL\n"));
            }

            /* Consume the NULL byte */

            AmlAddress++;
            NameString = AcpiExAllocateNameString (PrefixCount, 0);
            if (!NameString)
            {
                Status = AE_NO_MEMORY;
                break;
            }

            break;


        default:

            /* Name segment string */

            NameString = AcpiExAllocateNameString (PrefixCount, 1);
            if (!NameString)
            {
                Status = AE_NO_MEMORY;
                break;
            }

            Status = AcpiExNameSegment (&AmlAddress, NameString);
            break;
        }
    }

    if (AE_CTRL_PENDING == Status && HasPrefix)
    {
        /* Ran out of segments after processing a prefix */

        ACPI_REPORT_ERROR (
            ("ExDoName: Malformed Name at %p\n", NameString));
        Status = AE_AML_BAD_NAME;
    }

    *OutNameString = NameString;
    *OutNameLength = (UINT32) (AmlAddress - InAmlAddress);

    return_ACPI_STATUS (Status);
}


