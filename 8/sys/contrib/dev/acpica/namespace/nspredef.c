/******************************************************************************
 *
 * Module Name: nspredef - Validation of ACPI predefined methods and objects
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2009, Intel Corp.
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

#define __NSPREDEF_C__

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
AcpiNsCheckPackage (
    char                        *Pathname,
    ACPI_OPERAND_OBJECT         **ReturnObjectPtr,
    const ACPI_PREDEFINED_INFO  *Predefined);

static ACPI_STATUS
AcpiNsCheckPackageElements (
    char                        *Pathname,
    ACPI_OPERAND_OBJECT         **Elements,
    UINT8                       Type1,
    UINT32                      Count1,
    UINT8                       Type2,
    UINT32                      Count2,
    UINT32                      StartIndex);

static ACPI_STATUS
AcpiNsCheckObjectType (
    char                        *Pathname,
    ACPI_OPERAND_OBJECT         **ReturnObjectPtr,
    UINT32                      ExpectedBtypes,
    UINT32                      PackageIndex);

static ACPI_STATUS
AcpiNsCheckReference (
    char                        *Pathname,
    ACPI_OPERAND_OBJECT         *ReturnObject);

static ACPI_STATUS
AcpiNsRepairObject (
    UINT32                      ExpectedBtypes,
    UINT32                      PackageIndex,
    ACPI_OPERAND_OBJECT         **ReturnObjectPtr);

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

#define ACPI_NOT_PACKAGE    ACPI_UINT32_MAX


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsCheckPredefinedNames
 *
 * PARAMETERS:  Node            - Namespace node for the method/object
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
    ACPI_OPERAND_OBJECT         *ReturnObject = *ReturnObjectPtr;
    ACPI_STATUS                 Status = AE_OK;
    const ACPI_PREDEFINED_INFO  *Predefined;
    char                        *Pathname;


    /* Match the name for this method/object against the predefined list */

    Predefined = AcpiNsCheckForPredefinedName (Node);

    /* Get the full pathname to the object, for use in error messages */

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
        goto Exit;
    }

    /* If the method failed, we cannot validate the return object */

    if ((ReturnStatus != AE_OK) && (ReturnStatus != AE_CTRL_RETURN_VALUE))
    {
        goto Exit;
    }

    /*
     * Only validate the return value on the first successful evaluation of
     * the method. This ensures that any warnings will only be emitted during
     * the very first evaluation of the method/object.
     */
    if (Node->Flags & ANOBJ_EVALUATED)
    {
        goto Exit;
    }

    /* Mark the node as having been successfully evaluated */

    Node->Flags |= ANOBJ_EVALUATED;

    /*
     * If there is no return value, check if we require a return value for
     * this predefined name. Either one return value is expected, or none,
     * for both methods and other objects.
     *
     * Exit now if there is no return object. Warning if one was expected.
     */
    if (!ReturnObject)
    {
        if ((Predefined->Info.ExpectedBtypes) &&
            (!(Predefined->Info.ExpectedBtypes & ACPI_RTYPE_NONE)))
        {
            ACPI_ERROR ((AE_INFO,
                "%s: Missing expected return value", Pathname));

            Status = AE_AML_NO_RETURN_VALUE;
        }
        goto Exit;
    }

    /*
     * We have a return value, but if one wasn't expected, just exit, this is
     * not a problem
     *
     * For example, if the "Implicit Return" feature is enabled, methods will
     * always return a value
     */
    if (!Predefined->Info.ExpectedBtypes)
    {
        goto Exit;
    }

    /*
     * Check that the type of the return object is what is expected for
     * this predefined name
     */
    Status = AcpiNsCheckObjectType (Pathname, ReturnObjectPtr,
                Predefined->Info.ExpectedBtypes, ACPI_NOT_PACKAGE);
    if (ACPI_FAILURE (Status))
    {
        goto Exit;
    }

    /* For returned Package objects, check the type of all sub-objects */

    if (ReturnObject->Common.Type == ACPI_TYPE_PACKAGE)
    {
        Status = AcpiNsCheckPackage (Pathname, ReturnObjectPtr, Predefined);
    }

Exit:
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

    /* Argument count check for non-predefined methods/objects */

    if (!Predefined)
    {
        /*
         * Warning if too few or too many arguments have been passed by the
         * caller. An incorrect number of arguments may not cause the method
         * to fail. However, the method will fail if there are too few
         * arguments and the method attempts to use one of the missing ones.
         */
        if (UserParamCount < ParamCount)
        {
            ACPI_WARNING ((AE_INFO,
                "%s: Insufficient arguments - needs %d, found %d",
                Pathname, ParamCount, UserParamCount));
        }
        else if (UserParamCount > ParamCount)
        {
            ACPI_WARNING ((AE_INFO,
                "%s: Excess arguments - needs %d, found %d",
                Pathname, ParamCount, UserParamCount));
        }
        return;
    }

    /* Allow two different legal argument counts (_SCP, etc.) */

    RequiredParamsCurrent = Predefined->Info.ParamCount & 0x0F;
    RequiredParamsOld = Predefined->Info.ParamCount >> 4;

    if (UserParamCount != ACPI_UINT32_MAX)
    {
        /* Validate the user-supplied parameter count */

        if ((UserParamCount != RequiredParamsCurrent) &&
            (UserParamCount != RequiredParamsOld))
        {
            ACPI_WARNING ((AE_INFO,
                "%s: Parameter count mismatch - "
                "caller passed %d, ACPI requires %d",
                Pathname, UserParamCount, RequiredParamsCurrent));
        }
    }

    /*
     * Only validate the argument count on the first successful evaluation of
     * the method. This ensures that any warnings will only be emitted during
     * the very first evaluation of the method/object.
     */
    if (Node->Flags & ANOBJ_EVALUATED)
    {
        return;
    }

    /*
     * Check that the ASL-defined parameter count is what is expected for
     * this predefined name.
     */
    if ((ParamCount != RequiredParamsCurrent) &&
        (ParamCount != RequiredParamsOld))
    {
        ACPI_WARNING ((AE_INFO,
            "%s: Parameter count mismatch - ASL declared %d, ACPI requires %d",
            Pathname, ParamCount, RequiredParamsCurrent));
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsCheckForPredefinedName
 *
 * PARAMETERS:  Node            - Namespace node for the method/object
 *
 * RETURN:      Pointer to entry in predefined table. NULL indicates not found.
 *
 * DESCRIPTION: Check an object name against the predefined object list.
 *
 ******************************************************************************/

const ACPI_PREDEFINED_INFO *
AcpiNsCheckForPredefinedName (
    ACPI_NAMESPACE_NODE         *Node)
{
    const ACPI_PREDEFINED_INFO  *ThisName;


    /* Quick check for a predefined name, first character must be underscore */

    if (Node->Name.Ascii[0] != '_')
    {
        return (NULL);
    }

    /* Search info table for a predefined method/object name */

    ThisName = PredefinedNames;
    while (ThisName->Info.Name[0])
    {
        if (ACPI_COMPARE_NAME (Node->Name.Ascii, ThisName->Info.Name))
        {
            /* Return pointer to this table entry */

            return (ThisName);
        }

        /*
         * Skip next entry in the table if this name returns a Package
         * (next entry contains the package info)
         */
        if (ThisName->Info.ExpectedBtypes & ACPI_RTYPE_PACKAGE)
        {
            ThisName++;
        }

        ThisName++;
    }

    return (NULL);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsCheckPackage
 *
 * PARAMETERS:  Pathname        - Full pathname to the node (for error msgs)
 *              ReturnObjectPtr - Pointer to the object returned from the
 *                                evaluation of a method or object
 *              Predefined      - Pointer to entry in predefined name table
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Check a returned package object for the correct count and
 *              correct type of all sub-objects.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiNsCheckPackage (
    char                        *Pathname,
    ACPI_OPERAND_OBJECT         **ReturnObjectPtr,
    const ACPI_PREDEFINED_INFO  *Predefined)
{
    ACPI_OPERAND_OBJECT         *ReturnObject = *ReturnObjectPtr;
    const ACPI_PREDEFINED_INFO  *Package;
    ACPI_OPERAND_OBJECT         *SubPackage;
    ACPI_OPERAND_OBJECT         **Elements;
    ACPI_OPERAND_OBJECT         **SubElements;
    ACPI_STATUS                 Status;
    UINT32                      ExpectedCount;
    UINT32                      Count;
    UINT32                      i;
    UINT32                      j;


    ACPI_FUNCTION_NAME (NsCheckPackage);


    /* The package info for this name is in the next table entry */

    Package = Predefined + 1;

    ACPI_DEBUG_PRINT ((ACPI_DB_NAMES,
        "%s Validating return Package of Type %X, Count %X\n",
        Pathname, Package->RetInfo.Type, ReturnObject->Package.Count));

    /* Extract package count and elements array */

    Elements = ReturnObject->Package.Elements;
    Count = ReturnObject->Package.Count;

    /* The package must have at least one element, else invalid */

    if (!Count)
    {
        ACPI_WARNING ((AE_INFO,
            "%s: Return Package has no elements (empty)", Pathname));

        return (AE_AML_OPERAND_VALUE);
    }

    /*
     * Decode the type of the expected package contents
     *
     * PTYPE1 packages contain no subpackages
     * PTYPE2 packages contain sub-packages
     */
    switch (Package->RetInfo.Type)
    {
    case ACPI_PTYPE1_FIXED:

        /*
         * The package count is fixed and there are no sub-packages
         *
         * If package is too small, exit.
         * If package is larger than expected, issue warning but continue
         */
        ExpectedCount = Package->RetInfo.Count1 + Package->RetInfo.Count2;
        if (Count < ExpectedCount)
        {
            goto PackageTooSmall;
        }
        else if (Count > ExpectedCount)
        {
            ACPI_WARNING ((AE_INFO,
                "%s: Return Package is larger than needed - "
                "found %u, expected %u", Pathname, Count, ExpectedCount));
        }

        /* Validate all elements of the returned package */

        Status = AcpiNsCheckPackageElements (Pathname, Elements,
                    Package->RetInfo.ObjectType1, Package->RetInfo.Count1,
                    Package->RetInfo.ObjectType2, Package->RetInfo.Count2, 0);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }
        break;


    case ACPI_PTYPE1_VAR:

        /*
         * The package count is variable, there are no sub-packages, and all
         * elements must be of the same type
         */
        for (i = 0; i < Count; i++)
        {
            Status = AcpiNsCheckObjectType (Pathname, Elements,
                        Package->RetInfo.ObjectType1, i);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }
            Elements++;
        }
        break;


    case ACPI_PTYPE1_OPTION:

        /*
         * The package count is variable, there are no sub-packages. There are
         * a fixed number of required elements, and a variable number of
         * optional elements.
         *
         * Check if package is at least as large as the minimum required
         */
        ExpectedCount = Package->RetInfo3.Count;
        if (Count < ExpectedCount)
        {
            goto PackageTooSmall;
        }

        /* Variable number of sub-objects */

        for (i = 0; i < Count; i++)
        {
            if (i < Package->RetInfo3.Count)
            {
                /* These are the required package elements (0, 1, or 2) */

                Status = AcpiNsCheckObjectType (Pathname, Elements,
                            Package->RetInfo3.ObjectType[i], i);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }
            }
            else
            {
                /* These are the optional package elements */

                Status = AcpiNsCheckObjectType (Pathname, Elements,
                            Package->RetInfo3.TailObjectType, i);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }
            }
            Elements++;
        }
        break;


    case ACPI_PTYPE2_PKG_COUNT:

        /* First element is the (Integer) count of sub-packages to follow */

        Status = AcpiNsCheckObjectType (Pathname, Elements,
                    ACPI_RTYPE_INTEGER, 0);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        /*
         * Count cannot be larger than the parent package length, but allow it
         * to be smaller. The >= accounts for the Integer above.
         */
        ExpectedCount = (UINT32) (*Elements)->Integer.Value;
        if (ExpectedCount >= Count)
        {
            goto PackageTooSmall;
        }

        Count = ExpectedCount;
        Elements++;

        /* Now we can walk the sub-packages */

        /*lint -fallthrough */


    case ACPI_PTYPE2:
    case ACPI_PTYPE2_FIXED:
    case ACPI_PTYPE2_MIN:
    case ACPI_PTYPE2_COUNT:

        /*
         * These types all return a single package that consists of a variable
         * number of sub-packages
         */
        for (i = 0; i < Count; i++)
        {
            SubPackage = *Elements;
            SubElements = SubPackage->Package.Elements;

            /* Each sub-object must be of type Package */

            Status = AcpiNsCheckObjectType (Pathname, &SubPackage,
                        ACPI_RTYPE_PACKAGE, i);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            /* Examine the different types of sub-packages */

            switch (Package->RetInfo.Type)
            {
            case ACPI_PTYPE2:
            case ACPI_PTYPE2_PKG_COUNT:

                /* Each subpackage has a fixed number of elements */

                ExpectedCount =
                    Package->RetInfo.Count1 + Package->RetInfo.Count2;
                if (SubPackage->Package.Count != ExpectedCount)
                {
                    Count = SubPackage->Package.Count;
                    goto PackageTooSmall;
                }

                Status = AcpiNsCheckPackageElements (Pathname, SubElements,
                            Package->RetInfo.ObjectType1,
                            Package->RetInfo.Count1,
                            Package->RetInfo.ObjectType2,
                            Package->RetInfo.Count2, 0);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }
                break;

            case ACPI_PTYPE2_FIXED:

                /* Each sub-package has a fixed length */

                ExpectedCount = Package->RetInfo2.Count;
                if (SubPackage->Package.Count < ExpectedCount)
                {
                    Count = SubPackage->Package.Count;
                    goto PackageTooSmall;
                }

                /* Check the type of each sub-package element */

                for (j = 0; j < ExpectedCount; j++)
                {
                    Status = AcpiNsCheckObjectType (Pathname, &SubElements[j],
                                Package->RetInfo2.ObjectType[j], j);
                    if (ACPI_FAILURE (Status))
                    {
                        return (Status);
                    }
                }
                break;

            case ACPI_PTYPE2_MIN:

                /* Each sub-package has a variable but minimum length */

                ExpectedCount = Package->RetInfo.Count1;
                if (SubPackage->Package.Count < ExpectedCount)
                {
                    Count = SubPackage->Package.Count;
                    goto PackageTooSmall;
                }

                /* Check the type of each sub-package element */

                Status = AcpiNsCheckPackageElements (Pathname, SubElements,
                            Package->RetInfo.ObjectType1,
                            SubPackage->Package.Count, 0, 0, 0);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }
                break;

            case ACPI_PTYPE2_COUNT:

                /* First element is the (Integer) count of elements to follow */

                Status = AcpiNsCheckObjectType (Pathname, SubElements,
                            ACPI_RTYPE_INTEGER, 0);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }

                /* Make sure package is large enough for the Count */

                ExpectedCount = (UINT32) (*SubElements)->Integer.Value;
                if (SubPackage->Package.Count < ExpectedCount)
                {
                    Count = SubPackage->Package.Count;
                    goto PackageTooSmall;
                }

                /* Check the type of each sub-package element */

                Status = AcpiNsCheckPackageElements (Pathname,
                            (SubElements + 1),
                            Package->RetInfo.ObjectType1,
                            (ExpectedCount - 1), 0, 0, 1);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }
                break;

            default:
                break;
            }

            Elements++;
        }
        break;


    default:

        /* Should not get here if predefined info table is correct */

        ACPI_WARNING ((AE_INFO,
            "%s: Invalid internal return type in table entry: %X",
            Pathname, Package->RetInfo.Type));

        return (AE_AML_INTERNAL);
    }

    return (AE_OK);


PackageTooSmall:

    /* Error exit for the case with an incorrect package count */

    ACPI_WARNING ((AE_INFO, "%s: Return Package is too small - "
        "found %u, expected %u", Pathname, Count, ExpectedCount));

    return (AE_AML_OPERAND_VALUE);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsCheckPackageElements
 *
 * PARAMETERS:  Pathname        - Full pathname to the node (for error msgs)
 *              Elements        - Pointer to the package elements array
 *              Type1           - Object type for first group
 *              Count1          - Count for first group
 *              Type2           - Object type for second group
 *              Count2          - Count for second group
 *              StartIndex      - Start of the first group of elements
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Check that all elements of a package are of the correct object
 *              type. Supports up to two groups of different object types.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiNsCheckPackageElements (
    char                        *Pathname,
    ACPI_OPERAND_OBJECT         **Elements,
    UINT8                       Type1,
    UINT32                      Count1,
    UINT8                       Type2,
    UINT32                      Count2,
    UINT32                      StartIndex)
{
    ACPI_OPERAND_OBJECT         **ThisElement = Elements;
    ACPI_STATUS                 Status;
    UINT32                      i;


    /*
     * Up to two groups of package elements are supported by the data
     * structure. All elements in each group must be of the same type.
     * The second group can have a count of zero.
     */
    for (i = 0; i < Count1; i++)
    {
        Status = AcpiNsCheckObjectType (Pathname, ThisElement,
                    Type1, i + StartIndex);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }
        ThisElement++;
    }

    for (i = 0; i < Count2; i++)
    {
        Status = AcpiNsCheckObjectType (Pathname, ThisElement,
                    Type2, (i + Count1 + StartIndex));
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }
        ThisElement++;
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsCheckObjectType
 *
 * PARAMETERS:  Pathname        - Full pathname to the node (for error msgs)
 *              ReturnObjectPtr - Pointer to the object returned from the
 *                                evaluation of a method or object
 *              ExpectedBtypes  - Bitmap of expected return type(s)
 *              PackageIndex    - Index of object within parent package (if
 *                                applicable - ACPI_NOT_PACKAGE otherwise)
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Check the type of the return object against the expected object
 *              type(s). Use of Btype allows multiple expected object types.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiNsCheckObjectType (
    char                        *Pathname,
    ACPI_OPERAND_OBJECT         **ReturnObjectPtr,
    UINT32                      ExpectedBtypes,
    UINT32                      PackageIndex)
{
    ACPI_OPERAND_OBJECT         *ReturnObject = *ReturnObjectPtr;
    ACPI_STATUS                 Status = AE_OK;
    UINT32                      ReturnBtype;
    char                        TypeBuffer[48]; /* Room for 5 types */
    UINT32                      ThisRtype;
    UINT32                      i;
    UINT32                      j;


    /*
     * If we get a NULL ReturnObject here, it is a NULL package element,
     * and this is always an error.
     */
    if (!ReturnObject)
    {
        goto TypeErrorExit;
    }

    /* A Namespace node should not get here, but make sure */

    if (ACPI_GET_DESCRIPTOR_TYPE (ReturnObject) == ACPI_DESC_TYPE_NAMED)
    {
        ACPI_WARNING ((AE_INFO,
            "%s: Invalid return type - Found a Namespace node [%4.4s] type %s",
            Pathname, ReturnObject->Node.Name.Ascii,
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

        goto TypeErrorExit;
    }

    /* Is the object one of the expected types? */

    if (!(ReturnBtype & ExpectedBtypes))
    {
        /* Type mismatch -- attempt repair of the returned object */

        Status = AcpiNsRepairObject (ExpectedBtypes, PackageIndex,
                    ReturnObjectPtr);
        if (ACPI_SUCCESS (Status))
        {
            return (Status);
        }
        goto TypeErrorExit;
    }

    /* For reference objects, check that the reference type is correct */

    if (ReturnObject->Common.Type == ACPI_TYPE_LOCAL_REFERENCE)
    {
        Status = AcpiNsCheckReference (Pathname, ReturnObject);
    }

    return (Status);


TypeErrorExit:

    /* Create a string with all expected types for this predefined object */

    j = 1;
    TypeBuffer[0] = 0;
    ThisRtype = ACPI_RTYPE_INTEGER;

    for (i = 0; i < ACPI_NUM_RTYPES; i++)
    {
        /* If one of the expected types, concatenate the name of this type */

        if (ExpectedBtypes & ThisRtype)
        {
            ACPI_STRCAT (TypeBuffer, &AcpiRtypeNames[i][j]);
            j = 0;              /* Use name separator from now on */
        }
        ThisRtype <<= 1;    /* Next Rtype */
    }

    if (PackageIndex == ACPI_NOT_PACKAGE)
    {
        ACPI_WARNING ((AE_INFO,
            "%s: Return type mismatch - found %s, expected %s",
            Pathname, AcpiUtGetObjectTypeName (ReturnObject), TypeBuffer));
    }
    else
    {
        ACPI_WARNING ((AE_INFO,
            "%s: Return Package type mismatch at index %u - "
            "found %s, expected %s", Pathname, PackageIndex,
            AcpiUtGetObjectTypeName (ReturnObject), TypeBuffer));
    }

    return (AE_AML_OPERAND_TYPE);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsCheckReference
 *
 * PARAMETERS:  Pathname        - Full pathname to the node (for error msgs)
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
    char                        *Pathname,
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

    ACPI_WARNING ((AE_INFO,
        "%s: Return type mismatch - "
        "unexpected reference object type [%s] %2.2X",
        Pathname, AcpiUtGetReferenceName (ReturnObject),
        ReturnObject->Reference.Class));

    return (AE_AML_OPERAND_TYPE);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsRepairObject
 *
 * PARAMETERS:  Pathname        - Full pathname to the node (for error msgs)
 *              PackageIndex    - Used to determine if target is in a package
 *              ReturnObjectPtr - Pointer to the object returned from the
 *                                evaluation of a method or object
 *
 * RETURN:      Status. AE_OK if repair was successful.
 *
 * DESCRIPTION: Attempt to repair/convert a return object of a type that was
 *              not expected.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiNsRepairObject (
    UINT32                      ExpectedBtypes,
    UINT32                      PackageIndex,
    ACPI_OPERAND_OBJECT         **ReturnObjectPtr)
{
    ACPI_OPERAND_OBJECT         *ReturnObject = *ReturnObjectPtr;
    ACPI_OPERAND_OBJECT         *NewObject;
    ACPI_SIZE                   Length;


    switch (ReturnObject->Common.Type)
    {
    case ACPI_TYPE_BUFFER:

        if (!(ExpectedBtypes & ACPI_RTYPE_STRING))
        {
            return (AE_AML_OPERAND_TYPE);
        }

        /*
         * Have a Buffer, expected a String, convert. Use a ToString
         * conversion, no transform performed on the buffer data. The best
         * example of this is the _BIF method, where the string data from
         * the battery is often (incorrectly) returned as buffer object(s).
         */
        Length = 0;
        while ((Length < ReturnObject->Buffer.Length) &&
                (ReturnObject->Buffer.Pointer[Length]))
        {
            Length++;
        }

        /* Allocate a new string object */

        NewObject = AcpiUtCreateStringObject (Length);
        if (!NewObject)
        {
            return (AE_NO_MEMORY);
        }

        /*
         * Copy the raw buffer data with no transform. String is already NULL
         * terminated at Length+1.
         */
        ACPI_MEMCPY (NewObject->String.Pointer,
            ReturnObject->Buffer.Pointer, Length);

        /* Install the new return object */

        AcpiUtRemoveReference (ReturnObject);
        *ReturnObjectPtr = NewObject;

        /*
         * If the object is a package element, we need to:
         * 1. Decrement the reference count of the orignal object, it was
         *    incremented when building the package
         * 2. Increment the reference count of the new object, it will be
         *    decremented when releasing the package
         */
        if (PackageIndex != ACPI_NOT_PACKAGE)
        {
            AcpiUtRemoveReference (ReturnObject);
            AcpiUtAddReference (NewObject);
        }
        return (AE_OK);

    default:
        break;
    }

    return (AE_AML_OPERAND_TYPE);
}

