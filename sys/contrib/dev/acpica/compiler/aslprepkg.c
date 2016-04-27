/******************************************************************************
 *
 * Module Name: aslprepkg - support for ACPI predefined name package objects
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2016, Intel Corp.
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

#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include "aslcompiler.y.h"
#include <contrib/dev/acpica/include/acpredef.h>


#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslprepkg")


/* Local prototypes */

static ACPI_PARSE_OBJECT *
ApCheckPackageElements (
    const char                  *PredefinedName,
    ACPI_PARSE_OBJECT           *Op,
    UINT8                       Type1,
    UINT32                      Count1,
    UINT8                       Type2,
    UINT32                      Count2);

static void
ApCheckPackageList (
    const char                  *PredefinedName,
    ACPI_PARSE_OBJECT           *ParentOp,
    const ACPI_PREDEFINED_INFO  *Package,
    UINT32                      StartIndex,
    UINT32                      Count);

static void
ApPackageTooSmall (
    const char                  *PredefinedName,
    ACPI_PARSE_OBJECT           *Op,
    UINT32                      Count,
    UINT32                      ExpectedCount);

static void
ApZeroLengthPackage (
    const char                  *PredefinedName,
    ACPI_PARSE_OBJECT           *Op);

static void
ApPackageTooLarge (
    const char                  *PredefinedName,
    ACPI_PARSE_OBJECT           *Op,
    UINT32                      Count,
    UINT32                      ExpectedCount);

static void
ApCustomPackage (
    ACPI_PARSE_OBJECT           *ParentOp,
    const ACPI_PREDEFINED_INFO  *Predefined);


/*******************************************************************************
 *
 * FUNCTION:    ApCheckPackage
 *
 * PARAMETERS:  ParentOp            - Parser op for the package
 *              Predefined          - Pointer to package-specific info for
 *                                    the method
 *
 * RETURN:      None
 *
 * DESCRIPTION: Top-level validation for predefined name return package
 *              objects.
 *
 ******************************************************************************/

void
ApCheckPackage (
    ACPI_PARSE_OBJECT           *ParentOp,
    const ACPI_PREDEFINED_INFO  *Predefined)
{
    ACPI_PARSE_OBJECT           *Op;
    const ACPI_PREDEFINED_INFO  *Package;
    ACPI_STATUS                 Status;
    UINT32                      ExpectedCount;
    UINT32                      Count;
    UINT32                      i;


    /* The package info for this name is in the next table entry */

    Package = Predefined + 1;

    /* First child is the package length */

    Op = ParentOp->Asl.Child;
    Count = (UINT32) Op->Asl.Value.Integer;

    /*
     * Many of the variable-length top-level packages are allowed to simply
     * have zero elements. This allows the BIOS to tell the host that even
     * though the predefined name/method exists, the feature is not supported.
     * Other package types require one or more elements. In any case, there
     * is no need to continue validation.
     */
    if (!Count)
    {
        switch (Package->RetInfo.Type)
        {
        case ACPI_PTYPE1_FIXED:
        case ACPI_PTYPE1_OPTION:
        case ACPI_PTYPE2_PKG_COUNT:
        case ACPI_PTYPE2_REV_FIXED:

            ApZeroLengthPackage (Predefined->Info.Name, ParentOp);
            break;

        case ACPI_PTYPE1_VAR:
        case ACPI_PTYPE2:
        case ACPI_PTYPE2_COUNT:
        case ACPI_PTYPE2_FIXED:
        case ACPI_PTYPE2_MIN:
        case ACPI_PTYPE2_FIX_VAR:
        case ACPI_PTYPE2_VAR_VAR:
        default:

            break;
        }

        return;
    }

    /* Get the first element of the package */

    Op = Op->Asl.Next;

    /* Decode the package type */

    switch (Package->RetInfo.Type)
    {
    case ACPI_PTYPE_CUSTOM:

        ApCustomPackage (ParentOp, Predefined);
        break;

    case ACPI_PTYPE1_FIXED:
        /*
         * The package count is fixed and there are no subpackages
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
            ApPackageTooLarge (Predefined->Info.Name, ParentOp,
                Count, ExpectedCount);
        }

        /* Validate all elements of the package */

        ApCheckPackageElements (Predefined->Info.Name, Op,
            Package->RetInfo.ObjectType1, Package->RetInfo.Count1,
            Package->RetInfo.ObjectType2, Package->RetInfo.Count2);
        break;

    case ACPI_PTYPE1_VAR:
        /*
         * The package count is variable, there are no subpackages,
         * and all elements must be of the same type
         */
        for (i = 0; i < Count; i++)
        {
            ApCheckObjectType (Predefined->Info.Name, Op,
                Package->RetInfo.ObjectType1, i);
            Op = Op->Asl.Next;
        }
        break;

    case ACPI_PTYPE1_OPTION:
        /*
         * The package count is variable, there are no subpackages.
         * There are a fixed number of required elements, and a variable
         * number of optional elements.
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

                ApCheckObjectType (Predefined->Info.Name, Op,
                    Package->RetInfo3.ObjectType[i], i);
            }
            else
            {
                /* These are the optional package elements */

                ApCheckObjectType (Predefined->Info.Name, Op,
                    Package->RetInfo3.TailObjectType, i);
            }

            Op = Op->Asl.Next;
        }
        break;

    case ACPI_PTYPE2_REV_FIXED:

        /* First element is the (Integer) revision */

        ApCheckObjectType (Predefined->Info.Name, Op,
            ACPI_RTYPE_INTEGER, 0);

        Op = Op->Asl.Next;
        Count--;

        /* Examine the subpackages */

        ApCheckPackageList (Predefined->Info.Name, Op,
            Package, 1, Count);
        break;

    case ACPI_PTYPE2_PKG_COUNT:

        /* First element is the (Integer) count of subpackages to follow */

        Status = ApCheckObjectType (Predefined->Info.Name, Op,
            ACPI_RTYPE_INTEGER, 0);

        /* We must have an integer count from above (otherwise, use Count) */

        if (ACPI_SUCCESS (Status))
        {
            /*
             * Count cannot be larger than the parent package length, but
             * allow it to be smaller. The >= accounts for the Integer above.
             */
            ExpectedCount = (UINT32) Op->Asl.Value.Integer;
            if (ExpectedCount >= Count)
            {
                goto PackageTooSmall;
            }

            Count = ExpectedCount;
        }

        Op = Op->Asl.Next;

        /* Examine the subpackages */

        ApCheckPackageList (Predefined->Info.Name, Op,
            Package, 1, Count);
        break;

    case ACPI_PTYPE2_UUID_PAIR:

        /* The package contains a variable list of UUID Buffer/Package pairs */

        /* The length of the package must be even */

        if (Count & 1)
        {
            sprintf (MsgBuffer, "%4.4s: Package length, %d, must be even.",
                Predefined->Info.Name, Count);

            AslError (ASL_ERROR, ASL_MSG_RESERVED_PACKAGE_LENGTH,
                ParentOp->Asl.Child, MsgBuffer);
        }

        /* Validate the alternating types */

        for (i = 0; i < Count; ++i)
        {
            if (i & 1)
            {
                ApCheckObjectType (Predefined->Info.Name, Op,
                    Package->RetInfo.ObjectType2, i);
            }
            else
            {
                ApCheckObjectType (Predefined->Info.Name, Op,
                    Package->RetInfo.ObjectType1, i);
            }

            Op = Op->Asl.Next;
        }

        break;

    case ACPI_PTYPE2_VAR_VAR:

        /* Check for minimum size (ints at beginning + 1 subpackage) */

        ExpectedCount = Package->RetInfo4.Count1 + 1;
        if (Count < ExpectedCount)
        {
            goto PackageTooSmall;
        }

        /* Check the non-package elements at beginning of main package */

        for (i = 0; i < Package->RetInfo4.Count1; ++i)
        {
            Status = ApCheckObjectType (Predefined->Info.Name, Op,
                Package->RetInfo4.ObjectType1, i);
            Op = Op->Asl.Next;
        }

        /* Examine the variable-length list of subpackages */

        ApCheckPackageList (Predefined->Info.Name, Op,
            Package, Package->RetInfo4.Count1, Count);

        break;

    case ACPI_PTYPE2:
    case ACPI_PTYPE2_FIXED:
    case ACPI_PTYPE2_MIN:
    case ACPI_PTYPE2_COUNT:
    case ACPI_PTYPE2_FIX_VAR:
        /*
         * These types all return a single Package that consists of a
         * variable number of subpackages.
         */

        /* Examine the subpackages */

        ApCheckPackageList (Predefined->Info.Name, Op,
            Package, 0, Count);
        break;

    default:
        return;
    }

    return;

PackageTooSmall:
    ApPackageTooSmall (Predefined->Info.Name, ParentOp,
        Count, ExpectedCount);
}


/*******************************************************************************
 *
 * FUNCTION:    ApCustomPackage
 *
 * PARAMETERS:  ParentOp            - Parse op for the package
 *              Predefined          - Pointer to package-specific info for
 *                                    the method
 *
 * RETURN:      None
 *
 * DESCRIPTION: Validate packages that don't fit into the standard model and
 *              require custom code.
 *
 * NOTE: Currently used for the _BIX method only. When needed for two or more
 * methods, probably a detect/dispatch mechanism will be required.
 *
 ******************************************************************************/

static void
ApCustomPackage (
    ACPI_PARSE_OBJECT           *ParentOp,
    const ACPI_PREDEFINED_INFO  *Predefined)
{
    ACPI_PARSE_OBJECT           *Op;
    UINT32                      Count;
    UINT32                      ExpectedCount;
    UINT32                      Version;


    /* First child is the package length */

    Op = ParentOp->Asl.Child;
    Count = (UINT32) Op->Asl.Value.Integer;

    /* Get the version number, must be Integer */

    Op = Op->Asl.Next;
    Version = (UINT32) Op->Asl.Value.Integer;
    if (Op->Asl.ParseOpcode != PARSEOP_INTEGER)
    {
        AslError (ASL_ERROR, ASL_MSG_RESERVED_OPERAND_TYPE, Op, MsgBuffer);
        return;
    }

    /* Validate count (# of elements) */

    ExpectedCount = 21;         /* Version 1 */
    if (Version == 0)
    {
        ExpectedCount = 20;     /* Version 0 */
    }

    if (Count < ExpectedCount)
    {
        ApPackageTooSmall (Predefined->Info.Name, ParentOp,
            Count, ExpectedCount);
        return;
    }
    else if (Count > ExpectedCount)
    {
        ApPackageTooLarge (Predefined->Info.Name, ParentOp,
            Count, ExpectedCount);
    }

    /* Validate all elements of the package */

    Op = ApCheckPackageElements (Predefined->Info.Name, Op,
        ACPI_RTYPE_INTEGER, 16,
        ACPI_RTYPE_STRING, 4);

    /* Version 1 has a single trailing integer */

    if (Version > 0)
    {
        ApCheckPackageElements (Predefined->Info.Name, Op,
            ACPI_RTYPE_INTEGER, 1, 0, 0);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    ApCheckPackageElements
 *
 * PARAMETERS:  PredefinedName      - Name of the predefined object
 *              Op                  - Parser op for the package
 *              Type1               - Object type for first group
 *              Count1              - Count for first group
 *              Type2               - Object type for second group
 *              Count2              - Count for second group
 *
 * RETURN:      Next Op peer in the parse tree, after all specified elements
 *              have been validated. Used for multiple validations (calls
 *              to this function).
 *
 * DESCRIPTION: Validate all elements of a package. Works with packages that
 *              are defined to contain up to two groups of different object
 *              types.
 *
 ******************************************************************************/

static ACPI_PARSE_OBJECT *
ApCheckPackageElements (
    const char              *PredefinedName,
    ACPI_PARSE_OBJECT       *Op,
    UINT8                   Type1,
    UINT32                  Count1,
    UINT8                   Type2,
    UINT32                  Count2)
{
    UINT32                  i;


    /*
     * Up to two groups of package elements are supported by the data
     * structure. All elements in each group must be of the same type.
     * The second group can have a count of zero.
     *
     * Aborts check upon a NULL package element, as this means (at compile
     * time) that the remainder of the package elements are also NULL
     * (This is the only way to create NULL package elements.)
     */
    for (i = 0; (i < Count1) && Op; i++)
    {
        ApCheckObjectType (PredefinedName, Op, Type1, i);
        Op = Op->Asl.Next;
    }

    for (i = 0; (i < Count2) && Op; i++)
    {
        ApCheckObjectType (PredefinedName, Op, Type2, (i + Count1));
        Op = Op->Asl.Next;
    }

    return (Op);
}


/*******************************************************************************
 *
 * FUNCTION:    ApCheckPackageList
 *
 * PARAMETERS:  PredefinedName      - Name of the predefined object
 *              ParentOp            - Parser op of the parent package
 *              Package             - Package info for this predefined name
 *              StartIndex          - Index in parent package where list begins
 *              ParentCount         - Element count of parent package
 *
 * RETURN:      None
 *
 * DESCRIPTION: Validate the individual package elements for a predefined name.
 *              Handles the cases where the predefined name is defined as a
 *              Package of Packages (subpackages). These are the types:
 *
 *              ACPI_PTYPE2
 *              ACPI_PTYPE2_FIXED
 *              ACPI_PTYPE2_MIN
 *              ACPI_PTYPE2_COUNT
 *              ACPI_PTYPE2_FIX_VAR
 *              ACPI_PTYPE2_VAR_VAR
 *
 ******************************************************************************/

static void
ApCheckPackageList (
    const char                  *PredefinedName,
    ACPI_PARSE_OBJECT           *ParentOp,
    const ACPI_PREDEFINED_INFO  *Package,
    UINT32                      StartIndex,
    UINT32                      ParentCount)
{
    ACPI_PARSE_OBJECT           *SubPackageOp = ParentOp;
    ACPI_PARSE_OBJECT           *Op;
    ACPI_STATUS                 Status;
    UINT32                      Count;
    UINT32                      ExpectedCount;
    UINT32                      i;
    UINT32                      j;


    /*
     * Validate each subpackage in the parent Package
     *
     * Note: We ignore NULL package elements on the assumption that
     * they will be initialized by the BIOS or other ASL code.
     */
    for (i = 0; (i < ParentCount) && SubPackageOp; i++)
    {
        /* Each object in the list must be of type Package */

        Status = ApCheckObjectType (PredefinedName, SubPackageOp,
            ACPI_RTYPE_PACKAGE, i + StartIndex);
        if (ACPI_FAILURE (Status))
        {
            goto NextSubpackage;
        }

        /* Examine the different types of expected subpackages */

        Op = SubPackageOp->Asl.Child;

        /* First child is the package length */

        Count = (UINT32) Op->Asl.Value.Integer;
        Op = Op->Asl.Next;

        /*
         * Most subpackage must have at least one element, with
         * only rare exceptions. (_RDI)
         */
        if (!Count &&
            (Package->RetInfo.Type != ACPI_PTYPE2_VAR_VAR))
        {
            ApZeroLengthPackage (PredefinedName, SubPackageOp);
            goto NextSubpackage;
        }

        /*
         * Decode the package type.
         * PTYPE2 indicates that a "package of packages" is expected for
         * this name. The various flavors of PTYPE2 indicate the number
         * and format of the subpackages.
         */
        switch (Package->RetInfo.Type)
        {
        case ACPI_PTYPE2:
        case ACPI_PTYPE2_PKG_COUNT:
        case ACPI_PTYPE2_REV_FIXED:

            /* Each subpackage has a fixed number of elements */

            ExpectedCount = Package->RetInfo.Count1 + Package->RetInfo.Count2;
            if (Count < ExpectedCount)
            {
                ApPackageTooSmall (PredefinedName, SubPackageOp,
                    Count, ExpectedCount);
                break;
            }
            if (Count > ExpectedCount)
            {
                ApPackageTooLarge (PredefinedName, SubPackageOp,
                    Count, ExpectedCount);
                break;
            }

            ApCheckPackageElements (PredefinedName, Op,
                Package->RetInfo.ObjectType1, Package->RetInfo.Count1,
                Package->RetInfo.ObjectType2, Package->RetInfo.Count2);
            break;

        case ACPI_PTYPE2_FIX_VAR:
            /*
             * Each subpackage has a fixed number of elements and an
             * optional element
             */
            ExpectedCount = Package->RetInfo.Count1 + Package->RetInfo.Count2;
            if (Count < ExpectedCount)
            {
                ApPackageTooSmall (PredefinedName, SubPackageOp,
                    Count, ExpectedCount);
                break;
            }

            ApCheckPackageElements (PredefinedName, Op,
                Package->RetInfo.ObjectType1, Package->RetInfo.Count1,
                Package->RetInfo.ObjectType2,
                Count - Package->RetInfo.Count1);
            break;

        case ACPI_PTYPE2_VAR_VAR:
            /*
             * Must have at least the minimum number elements.
             * A zero PkgCount means the number of elements is variable.
             */
            ExpectedCount = Package->RetInfo4.PkgCount;
            if (ExpectedCount && (Count < ExpectedCount))
            {
                ApPackageTooSmall (PredefinedName, SubPackageOp,
                    Count, 1);
                break;
            }

            ApCheckPackageElements (PredefinedName, Op,
                Package->RetInfo4.SubObjectTypes,
                Package->RetInfo4.PkgCount,
                0, 0);
            break;

        case ACPI_PTYPE2_FIXED:

            /* Each subpackage has a fixed length */

            ExpectedCount = Package->RetInfo2.Count;
            if (Count < ExpectedCount)
            {
                ApPackageTooSmall (PredefinedName, SubPackageOp,
                    Count, ExpectedCount);
                break;
            }
            if (Count > ExpectedCount)
            {
                ApPackageTooLarge (PredefinedName, SubPackageOp,
                    Count, ExpectedCount);
                break;
            }

            /* Check each object/type combination */

            for (j = 0; j < ExpectedCount; j++)
            {
                ApCheckObjectType (PredefinedName, Op,
                    Package->RetInfo2.ObjectType[j], j);

                Op = Op->Asl.Next;
            }
            break;

        case ACPI_PTYPE2_MIN:

            /* Each subpackage has a variable but minimum length */

            ExpectedCount = Package->RetInfo.Count1;
            if (Count < ExpectedCount)
            {
                ApPackageTooSmall (PredefinedName, SubPackageOp,
                    Count, ExpectedCount);
                break;
            }

            /* Check the type of each subpackage element */

            ApCheckPackageElements (PredefinedName, Op,
                Package->RetInfo.ObjectType1, Count, 0, 0);
            break;

        case ACPI_PTYPE2_COUNT:
            /*
             * First element is the (Integer) count of elements, including
             * the count field (the ACPI name is NumElements)
             */
            Status = ApCheckObjectType (PredefinedName, Op,
                ACPI_RTYPE_INTEGER, 0);

            /* We must have an integer count from above (otherwise, use Count) */

            if (ACPI_SUCCESS (Status))
            {
                /*
                 * Make sure package is large enough for the Count and is
                 * is as large as the minimum size
                 */
                ExpectedCount = (UINT32) Op->Asl.Value.Integer;

                if (Count < ExpectedCount)
                {
                    ApPackageTooSmall (PredefinedName, SubPackageOp,
                        Count, ExpectedCount);
                    break;
                }
                else if (Count > ExpectedCount)
                {
                    ApPackageTooLarge (PredefinedName, SubPackageOp,
                        Count, ExpectedCount);
                }

                /* Some names of this type have a minimum length */

                if (Count < Package->RetInfo.Count1)
                {
                    ExpectedCount = Package->RetInfo.Count1;
                    ApPackageTooSmall (PredefinedName, SubPackageOp,
                        Count, ExpectedCount);
                    break;
                }

                Count = ExpectedCount;
            }

            /* Check the type of each subpackage element */

            Op = Op->Asl.Next;
            ApCheckPackageElements (PredefinedName, Op,
                Package->RetInfo.ObjectType1, (Count - 1), 0, 0);
            break;

        default:
            break;
        }

NextSubpackage:
        SubPackageOp = SubPackageOp->Asl.Next;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    ApPackageTooSmall
 *
 * PARAMETERS:  PredefinedName      - Name of the predefined object
 *              Op                  - Current parser op
 *              Count               - Actual package element count
 *              ExpectedCount       - Expected package element count
 *
 * RETURN:      None
 *
 * DESCRIPTION: Issue error message for a package that is smaller than
 *              required.
 *
 ******************************************************************************/

static void
ApPackageTooSmall (
    const char                  *PredefinedName,
    ACPI_PARSE_OBJECT           *Op,
    UINT32                      Count,
    UINT32                      ExpectedCount)
{

    sprintf (MsgBuffer, "%s: length %u, required minimum is %u",
        PredefinedName, Count, ExpectedCount);

    AslError (ASL_ERROR, ASL_MSG_RESERVED_PACKAGE_LENGTH, Op, MsgBuffer);
}


/*******************************************************************************
 *
 * FUNCTION:    ApZeroLengthPackage
 *
 * PARAMETERS:  PredefinedName      - Name of the predefined object
 *              Op                  - Current parser op
 *
 * RETURN:      None
 *
 * DESCRIPTION: Issue error message for a zero-length package (a package that
 *              is required to have a non-zero length). Variable length
 *              packages seem to be allowed to have zero length, however.
 *              Even if not allowed, BIOS code does it.
 *
 ******************************************************************************/

static void
ApZeroLengthPackage (
    const char                  *PredefinedName,
    ACPI_PARSE_OBJECT           *Op)
{

    sprintf (MsgBuffer, "%s: length is zero", PredefinedName);

    AslError (ASL_ERROR, ASL_MSG_RESERVED_PACKAGE_LENGTH, Op, MsgBuffer);
}


/*******************************************************************************
 *
 * FUNCTION:    ApPackageTooLarge
 *
 * PARAMETERS:  PredefinedName      - Name of the predefined object
 *              Op                  - Current parser op
 *              Count               - Actual package element count
 *              ExpectedCount       - Expected package element count
 *
 * RETURN:      None
 *
 * DESCRIPTION: Issue a remark for a package that is larger than expected.
 *
 ******************************************************************************/

static void
ApPackageTooLarge (
    const char                  *PredefinedName,
    ACPI_PARSE_OBJECT           *Op,
    UINT32                      Count,
    UINT32                      ExpectedCount)
{

    sprintf (MsgBuffer, "%s: length is %u, only %u required",
        PredefinedName, Count, ExpectedCount);

    AslError (ASL_REMARK, ASL_MSG_RESERVED_PACKAGE_LENGTH, Op, MsgBuffer);
}
