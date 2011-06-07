/*******************************************************************************
 *
 * Module Name: utxferror - Various error/warning output functions
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

#define __UTXFERROR_C__

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acnamesp.h>


#define _COMPONENT          ACPI_UTILITIES
        ACPI_MODULE_NAME    ("utxferror")

/*
 * This module is used for the in-kernel ACPICA as well as the ACPICA
 * tools/applications.
 *
 * For the iASL compiler case, the output is redirected to stderr so that
 * any of the various ACPI errors and warnings do not appear in the output
 * files, for either the compiler or disassembler portions of the tool.
 */
#ifdef ACPI_ASL_COMPILER
#include <stdio.h>

extern FILE                 *AcpiGbl_OutputFile;

#define ACPI_MSG_REDIRECT_BEGIN \
    FILE                    *OutputFile = AcpiGbl_OutputFile; \
    AcpiOsRedirectOutput (stderr);

#define ACPI_MSG_REDIRECT_END \
    AcpiOsRedirectOutput (OutputFile);

#else
/*
 * non-iASL case - no redirection, nothing to do
 */
#define ACPI_MSG_REDIRECT_BEGIN
#define ACPI_MSG_REDIRECT_END
#endif

/*
 * Common message prefixes
 */
#define ACPI_MSG_ERROR          "ACPI Error: "
#define ACPI_MSG_EXCEPTION      "ACPI Exception: "
#define ACPI_MSG_WARNING        "ACPI Warning: "
#define ACPI_MSG_INFO           "ACPI: "

/*
 * Common message suffix
 */
#define ACPI_MSG_SUFFIX \
    AcpiOsPrintf (" (%8.8X/%s-%u)\n", ACPI_CA_VERSION, ModuleName, LineNumber)


/*******************************************************************************
 *
 * FUNCTION:    AcpiError
 *
 * PARAMETERS:  ModuleName          - Caller's module name (for error output)
 *              LineNumber          - Caller's line number (for error output)
 *              Format              - Printf format string + additional args
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print "ACPI Error" message with module/line/version info
 *
 ******************************************************************************/

void ACPI_INTERNAL_VAR_XFACE
AcpiError (
    const char              *ModuleName,
    UINT32                  LineNumber,
    const char              *Format,
    ...)
{
    va_list                 ArgList;


    ACPI_MSG_REDIRECT_BEGIN;
    AcpiOsPrintf (ACPI_MSG_ERROR);

    va_start (ArgList, Format);
    AcpiOsVprintf (Format, ArgList);
    ACPI_MSG_SUFFIX;
    va_end (ArgList);

    ACPI_MSG_REDIRECT_END;
}

ACPI_EXPORT_SYMBOL (AcpiError)


/*******************************************************************************
 *
 * FUNCTION:    AcpiException
 *
 * PARAMETERS:  ModuleName          - Caller's module name (for error output)
 *              LineNumber          - Caller's line number (for error output)
 *              Status              - Status to be formatted
 *              Format              - Printf format string + additional args
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print "ACPI Exception" message with module/line/version info
 *              and decoded ACPI_STATUS.
 *
 ******************************************************************************/

void ACPI_INTERNAL_VAR_XFACE
AcpiException (
    const char              *ModuleName,
    UINT32                  LineNumber,
    ACPI_STATUS             Status,
    const char              *Format,
    ...)
{
    va_list                 ArgList;


    ACPI_MSG_REDIRECT_BEGIN;
    AcpiOsPrintf (ACPI_MSG_EXCEPTION "%s, ", AcpiFormatException (Status));

    va_start (ArgList, Format);
    AcpiOsVprintf (Format, ArgList);
    ACPI_MSG_SUFFIX;
    va_end (ArgList);

    ACPI_MSG_REDIRECT_END;
}

ACPI_EXPORT_SYMBOL (AcpiException)


/*******************************************************************************
 *
 * FUNCTION:    AcpiWarning
 *
 * PARAMETERS:  ModuleName          - Caller's module name (for error output)
 *              LineNumber          - Caller's line number (for error output)
 *              Format              - Printf format string + additional args
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print "ACPI Warning" message with module/line/version info
 *
 ******************************************************************************/

void ACPI_INTERNAL_VAR_XFACE
AcpiWarning (
    const char              *ModuleName,
    UINT32                  LineNumber,
    const char              *Format,
    ...)
{
    va_list                 ArgList;


    ACPI_MSG_REDIRECT_BEGIN;
    AcpiOsPrintf (ACPI_MSG_WARNING);

    va_start (ArgList, Format);
    AcpiOsVprintf (Format, ArgList);
    ACPI_MSG_SUFFIX;
    va_end (ArgList);

    ACPI_MSG_REDIRECT_END;
}

ACPI_EXPORT_SYMBOL (AcpiWarning)


/*******************************************************************************
 *
 * FUNCTION:    AcpiInfo
 *
 * PARAMETERS:  ModuleName          - Caller's module name (for error output)
 *              LineNumber          - Caller's line number (for error output)
 *              Format              - Printf format string + additional args
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print generic "ACPI:" information message. There is no
 *              module/line/version info in order to keep the message simple.
 *
 * TBD: ModuleName and LineNumber args are not needed, should be removed.
 *
 ******************************************************************************/

void ACPI_INTERNAL_VAR_XFACE
AcpiInfo (
    const char              *ModuleName,
    UINT32                  LineNumber,
    const char              *Format,
    ...)
{
    va_list                 ArgList;

#ifdef _KERNEL
    /* Temporarily hide too verbose printfs. */
    if (!bootverbose)
	return;
#endif

    ACPI_MSG_REDIRECT_BEGIN;
    AcpiOsPrintf (ACPI_MSG_INFO);

    va_start (ArgList, Format);
    AcpiOsVprintf (Format, ArgList);
    AcpiOsPrintf ("\n");
    va_end (ArgList);

    ACPI_MSG_REDIRECT_END;
}

ACPI_EXPORT_SYMBOL (AcpiInfo)


/*
 * The remainder of this module contains internal error functions that may
 * be configured out.
 */
#if !defined (ACPI_NO_ERROR_MESSAGES) && !defined (ACPI_BIN_APP)

/*******************************************************************************
 *
 * FUNCTION:    AcpiUtPredefinedWarning
 *
 * PARAMETERS:  ModuleName      - Caller's module name (for error output)
 *              LineNumber      - Caller's line number (for error output)
 *              Pathname        - Full pathname to the node
 *              NodeFlags       - From Namespace node for the method/object
 *              Format          - Printf format string + additional args
 *
 * RETURN:      None
 *
 * DESCRIPTION: Warnings for the predefined validation module. Messages are
 *              only emitted the first time a problem with a particular
 *              method/object is detected. This prevents a flood of error
 *              messages for methods that are repeatedly evaluated.
 *
 ******************************************************************************/

void ACPI_INTERNAL_VAR_XFACE
AcpiUtPredefinedWarning (
    const char              *ModuleName,
    UINT32                  LineNumber,
    char                    *Pathname,
    UINT8                   NodeFlags,
    const char              *Format,
    ...)
{
    va_list                 ArgList;


    /*
     * Warning messages for this method/object will be disabled after the
     * first time a validation fails or an object is successfully repaired.
     */
    if (NodeFlags & ANOBJ_EVALUATED)
    {
        return;
    }

    AcpiOsPrintf (ACPI_MSG_WARNING "For %s: ", Pathname);

    va_start (ArgList, Format);
    AcpiOsVprintf (Format, ArgList);
    ACPI_MSG_SUFFIX;
    va_end (ArgList);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtPredefinedInfo
 *
 * PARAMETERS:  ModuleName      - Caller's module name (for error output)
 *              LineNumber      - Caller's line number (for error output)
 *              Pathname        - Full pathname to the node
 *              NodeFlags       - From Namespace node for the method/object
 *              Format          - Printf format string + additional args
 *
 * RETURN:      None
 *
 * DESCRIPTION: Info messages for the predefined validation module. Messages
 *              are only emitted the first time a problem with a particular
 *              method/object is detected. This prevents a flood of
 *              messages for methods that are repeatedly evaluated.
 *
 ******************************************************************************/

void ACPI_INTERNAL_VAR_XFACE
AcpiUtPredefinedInfo (
    const char              *ModuleName,
    UINT32                  LineNumber,
    char                    *Pathname,
    UINT8                   NodeFlags,
    const char              *Format,
    ...)
{
    va_list                 ArgList;


    /*
     * Warning messages for this method/object will be disabled after the
     * first time a validation fails or an object is successfully repaired.
     */
    if (NodeFlags & ANOBJ_EVALUATED)
    {
        return;
    }

    AcpiOsPrintf (ACPI_MSG_INFO "For %s: ", Pathname);

    va_start (ArgList, Format);
    AcpiOsVprintf (Format, ArgList);
    ACPI_MSG_SUFFIX;
    va_end (ArgList);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtNamespaceError
 *
 * PARAMETERS:  ModuleName          - Caller's module name (for error output)
 *              LineNumber          - Caller's line number (for error output)
 *              InternalName        - Name or path of the namespace node
 *              LookupStatus        - Exception code from NS lookup
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print error message with the full pathname for the NS node.
 *
 ******************************************************************************/

void
AcpiUtNamespaceError (
    const char              *ModuleName,
    UINT32                  LineNumber,
    const char              *InternalName,
    ACPI_STATUS             LookupStatus)
{
    ACPI_STATUS             Status;
    UINT32                  BadName;
    char                    *Name = NULL;


    ACPI_MSG_REDIRECT_BEGIN;
    AcpiOsPrintf (ACPI_MSG_ERROR);

    if (LookupStatus == AE_BAD_CHARACTER)
    {
        /* There is a non-ascii character in the name */

        ACPI_MOVE_32_TO_32 (&BadName, ACPI_CAST_PTR (UINT32, InternalName));
        AcpiOsPrintf ("[0x%4.4X] (NON-ASCII)", BadName);
    }
    else
    {
        /* Convert path to external format */

        Status = AcpiNsExternalizeName (ACPI_UINT32_MAX,
                    InternalName, NULL, &Name);

        /* Print target name */

        if (ACPI_SUCCESS (Status))
        {
            AcpiOsPrintf ("[%s]", Name);
        }
        else
        {
            AcpiOsPrintf ("[COULD NOT EXTERNALIZE NAME]");
        }

        if (Name)
        {
            ACPI_FREE (Name);
        }
    }

    AcpiOsPrintf (" Namespace lookup failure, %s",
        AcpiFormatException (LookupStatus));

    ACPI_MSG_SUFFIX;
    ACPI_MSG_REDIRECT_END;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtMethodError
 *
 * PARAMETERS:  ModuleName          - Caller's module name (for error output)
 *              LineNumber          - Caller's line number (for error output)
 *              Message             - Error message to use on failure
 *              PrefixNode          - Prefix relative to the path
 *              Path                - Path to the node (optional)
 *              MethodStatus        - Execution status
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print error message with the full pathname for the method.
 *
 ******************************************************************************/

void
AcpiUtMethodError (
    const char              *ModuleName,
    UINT32                  LineNumber,
    const char              *Message,
    ACPI_NAMESPACE_NODE     *PrefixNode,
    const char              *Path,
    ACPI_STATUS             MethodStatus)
{
    ACPI_STATUS             Status;
    ACPI_NAMESPACE_NODE     *Node = PrefixNode;


    ACPI_MSG_REDIRECT_BEGIN;
    AcpiOsPrintf (ACPI_MSG_ERROR);

    if (Path)
    {
        Status = AcpiNsGetNode (PrefixNode, Path, ACPI_NS_NO_UPSEARCH,
                    &Node);
        if (ACPI_FAILURE (Status))
        {
            AcpiOsPrintf ("[Could not get node by pathname]");
        }
    }

    AcpiNsPrintNodePathname (Node, Message);
    AcpiOsPrintf (", %s", AcpiFormatException (MethodStatus));

    ACPI_MSG_SUFFIX;
    ACPI_MSG_REDIRECT_END;
}

#endif /* ACPI_NO_ERROR_MESSAGES */
