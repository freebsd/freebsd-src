/*******************************************************************************
 *
 * Module Name: utmisc - common utility procedures
 *              $Revision: 101 $
 *
 ******************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2004, Intel Corp.
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


#define _COMPONENT          ACPI_UTILITIES
        ACPI_MODULE_NAME    ("utmisc")


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtPrintString
 *
 * PARAMETERS:  String          - Null terminated ASCII string
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
 * PARAMETERS:  ObjHandle           - Handle whose pathname will be displayed
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

    AcpiOsPrintf ("%-12s  %s", AcpiUtGetTypeName (Type), (char *) Buffer.Pointer);

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
 * PARAMETERS:  Character           - The character to be examined
 *
 * RETURN:      1 if Character may appear in a name, else 0
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
    UINT32                  ThisDigit;
    ACPI_INTEGER            ReturnValue = 0;
    ACPI_INTEGER            Quotient;


    ACPI_FUNCTION_TRACE ("UtStroul64");


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
        ++String;
    }

    /*
     * If the input parameter Base is zero, then we need to
     * determine if it is decimal or hexadecimal:
     */
    if (Base == 0)
    {
        if ((*String == '0') &&
            (ACPI_TOLOWER (*(++String)) == 'x'))
        {
            Base = 16;
            ++String;
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
    if (Base == 16 &&
        *String == '0' &&
        ACPI_TOLOWER (*(++String)) == 'x')
    {
        String++;
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
            ThisDigit = (UINT8) ACPI_TOUPPER (*String);
            if (ACPI_IS_UPPER ((char) ThisDigit))
            {
                /* Convert ASCII Hex char to value */

                ThisDigit = ThisDigit - 'A' + 10;
            }
            else
            {
                goto ErrorExit;
            }
        }

        /* Check to see if digit is out of range */

        if (ThisDigit >= Base)
        {
            goto ErrorExit;
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
        ++String;
    }

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
 * FUNCTION:    AcpiUtStrupr
 *
 * PARAMETERS:  SrcString       - The source string to convert to
 *
 * RETURN:      SrcString
 *
 * DESCRIPTION: Convert string to uppercase
 *
 ******************************************************************************/

char *
AcpiUtStrupr (
    char                    *SrcString)
{
    char                    *String;


    ACPI_FUNCTION_ENTRY ();


    /* Walk entire string, uppercasing the letters */

    for (String = SrcString; *String; )
    {
        *String = (char) ACPI_TOUPPER (*String);
        String++;
    }

    return (SrcString);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtMutexInitialize
 *
 * PARAMETERS:  None.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create the system mutex objects.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtMutexInitialize (
    void)
{
    UINT32                  i;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("UtMutexInitialize");


    /*
     * Create each of the predefined mutex objects
     */
    for (i = 0; i < NUM_MUTEX; i++)
    {
        Status = AcpiUtCreateMutex (i);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }
    }

    Status = AcpiOsCreateLock (&AcpiGbl_GpeLock);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtMutexTerminate
 *
 * PARAMETERS:  None.
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Delete all of the system mutex objects.
 *
 ******************************************************************************/

void
AcpiUtMutexTerminate (
    void)
{
    UINT32                  i;


    ACPI_FUNCTION_TRACE ("UtMutexTerminate");


    /*
     * Delete each predefined mutex object
     */
    for (i = 0; i < NUM_MUTEX; i++)
    {
        (void) AcpiUtDeleteMutex (i);
    }

    AcpiOsDeleteLock (AcpiGbl_GpeLock);
    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtCreateMutex
 *
 * PARAMETERS:  MutexID         - ID of the mutex to be created
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create a mutex object.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtCreateMutex (
    ACPI_MUTEX_HANDLE       MutexId)
{
    ACPI_STATUS             Status = AE_OK;


    ACPI_FUNCTION_TRACE_U32 ("UtCreateMutex", MutexId);


    if (MutexId > MAX_MUTEX)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    if (!AcpiGbl_MutexInfo[MutexId].Mutex)
    {
        Status = AcpiOsCreateSemaphore (1, 1,
                        &AcpiGbl_MutexInfo[MutexId].Mutex);
        AcpiGbl_MutexInfo[MutexId].OwnerId = ACPI_MUTEX_NOT_ACQUIRED;
        AcpiGbl_MutexInfo[MutexId].UseCount = 0;
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtDeleteMutex
 *
 * PARAMETERS:  MutexID         - ID of the mutex to be deleted
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Delete a mutex object.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtDeleteMutex (
    ACPI_MUTEX_HANDLE       MutexId)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE_U32 ("UtDeleteMutex", MutexId);


    if (MutexId > MAX_MUTEX)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    Status = AcpiOsDeleteSemaphore (AcpiGbl_MutexInfo[MutexId].Mutex);

    AcpiGbl_MutexInfo[MutexId].Mutex = NULL;
    AcpiGbl_MutexInfo[MutexId].OwnerId = ACPI_MUTEX_NOT_ACQUIRED;

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtAcquireMutex
 *
 * PARAMETERS:  MutexID         - ID of the mutex to be acquired
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Acquire a mutex object.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtAcquireMutex (
    ACPI_MUTEX_HANDLE       MutexId)
{
    ACPI_STATUS             Status;
    UINT32                  i;
    UINT32                  ThisThreadId;


    ACPI_FUNCTION_NAME ("UtAcquireMutex");


    if (MutexId > MAX_MUTEX)
    {
        return (AE_BAD_PARAMETER);
    }

    ThisThreadId = AcpiOsGetThreadId ();

    /*
     * Deadlock prevention.  Check if this thread owns any mutexes of value
     * greater than or equal to this one.  If so, the thread has violated
     * the mutex ordering rule.  This indicates a coding error somewhere in
     * the ACPI subsystem code.
     */
    for (i = MutexId; i < MAX_MUTEX; i++)
    {
        if (AcpiGbl_MutexInfo[i].OwnerId == ThisThreadId)
        {
            if (i == MutexId)
            {
                ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
                        "Mutex [%s] already acquired by this thread [%X]\n",
                        AcpiUtGetMutexName (MutexId), ThisThreadId));

                return (AE_ALREADY_ACQUIRED);
            }

            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
                    "Invalid acquire order: Thread %X owns [%s], wants [%s]\n",
                    ThisThreadId, AcpiUtGetMutexName (i),
                    AcpiUtGetMutexName (MutexId)));

            return (AE_ACQUIRE_DEADLOCK);
        }
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_MUTEX,
                "Thread %X attempting to acquire Mutex [%s]\n",
                ThisThreadId, AcpiUtGetMutexName (MutexId)));

    Status = AcpiOsWaitSemaphore (AcpiGbl_MutexInfo[MutexId].Mutex,
                                    1, ACPI_WAIT_FOREVER);
    if (ACPI_SUCCESS (Status))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_MUTEX, "Thread %X acquired Mutex [%s]\n",
                    ThisThreadId, AcpiUtGetMutexName (MutexId)));

        AcpiGbl_MutexInfo[MutexId].UseCount++;
        AcpiGbl_MutexInfo[MutexId].OwnerId = ThisThreadId;
    }
    else
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Thread %X could not acquire Mutex [%s] %s\n",
                    ThisThreadId, AcpiUtGetMutexName (MutexId),
                    AcpiFormatException (Status)));
    }

    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtReleaseMutex
 *
 * PARAMETERS:  MutexID         - ID of the mutex to be released
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Release a mutex object.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtReleaseMutex (
    ACPI_MUTEX_HANDLE       MutexId)
{
    ACPI_STATUS             Status;
    UINT32                  i;
    UINT32                  ThisThreadId;


    ACPI_FUNCTION_NAME ("UtReleaseMutex");


    ThisThreadId = AcpiOsGetThreadId ();
    ACPI_DEBUG_PRINT ((ACPI_DB_MUTEX,
        "Thread %X releasing Mutex [%s]\n", ThisThreadId,
        AcpiUtGetMutexName (MutexId)));

    if (MutexId > MAX_MUTEX)
    {
        return (AE_BAD_PARAMETER);
    }

    /*
     * Mutex must be acquired in order to release it!
     */
    if (AcpiGbl_MutexInfo[MutexId].OwnerId == ACPI_MUTEX_NOT_ACQUIRED)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
                "Mutex [%s] is not acquired, cannot release\n",
                AcpiUtGetMutexName (MutexId)));

        return (AE_NOT_ACQUIRED);
    }

    /*
     * Deadlock prevention.  Check if this thread owns any mutexes of value
     * greater than this one.  If so, the thread has violated the mutex
     * ordering rule.  This indicates a coding error somewhere in
     * the ACPI subsystem code.
     */
    for (i = MutexId; i < MAX_MUTEX; i++)
    {
        if (AcpiGbl_MutexInfo[i].OwnerId == ThisThreadId)
        {
            if (i == MutexId)
            {
                continue;
            }

            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
                    "Invalid release order: owns [%s], releasing [%s]\n",
                    AcpiUtGetMutexName (i), AcpiUtGetMutexName (MutexId)));

            return (AE_RELEASE_DEADLOCK);
        }
    }

    /* Mark unlocked FIRST */

    AcpiGbl_MutexInfo[MutexId].OwnerId = ACPI_MUTEX_NOT_ACQUIRED;

    Status = AcpiOsSignalSemaphore (AcpiGbl_MutexInfo[MutexId].Mutex, 1);

    if (ACPI_FAILURE (Status))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Thread %X could not release Mutex [%s] %s\n",
                    ThisThreadId, AcpiUtGetMutexName (MutexId),
                    AcpiFormatException (Status)));
    }
    else
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_MUTEX, "Thread %X released Mutex [%s]\n",
                    ThisThreadId, AcpiUtGetMutexName (MutexId)));
    }

    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtCreateUpdateStateAndPush
 *
 * PARAMETERS:  *Object         - Object to be added to the new state
 *              Action          - Increment/Decrement
 *              StateList       - List the state will be added to
 *
 * RETURN:      None
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
 * FUNCTION:    AcpiUtCreatePkgStateAndPush
 *
 * PARAMETERS:  *Object         - Object to be added to the new state
 *              Action          - Increment/Decrement
 *              StateList       - List the state will be added to
 *
 * RETURN:      None
 *
 * DESCRIPTION: Create a new state and push it
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtCreatePkgStateAndPush (
    void                    *InternalObject,
    void                    *ExternalObject,
    UINT16                  Index,
    ACPI_GENERIC_STATE      **StateList)
{
    ACPI_GENERIC_STATE       *State;


    ACPI_FUNCTION_ENTRY ();


    State = AcpiUtCreatePkgState (InternalObject, ExternalObject, Index);
    if (!State)
    {
        return (AE_NO_MEMORY);
    }

    AcpiUtPushGenericState (StateList, State);
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtPushGenericState
 *
 * PARAMETERS:  ListHead            - Head of the state stack
 *              State               - State object to push
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Push a state object onto a state stack
 *
 ******************************************************************************/

void
AcpiUtPushGenericState (
    ACPI_GENERIC_STATE      **ListHead,
    ACPI_GENERIC_STATE      *State)
{
    ACPI_FUNCTION_TRACE ("UtPushGenericState");


    /* Push the state object onto the front of the list (stack) */

    State->Common.Next = *ListHead;
    *ListHead = State;

    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtPopGenericState
 *
 * PARAMETERS:  ListHead            - Head of the state stack
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Pop a state object from a state stack
 *
 ******************************************************************************/

ACPI_GENERIC_STATE *
AcpiUtPopGenericState (
    ACPI_GENERIC_STATE      **ListHead)
{
    ACPI_GENERIC_STATE      *State;


    ACPI_FUNCTION_TRACE ("UtPopGenericState");


    /* Remove the state object at the head of the list (stack) */

    State = *ListHead;
    if (State)
    {
        /* Update the list head */

        *ListHead = State->Common.Next;
    }

    return_PTR (State);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtCreateGenericState
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create a generic state object.  Attempt to obtain one from
 *              the global state cache;  If none available, create a new one.
 *
 ******************************************************************************/

ACPI_GENERIC_STATE *
AcpiUtCreateGenericState (void)
{
    ACPI_GENERIC_STATE      *State;


    ACPI_FUNCTION_ENTRY ();


    State = AcpiUtAcquireFromCache (ACPI_MEM_LIST_STATE);

    /* Initialize */

    if (State)
    {
        State->Common.DataType = ACPI_DESC_TYPE_STATE;
    }

    return (State);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtCreateThreadState
 *
 * PARAMETERS:  None
 *
 * RETURN:      Thread State
 *
 * DESCRIPTION: Create a "Thread State" - a flavor of the generic state used
 *              to track per-thread info during method execution
 *
 ******************************************************************************/

ACPI_THREAD_STATE *
AcpiUtCreateThreadState (
    void)
{
    ACPI_GENERIC_STATE      *State;


    ACPI_FUNCTION_TRACE ("UtCreateThreadState");


    /* Create the generic state object */

    State = AcpiUtCreateGenericState ();
    if (!State)
    {
        return_PTR (NULL);
    }

    /* Init fields specific to the update struct */

    State->Common.DataType = ACPI_DESC_TYPE_STATE_THREAD;
    State->Thread.ThreadId = AcpiOsGetThreadId ();

    return_PTR ((ACPI_THREAD_STATE *) State);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtCreateUpdateState
 *
 * PARAMETERS:  Object              - Initial Object to be installed in the
 *                                    state
 *              Action              - Update action to be performed
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create an "Update State" - a flavor of the generic state used
 *              to update reference counts and delete complex objects such
 *              as packages.
 *
 ******************************************************************************/

ACPI_GENERIC_STATE *
AcpiUtCreateUpdateState (
    ACPI_OPERAND_OBJECT     *Object,
    UINT16                  Action)
{
    ACPI_GENERIC_STATE      *State;


    ACPI_FUNCTION_TRACE_PTR ("UtCreateUpdateState", Object);


    /* Create the generic state object */

    State = AcpiUtCreateGenericState ();
    if (!State)
    {
        return_PTR (NULL);
    }

    /* Init fields specific to the update struct */

    State->Common.DataType = ACPI_DESC_TYPE_STATE_UPDATE;
    State->Update.Object = Object;
    State->Update.Value  = Action;

    return_PTR (State);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtCreatePkgState
 *
 * PARAMETERS:  Object              - Initial Object to be installed in the
 *                                    state
 *              Action              - Update action to be performed
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create a "Package State"
 *
 ******************************************************************************/

ACPI_GENERIC_STATE *
AcpiUtCreatePkgState (
    void                    *InternalObject,
    void                    *ExternalObject,
    UINT16                  Index)
{
    ACPI_GENERIC_STATE      *State;


    ACPI_FUNCTION_TRACE_PTR ("UtCreatePkgState", InternalObject);


    /* Create the generic state object */

    State = AcpiUtCreateGenericState ();
    if (!State)
    {
        return_PTR (NULL);
    }

    /* Init fields specific to the update struct */

    State->Common.DataType  = ACPI_DESC_TYPE_STATE_PACKAGE;
    State->Pkg.SourceObject = (ACPI_OPERAND_OBJECT *) InternalObject;
    State->Pkg.DestObject   = ExternalObject;
    State->Pkg.Index        = Index;
    State->Pkg.NumPackages  = 1;

    return_PTR (State);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtCreateControlState
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create a "Control State" - a flavor of the generic state used
 *              to support nested IF/WHILE constructs in the AML.
 *
 ******************************************************************************/

ACPI_GENERIC_STATE *
AcpiUtCreateControlState (
    void)
{
    ACPI_GENERIC_STATE      *State;


    ACPI_FUNCTION_TRACE ("UtCreateControlState");


    /* Create the generic state object */

    State = AcpiUtCreateGenericState ();
    if (!State)
    {
        return_PTR (NULL);
    }

    /* Init fields specific to the control struct */

    State->Common.DataType  = ACPI_DESC_TYPE_STATE_CONTROL;
    State->Common.State     = ACPI_CONTROL_CONDITIONAL_EXECUTING;

    return_PTR (State);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtDeleteGenericState
 *
 * PARAMETERS:  State               - The state object to be deleted
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Put a state object back into the global state cache.  The object
 *              is not actually freed at this time.
 *
 ******************************************************************************/

void
AcpiUtDeleteGenericState (
    ACPI_GENERIC_STATE      *State)
{
    ACPI_FUNCTION_TRACE ("UtDeleteGenericState");


    AcpiUtReleaseToCache (ACPI_MEM_LIST_STATE, State);
    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtDeleteGenericStateCache
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Purge the global state object cache.  Used during subsystem
 *              termination.
 *
 ******************************************************************************/

void
AcpiUtDeleteGenericStateCache (
    void)
{
    ACPI_FUNCTION_TRACE ("UtDeleteGenericStateCache");


    AcpiUtDeleteGenericCache (ACPI_MEM_LIST_STATE);
    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtWalkPackageTree
 *
 * PARAMETERS:  ObjDesc         - The Package object on which to resolve refs
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
 * RETURN:      checksum
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
 * FUNCTION:    AcpiUtGetResourceEndTag
 *
 * PARAMETERS:  ObjDesc         - The resource template buffer object
 *
 * RETURN:      Pointer to the end tag
 *
 * DESCRIPTION: Find the END_TAG resource descriptor in a resource template
 *
 ******************************************************************************/


UINT8 *
AcpiUtGetResourceEndTag (
    ACPI_OPERAND_OBJECT     *ObjDesc)
{
    UINT8                   BufferByte;
    UINT8                   *Buffer;
    UINT8                   *EndBuffer;


    Buffer    = ObjDesc->Buffer.Pointer;
    EndBuffer = Buffer + ObjDesc->Buffer.Length;

    while (Buffer < EndBuffer)
    {
        BufferByte = *Buffer;
        if (BufferByte & ACPI_RDESC_TYPE_MASK)
        {
            /* Large Descriptor - Length is next 2 bytes */

            Buffer += ((*(Buffer+1) | (*(Buffer+2) << 8)) + 3);
        }
        else
        {
            /* Small Descriptor.  End Tag will be found here */

            if ((BufferByte & ACPI_RDESC_SMALL_MASK) == ACPI_RDESC_TYPE_END_TAG)
            {
                /* Found the end tag descriptor, all done. */

                return (Buffer);
            }

            /* Length is in the header */

            Buffer += ((BufferByte & 0x07) + 1);
        }
    }

    /* End tag not found */

    return (NULL);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtReportError
 *
 * PARAMETERS:  ModuleName          - Caller's module name (for error output)
 *              LineNumber          - Caller's line number (for error output)
 *              ComponentId         - Caller's component ID (for error output)
 *              Message             - Error message to use on failure
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
 *              Message             - Error message to use on failure
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
 *              Message             - Error message to use on failure
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


