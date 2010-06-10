/******************************************************************************
 *
 * Module Name: aslpredef - support for ACPI predefined names
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

#define ACPI_CREATE_PREDEFINED_TABLE

#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include "aslcompiler.y.h"
#include <contrib/dev/acpica/include/amlcode.h>
#include <contrib/dev/acpica/include/acparser.h>
#include <contrib/dev/acpica/include/acpredef.h>


#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslpredef")


/* Local prototypes */

static UINT32
ApCheckForSpecialName (
    ACPI_PARSE_OBJECT       *Op,
    char                    *Name);

static void
ApCheckObjectType (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  ExpectedBtypes);

static void
ApGetExpectedTypes (
    char                    *Buffer,
    UINT32                  ExpectedBtypes);


/*
 * Names for the types that can be returned by the predefined objects.
 * Used for warning messages. Must be in the same order as the ACPI_RTYPEs
 */
static const char   *AcpiRtypeNames[] =
{
    "/Integer",
    "/String",
    "/Buffer",
    "/Package",
    "/Reference",
};

/*
 * Predefined names for use in Resource Descriptors. These names do not
 * appear in the global Predefined Name table (since these names never
 * appear in actual AML byte code, only in the original ASL)
 */
static const ACPI_PREDEFINED_INFO      ResourceNames[] = {
    {{"_ALN",     0,      0}},
    {{"_ASI",     0,      0}},
    {{"_ASZ",     0,      0}},
    {{"_ATT",     0,      0}},
    {{"_BAS",     0,      0}},
    {{"_BM_",     0,      0}},
    {{"_DEC",     0,      0}},
    {{"_GRA",     0,      0}},
    {{"_HE_",     0,      0}},
    {{"_INT",     0,      0}},
    {{"_LEN",     0,      0}},
    {{"_LL_",     0,      0}},
    {{"_MAF",     0,      0}},
    {{"_MAX",     0,      0}},
    {{"_MEM",     0,      0}},
    {{"_MIF",     0,      0}},
    {{"_MIN",     0,      0}},
    {{"_MTP",     0,      0}},
    {{"_RBO",     0,      0}},
    {{"_RBW",     0,      0}},
    {{"_RNG",     0,      0}},
    {{"_RT_",     0,      0}},  /* Acpi 3.0 */
    {{"_RW_",     0,      0}},
    {{"_SHR",     0,      0}},
    {{"_SIZ",     0,      0}},
    {{"_TRA",     0,      0}},
    {{"_TRS",     0,      0}},
    {{"_TSF",     0,      0}},  /* Acpi 3.0 */
    {{"_TTP",     0,      0}},
    {{"_TYP",     0,      0}},
    {{{0,0,0,0},  0,      0}}   /* Table terminator */
};

static const ACPI_PREDEFINED_INFO      ScopeNames[] = {
    {{"_SB_",     0,      0}},
    {{"_SI_",     0,      0}},
    {{"_TZ_",     0,      0}},
    {{{0,0,0,0},  0,      0}}   /* Table terminator */
};


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

void
ApCheckForPredefinedMethod (
    ACPI_PARSE_OBJECT       *Op,
    ASL_METHOD_INFO         *MethodInfo)
{
    UINT32                  Index;
    UINT32                  RequiredArgsCurrent;
    UINT32                  RequiredArgsOld;


    /* Check for a match against the predefined name list */

    Index = ApCheckForPredefinedName (Op, Op->Asl.NameSeg);

    switch (Index)
    {
    case ACPI_NOT_RESERVED_NAME:        /* No underscore or _Txx or _xxx name not matched */
    case ACPI_PREDEFINED_NAME:          /* Resource Name or reserved scope name */
    case ACPI_COMPILER_RESERVED_NAME:   /* A _Txx that was not emitted by compiler */

        /* Just return, nothing to do */
        break;


    case ACPI_EVENT_RESERVED_NAME:      /* _Lxx/_Exx/_Wxx/_Qxx methods */

        Gbl_ReservedMethods++;

        /* NumArguments must be zero for all _Lxx/_Exx/_Wxx/_Qxx methods */

        if (MethodInfo->NumArguments != 0)
        {
            sprintf (MsgBuffer, "%s requires %d", Op->Asl.ExternalName, 0);

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

        RequiredArgsCurrent = PredefinedNames[Index].Info.ParamCount & 0x0F;
        RequiredArgsOld = PredefinedNames[Index].Info.ParamCount >> 4;

        if ((MethodInfo->NumArguments != RequiredArgsCurrent) &&
            (MethodInfo->NumArguments != RequiredArgsOld))
        {
            sprintf (MsgBuffer, "%4.4s requires %d",
                PredefinedNames[Index].Info.Name, RequiredArgsCurrent);

            if (MethodInfo->NumArguments > RequiredArgsCurrent)
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
            PredefinedNames[Index].Info.ExpectedBtypes)
        {
            ApGetExpectedTypes (StringBuffer,
                PredefinedNames[Index].Info.ExpectedBtypes);

            sprintf (MsgBuffer, "%s required for %4.4s",
                StringBuffer, PredefinedNames[Index].Info.Name);

            AslError (ASL_WARNING, ASL_MSG_RESERVED_RETURN_VALUE, Op,
                MsgBuffer);
        }
        break;
    }
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
 *              method invocation are not checked.
 *
 ******************************************************************************/

void
ApCheckPredefinedReturnValue (
    ACPI_PARSE_OBJECT       *Op,
    ASL_METHOD_INFO         *MethodInfo)
{
    UINT32                  Index;
    ACPI_PARSE_OBJECT       *ReturnValueOp;


    /* Check parent method for a match against the predefined name list */

    Index = ApCheckForPredefinedName (MethodInfo->Op,
                MethodInfo->Op->Asl.NameSeg);

    switch (Index)
    {
    case ACPI_NOT_RESERVED_NAME:        /* No underscore or _Txx or _xxx name not matched */
    case ACPI_PREDEFINED_NAME:          /* Resource Name or reserved scope name */
    case ACPI_COMPILER_RESERVED_NAME:   /* A _Txx that was not emitted by compiler */
    case ACPI_EVENT_RESERVED_NAME:      /* _Lxx/_Exx/_Wxx/_Qxx methods */

        /* Just return, nothing to do */
        return;

    default: /* A standard predefined ACPI name */

        /* Exit if no return value expected */

        if (!PredefinedNames[Index].Info.ExpectedBtypes)
        {
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

            ApCheckObjectType (ReturnValueOp,
                PredefinedNames[Index].Info.ExpectedBtypes);
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
    UINT32                  Index;


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

    default: /* A standard predefined ACPI name */

        /*
         * If this predefined name requires input arguments, then
         * it must be implemented as a control method
         */
        if (PredefinedNames[Index].Info.ParamCount > 0)
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
        if (!PredefinedNames[Index].Info.ExpectedBtypes)
        {
            AslError (ASL_ERROR, ASL_MSG_RESERVED_METHOD, Op,
                "with zero arguments");
            return;
        }

        /* Typecheck the actual object, it is the next argument */

        ApCheckObjectType (Op->Asl.Child->Asl.Next,
            PredefinedNames[Index].Info.ExpectedBtypes);
        return;
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
    UINT32                  i;


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

    for (i = 0; PredefinedNames[i].Info.Name[0]; i++)
    {
        if (ACPI_COMPARE_NAME (Name, PredefinedNames[i].Info.Name))
        {
            /* Return index into predefined array */
            return (i);
        }
    }

    /* Check for resource names and predefined scope names */

    for (i = 0; ResourceNames[i].Info.Name[0]; i++)
    {
        if (ACPI_COMPARE_NAME (Name, ResourceNames[i].Info.Name))
        {
            return (ACPI_PREDEFINED_NAME);
        }
    }

    for (i = 0; ScopeNames[i].Info.Name[0]; i++)
    {
        if (ACPI_COMPARE_NAME (Name, ScopeNames[i].Info.Name))
        {
            return (ACPI_PREDEFINED_NAME);
        }
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
 * PARAMETERS:  Op              - Current parse node
 *              ExpectedBtypes  - Bitmap of expected return type(s)
 *
 * RETURN:      None
 *
 * DESCRIPTION: Check if the object type is one of the types that is expected
 *              by the predefined name. Only a limited number of object types
 *              can be returned by the predefined names.
 *
 ******************************************************************************/

static void
ApCheckObjectType (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  ExpectedBtypes)
{
    UINT32                  ReturnBtype;


    switch (Op->Asl.ParseOpcode)
    {
    case PARSEOP_ZERO:
    case PARSEOP_ONE:
    case PARSEOP_ONES:
    case PARSEOP_INTEGER:
        ReturnBtype = ACPI_RTYPE_INTEGER;
        break;

    case PARSEOP_BUFFER:
        ReturnBtype = ACPI_RTYPE_BUFFER;
        break;

    case PARSEOP_STRING_LITERAL:
        ReturnBtype = ACPI_RTYPE_STRING;
        break;

    case PARSEOP_PACKAGE:
        ReturnBtype = ACPI_RTYPE_PACKAGE;
        break;

    default:
        /* Not one of the supported object types */

        goto TypeErrorExit;
    }

    /* Exit if the object is one of the expected types */

    if (ReturnBtype & ExpectedBtypes)
    {
        return;
    }


TypeErrorExit:

    /* Format the expected types and emit an error message */

    ApGetExpectedTypes (StringBuffer, ExpectedBtypes);

    sprintf (MsgBuffer, "found %s, requires %s",
        UtGetOpName (Op->Asl.ParseOpcode), StringBuffer);

    AslError (ASL_ERROR, ASL_MSG_RESERVED_OPERAND_TYPE, Op,
        MsgBuffer);
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
    char                        TypeBuffer[48]; /* Room for 5 types */
    UINT32                      Count;


    /*
     * Predefined names/methods
     */
    printf ("\nPredefined Name Information\n\n");

    Count = 0;
    ThisName = PredefinedNames;
    while (ThisName->Info.Name[0])
    {
        printf ("%4.4s    Requires %d arguments, ",
            ThisName->Info.Name, ThisName->Info.ParamCount & 0x0F);

        if (ThisName->Info.ExpectedBtypes)
        {
            ApGetExpectedTypes (TypeBuffer, ThisName->Info.ExpectedBtypes);
            printf ("Must return: %s\n", TypeBuffer);
        }
        else
        {
            printf ("No return value\n");
        }

        /*
         * Skip next entry in the table if this name returns a Package
         * (next entry contains the package info)
         */
        if (ThisName->Info.ExpectedBtypes & ACPI_RTYPE_PACKAGE)
        {
            ThisName++;
        }

        Count++;
        ThisName++;
    }

    printf ("%u Predefined Names are recognized\n", Count);

    /*
     * Resource Descriptor names
     */
    printf ("\nResource Descriptor Predefined Names\n\n");

    Count = 0;
    ThisName = ResourceNames;
    while (ThisName->Info.Name[0])
    {
        printf ("%4.4s    Resource Descriptor\n", ThisName->Info.Name);
        Count++;
        ThisName++;
    }

    printf ("%u Resource Descriptor Names are recognized\n", Count);

    /*
     * Predefined scope names
     */
    printf ("\nPredefined Scope Names\n\n");

    ThisName = ScopeNames;
    while (ThisName->Info.Name[0])
    {
        printf ("%4.4s    Scope\n", ThisName->Info.Name);
        ThisName++;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    ApGetExpectedTypes
 *
 * PARAMETERS:  Buffer              - Where the formatted string is returned
 *              ExpectedBTypes      - Bitfield of expected data types
 *
 * RETURN:      None, formatted string
 *
 * DESCRIPTION: Format the expected object types into a printable string.
 *
 ******************************************************************************/

static void
ApGetExpectedTypes (
    char                        *Buffer,
    UINT32                      ExpectedBtypes)
{
    UINT32                      ThisRtype;
    UINT32                      i;
    UINT32                      j;


    j = 1;
    Buffer[0] = 0;
    ThisRtype = ACPI_RTYPE_INTEGER;

    for (i = 0; i < ACPI_NUM_RTYPES; i++)
    {
        /* If one of the expected types, concatenate the name of this type */

        if (ExpectedBtypes & ThisRtype)
        {
            ACPI_STRCAT (Buffer, &AcpiRtypeNames[i][j]);
            j = 0;              /* Use name separator from now on */
        }
        ThisRtype <<= 1;    /* Next Rtype */
    }
}

