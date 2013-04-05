/******************************************************************************
 *
 * Module Name: nspredef - Validation of ACPI predefined methods and objects
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2013, Intel Corp.
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

#define ACPI_CREATE_PREDEFINED_TABLE

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acnamesp.h>
#include <contrib/dev/acpica/include/acpredef.h>


#define _COMPONENT          ACPI_NAMESPACE
        ACPI_MODULE_NAME    ("nspredef")


/*******************************************************************************
 *
 * This module validates predefined ACPI objects that appear in the namespace,
 * at the time they are evaluated (via AcpiEvaluateObject). The purpose of this
 * validation is to detect problems with BIOS-exposed predefined ACPI objects
 * before the results are returned to the ACPI-related drivers.
 *
 * There are several areas that are validated:
 *
 *  1) The number of input arguments as defined by the method/object in the
 *      ASL is validated against the ACPI specification.
 *  2) The type of the return object (if any) is validated against the ACPI
 *      specification.
 *  3) For returned package objects, the count of package elements is
 *      validated, as well as the type of each package element. Nested
 *      packages are supported.
 *
 * For any problems found, a warning message is issued.
 *
 ******************************************************************************/


/* Local prototypes */

static ACPI_STATUS
AcpiNsCheckReference (
    ACPI_PREDEFINED_DATA        *Data,
    ACPI_OPERAND_OBJECT         *ReturnObject);

static UINT32
AcpiNsGetBitmappedType (
    ACPI_OPERAND_OBJECT         *ReturnObject);


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsCheckPredefinedNames
 *
 * PARAMETERS:  Node            - Namespace node for the method/object
 *              UserParamCount  - Number of parameters actually passed
 *              ReturnStatus    - Status from the object evaluation
 *              ReturnObjectPtr - Pointer to the object returned from the
 *                                evaluation of a method or object
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Check an ACPI name for a match in the predefined name list.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiNsCheckPredefinedNames (
    ACPI_NAMESPACE_NODE         *Node,
    UINT32                      UserParamCount,
    ACPI_STATUS                 ReturnStatus,
    ACPI_OPERAND_OBJECT         **ReturnObjectPtr)
{
    ACPI_STATUS                 Status = AE_OK;
    const ACPI_PREDEFINED_INFO  *Predefined;
    char                        *Pathname;
    ACPI_PREDEFINED_DATA        *Data;


    /* Match the name for this method/object against the predefined list */

    Predefined = AcpiUtMatchPredefinedMethod (Node->Name.Ascii);

    /* Get the full pathname to the object, for use in warning messages */

    Pathname = AcpiNsGetExternalPathname (Node);
    if (!Pathname)
    {
        return (AE_OK); /* Could not get pathname, ignore */
    }

    /*
     * Check that the parameter count for this method matches the ASL
     * definition. For predefined names, ensure that both the caller and
     * the method itself are in accordance with the ACPI specification.
     */
    AcpiNsCheckParameterCount (Pathname, Node, UserParamCount, Predefined);

    /* If not a predefined name, we cannot validate the return object */

    if (!Predefined)
    {
        goto Cleanup;
    }

    /*
     * If the method failed or did not actually return an object, we cannot
     * validate the return object
     */
    if ((ReturnStatus != AE_OK) && (ReturnStatus != AE_CTRL_RETURN_VALUE))
    {
        goto Cleanup;
    }

    /*
     * Return value validation and possible repair.
     *
     * 1) Don't perform return value validation/repair if this feature
     * has been disabled via a global option.
     *
     * 2) We have a return value, but if one wasn't expected, just exit,
     * this is not a problem. For example, if the "Implicit Return"
     * feature is enabled, methods will always return a value.
     *
     * 3) If the return value can be of any type, then we cannot perform
     * any validation, just exit.
     */
    if (AcpiGbl_DisableAutoRepair ||
        (!Predefined->Info.ExpectedBtypes) ||
        (Predefined->Info.ExpectedBtypes == ACPI_RTYPE_ALL))
    {
        goto Cleanup;
    }

    /* Create the parameter data block for object validation */

    Data = ACPI_ALLOCATE_ZEROED (sizeof (ACPI_PREDEFINED_DATA));
    if (!Data)
    {
        goto Cleanup;
    }
    Data->Predefined = Predefined;
    Data->Node = Node;
    Data->NodeFlags = Node->Flags;
    Data->Pathname = Pathname;

    /*
     * Check that the type of the main return object is what is expected
     * for this predefined name
     */
    Status = AcpiNsCheckObjectType (Data, ReturnObjectPtr,
                Predefined->Info.ExpectedBtypes, ACPI_NOT_PACKAGE_ELEMENT);
    if (ACPI_FAILURE (Status))
    {
        goto Exit;
    }

    /*
     * For returned Package objects, check the type of all sub-objects.
     * Note: Package may have been newly created by call above.
     */
    if ((*ReturnObjectPtr)->Common.Type == ACPI_TYPE_PACKAGE)
    {
        Data->ParentPackage = *ReturnObjectPtr;
        Status = AcpiNsCheckPackage (Data, ReturnObjectPtr);
        if (ACPI_FAILURE (Status))
        {
            goto Exit;
        }
    }

    /*
     * The return object was OK, or it was successfully repaired above.
     * Now make some additional checks such as verifying that package
     * objects are sorted correctly (if required) or buffer objects have
     * the correct data width (bytes vs. dwords). These repairs are
     * performed on a per-name basis, i.e., the code is specific to
     * particular predefined names.
     */
    Status = AcpiNsComplexRepairs (Data, Node, Status, ReturnObjectPtr);

Exit:
    /*
     * If the object validation failed or if we successfully repaired one
     * or more objects, mark the parent node to suppress further warning
     * messages during the next evaluation of the same method/object.
     */
    if (ACPI_FAILURE (Status) || (Data->Flags & ACPI_OBJECT_REPAIRED))
    {
        Node->Flags |= ANOBJ_EVALUATED;
    }
    ACPI_FREE (Data);

Cleanup:
    ACPI_FREE (Pathname);
    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsCheckParameterCount
 *
 * PARAMETERS:  Pathname        - Full pathname to the node (for error msgs)
 *              Node            - Namespace node for the method/object
 *              UserParamCount  - Number of args passed in by the caller
 *              Predefined      - Pointer to entry in predefined name table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Check that the declared (in ASL/AML) parameter count for a
 *              predefined name is what is expected (i.e., what is defined in
 *              the ACPI specification for this predefined name.)
 *
 ******************************************************************************/

void
AcpiNsCheckParameterCount (
    char                        *Pathname,
    ACPI_NAMESPACE_NODE         *Node,
    UINT32                      UserParamCount,
    const ACPI_PREDEFINED_INFO  *Predefined)
{
    UINT32                      ParamCount;
    UINT32                      RequiredParamsCurrent;
    UINT32                      RequiredParamsOld;


    /* Methods have 0-7 parameters. All other types have zero. */

    ParamCount = 0;
    if (Node->Type == ACPI_TYPE_METHOD)
    {
        ParamCount = Node->Object->Method.ParamCount;
    }

    if (!Predefined)
    {
        /*
         * Check the parameter count for non-predefined methods/objects.
         *
         * Warning if too few or too many arguments have been passed by the
         * caller. An incorrect number of arguments may not cause the method
         * to fail. However, the method will fail if there are too few
         * arguments and the method attempts to use one of the missing ones.
         */
        if (UserParamCount < ParamCount)
        {
            ACPI_WARN_PREDEFINED ((AE_INFO, Pathname, ACPI_WARN_ALWAYS,
                "Insufficient arguments - needs %u, found %u",
                ParamCount, UserParamCount));
        }
        else if (UserParamCount > ParamCount)
        {
            ACPI_WARN_PREDEFINED ((AE_INFO, Pathname, ACPI_WARN_ALWAYS,
                "Excess arguments - needs %u, found %u",
                ParamCount, UserParamCount));
        }
        return;
    }

    /*
     * Validate the user-supplied parameter count.
     * Allow two different legal argument counts (_SCP, etc.)
     */
    RequiredParamsCurrent = Predefined->Info.ArgumentList & METHOD_ARG_MASK;
    RequiredParamsOld = Predefined->Info.ArgumentList >> METHOD_ARG_BIT_WIDTH;

    if (UserParamCount != ACPI_UINT32_MAX)
    {
        if ((UserParamCount != RequiredParamsCurrent) &&
            (UserParamCount != RequiredParamsOld))
        {
            ACPI_WARN_PREDEFINED ((AE_INFO, Pathname, ACPI_WARN_ALWAYS,
                "Parameter count mismatch - "
                "caller passed %u, ACPI requires %u",
                UserParamCount, RequiredParamsCurrent));
        }
    }

    /*
     * Check that the ASL-defined parameter count is what is expected for
     * this predefined name (parameter count as defined by the ACPI
     * specification)
     */
    if ((ParamCount != RequiredParamsCurrent) &&
        (ParamCount != RequiredParamsOld))
    {
        ACPI_WARN_PREDEFINED ((AE_INFO, Pathname, Node->Flags,
            "Parameter count mismatch - ASL declared %u, ACPI requires %u",
            ParamCount, RequiredParamsCurrent));
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsCheckObjectType
 *
 * PARAMETERS:  Data            - Pointer to validation data structure
 *              ReturnObjectPtr - Pointer to the object returned from the
 *                                evaluation of a method or object
 *              ExpectedBtypes  - Bitmap of expected return type(s)
 *              PackageIndex    - Index of object within parent package (if
 *                                applicable - ACPI_NOT_PACKAGE_ELEMENT
 *                                otherwise)
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Check the type of the return object against the expected object
 *              type(s). Use of Btype allows multiple expected object types.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiNsCheckObjectType (
    ACPI_PREDEFINED_DATA        *Data,
    ACPI_OPERAND_OBJECT         **ReturnObjectPtr,
    UINT32                      ExpectedBtypes,
    UINT32                      PackageIndex)
{
    ACPI_OPERAND_OBJECT         *ReturnObject = *ReturnObjectPtr;
    ACPI_STATUS                 Status = AE_OK;
    char                        TypeBuffer[48]; /* Room for 5 types */


    /* A Namespace node should not get here, but make sure */

    if (ReturnObject &&
        ACPI_GET_DESCRIPTOR_TYPE (ReturnObject) == ACPI_DESC_TYPE_NAMED)
    {
        ACPI_WARN_PREDEFINED ((AE_INFO, Data->Pathname, Data->NodeFlags,
            "Invalid return type - Found a Namespace node [%4.4s] type %s",
            ReturnObject->Node.Name.Ascii,
            AcpiUtGetTypeName (ReturnObject->Node.Type)));
        return (AE_AML_OPERAND_TYPE);
    }

    /*
     * Convert the object type (ACPI_TYPE_xxx) to a bitmapped object type.
     * The bitmapped type allows multiple possible return types.
     *
     * Note, the cases below must handle all of the possible types returned
     * from all of the predefined names (including elements of returned
     * packages)
     */
    Data->ReturnBtype = AcpiNsGetBitmappedType (ReturnObject);
    if (Data->ReturnBtype == ACPI_RTYPE_ANY)
    {
        /* Not one of the supported objects, must be incorrect */
        goto TypeErrorExit;
    }

    /* For reference objects, check that the reference type is correct */

    if ((Data->ReturnBtype & ExpectedBtypes) == ACPI_RTYPE_REFERENCE)
    {
        Status = AcpiNsCheckReference (Data, ReturnObject);
        return (Status);
    }

    /* Attempt simple repair of the returned object if necessary */

    Status = AcpiNsSimpleRepair (Data, ExpectedBtypes,
                PackageIndex, ReturnObjectPtr);
    return (Status);


TypeErrorExit:

    /* Create a string with all expected types for this predefined object */

    AcpiUtGetExpectedReturnTypes (TypeBuffer, ExpectedBtypes);

    if (PackageIndex == ACPI_NOT_PACKAGE_ELEMENT)
    {
        ACPI_WARN_PREDEFINED ((AE_INFO, Data->Pathname, Data->NodeFlags,
            "Return type mismatch - found %s, expected %s",
            AcpiUtGetObjectTypeName (ReturnObject), TypeBuffer));
    }
    else
    {
        ACPI_WARN_PREDEFINED ((AE_INFO, Data->Pathname, Data->NodeFlags,
            "Return Package type mismatch at index %u - "
            "found %s, expected %s", PackageIndex,
            AcpiUtGetObjectTypeName (ReturnObject), TypeBuffer));
    }

    return (AE_AML_OPERAND_TYPE);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsCheckReference
 *
 * PARAMETERS:  Data            - Pointer to validation data structure
 *              ReturnObject    - Object returned from the evaluation of a
 *                                method or object
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Check a returned reference object for the correct reference
 *              type. The only reference type that can be returned from a
 *              predefined method is a named reference. All others are invalid.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiNsCheckReference (
    ACPI_PREDEFINED_DATA        *Data,
    ACPI_OPERAND_OBJECT         *ReturnObject)
{

    /*
     * Check the reference object for the correct reference type (opcode).
     * The only type of reference that can be converted to an ACPI_OBJECT is
     * a reference to a named object (reference class: NAME)
     */
    if (ReturnObject->Reference.Class == ACPI_REFCLASS_NAME)
    {
        return (AE_OK);
    }

    ACPI_WARN_PREDEFINED ((AE_INFO, Data->Pathname, Data->NodeFlags,
        "Return type mismatch - unexpected reference object type [%s] %2.2X",
        AcpiUtGetReferenceName (ReturnObject),
        ReturnObject->Reference.Class));

    return (AE_AML_OPERAND_TYPE);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsGetBitmappedType
 *
 * PARAMETERS:  ReturnObject    - Object returned from method/obj evaluation
 *
 * RETURN:      Object return type. ACPI_RTYPE_ANY indicates that the object
 *              type is not supported. ACPI_RTYPE_NONE indicates that no
 *              object was returned (ReturnObject is NULL).
 *
 * DESCRIPTION: Convert object type into a bitmapped object return type.
 *
 ******************************************************************************/

static UINT32
AcpiNsGetBitmappedType (
    ACPI_OPERAND_OBJECT         *ReturnObject)
{
    UINT32                      ReturnBtype;


    if (!ReturnObject)
    {
        return (ACPI_RTYPE_NONE);
    }

    /* Map ACPI_OBJECT_TYPE to internal bitmapped type */

    switch (ReturnObject->Common.Type)
    {
    case ACPI_TYPE_INTEGER:
        ReturnBtype = ACPI_RTYPE_INTEGER;
        break;

    case ACPI_TYPE_BUFFER:
        ReturnBtype = ACPI_RTYPE_BUFFER;
        break;

    case ACPI_TYPE_STRING:
        ReturnBtype = ACPI_RTYPE_STRING;
        break;

    case ACPI_TYPE_PACKAGE:
        ReturnBtype = ACPI_RTYPE_PACKAGE;
        break;

    case ACPI_TYPE_LOCAL_REFERENCE:
        ReturnBtype = ACPI_RTYPE_REFERENCE;
        break;

    default:
        /* Not one of the supported objects, must be incorrect */

        ReturnBtype = ACPI_RTYPE_ANY;
        break;
    }

    return (ReturnBtype);
}
