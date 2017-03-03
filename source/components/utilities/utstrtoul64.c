/*******************************************************************************
 *
 * Module Name: utstrtoul64 - string to 64-bit integer support
 *
 ******************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2017, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights. You may have additional license terms from the party that provided
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
 * to or modifications of the Original Intel Code. No other license or right
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
 * and the following Disclaimer and Export Compliance provision. In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change. Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee. Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution. In
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
 * HERE. ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT, ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES. INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS. INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES. THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government. In the
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
 *****************************************************************************
 *
 * Alternatively, you may choose to be licensed under the terms of the
 * following license:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Alternatively, you may choose to be licensed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 *****************************************************************************/

#include "acpi.h"
#include "accommon.h"


/*******************************************************************************
 *
 * The functions in this module satisfy the need for 64-bit string-to-integer
 * conversions on both 32-bit and 64-bit platforms.
 *
 ******************************************************************************/

#define _COMPONENT          ACPI_UTILITIES
        ACPI_MODULE_NAME    ("utstrtoul64")

/* Local prototypes */

static UINT64
AcpiUtStrtoulBase10 (
    char                    *String,
    UINT32                  Flags);

static UINT64
AcpiUtStrtoulBase16 (
    char                    *String,
    UINT32                  Flags);


/*******************************************************************************
 *
 * String conversion rules as written in the ACPI specification. The error
 * conditions and behavior are different depending on the type of conversion.
 *
 *
 * Implicit data type conversion: string-to-integer
 * --------------------------------------------------
 *
 * Base is always 16. This is the ACPI_STRTOUL_BASE16 case.
 *
 * Example:
 *      Add ("BA98", Arg0, Local0)
 *
 * The integer is initialized to the value zero.
 * The ASCII string is interpreted as a hexadecimal constant.
 *
 *  1)  A "0x" prefix is not allowed. However, ACPICA allows this for
 *      compatibility with previous ACPICA. (NO ERROR)
 *
 *  2)  Terminates when the size of an integer is reached (32 or 64 bits).
 *      (NO ERROR)
 *
 *  3)  The first non-hex character terminates the conversion without error.
 *      (NO ERROR)
 *
 *  4)  Conversion of a null (zero-length) string to an integer is not
 *      allowed. However, ACPICA allows this for compatibility with previous
 *      ACPICA. This conversion returns the value 0. (NO ERROR)
 *
 *
 * Explicit data type conversion:  ToInteger() with string operand
 * ---------------------------------------------------------------
 *
 * Base is either 10 (default) or 16 (with 0x prefix)
 *
 * Examples:
 *      ToInteger ("1000")
 *      ToInteger ("0xABCD")
 *
 *  1)  Can be (must be) either a decimal or hexadecimal numeric string.
 *      A hex value must be prefixed by "0x" or it is interpreted as a decimal.
 *
 *  2)  The value must not exceed the maximum of an integer value. ACPI spec
 *      states the behavior is "unpredictable", so ACPICA matches the behavior
 *      of the implicit conversion case.(NO ERROR)
 *
 *  3)  Behavior on the first non-hex character is not specified by the ACPI
 *      spec, so ACPICA matches the behavior of the implicit conversion case
 *      and terminates. (NO ERROR)
 *
 *  4)  A null (zero-length) string is illegal.
 *      However, ACPICA allows this for compatibility with previous ACPICA.
 *      This conversion returns the value 0. (NO ERROR)
 *
 ******************************************************************************/


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtStrtoul64
 *
 * PARAMETERS:  String                  - Null terminated input string
 *              Flags                   - Conversion info, see below
 *              ReturnValue             - Where the converted integer is
 *                                        returned
 *
 * RETURN:      Status and Converted value
 *
 * DESCRIPTION: Convert a string into an unsigned value. Performs either a
 *              32-bit or 64-bit conversion, depending on the input integer
 *              size in Flags (often the current mode of the interpreter).
 *
 * Values for Flags:
 *      ACPI_STRTOUL_32BIT      - Max integer value is 32 bits
 *      ACPI_STRTOUL_64BIT      - Max integer value is 64 bits
 *      ACPI_STRTOUL_BASE16     - Input string is hexadecimal. Default
 *                                is 10/16 based on string prefix (0x).
 *
 * NOTES:
 *   Negative numbers are not supported, as they are not supported by ACPI.
 *
 *   Supports only base 16 or base 10 strings/values. Does not
 *   support Octal strings, as these are not supported by ACPI.
 *
 * Current users of this support:
 *
 *  Interpreter - Implicit and explicit conversions, GPE method names
 *  Debugger    - Command line input string conversion
 *  iASL        - Main parser, conversion of constants to integers
 *  iASL        - Data Table Compiler parser (constant math expressions)
 *  iASL        - Preprocessor (constant math expressions)
 *  AcpiDump    - Input table addresses
 *  AcpiExec    - Testing of the AcpiUtStrtoul64 function
 *
 * Note concerning callers:
 *   AcpiGbl_IntegerByteWidth can be used to set the 32/64 limit. If used,
 *   this global should be set to the proper width. For the core ACPICA code,
 *   this width depends on the DSDT version. For iASL, the default byte
 *   width is always 8 for the parser, but error checking is performed later
 *   to flag cases where a 64-bit constant is defined in a 32-bit DSDT/SSDT.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtStrtoul64 (
    char                    *String,
    UINT32                  Flags,
    UINT64                  *ReturnValue)
{
    ACPI_STATUS             Status = AE_OK;
    UINT32                  Base;


    ACPI_FUNCTION_TRACE_STR (UtStrtoul64, String);


    /* Parameter validation */

    if (!String || !ReturnValue)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    *ReturnValue = 0;

    /* Check for zero-length string, returns 0 */

    if (*String == 0)
    {
        return_ACPI_STATUS (AE_OK);
    }

    /* Skip over any white space at start of string */

    while (isspace ((int) *String))
    {
        String++;
    }

    /* End of string? return 0 */

    if (*String == 0)
    {
        return_ACPI_STATUS (AE_OK);
    }

    /*
     * 1) The "0x" prefix indicates base 16. Per the ACPI specification,
     * the "0x" prefix is only allowed for implicit (non-strict) conversions.
     * However, we always allow it for compatibility with older ACPICA.
     */
    if ((*String == ACPI_ASCII_ZERO) &&
        (tolower ((int) *(String + 1)) == 'x'))
    {
        String += 2;    /* Go past the 0x */
        if (*String == 0)
        {
            return_ACPI_STATUS (AE_OK);     /* Return value 0 */
        }

        Base = 16;
    }

    /* 2) Force to base 16 (implicit conversion case) */

    else if (Flags & ACPI_STRTOUL_BASE16)
    {
        Base = 16;
    }

    /* 3) Default fallback is to Base 10 */

    else
    {
        Base = 10;
    }

    /* Skip all leading zeros */

    while (*String == ACPI_ASCII_ZERO)
    {
        String++;
        if (*String == 0)
        {
            return_ACPI_STATUS (AE_OK);     /* Return value 0 */
        }
    }

    /* Perform the base 16 or 10 conversion */

    if (Base == 16)
    {
        *ReturnValue = AcpiUtStrtoulBase16 (String, Flags);
    }
    else
    {
        *ReturnValue = AcpiUtStrtoulBase10 (String, Flags);
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtStrtoulBase10
 *
 * PARAMETERS:  String                  - Null terminated input string
 *              Flags                   - Conversion info
 *
 * RETURN:      64-bit converted integer
 *
 * DESCRIPTION: Performs a base 10 conversion of the input string to an
 *              integer value, either 32 or 64 bits.
 *              Note: String must be valid and non-null.
 *
 ******************************************************************************/

static UINT64
AcpiUtStrtoulBase10 (
    char                    *String,
    UINT32                  Flags)
{
    int                     AsciiDigit;
    UINT64                  NextValue;
    UINT64                  ReturnValue = 0;


    /* Main loop: convert each ASCII byte in the input string */

    while (*String)
    {
        AsciiDigit = *String;
        if (!isdigit (AsciiDigit))
        {
            /* Not ASCII 0-9, terminate */

            goto Exit;
        }

        /* Convert and insert (add) the decimal digit */

        NextValue =
            (ReturnValue * 10) + (AsciiDigit - ACPI_ASCII_ZERO);

        /* Check for overflow (32 or 64 bit) - return current converted value */

        if (((Flags & ACPI_STRTOUL_32BIT) && (NextValue > ACPI_UINT32_MAX)) ||
            (NextValue < ReturnValue)) /* 64-bit overflow case */
        {
            goto Exit;
        }

        ReturnValue = NextValue;
        String++;
    }

Exit:
    return (ReturnValue);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtStrtoulBase16
 *
 * PARAMETERS:  String                  - Null terminated input string
 *              Flags                   - conversion info
 *
 * RETURN:      64-bit converted integer
 *
 * DESCRIPTION: Performs a base 16 conversion of the input string to an
 *              integer value, either 32 or 64 bits.
 *              Note: String must be valid and non-null.
 *
 ******************************************************************************/

static UINT64
AcpiUtStrtoulBase16 (
    char                    *String,
    UINT32                  Flags)
{
    int                     AsciiDigit;
    UINT32                  ValidDigits = 1;
    UINT64                  ReturnValue = 0;


    /* Main loop: convert each ASCII byte in the input string */

    while (*String)
    {
        /* Check for overflow (32 or 64 bit) - return current converted value */

        if ((ValidDigits > 16) ||
            ((ValidDigits > 8) && (Flags & ACPI_STRTOUL_32BIT)))
        {
            goto Exit;
        }

        AsciiDigit = *String;
        if (!isxdigit (AsciiDigit))
        {
            /* Not Hex ASCII A-F, a-f, or 0-9, terminate */

            goto Exit;
        }

        /* Convert and insert the hex digit */

        ReturnValue =
            (ReturnValue << 4) | AcpiUtAsciiCharToHex (AsciiDigit);

        String++;
        ValidDigits++;
    }

Exit:
    return (ReturnValue);
}
