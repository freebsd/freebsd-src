/******************************************************************************
 *
 * Module Name: cmcopy - Internal to external object translation utilities
 *              $Revision: 56 $
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

#define __CMCOPY_C__

#include "acpi.h"
#include "acinterp.h"
#include "acnamesp.h"


#define _COMPONENT          MISCELLANEOUS
        MODULE_NAME         ("cmcopy")


typedef struct Search_st
{
    ACPI_OPERAND_OBJECT         *InternalObj;
    UINT32                      Index;
    ACPI_OBJECT                 *ExternalObj;

} PKG_SEARCH_INFO;


/* Used to traverse nested packages */

PKG_SEARCH_INFO                 Level[MAX_PACKAGE_DEPTH];

/******************************************************************************
 *
 * FUNCTION:    AcpiCmBuildExternalSimpleObject
 *
 * PARAMETERS:  *InternalObj    - Pointer to the object we are examining
 *              *Buffer         - Where the object is returned
 *              *SpaceUsed      - Where the data length is returned
 *
 * RETURN:      Status          - the status of the call
 *
 * DESCRIPTION: This function is called to place a simple object in a user
 *                  buffer.
 *
 *              The buffer is assumed to have sufficient space for the object.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiCmBuildExternalSimpleObject (
    ACPI_OPERAND_OBJECT     *InternalObj,
    ACPI_OBJECT             *ExternalObj,
    UINT8                   *DataSpace,
    UINT32                  *BufferSpaceUsed)
{
    UINT32                  Length = 0;
    UINT8                   *SourcePtr = NULL;


    FUNCTION_TRACE ("CmBuildExternalSimpleObject");


    /*
     * Check for NULL object case (could be an uninitialized
     * package element
     */

    if (!InternalObj)
    {
        *BufferSpaceUsed = 0;
        return_ACPI_STATUS (AE_OK);
    }

    /* Always clear the external object */

    MEMSET (ExternalObj, 0, sizeof (ACPI_OBJECT));

    /*
     * In general, the external object will be the same type as
     * the internal object
     */

    ExternalObj->Type = InternalObj->Common.Type;

    /* However, only a limited number of external types are supported */

    switch (ExternalObj->Type)
    {

    case ACPI_TYPE_STRING:

        Length = InternalObj->String.Length;
        ExternalObj->String.Length = InternalObj->String.Length;
        ExternalObj->String.Pointer = (NATIVE_CHAR *) DataSpace;
        SourcePtr = (UINT8 *) InternalObj->String.Pointer;
        break;


    case ACPI_TYPE_BUFFER:

        Length = InternalObj->Buffer.Length;
        ExternalObj->Buffer.Length = InternalObj->Buffer.Length;
        ExternalObj->Buffer.Pointer = DataSpace;
        SourcePtr = (UINT8 *) InternalObj->Buffer.Pointer;
        break;


    case ACPI_TYPE_NUMBER:

        ExternalObj->Number.Value= InternalObj->Number.Value;
        break;


    case INTERNAL_TYPE_REFERENCE:

        /*
         * This is an object reference.  We use the object type of "Any"
         * to indicate a reference object containing a handle to an ACPI
         * named object.
         */

        ExternalObj->Type = ACPI_TYPE_ANY;
        ExternalObj->Reference.Handle = InternalObj->Reference.Node;
        break;


    case ACPI_TYPE_PROCESSOR:

        ExternalObj->Processor.ProcId =
                                InternalObj->Processor.ProcId;

        ExternalObj->Processor.PblkAddress =
                                InternalObj->Processor.Address;

        ExternalObj->Processor.PblkLength =
                                InternalObj->Processor.Length;
        break;

    case ACPI_TYPE_POWER:

        ExternalObj->PowerResource.SystemLevel =
                            InternalObj->PowerResource.SystemLevel;

        ExternalObj->PowerResource.ResourceOrder =
                            InternalObj->PowerResource.ResourceOrder;
        break;

    default:
        return_ACPI_STATUS (AE_CTRL_RETURN_VALUE);
        break;
    }


    /* Copy data if necessary (strings or buffers) */

    if (Length)
    {
        /*
         * Copy the return data to the caller's buffer
         */
        MEMCPY ((void *) DataSpace, (void *) SourcePtr, Length);
    }


    *BufferSpaceUsed = (UINT32) ROUND_UP_TO_NATIVE_WORD (Length);

    return_ACPI_STATUS (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiCmBuildExternalPackageObject
 *
 * PARAMETERS:  *InternalObj    - Pointer to the object we are returning
 *              *Buffer         - Where the object is returned
 *              *SpaceUsed      - Where the object length is returned
 *
 * RETURN:      Status          - the status of the call
 *
 * DESCRIPTION: This function is called to place a package object in a user
 *              buffer.  A package object by definition contains other objects.
 *
 *              The buffer is assumed to have sufficient space for the object.
 *              The caller must have verified the buffer length needed using the
 *              AcpiCmGetObjectSize function before calling this function.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiCmBuildExternalPackageObject (
    ACPI_OPERAND_OBJECT     *InternalObj,
    UINT8                   *Buffer,
    UINT32                  *SpaceUsed)
{
    UINT8                   *FreeSpace;
    ACPI_OBJECT             *ExternalObj;
    UINT32                  CurrentDepth = 0;
    ACPI_STATUS             Status;
    UINT32                  Length = 0;
    UINT32                  ThisIndex;
    UINT32                  ObjectSpace;
    ACPI_OPERAND_OBJECT     *ThisInternalObj;
    ACPI_OBJECT             *ThisExternalObj;
    PKG_SEARCH_INFO         *LevelPtr;


    FUNCTION_TRACE ("CmBuildExternalPackageObject");


    /*
     * First package at head of the buffer
     */
    ExternalObj = (ACPI_OBJECT *) Buffer;

    /*
     * Free space begins right after the first package
     */
    FreeSpace = Buffer + ROUND_UP_TO_NATIVE_WORD (sizeof (ACPI_OBJECT));


    /*
     * Initialize the working variables
     */

    MEMSET ((void *) Level, 0, sizeof (Level));

    Level[0].InternalObj    = InternalObj;
    Level[0].ExternalObj    = ExternalObj;
    Level[0].Index          = 0;
    LevelPtr                = &Level[0];
    CurrentDepth            = 0;

    ExternalObj->Type               = InternalObj->Common.Type;
    ExternalObj->Package.Count      = InternalObj->Package.Count;
    ExternalObj->Package.Elements   = (ACPI_OBJECT *) FreeSpace;


    /*
     * Build an array of ACPI_OBJECTS in the buffer
     * and move the free space past it
     */

    FreeSpace += ExternalObj->Package.Count *
                 ROUND_UP_TO_NATIVE_WORD (sizeof (ACPI_OBJECT));


    while (1)
    {
        ThisIndex       = LevelPtr->Index;
        ThisInternalObj =
                (ACPI_OPERAND_OBJECT  *)
                LevelPtr->InternalObj->Package.Elements[ThisIndex];
        ThisExternalObj =
                (ACPI_OBJECT *)
                &LevelPtr->ExternalObj->Package.Elements[ThisIndex];


        /*
         * Check for
         * 1) Null object -- OK, this can happen if package
         *              element is never initialized
         * 2) Not an internal object - can be Node instead
         * 3) Any internal object other than a package.
         *
         * The more complex package case is handled later
         */

        if ((!ThisInternalObj) ||
            (!VALID_DESCRIPTOR_TYPE (
                ThisInternalObj, ACPI_DESC_TYPE_INTERNAL)) ||
            (!IS_THIS_OBJECT_TYPE (
                ThisInternalObj, ACPI_TYPE_PACKAGE)))
        {
            /*
             * This is a simple or null object -- get the size
             */

            Status =
                AcpiCmBuildExternalSimpleObject (ThisInternalObj,
                                                ThisExternalObj,
                                                FreeSpace,
                                                &ObjectSpace);
            if (ACPI_FAILURE (Status))
            {
                return_ACPI_STATUS (Status);
            }

            FreeSpace   += ObjectSpace;
            Length      += ObjectSpace;

            LevelPtr->Index++;
            while (LevelPtr->Index >=
                    LevelPtr->InternalObj->Package.Count)
            {
                /*
                 * We've handled all of the objects at this
                 * level.  This means that we have just
                 * completed a package.  That package may
                 * have contained one or more packages
                 * itself
                 */
                if (CurrentDepth == 0)
                {
                    /*
                     * We have handled all of the objects
                     * in the top level package just add
                     * the length of the package objects
                     * and get out
                     */
                    *SpaceUsed = Length;
                    return_ACPI_STATUS (AE_OK);
                }

                /*
                 * go back up a level and move the index
                 * past the just completed package object.
                 */
                CurrentDepth--;
                LevelPtr = &Level[CurrentDepth];
                LevelPtr->Index++;
            }
        }


        else
        {
            /*
             * This object is a package
             * -- we must go one level deeper
             */
            if (CurrentDepth >= MAX_PACKAGE_DEPTH-1)
            {
                /*
                 * Too many nested levels of packages
                 * for us to handle
                 */
                DEBUG_PRINT (ACPI_ERROR,
                    ("CmBuildPackageObject: Pkg nested too deep (max %d)\n",
                    MAX_PACKAGE_DEPTH));
                return_ACPI_STATUS (AE_LIMIT);
            }

            /*
             * Build the package object
             */
            ThisExternalObj->Type = ACPI_TYPE_PACKAGE;
            ThisExternalObj->Package.Count =
                                    ThisInternalObj->Package.Count;
            ThisExternalObj->Package.Elements =
                                        (ACPI_OBJECT *) FreeSpace;

            /*
             * Save space for the array of objects (Package elements)
             * update the buffer length counter
             */
            ObjectSpace = (UINT32) ROUND_UP_TO_NATIVE_WORD (
                                ThisExternalObj->Package.Count *
                                sizeof (ACPI_OBJECT));

            FreeSpace               += ObjectSpace;
            Length                  += ObjectSpace;

            CurrentDepth++;
            LevelPtr                = &Level[CurrentDepth];
            LevelPtr->InternalObj   = ThisInternalObj;
            LevelPtr->ExternalObj   = ThisExternalObj;
            LevelPtr->Index         = 0;
        }
    }
}


/******************************************************************************
 *
 * FUNCTION:    AcpiCmBuildExternalObject
 *
 * PARAMETERS:  *InternalObj    - The internal object to be converted
 *              *BufferPtr      - Where the object is returned
 *
 * RETURN:      Status          - the status of the call
 *
 * DESCRIPTION: This function is called to build an API object to be returned to
 *              the caller.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiCmBuildExternalObject (
    ACPI_OPERAND_OBJECT     *InternalObj,
    ACPI_BUFFER             *RetBuffer)
{
    ACPI_STATUS             Status;


    FUNCTION_TRACE ("CmBuildExternalObject");


    if (IS_THIS_OBJECT_TYPE (InternalObj, ACPI_TYPE_PACKAGE))
    {
        /*
         * Package objects contain other objects (which can be objects)
         * buildpackage does it all
         */
        Status =
            AcpiCmBuildExternalPackageObject (InternalObj,
                                                RetBuffer->Pointer,
                                                &RetBuffer->Length);
    }

    else
    {
        /*
         * Build a simple object (no nested objects)
         */
        Status =
            AcpiCmBuildExternalSimpleObject (InternalObj,
                            (ACPI_OBJECT *) RetBuffer->Pointer,
                            ((UINT8 *) RetBuffer->Pointer +
                            ROUND_UP_TO_NATIVE_WORD (
                                        sizeof (ACPI_OBJECT))),
                            &RetBuffer->Length);
        /*
         * build simple does not include the object size in the length
         * so we add it in here
         */
        RetBuffer->Length += sizeof (ACPI_OBJECT);
    }

    return_ACPI_STATUS (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiCmBuildInternalSimpleObject
 *
 * PARAMETERS:  *ExternalObj    - The external object to be converted
 *              *InternalObj    - Where the internal object is returned
 *
 * RETURN:      Status          - the status of the call
 *
 * DESCRIPTION: This function copies an external object to an internal one.
 *              NOTE: Pointers can be copied, we don't need to copy data.
 *              (The pointers have to be valid in our address space no matter
 *              what we do with them!)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiCmBuildInternalSimpleObject (
    ACPI_OBJECT             *ExternalObj,
    ACPI_OPERAND_OBJECT     *InternalObj)
{

    FUNCTION_TRACE ("CmBuildInternalSimpleObject");


    InternalObj->Common.Type = (UINT8) ExternalObj->Type;

    switch (ExternalObj->Type)
    {

    case ACPI_TYPE_STRING:

        InternalObj->String.Length  = ExternalObj->String.Length;
        InternalObj->String.Pointer = ExternalObj->String.Pointer;
        break;


    case ACPI_TYPE_BUFFER:

        InternalObj->Buffer.Length  = ExternalObj->Buffer.Length;
        InternalObj->Buffer.Pointer = ExternalObj->Buffer.Pointer;
        break;


    case ACPI_TYPE_NUMBER:
        /*
         * Number is included in the object itself
         */
        InternalObj->Number.Value   = ExternalObj->Number.Value;
        break;


    default:
        return_ACPI_STATUS (AE_CTRL_RETURN_VALUE);
        break;
    }


    return_ACPI_STATUS (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiCmBuildInternalPackageObject
 *
 * PARAMETERS:  *InternalObj    - Pointer to the object we are returning
 *              *Buffer         - Where the object is returned
 *              *SpaceUsed      - Where the length of the object is returned
 *
 * RETURN:      Status          - the status of the call
 *
 * DESCRIPTION: This function is called to place a package object in a user
 *              buffer.  A package object by definition contains other objects.
 *
 *              The buffer is assumed to have sufficient space for the object.
 *              The caller must have verified the buffer length needed using the
 *              AcpiCmGetObjectSize function before calling this function.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiCmBuildInternalPackageObject (
    ACPI_OPERAND_OBJECT     *InternalObj,
    UINT8                   *Buffer,
    UINT32                  *SpaceUsed)
{
    UINT8                   *FreeSpace;
    ACPI_OBJECT             *ExternalObj;
    UINT32                  CurrentDepth = 0;
    UINT32                  Length = 0;
    UINT32                  ThisIndex;
    UINT32                  ObjectSpace = 0;
    ACPI_OPERAND_OBJECT     *ThisInternalObj;
    ACPI_OBJECT             *ThisExternalObj;
    PKG_SEARCH_INFO         *LevelPtr;


    FUNCTION_TRACE ("CmBuildInternalPackageObject");


    /*
     * First package at head of the buffer
     */
    ExternalObj = (ACPI_OBJECT *)Buffer;

    /*
     * Free space begins right after the first package
     */
    FreeSpace = Buffer + sizeof(ACPI_OBJECT);


    /*
     * Initialize the working variables
     */

    MEMSET ((void *) Level, 0, sizeof(Level));

    Level[0].InternalObj    = InternalObj;
    Level[0].ExternalObj    = ExternalObj;
    LevelPtr                = &Level[0];
    CurrentDepth            = 0;

    ExternalObj->Type               = InternalObj->Common.Type;
    ExternalObj->Package.Count      = InternalObj->Package.Count;
    ExternalObj->Package.Elements   = (ACPI_OBJECT *)FreeSpace;


    /*
     * Build an array of ACPI_OBJECTS in the buffer
     * and move the free space past it
     */

    FreeSpace += ExternalObj->Package.Count * sizeof(ACPI_OBJECT);


    while (1)
    {
        ThisIndex       = LevelPtr->Index;

        ThisInternalObj = (ACPI_OPERAND_OBJECT  *)
                    &LevelPtr->InternalObj->Package.Elements[ThisIndex];

        ThisExternalObj = (ACPI_OBJECT *)
                    &LevelPtr->ExternalObj->Package.Elements[ThisIndex];

        if (IS_THIS_OBJECT_TYPE (ThisInternalObj, ACPI_TYPE_PACKAGE))
        {
            /*
             * If this object is a package then we go one deeper
             */
            if (CurrentDepth >= MAX_PACKAGE_DEPTH-1)
            {
                /*
                 * Too many nested levels of packages for us to handle
                 */
                DEBUG_PRINT (ACPI_ERROR,
                    ("CmBuildPackageObject: Pkg nested too deep (max %d)\n",
                    MAX_PACKAGE_DEPTH));
                return_ACPI_STATUS (AE_LIMIT);
            }

            /*
             * Build the package object
             */
            ThisExternalObj->Type               = ACPI_TYPE_PACKAGE;
            ThisExternalObj->Package.Count      = ThisInternalObj->Package.Count;
            ThisExternalObj->Package.Elements   = (ACPI_OBJECT *) FreeSpace;

            /*
             * Save space for the array of objects (Package elements)
             * update the buffer length counter
             */
            ObjectSpace             = ThisExternalObj->Package.Count *
                                            sizeof (ACPI_OBJECT);

            FreeSpace               += ObjectSpace;
            Length                  += ObjectSpace;

            CurrentDepth++;
            LevelPtr                = &Level[CurrentDepth];
            LevelPtr->InternalObj   = ThisInternalObj;
            LevelPtr->ExternalObj   = ThisExternalObj;
            LevelPtr->Index         = 0;

        }   /* if object is a package */

        else
        {
            FreeSpace   += ObjectSpace;
            Length      += ObjectSpace;

            LevelPtr->Index++;
            while (LevelPtr->Index >=
                    LevelPtr->InternalObj->Package.Count)
            {
                /*
                 * We've handled all of the objects at
                 * this level,  This means that we have
                 * just completed a package.  That package
                 * may have contained one or more packages
                 * itself
                 */
                if (CurrentDepth == 0)
                {
                    /*
                     * We have handled all of the objects
                     * in the top level package just add
                     * the length of the package objects
                     * and get out
                     */
                    *SpaceUsed = Length;
                    return_ACPI_STATUS (AE_OK);
                }

                /*
                 * go back up a level and move the index
                 * past the just completed package object.
                 */
                CurrentDepth--;
                LevelPtr = &Level[CurrentDepth];
                LevelPtr->Index++;
            }
        }   /* else object is NOT a package */
    }   /* while (1)  */
}


/******************************************************************************
 *
 * FUNCTION:    AcpiCmBuildInternalObject
 *
 * PARAMETERS:  *InternalObj    - The external object to be converted
 *              *BufferPtr      - Where the internal object is returned
 *
 * RETURN:      Status          - the status of the call
 *
 * DESCRIPTION: Converts an external object to an internal object.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiCmBuildInternalObject (
    ACPI_OBJECT             *ExternalObj,
    ACPI_OPERAND_OBJECT     *InternalObj)
{
    ACPI_STATUS             Status;


    FUNCTION_TRACE ("CmBuildInternalObject");


    if (ExternalObj->Type == ACPI_TYPE_PACKAGE)
    {
        /*
         * Package objects contain other objects (which can be objects)
         * buildpackage does it all
         */
/*
        Status = AcpiCmBuildInternalPackageObject(InternalObj,
                                                  RetBuffer->Pointer,
                                                  &RetBuffer->Length);
*/
        DEBUG_PRINT (ACPI_ERROR,
            ("CmBuildInternalObject: Packages as parameters not implemented!\n"));

        return_ACPI_STATUS (AE_NOT_IMPLEMENTED);
    }

    else
    {
        /*
         * Build a simple object (no nested objects)
         */
        Status = AcpiCmBuildInternalSimpleObject (ExternalObj, InternalObj);
        /*
         * build simple does not include the object size in the length
         * so we add it in here
         */
    }

    return_ACPI_STATUS (Status);
}

