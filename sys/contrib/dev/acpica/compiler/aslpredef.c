/******************************************************************************
 *
 * Module Name: aslpredef - support for ACPI predefined names
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
#define ACPI_CREATE_RESOURCE_TABLE

#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include "aslcompiler.y.h"
#include <contrib/dev/acpica/include/acpredef.h>
#include <contrib/dev/acpica/include/acnamesp.h>


#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslpredef")


/* Local prototypes */

static void
ApCheckForUnexpectedReturnValue (
    ACPI_PARSE_OBJECT       *Op,
    ASL_METHOD_INFO         *MethodInfo);

static UINT32
ApCheckForSpecialName (
    ACPI_PARSE_OBJECT       *Op,
    char                    *Name);


/*******************************************************************************
 *
 * FUNCTION:    ApCheckForPredefinedMethod
 *
 * PARAMETERS:  Op              - A parse node of type "METHOD".
 *              MethodInfo      - Saved info about this method
 *
 * RETURN:      None
 *
 * DESCRIPTION: If method is a predefined name, check that the number of
 *              arguments and the return type (returns a value or not)
 *              is correct.
 *
 ******************************************************************************/

BOOLEAN
ApCheckForPredefinedMethod (
    ACPI_PARSE_OBJECT       *Op,
    ASL_METHOD_INFO         *MethodInfo)
{
    UINT32                      Index;
    UINT32                      RequiredArgCount;
    const ACPI_PREDEFINED_INFO  *ThisName;


    /* Check for a match against the predefined name list */

    Index = ApCheckForPredefinedName (Op, Op->Asl.NameSeg);

    switch (Index)
    {
    case ACPI_NOT_RESERVED_NAME:        /* No underscore or _Txx or _xxx name not matched */
    case ACPI_PREDEFINED_NAME:          /* Resource Name or reserved scope name */
    case ACPI_COMPILER_RESERVED_NAME:   /* A _Txx that was not emitted by compiler */

        /* Just return, nothing to do */
        return (FALSE);


    case ACPI_EVENT_RESERVED_NAME:      /* _Lxx/_Exx/_Wxx/_Qxx methods */

        Gbl_ReservedMethods++;

        /* NumArguments must be zero for all _Lxx/_Exx/_Wxx/_Qxx methods */

        if (MethodInfo->NumArguments != 0)
        {
            sprintf (MsgBuffer, "%s requires %u", Op->Asl.ExternalName, 0);

            AslError (ASL_WARNING, ASL_MSG_RESERVED_ARG_COUNT_HI, Op,
                MsgBuffer);
        }
        break;


    default:
        /*
         * Matched a predefined method name
         *
         * Validate the ASL-defined argument count. Allow two different legal
         * arg counts.
         */
        Gbl_ReservedMethods++;
        ThisName = &AcpiGbl_PredefinedMethods[Index];
        RequiredArgCount = ThisName->Info.ArgumentList & METHOD_ARG_MASK;

        if (MethodInfo->NumArguments != RequiredArgCount)
        {
            sprintf (MsgBuffer, "%4.4s requires %u",
                ThisName->Info.Name, RequiredArgCount);

            if ((MethodInfo->NumArguments > RequiredArgCount) &&
                !(ThisName->Info.ArgumentList & ARG_COUNT_IS_MINIMUM))
            {
                AslError (ASL_WARNING, ASL_MSG_RESERVED_ARG_COUNT_HI, Op,
                    MsgBuffer);
            }
            else
            {
                AslError (ASL_WARNING, ASL_MSG_RESERVED_ARG_COUNT_LO, Op,
                    MsgBuffer);
            }
        }

        /*
         * Check if method returns no value, but the predefined name is
         * required to return a value
         */
        if (MethodInfo->NumReturnNoValue &&
            ThisName->Info.ExpectedBtypes)
        {
            AcpiUtGetExpectedReturnTypes (StringBuffer,
                ThisName->Info.ExpectedBtypes);

            sprintf (MsgBuffer, "%s required for %4.4s",
                StringBuffer, ThisName->Info.Name);

            AslError (ASL_WARNING, ASL_MSG_RESERVED_RETURN_VALUE, Op,
                MsgBuffer);
        }
        break;
    }

    return (TRUE);
}


/*******************************************************************************
 *
 * FUNCTION:    ApCheckForUnexpectedReturnValue
 *
 * PARAMETERS:  Op              - A parse node of type "RETURN".
 *              MethodInfo      - Saved info about this method
 *
 * RETURN:      None
 *
 * DESCRIPTION: Check for an unexpected return value from a predefined method.
 *              Invoked for predefined methods that are defined to not return
 *              any value. If there is a return value, issue a remark, since
 *              the ASL writer may be confused as to the method definition
 *              and/or functionality.
 *
 * Note: We ignore all return values of "Zero", since this is what a standalone
 *       Return() statement will always generate -- so we ignore it here --
 *       i.e., there is no difference between Return() and Return(Zero).
 *       Also, a null Return() will be disassembled to return(Zero) -- so, we
 *       don't want to generate extraneous remarks/warnings for a disassembled
 *       ASL file.
 *
 ******************************************************************************/

static void
ApCheckForUnexpectedReturnValue (
    ACPI_PARSE_OBJECT       *Op,
    ASL_METHOD_INFO         *MethodInfo)
{
    ACPI_PARSE_OBJECT       *ReturnValueOp;


    /* Ignore Return() and Return(Zero) (they are the same) */

    ReturnValueOp = Op->Asl.Child;
    if (ReturnValueOp->Asl.ParseOpcode == PARSEOP_ZERO)
    {
        return;
    }

    /* We have a valid return value, but the reserved name did not expect it */

    AslError (ASL_WARNING, ASL_MSG_RESERVED_NO_RETURN_VAL,
        Op, MethodInfo->Op->Asl.ExternalName);
}


/*******************************************************************************
 *
 * FUNCTION:    ApCheckPredefinedReturnValue
 *
 * PARAMETERS:  Op              - A parse node of type "RETURN".
 *              MethodInfo      - Saved info about this method
 *
 * RETURN:      None
 *
 * DESCRIPTION: If method is a predefined name, attempt to validate the return
 *              value. Only "static" types can be validated - a simple return
 *              of an integer/string/buffer/package or a named reference to
 *              a static object. Values such as a Localx or Argx or a control
 *              method invocation are not checked. Issue a warning if there is
 *              a valid return value, but the reserved method defines no
 *              return value.
 *
 ******************************************************************************/

void
ApCheckPredefinedReturnValue (
    ACPI_PARSE_OBJECT       *Op,
    ASL_METHOD_INFO         *MethodInfo)
{
    UINT32                      Index;
    ACPI_PARSE_OBJECT           *ReturnValueOp;
    const ACPI_PREDEFINED_INFO  *ThisName;


    /* Check parent method for a match against the predefined name list */

    Index = ApCheckForPredefinedName (MethodInfo->Op,
                MethodInfo->Op->Asl.NameSeg);

    switch (Index)
    {
    case ACPI_EVENT_RESERVED_NAME:      /* _Lxx/_Exx/_Wxx/_Qxx methods */

        /* No return value expected, warn if there is one */

        ApCheckForUnexpectedReturnValue (Op, MethodInfo);
        return;

    case ACPI_NOT_RESERVED_NAME:        /* No underscore or _Txx or _xxx name not matched */
    case ACPI_PREDEFINED_NAME:          /* Resource Name or reserved scope name */
    case ACPI_COMPILER_RESERVED_NAME:   /* A _Txx that was not emitted by compiler */

        /* Just return, nothing to do */
        return;

    default: /* A standard predefined ACPI name */

        ThisName = &AcpiGbl_PredefinedMethods[Index];
        if (!ThisName->Info.ExpectedBtypes)
        {
            /* No return value expected, warn if there is one */

            ApCheckForUnexpectedReturnValue (Op, MethodInfo);
            return;
        }

        /* Get the object returned, it is the next argument */

        ReturnValueOp = Op->Asl.Child;
        switch (ReturnValueOp->Asl.ParseOpcode)
        {
        case PARSEOP_ZERO:
        case PARSEOP_ONE:
        case PARSEOP_ONES:
        case PARSEOP_INTEGER:
        case PARSEOP_STRING_LITERAL:
        case PARSEOP_BUFFER:
        case PARSEOP_PACKAGE:

            /* Static data return object - check against expected type */

            ApCheckObjectType (ThisName->Info.Name, ReturnValueOp,
                ThisName->Info.ExpectedBtypes, ACPI_NOT_PACKAGE_ELEMENT);

            /* For packages, check the individual package elements */

            if (ReturnValueOp->Asl.ParseOpcode == PARSEOP_PACKAGE)
            {
                ApCheckPackage (ReturnValueOp, ThisName);
            }
            break;

        default:

            /*
             * All other ops are very difficult or impossible to typecheck at
             * compile time. These include all Localx, Argx, and method
             * invocations. Also, NAMESEG and NAMESTRING because the type of
             * any named object can be changed at runtime (for example,
             * CopyObject will change the type of the target object.)
             */
            break;
        }
    }
}


/*******************************************************************************
 *
 * FUNCTION:    ApCheckForPredefinedObject
 *
 * PARAMETERS:  Op              - A parse node
 *              Name            - The ACPI name to be checked
 *
 * RETURN:      None
 *
 * DESCRIPTION: Check for a predefined name for a static object (created via
 *              the ASL Name operator). If it is a predefined ACPI name, ensure
 *              that the name does not require any arguments (which would
 *              require a control method implemenation of the name), and that
 *              the type of the object is one of the expected types for the
 *              predefined name.
 *
 ******************************************************************************/

void
ApCheckForPredefinedObject (
    ACPI_PARSE_OBJECT       *Op,
    char                    *Name)
{
    UINT32                      Index;
    ACPI_PARSE_OBJECT           *ObjectOp;
    const ACPI_PREDEFINED_INFO  *ThisName;


    /*
     * Check for a real predefined name -- not a resource descriptor name
     * or a predefined scope name
     */
    Index = ApCheckForPredefinedName (Op, Name);

    switch (Index)
    {
    case ACPI_NOT_RESERVED_NAME:        /* No underscore or _Txx or _xxx name not matched */
    case ACPI_PREDEFINED_NAME:          /* Resource Name or reserved scope name */
    case ACPI_COMPILER_RESERVED_NAME:   /* A _Txx that was not emitted by compiler */

        /* Nothing to do */
        return;

    case ACPI_EVENT_RESERVED_NAME:      /* _Lxx/_Exx/_Wxx/_Qxx methods */

        /*
         * These names must be control methods, by definition in ACPI spec.
         * Also because they are defined to return no value. None of them
         * require any arguments.
         */
        AslError (ASL_ERROR, ASL_MSG_RESERVED_METHOD, Op,
            "with zero arguments");
        return;

    default:
        break;
    }

    /* A standard predefined ACPI name */

    /*
     * If this predefined name requires input arguments, then
     * it must be implemented as a control method
     */
    ThisName = &AcpiGbl_PredefinedMethods[Index];
    if ((ThisName->Info.ArgumentList & METHOD_ARG_MASK) > 0)
    {
        AslError (ASL_ERROR, ASL_MSG_RESERVED_METHOD, Op,
            "with arguments");
        return;
    }

    /*
     * If no return value is expected from this predefined name, then
     * it follows that it must be implemented as a control method
     * (with zero args, because the args > 0 case was handled above)
     * Examples are: _DIS, _INI, _IRC, _OFF, _ON, _PSx
     */
    if (!ThisName->Info.ExpectedBtypes)
    {
        AslError (ASL_ERROR, ASL_MSG_RESERVED_METHOD, Op,
            "with zero arguments");
        return;
    }

    /* Typecheck the actual object, it is the next argument */

    ObjectOp = Op->Asl.Child->Asl.Next;
    ApCheckObjectType (ThisName->Info.Name, Op->Asl.Child->Asl.Next,
        ThisName->Info.ExpectedBtypes, ACPI_NOT_PACKAGE_ELEMENT);

    /* For packages, check the individual package elements */

    if (ObjectOp->Asl.ParseOpcode == PARSEOP_PACKAGE)
    {
        ApCheckPackage (ObjectOp, ThisName);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    ApCheckForPredefinedName
 *
 * PARAMETERS:  Op              - A parse node
 *              Name            - NameSeg to check
 *
 * RETURN:      None
 *
 * DESCRIPTION: Check a NameSeg against the reserved list.
 *
 ******************************************************************************/

UINT32
ApCheckForPredefinedName (
    ACPI_PARSE_OBJECT       *Op,
    char                    *Name)
{
    UINT32                      i;
    const ACPI_PREDEFINED_INFO  *ThisName;


    if (Name[0] == 0)
    {
        AcpiOsPrintf ("Found a null name, external = %s\n",
            Op->Asl.ExternalName);
    }

    /* All reserved names are prefixed with a single underscore */

    if (Name[0] != '_')
    {
        return (ACPI_NOT_RESERVED_NAME);
    }

    /* Check for a standard predefined method name */

    ThisName = AcpiGbl_PredefinedMethods;
    for (i = 0; ThisName->Info.Name[0]; i++)
    {
        if (ACPI_COMPARE_NAME (Name, ThisName->Info.Name))
        {
            /* Return index into predefined array */
            return (i);
        }

        ThisName++; /* Does not account for extra package data, but is OK */
    }

    /* Check for resource names and predefined scope names */

    ThisName = AcpiGbl_ResourceNames;
    while (ThisName->Info.Name[0])
    {
        if (ACPI_COMPARE_NAME (Name, ThisName->Info.Name))
        {
            return (ACPI_PREDEFINED_NAME);
        }

        ThisName++;
    }

    ThisName = AcpiGbl_ScopeNames;
    while (ThisName->Info.Name[0])
    {
        if (ACPI_COMPARE_NAME (Name, ThisName->Info.Name))
        {
            return (ACPI_PREDEFINED_NAME);
        }

        ThisName++;
    }

    /* Check for _Lxx/_Exx/_Wxx/_Qxx/_T_x. Warning if unknown predefined name */

    return (ApCheckForSpecialName (Op, Name));
}


/*******************************************************************************
 *
 * FUNCTION:    ApCheckForSpecialName
 *
 * PARAMETERS:  Op              - A parse node
 *              Name            - NameSeg to check
 *
 * RETURN:      None
 *
 * DESCRIPTION: Check for the "special" predefined names -
 *              _Lxx, _Exx, _Qxx, _Wxx, and _T_x
 *
 ******************************************************************************/

static UINT32
ApCheckForSpecialName (
    ACPI_PARSE_OBJECT       *Op,
    char                    *Name)
{

    /*
     * Check for the "special" predefined names. We already know that the
     * first character is an underscore.
     *   GPE:  _Lxx
     *   GPE:  _Exx
     *   GPE:  _Wxx
     *   EC:   _Qxx
     */
    if ((Name[1] == 'L') ||
        (Name[1] == 'E') ||
        (Name[1] == 'W') ||
        (Name[1] == 'Q'))
    {
        /* The next two characters must be hex digits */

        if ((isxdigit ((int) Name[2])) &&
            (isxdigit ((int) Name[3])))
        {
            return (ACPI_EVENT_RESERVED_NAME);
        }
    }

    /* Check for the names reserved for the compiler itself: _T_x */

    else if ((Op->Asl.ExternalName[1] == 'T') &&
             (Op->Asl.ExternalName[2] == '_'))
    {
        /* Ignore if actually emitted by the compiler */

        if (Op->Asl.CompileFlags & NODE_COMPILER_EMITTED)
        {
            return (ACPI_NOT_RESERVED_NAME);
        }

        /*
         * Was not actually emitted by the compiler. This is a special case,
         * however. If the ASL code being compiled was the result of a
         * dissasembly, it may possibly contain valid compiler-emitted names
         * of the form "_T_x". We don't want to issue an error or even a
         * warning and force the user to manually change the names. So, we
         * will issue a remark instead.
         */
        AslError (ASL_REMARK, ASL_MSG_COMPILER_RESERVED, Op, Op->Asl.ExternalName);
        return (ACPI_COMPILER_RESERVED_NAME);
    }

    /*
     * The name didn't match any of the known predefined names. Flag it as a
     * warning, since the entire namespace starting with an underscore is
     * reserved by the ACPI spec.
     */
    AslError (ASL_WARNING, ASL_MSG_UNKNOWN_RESERVED_NAME, Op,
        Op->Asl.ExternalName);

    return (ACPI_NOT_RESERVED_NAME);
}


/*******************************************************************************
 *
 * FUNCTION:    ApCheckObjectType
 *
 * PARAMETERS:  PredefinedName  - Name of the predefined object we are checking
 *              Op              - Current parse node
 *              ExpectedBtypes  - Bitmap of expected return type(s)
 *              PackageIndex    - Index of object within parent package (if
 *                                applicable - ACPI_NOT_PACKAGE_ELEMENT
 *                                otherwise)
 *
 * RETURN:      None
 *
 * DESCRIPTION: Check if the object type is one of the types that is expected
 *              by the predefined name. Only a limited number of object types
 *              can be returned by the predefined names.
 *
 ******************************************************************************/

ACPI_STATUS
ApCheckObjectType (
    const char              *PredefinedName,
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  ExpectedBtypes,
    UINT32                  PackageIndex)
{
    UINT32                  ReturnBtype;
    char                    *TypeName;


    if (!Op)
    {
        return (AE_TYPE);
    }

    /* Map the parse opcode to a bitmapped return type (RTYPE) */

    switch (Op->Asl.ParseOpcode)
    {
    case PARSEOP_ZERO:
    case PARSEOP_ONE:
    case PARSEOP_ONES:
    case PARSEOP_INTEGER:
        ReturnBtype = ACPI_RTYPE_INTEGER;
        TypeName = "Integer";
        break;

    case PARSEOP_STRING_LITERAL:
        ReturnBtype = ACPI_RTYPE_STRING;
        TypeName = "String";
        break;

    case PARSEOP_BUFFER:
        ReturnBtype = ACPI_RTYPE_BUFFER;
        TypeName = "Buffer";
        break;

    case PARSEOP_PACKAGE:
    case PARSEOP_VAR_PACKAGE:
        ReturnBtype = ACPI_RTYPE_PACKAGE;
        TypeName = "Package";
        break;

    case PARSEOP_NAMESEG:
    case PARSEOP_NAMESTRING:
        ReturnBtype = ACPI_RTYPE_REFERENCE;
        TypeName = "Reference";
        break;

    default:
        /* Not one of the supported object types */

        TypeName = UtGetOpName (Op->Asl.ParseOpcode);
        goto TypeErrorExit;
    }

    /* Exit if the object is one of the expected types */

    if (ReturnBtype & ExpectedBtypes)
    {
        return (AE_OK);
    }


TypeErrorExit:

    /* Format the expected types and emit an error message */

    AcpiUtGetExpectedReturnTypes (StringBuffer, ExpectedBtypes);

    if (PackageIndex == ACPI_NOT_PACKAGE_ELEMENT)
    {
        sprintf (MsgBuffer, "%4.4s: found %s, %s required",
            PredefinedName, TypeName, StringBuffer);
    }
    else
    {
        sprintf (MsgBuffer, "%4.4s: found %s at index %u, %s required",
            PredefinedName, TypeName, PackageIndex, StringBuffer);
    }

    AslError (ASL_ERROR, ASL_MSG_RESERVED_OPERAND_TYPE, Op, MsgBuffer);
    return (AE_TYPE);
}


/*******************************************************************************
 *
 * FUNCTION:    ApDisplayReservedNames
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Dump information about the ACPI predefined names and predefined
 *              resource descriptor names.
 *
 ******************************************************************************/

void
ApDisplayReservedNames (
    void)
{
    const ACPI_PREDEFINED_INFO  *ThisName;
    UINT32                      Count;
    UINT32                      NumTypes;


    /*
     * Predefined names/methods
     */
    printf ("\nPredefined Name Information\n\n");

    Count = 0;
    ThisName = AcpiGbl_PredefinedMethods;
    while (ThisName->Info.Name[0])
    {
        AcpiUtDisplayPredefinedMethod (MsgBuffer, ThisName, FALSE);
        Count++;
        ThisName = AcpiUtGetNextPredefinedMethod (ThisName);
    }

    printf ("%u Predefined Names are recognized\n", Count);

    /*
     * Resource Descriptor names
     */
    printf ("\nPredefined Names for Resource Descriptor Fields\n\n");

    Count = 0;
    ThisName = AcpiGbl_ResourceNames;
    while (ThisName->Info.Name[0])
    {
        NumTypes = AcpiUtGetResourceBitWidth (MsgBuffer,
            ThisName->Info.ArgumentList);

        printf ("%4.4s    Field is %s bits wide%s\n",
            ThisName->Info.Name, MsgBuffer,
            (NumTypes > 1) ? " (depending on descriptor type)" : "");

        Count++;
        ThisName++;
    }

    printf ("%u Resource Descriptor Field Names are recognized\n", Count);

    /*
     * Predefined scope names
     */
    printf ("\nPredefined Scope/Device Names (automatically created at root)\n\n");

    ThisName = AcpiGbl_ScopeNames;
    while (ThisName->Info.Name[0])
    {
        printf ("%4.4s    Scope/Device\n", ThisName->Info.Name);
        ThisName++;
    }
}
