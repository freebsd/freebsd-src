/*******************************************************************************
 *
 * Module Name: utmisc - common utility procedures
 *
 ******************************************************************************/

/*
 * Copyright (C) 2000 - 2011, Intel Corp.
 * All rights reserved.
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
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */


#define __UTMISC_C__

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acnamesp.h>


#define _COMPONENT          ACPI_UTILITIES
        ACPI_MODULE_NAME    ("utmisc")


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtValidateException
 *
 * PARAMETERS:  Status       - The ACPI_STATUS code to be formatted
 *
 * RETURN:      A string containing the exception text. NULL if exception is
 *              not valid.
 *
 * DESCRIPTION: This function validates and translates an ACPI exception into
 *              an ASCII string.
 *
 ******************************************************************************/

const char *
AcpiUtValidateException (
    ACPI_STATUS             Status)
{
    UINT32                  SubStatus;
    const char              *Exception = NULL;


    ACPI_FUNCTION_ENTRY ();


    /*
     * Status is composed of two parts, a "type" and an actual code
     */
    SubStatus = (Status & ~AE_CODE_MASK);

    switch (Status & AE_CODE_MASK)
    {
    case AE_CODE_ENVIRONMENTAL:

        if (SubStatus <= AE_CODE_ENV_MAX)
        {
            Exception = AcpiGbl_ExceptionNames_Env [SubStatus];
        }
        break;

    case AE_CODE_PROGRAMMER:

        if (SubStatus <= AE_CODE_PGM_MAX)
        {
            Exception = AcpiGbl_ExceptionNames_Pgm [SubStatus];
        }
        break;

    case AE_CODE_ACPI_TABLES:

        if (SubStatus <= AE_CODE_TBL_MAX)
        {
            Exception = AcpiGbl_ExceptionNames_Tbl [SubStatus];
        }
        break;

    case AE_CODE_AML:

        if (SubStatus <= AE_CODE_AML_MAX)
        {
            Exception = AcpiGbl_ExceptionNames_Aml [SubStatus];
        }
        break;

    case AE_CODE_CONTROL:

        if (SubStatus <= AE_CODE_CTRL_MAX)
        {
            Exception = AcpiGbl_ExceptionNames_Ctrl [SubStatus];
        }
        break;

    default:
        break;
    }

    return (ACPI_CAST_PTR (const char, Exception));
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtIsPciRootBridge
 *
 * PARAMETERS:  Id              - The HID/CID in string format
 *
 * RETURN:      TRUE if the Id is a match for a PCI/PCI-Express Root Bridge
 *
 * DESCRIPTION: Determine if the input ID is a PCI Root Bridge ID.
 *
 ******************************************************************************/

BOOLEAN
AcpiUtIsPciRootBridge (
    char                    *Id)
{

    /*
     * Check if this is a PCI root bridge.
     * ACPI 3.0+: check for a PCI Express root also.
     */
    if (!(ACPI_STRCMP (Id,
            PCI_ROOT_HID_STRING)) ||

        !(ACPI_STRCMP (Id,
            PCI_EXPRESS_ROOT_HID_STRING)))
    {
        return (TRUE);
    }

    return (FALSE);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtIsAmlTable
 *
 * PARAMETERS:  Table               - An ACPI table
 *
 * RETURN:      TRUE if table contains executable AML; FALSE otherwise
 *
 * DESCRIPTION: Check ACPI Signature for a table that contains AML code.
 *              Currently, these are DSDT,SSDT,PSDT. All other table types are
 *              data tables that do not contain AML code.
 *
 ******************************************************************************/

BOOLEAN
AcpiUtIsAmlTable (
    ACPI_TABLE_HEADER       *Table)
{

    /* These are the only tables that contain executable AML */

    if (ACPI_COMPARE_NAME (Table->Signature, ACPI_SIG_DSDT) ||
        ACPI_COMPARE_NAME (Table->Signature, ACPI_SIG_PSDT) ||
        ACPI_COMPARE_NAME (Table->Signature, ACPI_SIG_SSDT))
    {
        return (TRUE);
    }

    return (FALSE);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtAllocateOwnerId
 *
 * PARAMETERS:  OwnerId         - Where the new owner ID is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Allocate a table or method owner ID. The owner ID is used to
 *              track objects created by the table or method, to be deleted
 *              when the method exits or the table is unloaded.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtAllocateOwnerId (
    ACPI_OWNER_ID           *OwnerId)
{
    UINT32                  i;
    UINT32                  j;
    UINT32                  k;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (UtAllocateOwnerId);


    /* Guard against multiple allocations of ID to the same location */

    if (*OwnerId)
    {
        ACPI_ERROR ((AE_INFO, "Owner ID [0x%2.2X] already exists", *OwnerId));
        return_ACPI_STATUS (AE_ALREADY_EXISTS);
    }

    /* Mutex for the global ID mask */

    Status = AcpiUtAcquireMutex (ACPI_MTX_CACHES);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /*
     * Find a free owner ID, cycle through all possible IDs on repeated
     * allocations. (ACPI_NUM_OWNERID_MASKS + 1) because first index may have
     * to be scanned twice.
     */
    for (i = 0, j = AcpiGbl_LastOwnerIdIndex;
         i < (ACPI_NUM_OWNERID_MASKS + 1);
         i++, j++)
    {
        if (j >= ACPI_NUM_OWNERID_MASKS)
        {
            j = 0;  /* Wraparound to start of mask array */
        }

        for (k = AcpiGbl_NextOwnerIdOffset; k < 32; k++)
        {
            if (AcpiGbl_OwnerIdMask[j] == ACPI_UINT32_MAX)
            {
                /* There are no free IDs in this mask */

                break;
            }

            if (!(AcpiGbl_OwnerIdMask[j] & (1 << k)))
            {
                /*
                 * Found a free ID. The actual ID is the bit index plus one,
                 * making zero an invalid Owner ID. Save this as the last ID
                 * allocated and update the global ID mask.
                 */
                AcpiGbl_OwnerIdMask[j] |= (1 << k);

                AcpiGbl_LastOwnerIdIndex = (UINT8) j;
                AcpiGbl_NextOwnerIdOffset = (UINT8) (k + 1);

                /*
                 * Construct encoded ID from the index and bit position
                 *
                 * Note: Last [j].k (bit 255) is never used and is marked
                 * permanently allocated (prevents +1 overflow)
                 */
                *OwnerId = (ACPI_OWNER_ID) ((k + 1) + ACPI_MUL_32 (j));

                ACPI_DEBUG_PRINT ((ACPI_DB_VALUES,
                    "Allocated OwnerId: %2.2X\n", (unsigned int) *OwnerId));
                goto Exit;
            }
        }

        AcpiGbl_NextOwnerIdOffset = 0;
    }

    /*
     * All OwnerIds have been allocated. This typically should
     * not happen since the IDs are reused after deallocation. The IDs are
     * allocated upon table load (one per table) and method execution, and
     * they are released when a table is unloaded or a method completes
     * execution.
     *
     * If this error happens, there may be very deep nesting of invoked control
     * methods, or there may be a bug where the IDs are not released.
     */
    Status = AE_OWNER_ID_LIMIT;
    ACPI_ERROR ((AE_INFO,
        "Could not allocate new OwnerId (255 max), AE_OWNER_ID_LIMIT"));

Exit:
    (void) AcpiUtReleaseMutex (ACPI_MTX_CACHES);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtReleaseOwnerId
 *
 * PARAMETERS:  OwnerIdPtr          - Pointer to a previously allocated OwnerID
 *
 * RETURN:      None. No error is returned because we are either exiting a
 *              control method or unloading a table. Either way, we would
 *              ignore any error anyway.
 *
 * DESCRIPTION: Release a table or method owner ID.  Valid IDs are 1 - 255
 *
 ******************************************************************************/

void
AcpiUtReleaseOwnerId (
    ACPI_OWNER_ID           *OwnerIdPtr)
{
    ACPI_OWNER_ID           OwnerId = *OwnerIdPtr;
    ACPI_STATUS             Status;
    UINT32                  Index;
    UINT32                  Bit;


    ACPI_FUNCTION_TRACE_U32 (UtReleaseOwnerId, OwnerId);


    /* Always clear the input OwnerId (zero is an invalid ID) */

    *OwnerIdPtr = 0;

    /* Zero is not a valid OwnerID */

    if (OwnerId == 0)
    {
        ACPI_ERROR ((AE_INFO, "Invalid OwnerId: 0x%2.2X", OwnerId));
        return_VOID;
    }

    /* Mutex for the global ID mask */

    Status = AcpiUtAcquireMutex (ACPI_MTX_CACHES);
    if (ACPI_FAILURE (Status))
    {
        return_VOID;
    }

    /* Normalize the ID to zero */

    OwnerId--;

    /* Decode ID to index/offset pair */

    Index = ACPI_DIV_32 (OwnerId);
    Bit = 1 << ACPI_MOD_32 (OwnerId);

    /* Free the owner ID only if it is valid */

    if (AcpiGbl_OwnerIdMask[Index] & Bit)
    {
        AcpiGbl_OwnerIdMask[Index] ^= Bit;
    }
    else
    {
        ACPI_ERROR ((AE_INFO,
            "Release of non-allocated OwnerId: 0x%2.2X", OwnerId + 1));
    }

    (void) AcpiUtReleaseMutex (ACPI_MTX_CACHES);
    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtStrupr (strupr)
 *
 * PARAMETERS:  SrcString       - The source string to convert
 *
 * RETURN:      None
 *
 * DESCRIPTION: Convert string to uppercase
 *
 * NOTE: This is not a POSIX function, so it appears here, not in utclib.c
 *
 ******************************************************************************/

void
AcpiUtStrupr (
    char                    *SrcString)
{
    char                    *String;


    ACPI_FUNCTION_ENTRY ();


    if (!SrcString)
    {
        return;
    }

    /* Walk entire string, uppercasing the letters */

    for (String = SrcString; *String; String++)
    {
        *String = (char) ACPI_TOUPPER (*String);
    }

    return;
}


#ifdef ACPI_ASL_COMPILER
/*******************************************************************************
 *
 * FUNCTION:    AcpiUtStrlwr (strlwr)
 *
 * PARAMETERS:  SrcString       - The source string to convert
 *
 * RETURN:      None
 *
 * DESCRIPTION: Convert string to lowercase
 *
 * NOTE: This is not a POSIX function, so it appears here, not in utclib.c
 *
 ******************************************************************************/

void
AcpiUtStrlwr (
    char                    *SrcString)
{
    char                    *String;


    ACPI_FUNCTION_ENTRY ();


    if (!SrcString)
    {
        return;
    }

    /* Walk entire string, lowercasing the letters */

    for (String = SrcString; *String; String++)
    {
        *String = (char) ACPI_TOLOWER (*String);
    }

    return;
}
#endif


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtPrintString
 *
 * PARAMETERS:  String          - Null terminated ASCII string
 *              MaxLength       - Maximum output length
 *
 * RETURN:      None
 *
 * DESCRIPTION: Dump an ASCII string with support for ACPI-defined escape
 *              sequences.
 *
 ******************************************************************************/

void
AcpiUtPrintString (
    char                    *String,
    UINT8                   MaxLength)
{
    UINT32                  i;


    if (!String)
    {
        AcpiOsPrintf ("<\"NULL STRING PTR\">");
        return;
    }

    AcpiOsPrintf ("\"");
    for (i = 0; String[i] && (i < MaxLength); i++)
    {
        /* Escape sequences */

        switch (String[i])
        {
        case 0x07:
            AcpiOsPrintf ("\\a");       /* BELL */
            break;

        case 0x08:
            AcpiOsPrintf ("\\b");       /* BACKSPACE */
            break;

        case 0x0C:
            AcpiOsPrintf ("\\f");       /* FORMFEED */
            break;

        case 0x0A:
            AcpiOsPrintf ("\\n");       /* LINEFEED */
            break;

        case 0x0D:
            AcpiOsPrintf ("\\r");       /* CARRIAGE RETURN*/
            break;

        case 0x09:
            AcpiOsPrintf ("\\t");       /* HORIZONTAL TAB */
            break;

        case 0x0B:
            AcpiOsPrintf ("\\v");       /* VERTICAL TAB */
            break;

        case '\'':                      /* Single Quote */
        case '\"':                      /* Double Quote */
        case '\\':                      /* Backslash */
            AcpiOsPrintf ("\\%c", (int) String[i]);
            break;

        default:

            /* Check for printable character or hex escape */

            if (ACPI_IS_PRINT (String[i]))
            {
                /* This is a normal character */

                AcpiOsPrintf ("%c", (int) String[i]);
            }
            else
            {
                /* All others will be Hex escapes */

                AcpiOsPrintf ("\\x%2.2X", (INT32) String[i]);
            }
            break;
        }
    }
    AcpiOsPrintf ("\"");

    if (i == MaxLength && String[i])
    {
        AcpiOsPrintf ("...");
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtDwordByteSwap
 *
 * PARAMETERS:  Value           - Value to be converted
 *
 * RETURN:      UINT32 integer with bytes swapped
 *
 * DESCRIPTION: Convert a 32-bit value to big-endian (swap the bytes)
 *
 ******************************************************************************/

UINT32
AcpiUtDwordByteSwap (
    UINT32                  Value)
{
    union
    {
        UINT32              Value;
        UINT8               Bytes[4];
    } Out;
    union
    {
        UINT32              Value;
        UINT8               Bytes[4];
    } In;


    ACPI_FUNCTION_ENTRY ();


    In.Value = Value;

    Out.Bytes[0] = In.Bytes[3];
    Out.Bytes[1] = In.Bytes[2];
    Out.Bytes[2] = In.Bytes[1];
    Out.Bytes[3] = In.Bytes[0];

    return (Out.Value);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtSetIntegerWidth
 *
 * PARAMETERS:  Revision            From DSDT header
 *
 * RETURN:      None
 *
 * DESCRIPTION: Set the global integer bit width based upon the revision
 *              of the DSDT.  For Revision 1 and 0, Integers are 32 bits.
 *              For Revision 2 and above, Integers are 64 bits.  Yes, this
 *              makes a difference.
 *
 ******************************************************************************/

void
AcpiUtSetIntegerWidth (
    UINT8                   Revision)
{

    if (Revision < 2)
    {
        /* 32-bit case */

        AcpiGbl_IntegerBitWidth    = 32;
        AcpiGbl_IntegerNybbleWidth = 8;
        AcpiGbl_IntegerByteWidth   = 4;
    }
    else
    {
        /* 64-bit case (ACPI 2.0+) */

        AcpiGbl_IntegerBitWidth    = 64;
        AcpiGbl_IntegerNybbleWidth = 16;
        AcpiGbl_IntegerByteWidth   = 8;
    }
}


#ifdef ACPI_DEBUG_OUTPUT
/*******************************************************************************
 *
 * FUNCTION:    AcpiUtDisplayInitPathname
 *
 * PARAMETERS:  Type                - Object type of the node
 *              ObjHandle           - Handle whose pathname will be displayed
 *              Path                - Additional path string to be appended.
 *                                      (NULL if no extra path)
 *
 * RETURN:      ACPI_STATUS
 *
 * DESCRIPTION: Display full pathname of an object, DEBUG ONLY
 *
 ******************************************************************************/

void
AcpiUtDisplayInitPathname (
    UINT8                   Type,
    ACPI_NAMESPACE_NODE     *ObjHandle,
    char                    *Path)
{
    ACPI_STATUS             Status;
    ACPI_BUFFER             Buffer;


    ACPI_FUNCTION_ENTRY ();


    /* Only print the path if the appropriate debug level is enabled */

    if (!(AcpiDbgLevel & ACPI_LV_INIT_NAMES))
    {
        return;
    }

    /* Get the full pathname to the node */

    Buffer.Length = ACPI_ALLOCATE_LOCAL_BUFFER;
    Status = AcpiNsHandleToPathname (ObjHandle, &Buffer);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Print what we're doing */

    switch (Type)
    {
    case ACPI_TYPE_METHOD:
        AcpiOsPrintf ("Executing    ");
        break;

    default:
        AcpiOsPrintf ("Initializing ");
        break;
    }

    /* Print the object type and pathname */

    AcpiOsPrintf ("%-12s  %s",
        AcpiUtGetTypeName (Type), (char *) Buffer.Pointer);

    /* Extra path is used to append names like _STA, _INI, etc. */

    if (Path)
    {
        AcpiOsPrintf (".%s", Path);
    }
    AcpiOsPrintf ("\n");

    ACPI_FREE (Buffer.Pointer);
}
#endif


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtValidAcpiChar
 *
 * PARAMETERS:  Char            - The character to be examined
 *              Position        - Byte position (0-3)
 *
 * RETURN:      TRUE if the character is valid, FALSE otherwise
 *
 * DESCRIPTION: Check for a valid ACPI character. Must be one of:
 *              1) Upper case alpha
 *              2) numeric
 *              3) underscore
 *
 *              We allow a '!' as the last character because of the ASF! table
 *
 ******************************************************************************/

BOOLEAN
AcpiUtValidAcpiChar (
    char                    Character,
    UINT32                  Position)
{

    if (!((Character >= 'A' && Character <= 'Z') ||
          (Character >= '0' && Character <= '9') ||
          (Character == '_')))
    {
        /* Allow a '!' in the last position */

        if (Character == '!' && Position == 3)
        {
            return (TRUE);
        }

        return (FALSE);
    }

    return (TRUE);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtValidAcpiName
 *
 * PARAMETERS:  Name            - The name to be examined
 *
 * RETURN:      TRUE if the name is valid, FALSE otherwise
 *
 * DESCRIPTION: Check for a valid ACPI name.  Each character must be one of:
 *              1) Upper case alpha
 *              2) numeric
 *              3) underscore
 *
 ******************************************************************************/

BOOLEAN
AcpiUtValidAcpiName (
    UINT32                  Name)
{
    UINT32                  i;


    ACPI_FUNCTION_ENTRY ();


    for (i = 0; i < ACPI_NAME_SIZE; i++)
    {
        if (!AcpiUtValidAcpiChar ((ACPI_CAST_PTR (char, &Name))[i], i))
        {
            return (FALSE);
        }
    }

    return (TRUE);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtRepairName
 *
 * PARAMETERS:  Name            - The ACPI name to be repaired
 *
 * RETURN:      Repaired version of the name
 *
 * DESCRIPTION: Repair an ACPI name: Change invalid characters to '*' and
 *              return the new name. NOTE: the Name parameter must reside in
 *              read/write memory, cannot be a const.
 *
 * An ACPI Name must consist of valid ACPI characters. We will repair the name
 * if necessary because we don't want to abort because of this, but we want
 * all namespace names to be printable. A warning message is appropriate.
 *
 * This issue came up because there are in fact machines that exhibit
 * this problem, and we want to be able to enable ACPI support for them,
 * even though there are a few bad names.
 *
 ******************************************************************************/

void
AcpiUtRepairName (
    char                    *Name)
{
    UINT32                  i;
    BOOLEAN                 FoundBadChar = FALSE;


    ACPI_FUNCTION_NAME (UtRepairName);


    /* Check each character in the name */

    for (i = 0; i < ACPI_NAME_SIZE; i++)
    {
        if (AcpiUtValidAcpiChar (Name[i], i))
        {
            continue;
        }

        /*
         * Replace a bad character with something printable, yet technically
         * still invalid. This prevents any collisions with existing "good"
         * names in the namespace.
         */
        Name[i] = '*';
        FoundBadChar = TRUE;
    }

    if (FoundBadChar)
    {
        /* Report warning only if in strict mode or debug mode */

        if (!AcpiGbl_EnableInterpreterSlack)
        {
            ACPI_WARNING ((AE_INFO,
                "Found bad character(s) in name, repaired: [%4.4s]\n", Name));
        }
        else
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_INFO,
                "Found bad character(s) in name, repaired: [%4.4s]\n", Name));
        }
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtStrtoul64
 *
 * PARAMETERS:  String          - Null terminated string
 *              Base            - Radix of the string: 16 or ACPI_ANY_BASE;
 *                                ACPI_ANY_BASE means 'in behalf of ToInteger'
 *              RetInteger      - Where the converted integer is returned
 *
 * RETURN:      Status and Converted value
 *
 * DESCRIPTION: Convert a string into an unsigned value. Performs either a
 *              32-bit or 64-bit conversion, depending on the current mode
 *              of the interpreter.
 *              NOTE: Does not support Octal strings, not needed.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtStrtoul64 (
    char                    *String,
    UINT32                  Base,
    UINT64                  *RetInteger)
{
    UINT32                  ThisDigit = 0;
    UINT64                  ReturnValue = 0;
    UINT64                  Quotient;
    UINT64                  Dividend;
    UINT32                  ToIntegerOp = (Base == ACPI_ANY_BASE);
    UINT32                  Mode32 = (AcpiGbl_IntegerByteWidth == 4);
    UINT8                   ValidDigits = 0;
    UINT8                   SignOf0x = 0;
    UINT8                   Term = 0;


    ACPI_FUNCTION_TRACE_STR (UtStroul64, String);


    switch (Base)
    {
    case ACPI_ANY_BASE:
    case 16:
        break;

    default:
        /* Invalid Base */
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    if (!String)
    {
        goto ErrorExit;
    }

    /* Skip over any white space in the buffer */

    while ((*String) && (ACPI_IS_SPACE (*String) || *String == '\t'))
    {
        String++;
    }

    if (ToIntegerOp)
    {
        /*
         * Base equal to ACPI_ANY_BASE means 'ToInteger operation case'.
         * We need to determine if it is decimal or hexadecimal.
         */
        if ((*String == '0') && (ACPI_TOLOWER (*(String + 1)) == 'x'))
        {
            SignOf0x = 1;
            Base = 16;

            /* Skip over the leading '0x' */
            String += 2;
        }
        else
        {
            Base = 10;
        }
    }

    /* Any string left? Check that '0x' is not followed by white space. */

    if (!(*String) || ACPI_IS_SPACE (*String) || *String == '\t')
    {
        if (ToIntegerOp)
        {
            goto ErrorExit;
        }
        else
        {
            goto AllDone;
        }
    }

    /*
     * Perform a 32-bit or 64-bit conversion, depending upon the current
     * execution mode of the interpreter
     */
    Dividend = (Mode32) ? ACPI_UINT32_MAX : ACPI_UINT64_MAX;

    /* Main loop: convert the string to a 32- or 64-bit integer */

    while (*String)
    {
        if (ACPI_IS_DIGIT (*String))
        {
            /* Convert ASCII 0-9 to Decimal value */

            ThisDigit = ((UINT8) *String) - '0';
        }
        else if (Base == 10)
        {
            /* Digit is out of range; possible in ToInteger case only */

            Term = 1;
        }
        else
        {
            ThisDigit = (UINT8) ACPI_TOUPPER (*String);
            if (ACPI_IS_XDIGIT ((char) ThisDigit))
            {
                /* Convert ASCII Hex char to value */

                ThisDigit = ThisDigit - 'A' + 10;
            }
            else
            {
                Term = 1;
            }
        }

        if (Term)
        {
            if (ToIntegerOp)
            {
                goto ErrorExit;
            }
            else
            {
                break;
            }
        }
        else if ((ValidDigits == 0) && (ThisDigit == 0) && !SignOf0x)
        {
            /* Skip zeros */
            String++;
            continue;
        }

        ValidDigits++;

        if (SignOf0x && ((ValidDigits > 16) || ((ValidDigits > 8) && Mode32)))
        {
            /*
             * This is ToInteger operation case.
             * No any restrictions for string-to-integer conversion,
             * see ACPI spec.
             */
            goto ErrorExit;
        }

        /* Divide the digit into the correct position */

        (void) AcpiUtShortDivide ((Dividend - (UINT64) ThisDigit),
                    Base, &Quotient, NULL);

        if (ReturnValue > Quotient)
        {
            if (ToIntegerOp)
            {
                goto ErrorExit;
            }
            else
            {
                break;
            }
        }

        ReturnValue *= Base;
        ReturnValue += ThisDigit;
        String++;
    }

    /* All done, normal exit */

AllDone:

    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "Converted value: %8.8X%8.8X\n",
        ACPI_FORMAT_UINT64 (ReturnValue)));

    *RetInteger = ReturnValue;
    return_ACPI_STATUS (AE_OK);


ErrorExit:
    /* Base was set/validated above */

    if (Base == 10)
    {
        return_ACPI_STATUS (AE_BAD_DECIMAL_CONSTANT);
    }
    else
    {
        return_ACPI_STATUS (AE_BAD_HEX_CONSTANT);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtCreateUpdateStateAndPush
 *
 * PARAMETERS:  Object          - Object to be added to the new state
 *              Action          - Increment/Decrement
 *              StateList       - List the state will be added to
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create a new state and push it
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtCreateUpdateStateAndPush (
    ACPI_OPERAND_OBJECT     *Object,
    UINT16                  Action,
    ACPI_GENERIC_STATE      **StateList)
{
    ACPI_GENERIC_STATE       *State;


    ACPI_FUNCTION_ENTRY ();


    /* Ignore null objects; these are expected */

    if (!Object)
    {
        return (AE_OK);
    }

    State = AcpiUtCreateUpdateState (Object, Action);
    if (!State)
    {
        return (AE_NO_MEMORY);
    }

    AcpiUtPushGenericState (StateList, State);
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtWalkPackageTree
 *
 * PARAMETERS:  SourceObject        - The package to walk
 *              TargetObject        - Target object (if package is being copied)
 *              WalkCallback        - Called once for each package element
 *              Context             - Passed to the callback function
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Walk through a package
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtWalkPackageTree (
    ACPI_OPERAND_OBJECT     *SourceObject,
    void                    *TargetObject,
    ACPI_PKG_CALLBACK       WalkCallback,
    void                    *Context)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_GENERIC_STATE      *StateList = NULL;
    ACPI_GENERIC_STATE      *State;
    UINT32                  ThisIndex;
    ACPI_OPERAND_OBJECT     *ThisSourceObj;


    ACPI_FUNCTION_TRACE (UtWalkPackageTree);


    State = AcpiUtCreatePkgState (SourceObject, TargetObject, 0);
    if (!State)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    while (State)
    {
        /* Get one element of the package */

        ThisIndex     = State->Pkg.Index;
        ThisSourceObj = (ACPI_OPERAND_OBJECT *)
                        State->Pkg.SourceObject->Package.Elements[ThisIndex];

        /*
         * Check for:
         * 1) An uninitialized package element.  It is completely
         *    legal to declare a package and leave it uninitialized
         * 2) Not an internal object - can be a namespace node instead
         * 3) Any type other than a package.  Packages are handled in else
         *    case below.
         */
        if ((!ThisSourceObj) ||
            (ACPI_GET_DESCRIPTOR_TYPE (ThisSourceObj) != ACPI_DESC_TYPE_OPERAND) ||
            (ThisSourceObj->Common.Type != ACPI_TYPE_PACKAGE))
        {
            Status = WalkCallback (ACPI_COPY_TYPE_SIMPLE, ThisSourceObj,
                                    State, Context);
            if (ACPI_FAILURE (Status))
            {
                return_ACPI_STATUS (Status);
            }

            State->Pkg.Index++;
            while (State->Pkg.Index >= State->Pkg.SourceObject->Package.Count)
            {
                /*
                 * We've handled all of the objects at this level,  This means
                 * that we have just completed a package.  That package may
                 * have contained one or more packages itself.
                 *
                 * Delete this state and pop the previous state (package).
                 */
                AcpiUtDeleteGenericState (State);
                State = AcpiUtPopGenericState (&StateList);

                /* Finished when there are no more states */

                if (!State)
                {
                    /*
                     * We have handled all of the objects in the top level
                     * package just add the length of the package objects
                     * and exit
                     */
                    return_ACPI_STATUS (AE_OK);
                }

                /*
                 * Go back up a level and move the index past the just
                 * completed package object.
                 */
                State->Pkg.Index++;
            }
        }
        else
        {
            /* This is a subobject of type package */

            Status = WalkCallback (ACPI_COPY_TYPE_PACKAGE, ThisSourceObj,
                                        State, Context);
            if (ACPI_FAILURE (Status))
            {
                return_ACPI_STATUS (Status);
            }

            /*
             * Push the current state and create a new one
             * The callback above returned a new target package object.
             */
            AcpiUtPushGenericState (&StateList, State);
            State = AcpiUtCreatePkgState (ThisSourceObj,
                                            State->Pkg.ThisTargetObj, 0);
            if (!State)
            {
                /* Free any stacked Update State objects */

                while (StateList)
                {
                    State = AcpiUtPopGenericState (&StateList);
                    AcpiUtDeleteGenericState (State);
                }
                return_ACPI_STATUS (AE_NO_MEMORY);
            }
        }
    }

    /* We should never get here */

    return_ACPI_STATUS (AE_AML_INTERNAL);
}


