/*******************************************************************************
 *
 * Module Name: utmisc - common utility procedures
 *              $Revision: 1.125 $
 *
 ******************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2005, Intel Corp.
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


#define __UTMISC_C__

#include <contrib/dev/acpica/acpi.h>
#include <contrib/dev/acpica/acnamesp.h>
#include <contrib/dev/acpica/amlresrc.h>


#define _COMPONENT          ACPI_UTILITIES
        ACPI_MODULE_NAME    ("utmisc")


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
    ACPI_NATIVE_UINT        i;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("UtAllocateOwnerId");


    /* Guard against multiple allocations of ID to the same location */

    if (*OwnerId)
    {
        ACPI_REPORT_ERROR (("Owner ID [%2.2X] already exists\n", *OwnerId));
        return_ACPI_STATUS (AE_ALREADY_EXISTS);
    }

    /* Mutex for the global ID mask */

    Status = AcpiUtAcquireMutex (ACPI_MTX_CACHES);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Find a free owner ID */

    for (i = 0; i < 32; i++)
    {
        if (!(AcpiGbl_OwnerIdMask & (1 << i)))
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_VALUES,
                "Current OwnerId mask: %8.8X New ID: %2.2X\n",
                AcpiGbl_OwnerIdMask, (unsigned int) (i + 1)));

            AcpiGbl_OwnerIdMask |= (1 << i);
            *OwnerId = (ACPI_OWNER_ID) (i + 1);
            goto Exit;
        }
    }

    /*
     * If we are here, all OwnerIds have been allocated. This probably should
     * not happen since the IDs are reused after deallocation. The IDs are
     * allocated upon table load (one per table) and method execution, and
     * they are released when a table is unloaded or a method completes
     * execution.
     */
    *OwnerId = 0;
    Status = AE_OWNER_ID_LIMIT;
    ACPI_REPORT_ERROR ((
        "Could not allocate new OwnerId (32 max), AE_OWNER_ID_LIMIT\n"));

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
 * DESCRIPTION: Release a table or method owner ID.  Valid IDs are 1 - 32
 *
 ******************************************************************************/

void
AcpiUtReleaseOwnerId (
    ACPI_OWNER_ID           *OwnerIdPtr)
{
    ACPI_OWNER_ID           OwnerId = *OwnerIdPtr;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE_U32 ("UtReleaseOwnerId", OwnerId);


    /* Always clear the input OwnerId (zero is an invalid ID) */

    *OwnerIdPtr = 0;

    /* Zero is not a valid OwnerID */

    if ((OwnerId == 0) || (OwnerId > 32))
    {
        ACPI_REPORT_ERROR (("Invalid OwnerId: %2.2X\n", OwnerId));
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

    /* Free the owner ID only if it is valid */

    if (AcpiGbl_OwnerIdMask & (1 << OwnerId))
    {
        AcpiGbl_OwnerIdMask ^= (1 << OwnerId);
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
            AcpiOsPrintf ("\\a");        /* BELL */
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

    if (Revision <= 1)
    {
        AcpiGbl_IntegerBitWidth    = 32;
        AcpiGbl_IntegerNybbleWidth = 8;
        AcpiGbl_IntegerByteWidth   = 4;
    }
    else
    {
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

    ACPI_MEM_FREE (Buffer.Pointer);
}
#endif


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
    char                    *NamePtr = (char *) &Name;
    char                    Character;
    ACPI_NATIVE_UINT        i;


    ACPI_FUNCTION_ENTRY ();


    for (i = 0; i < ACPI_NAME_SIZE; i++)
    {
        Character = *NamePtr;
        NamePtr++;

        if (!((Character == '_') ||
              (Character >= 'A' && Character <= 'Z') ||
              (Character >= '0' && Character <= '9')))
        {
            return (FALSE);
        }
    }

    return (TRUE);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtValidAcpiCharacter
 *
 * PARAMETERS:  Character           - The character to be examined
 *
 * RETURN:      1 if Character may appear in a name, else 0
 *
 * DESCRIPTION: Check for a printable character
 *
 ******************************************************************************/

BOOLEAN
AcpiUtValidAcpiCharacter (
    char                    Character)
{

    ACPI_FUNCTION_ENTRY ();

    return ((BOOLEAN)   ((Character == '_') ||
                        (Character >= 'A' && Character <= 'Z') ||
                        (Character >= '0' && Character <= '9')));
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtStrtoul64
 *
 * PARAMETERS:  String          - Null terminated string
 *              Base            - Radix of the string: 10, 16, or ACPI_ANY_BASE
 *              RetInteger      - Where the converted integer is returned
 *
 * RETURN:      Status and Converted value
 *
 * DESCRIPTION: Convert a string into an unsigned value.
 *              NOTE: Does not support Octal strings, not needed.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtStrtoul64 (
    char                    *String,
    UINT32                  Base,
    ACPI_INTEGER            *RetInteger)
{
    UINT32                  ThisDigit = 0;
    ACPI_INTEGER            ReturnValue = 0;
    ACPI_INTEGER            Quotient;


    ACPI_FUNCTION_TRACE ("UtStroul64");


    if ((!String) || !(*String))
    {
        goto ErrorExit;
    }

    switch (Base)
    {
    case ACPI_ANY_BASE:
    case 10:
    case 16:
        break;

    default:
        /* Invalid Base */
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /* Skip over any white space in the buffer */

    while (ACPI_IS_SPACE (*String) || *String == '\t')
    {
        String++;
    }

    /*
     * If the input parameter Base is zero, then we need to
     * determine if it is decimal or hexadecimal:
     */
    if (Base == 0)
    {
        if ((*String == '0') &&
            (ACPI_TOLOWER (*(String + 1)) == 'x'))
        {
            Base = 16;
            String += 2;
        }
        else
        {
            Base = 10;
        }
    }

    /*
     * For hexadecimal base, skip over the leading
     * 0 or 0x, if they are present.
     */
    if ((Base == 16) &&
        (*String == '0') &&
        (ACPI_TOLOWER (*(String + 1)) == 'x'))
    {
        String += 2;
    }

    /* Any string left? */

    if (!(*String))
    {
        goto ErrorExit;
    }

    /* Main loop: convert the string to a 64-bit integer */

    while (*String)
    {
        if (ACPI_IS_DIGIT (*String))
        {
            /* Convert ASCII 0-9 to Decimal value */

            ThisDigit = ((UINT8) *String) - '0';
        }
        else
        {
            if (Base == 10)
            {
                /* Digit is out of range */

                goto ErrorExit;
            }

            ThisDigit = (UINT8) ACPI_TOUPPER (*String);
            if (ACPI_IS_XDIGIT ((char) ThisDigit))
            {
                /* Convert ASCII Hex char to value */

                ThisDigit = ThisDigit - 'A' + 10;
            }
            else
            {
                /*
                 * We allow non-hex chars, just stop now, same as end-of-string.
                 * See ACPI spec, string-to-integer conversion.
                 */
                break;
            }
        }

        /* Divide the digit into the correct position */

        (void) AcpiUtShortDivide ((ACPI_INTEGER_MAX - (ACPI_INTEGER) ThisDigit),
                    Base, &Quotient, NULL);
        if (ReturnValue > Quotient)
        {
            goto ErrorExit;
        }

        ReturnValue *= Base;
        ReturnValue += ThisDigit;
        String++;
    }

    /* All done, normal exit */

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


    ACPI_FUNCTION_TRACE ("UtWalkPackageTree");


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
            (ACPI_GET_OBJECT_TYPE (ThisSourceObj) != ACPI_TYPE_PACKAGE))
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
                return_ACPI_STATUS (AE_NO_MEMORY);
            }
        }
    }

    /* We should never get here */

    return_ACPI_STATUS (AE_AML_INTERNAL);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtGenerateChecksum
 *
 * PARAMETERS:  Buffer          - Buffer to be scanned
 *              Length          - number of bytes to examine
 *
 * RETURN:      The generated checksum
 *
 * DESCRIPTION: Generate a checksum on a raw buffer
 *
 ******************************************************************************/

UINT8
AcpiUtGenerateChecksum (
    UINT8                   *Buffer,
    UINT32                  Length)
{
    UINT32                  i;
    signed char             Sum = 0;


    for (i = 0; i < Length; i++)
    {
        Sum = (signed char) (Sum + Buffer[i]);
    }

    return ((UINT8) (0 - Sum));
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtGetResourceType
 *
 * PARAMETERS:  Aml             - Pointer to the raw AML resource descriptor
 *
 * RETURN:      The Resource Type with no extraneous bits (except the
 *              Large/Small descriptor bit -- this is left alone)
 *
 * DESCRIPTION: Extract the Resource Type/Name from the first byte of
 *              a resource descriptor.
 *
 ******************************************************************************/

UINT8
AcpiUtGetResourceType (
    void                    *Aml)
{
    ACPI_FUNCTION_ENTRY ();


    /*
     * Byte 0 contains the descriptor name (Resource Type)
     * Determine if this is a small or large resource
     */
    if (*((UINT8 *) Aml) & ACPI_RESOURCE_NAME_LARGE)
    {
        /* Large Resource Type -- bits 6:0 contain the name */

        return (*((UINT8 *) Aml));
    }
    else
    {
        /* Small Resource Type -- bits 6:3 contain the name */

        return ((UINT8) (*((UINT8 *) Aml) & ACPI_RESOURCE_NAME_SMALL_MASK));
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtGetResourceLength
 *
 * PARAMETERS:  Aml             - Pointer to the raw AML resource descriptor
 *
 * RETURN:      Byte Length
 *
 * DESCRIPTION: Get the "Resource Length" of a raw AML descriptor. By
 *              definition, this does not include the size of the descriptor
 *              header or the length field itself.
 *
 ******************************************************************************/

UINT16
AcpiUtGetResourceLength (
    void                    *Aml)
{
    UINT16                  ResourceLength;


    ACPI_FUNCTION_ENTRY ();


    /*
     * Byte 0 contains the descriptor name (Resource Type)
     * Determine if this is a small or large resource
     */
    if (*((UINT8 *) Aml) & ACPI_RESOURCE_NAME_LARGE)
    {
        /* Large Resource type -- bytes 1-2 contain the 16-bit length */

        ACPI_MOVE_16_TO_16 (&ResourceLength, &((UINT8 *) Aml)[1]);

    }
    else
    {
        /* Small Resource type -- bits 2:0 of byte 0 contain the length */

        ResourceLength = (UINT16) (*((UINT8 *) Aml) &
                                    ACPI_RESOURCE_NAME_SMALL_LENGTH_MASK);
    }

    return (ResourceLength);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtGetDescriptorLength
 *
 * PARAMETERS:  Aml             - Pointer to the raw AML resource descriptor
 *
 * RETURN:      Byte length
 *
 * DESCRIPTION: Get the total byte length of a raw AML descriptor, including the
 *              length of the descriptor header and the length field itself.
 *              Used to walk descriptor lists.
 *
 ******************************************************************************/

UINT32
AcpiUtGetDescriptorLength (
    void                    *Aml)
{
    UINT32                  DescriptorLength;


    ACPI_FUNCTION_ENTRY ();


    /* First get the Resource Length (Does not include header length) */

    DescriptorLength = AcpiUtGetResourceLength (Aml);

    /* Determine if this is a small or large resource */

    if (*((UINT8 *) Aml) & ACPI_RESOURCE_NAME_LARGE)
    {
        DescriptorLength += sizeof (AML_RESOURCE_LARGE_HEADER);
    }
    else
    {
        DescriptorLength += sizeof (AML_RESOURCE_SMALL_HEADER);
    }

    return (DescriptorLength);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtGetResourceEndTag
 *
 * PARAMETERS:  ObjDesc         - The resource template buffer object
 *
 * RETURN:      Pointer to the end tag
 *
 * DESCRIPTION: Find the END_TAG resource descriptor in an AML resource template
 *
 ******************************************************************************/


UINT8 *
AcpiUtGetResourceEndTag (
    ACPI_OPERAND_OBJECT     *ObjDesc)
{
    UINT8                   *Aml;
    UINT8                   *EndAml;


    Aml    = ObjDesc->Buffer.Pointer;
    EndAml = Aml + ObjDesc->Buffer.Length;

    /* Walk the resource template, one descriptor per loop */

    while (Aml < EndAml)
    {
        if (AcpiUtGetResourceType (Aml) == ACPI_RESOURCE_NAME_END_TAG)
        {
            /* Found the end_tag descriptor, all done */

            return (Aml);
        }

        /* Point to the next resource descriptor */

        Aml += AcpiUtGetResourceLength (Aml);
    }

    /* End tag was not found */

    return (NULL);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtReportError
 *
 * PARAMETERS:  ModuleName          - Caller's module name (for error output)
 *              LineNumber          - Caller's line number (for error output)
 *              ComponentId         - Caller's component ID (for error output)
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print error message
 *
 ******************************************************************************/

void
AcpiUtReportError (
    char                    *ModuleName,
    UINT32                  LineNumber,
    UINT32                  ComponentId)
{

    AcpiOsPrintf ("%8s-%04d: *** Error: ", ModuleName, LineNumber);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtReportWarning
 *
 * PARAMETERS:  ModuleName          - Caller's module name (for error output)
 *              LineNumber          - Caller's line number (for error output)
 *              ComponentId         - Caller's component ID (for error output)
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print warning message
 *
 ******************************************************************************/

void
AcpiUtReportWarning (
    char                    *ModuleName,
    UINT32                  LineNumber,
    UINT32                  ComponentId)
{

    AcpiOsPrintf ("%8s-%04d: *** Warning: ", ModuleName, LineNumber);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtReportInfo
 *
 * PARAMETERS:  ModuleName          - Caller's module name (for error output)
 *              LineNumber          - Caller's line number (for error output)
 *              ComponentId         - Caller's component ID (for error output)
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print information message
 *
 ******************************************************************************/

void
AcpiUtReportInfo (
    char                    *ModuleName,
    UINT32                  LineNumber,
    UINT32                  ComponentId)
{

    AcpiOsPrintf ("%8s-%04d: *** Info: ", ModuleName, LineNumber);
}


